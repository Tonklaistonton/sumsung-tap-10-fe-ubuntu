// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Authors:
 * Storage Driver <storage.sec@samsung.com>
 */

#ifndef __MMC_SEC_MISC_H__
#define __MMC_SEC_MISC_H__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/sec_class.h>
#include <linux/sec_debug.h>
#include "mmc-sec-feature.h"

extern struct device *sec_sdcard_cmd_dev;
extern struct device *sec_sdinfo_cmd_dev;
extern struct device *sec_sddata_cmd_dev;
#endif

