// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * yuvp HW control APIs
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "is-hw-api-yuvp-v3_0.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-yuvp-v13_0.h"
#include "pmio.h"
#include "pablo-hw-api-common-ctrl.h"

#define YUVP_LUT_REG_CNT 2560
#define YUVP_LUT_NUM 2 /* DRC, Face DRC */

/* CMDQ Interrupt group mask */
#define YUVP_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
#define YUVP_INT_GRP_EN_MASK_FRO_FIRST BIT_MASK(PCC_INT_GRP_FRAME_START)
#define YUVP_INT_GRP_EN_MASK_FRO_MIDDLE 0
#define YUVP_INT_GRP_EN_MASK_FRO_LAST BIT_MASK(PCC_INT_GRP_FRAME_END)

enum mtnr0_cotf_in_id {
	YUVP_COTF_IN_YUV,
};

enum mtnr0_cotf_out_id {
	YUVP_COTF_OUT_YUV,
};

unsigned int yuvp_hw_is_occurred0(unsigned int status, enum yuvp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s, status(0x%x) type(%d)\n", __func__, status, type);

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_YUVP_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_YUVP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_YUVP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_YUVP_COREX_END_INT_1;
		break;
	case INTR_SETTING_DONE:
		mask = 1 << INTR0_YUVP_SETTING_DONE_INT;
		break;
	case INTR_ERR:
		mask = INT0_YUVP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_is_occurred0);

unsigned int yuvp_hw_is_occurred1(unsigned int status, enum yuvp_event_type type)
{
	u32 mask;

	dbg_hw(4, "[API] %s, status(0x%x) type(%d)\n", __func__, status, type);

	switch (type) {
	case INTR_ERR:
		mask = INT1_YUVP_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_is_occurred1);

void yuvp_hw_s_init(void *base)
{
	/* do nothing */
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_init);

int yuvp_hw_wait_idle(void *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	idle = 0;
	int_all = 0;
	idle = YUVP_GET_F(base, YUVP_R_IDLENESS_STATUS, YUVP_F_IDLENESS_STATUS);
	int_all = YUVP_GET_R(base, YUVP_R_INT_REQ_INT0);

	info_hw("[YUVP] idle status before disable (idle:%d, int1:0x%X)\n", idle, int_all);

	while (!idle) {
		idle = YUVP_GET_F(base, YUVP_R_IDLENESS_STATUS, YUVP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= YUVP_TRY_COUNT) {
			err_hw("[YUVPP] timeout waiting idle - disable fail");
			yuvp_hw_dump(base, HW_DUMP_CR);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int_all = YUVP_GET_R(base, YUVP_R_INT_REQ_INT0);

	info_hw("[YUVP] idle status after disable (idle:%d, int1:0x%X)\n", idle, int_all);

	return ret;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_wait_idle);

static const struct is_reg yuvp_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0200, "CHAIN_IMG_SIZE" },
	{ 0x0210, "CHAIN_MUX_SELECT" },
	{ 0x0214, "CHAIN_DEMUX_ENABLE" },
	{ 0x0218, "RDMA_IN_FORMAT" },
	{ 0x0250, "YUV_CROP_CTRL" },
	{ 0x0254, "YUV_CROP_START" },
	{ 0x0258, "YUV_CROP_SIZE" },
	{ 0x0260, "MCSC_RDMA_SIZE" },
	/* CINFIFO Status */
	{ 0x0e00, "YUV_CINFIFO_ENABLE" },
	{ 0x0e14, "YUV_CINFIFO_STATUS" },
	{ 0x0e18, "YUV_CINFIFO_INPUT_CNT" },
	{ 0x0e1c, "YUV_CINFIFO_STALL_CNT" },
	{ 0x0e20, "YUV_CINFIFO_FIFO_FULLNESS" },
	{ 0x0e40, "YUV_CINFIFO_INT" },
	/* COUTFIFO Status */
	{ 0x0f00, "YUV_COUTFIFO_ENABLE" },
	{ 0x0f14, "YUV_COUTFIFO_STATUS" },
	{ 0x0f18, "YUV_COUTFIFO_INPUT_CNT" },
	{ 0x0f1c, "YUV_COUTFIFO_STALL_CNT" },
	{ 0x0f20, "YUV_COUTFIFO_FIFO_FULLNESS" },
	{ 0x0f40, "YUV_COUTFIFO_INT" },
};

static void yuvp_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, YUVP_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(yuvp_dbg_cr); i++) {
		cr = &yuvp_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);

	for (i = 0; i < YUVP_CHAIN_DEBUG_COUNTER_MAX; i++) {
		YUVP_SET_R(pmio, YUVP_R_CHAIN_DEBUG_CNT_SEL, yuvp_counter[i].counter);
		sfrinfo("[CHAIN_DEBUG] counter:[%s], row:[%d], col:[%d]\n", yuvp_counter[i].name,
			YUVP_GET_F(pmio, YUVP_R_CHAIN_DEBUG_CNT_VAL, YUVP_F_CHAIN_DEBUG_ROW_CNT),
			YUVP_GET_F(pmio, YUVP_R_CHAIN_DEBUG_CNT_VAL, YUVP_F_CHAIN_DEBUG_COL_CNT));
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

void yuvp_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		info_hw("[YUVP]%s:DUMP CR\n", __FILENAME__);
		is_hw_dump_regs(pmio_get_base(pmio), yuvp_regs, YUVP_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		info_hw("[YUVP]%s:DUMP DBG_STATE\n", __FILENAME__);
		yuvp_hw_dump_dbg_state(pmio);
		break;
	default:
		err_hw("[YUVP]%s:Not supported dump_mode %u", __FILENAME__, mode);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_dump);

void yuvp_hw_s_coutfifo(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO_INTERVALS,
		YUVP_F_YUV_COUTFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO_INTERVAL_VBLANK,
		YUVP_F_YUV_COUTFIFO_INTERVAL_VBLANK, VBLANK_CYCLE);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO_ENABLE, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO_CONFIG,
		YUVP_F_YUV_COUTFIFO_DEBUG_EN, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_COUTFIFO_CONFIG,
		YUVP_F_YUV_COUTFIFO_VVALID_RISE_AT_FIRST_DATA_EN, 1);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_coutfifo);

void yuvp_hw_s_cinfifo(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO_INTERVALS,
		YUVP_F_YUV_CINFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO_ENABLE, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO_CONFIG,
		YUVP_F_YUV_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO_CONFIG,
		YUVP_F_YUV_CINFIFO_DEBUG_EN, 1);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_cinfifo);

void yuvp_hw_s_strgen(void *base, u32 set_id)
{
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO_CONFIG,
		YUVP_F_YUV_CINFIFO_STRGEN_MODE_EN, 1);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO_CONFIG,
		YUVP_F_YUV_CINFIFO_STRGEN_MODE_DATA_TYPE, 0);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_CINFIFO_CONFIG,
		YUVP_F_YUV_CINFIFO_STRGEN_MODE_DATA, 128);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_strgen);

static void _yuvp_hw_s_common(void *base)
{
	dbg_hw(4, "[API] %s\n", __func__);

	YUVP_SET_R(base, YUVP_R_STAT_RDMACL_EN, 1);
}

static void _yuvp_hw_s_secure_id(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	/*
	 * Set Paramer Value
	 *
	 * scenario
	 * 0: Non-secure,  1: Secure
	 * TODO: get secure scenario
	 */
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_SECU_CTRL_SEQID, 0);
}

static void _yuvp_hw_s_fro(void *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);
}

void yuvp_hw_s_core(void *base, u32 num_buffers, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	_yuvp_hw_s_common(base);
	_yuvp_hw_s_secure_id(base, set_id);
	_yuvp_hw_s_fro(base, num_buffers, set_id);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_core);

void yuvp_hw_dma_dump(struct is_common_dma *dma)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_print_info, 0);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_dma_dump);

static void __yuvp_hw_s_rdma_in_format(void *base, struct param_dma_input *dma_input)
{
	u32 hwformat = dma_input->format;
	u32 pixel_size = dma_input->msb + 1;
	u32 order = dma_input->order;
	u32 rdma_format = 0;

	switch (hwformat) {
	case DMA_INOUT_FORMAT_YUV444:
		rdma_format = 0; /* 3p */
		break;
	case DMA_INOUT_FORMAT_YUV422:
		if (pixel_size == 10) {
			/* 10bit */
			rdma_format = 1;
		} else {
			if (order == DMA_INOUT_ORDER_CbCr) /* U first 2p */
				rdma_format = 2;
			else if (order == DMA_INOUT_ORDER_CrCb) /* V first 2p */
				rdma_format = 3;
			else
				err_hw("[YUVP] invalid order(%d)", order);
		}
		break;
	case DMA_INOUT_FORMAT_YUV420:
		if (order == DMA_INOUT_ORDER_CbCr) /* U first 2p */
			rdma_format = 4;
		else if (order == DMA_INOUT_ORDER_CrCb) /* V first 2p */
			rdma_format = 5;
		else
			err_hw("[YUVP] invalid order(%d)", order);
		break;
	case DMA_INOUT_FORMAT_Y:
		/* mono y only 1p */
		rdma_format = 6;
		break;
	default:
		err_hw("[YUVP] invalid hwformat(%d)", hwformat);
		break;
	}

	dbg_hw(4, "[YUVP] %s: rdma_format(%d)", __func__, rdma_format);

	YUVP_SET_R(base, YUVP_R_RDMA_IN_FORMAT, rdma_format);
}

static void __yuvp_hw_s_rdma_size(void *base, struct param_dma_input *dma_input)
{
	u32 width;
	u32 height;

	width = dma_input->width;
	height = dma_input->height;

	dbg_hw(4, "[YUVP] %s: rdma_size(%dx%d)", __func__, width, height);

	YUVP_SET_F(base, YUVP_R_MCSC_RDMA_SIZE, YUVP_F_MCSC_RDMA_SIZE_X, width);
	YUVP_SET_F(base, YUVP_R_MCSC_RDMA_SIZE, YUVP_F_MCSC_RDMA_SIZE_Y, height);
}

int yuvp_hw_s_rdma_init(struct is_common_dma *dma, struct yuvp_param_set *param_set, u32 enable,
	u32 vhist_grid_num, u32 drc_grid_w, u32 drc_grid_h, u32 in_crop_size_x, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size, u32 *strip_offset, u32 *header_offset)
{
	int ret = SET_SUCCESS;
	struct param_dma_input *dma_input = NULL;
	u32 comp_sbwc_en, comp_64b_align, quality_control = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_dma_width;
	u32 format, en_votf, bus_info;
	u32 strip_enable;
	u32 strip_offset_in_pixel = 0, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_index = param_set->stripe_input.index;
	u32 strip_start_pos = (strip_index) ? (param_set->stripe_input.start_pos_x) : 0;
	bool img_flag = true;

	dbg_hw(4, "[API] %s: enable(%d), strip idx(%d), pos(%d)\n", __func__, enable, strip_index,
		strip_start_pos);

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	switch (dma->id) {
	case YUVP_RDMA_Y:
	case YUVP_RDMA_U:
	case YUVP_RDMA_V:
		dma_input = &param_set->dma_input;
		width = dma_input->width;
		height = dma_input->height;
		full_dma_width = width;
		break;
	case YUVP_RDMA_SEG:
		dma_input = &param_set->dma_input_seg;
		strip_offset_in_pixel = strip_start_pos;
		width = dma_input->width;
		height = dma_input->height;
		img_flag = false;
		full_dma_width = (strip_enable) ? param_set->stripe_input.full_width : width;
		break;
	case YUVP_RDMA_DRC0:
	case YUVP_RDMA_DRC1:
		dma_input = &param_set->dma_input_drc;
		width = ((drc_grid_w + 3) / 4) * 4;
		height = drc_grid_h;
		img_flag = false;
		full_dma_width = DIV_ROUND_UP(width * dma_input->bitwidth, BITS_PER_BYTE);
		break;
	case YUVP_RDMA_CLAHE:
		dma_input = &param_set->dma_input_clahe;
		width = 1024;
		height = vhist_grid_num;
		img_flag = false;
		full_dma_width = width;
		break;
	case YUVP_RDMA_PCC_HIST:
		dma_input = &param_set->dma_input_pcchist;
		width = YUVP_PCC_HIST_WIDTH;
		height = YUVP_PCC_HIST_HEIGHT;
		img_flag = false;
		full_dma_width = width;
		break;
	default:
		err_hw("[YUVP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	en_votf = dma_input->v_otf_enable;
	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;
	bus_info = en_votf ? (cache_hint << 4) : 0x00000000; /* cache hint [6:4] */

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 0) {
		stride_1p = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, full_dma_width, 16, img_flag);

		if (strip_enable && strip_offset_in_pixel)
			strip_offset_in_byte = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
				hwformat, strip_offset_in_pixel, 16, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_dma_width,
			comp_64b_align, quality_control, YUVP_COMP_BLOCK_WIDTH,
			YUVP_COMP_BLOCK_HEIGHT);
		header_stride_1p =
			is_hw_dma_get_header_stride(full_dma_width, YUVP_COMP_BLOCK_WIDTH, 16);

		if (strip_enable && strip_offset_in_pixel) {
			strip_offset_in_byte = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
				strip_offset_in_pixel, comp_64b_align, quality_control,
				YUVP_COMP_BLOCK_WIDTH, YUVP_COMP_BLOCK_HEIGHT);
			strip_header_offset_in_byte = is_hw_dma_get_header_stride(
				strip_offset_in_pixel, YUVP_COMP_BLOCK_WIDTH, 0);
		}
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + YUVP_COMP_BLOCK_HEIGHT - 1) / YUVP_COMP_BLOCK_HEIGHT) *
				stride_1p;
		break;
	default:
		break;
	}

	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	if (dma->id == YUVP_RDMA_Y) {
		__yuvp_hw_s_rdma_in_format(dma->base, dma_input);
		__yuvp_hw_s_rdma_size(dma->base, dma_input);
	}

	dbg_hw(3, "%s : [%s] sbwc(%d) strip_ofs %d, fmt %d, width %d, height %d\n", __func__,
		dma->name, comp_sbwc_en, strip_offset_in_pixel, format, width, height);
	dbg_hw(3, "%s : [%s] img_ofs %d, header_ofs %d, stride (%d, %d)\n", __func__, dma->name,
		*strip_offset, *header_offset, stride_1p, header_stride_1p);

	return ret;
}

int yuvp_hw_rdma_create(struct is_common_dma *dma, void *base, u32 input_id)
{
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[YUVP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (input_id) {
	case YUVP_RDMA_Y:
		dma->reg_ofs = YUVP_R_YUV_RDMAY_EN;
		dma->field_ofs = YUVP_F_YUV_RDMAY_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_Y");
		break;
	case YUVP_RDMA_U:
		dma->reg_ofs = YUVP_R_YUV_RDMAU_EN;
		dma->field_ofs = YUVP_F_YUV_RDMAU_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_U");
		break;
	case YUVP_RDMA_V:
		dma->reg_ofs = YUVP_R_YUV_RDMAV_EN;
		dma->field_ofs = YUVP_F_YUV_RDMAV_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_V");
		break;
	case YUVP_RDMA_SEG:
		dma->reg_ofs = YUVP_R_STAT_RDMASEG_EN;
		dma->field_ofs = YUVP_F_STAT_RDMASEG_EN;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_SEG0");
		break;
	case YUVP_RDMA_DRC0:
		dma->reg_ofs = YUVP_R_STAT_RDMADRC_EN;
		dma->field_ofs = YUVP_F_STAT_RDMADRC_EN;
		available_bayer_format_map = 0x1077 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_DRC0");
		break;
	case YUVP_RDMA_DRC1:
		dma->reg_ofs = YUVP_R_STAT_RDMADRC1_EN;
		dma->field_ofs = YUVP_F_STAT_RDMADRC1_EN;
		available_bayer_format_map = 0x1077 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_DRC1");
		break;
	case YUVP_RDMA_CLAHE:
		dma->reg_ofs = YUVP_R_STAT_RDMACLAHE_EN;
		dma->field_ofs = YUVP_F_STAT_RDMACLAHE_EN;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_CLAHE");
		break;
	case YUVP_RDMA_PCC_HIST:
		dma->reg_ofs = YUVP_R_STAT_RDMAPCCHIST_EN;
		dma->field_ofs = YUVP_F_STAT_RDMAPCCHIST_EN;
		available_bayer_format_map = 0x77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_RDMA_PCCHIST");
		break;
	default:
		err_hw("[YUVP] invalid input_id[%d]", input_id);
		ret = SET_ERROR;
		goto func_exit;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

func_exit:
	__putname(name);

	return ret;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_rdma_create);

int yuvp_hw_get_input_dva(
	u32 id, u32 *cmd, pdma_addr_t **input_dva, struct yuvp_param_set *param_set, u32 grid_en)
{
	switch (id) {
	case YUVP_RDMA_Y:
	case YUVP_RDMA_U:
		*input_dva = param_set->input_dva;
		*cmd = param_set->dma_input.cmd;
		break;
	case YUVP_RDMA_V:
		if (param_set->dma_input.plane == DMA_INOUT_PLANE_3) {
			*input_dva = param_set->input_dva;
			*cmd = param_set->dma_input.cmd;
		} else
			*cmd = 0;
		break;
	case YUVP_RDMA_SEG:
		*input_dva = param_set->input_dva_seg;
		*cmd = param_set->dma_input_seg.cmd;
		break;
	case YUVP_RDMA_DRC0:
		*input_dva = param_set->input_dva_drc;
		*cmd = param_set->dma_input_drc.cmd;
		break;
	case YUVP_RDMA_DRC1:
		*input_dva = param_set->input_dva_drc;
		*cmd = (grid_en == 1) ? DMA_INPUT_COMMAND_DISABLE : param_set->dma_input_drc.cmd;
		break;
	case YUVP_RDMA_CLAHE:
		*input_dva = param_set->input_dva_clahe;
		*cmd = param_set->dma_input_clahe.cmd;
		break;
	case YUVP_RDMA_PCC_HIST:
		*input_dva = param_set->input_dva_pcchist;
		*cmd = param_set->dma_input_pcchist.cmd;
		break;
	default:
		return -1;
	}

	return 0;
}

int yuvp_hw_get_rdma_cache_hint(u32 id, u32 *cache_hint)
{
	switch (id) {
	case YUVP_RDMA_Y:
	case YUVP_RDMA_U:
	case YUVP_RDMA_V:
	case YUVP_RDMA_SEG:
	case YUVP_RDMA_CLAHE:
	case YUVP_RDMA_DRC0:
	case YUVP_RDMA_DRC1:
	case YUVP_RDMA_PCC_HIST:
		*cache_hint = 0x7; /* 111: last-access-read */
		return 0;
	default:
		return -1;
	}
}

void yuvp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_rdma_corex_id);

int yuvp_hw_s_rdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset)
{
	int ret, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	ret = SET_SUCCESS;

	dbg_hw(4, "[API] %s (%d 0x%8llx 0x%8llx), strip_offset(%d, %d), size(%d)\n", __func__,
		dma->id, (unsigned long long)addr[0], (unsigned long long)addr[1], strip_offset,
		header_offset, payload_size);

	switch (dma->id) {
	case YUVP_RDMA_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i];
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVP_RDMA_U:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 1];
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVP_RDMA_V:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 2];
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case YUVP_RDMA_SEG:
	case YUVP_RDMA_DRC0:
	case YUVP_RDMA_DRC1:
	case YUVP_RDMA_CLAHE:
	case YUVP_RDMA_PCC_HIST:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[YUVP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case YUVP_RDMA_Y:
		case YUVP_RDMA_U:
		case YUVP_RDMA_V:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = address[i] + payload_size;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, plane, buf_idx, num_buffers);
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_rdma_addr);

/* WDMA */
int yuvp_hw_s_wdma_init(struct is_common_dma *dma, struct yuvp_param_set *param_set, u32 enable,
	u32 vhist_grid_num, u32 drc_grid_w, u32 drc_grid_h, u32 in_crop_size_x, u32 cache_hint,
	u32 *sbwc_en, u32 *payload_size)
{
	int ret = SET_SUCCESS;
	struct param_dma_output *dma_output = NULL;
	u32 comp_sbwc_en, comp_64b_align, quality_control = 0;
	u32 stride_1p, header_stride_1p;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 format, en_votf, bus_info;
	u32 strip_enable;
	bool img_flag = true;

	dbg_hw(4, "[API] %s\n", __func__);

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;

	ret = CALL_DMA_OPS(dma, dma_enable, enable);
	if (enable == 0)
		return 0;

	switch (dma->id) {
	case YUVP_WDMA_PCC_HIST:
		dma_output = &param_set->dma_output_pcchist;
		width = YUVP_PCC_HIST_WIDTH;
		height = YUVP_PCC_HIST_HEIGHT;
		img_flag = false;
		break;
	default:
		err_hw("[YUVP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	en_votf = dma_output->v_otf_enable;
	hwformat = dma_output->format;
	sbwc_type = dma_output->sbwc_type;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;
	bus_info = en_votf ? (cache_hint << 4) : 0x00000000; /* cache hint [6:4] */

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 0)
		stride_1p = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, width, 16, img_flag);
	else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p =
			is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width, comp_64b_align,
				quality_control, YUVP_COMP_BLOCK_WIDTH, YUVP_COMP_BLOCK_HEIGHT);
		header_stride_1p = is_hw_dma_get_header_stride(width, YUVP_COMP_BLOCK_WIDTH, 16);
	} else {
		return SET_ERROR;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + YUVP_COMP_BLOCK_HEIGHT - 1) / YUVP_COMP_BLOCK_HEIGHT) *
				stride_1p;
		break;
	default:
		break;
	}

	return ret;
}

int yuvp_hw_wdma_create(struct is_common_dma *dma, void *base, u32 input_id)
{
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[YUVP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (input_id) {
	case YUVP_WDMA_PCC_HIST:
		dma->reg_ofs = YUVP_R_STAT_WDMAPCCHIST_EN;
		dma->field_ofs = YUVP_F_STAT_WDMAPCCHIST_EN;
		available_bayer_format_map = 0X77 & IS_BAYER_FORMAT_MASK;
		snprintf(name, PATH_MAX, "YUVP_WDMA_PCCHIST");
		break;
	default:
		err_hw("[YUVP] invalid input_id[%d]", input_id);
		ret = SET_ERROR;
		goto func_exit;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

func_exit:
	__putname(name);

	return ret;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_wdma_create);

int yuvp_hw_get_output_dva(
	u32 id, u32 *cmd, pdma_addr_t **output_dva, struct yuvp_param_set *param_set)
{
	switch (id) {
	case YUVP_WDMA_PCC_HIST:
		*output_dva = param_set->output_dva_pcchist;
		*cmd = param_set->dma_output_pcchist.cmd;
		break;
	default:
		return -1;
	}
	return 0;
}

int yuvp_hw_get_wdma_cache_hint(u32 id, u32 *cache_hint)
{
	switch (id) {
	case YUVP_WDMA_PCC_HIST:
		*cache_hint = 0x7; /* 111: last-access-read */
		return 0;
	default:
		return -1;
	}
}

void yuvp_hw_s_wdma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_wdma_corex_id);

int yuvp_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size)
{
	int ret, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	ret = SET_SUCCESS;

	dbg_hw(4, "[API] %s (%d 0x%08llx 0x%08llx)\n", __func__, dma->id,
		(unsigned long long)addr[0], (unsigned long long)addr[1]);

	switch (dma->id) {
	case YUVP_WDMA_PCC_HIST:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i];
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_hw("[YUVP] invalid dma_id[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case YUVP_WDMA_PCC_HIST:
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, plane, buf_idx, num_buffers);
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_wdma_addr);

void yuvp_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = INT0_YUVP_EN_MASK;
	int_en[PCC_INT_1] = INT1_YUVP_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_g_int_en);

u32 yuvp_hw_g_int_grp_en(void)
{
	return YUVP_INT_GRP_EN_MASK;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_g_int_grp_en);

void yuvp_hw_s_block_bypass(void *base, u32 set_id)
{
	dbg_hw(4, "[API] %s\n", __func__);

#if (IS_ENABLED(CONFIG_YUVP_BYPASS_DEBUG)) /* only enable for debug */
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DTP_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_STAT_SEGMAPPING_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUV420TO422_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUV422TO444_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUVTORGB_BYPASS, 1);
#endif

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DEGAMMARGB_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DGF_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCDIST_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_DRCTMC_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CCM_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_GAMMARGB_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_PRC_BYPASS, 1);

#if (IS_ENABLED(CONFIG_YUVP_BYPASS_DEBUG))
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_PREFERCOLORCORRECT_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_PCCHIST_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_RGBTOYUV_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_INVGAMMARGBSHARPEN_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_GAMMARGBSHARPEN_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_RGBTOYUVSHARPEN_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_GAMMAOETF_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_YUV444TO422_BYPASS, 1);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DITHER_BYPASS, 1);
#endif
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_block_bypass);

void yuvp_hw_s_clahe_bypass(void *base, u32 set_id, u32 enable)
{
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_RGB_CLAHE_BYPASS, enable);
}

void yuvp_hw_s_svhist_bypass(void *base, u32 set_id, u32 enable)
{
	/* Not support */
}

int yuvp_hw_s_cnr_size(void *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg)
{
	return 0;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_cnr_size);

int yuvp_hw_s_sharpen_size(void *base, u32 set_id, u32 yuvpp_strip_start_pos, u32 frame_width,
	u32 dma_width, u32 dma_height, u32 strip_enable, struct yuvp_radial_cfg *radial_cfg)
{
	u32 val;
	u32 offset_x, offset_y, start_crop_x, start_crop_y;
	int binning_x, binning_y;

	offset_x = radial_cfg->sensor_crop_x +
		   (radial_cfg->bns_binning_x * (radial_cfg->rgbp_crop_offset_x + yuvpp_strip_start_pos) /
			   1000);
	offset_y = radial_cfg->sensor_crop_y +
		   (radial_cfg->bns_binning_y * radial_cfg->rgbp_crop_offset_y / 1000);

	start_crop_x = radial_cfg->sensor_binning_x * offset_x / 1000;
	start_crop_y = radial_cfg->sensor_binning_y * offset_y / 1000;

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x *
		    radial_cfg->sw_binning_x * 1024ULL * radial_cfg->rgbp_crop_w / frame_width /
		    1000 / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y *
		    radial_cfg->sw_binning_y * 1024ULL * radial_cfg->rgbp_crop_h / dma_height /
		    1000 / 1000 / 1000;

	dbg_hw(4, "[API]%s: sensor (%dx%d) img(%dx%d)\n", __func__, radial_cfg->sensor_full_width,
		radial_cfg->sensor_full_height, dma_width, dma_height);
	dbg_hw(4, "[API]%s: start_crop_x %d, start_crop_y %d\n", __func__, start_crop_x,
		start_crop_y);
	dbg_hw(4, "[API]%s: binning_x %d, binning_y %d\n", __func__, binning_x, binning_y);

	val = 0;
	val = YUVP_SET_V(base, val, YUVP_F_YUV_SHARPENHANCER_CROP_X, start_crop_x);
	val = YUVP_SET_V(base, val, YUVP_F_YUV_SHARPENHANCER_CROP_Y, start_crop_y);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_CROP, val);

	val = 0;
	val = YUVP_SET_V(base, val, YUVP_F_YUV_SHARPENHANCER_BINNING_X, binning_x);
	val = YUVP_SET_V(base, val, YUVP_F_YUV_SHARPENHANCER_BINNING_Y, binning_y);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_SHARPENHANCER_BINNING, val);

	return 0;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_sharpen_size);

int yuvp_hw_s_strip(void *base, u32 set_id, u32 strip_enable, u32 strip_start_pos,
	enum yuvp_strip_type type, u32 left, u32 right, u32 full_width)
{
	dbg_hw(4, "[API] %s\n", __func__);

	if (!strip_enable)
		strip_start_pos = 0x0;

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_STRIP_TYPE, type);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_STRIP_START_POSITION,
		YUVP_F_CHAIN_STRIP_START_POSITION, strip_start_pos);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_STRIP_START_POSITION,
		YUVP_F_CHAIN_FULL_FRAME_WIDTH, full_width);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_STRIP_OVERLAP_SIZE,
		YUVP_F_CHAIN_STRIP_OVERLAP_LEFT, left);
	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_STRIP_OVERLAP_SIZE,
		YUVP_F_CHAIN_STRIP_OVERLAP_RIGHT, right);

	return 0;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_strip);

void yuvp_hw_s_size(void *base, u32 set_id, u32 dma_width, u32 dma_height, bool strip_enable)
{
	u32 val;

	dbg_hw(4, "[API] %s (%d x %d)\n", __func__, dma_width, dma_height);

	val = 0;
	val = YUVP_SET_V(base, val, YUVP_F_CHAIN_IMG_WIDTH, dma_width);
	val = YUVP_SET_V(base, val, YUVP_F_CHAIN_IMG_HEIGHT, dma_height);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_IMG_SIZE, val);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_size);

void yuvp_hw_g_size(void *base, u32 set_id, u32 *width, u32 *height)
{
	*width = YUVP_GET_F(
		base, GET_COREX_OFFSET(set_id) + YUVP_R_CHAIN_IMG_SIZE, YUVP_F_CHAIN_IMG_WIDTH);
	*height = YUVP_GET_F(
		base, GET_COREX_OFFSET(set_id) + YUVP_R_CHAIN_IMG_SIZE, YUVP_F_CHAIN_IMG_HEIGHT);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_g_size);

void yuvp_hw_s_input_path(void *base, u32 set_id, enum yuvp_input_path_type path,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, path);

	if (path == YUVP_INPUT_OTF)
		frame_cfg->cotf_in_en |= BIT_MASK(YUVP_COTF_IN_YUV);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_input_path);

void yuvp_hw_s_output_path(
	void *base, u32 set_id, int path, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, path);

	if (path)
		frame_cfg->cotf_out_en |= BIT_MASK(YUVP_COTF_OUT_YUV);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_output_path);

enum yuvp_chain_demux_type {
	YUVP_DEMUX_RDMA_YUV444_TO_DTP = (1 << 0),
	YUVP_DEMUX_RDMA_YUV422_TO_COUT = (1 << 1),
	YUVP_DEMUX_SEGCONF_DRCTMC = (1 << 8),
	YUVP_DEMUX_SEGCONF_CCM = (1 << 9),
	YUVP_DEMUX_SEGCONF_PCC = (1 << 10),
	YUVP_DEMUX_SEGCONF_SHARPEN = (1 << 11),
};

void yuvp_hw_s_demux_enable(
	void *base, u32 set_id, struct yuvp_param_set *param_set, struct is_yuvp_config config)
{
	u32 demux_sel = 0;
	struct param_dma_input *dma_input = NULL;
	u32 demux_need = 0;

	if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		dma_input = &param_set->dma_input;

		if (dma_input->format == DMA_INOUT_FORMAT_YUV444)
			demux_sel |= YUVP_DEMUX_RDMA_YUV444_TO_DTP;
		else
			demux_sel |= YUVP_DEMUX_RDMA_YUV422_TO_COUT;
	}

	if (!IS_ENABLED(CONFIG_USE_YUVP_SEGCONF_RDMA))
		demux_need = 1;

	if (param_set->dma_input_seg.cmd == DMA_INPUT_COMMAND_ENABLE)
		demux_need = 1;

	if (demux_need) {
		if (config.drc_contents_aware_isp_en)
			demux_sel |= YUVP_DEMUX_SEGCONF_DRCTMC;
		if (config.ccm_contents_aware_isp_en)
			demux_sel |= YUVP_DEMUX_SEGCONF_CCM;
		if (config.pcc_contents_aware_isp_en)
			demux_sel |= YUVP_DEMUX_SEGCONF_PCC;
		if (config.sharpen_contents_aware_isp_en)
			demux_sel |= YUVP_DEMUX_SEGCONF_SHARPEN;
	}

	dbg_hw(4, "[API] %s demux_sel(%d)\n", __func__, demux_sel);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_DEMUX_ENABLE, demux_sel);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_demux_enable);

void yuvp_hw_s_mono_mode(void *base, u32 set_id, bool enable)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, enable);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_mono_mode);

void yuvp_hw_s_mux_dtp(void *base, u32 set_id, enum yuvp_mux_dtp_type type)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, type);

	if (type > YUVP_MUX_DTP_RDMA_YUV) {
		err_hw("[YUVP] invalid dtp type(%d)", type);
		return;
	}

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_MUX_SELECT, YUVP_F_MUX_DTP_SELECT,
		type);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_mux_dtp);

void yuvp_hw_s_mux_cout_sel(void *base, u32 set_id, struct yuvp_param_set *param_set)
{
	struct param_dma_input *dma_input = &param_set->dma_input;
	u32 sel = 0;

	/* TODO: how to know YUVP and MCSC */
	if (dma_input->cmd == DMA_INPUT_COMMAND_ENABLE) {
		if (dma_input->format == DMA_INOUT_FORMAT_YUV444)
			sel = 0;
		else
			sel = 1;
	}

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_MUX_SELECT,
		YUVP_F_MUX_COUTFIFO_SELECT, sel);

	dbg_hw(4, "[API] %s (%d)\n", __func__, sel);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_mux_cout_sel);

void yuvp_hw_s_mux_segconf_sel(void *base, u32 set_id, u32 sel)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, sel);

	YUVP_SET_F(base + GET_COREX_OFFSET(set_id), YUVP_R_CHAIN_MUX_SELECT,
		YUVP_F_MUX_SEGCONF_SELECT, sel);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_mux_segconf_sel);

void yuvp_hw_s_dtp_pattern(void *base, u32 set_id, enum yuvp_dtp_pattern pattern)
{
	dbg_hw(4, "[API] %s (%d)\n", __func__, pattern);

	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DTP_BYPASS, 0);
	YUVP_SET_R(base + GET_COREX_OFFSET(set_id), YUVP_R_YUV_DTP_TEST_PATTERN_MODE, pattern);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_dtp_pattern);

u32 yuvp_hw_g_rdma_max_cnt(void)
{
	return YUVP_RDMA_MAX;
}

u32 yuvp_hw_g_wdma_max_cnt(void)
{
	return YUVP_WDMA_MAX;
}

u32 yuvp_hw_g_reg_cnt(void)
{
	return YUVP_REG_CNT + YUVP_LUT_REG_CNT * YUVP_LUT_NUM;
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_g_reg_cnt);

void yuvp_hw_s_crc(void *base, u32 seed)
{
	YUVP_SET_F(base, YUVP_R_YUV_CINFIFO_STREAM_CRC, YUVP_F_YUV_CINFIFO_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_DTP_STREAM_CRC, YUVP_F_YUV_DTP_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_YUV422TO444_STREAM_CRC, YUVP_F_YUV_YUV422TO444_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_YUVTORGB_STREAM_CRC, YUVP_F_YUV_YUVTORGB_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_CLAHE_STREAM_CRC, YUVP_F_RGB_CLAHE_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_INVCCM33_STREAM_CRC, YUVP_F_RGB_INVCCM33_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DEGAMMARGB_STREAM_CRC, YUVP_F_RGB_DEGAMMARGB_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_GAMMARGB_STREAM_CRC, YUVP_F_RGB_GAMMARGB_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_CCM_STREAM_CRC, YUVP_F_RGB_CCM_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_PRC_STREAM_CRC, YUVP_F_RGB_PRC_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETA_LUT_CRC, YUVP_F_RGB_DRCTMC_SETA_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETB_LUT_CRC, YUVP_F_RGB_DRCTMC_SETB_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETA_FACE_LUT_CRC,
		YUVP_F_RGB_DRCTMC_SETA_FACE_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_SETB_FACE_LUT_CRC,
		YUVP_F_RGB_DRCTMC_SETB_FACE_LUT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCTMC_STREAM_CRC, YUVP_F_RGB_DRCTMC_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DGF_STREAM_CRC, YUVP_F_RGB_DGF_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_DRCDIST_STREAM_CRC, YUVP_F_RGB_DRCDIST_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_PREFERCOLORCORRECT_STREAM_CRC,
		YUVP_F_RGB_PREFERCOLORCORRECT_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_PCCHIST_STREAM_CRC, YUVP_F_RGB_PCCHIST_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_RGBTOYUV_STREAM_CRC, YUVP_F_RGB_RGBTOYUV_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_INVGAMMARGBSHARPEN_STREAM_CRC,
		YUVP_F_RGB_INVGAMMARGBSHARPEN_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_INVCCM33SHARPEN_STREAM_CRC,
		YUVP_F_RGB_INVCCM33SHARPEN_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_GAMMARGBSHARPEN_STREAM_CRC,
		YUVP_F_RGB_GAMMARGBSHARPEN_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_RGB_RGBTOYUVSHARPEN_STREAM_CRC,
		YUVP_F_RGB_RGBTOYUVSHARPEN_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_SHARPENHANCER_STREAM_CRC,
		YUVP_F_YUV_SHARPENHANCER_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_GAMMAOETF_STREAM_CRC, YUVP_F_YUV_GAMMAOETF_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_YUV444TO422_STREAM_CRC, YUVP_F_YUV_YUV444TO422_CRC_SEED, seed);
	YUVP_SET_F(base, YUVP_R_YUV_DITHER_STREAM_CRC, YUVP_F_YUV_DITHER_CRC_SEED, seed);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_s_crc);

void yuvp_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &yuvp_volatile_ranges_table;
	cfg->wr_noinc_table = &yuvp_wr_noinc_ranges_table;

	cfg->max_register = YUVP_R_YUV_DITHER_STREAM_CRC;
	cfg->num_reg_defaults_raw = (YUVP_R_YUV_DITHER_STREAM_CRC >> 2) + 1;
	cfg->dma_addr_shift = 4;

	cfg->ranges = yuvp_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(yuvp_range_cfgs);

	cfg->fields = yuvp_field_descs;
	cfg->num_fields = ARRAY_SIZE(yuvp_field_descs);
}
KUNIT_EXPORT_SYMBOL(yuvp_hw_init_pmio_config);
