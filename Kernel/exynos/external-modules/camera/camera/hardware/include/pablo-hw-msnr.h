/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_MSNR_H
#define IS_HW_MSNR_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"

enum is_hw_msnr_wdma_index { MSNR_WDMA_LME, MSNR_WDMA_MAX };

enum is_hw_msnr_irq_src {
	MSNR_INTR_0,
	MSNR_INTR_1,
	MSNR_INTR_MAX,
};

enum is_hw_tnr_mode {
	MSNR_TNR_MODE_PREPARE,
	MSNR_TNR_MODE_FIRST,
	MSNR_TNR_MODE_NORMAL,
	MSNR_TNR_MODE_FUSION,
};

enum is_hw_msnr_dbg_mode {
	MSNR_DBG_DUMP_REG,
	MSNR_DBG_DUMP_REG_ONCE,
	MSNR_DBG_S2D,
	MSNR_DBG_SKIP_DDK,
	MSNR_DBG_BYPASS,
};

struct is_hw_msnr {
	struct is_lib_isp lib[IS_STREAM_COUNT];
	struct msnr_param_set param_set[IS_STREAM_COUNT];
	struct is_common_dma wdma[MSNR_WDMA_MAX];
	struct is_msnr_config config[IS_STREAM_COUNT];
	struct pablo_common_ctrl *pcc;
	u32 irq_state[MSNR_INTR_MAX];
	u32 instance;
	u32 repeat_instance;
	u32 repeat_state;
	unsigned long state;
	struct is_priv_buf *pb_c_loader_payload;
	unsigned long kva_c_loader_payload;
	dma_addr_t dva_c_loader_payload;
	struct is_priv_buf *pb_c_loader_header;
	unsigned long kva_c_loader_header;
	dma_addr_t dva_c_loader_header;
};

void is_hw_msnr_s_debug_type(int type);
void is_hw_msnr_c_debug_type(int type);
const struct kernel_param *is_hw_msnr_get_debug_kernel_param_kunit_wrapper(void);

#endif
