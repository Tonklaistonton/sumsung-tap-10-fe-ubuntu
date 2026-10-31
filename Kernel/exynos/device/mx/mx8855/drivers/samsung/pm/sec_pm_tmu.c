/*
 * sec_pm_tmu.c
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
#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/suspend.h>
#include <soc/samsung/tmu.h>
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif

#define TZ_LOG_PERIOD	10
#define MAX_BUF_SIZE 256
#define NR_THERMAL_SENSOR_MAX 10

struct sec_pm_tmu_info {
	struct device *dev;
	struct device *exynos_tmu_dev;

	struct notifier_block sec_pm_tmu_nb;
	struct delayed_work tmu_work;
	unsigned int tmu_log_period;
	int tmu_temp[NR_THERMAL_SENSOR_MAX];
};

static ssize_t tmu_temp_print(struct sec_pm_tmu_info *info, char *buf)
{
	int i, id_max = 0;
	ssize_t ret = 0;
	int *temp = info->tmu_temp;

	id_max = exynos_thermal_get_tz_temps(temp, NR_THERMAL_SENSOR_MAX);

	if (id_max < 0) {
		pr_err("%s: fail to get TMU temp(%d)\n", __func__, id_max);
		return ret;
	}

	for (i = 0; i <= id_max; i++)
		ret += snprintf(buf + ret, MAX_BUF_SIZE - ret, "%d,", temp[i] / 1000);

	sprintf(buf + ret - 1, "\n");

	return ret;
}

static void tmu_temp_print_work(struct work_struct *work)
{
	char buf[MAX_BUF_SIZE] = {0, };
	struct sec_pm_tmu_info *info = container_of(to_delayed_work(work),
			struct sec_pm_tmu_info, tmu_work);

	tmu_temp_print(info, buf);
	pr_info("%s: %s", "exynos_tmu", buf);

	schedule_delayed_work(&info->tmu_work, info->tmu_log_period * HZ);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
static ssize_t curr_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_tmu_info *info = dev_get_drvdata(dev);

	return tmu_temp_print(info, buf);
}
static DEVICE_ATTR_RO(curr_temp);

static struct attribute *sec_pm_tmu_attrs[] = {
	&dev_attr_curr_temp.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_tmu);
#endif /* CONFIG_DRV_SAMSUNG */

static int suspend_resume_tmu_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	struct sec_pm_tmu_info *info = container_of(notifier,
			struct sec_pm_tmu_info, sec_pm_tmu_nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		cancel_delayed_work_sync(&info->tmu_work);
		break;
	case PM_POST_SUSPEND:
		schedule_delayed_work(&info->tmu_work, info->tmu_log_period * HZ);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int sec_pm_tmu_probe(struct platform_device *pdev)
{
	struct sec_pm_tmu_info *info;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	struct device *exynos_tmu_dev;
#endif
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: Fail to alloc info\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	info->tmu_log_period = TZ_LOG_PERIOD;

	info->sec_pm_tmu_nb.notifier_call = suspend_resume_tmu_event;
	ret = register_pm_notifier(&info->sec_pm_tmu_nb);
	if (ret) {
		dev_err(info->dev, "%s: failed to register PM notifier(%d)\n",
				__func__, ret);
		return ret;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	exynos_tmu_dev = sec_device_create(info, "exynos_tmu");

	if (IS_ERR(exynos_tmu_dev)) {
		dev_err(info->dev, "%s: fail to create exynos_tmu_dev\n", __func__);
		return PTR_ERR(exynos_tmu_dev);
	}

	info->exynos_tmu_dev = exynos_tmu_dev;

	ret = sysfs_create_groups(&exynos_tmu_dev->kobj, sec_pm_tmu_groups);
	if (ret) {
		dev_err(info->dev, "%s: failed to create sysfs groups(%d)\n",
				__func__, ret);
		sec_device_destroy(exynos_tmu_dev->devt);
		return ret;
	}
#endif /* CONFIG_DRV_SAMSUNG */
	INIT_DELAYED_WORK(&info->tmu_work, tmu_temp_print_work);
	schedule_delayed_work(&info->tmu_work, info->tmu_log_period * HZ);

	return 0;
}

static int sec_pm_tmu_remove(struct platform_device *pdev)
{
	struct sec_pm_tmu_info *info = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_device_destroy(info->exynos_tmu_dev->devt);
#endif
	cancel_delayed_work_sync(&info->tmu_work);

	return 0;
}

static const struct of_device_id sec_pm_tmu_match[] = {
	{ .compatible = "samsung,sec-pm-tmu", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_tmu_match);

static struct platform_driver sec_pm_tmu_driver = {
	.driver = {
		.name = "sec-pm-tmu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_tmu_match),
	},
	.probe = sec_pm_tmu_probe,
	.remove = sec_pm_tmu_remove,
};

module_platform_driver(sec_pm_tmu_driver);

MODULE_AUTHOR("Jonghyeon Cho <jongjaaa.cho@samsung.com>");
MODULE_DESCRIPTION("SEC PM Thermal Management Unit Driver");
MODULE_LICENSE("GPL");
