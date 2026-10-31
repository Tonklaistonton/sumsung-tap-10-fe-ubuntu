/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MTNR0 HW control APIs
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_MTNR0_V13_0_H
#define IS_HW_API_MTNR0_V13_0_H

#include "pablo-hw-mtnr0.h"
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

enum mtnr0_event_type {
	INTR_FRAME_START,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_SETTING_DONE,
	INTR_ERR,
};

enum mtnr0_dtp_type {
	MTNR0_DTP_BYPASS,
	MTNR0_DTP_SOLID_IMAGE,
	MTNR0_DTP_COLOR_BAR,
};

enum mtnr0_dtp_color_bar {
	MTNR0_DTP_COLOR_BAR_BT601,
	MTNR0_DTP_COLOR_BAR_BT709,
};

struct mtnr0_radial_cfg {
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

u32 mtnr0_hw_is_occurred(unsigned int status, enum mtnr0_event_type type);
u32 mtnr0_hw_is_occurred1(unsigned int status, enum mtnr0_event_type type);
int mtnr0_hw_wait_idle(struct pablo_mmio *base);
void mtnr0_hw_dump(struct pablo_mmio *pmio, u32 mode);
void mtnr0_hw_s_core(struct pablo_mmio *base, u32 set_id);
void mtnr0_hw_dma_dump(struct is_common_dma *dma);
void mtnr0_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id);
int mtnr0_hw_s_rdma_init(struct is_common_dma *dma, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_mtnr0_config *config);
int mtnr0_hw_s_rdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset);
int mtnr0_hw_s_wdma_init(struct is_common_dma *dma, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_mtnr0_config *config);
int mtnr0_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset);
void mtnr0_hw_g_int_en(u32 *int_en);
u32 mtnr0_hw_g_int_grp_en(void);
void mtnr0_hw_s_otf_input_mtnr1_wgt(struct pablo_mmio *base, u32 set_id, u32 enable,
	struct pablo_common_ctrl_frame_cfg *frame_cfg);
void mtnr0_hw_s_otf_output_msnr_l0(struct pablo_mmio *base, u32 set_id, u32 enable,
	struct pablo_common_ctrl_frame_cfg *frame_cfg);
void mtnr0_hw_s_input_size_l0(struct pablo_mmio *base, u32 set_id, u32 width, u32 height);
void mtnr0_hw_s_input_size_l1(struct pablo_mmio *base, u32 set_id, u32 width, u32 height);
void mtnr0_hw_s_input_size_l4(struct pablo_mmio *base, u32 set_id, u32 width, u32 height);
int mtnr0_hw_rdma_create(struct is_common_dma *dma, void *base, u32 input_id);
int mtnr0_hw_wdma_create(struct is_common_dma *dma, void *base, u32 input_id);
void mtnr0_hw_s_block_bypass(struct pablo_mmio *base, u32 set_id);
void mtnr0_hw_s_geomatch_size(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct is_mtnr0_config *mtnr_config);
void mtnr0_hw_s_mixer_size(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct mtnr0_radial_cfg *radial_cfg,
	struct is_mtnr0_config *mtnr_config);
void mtnr0_hw_s_crop_clean_img_otf(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass);
void mtnr0_hw_s_crop_wgt_otf(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass);
void mtnr0_hw_s_crop_clean_img_dma(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass);
void mtnr0_hw_s_crop_wgt_dma(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass);
void mtnr0_hw_s_img_bitshift(struct pablo_mmio *base, u32 set_id, u32 img_shift_bit);
void mtnr0_hw_g_img_bitshift(struct pablo_mmio *base, u32 set_id, u32 *shift, u32 *shift_chroma,
	u32 *offset, u32 *offset_chroma);
void mtnr0_hw_s_mono_mode(struct pablo_mmio *base, u32 set_id, bool enable);
void mtnr0_hw_s_mvf_resize_offset(struct pablo_mmio *base, u32 set_id,
		u32 in_w, u32 in_h, u32 out_w, u32 out_h, u32 pos);
void mtnr0_hw_s_crc(struct pablo_mmio *base, u32 seed);
u32 mtnr0_hw_g_reg_cnt(void);
void mtnr0_hw_s_dtp(struct pablo_mmio *base, u32 enable, enum mtnr0_dtp_type type, u32 y, u32 u,
	u32 v, enum mtnr0_dtp_color_bar cb);
void mtnr0_hw_debug_s_geomatch_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode);
void mtnr0_hw_debug_s_mixer_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode);
void mtnr0_hw_s_strgen(struct pablo_mmio *base, u32 set_id);
void mtnr0_hw_init_pmio_config(struct pmio_config *cfg);
void mtnr0_hw_pmio_write_test(struct pablo_mmio *base, u32 set_id);
void mtnr0_hw_pmio_read_test(struct pablo_mmio *base, u32 set_id);

void mtnr0_hw_s_seg_otf_to_msnr(struct pablo_mmio *base, u32 en);
void mtnr0_hw_s_still_last_frame_en(struct pablo_mmio *base, u32 en);
void mtnr0_hw_s_l0_bypass(struct pablo_mmio *base, u32 bypass);

#endif
