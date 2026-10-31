/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MLSC HW control APIs
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_API_MLSC_H
#define PABLO_HW_API_MLSC_H

#include "is-hw-api-type.h"
#include "pablo-hw-api-common.h"
#include "is-interface-ddk.h"
#include "pablo-hw-api-common-ctrl.h"
#include "is-hw-common-dma.h"
#include "pablo-crta-interface.h"

enum mlsc_input_path { OTF = 0, DMA = 1, VOTF = 2, MLSC_INPUT_PATH_NUM };

enum mlsc_int_type {
	INT_FRAME_START,
	INT_FRAME_END,
	INT_COREX_END,
	INT_SETTING_DONE,
	INT_ERR0,
	INT_WARN0,
	INT_ERR1,
	INT_TYPE_NUM,
};

enum mlsc_dma_id {
	MLSC_DMA_NONE,
	MLSC_R_CL,
	MLSC_R_Y,
	MLSC_R_UV,
	MLSC_RDMA_NUM,
	MLSC_W_YUV444_Y = MLSC_RDMA_NUM,
	MLSC_W_YUV444_U,
	MLSC_W_YUV444_V,
	MLSC_W_GLPG0_Y,
	MLSC_W_GLPG1_Y,
	MLSC_W_GLPG1_U,
	MLSC_W_GLPG1_V,
	MLSC_W_GLPG2_Y,
	MLSC_W_GLPG2_U,
	MLSC_W_GLPG2_V,
	MLSC_W_GLPG3_Y,
	MLSC_W_GLPG3_U,
	MLSC_W_GLPG3_V,
	MLSC_W_GLPG4_Y,
	MLSC_W_GLPG4_U,
	MLSC_W_GLPG4_V,
	MLSC_W_SVHIST,
	MLSC_W_FDPIG,
	MLSC_W_LMEDS,
	MLSC_W_CAV,
	MLSC_DMA_NUM
};
struct mlsc_lic_cfg {
	bool bypass;
	enum mlsc_input_path input_path;
};

struct mlsc_size_cfg {
	u32 input_w;
	u32 input_h;
	struct is_crop rms_crop;
	u32 rms_crop_ratio;
	struct is_crop bcrop;
	struct is_rectangle lmeds_dst;
};

struct mlsc_radial_cfg {
	u32 sensor_binning_x;
	u32 sensor_binning_y;
	u32 sensor_crop_x;
	u32 sensor_crop_y;
	u32 csis_binning_x;
	u32 csis_binning_y;
	u32 rgbp_crop_offset_x;
	u32 rgbp_crop_offset_y;
	u32 rgbp_crop_w;
	u32 rgbp_crop_h;
};

/*
 * PMIO
 */
void mlsc_hw_g_pmio_cfg(struct pmio_config *pcfg);
u32 mlsc_hw_g_reg_cnt(void);

/*
 * CTRL_CONFIG
 */
int mlsc_hw_s_otf(struct pablo_mmio *pmio, bool en);
void mlsc_hw_init(struct pablo_mmio *pmio);
int mlsc_hw_wait_idle(struct pablo_mmio *pmio);
int mlsc_hw_s_reset(struct pablo_mmio *pmio);
void mlsc_hw_s_sr(struct pablo_mmio *pmio, bool enable);

/*
 * CHAIN_CONFIG
 */
void mlsc_hw_s_core(struct pablo_mmio *pmio, u32 in_w, u32 in_h);
int mlsc_hw_s_path(struct pablo_mmio *pmio, enum mlsc_input_path input,
	struct pablo_common_ctrl_frame_cfg *frame_cfg);

/*
 * Debug
 */
void mlsc_hw_s_dtp(struct pablo_mmio *base);
void mlsc_hw_s_strgen(struct pablo_mmio *base);
void mlsc_hw_dump(struct pablo_mmio *pmio, u32 mode);

/*
 * LIC
 */
void mlsc_hw_s_lic_cfg(struct pablo_mmio *pmio, struct mlsc_lic_cfg *cfg);
void mlsc_hw_s_lbctrl(struct pablo_mmio *pmio);

/*
 * Function
 */
void mlsc_hw_s_bypass(struct pablo_mmio *pmio);
int mlsc_hw_s_ds_cfg(struct pablo_mmio *pmio, enum mlsc_dma_id dma_id,
	struct mlsc_size_cfg *size_cfg, struct is_mlsc_config *conf, struct mlsc_param_set *p_set);
int mlsc_hw_s_glpg(
	struct pablo_mmio *pmio, struct mlsc_param_set *p_set, struct mlsc_size_cfg *size);
void mlsc_hw_s_config(struct pablo_mmio *pmio, struct mlsc_size_cfg *size,
	struct mlsc_param_set *p_set, struct is_mlsc_config *conf);
void mlsc_hw_s_menr_cfg(struct pablo_mmio *pmio, struct mlsc_radial_cfg *radial_cfg,
	struct is_rectangle *lmeds_dst);
u32 mlsc_hw_g_edge_score(struct pablo_mmio *pmio);

/*
 * CONTINT
 */
void mlsc_hw_g_int_en(u32 *int_en);
u32 mlsc_hw_g_int_grp_en(void);
bool mlsc_hw_is_occurred(u32 status, ulong type);
void mlsc_hw_clr_cotf_err(struct pablo_mmio *pmio);

/*
 * DMA
 */
int mlsc_hw_create_dma(struct pablo_mmio *pmio, enum mlsc_dma_id dma_id, struct is_common_dma *dma);
int mlsc_hw_s_rdma_cfg(struct is_common_dma *dma, struct mlsc_param_set *param, u32 num_buffers);
int mlsc_hw_s_wdma_cfg(
	struct is_common_dma *dma, struct mlsc_param_set *param, u32 num_buffers, int disable);
void mlsc_hw_s_dma_debug(struct pablo_mmio *pmio, enum mlsc_dma_id dma_id);

#endif /* PABLO_HW_API_MLSC_H */
