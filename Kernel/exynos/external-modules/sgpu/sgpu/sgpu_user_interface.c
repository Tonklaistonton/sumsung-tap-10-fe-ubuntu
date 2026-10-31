/*
 * @file sgpu_user_interface.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * @brief Contains functions for user interface and sysfs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/devfreq.h>
#include "sgpu_governor.h"
#include "sgpu_user_interface.h"
#include "sgpu_utilization.h"
#include "amdgpu.h"
#include "sgpu_devfreq.h"
#ifdef CONFIG_DRM_SGPU_EXYNOS
#include "exynos_gpu_interface.h"
#endif

static ssize_t dvfs_table_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	int i;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE,
			  "      freq voltage min max down(ms)\n");
	for (i = 0; i < df->profile->max_state; i++) {
		struct dev_pm_opp *target_opp;
		unsigned long freq = df->profile->freq_table[i];
		unsigned long volt = 0;

		target_opp = devfreq_recommended_opp(dev->parent, &freq, 0);
		if (freq == df->profile->freq_table[i] && !IS_ERR(target_opp))
			volt = dev_pm_opp_get_voltage(target_opp);
		else
			freq = df->profile->freq_table[i];
		dev_pm_opp_put(target_opp);

		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "%10lu %7lu %3u %3u %7u\n",
				   freq, volt,
				   data->min_thresholds[i], data->max_thresholds[i],
				   data->downstay_times[i]);
	}

	return count;
}
static DEVICE_ATTR_RO(dvfs_table);

static ssize_t tokenized_show(char *buf, struct devfreq_dev_profile *dp, int *array)
{
	ssize_t count = 0;
	int i;
	int value;
	value = array[dp->max_state - 1];
	count += scnprintf(&buf[count], PAGE_SIZE - count, "%d ", value);
	for (i = dp->max_state - 2; i >= 0; i--) {
		if (array[i] != value) {
			value = array[i];
			count += scnprintf(&buf[count], PAGE_SIZE - count, "%lu:%d ",
					   dp->freq_table[i],
					   array[i]);
		}
	}
	if (count)
		count--;
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");
	return count;
}

static ssize_t min_thresholds_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	return tokenized_show(buf, df->profile, data->min_thresholds);
}

static ssize_t min_thresholds_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	int *array;

	array = sgpu_get_array_data(df->profile, buf);
	if (IS_ERR(array)) {
		dev_err(dev, "fail min_thresholds tokenized to array\n");
		return PTR_ERR_OR_ZERO(array);
	}
	mutex_lock(&data->lock);
	kfree(data->min_thresholds);
	data->min_thresholds = array;
	mutex_unlock(&data->lock);
	return count;
}
static DEVICE_ATTR_RW(min_thresholds);

static ssize_t max_thresholds_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	return tokenized_show(buf, df->profile, data->max_thresholds);
}

static ssize_t max_thresholds_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	int *array;

	array = sgpu_get_array_data(df->profile, buf);
	if (IS_ERR(array)) {
		dev_err(dev, "fail max_thresholds tokenized to array\n");
		return PTR_ERR_OR_ZERO(array);
	}
	mutex_lock(&data->lock);
	kfree(data->max_thresholds);
	data->max_thresholds = array;
	mutex_unlock(&data->lock);
	return count;
}
static DEVICE_ATTR_RW(max_thresholds);

static ssize_t downstay_times_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	return tokenized_show(buf, df->profile, data->downstay_times);
}

static ssize_t downstay_times_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	uint32_t *array;

	array = sgpu_get_array_data(df->profile, buf);
	if (IS_ERR(array)) {
		dev_err(dev, "fail downstay_times tokenized to array\n");
		return PTR_ERR_OR_ZERO(array);
	}
	mutex_lock(&data->lock);
	kfree(data->downstay_times);
	data->downstay_times = array;
	mutex_unlock(&data->lock);

	return count;
}
static DEVICE_ATTR_RW(downstay_times);

static ssize_t available_utilization_sources_show(struct device *dev,
						  struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);

	return sgpu_utilization_all_src_show(df, buf);
}
static DEVICE_ATTR_RO(available_utilization_sources);

static ssize_t utilization_source_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	ssize_t count;

	count = sgpu_utilization_current_src_show(df, buf);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t utilization_source_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	int ret;
	char str_utilization_source[DEVFREQ_NAME_LEN + 1];

	ret = sscanf(buf, "%" __stringify(DEVFREQ_NAME_LEN) "s", str_utilization_source);
	if (ret != 1)
		return -EINVAL;

	ret = sgpu_utilization_src_change(df, str_utilization_source);
	if (ret) {
		dev_warn(dev, "%s: utilization_source %s not started(%d)\n",
			 __func__, str_utilization_source, ret);
		return ret;
	}

	return count;
}
static DEVICE_ATTR_RW(utilization_source);

static ssize_t valid_time_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_devfreq_data *data = df->last_status.private_data;
	return scnprintf(buf, PAGE_SIZE, "%u\n", data->valid_time);
}

static ssize_t valid_time_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_devfreq_data *data = df->last_status.private_data;
	uint32_t value;
	int ret;

	if (!df->governor)
		return -EINVAL;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	data->valid_time = value;

	ret = count;
	return ret;
}
static DEVICE_ATTR_RW(valid_time);

static ssize_t max_freq_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	return scnprintf(buf, PAGE_SIZE, "%lu\n",
			 data->sys_max_freq);
}

static ssize_t max_freq_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
	unsigned long value;
	int ret;

	ret = sscanf(buf, "%lu", &value);
	if (ret != 1)
		return -EINVAL;

	if (!dev_pm_qos_request_active(&df_data->sys_pm_qos_max))
		return -EINVAL;

	SGPU_LOG(df_data->adev, DMSG_INFO, DMSG_DVFS,
		 "MAX_REQUEST sys_pm_qos=%lu", value);
	DRM_INFO("[sgpu] MAX_REQUEST sys_pm_qos=%lu", value);
	data->sys_max_freq = value;
	dev_pm_qos_update_request(&df_data->sys_pm_qos_max, value / HZ_PER_KHZ);

	ret = count;
	return ret;
}
static DEVICE_ATTR_RW(max_freq);

static ssize_t min_freq_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	return scnprintf(buf, PAGE_SIZE, "%lu\n",
			 data->sys_min_freq);
}

static ssize_t min_freq_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	struct sgpu_devfreq_data *df_data = df->last_status.private_data;
	struct amdgpu_device *adev = data->adev;
	unsigned long value;
	int ret;

	ret = sscanf(buf, "%lu", &value);
	if (ret != 1)
		return -EINVAL;

	if (!dev_pm_qos_request_active(&df_data->sys_pm_qos_min))
		return -EINVAL;

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS,
		 "MIN_REQUEST sys_pm_qos=%lu", value);
	DRM_INFO("[sgpu] MIN_REQUEST sys_pm_qos=%lu", value);
	data->sys_min_freq = value;
	dev_pm_qos_update_request(&df_data->sys_pm_qos_min, value / HZ_PER_KHZ);

	ret = count;
	return ret;
}
static DEVICE_ATTR_RW(min_freq);

static ssize_t wakeup_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	uint32_t value;
	int ret;

	if (!df->governor)
		return -EINVAL;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->wakeup_lock = value ? true : false;
	mutex_unlock(&data->lock);

	ret = count;
	return ret;
}

static ssize_t wakeup_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	return scnprintf(buf, PAGE_SIZE, "%d\n", data->wakeup_lock);
}
static DEVICE_ATTR_RW(wakeup);

static ssize_t highspeed_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct devfreq_dev_profile *dp = df->profile;
	struct sgpu_governor_data *data = df->governor_data;
	unsigned long freq, i;

	if (sscanf(buf, "%lu", &freq) != 1)
		return -EINVAL;

	data->highspeed_freq = freq;
	data->highspeed_level = 0;
	for(i = 1; i < dp->max_state; i++) {
		if(dp->freq_table[i] >= data->highspeed_freq)
			data->highspeed_level = i;
		else
			break;
	}

	buf = strpbrk(buf, " ");
	if (buf == NULL)
		return -EINVAL;

	buf++;
	if (sscanf(buf, "%u", &data->highspeed_load) != 1)
		return -EINVAL;

	buf = strpbrk(buf, " ");
	if (buf == NULL)
		return -EINVAL;

	buf++;
	if (sscanf(buf, "%u", &data->highspeed_delay) != 1)
		return -EINVAL;
	return count;
}

static ssize_t highspeed_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE,
			  "highspeed freq load delay\n");
	count += scnprintf(&buf[count], PAGE_SIZE - count,
			   "     %9lu %4u %5u\n",
			   data->highspeed_freq, data->highspeed_load,
			   data->highspeed_delay);

	return count;
}
static DEVICE_ATTR_RW(highspeed);

static ssize_t available_governors_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);

	return sgpu_governor_all_info_show(df, buf);
}
static DEVICE_ATTR_RO(available_governors);

static ssize_t governor_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	ssize_t count;

	count = sgpu_governor_current_info_show(df, buf, PAGE_SIZE);
	if (count < 0) {
		DRM_WARN("Error reading current gpu governor info: %zd\n", count);
		return -EINVAL;
	}
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t governor_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	int ret;
	char str_governor[DEVFREQ_NAME_LEN + 1];

	if (!df->governor || !df->governor_data)
		return -EINVAL;
	ret = sscanf(buf, "%" __stringify(DEVFREQ_NAME_LEN) "s", str_governor);
	if (ret != 1)
		return -EINVAL;

	ret = sgpu_governor_change(df, str_governor);
	if (ret) {
		dev_warn(dev, "%s: Governor %s not started(%d)\n",
			 __func__, df->governor->name, ret);
		return ret;
	}

	if (!ret) {
		sgpu_governor_current_info_show(df, str_governor,
						sizeof(str_governor));
		dev_info(dev, "governor : %s\n", str_governor);
		ret = count;
	}
	return ret;
}
static DEVICE_ATTR_RW(governor);

static ssize_t power_ratio_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%u\n", data->power_ratio);

	return count;
}

static ssize_t power_ratio_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	uint32_t value;
	int ret;

	ret = sscanf(buf, "%u", &value);
	if (ret != 1)
		return -EINVAL;

	mutex_lock(&data->lock);
	data->power_ratio = value;
	mutex_unlock(&data->lock);

	ret = count;
	return ret;
}

static DEVICE_ATTR_RW(power_ratio);

static ssize_t current_utilization_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data udata = ((struct sgpu_devfreq_data *)stat->private_data)->udata;
	uint64_t utilization = udata.last_util;

	ssize_t count;
	count = scnprintf(buf, PAGE_SIZE, "%llu\n", utilization);
	return count;
}

static DEVICE_ATTR_RO(current_utilization);

static ssize_t current_cu_utilization_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct devfreq_dev_status *stat = &df->last_status;
	struct utilization_data udata = ((struct sgpu_devfreq_data *)stat->private_data)->udata;
	uint64_t cu_utilization = udata.last_cu_util;
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%llu\n", cu_utilization);
	return count;
}

static DEVICE_ATTR_RO(current_cu_utilization);


static ssize_t compute_weight_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	ssize_t count = 0;

	count = scnprintf(buf, PAGE_SIZE, "%d\n",
			  data->compute_weight);

	return count;
}

static ssize_t compute_weight_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct sgpu_governor_data *data = df->governor_data;
	int compute_weight, ret;

	ret = sscanf(buf, "%d", &compute_weight);
	if (ret != 1 || compute_weight > 10000 || compute_weight < 100) {
		dev_warn(dev, "compute_weight range : (100 ~ 10000)\n");
		return -EINVAL;
	}
	data->compute_weight = compute_weight;

	return count;

}
static DEVICE_ATTR_RW(compute_weight);


static struct attribute *sgpu_devfreq_sysfs_entries[] = {
	/* just show */
	&dev_attr_dvfs_table.attr,
	&dev_attr_available_governors.attr,
	&dev_attr_available_utilization_sources.attr,
	&dev_attr_current_utilization.attr,
	&dev_attr_current_cu_utilization.attr,
	/* show and set */
	&dev_attr_highspeed.attr,
	&dev_attr_utilization_source.attr,
	&dev_attr_governor.attr,
	&dev_attr_wakeup.attr,
	&dev_attr_min_freq.attr,
	&dev_attr_max_freq.attr,
	&dev_attr_valid_time.attr,
	&dev_attr_min_thresholds.attr,
	&dev_attr_max_thresholds.attr,
	&dev_attr_downstay_times.attr,
	&dev_attr_power_ratio.attr,
	&dev_attr_compute_weight.attr,
	NULL,
};

static struct attribute_group sgpu_devfreq_attr_group = {
	.name = "interface",
	.attrs = sgpu_devfreq_sysfs_entries,
};

int sgpu_create_sysfs_file(struct devfreq *df)
{
	int ret = 0;
	ret = sysfs_create_group(&df->dev.kobj, &sgpu_devfreq_attr_group);
	if (ret)
		dev_err(df->dev.parent, "failed create sysfs for governor data\n");

	return ret;
}

void sgpu_remove_sysfs_file(struct devfreq *df)
{
	sysfs_remove_group(&df->dev.kobj, &sgpu_devfreq_attr_group);

#if IS_ENABLED(CONFIG_EXYNOS_GPU_PROFILER)
	sysfs_remove_group(&df->dev.kobj, &sgpu_profiler_attr_group);
#endif /* CONFIG_EXYNOS_GPU_PROFILER */
}
