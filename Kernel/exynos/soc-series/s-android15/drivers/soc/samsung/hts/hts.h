/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_H_
#define _HTS_H_

#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/plist.h>
#include <linux/cpumask.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/kernel_stat.h>
#include <linux/notifier.h>
#include <linux/atomic/atomic-instrumented.h>

#define MAX_GROUP_ENTRIES_COUNT		(4)
#define MAX_CGROUP_ENTRIES_COUNT	(10)

enum hts_mode {
	MODE_NONE = -1,
	MODE_APPLAUNCH,
	MODE_COUNT
};

enum hts_knob {
	KNOB_CTRL1,
	KNOB_CTRL2,
	KNOB_COUNT
};

enum hts_freq {
	FREQ_NONE = -1,
	FREQ_CPU,
	FREQ_DSU,
	FREQ_MIF,
	FREQ_COUNT
};

struct hts_config {
	unsigned long			value[MAX_GROUP_ENTRIES_COUNT][KNOB_COUNT];
};

struct hts_mmap {
	atomic_t			ref_count;

	struct page			*buffer_page;
	unsigned long			*buffer_event;
	int				buffer_size;
};

struct hts_notifier_ems {
	int				prev_mode;
	struct notifier_block		noti_block;
};

struct hts_notifier_cpu {
	ktime_t				prev_time[MAX_GROUP_ENTRIES_COUNT];
	unsigned int			freq[MAX_GROUP_ENTRIES_COUNT];
};

struct hts_notifier_dsu {
	ktime_t				prev_time;
	unsigned int			freq;

	struct notifier_block		noti_block;
};

struct hts_notifier_devfreq {
	ktime_t				prev_time;
	unsigned int			freq;

	struct notifier_block		noti_block;
};

struct hts_notifier {
	struct hts_notifier_ems		ems;
	struct hts_notifier_cpu		cpu;
	struct hts_notifier_dsu		dsu;
	struct hts_notifier_devfreq	mif;

	spinlock_t			lock;
	struct hts_mmap			mmap;
};

struct hts_sysfs {
	struct device_attribute		enable;

	struct device_attribute		log_mask;
	struct device_attribute		ref_mask;
	struct device_attribute		def_mask;
	struct device_attribute		backup;
	struct device_attribute		core_thre;
	struct device_attribute		total_thre;
	struct device_attribute		group_mask;
	struct device_attribute		enable_mask;
	struct device_attribute		cgroup;

	struct device_attribute		extended_control;
	struct device_attribute		extended_control2;

	struct device_attribute		transition;
	struct device_attribute		written;
};

struct hts_percpu {
	unsigned long			backup[CONFIG_VENDOR_NR_CPUS][KNOB_COUNT];
	unsigned long			predefined[MODE_COUNT][MAX_GROUP_ENTRIES_COUNT][KNOB_COUNT];
	struct kernel_cpustat		core_util[CONFIG_VENDOR_NR_CPUS];
	unsigned long			transition_count[CONFIG_VENDOR_NR_CPUS];
	unsigned long			written_count[CONFIG_VENDOR_NR_CPUS][KNOB_COUNT];
};

struct hts_devfs {
	struct file_operations		fops;
	struct miscdevice		miscdev;

	int				enabled;
	int				ems_mode;

	struct workqueue_struct 	*wq;
	struct delayed_work		work;
	unsigned long			eval_tick_ms;

	wait_queue_head_t		wait_queue;
	int				wakeup_waiter;

	cpumask_t			log_mask;
	cpumask_t			ref_mask;
	cpumask_t			def_mask;

	unsigned long			core_active_thre;
	unsigned long			total_active_thre;

	atomic_t			predefined_mode;

	int				available_cpu[MAX_GROUP_ENTRIES_COUNT][KNOB_COUNT];
	cpumask_t			mask_cpu[MAX_GROUP_ENTRIES_COUNT];
	int				mask_count;

	int				target_cgroup[MAX_CGROUP_ENTRIES_COUNT];
	int				cgroup_count;

	struct mutex			ioctl_lock;
};

struct hts_etc_data {
	int				probed;
	int				enabled_count;
};

struct hts_drvdata {
	struct platform_device		 *pdev;
	struct hts_devfs		devfs;
	struct hts_sysfs		sysfs;

	struct hts_percpu		percpu;

	struct hts_notifier		notifier;

	struct hts_etc_data		etc;
};

#endif /* _HTS_H_ */
