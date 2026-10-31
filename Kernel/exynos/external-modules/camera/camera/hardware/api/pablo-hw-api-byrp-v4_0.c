// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * BYRP HW control APIs
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "pablo-hw-api-byrp-v4_0.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/pablo-sfr-byrp-v4_0.h"
#include "pmio.h"
#include "pablo-hw-api-common-ctrl.h"
#include "pablo-crta-interface.h"

#define BYRP_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define BYRP_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define BYRP_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define BYRP_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define BYRP_GET_R(base, R) PMIO_GET_R(base, R)

/* CMDQ Interrupt group mask */
#define BYRP_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
#define BYRP_INT_GRP_EN_MASK_FRO_FIRST BIT_MASK(PCC_INT_GRP_FRAME_START)
#define BYRP_INT_GRP_EN_MASK_FRO_MIDDLE 0
#define BYRP_INT_GRP_EN_MASK_FRO_LAST BIT_MASK(PCC_INT_GRP_FRAME_END)

#define BYRP_CH_CNT 5
#define LIC_HBLALK_CYCLE 50
#define CDAF_LENGTH 211

enum byrp_cotf_in_id {
	BYRP_COTF_IN_BYR,
};

enum byrp_cotf_out_id {
	BYRP_COTF_OUT_BYR,
};

struct byrp_line_buffer {
	u32 offset[BYRP_CH_CNT];
};

/* for image max width : 4080 */
/* EVT0 : assume that ctx3 is used for reprocessing */
static struct byrp_line_buffer lb_offset_evt0[] = {
	[0].offset = { 0, 4096, 8192, 12288, 23760 }, /* offset < 26000 & 16px aligned (8ppc) */
	[1].offset = { 0, 4096, 8192, 12288, 16383 }, /* offset < 16384 & 16px aligned (8ppc) */
	[2].offset = { 0, 4096, 8192, 12288, 16383 }, /* offset < 16384 & 16px aligned (8ppc) */
};
/* EVT1.1~ : assume that ctx0 is used for reprocessing */
static struct byrp_line_buffer lb_offset[] = {
	[0].offset = { 0, 13712, 17808, 21904, 25984 }, /* offset < 26000 & 16px aligned (8ppc) */
	[1].offset = { 0, 4096, 8192, 12288, 16383 }, /* offset < 16384 & 16px aligned (8ppc) */
	[2].offset = { 0, 4096, 8192, 12288, 16383 }, /* offset < 16384 & 16px aligned (8ppc) */
};

unsigned int byrp_hw_is_occurred(unsigned int status, enum byrp_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_BYRP_FRAME_START_INT;
		break;
	case INTR_FRAME_CINROW:
		mask = 1 << INTR0_BYRP_ROW_COL_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_BYRP_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_BYRP_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_BYRP_COREX_END_INT_1;
		break;
	case INTR_SETTING_DONE:
		mask = 1 << INTR0_BYRP_SETTING_DONE_INT;
		break;
	case INTR_ERR0:
		mask = BYRP_INT0_ERR_MASK;
		break;
	case INTR_WARN0:
		mask = BYRP_INT0_WARN_MASK;
		break;
	case INTR_FRAME_CDAF:
		mask = 1 << INTR1_BYRP_CDAF_AF_INT;
		break;
	case INTR_ERR1:
		mask = BYRP_INT1_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_is_occurred);

void byrp_hw_s_init_common(struct pablo_mmio *base)
{
	u32 ch;
	struct byrp_line_buffer *lb;

	if (exynos_soc_info.main_rev >= 1 &&
		exynos_soc_info.sub_rev >= 1)
		lb = lb_offset;
	else
		lb = lb_offset_evt0;

	for (ch = 0; ch < BYRP_CH_CNT; ch++) {
		BYRP_SET_F(base, BYRP_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x8 * ch),
			BYRP_F_CHAIN_LBCTRL_OFFSET_GRP0_C0 + (ch * 3), lb[0].offset[ch]);
		BYRP_SET_F(base, BYRP_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x8 * ch),
			BYRP_F_CHAIN_LBCTRL_OFFSET_GRP1_C0 + (ch * 3), lb[1].offset[ch]);
		BYRP_SET_F(base, BYRP_R_CHAIN_LBCTRL_OFFSET_GRP2TO3_C0 + (0x8 * ch),
			BYRP_F_CHAIN_LBCTRL_OFFSET_GRP2_C0 + (ch * 3), lb[2].offset[ch]);
	}
}

void byrp_hw_s_sr(struct pablo_mmio *base, bool enable, u32 ch_id)
{
	if (!enable) {
		/**
		 * Before releasing the DMA of Shared Resource,
		 * the module must be disabled in advance.
		 */
		pmio_cache_set_only(base, false);
		BYRP_SET_R(base, BYRP_R_BYR_RDMABYRIN_EN, 0);
		pmio_cache_set_only(base, true);

		BYRP_SET_R(base, BYRP_R_ALLOC_SR_ENABLE, enable);
		return;
	}

	BYRP_SET_F(base, BYRP_R_ALLOC_SR_GRP_0TO3, BYRP_F_ALLOC_SR_GRP0, 3);
	BYRP_SET_F(base, BYRP_R_ALLOC_SR_GRP_4TO7, BYRP_F_ALLOC_SR_GRP4, ch_id);
	BYRP_SET_F(base, BYRP_R_ALLOC_SR_GRP_4TO7, BYRP_F_ALLOC_SR_GRP5, 3);

	BYRP_SET_R(base, BYRP_R_ALLOC_SR_ENABLE, enable);
}

int byrp_hw_s_init(struct pablo_mmio *base, u32 ch_id)
{
	if (ch_id >= BYRP_CH_CNT) {
		err_hw("[BYRP] wrong ch_id: %d", ch_id);
		return -EINVAL;
	}

	BYRP_SET_F(base, BYRP_R_AUTO_MASK_PREADY, BYRP_F_AUTO_MASK_PREADY, 1);
	BYRP_SET_R(base, BYRP_R_CHAIN_WDMADNG_STALL_CNT_CTRL, 1);

	BYRP_SET_R(base, BYRP_R_BYR_RDMACLOAD_EN, 1);

	return 0;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_init);

void byrp_hw_s_strgen(struct pablo_mmio *base)
{
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_STRGEN_MODE_EN, 1);
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_STRGEN_MODE_DATA_TYPE, 1);
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_STRGEN_MODE_DATA, 255);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_strgen);

void byrp_hw_s_dtp(struct pablo_mmio *base, bool enable, u32 width, u32 height)
{
	u32 val;

	if (enable) {
		dbg_hw(1, "[API][%s] dtp color bar pattern is enabled!\n", __func__);

		BYRP_SET_F(base, BYRP_R_BYR_DTP_BYPASS, BYRP_F_BYR_DTP_BYPASS, !enable);
		BYRP_SET_F(base, BYRP_R_BYR_DTP_MODE, BYRP_F_BYR_DTP_MODE,
			0x8); /* color bar pattern */

		/* guide value */
		BYRP_SET_R(base, BYRP_R_BYR_DTP_HIGHER_LIMIT, 16383);
		BYRP_SET_R(base, BYRP_R_BYR_DTP_PATTERN_SIZE_X, width);
		BYRP_SET_R(base, BYRP_R_BYR_DTP_PATTERN_SIZE_Y, height);

		val = 0;
		val = BYRP_SET_V(base, val, BYRP_F_BYR_DTP_LAYER_WEIGHTS_0_0, 0x0);
		val = BYRP_SET_V(base, val, BYRP_F_BYR_DTP_LAYER_WEIGHTS_0_1, 0x0);
		val = BYRP_SET_V(base, val, BYRP_F_BYR_DTP_LAYER_WEIGHTS_0_2, 0x0);
		val = BYRP_SET_V(base, val, BYRP_F_BYR_DTP_LAYER_WEIGHTS_0_3, 0x0);
		BYRP_SET_R(base, BYRP_R_BYR_DTP_LAYER_WEIGHTS, val);

		val = 0;
		val = BYRP_SET_V(base, val, BYRP_F_BYR_DTP_LAYER_WEIGHTS_0_4, 0x20);
		BYRP_SET_R(base, BYRP_R_BYR_DTP_LAYER_WEIGHTS_1, val);

		BYRP_SET_F(base, BYRP_R_BYR_BITMASK0_BITTAGEOUT, BYRP_F_BYR_BITMASK0_BITTAGEOUT,
			0xE); /* dtp 14bit out */
	} else {
		BYRP_SET_F(base, BYRP_R_BYR_DTP_BYPASS, BYRP_F_BYR_DTP_BYPASS, 0x1);
	}
}

void byrp_hw_s_cinfifo(struct pablo_mmio *base, bool enable)
{
	if (!enable) {
		BYRP_SET_R(base, BYRP_R_BYR_CINFIFO_ENABLE, 0);

		return;
	}

	BYRP_SET_F(
		base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_AUTO_RECOVERY_EN, 1);

	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_ROL_EN, 1);
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_ROL_RESET_ON_FRAME_START, 1);
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_DEBUG_EN, 1);
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_STRGEN_MODE_EN, 0);

	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_INTERVAL_VBLANK, BYRP_F_BYR_CINFIFO_INTERVAL_VBLANK,
		VBLANK_CYCLE); /* Cycle: 32-bit: 2 ~ */
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_INTERVALS, BYRP_F_BYR_CINFIFO_INTERVAL_HBLANK,
		HBLANK_CYCLE); /* Cycle: 16-bit: 1 ~ */
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_INTERVALS, BYRP_F_BYR_CINFIFO_INTERVAL_PIXEL,
		0x0); /* Cycle: 16-bit: 0 ~ */

	BYRP_SET_R(base, BYRP_R_BYR_CINFIFO_INT_ENABLE, 0xF);

	BYRP_SET_R(base, BYRP_R_BYR_CINFIFO_ENABLE, 1);
}

static void byrp_hw_s_coutfifo(struct pablo_mmio *base, bool enable)
{
	u32 val = 0;

	BYRP_SET_F(base, BYRP_R_BYR_COUTFIFO_INTERVALS, BYRP_F_BYR_COUTFIFO_INTERVAL_HBLANK,
		HBLANK_CYCLE);
	BYRP_SET_F(base, BYRP_R_BYR_COUTFIFO_INTERVAL_VBLANK, BYRP_F_BYR_COUTFIFO_INTERVAL_VBLANK,
		VBLANK_CYCLE);
	BYRP_SET_R(base, BYRP_R_BYR_COUTFIFO_ENABLE, enable);
	BYRP_SET_R(base, BYRP_R_BYR_COUTFIFO_INT_ENABLE, 0x7);

	/* DEBUG */
	val = BYRP_SET_V(base, val, BYRP_F_BYR_COUTFIFO_VVALID_RISE_AT_FIRST_DATA_EN, 0);
	val = BYRP_SET_V(base, val, BYRP_F_BYR_COUTFIFO_DEBUG_EN, 1); /* stall cnt */
	val = BYRP_SET_V(base, val, BYRP_F_BYR_COUTFIFO_BACK_STALL_EN, 1); /* RGBP is ready */
	BYRP_SET_R(base, BYRP_R_BYR_COUTFIFO_CONFIG, val);
}

void __byrp_hw_s_lic_cfg(void __iomem *base, bool rdma_en, u32 bit_in)
{
	u32 val;

	val = 0;
	val = BYRP_SET_V(base, val, BYRP_F_LIC_BYPASS, 0);
	val = BYRP_SET_V(base, val, BYRP_F_LIC_DEBUG_ON, 1);
	val = BYRP_SET_V(base, val, BYRP_F_LIC_FAKE_GEN_ON, 0);
	BYRP_SET_R(base, BYRP_R_LIC_INPUT_MODE, val);

	if (bit_in == 12)
		BYRP_SET_F(base, BYRP_R_LIC_INPUT_CONFIG_0, BYRP_F_LIC_BIT_MODE, 1);
	else if (bit_in == 14)
		BYRP_SET_F(base, BYRP_R_LIC_INPUT_CONFIG_0, BYRP_F_LIC_BIT_MODE, 2);
	else
		BYRP_SET_F(base, BYRP_R_LIC_INPUT_CONFIG_0, BYRP_F_LIC_BIT_MODE, 0);

	BYRP_SET_F(base, BYRP_R_LIC_INPUT_CONFIG_0, BYRP_F_LIC_RDMA_EN, rdma_en);

	val = 0;
	val = BYRP_SET_V(base, val, BYRP_F_LIC_IN_HBLANK_CYCLE, LIC_HBLALK_CYCLE);
	val = BYRP_SET_V(base, val, BYRP_F_LIC_OUT_HBLANK_CYCLE, LIC_HBLALK_CYCLE);
	BYRP_SET_R(base, BYRP_R_LIC_INPUT_BLANK, val);
}

void byrp_hw_s_path(struct pablo_mmio *base, struct byrp_param_set *param_set,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	u32 cin_en, rdma, cout_en, dng_select, bit_in;
	struct pablo_common_ctrl_cr_set *ext_cr_set;

	cin_en = param_set->otf_input.cmd;
	cout_en = 1;
	dng_select = 0; /* 0: from cgras, 1: from byrhdr */
	rdma = param_set->dma_input.cmd;

	if (cin_en && rdma) {
		err_hw("do not support both cin and rdma");
		cin_en = 0;
	} else if (!cin_en && !rdma) {
		err_hw("both cin and rdma are disabled");
	}

	frame_cfg->cotf_in_en = cin_en ? BIT_MASK(BYRP_COTF_IN_BYR) : 0;
	frame_cfg->cotf_out_en = BIT_MASK(BYRP_COTF_OUT_BYR);

	if (cin_en)
		bit_in = param_set->otf_input.bitwidth;
	else
		bit_in = param_set->dma_input.msb + 1;

	if (bit_in == 10)
		BYRP_SET_R(base, BYRP_R_CHAIN_INPUT_BITWIDTH, 0);
	else if (bit_in == 12)
		BYRP_SET_R(base, BYRP_R_CHAIN_INPUT_BITWIDTH, 1);
	else if (bit_in == 14)
		BYRP_SET_R(base, BYRP_R_CHAIN_INPUT_BITWIDTH, 2);
	else
		BYRP_SET_R(base, BYRP_R_CHAIN_INPUT_BITWIDTH, 3);

	BYRP_SET_F(base, BYRP_R_CHAIN_OUTPUT_0_SELECT, BYRP_F_CHAIN_OUTPUT_0_SELECT, dng_select);
	BYRP_SET_F(base, BYRP_R_CHAIN_OUTPUT_0_SELECT, BYRP_F_CHAIN_OUTPUT_0_MASK_0, !dng_select);
	BYRP_SET_F(base, BYRP_R_CHAIN_OUTPUT_0_SELECT, BYRP_F_CHAIN_OUTPUT_0_MASK_1, dng_select);

	BYRP_SET_F(
		base, BYRP_R_OTF_PLATFORM_INPUT_MUX_0TO3, BYRP_F_OTF_PLATFORM_INPUT_0_MUX, cin_en);

	byrp_hw_s_cinfifo(base, cin_en);
	byrp_hw_s_coutfifo(base, cout_en);
	__byrp_hw_s_lic_cfg(base, rdma, bit_in);

	ext_cr_set = &frame_cfg->ext_cr_set;
	ext_cr_set->cr = byrp_ext_cr_set;
	ext_cr_set->size = ARRAY_SIZE(byrp_ext_cr_set);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_path);

int byrp_hw_wait_idle(struct pablo_mmio *base)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int0_all;
	u32 int1_all;
	u32 try_cnt = 0;

	idle = BYRP_GET_F(base, BYRP_R_IDLENESS_STATUS, BYRP_F_IDLENESS_STATUS);
	int0_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT0_STATUS);
	int1_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT1_STATUS);

	info_hw("[BYRP] idle status before disable (idle: %d, int0: 0x%X, int1: 0x%X)\n", idle,
		int0_all, int1_all);

	while (!idle) {
		idle = BYRP_GET_F(base, BYRP_R_IDLENESS_STATUS, BYRP_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= BYRP_TRY_COUNT) {
			err_hw("[BYRP] timeout waiting idle - disable fail");
			byrp_hw_dump(base, HW_DUMP_CR);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT0_STATUS);
	int1_all = BYRP_GET_R(base, BYRP_R_INT_REQ_INT1_STATUS);

	info_hw("[BYRP] idle status after disable (idle: %d, int0: 0x%X, int1: 0x%X)\n", idle,
		int0_all, int1_all);

	return ret;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_wait_idle);

int byrp_hw_s_reset(struct pablo_mmio *base)
{
	int ret;
	u32 val = 0;
	u32 retry = BYRP_TRY_COUNT;

	BYRP_SET_R(base, BYRP_R_SW_RESET, 1);
	do {
		val = BYRP_GET_R(base, BYRP_R_SW_RESET);
		if (val)
			udelay(1);
		else
			break;
	} while (--retry);

	if (val) {
		err_hw("[BYRP] sw reset timeout(%#x)", val);
		return -ETIME;
	}

	ret = byrp_hw_wait_idle(base);

	return ret;
}

static const struct is_reg byrp_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0200, "GLOBAL_IMAGE_RESOLUTION" },
	{ 0x0204, "IMAGE_RESOLUTION_CROP" },
	{ 0x0208, "IMAGE_RESOLUTION_CROPDNG" },
	{ 0x020c, "IMAGE_RESOLUTION_SMCB0" },
	{ 0x0210, "IMAGE_RESOLUTION_SMCB1" },
	/* Chain Path */
	{ 0x0220, "CHAIN_OTF_34_SELECT" },
	{ 0x0228, "CHAIN_OUTPUT_0_SELECT" },
	{ 0x0300, "ALLOC_SR_ENABLE" },
	{ 0x0304, "ALLOC_SR_GRP_0TO3" },
	{ 0x0308, "ALLOC_SR_GRP_4TO7" },
	{ 0x0330, "OTF_PLATFORM_SINK_DEMUX_0TO3" },
	{ 0x0340, "OTF_PLATFORM_INPUT_MUX_0TO3" },
	{ 0x0344, "OTF_PLATFORM_INPUT_MUX_4TO7" },
	{ 0x0348, "OTF_PLATFORM_OUTPUT_DEMUX_0TO3" },
	{ 0x034c, "OTF_PLATFORM_OUTPUT_DEMUX_4TO7" },
	/* CINFIFO 0 Status */
	{ 0x1000, "BYR_CINFIFO_ENABLE" },
	{ 0x1014, "BYR_CINFIFO_STATUS" },
	{ 0x1018, "BYR_CINFIFO_INPUT_CNT" },
	{ 0x101c, "BYR_CINFIFO_STALL_CNT" },
	{ 0x1020, "BYR_CINFIFO_FIFO_FULLNESS" },
	{ 0x1040, "BYR_CINFIFO_INT" },
	/* COUTFIFO 0 Status */
	{ 0x1200, "BYR_COUTFIFO_ENABLE" },
	{ 0x1214, "BYR_COUTFIFO_STATUS" },
	{ 0x1218, "BYR_COUTFIFO_INPUT_CNT" },
	{ 0x121c, "BYR_COUTFIFO_STALL_CNT" },
	{ 0x1220, "BYR_COUTFIFO_FIFO_FULLNESS" },
	{ 0x1240, "BYR_COUTFIFO_INT" },
	/* LIC */
	{ 0x3020, "LIC_OUTPUT_ERROR" },
	{ 0x3024, "LIC_OUTPUT_POSITION" },
	{ 0x3028, "LIC_OUTPUT_MEM_STATUS" },
	{ 0x3030, "LIC_DEBUG_IN_HVCNT" },
	{ 0x3034, "LIC_DEBUG_IN_FCNT" },
	{ 0x3038, "LIC_DEBUG_OUT_HVCNT" },
	{ 0x303c, "LIC_DEBUG_OUT_FCNT" },

};

static void byrp_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, BYRP_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(byrp_dbg_cr); i++) {
		cr = &byrp_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

void byrp_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		info_hw("[BYRP]%s:DUMP CR\n", __FILENAME__);
		is_hw_dump_regs(pmio_get_base(pmio), byrp_regs, BYRP_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		info_hw("[BYRP]%s:DUMP DBG_STATE\n", __FILENAME__);
		byrp_hw_dump_dbg_state(pmio);
		break;
	default:
		err_hw("[BYRP]%s:Not supported dump_mode %u", __FILENAME__, mode);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(byrp_hw_dump);

static void byrp_hw_g_rdma_fmt_map(int dma_id, ulong *byr_fmt_map)
{
	switch (dma_id) {
	case BYRP_RDMA_IMG: /* 0,1,2,4,5,6,7,8,9,10,11,12,13,14 */
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_PACK) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_ANDROID10) | BIT_MASK(DMA_FMT_U12BIT_PACK) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_ANDROID12) | BIT_MASK(DMA_FMT_U14BIT_PACK) |
				       BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)) &
			       IS_BAYER_FORMAT_MASK;
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma_id);
		break;
	}
}

static void byrp_hw_g_wdma_fmt_map(int dma_id, ulong *byr_fmt_map)
{
	switch (dma_id) {
	case BYRP_WDMA_BYR: /* 0,1,2,4,5,6,8,9,10,12,13,14 */
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_PACK) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_PACK) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U14BIT_PACK) |
				       BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)) &
			       IS_BAYER_FORMAT_MASK;
		break;
	case BYRP_WDMA_THSTAT_PRE:
	case BYRP_WDMA_CDAF:
	case BYRP_WDMA_RGBYHIST:
	case BYRP_WDMA_THSTAT_AE:
	case BYRP_WDMA_THSTAT_AWB:
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK)) & IS_BAYER_FORMAT_MASK;
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma_id);
		break;
	}
}

static void byrp_hw_s_fro(struct pablo_mmio *base, u32 num_buffer)
{
	u32 center_frame_num = (num_buffer > 1) ? (((num_buffer + 1) / 2) - 1) : 0;

	BYRP_SET_F(base, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_THSTAT_PRE, center_frame_num);
	BYRP_SET_F(base, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_CDAF, center_frame_num);
	BYRP_SET_F(base, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_RGBYHIST, center_frame_num);
	BYRP_SET_F(base, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_THSTAT, center_frame_num);
}

void byrp_hw_g_input_param(struct byrp_param_set *param_set, u32 instance, u32 id,
	struct param_dma_input **dma_input, dma_addr_t **input_dva)
{
	switch (id) {
	case BYRP_RDMA_IMG:
		*input_dva = param_set->input_dva;
		*dma_input = &param_set->dma_input;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		break;
	}
}

int byrp_hw_s_rdma_addr(struct is_common_dma *dma, dma_addr_t *addr, u32 num_buffers,
	u32 comp_sbwc_en, u32 payload_size)
{
	int ret;
	u32 i;
	dma_addr_t header_addr[IS_MAX_FRO];

	switch (dma->id) {
	case BYRP_RDMA_IMG:
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, addr, 0, 0, num_buffers);
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma->id);
		return SET_ERROR;
	}

	if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case BYRP_RDMA_IMG:
			for (i = 0; i < num_buffers; i++)
				header_addr[i] = addr[i] + payload_size;
			break;
		default:
			break;
		}
		ret = CALL_DMA_OPS(dma, dma_set_header_addr, header_addr, 0, 0, num_buffers);
	}

	return ret;
}

void byrp_hw_s_rdma_init(
	struct is_common_dma *dma, struct byrp_param_set *param_set, u32 num_buffers)
{
	struct param_dma_input *dma_input;
	u32 comp_sbwc_en, comp_64b_align = 1, quality_control = 0;
	u32 dma_stride_1p, dma_header_stride_1p = 0, stride_align = 1;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 format, bus_info;
	int ret = 0;
	u32 width, height;
	u32 payload_size = 0;
	u32 cache_hint = 0;
	dma_addr_t *input_dva;

	switch (dma->id) {
	case BYRP_RDMA_IMG:
		dma_input = &param_set->dma_input;
		input_dva = param_set->input_dva;
		stride_align = 32;
		cache_hint = IS_LLC_CACHE_HINT_LAST_ACCESS;
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma->id);
		return;
		;
	}

	if (dma_input->cmd == DMA_INPUT_COMMAND_DISABLE)
		goto skip_dma;

	width = dma_input->width;
	height = dma_input->height;
	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;
	bus_info = 0x00000000UL; /* cache hint [6:4] */

	dbg_hw(2, "[API][%s]%s %dx%d, format: %d, bitwidth: %d\n", __func__, dma->name, width,
		height, hwformat, memory_bitwidth);

	comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 0) {
		dma_stride_1p = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, width, stride_align, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		dma_stride_1p =
			is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, width, comp_64b_align,
				quality_control, BYRP_COMP_BLOCK_WIDTH, BYRP_COMP_BLOCK_HEIGHT);
		dma_header_stride_1p =
			is_hw_dma_get_header_stride(width, BYRP_COMP_BLOCK_WIDTH, stride_align);
	} else {
		err_hw("[BYRP][%s] Invalid SBWC mode. ret %d", dma->name, comp_sbwc_en);
		goto skip_dma;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, dma_stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);

	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, dma_header_stride_1p, 0);
		payload_size = DIV_ROUND_UP(height, BYRP_COMP_BLOCK_HEIGHT) * dma_stride_1p;
		break;
	default:
		break;
	}

	byrp_hw_s_rdma_addr(dma, input_dva, num_buffers, comp_sbwc_en, payload_size);

	dbg_hw(2, "[BYRP][%s]dma_cfg: ON\n", dma->name);
	CALL_DMA_OPS(dma, dma_enable, 1);

	return;

skip_dma:
	dbg_hw(2, "[BYRP][%s]dma_cfg: OFF\n", dma->name);
	CALL_DMA_OPS(dma, dma_enable, 0);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_rdma_init);

void byrp_hw_s_wdma_init(
	struct is_common_dma *dma, struct byrp_param_set *param_set, u32 num_buffers)
{
	struct param_dma_output *dma_output;
	pdma_addr_t *output_dva;
	u32 stride_1p;
	u32 hwformat, memory_bitwidth, pixelsize;
	u32 i, width, height, format;
	dma_addr_t addr[IS_MAX_FRO];
	bool img_flag = false;
	int ret = 0;

	switch (dma->id) {
	case BYRP_WDMA_BYR:
		output_dva = param_set->output_dva_byr;
		dma_output = &param_set->dma_output_byr;
		img_flag = true;
		break;
	case BYRP_WDMA_THSTAT_PRE:
		output_dva = &param_set->output_dva_pre;
		dma_output = &param_set->dma_output_pre;
		break;
	case BYRP_WDMA_CDAF:
		output_dva = &param_set->output_dva_cdaf;
		dma_output = &param_set->dma_output_cdaf;
		break;
	case BYRP_WDMA_RGBYHIST:
		output_dva = &param_set->output_dva_rgby;
		dma_output = &param_set->dma_output_rgby;
		break;
	case BYRP_WDMA_THSTAT_AE:
		output_dva = &param_set->output_dva_ae;
		dma_output = &param_set->dma_output_ae;
		break;
	case BYRP_WDMA_THSTAT_AWB:
		output_dva = &param_set->output_dva_awb;
		dma_output = &param_set->dma_output_awb;
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma->id);
		return;
	}

	if (dma_output->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		goto skip_dma;

	width = dma_output->width;
	height = dma_output->height;
	hwformat = dma_output->format;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	if (!is_hw_dma_get_bayer_format(memory_bitwidth, pixelsize, hwformat, 0, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	stride_1p = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize, hwformat, width, 1, true);

	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	if (ret)
		goto skip_dma;

	for (i = 0; i < num_buffers; i++)
		addr[i] = img_flag ? output_dva[i] : output_dva[0];
	ret = CALL_DMA_OPS(dma, dma_set_img_addr, addr, 0, 0, num_buffers);

	dbg_hw(2, "[API][%s]%s %dx%d, stride_1p:%d, format: %d, bitwidth: %d\n", __func__,
		dma->name, width, height, stride_1p, hwformat, memory_bitwidth);

	CALL_DMA_OPS(dma, dma_enable, 1);

	return;

skip_dma:
	dbg_hw(2, "[BYRP][%s]dma_cfg: OFF\n", dma->name);

	CALL_DMA_OPS(dma, dma_enable, 0);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_wdma_init);

static int byrp_hw_rdma_create_pmio(struct is_common_dma *dma, void *base, u32 dma_id)
{
	ulong available_bayer_format_map;
	int ret = SET_SUCCESS;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[BYRP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (dma_id) {
	case BYRP_RDMA_IMG:
		dma->reg_ofs = BYRP_R_BYR_RDMABYRIN_EN;
		dma->field_ofs = BYRP_F_BYR_RDMABYRIN_EN;

		byrp_hw_g_rdma_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_RDMA_BYR");
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma_id);
		ret = SET_ERROR;
		goto err_dma_create;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, dma_id, name, available_bayer_format_map, 0, 0);
	CALL_DMA_OPS(dma, dma_set_corex_id, COREX_DIRECT);

err_dma_create:
	__putname(name);

	return ret;
}

int byrp_hw_rdma_create(struct is_common_dma *dma, void *base, u32 dma_id)
{
	return byrp_hw_rdma_create_pmio(dma, base, dma_id);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_rdma_create);

static int byrp_hw_wdma_create_pmio(struct is_common_dma *dma, void *base, u32 dma_id)
{
	ulong available_bayer_format_map;
	int ret;
	char *name;

	name = __getname();
	if (!name) {
		err_hw("[BYRP] Failed to get name buffer");
		return -ENOMEM;
	}

	switch (dma_id) {
	case BYRP_WDMA_BYR:
		dma->reg_ofs = BYRP_R_BYR_WDMADNG_EN;
		dma->field_ofs = BYRP_F_BYR_WDMADNG_EN;

		byrp_hw_g_wdma_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_WDMA_BYR");
		break;
	case BYRP_WDMA_THSTAT_PRE:
		dma->reg_ofs = BYRP_R_STAT_WDMATHSTATPRE_EN;
		dma->field_ofs = BYRP_F_STAT_WDMATHSTATPRE_EN;

		byrp_hw_g_wdma_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_WDMA_THSTATPRE");
		break;
	case BYRP_WDMA_CDAF:
		dma->reg_ofs = BYRP_R_STAT_WDMACDAF_EN;
		dma->field_ofs = BYRP_F_STAT_WDMACDAF_EN;

		byrp_hw_g_wdma_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_WDMA_CDAF");
		break;
	case BYRP_WDMA_RGBYHIST:
		dma->reg_ofs = BYRP_R_STAT_WDMARGBYHIST_EN;
		dma->field_ofs = BYRP_F_STAT_WDMARGBYHIST_EN;

		byrp_hw_g_wdma_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_WDMA_RGBYHIST");
		break;
	case BYRP_WDMA_THSTAT_AE:
		dma->reg_ofs = BYRP_R_STAT_WDMATHSTATAE_EN;
		dma->field_ofs = BYRP_F_STAT_WDMATHSTATAE_EN;

		byrp_hw_g_wdma_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_WDMA_THSTATAE");
		break;
	case BYRP_WDMA_THSTAT_AWB:
		dma->reg_ofs = BYRP_R_STAT_WDMATHSTATAWB_EN;
		dma->field_ofs = BYRP_F_STAT_WDMATHSTATAWB_EN;

		byrp_hw_g_wdma_fmt_map(dma_id, &available_bayer_format_map);
		snprintf(name, PATH_MAX, "BYR_WDMA_THSTATAWB");
		break;
	default:
		err_hw("[BYRP] NOT support DMA[%d]", dma_id);
		ret = SET_ERROR;
		goto err_dma_create;
	}
	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, dma_id, name, available_bayer_format_map, 0, 0);
	CALL_DMA_OPS(dma, dma_set_corex_id, COREX_DIRECT);

err_dma_create:
	__putname(name);

	return ret;
}

int byrp_hw_wdma_create(struct is_common_dma *dma, void *base, u32 dma_id)
{
	return byrp_hw_wdma_create_pmio(dma, base, dma_id);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_wdma_create);

void byrp_hw_s_dma_cfg(struct byrp_param_set *param_set, struct is_byrp_config *conf)
{
	/* Each formula follows the guide from IQ */
	param_set->dma_output_pre.cmd = !conf->thstat_pre_bypass;
	if (param_set->dma_output_pre.cmd) {
		param_set->dma_output_pre.width = conf->thstat_pre_grid_w * 2 * 12;
		param_set->dma_output_pre.height = conf->thstat_pre_grid_h;

		if (!param_set->dma_output_pre.width || !param_set->dma_output_pre.height) {
			param_set->dma_output_pre.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][BYRP][F%d] Invalid PRE size %dx%d", param_set->instance_id,
				param_set->fcount, param_set->dma_output_pre.width,
				param_set->dma_output_pre.height);
		}

		dbg_hw(3, "[%d][BYRP]set_config-%s:[F%d] pre %dx%d\n", param_set->instance_id,
			__func__, param_set->fcount, conf->thstat_pre_grid_w,
			conf->thstat_pre_grid_h);
	}

	param_set->dma_output_awb.cmd = !conf->thstat_awb_bypass;
	if (param_set->dma_output_awb.cmd) {
		param_set->dma_output_awb.width = conf->thstat_awb_grid_w * 2 * 8;
		param_set->dma_output_awb.height = conf->thstat_awb_grid_h;

		if (!param_set->dma_output_awb.width || !param_set->dma_output_awb.height) {
			param_set->dma_output_awb.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][BYRP][F%d] Invalid AWB size %dx%d", param_set->instance_id,
				param_set->fcount, param_set->dma_output_awb.width,
				param_set->dma_output_awb.height);
		}

		dbg_hw(3, "[%d][BYRP]set_config-%s:[F%d] awb %dx%d\n", param_set->instance_id,
			__func__, param_set->fcount, conf->thstat_awb_grid_w,
			conf->thstat_awb_grid_h);
	}

	param_set->dma_output_ae.cmd = !conf->thstat_ae_bypass;
	if (param_set->dma_output_ae.cmd) {
		param_set->dma_output_ae.width = conf->thstat_ae_grid_w * 2 * 8;
		param_set->dma_output_ae.height = conf->thstat_ae_grid_h;

		if (!param_set->dma_output_ae.width || !param_set->dma_output_ae.height) {
			param_set->dma_output_ae.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][BYRP][F%d] Invalid AE size %dx%d", param_set->instance_id,
				param_set->fcount, param_set->dma_output_ae.width,
				param_set->dma_output_ae.height);
		}

		dbg_hw(3, "[%d][BYRP]set_config-%s:[F%d] ae %dx%d\n", param_set->instance_id,
			__func__, param_set->fcount, conf->thstat_ae_grid_w,
			conf->thstat_ae_grid_h);
	}

	param_set->dma_output_rgby.cmd = !conf->rgbyhist_bypass;
	if (param_set->dma_output_rgby.cmd) {
		param_set->dma_output_rgby.width =
			conf->rgbyhist_bin_num * 4 * conf->rgbyhist_hist_num;
		param_set->dma_output_rgby.height = 1;

		if (!param_set->dma_output_rgby.width || !param_set->dma_output_rgby.height) {
			param_set->dma_output_rgby.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][BYRP][F%d] Invalid RGBY size %dx%d", param_set->instance_id,
				param_set->fcount, param_set->dma_output_rgby.width,
				param_set->dma_output_rgby.height);
		}

		dbg_hw(3, "[%d][BYRP]set_config-%s:[F%d] rgby %d, %d\n", param_set->instance_id,
			__func__, param_set->fcount, conf->rgbyhist_bin_num,
			conf->rgbyhist_hist_num);
	}

	param_set->dma_output_cdaf.cmd = !(conf->cdaf_bypass || conf->cdaf_mw_bypass);
	if (param_set->dma_output_cdaf.cmd) {
		param_set->dma_output_cdaf.width = conf->cdaf_mw_x * 48; /* 48 bytes/grid */
		param_set->dma_output_cdaf.height = conf->cdaf_mw_y;

		if (!param_set->dma_output_cdaf.width || !param_set->dma_output_cdaf.height) {
			param_set->dma_output_cdaf.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][CSTAT][F%d] Invalid CDAF Multi Window size %dx%d",
				param_set->instance_id, param_set->fcount,
				param_set->dma_output_cdaf.width,
				param_set->dma_output_cdaf.height);
		}

		dbg_hw(3, "[%d][CSTAT]set_config-%s:[F%d] cdaf_mw %dx%d\n", param_set->instance_id,
			__func__, param_set->fcount, conf->cdaf_mw_x, conf->cdaf_mw_y);
	}
}

int byrp_hw_g_cdaf_data(struct pablo_mmio *pmio, void *data)
{
	u32 i, val;
	u32 *dst = (u32 *)data;
	u32 stat_length = CDAF_LENGTH;
	void *ctx;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;

	/* Initialize the read access pointer */
	pmio->reg_write(ctx, BYRP_R_BYR_CDAF_STAT_START_ADD, 0);
	pmio->reg_read(ctx, BYRP_R_BYR_CDAF_STAT_ADD_WRITE_TRIGGER, &val);

	for (i = 0; i < stat_length; i++)
		pmio->reg_read(ctx, BYRP_R_BYR_CDAF_STAT_ACCESS, &dst[i]);

	pmio->reg_write(ctx, BYRP_R_BYR_CDAF_STAT_ACCESS_END, 1);
	pmio->reg_write(ctx, BYRP_R_BYR_CDAF_STAT_ACCESS_END, 0);

	return 0;
}

struct param_dma_output *byrp_hw_s_stat_cfg(
	u32 dma_id, dma_addr_t addr, struct byrp_param_set *p_set)
{
	struct param_dma_output *dma_out;

	switch (dma_id) {
	case BYRP_WDMA_THSTAT_PRE:
		dma_out = &p_set->dma_output_pre;
		p_set->output_dva_pre = addr;
		break;
	case BYRP_WDMA_THSTAT_AE:
		dma_out = &p_set->dma_output_ae;
		p_set->output_dva_ae = addr;
		break;
	case BYRP_WDMA_THSTAT_AWB:
		dma_out = &p_set->dma_output_awb;
		p_set->output_dva_awb = addr;
		break;
	case BYRP_WDMA_RGBYHIST:
		dma_out = &p_set->dma_output_rgby;
		p_set->output_dva_rgby = addr;
		break;
	case BYRP_WDMA_CDAF:
		dma_out = &p_set->dma_output_cdaf;
		p_set->output_dva_cdaf = addr;
		break;
	default:
		return NULL;
	}

	if (dma_out->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		return NULL;

	dma_out->format = DMA_INOUT_FORMAT_BAYER;
	dma_out->order = DMA_INOUT_ORDER_NO;
	dma_out->bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	dma_out->msb = dma_out->bitwidth - 1;
	dma_out->plane = DMA_INOUT_PLANE_1;

	return dma_out;
}

void byrp_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = BYRP_INT0_EN_MASK;
	int_en[PCC_INT_1] = BYRP_INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_int_en);

u32 byrp_hw_g_int_grp_en(void)
{
	return BYRP_INT_GRP_EN_MASK;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_int_grp_en);

void byrp_hw_cotf_error_handle(struct pablo_mmio *base)
{
	u32 cinfifo, coutfifo;

	cinfifo = BYRP_GET_R(base, BYRP_R_BYR_CINFIFO_INT);
	BYRP_SET_R(base, BYRP_R_BYR_CINFIFO_INT_CLEAR, cinfifo);

	coutfifo = BYRP_GET_R(base, BYRP_R_BYR_COUTFIFO_INT);
	BYRP_SET_R(base, BYRP_R_BYR_COUTFIFO_INT_CLEAR, coutfifo);

	info_hw("[BYRP] %s cinfifo(0x%x) coutfifo(0x%x)\n", __func__, cinfifo, coutfifo);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_cotf_error_handle);

void byrp_hw_s_block_bypass(struct pablo_mmio *base)
{
	BYRP_SET_F(base, BYRP_R_BYR_BITMASK0_BYPASS, BYRP_F_BYR_BITMASK0_BYPASS, 0x0);
	BYRP_SET_F(base, BYRP_R_BYR_GAMMASENSOR_BYPASS, BYRP_F_BYR_GAMMASENSOR_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_BLCBYR_BYPASS, BYRP_F_BYR_BLCBYR_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_AFIDENTBPC_BYPASS, BYRP_F_BYR_AFIDENTBPC_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_BPCSUSPMAP_BYPASS, BYRP_F_BYR_BPCSUSPMAP_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_BPCGGC_BYPASS, BYRP_F_BYR_BPCGGC_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_BPCFLATDETECTOR_BYPASS, BYRP_F_BYR_BPCFLATDETECTOR_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_BPCDIRDETECTOR_BYPASS, BYRP_F_BYR_BPCDIRDETECTOR_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_BYPASS, BYRP_F_BYR_DISPARITY_BYPASS, 0x1);

	BYRP_SET_F(base, BYRP_R_BYR_PREDNS0_BYPASS, BYRP_F_BYR_PREDNS0_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_BYRHDR_BYPASS, BYRP_F_BYR_BYRHDR_BYPASS, 0x1);

	BYRP_SET_F(base, BYRP_R_BYR_CGRAS_BYPASS_REG, BYRP_F_BYR_CGRAS_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_WBGDNG_BYPASS, BYRP_F_BYR_WBGDNG_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_BLCDNG_BYPASS, BYRP_F_BYR_BLCDNG_BYPASS, 0x1);

	BYRP_SET_F(base, BYRP_R_TETRA_SMCB0_CTRL, BYRP_F_TETRA_SMCB0_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATPRE_BYPASS, BYRP_F_BYR_THSTATPRE_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_TETRA_SMCB1_CTRL, BYRP_F_TETRA_SMCB1_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_CDAF_BYPASS, BYRP_F_BYR_CDAF_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_RGBYHIST_BYPASS, BYRP_F_BYR_RGBYHIST_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATAE_BYPASS, BYRP_F_BYR_THSTATAE_BYPASS, 0x1);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATAWB_BYPASS, BYRP_F_BYR_THSTATAWB_BYPASS, 0x1);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_block_bypass);

static void byrp_hw_s_block_crc(struct pablo_mmio *base, u32 seed)
{
	BYRP_SET_F(base, BYRP_R_BYR_CINFIFO_STREAM_CRC, BYRP_F_BYR_CINFIFO_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_COUTFIFO_STREAM_CRC, BYRP_F_BYR_COUTFIFO_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_DTP_STREAM_CRC, BYRP_F_BYR_DTP_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_CROPIN_STREAM_CRC, BYRP_F_BYR_CROPIN_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_BITMASK0_CRC, BYRP_F_BYR_BITMASK0_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_GAMMASENSOR_STREAM_CRC, BYRP_F_BYR_GAMMASENSOR_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_BLCBYR_STREAM_CRC, BYRP_F_BYR_BLCBYR_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_AFIDENTBPC_STREAM_CRC, BYRP_F_BYR_AFIDENTBPC_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_BPCSUSPMAP_CRC_SEED, BYRP_F_BYR_BPCSUSPMAP_CRC_SEED, seed);
	BYRP_SET_F(
		base, BYRP_R_BYR_BPCDIRDETECTOR_CRC_SEED, BYRP_F_BYR_BPCDIRDETECTOR_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_PREDNS0_STREAM_CRC, BYRP_F_BYR_PREDNS0_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_BYRHDR_STREAM_CRC, BYRP_F_BYR_BYRHDR_CRC_SEED, seed);

	BYRP_SET_F(base, BYRP_R_BYR_CGRAS_CRC, BYRP_F_BYR_CGRAS_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_WBGDNG_STREAM_CRC, BYRP_F_BYR_WBGDNG_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_BLCDNG_STREAM_CRC, BYRP_F_BYR_BLCDNG_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_CROPDNG_STREAM_CRC, BYRP_F_BYR_CROPDNG_CRC_SEED, seed);

	BYRP_SET_F(base, BYRP_R_TETRA_SMCB0_CRC, BYRP_F_TETRA_SMCB0_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATPRE_CRC, BYRP_F_BYR_THSTATPRE_CRC_SEED, seed);

	BYRP_SET_F(base, BYRP_R_TETRA_SMCB1_CRC, BYRP_F_TETRA_SMCB1_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATAE_CRC, BYRP_F_BYR_THSTATAE_CRC_SEED, seed);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATAWB_CRC, BYRP_F_BYR_THSTATAWB_CRC_SEED, seed);
}

void byrp_hw_s_bitmask(struct pablo_mmio *base, u32 bit_in, u32 bit_out)
{
	BYRP_SET_F(base, BYRP_R_BYR_BITMASK0_BITTAGEIN, BYRP_F_BYR_BITMASK0_BITTAGEIN, bit_in);
	BYRP_SET_F(base, BYRP_R_BYR_BITMASK0_BITTAGEOUT, BYRP_F_BYR_BITMASK0_BITTAGEOUT, bit_out);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_bitmask);

static void byrp_hw_s_pixel_order(struct pablo_mmio *base, u32 pixel_order)
{
	BYRP_SET_F(base, BYRP_R_BYR_DTP_PIXEL_ORDER, BYRP_F_BYR_DTP_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_BPCSUSPMAP_PIXEL_ORDER, BYRP_F_BYR_BPCSUSPMAP_PIXEL_ORDER,
		pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_BPCGGC_PIXEL_ORDER, BYRP_F_BYR_BPCGGC_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_BPCFLATDETECTOR_PIXEL_ORDER,
		BYRP_F_BYR_BPCFLATDETECTOR_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_BPCDIRDETECTOR_PIXEL_ORDER,
		BYRP_F_BYR_BPCDIRDETECTOR_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_PIXEL_ORDER, BYRP_F_BYR_DISPARITY_PIXEL_ORDER,
		pixel_order);
	BYRP_SET_F(
		base, BYRP_R_BYR_PREDNS0_PIXEL_ORDER, BYRP_F_BYR_PREDNS0_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_BYRHDR_PIXEL_ORDER, BYRP_F_BYR_BYRHDR_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_CGRAS_PIXEL_ORDER, BYRP_F_BYR_CGRAS_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_WBGDNG_PIXEL_ORDER, BYRP_F_BYR_WBGDNG_PIXEL_ORDER, pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATPRE_PIXEL_ORDER, BYRP_F_BYR_THSTATPRE_PIXEL_ORDER,
		pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_RGBYHIST_PIXEL_ORDER, BYRP_F_BYR_RGBYHIST_PIXEL_ORDER,
		pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATAE_PIXEL_ORDER, BYRP_F_BYR_THSTATAE_PIXEL_ORDER,
		pixel_order);
	BYRP_SET_F(base, BYRP_R_BYR_THSTATAWB_PIXEL_ORDER, BYRP_F_BYR_THSTATAWB_PIXEL_ORDER,
		pixel_order);
}

static void byrp_hw_s_mono_mode(struct pablo_mmio *base, bool enable)
{
	BYRP_SET_F(base, BYRP_R_BYR_BPCDIRDETECTOR_MONO_MODE_EN,
		BYRP_F_BYR_BPCDIRDETECTOR_MONO_MODE_EN, enable);
	BYRP_SET_F(
		base, BYRP_R_BYR_RGBYHIST_MONO_MODE_EN, BYRP_F_BYR_RGBYHIST_MONO_MODE_EN, enable);
}

void byrp_hw_s_bcrop_size(
	struct pablo_mmio *base, u32 bcrop_num, u32 x, u32 y, u32 width, u32 height)
{
	switch (bcrop_num) {
	case BYRP_BCROP_BYR:
		dbg_hw(1, "[API][%s] BYRP_BCROP_BYR -> x(%d), y(%d), w(%d), h(%d)\n", __func__, x,
			y, width, height);
		BYRP_SET_F(base, BYRP_R_BYR_CROPIN_BYPASS, BYRP_F_BYR_CROPIN_BYPASS, 0x0);
		BYRP_SET_F(base, BYRP_R_BYR_CROPIN_START_X, BYRP_F_BYR_CROPIN_START_X, x);
		BYRP_SET_F(base, BYRP_R_BYR_CROPIN_START_Y, BYRP_F_BYR_CROPIN_START_Y, y);
		BYRP_SET_F(base, BYRP_R_BYR_CROPIN_SIZE_X, BYRP_F_BYR_CROPIN_SIZE_X, width);
		BYRP_SET_F(base, BYRP_R_BYR_CROPIN_SIZE_Y, BYRP_F_BYR_CROPIN_SIZE_Y, height);
		BYRP_SET_F(base, BYRP_R_IMAGE_RESOLUTION_CROP, BYRP_F_IMG_WIDTH_CROP, width);
		BYRP_SET_F(base, BYRP_R_IMAGE_RESOLUTION_CROP, BYRP_F_IMG_HEIGHT_CROP, height);
		break;
	case BYRP_BCROP_DNG:
		dbg_hw(1, "[API][%s] BYRP_BCROP_DNG -> x(%d), y(%d), w(%d), h(%d)\n", __func__, x,
			y, width, height);
		BYRP_SET_F(base, BYRP_R_BYR_CROPDNG_BYPASS, BYRP_F_BYR_CROPDNG_BYPASS, 0x0);
		BYRP_SET_F(base, BYRP_R_BYR_CROPDNG_START_X, BYRP_F_BYR_CROPDNG_START_X, x);
		BYRP_SET_F(base, BYRP_R_BYR_CROPDNG_START_Y, BYRP_F_BYR_CROPDNG_START_Y, y);
		BYRP_SET_F(base, BYRP_R_BYR_CROPDNG_SIZE_X, BYRP_F_BYR_CROPDNG_SIZE_X, width);
		BYRP_SET_F(base, BYRP_R_BYR_CROPDNG_SIZE_Y, BYRP_F_BYR_CROPDNG_SIZE_Y, height);
		BYRP_SET_F(base, BYRP_R_IMAGE_RESOLUTION_CROPDNG, BYRP_F_IMG_WIDTH_CROPDNG, width);
		BYRP_SET_F(
			base, BYRP_R_IMAGE_RESOLUTION_CROPDNG, BYRP_F_IMG_HEIGHT_CROPDNG, height);
		break;
	default:
		err_hw("[BYRP] invalid bcrop number[%d]", bcrop_num);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_bcrop_size);

void byrp_hw_s_mcb_size(struct pablo_mmio *base, u32 width, u32 height)
{
	BYRP_SET_F(base, BYRP_R_IMAGE_RESOLUTION_SMCB0, BYRP_F_IMG_WIDTH_SMCB0, width);
	BYRP_SET_F(base, BYRP_R_IMAGE_RESOLUTION_SMCB0, BYRP_F_IMG_HEIGHT_SMCB0, height);
	BYRP_SET_F(base, BYRP_R_IMAGE_RESOLUTION_SMCB1, BYRP_F_IMG_WIDTH_SMCB1, width);
	BYRP_SET_F(base, BYRP_R_IMAGE_RESOLUTION_SMCB1, BYRP_F_IMG_HEIGHT_SMCB1, height);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_mcb_size);

void byrp_hw_s_grid_cfg(struct pablo_mmio *base, struct byrp_grid_cfg *cfg)
{
	u32 val;

	val = 0;
	val = BYRP_SET_V(base, val, BYRP_F_BYR_CGRAS_BINNING_X, cfg->binning_x);
	val = BYRP_SET_V(base, val, BYRP_F_BYR_CGRAS_BINNING_Y, cfg->binning_y);
	BYRP_SET_R(base, BYRP_R_BYR_CGRAS_BINNING_X, val);

	BYRP_SET_F(base, BYRP_R_BYR_CGRAS_CROP_START_X, BYRP_F_BYR_CGRAS_CROP_START_X, cfg->crop_x);
	BYRP_SET_F(base, BYRP_R_BYR_CGRAS_CROP_START_Y, BYRP_F_BYR_CGRAS_CROP_START_Y, cfg->crop_y);

	val = 0;
	val = BYRP_SET_V(base, val, BYRP_F_BYR_CGRAS_CROP_RADIAL_X, cfg->crop_radial_x);
	val = BYRP_SET_V(base, val, BYRP_F_BYR_CGRAS_CROP_RADIAL_Y, cfg->crop_radial_y);
	BYRP_SET_R(base, BYRP_R_BYR_CGRAS_CROP_START_REAL, val);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_grid_cfg);

void byrp_hw_s_disparity_size(struct pablo_mmio *base, struct is_hw_size_config *size_config)
{
	u32 binning_x, binning_y;

	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_SENSOR_WIDTH, BYRP_F_BYR_DISPARITY_SENSOR_WIDTH,
		size_config->sensor_calibrated_width);
	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_SENSOR_HEIGHT, BYRP_F_BYR_DISPARITY_SENSOR_HEIGHT,
		size_config->sensor_calibrated_height);
	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_CROP_X, BYRP_F_BYR_DISPARITY_CROP_X,
		size_config->sensor_crop_x);
	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_CROP_Y, BYRP_F_BYR_DISPARITY_CROP_Y,
		size_config->sensor_crop_y);

	if (size_config->sensor_binning_x < 2000)
		binning_x = 0;
	else if (size_config->sensor_binning_x < 4000)
		binning_x = 1;
	else if (size_config->sensor_binning_x < 8000)
		binning_x = 2;
	else
		binning_x = 3;

	if (size_config->sensor_binning_y < 2000)
		binning_y = 0;
	else if (size_config->sensor_binning_y < 4000)
		binning_y = 1;
	else if (size_config->sensor_binning_y < 8000)
		binning_y = 2;
	else
		binning_y = 3;

	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_BINNING, BYRP_F_BYR_DISPARITY_BINNING_X, binning_x);
	BYRP_SET_F(base, BYRP_R_BYR_DISPARITY_BINNING, BYRP_F_BYR_DISPARITY_BINNING_Y, binning_y);
}

void byrp_hw_s_chain_size(struct pablo_mmio *base, u32 width, u32 height)
{
	u32 val = 0;

	val = BYRP_SET_V(base, val, BYRP_F_GLOBAL_IMG_WIDTH, width);
	val = BYRP_SET_V(base, val, BYRP_F_GLOBAL_IMG_HEIGHT, height);
	BYRP_SET_R(base, BYRP_R_GLOBAL_IMAGE_RESOLUTION, val);

	val = BYRP_SET_V(base, val, BYRP_F_IMG_WIDTH_CROP, width);
	val = BYRP_SET_V(base, val, BYRP_F_IMG_HEIGHT_CROP, height);
	BYRP_SET_R(base, BYRP_R_IMAGE_RESOLUTION_CROP, val);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_chain_size);

void byrp_hw_g_chain_size(struct pablo_mmio *base, u32 set_id, u32 *width, u32 *height)
{
	*width = BYRP_GET_F(base, BYRP_R_GLOBAL_IMAGE_RESOLUTION, BYRP_F_GLOBAL_IMG_WIDTH);
	*height = BYRP_GET_F(base, BYRP_R_GLOBAL_IMAGE_RESOLUTION, BYRP_F_GLOBAL_IMG_HEIGHT);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_chain_size);

void byrp_hw_s_core(struct pablo_mmio *base, u32 num_buffers, struct byrp_param_set *param_set)
{
	bool mono_mode_en = 0;
	u32 pixel_order;
	u32 seed;

	if (param_set->otf_input.cmd)
		pixel_order = param_set->otf_input.order;
	else if (param_set->dma_input.cmd)
		pixel_order = param_set->dma_input.order;
	else
		pixel_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	byrp_hw_s_pixel_order(base, pixel_order);
	byrp_hw_s_mono_mode(base, mono_mode_en);
	byrp_hw_s_fro(base, num_buffers);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		byrp_hw_s_block_crc(base, seed);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_core);

u32 byrp_hw_g_rdma_max_cnt(void)
{
	return BYRP_RDMA_MAX;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_rdma_max_cnt);

u32 byrp_hw_g_wdma_max_cnt(void)
{
	return BYRP_WDMA_MAX;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_wdma_max_cnt);

u32 byrp_hw_g_reg_cnt(void)
{
	return BYRP_REG_CNT + BYRP_LUT_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_reg_cnt);

u32 byrp_hw_g_rdma_cfg_max_cnt(void)
{
	return BYRP_RDMA_CFG_MAX;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_rdma_cfg_max_cnt);

u32 byrp_hw_g_wdma_cfg_max_cnt(void)
{
	return BYRP_WDMA_CFG_MAX;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_wdma_cfg_max_cnt);

void byrp_hw_s_internal_shot(struct byrp_param_set *dst)
{
	dst->dma_output_byr.cmd = DMA_OUTPUT_COMMAND_DISABLE;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_internal_shot);

void byrp_hw_s_external_shot(
	struct is_param_region *param_region, IS_DECLARE_PMAP(pmap), struct byrp_param_set *dst)
{
	struct byrp_param *src = &param_region->byrp;

	if (test_bit(PARAM_SENSOR_CONFIG, pmap))
		memcpy(&dst->sensor_config, &param_region->sensor.config,
			sizeof(struct param_sensor_config));

	if (test_bit(PARAM_BYRP_CONTROL, pmap))
		memcpy(&dst->control, &src->control, sizeof(struct param_control));

	/* check input */
	if (test_bit(PARAM_BYRP_OTF_INPUT, pmap))
		memcpy(&dst->otf_input, &src->otf_input, sizeof(struct param_otf_input));

	if (test_bit(PARAM_BYRP_DMA_INPUT, pmap))
		memcpy(&dst->dma_input, &src->dma_input, sizeof(struct param_dma_input));

	/* check output */
	if (test_bit(PARAM_BYRP_OTF_OUTPUT, pmap))
		memcpy(&dst->otf_output, &src->otf_output, sizeof(struct param_otf_output));

	if (test_bit(PARAM_BYRP_BYR, pmap))
		memcpy(&dst->dma_output_byr, &src->dma_output_byr, sizeof(struct param_dma_output));
}
KUNIT_EXPORT_SYMBOL(byrp_hw_s_external_shot);

int byrp_hw_g_rdma_param_ptr(u32 id, struct is_frame *dma_frame, struct byrp_param_set *param_set,
	char *name, dma_addr_t **dma_frame_dva, struct param_dma_input **pdi,
	pdma_addr_t **param_set_dva)
{
	int ret = 0;

	switch (id) {
	case BYRP_RDMA_CFG_IMG:
		*dma_frame_dva = dma_frame->dvaddr_buffer;
		*pdi = &param_set->dma_input;
		*param_set_dva = param_set->input_dva;
		sprintf(name, "byrp_img");
		break;
	default:
		ret = -EINVAL;
		err_hw("[BYRP] invalid rdma param id[%d]", id);
		break;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_rdma_param_ptr);

int byrp_hw_g_wdma_param_ptr(u32 id, struct is_frame *dma_frame, struct byrp_param_set *param_set,
	char *name, dma_addr_t **dma_frame_dva, struct param_dma_output **pdo,
	pdma_addr_t **param_set_dva)
{
	int ret = 0;

	switch (id) {
	case BYRP_WDMA_CFG_BYR:
		*dma_frame_dva = dma_frame->dva_byrp_byr;
		*pdo = &param_set->dma_output_byr;
		*param_set_dva = param_set->output_dva_byr;
		sprintf(name, "byrp_wdma_befor_wbg");
		break;
	default:
		ret = -EINVAL;
		err_hw("[BYRP] invalid wdma param id[%d]", id);
		break;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_wdma_param_ptr);

void byrp_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->rd_table = &byrp_rd_ranges_table;
	cfg->volatile_table = &byrp_volatile_table;
	cfg->wr_noinc_table = &byrp_wr_noinc_table;

	cfg->max_register = BYRP_R_BYR_THSTATAWB_CRC;
	cfg->num_reg_defaults_raw = (BYRP_R_BYR_THSTATAWB_CRC >> 2) + 1;
	cfg->dma_addr_shift = 4;

	cfg->ranges = byrp_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(byrp_range_cfgs);

	cfg->fields = byrp_field_descs;
	cfg->num_fields = ARRAY_SIZE(byrp_field_descs);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_init_pmio_config);

void byrp_hw_g_binning_size(struct pablo_mmio *base, u32 *binning_x, u32 *binning_y)
{
	*binning_x = BYRP_GET_F(base, BYRP_R_BYR_CGRAS_BINNING_X, BYRP_F_BYR_CGRAS_BINNING_X);
	*binning_y = BYRP_GET_F(base, BYRP_R_BYR_CGRAS_BINNING_X, BYRP_F_BYR_CGRAS_BINNING_Y);
}
KUNIT_EXPORT_SYMBOL(byrp_hw_g_binning_size);
