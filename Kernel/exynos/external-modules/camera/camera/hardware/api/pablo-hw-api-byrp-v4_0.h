/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * byrp HW control APIs
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_BYRP_4_0_H
#define IS_HW_API_BYRP_4_0_H

#include "is-hw-common-dma.h"
#include "is-type.h"
#include "is-hw-api-type.h"
#include "pablo-mmio.h"

#define COREX_IGNORE 0
#define COREX_COPY 1
#define COREX_SWAP 2

#define HW_TRIGGER 0
#define SW_TRIGGER 1

struct pablo_common_ctrl_frame_cfg;

enum is_hw_byrp_irq_src {
	BYRP_INT0,
	BYRP_INT1,
	BYRP_INTR_MAX,
};

enum byrp_input_path_type { OTF = 0, STRGEN = 1, DMA = 2 };

enum byrp_output_path {
	BYRP_ZSL_RESERVED = 0,
	BYRP_PREGAMMA_TO_WDMA,
	BYRP_CGRAS_TO_WDMA,
};

enum set_status { SET_SUCCESS, SET_ERROR };

enum byrp_event_type {
	INTR_FRAME_START,
	INTR_FRAME_CINROW,
	INTR_FRAME_CDAF,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_SETTING_DONE,
	INTR_ERR0,
	INTR_WARN0,
	INTR_ERR1
};

enum byrp_bcrop_type {
	BYRP_BCROP_BYR = 0, /* Not Use */
	BYRP_BCROP_DNG = 1, /* Not Use */
	BYRP_BCROP_MAX
};

enum byrp_img_fmt {
	BYRP_IMG_FMT_8BIT = 0,
	BYRP_IMG_FMT_10BIT = 1,
	BYRP_IMG_FMT_12BIT = 2,
	BYRP_IMG_FMT_14BIT = 3,
	BYRP_IMG_FMT_9BIT = 4,
	BYRP_IMG_FMT_11BIT = 5,
	BYRP_IMG_FMT_13BIT = 6,
	BYRP_IMG_FMT_MAX
};

enum byrp_rdma_index { BYRP_RDMA_IMG, BYRP_RDMA_MAX };

enum byrp_wdma_index {
	BYRP_WDMA_BYR,
	BYRP_WDMA_THSTAT_PRE,
	BYRP_WDMA_CDAF,
	BYRP_WDMA_RGBYHIST,
	BYRP_WDMA_THSTAT_AE,
	BYRP_WDMA_THSTAT_AWB,
	BYRP_WDMA_MAX
};

enum byrp_rdma_cfg_index { BYRP_RDMA_CFG_IMG, BYRP_RDMA_CFG_MAX };

enum byrp_wdma_cfg_index { BYRP_WDMA_CFG_BYR, BYRP_WDMA_CFG_MAX };

struct byrp_grid_cfg {
	u32 binning_x; /* @4.10 */
	u32 binning_y; /* @4.10 */
	u32 step_x; /* @10.10, virtual space */
	u32 step_y; /* @10.10, virtual space */
	u32 crop_x; /* 0-32K@15.10, virtual space */
	u32 crop_y; /* 0-24K@15.10, virtual space */
	u32 crop_radial_x;
	u32 crop_radial_y;
};

struct is_byrp_config;

void byrp_hw_s_init_common(struct pablo_mmio *base);
int byrp_hw_s_init(struct pablo_mmio *base, u32 ch_id);
void byrp_hw_s_sr(struct pablo_mmio *base, bool enable, u32 ch_id);
unsigned int byrp_hw_is_occurred(unsigned int status, enum byrp_event_type type);
int byrp_hw_wait_idle(struct pablo_mmio *base);
int byrp_hw_s_reset(struct pablo_mmio *base);
void byrp_hw_dump(struct pablo_mmio *pmio, u32 mode);
void byrp_hw_s_core(struct pablo_mmio *base, u32 num_buffers, struct byrp_param_set *param_set);
void byrp_hw_s_path(struct pablo_mmio *base, struct byrp_param_set *param_set,
	struct pablo_common_ctrl_frame_cfg *frame_cfg);
void byrp_hw_g_input_param(struct byrp_param_set *param_set, u32 instance, u32 id,
	struct param_dma_input **dma_input, dma_addr_t **input_dva);
void byrp_hw_s_rdma_init(
	struct is_common_dma *dma, struct byrp_param_set *param_set, u32 num_buffers);
int byrp_hw_rdma_create(struct is_common_dma *dma, void *base, u32 input_id);
void byrp_hw_s_wdma_init(
	struct is_common_dma *dma, struct byrp_param_set *param_set, u32 num_buffers);
int byrp_hw_wdma_create(struct is_common_dma *dma, void *base, u32 input_id);
void byrp_hw_s_dma_cfg(struct byrp_param_set *param_set, struct is_byrp_config *conf);
int byrp_hw_g_cdaf_data(struct pablo_mmio *base, void *data);
struct param_dma_output *byrp_hw_s_stat_cfg(
	u32 dma_id, dma_addr_t addr, struct byrp_param_set *p_set);
void byrp_hw_g_int_en(u32 *int_en);
u32 byrp_hw_g_int_grp_en(void);
void byrp_hw_cotf_error_handle(struct pablo_mmio *base);
void byrp_hw_s_bcrop_size(
	struct pablo_mmio *base, u32 bcrop_num, u32 x, u32 y, u32 width, u32 height);
void byrp_hw_s_mcb_size(struct pablo_mmio *base, u32 width, u32 height);
void byrp_hw_s_grid_cfg(struct pablo_mmio *base, struct byrp_grid_cfg *cfg);
void byrp_hw_s_disparity_size(struct pablo_mmio *base, struct is_hw_size_config *size_config);
void byrp_hw_s_chain_size(struct pablo_mmio *base, u32 width, u32 height);
void byrp_hw_g_chain_size(struct pablo_mmio *base, u32 set_id, u32 *width, u32 *height);
void byrp_hw_s_block_bypass(struct pablo_mmio *base);
void byrp_hw_s_bitmask(struct pablo_mmio *base, u32 bit_in, u32 bit_out);
void byrp_hw_s_strgen(struct pablo_mmio *base);
void byrp_hw_s_dtp(struct pablo_mmio *base, bool enable, u32 width, u32 height);
void byrp_hw_s_cinfifo(struct pablo_mmio *base, bool enable);
u32 byrp_hw_g_rdma_max_cnt(void);
u32 byrp_hw_g_wdma_max_cnt(void);
u32 byrp_hw_g_rdma_cfg_max_cnt(void);
u32 byrp_hw_g_wdma_cfg_max_cnt(void);
u32 byrp_hw_g_reg_cnt(void);
void byrp_hw_s_internal_shot(struct byrp_param_set *dst);
void byrp_hw_s_external_shot(
	struct is_param_region *param_region, IS_DECLARE_PMAP(pmap), struct byrp_param_set *dst);
int byrp_hw_g_rdma_param_ptr(u32 id, struct is_frame *dma_frame, struct byrp_param_set *param_set,
	char *name, dma_addr_t **dma_frame_dva, struct param_dma_input **pdi,
	pdma_addr_t **param_set_dva);
int byrp_hw_g_wdma_param_ptr(u32 id, struct is_frame *dma_frame, struct byrp_param_set *param_set,
	char *name, dma_addr_t **dma_frame_dva, struct param_dma_output **pdo,
	pdma_addr_t **param_set_dva);
void byrp_hw_init_pmio_config(struct pmio_config *cfg);
void byrp_hw_g_binning_size(struct pablo_mmio *base, u32 *binning_x, u32 *binning_y);
#endif
