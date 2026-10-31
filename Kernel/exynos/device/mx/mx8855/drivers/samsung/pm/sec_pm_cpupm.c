/*
 * sec_pm_cpupm.c
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
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic.h>
#include <linux/muic/common/muic_notifier.h>
#include <soc/samsung/exynos-cpupm.h>
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif /* CONFIG_PDIC_NOTIFIER */
#endif /* CONFIG_MUIC_NOTIFIER */
/*********************************************************************
 *            Disable SICD when attaching JIG cable                  *
 *********************************************************************/
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
struct notifier_block cpuidle_muic_nb;
static int sec_pm_cpupm_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	PD_NOTI_ATTACH_TYPEDEF *p_noti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s: JIG(%d) is detached, enable SICD state\n", __func__, attached_dev);
			enable_power_mode(0, POWERMODE_TYPE_SYSTEM);
			enable_power_mode(0, POWERMODE_TYPE_DSU);
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			pr_info("%s: JIG(%d) is attached, disable SICD state\n", __func__, attached_dev);
			disable_power_mode(0, POWERMODE_TYPE_DSU);
			disable_power_mode(0, POWERMODE_TYPE_SYSTEM);
		} else {
			pr_err("%s: ACTION Error!(%lu)\n", __func__, action);
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
static void sec_pm_cpupm_muic_notifier_init(void)
{
	muic_notifier_register(&cpuidle_muic_nb, sec_pm_cpupm_muic_notifier,
			MUIC_NOTIFY_DEV_CPUIDLE);
}
#else
static inline void sec_pm_cpupm_muic_notifier_init(void) {}
#endif /* !CONFIG_MUIC_NOTIFIER */
static int sec_pm_cpupm_probe(struct platform_device *pdev)
{
	sec_pm_cpupm_muic_notifier_init();
	return 0;
}
static const struct of_device_id sec_pm_cpupm_match[] = {
	{ .compatible = "samsung,sec-pm-cpupm", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_cpupm_match);
static struct platform_driver sec_pm_cpupm_driver = {
	.driver = {
		.name = "sec-pm-cpupm",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_cpupm_match),
	},
	.probe = sec_pm_cpupm_probe,
};
static int __init sec_pm_cpupm_init(void)
{
	return platform_driver_register(&sec_pm_cpupm_driver);
}
late_initcall(sec_pm_cpupm_init);
static void __exit sec_pm_cpupm_exit(void)
{
	platform_driver_unregister(&sec_pm_cpupm_driver);
}
module_exit(sec_pm_cpupm_exit);
MODULE_AUTHOR("Jonghyeon Cho <jongjaaa.cho@samsung.com>");
MODULE_DESCRIPTION("SEC PM CPU Power Management Driver");
MODULE_LICENSE("GPL");
