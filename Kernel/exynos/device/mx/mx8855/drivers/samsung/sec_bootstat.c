/* sec_bootstat.c
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/notifier.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sec_class.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/arch_topology.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <clocksource/arm_arch_timer.h>
#include "sec_bootstat.h"

#define FREQ_LEN 4

#define MAX_CLUSTERS NR_CPUS
#define MAX_CLUSTER_OUTPUT ((FREQ_LEN + 1) * MAX_CLUSTERS + 1)

#define MAX_THERMAL_ZONES 10
#define THERMAL_DNAME_LENGTH 5

#define MAX_LENGTH_OF_BOOTING_LOG 90
#define MIN_LENGTH_OF_BOOTING_LOG 15

#define MAX_EVENTS_EBS 150
#define DELAY_TIME_EBS 30000

int __read_mostly boot_time_bl1;
module_param(boot_time_bl1, int, 0440);

int __read_mostly boot_time_bl2;
module_param(boot_time_bl2, int, 0440);

int __read_mostly boot_time_bl3;
module_param(boot_time_bl3, int, 0440);

struct boot_event {
	struct list_head list;
	char string[MAX_LENGTH_OF_BOOTING_LOG];
	unsigned int time;
	int freq[MAX_CLUSTERS];
	int online;
	int temp[MAX_THERMAL_ZONES];
};

struct tz_info {
	char name[THERMAL_NAME_LENGTH];
	char display_name[THERMAL_DNAME_LENGTH];
};

static u32 arch_timer_start;

static unsigned int events_ebs;
static bool ebs_finished;

static unsigned int boot_complete_time;

static LIST_HEAD(boot_time_list);
static LIST_HEAD(enhanced_boot_time_list);

static DEFINE_MUTEX(bootstat_lock);

static int num_clusters;
static int num_cpus_cluster[MAX_CLUSTERS];

static int num_thermal_zones;
static struct tz_info thermal_zones[MAX_THERMAL_ZONES];

static void sec_bootstat_get_cpuinfo(int *freq, int *online)
{
	int cpu;

	*online = cpumask_bits(cpu_online_mask)[0];
	for_each_online_cpu(cpu)
		freq[topology_cluster_id(cpu)] = cpufreq_get(cpu);
}

static void sec_bootstat_get_thermal(int *temp)
{
	int i, ret;
	struct thermal_zone_device *tzd;

	for (i = 0; i < num_thermal_zones; i++) {
		tzd = thermal_zone_get_zone_by_name(thermal_zones[i].name);
		if (IS_ERR(tzd)) {
			pr_err("can't find thermal zone %s\n", thermal_zones[i].name);
			continue;
		}
		ret = thermal_zone_get_temp(tzd, &temp[i]);
		if (ret)
			pr_err("failed to get temperature for %s (%d)\n", tzd->type, ret);
	}
}

static void sec_enhanced_boot_stat_record(struct boot_event *entry, const char *buf)
{
	int offset = buf[10] == ':' ? 12 : 0; // !@Boot_EBS:   Took  ??ms ~
	strscpy(entry->string, buf + offset, MAX_LENGTH_OF_BOOTING_LOG);
	list_add(&entry->list, &enhanced_boot_time_list);

	if (++events_ebs >= MAX_EVENTS_EBS)
		ebs_finished = true;
}

static void sec_boot_stat_record(struct boot_event *entry, const char *buf)
{
	sec_bootstat_get_thermal(entry->temp);
	strscpy(entry->string, buf, MAX_LENGTH_OF_BOOTING_LOG);
	list_add_tail(&entry->list, &boot_time_list);

	if (!strncmp(entry->string, "!@Boot: bootcomplete", 20))
		boot_complete_time = entry->time;
}

static void sec_bootstat_add(const char *buf, size_t count)
{
	struct boot_event *entry;
	u64 t;

	// Filter strings that are not bootstat entries
	// Must be either "!@Boot_" or "!@Boot:" (do not include "!@BootAnimation", "!@Boot_SystemServer")
	if (count <= MIN_LENGTH_OF_BOOTING_LOG || strncmp(buf, "!@Boot", 6)
		|| (buf[6] != '_' && buf[6] != ':') || !strncmp(buf + 7, "SystemServer", 12))
		return;

	mutex_lock(&bootstat_lock);

	if (ebs_finished)
		goto out;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		goto out;

	t = local_clock();
	do_div(t, 1000000);
	entry->time = (unsigned int) t;

	sec_bootstat_get_cpuinfo(entry->freq, &entry->online);

	// Classify logs starting with !@Boot_EBS
	if (!strncmp(buf + 7, "EBS", 3))
		sec_enhanced_boot_stat_record(entry, buf);
	else
		sec_boot_stat_record(entry, buf);

	if (boot_complete_time && !ebs_finished)
		ebs_finished = (t - boot_complete_time) >= DELAY_TIME_EBS;

out:
	mutex_unlock(&bootstat_lock);
}

static void print_format(struct boot_event *entry, struct seq_file *m, int delta)
{
	int i, cpu, offset;
	char out[MAX_CLUSTER_OUTPUT];

	seq_printf(m, "%-50.50s %6u %6u %6d ", entry->string, entry->time + arch_timer_start,
			entry->time, delta);

	// print frequency of clusters
	offset = 0;
	for (i = 0; i < num_clusters; ++i)
		offset += sprintf(out + offset, "%-*d ", FREQ_LEN, entry->freq[i] / 1000);
	seq_printf(m, "%-*s", (int)sizeof("Frequency ") - 1, out);

	// print online cpus by cluster
	offset = 0;
	for (i = 0; i < num_clusters; i++) {
		offset += sprintf(out + offset, "[");
		for_each_possible_cpu(cpu)
			if (topology_cluster_id(cpu) == i)
				offset += sprintf(out + offset, "%d", (entry->online >> cpu) & 1);
		offset += sprintf(out + offset, "]");
	}
	offset += sprintf(out + offset, " ");
	seq_printf(m, "%-*s", (int)sizeof("Online Mask ") - 1, out);

	// print temperature of thermal zones
	for (i = 0; i < num_thermal_zones; i++)
		seq_printf(m, "[%4d]", entry->temp[i] / 1000);
	seq_putc(m, '\n');
}

static int sec_boot_stat_proc_show(struct seq_file *m, void *v)
{
	int offset;
	char out[MAX_CLUSTER_OUTPUT];
	size_t i = 0;
	unsigned int last_time = 0;
	struct boot_event *entry;

	seq_printf(m, "%71s %-*s%-*s %s\n", "", (FREQ_LEN + 1) * num_clusters, "Frequency ",
			2 * num_clusters + nr_cpu_ids, "Online Mask", "Temperature");
	seq_printf(m, "%-50s %6s %6s %6s ", "Boot Events", "time", "ktime", "delta");

	offset = 0;
	for (i = 0; i < num_clusters; i++)
		offset += sprintf(out + offset, "f_c%lu ", i);
	seq_printf(m, "%-*s", (int)sizeof("Frequency ") - 1, out);

	offset = 0;
	for (i = 0; i < num_clusters; i++)
		offset += sprintf(out + offset, "C%lu%*s", i, num_cpus_cluster[i], "");
	offset += sprintf(out + offset, " ");
	seq_printf(m, "%-*s", (int)sizeof("Online Mask ") - 1, out);

	for (i = 0; i < num_thermal_zones; i++)
		seq_printf(m, "[%4s]", thermal_zones[i].display_name);
	seq_puts(m, "\n");
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "BOOTLOADER - KERNEL\n");
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	seq_printf(m, "%-50s %6u %6u %6u\n", "MCT is initialized in bl2", 0, 0, 0);
	seq_printf(m, "%-50s %6u %6u %6u\n", "start kernel timer", arch_timer_start, 0, arch_timer_start);
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "FRAMEWORK\n");
	seq_puts(m, "--------------------------------------------------------------------------------------------------------------\n");
	list_for_each_entry(entry, &boot_time_list, list) {
		print_format(entry, m, entry->time - last_time);
		last_time = entry->time;
	}

	return 0;
}

static int sec_enhanced_boot_stat_proc_show(struct seq_file *m, void *v)
{
	int i;
	unsigned int last_time = 0;
	struct boot_event *entry;

	seq_printf(m, "%-90s %6s %6s %6s ", "Boot Events", "time", "ktime", "delta");

	for (i = 0; i < num_clusters; i++)
		seq_printf(m, "f_c%d ", (int)i);

	seq_putc(m, '\n');
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "BOOTLOADER - KERNEL\n");
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: MCT_is_initialized", 0, 0, 0);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: boot_time_bl1", boot_time_bl1, 0, boot_time_bl1);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: boot_time_bl2", boot_time_bl2, 0,
			boot_time_bl2 - boot_time_bl1);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: boot_time_bl3", boot_time_bl3, 0,
			boot_time_bl3 - boot_time_bl2);
	seq_printf(m, "%-90s %6u %6u %6u\n", "!@Boot_EBS_B: start_kernel_timer", arch_timer_start, 0,
			arch_timer_start - boot_time_bl3);
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	seq_puts(m, "FRAMEWORK\n");
	seq_puts(m, "-------------------------------------------------------------------------------------------------------------------------\n");
	list_for_each_entry_reverse(entry, &enhanced_boot_time_list, list) {
		seq_printf(m, "%-90s %6u %6u ", entry->string, entry->time + arch_timer_start, entry->time);
		if (entry->string[0] == '!') {
			seq_printf(m, "%6u ", entry->time - last_time);
			last_time = entry->time;
		} else {
			seq_printf(m, "%6s ", "");
		}
		for (i = 0; i < num_clusters; i++)
			seq_printf(m, "%4d ", entry->freq[i] / 1000);
		seq_putc(m, '\n');
	}
	return 0;
}

static int sec_boot_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_boot_stat_proc_show, NULL);
}

static int sec_enhanced_boot_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_enhanced_boot_stat_proc_show, NULL);
}

static const struct proc_ops sec_boot_stat_proc_fops = {
	.proc_open    = sec_boot_stat_proc_open,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops sec_enhanced_boot_stat_proc_fops = {
	.proc_open    = sec_enhanced_boot_stat_proc_open,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static ssize_t store_boot_stat(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	// count + 1 because count doesn't account for NULL character
	sec_bootstat_add(buf, count + 1);
	return count;
}

static DEVICE_ATTR(boot_stat, 0220, NULL, store_boot_stat);

static int bootstat_pmsg_notify(struct notifier_block *nb,
				unsigned long action, void *data)
{
	pmsg_notifier_data *nb_data = data;
	char *buf = nb_data->buffer;
	size_t count = nb_data->count;

	if(count < 2 || buf[0] != '!' || buf[1] != '@')
		return NOTIFY_DONE;

	buf[count - 1] = '\0';

	pr_info("%s\n", buf);

	sec_bootstat_add(buf, count);

	return NOTIFY_DONE;
}

static struct notifier_block bootstat_pmsg_nb = {
	.notifier_call = bootstat_pmsg_notify,
};

static int get_thermal_zones(void)
{
	int i;
	struct device_node *np, *tz;
	const char *zname, *dname;

	np = of_find_node_by_path("/sec-bootstat/thermal-zones");
	if (!np)
		goto thermal_fail;

	for_each_available_child_of_node(np, tz) {
		if (of_property_read_string(tz, "zone-name", &zname) < 0) {
			pr_err("invalid or no zone-name for thermal zone: %s\n", tz->name);
			continue;
		} else {
			strscpy(thermal_zones[num_thermal_zones].name, zname, THERMAL_NAME_LENGTH);
		}

		if (of_property_read_string(tz, "display-name", &dname) < 0) {
			pr_err("invalid or no display-name for thermal zone: %s\n", tz->name);
			strscpy(thermal_zones[num_thermal_zones].display_name, tz->name, THERMAL_DNAME_LENGTH);
		} else {
			strscpy(thermal_zones[num_thermal_zones].display_name, dname, THERMAL_DNAME_LENGTH);
		}

		++num_thermal_zones;
		if (num_thermal_zones == MAX_THERMAL_ZONES) {
			pr_info("reached maximum thermal zones\n");
			break;
		}
	}
	of_node_put(np);

	if (!num_thermal_zones)
		goto thermal_fail;

	pr_info("num_thermal_zones = %d\n", num_thermal_zones);
	for (i = 0; i < num_thermal_zones; i++)
		pr_info("thermal zone %d: %s\n", i, thermal_zones[i].name);

	return 0;

thermal_fail:
	pr_err("No thermal zones found, thermal info will not be shown in bootstat\n");
	return -EINVAL;
}

static int get_arch_timer_start(void)
{
	struct device_node *np;
	int ret;
	u32 clock_frequency;
	u64 ktimer_msec;
	u64 arch_timer_msec;

	clock_frequency = arch_timer_get_cntfrq();
	if (!clock_frequency) {
		pr_info("arch_timer_get_cntfrq failed... looking into DT for clock-frequency\n");
		np = of_find_node_by_path("/timer");
		if (!np) {
			pr_err("no timer in DT\n");
			return -ENODEV;
		}

		ret = of_property_read_u32(np, "clock-frequency", &clock_frequency);
		of_node_put(np);

		if (ret < 0) {
			pr_err("no clock-frequency found in DT\n");
			return -EINVAL;
		}
	}

	arch_timer_msec = arch_timer_read_counter();
	do_div(arch_timer_msec, (clock_frequency / 1000));

	ktimer_msec = local_clock();
	do_div(ktimer_msec, 1000000);

	arch_timer_start = (u32)(arch_timer_msec - ktimer_msec);

	return 0;
}

static int get_num_clusters(void)
{
	int cpu;
	int cluster;
	int max_cluster_id = -1;

	for_each_possible_cpu(cpu) {
		cluster = topology_cluster_id(cpu);
		num_cpus_cluster[cluster]++;
		max_cluster_id = (max_cluster_id < cluster ? cluster : max_cluster_id);
	}

	if (max_cluster_id == -1) {
		pr_err("no cluster found\n");
		return -EINVAL;
	}

	num_clusters = max_cluster_id + 1; // +1 of max_cluster_id

	pr_info("num_clusters = %d\n", num_clusters);

	return 0;
}

static int __init_or_module sec_bootstat_init(void)
{
	struct proc_dir_entry *entry;
	struct device *dev;

	if (get_num_clusters() < 0)
		return -EINVAL;

	if (get_arch_timer_start() < 0)
		return -EINVAL;

	get_thermal_zones();

	// proc
	entry = proc_create("boot_stat", 0444, NULL, &sec_boot_stat_proc_fops);
	if (!entry) {
		pr_err("Failed to create /proc/boot_stat");
		return -ENOMEM;
	}

	entry = proc_create("enhanced_boot_stat", 0444, NULL, &sec_enhanced_boot_stat_proc_fops);
	if (!entry) {
		pr_err("Failed to create /proc/enhanced_boot_stat");
		goto fail1;
	}

	// sysfs
	dev = sec_device_create(NULL, "bsp");
	if (IS_ERR(dev)) {
		pr_err("%s: Failed to create device\n", __func__);
		goto fail2;
	}

	if (device_create_file(dev, &dev_attr_boot_stat) < 0)
		pr_err("%s: Failed to create device file, some boot entries will not be shown\n", __func__);

	pmsg_chain_register(&bootstat_pmsg_nb);

	return 0;

 fail2: remove_proc_entry("enhanced_boot_stat", NULL);
 fail1: remove_proc_entry("boot_stat", NULL);
	return -ENOMEM;
}

module_init(sec_bootstat_init);

MODULE_DESCRIPTION("Samsung boot-stat driver");
MODULE_LICENSE("GPL v2");

