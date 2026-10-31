#define pr_fmt(fmt) "mem_lat: " fmt

#include <linux/kernel.h>
#include <linux/sizes.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include "lealt.h"
#include <soc/samsung/exynos-sci.h>
#include <soc/samsung/exynos-wow.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/ems.h>
#include "../kernel/sched/ems/ems.h"
#include <soc/samsung/exynos-devfreq.h>
#include <linux/exynos-cpufreq.h>
#include <soc/samsung/exynos-mifgov.h>

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif

#include <trace/events/power.h>
#define CREATE_TRACE_POINTS
#include <trace/events/governor_lealt_trace.h>
#define MAX_LOAD 84;

#define NR_SAMPLES 10

struct lealt_gov_data {
	struct mutex	lock;

	cpumask_t               polling_cpus;
	spinlock_t work_control_lock;
	struct work_struct	work;
	struct work_struct	mifgov_work;
	bool control_mifgov;
	bool work_started;
	unsigned long last_work_time;

	unsigned int polling_ms;
	unsigned int polling_ms_min;
	unsigned int polling_ms_max;
	struct notifier_block cpufreq_trans_nb;
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	struct notifier_block cpupm_nb;
#endif
	unsigned long expires;
	struct timer_list deferrable_timer[CONFIG_VENDOR_NR_CPUS];

	struct exynos_devfreq_freq_infos devfreq_infos;
	struct notifier_block		 devfreq_trans_nb;
	u64 *time_in_state;
	u64 last_stat_updated;
	struct exynos_devfreq_freqs freqs;
	spinlock_t    time_in_state_lock;

	unsigned long governor_freq;
	enum lealt_llc_control governor_llc;
	unsigned long mif_bw_util;
	unsigned long mif_bus_width;

	int ems_mode;
	struct notifier_block		emstune_notifier;

	int force_mode;

	struct lealt_sample samples[NR_SAMPLES];
	int cur_idx;
	unsigned int cur_seq_no;
	s64 hold_time_freq;
	s64 hold_time_llc;

	unsigned long llc_on_th;
	unsigned long llc_off_th;

	unsigned long efficient_freq;

	unsigned int		*alt_target_load;
	unsigned int		alt_num_target_load;

	u32 trace_on;

	struct exynos_wow_meta wow_meta;
	struct exynos_wow_profile last_profile;

	struct exynos_pm_qos_request	pm_qos_min;

	struct device *dev;
	struct device *devfreq_dev;
};

struct cpufreq_stability {
	int domain_id;
	u32 min_freq;

	unsigned int threshold;
	unsigned int stability;
	unsigned int freq_cur;
	unsigned int freq_high;
	unsigned int freq_low;
	struct list_head node;
};

static LIST_HEAD(cpufreq_stability_list);

enum cpufreq_stability_state {
	CPUFREQ_STABILITY_NORMAL,
	CPUFREQ_STABILITY_ALERT,
};

static struct workqueue_struct *lealt_wq;
static struct lealt_gov_data *gov_data_global;

static void init_cpufreq_stability_all(void);
static void lealt_set_polling_ms(struct lealt_gov_data *gov_data,
				 unsigned int new_ms);

#define show_gov_data_attr(name) \
static ssize_t show_##name(struct device *dev,				\
			struct device_attribute *attr, char *buf)	\
{									\
	struct platform_device *pdev = to_platform_device(dev);	\
	struct lealt_gov_data *gov_data = platform_get_drvdata(pdev);	\
	ssize_t count = 0; \
	mutex_lock(&gov_data->lock);	\
	count += snprintf(buf + count, PAGE_SIZE, "%s: %lld\n",\
		#name, (long long)gov_data->name); \
	mutex_unlock(&gov_data->lock);	\
	return count;		\
}

#define store_gov_data_attr(name, _min, _max) \
static ssize_t store_##name(struct device *dev,				\
			struct device_attribute *attr, const char *buf,	\
			size_t count)					\
{									\
	struct platform_device *pdev = to_platform_device(dev);	\
	struct lealt_gov_data *gov_data = platform_get_drvdata(pdev);	\
	int ret;						\
	unsigned int val;	\
	mutex_lock(&gov_data->lock);	\
	ret = sscanf(buf, "%u", &val);	\
	if (ret != 1) {							\
		mutex_unlock(&gov_data->lock);				\
		return -EINVAL;	\
	}								\
	val = max(val, _min);					\
	val = min(val, _max);					\
	gov_data->name = val;	\
	mutex_unlock(&gov_data->lock);	\
	return count;			\
}

/* Show Current ALT Parameter Info */
static ssize_t alt_target_load_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lealt_gov_data *gov_data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;

	for (i = 0; i < gov_data->alt_num_target_load; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "%d%s",
				  gov_data->alt_target_load[i],
				  (i == gov_data->alt_num_target_load - 1) ?
				  "" : (i % 2) ? ":" : " ");
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");

	return count;
}
static DEVICE_ATTR_RO(alt_target_load);

static struct lealt_sample *lealt_get_cur_sample(struct lealt_gov_data *gov_data)
{
	struct lealt_sample *sample = &gov_data->samples[gov_data->cur_idx++];

	// Circular index
	gov_data->cur_idx = gov_data->cur_idx < NR_SAMPLES ?
				 gov_data->cur_idx:
				 0;

	// Initialize sample
	memset(sample, 0, sizeof(struct lealt_sample));
	return sample;
}

static int lealt_get_prev_idx(int index)
{
	// Circular index
	 return index - 1 > 0 ? index - 1 : NR_SAMPLES - 1;
}

static int lealt_backtrace_samples(
				struct lealt_gov_data *gov_data,
				unsigned long *final_freq,
				enum lealt_llc_control *final_llc)
{

	unsigned long sum_duration = 0;
	int idx = gov_data->cur_idx;
	unsigned long max_freq = 0;
	int consider_cnt_freq = 0, consider_cnt_llc = 0;
	enum lealt_llc_control max_llc = LEALT_LLC_CONTROL_OFF;
	int i = 0;
	for (i = 0; i < NR_SAMPLES; i++) {
		struct lealt_sample *sample;
		idx = lealt_get_prev_idx(idx);
		sample = &gov_data->samples[idx];
		sum_duration += sample->duration;
		if (consider_cnt_freq == 0 ||
				sum_duration <= gov_data->hold_time_freq) {
			max_freq = sample->next_freq > max_freq ?
				sample->next_freq :
				max_freq;
			consider_cnt_freq++;
		}
		if (consider_cnt_llc == 0 ||
				sum_duration <= gov_data->hold_time_llc) {
			max_llc = sample->next_llc > max_llc ?
				sample->next_llc :
				max_llc;
			consider_cnt_llc++;
		}
		if (gov_data->trace_on)
			trace_lealt_sample_backtrace(sample->seq_no,
						sample->duration,
						sample->next_freq,
						sample->next_llc);
		if (sum_duration > gov_data->hold_time_freq
			&& sum_duration > gov_data->hold_time_llc)
			break;
	}
	*final_freq = max_freq;
	*final_llc = max_llc;
	return 0;
}

static int lealt_decide_next_llc(struct lealt_gov_data *gov_data,
				struct lealt_sample *sample)
{
	if (sample->next_llc == LEALT_LLC_CONTROL_ON)
		return 0;

	if (!gov_data->llc_on_th)
		return 0;

	if (sample->mif_active_freq > gov_data->llc_on_th)
		sample->next_llc = LEALT_LLC_CONTROL_ON;
	else if (sample->mif_active_freq < gov_data->llc_off_th)
		sample->next_llc = LEALT_LLC_CONTROL_OFF;
	else
		sample->next_llc = LEALT_LLC_CONTROL_NONE;

	return 0;
}

static int lealt_control_llc(
			struct lealt_gov_data *gov_data,
			enum lealt_llc_control llc_control)
{
	if (llc_control == LEALT_LLC_CONTROL_NONE)
		return 0;

	if (gov_data->governor_llc == llc_control)
		return 0;

	switch(llc_control) {
	case LEALT_LLC_CONTROL_OFF:
		llc_region_alloc(LLC_REGION_CPU, 0, 0);
		break;
	case LEALT_LLC_CONTROL_ON:
		llc_region_alloc(LLC_REGION_CPU, 1, LLC_WAY_MAX);
		break;
	default:
		break;
	}

	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: GovernorLLC", llc_control,
			raw_smp_processor_id());

	gov_data->governor_llc = llc_control;

	return 0;
}
static int lealt_decide_next_freq(struct lealt_sample *sample)
{
	unsigned long next_freq = 0;
	unsigned long maxlock = sample->lat_maxlock > sample->bw_maxlock ?
					sample->lat_maxlock : sample->bw_maxlock;

	if (sample->targetload)
		next_freq = sample->mif_active_load * sample->mif_avg_freq
				/ sample->targetload;

	if (next_freq < sample->lat_minlock)
		next_freq = sample->lat_minlock;

	if (next_freq > maxlock)
		next_freq = maxlock;

	sample->next_freq = next_freq;

	return 0;
}

static int lealt_get_wow_data(struct lealt_gov_data *gov_data,
			      struct lealt_sample *sample)
{
	struct exynos_wow_profile profile;
	struct exynos_wow_profile_data *new_pd, *old_pd;
	u32 bus_width;

	exynos_wow_get_data(&profile);
	sample->duration = (profile.ktime - gov_data->last_profile.ktime)
		/ NSEC_PER_USEC;

	sample->mif_active_load = 0;

	if (gov_data->wow_meta.data[CPU].enable) {
		new_pd = &profile.data[CPU];
		old_pd = &gov_data->last_profile.data[CPU];
		bus_width = gov_data->wow_meta.data[CPU].bus_width;
	} else {
		new_pd = &profile.data[MIF];
		old_pd = &gov_data->last_profile.data[MIF];
		bus_width = gov_data->wow_meta.data[MIF].bus_width;
	}

	if (new_pd->ccnt - old_pd->ccnt)
		sample->mif_active_load = (new_pd->active - old_pd->active)
				* 100UL / (new_pd->ccnt - old_pd->ccnt);

	if (sample->mif_active_load > 100)
	       sample->mif_active_load = 100;

	if (!sample->duration || !gov_data->mif_bus_width
		    || !gov_data->mif_bw_util)
		goto out;

	sample->bw_maxlock = (new_pd->transfer_data - old_pd->transfer_data)
			* bus_width / (sample->duration / USEC_PER_MSEC)
			/ gov_data->mif_bus_width * 100 / gov_data->mif_bw_util;
	if (gov_data->trace_on) {
		trace_clock_set_rate("LEALT: max_freq_TDATA",
			sample->bw_maxlock,
			raw_smp_processor_id());
	}

out:
	gov_data->last_profile = profile;

	return 0;
}

static int lealt_update_time_in_state(struct lealt_gov_data *gov_data,
						u32 lv, u64 freq)
{
	unsigned long cur_time;

	if (lv >= gov_data->devfreq_infos.max_state) {
		dev_info(gov_data->dev, "failed to update TS %llu\n", freq);
		return -EINVAL;
	}

	cur_time = ktime_get();
	gov_data->time_in_state[lv] +=
			cur_time - gov_data->last_stat_updated;
	gov_data->last_stat_updated = cur_time;

	return 0;
}

static u64 lealt_get_avg_freq(struct lealt_gov_data *gov_data)
{
	int i, max_state = gov_data->devfreq_infos.max_state;
	struct exynos_devfreq_freqs *freqs;
	u64 product_sum = 0, time_sum = 0;
	unsigned long flags;

	spin_lock_irqsave(&gov_data->time_in_state_lock, flags);
	freqs = &gov_data->freqs;
	lealt_update_time_in_state(gov_data, freqs->new_lv, freqs->new_freq);
	spin_unlock_irqrestore(&gov_data->time_in_state_lock, flags);

	spin_lock_irqsave(&gov_data->time_in_state_lock, flags);
	for (i = 0; i < max_state; i++) {
		u64 *time = &gov_data->time_in_state[i];
		u32 *freq = &gov_data->devfreq_infos.freq_table[i];

		product_sum += (*time) * (*freq);
		time_sum += (*time);

		*time = 0;
	}
	spin_unlock_irqrestore(&gov_data->time_in_state_lock, flags);

	if (!time_sum || !product_sum) {
		dev_info(gov_data->dev, "failed to get avg frequency\n");
		return freqs->new_freq;
	}

	return product_sum / time_sum;
}

static unsigned long lealt_get_freq(struct lealt_gov_data *gov_data)
{
	struct lealt_sample *sample;
	unsigned long freq;
	enum lealt_llc_control llc_control = LEALT_LLC_CONTROL_OFF;
	int i = 0, ems_mode = gov_data->ems_mode;

	/* Get current sample */
	sample = lealt_get_cur_sample(gov_data);
	sample->seq_no = gov_data->cur_seq_no++;
	sample->targetload = MAX_LOAD;

	/* Get targetload, lat_maxlock */
	lealt_get_wow_data(gov_data, sample);

	/* Get mif_avg_freq, mif_active_freq */
	sample->mif_avg_freq = lealt_get_avg_freq(gov_data);
	sample->mif_active_freq
		= sample->mif_avg_freq * sample->mif_active_load / 100UL;

	if (gov_data->force_mode)
		ems_mode = gov_data->force_mode - 1;

	/* Get lat_minlock, lat_maxlock, targetload, next_llc */
	if(ems_mode) {
		/* operate like ALT */
		for (i = 0; i < gov_data->alt_num_target_load - 1 &&
		     sample->mif_avg_freq >= gov_data->alt_target_load[i + 1];
		     i += 2);
		sample->targetload = gov_data->alt_target_load[i];
		sample->lat_maxlock = gov_data->alt_target_load[
					gov_data->alt_num_target_load - 2];
		sample->lat_minlock = 0;
		sample->next_llc = LEALT_LLC_CONTROL_OFF;
	} else {
		lealt_mon_update_counts_all();
		lealt_mon_get_metrics(sample);
		lealt_decide_next_llc(gov_data, sample);
	}

	/* Get next_freq */
	lealt_decide_next_freq(sample);

	/* Consider multi samples */
	lealt_backtrace_samples(gov_data, &freq, &llc_control);

	/* Apply finally decided llc_state */
	lealt_control_llc(gov_data, llc_control);

	/* Make sure to keep efficient_freq when LLC is trying to turn off */
	if (gov_data->governor_llc == LEALT_LLC_CONTROL_ON)
		freq = freq < gov_data->efficient_freq ?
			gov_data->efficient_freq : freq;

	if (freq > gov_data->devfreq_infos.max_freq)
		freq = gov_data->devfreq_infos.max_freq;

	/* Trace current sample */
	if (gov_data->trace_on) {
		trace_clock_set_rate("LEALT: ActiveLoad",
			sample->mif_active_load,
			raw_smp_processor_id());
		trace_clock_set_rate("LEALT: ActiveFreq",
			sample->mif_active_freq,
			raw_smp_processor_id());
		trace_lealt_governor_latency_dev(sample->lat_cpu,
				 sample->lat_freq, sample->targetload);
		trace_lealt_sample_cur(sample->seq_no, sample->duration,
				sample->next_freq, sample->next_llc,
				sample->mif_avg_freq, sample->mif_active_load,
				sample->targetload, sample->mif_active_freq,
				sample->lat_minlock,
				sample->lat_maxlock);
	}

	return freq;
}

show_gov_data_attr(mif_bw_util)
store_gov_data_attr(mif_bw_util, 1U, 100U)
static DEVICE_ATTR(mif_bw_util, 0644, show_mif_bw_util, store_mif_bw_util);

show_gov_data_attr(mif_bus_width)
store_gov_data_attr(mif_bus_width, 1U, 100U)
static DEVICE_ATTR(mif_bus_width, 0644, show_mif_bus_width, store_mif_bus_width);

show_gov_data_attr(hold_time_freq)
store_gov_data_attr(hold_time_freq, 1U, 200000U)
static DEVICE_ATTR(hold_time_freq, 0644, show_hold_time_freq, store_hold_time_freq);

show_gov_data_attr(hold_time_llc)
store_gov_data_attr(hold_time_llc, 1U, 200000U)
static DEVICE_ATTR(hold_time_llc, 0644, show_hold_time_llc, store_hold_time_llc);

show_gov_data_attr(llc_on_th)
store_gov_data_attr(llc_on_th, 0U, 4206000U)
static DEVICE_ATTR(llc_on_th, 0644, show_llc_on_th, store_llc_on_th);

show_gov_data_attr(llc_off_th)
store_gov_data_attr(llc_off_th, 0U, 4206000U)
static DEVICE_ATTR(llc_off_th, 0644, show_llc_off_th, store_llc_off_th);

show_gov_data_attr(polling_ms_min)
store_gov_data_attr(polling_ms_min, 4U, 1024U)
static DEVICE_ATTR(polling_ms_min, 0644,
	show_polling_ms_min, store_polling_ms_min);

show_gov_data_attr(polling_ms_max)
store_gov_data_attr(polling_ms_max, 4U, 1024U)
static DEVICE_ATTR(polling_ms_max, 0644,
	show_polling_ms_max, store_polling_ms_max);

show_gov_data_attr(efficient_freq)
store_gov_data_attr(efficient_freq, 0U, 4206000U)
static DEVICE_ATTR(efficient_freq, 0644,
	show_efficient_freq, store_efficient_freq);

show_gov_data_attr(trace_on)
store_gov_data_attr(trace_on, 0U, 1U)
static DEVICE_ATTR(trace_on, 0644, show_trace_on, store_trace_on);

show_gov_data_attr(force_mode)
store_gov_data_attr(force_mode, 0U, 1U)
static DEVICE_ATTR(force_mode, 0644, show_force_mode, store_force_mode);

show_gov_data_attr(governor_freq)
static DEVICE_ATTR(governor_freq, 0440, show_governor_freq, NULL);

show_gov_data_attr(governor_llc)
static DEVICE_ATTR(governor_llc, 0440, show_governor_llc, NULL);

static struct attribute *lealt_gov_attr[] = {
	&dev_attr_alt_target_load.attr,
	&dev_attr_polling_ms_min.attr,
	&dev_attr_polling_ms_max.attr,
	&dev_attr_mif_bw_util.attr,
	&dev_attr_mif_bus_width.attr,
	&dev_attr_hold_time_freq.attr,
	&dev_attr_hold_time_llc.attr,
	&dev_attr_llc_on_th.attr,
	&dev_attr_llc_off_th.attr,
	&dev_attr_efficient_freq.attr,
	&dev_attr_trace_on.attr,
	&dev_attr_force_mode.attr,
	&dev_attr_governor_freq.attr,
	&dev_attr_governor_llc.attr,
	NULL,
};

static struct attribute_group lealt_gov_attr_group = {
	.name = "attr",
	.attrs = lealt_gov_attr,
};

#if IS_ENABLED(CONFIG_SCHED_EMS)
static int lealt_emstune_notifier_call(struct notifier_block *nb,
                                unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct lealt_gov_data *gov_data
		= container_of(nb, struct lealt_gov_data, emstune_notifier);
	unsigned long flags;

	gov_data->ems_mode = cur_set->mode;
	if(gov_data->trace_on)
		trace_clock_set_rate("LEALT: ems_mode",
			gov_data->ems_mode,
			raw_smp_processor_id());

	spin_lock_irqsave(&gov_data->work_control_lock, flags);
	init_cpufreq_stability_all();
	lealt_set_polling_ms(gov_data, gov_data->polling_ms_max);
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);

	return NOTIFY_OK;
}
#endif

static void lealt_monitor_work(struct work_struct *work)
{
	struct lealt_gov_data *gov_data =
		container_of(work, struct lealt_gov_data, work);
	unsigned long flags, freq;

	if (gov_data->trace_on)
		trace_clock_set_rate("LEALT: monitor_work_on", 1,
			raw_smp_processor_id());

	spin_lock_irqsave(&gov_data->work_control_lock, flags);
	init_cpufreq_stability_all();
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);

	mutex_lock(&gov_data->lock);
	freq = lealt_get_freq(gov_data);
	mutex_unlock(&gov_data->lock);

	if (gov_data->governor_freq == freq)
		goto out;

	exynos_pm_qos_update_request(&gov_data->pm_qos_min, freq);
	gov_data->governor_freq = freq;

out:
	if (gov_data->trace_on) {
		trace_clock_set_rate("LEALT: monitor_work_on", 0,
			raw_smp_processor_id());
		trace_clock_set_rate("LEALT: GovernorFreq",
			gov_data->governor_freq,
			raw_smp_processor_id());
	}
}

static void init_cpufreq_stability(struct cpufreq_stability *stability,
				    unsigned int initial_freq)
{
	stability->freq_cur = initial_freq;
	stability->freq_high = initial_freq;
	stability->freq_low = initial_freq;
	stability->stability = 100;
}

static void init_cpufreq_stability_all(void)
{
	struct cpufreq_stability *stability;

	list_for_each_entry(stability, &cpufreq_stability_list, node) {
		init_cpufreq_stability(stability, stability->freq_cur);
	}
}

static int is_cpufreq_idle(void)
{
	struct cpufreq_stability *stability;

	if (list_empty(&cpufreq_stability_list))
		return 0;

	list_for_each_entry(stability, &cpufreq_stability_list, node) {
		if (stability->freq_high > stability->min_freq)
			return 0;
	}

	return 1;
}

static void lealt_set_polling_ms(struct lealt_gov_data *gov_data,
				 unsigned int new_ms)
{
	unsigned long now = jiffies;
	unsigned long target_next = gov_data->last_work_time +
		msecs_to_jiffies(gov_data->polling_ms_min);

	/* capped by min max */
	new_ms = new_ms < gov_data->polling_ms_min ?
		gov_data->polling_ms_min : new_ms;

	new_ms = new_ms > gov_data->polling_ms_max ?
		gov_data->polling_ms_max : new_ms;

	gov_data->polling_ms = new_ms;

	/* Get max */
	target_next = target_next > now ? target_next : now;

	/* Pull-in next expires */
	if (target_next < gov_data->expires) {
		gov_data->expires = target_next;
	}

	return;
}

#if IS_ENABLED(CONFIG_EXYNOS_CPUFREQ)
static int alloc_cpufreq_stability(int domain_id)
{
	struct cpufreq_stability *stability;
	u32 min_freq;
	int ret = 0;

	ret = exynos_cpufreq_get_minfreq(domain_id, &min_freq);
	if (ret) {
		pr_err("%s: failed to get min_freq\n", __func__);
		return ret;
	}

	/* Init and Add */
	stability = kzalloc(sizeof(struct cpufreq_stability), GFP_ATOMIC);
	if (!stability)
		return -ENOMEM;

	stability->domain_id = domain_id;
	stability->min_freq = min_freq;
	stability->threshold = lealt_mon_get_stability(domain_id);
	init_cpufreq_stability(stability, 0);
	list_add_tail(&stability->node, &cpufreq_stability_list);

	return ret;
}

static enum cpufreq_stability_state
update_cpufreq_stability(struct lealt_gov_data *gov_data,
		struct cpufreq_stability *stability, unsigned int new_freq)
{
	int ret;

	if (!stability->threshold)
		return CPUFREQ_STABILITY_NORMAL;

	stability->freq_cur = new_freq;

	if (new_freq > stability->freq_high)
		stability->freq_high = new_freq;
	if (new_freq < stability->freq_low)
		stability->freq_low = new_freq;

	stability->stability = 100 -
		(100 * (stability->freq_high - stability->freq_low))
		/ stability->freq_high;

	ret = stability->stability > stability->threshold ?
		CPUFREQ_STABILITY_NORMAL :
		CPUFREQ_STABILITY_ALERT;

	return ret;
}

static int devfreq_trans_notifier(struct notifier_block *nb,
					  unsigned long val, void *data)
{
	struct lealt_gov_data *gov_data =
		container_of(nb, struct lealt_gov_data, devfreq_trans_nb);
	struct exynos_devfreq_freqs *freqs = data;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&gov_data->time_in_state_lock, flags);

	ret = lealt_update_time_in_state(gov_data, freqs->old_lv, freqs->old_freq);
	if (ret)
		goto unlock_out;

	gov_data->freqs = *freqs;

unlock_out:
	spin_unlock_irqrestore(&gov_data->time_in_state_lock, flags);

	return NOTIFY_OK;
}

static int cpufreq_trans_notifier(struct notifier_block *nb,
					  unsigned long val, void *data)
{
	struct lealt_gov_data *gov_data =
		container_of(nb, struct lealt_gov_data, cpufreq_trans_nb);
	unsigned int domain_id = *(unsigned int *)data;
	unsigned int new_freq = val;
	struct cpufreq_stability *stability;
	enum cpufreq_stability_state stability_ret = CPUFREQ_STABILITY_NORMAL;
	unsigned long flags;

	spin_lock_irqsave(&gov_data->work_control_lock, flags);
	list_for_each_entry(stability, &cpufreq_stability_list, node) {
		if (stability->domain_id == domain_id) {
			break;
		}
	}

	if (list_entry_is_head(stability, &cpufreq_stability_list, node)) {
		alloc_cpufreq_stability(domain_id);
		goto out;
	}

	/* Update */
	stability_ret = update_cpufreq_stability(gov_data, stability, new_freq);

	if (stability_ret == CPUFREQ_STABILITY_ALERT) {
		lealt_set_polling_ms(gov_data, gov_data->polling_ms_min);
		if (gov_data->trace_on)
			trace_lealt_alert_stability(
				stability_ret,
				stability->stability,
				stability->domain_id,
				stability->freq_cur,
				stability->freq_high,
				stability->freq_low);
	}

out:
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
	return NOTIFY_OK;
}
#endif

/* get frequency and delay time data from string */
static unsigned int *get_tokenized_data(const char *buf, int *num_tokens)
{
	const char *cp;
	int i;
	int ntokens = 1;
	unsigned int *tokenized_data;
	int err = -EINVAL;

	cp = buf;
	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!(ntokens & 0x1))
		goto err;

	tokenized_data = kmalloc(ntokens * sizeof(unsigned int), GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	*num_tokens = ntokens;
	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

static int gov_parse_dt(struct platform_device *pdev,
			struct lealt_gov_data *gov_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 polling_minmax[2];
	u32 val;
	const char *buf;
	struct platform_device *devfreq_pdev;

	if (of_property_read_string(np, "polling-cpus", &buf)) {
		dev_err(&pdev->dev, "No cpumask specified\n");
		buf = "0-1";
	}

	cpulist_parse(buf, &gov_data->polling_cpus);
	cpumask_and(&gov_data->polling_cpus, &gov_data->polling_cpus,
		    cpu_online_mask);
	if (cpumask_weight(&gov_data->polling_cpus) == 0) {
		dev_err(&pdev->dev, "No possible cpus\n");
		cpumask_copy(&gov_data->polling_cpus, cpu_online_mask);
	}

	if (!of_property_read_u32_array(np, "polling_ms",
		(u32 *)&polling_minmax, (size_t)(ARRAY_SIZE(polling_minmax)))) {
		gov_data->polling_ms_min = polling_minmax[0];
		gov_data->polling_ms_max = polling_minmax[1];
		gov_data->polling_ms = gov_data->polling_ms_min;
	}

	if (!of_property_read_u32(np, "mif_bw_util", &val)) {
		gov_data->mif_bw_util = val;
	} else {
		dev_err(&pdev->dev, "There is no mif_bw_util\n");
	}

	if (!of_property_read_u32(np, "mif_bus_width", &val)) {
		gov_data->mif_bus_width = val;
	} else {
		dev_err(&pdev->dev, "There is no mif_bus_width\n");
	}

	if (!of_property_read_u32(np, "hold_time_freq", &val)) {
		gov_data->hold_time_freq = val;
	} else {
		dev_err(&pdev->dev, "There is no hold_time_freq\n");
	}

	if (!of_property_read_u32(np, "hold_time_llc", &val)) {
		gov_data->hold_time_llc = val;
	} else {
		dev_err(&pdev->dev, "There is no hold_time_llc\n");
	}

	if (!of_property_read_u32(np, "llc_on_th", &val)) {
		gov_data->llc_on_th = val;
	} else {
		dev_err(&pdev->dev, "There is no llc_on_th\n");
	}

	if (!of_property_read_u32(np, "llc_off_th", &val)) {
		gov_data->llc_off_th = val;
	} else {
		dev_err(&pdev->dev, "There is no llc_off_th\n");
	}

	if (!of_property_read_u32(np, "efficient_freq", &val)) {
		gov_data->efficient_freq = val;
	} else {
		dev_err(&pdev->dev, "There is no efficient_freq\n");
	}

	if (!of_property_read_string(np, "target_load", &buf)) {
		/* Parse target load table */
		int ntokens;
		gov_data->alt_target_load = get_tokenized_data(buf, &ntokens);
		gov_data->alt_num_target_load = ntokens;
	}

	np = of_parse_phandle(pdev->dev.of_node, "target-dev", 0);
	if (!np) {
		dev_err(&pdev->dev, "Unable to find target-dev\n");
		return -ENODEV;
	}

	devfreq_pdev = of_find_device_by_node(np);
	gov_data->devfreq_dev = &devfreq_pdev->dev;

	return 0;
}
static int lealt_cpupm_notifier(struct notifier_block *self,
					unsigned long cmd, void *v)
{
	struct lealt_gov_data *gov_data =
		container_of(self, struct lealt_gov_data, cpupm_nb);
	switch (cmd) {
	case DSUPD_ENTER:
	case SICD_ENTER:
		if(gov_data->trace_on)
			trace_clock_set_rate("LEALT: SICD on",
				1,
				raw_smp_processor_id());
		break;
	case DSUPD_EXIT:
	case SICD_EXIT:
		if(gov_data->trace_on)
			trace_clock_set_rate("LEALT: SICD on",
				0,
				raw_smp_processor_id());
		break;
	}
	return NOTIFY_DONE;
}

static void lealt_control_mifgov_work_func(struct work_struct *work)
{
	struct lealt_gov_data *gov_data =
		container_of(work, struct lealt_gov_data, mifgov_work);

	exynos_mifgov_run(gov_data->control_mifgov, "LEALT");
}

static void lealt_control_mifgov(struct lealt_gov_data *gov_data, bool control)
{
	int cpu = raw_smp_processor_id();

	if (gov_data->control_mifgov == control)
		return;

	gov_data->control_mifgov = control;
	queue_work_on(cpu, lealt_wq, &gov_data->mifgov_work);
}

static void lealt_deferrable_timer_handler(struct timer_list *timer)
{
	int cpu = raw_smp_processor_id();
	struct lealt_gov_data *gov_data =
		container_of(timer, struct lealt_gov_data, deferrable_timer[cpu]);
	unsigned long now = jiffies;
	unsigned long flags;
	int ret;

	ret = spin_trylock_irqsave(&gov_data->work_control_lock, flags);
	if (!ret)
		goto out;

	if (now < gov_data->expires)
		goto unlock_out;

	if (!gov_data->work_started)
		goto unlock_out;

	if (is_cpufreq_idle()
	    && gov_data->governor_freq <= gov_data->devfreq_infos.min_freq) {
		lealt_control_mifgov(gov_data, false);
		goto unlock_out;
	}

	lealt_control_mifgov(gov_data, true);
	queue_work_on(cpu, lealt_wq, &gov_data->work);
	//trace_lealt_queue_work(gov_data->polling_ms);

	gov_data->expires = now +
		msecs_to_jiffies(gov_data->polling_ms);
	gov_data->last_work_time = now;
	if(gov_data->trace_on)
		trace_lealt_queue_work(gov_data->polling_ms);
	/* double polling ms */
	gov_data->polling_ms = gov_data->polling_ms << 1;
	if (gov_data->polling_ms > gov_data->polling_ms_max)
		gov_data->polling_ms = gov_data->polling_ms_max;
unlock_out:
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);
out:
	/* Re-arm deferrable timer */
	mod_timer(timer, now + msecs_to_jiffies(10));
}

static int gov_monitor_initialize(struct lealt_gov_data *gov_data)
{
	int cpu;

	if (!gov_data->polling_ms) {
		dev_err(gov_data->dev, "No polling period\n");
		return -EINVAL;
	}

	lealt_wq = alloc_workqueue("lealt_wq",
			__WQ_LEGACY | WQ_FREEZABLE | WQ_MEM_RECLAIM, 1);
	if (!lealt_wq) {
		dev_err(gov_data->dev, "Couldn't create lealt workqueue.\n");
		return -ENOMEM;
	}

	INIT_WORK(&gov_data->work, &lealt_monitor_work);
	INIT_WORK(&gov_data->mifgov_work, &lealt_control_mifgov_work_func);
	spin_lock_init(&gov_data->work_control_lock);

	for_each_cpu(cpu, &gov_data->polling_cpus) {
		timer_setup(&gov_data->deferrable_timer[cpu],
			lealt_deferrable_timer_handler,
			TIMER_DEFERRABLE | TIMER_PINNED);
		gov_data->deferrable_timer[cpu].expires = jiffies +
			msecs_to_jiffies(10);
		add_timer_on(&gov_data->deferrable_timer[cpu], cpu);
	}

	dev_info(gov_data->dev,"LEALT work initialized with period : %u\n",
		 gov_data->polling_ms);
	return 0;
}

static int lealt_gov_cpuhp_up(unsigned int cpu)
{
	if (!(cpumask_test_cpu(cpu, &gov_data_global->polling_cpus) && cpu_online(cpu)))
		return 0;

	gov_data_global->deferrable_timer[cpu].expires =
		jiffies + msecs_to_jiffies(10);
	add_timer_on(&gov_data_global->deferrable_timer[cpu], cpu);

	return 0;
}

static int lealt_gov_cpuhp_down(unsigned int cpu)
{
	if (!(cpumask_test_cpu(cpu, &gov_data_global->polling_cpus) && cpu_online(cpu)))
		return 0;

	del_timer_sync(&gov_data_global->deferrable_timer[cpu]);

	return 0;
}

static int lealt_gov_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct lealt_gov_data *gov_data;

	gov_data = devm_kzalloc(&pdev->dev,
			 sizeof(struct lealt_gov_data), GFP_KERNEL);
	if (!gov_data)
		return -ENOMEM;

	ret = gov_parse_dt(pdev, gov_data);
	if (ret)
		goto out;

	gov_data->dev = &pdev->dev;

	mutex_init(&gov_data->lock);
	spin_lock_init(&gov_data->time_in_state_lock);

	ret = sysfs_create_group(&pdev->dev.kobj, &lealt_gov_attr_group);
	if (ret)
		goto out;

	ret = gov_monitor_initialize(gov_data);
	if (ret)
		goto out;

	exynos_wow_get_meta(&gov_data->wow_meta);

	ret = exynos_devfreq_get_freq_infos(gov_data->devfreq_dev,
				      &gov_data->devfreq_infos);
	if (ret)
		goto out;

	gov_data->time_in_state = devm_kzalloc(&pdev->dev, sizeof(u64)
			* gov_data->devfreq_infos.max_state, GFP_KERNEL);

	exynos_pm_qos_add_request(&gov_data->pm_qos_min,
			  (int)gov_data->devfreq_infos.pm_qos_class, 0);

	gov_data_global = gov_data;

	smp_store_release(&gov_data->work_started, true);

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "lealt-gov",
				lealt_gov_cpuhp_up, lealt_gov_cpuhp_down);

	gov_data->devfreq_trans_nb.notifier_call = devfreq_trans_notifier;
	exynos_devfreq_register_trans_notifier(gov_data->devfreq_dev,
					       &gov_data->devfreq_trans_nb);

#if IS_ENABLED(CONFIG_EXYNOS_CPUFREQ)
	gov_data->cpufreq_trans_nb.notifier_call
				= cpufreq_trans_notifier;
	exynos_cpufreq_register_transition_notifier(
					&gov_data->cpufreq_trans_nb);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	gov_data->cpupm_nb.notifier_call = lealt_cpupm_notifier;
	exynos_cpupm_notifier_register(&gov_data->cpupm_nb);
#endif

#if IS_ENABLED(CONFIG_SCHED_EMS)
	gov_data->emstune_notifier.notifier_call = lealt_emstune_notifier_call;
	emstune_register_notifier(&gov_data->emstune_notifier);
#endif

	platform_set_drvdata(pdev, gov_data);

out:
	return ret;
}

static int lealt_gov_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lealt_gov_data *gov_data = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&gov_data->work_control_lock, flags);
	gov_data->work_started = false;
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);

	return 0;
}

static int lealt_gov_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct lealt_gov_data *gov_data = platform_get_drvdata(pdev);
	unsigned long flags;

	spin_lock_irqsave(&gov_data->work_control_lock, flags);
	init_cpufreq_stability_all();
	gov_data->work_started = true;
	lealt_set_polling_ms(gov_data, gov_data->polling_ms_max);
	spin_unlock_irqrestore(&gov_data->work_control_lock, flags);

	return 0;
}

static SIMPLE_DEV_PM_OPS(lealt_gov_pm, lealt_gov_suspend, lealt_gov_resume);

static const struct of_device_id lealt_gov_match[] = {
	{ .compatible = "samsung,lealt-gov", },
	{},
};

static struct platform_driver lealt_gov_driver = {
	.probe = lealt_gov_probe,
	.driver = {
		.name = "lealt-gov",
		.pm = &lealt_gov_pm,
		.owner = THIS_MODULE,
		.of_match_table = lealt_gov_match,
	},
};
module_platform_driver(lealt_gov_driver);

MODULE_LICENSE("GPL v2");
