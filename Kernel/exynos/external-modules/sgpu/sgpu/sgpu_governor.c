/*
* @file sgpu_governor.c
* @copyright 2020 Samsung Electronics
*/

#include <linux/devfreq.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "sgpu_governor.h"
#include "sgpu_utilization.h"
#include "sgpu_user_interface.h"
#include "sgpu_devfreq.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS
#if IS_ENABLED(CONFIG_CAL_IF)
#include <soc/samsung/cal-if.h>
#include <soc/samsung/fvmap.h>
#endif /* CONFIG_CAL_IF */
#include <linux/notifier.h>
#include "exynos_gpu_interface.h"
#endif /* CONFIG_DRM_SGPU_EXYNOS */
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
#include <soc/samsung/exynos-dm.h>
#endif /* CONFIG_EXYNOS_ESCA_DVFS_MANAGER */

#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GPU)
#include <soc/samsung/profiler/exynos-profiler-extif.h>
#endif /* CONFIG_EXYNOS_PROFILER_GPU */

#include <trace/events/power.h>
#define CREATE_TRACE_POINTS

/* get frequency and delay time data from string */
unsigned int *sgpu_get_array_data(struct devfreq_dev_profile *dp, const char *buf)
{
	const char *cp;
	int i, j;
	int ntokens = 1;
	unsigned int *tokenized_data, *array_data;
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

	array_data = kmalloc(dp->max_state * sizeof(unsigned int), GFP_KERNEL);
	if (!array_data) {
		err = -ENOMEM;
		goto err_kfree;
	}

	for (i = dp->max_state - 1, j = 0; i >= 0; i--) {
		while(j < ntokens - 1 && dp->freq_table[i] >= tokenized_data[j + 1])
			j += 2;
		array_data[i] = tokenized_data[j];
	}
	kfree(tokenized_data);

	return array_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

uint64_t sgpu_governor_calc_utilization(struct devfreq *df)
{
	struct devfreq_dev_status *stat = &df->last_status;
	struct sgpu_governor_data *gdata = df->governor_data;
	struct sgpu_devfreq_data *df_data = stat->private_data;
	struct utilization_data *udata = &df_data->udata;
	struct utilization_timeinfo *sw_info = &udata->timeinfo[SGPU_TIMEINFO_SW];
	unsigned long cu_busy_time = sw_info->cu_busy_time;

	/*
	 * This function should be called while df->lock is being held as it is
	 * supposed to be called in governor callback function to calculate
	 * next target frequency. However since this function now get exposed
	 * to out of KMD to support external governor, checking mutex lock is
	 * required.
	 */
	if (!mutex_is_locked(&df->lock)) {
		DRM_ERROR("Tried Updating GPU util without lock : %s\n",
			  df->governor->name);
		return 0;
	}

	udata->last_util = div64_u64(cu_busy_time *
				      (gdata->compute_weight - 100) +
				      sw_info->busy_time * 100LL,
				     sw_info->total_time);

	if (udata->last_util > 100)
		udata->last_util = 100;

	udata->last_cu_util = div64_u64(cu_busy_time * 100LL, sw_info->total_time);

	if (udata->last_util && udata->last_util == udata->last_cu_util)
		df_data->cl_boost_status = true;
	else
		df_data->cl_boost_status = false;

	return udata->last_util;
}

#define NORMALIZE_SHIFT (10)
#define NORMALIZE_FACT  (1<<(NORMALIZE_SHIFT))
#define NORMALIZE_FACT3 (1<<((NORMALIZE_SHIFT)*3))
#define ITERATION_MAX	(10)

static uint64_t cube_root(uint64_t value)
{
	uint32_t index, iter;
	uint64_t cube, cur, prev = 0;

	if (value == 0)
		return 0;

	index = fls64(value);
	index = (index - 1)/3 + 1;

	/* Implementation of Newton-Raphson method for approximating
	   the cube root */
	iter = ITERATION_MAX;

	cur = ((uint64_t)1 << index);
	cube = cur*cur*cur;

	while (iter) {
		if ((cube-value == 0) || (prev == cur))
			return cur;
		prev = cur;
		cur = (value + 2*cube) / (3*cur*cur);
		cube = cur*cur*cur;
		iter--;
	}

	return prev;
}

static int sgpu_conservative_get_threshold(struct devfreq *df,
					   uint32_t *max, uint32_t *min)
{
	struct sgpu_governor_data *gdata = df->governor_data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data udata = ((struct sgpu_devfreq_data *)stat->private_data)->udata;
	struct utilization_timeinfo *sw_info = &udata.timeinfo[SGPU_TIMEINFO_SW];

	uint64_t coefficient, ratio;
	uint32_t power_ratio;
	unsigned long sw_busy_time;
	uint32_t max_threshold, min_threshold;


	sw_busy_time = sw_info->busy_time;
	power_ratio  = gdata->power_ratio;
	max_threshold = *max;
	min_threshold = *min;

	if (sw_busy_time == 0)
		coefficient = 1;
	else
		coefficient = div64_u64(sw_busy_time * 100 * NORMALIZE_FACT3,
					sw_busy_time * 100);

	if (coefficient == 1)
		ratio = NORMALIZE_FACT;
	else
		ratio = cube_root(coefficient);

	if(ratio == 0)
		ratio = NORMALIZE_FACT;

	*max = (uint32_t)div64_u64((uint64_t)max_threshold * NORMALIZE_FACT, ratio);
	*min = (uint32_t)div64_u64((uint64_t)min_threshold * NORMALIZE_FACT, ratio);

	trace_sgpu_utilization_sw_source_data(sw_info, power_ratio, ratio,
							NORMALIZE_FACT);
	trace_sgpu_governor_conservative_threshold(*max, *min, max_threshold,
						   min_threshold);

	return 0;

}

static int sgpu_dvfs_governor_conservative_get_target(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->governor_data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data udata = ((struct sgpu_devfreq_data *)stat->private_data)->udata;
	uint32_t max_threshold = data->max_thresholds[*level];
	uint32_t min_threshold = data->min_thresholds[*level];
	uint64_t utilization = sgpu_governor_calc_utilization(df);

	if (df->previous_freq < data->highspeed_freq &&
	    utilization > data->highspeed_load) {
		if (time_after(jiffies, data->expire_highspeed_delay)) {
			*level = data->highspeed_level;
			return 0;
		}
	} else {
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);
	}

	if (udata.utilization_src->hw_source_valid) {
		sgpu_conservative_get_threshold(df, &max_threshold,
						&min_threshold);
	}

	if (utilization > max_threshold &&
	    *level > 0) {
		(*level)--;
	} else if (utilization < min_threshold) {
		if (time_after(jiffies, data->expire_jiffies) &&
		    *level < df->profile->max_state - 1 ) {
			(*level)++;
		}
	} else {
		data->expire_jiffies = jiffies +
			msecs_to_jiffies(data->downstay_times[*level]);
	}

	trace_clock_set_rate("Gpu Utilization", utilization, 0);

	return 0;
}

static int sgpu_dvfs_governor_conservative_clear(struct devfreq *df, uint32_t level)
{
	struct sgpu_governor_data *data = df->governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;

	data->expire_jiffies = jiffies +
		msecs_to_jiffies(data->downstay_times[level]);
	if (df_data->current_level == level ||
	    (df_data->current_level >= data->highspeed_level && level < data->highspeed_level))
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);

	return 0;
}


unsigned long sgpu_interactive_target_freq(struct devfreq *df,
					   uint64_t utilization,
					   uint32_t target_load)
{
	struct sgpu_governor_data *gdata = df->governor_data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data udata = ((struct sgpu_devfreq_data *)stat->private_data)->udata;
	struct utilization_timeinfo *sw_info = &udata.timeinfo[SGPU_TIMEINFO_SW];

	unsigned long target_freq = 0;
	uint64_t coefficient, freq_ratio;
	uint32_t power_ratio, new_target_load;
	unsigned long sw_busy_time;


	sw_busy_time = sw_info->busy_time;

	power_ratio  = gdata->power_ratio;

	if (sw_busy_time == 0)
		coefficient = NORMALIZE_FACT3;
	else
		coefficient = div64_u64(sw_busy_time * 100 * NORMALIZE_FACT3,
					sw_busy_time * 100);

	freq_ratio = cube_root(coefficient);

	if(freq_ratio == 0)
		freq_ratio = NORMALIZE_FACT;

	target_freq = div64_u64(freq_ratio * utilization * df->previous_freq,
				(uint64_t)target_load * NORMALIZE_FACT);

	new_target_load = div64_u64(target_load * NORMALIZE_FACT, freq_ratio);

	trace_sgpu_utilization_sw_source_data(sw_info, power_ratio,
					      freq_ratio, NORMALIZE_FACT);
	trace_sgpu_governor_interactive_freq(df, utilization, target_load,
					     target_freq, new_target_load);

	return target_freq;
}

static int sgpu_dvfs_governor_interactive_get_target(struct devfreq *df, uint32_t *level)
{
	struct sgpu_governor_data *data = df->governor_data;
	struct devfreq_dev_status *stat = &df->last_status;
	struct sgpu_devfreq_data *df_data = stat->private_data;
	struct utilization_data udata = df_data->udata;
	unsigned long target_freq;
	uint32_t target_load;
	uint64_t utilization = sgpu_governor_calc_utilization(df);

	if (df->previous_freq < data->highspeed_freq &&
	    utilization > data->highspeed_load) {
		if (time_after(jiffies, data->expire_highspeed_delay)) {
			*level = data->highspeed_level;
			return 0;
		}
	} else {
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);
	}
	target_load = data->max_thresholds[*level];

	if (udata.utilization_src->hw_source_valid)
		target_freq = sgpu_interactive_target_freq(df, utilization,
							   target_load);
	else
		target_freq = div64_u64(utilization * df->previous_freq,
					target_load);

	if (target_freq > df->previous_freq) {
		while (df->profile->freq_table[*level] < target_freq && *level > 0)
			(*level)--;

		data->expire_jiffies = jiffies +
			msecs_to_jiffies(data->downstay_times[*level]);
	} else {
		while (df->profile->freq_table[*level] > target_freq &&
		       *level < df->profile->max_state - 1)
			(*level)++;
		if (df->profile->freq_table[*level] < target_freq)
			(*level)--;

		if (*level > df_data->current_level + 1) {
			target_load = data->max_thresholds[*level];
			if (div64_u64(utilization *
				      df->profile->freq_table[df_data->current_level],
				      df->profile->freq_table[*level]) > target_load) {
				(*level)--;
			}
		}

		if (*level == df_data->current_level) {
			data->expire_jiffies = jiffies +
				msecs_to_jiffies(data->downstay_times[*level]);
		} else if (time_before(jiffies, data->expire_jiffies)) {
			*level = df_data->current_level;
			return 0;
		}
	}


	return 0;
}

static int sgpu_dvfs_governor_interactive_clear(struct devfreq *df, uint32_t level)
{
	struct sgpu_governor_data *data = df->governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
	int target_load;
	uint64_t downstay_jiffies;

	target_load = data->max_thresholds[level];
	downstay_jiffies = msecs_to_jiffies(data->downstay_times[level]);

	if (level > df_data->current_level && df->profile->freq_table[level] != df_data->max_freq)
		data->expire_jiffies = jiffies +
			msecs_to_jiffies(df_data->valid_time);
	else
		data->expire_jiffies = jiffies + downstay_jiffies;
	if (df_data->current_level == level ||
	    (df_data->current_level >= data->highspeed_level && level < data->highspeed_level))
		data->expire_highspeed_delay = jiffies +
			msecs_to_jiffies(data->highspeed_delay);

	return 0;
}

static int sgpu_dvfs_governor_static_get_target(struct devfreq *df, uint32_t *level)
{
	static uint32_t updown = 0;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;

	if (!(updown & 0x1)) {
		if (df->profile->freq_table[*level] < df_data->max_freq && *level > 0)
			(*level)--;
	} else {
		if (df->profile->freq_table[*level] > df_data->min_freq &&
		    *level < df->profile->max_state - 1)
			(*level)++;
	}
	if (df_data->current_level == *level) {
		/* change up and down direction */
		if ((updown & 0x1)) {
			if (df->profile->freq_table[*level] < df_data->max_freq && *level > 0)
				(*level)--;
		} else {
			if (df->profile->freq_table[*level] > df_data->min_freq &&
			    *level < df->profile->max_state - 1)
				(*level)++;
		}
		if (df_data->current_level != *level) {
			/* increase direction change count */
			updown++;
		}
	}

	return 0;
}

static struct sgpu_governor_info governor_info[SGPU_MAX_GOVERNOR_NUM] = {
	[SGPU_DVFS_GOVERNOR_STATIC] = {
		"static",
		sgpu_dvfs_governor_static_get_target,
		NULL,
	},
	[SGPU_DVFS_GOVERNOR_CONSERVATIVE] = {
		"conservative",
		sgpu_dvfs_governor_conservative_get_target,
		sgpu_dvfs_governor_conservative_clear,
	},
	[SGPU_DVFS_GOVERNOR_INTERACTIVE] = {
		"interactive",
		sgpu_dvfs_governor_interactive_get_target,
		sgpu_dvfs_governor_interactive_clear,
	},
};

static int devfreq_sgpu_func(struct devfreq *df, unsigned long *freq)
{
	int err = 0;
	struct sgpu_governor_data *data = df->governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
	struct amdgpu_device *adev = df_data->adev;
	struct utilization_data *udata = &df_data->udata;
	struct utilization_timeinfo *sw_info = &udata->timeinfo[SGPU_TIMEINFO_SW];
	struct device *dev= df->dev.parent;
	uint32_t level = df_data->current_level;
	struct dev_pm_opp *target_opp;
	int32_t qos_min_freq, qos_max_freq;
	uint32_t min_freq, max_freq;
	uint32_t gvf_start_level = df_data->gvf_start_level;

	qos_max_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MAX_FREQUENCY);
	qos_min_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MIN_FREQUENCY);

	df_data->max_freq = min(df->scaling_max_freq,
			     (unsigned long)HZ_PER_KHZ * qos_max_freq);
	max_freq = df_data->max_freq;

	if (gvf_start_level < df->profile->max_state &&
		df->profile->freq_table[gvf_start_level] < df_data->max_freq) {
		target_opp = devfreq_recommended_opp(dev, &df_data->max_freq,
				DEVFREQ_FLAG_LEAST_UPPER_BOUND);
		if (IS_ERR(target_opp)) {
			dev_err(dev, "max_freq: not found valid OPP table\n");
			return PTR_ERR(target_opp);
		}
		dev_pm_opp_put(target_opp);
	}

	df_data->min_freq = max(df->scaling_min_freq,
			      (unsigned long)HZ_PER_KHZ * qos_min_freq);
	df_data->min_freq = min(df_data->max_freq, df_data->min_freq);
	min_freq = df_data->min_freq;

	trace_clock_set_rate("Gpu Min Limit", df_data->min_freq, 0);
	trace_clock_set_rate("Gpu Max Limit", df_data->max_freq, 0);

	/* in suspend or power_off*/
	if (atomic_read(&df->suspend_count) > 0) {
		*freq = max(df_data->min_freq, min(df_data->max_freq,	df->resume_freq));
		df->resume_freq = *freq;
		df->suspend_freq = 0;
		dev_info(adev->dev, "cancel devfreq update in gpu power down\n");
		return -EBUSY;
	}

	err = df->profile->get_dev_status(df->dev.parent, &df->last_status);
	if (err)
		return err;

	if (sw_info->prev_total_time) {
#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_notify_utilization();
#endif
		data->governor->get_target(df, &level);
	}

	if (!df_data->cl_boost_disable && !data->mm_min_clock &&
	    df_data->cl_boost_status) {
		level = data->cl_boost_level;
		data->expire_jiffies = jiffies +
			msecs_to_jiffies(data->downstay_times[level]);
	}

	while (df->profile->freq_table[level] < df_data->min_freq && level > 0)
		level--;
	while (df->profile->freq_table[level] > df_data->max_freq &&
	       level < df->profile->max_state - 1)
		level++;

	if (level >= gvf_start_level) {
		min_freq = max_freq = sgpu_gvf_get_run_freq(df_data->adev);
		sgpu_gvf_set_level(df_data->adev, level - gvf_start_level + 1);
	} else
		sgpu_gvf_set_level(df_data->adev, 0);

	*freq = df->profile->freq_table[level];
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	policy_update_call_to_DM(df_data->dm_type, min_freq, max_freq);
#endif
	return err;
}

static struct sgpu_governor_data gov_data;

static int sgpu_governor_notifier_call(struct notifier_block *nb,
				       unsigned long event, void *ptr)
{
	struct sgpu_devfreq_data *df_data = container_of(nb, struct sgpu_devfreq_data,
						       nb_trans);
	struct utilization_data udata = df_data->udata;
	struct devfreq *df = df_data->devfreq;
	struct drm_device *ddev = adev_to_drm(df_data->adev);
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)ptr;
	struct sgpu_governor_data *data = df->governor_data;
	struct utilization_timeinfo *sw_info = &udata.timeinfo[SGPU_TIMEINFO_SW];

	if (data != &gov_data)
		return NOTIFY_DONE;

	/* in suspend or power_off*/
	if (ddev->switch_power_state == DRM_SWITCH_POWER_OFF ||
	    ddev->switch_power_state == DRM_SWITCH_POWER_DYNAMIC_OFF)
		return NOTIFY_DONE;

	if (freqs->old == freqs->new && !sw_info->prev_total_time)
		return NOTIFY_DONE;

	switch (event) {
	case DEVFREQ_PRECHANGE:
		sgpu_utilization_trace_before(&df->last_status, freqs->new);
		break;
	case DEVFREQ_POSTCHANGE:
		sgpu_utilization_trace_after(&df->last_status, freqs->new);
		if (data && data->governor->clear && freqs->old != freqs->new)
			data->governor->clear(df, df_data->current_level);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int devfreq_sgpu_handler(struct devfreq *df, unsigned int event, void *data)
{
	struct sgpu_governor_data *governor_data = df->governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
	unsigned long min_freq, max_freq;
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	unsigned long cur_freq;
#endif
	int ret = 0;

	mutex_lock(&gov_data.lock);

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = sgpu_governor_start(df);
		if (ret)
			goto out;

#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_update_time_in_state(0);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
		devfreq_monitor_start(df);
		break;
	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(df);
		sgpu_governor_stop(df);
		break;
	case DEVFREQ_GOV_UPDATE_INTERVAL:
		devfreq_update_interval(df, (unsigned int*)data);
		break;
	case DEVFREQ_GOV_SUSPEND:
		min_freq = df->profile->freq_table[df_data->gvf_start_level - 1];
		max_freq = df->profile->freq_table[0];
		devfreq_monitor_suspend(df);
		if (governor_data->wakeup_lock)
			df->resume_freq = df->previous_freq;
		else
			df->resume_freq = 0;
		sgpu_utilization_trace_stop(&df->last_status);
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
		if (df->profile->get_cur_freq)
			df->profile->get_cur_freq(df->dev.parent, &cur_freq);
		else
			cur_freq = df->previous_freq;
		if (df_data->adev->gpu_dss_freq_id)
			dbg_snapshot_freq(df_data->adev->gpu_dss_freq_id, cur_freq,
							min_freq, DSS_FLAG_IN);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
		policy_update_call_to_DM(df_data->dm_type, min_freq, max_freq);
		/* gpu freq should be changed to min_freq before entering suspend */
		DM_CALL(df_data->dm_type, &min_freq);
#elif IS_ENABLED(CONFIG_CAL_IF)
		cal_dfs_set_rate(df_data->adev->cal_id, min_freq);
#endif /* CONFIG_CAL_IF */
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
		if (df_data->adev->gpu_dss_freq_id)
			dbg_snapshot_freq(df_data->adev->gpu_dss_freq_id, cur_freq,
							min_freq, DSS_FLAG_OUT);
#endif
		governor_data->in_suspend = true;
		df_data->cl_boost_status = false;
#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_update_time_in_state(df->previous_freq);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
		break;
	case DEVFREQ_GOV_RESUME:
		min_freq = df->profile->freq_table[df_data->gvf_start_level - 1];
		governor_data->in_suspend = false;
		if (df->suspend_freq == 0)
			df->suspend_freq = min_freq;
		sgpu_utilization_trace_start(&df->last_status);
		if (governor_data->governor->clear)
			governor_data->governor->clear(df, df_data->current_level);
		devfreq_monitor_resume(df);
#ifdef CONFIG_DRM_SGPU_EXYNOS
		gpu_dvfs_update_time_in_state(0);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
		break;
	default:
		break;
	}

out:
	mutex_unlock(&gov_data.lock);
	return ret;
}

static struct devfreq_governor devfreq_governor_sgpu = {
	.name = "sgpu_governor",
	.get_target_freq = devfreq_sgpu_func,
	.event_handler = devfreq_sgpu_handler,
};

ssize_t sgpu_governor_all_info_show(struct devfreq *df, char *buf)
{
	int i;
	ssize_t count = 0;
	if (!df->governor || !df->governor_data)
		return -EINVAL;

	for (i = 0; i < SGPU_MAX_GOVERNOR_NUM; i++) {
		struct sgpu_governor_info *governor = &governor_info[i];
		if (governor->name)
			count += scnprintf(&buf[count], (PAGE_SIZE - count - 2),
					"%s ", governor->name);
	}
	/* Truncate the trailing space */
	if (count)
		count--;

	count += sprintf(&buf[count], "\n");

	return count;
}

ssize_t sgpu_governor_current_info_show(struct devfreq *df, char *buf,
					size_t size)
{
	struct sgpu_governor_data *data;

	if (!df || !df->governor_data)
		return -EINVAL;

	data = df->governor_data;

	return scnprintf(buf, size, "%s", data->governor->name);
}

int sgpu_governor_change(struct devfreq *df, char *str_governor)
{
	int i;
	struct sgpu_governor_data *data = df->governor_data;

	for (i = 0; i < SGPU_MAX_GOVERNOR_NUM; i++) {
		if (!governor_info[i].name)
			continue;

		if (!strncmp(governor_info[i].name, str_governor, DEVFREQ_NAME_LEN)) {
			mutex_lock(&gov_data.lock);
			if (!data->in_suspend)
				devfreq_monitor_stop(df);
			data->governor = &governor_info[i];
			if (!data->in_suspend)
				devfreq_monitor_start(df);
			mutex_unlock(&gov_data.lock);
			return 0;
		}
	}

	DRM_WARN("%s: Governor %s not started\n", __func__, df->governor->name);

	return -ENODEV;
}

#define DVFS_TABLE_ROW_MAX			1
#define DEFAULT_GOVERNOR			SGPU_DVFS_GOVERNOR_CONSERVATIVE
#define DEFAULT_INITIAL_FREQ			24000
#define DEFAULT_HIGHSPEED_FREQ			500000
#define DEFAULT_HIGHSPEED_LOAD			99
#define DEFAULT_HIGHSPEED_DELAY			0
#define DEFAULT_POWER_RATIO			50
#define DEFAULT_CL_BOOST_FREQ			999000
#define DEFAULT_COMPUTE_WEIGHT			100

static void sgpu_governor_dt_preparse(struct device *dev,
				      struct devfreq_dev_profile *dp,
				      struct sgpu_governor_data *data)
{
	uint32_t value;

	if (!of_property_read_u32(dev->of_node, "highspeed_freq", &value))
		data->highspeed_freq = (unsigned long)value;
	else
		data->highspeed_freq = DEFAULT_HIGHSPEED_FREQ;

	if (of_property_read_u32(dev->of_node, "highspeed_load",
				 &data->highspeed_load))
		data->highspeed_load = DEFAULT_HIGHSPEED_LOAD;

	if (of_property_read_u32(dev->of_node, "highspeed_delay",
				 &data->highspeed_delay))
		data->highspeed_delay = DEFAULT_HIGHSPEED_DELAY;

	if (!of_property_read_u32(dev->of_node, "cl_boost_freq", &value))
		data->cl_boost_freq = value;
	else
		data->cl_boost_freq = DEFAULT_CL_BOOST_FREQ;

	if (of_property_read_u32(dev->of_node, "compute_weight",
				   &data->compute_weight))
		data->compute_weight = DEFAULT_COMPUTE_WEIGHT;
}

/* These need to be parsed after dvfs table set */
static int sgpu_governor_dt_postparse(struct device *dev,
				      struct devfreq_dev_profile *dp,
				      struct sgpu_governor_data *data)
{
	const char *tmp_str;
	int ret = 0;

	if (of_property_read_string(dev->of_node, "min_threshold", &tmp_str))
		tmp_str = "60";
	data->min_thresholds = sgpu_get_array_data(dp, tmp_str);
	if (IS_ERR(data->min_thresholds)) {
		ret = PTR_ERR(data->min_thresholds);
		dev_err(dev, "fail minimum threshold tokenized %d\n", ret);
		goto err_min_threshold;
	}

	if (of_property_read_string(dev->of_node, "max_threshold", &tmp_str))
		tmp_str = "75";
	data->max_thresholds = sgpu_get_array_data(dp, tmp_str);
	if (IS_ERR(data->max_thresholds)) {
		ret = PTR_ERR(data->max_thresholds);
		dev_err(dev, "fail maximum threshold tokenized %d\n", ret);
		goto err_max_threshold;
	}

	if (of_property_read_string(dev->of_node, "downstay_time", &tmp_str))
		tmp_str = "32";
	data->downstay_times = sgpu_get_array_data(dp, tmp_str);
	if (IS_ERR(data->downstay_times)) {
		ret = PTR_ERR(data->downstay_times);
		dev_err(dev, "fail down stay time tokenized %d\n", ret);
		goto err_downstay_time;
	}

	return ret;

err_downstay_time:
	kfree(data->max_thresholds);
err_max_threshold:
	kfree(data->min_thresholds);
err_min_threshold:
	return ret;
}

#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GPU)
static void sgpu_governor_register_profiler_governor(struct amdgpu_device *adev)
{
	struct sgpu_governor_info *gov_profiler = &governor_info[SGPU_DVFS_GOVERNOR_PROFILER];
	char *gov_name = NULL;

	gov_name = sgpu_profiler_governor_get_name();
	if (!gov_name) {
		dev_info(adev->dev, "Failed to register profiler governor");
		return;
	}

	gov_profiler->name = gov_name;
	gov_profiler->get_target = sgpu_profiler_governor_get_target;
	gov_profiler->clear = sgpu_profiler_governor_clear;

	dev_info(adev->dev, "Register governor : %s", gov_profiler->name);
}
#else
#define sgpu_governor_register_profiler_governor(adev)	do { } while (0)
#endif /* CONFIG_EXYNOS_PROFILER_GPU */

int sgpu_governor_init(struct device *dev, struct devfreq_dev_profile *dp)
{
	struct sgpu_governor_data *data = &gov_data;
	int ret = 0, i, j, k, cur_level;
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct sgpu_gvf *gvf = &adev->gvf;
	int gvf_max_level = sgpu_gvf_get_max_level(adev);
#ifdef CONFIG_DRM_SGPU_EXYNOS
	uint32_t dt_freq;
	unsigned long max_freq, min_freq;
	struct freq_volt *g3d_rate_volt = NULL;
	uint32_t *freq_table;
	int freq_table_size;
	unsigned long cal_maxfreq, cal_minfreq, boot_freq;
#endif /* CONFIG_DRM_SGPU_EXYNOS*/

	dp->initial_freq = DEFAULT_INITIAL_FREQ;
	dp->polling_ms = sgpu_devfreq_polling_ms;
	dp->max_state = DVFS_TABLE_ROW_MAX;

	sgpu_governor_register_profiler_governor(adev);

	sgpu_governor_dt_preparse(dev, dp, data);
	data->governor = &governor_info[DEFAULT_GOVERNOR];
	data->wakeup_lock = true;
	data->in_suspend = false;
	data->adev = adev;
	data->power_ratio = DEFAULT_POWER_RATIO;
	data->cl_boost_level = 0;
	data->mm_min_clock = 0;

	mutex_init(&data->lock);

#ifdef CONFIG_DRM_SGPU_EXYNOS
	freq_table_size = of_property_count_u32_elems(dev->of_node,
						      "freq_table");
	if (freq_table_size < 0) {
		dev_err(dev, "Cannot find freq-table node in DT\n");
		ret = freq_table_size;
		goto err;
	}

	freq_table_size += gvf_max_level;

	freq_table = kcalloc(freq_table_size, sizeof(uint32_t), GFP_KERNEL);
	if (!freq_table) {
		ret = -ENOMEM;
		goto err;
	}

	ret = of_property_read_u32_array(dev->of_node, "freq_table",
					 freq_table + gvf_max_level,
					 freq_table_size - gvf_max_level);
	if (ret) {
		dev_err(dev, "Cannot read the freq-table node in DT\n");
		goto err_kfree1;
	}

	for (i = 0; i < gvf_max_level; i++)
		freq_table[i] = gvf->table[i];
	dp->max_state = freq_table_size;

	g3d_rate_volt = kcalloc(freq_table_size, sizeof(struct freq_volt),
				GFP_KERNEL);
	if (!g3d_rate_volt) {
		ret = -ENOMEM;
		goto err_kfree1;
	}

#if IS_ENABLED(CONFIG_CAL_IF)
	dp->initial_freq = cal_dfs_get_boot_freq(adev->cal_id);
	cal_maxfreq = cal_dfs_get_max_freq(adev->cal_id);
	cal_minfreq = cal_dfs_get_min_freq(adev->cal_id);
#else
	dp->initial_freq = cal_maxfreq = cal_minfreq = 303000;
#endif /* CONFIG_CAL_IF */
	boot_freq = dp->initial_freq;

	ret = of_property_read_u32(dev->of_node, "max_freq", &dt_freq);
	if (!ret) {
		max_freq = (unsigned long)dt_freq;
		max_freq = min(max_freq, cal_maxfreq);
	} else {
		max_freq = cal_maxfreq;
	}

	ret = of_property_read_u32(dev->of_node, "min_freq", &dt_freq);
	if (!ret) {
		min_freq = (unsigned long)dt_freq;
		min_freq = max(min_freq, cal_minfreq);
	} else {
		min_freq = cal_minfreq;
	}

	min_freq = min(max_freq, min_freq);

	dp->freq_table = kcalloc(dp->max_state, sizeof(*(dp->freq_table)),
				GFP_KERNEL);
	if (!dp->freq_table) {
		ret = -ENOMEM;
		goto err_kfree2;
	}

	for (i = freq_table_size - 1, j = 0, k = 0; i >= 0; i--) {
		if (freq_table[i] > max_freq)
			continue;
		dp->freq_table[k++] = freq_table[i];

		if (freq_table[i] < min_freq)
			continue;
		g3d_rate_volt[j++].rate = freq_table[i];
	}
	dp->max_state = k;

#if IS_ENABLED(CONFIG_CAL_IF)
	cal_dfs_get_freq_volt_table(adev->cal_id, g3d_rate_volt, j);
#endif /* CONFIG_CAL_IF */

	adev->gpu_dss_freq_id = 0;
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	adev->gpu_dss_freq_id = dbg_snapshot_get_freq_idx("G3D");
#endif

#endif /* CONFIG_DRM_SGPU_EXYNOS */
	for (i = 0; i < j; i++) {
		uint32_t freq, volt;

#ifdef CONFIG_DRM_SGPU_EXYNOS
		freq =  g3d_rate_volt[i].rate;
		volt =  g3d_rate_volt[i].volt;
#else
		freq = dp->initial_freq;
		volt = 0;
#endif
		if (freq >= dp->initial_freq) {
			cur_level = i;
		}

		if (freq >= data->highspeed_freq) {
			data->highspeed_level = i;
		}

		if (freq >= data->cl_boost_freq)
			data->cl_boost_level = i;

		ret = dev_pm_opp_add(dev, freq, volt);
		if (ret) {
			dev_err(dev, "failed to add opp entries\n");
			goto err_kfree3;
		}
	}
	dp->initial_freq = dp->freq_table[cur_level];

#ifdef CONFIG_DRM_SGPU_EXYNOS
	gpu_dvfs_init_table(g3d_rate_volt, dp->freq_table, dp->max_state, j);
	gpu_dvfs_init_utilization_notifier_list();
#endif
	ret = sgpu_governor_dt_postparse(dev, dp, data);
	if (ret) {
		dev_err(dev, "failed to dt tokenized %d\n", ret);
		goto err_kfree3;
	}

	ret = devfreq_add_governor(&devfreq_governor_sgpu);
	if (ret) {
		dev_err(dev, "failed to add governor %d\n", ret);
		goto err_kfree3;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
	kfree(freq_table);
	kfree(g3d_rate_volt);
#endif

	return ret;

err_kfree3:
	kfree(dp->freq_table);
err_kfree2:
#ifdef CONFIG_DRM_SGPU_EXYNOS
	kfree(g3d_rate_volt);
#endif
err_kfree1:
#ifdef CONFIG_DRM_SGPU_EXYNOS
	kfree(freq_table);
#endif
err:
	return ret;
}

void sgpu_governor_deinit(struct devfreq *df)
{
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
	int ret = 0;

	kfree(df->profile->freq_table);
	ret = devfreq_remove_governor(&devfreq_governor_sgpu);
	devm_devfreq_unregister_notifier(df->dev.parent, df, &df_data->nb_trans,
					 DEVFREQ_TRANSITION_NOTIFIER);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);
}

int sgpu_governor_start(struct devfreq *df)
{
	struct sgpu_governor_data *governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
	int ret = 0;

	df->governor_data = &gov_data;
	governor_data = df->governor_data;

	if (!df_data) { /* when initializing governor */
		ret = sgpu_devfreq_data_init(governor_data->adev, df);
		if (ret)
			return ret;
		df_data = df->last_status.private_data;
		df_data->nb_trans.notifier_call = sgpu_governor_notifier_call;
		devm_devfreq_register_notifier(df->dev.parent, df, &df_data->nb_trans,
					       DEVFREQ_TRANSITION_NOTIFIER);
	} else { /* when governor changing*/
		ret = sgpu_create_sysfs_file(df);
		if (ret) {
			dev_err(df->dev.parent, "Unable to create sysfs node %d\n", ret);
			return ret;
		}
	}

	sgpu_utilization_trace_start(&df->last_status);

	if (governor_data->governor->clear)
		governor_data->governor->clear(df, df_data->current_level);

	return ret;
}

void sgpu_governor_stop(struct devfreq *df)
{
	sgpu_remove_sysfs_file(df);
	df->governor_data = NULL;
}
