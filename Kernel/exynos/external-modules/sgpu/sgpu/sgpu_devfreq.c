/*
 * @file sgpu_devfreq.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * @brief Contains the implementaton of main devfreq handler.
 *  Devfreq handler manages devfreq feature.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/devfreq.h>
#include "amdgpu.h"
#include "sgpu_governor.h"
#include "sgpu_user_interface.h"
#include "sgpu_utilization.h"
#include "amdgpu_trace.h"
#include "sgpu_power_trace.h"
#include "sgpu_devfreq.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS
#if IS_ENABLED(CONFIG_CAL_IF)
#include <soc/samsung/cal-if.h>
#endif /* CONFIG_CAL_IF */
#include "exynos_gpu_interface.h"

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
#include <soc/samsung/exynos-dm.h>
#endif /* CONFIG_EXYNOS_ESCA_DVFS_MANAGER */
#endif /* CONFIG_DRM_SGPU_EXYNOS */
#include "nv.h"

static void sgpu_devfreq_set_didt_edc(struct amdgpu_device *adev, unsigned long freq)
{
	struct sgpu_didt_edc *didt = &adev->didt_edc;

	if (!didt->enable)
		return;

	if (amdgpu_in_reset(adev) || atomic_read(&adev->ifpo.count) == 0) {
		SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "Defer didt/edc setting");
		return;
	}

	sgpu_ifpo_lock(adev);
	vangogh_lite_set_didt_edc(adev, freq);
	sgpu_ifpo_unlock(adev);
}

void sgpu_devfreq_request_freq(struct amdgpu_device *adev, int dm_type,
			       unsigned long cur_freq, unsigned long *target_freq)
{
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	if (adev->gpu_dss_freq_id)
		dbg_snapshot_freq(adev->gpu_dss_freq_id, cur_freq, *target_freq, DSS_FLAG_IN);
#endif /* CONFIG_DEBUG_SNAPSHOT */
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	DM_CALL(dm_type, target_freq);
#elif IS_ENABLED(CONFIG_CAL_IF)
	cal_dfs_set_rate(adev->cal_id, *target_freq);
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	if (adev->gpu_dss_freq_id)
		dbg_snapshot_freq(adev->gpu_dss_freq_id, cur_freq, *target_freq, DSS_FLAG_OUT);
#endif /* CONFIG_DEBUG_SNAPSHOT */
}

static int sgpu_devfreq_target(struct device *dev, unsigned long *target_freq, u32 flags)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct devfreq *df = adev->devfreq;
	struct sgpu_devfreq_data *data = df->last_status.private_data;
	unsigned long cur_freq, qos_min_freq, qos_max_freq;
	unsigned long request_freq = *target_freq;
	int i;
	struct dev_pm_opp *target_opp;

#if defined(CONFIG_DRM_SGPU_EXYNOS) && !defined(CONFIG_SOC_S5E9925_EVT0)
	uint32_t cnt_value;
#endif /* CONFIG_DRM_SGPU_EXYNOS && !CONFIG_SOC_S5E9925_EVT0 */
	if (df->profile->get_cur_freq)
		df->profile->get_cur_freq(df->dev.parent, &cur_freq);
	else
		cur_freq = df->previous_freq;
	qos_min_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MIN_FREQUENCY);
	qos_max_freq = dev_pm_qos_read_value(dev, DEV_PM_QOS_MAX_FREQUENCY);
	if (qos_min_freq >= qos_max_freq)
		flags |= DEVFREQ_FLAG_LEAST_UPPER_BOUND;

	if (data->gvf_start_level < df->profile->max_state &&
		df->profile->freq_table[data->gvf_start_level] >= *target_freq)
		request_freq = sgpu_gvf_get_run_freq(adev);
	else {
		target_opp = devfreq_recommended_opp(dev, target_freq, flags);
		if (IS_ERR(target_opp)) {
			dev_err(dev, "target_freq: not found valid OPP table\n");
			return PTR_ERR(target_opp);
		}
		dev_pm_opp_put(target_opp);
	}

	trace_gpu_frequency(0, *target_freq);

	if (cur_freq == *target_freq)
		return 0;

	/* in suspend or power_off*/
	if (atomic_read(&df->suspend_count) > 0) {
		*target_freq = df->previous_freq;
		return 0;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
	cnt_value = readl(adev->pm.pmu_mmio + 0x5c);
	DRM_DEBUG("[SWAT CNT1] CUR_2AD Count : %#x, CUR_1AD Count : %#x",
		  (cnt_value >> 16) & 0xffff, cnt_value & 0xffff);
	cnt_value = readl(adev->pm.pmu_mmio + 0x60);
	DRM_DEBUG("[SWAT CNT2] CUR_2BD Count : %#x, CUR_1BD Count : %#x",
		  (cnt_value >> 16) & 0xffff, cnt_value & 0xffff);

	/* BG3D_DVFS_CTL 0x058 disable */
	writel(0x0, adev->pm.pmu_mmio + 0x58);

	/* Polling busy bit (BG3D_DVFS_CTL[12])  */
	while(readl(adev->pm.pmu_mmio + 0x58) & 0x1000) ;

	sgpu_devfreq_request_freq(adev, data->dm_type, cur_freq, &request_freq);

	/* BG3D_DVFS_CTL 0x058 enable */
	writel(0x1, adev->pm.pmu_mmio + 0x58);

#endif /* CONFIG_DRM_SGPU_EXYNOS */

	for (i = 0; i < df->profile->max_state; i++) {
		if (df->profile->freq_table[i] == request_freq)
			data->current_level = i;
	}

	sgpu_devfreq_set_didt_edc(adev, request_freq);

	trace_sgpu_devfreq_set_target(cur_freq, request_freq);

	/* Always update cur_freq if it's GVF level */
	if (data->gvf_start_level < df->profile->max_state &&
		df->profile->freq_table[data->gvf_start_level] >= *target_freq) {
		if (*target_freq >= df->profile->freq_table[df->profile->max_state -1])
			data->old_freq = *target_freq;
		else
			data->old_freq = df->profile->freq_table[df->profile->max_state -1];
	}

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "set_freq:%8lu", request_freq);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
int sgpu_dm_freq_scaler(int dm_type, void *devdata, u32 target_freq, unsigned int relation)
{
	struct sgpu_devfreq_data *data = devdata;
	struct devfreq *df = data->devfreq;
	struct amdgpu_device *adev = data->adev;
	int i;

	if (!df || !df->profile)
		return 0;

	for (i = 0; i < df->profile->max_state; i++) {
		if (df->profile->freq_table[i] == target_freq)
			data->current_level = i;
	}

	data->old_freq = target_freq;
	trace_gpu_frequency(0, target_freq);
	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "noti_freq:%u", target_freq);
	return 0;
}
#endif /* CONFIG_EXYNOS_ESCA_DVFS_MANAGER */

static int sgpu_devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);

	stat->current_frequency = adev->devfreq->previous_freq;
	return sgpu_utilization_capture(stat);
}

static int sgpu_register_pm_qos(struct devfreq *df)
{
	struct sgpu_governor_data *data = df->governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;

	data->devfreq = df;

	data->sys_min_freq = df->scaling_min_freq;
	data->sys_max_freq = df->scaling_max_freq;

	/* Round down to kHz for PM QoS */
	dev_pm_qos_add_request(df->dev.parent, &df_data->sys_pm_qos_min,
			       DEV_PM_QOS_MIN_FREQUENCY,
			       data->sys_min_freq / HZ_PER_KHZ);
	dev_pm_qos_add_request(df->dev.parent, &df_data->sys_pm_qos_max,
			       DEV_PM_QOS_MAX_FREQUENCY,
			       data->sys_max_freq / HZ_PER_KHZ);

	return 0;
}

static int sgpu_unregister_pm_qos(struct devfreq *df)
{
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;

	if (!df_data)
		return -EINVAL;

	dev_pm_qos_remove_request(&df_data->sys_pm_qos_min);
	dev_pm_qos_remove_request(&df_data->sys_pm_qos_max);

	return 0;
}

static int sgpu_devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct devfreq *df = adev->devfreq;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
#endif /* CONFIG_EXYNOS_ESCA_DVFS_MANAGER */

	if (atomic_read(&df->suspend_count) > 0) {
		*freq = 0;
		return 0;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	*freq = df_data->old_freq;
#elif IS_ENABLED(CONFIG_CAL_IF)
	*freq = cal_dfs_cached_get_rate(adev->cal_id);
#endif /* CONFIG_CAL_IF */
#else
	*freq = adev->devfreq->previous_freq;
#endif /* CONFIG_DRM_SGPU_EXYNOS */

	return 0;
}

static void sgpu_devfreq_exit(struct device *dev)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);

#ifdef CONFIG_DRM_SGPU_EXYNOS
	exynos_interface_deinit(adev->devfreq);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
	sgpu_unregister_pm_qos(adev->devfreq);
	sgpu_governor_deinit(adev->devfreq);

	kfree(adev->devfreq->profile);

	return;
}

static ssize_t time_in_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	uint64_t cur_time, state_time;
	int i, lev, prev_lev = 0;
	ssize_t count = 0;

	mutex_lock(&df->lock);

	cur_time = get_jiffies_64();
	for (lev = 0; lev < df->profile->max_state; lev++) {
		if (df->previous_freq == df->profile->freq_table[lev]) {
			prev_lev = lev;
			break;
		}
	}
	if (prev_lev == df->profile->max_state) {
		mutex_unlock(&df->lock);
		return 0;
	}

	count += scnprintf(&buf[count], PAGE_SIZE - count,
			   "    FREQ    TIME(ms)\n");
	for (i = 0; i < df->profile->max_state; i++) {
		state_time = df->stats.time_in_state[i];
		if (!df->stop_polling && prev_lev == i)
			state_time += cur_time - df->stats.last_update;

		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "%8lu  %10llu\n", df->profile->freq_table[i],
				   jiffies64_to_msecs(state_time));
	}

	mutex_unlock(&df->lock);

	return count;
}
static DEVICE_ATTR_RO(time_in_state);

static ssize_t total_kernel_pages_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev->parent);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	ssize_t count = 0;

	count = scnprintf(buf, PAGE_SIZE, "total_kernel_pages = %zd\n", adev->num_kernel_pages);

	return count;
}
static DEVICE_ATTR_RO(total_kernel_pages);

static ssize_t polling_ms_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count = scnprintf(buf, PAGE_SIZE, "%d\n", gpu_dvfs_get_polling_interval());

	return count;
}

static ssize_t polling_ms_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned int polling_ms, ret;

	ret = sscanf(buf, "%d", &polling_ms);
	if (ret < 0)
		return -EINVAL;

	gpu_dvfs_set_polling_interval(polling_ms);

	return count;
}
static DEVICE_ATTR_RW(polling_ms);

int sgpu_devfreq_init(struct amdgpu_device *adev)
{
	struct devfreq_dev_profile *dp;
	int ret = 0;

	dp = kzalloc(sizeof(struct devfreq_dev_profile), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	dp->target = sgpu_devfreq_target;
	dp->get_dev_status = sgpu_devfreq_status;
	dp->get_cur_freq = sgpu_devfreq_cur_freq;
	dp->exit = sgpu_devfreq_exit;
	dp->timer = DEVFREQ_TIMER_DELAYED;

	/* Initialize DIDT/EDC variables */
	vangogh_lite_didt_edc_init(adev);

	ret = sgpu_gvf_init(adev);
	if (ret)
		dev_err(adev->dev, "GPU Virtual Frequency init failed\n");

	ret = sgpu_governor_init(adev->dev, dp);
	if (ret)
		goto err_gov;

	adev->devfreq = devfreq_add_device(adev->dev, dp, "sgpu_governor", NULL);
	if (IS_ERR(adev->devfreq)) {
		dev_err(adev->dev, "Unable to register with devfreq %d\n", ret);
		ret = PTR_ERR(adev->devfreq);
		goto err_devfreq;
	}

	adev->devfreq->suspend_freq = dp->initial_freq;

	ret = sgpu_register_pm_qos(adev->devfreq);
	if (ret) {
		dev_err(adev->dev, "Unable to register pm QoS requests of devfreq %d\n", ret);
		goto err_noti;
	}

	ret = sysfs_create_file(&adev->devfreq->dev.kobj, &dev_attr_time_in_state.attr);
	if (ret) {
		dev_err(adev->dev, "Unable to create sysfs node time_in_state %d\n", ret);
		goto err_sysfs;
	}
	ret = sysfs_create_file(&adev->devfreq->dev.kobj, &dev_attr_total_kernel_pages.attr);
	if (ret) {
		dev_err(adev->dev, "Unable to create sysfs node total_kernel_pages %d\n", ret);
		goto err_sysfs;
	}
	ret = sysfs_create_file(&adev->devfreq->dev.kobj, &dev_attr_polling_ms.attr);
	if (ret) {
		dev_err(adev->dev, "Unable to create sysfs node polling_ms %d\n", ret);
		goto err_sysfs;
	}
	ret = sgpu_create_sysfs_file(adev->devfreq);
	if (ret) {
		dev_err(adev->dev, "Unable to create sysfs node interface %d\n", ret);
		goto err_sysfs;
	}

#ifdef CONFIG_DRM_SGPU_EXYNOS
	ret = exynos_interface_init(adev->devfreq);
	if (ret) {
		dev_err(adev->dev, "Unable to create exynos interface %d\n", ret);
		goto err_sysfs;
	}
#endif /* CONFIG_DRM_SGPU_EXYNOS */
	return ret;

err_sysfs:
	sgpu_unregister_pm_qos(adev->devfreq);
err_noti:
	devfreq_remove_device(adev->devfreq);
err_devfreq:
	sgpu_governor_deinit(adev->devfreq);
	sgpu_gvf_fini(adev);
err_gov:
	kfree(dp);
	return ret;
}

static void get_cal_min_max_freq(unsigned int cal_id, unsigned long *minfreq,
				 unsigned long *maxfreq)
{
#if IS_ENABLED(CONFIG_CAL_IF)
	*maxfreq = cal_dfs_get_max_freq(cal_id);
	*minfreq = cal_dfs_get_min_freq(cal_id);
#else
	*maxfreq = *minfreq = 303000;
#endif /* CONFIG_CAL_IF */
}

static void sgpu_devfreq_dm_init(struct device *dev, struct sgpu_devfreq_data *data,
			      struct devfreq_dev_profile *dp, unsigned long max_freq,
			      unsigned long min_freq)
{
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	if (of_property_read_u32(dev->of_node, "dm_type", &data->dm_type))
		/* DM_G3D = 0x0A */
		data->dm_type = 10;
	exynos_dm_data_init(data->dm_type, data,
			min_freq, max_freq, dp->initial_freq);

	register_exynos_dm_freq_scaler(data->dm_type,
			sgpu_dm_freq_scaler);
#endif /* CONFIG_EXYNOS_ESCA_DVFS_MANAGER */
}

static void sgpu_devfreq_request_init_freq(struct amdgpu_device *adev,
					   struct sgpu_devfreq_data *data,
			      		   struct devfreq_dev_profile *dp,
					   unsigned long boot_freq)
{
	sgpu_devfreq_request_freq(adev, data->dm_type, boot_freq, &dp->initial_freq);
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	data->old_freq = dp->initial_freq;
#endif
}

int sgpu_devfreq_data_init(struct amdgpu_device *adev, struct devfreq *df)
{
	int ret = 0, i;
	uint32_t dt_freq;
	unsigned long max_freq, min_freq;
	unsigned long cal_maxfreq, cal_minfreq, boot_freq;
	struct device *dev = adev->dev;
	struct devfreq_dev_profile *dp = df->profile;
	struct devfreq_dev_status *stat = &df->last_status;
	struct sgpu_devfreq_data *data = kzalloc(sizeof(struct sgpu_devfreq_data),
						GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	data->adev = adev;
	data->devfreq = df;
	data->valid_time = sgpu_devfreq_polling_ms;
	data->cl_boost_disable = false;
	data->cl_boost_status = false;

	get_cal_min_max_freq(adev->cal_id, &cal_minfreq, &cal_maxfreq);

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

	for (i = 0; i < dp->max_state; i++) {
		uint32_t freq = dp->freq_table[i];

		if (freq == dp->initial_freq) {
			data->current_level = i;
			break;
		}
	}

	data->gvf_start_level = dp->max_state - sgpu_gvf_get_max_level(adev);

	sgpu_devfreq_dm_init(dev, data, dp, max_freq, min_freq);

	sgpu_devfreq_request_init_freq(adev, data, dp, boot_freq);

	/* BG3D_DVFS_CTL 0x058 enable */
	writel(0x1, adev->pm.pmu_mmio + 0x58);

	/* initialize utilization data */
	sgpu_utilization_init(&data->udata);

	stat->private_data = data;
	return 0;
}

void sgpu_devfreq_data_deinit(struct devfreq *df)
{
	struct devfreq_dev_status *stat = &df->last_status;

	kfree(stat->private_data);
	stat->private_data = NULL;
}

static uint64_t time_in_state[TABLE_MAX];
void sgpu_devfreq_time_in_state_update(struct amdgpu_device *adev, uint64_t diff_time)
{
	struct devfreq *df = adev->devfreq;
	int lev, prev_lev = df->profile->max_state;

	for (lev = 0; lev < df->profile->max_state; lev++) {
		if (df->previous_freq == df->profile->freq_table[lev]) {
			prev_lev = lev;
			break;
		}
	}

	time_in_state[prev_lev] += diff_time;
}

ssize_t sgpu_devfreq_time_in_state_show(struct amdgpu_device *adev, char *buf)
{
	struct devfreq *df = adev->devfreq;
	struct sgpu_pm_monitor *monitor = &adev->pm_monitor;
	int i;
	ssize_t count = 0;

	if (atomic_read(&df->suspend_count) > 0 || df->stop_polling)
		goto print;

	sgpu_pm_monitor_update(adev, monitor->pm_stats.power_status);

print:
	count += scnprintf(&buf[count], PAGE_SIZE - count,
			   "    FREQ    TIME(us)\n");
	for (i = 0; i < df->profile->max_state; i++) {
		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "%8lu  %10llu\n", df->profile->freq_table[i],
				   time_in_state[i] / 1000);
	}

	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n  IFPO  %12lluus\n",
			monitor->pm_stats.state_time[SGPU_POWER_STATE_IFPO] / 1000);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "  PWRUP %12lluus\n",
			monitor->pm_stats.state_time[SGPU_POWER_STATE_PWRUP] / 1000);

	return count;
}

void sgpu_devfreq_time_in_state_freq_change(struct amdgpu_device *adev)
{
	struct sgpu_pm_monitor *monitor = &adev->pm_monitor;

	if (monitor->pm_stats.power_status == SGPU_POWER_STATE_PWRUP)
		sgpu_pm_monitor_update(adev, SGPU_POWER_STATE_PWRUP);
}
