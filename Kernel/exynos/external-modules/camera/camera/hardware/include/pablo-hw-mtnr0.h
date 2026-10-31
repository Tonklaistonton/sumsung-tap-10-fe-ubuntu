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

#ifndef IS_HW_MTNR0_H
#define IS_HW_MTNR0_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"

#define SET_MTNR0_MUTLI_BUFFER_ADDR(nb, np, a)                                                     \
	do {                                                                                       \
		int __b, __p;                                                                      \
		for (__b = 1; __b < (nb); ++__b)                                                   \
			for (__p = 0; __p < (np); ++__p)                                           \
				(a)[(__b * (np)) + __p] = (a)[__p];                                \
	} while (0)

#define SET_MTNR0_MUTLI_BUFFER_ADDR_SWAP(nb, np, a1, a2)                                           \
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
	(config->skip_wdma && !config->mixerL0_still_en &&                                         \
		(config->mixerL0_mode == MTNR0_TNR_MODE_NORMAL))
#define PARTIAL(stripe_input)                                                                      \
	(stripe_input->total_count > 1 && stripe_input->index < (stripe_input->total_count - 1))
#define EVEN_BATCH(frame) (frame->num_buffers > 1 && (frame->num_buffers % 2) == 0)

enum is_hw_mtnr0_rdma_index {
	MTNR0_RDMA_CUR_L0_Y,
	MTNR0_RDMA_CUR_L4_Y,
	MTNR0_RDMA_PREV_L0_Y,
	MTNR0_RDMA_PREV_L0_Y_1,
	MTNR0_RDMA_PREV_L0_WGT,
	MTNR0_RDMA_SEG_L0,
	MTNR0_RDMA_MV_GEOMATCH,
	MTNR0_RDMA_MAX
};

enum is_hw_mtnr0_wdma_index { MTNR0_WDMA_PREV_L0_Y, MTNR0_WDMA_PREV_L0_WGT, MTNR0_WDMA_MAX };

enum is_hw_mtnr0_irq_src {
	MTNR0_INTR_0,
	MTNR0_INTR_1,
	MTNR0_INTR_MAX,
};

enum is_hw_tnr_mode {
	MTNR0_TNR_MODE_PREPARE,
	MTNR0_TNR_MODE_FIRST,
	MTNR0_TNR_MODE_NORMAL,
	MTNR0_TNR_MODE_FUSION,
};

enum is_hw_mtnr0_dbg_mode {
	MTNR0_DBG_DUMP_REG,
	MTNR0_DBG_DUMP_REG_ONCE,
	MTNR0_DBG_S2D,
	MTNR0_DBG_SKIP_DDK,
	MTNR0_DBG_BYPASS,
	MTNR0_DBG_DTP,
	MTNR0_DBG_TNR,
};

enum is_hw_mtnr0_subdev {
	MTNR0_SUBDEV_PREV_YUV,
	MTNR0_SUBDEV_PREV_W,
	MTNR0_SUBDEV_YUV,
	MTNR0_SUBDEV_W,
	MTNR0_SUBDEV_END,
	MTNR0_SUBDEV_PREV_YUV_2NR = MTNR0_SUBDEV_END,
	MTNR0_SUBDEV_PREV_W_2NR,
	MTNR0_SUBDEV_YUV_2NR,
	MTNR0_SUBDEV_W_2NR,
	MTNR0_SUBDEV_MAX,
};

static const char *mtnr0_internal_buf_name[MTNR0_SUBDEV_MAX] = {
	"MTNR0_L0Y_IN",
	"MTNR0_L0W_IN",
	"MTNR0_L0Y_OT",
	"MTNR0_L0W_OT",
	/* for 2NR */
	"MTNR0_L0Y_I2",
	"MTNR0_L0W_I2",
	"MTNR0_L0Y_O2",
	"MTNR0_L0W_O2",
};

struct is_hw_mtnr0 {
	struct is_lib_isp lib[IS_STREAM_COUNT];
	struct mtnr_param_set param_set[IS_STREAM_COUNT];
	struct is_common_dma rdma[MTNR0_RDMA_MAX];
	struct is_common_dma wdma[MTNR0_WDMA_MAX];
	struct is_mtnr0_config config[IS_STREAM_COUNT];
	struct pablo_common_ctrl *pcc;
	u32 irq_state[MTNR0_INTR_MAX];
	u32 instance;
	u32 repeat_instance;
	u32 repeat_state;
	unsigned long state;
	struct pablo_internal_subdev subdev[IS_STREAM_COUNT][MTNR0_SUBDEV_MAX];
	struct is_priv_buf *pb_c_loader_payload;
	unsigned long kva_c_loader_payload;
	dma_addr_t dva_c_loader_payload;
	struct is_priv_buf *pb_c_loader_header;
	unsigned long kva_c_loader_header;
	dma_addr_t dva_c_loader_header;
};

void is_hw_mtnr0_s_debug_type(int type);
void is_hw_mtnr0_c_debug_type(int type);
const struct kernel_param *is_hw_mtnr0_get_debug_kernel_param_kunit_wrapper(void);

#endif
