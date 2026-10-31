// SPDX-License-Identifier: GPL-2.0
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

#include <linux/delay.h>
#include "is-hw-api-mtnr0-v13.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-mtnr0-v13_0.h"
#include "pmio.h"
#include "pablo-hw-api-common-ctrl.h"

#define MTNR0_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define MTNR0_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define MTNR0_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define MTNR0_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define MTNR0_GET_R(base, R) PMIO_GET_R(base, R)

#define VBLANK_CYCLE 0xA
#define HBLANK_CYCLE 0x2E
#define PBLANK_CYCLE 0

#define info_mtnr(fmt, args...) info_common("[HW][MTNR0]", fmt, ##args)
#define info_mtnr0_ver(fmt, args...) info_common("[HW][MTNR0](v13.0)", fmt, ##args)
#define err_mtnr(fmt, args...)                                                                     \
	err_common("[HW][MTNR0][ERR]%s:%d:", fmt "\n", __func__, __LINE__, ##args)
#define dbg_mtnr(level, fmt, args...)                                                              \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_HW)) >= (level), "[HW][MTNR0]", fmt, ##args)

/* CMDQ Interrupt group mask */
#define MTNR0_INT_GRP_EN_MASK                                                                      \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
#define MTNR0_INT_GRP_EN_MASK_FRO_FIRST BIT_MASK(PCC_INT_GRP_FRAME_START)
#define MTNR0_INT_GRP_EN_MASK_FRO_MIDDLE 0
#define MTNR0_INT_GRP_EN_MASK_FRO_LAST BIT_MASK(PCC_INT_GRP_FRAME_END)

enum mtnr0_cotf_in_id {
	MTNR0_COTF_IN_MTNR1_WGT,
};

enum mtnr0_cotf_out_id {
	MTNR0_COTF_OUT_MSNR_L0,
};

static const char *const is_hw_mtnr0_rdma_name[] = {
	"MTNR0_RDMA_C_L0_Y",
	"MTNR0_RDMA_C_L4_Y",
	"MTNR0_RDMA_P_L0_Y_0",
	"MTNR0_RDMA_P_L0_Y_1",
	"MTNR0_RDMA_P_L0_WGT",
	"MTNR0_RDMA_SEG_L0",
	"MTNR0_RDMA_MV_GEOMATCH",
};

static const char *const is_hw_mtnr0_wdma_name[] = {
	"MTNR0_WDMA_P_L0_Y",
	"MTNR0_WDMA_P_L0_WGT",
};

static void __mtnr0_hw_s_secure_id(struct pablo_mmio *base, u32 set_id)
{
	MTNR0_SET_F(base, MTNR0_R_SECU_CTRL_SEQID, MTNR0_F_SECU_CTRL_SEQID,
		0); /* TODO: get secure scenario */
}

u32 mtnr0_hw_is_occurred(unsigned int status, enum mtnr0_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_MTNR0_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_MTNR0_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_MTNR0_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_MTNR0_COREX_END_INT_1;
		break;
	case INTR_SETTING_DONE:
		mask = 1 << INTR0_MTNR0_SETTING_DONE_INT;
		break;
	case INTR_ERR:
		mask = MTNR0_INT0_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

u32 mtnr0_hw_is_occurred1(unsigned int status, enum mtnr0_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_ERR:
		mask = MTNR0_INT1_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int mtnr0_hw_wait_idle(struct pablo_mmio *base)
{
	int ret = 0;
	u32 idle;
	u32 int0_all, int1_all;
	u32 try_cnt = 0;

	idle = MTNR0_GET_F(base, MTNR0_R_IDLENESS_STATUS, MTNR0_F_IDLENESS_STATUS);
	int0_all = MTNR0_GET_R(base, MTNR0_R_INT_REQ_INT0);
	int1_all = MTNR0_GET_R(base, MTNR0_R_INT_REQ_INT1);

	info_mtnr(
		"idle status before disable (idle:%d, int:0x%X, 0x%X)\n", idle, int0_all, int1_all);

	while (!idle) {
		idle = MTNR0_GET_F(base, MTNR0_R_IDLENESS_STATUS, MTNR0_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= MTNR0_TRY_COUNT) {
			err_mtnr("timeout waiting idle - disable fail");
			mtnr0_hw_dump(base, HW_DUMP_CR);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = MTNR0_GET_R(base, MTNR0_R_INT_REQ_INT0);

	info_mtnr(
		"idle status after disable (idle:%d, int:0x%X, 0x%X)\n", idle, int0_all, int1_all);

	return ret;
}

void mtnr0_hw_s_core(struct pablo_mmio *base, u32 set_id)
{
	MTNR0_SET_R(base, MTNR0_R_YUV_RDMACL_EN, 1);
	__mtnr0_hw_s_secure_id(base, set_id);
}

static const struct is_reg mtnr0_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0210, "YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L0" },
	{ 0x0214, "YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L1" },
	{ 0x0218, "YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L4" },
	/* CINFIFO 0 Status */
	{ 0x0e00, "STAT_CINFIFOMTNR1WGT_ENABLE" },
	{ 0x0e04, "STAT_CINFIFOMTNR1WGT_CONFIG" },
	{ 0x0e08, "STAT_CINFIFOMTNR1WGT_STALL_CTRL" },
	{ 0x0e0c, "STAT_CINFIFOMTNR1WGT_INTERVAL_VBLANK" },
	{ 0x0e10, "STAT_CINFIFOMTNR1WGT_INTERVALS" },
	{ 0x0e14, "STAT_CINFIFOMTNR1WGT_STATUS" },
	{ 0x0e18, "STAT_CINFIFOMTNR1WGT_INPUT_CNT" },
	{ 0x0e1c, "STAT_CINFIFOMTNR1WGT_STALL_CNT" },
	{ 0x0e20, "STAT_CINFIFOMTNR1WGT_FIFO_FULLNESS" },
	{ 0x0e40, "STAT_CINFIFOMTNR1WGT_INT" },
	{ 0x0e44, "STAT_CINFIFOMTNR1WGT_INT_ENABLE" },
	{ 0x0e48, "STAT_CINFIFOMTNR1WGT_INT_STATUS" },
	{ 0x0e4c, "STAT_CINFIFOMTNR1WGT_INT_CLEAR" },
	{ 0x0e50, "STAT_CINFIFOMTNR1WGT_CORRUPTED_COND_ENABLE" },
	{ 0x0e54, "STAT_CINFIFOMTNR1WGT_ROL_SELECT" },
	{ 0x0e70, "STAT_CINFIFOMTNR1WGT_INTERVAL_VBLANK_AR" },
	{ 0x0e74, "STAT_CINFIFOMTNR1WGT_INTERVAL_HBLANK_AR" },
	{ 0x0e7c, "STAT_CINFIFOMTNR1WGT_STREAM_CRC" },
	/* COUTFIFO 0 Status */
	{ 0x0f00, "YUV_COUTFIFOMSNRL0_ENABLE" },
	{ 0x0f04, "YUV_COUTFIFOMSNRL0_CONFIG" },
	{ 0x0f08, "YUV_COUTFIFOMSNRL0_STALL_CTRL" },
	{ 0x0f0c, "YUV_COUTFIFOMSNRL0_INTERVAL_VBLANK" },
	{ 0x0f10, "YUV_COUTFIFOMSNRL0_INTERVALS" },
	{ 0x0f14, "YUV_COUTFIFOMSNRL0_STATUS" },
	{ 0x0f18, "YUV_COUTFIFOMSNRL0_INPUT_CNT" },
	{ 0x0f1c, "YUV_COUTFIFOMSNRL0_STALL_CNT" },
	{ 0x0f20, "YUV_COUTFIFOMSNRL0_FIFO_FULLNESS" },
	{ 0x0f24, "YUV_COUTFIFOMSNRL0_VVALID_RISE_MODE" },
	{ 0x0f40, "YUV_COUTFIFOMSNRL0_INT" },
	{ 0x0f44, "YUV_COUTFIFOMSNRL0_INT_ENABLE" },
	{ 0x0f48, "YUV_COUTFIFOMSNRL0_INT_STATUS" },
	{ 0x0f4c, "YUV_COUTFIFOMSNRL0_INT_CLEAR" },
	{ 0x0f50, "YUV_COUTFIFOMSNRL0_CORRUPTED_COND_ENABLE" },
	{ 0x0f54, "YUV_COUTFIFOMSNRL0_ROL_SELECT" },
	{ 0x0f7c, "YUV_COUTFIFOMSNRL0_STREAM_CRC" },
	/* ETC */
	{ 0x7600, "YUV_MIXERL0_ENABLE" },
	{ 0x7604, "YUV_MIXERL0_STILL_EN" },
	{ 0x7608, "YUV_MIXERL0_WGT_UPDATE_EN" },
	{ 0x760c, "YUV_MIXERL0_MODE" },
	{ 0x7610, "YUV_MIXERL0_MINIMUM_FLIT_EN" },
	{ 0x7614, "YUV_MIXERL0_SAD_SHIFT" },
	{ 0x7618, "YUV_MIXERL0_SAD_Y_GAIN" },
};

static void mtnr0_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, MTNR0_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(mtnr0_dbg_cr); i++) {
		cr = &mtnr0_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

void mtnr0_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		info_mtnr0_ver("DUMP CR\n");
		is_hw_dump_regs(pmio_get_base(pmio), mtnr0_regs, MTNR0_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		info_mtnr0_ver("DUMP DBG_STATE\n");
		mtnr0_hw_dump_dbg_state(pmio);
		break;
	default:
		err_mtnr("%s:Not supported dump_mode %d", __FILENAME__, mode);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_dump);

void mtnr0_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0);
}

int mtnr0_hw_s_rdma_init(struct is_common_dma *dma, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_mtnr0_config *config)
{
	int ret = 0;
	u32 comp_sbwc_en = 0, comp_64b_align = 0;
	u32 quality_control = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_frame_width, full_dma_width;
	u32 format;
	u32 strip_offset_in_pixel = 0, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 strip_index = stripe_input->index;
	u32 strip_start_pos = (strip_index) ? (stripe_input->start_pos_x) : 0;
	u32 mv_width, mv_height;
	u32 bus_info = 0;
	u32 en_32b_pa = 0;
	bool is_image = true;
	u32 msb_align = 0;

	ret = CALL_DMA_OPS(dma, dma_enable, dma_input->cmd);

	if (dma_input->cmd == 0)
		return 0;

	full_frame_width = (strip_enable) ? stripe_input->full_width : frame_width;

	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;

	switch (dma->id) {
	case MTNR0_RDMA_CUR_L4_Y:
		if (config->mixerL0_still_en) {
			pixelsize = DMA_INOUT_BIT_WIDTH_12BIT;
			memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
		}
		fallthrough;
	case MTNR0_RDMA_CUR_L0_Y:
		strip_offset_in_pixel = dma_input->dma_crop_offset;
		width = dma_input->dma_crop_width;
		height = frame_height;
		full_dma_width = full_frame_width;

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;

		msb_align = 1;
		break;
	case MTNR0_RDMA_PREV_L0_Y:
	case MTNR0_RDMA_PREV_L0_Y_1:
		width = full_frame_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_Y;
		memory_bitwidth = config->imgL0_bit;
		pixelsize = config->imgL0_bit;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type)
			en_32b_pa = 1;

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSY_64B :
					DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;

		full_dma_width = full_frame_width;
		msb_align = 1;
		break;
	case MTNR0_RDMA_PREV_L0_WGT:
		width = ((full_frame_width / 2 + 15) / 16) * 16 * 2;
		height = (frame_height / 2 + 1) / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		en_32b_pa = 0;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		full_dma_width = width;
		break;
	case MTNR0_RDMA_MV_GEOMATCH:
		mv_width = config->mvc_in_w * 4;
		mv_height = config->mvc_in_h;
		width = mv_width;
		height = mv_height;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		strip_offset_in_byte = (strip_start_pos / mv_width) * mv_width / 8;
		full_dma_width = mv_width;
		is_image = false;
		break;
	case MTNR0_RDMA_SEG_L0:
		width = frame_width;
		height = frame_height;
		full_dma_width = full_frame_width;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		strip_offset_in_pixel = strip_start_pos;
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (en_32b_pa)
		comp_64b_align = 1;

	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 2)
		quality_control = LOSSY_COMP_FOOTPRINT;

	if (comp_sbwc_en == 0) {
		stride_1p = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, full_dma_width, 16, is_image);
		if (strip_enable && strip_offset_in_pixel)
			strip_offset_in_byte = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
				hwformat, strip_offset_in_pixel, 16, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_dma_width,
			comp_64b_align, quality_control, MTNR0_COMP_BLOCK_WIDTH,
			MTNR0_COMP_BLOCK_HEIGHT);
		header_stride_1p =
			is_hw_dma_get_header_stride(full_dma_width, MTNR0_COMP_BLOCK_WIDTH, 16);
		if (strip_enable && strip_offset_in_pixel) {
			strip_offset_in_byte = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
				strip_offset_in_pixel, comp_64b_align, quality_control,
				MTNR0_COMP_BLOCK_WIDTH, MTNR0_COMP_BLOCK_HEIGHT);
			strip_header_offset_in_byte = is_hw_dma_get_header_stride(
				strip_offset_in_pixel, MTNR0_COMP_BLOCK_WIDTH, 0);
		}
	} else {
		return -EINVAL;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);
	ret |= CALL_DMA_OPS(dma, dma_set_cache_32b_pa, en_32b_pa);
	ret |= CALL_DMA_OPS(dma, dma_set_msb_align, 0, msb_align);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + MTNR0_COMP_BLOCK_HEIGHT - 1) / MTNR0_COMP_BLOCK_HEIGHT) *
				stride_1p;
		break;
	default:
		break;
	}

	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	dbg_mtnr(3,
		"%s : dma_id %d, sbwc(%d), 64b_align(%d), quality(%d), st_ofs %d, width %d, height %d, img_ofs %d, header_ofs %d, stride (%d, %d)\n",
		dma->name, dma->id,
		comp_sbwc_en, comp_64b_align, quality_control, strip_start_pos, width, height, *strip_offset,
		*header_offset, stride_1p, header_stride_1p);
	dbg_mtnr(3,
		"%s : dma_id %d, payload_size(%d), format(%d/%d), bit(%d) pix(%d)\n",
		dma->name, dma->id, *payload_size, hwformat, format, memory_bitwidth, pixelsize);

	return ret;
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_s_rdma_init);

int mtnr0_hw_s_wdma_init(struct is_common_dma *dma, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_mtnr0_config *config)
{
	int ret = 0;
	u32 comp_sbwc_en = 0, comp_64b_align = 0;
	u32 quality_control = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_frame_width, full_dma_width = 0;
	u32 format;
	u32 strip_offset_in_pixel, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 bus_info = 0;
	u32 en_32b_pa = 0;

	ret = CALL_DMA_OPS(dma, dma_enable, dma_output->cmd);
	if (dma_output->cmd == 0) {
		ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, DMA_OUTPUT_SBWC_DISABLE);
		return ret;
	}

	full_frame_width = (strip_enable) ? stripe_input->full_width : frame_width;
	hwformat = dma_output->format;
	sbwc_type = dma_output->sbwc_type;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	switch (dma->id) {
	case MTNR0_WDMA_PREV_L0_Y:
		strip_offset_in_pixel = dma_output->dma_crop_offset_x;
		width = dma_output->dma_crop_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_YUV422;
		memory_bitwidth = config->imgL0_bit;
		pixelsize = config->imgL0_bit;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type) {
			bus_info |= 1 << IS_32B_WRITE_ALLOC_SHIFT;
			en_32b_pa = 1;
		}

		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSY_64B :
					DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;

		full_dma_width = full_frame_width;
		break;
	case MTNR0_WDMA_PREV_L0_WGT:
		width = ((dma_output->dma_crop_width / 2 + 15) / 16) * 16 * 2;
		full_dma_width = ((full_frame_width / 2 + 15) / 16) * 16 * 2;
		strip_offset_in_pixel = dma_output->dma_crop_offset_x;
		height = (frame_height / 2 + 1) / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (en_32b_pa)
		comp_64b_align = 1;

	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 2)
		quality_control = LOSSY_COMP_FOOTPRINT;

	if (comp_sbwc_en == 0) {
		stride_1p = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, full_dma_width, 16, true);
		if (strip_enable)
			strip_offset_in_byte = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
				hwformat, strip_offset_in_pixel, 16, true);

	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_dma_width,
			comp_64b_align, quality_control, MTNR0_COMP_BLOCK_WIDTH,
			MTNR0_COMP_BLOCK_HEIGHT);
		header_stride_1p =
			is_hw_dma_get_header_stride(full_dma_width, MTNR0_COMP_BLOCK_WIDTH, 16);

		if (strip_enable) {
			strip_offset_in_byte = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
				strip_offset_in_pixel, comp_64b_align, quality_control,
				MTNR0_COMP_BLOCK_WIDTH, MTNR0_COMP_BLOCK_HEIGHT);
			strip_header_offset_in_byte = is_hw_dma_get_header_stride(
				strip_offset_in_pixel, MTNR0_COMP_BLOCK_WIDTH, 0);
		}
	} else {
		return -EINVAL;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);
	ret |= CALL_DMA_OPS(dma, dma_set_cache_32b_pa, en_32b_pa);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + MTNR0_COMP_BLOCK_HEIGHT - 1) / MTNR0_COMP_BLOCK_HEIGHT) *
				stride_1p;
		break;
	default:
		break;
	}

	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	dbg_mtnr(3, "%s : dma_id %d, width %d, height %d, format(%d/%d), bit(%d)\
		 strip_offset_in_pixel %d, \
		strip_offset_in_byte %d, strip_header_offset_in_byte %d, payload_size %d\n",
		dma->name, dma->id, width, height, hwformat, format,  memory_bitwidth,
		strip_offset_in_pixel, strip_offset_in_byte,
		strip_header_offset_in_byte, *payload_size);
	dbg_mtnr(3, "%s: comp_sbwc_en %d, pixelsize %d, comp_64b_align %d, quality_control %d, \
		stride_1p %d, header_stride_1p %d\n",
		dma->name,
		comp_sbwc_en, pixelsize, comp_64b_align, quality_control, stride_1p,
		header_stride_1p);

	return ret;
}

static int mtnr0_hw_rdma_create_pmio(struct is_common_dma *dma, void *base, u32 input_id)
{
	int ret = 0;
	ulong available_bayer_format_map;
	const char *name;

	switch (input_id) {
	case MTNR0_RDMA_CUR_L0_Y:
		dma->reg_ofs = MTNR0_R_YUV_RDMACURRINL0Y_EN;
		dma->field_ofs = MTNR0_F_YUV_RDMACURRINL0Y_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_rdma_name[MTNR0_RDMA_CUR_L0_Y];
		break;
	case MTNR0_RDMA_CUR_L4_Y:
		dma->reg_ofs = MTNR0_R_YUV_RDMACURRINL4Y_EN;
		dma->field_ofs = MTNR0_F_YUV_RDMACURRINL4Y_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_rdma_name[MTNR0_RDMA_CUR_L4_Y];
		break;
	case MTNR0_RDMA_PREV_L0_Y:
		dma->reg_ofs = MTNR0_R_YUV_RDMAPREVINL0Y_EN;
		dma->field_ofs = MTNR0_F_YUV_RDMAPREVINL0Y_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_rdma_name[MTNR0_RDMA_PREV_L0_Y];
		break;
	case MTNR0_RDMA_PREV_L0_Y_1:
		dma->reg_ofs = MTNR0_R_YUV_RDMAPREVINL0Y_1_EN;
		dma->field_ofs = MTNR0_F_YUV_RDMAPREVINL0Y_1_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_rdma_name[MTNR0_RDMA_PREV_L0_Y_1];
		break;
	case MTNR0_RDMA_PREV_L0_WGT:
		dma->reg_ofs = MTNR0_R_STAT_RDMAPREVWGTINL0_EN;
		dma->field_ofs = MTNR0_F_STAT_RDMAPREVWGTINL0_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_rdma_name[MTNR0_RDMA_PREV_L0_WGT];
		break;
	case MTNR0_RDMA_SEG_L0:
		dma->reg_ofs = MTNR0_R_STAT_RDMASEGL0_EN;
		dma->field_ofs = MTNR0_F_STAT_RDMASEGL0_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_rdma_name[MTNR0_RDMA_SEG_L0];
		break;
	case MTNR0_RDMA_MV_GEOMATCH:
		dma->reg_ofs = MTNR0_R_STAT_RDMAMVGEOMATCH_EN;
		dma->field_ofs = MTNR0_F_STAT_RDMAMVGEOMATCH_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_rdma_name[MTNR0_RDMA_MV_GEOMATCH];
		break;
	default:
		err_mtnr("invalid input_id[%d]", input_id);
		return -EINVAL;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

	return ret;
}

int mtnr0_hw_rdma_create(struct is_common_dma *dma, void *base, u32 dma_id)
{
	return mtnr0_hw_rdma_create_pmio(dma, base, dma_id);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_rdma_create);

static int mtnr0_hw_wdma_create_pmio(struct is_common_dma *dma, void *base, u32 input_id)
{
	int ret = 0;
	ulong available_bayer_format_map;
	const char *name;

	switch (input_id) {
	case MTNR0_WDMA_PREV_L0_Y:
		dma->reg_ofs = MTNR0_R_YUV_WDMAPREVOUTL0Y_EN;
		dma->field_ofs = MTNR0_F_YUV_WDMAPREVOUTL0Y_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_wdma_name[MTNR0_WDMA_PREV_L0_Y];
		break;
	case MTNR0_WDMA_PREV_L0_WGT:
		dma->reg_ofs = MTNR0_R_STAT_WDMAPREVWGTOUTL0_EN;
		dma->field_ofs = MTNR0_F_STAT_WDMAPREVWGTOUTL0_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr0_wdma_name[MTNR0_WDMA_PREV_L0_WGT];
		break;
	default:
		err_mtnr("invalid input_id[%d]", input_id);
		return -EINVAL;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

	return ret;
}

int mtnr0_hw_wdma_create(struct is_common_dma *dma, void *base, u32 dma_id)
{
	return mtnr0_hw_wdma_create_pmio(dma, base, dma_id);
}

void mtnr0_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int mtnr0_hw_s_rdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset)
{
	int ret = 0, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MTNR0_RDMA_CUR_L0_Y:
	case MTNR0_RDMA_CUR_L4_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR0_RDMA_PREV_L0_Y:
	case MTNR0_RDMA_PREV_L0_Y_1:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i];
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR0_RDMA_PREV_L0_WGT:
	case MTNR0_RDMA_MV_GEOMATCH:
	case MTNR0_RDMA_SEG_L0:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if ((comp_sbwc_en == 1) || (comp_sbwc_en == 2)) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case MTNR0_RDMA_CUR_L0_Y:
		case MTNR0_RDMA_CUR_L4_Y:
		case MTNR0_RDMA_PREV_L0_Y:
		case MTNR0_RDMA_PREV_L0_Y_1:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[i] + payload_size + header_offset;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, plane, buf_idx, num_buffers);
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_s_rdma_addr);

int mtnr0_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset)
{
	int ret = 0, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MTNR0_WDMA_PREV_L0_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR0_WDMA_PREV_L0_WGT:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if ((comp_sbwc_en == 1) || (comp_sbwc_en == 2)) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case MTNR0_WDMA_PREV_L0_Y:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[i] + payload_size + header_offset;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, plane, buf_idx, num_buffers);
	}

	return ret;
}

void mtnr0_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = MTNR0_INT0_EN_MASK;
	int_en[PCC_INT_1] = MTNR0_INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_g_int_en);

u32 mtnr0_hw_g_int_grp_en(void)
{
	return MTNR0_INT_GRP_EN_MASK;
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_g_int_grp_en);

void mtnr0_hw_s_block_bypass(struct pablo_mmio *base, u32 set_id)
{
	dbg_mtnr(4, "%s\n", __func__);

	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_OTF_SEG_EN, MTNR0_F_YUV_OTF_SEG_EN, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_EN, MTNR0_F_YUV_GEOMATCHL0_EN, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_BYPASS, MTNR0_F_YUV_GEOMATCHL0_BYPASS, 1);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_GEOMATCHL0_MATCH_ENABLE, MTNR0_F_YUV_GEOMATCHL0_MATCH_ENABLE, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_TNR_WGT_EN, MTNR0_F_YUV_GEOMATCHL0_TNR_WGT_EN, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_TNR_WGT_BYPASS,
		MTNR0_F_YUV_GEOMATCHL0_TNR_WGT_BYPASS, 1);
	MTNR0_SET_F(base, MTNR0_R_STAT_MVCONTROLLER_ENABLE, MTNR0_F_STAT_MVCONTROLLER_ENABLE, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_ENABLE, MTNR0_F_YUV_MIXERL0_ENABLE, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_ENABLE, MTNR0_F_YUV_MIXERL0_MERGE_BYPASS, 1);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_STILL_EN, MTNR0_F_YUV_MIXERL0_STILL_EN, 0);

	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_UPDATE_WGTTOMEM_EN,
		MTNR0_F_YUV_MIXERL0_UPDATE_WGTTOMEM_EN, 0);

	MTNR0_SET_R(base, MTNR0_R_YUV_MIXERL0_MC_REFINE_EN, 0);

	MTNR0_SET_F(base, MTNR0_R_STAT_SEGMAPPING_BYPASS, MTNR0_F_STAT_SEGMAPPING_BYPASS, 1);
	MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANOTFL0_BYPASS, MTNR0_F_YUV_CROPCLEANOTFL0_BYPASS, 1);
	MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANDMAL0_BYPASS, MTNR0_F_YUV_CROPCLEANDMAL0_BYPASS, 1);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_CROPWEIGHTDMAL0_BYPASS, MTNR0_F_YUV_CROPWEIGHTDMAL0_BYPASS, 1);
}

void mtnr0_hw_s_otf_input_mtnr1_wgt(struct pablo_mmio *base, u32 set_id, u32 enable,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	dbg_mtnr(4, "%s en(%d)\n", __func__, enable);

	if (enable)
		frame_cfg->cotf_in_en |= BIT_MASK(MTNR0_COTF_IN_MTNR1_WGT);

	MTNR0_SET_R(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_ENABLE, 1);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_CONFIG,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_STALL_BEFORE_FRAME_START_EN, 1);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_CONFIG,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_AUTO_RECOVERY_EN, 0);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_CONFIG,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_DEBUG_EN, 1);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_INTERVALS,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_INTERVALS,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_INTERVAL_PIXEL, PBLANK_CYCLE);

	MTNR0_SET_F(base, MTNR0_R_IP_USE_CINFIFO_NEW_FRAME_IN,
		MTNR0_F_IP_USE_CINFIFO_NEW_FRAME_IN, enable);
}

void mtnr0_hw_s_otf_output_msnr_l0(struct pablo_mmio *base, u32 set_id, u32 enable,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	dbg_mtnr(4, "%s en(%d)\n", __func__, enable);

	if (enable)
		frame_cfg->cotf_out_en |= BIT_MASK(MTNR0_COTF_OUT_MSNR_L0);

	MTNR0_SET_R(base, MTNR0_R_YUV_COUTFIFOMSNRL0_ENABLE, enable);
	MTNR0_SET_F(base, MTNR0_R_YUV_COUTFIFOMSNRL0_CONFIG,
		MTNR0_F_YUV_COUTFIFOMSNRL0_VVALID_RISE_AT_FIRST_DATA_EN, 0);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_COUTFIFOMSNRL0_CONFIG, MTNR0_F_YUV_COUTFIFOMSNRL0_DEBUG_EN, 1);
	MTNR0_SET_F(base, MTNR0_R_YUV_COUTFIFOMSNRL0_INTERVAL_VBLANK,
		MTNR0_F_YUV_COUTFIFOMSNRL0_INTERVAL_VBLANK, VBLANK_CYCLE);

	MTNR0_SET_F(base, MTNR0_R_YUV_COUTFIFOMSNRL0_INTERVALS,
		MTNR0_F_YUV_COUTFIFOMSNRL0_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR0_SET_F(base, MTNR0_R_YUV_COUTFIFOMSNRL0_INTERVALS,
		MTNR0_F_YUV_COUTFIFOMSNRL0_INTERVAL_PIXEL, PBLANK_CYCLE);
}

void mtnr0_hw_s_input_size_l0(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	dbg_mtnr(4, "%s size(%dx%d)\n", __func__, width, height);

	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L0, MTNR0_F_YUV_IN_IMG_SZ_WIDTH_L0,
		width);
	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L0, MTNR0_F_YUV_IN_IMG_SZ_HEIGHT_L0,
		height);
}

void mtnr0_hw_s_input_size_l1(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	dbg_mtnr(4, "%s size(%dx%d)\n", __func__, width, height);

	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L1, MTNR0_F_YUV_IN_IMG_SZ_WIDTH_L1,
		width);
	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L1, MTNR0_F_YUV_IN_IMG_SZ_HEIGHT_L1,
		height);
}

void mtnr0_hw_s_input_size_l4(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	dbg_mtnr(4, "%s size(%dx%d)\n", __func__, width, height);

	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L4, MTNR0_F_YUV_IN_IMG_SZ_WIDTH_L4,
		width);
	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L4, MTNR0_F_YUV_IN_IMG_SZ_HEIGHT_L4,
		height);
}

void mtnr0_hw_s_geomatch_size(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct is_mtnr0_config *mtnr_config)
{
	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_REF_IMG_SIZE, MTNR0_F_YUV_GEOMATCHL0_REF_IMG_WIDTH,
		dma_width);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_REF_IMG_SIZE,
		MTNR0_F_YUV_GEOMATCHL0_REF_IMG_HEIGHT, height);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_REF_ROI_START,
		MTNR0_F_YUV_GEOMATCHL0_REF_ROI_START_X, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_REF_ROI_START,
		MTNR0_F_YUV_GEOMATCHL0_REF_ROI_START_Y, 0);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_ROI_SIZE, MTNR0_F_YUV_GEOMATCHL0_ROI_SIZE_X,
		frame_width);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_GEOMATCHL0_ROI_SIZE, MTNR0_F_YUV_GEOMATCHL0_ROI_SIZE_Y, height);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_IMG_SIZE, MTNR0_F_YUV_GEOMATCHL0_SCH_IMG_WIDTH,
		frame_width);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_IMG_SIZE,
		MTNR0_F_YUV_GEOMATCHL0_SCH_IMG_HEIGHT, height);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_ACTIVE_START,
		MTNR0_F_YUV_GEOMATCHL0_SCH_ACTIVE_START_X, strip_start_pos);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_ACTIVE_START,
		MTNR0_F_YUV_GEOMATCHL0_SCH_ACTIVE_START_Y, 0);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_ACTIVE_SIZE,
		MTNR0_F_YUV_GEOMATCHL0_SCH_ACTIVE_SIZE_X, dma_width);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_ACTIVE_SIZE,
		MTNR0_F_YUV_GEOMATCHL0_SCH_ACTIVE_SIZE_Y, height);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_ROI_START,
		MTNR0_F_YUV_GEOMATCHL0_SCH_ROI_START_X, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_SCH_ROI_START,
		MTNR0_F_YUV_GEOMATCHL0_SCH_ROI_START_Y, 0);
}

void mtnr0_hw_s_mixer_size(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct mtnr0_radial_cfg *radial_cfg,
	struct is_mtnr0_config *mtnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x *
		    radial_cfg->sw_binning_x * 1024ULL * radial_cfg->rgbp_crop_w / frame_width /
		    1000 / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y *
		    radial_cfg->sw_binning_y * 1024ULL * radial_cfg->rgbp_crop_h / height /
		    1000 / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
		   (radial_cfg->bns_binning_x * (radial_cfg->rgbp_crop_offset_x + strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
		   (radial_cfg->bns_binning_y * radial_cfg->rgbp_crop_offset_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_mtnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_BINNING_X, MTNR0_F_YUV_MIXERL0_BINNING_X, binning_x);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_BINNING_X, MTNR0_F_YUV_MIXERL0_BINNING_Y, binning_y);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_RADIAL_CENTER_X, MTNR0_F_YUV_MIXERL0_RADIAL_CENTER_X,
		radial_center_x);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_RADIAL_CENTER_X, MTNR0_F_YUV_MIXERL0_RADIAL_CENTER_Y,
		radial_center_y);
}

void mtnr0_hw_s_crop_clean_img_otf(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass)
{
	dbg_mtnr(4, "%s size(%dx%d) bypass(%d)\n", __func__, width, height, bypass);

	MTNR0_SET_F(
		base, MTNR0_R_YUV_CROPCLEANOTFL0_BYPASS, MTNR0_F_YUV_CROPCLEANOTFL0_BYPASS, bypass);

	if (!bypass) {
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANOTFL0_START,
			MTNR0_F_YUV_CROPCLEANOTFL0_START_X, start_x);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANOTFL0_START,
			MTNR0_F_YUV_CROPCLEANOTFL0_START_Y, 0);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANOTFL0_SIZE,
			MTNR0_F_YUV_CROPCLEANOTFL0_SIZE_X, width);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANOTFL0_SIZE,
			MTNR0_F_YUV_CROPCLEANOTFL0_SIZE_Y, height);
	}
}

void mtnr0_hw_s_crop_clean_img_dma(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass)
{
	dbg_mtnr(4, "%s size(%dx%d) bypass(%d)\n", __func__, width, height, bypass);

	MTNR0_SET_F(
		base, MTNR0_R_YUV_CROPCLEANDMAL0_BYPASS, MTNR0_F_YUV_CROPCLEANDMAL0_BYPASS, bypass);

	if (!bypass) {
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANDMAL0_START,
			MTNR0_F_YUV_CROPCLEANDMAL0_START_X, start_x);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANDMAL0_START,
			MTNR0_F_YUV_CROPCLEANDMAL0_START_Y, 0);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANDMAL0_SIZE,
			MTNR0_F_YUV_CROPCLEANDMAL0_SIZE_X, width);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPCLEANDMAL0_SIZE,
			MTNR0_F_YUV_CROPCLEANDMAL0_SIZE_Y, height);
	}
}

void mtnr0_hw_s_crop_wgt_dma(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass)
{
	dbg_mtnr(4, "%s size(%dx%d) bypass(%d)\n", __func__, width, height, bypass);

	MTNR0_SET_F(base, MTNR0_R_YUV_CROPWEIGHTDMAL0_BYPASS, MTNR0_F_YUV_CROPWEIGHTDMAL0_BYPASS,
		bypass);

	if (!bypass) {
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPWEIGHTDMAL0_START,
			MTNR0_F_YUV_CROPWEIGHTDMAL0_START_X, start_x);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPWEIGHTDMAL0_START,
			MTNR0_F_YUV_CROPWEIGHTDMAL0_START_Y, 0);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPWEIGHTDMAL0_SIZE,
			MTNR0_F_YUV_CROPWEIGHTDMAL0_SIZE_X, width);
		MTNR0_SET_F(base, MTNR0_R_YUV_CROPWEIGHTDMAL0_SIZE,
			MTNR0_F_YUV_CROPWEIGHTDMAL0_SIZE_Y, height);
	}
}

void mtnr0_hw_s_img_bitshift(struct pablo_mmio *base, u32 set_id, u32 img_shift_bit)
{
	if (img_shift_bit) {
		MTNR0_SET_R(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_BYPASS, 0);
		MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_LSHIFT,
			MTNR0_F_YUV_DATASHIFTERPREVRDMAL0_LSHIFT_Y, img_shift_bit);
		MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_LSHIFT,
			MTNR0_F_YUV_DATASHIFTERPREVRDMAL0_OFFSET_Y, 0);

		MTNR0_SET_R(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERWDMA_BYPASS, 0);
		MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERWDMA_RSHIFT,
			MTNR0_F_YUV_DATASHIFTERWDMAL0_RSHIFT_Y, img_shift_bit);
		MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERWDMA_RSHIFT,
			MTNR0_F_YUV_DATASHIFTERWDMAL0_OFFSET_Y, 0);
	} else {
		MTNR0_SET_R(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_BYPASS, 1);
		MTNR0_SET_R(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERWDMA_BYPASS, 1);
	}
}

void mtnr0_hw_g_img_bitshift(struct pablo_mmio *base, u32 set_id, u32 *shift, u32 *shift_chroma,
	u32 *offset, u32 *offset_chroma)
{
	*shift = MTNR0_GET_F(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_LSHIFT,
		MTNR0_F_YUV_DATASHIFTERPREVRDMAL0_LSHIFT_Y);
	*offset = MTNR0_GET_F(base, MTNR0_R_YUV_MAIN_CTRL_DATASHIFTERRDMA_LSHIFT,
		MTNR0_F_YUV_DATASHIFTERPREVRDMAL0_OFFSET_Y);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_g_img_bitshift);

void mtnr0_hw_s_mono_mode(struct pablo_mmio *base, u32 set_id, bool enable)
{
}

void mtnr0_hw_s_mvf_resize_offset(struct pablo_mmio *base, u32 set_id,
		u32 in_w, u32 in_h, u32 out_w, u32 out_h, u32 pos)
{
	int offset_x;
	int offset_y;

	int inverse_scale_x = (((1 << 8) * in_w + (out_w / 2)) / out_w);
	int resized_offset_x = (((inverse_scale_x - (1 << 8)) + 1) / 2);

	int inverse_scale_y = (((1 << 8) * in_h + (out_h / 2)) / out_h);
	int resized_offset_y = (((inverse_scale_y - (1 << 8)) + 1) / 2);

	int M = (pos / 32) * inverse_scale_x + resized_offset_x;

	offset_x = M % (1 << 8);
	offset_y = resized_offset_y;

	dbg_mtnr(4, "%s in(%dx%d), out(%dx%d), offset(%d, %d)\n", __func__,
			in_w, in_h, out_w, out_h, offset_x, offset_y);

	MTNR0_SET_F(base, MTNR0_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR0_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X, offset_x);
	MTNR0_SET_F(base, MTNR0_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR0_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_Y, offset_y);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_s_mvf_resize_offset);

void mtnr0_hw_s_crc(struct pablo_mmio *base, u32 seed)
{
	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_CRC_EN, MTNR0_F_YUV_CRC_SEED, seed);
	MTNR0_SET_F(base, MTNR0_R_YUV_DTPL0_STREAM_CRC, MTNR0_F_YUV_DTPL0_CRC_SEED, seed);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_STREAM_CRC,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_CRC_SEED, seed);
	MTNR0_SET_F(base, MTNR0_R_YUV_COUTFIFOMSNRL0_STREAM_CRC,
		MTNR0_F_YUV_COUTFIFOMSNRL0_CRC_SEED, seed);
}

void mtnr0_hw_s_dtp(struct pablo_mmio *base, u32 enable, enum mtnr0_dtp_type type, u32 y, u32 u,
	u32 v, enum mtnr0_dtp_color_bar cb)
{
	dbg_mtnr(4, "%s %d\n", __func__, enable);

	if (enable) {
		MTNR0_SET_F(
			base, MTNR0_R_YUV_DTPL0_CTRL, MTNR0_F_YUV_DTPL0_TEST_PATTERN_MODE, type);
		if (type == MTNR0_DTP_SOLID_IMAGE) {
			MTNR0_SET_F(base, MTNR0_R_YUV_DTPL0_CTRL, MTNR0_F_YUV_DTPL0_TEST_DATA_Y, y);
		} else {
			MTNR0_SET_F(
				base, MTNR0_R_YUV_DTPL0_CTRL, MTNR0_F_YUV_DTPL0_YUV_STANDARD, cb);
		}
		MTNR0_SET_F(base, MTNR0_R_YUV_DTPL0_CTRL, MTNR0_F_YUV_DTPL0_BYPASS, 0);
	} else {
		MTNR0_SET_F(base, MTNR0_R_YUV_DTPL0_CTRL, MTNR0_F_YUV_DTPL0_BYPASS, 1);
	}
}

void mtnr0_hw_debug_s_geomatch_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode)
{
	dbg_mtnr(4, "%s mode(%d)\n", __func__, tnr_mode);

	if (tnr_mode == MTNR0_TNR_MODE_PREPARE)
		MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_EN, MTNR0_F_YUV_GEOMATCHL0_EN, 0);
	else
		MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_EN, MTNR0_F_YUV_GEOMATCHL0_EN, 1);

	MTNR0_SET_F(base, MTNR0_R_YUV_GEOMATCHL0_MC_LMC_TNR_MODE,
		MTNR0_F_YUV_GEOMATCHL0_MC_LMC_TNR_MODE, tnr_mode);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_GEOMATCHL0_MATCH_ENABLE, MTNR0_F_YUV_GEOMATCHL0_MATCH_ENABLE, 0);
}

void mtnr0_hw_debug_s_mixer_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode)
{
	dbg_mtnr(4, "%s mode(%d)\n", __func__, tnr_mode);

	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_ENABLE, MTNR0_F_YUV_MIXERL0_ENABLE, 1);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_MODE, MTNR0_F_YUV_MIXERL0_MODE, tnr_mode);

	/* avoid rule checker assertion*/
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_CONFIG,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_ROL_RESET_ON_FRAME_START, 0);

	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_0,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_0, 127);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_0,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_1, 84);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_2,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_2, 101);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_2,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_3, 138);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_4,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_4, 309);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_4,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_5, 309);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_6,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_6, 309);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_SLOPE_0_6,
		MTNR0_F_YUV_MIXERL0_THRESH_SLOPE_0_7, 309);

	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_0, MTNR0_F_YUV_MIXERL0_THRESH_0_0, 86);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_0, MTNR0_F_YUV_MIXERL0_THRESH_0_1, 96);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_2, MTNR0_F_YUV_MIXERL0_THRESH_0_2, 86);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_2, MTNR0_F_YUV_MIXERL0_THRESH_0_3, 43);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_4, MTNR0_F_YUV_MIXERL0_THRESH_0_4, 21);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_4, MTNR0_F_YUV_MIXERL0_THRESH_0_5, 21);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_6, MTNR0_F_YUV_MIXERL0_THRESH_0_6, 21);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_THRESH_0_6, MTNR0_F_YUV_MIXERL0_THRESH_0_7, 21);

	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_0, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_0, 86);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_0, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_1, 96);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_2, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_2, 86);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_2, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_3, 43);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_4, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_4, 21);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_4, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_5, 21);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_6, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_6, 21);
	MTNR0_SET_F(
		base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_0_6, MTNR0_F_YUV_MIXERL0_SUB_THRESH_0_7, 21);

	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_0,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_0, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_0,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_1, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_2,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_2, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_2,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_3, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_4,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_4, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_4,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_5, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_6,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_6, 0);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_WIDTH_0_6,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_WIDTH_0_7, 0);

	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_0,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_0, 16383);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_0,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_1, 16383);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_2,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_2, 16383);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_2,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_3, 16383);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_4,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_4, 16383);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_4,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_5, 16383);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_6,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_6, 16383);
	MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_SUB_THRESH_SLOPE_0_6,
		MTNR0_F_YUV_MIXERL0_SUB_THRESH_SLOPE_0_7, 16383);

	if ((tnr_mode == MTNR0_TNR_MODE_PREPARE) || (tnr_mode == MTNR0_TNR_MODE_FUSION)) {
		MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_WGT_UPDATE_EN,
			MTNR0_F_YUV_MIXERL0_WGT_UPDATE_EN, 0);
	} else {
		MTNR0_SET_F(base, MTNR0_R_YUV_MIXERL0_WGT_UPDATE_EN,
			MTNR0_F_YUV_MIXERL0_WGT_UPDATE_EN, 1);
	}
}

void mtnr0_hw_s_strgen(struct pablo_mmio *base, u32 set_id)
{
	dbg_mtnr(4, "%s\n", __func__);

	/* STRGEN setting */
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_CONFIG,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_STRGEN_MODE_EN, 1);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_CONFIG,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_STRGEN_MODE_DATA_TYPE, 1);
	MTNR0_SET_F(base, MTNR0_R_STAT_CINFIFOMTNR1WGT_CONFIG,
		MTNR0_F_STAT_CINFIFOMTNR1WGT_STRGEN_MODE_DATA, 255);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_s_strgen);

void mtnr0_hw_s_seg_otf_to_msnr(struct pablo_mmio *base, u32 en)
{
	dbg_mtnr(4, "%s, enable %d\n", __func__, en);

	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_OTF_SEG_EN, MTNR0_F_YUV_OTF_SEG_EN, en);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_s_seg_otf_to_msnr);

void mtnr0_hw_s_still_last_frame_en(struct pablo_mmio *base, u32 en)
{
	dbg_mtnr(4, "%s, en(%d)\n", __func__, en);

	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_STILL_LAST_FRAME_EN,
		MTNR0_F_YUV_MAIN_CTRL_STILL_LAST_FRAME_EN, en);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_s_still_last_frame_en);

void mtnr0_hw_s_l0_bypass(struct pablo_mmio *base, u32 bypass)
{
	dbg_mtnr(4, "%s, bypass(%d)\n", __func__, bypass);

	MTNR0_SET_F(base, MTNR0_R_YUV_MAIN_CTRL_L0_BYPASS, MTNR0_F_YUV_MAIN_CTRL_L0_BYPASS, bypass);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_s_l0_bypass);

u32 mtnr0_hw_g_reg_cnt(void)
{
	return MTNR0_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_g_reg_cnt);

const struct pmio_field_desc *mtnr0_hw_g_field_descs(void)
{
	return mtnr0_field_descs;
}

unsigned int mtnr0_hw_g_num_field_descs(void)
{
	return ARRAY_SIZE(mtnr0_field_descs);
}

const struct pmio_access_table *mtnr0_hw_g_access_table(int type)
{
	switch (type) {
	case 0:
		return &mtnr0_volatile_ranges_table;
	case 1:
		return &mtnr0_wr_noinc_ranges_table;
	default:
		return NULL;
	};

	return NULL;
}

void mtnr0_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &mtnr0_volatile_ranges_table;
	cfg->wr_noinc_table = &mtnr0_wr_noinc_ranges_table;

	cfg->max_register = MTNR0_R_YUV_CRC_CMN_WDMA_IN;
	cfg->num_reg_defaults_raw = (MTNR0_R_YUV_CRC_CMN_WDMA_IN >> 2) + 1;
	cfg->dma_addr_shift = 4;

	cfg->ranges = mtnr0_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(mtnr0_range_cfgs);

	cfg->fields = mtnr0_field_descs;
	cfg->num_fields = ARRAY_SIZE(mtnr0_field_descs);
}
KUNIT_EXPORT_SYMBOL(mtnr0_hw_init_pmio_config);
