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

#ifndef IS_HW_MTNR1_H
#define IS_HW_MTNR1_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"

#define SET_MTNR1_MUTLI_BUFFER_ADDR(nb, np, a)                                                     \
	do {                                                                                       \
		int __b, __p;                                                                      \
		for (__b = 1; __b < (nb); ++__b)                                                   \
			for (__p = 0; __p < (np); ++__p)                                           \
				(a)[(__b * (np)) + __p] = (a)[__p];                                \
	} while (0)

#define SET_MTNR1_MUTLI_BUFFER_ADDR_SWAP(nb, np, a1, a2)                                           \
	do {                                                                                       \
		int __b, __p;                                                                      \
		for (__b = 1; __b < (nb); ++__b) {                                                 \
			for (__p = 0; __p < (np); ++__p) {                                         \
				(a1)[(__b * (np)) + __p] = (a1)[__p];                              \
				(a2)[(__b * (np)) + __p] = (a2)[__p];                              \
				if (__b % 2)                                                       \
					SWAP((a1)[(__b * (np)) + __p], (a2)[(__b * (np)) + __p],   \
						typeof((a1)[(__b * (np)) + __p]));                 \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define SKIP_MIX(config)                                                                           \
	(config->skip_wdma && !config->mixerL1_still_en &&                                         \
		(config->mixerL1_mode == MTNR1_TNR_MODE_NORMAL))
#define PARTIAL(stripe_input)                                                                      \
	(stripe_input->total_count > 1 && stripe_input->index < (stripe_input->total_count - 1))
#define EVEN_BATCH(frame) (frame->num_buffers > 1 && (frame->num_buffers % 2) == 0)

enum is_hw_mtnr1_rdma_index {
	MTNR1_RDMA_CUR_L1_Y,
	MTNR1_RDMA_CUR_L1_U,
	MTNR1_RDMA_CUR_L1_V,
	MTNR1_RDMA_CUR_L2_Y,
	MTNR1_RDMA_CUR_L2_U,
	MTNR1_RDMA_CUR_L2_V,
	MTNR1_RDMA_CUR_L3_Y,
	MTNR1_RDMA_CUR_L3_U,
	MTNR1_RDMA_CUR_L3_V,
	MTNR1_RDMA_CUR_L4_Y,
	MTNR1_RDMA_CUR_L4_U,
	MTNR1_RDMA_CUR_L4_V,
	MTNR1_RDMA_PREV_L1_Y,
	MTNR1_RDMA_PREV_L1_U,
	MTNR1_RDMA_PREV_L1_V,
	MTNR1_RDMA_PREV_L2_Y,
	MTNR1_RDMA_PREV_L2_U,
	MTNR1_RDMA_PREV_L2_V,
	MTNR1_RDMA_PREV_L3_Y,
	MTNR1_RDMA_PREV_L3_U,
	MTNR1_RDMA_PREV_L3_V,
	MTNR1_RDMA_PREV_L4_Y,
	MTNR1_RDMA_PREV_L4_U,
	MTNR1_RDMA_PREV_L4_V,
	MTNR1_RDMA_PREV_L1_WGT,
	MTNR1_RDMA_SEG_L1,
	MTNR1_RDMA_SEG_L2,
	MTNR1_RDMA_SEG_L3,
	MTNR1_RDMA_SEG_L4,
	MTNR1_RDMA_MV_GEOMATCH,
	MTNR1_RDMA_MAX
};

enum is_hw_mtnr1_wdma_index {
	MTNR1_WDMA_PREV_L1_Y,
	MTNR1_WDMA_PREV_L1_U,
	MTNR1_WDMA_PREV_L1_V,
	MTNR1_WDMA_PREV_L2_Y,
	MTNR1_WDMA_PREV_L2_U,
	MTNR1_WDMA_PREV_L2_V,
	MTNR1_WDMA_PREV_L3_Y,
	MTNR1_WDMA_PREV_L3_U,
	MTNR1_WDMA_PREV_L3_V,
	MTNR1_WDMA_PREV_L4_Y,
	MTNR1_WDMA_PREV_L4_U,
	MTNR1_WDMA_PREV_L4_V,
	MTNR1_WDMA_PREV_L1_WGT,
	MTNR1_WDMA_MAX
};

enum is_hw_mtnr1_irq_src {
	MTNR1_INTR_0,
	MTNR1_INTR_1,
	MTNR1_INTR_MAX,
};

enum is_hw_tnr_mode {
	MTNR1_TNR_MODE_PREPARE,
	MTNR1_TNR_MODE_FIRST,
	MTNR1_TNR_MODE_NORMAL,
	MTNR1_TNR_MODE_FUSION,
};

enum is_hw_mtnr1_dbg_mode {
	MTNR1_DBG_DUMP_REG,
	MTNR1_DBG_DUMP_REG_ONCE,
	MTNR1_DBG_S2D,
	MTNR1_DBG_SKIP_DDK,
	MTNR1_DBG_BYPASS,
	MTNR1_DBG_DTP,
	MTNR1_DBG_TNR,
};

enum is_hw_mtnr1_subdev {
	MTNR1_SD_IN_L1_YUV,
	MTNR1_SD_IN_L2_YUV,
	MTNR1_SD_IN_L3_YUV,
	MTNR1_SD_IN_L4_YUV,
	MTNR1_SD_IN_L1_WGT,
	MTNR1_SD_OUT_L1_YUV,
	MTNR1_SD_OUT_L2_YUV,
	MTNR1_SD_OUT_L3_YUV,
	MTNR1_SD_OUT_L4_YUV,
	MTNR1_SD_OUT_L1_WGT,
	MTNR1_SD_END,
	MTNR1_SD_IN_L1_YUV_2NR = MTNR1_SD_END,
	MTNR1_SD_IN_L2_YUV_2NR,
	MTNR1_SD_IN_L3_YUV_2NR,
	MTNR1_SD_IN_L4_YUV_2NR,
	MTNR1_SD_IN_L1_WGT_2NR,
	MTNR1_SD_OUT_L1_YUV_2NR,
	MTNR1_SD_OUT_L2_YUV_2NR,
	MTNR1_SD_OUT_L3_YUV_2NR,
	MTNR1_SD_OUT_L4_YUV_2NR,
	MTNR1_SD_OUT_L1_WGT_2NR,
	MTNR1_SD_MAX,
};

static const char *mtnr1_internal_buf_name[MTNR1_SD_MAX] = {
	"MTNR1_L1Y_IN",
	"MTNR1_L2Y_IN",
	"MTNR1_L3Y_IN",
	"MTNR1_L4Y_IN",
	"MTNR1_L1W_IN",
	"MTNR1_L1Y_OT",
	"MTNR1_L2Y_OT",
	"MTNR1_L3Y_OT",
	"MTNR1_L4Y_OT",
	"MTNR1_L1W_OT",
	/* For 2NR */
	"MTNR1_L1Y_I2",
	"MTNR1_L2Y_I2",
	"MTNR1_L3Y_I2",
	"MTNR1_L4Y_I2",
	"MTNR1_L1W_I2",
	"MTNR1_L1Y_O2",
	"MTNR1_L2Y_O2",
	"MTNR1_L3Y_O2",
	"MTNR1_L4Y_O2",
	"MTNR1_L1W_O2",
};

struct is_hw_mtnr1 {
	struct is_lib_isp lib[IS_STREAM_COUNT];
	struct mtnr_param_set param_set[IS_STREAM_COUNT];
	struct is_common_dma rdma[MTNR1_RDMA_MAX];
	struct is_common_dma wdma[MTNR1_WDMA_MAX];
	struct is_mtnr1_config config[IS_STREAM_COUNT];
	struct pablo_common_ctrl *pcc;
	u32 irq_state[MTNR1_INTR_MAX];
	u32 instance;
	u32 repeat_instance;
	u32 repeat_state;
	unsigned long state;
	struct pablo_internal_subdev subdev[IS_STREAM_COUNT][MTNR1_SD_MAX];
	struct is_priv_buf *pb_c_loader_payload;
	unsigned long kva_c_loader_payload;
	dma_addr_t dva_c_loader_payload;
	struct is_priv_buf *pb_c_loader_header;
	unsigned long kva_c_loader_header;
	dma_addr_t dva_c_loader_header;
};

void is_hw_mtnr1_s_debug_type(int type);
void is_hw_mtnr1_c_debug_type(int type);
const struct kernel_param *is_hw_mtnr1_get_debug_kernel_param_kunit_wrapper(void);

#endif
