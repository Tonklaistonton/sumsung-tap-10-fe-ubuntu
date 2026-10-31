// SPDX-License-Identifier: GPL-2.0-only
/*
 * @file sgpu_devfreq.h
 * Copyright (C) 2024 Samsung Electronics
 */

#ifndef _SGPU_DEVFREQ_H_
#define _SGPU_DEVFREQ_H_

#include <linux/devfreq.h>
#include "sgpu_utilization.h"

struct sgpu_devfreq_data {
	struct devfreq			*devfreq;
	struct amdgpu_device		*adev;

	struct notifier_block           nb_trans;

	struct dev_pm_qos_request	sys_pm_qos_min;
	struct dev_pm_qos_request	sys_pm_qos_max;

	/* msec */
	unsigned int			valid_time;

	/* current state */
	unsigned long			min_freq;
	unsigned long			max_freq;
	int				current_level;

	u32				gvf_start_level;

	/* cl_boost */
	bool				cl_boost_disable;
	bool				cl_boost_status;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	int				dm_type;
	unsigned long			old_freq;
#endif /* CONFIG_EXYNOS_ESCA_DVFS_MANAGER */

	/* utilization data */
	struct utilization_data		udata;
};

void sgpu_devfreq_request_freq(struct amdgpu_device *adev, int dm_type,
			       unsigned long cur_freq, unsigned long *target_freq);
int sgpu_devfreq_init(struct amdgpu_device *adev);
int sgpu_devfreq_data_init(struct amdgpu_device *adev, struct devfreq *df);
void sgpu_devfreq_data_deinit(struct devfreq *df);
ssize_t sgpu_devfreq_time_in_state_show(struct amdgpu_device *adev, char *buf);
void sgpu_devfreq_time_in_state_update(struct amdgpu_device *adev, uint64_t diff_time);
void sgpu_devfreq_time_in_state_freq_change(struct amdgpu_device *adev);
#endif
