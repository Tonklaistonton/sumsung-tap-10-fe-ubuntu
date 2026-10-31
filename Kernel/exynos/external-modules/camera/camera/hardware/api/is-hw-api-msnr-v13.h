/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MSNR HW control APIs
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_MSNR_V13_0_H
#define IS_HW_API_MSNR_V13_0_H

#include "pablo-hw-msnr.h"
#include "is-hw-common-dma.h"
#include "pablo-mmio.h"
#include "is-hw-api-type.h"

#define COREX_IGNORE (0)
#define COREX_COPY (1)
#define COREX_SWAP (2)

#define HW_TRIGGER (0)
#define SW_TRIGGER (1)

struct pablo_common_ctrl_frame_cfg;

enum set_status {
	SET_SUCCESS,
	SET_ERROR,
};

enum msnr_event_type {
	INTR_FRAME_START,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_SETTING_DONE,
	INTR_ERR,
};

enum msnr_strip_type {
	MSNR_STRIP_NONE,
	MSNR_STRIP_FIRST,
	MSNR_STRIP_LAST,
	MSNR_STRIP_MID,
};

struct msnr_radial_cfg {
	u32 sensor_full_width;
	u32 sensor_full_height;
	u32 sensor_binning_x;
	u32 sensor_binning_y;
	u32 sensor_crop_x;
	u32 sensor_crop_y;
	u32 bns_binning_x;
	u32 bns_binning_y;
	u32 sw_binning_x;
	u32 sw_binning_y;
	u32 rgbp_crop_offset_x;
	u32 rgbp_crop_offset_y;
	u32 rgbp_crop_w;
	u32 rgbp_crop_h;
};

u32 msnr_hw_is_occurred(unsigned int status, enum msnr_event_type type);
u32 msnr_hw_is_occurred1(unsigned int status, enum msnr_event_type type);
int msnr_hw_wait_idle(struct pablo_mmio *base);
void msnr_hw_dump(struct pablo_mmio *pmio, u32 mode);
void msnr_hw_s_core(struct pablo_mmio *base, u32 set_id);
void msnr_hw_dma_dump(struct is_common_dma *dma);
void msnr_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id);
int msnr_hw_s_wdma_init(struct is_common_dma *dma, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_msnr_config *config);
int msnr_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset);
void msnr_hw_g_int_en(u32 *int_en);
u32 msnr_hw_g_int_grp_en(void);
int msnr_hw_wdma_create(struct is_common_dma *dma, void *base, u32 input_id);
void msnr_hw_s_block_bypass(struct pablo_mmio *base, u32 set_id);
void msnr_hw_s_crc(struct pablo_mmio *base, u32 seed);

void msnr_hw_s_otf_path(
	struct pablo_mmio *base, u32 enable, struct pablo_common_ctrl_frame_cfg *frame_cfg);
void msnr_hw_s_otf_dlnr_path(
	struct pablo_mmio *base, u32 en, struct pablo_common_ctrl_frame_cfg *frame_cfg);
void msnr_hw_s_chain_img_size_l0(struct pablo_mmio *base, u32 w, u32 h);
void msnr_hw_s_chain_img_size_l1(struct pablo_mmio *base, u32 w, u32 h);
void msnr_hw_s_chain_img_size_l2(struct pablo_mmio *base, u32 w, u32 h);
void msnr_hw_s_chain_img_size_l3(struct pablo_mmio *base, u32 w, u32 h);
void msnr_hw_s_chain_img_size_l4(struct pablo_mmio *base, u32 w, u32 h);

void msnr_hw_s_dlnr_enable(struct pablo_mmio *base, u32 en);
void msnr_hw_s_dslme_input_enable(struct pablo_mmio *base, u32 en);
void msnr_hw_s_dslme_input_select(struct pablo_mmio *base, u32 sel);
void msnr_hw_s_dslme_config(struct pablo_mmio *base, u32 bypass, u32 w, u32 h);
void msnr_hw_s_segconf_output_enable(struct pablo_mmio *base, u32 en);

void msnr_hw_s_crop_yuv(struct pablo_mmio *base, u32 bypass, struct is_crop crop);
void msnr_hw_s_crop_hf(struct pablo_mmio *base, u32 bypass, struct is_crop crop);

void msnr_hw_s_strip_size(struct pablo_mmio *base, u32 type, u32 offset, u32 full_width);

void msnr_hw_s_radial_l0(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config);
void msnr_hw_s_radial_l1(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config);
void msnr_hw_s_radial_l2(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config);
void msnr_hw_s_radial_l3(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config);
void msnr_hw_s_radial_l4(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config);

u32 msnr_hw_g_reg_cnt(void);
void msnr_hw_s_strgen(struct pablo_mmio *base, u32 set_id);
void msnr_hw_init_pmio_config(struct pmio_config *cfg);
void msnr_hw_pmio_write_test(struct pablo_mmio *base, u32 set_id);
void msnr_hw_pmio_read_test(struct pablo_mmio *base, u32 set_id);

void msnr_hw_s_decomp_lowpower(struct pablo_mmio *base, u32 en);
#endif
