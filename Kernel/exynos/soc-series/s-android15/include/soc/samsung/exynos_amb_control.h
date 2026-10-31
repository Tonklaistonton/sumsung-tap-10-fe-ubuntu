
/*
 * exynos_amb_control.c - Samsung AMB control (Ambient Thermal Control module)
 *
 *  Copyright (C) 2021 Samsung Electronics
 *  Hanjun Shin <hanjun.shin@samsung.com>
 *  Youngjin Lee <youngjin0.lee@samsung.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/threads.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include <soc/samsung/exynos-cpuhp.h>
#include <uapi/linux/sched/types.h>
#include <soc/samsung/exynos_pm_qos.h>
#define AMB_TZ_NUM	(5)

#define DEFAULT_SAMPLING_RATE	1000
#define HIGH_SAMPLING_RATE	100
#define AMB_SWITCH_ON_TEMP	55000
#define MIF_HOT_SWITCH_ON_TEMP	50000
#define HOTPLUG_IN_THRESHOLD	60000
#define HOTPLUG_OUT_THRESHOLD	65000
#define MCINFO_THRESHOLD_TEMP	60000

#define MIF_DOWN_THRESHOLD_TEMP		65000
#define MIF_UP_THRESHOLD_TEMP		60000
#define MIF_THROTTLE_FREQ		2028000

#define MIF_HOT_OFF_THRESHOLD_TEMP	101000
#define MIF_HOT_ON_THRESHOLD_TEMP	95000
#define MIF_HOT_THROTTLE_FREQ		4780000

#define TRIGGER_COND_HIGH	0
#define TRIGGER_COND_LOW	1
#define TRIGGER_COND_NOTUSED	2

struct ambient_thermal_zone_config {
	int hotplug_in_threshold;
	int hotplug_out_threshold;
	bool is_cpu_hotplugged_out;
	int hotplug_enable;
	struct cpumask cpu_domain;
};

struct exynos_amb_control_data {
	struct thermal_zone_device *amb_tzd;
	struct thermal_zone_device *mif_tzd;

	unsigned int num_amb_tz_configs;
	struct ambient_thermal_zone_config *amb_tz_config;
	struct cpumask cpuhp_req_mask;
	struct mutex lock;
	struct kthread_delayed_work amb_dwork;
	struct kthread_worker amb_worker;
	struct notifier_block pm_notify;

	unsigned int default_sampling_rate;
	unsigned int high_sampling_rate;
	unsigned int period_amb;
	int amb_switch_on;
	int mcinfo_threshold;

	bool use_mif_throttle;
	int mif_up_threshold;
	int mif_down_threshold;
	int mif_throttle_freq;
	struct exynos_pm_qos_request amb_mif_qos;

	int mif_temp;
	int mif_hot_switch_on;
	int mif_hot_off_threshold;
	int mif_hot_on_threshold;
	u32 mif_hot_throttle_freq;
	struct exynos_pm_qos_request mif_hot_qos;
	unsigned long last_update_time;
	unsigned int period_mif;

	int emul_temp;

	bool in_suspend;
	bool amb_disabled;
	bool hotplug_completely_off;
};
