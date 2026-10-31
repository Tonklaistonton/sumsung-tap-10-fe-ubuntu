/*
 * sec_pm_regulator.c
 *
 *  Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author: Jonghyeon Cho <jongjaaa.cho@samsung.com>
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/of.h>
#include <linux/sec_class.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>

#define DEFAULT_LATE_CLEANUP_WAKELOCK_TIMEOUT_MS 35000
#define MAX_BUF_SIZE 40

struct sec_pm_regulator_info {
	struct device *dev;
	struct device *sec_pm_regulator_dev;

	struct regulator *reg;
	char regulator_name[MAX_BUF_SIZE];
	int saved_voltage;
	unsigned int enable_count;

	struct wakeup_source *regulator_init_ws;
	unsigned int late_cleanup_wakelock_timeout;
};

static int sec_pm_regulator_parse_dt(struct platform_device *pdev)
{
	struct sec_pm_regulator_info *info = platform_get_drvdata(pdev);

	if (!info || !pdev->dev.of_node)
		return -ENODEV;

	if (of_property_read_u32(pdev->dev.of_node, "late_cleanup_wakelock",
							&info->late_cleanup_wakelock_timeout))
		info->late_cleanup_wakelock_timeout = DEFAULT_LATE_CLEANUP_WAKELOCK_TIMEOUT_MS;

	return 0;
}

static int disable_all_enable_counts(struct device *dev,
						struct sec_pm_regulator_info *info)
{
	int ret = 0;

	while (info->enable_count > 0) {
		ret = regulator_disable(info->reg);
		if (ret) {
			dev_err(dev, "%s: failed to disable regulator(%d)\n", __func__, ret);
			return ret;
		}
		info->enable_count--;
	}

	return ret;
}

static ssize_t cur_regulator_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_regulator_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", info->regulator_name);
}

static ssize_t cur_regulator_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct sec_pm_regulator_info *info = dev_get_drvdata(dev);
	char supply_regulator[MAX_BUF_SIZE] = {0,};
	int ret, voltage;
	struct regulator *reg;

	if (strlen(buf) >= MAX_BUF_SIZE)
		return -EINVAL;

	ret = sscanf(buf, "%s", supply_regulator);
	if (ret != 1) {
		dev_err(dev, "%s: invalid args(%d)\n", __func__, ret);
		return -EINVAL;
	}

	if (info->reg) {
		ret = regulator_set_voltage(info->reg, info->saved_voltage, info->saved_voltage);
		if (ret) {
			dev_err(dev, "%s: failed to set voltage(%d)\n", __func__, ret);
			return count;
		}
		ret = disable_all_enable_counts(dev, info);
		if (ret)
			return count;
		regulator_put(info->reg);
		info->reg = 0;
		info->regulator_name[0] = '\0';
	}

	if (!strncmp(supply_regulator, "-1", 2))
		return count;

	reg = devm_regulator_get(dev, supply_regulator);
	if (IS_ERR(reg)) {
		dev_err(dev, "%s: no such regulator\n", __func__);
		return count;
	}

	voltage = regulator_get_voltage(reg);
	if (voltage < 0) {
		dev_err(dev, "%s: failed to get voltage information(%d)\n", __func__, voltage);
		return count;
	}

	info->saved_voltage = voltage;
	dev_info(dev, "%s: store the initial voltage of %s (%d uA)\n", __func__, supply_regulator, voltage);
	info->reg = reg;
	strncpy(info->regulator_name, supply_regulator, sizeof(info->regulator_name));

	return count;
}

static ssize_t enable_regulator_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_regulator_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->reg)
		return sprintf(buf, "%s: current regulator is not set\n", __func__);

	ret = regulator_is_enabled(info->reg);
	if (ret < 0)
		return sprintf(buf, "%s: failed to get enable/disable information(%d)\n", __func__, ret);

	return sprintf(buf, "%s\n", ret ? "enabled" : "disabled");
}

static ssize_t enable_regulator_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct sec_pm_regulator_info *info = dev_get_drvdata(dev);
	bool state;
	int ret;

	ret = kstrtobool(buf, &state);
	if (ret) {
		dev_err(dev, "%s: invalid args(%d)\n", __func__, ret);
		return -EINVAL;
	}

	if (!info->reg) {
		dev_err(dev, "%s: current regulator is not set\n", __func__);
		return count;
	}

	if (state) {
		ret = regulator_enable(info->reg);
		if (ret) {
			dev_err(dev, "%s: failed to enable regulator(%d)\n", __func__, ret);
			return count;
		}
		info->enable_count++;
	} else {
		ret = disable_all_enable_counts(dev, info);
		if (ret)
			return count;
	}

	return count;
}

static ssize_t get_voltage_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_regulator_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->reg)
		return sprintf(buf, "%s: current regulator is not set\n", __func__);

	ret = regulator_get_voltage(info->reg);
	if (ret < 0)
		return sprintf(buf, "%s: failed to get voltage information(%d)\n", __func__, ret);

	return sprintf(buf, "%d\n", ret);
}

static ssize_t set_voltage_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct sec_pm_regulator_info *info = dev_get_drvdata(dev);
	int min_uV, max_uV;
	int ret;

	ret = sscanf(buf, "%d %d", &min_uV, &max_uV);

	/* If only one argument is entered
	 * that value is used for both min_uV and max_uV.
	 */
	if (ret == 1)
		max_uV = min_uV;
	else if (ret != 2) {
		dev_err(dev, "%s: invalid args(%d)\n", __func__, ret);
		return -EINVAL;
	}

	if (!info->reg) {
		dev_err(dev, "%s: current regulator is not set\n", __func__);
		return count;
	}

	ret = regulator_set_voltage(info->reg, min_uV, max_uV);
	if (ret)
		dev_err(dev, "%s: failed to set voltage(%d)\n", __func__, ret);

	return count;
}

static DEVICE_ATTR_RW(cur_regulator);
static DEVICE_ATTR_RW(enable_regulator);
static DEVICE_ATTR_RO(get_voltage);
static DEVICE_ATTR_WO(set_voltage);

static struct attribute *sec_pm_regulator_attrs[] = {
	&dev_attr_cur_regulator.attr,
	&dev_attr_enable_regulator.attr,
	&dev_attr_get_voltage.attr,
	&dev_attr_set_voltage.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_regulator);

static int sec_pm_regulator_probe(struct platform_device *pdev)
{
	struct sec_pm_regulator_info *info;
	struct device *sec_pm_regulator_dev;
	int ret = 0;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: Fail to alloc info\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	ret = sec_pm_regulator_parse_dt(pdev);
	if (ret) {
		dev_err(info->dev, "%s: Fail to parse device tree\n", __func__);
		goto probe_end;
	}

	sec_pm_regulator_dev = sec_device_create(info, "sec_pm_regulator");

	if (IS_ERR(sec_pm_regulator_dev)) {
		dev_err(info->dev, "%s: failed to create sec_pm_regulator\n", __func__);
		ret = PTR_ERR(sec_pm_regulator_dev);
		goto probe_end;
	}

	ret = sysfs_create_groups(&sec_pm_regulator_dev->kobj, sec_pm_regulator_groups);
	if (ret) {
		dev_err(info->dev, "%s: failed to create sysfs groups(%d)\n",
				__func__, ret);
		goto err_create_sysfs;
	}

	info->regulator_init_ws = wakeup_source_register(info->dev, "regulator_init_ws");
	if (info->regulator_init_ws) {
		__pm_wakeup_event(info->regulator_init_ws, info->late_cleanup_wakelock_timeout);
		dev_info(info->dev, "%s: acquire wakelock %dms\n", __func__, info->late_cleanup_wakelock_timeout);
	} else {
		dev_err(info->dev, "%s: Fail to register wakeup_source\n", __func__);
		ret = -EPERM;
		goto err_create_sysfs;
	}

	return ret;

err_create_sysfs:
	sec_device_destroy(sec_pm_regulator_dev->devt);
probe_end:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int sec_pm_regulator_remove(struct platform_device *pdev)
{
	struct sec_pm_regulator_info *info = platform_get_drvdata(pdev);

	sec_device_destroy(info->sec_pm_regulator_dev->devt);

	return 0;
}

static const struct of_device_id sec_pm_regulator_match[] = {
	{ .compatible = "samsung,sec-pm-regulator", },
	{ },
};

static struct platform_driver sec_pm_regulator_driver = {
	.driver = {
		.name = "sec-pm-regulator",
		.of_match_table = of_match_ptr(sec_pm_regulator_match),
	},
	.probe = sec_pm_regulator_probe,
	.remove = sec_pm_regulator_remove,
};

module_platform_driver(sec_pm_regulator_driver);

MODULE_AUTHOR("Jonghyeon Cho <jongjaaa.cho@samsung.com>");
MODULE_DESCRIPTION("System Power Regulator debugging driver");
MODULE_LICENSE("GPL");
