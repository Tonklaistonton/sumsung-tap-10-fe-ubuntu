/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef PABLO_HW_RGBP_H
#define PABLO_HW_RGBP_H

#include "is-param.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"
#include "pablo-crta-interface.h"

struct pablo_hw_rgbp_iq {
	struct cr_set *regs;
	u32 size;
	spinlock_t slock;

	u32 fcount;
	unsigned long state;
};

struct pablo_hw_rgbp {
	struct is_rgbp_config config[IS_STREAM_COUNT];
	struct rgbp_param_set param_set[IS_STREAM_COUNT];

	struct pablo_internal_subdev subdev_cloader;
	u32 header_size;

	struct pablo_common_ctrl *pcc;

	const struct rgbp_hw_ops *ops;

	struct is_common_dma *rdma;
	struct is_common_dma *wdma;
	u32 rdma_cnt;
	u32 wdma_cnt;
	u32 rdma_cfg_cnt;
	u32 wdma_cfg_cnt;

	struct pablo_hw_rgbp_iq iq_set;
	struct pablo_hw_rgbp_iq cur_iq_set;

	u32 instance;
	unsigned long state;
	unsigned long event_state;

	u32 cinrow_time;
	u32 cinrow_ratio;
	u32 post_frame_gap;

	atomic_t start_fcount;

	struct pablo_icpu_adt *icpu_adt;
	struct is_sensor_interface *sensor_itf[IS_STREAM_COUNT];
};

#include "is-hw.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"

enum is_hw_rgbp_irq_src {
	RGBP_INTR0,
	RGBP_INTR1,
	RGBP_INTR_MAX,
};

enum is_hw_rgbp_dbg_mode {
	RGBP_DBG_DUMP_REG,
	RGBP_DBG_DTP,
};

enum is_hw_rgbp_event_type {
	RGBP_INIT,
	/* INT1 */
	RGBP_FS,
	RGBP_FR,
	RGBP_FE,
	RGBP_SETTING_DONE,
};

struct is_hw_rgbp_sc_size {
	u32	input_h_size;
	u32	input_v_size;
	u32	dst_h_size;
	u32	dst_v_size;
};

#endif /* PABLO_HW_RGBP_H */
