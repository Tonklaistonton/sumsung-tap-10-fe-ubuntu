#define pr_fmt(fmt) "lealt-mon: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/cpuidle.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/of_fdt.h>
#include "lealt.h"
#include <linux/perf_event.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <trace/hooks/cpuidle.h>
#include <linux/spinlock.h>

#if IS_ENABLED(CONFIG_SCHED_EMS)
#include "../kernel/sched/ems/ems.h"
#endif

static DEFINE_PER_CPU(bool, is_idle);
static DEFINE_PER_CPU(bool, is_on);

#define INST_EV	0x08
#define CYC_EV	0x11
#define MISS_EV 0x37

struct event_data {
	struct perf_event *pevent;
	unsigned long prev_count;
	unsigned long last_delta;
	unsigned long total;
};

struct cpu_data {
	struct event_data common_evs[NUM_COMMON_EVS];
	unsigned long freq;
	unsigned long inst;
	unsigned long cyc;
	unsigned long cachemiss;
};

struct lealt_mon_data {
	struct			list_head node;
	cpumask_t		cpus;
	int			domain_id;
	unsigned int		common_ev_ids[NUM_COMMON_EVS];
	struct cpu_data		*cpus_data;
	ktime_t			last_update_ts;
	unsigned long		last_ts_delta_us;

	struct mutex		lock;
	bool			initialized;

	/* Data */
	unsigned long		lat_freq;
	int			lat_cpu;

	/* Parameters */
	unsigned int		ratio_ceil;
	unsigned int		minlock_ratio;
	unsigned int		maxlock_ratio;
	unsigned int		llc_on_th_cpu;
	unsigned int		stability_th;
	struct core_dev_map	*freq_map;
};

#define to_cpu_data(mon, cpu) \
	(&mon->cpus_data[cpu - cpumask_first(&mon->cpus)])

static LIST_HEAD(mon_list);
static DEFINE_PER_CPU(struct lealt_mon_data*, mon_p);

#define show_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct lealt_mon_data *mon = platform_get_drvdata(pdev);	\
	return scnprintf(buf, PAGE_SIZE, "%s: %s: %u\n",	\
			dev_name(&pdev->dev), #name, mon->name);	\
}

#define store_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct platform_device *pdev = to_platform_device(dev);	\
	struct lealt_mon_data *mon = platform_get_drvdata(pdev);	\
	unsigned int val;						\
	if (kstrtouint(buf, 10, &val)) {				\
		return -EINVAL;					\
	}							\
	val = max(val, _min);					\
	val = min(val, _max);					\
	mon->name = val;					\
	return count;							\
}

static inline void read_event(struct event_data *event)
{
	if (!event->pevent)
		return;

	if (!per_cpu(is_on, event->pevent->cpu))
		return;

	if (per_cpu(is_idle, event->pevent->cpu)) {
		rmb();
		event->last_delta = event->total - event->prev_count;
		event->prev_count = event->total;
	} else {
		u64 total, enabled, running;
		total = perf_event_read_value(event->pevent, &enabled, &running);
		event->last_delta = total - event->prev_count;
		event->prev_count = total;
	}
}

static inline void read_event_local(struct event_data *event)
{
	u64 total, enabled, running;
	int ret = 0;

	if (!event->pevent)
		return;

	if (event->pevent->oncpu == -1)
		return;

	if (!per_cpu(is_on, event->pevent->cpu))
		return;

	ret = perf_event_read_local(event->pevent, &total,
			  &enabled, &running);

	if (ret)
		pr_err("read event fail %d\n", ret);
	else
		event->total = total;

}

static void update_counts_idle_core(struct lealt_mon_data *mon, int cpu)
{
	unsigned int i;
	struct cpu_data *cpu_data = to_cpu_data(mon, cpu);
	struct event_data *common_evs = cpu_data->common_evs;

	for (i = 0; i < NUM_COMMON_EVS; i++)
		read_event_local(&common_evs[i]);
}

static void vendor_update_event_cpu_idle_enter(void *data,
				int *state, struct cpuidle_device *dev)
{
	struct lealt_mon_data *mon;

	if (!__this_cpu_read(is_on))
		return;

	mon = this_cpu_read(mon_p);
	if (!mon) {
		pr_err("failed to find per_cpu_p (CPU: %d)", dev->cpu);
		return;
	}

	update_counts_idle_core(mon, dev->cpu);

	wmb();
	__this_cpu_write(is_idle, true);
}

static void vendor_update_event_cpu_idle_exit(void *data,
				int state, struct cpuidle_device *dev)
{
	if (!__this_cpu_read(is_on))
		return;
	__this_cpu_write(is_idle, false);
}

static void update_counts(struct lealt_mon_data *mon)
{
	unsigned int cpu, i;
	ktime_t now = ktime_get();
	unsigned long delta = ktime_us_delta(now, mon->last_update_ts);
	struct cpu_data *cpu_data;
	struct event_data *common_evs;

	mon->last_ts_delta_us = delta;
	mon->last_update_ts = now;

	for_each_cpu(cpu, &mon->cpus) {
		cpu_data = to_cpu_data(mon, cpu);
		common_evs = cpu_data->common_evs;
		for (i = 0; i < NUM_COMMON_EVS; i++)
			read_event(&common_evs[i]);

		cpu_data->freq = common_evs[CYC_IDX].last_delta / delta;
		cpu_data->inst = common_evs[INST_IDX].last_delta;
		cpu_data->cyc = common_evs[CYC_IDX].last_delta;
		cpu_data->cachemiss = common_evs[MISS_IDX].last_delta;
	}
}

static unsigned long core_to_targetload(struct core_dev_map *map,
		unsigned long coref)
{
	unsigned long freq = 0;

	if (!map)
		goto out;

	while (map->core_mhz && map->core_mhz < coref)
		map++;
	if (!map->core_mhz)
		map--;
	freq = map->targetload;

out:
	pr_debug("freq: %lu -> dev: %lu\n", coref, freq);
	return freq;
}

static int update_next_llc(struct lealt_mon_data *mon,
			struct lealt_sample *sample)
{
	if (!mon->llc_on_th_cpu)
		return 0;

	if (mon->lat_freq > mon->llc_on_th_cpu)
		sample->next_llc = LEALT_LLC_CONTROL_ON;

	return 0;
}

static int get_lat_cpu(struct lealt_mon_data *mon)
{
	unsigned int cpu;

	for_each_cpu(cpu, &mon->cpus) {
		struct cpu_data *cpu_data = to_cpu_data(mon, cpu);
		unsigned int ratio = 0;

		if (!cpu_data->freq || !cpu_data->cachemiss) {
			continue;
		}

		ratio = cpu_data->inst / cpu_data->cachemiss;

		if (ratio <= mon->ratio_ceil
			&& cpu_data->freq > mon->lat_freq) {
			mon->lat_cpu = cpu;
			mon->lat_freq = cpu_data->freq;
		}
	}

	return 0;
}

static int update_latlock(struct lealt_mon_data *mon,
			   struct lealt_sample *sample)
{
	unsigned long minlock = mon->lat_freq
				* mon->minlock_ratio / 100UL
				* 1000UL;
	unsigned long maxlock = mon->lat_freq
				* mon->maxlock_ratio / 100UL
				* 1000UL;

	sample->lat_minlock = minlock > sample->lat_minlock ?
		minlock : sample->lat_minlock;
	sample->lat_maxlock = maxlock > sample->lat_maxlock ?
		maxlock : sample->lat_maxlock;

	return 0;
}

static int update_targetload(struct lealt_mon_data *mon,
		      struct lealt_sample *sample)
{
	struct cpu_data *cpu_data;
	int cpu;
	unsigned long targetload;

	mon->lat_freq = 0;

	for_each_cpu(cpu, &mon->cpus) {
		cpu_data = to_cpu_data(mon, cpu);

		if (cpu_data->freq > mon->lat_freq) {
			mon->lat_freq = cpu_data->freq;
			mon->lat_cpu = cpu;
		}
	}

	targetload = core_to_targetload(mon->freq_map, mon->lat_freq);

	if (targetload < sample->targetload) {
		sample->targetload = targetload;
		sample->lat_freq = mon->lat_freq;
		sample->lat_cpu = mon->lat_cpu;
	}

	return 0;
}

int lealt_mon_get_stability(int domain_id)
{
	struct lealt_mon_data *mon = NULL;

	list_for_each_entry(mon, &mon_list, node) {
		if (mon->domain_id == domain_id)
			break;
	}

	if (!mon)
		return 0;

	pr_debug("%s: domain_id: %d, threshold: %u\n",
	       __func__, domain_id, mon->stability_th);

	return mon->stability_th;
}
EXPORT_SYMBOL_GPL(lealt_mon_get_stability);

int lealt_mon_get_metrics(struct lealt_sample *sample)
{
	struct lealt_mon_data *mon;

	list_for_each_entry(mon, &mon_list, node) {
		mutex_lock(&mon->lock);
		get_lat_cpu(mon);
		update_next_llc(mon, sample);
		update_targetload(mon, sample);
		update_latlock(mon, sample);
		mutex_unlock(&mon->lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(lealt_mon_get_metrics);

int lealt_mon_update_counts_all(void)
{
	struct lealt_mon_data *mon;

	list_for_each_entry(mon, &mon_list, node) {
		mutex_lock(&mon->lock);
		update_counts(mon);
		mutex_unlock(&mon->lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(lealt_mon_update_counts_all);

static void delete_event(struct event_data *event)
{
	event->prev_count = event->last_delta = 0;
	if (event->pevent) {
		exynos_perf_event_release_kernel(event->pevent);
		event->pevent = NULL;
	}
}

static struct perf_event_attr *alloc_attr(void)
{
	struct perf_event_attr *attr;

	attr = kzalloc(sizeof(struct perf_event_attr), GFP_KERNEL);
	if (!attr)
		return attr;

	attr->type = PERF_TYPE_RAW;
	attr->size = (u32)sizeof(struct perf_event_attr);
	attr->pinned = 1;

	return attr;
}

static int set_event(struct event_data *ev, int cpu, unsigned int event_id,
		     struct perf_event_attr *attr)
{
	struct perf_event *pevent;

	if (!event_id)
		return 0;

	attr->config = event_id;
	pevent = exynos_perf_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
	if (!pevent)
		return -ENOMEM;

	ev->pevent = pevent;

	return 0;
}

static int lealt_mon_cpuhp_up(unsigned int cpu)
{
	int ret = 0;
	unsigned int i;
	struct lealt_mon_data *mon;
	struct cpu_data *cpu_data;
	struct event_data *common_evs;
	struct perf_event_attr *attr;

	mon = per_cpu(mon_p, cpu);
	if (!mon) {
		pr_err("failed to find per_cpu_p (CPU: %d)", cpu);
		return 0;
	}

	attr = alloc_attr();
	if (!attr)
		return -ENOMEM;

	mutex_lock(&mon->lock);
	cpu_data = to_cpu_data(mon, cpu);
	common_evs = cpu_data->common_evs;
	for (i = 0; i < NUM_COMMON_EVS; i++) {
		ret = set_event(&common_evs[i], cpu,
		  mon->common_ev_ids[i], attr);
		if (ret) {
			pr_err("set event %u on CPU %u fail: %d",
			  mon->common_ev_ids[i],
			  cpu, ret);
			goto unlock_out;
		}
	}

	per_cpu(is_on, cpu) = true;

unlock_out:
	mutex_unlock(&mon->lock);
	kfree(attr);
	return ret;
}

static int lealt_mon_cpuhp_down(unsigned int cpu)
{
	int ret = 0;
	unsigned int i;
	struct lealt_mon_data *mon;
	struct cpu_data *cpu_data;
	struct event_data *common_evs;

	mon = per_cpu(mon_p, cpu);
	if (!mon) {
		pr_err("failed to find per_cpu_p (CPU: %d)", cpu);
		return 0;
	}

	mutex_lock(&mon->lock);
	cpu_data = to_cpu_data(mon, cpu);
	common_evs = cpu_data->common_evs;

	for (i = 0; i < NUM_COMMON_EVS; i++)
		delete_event(&common_evs[i]);

	per_cpu(is_on, cpu) = false;

	mutex_unlock(&mon->lock);

	return ret;
}

static int get_mask_from_dev_handle(struct platform_device *pdev,
					cpumask_t *mask)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_phandle;
	struct device *cpu_dev;
	int cpu, i = 0;
	int ret = -ENOENT;

	dev_phandle = of_parse_phandle(dev->of_node, "cpulist", i++);
	while (dev_phandle) {
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpumask_set_cpu(cpu, mask);
				ret = 0;
				break;
			}
		}
		dev_phandle = of_parse_phandle(dev->of_node,
						"cpulist", i++);
	}

	return ret;
}

#define NUM_COLS	2
static struct core_dev_map *init_core_dev_map(struct device *dev,
					struct device_node *of_node,
					char *prop_name)
{
	int len, nf, i, j;
	u32 data;
	struct core_dev_map *tbl;
	int ret;

	if (!of_node)
		of_node = dev->of_node;

	if (!of_find_property(of_node, prop_name, &len))
		return NULL;
	len /= sizeof(data);

	if (len % NUM_COLS || len == 0)
		return NULL;
	nf = len / NUM_COLS;

	tbl = devm_kzalloc(dev, (nf + 1) * sizeof(struct core_dev_map),
			GFP_KERNEL);
	if (!tbl)
		return NULL;

	for (i = 0, j = 0; i < nf; i++, j += 2) {
		ret = of_property_read_u32_index(of_node, prop_name, j,
				&data);
		if (ret)
			return NULL;
		tbl[i].core_mhz = data / 1000;

		ret = of_property_read_u32_index(of_node, prop_name, j + 1,
				&data);
		if (ret)
			return NULL;
		tbl[i].targetload = data;
		pr_debug("Entry%d CPU:%u, TargetLoad:%u\n", i, tbl[i].core_mhz,
				tbl[i].targetload);
	}
	tbl[i].core_mhz = 0;

	return tbl;
}

static int lealt_mon_parse_dt(struct lealt_mon_data *mon,
				struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	unsigned int val, len;

	if (get_mask_from_dev_handle(pdev, &mon->cpus)) {
		dev_err(dev, "No CPUs specified.\n");
		return -ENODEV;
	}

	len = strlen(dev->of_node->name);
	mon->domain_id = dev->of_node->name[len - 1] - '0';

	ret = of_property_read_u32(dev->of_node, "inst-ev", &val);
	if (ret) {
		dev_dbg(dev, "Inst event not specified. Using def:0x%x\n",
			INST_EV);
		val = INST_EV;
	}
	mon->common_ev_ids[INST_IDX] = val;

	ret = of_property_read_u32(dev->of_node, "cyc-ev", &val);
	if (ret) {
		dev_dbg(dev, "Cyc event not specified. Using def:0x%x\n",
			CYC_EV);
		val = CYC_EV;
	}
	mon->common_ev_ids[CYC_IDX] = val;

	ret = of_property_read_u32(dev->of_node, "cachemiss-ev", &val);
	if (ret) {
		dev_dbg(dev, "Stall event not specified. Skipping.\n");
		val = MISS_EV;
	}
	mon->common_ev_ids[MISS_IDX] = val;

	ret = of_property_read_u32(dev->of_node, "ratio_ceil", &val);
	if (ret) {
		dev_dbg(dev, "ratio_ceil not specified. Skipping.\n");
		val = 400;
	}
	mon->ratio_ceil = val;

	ret = of_property_read_u32(dev->of_node, "minlock-ratio", &val);
	if (ret) {
		dev_dbg(dev, "base-minlock-ratio not specified. Skipping.\n");
		val = 0;
	}
	mon->minlock_ratio = val;

	ret = of_property_read_u32(dev->of_node, "maxlock-ratio", &val);
	if (ret) {
		dev_dbg(dev, "base-maxlock-ratio not specified. Skipping.\n");
		val = 0;
	}
	mon->maxlock_ratio = val;

	ret = of_property_read_u32(dev->of_node, "llc_on_th_cpu", &val);
	if (ret) {
		dev_dbg(dev, "llc_on_th_cpu not specified. Skipping.\n");
		val = 0;
	}
	mon->llc_on_th_cpu = val;

	ret = of_property_read_u32(dev->of_node, "stability_th", &val);
	if (ret) {
		dev_dbg(dev, "stability_th not specified. Skipping.\n");
		val = 0;
	}
	mon->stability_th = val;

	mon->freq_map = init_core_dev_map(dev, NULL,
					      "core-targetload-table");
	return 0;
}

show_attr(ratio_ceil)
store_attr(ratio_ceil, 1U, 20000U)
static DEVICE_ATTR(ratio_ceil, 0644, show_ratio_ceil, store_ratio_ceil);

show_attr(minlock_ratio)
store_attr(minlock_ratio, 0U, 1000U)
static DEVICE_ATTR(minlock_ratio, 0644, show_minlock_ratio,
						 store_minlock_ratio);

show_attr(maxlock_ratio)
store_attr(maxlock_ratio, 0U, 1000U)
static DEVICE_ATTR(maxlock_ratio, 0644, show_maxlock_ratio,
						 store_maxlock_ratio);

show_attr(llc_on_th_cpu)
store_attr(llc_on_th_cpu, 0U, 3168000U)
static DEVICE_ATTR(llc_on_th_cpu, 0644, show_llc_on_th_cpu, store_llc_on_th_cpu);

static struct attribute *lealt_mon_attr[] = {
	&dev_attr_ratio_ceil.attr,
	&dev_attr_minlock_ratio.attr,
	&dev_attr_maxlock_ratio.attr,
	&dev_attr_llc_on_th_cpu.attr,
	NULL,
};

static struct attribute_group lealt_mon_attr_group = {
	.name = "attrs",
	.attrs = lealt_mon_attr,
};

static bool hook_registered;
static bool cpuhp_registered;
static int lealt_mon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lealt_mon_data *mon;
	int ret = 0, cpu;
	unsigned int num_cpus;

	mon = devm_kzalloc(dev, sizeof(*mon), GFP_KERNEL);
	if (!mon)
		return -ENOMEM;

	ret = lealt_mon_parse_dt(mon, pdev);
	if (ret) {
		dev_err(dev, "Failure to probe lealt device: %d\n", ret);
		return ret;
	}

	num_cpus = cpumask_weight(&mon->cpus);
	mon->cpus_data =
		devm_kzalloc(dev, num_cpus * sizeof(*mon->cpus_data),
			     GFP_KERNEL);
	if (!mon->cpus_data)
		return -ENOMEM;

	mutex_init(&mon->lock);

	INIT_LIST_HEAD(&mon->node);
	list_add_tail(&mon->node, &mon_list);

	platform_set_drvdata(pdev, mon);

	for_each_cpu(cpu, &mon->cpus) {
		if (!cpumask_test_cpu(cpu, cpu_online_mask))
			break;
		per_cpu(mon_p, cpu) = mon;
		wmb();
		lealt_mon_cpuhp_up(cpu);
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &lealt_mon_attr_group);
	if (ret)
		return ret;

	if (!cpuhp_registered) {
		ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
			"lealt-mon", lealt_mon_cpuhp_up, lealt_mon_cpuhp_down);
		if (ret < 0) {
			dev_err(dev, "Register CPU hotplug notifier fail %d\n", ret);
			return ret;
		}
		cpuhp_registered = true;
	}

	if (!hook_registered) {
		ret = register_trace_android_vh_cpu_idle_enter(
				vendor_update_event_cpu_idle_enter, NULL);
		if (ret) {
			dev_err(dev, "Register enter vendor hook fail %d\n", ret);
			return ret;
		}

		ret = register_trace_android_vh_cpu_idle_exit(
				vendor_update_event_cpu_idle_exit, NULL);
		if (ret) {
			dev_err(dev, "Register exit vendor hook fail %d\n", ret);
			return ret;
		}

		hook_registered = true;
	}

	return 0;
}

static const struct of_device_id lealt_match_table[] = {
	{ .compatible = "lealt-mon", },
	{}
};

static struct platform_driver lealt_mon_driver = {
	.probe = lealt_mon_probe,
	.driver = {
		.name = "lealt-mon",
		.of_match_table = lealt_match_table,
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(lealt_mon_driver);

MODULE_SOFTDEP("post: exynos_thermal");
MODULE_LICENSE("GPL v2");
