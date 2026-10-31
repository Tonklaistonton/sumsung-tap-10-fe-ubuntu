// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   USB uevent debug Driver for Exynos
 *
 *   Copyright (c) 2024 by Eomji Oh <eomji.oh@samsung.com>
 *
 */
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <trace/hooks/usb.h>

void exynos_android_work(void *data, bool connected, bool disconnected, bool configured, bool uevent_sent)
{
	char *disconnected_strs[2] = { "USB_STATE=DISCONNECTED", NULL };
	char *connected_strs[2] = { "USB_STATE=CONNECTED", NULL };
	char *configured_strs[2] = { "USB_STATE=CONFIGURED", NULL };


	if (connected) {
		pr_info("[%s] sent uevent %s\n", __func__, connected_strs[0]);
	}

	if (configured) {
		pr_info("[%s] sent uevent %s\n", __func__, configured_strs[0]);
	}

	if (disconnected) {
		pr_info("[%s] sent uevent %s\n", __func__, disconnected_strs[0]);
	}

	if (!uevent_sent) {
		pr_info("[%s] did not send uevent\n", __func__);
	}

}

int exynos_usb_debug_init(void)
{
	pr_info("%s\n", __func__);
	register_trace_android_vh_configfs_uevent_work(exynos_android_work, NULL);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_usb_debug_init);

void exynos_usb_debug_exit(void)
{
	pr_info("%s\n", __func__);
}
EXPORT_SYMBOL_GPL(exynos_usb_debug_exit);

MODULE_AUTHOR("Eomji Oh <eomji.oh@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos USB debug driver");
