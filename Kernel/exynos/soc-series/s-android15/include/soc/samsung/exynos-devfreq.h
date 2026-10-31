/* linux/arch/arm64/mach-exynos/include/mach/exynos-devfreq.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXYNOS_DEVFREQ_H_
#define __EXYNOS_DEVFREQ_H_

#include <linux/devfreq.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/clk.h>
#include <soc/samsung/exynos-dm.h>

#include <soc/samsung/exynos-wow.h>
#include <soc/samsung/cal-if.h>

#define EXYNOS_DEVFREQ_MODULE_NAME	"exynos-devfreq"
#define VOLT_STEP			25000
#define MAX_NR_CONSTRAINT		DM_TYPE_END
#define DATA_INIT			5
#define SET_CONST			1
#define RELEASE				2
#define KHZ				(1000)

#define DEVFREQ_MIF                     0
#define DEVFREQ_INT                     1

struct devfreq_notifier_block {
       struct notifier_block nb;
       struct devfreq *df;
};

struct exynos_devfreq_freqs {
	unsigned long new_freq;
	unsigned long old_freq;
	u32 new_lv;
	u32 old_lv;
};

struct exynos_devfreq_opp_table {
	u32 idx;
	u32 freq;
	u32 volt;
};

struct exynos_devfreq_freq_infos {
	/* Basic freq infos */
	// min/max/cur frequency
	u32 max_freq;
	u32 min_freq;
	u32 cur_freq;
	// min/max pm_qos node
	u32 pm_qos_class;
	u32 pm_qos_class_max;
	// num of freqs
	u32 max_state;
	u32 *freq_table;
};

struct exynos_devfreq_data {
	struct device				*dev;
	struct devfreq				*devfreq;
	struct mutex				lock;
	spinlock_t				update_status_lock;
	struct clk				*clk;

	bool					devfreq_disabled;

	u32		devfreq_type;

	struct dvfs_rate_volt			*opp_list;

	u32					default_qos;

	u32					max_state;
	struct devfreq_dev_profile		devfreq_profile;

	const char				*governor_name;
	u32					cal_qos_max;
	u32					dfs_id;
	u32					old_freq;
	u32					new_freq;
	u32					min_freq;
	u32					max_freq;
	u32					reboot_freq;
	u32					boot_freq;
	u64					suspend_freq;
	u64					suspend_req;

	u32					pm_qos_class;
	u32					pm_qos_class_max;
	struct exynos_pm_qos_request		sys_pm_qos_min;
	struct exynos_pm_qos_request		sys_pm_qos_max;
#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG)
	struct exynos_pm_qos_request		debug_pm_qos_min;
	struct exynos_pm_qos_request		debug_pm_qos_max;
#endif
	struct exynos_pm_qos_request		default_pm_qos_min;
	struct exynos_pm_qos_request		default_pm_qos_max;
	struct exynos_pm_qos_request		boot_pm_qos;
	u32					boot_qos_timeout;

	struct srcu_notifier_head		trans_nh;
	struct notifier_block			reboot_notifier;

	u32					ess_flag;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	u32		dm_type;
	u32		nr_constraint;
	struct exynos_dm_constraint		**constraint;
#endif
	void					*private_data;
	bool					use_acpm;
	bool					use_dtm;

	struct devfreq_notifier_block nb;
	struct devfreq_notifier_block nb_max;

	struct exynos_pm_domain *pm_domain;
	const char				*pd_name;
	unsigned long *time_in_state;
	unsigned long last_stat_updated;
};

#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ) || IS_ENABLED(CONFIG_ARM_EXYNOS_ESCA_DEVFREQ)
extern unsigned long exynos_devfreq_get_domain_freq(unsigned int devfreq_type);
extern int exynos_devfreq_get_freq_infos(struct device *dev,
					struct exynos_devfreq_freq_infos *infos);
extern int exynos_devfreq_governor_nop_init(void);
extern int exynos_devfreq_register_trans_notifier(struct device *dev,
					   struct notifier_block *nb);
extern int exynos_devfreq_unregister_trans_notifier(struct device *dev,
					     struct notifier_block *nb);
extern int exynos_devfreq_get_recommended_freq(struct device *dev,
					unsigned long *target_freq, u32 flags);
#else
static inline unsigned long exynos_devfreq_get_domain_freq(unsigned int devfreq_type)
{
	return 0;
}
extern inline int exynos_devfreq_get_freq_infos(struct device *dev,
					struct exynos_devfreq_freq_infos *infos)
{
	return -ENODEV;
}
static int exynos_devfreq_register_trans_notifier(struct device *dev,
					   struct notifier_block *nb);
{
	return -ENODEV;
}
static int exynos_devfreq_unregister_trans_notifier(struct device *dev,
					     struct notifier_block *nb)
{
	return -ENODEV;
}
static int exynos_devfreq_get_recommended_freq(struct device *dev,
					unsigned long *target_freq, u32 flags)
{
	return -ENODEV;
}
#endif
#endif	/* __EXYNOS_DEVFREQ_H_ */
