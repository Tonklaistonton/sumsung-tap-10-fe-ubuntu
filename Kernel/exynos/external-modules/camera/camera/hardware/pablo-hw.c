// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include "pablo-hw-common-ctrl.h"

static int __init pablo_hw_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_PABLO_CMN_CTRL)
	ret = platform_driver_register(pablo_common_ctrl_hw_get_driver());
	if (ret) {
		pr_err("pablo_common_ctrl_hw_driver register failed(%d)", ret);
		return ret;
	}
#endif

	return ret;
}
module_init(pablo_hw_init);

static void __exit pablo_hw_exit(void)
{
}
module_exit(pablo_hw_exit)

MODULE_DESCRIPTION("Exynos Pablo HW");
MODULE_LICENSE("GPL");
