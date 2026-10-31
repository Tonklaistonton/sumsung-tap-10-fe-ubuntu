// SPDX-License-Identifier: GPL-2.0
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

#include <linux/delay.h>
#include "is-hw-api-msnr-v13.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-msnr-v13_0.h"
#include "pmio.h"
#include "pablo-hw-api-common-ctrl.h"

#define MSNR_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define MSNR_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define MSNR_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define MSNR_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define MSNR_GET_R(base, R) PMIO_GET_R(base, R)

#define VBLANK_CYCLE 0xA
#define HBLANK_CYCLE 0x2E
#define PBLANK_CYCLE 0

#define info_msnr(fmt, args...) info_common("[HW][MSNR]", fmt, ##args)
#define info_msnr_ver(fmt, args...) info_common("[HW][MSNR](v13.0)", fmt, ##args)
#define err_msnr(fmt, args...) err_common("[HW][MSNR][ERR]%s:%d:", fmt "\n", __func__, __LINE__, ##args)
#define dbg_msnr(level, fmt, args...)                                                              \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_HW)) >= (level), "[HW][MSNR]", fmt, ##args)

/* CMDQ Interrupt group mask */
#define MSNR_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
#define MSNR_INT_GRP_EN_MASK_FRO_FIRST BIT_MASK(PCC_INT_GRP_FRAME_START)
#define MSNR_INT_GRP_EN_MASK_FRO_MIDDLE 0
#define MSNR_INT_GRP_EN_MASK_FRO_LAST BIT_MASK(PCC_INT_GRP_FRAME_END)

enum mtnr0_cotf_in_id {
	MSNR_COTF_IN_YUV_L0,
	MSNR_COTF_IN_YUV_L1,
	MSNR_COTF_IN_YUV_L2,
	MSNR_COTF_IN_YUV_L3,
	MSNR_COTF_IN_YUV_L4,
};

enum mtnr0_cotf_out_id {
	MSNR_COTF_OUT_YUV,
	MSNR_COTF_OUT_STAT,
};

static const char *const is_hw_msnr_wdma_name[] = {
	"MSNR_WDMA_LME",
};

static void __msnr_hw_s_secure_id(struct pablo_mmio *base, u32 set_id)
{
	MSNR_SET_F(base, MSNR_R_SECU_CTRL_SEQID, MSNR_F_SECU_CTRL_SEQID,
		0); /* TODO: get secure scenario */
}

u32 msnr_hw_is_occurred(unsigned int status, enum msnr_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_MSNR_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_MSNR_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_MSNR_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_MSNR_COREX_END_INT_1;
		break;
	case INTR_SETTING_DONE:
		mask = 1 << INTR0_MSNR_SETTING_DONE_INT;
		break;
	case INTR_ERR:
		mask = MSNR_INT0_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

u32 msnr_hw_is_occurred1(unsigned int status, enum msnr_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_ERR:
		mask = MSNR_INT1_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int msnr_hw_wait_idle(struct pablo_mmio *base)
{
	int ret = 0;
	u32 idle;
	u32 int0_all, int1_all;
	u32 try_cnt = 0;

	idle = MSNR_GET_F(base, MSNR_R_IDLENESS_STATUS, MSNR_F_IDLENESS_STATUS);
	int0_all = MSNR_GET_R(base, MSNR_R_INT_REQ_INT0);
	int1_all = MSNR_GET_R(base, MSNR_R_INT_REQ_INT1);

	info_msnr(
		"idle status before disable (idle:%d, int:0x%X, 0x%X)\n", idle, int0_all, int1_all);

	while (!idle) {
		idle = MSNR_GET_F(base, MSNR_R_IDLENESS_STATUS, MSNR_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= MSNR_TRY_COUNT) {
			err_msnr("timeout waiting idle - disable fail");
			msnr_hw_dump(base, HW_DUMP_CR);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = MSNR_GET_R(base, MSNR_R_INT_REQ_INT0);
	int1_all = MSNR_GET_R(base, MSNR_R_INT_REQ_INT1);

	info_msnr(
		"idle status after disable (idle:%d, int:0x%X, 0x%X)\n", idle, int0_all, int1_all);

	return ret;
}

void msnr_hw_s_core(struct pablo_mmio *base, u32 set_id)
{
	MSNR_SET_R(base, MSNR_R_STAT_RDMACL_EN, 1);
	__msnr_hw_s_secure_id(base, set_id);
}

static const struct is_reg msnr_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0200, "CHAIN_L0_IMG_SIZE" }, { 0x0204, "CHAIN_L1_IMG_SIZE" },
	{ 0x0208, "CHAIN_L2_IMG_SIZE" }, { 0x020c, "CHAIN_L3_IMG_SIZE" },
	{ 0x0210, "CHAIN_L4_IMG_SIZE" }, { 0x0220, "CHAIN_MUX_SELECT" },
	{ 0x0224, "CHAIN_DEMUX_ENABLE" },
	/* CINFIFO 0 Status */
	{ 0x0e00, "YUV_CINFIFOL0_ENABLE" }, { 0x0e14, "YUV_CINFIFOL0_STATUS" },
	{ 0x0e18, "YUV_CINFIFOL0_INPUT_CNT" }, { 0x0e1c, "YUV_CINFIFOL0_STALL_CNT" },
	{ 0x0e20, "YUV_CINFIFOL0_FIFO_FULLNESS" }, { 0x0e40, "YUV_CINFIFOL0_INT" },
	/* CINFIFO 1 Status */
	{ 0x0e80, "YUV_CINFIFOL1_ENABLE" }, { 0x0e94, "YUV_CINFIFOL1_STATUS" },
	{ 0x0e98, "YUV_CINFIFOL1_INPUT_CNT" }, { 0x0e9c, "YUV_CINFIFOL1_STALL_CNT" },
	{ 0x0ea0, "YUV_CINFIFOL1_FIFO_FULLNESS" }, { 0x0ec0, "YUV_CINFIFOL1_INT" },
	/* CINFIFO 2 Status */
	{ 0x1000, "YUV_CINFIFOL2_ENABLE" }, { 0x1014, "YUV_CINFIFOL2_STATUS" },
	{ 0x1018, "YUV_CINFIFOL2_INPUT_CNT" }, { 0x101c, "YUV_CINFIFOL2_STALL_CNT" },
	{ 0x1020, "YUV_CINFIFOL2_FIFO_FULLNESS" }, { 0x1040, "YUV_CINFIFOL2_INT" },
	/* CINFIFO 3 Status */
	{ 0x1080, "YUV_CINFIFOL3_ENABLE" }, { 0x1094, "YUV_CINFIFOL3_STATUS" },
	{ 0x1098, "YUV_CINFIFOL3_INPUT_CNT" }, { 0x109c, "YUV_CINFIFOL3_STALL_CNT" },
	{ 0x10a0, "YUV_CINFIFOL3_FIFO_FULLNESS" }, { 0x10c0, "YUV_CINFIFOL3_INT" },
	/* CINFIFO 4 Status */
	{ 0x1200, "YUV_CINFIFOL4_ENABLE" }, { 0x1214, "YUV_CINFIFOL4_STATUS" },
	{ 0x1218, "YUV_CINFIFOL4_INPUT_CNT" }, { 0x121c, "YUV_CINFIFOL4_STALL_CNT" },
	{ 0x1220, "YUV_CINFIFOL4_FIFO_FULLNESS" }, { 0x1240, "YUV_CINFIFOL4_INT" },
	/* COUTFUFO 0 */
	{ 0x0f00, "YUV_COUTFIFO_ENABLE" }, { 0x0f14, "YUV_COUTFIFO_STATUS" },
	{ 0x0f18, "YUV_COUTFIFO_INPUT_CNT" }, { 0x0f1c, "YUV_COUTFIFO_STALL_CNT" },
	{ 0x0f20, "YUV_COUTFIFO_FIFO_FULLNESS" }, { 0x0f40, "YUV_COUTFIFO_INT" },
	/* COUTFUFO 1 */
	{ 0x0f80, "STAT_COUTFIFO_ENABLE" }, { 0x0f94, "STAT_COUTFIFO_STATUS" },
	{ 0x0f98, "STAT_COUTFIFO_INPUT_CNT" }, { 0x0f9c, "STAT_COUTFIFO_STALL_CNT" },
	{ 0x0fa0, "STAT_COUTFIFO_FIFO_FULLNESS" }, { 0x0fc0, "STAT_COUTFIFO_INT" },
	/* ETC */
};

static void msnr_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, MSNR_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(msnr_dbg_cr); i++) {
		cr = &msnr_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

void msnr_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		info_msnr_ver("DUMP CR\n");
		is_hw_dump_regs(pmio_get_base(pmio), msnr_regs, MSNR_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		info_msnr_ver("DUMP DBG_STATE\n");
		msnr_hw_dump_dbg_state(pmio);
		break;
	default:
		err_msnr("%s:Not supported dump_mode %d", __FILENAME__, mode);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(msnr_hw_dump);

void msnr_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0);
}

int msnr_hw_s_wdma_init(struct is_common_dma *dma, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_msnr_config *config)
{
	int ret = 0;
	u32 comp_sbwc_en = 0, comp_64b_align = 1;
	u32 quality_control = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize;
	u32 width, height;
	u32 full_frame_width;
	u32 format;
	u32 strip_offset_in_pixel, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 bus_info = 0;

	ret = CALL_DMA_OPS(dma, dma_enable, dma_output->cmd);
	if (dma_output->cmd == 0) {
		ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, DMA_OUTPUT_SBWC_DISABLE);
		return ret;
	}

	full_frame_width = (strip_enable) ? stripe_input->full_width : frame_width;
	hwformat = dma_output->format;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	switch (dma->id) {
	case MSNR_WDMA_LME:
		strip_offset_in_pixel = dma_output->dma_crop_offset_x;
		width = dma_output->dma_crop_width;
		height = frame_height;
		/* TODO: check this out */
		bus_info = IS_LLC_CACHE_HINT_CACHE_NOALLOC_TYPE << IS_LLC_CACHE_HINT_SHIFT;
		break;
	default:
		err_msnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= -EINVAL;

	stride_1p = is_hw_dma_get_img_stride(
		memory_bitwidth, pixelsize, hwformat, full_frame_width, 16, true);
	if (strip_enable)
		strip_offset_in_byte = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, strip_offset_in_pixel, 16, true);

	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	*payload_size = 0;
	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	dbg_msnr(3, "%s : dma_id %d, width %d, height %d, \
		 strip_offset_in_pixel %d, \
		strip_offset_in_byte %d, strip_header_offset_in_byte %d, payload_size %d\n",
		__func__, dma->id, width, height, strip_offset_in_pixel, strip_offset_in_byte,
		strip_header_offset_in_byte, *payload_size);
	dbg_msnr(3, "comp_sbwc_en %d, pixelsize %d, comp_64b_align %d, quality_control %d, \
		stride_1p %d, header_stride_1p %d\n",
		comp_sbwc_en, pixelsize, comp_64b_align, quality_control, stride_1p,
		header_stride_1p);

	return ret;
}

static int msnr_hw_wdma_create_pmio(struct is_common_dma *dma, void *base, u32 input_id)
{
	int ret = 0;
	ulong available_bayer_format_map;
	const char *name;

	switch (input_id) {
	case MSNR_WDMA_LME:
		dma->reg_ofs = MSNR_R_Y_WDMALME_EN;
		dma->field_ofs = MSNR_F_Y_WDMALME_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_msnr_wdma_name[MSNR_WDMA_LME];
		break;
	default:
		err_msnr("invalid input_id[%d]", input_id);
		return -EINVAL;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

	return ret;
}

int msnr_hw_wdma_create(struct is_common_dma *dma, void *base, u32 dma_id)
{
	return msnr_hw_wdma_create_pmio(dma, base, dma_id);
}

void msnr_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int msnr_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset)
{
	int ret = 0, i;
	dma_addr_t address[IS_MAX_FRO];

	switch (dma->id) {
	case MSNR_WDMA_LME:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	default:
		err_msnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	return ret;
}

void msnr_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = MSNR_INT0_EN_MASK;
	int_en[PCC_INT_1] = MSNR_INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}
KUNIT_EXPORT_SYMBOL(msnr_hw_g_int_en);

u32 msnr_hw_g_int_grp_en(void)
{
	return MSNR_INT_GRP_EN_MASK;
}
KUNIT_EXPORT_SYMBOL(msnr_hw_g_int_grp_en);

void msnr_hw_s_block_bypass(struct pablo_mmio *base, u32 set_id)
{
	dbg_msnr(4, "%s:", __func__);

#if (IS_ENABLED(CONFIG_MSNR_BYPASS_DEBUG)) /* only enable for debug */
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL0_BYPASS, MSNR_F_YUV_MSNRL0_BYPASS, 1);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL1_BYPASS, MSNR_F_YUV_MSNRL1_BYPASS, 1);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL2_BYPASS, MSNR_F_YUV_MSNRL2_BYPASS, 1);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL3_BYPASS, MSNR_F_YUV_MSNRL3_BYPASS, 1);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL4_BYPASS, MSNR_F_YUV_MSNRL4_BYPASS, 1);

	MSNR_SET_F(base, MSNR_R_YUV_YUVBLC_BYPASS, MSNR_F_YUV_YUVBLC_BYPASS, 1);
	MSNR_SET_F(base, MSNR_R_Y_DUALLAYERDECOMP_BYPASS, MSNR_F_Y_DUALLAYERDECOMP_BYPASS, 1);

	MSNR_SET_F(base, MSNR_R_Y_DSLME_BYPASS, MSNR_F_Y_DSLME_BYPASS, 1);
#endif
}

void msnr_hw_s_otf_path(
	struct pablo_mmio *base, u32 enable, struct pablo_common_ctrl_frame_cfg *frame_cfg)

{
	dbg_msnr(4, "%s: %d", __func__, enable);

	if (enable) {
		frame_cfg->cotf_in_en |= BIT_MASK(MSNR_COTF_IN_YUV_L0);
		frame_cfg->cotf_in_en |= BIT_MASK(MSNR_COTF_IN_YUV_L1);
		frame_cfg->cotf_in_en |= BIT_MASK(MSNR_COTF_IN_YUV_L2);
		frame_cfg->cotf_in_en |= BIT_MASK(MSNR_COTF_IN_YUV_L3);
		frame_cfg->cotf_in_en |= BIT_MASK(MSNR_COTF_IN_YUV_L4);

		frame_cfg->cotf_out_en |= BIT_MASK(MSNR_COTF_OUT_YUV);
	}

	/* CIN FIFO */
	MSNR_SET_R(base, MSNR_R_YUV_CINFIFOL0_ENABLE, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_CONFIG,
		MSNR_F_YUV_CINFIFOL0_STALL_BEFORE_FRAME_START_EN, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_CONFIG, MSNR_F_YUV_CINFIFOL0_AUTO_RECOVERY_EN, 0);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_CONFIG, MSNR_F_YUV_CINFIFOL0_DEBUG_EN, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_INTERVALS, MSNR_F_YUV_CINFIFOL0_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_INTERVALS, MSNR_F_YUV_CINFIFOL0_INTERVAL_PIXEL,
		PBLANK_CYCLE);

	MSNR_SET_R(base, MSNR_R_YUV_CINFIFOL1_ENABLE, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_CONFIG,
		MSNR_F_YUV_CINFIFOL1_STALL_BEFORE_FRAME_START_EN, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_CONFIG, MSNR_F_YUV_CINFIFOL1_AUTO_RECOVERY_EN, 0);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_CONFIG, MSNR_F_YUV_CINFIFOL1_DEBUG_EN, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_INTERVALS, MSNR_F_YUV_CINFIFOL1_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_INTERVALS, MSNR_F_YUV_CINFIFOL1_INTERVAL_PIXEL,
		PBLANK_CYCLE);

	MSNR_SET_R(base, MSNR_R_YUV_CINFIFOL2_ENABLE, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_CONFIG,
		MSNR_F_YUV_CINFIFOL2_STALL_BEFORE_FRAME_START_EN, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_CONFIG, MSNR_F_YUV_CINFIFOL2_AUTO_RECOVERY_EN, 0);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_CONFIG, MSNR_F_YUV_CINFIFOL2_DEBUG_EN, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_INTERVALS, MSNR_F_YUV_CINFIFOL2_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_INTERVALS, MSNR_F_YUV_CINFIFOL2_INTERVAL_PIXEL,
		PBLANK_CYCLE);

	MSNR_SET_R(base, MSNR_R_YUV_CINFIFOL3_ENABLE, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_CONFIG,
		MSNR_F_YUV_CINFIFOL3_STALL_BEFORE_FRAME_START_EN, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_CONFIG, MSNR_F_YUV_CINFIFOL3_AUTO_RECOVERY_EN, 0);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_CONFIG, MSNR_F_YUV_CINFIFOL3_DEBUG_EN, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_INTERVALS, MSNR_F_YUV_CINFIFOL3_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_INTERVALS, MSNR_F_YUV_CINFIFOL3_INTERVAL_PIXEL,
		PBLANK_CYCLE);

	MSNR_SET_R(base, MSNR_R_YUV_CINFIFOL4_ENABLE, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_CONFIG,
		MSNR_F_YUV_CINFIFOL4_STALL_BEFORE_FRAME_START_EN, enable);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_CONFIG, MSNR_F_YUV_CINFIFOL4_AUTO_RECOVERY_EN, 0);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_CONFIG, MSNR_F_YUV_CINFIFOL4_DEBUG_EN, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_INTERVALS, MSNR_F_YUV_CINFIFOL4_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_INTERVALS, MSNR_F_YUV_CINFIFOL4_INTERVAL_PIXEL,
		PBLANK_CYCLE);

	MSNR_SET_F(base, MSNR_R_CHAIN_LBCTRL_HBLANK, MSNR_F_CHAIN_LBCTRL_HBLANK,
		HBLANK_CYCLE);

	/* COUT FIFO */
	MSNR_SET_R(base, MSNR_R_YUV_COUTFIFO_ENABLE, enable);
	MSNR_SET_F(base, MSNR_R_YUV_COUTFIFO_CONFIG,
		MSNR_F_YUV_COUTFIFO_VVALID_RISE_AT_FIRST_DATA_EN, 1);
	MSNR_SET_F(base, MSNR_R_YUV_COUTFIFO_CONFIG, MSNR_F_YUV_COUTFIFO_DEBUG_EN, 1);
	MSNR_SET_F(base, MSNR_R_YUV_COUTFIFO_INTERVAL_VBLANK, MSNR_F_YUV_COUTFIFO_INTERVAL_VBLANK,
		VBLANK_CYCLE);

	MSNR_SET_F(base, MSNR_R_YUV_COUTFIFO_INTERVALS, MSNR_F_YUV_COUTFIFO_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	MSNR_SET_F(base, MSNR_R_YUV_COUTFIFO_INTERVALS, MSNR_F_YUV_COUTFIFO_INTERVAL_PIXEL,
		PBLANK_CYCLE);

	MSNR_SET_F(base, MSNR_R_STAT_COUTFIFO_CONFIG, MSNR_F_STAT_COUTFIFO_BACK_STALL_EN,
		0);
}

void msnr_hw_s_otf_dlnr_path(
	struct pablo_mmio *base, u32 en, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	dbg_msnr(4, "%s: %d", __func__, en);

	if (en)
		frame_cfg->cotf_out_en |= BIT_MASK(MSNR_COTF_OUT_STAT);

	/* COUT FIFO */
	MSNR_SET_R(base, MSNR_R_STAT_COUTFIFO_ENABLE, en);
	MSNR_SET_F(base, MSNR_R_STAT_COUTFIFO_CONFIG,
		MSNR_F_STAT_COUTFIFO_VVALID_RISE_AT_FIRST_DATA_EN, 1);
	MSNR_SET_F(base, MSNR_R_STAT_COUTFIFO_CONFIG, MSNR_F_STAT_COUTFIFO_DEBUG_EN, 1);
	MSNR_SET_F(base, MSNR_R_STAT_COUTFIFO_INTERVAL_VBLANK, MSNR_F_STAT_COUTFIFO_INTERVAL_VBLANK,
		VBLANK_CYCLE);

	MSNR_SET_F(base, MSNR_R_STAT_COUTFIFO_INTERVALS, MSNR_F_STAT_COUTFIFO_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	MSNR_SET_F(base, MSNR_R_STAT_COUTFIFO_INTERVALS, MSNR_F_STAT_COUTFIFO_INTERVAL_PIXEL,
		PBLANK_CYCLE);
}

void msnr_hw_s_chain_img_size_l0(struct pablo_mmio *base, u32 w, u32 h)
{
	dbg_msnr(4, "%s: %dx%d", __func__, w, h);

	MSNR_SET_F(base, MSNR_R_CHAIN_L0_IMG_SIZE, MSNR_F_CHAIN_L0_IMG_WIDTH, w);
	MSNR_SET_F(base, MSNR_R_CHAIN_L0_IMG_SIZE, MSNR_F_CHAIN_L0_IMG_HEIGHT, h);
}

void msnr_hw_s_chain_img_size_l1(struct pablo_mmio *base, u32 w, u32 h)
{
	dbg_msnr(4, "%s: (%dx%d)", __func__, w, h);

	MSNR_SET_F(base, MSNR_R_CHAIN_L1_IMG_SIZE, MSNR_F_CHAIN_L1_IMG_WIDTH, w);
	MSNR_SET_F(base, MSNR_R_CHAIN_L1_IMG_SIZE, MSNR_F_CHAIN_L1_IMG_HEIGHT, h);
}

void msnr_hw_s_chain_img_size_l2(struct pablo_mmio *base, u32 w, u32 h)
{
	dbg_msnr(4, "%s: (%dx%d)", __func__, w, h);

	MSNR_SET_F(base, MSNR_R_CHAIN_L2_IMG_SIZE, MSNR_F_CHAIN_L2_IMG_WIDTH, w);
	MSNR_SET_F(base, MSNR_R_CHAIN_L2_IMG_SIZE, MSNR_F_CHAIN_L2_IMG_HEIGHT, h);
}

void msnr_hw_s_chain_img_size_l3(struct pablo_mmio *base, u32 w, u32 h)
{
	dbg_msnr(4, "%s: (%dx%d)", __func__, w, h);

	MSNR_SET_F(base, MSNR_R_CHAIN_L3_IMG_SIZE, MSNR_F_CHAIN_L3_IMG_WIDTH, w);
	MSNR_SET_F(base, MSNR_R_CHAIN_L3_IMG_SIZE, MSNR_F_CHAIN_L3_IMG_HEIGHT, h);
}

void msnr_hw_s_chain_img_size_l4(struct pablo_mmio *base, u32 w, u32 h)
{
	dbg_msnr(4, "%s: (%dx%d)", __func__, w, h);

	MSNR_SET_F(base, MSNR_R_CHAIN_L4_IMG_SIZE, MSNR_F_CHAIN_L4_IMG_WIDTH, w);
	MSNR_SET_F(base, MSNR_R_CHAIN_L4_IMG_SIZE, MSNR_F_CHAIN_L4_IMG_HEIGHT, h);
}

void msnr_hw_s_dlnr_enable(struct pablo_mmio *base, u32 en)
{
	dbg_msnr(4, "%s: %d", __func__, en);

	MSNR_SET_F(base, MSNR_R_CHAIN_MUX_SELECT, MSNR_F_MSNR_DUALLAYER_MODE, en);
	MSNR_SET_F(base, MSNR_R_Y_DUALLAYERDECOMP_BYPASS, MSNR_F_Y_DUALLAYERDECOMP_BYPASS, !en);
}

void msnr_hw_s_decomp_lowpower(struct pablo_mmio *base, u32 en)
{
	dbg_msnr(4, "%s: en(%d)", __func__, en);

	/* STRGEN setting */
	MSNR_SET_F(
		base, MSNR_R_Y_DUALLAYERDECOMP_CONFIG, MSNR_F_Y_DUALLAYERDECOMP_LOW_POWER_EN, en);
}
KUNIT_EXPORT_SYMBOL(msnr_hw_s_decomp_lowpower);

void msnr_hw_s_dslme_input_enable(struct pablo_mmio *base, u32 en)
{
	dbg_msnr(4, "%s: %d", __func__, en);

	MSNR_SET_F(base, MSNR_R_CHAIN_MUX_SELECT, MSNR_F_MSNR_DSLME_INPUT_ENABLE, en);
}

void msnr_hw_s_dslme_input_select(struct pablo_mmio *base, u32 sel)
{
	dbg_msnr(4, "%s: %d", __func__, sel);

	MSNR_SET_F(base, MSNR_R_CHAIN_MUX_SELECT, MSNR_F_MSNR_DSLME_INPUT_SELECT, sel);
}

void msnr_hw_s_dslme_config(struct pablo_mmio *base, u32 bypass, u32 w, u32 h)
{
	dbg_msnr(4, "%s: bypass(%d)", __func__, bypass);

	MSNR_SET_F(base, MSNR_R_Y_DSLME_BYPASS, MSNR_F_Y_DSLME_BYPASS, bypass);
	MSNR_SET_F(base, MSNR_R_Y_DSLME_OUT, MSNR_F_Y_DSLME_OUT_W, w);
	MSNR_SET_F(base, MSNR_R_Y_DSLME_OUT, MSNR_F_Y_DSLME_OUT_H, h);
}

void msnr_hw_s_segconf_output_enable(struct pablo_mmio *base, u32 en)
{
	dbg_msnr(4, "%s: %d", __func__, en);

	MSNR_SET_F(base, MSNR_R_CHAIN_MUX_SELECT, MSNR_F_MSNR_SEGCONF_OUTPUT_ENABLE, en);
}

void msnr_hw_s_crop_yuv(struct pablo_mmio *base, u32 bypass, struct is_crop crop)
{
	dbg_msnr(4, "%s: bypass: %d, crop(%d %d %d %d)", __func__, bypass,
			crop.x, crop.y, crop.w, crop.h);

	MSNR_SET_R(base, MSNR_R_YUV_CROP_CTRL, bypass);
	MSNR_SET_F(base, MSNR_R_YUV_CROP_START, MSNR_F_YUV_CROP_START_X, crop.x);
	MSNR_SET_F(base, MSNR_R_YUV_CROP_START, MSNR_F_YUV_CROP_START_Y, crop.y);
	MSNR_SET_F(base, MSNR_R_YUV_CROP_SIZE, MSNR_F_YUV_CROP_SIZE_X, crop.w);
	MSNR_SET_F(base, MSNR_R_YUV_CROP_SIZE, MSNR_F_YUV_CROP_SIZE_Y, crop.h);
}

void msnr_hw_s_crop_hf(struct pablo_mmio *base, u32 bypass, struct is_crop crop)
{
	dbg_msnr(4, "%s: bypass: %d, crop(%d %d %d %d)", __func__, bypass,
			crop.x, crop.y, crop.w, crop.h);

	MSNR_SET_R(base, MSNR_R_STAT_CROP_CTRL, bypass);
	MSNR_SET_F(base, MSNR_R_STAT_CROP_START, MSNR_F_STAT_CROP_START_X, crop.x);
	MSNR_SET_F(base, MSNR_R_STAT_CROP_START, MSNR_F_STAT_CROP_START_Y, crop.y);
	MSNR_SET_F(base, MSNR_R_STAT_CROP_SIZE, MSNR_F_STAT_CROP_SIZE_X, crop.w);
	MSNR_SET_F(base, MSNR_R_STAT_CROP_SIZE, MSNR_F_STAT_CROP_SIZE_Y, crop.h);
}

void msnr_hw_s_strip_size(struct pablo_mmio *base, u32 type, u32 offset, u32 full_width)
{
	dbg_msnr(4, "%s: type: %d", __func__, type);

	MSNR_SET_R(base, MSNR_R_CHAIN_STRIP_TYPE, type);
	MSNR_SET_F(
		base, MSNR_R_CHAIN_STRIP_START_POSITION, MSNR_F_CHAIN_STRIP_START_POSITION, offset);
	MSNR_SET_F(
		base, MSNR_R_CHAIN_STRIP_START_POSITION, MSNR_F_CHAIN_FULL_FRAME_WIDTH, full_width);
}

void msnr_hw_s_radial_l0(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

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

	dbg_msnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL0_BINNING_X, MSNR_F_YUV_MSNRL0_RADIAL_BINNING_X, binning_x);
	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL0_BINNING_Y, MSNR_F_YUV_MSNRL0_RADIAL_BINNING_Y, binning_y);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL0_RADIAL_CENTER, MSNR_F_YUV_MSNRL0_RADIAL_CENTER_X,
		radial_center_x);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL0_RADIAL_CENTER, MSNR_F_YUV_MSNRL0_RADIAL_CENTER_Y,
		radial_center_y);
}

void msnr_hw_s_radial_l1(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

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

	dbg_msnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL1_BINNING_X, MSNR_F_YUV_MSNRL1_RADIAL_BINNING_X, binning_x);
	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL1_BINNING_Y, MSNR_F_YUV_MSNRL1_RADIAL_BINNING_Y, binning_y);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL1_RADIAL_CENTER, MSNR_F_YUV_MSNRL1_RADIAL_CENTER_X,
		radial_center_x);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL1_RADIAL_CENTER, MSNR_F_YUV_MSNRL1_RADIAL_CENTER_Y,
		radial_center_y);
}

void msnr_hw_s_radial_l2(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

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

	dbg_msnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL2_BINNING_X, MSNR_F_YUV_MSNRL2_RADIAL_BINNING_X, binning_x);
	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL2_BINNING_Y, MSNR_F_YUV_MSNRL2_RADIAL_BINNING_Y, binning_y);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL2_RADIAL_CENTER, MSNR_F_YUV_MSNRL2_RADIAL_CENTER_X,
		radial_center_x);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL2_RADIAL_CENTER, MSNR_F_YUV_MSNRL2_RADIAL_CENTER_Y,
		radial_center_y);
}

void msnr_hw_s_radial_l3(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

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

	dbg_msnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL3_BINNING_X, MSNR_F_YUV_MSNRL3_RADIAL_BINNING_X, binning_x);
	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL3_BINNING_Y, MSNR_F_YUV_MSNRL3_RADIAL_BINNING_Y, binning_y);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL3_RADIAL_CENTER, MSNR_F_YUV_MSNRL3_RADIAL_CENTER_X,
		radial_center_x);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL3_RADIAL_CENTER, MSNR_F_YUV_MSNRL3_RADIAL_CENTER_Y,
		radial_center_y);
}

void msnr_hw_s_radial_l4(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 height,
	bool strip_enable, u32 strip_start_pos, struct msnr_radial_cfg *radial_cfg,
	struct is_msnr_config *msnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

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

	dbg_msnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL4_BINNING_X, MSNR_F_YUV_MSNRL4_RADIAL_BINNING_X, binning_x);
	MSNR_SET_F(
		base, MSNR_R_YUV_MSNRL4_BINNING_Y, MSNR_F_YUV_MSNRL4_RADIAL_BINNING_Y, binning_y);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL4_RADIAL_CENTER, MSNR_F_YUV_MSNRL4_RADIAL_CENTER_X,
		radial_center_x);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL4_RADIAL_CENTER, MSNR_F_YUV_MSNRL4_RADIAL_CENTER_Y,
		radial_center_y);
}

void msnr_hw_s_crc(struct pablo_mmio *base, u32 seed)
{
	dbg_msnr(4, "%s: ", __func__);

	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_STREAM_CRC, MSNR_F_YUV_CINFIFOL0_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_STREAM_CRC, MSNR_F_YUV_CINFIFOL1_CRC_SEED, seed);

	MSNR_SET_F(base, MSNR_R_YUV_COUTFIFO_STREAM_CRC, MSNR_F_YUV_COUTFIFO_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_STAT_COUTFIFO_STREAM_CRC, MSNR_F_STAT_COUTFIFO_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_STREAM_CRC, MSNR_F_YUV_CINFIFOL2_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_STREAM_CRC, MSNR_F_YUV_CINFIFOL3_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_STREAM_CRC, MSNR_F_YUV_CINFIFOL4_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_PRMDRECONL0_CRC, MSNR_F_YUV_PRMDRECONL0_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL0_CRC, MSNR_F_YUV_MSNRL0_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_PRMDRECONL1_CRC, MSNR_F_YUV_PRMDRECONL1_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL1_CRC, MSNR_F_YUV_MSNRL1_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_PRMDRECONL2_CRC, MSNR_F_YUV_PRMDRECONL2_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL2_CRC, MSNR_F_YUV_MSNRL2_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_PRMDRECONL3_CRC, MSNR_F_YUV_PRMDRECONL3_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL3_CRC, MSNR_F_YUV_MSNRL3_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_MSNRL4_CRC, MSNR_F_YUV_MSNRL4_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_YUV_YUVBLC_CRC, MSNR_F_YUV_YUVBLC_CRC_SEED, seed);
	MSNR_SET_F(
		base, MSNR_R_Y_DUALLAYERDECOMP_STREAM_CRC, MSNR_F_Y_DUALLAYERDECOMP_CRC_SEED, seed);
	MSNR_SET_F(base, MSNR_R_Y_DUALLAYERGAMMALB_STREAM_CRC, MSNR_F_Y_DUALLAYERGAMMALB_CRC_SEED,
		seed);
	MSNR_SET_F(base, MSNR_R_Y_DUALLAYERGAMMAAB_STREAM_CRC, MSNR_F_Y_DUALLAYERGAMMAAB_CRC_SEED,
		seed);
	MSNR_SET_F(base, MSNR_R_Y_DSLME_STREAM_CRC, MSNR_F_Y_DSLME_CRC_SEED, seed);
}

void msnr_hw_s_strgen(struct pablo_mmio *base, u32 set_id)
{
	dbg_msnr(4, "%s: ", __func__);

	/* STRGEN setting */
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_CONFIG, MSNR_F_YUV_CINFIFOL0_STRGEN_MODE_EN, 1);
	MSNR_SET_F(
		base, MSNR_R_YUV_CINFIFOL0_CONFIG, MSNR_F_YUV_CINFIFOL0_STRGEN_MODE_DATA_TYPE, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL0_CONFIG, MSNR_F_YUV_CINFIFOL0_STRGEN_MODE_DATA, 255);

	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_CONFIG, MSNR_F_YUV_CINFIFOL1_STRGEN_MODE_EN, 1);
	MSNR_SET_F(
		base, MSNR_R_YUV_CINFIFOL1_CONFIG, MSNR_F_YUV_CINFIFOL1_STRGEN_MODE_DATA_TYPE, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL1_CONFIG, MSNR_F_YUV_CINFIFOL1_STRGEN_MODE_DATA, 255);

	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_CONFIG, MSNR_F_YUV_CINFIFOL2_STRGEN_MODE_EN, 1);
	MSNR_SET_F(
		base, MSNR_R_YUV_CINFIFOL2_CONFIG, MSNR_F_YUV_CINFIFOL2_STRGEN_MODE_DATA_TYPE, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL2_CONFIG, MSNR_F_YUV_CINFIFOL2_STRGEN_MODE_DATA, 255);

	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_CONFIG, MSNR_F_YUV_CINFIFOL3_STRGEN_MODE_EN, 1);
	MSNR_SET_F(
		base, MSNR_R_YUV_CINFIFOL3_CONFIG, MSNR_F_YUV_CINFIFOL3_STRGEN_MODE_DATA_TYPE, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL3_CONFIG, MSNR_F_YUV_CINFIFOL3_STRGEN_MODE_DATA, 255);

	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_CONFIG, MSNR_F_YUV_CINFIFOL4_STRGEN_MODE_EN, 1);
	MSNR_SET_F(
		base, MSNR_R_YUV_CINFIFOL4_CONFIG, MSNR_F_YUV_CINFIFOL4_STRGEN_MODE_DATA_TYPE, 1);
	MSNR_SET_F(base, MSNR_R_YUV_CINFIFOL4_CONFIG, MSNR_F_YUV_CINFIFOL4_STRGEN_MODE_DATA, 255);
}
KUNIT_EXPORT_SYMBOL(msnr_hw_s_strgen);

u32 msnr_hw_g_reg_cnt(void)
{
	return MSNR_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(msnr_hw_g_reg_cnt);

const struct pmio_field_desc *msnr_hw_g_field_descs(void)
{
	return msnr_field_descs;
}

unsigned int msnr_hw_g_num_field_descs(void)
{
	return ARRAY_SIZE(msnr_field_descs);
}

const struct pmio_access_table *msnr_hw_g_access_table(int type)
{
	switch (type) {
	case 0:
		return &msnr_volatile_ranges_table;
	case 1:
		return &msnr_wr_noinc_ranges_table;
	default:
		return NULL;
	};

	return NULL;
}

void msnr_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &msnr_volatile_ranges_table;
	cfg->wr_noinc_table = &msnr_wr_noinc_ranges_table;

	cfg->max_register = MSNR_R_Y_DSLME_STREAM_CRC;
	cfg->num_reg_defaults_raw = (MSNR_R_Y_DSLME_STREAM_CRC >> 2) + 1;
	cfg->dma_addr_shift = 4;

	cfg->ranges = msnr_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(msnr_range_cfgs);

	cfg->fields = msnr_field_descs;
	cfg->num_fields = ARRAY_SIZE(msnr_field_descs);
}
KUNIT_EXPORT_SYMBOL(msnr_hw_init_pmio_config);
