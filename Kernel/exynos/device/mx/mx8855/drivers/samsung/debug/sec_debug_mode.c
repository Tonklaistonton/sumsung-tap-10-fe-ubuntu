// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/panic_notifier.h>
#include <linux/of.h>
#include <linux/sec_debug.h>
#include <soc/samsung/exynos/debug-snapshot.h>

static int debug_level;
static int force_upload;

static int get_debug_level(void)
{
	struct device_node *np;
	unsigned int val = 0;
	int ret = 0;

	np = of_find_node_by_path("/sec_debug_level");
	if (!np) {
		pr_crit("%s: no sec_debug_level	in dt\n", __func__);
		return -1;
	}

	if (of_device_is_available(np)) {
		pr_info("%s: sec_debug_level node is available\n", __func__);

		ret = of_property_read_u32(np, "value", &val);
		if (ret) {
			pr_info("%s: value is Empty\n", __func__);
			return -1;
		}

		pr_info("%s: sec_debug_level %d\n", __func__, val);
		return (int)val;
	}

	pr_err("%s: sec_debug_level node is not available\n", __func__);

	return 0;
}

static int get_force_upload(void)
{
	struct device_node *np;

	np = of_find_node_by_path("/sec_force_upload");
	if (!np) {
		pr_crit("%s: no sec_force_upload in dt\n", __func__);
		return -1;
	}

	if (of_device_is_available(np)) {
		pr_info("%s: sec_force_upload node is available\n", __func__);
		/* status okay means force upload value in param was 0x5 */
		return 5;
	}

	pr_info("%s: sec_force_upload node is not available\n", __func__);
	return 0;
}

unsigned int sec_debug_get_debug_level(void)
{
	return (unsigned int)debug_level;
}
EXPORT_SYMBOL(sec_debug_get_debug_level);

bool sec_debug_get_force_upload(void)
{
	return !!force_upload;
}
EXPORT_SYMBOL(sec_debug_get_force_upload);

static int __init secdbg_mode_init(void)
{
	debug_level = get_debug_level();

	if (debug_level == -1) {
		pr_err("%s: failed to get debug level\n", __func__);
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		pr_err("%s: set 0 for debug level\n", __func__);
		debug_level = 0;
#else
		/* expect panic for debugging */
		return -1;
#endif
	}

	pr_info("%s: debug level: %x\n", __func__, debug_level);

	force_upload = get_force_upload();

	if (force_upload == -1) {
		pr_err("%s: failed to get force upload\n", __func__);
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		pr_err("%s: set 0 for force upload\n", __func__);
		force_upload = 0;
#else
		/* expect panic for debugging */
		return -1;
#endif
	}

	pr_info("%s: force upload: %x\n", __func__, force_upload);

	return 0;
}
module_init(secdbg_mode_init);

static void __exit secdbg_mode_exit(void)
{
	return;
}
module_exit(secdbg_mode_exit);

MODULE_DESCRIPTION("Samsung Debug mode driver");
MODULE_LICENSE("GPL v2");
