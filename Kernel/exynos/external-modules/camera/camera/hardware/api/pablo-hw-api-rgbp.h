/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * rgbp HW control APIs
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_API_RGBP_H
#define PABLO_HW_API_RGBP_H

#include "is-hw-api-type.h"
#include "pablo-hw-api-common-ctrl.h"
#include "is-hw-common-dma.h"
#include "pablo-hw-rgbp.h"

enum rgbp_dma_type{
	RGBP_RDMA,
	RGBP_WDMA,
};

enum rgbp_int_type {
	INT_FRAME_START,
	INT_FRAME_ROW,
	INT_FRAME_END,
	INT_COREX_END,
	INT_SETTING_DONE,
	INT_ERR0,
	INT_WARN0,
	INT_ERR1,
	INT_TYPE_NUM,
};

struct rgbp_hw_ops {
	int (*reset)(struct pablo_mmio *pmio);
	void (*init)(struct pablo_mmio *pmio, u32 ch);
	void (*s_core)(struct pablo_mmio *pmio, struct rgbp_param_set *param_set,
		struct is_rgbp_config *cfg);
	void (*s_path)(struct pablo_mmio *pmio, struct rgbp_param_set *param_set,
		struct pablo_common_ctrl_frame_cfg *frame_cfg, struct is_rgbp_config *cfg);
	void (*s_lbctrl)(struct pablo_mmio *pmio);
	void (*g_int_en)(u32 *int_en);
	u32 (*g_int_grp_en)(void);
	void (*s_int_on_col_row)(struct pablo_mmio *pmio, bool enable, u32 col, u32 row);
	int (*wait_idle)(struct pablo_mmio *pmio);
	void (*dump)(struct pablo_mmio *pmio, u32 mode);
	bool (*is_occurred)(u32 status, ulong type);
	void (*s_strgen)(struct pablo_mmio *pmio);
	void (*s_dtp)(struct pablo_mmio *pmio, struct rgbp_param_set *param_set, u32 enable);
	void (*s_bypass)(struct pablo_mmio *pmio);
	void (*s_rms_crop)(struct pablo_size *sensor_size, struct pablo_area *sensor_crop,
			   struct rgbp_param *rgbp_p, u32 rms_crop_ratio);
	void (*clr_cotf_err)(struct pablo_mmio *pmio);
};

const struct rgbp_hw_ops *rgbp_hw_g_ops(void);


#define	RGBP_USE_MMIO	0

#define COREX_IGNORE			(0)
#define COREX_COPY			(1)
#define COREX_SWAP			(2)

#define HW_TRIGGER			(0)
#define SW_TRIGGER			(1)

#define RGBP_RATIO_X8_8      1048576
#define RGBP_RATIO_X7_8      1198373
#define RGBP_RATIO_X6_8      1398101
#define RGBP_RATIO_X5_8      1677722
#define RGBP_RATIO_X4_8      2097152
#define RGBP_RATIO_X3_8      2796203
#define RGBP_RATIO_X2_8      4194304

enum set_status {
	SET_SUCCESS,
	SET_ERROR
};

enum rgbp_input_path_type {
	OTF = 0,
	STRGEN = 1,
	DMA = 2
};

enum rgbp_hf_output_path_type {
	HF_OFF = 0,
	HF_DMA = 1,
	HF_OTF = 2
};

enum rgbp_event_type {
	INTR0_FRAME_START_INT,
	INTR0_FRAME_END_INT,
	INTR0_CMDQ_HOLD_INT,
	INTR0_SETTING_DONE_INT,
	INTR0_C_LOADER_END_INT,
	INTR0_COREX_END_INT_0,
	INTR0_COREX_END_INT_1,
	INTR0_ROW_COL_INT,
	INTR0_FREEZE_ON_ROW_COL_INT,
	INTR0_TRANS_STOP_DONE_INT,
	INTR0_CMDQ_ERROR_INT,
	INTR0_C_LOADER_ERROR_INT,
	INTR0_COREX_ERROR_INT,
	INTR0_CINFIFO_0_OVERFLOW_ERROR_INT,
	INTR0_CINFIFO_0_OVERLAP_ERROR_INT,
	INTR0_CINFIFO_0_PIXEL_CNT_ERROR_INT,
	INTR0_CINFIFO_0_INPUT_PROTOCOL_ERROR_INT,
	INTR0_COUTFIFO_0_PIXEL_CNT_ERROR_INT,
	INTR0_COUTFIFO_0_INPUT_PROTOCOL_ERROR_INT,
	INTR0_COUTFIFO_0_OVERFLOW_ERROR_INT,
	INTR0_VOTF_GLOBAL_ERROR_INT,
	INTR0_VOTF_LOST_CONNECTION_INT,
	INTR0_OTF_SEQ_ID_ERROR_INT,
	INTR0_ERR0,
};

enum rgbp_event_type1 {
	INTR1_VOTFLOSTFLUSH_INT,
	INTR1_VOTF0RDMAGLOBALERROR_INT,
	INTR1_VOTF1WDMAGLOBALERROR_INT,
	INTR1_VOTF0RDMALOSTCONNECTION_INT,
	INTR1_VOTF0RDMALOSTFLUSH_INT,
	INTR1_VOTF1WDMALOSTFLUSH_INT,
	INTR1_DTP_DBG_CNT_ERROR_INT,
	INTR1_BDNS_DBG_CNT_ERROR_INT,
	INTR1_TDMSC_DBG_CNT_ERROR_INT,
	INTR1_DMSC_DBG_CNT_ERROR_INT,
	INTR1_LSC_DBG_CNT_ERROR_INT,
	INTR1_RGB_GAMMA_DBG_CNT_ERROR_INT,
	INTR1_GTM_DBG_CNT_ERROR_INT,
	INTR1_RGB2YUV_DBG_CNT_ERROR_INT,
	INTR1_YUV444TO422_DBG_CNT_ERROR_INT,
	INTR1_SATFLAG_DBG_CNT_ERROR_INT,
	INTR1_DECOMP_DBG_CNT_ERROR_INT,
	INTR1_CCM_DBG_CNT_ERROR_INT,
	INTR1_YUVSC_DBG_CNT_ERROR_INT,
	INTR1_PPC_CONV_DBG_CNT_ERROR_INT,
	INTR1_SYNC_MERGE_COUTFIFO_DBG_CNT_ERROR_INT,
	INTR1_ERR1,
};

enum rgbp_filter_coeff {
	RGBP_COEFF_X8_8 = 0,	/* A (8/8 ~ ) */
	RGBP_COEFF_X7_8 = 1,	/* B (7/8 ~ ) */
	RGBP_COEFF_X6_8 = 2,	/* C (6/8 ~ ) */
	RGBP_COEFF_X5_8 = 3,	/* D (5/8 ~ ) */
	RGBP_COEFF_X4_8 = 4,	/* E (4/8 ~ ) */
	RGBP_COEFF_X3_8 = 5,	/* F (3/8 ~ ) */
	RGBP_COEFF_X2_8 = 6,	/* G (2/8 ~ ) */
	RGBP_COEFF_MAX
};

enum is_rgbp_rgb_format {
	RGBP_DMA_FMT_RGB_RGBA8888 = 0,
	RGBP_DMA_FMT_RGB_ARGB8888,
	RGBP_DMA_FMT_RGB_RGBA1010102,
	RGBP_DMA_FMT_RGB_ARGB1010102,
	RGBP_DMA_FMT_RGB_RGBA16161616,
	RGBP_DMA_FMT_RGB_ARGB16161616,
	RGBP_DMA_FMT_RGB_BGRA8888 = 8,
	RGBP_DMA_FMT_RGB_ABGR8888,
	RGBP_DMA_FMT_RGB_BGRA1010102,
	RGBP_DMA_FMT_RGB_ABGR1010102,
	RGBP_DMA_FMT_RGB_BGRA16161616,
	RGBP_DMA_FMT_RGB_ABGR16161616,
};

struct rgbp_v_coef {
	int v_coef_a[9];
	int v_coef_b[9];
	int v_coef_c[9];
	int v_coef_d[9];
};

struct rgbp_h_coef {
	int h_coef_a[9];
	int h_coef_b[9];
	int h_coef_c[9];
	int h_coef_d[9];
	int h_coef_e[9];
	int h_coef_f[9];
	int h_coef_g[9];
	int h_coef_h[9];
};

struct rgbp_grid_cfg {
	u32 binning_x; /* @4.10 */
	u32 binning_y; /* @4.10 */
	u32 step_x; /* @10.10, virtual space */
	u32 step_y; /* @10.10, virtual space */
	u32 crop_x; /* 0-32K@15.10, virtual space */
	u32 crop_y; /* 0-24K@15.10, virtual space */
	u32 crop_radial_x;
	u32 crop_radial_y;
};

struct rgbp_radial_cfg {
	u32 sensor_full_width;
	u32 sensor_full_height;
	u32 sensor_binning_x;
	u32 sensor_binning_y;
	u32 sensor_crop_x;
	u32 sensor_crop_y;
	u32 bns_binning_x;
	u32 bns_binning_y;
	u32 taa_crop_x;
	u32 taa_crop_y;
	u32 taa_crop_width;
	u32 taa_crop_height;
};

struct is_rgbp_chain_set {
	u32	mux_dtp_sel;
	u32	mux_luma_sel;
	u32	mux_postgamma_sel;
	u32	mux_wdma_rep_sel;
	u32	demux_dmsc_en;
	u32	demux_yuvsc_en;
	u32	demux_rdmabyr_en;
	u32	demux_otfout_en;
	u32	satflg_en;
	u32	mux_hf_sel;
};

struct rgbp_dma_cfg {
	u32	enable;
	u32	cache_hint;
	u32	num_buffers;
};

struct rgbp_dma_addr_cfg {
	u32	sbwc_en;
	u32	payload_size;
	u32	strip_offset;
	u32	header_offset;
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_kunit_rgbp_hw_s_rgb_rdma_format(void *base, u32 rgb_format);
int pablo_kunit_rgbp_hw_s_rgb_wdma_format(void *base, u32 rgb_format);
#endif

void rgbp_hw_s_otf(struct pablo_mmio *pmio, bool en);
pdma_addr_t *rgbp_hw_g_output_dva(struct rgbp_param_set *param_set, u32 instance, u32 dma_id,
	u32 *cmd);
pdma_addr_t *rgbp_hw_g_input_dva(struct rgbp_param_set *param_set, u32 instance, u32 dma_id,
	u32 *cmd);
int rgbp_hw_s_rdma_addr(struct is_common_dma *rdma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size);

void rgbp_hw_s_dma_cfg(struct rgbp_param_set *param_set, struct is_rgbp_config *cfg);
int rgbp_hw_s_wdma_cfg(struct is_common_dma *dma, void *base, struct rgbp_param_set *param_set,
	pdma_addr_t *output_dva, struct rgbp_dma_cfg *dma_cfg);
int rgbp_hw_s_rdma_cfg(struct is_common_dma *rdma, struct rgbp_param_set *param_set, u32 enable,
	u32 cache_hint, u32 *sbwc_en, u32 *payload_size);

void rgbp_hw_s_internal_shot(struct rgbp_param_set *param_set);
void rgbp_hw_s_external_shot(struct is_param_region *param_region, struct rgbp_param_set *param_set,
	IS_DECLARE_PMAP(pmap));

int rgbp_hw_g_rdma_param(struct is_frame *frame, dma_addr_t **frame_dva,
	struct rgbp_param_set *param_set, pdma_addr_t **param_set_dva,
	struct param_dma_input **pdo, char *name, u32 cfg_id);
int rgbp_hw_g_wdma_param(struct is_frame *frame, dma_addr_t **frame_dva,
	struct rgbp_param_set *param_set, pdma_addr_t **param_set_dva,
	struct param_dma_output **pdo, char *name, u32 cfg_id);
int rgbp_hw_create_dma(struct is_common_dma *dma, struct pablo_mmio *pmio,
	enum rgbp_dma_type type, u32 dma_id);
void rgbp_hw_g_dma_cnt(u32 *dma_cnt, u32 *dma_cfg_cnt, enum rgbp_dma_type type);

void rgbp_hw_g_pmio_cfg(struct pmio_config *pcfg);

u32 rgbp_hw_g_reg_cnt(void);
#endif
