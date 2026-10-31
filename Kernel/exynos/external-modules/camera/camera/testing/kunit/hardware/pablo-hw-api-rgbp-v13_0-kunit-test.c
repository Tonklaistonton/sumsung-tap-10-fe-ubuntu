// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-type.h"
#include "pmio.h"
#include "is-hw-common-dma.h"
#include "hardware/api/pablo-hw-api-rgbp.h"
#include "hardware/sfr/pablo-sfr-rgbp-v13_0.h"

#define CALL_HWAPI_OPS(op, args...) test_ctx.ops->op(args)

/* PMIO MACRO */
#define SET_CR(base, R, val) PMIO_SET_R(base, R, val)
#define SET_CR_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define SET_CR_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)

#define GET_CR(base, R) PMIO_GET_R(base, R)
#define GET_CR_F(base, R, F) PMIO_GET_F(base, R, F)

/* LOG MACRO */
#define HW_NAME "RGBP"
#define rgbp_info(fmt, args...) info_hw("[%s]" fmt "\n", HW_NAME, ##args)
#define rgbp_dbg(level, fmt, args...) dbg_hw(level, "[%s]" fmt "\n", HW_NAME, ##args)
#define rgbp_warn(fmt, args...) warn_hw("[%s]" fmt, HW_NAME, ##args)
#define rgbp_err(fmt, args...) err_hw("[%s]" fmt, HW_NAME, ##args)

static struct pablo_hw_rgbp_api_kunit_test_ctx {
	void *base;
	const struct rgbp_hw_ops *ops;
	struct pmio_config pmio_config;
	struct pablo_mmio *pmio;
	struct pmio_field *pmio_fields;
	struct rgbp_param_set p_set;
	struct is_rgbp_config cfg;
} test_ctx;

#define RGBP_LIC_CH_CNT 4
#define LIC_HBLALK_CYCLE 50

enum byrp_lic_bit_mode {
	BAYER = 1,
	RGB = 3,
};

static void pablo_hw_api_rgbp_hw_reset_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(reset, pmio);
}

static void pablo_hw_api_rgbp_hw_init_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio = test_ctx.pmio;
	u32 lic_ch = __LINE__ % RGBP_LIC_CH_CNT;

	SET_CR(pmio, RGBP_R_BYR_CINFIFO_ENABLE, 0);
	SET_CR(pmio, RGBP_R_RGB_COUTFIFO_ENABLE, 0);

	CALL_HWAPI_OPS(init, pmio, lic_ch);

	/* s_otf */
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_BYR_CINFIFO_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_RGB_COUTFIFO_ENABLE), 1);

	/* s_cloader */
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_STAT_RDMACL_EN), 1);

	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_ALLOC_SR_ENABLE), 1);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_ALLOC_SR_GRP_0TO3, RGBP_F_ALLOC_SR_GRP0), lic_ch);
}

static void _rgbp_hw_s_chain_kunit_test(
	struct kunit *test, struct pablo_mmio *pmio, struct rgbp_param_set *param_set)
{
	struct is_crop in, out;

	in.w = param_set->otf_input.width;
	in.h = param_set->otf_input.height;

	out.w = param_set->otf_output.width;
	out.h = param_set->otf_output.height;

	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_SRC_IMG_SIZE, RGBP_F_CHAIN_SRC_IMG_WIDTH), in.w);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_SRC_IMG_SIZE, RGBP_F_CHAIN_SRC_IMG_HEIGHT), in.h);

	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_MCB_IMG_SIZE, RGBP_F_CHAIN_MCB_IMG_WIDTH), in.w);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_MCB_IMG_SIZE, RGBP_F_CHAIN_MCB_IMG_HEIGHT), in.h);

	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_DST_IMG_SIZE, RGBP_F_CHAIN_DST_IMG_WIDTH), out.w);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_CHAIN_DST_IMG_SIZE, RGBP_F_CHAIN_DST_IMG_HEIGHT), out.h);

	/* lic cfg */
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_MODE, RGBP_F_LIC_BYPASS), 0);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_MODE, RGBP_F_LIC_DEBUG_ON), 1);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_MODE, RGBP_F_LIC_FAKE_GEN_ON), 0);

	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_BLANK, RGBP_F_LIC_IN_HBLANK_CYCLE),
		LIC_HBLALK_CYCLE);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_BLANK, RGBP_F_LIC_OUT_HBLANK_CYCLE),
		LIC_HBLALK_CYCLE);
}

static void pablo_hw_api_rgbp_hw_s_core_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct rgbp_param_set *p_set;
	struct is_rgbp_config *cfg;
	struct is_crop otf = { 0, 0, 1440, 1080 };
	struct is_crop dma = { 0, 0, 640, 480 };
	u32 lic_bit_mode[] = { BAYER, RGB };
	u32 rdma_en, pixel_order, bcrop_ofs_x, bcrop_ofs_y, bcrop_w, bcrop_h;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;
	cfg = &test_ctx.cfg;

	p_set->otf_input.width = p_set->otf_output.width = otf.w;
	p_set->otf_input.height = p_set->otf_output.height = otf.h;
	pixel_order = p_set->otf_input.order = 1;

	bcrop_ofs_x = p_set->otf_input.bayer_crop_offset_x = __LINE__;
	bcrop_ofs_y = p_set->otf_input.bayer_crop_offset_y = __LINE__;
	bcrop_w = p_set->otf_input.bayer_crop_width = __LINE__;
	bcrop_h = p_set->otf_input.bayer_crop_height = __LINE__;

	p_set->dma_output_sat.width = dma.w;
	p_set->dma_output_sat.height = dma.h;

	rdma_en = p_set->dma_input.cmd = 0;

	/* TC#1. RGBP Chain configuration. */
	CALL_HWAPI_OPS(s_core, pmio, p_set, cfg);

	_rgbp_hw_s_chain_kunit_test(test, pmio, p_set);

	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_CONFIG_0, RGBP_F_LIC_BIT_MODE),
		lic_bit_mode[rdma_en]);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_CONFIG_0, RGBP_F_LIC_RDMA_EN), rdma_en);

	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_TETRA_TDMSC_TETRA_PIXEL_ORDER), pixel_order);

	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_RGB_DMSCCROP_BYPASS), 0);

	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_RGB_DMSCCROP_START_X), bcrop_ofs_x);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_RGB_DMSCCROP_START_Y), bcrop_ofs_y);

	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_RGB_DMSCCROP_SIZE_X), bcrop_w);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_RGB_DMSCCROP_SIZE_Y), bcrop_h);

	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_CHAIN_DS_SAT_IMG_SIZE, RGBP_F_CHAIN_DS_SAT_IMG_WIDTH), dma.w);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_CHAIN_DS_SAT_IMG_SIZE, RGBP_F_CHAIN_DS_SAT_IMG_HEIGHT),
		dma.h);

	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_YUV_DSSAT_OUTPUT_SIZE, RGBP_F_YUV_DSSAT_OUTPUT_W), dma.w);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_YUV_DSSAT_OUTPUT_SIZE, RGBP_F_YUV_DSSAT_OUTPUT_H), dma.h);

	rdma_en = p_set->dma_input.cmd = 1;

	CALL_HWAPI_OPS(s_core, pmio, p_set, cfg);

	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_CONFIG_0, RGBP_F_LIC_BIT_MODE),
		lic_bit_mode[rdma_en]);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_LIC_INPUT_CONFIG_0, RGBP_F_LIC_RDMA_EN), rdma_en);
}

static void pablo_hw_api_rgbp_hw_s_path_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct rgbp_param_set *p_set;
	struct pablo_common_ctrl_frame_cfg frame_cfg;
	struct is_rgbp_config *cfg;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;
	cfg = &test_ctx.cfg;

	/* TC#1. Enable every path. */
	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));
	p_set->otf_input.cmd = 1;
	p_set->otf_output.cmd = 1;

	cfg->rgbyhist_hdr_bypass = 1;
	cfg->thstat_awb_hdr_bypass = 1;
	cfg->drcclct_bypass = 1;
	p_set->dma_output_sat.cmd = 1;

	CALL_HWAPI_OPS(s_path, pmio, p_set, &frame_cfg, cfg);

	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_in_en, 0x1);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_out_en, 0x1);

	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_MCB_ENABLE), 0);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_BLC_ENABLE), 2);

	/* TC#2. Disable every path. */
	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));
	p_set->otf_input.cmd = 0;
	p_set->otf_output.cmd = 0;

	cfg->rgbyhist_hdr_bypass = 0;
	cfg->thstat_awb_hdr_bypass = 0;
	cfg->drcclct_bypass = 0;
	p_set->dma_output_sat.cmd = 0;

	CALL_HWAPI_OPS(s_path, pmio, p_set, &frame_cfg, cfg);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_in_en, 0x0);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_out_en, 0x0);

	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_MCB_ENABLE), 3);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_BLC_ENABLE), 1);
}

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

static void pablo_hw_api_rgbp_hw_s_lbctrl_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int lic_ch;
	struct rgbp_line_buffer *lb;

	if (exynos_soc_info.main_rev >= 1 &&
		exynos_soc_info.sub_rev >= 1)
		lb = lb_offset;
	else
		lb = lb_offset_evt0;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(s_lbctrl, pmio);

	for (lic_ch = 0; lic_ch < RGBP_LIC_CH_CNT; lic_ch++) {
		KUNIT_EXPECT_EQ(test,
			GET_CR_F(pmio, RGBP_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x10 * lic_ch),
				RGBP_F_CHAIN_LBCTRL_OFFSET_GRP0_C0 + (lic_ch * 3)),
			lb[0].offset[lic_ch]);
		KUNIT_EXPECT_EQ(test,
			GET_CR_F(pmio, RGBP_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x10 * lic_ch),
				RGBP_F_CHAIN_LBCTRL_OFFSET_GRP1_C0 + (lic_ch * 3)),
			lb[1].offset[lic_ch]);
		KUNIT_EXPECT_EQ(test,
			GET_CR_F(pmio, RGBP_R_CHAIN_LBCTRL_OFFSET_GRP2TO3_C0 + (0x10 * lic_ch),
				RGBP_F_CHAIN_LBCTRL_OFFSET_GRP2_C0 + (lic_ch * 3)),
			lb[2].offset[lic_ch]);
	}
}

static void pablo_hw_api_rgbp_hw_g_int_en_kunit_test(struct kunit *test)
{
	u32 int_en[PCC_INT_ID_NUM] = { 0 };

	CALL_HWAPI_OPS(g_int_en, int_en);

	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_0], INT0_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_1], INT1_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_CMDQ_INT], 0);
	KUNIT_EXPECT_EQ(test, int_en[PCC_COREX_INT], 0);
}

#define RGBP_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
static void pablo_hw_api_rgbp_hw_g_int_grp_en_kunit_test(struct kunit *test)
{
	u32 int_grp_en;

	int_grp_en = CALL_HWAPI_OPS(g_int_grp_en);

	KUNIT_EXPECT_EQ(test, int_grp_en, RGBP_INT_GRP_EN_MASK);
}

static void pablo_hw_api_rgbp_hw_s_int_on_col_row_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int col, row;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(s_int_on_col_row, pmio, false, 0, 0);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_COL_EN), 0);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_ROW_EN), 0);

	col = __LINE__;
	row = __LINE__;
	CALL_HWAPI_OPS(s_int_on_col_row, pmio, true, col, row);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW_POS, RGBP_F_IP_INT_COL_POS), col);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW_POS, RGBP_F_IP_INT_ROW_POS), row);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_SRC_SEL), 0);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_COL_EN), 1);
	KUNIT_EXPECT_EQ(test, GET_CR_F(pmio, RGBP_R_IP_INT_ON_COL_ROW, RGBP_F_IP_INT_ROW_EN), 1);
}

static void pablo_hw_api_rgbp_hw_wait_idle_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int ret;

	pmio = test_ctx.pmio;

	/* TC#1. Timeout to wait idleness. */
	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	/* TC#2. Succeed to wait idleness. */
	SET_CR(pmio, RGBP_R_IDLENESS_STATUS, 1);

	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_rgbp_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_CR);
	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_DBG_STATE);
	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_MODE_NUM);
}

static void pablo_hw_api_rgbp_hw_is_occurred_kunit_test(struct kunit *test)
{
	bool occur;
	u32 status;
	ulong type;

	/* TC#1. No interrupt. */
	status = 0;
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	/* TC#2. Test each interrupt. */
	status = BIT_MASK(INTR0_RGBP_FRAME_START_INT);
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_RGBP_ROW_COL_INT);
	type = BIT_MASK(INT_FRAME_ROW);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_RGBP_FRAME_END_INT);
	type = BIT_MASK(INT_FRAME_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_RGBP_COREX_END_INT_0);
	type = BIT_MASK(INT_COREX_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_RGBP_SETTING_DONE_INT);
	type = BIT_MASK(INT_SETTING_DONE);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_RGBP_CMDQ_ERROR_INT);
	type = BIT_MASK(INT_ERR0);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR1_RGBP_LIC_DBG_CNT_ERR);
	type = BIT_MASK(INT_ERR1);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	/* TC#3. Test interrupt ovarlapping. */
	status = BIT_MASK(INTR0_RGBP_FRAME_START_INT);
	type = BIT_MASK(INT_FRAME_START) | BIT_MASK(INT_FRAME_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	status = BIT_MASK(INTR0_RGBP_FRAME_START_INT) | BIT_MASK(INTR0_RGBP_FRAME_END_INT);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);
}

static void pablo_hw_api_rgbp_hw_s_strgen_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(s_strgen, pmio);

	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_STRGEN_MODE_EN), 1);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_STRGEN_MODE_DATA_TYPE),
		1);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_STRGEN_MODE_DATA),
		255);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_IP_USE_CINFIFO_NEW_FRAME_IN), PCC_ASAP);
}

static void pablo_hw_api_rgbp_hw_s_dtp_kunit_test(struct kunit *test)
{
	struct rgbp_param_set *p_set;
	struct pablo_mmio *pmio;
	u32 w, h;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;

	w = __LINE__;
	h = __LINE__;

	p_set->otf_input.width = w;
	p_set->otf_input.height = h;

	CALL_HWAPI_OPS(s_dtp, pmio, p_set, false);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_BYR_DTP_X_OUTPUT_SIZE, RGBP_F_BYR_DTP_X_OUTPUT_SIZE), w);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, RGBP_R_BYR_DTP_Y_OUTPUT_SIZE, RGBP_F_BYR_DTP_Y_OUTPUT_SIZE), h);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_BYR_DTP_TEST_PATTERN_MODE), 0);

	CALL_HWAPI_OPS(s_dtp, pmio, p_set, true);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_BYR_DTP_TEST_PATTERN_MODE), 2);
}

static void pablo_hw_api_rgbp_hw_s_bypass_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(s_bypass, pmio);

	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_BYR_CGRAS_BYPASS_REG, RGBP_F_BYR_CGRAS_BYPASS), 1);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, RGBP_R_RGB_DRCCLCT_BYPASS, RGBP_F_RGB_DRCCLCT_BYPASS), 1);
}

static void pablo_hw_api_rgbp_hw_clr_cotf_err_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio = test_ctx.pmio;
	const u32 val = __LINE__;

	SET_CR(pmio, RGBP_R_BYR_CINFIFO_INT, val);
	SET_CR(pmio, RGBP_R_RGB_COUTFIFO_INT, val);

	CALL_HWAPI_OPS(clr_cotf_err, pmio);

	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_BYR_CINFIFO_INT_CLEAR), val);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, RGBP_R_RGB_COUTFIFO_INT_CLEAR), val);
}

static struct kunit_case pablo_hw_api_rgbp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_rgbp_hw_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_lbctrl_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int_grp_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_int_on_col_row_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_dtp_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_clr_cotf_err_kunit_test),
	{},
};

static struct pablo_mmio *pablo_hw_api_rgbp_pmio_init(void)
{
	struct pmio_config *pcfg;
	struct pablo_mmio *pmio;

	pcfg = &test_ctx.pmio_config;

	pcfg->name = "RGBP";
	pcfg->mmio_base = test_ctx.base;
	pcfg->cache_type = PMIO_CACHE_NONE;

	rgbp_hw_g_pmio_cfg(pcfg);

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	if (pmio_field_bulk_alloc(pmio, &test_ctx.pmio_fields, pcfg->fields, pcfg->num_fields))
		goto err_field_bulk_alloc;

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
err_init:
	return NULL;
}

static int pablo_hw_api_rgbp_kunit_test_init(struct kunit *test)
{
	memset(&test_ctx, 0, sizeof(test_ctx));

	test_ctx.base = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.base);

	test_ctx.ops = rgbp_hw_g_ops();

	test_ctx.pmio = pablo_hw_api_rgbp_pmio_init();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.pmio);

	return 0;
}

static void pablo_hw_api_rgbp_pmio_deinit(void)
{
	if (test_ctx.pmio_fields) {
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		test_ctx.pmio_fields = NULL;
	}

	if (test_ctx.pmio) {
		pmio_exit(test_ctx.pmio);
		test_ctx.pmio = NULL;
	}
}

static void pablo_hw_api_rgbp_kunit_test_exit(struct kunit *test)
{
	pablo_hw_api_rgbp_pmio_deinit();
	kunit_kfree(test, test_ctx.base);

	memset(&test_ctx, 0, sizeof(test_ctx));
}

struct kunit_suite pablo_hw_api_rgbp_kunit_test_suite = {
	.name = "pablo-hw-api-rgbp-v13_0-kunit-test",
	.init = pablo_hw_api_rgbp_kunit_test_init,
	.exit = pablo_hw_api_rgbp_kunit_test_exit,
	.test_cases = pablo_hw_api_rgbp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_rgbp_kunit_test_suite);

MODULE_LICENSE("GPL");
