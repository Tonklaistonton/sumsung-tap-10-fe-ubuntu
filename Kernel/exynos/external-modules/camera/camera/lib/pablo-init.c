/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include "pablo-init.h"

static const struct of_device_id pablo_init_of_table[] = {
	{
		.name = "pablo-init",
		.compatible = "samsung,pablo-init",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pablo_init_of_table);

static int __init pablo_init_probe(struct platform_device *pdev)
{
	printk("%s:start\n", __func__);

	return 0;
}

static struct platform_driver pablo_init_driver = {
	.driver = {
		.name = "pablo-init",
		.owner = THIS_MODULE,
		.of_match_table = pablo_init_of_table,
	}
};

#ifdef MODULE
builtin_platform_driver_probe(pablo_init_driver, pablo_init_probe);
#else
static int __init pablo_init_initalize(void)
{
	int ret;

	ret = platform_driver_probe(&pablo_init_driver, pablo_init_probe);
	if (ret)
		pr_err("%s: platform_driver_probe is failed(%d)", __func__, ret);

	return ret;
}
device_initcall_sync(pablo_init_initalize);
#endif

MODULE_DESCRIPTION("Samsung EXYNOS SoC Pablo Init driver");
MODULE_ALIAS("platform:samsung-pablo-init");
MODULE_LICENSE("GPL v2");