// SPDX-License-Identifier: GPL-2.0-only
/*
 * @file sgpu_gvf.h
 * @copyright 2024 Samsung Electronics
 */

#ifndef __SGPU_GVF_H__
#define __SGPU_GVF_H__

#include <linux/completion.h>
#include <linux/devfreq.h>
#include "sgpu_gvf_governor.h"

#define SGPU_GVF_SAMPLING_TIME_MS	30
#define SGPU_GVF_MAX_REST_RATIO		60
#define SGPU_GVF_MIN_REST_RATIO		20

enum sgpu_gvf_params {
	SGPU_GVF_PARAM_SAMPLING_TIME_MS,
	SGPU_GVF_PARAM_TARGET_RATIO,
	SGPU_GVF_PARAM_MAX_REST_RATIO,
	SGPU_GVF_PARAM_MIN_REST_RATIO,

	SGPU_GVF_PARAM_NR
};

struct sgpu_gvf {
	bool				enable;
	bool				initialized;

	uint32_t			*table;
	uint32_t			max_level;
	uint32_t			level;
	uint32_t			run_freq;

	wait_queue_head_t		waitq;
	struct task_struct		*thread;
	struct timer_list		wakeup_timer;
	unsigned long			expire_jiffies;

	/* Common params */
	uint64_t			base_time_ns;
	uint64_t			base_idle_time_ns;
	uint32_t			monitor_window_ms;
	uint32_t			sampling_time_ms;

	/* utils */
	struct sgpu_gvf_governor	*governor;
	struct sgpu_gvf_injector	*injector;

	struct mutex			lock;

#ifdef CONFIG_DEBUG_FS
	struct dentry			*debugfs_start;
	struct dentry			*debugfs_lock_freq;
	struct dentry			*debugfs_monitor_window_ms;
	struct dentry			*debugfs_params;
#endif
};

#if IS_ENABLED(CONFIG_DRM_SGPU_GVF)
int sgpu_gvf_init(struct amdgpu_device *adev);
void sgpu_gvf_fini(struct amdgpu_device *adev);
void sgpu_gvf_set_level(struct amdgpu_device *adev, uint32_t level);
void sgpu_gvf_resume(struct amdgpu_device *adev);
void sgpu_gvf_suspend(struct amdgpu_device *adev);
uint32_t sgpu_gvf_get_param(uint32_t level, uint32_t param);
void sgpu_gvf_set_param(uint32_t level, uint32_t param, uint32_t val);
uint32_t sgpu_gvf_get_max_level(struct amdgpu_device *adev);
uint32_t sgpu_gvf_get_run_freq(struct amdgpu_device *adev);
int sgpu_debugfs_gvf_init(struct amdgpu_device *adev);
#else
#define sgpu_gvf_init(adev)				(0)
#define sgpu_gvf_fini(adev)				do {} while (0)
#define sgpu_gvf_set_level(adev, level)			do {} while (0)
#define sgpu_gvf_resume(adev)				do {} while (0)
#define sgpu_gvf_suspend(adev)				do {} while (0)
#define sgpu_gvf_get_param(level, param)		(0)
#define sgpu_gvf_set_param(level, param, val)		do {} while (0)
#define sgpu_gvf_get_max_level(adev)			(0)
#define sgpu_gvf_get_run_freq(adev)			(0)
#define sgpu_debugfs_gvf_init(adev)			(0)
#endif /* CONFIG_DRM_SGPU_GVF */

#endif /* __SGPU_GVF_H__ */
