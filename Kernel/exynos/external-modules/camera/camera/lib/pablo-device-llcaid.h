/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_DEVICE_LLCAID_H
#define PABLO_DEVICE_LLCAID_H

#define PABLO_MAX_NUM_LLCAID_GROUP 10
#define PABLO_MAX_NUM_STREAM 5
#define PABLO_MAX_NUM_SET_VAL 5
#define PABLO_SET_VAL_IDX_EN 0
#define PABLO_SET_VAL_IDX_CTRL 1
#define PABLO_STREAM_RD_CTRL_OFFSET 0x4
#define PABLO_STREAM_WD_CTRL_OFFSET 0x8

enum pablo_llcaid_dbg_mode {
	LLCAID_DBG_DUMP_CONFIG = 0,
	LLCAID_DBG_SKIP_CONFIG,
};

struct pablo_llcaid_stream_info {
	unsigned int set_val[PABLO_MAX_NUM_SET_VAL];
};

struct pablo_device_llcaid {
	struct device *dev;
	void *base[PABLO_MAX_NUM_STREAM];
	int id;
	char instance_name[16];
	struct pablo_llcaid_stream_info *stream[PABLO_MAX_NUM_STREAM];
};

void pablo_device_llcaid_config(void);
struct pablo_device_llcaid *pablo_device_llcaid_get(u32 idx);
void pablo_device_set_llcaid_group(struct pablo_device_llcaid *llcaid_group_data, int num);

struct platform_driver *pablo_llcaid_get_platform_driver(void);
#endif
