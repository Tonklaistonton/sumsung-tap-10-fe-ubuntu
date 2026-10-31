/*
* @file sgpu_governor.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_GOVERNOR_H_
#define _SGPU_GOVERNOR_H_

#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <governor.h>
#include "amdgpu.h"
#include "sgpu_devfreq.h"

#define HZ_PER_KHZ			1000

typedef enum {
	SGPU_DVFS_GOVERNOR_STATIC = 0,
	SGPU_DVFS_GOVERNOR_CONSERVATIVE,
	SGPU_DVFS_GOVERNOR_INTERACTIVE,
	SGPU_DVFS_GOVERNOR_PROFILER,
	SGPU_MAX_GOVERNOR_NUM,
} gpu_governor_type;

struct sgpu_governor_info {
	char *name;
	int (*get_target)(struct devfreq *df, uint32_t *level);
	int (*clear)(struct devfreq *df, uint32_t level);
};

struct sgpu_governor_data {
	struct devfreq			*devfreq;
	struct amdgpu_device		*adev;
#ifdef CONFIG_DRM_SGPU_EXYNOS
	struct notifier_block		nb_tmu;
#endif
	unsigned long			sys_max_freq;
	unsigned long			sys_min_freq;

	struct timer_list		task_timer;
	struct task_struct		*update_task;
	bool				wakeup_lock;

	unsigned long			highspeed_freq;
	uint32_t			highspeed_load;
	uint32_t			highspeed_delay;
	uint32_t			highspeed_level;
	uint32_t			*downstay_times;
	uint32_t			*min_thresholds;
	uint32_t			*max_thresholds;

	/* current state */
	bool				in_suspend;
	struct sgpu_governor_info	*governor;
	unsigned long			expire_jiffies;
	unsigned long			expire_highspeed_delay;
	uint32_t			power_ratio;
	struct mutex			lock;

	/* %, additional weight for compute scenario */
	int				compute_weight;

	/* cl_boost */
	uint32_t			cl_boost_level;
	unsigned long			cl_boost_freq;
	unsigned long			mm_min_clock;
};

ssize_t sgpu_governor_all_info_show(struct devfreq *df, char *buf);
ssize_t sgpu_governor_current_info_show(struct devfreq *df, char *buf,
					size_t size);
int sgpu_governor_change(struct devfreq *df, char *str_governor);
int sgpu_governor_init(struct device *dev, struct devfreq_dev_profile *dp);
void sgpu_governor_deinit(struct devfreq *df);
int sgpu_governor_start(struct devfreq *df);
void sgpu_governor_stop(struct devfreq *df);
unsigned int *sgpu_get_array_data(struct devfreq_dev_profile *dp, const char *buf);
uint64_t sgpu_governor_calc_utilization(struct devfreq *df);

static inline void sgpu_governor_set_mm_min_clock(struct sgpu_governor_data *data,
						  unsigned long freq)
{
	data->mm_min_clock = freq;
}

static inline unsigned long sgpu_governor_get_mm_min_clock(struct sgpu_governor_data *data)
{
	return data->mm_min_clock;
}

static inline void sgpu_set_cl_boost_disabled(struct amdgpu_device *adev, bool disable)
{
	struct sgpu_devfreq_data *df_data = adev->devfreq->last_status.private_data;

	df_data->cl_boost_disable = disable;
}
static inline unsigned long sgpu_get_cl_boost_disabled(struct amdgpu_device *adev)
{
	struct sgpu_devfreq_data *df_data = adev->devfreq->last_status.private_data;

	return df_data->cl_boost_disable;
}

#endif
