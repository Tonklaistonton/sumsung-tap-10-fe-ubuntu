/*
 * sec_pm_debug.c
 *
 *  Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 * Author: Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Revise code for kernel 6.6
 * Author: Jonghyeon Cho <jongjaaa.cho@samsung.com>
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/suspend.h>
#include <linux/rtc.h>
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif

#define WS_LOG_PERIOD	10
#define MAX_WAKE_SOURCES_LEN 256
#include <linux/pmic/s2p_regulator.h>

struct sec_pm_debug_info {
	struct device *dev;
	struct device *sec_pm_dev;
	struct device_node *np;

	struct notifier_block sec_pm_debug_nb;
	struct delayed_work ws_work;
	unsigned int ws_log_period;

	unsigned long long sleep_count;
	struct timespec64 total_sleep_time;
	ktime_t last_monotime; /* monotonic time before last suspend */
	ktime_t curr_monotime; /* monotonic time after last suspend */
	ktime_t last_stime; /* monotonic boottime offset before last suspend */
	ktime_t curr_stime; /* monotonic boottime offset after last suspend */

	uint8_t *pmic_onsrc;
	uint8_t *pmic_offsrc;
	int onsrc_reg_cnt;
	int offsrc_reg_cnt;
	int main_pmic_cnt;
	int sub_pmic_cnt;
};

extern void pm_get_active_wakeup_sources(char *pending_wakeup_source, size_t max);

static void wake_sources_print_acquired(void)
{
	char wake_sources_acquired[MAX_WAKE_SOURCES_LEN];

	pm_get_active_wakeup_sources(wake_sources_acquired, MAX_WAKE_SOURCES_LEN);
	pr_info("PM: %s\n", wake_sources_acquired);
}

static void wake_sources_print_acquired_work(struct work_struct *work)
{
	struct sec_pm_debug_info *info = container_of(to_delayed_work(work),
			struct sec_pm_debug_info, ws_work);

	wake_sources_print_acquired();
	schedule_delayed_work(&info->ws_work, info->ws_log_period * HZ);
}

static void pm_suspend_marker(char *annotation)
{
	struct timespec64 ts;
	struct rtc_time tm;

	ktime_get_real_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, &tm);

	pr_info("PM: suspend %s %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
		annotation, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

/*********************************************************************
 *            Sysfs nodes - sys/class/sec/pm                         *
 *********************************************************************/

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
static ssize_t sleep_time_sec_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_debug_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%lld\n", info->total_sleep_time.tv_sec);
}

static ssize_t sleep_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_debug_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%llu\n", info->sleep_count);
}

static ssize_t ws_log_period_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_debug_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->ws_log_period);
}

static ssize_t ws_log_period_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int period;
	struct sec_pm_debug_info *info = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 0, &period);

	if (ret < 0) {
		dev_err(dev, "%s: invalid args(%d)\n", __func__, ret);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&info->ws_work);
	info->ws_log_period = period;
	schedule_delayed_work(&info->ws_work, info->ws_log_period * HZ);

	return count;
}

static ssize_t pwr_on_off_src_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_debug_info *info = dev_get_drvdata(dev);
	int i;
	int ret = 0;

	ret = sprintf(buf, "ONSRC:");
	for (i = 0; i < info->onsrc_reg_cnt; i++)
		ret += sprintf(buf + ret, "0x%02X,", info->pmic_onsrc[i]);

	ret--;
	ret += sprintf(buf + ret, " OFFSRC:");
	for (i = 0; i < info->offsrc_reg_cnt; i++)
		ret += sprintf(buf + ret, "0x%02X,", info->pmic_offsrc[i]);

	sprintf(buf + ret - 1, "\n");

	return ret;
}

static DEVICE_ATTR_RO(sleep_time_sec);
static DEVICE_ATTR_RO(sleep_count);
static DEVICE_ATTR_RW(ws_log_period);
static DEVICE_ATTR_RO(pwr_on_off_src);

static struct attribute *sec_pm_debug_attrs[] = {
	&dev_attr_sleep_time_sec.attr,
	&dev_attr_sleep_count.attr,
	&dev_attr_ws_log_period.attr,
	&dev_attr_pwr_on_off_src.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_debug);
#endif /* CONFIG_DRV_SAMSUNG */

static int suspend_resume_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	struct timespec64 sleep_time;
	struct timespec64 total_time;
	struct timespec64 suspend_resume_time;
	struct sec_pm_debug_info *info = container_of(notifier,
			struct sec_pm_debug_info, sec_pm_debug_nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		pm_suspend_marker("entry");
		/* monotonic time since boot */
		info->last_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		info->last_stime = ktime_get_boottime();

		cancel_delayed_work_sync(&info->ws_work);
		break;
	case PM_POST_SUSPEND:
		/* monotonic time since boot */
		info->curr_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		info->curr_stime = ktime_get_boottime();

		total_time = ktime_to_timespec64(ktime_sub(info->curr_stime, info->last_stime));
		suspend_resume_time =
			ktime_to_timespec64(ktime_sub(info->curr_monotime, info->last_monotime));
		sleep_time = timespec64_sub(total_time, suspend_resume_time);

		info->total_sleep_time = timespec64_add(info->total_sleep_time, sleep_time);
		info->sleep_count++;

		pm_suspend_marker("exit");
		schedule_delayed_work(&info->ws_work, info->ws_log_period * HZ);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int sec_pm_debug_parse_dt(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info = platform_get_drvdata(pdev);

	if (!info || !pdev->dev.of_node)
		return -ENODEV;

	info->np = pdev->dev.of_node;

	if (of_property_read_u32(info->np, "main_pmic_cnt", &info->main_pmic_cnt)) {
		dev_err(info->dev, "failed to get main pmic cnt\n");
		return -EINVAL;
	}

	if (of_property_read_u32(info->np, "sub_pmic_cnt", &info->sub_pmic_cnt)) {
		dev_err(info->dev, "failed to get sub pmic cnt\n");
		return -EINVAL;
	}

	if (of_property_read_u32(info->np, "onsrc_reg_cnt", &info->onsrc_reg_cnt)) {
		dev_err(info->dev, "failed to get onsrc register cnt\n");
		return -EINVAL;
	}

	if (of_property_read_u32(info->np, "offsrc_reg_cnt", &info->offsrc_reg_cnt)) {
		dev_err(info->dev, "failed to get offsrc register cnt\n");
		return -EINVAL;
	}

	return 0;
}

static int sec_pm_debug_probe(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	struct device *sec_pm_dev;
#endif
	int ret;
	int pmic_cnt;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: Fail to alloc info\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	ret = sec_pm_debug_parse_dt(pdev);
	if (ret) {
		dev_err(info->dev, "%s: fail to parse dt\n", __func__);
		return ret;
	}

	/* Set to default logging period (10s) */
	info->ws_log_period = WS_LOG_PERIOD;

	/* Register PM notifier */
	info->sec_pm_debug_nb.notifier_call = suspend_resume_pm_event;
	ret = register_pm_notifier(&info->sec_pm_debug_nb);
	if (ret) {
		dev_err(info->dev, "%s: failed to register PM notifier(%d)\n",
				__func__, ret);
		return ret;
	}

	/* Get and clear power on off sources */
	info->pmic_onsrc = devm_kcalloc(&pdev->dev, info->onsrc_reg_cnt, sizeof(int), GFP_KERNEL);
	ret = s2p_get_pwronsrc(0, info->pmic_onsrc, info->onsrc_reg_cnt);
	if (ret < 0)
		dev_err(&pdev->dev, "failed to read PWRONSRC %d\n", ret);

	info->pmic_offsrc = devm_kcalloc(&pdev->dev, info->offsrc_reg_cnt, sizeof(int), GFP_KERNEL);
	ret = s2p_get_pwroffsrc(0, info->pmic_offsrc, info->offsrc_reg_cnt);
	if (ret < 0)
		dev_err(&pdev->dev, "failed to read OFFSRC %d\n", ret);

	pmic_cnt = info->main_pmic_cnt + info->sub_pmic_cnt;
	for (int i = 0; i < pmic_cnt; i++) {
		ret = s2p_clear_pwroffsrc(i);
		if (ret)
			dev_err(&pdev->dev, "failed to write OFFSRC %d\n", i);
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_pm_dev = sec_device_create(info, "pm");

	if (IS_ERR(sec_pm_dev)) {
		dev_err(info->dev, "%s: fail to create sec_pm_dev\n", __func__);
		return PTR_ERR(sec_pm_dev);
	}

	info->sec_pm_dev = sec_pm_dev;

	ret = sysfs_create_groups(&sec_pm_dev->kobj, sec_pm_debug_groups);
	if (ret) {
		dev_err(info->dev, "%s: failed to create sysfs groups(%d)\n",
				__func__, ret);
		goto err_create_sysfs;
	}

#endif /* CONFIG_DRV_SAMSUNG */
	INIT_DELAYED_WORK(&info->ws_work, wake_sources_print_acquired_work);
	schedule_delayed_work(&info->ws_work, info->ws_log_period * HZ);

	return 0;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
err_create_sysfs:
	sec_device_destroy(sec_pm_dev->devt);

	return ret;
#endif /* CONFIG_DRV_SAMSUNG */
}

static int sec_pm_debug_remove(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	sec_device_destroy(info->sec_pm_dev->devt);
#endif
	cancel_delayed_work_sync(&info->ws_work);

	return 0;
}

static const struct of_device_id sec_pm_debug_match[] = {
	{ .compatible = "samsung,sec-pm-debug", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_debug_match);

static struct platform_driver sec_pm_debug_driver = {
	.driver = {
		.name = "sec-pm-debug",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_debug_match),
	},
	.probe = sec_pm_debug_probe,
	.remove = sec_pm_debug_remove,
};

static int __init sec_pm_debug_init(void)
{
	return platform_driver_register(&sec_pm_debug_driver);
}
late_initcall(sec_pm_debug_init);

static void __exit sec_pm_debug_exit(void)
{
	platform_driver_unregister(&sec_pm_debug_driver);
}
module_exit(sec_pm_debug_exit);

MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_AUTHOR("Jonghyeon Cho <jongjaaa.cho@samsung.com>");
MODULE_DESCRIPTION("SEC PM Debug Driver");
MODULE_LICENSE("GPL");
