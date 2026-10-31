// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_hw_param.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/sec_class.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "sec_debug_internal.h"

/* maximum size of sysfs */
#define DATA_SIZE 1024
#define LOT_STRING_LEN 5

/* function name prefix: secdbg_hprm */
char __read_mostly *dram_size;
module_param(dram_size, charp, 0440);

/* this is same with androidboot.dram_info */
char __read_mostly *dram_info;
module_param(dram_info, charp, 0440);

static void secdbg_hprm_set_hw_exin(void)
{
	secdbg_exin_set_hwid(id_get_asb_ver(), id_get_product_line(), dram_info);
}

static int __init secdbg_hw_param_init(void)
{
	struct device *dev;

	pr_info("%s: start\n", __func__);
	pr_info("%s: from cmdline: dram_size: %s\n", __func__, dram_size);
	pr_info("%s: from cmdline: dram_info: %s\n", __func__, dram_info);

	secdbg_hprm_set_hw_exin();

	dev = sec_device_create(NULL, "sec_hw_param");

	return 0;
}
module_init(secdbg_hw_param_init);

MODULE_DESCRIPTION("Samsung Debug HW Parameter driver");
MODULE_LICENSE("GPL v2");
