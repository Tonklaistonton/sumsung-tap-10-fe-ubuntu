// SPDX-License-Identifier: GPL-2.0
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

#include "is-hw.h"
#include "pmio.h"
#include "is-hw-common-dma.h"
#include "sfr/pablo-sfr-rgbp-v13_0.h"
#include "pablo-hw-api-rgbp.h"


/* PMIO MACRO */
#define SET_CR(base, R, val)		PMIO_SET_R(base, R, val)
#define SET_CR_F(base, R, F, val)	PMIO_SET_F(base, R, F, val)
#define SET_CR_V(base, reg_val, F, val)	PMIO_SET_V(base, reg_val, F, val)

#define GET_CR(base, R)			PMIO_GET_R(base, R)
#define GET_CR_F(base, R, F)		PMIO_GET_F(base, R, F)

/* LOG MACRO */
#define HW_NAME		"RGBP"
#define rgbp_dbg(level, fmt, args...)	dbg_hw(level, "[%s]" fmt "\n", HW_NAME, ##args)
#define rgbp_info(fmt, args...)		info_hw("[%s]" fmt "\n", HW_NAME, ##args)
#define rgbp_warn(fmt, args...)		warn_hw("[%s]" fmt, HW_NAME, ##args)
#define rgbp_err(fmt, args...)		err_hw("[%s]" fmt, HW_NAME, ##args)

/* CMDQ Interrupt group mask */
#define RGBP_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
	 BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                        \
	 BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                        \
	 BIT_MASK(PCC_INT_GRP_ENABLE_ALL))

/* Tuning Parameters */
#define HBLANK_CYCLE			0x2D
#define VBLANK_CYCLE			0xA

#define RGBP_LIC_CH_CNT			4
#define LIC_HBLALK_CYCLE 50

struct rgbp_line_buffer {
	u32 offset[RGBP_LIC_CH_CNT];
};

/* for image max width : 4080 */
/* EVT0 : assume that ctx3 is used for reprocessing */
static struct rgbp_line_buffer lb_offset_evt0[] = {
	[0].offset = { 0, 4096, 8192, 12288 }, /* offset < 26000 & 16px aligned (8ppc) */
	[1].offset = { 0, 0, 0, 0 }, /* offset < 20800 & 16px aligned (8ppc) */
	[2].offset = { 0, 4096, 8192, 12288 }, /* offset < 16384 & 16px aligned (8ppc) */
};
/* EVT1.1~ : assume that ctx0 is used for reprocessing */
static struct rgbp_line_buffer lb_offset[] = {
	[0].offset = { 0, 13712, 17808, 21904 }, /* offset < 26000 & 16px aligned (8ppc) */
	[1].offset = { 0, 0, 0, 0 }, /* offset < 20800 & 16px aligned (8ppc) */
	[2].offset = { 0, 4096, 8192, 12288 }, /* offset < 16384 & 16px aligned (8ppc) */
};

enum rgbp_cotf_in_id {
	RGBP_COTF_IN,
};

enum rgbp_cotf_out_id {
	RGBP_COTF_OUT,
};

enum byrp_lic_bit_mode {
	BAYER = 1,
	RGB = 3,
};

struct rgbp_int_mask {
	ulong bitmask;
	bool partial;
};

static struct rgbp_int_mask int_mask[INT_TYPE_NUM] = {
	[INT_FRAME_START] = { BIT_MASK(INTR0_RGBP_FRAME_START_INT), false },
	[INT_FRAME_ROW] = { BIT_MASK(INTR0_RGBP_ROW_COL_INT), false },
	[INT_FRAME_END] = { BIT_MASK(INTR0_RGBP_FRAME_END_INT), false },
	[INT_COREX_END] = { BIT_MASK(INTR0_RGBP_COREX_END_INT_0), false },
	[INT_SETTING_DONE] = { BIT_MASK(INTR0_RGBP_SETTING_DONE_INT), false },
	[INT_ERR0] = { INT0_ERR_MASK, true },
	[INT_WARN0] = { INT0_WARN_MASK, true },
	[INT_ERR1] = { INT1_ERR_MASK, true },
};

/* Internal functions */
static void _rgbp_hw_s_otf(struct pablo_mmio *pmio, bool en)
{
	if (en) {
		/* cin */
		SET_CR(pmio, RGBP_R_BYR_CINFIFO_ENABLE, 1);

		SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG,
				RGBP_F_BYR_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);
		SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG,
				RGBP_F_BYR_CINFIFO_AUTO_RECOVERY_EN, 0);
		SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG,
				RGBP_F_BYR_CINFIFO_DEBUG_EN, 1);

		SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_INTERVALS,
				RGBP_F_BYR_CINFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

		SET_CR_F(pmio, RGBP_R_CHAIN_LBCTRL_HBLANK,
				RGBP_F_CHAIN_LBCTRL_HBLANK, HBLANK_CYCLE);

		SET_CR(pmio, RGBP_R_BYR_CINFIFO_INT_ENABLE, 0xF);

		/* cout */
		SET_CR(pmio, RGBP_R_RGB_COUTFIFO_ENABLE, 1);

		SET_CR_F(pmio, RGBP_R_RGB_COUTFIFO_CONFIG,
			RGBP_F_RGB_COUTFIFO_VVALID_RISE_AT_FIRST_DATA_EN, 0);
		SET_CR_F(pmio, RGBP_R_RGB_COUTFIFO_CONFIG, RGBP_F_RGB_COUTFIFO_DEBUG_EN, 1);
		SET_CR_F(pmio, RGBP_R_RGB_COUTFIFO_CONFIG,
				RGBP_F_RGB_COUTFIFO_BACK_STALL_EN, 1);

		SET_CR_F(pmio, RGBP_R_RGB_COUTFIFO_INTERVAL_VBLANK,
			 RGBP_F_RGB_COUTFIFO_INTERVAL_VBLANK, VBLANK_CYCLE);

		SET_CR_F(pmio, RGBP_R_RGB_COUTFIFO_INTERVALS,
			 RGBP_F_RGB_COUTFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

		SET_CR(pmio, RGBP_R_RGB_COUTFIFO_INT_ENABLE, 0x7);
	} else {
		SET_CR(pmio, RGBP_R_BYR_CINFIFO_ENABLE, 0);
		SET_CR(pmio, RGBP_R_RGB_COUTFIFO_ENABLE, 0);
	}
}

void rgbp_hw_s_otf(struct pablo_mmio *pmio, bool en)
{
	_rgbp_hw_s_otf(pmio, en);
}

static void _rgbp_hw_s_cloader(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, RGBP_R_STAT_RDMACL_EN, RGBP_F_STAT_RDMACL_EN, 1);
}

static void _rgbp_hw_s_sr(struct pablo_mmio *pmio, u32 lic_ch)
{
	if (lic_ch >= RGBP_LIC_CH_CNT)
		rgbp_err("wrong lic_ch: %d", lic_ch);

	SET_CR(pmio, RGBP_R_ALLOC_SR_ENABLE, 1);
	SET_CR_F(pmio, RGBP_R_ALLOC_SR_GRP_0TO3, RGBP_F_ALLOC_SR_GRP0, lic_ch);
	SET_CR_F(pmio, RGBP_R_ALLOC_SR_GRP_0TO3, RGBP_F_ALLOC_SR_GRP1, lic_ch);
	SET_CR_F(pmio, RGBP_R_ALLOC_SR_GRP_0TO3, RGBP_F_ALLOC_SR_GRP2, lic_ch);

	SET_CR_F(pmio, RGBP_R_ALLOC_SR_GRP_4TO7, RGBP_F_ALLOC_SR_GRP4, lic_ch);
	SET_CR_F(pmio, RGBP_R_ALLOC_SR_GRP_4TO7, RGBP_F_ALLOC_SR_GRP5, lic_ch);
}

static void rgbp_hw_init(struct pablo_mmio *pmio, u32 lic_ch)
{
	_rgbp_hw_s_otf(pmio, true);
	_rgbp_hw_s_cloader(pmio);
	_rgbp_hw_s_sr(pmio, lic_ch);
}

static inline void _rgbp_hw_g_input_size(struct rgbp_param_set *param_set, struct is_crop *in)
{
	if (param_set->otf_input.cmd) {
		in->w = param_set->otf_input.width;
		in->h = param_set->otf_input.height;
	} else if (param_set->dma_input.cmd) {
		in->w = param_set->dma_input.width;
		in->h = param_set->dma_input.height;
	} else {
		in->w = param_set->otf_input.width;
		in->h = param_set->otf_input.height;
	}
}

static void _rgbp_hw_lic_cfg(struct pablo_mmio *pmio, u32 rdma_en)
{
	u32 val;
	u32 lic_bit_mode[] = { BAYER, RGB };

	val = 0;
	val = SET_CR_V(pmio, val, RGBP_F_LIC_BYPASS, 0);
	val = SET_CR_V(pmio, val, RGBP_F_LIC_DEBUG_ON, 1);
	val = SET_CR_V(pmio, val, RGBP_F_LIC_FAKE_GEN_ON, 0);
	SET_CR(pmio, RGBP_R_LIC_INPUT_MODE, val);

	SET_CR_F(pmio, RGBP_R_LIC_INPUT_CONFIG_0, RGBP_F_LIC_BIT_MODE, lic_bit_mode[rdma_en]);
	SET_CR_F(pmio, RGBP_R_LIC_INPUT_CONFIG_0, RGBP_F_LIC_RDMA_EN, rdma_en);

	val = 0;
	val = SET_CR_V(pmio, val, RGBP_F_LIC_IN_HBLANK_CYCLE, LIC_HBLALK_CYCLE);
	val = SET_CR_V(pmio, val, RGBP_F_LIC_OUT_HBLANK_CYCLE, LIC_HBLALK_CYCLE);
	SET_CR(pmio, RGBP_R_LIC_INPUT_BLANK, val);
}

static void _rgbp_hw_s_chain(struct pablo_mmio *pmio, struct rgbp_param_set *param_set)
{
	struct is_crop in, out;

	_rgbp_hw_g_input_size(param_set, &in);

	out.w = param_set->otf_output.width;
	out.h = param_set->otf_output.height;

	SET_CR_F(pmio, RGBP_R_CHAIN_SRC_IMG_SIZE, RGBP_F_CHAIN_SRC_IMG_WIDTH, in.w);
	SET_CR_F(pmio, RGBP_R_CHAIN_SRC_IMG_SIZE, RGBP_F_CHAIN_SRC_IMG_HEIGHT, in.h);

	SET_CR_F(pmio, RGBP_R_CHAIN_MCB_IMG_SIZE, RGBP_F_CHAIN_MCB_IMG_WIDTH, in.w);
	SET_CR_F(pmio, RGBP_R_CHAIN_MCB_IMG_SIZE, RGBP_F_CHAIN_MCB_IMG_HEIGHT, in.h);

	SET_CR_F(pmio, RGBP_R_CHAIN_DST_IMG_SIZE, RGBP_F_CHAIN_DST_IMG_WIDTH, out.w);
	SET_CR_F(pmio, RGBP_R_CHAIN_DST_IMG_SIZE, RGBP_F_CHAIN_DST_IMG_HEIGHT, out.h);

	_rgbp_hw_lic_cfg(pmio, param_set->dma_input.cmd);
}

static void _rgbp_hw_s_pixel_order(struct pablo_mmio *pmio, u32 pixel_order)
{
	SET_CR(pmio, RGBP_R_TETRA_TDMSC_TETRA_PIXEL_ORDER, pixel_order);
	SET_CR(pmio, RGBP_R_RGB_DMSC_PIXEL_ORDER, pixel_order);
	SET_CR(pmio, RGBP_R_BYR_CGRAS_PIXEL_ORDER, pixel_order);
	SET_CR_F(pmio, RGBP_R_BYR_WBGNONBYR_PIXEL_ORDER, RGBP_F_BYR_WBGNONBYR_PIXEL_ORDER,
		pixel_order);
	SET_CR(pmio, RGBP_R_BYR_RGBYHISTHDR_PIXEL_ORDER, pixel_order);
	SET_CR(pmio, RGBP_R_BYR_THSTATAWBHDR_PIXEL_ORDER, pixel_order);
}

static void _rgbp_hw_s_dmsc_crop(struct pablo_mmio *pmio, struct rgbp_param_set *param_set)
{
	struct param_otf_input *otf_input = &param_set->otf_input;

	SET_CR(pmio, RGBP_R_RGB_DMSCCROP_BYPASS, 0);

	SET_CR(pmio, RGBP_R_RGB_DMSCCROP_START_X, otf_input->bayer_crop_offset_x);
	SET_CR(pmio, RGBP_R_RGB_DMSCCROP_START_Y, otf_input->bayer_crop_offset_y);

	SET_CR(pmio, RGBP_R_RGB_DMSCCROP_SIZE_X, otf_input->bayer_crop_width);
	SET_CR(pmio, RGBP_R_RGB_DMSCCROP_SIZE_Y, otf_input->bayer_crop_height);
}

u32 _rgbp_hw_g_ds_sat_shitf(u32 sat_scale)
{
	u32 ret;

	if (sat_scale == 4096)
		ret = 26;
	else if (sat_scale <= 8192)
		ret = 27;
	else if (sat_scale <= 16384)
		ret = 28;
	else if (sat_scale <= 32768)
		ret = 29;
	else if (sat_scale <= 65536)
		ret = 30;
	else
		ret = 31;

	return ret;
}

static void _rgbp_hw_s_ds_sat(struct pablo_mmio *pmio, struct rgbp_param_set *param_set)
{
	struct is_crop dma_out, otf_out;
	u32 sat_inv_shift_x, sat_inv_shift_y;
	u32 sat_scale_x, sat_scale_y;
	u32 val;

	dma_out.w = param_set->dma_output_sat.width;
	dma_out.h = param_set->dma_output_sat.height;

	otf_out.w = param_set->otf_output.width;
	otf_out.h = param_set->otf_output.height;

	sat_scale_x = otf_out.w * 4096 / dma_out.w;
	SET_CR(pmio, RGBP_R_YUV_DSSAT_SCALE_X, sat_scale_x);

	sat_scale_y = otf_out.h * 4096 / dma_out.h;
	SET_CR(pmio, RGBP_R_YUV_DSSAT_SCALE_Y, sat_scale_y);

	sat_inv_shift_x = _rgbp_hw_g_ds_sat_shitf(sat_scale_x);
	sat_inv_shift_y = _rgbp_hw_g_ds_sat_shitf(sat_scale_y);

	val = 0;
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_INV_SHIFT_X, sat_inv_shift_x);
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_INV_SHIFT_Y, sat_inv_shift_y);
	SET_CR(pmio, RGBP_R_YUV_DSSAT_INV_SHIFT, val);

	val = 0;
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_INV_SCALE_X,
				(1 << sat_inv_shift_x) / sat_scale_x);
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_INV_SCALE_Y,
				(1 << sat_inv_shift_y) / sat_scale_y);
	SET_CR(pmio, RGBP_R_YUV_DSSAT_INV_SCALE, val);

	val = 0;
	val = SET_CR_V(pmio, val, RGBP_F_CHAIN_DS_SAT_IMG_WIDTH, dma_out.w);
	val = SET_CR_V(pmio, val, RGBP_F_CHAIN_DS_SAT_IMG_HEIGHT, dma_out.h);
	SET_CR(pmio, RGBP_R_CHAIN_DS_SAT_IMG_SIZE, val);

	val = 0;
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_CROP_SIZE_X, dma_out.w);
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_CROP_SIZE_Y, dma_out.h);
	SET_CR(pmio, RGBP_R_YUV_DSSAT_CROP_SIZE, val);

	val = 0;
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_OUTPUT_W, dma_out.w);
	val = SET_CR_V(pmio, val, RGBP_F_YUV_DSSAT_OUTPUT_H, dma_out.h);
	SET_CR(pmio, RGBP_R_YUV_DSSAT_OUTPUT_SIZE, val);
}

static void _rgbp_hw_s_crc(struct pablo_mmio *pmio, u32 seed)
{
	SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_STREAM_CRC, RGBP_F_BYR_CINFIFO_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_COUTFIFO_STREAM_CRC, RGBP_F_RGB_COUTFIFO_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_BYR_DTP_STREAM_CRC, RGBP_F_TETRA_TDMSC_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_TETRA_TDMSC_CRC, RGBP_F_TETRA_TDMSC_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_DMSCCROP_STREAM_CRC, RGBP_F_RGB_DMSCCROP_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_DMSC_STREAM_CRC, RGBP_F_RGB_DMSC_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_POSTGAMMA_STREAM_CRC, RGBP_F_RGB_POSTGAMMA_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_LUMAADAPLSC_STREAM_CRC, RGBP_F_RGB_LUMAADAPLSC_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_GTM_STREAM_CRC, RGBP_F_RGB_GTM_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_BYR_CGRAS_CRC, RGBP_F_BYR_CGRAS_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_BYR_WBGNONBYR_STREAM_CRC, RGBP_F_BYR_WBGNONBYR_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_BYR_PREGAMMA_STREAM_CRC, RGBP_F_BYR_PREGAMMA_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_TETRA_SMCB2_CRC, RGBP_F_TETRA_SMCB2_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_BYR_THSTATAWBHDR_CRC, RGBP_F_BYR_THSTATAWBHDR_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_BLC_STREAM_CRC, RGBP_F_RGB_BLC_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_DRCCLCT_STREAM_CRC, RGBP_F_RGB_DRCCLCT_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_CCM33SAT_CRC, RGBP_F_RGB_CCM33SAT_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_GAMMASAT_STREAM_CRC, RGBP_F_RGB_GAMMASAT_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_RGB_RGBTOYUVSAT_STREAM_CRC, RGBP_F_RGB_RGBTOYUVSAT_CRC_SEED, seed);
	SET_CR_F(pmio, RGBP_R_YUV_DSSAT_CRC, RGBP_F_YUV_DSSAT_CRC_SEED, seed);
}

static void rgbp_hw_s_core(struct pablo_mmio *pmio, struct rgbp_param_set *param_set,
	struct is_rgbp_config *cfg)
{
	u32 pixel_order, seed;

	_rgbp_hw_s_chain(pmio, param_set);

	pixel_order = param_set->otf_input.order;
	_rgbp_hw_s_pixel_order(pmio, pixel_order);

	_rgbp_hw_s_dmsc_crop(pmio, param_set);

	_rgbp_hw_s_ds_sat(pmio, param_set);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		_rgbp_hw_s_crc(pmio, seed);
}

static void _rgbp_hw_s_cin(struct pablo_mmio *pmio, struct rgbp_param_set *param_set, u32 *cotf_en)
{
	if (param_set->otf_input.cmd)
		*cotf_en |= BIT_MASK(RGBP_COTF_IN);
}

static void _rgbp_hw_s_cout(struct pablo_mmio *pmio, struct rgbp_param_set *param_set, u32 *cotf_en)
{
	if (param_set->otf_output.cmd)
		*cotf_en |= BIT_MASK(RGBP_COTF_OUT);
}

static void _rgbp_hw_s_demux(struct pablo_mmio *pmio, struct rgbp_param_set *param_set,
	struct is_rgbp_config *cfg)
{
	u32 rgbyhist_hdr_en, thstat_awb_hdr_en, drcclct_en, dssat_en;

	rgbyhist_hdr_en = !cfg->rgbyhist_hdr_bypass;
	thstat_awb_hdr_en = !cfg->thstat_awb_hdr_bypass;
	drcclct_en = !cfg->drcclct_bypass;
	dssat_en = param_set->dma_output_sat.cmd;

	SET_CR_F(pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_MCB_ENABLE,
			thstat_awb_hdr_en << 1 | rgbyhist_hdr_en);
	SET_CR_F(pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_BLC_ENABLE,
			dssat_en << 1 | drcclct_en);
}

static void rgbp_hw_s_path(struct pablo_mmio *pmio, struct rgbp_param_set *param_set,
	struct pablo_common_ctrl_frame_cfg *frame_cfg, struct is_rgbp_config *cfg)
{
	struct pablo_common_ctrl_cr_set *ext_cr_set;

	_rgbp_hw_s_cin(pmio, param_set, &frame_cfg->cotf_in_en);
	_rgbp_hw_s_cout(pmio, param_set, &frame_cfg->cotf_out_en);
	_rgbp_hw_s_demux(pmio, param_set, cfg);

	ext_cr_set = &frame_cfg->ext_cr_set;
	ext_cr_set->cr = rgbp_ext_cr_set;
	ext_cr_set->size = ARRAY_SIZE(rgbp_ext_cr_set);
}

static void rgbp_hw_s_lbctrl(struct pablo_mmio *pmio)
{
	int lic_ch;
	struct rgbp_line_buffer *lb;

	if (exynos_soc_info.main_rev >= 1 &&
		exynos_soc_info.sub_rev >= 1)
		lb = lb_offset;
	else
		lb = lb_offset_evt0;

	for (lic_ch = 0; lic_ch < RGBP_LIC_CH_CNT; lic_ch++) {
		SET_CR_F(pmio, RGBP_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x10 * lic_ch),
			RGBP_F_CHAIN_LBCTRL_OFFSET_GRP0_C0 + (lic_ch * 3), lb[0].offset[lic_ch]);
		SET_CR_F(pmio, RGBP_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x10 * lic_ch),
			RGBP_F_CHAIN_LBCTRL_OFFSET_GRP1_C0 + (lic_ch * 3), lb[1].offset[lic_ch]);
		SET_CR_F(pmio, RGBP_R_CHAIN_LBCTRL_OFFSET_GRP2TO3_C0 + (0x10 * lic_ch),
			RGBP_F_CHAIN_LBCTRL_OFFSET_GRP2_C0 + (lic_ch * 3), lb[2].offset[lic_ch]);
	}
}

static void rgbp_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = INT0_EN_MASK;
	int_en[PCC_INT_1] = INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}

static u32 rgbp_hw_g_int_grp_en(void)
{
	return RGBP_INT_GRP_EN_MASK;
}

static void rgbp_hw_s_int_on_col_row(struct pablo_mmio *pmio, bool enable, u32 col, u32 row)
{
	if (!enable) {
		SET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_COL_EN, 0);
		SET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_ROW_EN, 0);

		return;
	}

	SET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW_POS, RGBP_F_IP_INT_COL_POS, col);
	SET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW_POS, RGBP_F_IP_INT_ROW_POS, row);

	SET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_SRC_SEL, 0);
	SET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_COL_EN, 1);
	SET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_ROW_EN, 1);

	dbg_hw(2, "[RGBP]Set LINE_INT %dx%d, %x %x\n", col, row,
		GET_CR(pmio, RGBP_R_IP_INT_ON_COL_ROW_POS), GET_CR(pmio, RGBP_R_IP_INT_ON_COL_ROW));
}

static inline u32 _rgbp_hw_g_int_state(struct pablo_mmio *pmio,
		u32 int_src, u32 int_clr, bool clear)
{
	u32 int_state;

	int_state = GET_CR(pmio, int_src);

	if (clear)
		SET_CR(pmio, int_clr, int_state);

	return int_state;
}

static inline u32 _rgbp_hw_g_idleness_state(struct pablo_mmio *pmio)
{
	return GET_CR_F(pmio, RGBP_R_IDLENESS_STATUS, RGBP_F_IDLENESS_STATUS);
}

static int _rgbp_hw_wait_idleness(struct pablo_mmio *pmio)
{
	u32 retry = 0;

	while (!_rgbp_hw_g_idleness_state(pmio)) {
		if (retry++ > RGBP_TRY_COUNT) {
			rgbp_err("Failed to wait IDLENESS. retry %u", retry);
			return -ETIME;
		}

		usleep_range(3, 4);
	}

	return 0;
}

static int rgbp_hw_wait_idle(struct pablo_mmio *pmio)
{
	u32 idle, int0_state, int1_state;
	int ret;

	idle = _rgbp_hw_g_idleness_state(pmio);
	int0_state = _rgbp_hw_g_int_state(pmio, RGBP_R_INT_REQ_INT0, 0, false);
	int1_state = _rgbp_hw_g_int_state(pmio, RGBP_R_INT_REQ_INT1, 0, false);

	rgbp_info("Wait IDLENESS start. idleness 0x%x int0 0x%x int1 0x%x",
			idle, int0_state, int1_state);

	ret = _rgbp_hw_wait_idleness(pmio);

	idle = _rgbp_hw_g_idleness_state(pmio);
	int0_state = _rgbp_hw_g_int_state(pmio, RGBP_R_INT_REQ_INT0, 0, false);
	int1_state = _rgbp_hw_g_int_state(pmio, RGBP_R_INT_REQ_INT1, 0, false);

	rgbp_info("Wait IDLENESS done. idleness 0x%x int0 0x%x int1 0x%x",
			idle, int0_state, int1_state);

	return ret;
}

static int rgbp_hw_s_reset(struct pablo_mmio *pmio)
{
	int ret;
	u32 val = 0;
	u32 retry = RGBP_TRY_COUNT;

	SET_CR(pmio, RGBP_R_SW_RESET, 1);
	do {
		val = GET_CR(pmio, RGBP_R_SW_RESET);
		if (val)
			udelay(1);
		else
			break;
	} while (--retry);

	if (val) {
		err_hw("[RGBP] sw reset timeout(%#x)", val);
		return -ETIME;
	}

	ret = rgbp_hw_wait_idle(pmio);

	return ret;
}

static const struct is_reg rgbp_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0200, "RGBP_R_CHAIN_SRC_IMG_SIZE" },
	{ 0x0204, "RGBP_R_CHAIN_DST_IMG_SIZE" },
	{ 0x0208, "RGBP_R_CHAIN_MCB_IMG_SIZE" },
	{ 0x020C, "RGBP_R_CHAIN_DS_SAT_IMG_SIZE" },
	/* Chain Path */
	{ 0x0214, "RGBP_R_CHAIN_DEMUX_ENABLE" },
	/* CINFIFO 0 Status */
	{ 0x1000, "RGBP_R_BYR_CINFIFO_ENABLE" },
	{ 0x1014, "RGBP_R_BYR_CINFIFO_STATUS" },
	{ 0x1018, "RGBP_R_BYR_CINFIFO_INPUT_CNT" },
	{ 0x101c, "RGBP_R_BYR_CINFIFO_STALL_CNT" },
	{ 0x1020, "RGBP_R_BYR_CINFIFO_FIFO_FULLNESS" },
	{ 0x1040, "RGBP_R_BYR_CINFIFO_INT" },
	/* COUTFIFO 0 Status */
	{ 0x1200, "RGBP_R_RGB_COUTFIFO_ENABLE" },
	{ 0x1214, "RGBP_R_RGB_COUTFIFO_STATUS" },
	{ 0x1218, "RGBP_R_RGB_COUTFIFO_INPUT_CNT" },
	{ 0x121c, "RGBP_R_RGB_COUTFIFO_STALL_CNT" },
	{ 0x1220, "RGBP_R_RGB_COUTFIFO_FIFO_FULLNESS" },
	{ 0x1240, "RGBP_R_RGB_COUTFIFO_INT" },
	/* LIC */
	{ 0x6A20, "LIC_OUTPUT_ERROR" },
	{ 0x6A24, "LIC_OUTPUT_POSITION" },
	{ 0x6A28, "LIC_OUTPUT_MEM_STATUS" },
	{ 0x6A30, "LIC_DEBUG_IN_HVCNT" },
	{ 0x6A34, "LIC_DEBUG_IN_FCNT" },
	{ 0x6A38, "LIC_DEBUG_OUT_HVCNT" },
	{ 0x6A3c, "LIC_DEBUG_OUT_FCNT" },
};

static void _rgbp_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, RGBP_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(rgbp_dbg_cr); i++) {
		cr = &rgbp_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

static void rgbp_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		rgbp_info("%s:DUMP CR", __FILENAME__);
		is_hw_dump_regs(pmio->mmio_base, rgbp_regs, RGBP_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		rgbp_info("%s:DUMP DBG_STATE", __FILENAME__);
		_rgbp_hw_dump_dbg_state(pmio);
		break;
	default:
		rgbp_err("%s:Not supported dump_mode %d", __FILENAME__, mode);
		break;
	}
}

static bool rgbp_hw_is_occurred(u32 status, ulong type)
{
	ulong i;
	u32 bitmask = 0, partial_mask = 0;
	bool occur = true;

	for_each_set_bit(i, &type, INT_TYPE_NUM) {
		if (int_mask[i].partial)
			partial_mask |= int_mask[i].bitmask;
		else
			bitmask |= int_mask[i].bitmask;
	}

	if (bitmask)
		occur = occur && ((status & bitmask) == bitmask);

	if (partial_mask)
		occur = occur && (status & partial_mask);

	return occur;
}

static void rgbp_hw_s_strgen(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_STRGEN_MODE_EN, 1);
	SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_STRGEN_MODE_DATA_TYPE, 1);
	SET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_STRGEN_MODE_DATA, 255);
	SET_CR(pmio, RGBP_R_IP_USE_CINFIFO_NEW_FRAME_IN, 0);
}

/*
 * pattern
 * smiadtp_test_pattern_mode - select the pattern:
 * 0 - no pattern (default)
 * 1 - solid color
 * 2 - 100% color bars
 * 3 - "fade to grey" over color bars
 * 4 - PN9
 * 5...255 - reserved
 * 256 - Macbeth color chart
 * 257 - PN12
 * 258...511 - reserved
 */
static void rgbp_hw_s_dtp(struct pablo_mmio *pmio, struct rgbp_param_set *param_set, u32 enable)
{
	struct is_crop in = { 0, };

	_rgbp_hw_g_input_size(param_set, &in);

	SET_CR_F(pmio, RGBP_R_BYR_DTP_X_OUTPUT_SIZE, RGBP_F_BYR_DTP_X_OUTPUT_SIZE, in.w);
	SET_CR_F(pmio, RGBP_R_BYR_DTP_Y_OUTPUT_SIZE, RGBP_F_BYR_DTP_Y_OUTPUT_SIZE, in.h);

	if (unlikely(enable))
		SET_CR(pmio, RGBP_R_BYR_DTP_TEST_PATTERN_MODE, 2);
}

static void rgbp_hw_s_bypass(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, RGBP_R_BYR_CGRAS_BYPASS_REG, RGBP_F_BYR_CGRAS_BYPASS, 1);
	SET_CR_F(pmio, RGBP_R_RGB_DRCCLCT_BYPASS, RGBP_F_RGB_DRCCLCT_BYPASS, 1);
}

static void rgbp_hw_s_rms_crop(struct pablo_size *sensor_size, struct pablo_area *sensor_crop,
			       struct rgbp_param *rgbp_p, u32 rms_crop_ratio)
{
	struct pablo_point ss_aspect_ratio_offset;
	struct pablo_area ss_rms_crop;
	struct pablo_area rms_crop;
	struct pablo_size in_size;

	/* Get sensor crop offset of aspect ratio */
	ss_aspect_ratio_offset.x =
		(sensor_size->width - (sensor_crop->size.width * rms_crop_ratio / 10)) >> 1;
	ss_aspect_ratio_offset.y =
		(sensor_size->height - (sensor_crop->size.height * rms_crop_ratio / 10)) >> 1;

	/* Remove sensor crop offset of aspect ratio from sensor crop region */
	ss_rms_crop.offset.x = ZERO_IF_NEG(sensor_crop->offset.x - ss_aspect_ratio_offset.x);
	ss_rms_crop.offset.y = ZERO_IF_NEG(sensor_crop->offset.y - ss_aspect_ratio_offset.y);
	in_size.width = ss_rms_crop.size.width = sensor_crop->size.width;
	in_size.height = ss_rms_crop.size.height = sensor_crop->size.height;

	/**
	 * RGBP DMSC crop
	 */
	rms_crop.offset.x = rgbp_p->otf_input.bayer_crop_offset_x;
	rms_crop.offset.y = rgbp_p->otf_input.bayer_crop_offset_y;
	rms_crop.size.width = rgbp_p->otf_input.bayer_crop_width;
	rms_crop.size.height = rgbp_p->otf_input.bayer_crop_height;

	/* Move crop coordinate forward sensor remosaic crop */
	rms_crop.offset.x = rms_crop.offset.x * rms_crop_ratio / 10;
	rms_crop.offset.y = rms_crop.offset.y * rms_crop_ratio / 10;
	rms_crop.size.width = rms_crop.size.width * rms_crop_ratio / 10;
	rms_crop.size.height = rms_crop.size.height * rms_crop_ratio / 10;

	/* Remove the overlapping crop offset */
	rms_crop.offset.x = ZERO_IF_NEG(rms_crop.offset.x - ss_rms_crop.offset.x);
	rms_crop.offset.y = ZERO_IF_NEG(rms_crop.offset.y - ss_rms_crop.offset.y);

	/* Boundary check */
	rms_crop.size.width = MIN(rms_crop.size.width, in_size.width);
	rms_crop.size.height = MIN(rms_crop.size.height, in_size.height);

	/* Check HW align constraint */
	rms_crop.size.width = ALIGN_DOWN(rms_crop.size.width, 2);

	/* Update dmsc crop */
	rgbp_p->otf_input.bayer_crop_offset_x = rms_crop.offset.x;
	rgbp_p->otf_input.bayer_crop_offset_y = rms_crop.offset.y;
	in_size.width = rgbp_p->otf_input.bayer_crop_width = rms_crop.size.width;
	in_size.height = rgbp_p->otf_input.bayer_crop_height = rms_crop.size.height;

	/* Update OTF out */
	rgbp_p->otf_output.width = in_size.width;
	rgbp_p->otf_output.height = in_size.height;

	/**
	 * RGBP DS_SAT crop
	 */
	rms_crop.offset.x = rgbp_p->sat.dma_crop_offset_x;
	rms_crop.offset.y = rgbp_p->sat.dma_crop_offset_y;
	rms_crop.size.width = rgbp_p->sat.dma_crop_width;
	rms_crop.size.height = rgbp_p->sat.dma_crop_height;

	/* Scale-up to RMS crop region */
	rms_crop.offset.x = rms_crop.offset.x * rms_crop_ratio / 10;
	rms_crop.offset.y = rms_crop.offset.y * rms_crop_ratio / 10;
	rms_crop.size.width = rms_crop.size.width * rms_crop_ratio / 10;
	rms_crop.size.height = rms_crop.size.height * rms_crop_ratio / 10;

	/* Boundary check */
	rms_crop.size.width = MIN(rms_crop.size.width, in_size.width);
	rms_crop.size.height = MIN(rms_crop.size.height, in_size.height);
	if (rms_crop.offset.x + rms_crop.size.width > in_size.width)
		rms_crop.offset.x = in_size.width - rms_crop.size.width;
	if (rms_crop.offset.y + rms_crop.size.height > in_size.height)
		rms_crop.offset.y = in_size.height - rms_crop.size.height;

	/* Check HW align constraint */
	rms_crop.size.width = ALIGN_DOWN(rms_crop.size.width, 2);

	/* Update ds_sat crop */
	rgbp_p->sat.dma_crop_offset_x = rms_crop.offset.x;
	rgbp_p->sat.dma_crop_offset_y = rms_crop.offset.y;
	rgbp_p->sat.dma_crop_width = rms_crop.size.width;
	rgbp_p->sat.dma_crop_height = rms_crop.size.height;
}

static void rgbp_hw_clr_cotf_err(struct pablo_mmio *pmio)
{
	u32 val;

	val = GET_CR(pmio, RGBP_R_BYR_CINFIFO_INT);
	SET_CR(pmio, RGBP_R_BYR_CINFIFO_INT_CLEAR, val);

	val = GET_CR(pmio, RGBP_R_RGB_COUTFIFO_INT);
	SET_CR(pmio, RGBP_R_RGB_COUTFIFO_INT_CLEAR, val);
}

static const struct rgbp_hw_ops hw_ops = {
	.reset = rgbp_hw_s_reset,
	.init = rgbp_hw_init,
	.s_core = rgbp_hw_s_core,
	.s_path = rgbp_hw_s_path,
	.s_lbctrl = rgbp_hw_s_lbctrl,
	.g_int_en = rgbp_hw_g_int_en,
	.g_int_grp_en = rgbp_hw_g_int_grp_en,
	.s_int_on_col_row = rgbp_hw_s_int_on_col_row,
	.wait_idle = rgbp_hw_wait_idle,
	.dump = rgbp_hw_dump,
	.is_occurred = rgbp_hw_is_occurred,
	.s_strgen = rgbp_hw_s_strgen,
	.s_dtp = rgbp_hw_s_dtp,
	.s_bypass = rgbp_hw_s_bypass,
	.s_rms_crop = rgbp_hw_s_rms_crop,
	.clr_cotf_err = rgbp_hw_clr_cotf_err,
};

const struct rgbp_hw_ops *rgbp_hw_g_ops(void)
{
	return &hw_ops;
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_g_ops);

pdma_addr_t *rgbp_hw_g_output_dva(struct rgbp_param_set *param_set, u32 instance, u32 dma_id,
		u32 *cmd)
{
	pdma_addr_t *output_dva = NULL;

	switch (dma_id) {
	case RGBP_WDMA_HIST:
		output_dva = param_set->output_dva_hist;
		*cmd = param_set->dma_output_hist.cmd;
		break;
	case RGBP_WDMA_AWB:
		output_dva = param_set->output_dva_awb;
		*cmd = param_set->dma_output_awb.cmd;
		break;
	case RGBP_WDMA_DRC:
		*cmd = param_set->dma_output_drc.cmd;
		output_dva = param_set->output_dva_drc;
		break;
	case RGBP_WDMA_SAT:
		output_dva = param_set->output_dva_sat;
		*cmd = param_set->dma_output_sat.cmd;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, dma_id);
		break;
	}

	return output_dva;
}

pdma_addr_t *rgbp_hw_g_input_dva(struct rgbp_param_set *param_set, u32 instance, u32 dma_id,
		u32 *cmd)
{
	pdma_addr_t *input_dva = NULL;

	switch (dma_id) {
	case RGBP_RDMA_REP_R:
	case RGBP_RDMA_REP_G:
	case RGBP_RDMA_REP_B:
		input_dva = param_set->input_dva;
		*cmd = param_set->dma_input.cmd;
		break;
	default:
		merr_hw("Invalid ID (%d)", instance, dma_id);
		break;
	}

	return input_dva;
}

int rgbp_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 num_buffers,
		struct rgbp_dma_addr_cfg *dma_addr_cfg)
{
	int ret, i, p;
	dma_addr_t address[IS_MAX_FRO];
	//dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case RGBP_WDMA_HIST:
	case RGBP_WDMA_AWB:
	case RGBP_WDMA_DRC:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + i);
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, 0, 0, num_buffers);
		break;
	case RGBP_WDMA_SAT:
		for (p = 0; p < 2; p++) {
			for (i = 0; i < num_buffers; i++)
				address[i] = (dma_addr_t)*(addr + ((i * 2) + p));
			ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, p, 0, num_buffers);
		}
		break;
	default:
		err_hw("[RGBP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	// if (dma_addr_cfg->sbwc_en) {
	// 	/* Lossless, Lossy need to set header base address */
	// 	switch (dma->id) {
	// 	case RGBP_WDMA_HF:
	// 	case RGBP_WDMA_Y:
	// 	case RGBP_WDMA_UV:
	// 		for (i = 0; i < num_buffers; i++)
	// 			hdr_addr[i] = address[i] + dma_addr_cfg->payload_size;
	// 		break;
	// 	default:
	// 		break;
	// 	}

	// 	ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, 0, 0, num_buffers);
	// }

	return ret;
}

int rgbp_hw_s_rdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
		int buf_idx, u32 comp_sbwc_en, u32 payload_size)
{
	int ret, i;
	dma_addr_t address[IS_MAX_FRO];

	switch (dma->id) {
	case RGBP_RDMA_REP_R:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (3 * i));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case RGBP_RDMA_REP_G:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (3 * i + 1));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case RGBP_RDMA_REP_B:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)*(addr + (3 * i + 2));
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	default:
		rgbp_err("Invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	return ret;
}

void rgbp_hw_s_dma_cfg(struct rgbp_param_set *param_set, struct is_rgbp_config *cfg)
{
	if (!cfg->rgbyhist_hdr_bypass) {
		param_set->dma_output_hist.width =
				cfg->rgbyhist_hdr_bin_num * 4 * cfg->rgbyhist_hdr_hist_num;
		param_set->dma_output_hist.height = 1;

		if (!param_set->dma_output_hist.width || !param_set->dma_output_hist.height) {
			param_set->dma_output_hist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			rgbp_warn("[%d][F%d] Invalid RGBYHIST size %dx%d", param_set->instance,
				param_set->fcount, param_set->dma_output_hist.width,
				param_set->dma_output_hist.height);
		}

		rgbp_dbg(3, "[%d]set_config:[F%d] RGBYHIST %d, %d\n", param_set->instance,
			param_set->fcount, cfg->rgbyhist_hdr_bin_num,
			cfg->rgbyhist_hdr_hist_num);
	}

	if (!cfg->thstat_awb_hdr_bypass) {
		param_set->dma_output_awb.width = cfg->thstat_awb_hdr_grid_w * 16;
		param_set->dma_output_awb.height = cfg->thstat_awb_hdr_grid_h;

		if (!param_set->dma_output_awb.width || !param_set->dma_output_awb.height) {
			param_set->dma_output_awb.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			rgbp_warn("[%d][F%d] Invalid AWB size %dx%d", param_set->instance,
				param_set->fcount, param_set->dma_output_awb.width,
				param_set->dma_output_awb.height);
		}

		rgbp_dbg(3, "[%d]set_config:[F%d] AWB %dx%d\n", param_set->instance,
			param_set->fcount, cfg->thstat_awb_hdr_grid_w,
			cfg->thstat_awb_hdr_grid_h);
	}

	if (!cfg->drcclct_bypass) {
		param_set->dma_output_drc.width = ALIGN(cfg->drc_grid_w, 4);
		param_set->dma_output_drc.height = cfg->drc_grid_h;

		if (!param_set->dma_output_drc.width || !param_set->dma_output_drc.height) {
			param_set->dma_output_drc.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			rgbp_warn("[%d][F%d] Invalid DRCGRID size %dx%d", param_set->instance,
				param_set->fcount, param_set->dma_output_drc.width,
				param_set->dma_output_drc.height);
		}

		rgbp_dbg(3, "[%d]set_config:[F%d] DRCGRID %dx%d\n", param_set->instance,
			param_set->fcount, cfg->drc_grid_w, cfg->drc_grid_h);
	} else {
		if (param_set->dma_output_drc.cmd)
			rgbp_warn("[%d][F%d] drc wdma mismatch: dma_output_drc(%d), bypass(%d)",
				param_set->instance, param_set->fcount,
				param_set->dma_output_drc.cmd, cfg->drcclct_bypass);

		param_set->dma_output_drc.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}
}

int rgbp_hw_s_wdma_cfg(struct is_common_dma *dma, void *base, struct rgbp_param_set *param_set,
		pdma_addr_t *output_dva, struct rgbp_dma_cfg *dma_cfg)
{
	struct param_dma_output *dma_output;
	u32 stride_1p = 0, stride_2p = 0;
	u32 hwformat, memory_bitwidth, pixelsize;
	u32 width, height;
	u32 format, en_votf, bus_info, dma_format;
	u32 en_32b_pa = 0;
	bool img_flag = false;
	struct rgbp_dma_addr_cfg dma_addr_cfg;
	int ret = SET_SUCCESS;

	ret = CALL_DMA_OPS(dma, dma_enable, dma_cfg->enable);
	if (dma_cfg->enable == 0)
		return 0;

	switch (dma->id) {
	case RGBP_WDMA_HIST:
		dma_output = &param_set->dma_output_hist;
		dma_format = DMA_FMT_BAYER;
		break;
	case RGBP_WDMA_AWB:
		dma_output = &param_set->dma_output_awb;
		dma_format = DMA_FMT_BAYER;
		break;
	case RGBP_WDMA_DRC:
		dma_output = &param_set->dma_output_drc;
		dma_format = DMA_FMT_BAYER;
		dma_output->stride_plane0 = DIV_ROUND_UP(dma_output->width * 14, BITS_PER_BYTE);
		break;
	case RGBP_WDMA_SAT:
		dma_output = &param_set->dma_output_sat;
		dma_format = DMA_FMT_YUV;
		img_flag = true;
		break;
	default:
		rgbp_err("Invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	width = dma_output->width;
	height = dma_output->height;
	en_votf = dma_output->v_otf_enable;
	hwformat = dma_output->format;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;
	bus_info = en_votf ? (dma_cfg->cache_hint << 4) : 0x00000000UL;  /* cache hint [6:4] */

	stride_1p = dma_output->stride_plane0 ? dma_output->stride_plane0 : width;
	stride_2p = dma_output->stride_plane1 ? dma_output->stride_plane1 : width;

	if (dma->available_bayer_format_map)
		ret = is_hw_dma_get_bayer_format(
			memory_bitwidth, pixelsize, hwformat, false, true, &format);
	else
		format = is_hw_dma_get_yuv_format(memory_bitwidth, hwformat, dma_output->plane,
			dma_output->order);

	stride_1p = is_hw_dma_get_img_stride(
		memory_bitwidth, pixelsize, hwformat, stride_1p, 16, img_flag);

	switch (dma->id) {
	case RGBP_WDMA_SAT:
		stride_2p = stride_1p;
		break;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, dma_format);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, stride_2p, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);
	ret |= CALL_DMA_OPS(dma, dma_set_cache_32b_pa, en_32b_pa);

	if (ret)
		return ret;

	if (dma_cfg->enable == DMA_OUTPUT_COMMAND_ENABLE) {
		ret = rgbp_hw_s_wdma_addr(dma, output_dva, dma_cfg->num_buffers,
					&dma_addr_cfg);
		if (ret) {
			err_hw("[RGBP] failed to set RGBP_WDMA(%d) address", dma->id);

			return -EINVAL;
		}
	}

	dbg_hw(2, "[RGBP][%s]dma_cfg: size %dx%d format %d-%d\n", dma->name, width, height,
		dma_format, format);
	dbg_hw(2, "[RGBP][%s]stride_cfg: img %d bit_width %d/%d\n", dma->name, stride_1p, pixelsize,
		memory_bitwidth);
	dbg_hw(2, "[RGBP][%s]dma_addr: img[0] 0x%llx\n", dma->name, output_dva[0]);

	return ret;
}

int rgbp_hw_s_rdma_cfg(struct is_common_dma *dma, struct rgbp_param_set *param_set, u32 enable,
		u32 cache_hint, u32 *sbwc_en, u32 *payload_size)
{
	struct param_dma_input *dma_input;
	u32 comp_sbwc_en = 0;
	u32 stride_1p = 0;
	u32 hw_format, bit_width, pixel_size, sbwc_type;
	u32  width, height;
	u32 format, en_votf, bus_info;
	int ret;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	switch (dma->id) {
	case RGBP_RDMA_REP_R:
	case RGBP_RDMA_REP_G:
	case RGBP_RDMA_REP_B:
		dma_input = &param_set->dma_input;
		break;
	default:
		rgbp_err("Invalid dma_id[%d]", dma->id);
		return -EINVAL;
	break;
	}

	width = dma_input->width;
	height = dma_input->height;
	en_votf = dma_input->v_otf_enable;
	hw_format = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	bit_width = dma_input->bitwidth;
	pixel_size = dma_input->msb + 1;
	bus_info = en_votf ? (cache_hint << 4) : 0x00000000UL;  /* cache hint [6:4] */
	*sbwc_en = comp_sbwc_en = 0;
	*payload_size = 0;

	stride_1p = is_hw_dma_get_img_stride(bit_width, pixel_size, hw_format, width, 16, true);
	if (is_hw_dma_get_bayer_format(bit_width, pixel_size, hw_format, comp_sbwc_en, true,
		&format))
		ret |= DMA_OPS_ERROR;

	ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);

	return ret;
}

int rgbp_hw_g_rdma_param(struct is_frame *frame, dma_addr_t **frame_dva,
		struct rgbp_param_set *param_set, pdma_addr_t **param_set_dva,
		struct param_dma_input **pdi, char *name, u32 cfg_id)
{
	int ret = 0;

	switch (cfg_id) {
	case RGBP_RDMA_CFG_RGB:
		*frame_dva = frame->dvaddr_buffer;
		*pdi = &param_set->dma_input;
		*param_set_dva = param_set->input_dva;
		sprintf(name, "RGBP_RDMA_RGB");
		break;
	default:
		ret = -EINVAL;
		rgbp_err("Invalid rdma param cfg_id[%d]", cfg_id);
		break;
	}

	return ret;
}

int rgbp_hw_g_wdma_param(struct is_frame *frame, dma_addr_t **frame_dva,
		struct rgbp_param_set *param_set, pdma_addr_t **param_set_dva,
		struct param_dma_output **pdo, char *name, u32 cfg_id)
{
	int ret = 0;

	switch (cfg_id) {
	case RGBP_WDMA_CFG_HIST:
		*frame_dva = frame->dva_rgbp_hist;
		*pdo = &param_set->dma_output_hist;
		*param_set_dva = param_set->output_dva_hist;
		sprintf(name, "RGBP_WDMA_HIST");
		break;
	case RGBP_WDMA_CFG_AWB:
		*frame_dva = frame->dva_rgbp_awb;
		*pdo = &param_set->dma_output_awb;
		*param_set_dva = param_set->output_dva_awb;
		sprintf(name, "RGBP_WDMA_AWB");
		break;
	case RGBP_WDMA_CFG_DRC:
		*frame_dva = frame->dva_rgbp_drc;
		*pdo = &param_set->dma_output_drc;
		*param_set_dva = param_set->output_dva_drc;
		sprintf(name, "RGBP_WDMA_DRC");
		break;
	case RGBP_WDMA_CFG_SAT:
		*frame_dva = frame->dva_rgbp_sat;
		*pdo = &param_set->dma_output_sat;
		*param_set_dva = param_set->output_dva_sat;
		sprintf(name, "RGBP_WDMA_SAT");
		break;
	default:
		ret = -EINVAL;
		rgbp_err("Invalid wdma param cfg_id[%d]", cfg_id);
		break;
	}

	return ret;
}

void rgbp_hw_s_internal_shot(struct rgbp_param_set *param_set)
{
	param_set->dma_output_awb.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	param_set->dma_output_drc.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	param_set->dma_output_hist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	param_set->dma_output_sat.cmd = DMA_OUTPUT_COMMAND_DISABLE;
}

void rgbp_hw_s_external_shot(struct is_param_region *param_region, struct rgbp_param_set *param_set,
	IS_DECLARE_PMAP(pmap))
{
	struct rgbp_param *param;

	param = &param_region->rgbp;

	if (test_bit(PARAM_SENSOR_CONFIG, pmap))
		memcpy(&param_set->sensor_config, &param_region->sensor.config,
			sizeof(struct param_sensor_config));

	if (test_bit(PARAM_RGBP_CONTROL, pmap))
		memcpy(&param_set->control, &param->control,
			sizeof(struct param_control));

	if (test_bit(PARAM_RGBP_OTF_INPUT, pmap))
		memcpy(&param_set->otf_input, &param->otf_input,
			sizeof(struct param_otf_input));

	if (test_bit(PARAM_RGBP_OTF_OUTPUT, pmap))
		memcpy(&param_set->otf_output, &param->otf_output,
			sizeof(struct param_otf_output));

	if (test_bit(PARAM_RGBP_DMA_INPUT, pmap))
		memcpy(&param_set->dma_input, &param->dma_input,
			sizeof(struct param_dma_input));

	if (test_bit(PARAM_RGBP_HIST, pmap))
		memcpy(&param_set->dma_output_hist, &param->hist,
			sizeof(struct param_otf_output));

	if (test_bit(PARAM_RGBP_AWB, pmap))
		memcpy(&param_set->dma_output_awb, &param->awb,
			sizeof(struct param_dma_input));

	if (test_bit(PARAM_RGBP_DRC, pmap))
		memcpy(&param_set->dma_output_drc, &param->drc,
			sizeof(struct param_dma_input));

	if (test_bit(PARAM_RGBP_SAT, pmap))
		memcpy(&param_set->dma_output_sat, &param->sat,
			sizeof(struct param_dma_input));
}

static int _rgbp_hw_create_rdma(struct is_common_dma *dma, void *base, enum rgbp_rdma_id dma_id)
{
	ulong bayer_fmt_map;
	char *name;
	int ret = 0;

	name = __getname();
	if (!name) {
		rgbp_err("Failed to get name buffer");
		return -ENOMEM;
	}

	switch (dma_id) {
	case RGBP_RDMA_REP_R:
		dma->reg_ofs = RGBP_R_RGB_RDMAREPR_EN;
		dma->field_ofs = RGBP_F_RGB_RDMAREPR_EN;

		/* Bayer: 0,1,2,4,5,6,8,9,10 */
		bayer_fmt_map = 0x777;
		snprintf(name, PATH_MAX, "RGBP_RDMA_REP_R");
		break;
	case RGBP_RDMA_REP_G:
		dma->reg_ofs = RGBP_R_RGB_RDMAREPG_EN;
		dma->field_ofs = RGBP_F_RGB_RDMAREPG_EN;

		/* Bayer: 0,1,2,4,5,6,8,9,10 */
		bayer_fmt_map = 0x777;
		snprintf(name, PATH_MAX, "RGBP_RDMA_REP_G");
		break;
	case RGBP_RDMA_REP_B:
		dma->reg_ofs = RGBP_R_RGB_RDMAREPB_EN;
		dma->field_ofs = RGBP_F_RGB_RDMAREPB_EN;

		/* Bayer: 0,1,2,4,5,6,8,9,10 */
		bayer_fmt_map = 0x777;
		snprintf(name, PATH_MAX, "RGBP_RDMA_REP_B");
		break;
	default:
		rgbp_err("Invalid dma_id[%d]", dma_id);
		ret = -EINVAL;
		goto out;
	}

	pmio_dma_set_ops(dma);
	pmio_dma_create(dma, base, dma_id, name, bayer_fmt_map, 0, 0);

	CALL_DMA_OPS(dma, dma_set_corex_id, COREX_DIRECT);

	rgbp_dbg(2, "[%s] created. id %d bayer_format 0x%lX",
		name, dma_id, bayer_fmt_map);

out:
	__putname(name);

	return ret;
}

static int _rgbp_hw_create_wdma(struct is_common_dma *dma, void *base, enum rgbp_wdma_id dma_id)
{
	ulong bayer_fmt_map, yuv_fmt_map;
	char *name;
	int ret = 0;

	name = __getname();
	if (!name) {
		rgbp_err("Failed to get name buffer");
		return -ENOMEM;
	}

	bayer_fmt_map = yuv_fmt_map = 0;
	switch (dma_id) {
	case RGBP_WDMA_HIST:
		dma->reg_ofs = RGBP_R_STAT_WDMARGBYHISTHDR_EN;
		dma->field_ofs = RGBP_F_STAT_WDMARGBYHISTHDR_EN;

		/* Stat: 0 */
		bayer_fmt_map = 0x1;
		snprintf(name, PATH_MAX, "RGBP_WDMA_HIST");
		break;
	case RGBP_WDMA_AWB:
		dma->reg_ofs = RGBP_R_STAT_WDMATHSTATAWBHDR_EN;
		dma->field_ofs = RGBP_F_STAT_WDMATHSTATAWBHDR_EN;

		/* Stat: 0 */
		bayer_fmt_map = 0x1;
		snprintf(name, PATH_MAX, "RGBP_WDMA_AWB");
		break;
	case RGBP_WDMA_DRC:
		dma->reg_ofs = RGBP_R_STAT_WDMADRCGRID_EN;
		dma->field_ofs = RGBP_F_STAT_WDMADRCGRID_EN;

		/* Bayer: 12,13,14 */
		bayer_fmt_map = 0x7000;
		snprintf(name, PATH_MAX, "RGBP_WDMA_DRC");
		break;
	case RGBP_WDMA_SAT:
		dma->reg_ofs = RGBP_R_YUV_WDMASAT_EN;
		dma->field_ofs = RGBP_F_YUV_WDMASAT_EN;

		/* YUV: 7,8 */
		yuv_fmt_map = 0x180;
		snprintf(name, PATH_MAX, "RGBP_WDMA_SAT");
		break;
	default:
		rgbp_err("Invalid dma_id[%d]", dma->id);
		ret = -EINVAL;
		goto out;
	}

	pmio_dma_set_ops(dma);
	pmio_dma_create(dma, base, dma_id, name, bayer_fmt_map,
			yuv_fmt_map, 0);

	CALL_DMA_OPS(dma, dma_set_corex_id, COREX_DIRECT);

	rgbp_dbg(2, "[%s] created. id %d bayer_format 0x%lX yuv_format 0x%lX",
		name, dma_id, bayer_fmt_map, yuv_fmt_map);

out:
	__putname(name);

	return ret;
}

int rgbp_hw_create_dma(struct is_common_dma *dma, struct pablo_mmio *pmio,
		u32 dma_id, enum rgbp_dma_type type)
{
	if (type == RGBP_RDMA)
		return _rgbp_hw_create_rdma(dma, pmio, dma_id);
	else
		return _rgbp_hw_create_wdma(dma, pmio, dma_id);
}

void rgbp_hw_g_dma_cnt(u32 *dma_cnt, u32 *dma_cfg_cnt, enum rgbp_dma_type type)
{
	if (type == RGBP_RDMA) {
		*dma_cnt = RGBP_RDMA_MAX;
		*dma_cfg_cnt = RGBP_RDMA_CFG_MAX;
	} else {
		*dma_cnt = RGBP_WDMA_MAX;
		*dma_cfg_cnt = RGBP_WDMA_CFG_MAX;
	}
}

/* Debugging Methodologies */
#define USE_DLFE_IO_MOCK	0
#if IS_ENABLED(USE_DLFE_IO_MOCK)
static int rgbp_reg_read_mock(void *ctx, unsigned int reg, unsigned int *val)
{
	rgbp_info("reg_read: 0x%04x", reg);

	return 0;
}

static int rgbp_reg_write_mock(void *ctx, unsigned int reg, unsigned int val)
{
	rgbp_info("reg_write: 0x%04x 0x%08x", reg, val);

	return 0;
}
#else
#define rgbp_reg_read_mock	NULL
#define rgbp_reg_write_mock	NULL
#endif

void rgbp_hw_g_pmio_cfg(struct pmio_config *pcfg)
{
	pcfg->rd_table = &rgbp_rd_ranges_table;
	pcfg->volatile_table = &rgbp_volatile_ranges_table;

	pcfg->max_register = RGBP_R_LIC_DEBUG_OUT_FCNT;
	pcfg->num_reg_defaults_raw = (pcfg->max_register / PMIO_REG_STRIDE) + 1;
	pcfg->dma_addr_shift = LSB_BIT;

	pcfg->fields = rgbp_field_descs;
	pcfg->num_fields = ARRAY_SIZE(rgbp_field_descs);

	if (IS_ENABLED(USE_RGBP_IO_MOCK)) {
		pcfg->reg_read = rgbp_reg_read_mock;
		pcfg->reg_write = rgbp_reg_write_mock;
	}
}
KUNIT_EXPORT_SYMBOL(rgbp_hw_g_pmio_cfg);

u32 rgbp_hw_g_reg_cnt(void)
{
	return RGBP_REG_CNT + RGBP_LUT_REG_CNT;
}
