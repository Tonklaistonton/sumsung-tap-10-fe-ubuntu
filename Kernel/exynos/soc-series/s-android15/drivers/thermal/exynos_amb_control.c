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

#include <soc/samsung/exynos_amb_control.h>
#include <soc/samsung/exynos-mcinfo.h>
#include <linux/ems.h>
#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif

#include "exynos_tmu.h"

static struct exynos_amb_control_data *amb_control;

static int exynos_amb_control_set_polling(struct exynos_amb_control_data *data,
		unsigned int delay)
{
	kthread_mod_delayed_work(&data->amb_worker, &data->amb_dwork,
			msecs_to_jiffies(delay));
	return 0;
}

/* sysfs nodes for amb control */
#define sysfs_printf(...) count += snprintf(buf + count, PAGE_SIZE, __VA_ARGS__)
static ssize_t
exynos_amb_control_info_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0, i;

	mutex_lock(&data->lock);

	sysfs_printf("Exynos AMB Control Informations\n\n");
	sysfs_printf("AMB Thermal Zone Info\n");
	sysfs_printf("AMB thermal zone name : %s\n", data->amb_tzd->type);
	sysfs_printf("Default sampling rate : %u\n", data->default_sampling_rate);
	sysfs_printf("High sampling rate : %u\n", data->high_sampling_rate);
	sysfs_printf("AMB switch_on : %u\n", data->amb_switch_on);
	sysfs_printf("==============================================\n");

	if (data->use_mif_throttle) {
		sysfs_printf("MIF throttle temp : %u\n", data->mif_down_threshold);
		sysfs_printf("MIF release temp : %u\n", data->mif_up_threshold);
		sysfs_printf("MIF throttle freq : %u\n", data->mif_throttle_freq);
		sysfs_printf("==============================================\n");
	}

	for (i = 0; i < data->num_amb_tz_configs; i++) {
		struct ambient_thermal_zone_config *tz_config = &data->amb_tz_config[i];

		if (cpumask_empty(&tz_config->cpu_domain))
			continue;

		sysfs_printf("CPU hotplug domains : 0x%x\n", *(unsigned int *)cpumask_bits(&tz_config->cpu_domain));
		sysfs_printf("CPU hotplug out temp : %u\n", tz_config->hotplug_out_threshold);
		sysfs_printf("CPU hotplug in temp : %u\n", tz_config->hotplug_in_threshold);

		sysfs_printf("==============================================\n");
	}

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
exynos_amb_control_set_config_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0, i;

	sysfs_printf("Usage for set_config node\n");

	sysfs_printf("[0] AMB thermal zone config (sampling rate/temp)\n");
	sysfs_printf("\t# echo 0 [type] [value] > amb_control_set_config\n");
	sysfs_printf("\ttype : 0/1/2/3=default_sampling_rate/high_sampling_rate/amb_switch_on\n");

	sysfs_printf("[1] CPU Hotplug config (hotplug temp/domain)\n");
	sysfs_printf("\t# echo 1 [zone] [type] [value] > amb_control_set_config\n");
	sysfs_printf("\tzone : ");
	for (i = 0; i < data->num_amb_tz_configs - 1; i++)
		sysfs_printf("%u/", i);
	sysfs_printf("%u=", i);
	sysfs_printf("\ttype : 0/1/2/3=hotplug_in_threshold/hotplug_out_threshold/hotplug_enable/cpu_domain(hex)\n");

	return count;
}

static ssize_t
exynos_amb_control_set_config_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	struct ambient_thermal_zone_config *tz_config = NULL;
	unsigned int id, zone, type, value;
	char string[20];

	mutex_lock(&data->lock);

	if (sscanf(buf, "%d ", &id) != 1)
		goto out;

	if (id == 0) {
		if (sscanf(buf, "%u %u %u", &id, &type, &value) != 3)
			goto out;

		switch (type) {
			case 0:
				data->default_sampling_rate = value;
				break;
			case 1:
				data->high_sampling_rate = value;
				break;
			case 2:
				data->amb_switch_on = value;
				break;
			default:
				break;
		}
	} else if (id == 1) {
		if (sscanf(buf, "%u %u %u ", &id, &zone, &type) != 3)
			goto out;

		if (type == 3) {
			if (sscanf(buf, "%u %u %u 0x%x", &id, &zone, &type, &value) != 4)
				goto out;
		} else {
			if (sscanf(buf, "%u %u %u %u", &id, &zone, &type, &value) != 4)
				goto out;
		}

		if (zone >= data->num_amb_tz_configs)
			goto out;

		tz_config = &data->amb_tz_config[zone];

		switch (type) {
			case 0:
				if (value < 125000)
					tz_config->hotplug_in_threshold = value;
				break;
			case 1:
				if (value < 125000)
					tz_config->hotplug_out_threshold = value;
				break;
			case 2:
				tz_config->hotplug_enable = !!value;
				break;
			case 3:
				snprintf(string, sizeof(string), "%x", value);
				cpumask_parse(string, &tz_config->cpu_domain);
				break;
			default:
				break;
		}
	}

out:
	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
exynos_amb_emul_temp_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0;

	mutex_lock(&data->lock);

	sysfs_printf("%d\n", data->emul_temp);

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
exynos_amb_emul_temp_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	u32 emul_temp;

	mutex_lock(&data->lock);

	sscanf(buf, "%u", &emul_temp);
	data->emul_temp = emul_temp;

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
exynos_amb_control_mode_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0;

	mutex_lock(&data->lock);

	sysfs_printf("%s\n", data->amb_disabled ? "disabled" : "enabled");

	mutex_unlock(&data->lock);

	return count;
}

static ssize_t
exynos_amb_control_mode_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	int new_mode, prev_mode;

	mutex_lock(&data->lock);

	sscanf(buf, "%d", &new_mode);

	prev_mode = data->amb_disabled;
	data->amb_disabled = !!new_mode;

	mutex_unlock(&data->lock);

	if (data->amb_disabled && !prev_mode) {
		kthread_cancel_delayed_work_sync(&data->amb_dwork);
		pr_info("%s: amb control is disabled\n", __func__);
	} else if (!data->amb_disabled && prev_mode) {
		exynos_amb_control_set_polling(data, data->default_sampling_rate);
		pr_info("%s: amb control is enabled\n", __func__);
	}

	return count;
}

static DEVICE_ATTR(amb_control_info, S_IRUGO, exynos_amb_control_info_show, NULL);
static DEVICE_ATTR(amb_control_set_config, S_IWUSR | S_IRUGO,
		exynos_amb_control_set_config_show, exynos_amb_control_set_config_store);
static DEVICE_ATTR(amb_control_disable, S_IWUSR | S_IRUGO,
		exynos_amb_control_mode_show, exynos_amb_control_mode_store);
static DEVICE_ATTR(emul_temp, S_IWUSR | S_IRUGO,
		exynos_amb_emul_temp_show, exynos_amb_emul_temp_store);

static struct attribute *exynos_amb_control_attrs[] = {
	&dev_attr_amb_control_info.attr,
	&dev_attr_amb_control_set_config.attr,
	&dev_attr_amb_control_disable.attr,
	&dev_attr_emul_temp.attr,
	NULL,
};

static const struct attribute_group exynos_amb_control_attr_group = {
	.attrs = exynos_amb_control_attrs,
};

/* what does amb do ?
 *
 * set hotplug in/out theshold when amb temp is reaches on given value
 * change control temp of cpu
 * change throttle temp of ISP
 */
static int exynos_amb_controller(struct exynos_amb_control_data *data)
{
	unsigned int i;
	int amb_temp = 0;
	struct cpumask mask;

	/* Get temperature */
	thermal_zone_get_temp(data->amb_tzd, &amb_temp);

	if (data->emul_temp)
		amb_temp = data->emul_temp;

	exynos_mcinfo_set_ambient_status(amb_temp > data->mcinfo_threshold);

	cpumask_copy(&mask, cpu_possible_mask);

	/* Check MIF throttle condition */
	if (data->use_mif_throttle) {
		if (amb_temp > data->mif_down_threshold)
			exynos_pm_qos_update_request(&data->amb_mif_qos, data->mif_throttle_freq);
		else if (amb_temp <= data->mif_up_threshold)
			exynos_pm_qos_update_request(&data->amb_mif_qos, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	}

	/* Traverse all amb tz config */
	for (i = 0; i < data->num_amb_tz_configs; i++) {
		struct ambient_thermal_zone_config *tz_config = &data->amb_tz_config[i];

		/* Check hotplug condition */
		if (!cpumask_empty(&tz_config->cpu_domain)) {
			if (tz_config->is_cpu_hotplugged_out)
				cpumask_andnot(&mask, &mask, &tz_config->cpu_domain);

			if (!tz_config->is_cpu_hotplugged_out && amb_temp > tz_config->hotplug_out_threshold) {
				tz_config->is_cpu_hotplugged_out = true;
				cpumask_andnot(&mask, &mask, &tz_config->cpu_domain);
			} else if (tz_config->is_cpu_hotplugged_out && amb_temp <= tz_config->hotplug_in_threshold) {
				tz_config->is_cpu_hotplugged_out = false;
				cpumask_or(&mask, &mask, &tz_config->cpu_domain);
			}
		}
	}

	if (!cpumask_equal(&data->cpuhp_req_mask, &mask)) {
		if (data->hotplug_completely_off)
			exynos_cpuhp_update_request("amb_cpuhp", &mask);
		else
			ecs_request("amb_cpuhp", &mask, ECS_MAX);
		cpumask_copy(&data->cpuhp_req_mask, &mask);
	}

	if (amb_temp > data->amb_switch_on)
		data->period_amb = data->high_sampling_rate;
	else
		data->period_amb = data->default_sampling_rate;

	return 0;
}

static int exynos_mif_controller(struct exynos_amb_control_data *data)
{
	int mif_temp = 0;
	unsigned long now = jiffies;

	if (msecs_to_jiffies(now - data->last_update_time) < 250) {
		return 0;
	}

	data->last_update_time = now;

	/* Get temperature */
	thermal_zone_get_temp(data->mif_tzd, &mif_temp);

	data->mif_temp = mif_temp;

	if (mif_temp > data->mif_hot_on_threshold) {
		llc_set_enable(DISABLE_LLC, LLC_ENABLE_THERMAL);
		exynos_pm_qos_update_request(&data->mif_hot_qos,
					     data->mif_hot_throttle_freq);
	} else if (mif_temp <= data->mif_hot_off_threshold){
		llc_set_enable(ENABLE_LLC, LLC_ENABLE_THERMAL);
		exynos_pm_qos_update_request(&data->mif_hot_qos,
					PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	}

	if (mif_temp > data->mif_hot_switch_on)
		data->period_mif = data->high_sampling_rate;
	else
		data->period_mif = data->default_sampling_rate;

	return 0;
}

static void exynos_amb_control_work_func(struct kthread_work *work)
{
	struct exynos_amb_control_data *data =
			container_of(work, struct exynos_amb_control_data, amb_dwork.work);
	u32 period = data->default_sampling_rate;

	mutex_lock(&data->lock);

	if (data->in_suspend || data->amb_disabled) {
		pr_info("%s: amb control is %s\n", __func__,
			data->in_suspend ? "suspended" : "disabled");
		goto out;
	}

	exynos_amb_controller(data);
	exynos_mif_controller(data);

	period = min(data->period_amb, data->period_mif);
	exynos_amb_control_set_polling(data, period);

out:
	mutex_unlock(&data->lock);
};

static int exynos_amb_control_work_init(struct platform_device *pdev)
{
	struct cpumask mask;
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 4 - 1 };
	struct task_struct *thread;
	int ret = 0;

	kthread_init_worker(&data->amb_worker);
	thread = kthread_create(kthread_worker_fn, &data->amb_worker,
			"thermal_amb");
	if (IS_ERR(thread)) {
		dev_err(&pdev->dev, "failed to create amb worker thread: %ld\n",
				PTR_ERR(thread));
		return PTR_ERR(thread);
	}

	cpulist_parse("0-3", &mask);
	cpumask_and(&mask, cpu_possible_mask, &mask);
	set_cpus_allowed_ptr(thread, &mask);

	ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
	if (ret) {
		kthread_stop(thread);
		dev_warn(&pdev->dev, "failed to set amb worker thread as SCHED_FIFO\n");
		return ret;
	}

	kthread_init_delayed_work(&data->amb_dwork, exynos_amb_control_work_func);
	exynos_amb_control_set_polling(data, 0);

	wake_up_process(thread);

	return ret;
}

static int exynos_amb_control_parse_dt(struct platform_device *pdev)
{
	const char *buf;
	struct device_node *np, *child;
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);
	int i = 0;

	np = pdev->dev.of_node;

	/* Get common amb_control configs */
	if (of_property_read_string(np, "amb_tz_name", &buf)) {
		dev_err(&pdev->dev, "failed to get amb_tz_name\n");
		return -ENODEV;
	}

	data->amb_tzd = thermal_zone_get_zone_by_name(buf);
	if (!data->amb_tzd) {
		dev_err(&pdev->dev, "failed to get amb_tzd\n");
		return -ENODEV;
	}

	dev_info(&pdev->dev, "Amb tz name %s\n", data->amb_tzd->type);

	if (of_property_read_string(np, "mif_tz_name", &buf)) {
		dev_err(&pdev->dev, "failed to get mif_tz_name\n");
		return -ENODEV;
	}

	data->mif_tzd = thermal_zone_get_zone_by_name(buf);
	if (!data->mif_tzd) {
		dev_err(&pdev->dev, "failed to get mif_tzd\n");
		return -ENODEV;
	}

	dev_info(&pdev->dev, "MIF tz name %s\n", data->mif_tzd->type);

	if (of_property_read_u32(np, "default_sampling_rate", &data->default_sampling_rate))
		data->default_sampling_rate = DEFAULT_SAMPLING_RATE;

	dev_info(&pdev->dev, "Default sampling rate (%u)\n", data->default_sampling_rate);

	if (of_property_read_u32(np, "high_sampling_rate", &data->high_sampling_rate))
		data->high_sampling_rate = HIGH_SAMPLING_RATE;
	dev_info(&pdev->dev, "High sampling rate (%u)\n", data->high_sampling_rate);

	if (of_property_read_u32(np, "amb_switch_on", &data->amb_switch_on))
		data->amb_switch_on = AMB_SWITCH_ON_TEMP;
	dev_info(&pdev->dev, "amb siwtch on temp (%u)\n", data->amb_switch_on);

	if (of_property_read_u32(np, "mif_hot_switch_on", &data->mif_hot_switch_on))
		data->mif_hot_switch_on = MIF_HOT_SWITCH_ON_TEMP;
	dev_info(&pdev->dev, "mif_hot switch on temp (%u)\n", data->mif_hot_switch_on);

	if (of_property_read_u32(np, "mcinfo_threshold", &data->mcinfo_threshold))
		data->mcinfo_threshold = MCINFO_THRESHOLD_TEMP;
	dev_info(&pdev->dev, "mcinfo_threshold (%u)\n", data->mcinfo_threshold);

	data->period_amb = data->default_sampling_rate;
	data->period_mif = data->default_sampling_rate;
	dev_info(&pdev->dev, "Current sampling rate (%u)\n", data->default_sampling_rate);

	data->use_mif_throttle = of_property_read_bool(np, "use_mif_throttle");
	dev_info(&pdev->dev, "use mif throttle (%s)\n", data->use_mif_throttle ? "true" : "false");
	if (data->use_mif_throttle) {
		exynos_pm_qos_add_request(&data->amb_mif_qos,
				PM_QOS_BUS_THROUGHPUT_MAX, PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);

		if (of_property_read_u32(np, "mif_down_threshold", &data->mif_down_threshold))
			data->mif_down_threshold = MIF_DOWN_THRESHOLD_TEMP;
		dev_info(&pdev->dev, "mif down threshold (%u)\n", data->mif_down_threshold);

		if (of_property_read_u32(np, "mif_up_threshold", &data->mif_up_threshold))
			data->mif_up_threshold = MIF_UP_THRESHOLD_TEMP;
		dev_info(&pdev->dev, "mif up threshold (%u)\n", data->mif_up_threshold);

		if (of_property_read_u32(np, "mif_throttle_freq", &data->mif_throttle_freq))
			data->mif_throttle_freq = MIF_THROTTLE_FREQ;
		dev_info(&pdev->dev, "mif throttle freq (%u)\n", data->mif_throttle_freq);
	}

	if (of_property_read_u32(np, "mif_hot_off_threshold", &data->mif_hot_off_threshold))
		data->mif_hot_off_threshold = MIF_HOT_OFF_THRESHOLD_TEMP;
	dev_info(&pdev->dev, "mif_hot off threshold (%u)\n", data->mif_hot_off_threshold);

	if (of_property_read_u32(np, "mif_hot_on_threshold", &data->mif_hot_on_threshold))
		data->mif_hot_on_threshold = MIF_HOT_ON_THRESHOLD_TEMP;
	dev_info(&pdev->dev, "mif_hot on threshold (%u)\n", data->mif_hot_on_threshold);

	if (of_property_read_u32(np, "mif_hot_throttle_freq", &data->mif_hot_throttle_freq))
		data->mif_hot_throttle_freq = MIF_HOT_THROTTLE_FREQ;
	dev_info(&pdev->dev, "mif_hot throttle freq (%u)\n", data->mif_hot_throttle_freq);
	exynos_pm_qos_add_request(&data->mif_hot_qos, PM_QOS_BUS_THROUGHPUT_MAX,
				  PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);

	if (of_property_read_bool(np, "hotplug_completely_off"))
			data->hotplug_completely_off = true;

	/* Get configs for each thermal zones */
	data->num_amb_tz_configs = of_get_child_count(np);
	data->amb_tz_config = kzalloc(sizeof(struct ambient_thermal_zone_config) *
			data->num_amb_tz_configs, GFP_KERNEL);

	for_each_available_child_of_node(np, child) {
		struct ambient_thermal_zone_config *tz_config = &data->amb_tz_config[i++];

		// set cpu hotplug
		if (!of_property_read_string(child, "hotplug_cpu_list", &buf)) {
			cpulist_parse(buf, &tz_config->cpu_domain);
			dev_info(&pdev->dev, "Hotplug control for CPU %s\n", buf);
			if (of_property_read_u32(child, "hotplug_in_threshold",
					&tz_config->hotplug_in_threshold))
				tz_config->hotplug_in_threshold = HOTPLUG_IN_THRESHOLD;
			dev_info(&pdev->dev, "Hotplug in threshold (%u)\n",
					tz_config->hotplug_in_threshold);
			if (of_property_read_u32(child, "hotplug_out_threshold",
					&tz_config->hotplug_out_threshold))
				tz_config->hotplug_out_threshold = HOTPLUG_OUT_THRESHOLD;
			dev_info(&pdev->dev, "Hotplug out threshold (%u)\n",
					tz_config->hotplug_out_threshold);
		}
	}

	if (data->hotplug_completely_off)
		exynos_cpuhp_add_request("amb_cpuhp", cpu_possible_mask);
	else
		ecs_request_register("amb_cpuhp", cpu_possible_mask, ECS_MAX);

	return 0;
}
static int exynos_amb_control_check_devs_dep(struct platform_device *pdev)
{
	const char *buf;
	struct device_node *np, *child;

	np = pdev->dev.of_node;

	/* Check dependency of all drivers */
	if (!of_property_read_string(np, "amb_tz_name", &buf)) {
		if (PTR_ERR(thermal_zone_get_zone_by_name(buf)) == -ENODEV) {
			dev_err(&pdev->dev, "amb thermal zone is not registered!\n");
			return -EPROBE_DEFER;
		}
	} else {
		dev_err(&pdev->dev, "ambient thermal zone is not defined!\n");
		return -ENODEV;
	}

	for_each_available_child_of_node(np, child) {
		struct device_node *tz_np = of_parse_phandle(child, "tz", 0);

		if (tz_np == NULL) {
			dev_err(&pdev->dev, "thermal zone is not defined!\n");
			return -ENODEV;
		}

		if (PTR_ERR(thermal_zone_get_zone_by_name(tz_np->name)) == -ENODEV) {
			dev_err(&pdev->dev, "%s thermal zone is not registered!\n", tz_np->name);
			return -EPROBE_DEFER;
		}
	}

	return 0;
}

static int exynos_amb_control_pm_notify(struct notifier_block *nb,
			     unsigned long mode, void *_unused)
{
	struct exynos_amb_control_data *data = container_of(nb,
			struct exynos_amb_control_data, pm_notify);

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		mutex_lock(&data->lock);
		data->in_suspend = true;
		mutex_unlock(&data->lock);
		kthread_cancel_delayed_work_sync(&data->amb_dwork);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&data->lock);
		data->in_suspend = false;
		mutex_unlock(&data->lock);
		exynos_amb_control_set_polling(data, data->default_sampling_rate);
		break;
	default:
		break;
	}
	return 0;
}

static int exynos_amb_control_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = exynos_amb_control_check_devs_dep(pdev);

	if (ret)
		return ret;

	amb_control = kzalloc(sizeof(struct exynos_amb_control_data), GFP_KERNEL);
	platform_set_drvdata(pdev, amb_control);

	ret = exynos_amb_control_parse_dt(pdev);

	if (ret) {
		dev_err(&pdev->dev, "failed to parse dt (%d)\n", ret);
		kfree(amb_control);
		amb_control = NULL;
		return ret;
	}

	mutex_init(&amb_control->lock);

	exynos_amb_control_work_init(pdev);

	amb_control->pm_notify.notifier_call = exynos_amb_control_pm_notify;
	register_pm_notifier(&amb_control->pm_notify);

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_amb_control_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create exynos amb control attr group");

	dev_info(&pdev->dev, "Probe exynos amb controller successfully\n");

	return 0;
}

static int exynos_amb_control_remove(struct platform_device *pdev)
{
	struct exynos_amb_control_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->lock);

	data->amb_disabled = true;

	mutex_unlock(&data->lock);

	kthread_cancel_delayed_work_sync(&data->amb_dwork);

	return 0;
}

static const struct of_device_id exynos_amb_control_match[] = {
	{ .compatible = "samsung,exynos-amb-control", },
	{ /* sentinel */ },
};

static struct platform_driver exynos_amb_control_driver = {
	.driver = {
		.name   = "exynos-amb-control",
		.of_match_table = exynos_amb_control_match,
		.suppress_bind_attrs = true,
	},
	.probe = exynos_amb_control_probe,
	.remove	= exynos_amb_control_remove,
};
module_platform_driver(exynos_amb_control_driver);

MODULE_DEVICE_TABLE(of, exynos_amb_control_match);

MODULE_AUTHOR("Hanjun Shin <hanjun.shin@samsung.com>");
MODULE_DESCRIPTION("EXYNOS AMB CONTROL Driver");
MODULE_LICENSE("GPL");
