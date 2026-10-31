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
#include "lme/pablo-lme.h"
#include "lme/pablo-hw-api-lme.h"
#include "lme/pablo-hw-reg-lme-v13_0_0.h"
#include "is-common-enum.h"
#include "is-type.h"
#include "is-hw-common-dma.h"

#define LME_SET_R(base, R, val) PMIO_SET_R((base)->pmio, R, val)

#define LME_GET_F(base, R, F) PMIO_GET_F((base)->pmio, R, F)
#define LME_GET_R(base, R) PMIO_GET_R((base)->pmio, R)

#define LME_MSB(x) ((x) >> 32)
#define LME_LSB(x) ((x) & 0xFFFFFFFF)

static struct camerapp_hw_api_lme_kunit_test_ctx {
	void *addr;
	u32 width, height, stride;
	struct lme_dev *lme;
	struct resource rsc;
} test_ctx;

const struct lme_variant lme_variant = {
	.limit_input = {
		.min_w		= 90,
		.min_h		= 60,
		.max_w		= 1664,
		.max_h		= 1248,
	},
	.version		= 0x13000000,
};

static const struct lme_fmt lme_formats[] = {
	{
		.name = "GREY",
		.pixelformat = V4L2_PIX_FMT_GREY,
		.cfg_val = LME_CFG_FMT_GREY,
		.bitperpixel = { 8 },
		.num_planes = 1,
		.num_comp = 1,
		.h_shift = 1,
		.v_shift = 1,
	},
};

/* Define the test cases. */
static void camerapp_hw_api_lme_get_size_constraints_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	const struct lme_variant *constraints;
	u32 val;

	LME_SET_R(lme, LME_R_IP_VERSION, lme_variant.version);

	constraints = camerapp_hw_lme_get_size_constraints(lme->pmio);
	lme->variant = constraints;
	val = camerapp_hw_lme_get_ver(lme->pmio);

	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_w, lme_variant.limit_input.min_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_h, lme_variant.limit_input.min_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_w, lme_variant.limit_input.max_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_h, lme_variant.limit_input.max_h);
	KUNIT_EXPECT_EQ(test, constraints->version, lme_variant.version);
	KUNIT_EXPECT_EQ(test, constraints->version, val);
}

static void camerapp_hw_api_lme_sfr_dump_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;

	camerapp_lme_sfr_dump(lme->regs_base);
}

static void camerapp_hw_api_lme_start_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	struct c_loader_buffer clb;
	u32 val;

	/* APB-DIRECT */
	camerapp_hw_lme_start(lme->pmio, &clb);

	val = LME_GET_F(lme, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)3);
	val = LME_GET_R(lme, LME_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_R(lme, LME_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	/* C-LOADER */
	clb.header_dva = 0xDEADDEAD;
	clb.num_of_headers = 5;
	camerapp_hw_lme_start(lme->pmio, &clb);

	val = LME_GET_R(lme, LME_R_CMDQ_QUE_CMD_H);
	KUNIT_EXPECT_EQ(test, val, (u32)(clb.header_dva >> 4));
	val = LME_GET_R(lme, LME_R_CMDQ_QUE_CMD_M);
	KUNIT_EXPECT_EQ(test, val, (u32)((1 << 12) | clb.num_of_headers));
	val = LME_GET_F(lme, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_R(lme, LME_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_R(lme, LME_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_api_lme_stop_kunit_test(struct kunit *test)
{
}

static void camerapp_hw_api_lme_sw_reset_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 val;

	val = camerapp_hw_lme_dma_reset(lme->pmio);
	KUNIT_EXPECT_EQ(test, val, (u32)20001);
	val = LME_GET_R(lme, LME_R_TRANS_STOP_REQ);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	/* in order to check next function */
	LME_SET_R(lme, LME_R_TRANS_STOP_REQ_RDY, 1);

	val = camerapp_hw_lme_sw_reset(lme->pmio);
	KUNIT_EXPECT_EQ(test, val, (u32)20001);
	val = LME_GET_R(lme, LME_R_SW_RESET);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_lme_get_intr_status_and_clear_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 val0, c0, c1;
	u32 int0 = 0xAAAABBBB;
	u32 int1 = 0xCCCCDDDD;

	LME_SET_R(lme, LME_R_INT_REQ_INT0, int0);
	LME_SET_R(lme, LME_R_INT_REQ_INT1, int1);

	val0 = camerapp_hw_lme_get_intr_status_and_clear(lme->pmio);

	c0 = LME_GET_R(lme, LME_R_INT_REQ_INT0_CLEAR);
	c1 = LME_GET_R(lme, LME_R_INT_REQ_INT1_CLEAR);

	KUNIT_EXPECT_EQ(test, val0, int0);
	KUNIT_EXPECT_EQ(test, c0, int0);
	KUNIT_EXPECT_EQ(test, c1, int1);
}

static void camerapp_hw_lme_get_fs_fe_kunit_test(struct kunit *test)
{
	u32 fs = LME_INT_FRAME_START, fe = LME_INT_FRAME_END;
	u32 val1, val2;

	val1 = camerapp_hw_lme_get_int_frame_start();
	val2 = camerapp_hw_lme_get_int_frame_end();

	KUNIT_EXPECT_EQ(test, fs, val1);
	KUNIT_EXPECT_EQ(test, fe, val2);
}

static void camerapp_hw_lme_get_output_size_kunit_test(struct kunit *test)
{
	u32 width = 1664, height = 1248;
	u32 val1, val2;
	u32 ref1, ref2;

	ref1 = 832;
	ref2 = 312;
	camerapp_hw_lme_get_output_size(width, height, &val1, &val2, LME_SPS_8X4, LME_WDMA_MV_OUT);
	KUNIT_EXPECT_EQ(test, ref1, val1);
	KUNIT_EXPECT_EQ(test, ref2, val2);

	ref1 = 624;
	ref2 = 312;
	camerapp_hw_lme_get_output_size(width, height, &val1, &val2, LME_SPS_8X4, LME_WDMA_SAD_OUT);
	KUNIT_EXPECT_EQ(test, ref1, val1);
	KUNIT_EXPECT_EQ(test, ref2, val2);

	ref1 = 3328;
	ref2 = 624;
	camerapp_hw_lme_get_output_size(width, height, &val1, &val2, LME_SPS_2X2, LME_WDMA_MV_OUT);
	KUNIT_EXPECT_EQ(test, ref1, val1);
	KUNIT_EXPECT_EQ(test, ref2, val2);

	ref1 = 2496;
	ref2 = 624;
	camerapp_hw_lme_get_output_size(width, height, &val1, &val2, LME_SPS_2X2, LME_WDMA_SAD_OUT);
	KUNIT_EXPECT_EQ(test, ref1, val1);
	KUNIT_EXPECT_EQ(test, ref2, val2);
}

static void camerapp_hw_lme_get_mbmv_size_kunit_test(struct kunit *test)
{
	u32 width = 1664, height = 1248;
	u32 val1 = 208, val2 = 78;

	camerapp_hw_lme_get_mbmv_size(&width, &height);

	KUNIT_EXPECT_EQ(test, width, val1);
	KUNIT_EXPECT_EQ(test, height, val2);
}

static void camerapp_hw_lme_set_initialization_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 val;

	camerapp_hw_lme_set_initialization(lme->pmio);

	val = LME_GET_F(lme, LME_R_CMDQ_VHD_CONTROL, LME_F_CMDQ_VHD_VBLANK_QRUN_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_F(lme, LME_R_CMDQ_VHD_CONTROL, LME_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_F(lme, LME_R_DEBUG_CLOCK_ENABLE, LME_F_DEBUG_CLOCK_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = LME_GET_R(lme, LME_R_C_LOADER_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_R(lme, LME_R_STAT_RDMACL_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	val = LME_GET_F(lme, LME_R_CMDQ_QUE_CMD_L, LME_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0xbf); /* LME_INT_GRP_EN_MASK */
	val = LME_GET_F(lme, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)3);
	val = LME_GET_R(lme, LME_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_R(lme, LME_R_INT_REQ_INT0_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)LME_INT_EN_MASK);
	val = LME_GET_R(lme, LME_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	/* W/A */
	val = LME_GET_R(lme, LME_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = LME_GET_R(lme, LME_R_FORCE_INTERNAL_CLOCK);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_lme_check_block_reg(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 ref;
	u32 val;

	/* camerapp_hw_lme_set_cache */
	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_Y_LME_BYPASS);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_IGNORE_PREFETCH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_DATA_REQ_CNT_EN);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_PRE_REQ_CNT_EN);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0x8;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_CACHE_8BIT_CACHE_CADDR_OFFSET);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0x40;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_PIX_CONFIG_0, LME_F_Y_LME_PRVCMGAIN);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_PIX_CONFIG_0, LME_F_Y_LME_PRVIMGHEIGHT);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0x40;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_PIX_CONFIG_1, LME_F_Y_LME_CURCMGAIN);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_PIX_CONFIG_1, LME_F_Y_LME_CURCMOFFSET);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* camerapp_hw_lme_set_block_bypass */
	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_Y_LME_BYPASS);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_set_mvct(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 ref, val;

	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_LME_CONFIG, LME_F_Y_LME_MODE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_LME_CONFIG, LME_F_Y_LME_FIRSTFRAME);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_LME_CONFIG, LME_F_MVCT_8BIT_LME_FW_FRAME_ONLY);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_CONFIG, LME_F_Y_LME_USEAD);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_CONFIG, LME_F_Y_LME_USESAD);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_CONFIG, LME_F_Y_LME_USECT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_CONFIG, LME_F_Y_LME_USEZSAD);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_WEIGHT, LME_F_Y_LME_WEIGHTCT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 5;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_WEIGHT, LME_F_Y_LME_WEIGHTAD);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_WEIGHT, LME_F_Y_LME_WEIGHTSAD);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_WEIGHT, LME_F_Y_LME_WEIGHTZSAD);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 3;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MVE_WEIGHT, LME_F_Y_LME_NOISELEVEL);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 128;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MV_SR, LME_F_Y_LME_MPSSRCHRANGEX);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 128;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_MV_SR, LME_F_Y_LME_MPSSRCHRANGEY);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_cache_size(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 ref, val;

	ref = test_ctx.width;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_IMAGE0_CONFIG, LME_F_Y_LME_PRVIMGWIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = test_ctx.height;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_IMAGE0_CONFIG, LME_F_Y_LME_PRVIMGHEIGHT);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_CROP_CONFIG_START_0, LME_F_Y_LME_PRVROISTARTX);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_CROP_CONFIG_START_0, LME_F_Y_LME_PRVROISTARTY);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = test_ctx.stride;
	val = LME_GET_R(lme, LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_0);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = test_ctx.width;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_IMAGE1_CONFIG, LME_F_Y_LME_CURIMGWIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = test_ctx.height;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_IMAGE1_CONFIG, LME_F_Y_LME_CURIMGHEIGHT);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_CROP_CONFIG_START_1, LME_F_Y_LME_CURROISTARTX);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_CACHE_8BIT_CROP_CONFIG_START_1, LME_F_Y_LME_CURROISTARTY);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = test_ctx.stride;
	val = LME_GET_R(lme, LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_1);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_mvct_size(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 ref, val;

	ref = 2; /* get from : prefetch_gap = DIV_ROUND_UP(width * 8 / 100, 16) */
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_LME_PREFETCH, LME_F_MVCT_8BIT_LME_PREFETCH_GAP);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_LME_PREFETCH, LME_F_MVCT_8BIT_LME_PREFETCH_EN);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = test_ctx.width;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_IMAGE_DIMENTIONS, LME_F_Y_LME_ROISIZEX);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = test_ctx.height;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_IMAGE_DIMENTIONS, LME_F_Y_LME_ROISIZEY);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_first_frame(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 ref, val;

	ref = 1;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_LME_CONFIG, LME_F_Y_LME_FIRSTFRAME);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_sps_out_mode(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	u32 ref, val;

	ref = 0;
	val = LME_GET_F(lme, LME_R_MVCT_8BIT_LME_CONFIG, LME_F_Y_LME_LME_8X8SEARCH);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_size_regs(struct kunit *test)
{
	camerapp_hw_lme_check_cache_size(test);
	camerapp_hw_lme_check_mvct_size(test);
	camerapp_hw_lme_check_first_frame(test);
	camerapp_hw_lme_check_sps_out_mode(test);
}

static void camerapp_hw_lme_check_rdma_init(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	struct lme_mbmv *mbmv = &lme->current_ctx->mbmv;
	u32 ref, val;

	/* LME_RDMA_CACHE_IN_0 */
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_CACHE_IN_CLIENT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0xf;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_CACHE_IN_GEOM_BURST_LENGTH);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_CACHE_IN_BURST_ALIGNMENT);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 1;
	val = LME_GET_R(lme, LME_R_CACHE_IN_RDMAYIN_EN);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* LME_RDMA_MBMV_IN */
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_CLIENT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_MBMV_IN_RDMAYIN_EN);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0x2;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BURST_LENGTH);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = mbmv->width;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LWIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = mbmv->height;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_COUNT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = mbmv->width_align;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_TOTAL_WIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_DIRECTION_HW);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = mbmv->width;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_LWIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0x14;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_LINEGAP);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PREGAP);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0x14;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_POSTGAP);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PIXELGAP);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0x1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_STALLGAP);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0x8;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PACKING);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BPAD_SET);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BPAD_TYPE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BSHIFT_SET);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_CH_MIX_0);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_CH_MIX_1);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_BURST_ALIGNMENT);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_rdma_addr(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	struct lme_ctx *ctx = lme->current_ctx;
	struct lme_mbmv *mbmv;
	struct lme_frame *d_frame, *s_frame;

	u32 total_width, line_count;
	u32 val, ref;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;
	mbmv = &ctx->mbmv;

	/* LME_RDMA_CACHE_IN_0 */
	ref = DVA_36BIT_LOW(s_frame->addr.curr_in);
	val = LME_GET_R(lme, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_HIGH(s_frame->addr.curr_in);
	val = LME_GET_R(lme, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* LME_RDMA_CACHE_IN_1 */
	ref = DVA_36BIT_LOW(s_frame->addr.prev_in);
	val = LME_GET_R(lme, LME_R_CACHE_8BIT_BASE_ADDR_1P_1_LSB);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = DVA_36BIT_HIGH(s_frame->addr.prev_in);
	val = LME_GET_R(lme, LME_R_CACHE_8BIT_BASE_ADDR_1P_1_MSB);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* LME_RDMA_MBMV_IN */
	ref = 1;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_CONF,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_EN_0);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_CONF,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_EN_1);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_CONF,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_SIZE);
	KUNIT_EXPECT_EQ(test, val, ref);

	total_width = mbmv->width;
	line_count = mbmv->height;

	ref = DVA_36BIT_LOW(mbmv->actual_mbmv_in_0);
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB_0);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_HIGH(mbmv->actual_mbmv_in_0);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_0);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_LOW(mbmv->actual_mbmv_in_1);
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB,
		LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB_1);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_HIGH(mbmv->actual_mbmv_in_1);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_1);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_rdma(struct kunit *test)
{
	camerapp_hw_lme_check_rdma_init(test);
	camerapp_hw_lme_check_rdma_addr(test);
}

static void camerapp_hw_lme_check_wdma_init(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	struct lme_ctx *ctx = lme->current_ctx;
	struct lme_frame *d_frame, *s_frame;
	struct lme_mbmv *mbmv;
	enum lme_sps_mode sps_mode = LME_SPS_8X4;
	u32 total_width, line_count;
	u32 val, ref;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;
	mbmv = &ctx->mbmv;

	/* MV OUT */
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_CLIENT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_SPS_MV_OUT_WDMAMV_EN);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 3;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BURST_LENGTH);
	KUNIT_EXPECT_EQ(test, val, ref);

	camerapp_hw_lme_get_output_size(s_frame->width, s_frame->height, &total_width, &line_count,
		sps_mode, LME_WDMA_MV_OUT);

	ref = total_width;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = line_count;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = total_width;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_TOTAL_WIDTH,
		LME_F_SPS_MV_OUT_WDMAMV_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_DIRECTION_HW);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 16;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_PACKING);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 4;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BPAD_SET);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BPAD_TYPE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 4;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BSHIFT_SET);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_CH_MIX_0);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_CH_MIX_1);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_BURST_ALIGNMENT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_SELF_HW_FLUSH_ENABLE);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* LME_WDMA_SAD_OUT */
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_CLIENT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_SAD_OUT_WDMA_EN);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 3;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BURST_LENGTH);
	KUNIT_EXPECT_EQ(test, val, ref);

	camerapp_hw_lme_get_output_size(s_frame->width, s_frame->height, &total_width, &line_count,
		sps_mode, LME_WDMA_SAD_OUT);

	ref = total_width;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LWIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = line_count;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LINE_COUNT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = total_width;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_TOTAL_WIDTH,
		LME_F_SAD_OUT_WDMA_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LINE_DIRECTION_HW);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 24;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_PACKING);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 8;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BPAD_SET);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BPAD_TYPE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 8;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BSHIFT_SET);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_BURST_ALIGNMENT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_SELF_HW_FLUSH_ENABLE);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* MBMV OUT */
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_CLIENT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_MBMV_OUT_WDMAMV_EN);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 3;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BURST_LENGTH);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = mbmv->width;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LWIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = mbmv->height;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LINE_COUNT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = mbmv->width_align;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_TOTAL_WIDTH);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LINE_DIRECTION_HW);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 8;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_PACKING);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BPAD_SET);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BPAD_TYPE);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_MNM,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BSHIFT_SET);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_CH_MIX_0);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_CH_MIX_1);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_BURST_ALIGNMENT);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 0;
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_CLIENT_FLUSH);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_wdma_addr(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	struct lme_ctx *ctx = lme->current_ctx;
	struct lme_mbmv *mbmv;
	struct lme_frame *d_frame, *s_frame;
	enum lme_sps_mode sps_mode = LME_SPS_8X4;

	u32 total_width, line_count;
	u32 val, ref;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;
	mbmv = &ctx->mbmv;

	/* MV OUT */
	camerapp_hw_lme_get_output_size(s_frame->width, s_frame->height, &total_width, &line_count,
		sps_mode, LME_WDMA_MV_OUT);

	ref = DVA_36BIT_LOW(d_frame->addr.actual_mv_out);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_LSB);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = DVA_36BIT_HIGH(d_frame->addr.actual_mv_out);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_0);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* MBMV OUT */
	ref = 1;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_CONF,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_EN_0);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_CONF,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_EN_1);
	KUNIT_EXPECT_EQ(test, val, ref);
	ref = 1;
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_CONF,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_ROTATION_SIZE);
	KUNIT_EXPECT_EQ(test, val, ref);

	total_width = mbmv->width;
	line_count = mbmv->height;

	ref = DVA_36BIT_LOW(mbmv->actual_mbmv_out_0);
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB_0);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_HIGH(mbmv->actual_mbmv_out_0);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_0);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_LOW(mbmv->actual_mbmv_out_1);
	val = LME_GET_F(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB,
		LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB_1);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_HIGH(mbmv->actual_mbmv_out_1);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_1);
	KUNIT_EXPECT_EQ(test, val, ref);

	/* SAD OUT */
	camerapp_hw_lme_get_output_size(s_frame->width, s_frame->height, &total_width, &line_count,
		sps_mode, LME_WDMA_SAD_OUT);

	ref = DVA_36BIT_LOW(d_frame->addr.actual_sad_out);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BASE_ADDR_LSB);
	KUNIT_EXPECT_EQ(test, val, ref);

	ref = DVA_36BIT_HIGH(d_frame->addr.actual_sad_out);
	val = LME_GET_R(lme, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BASE_ADDR_0);
	KUNIT_EXPECT_EQ(test, val, ref);
}

static void camerapp_hw_lme_check_wdma(struct kunit *test)
{
	camerapp_hw_lme_check_wdma_init(test);
	camerapp_hw_lme_check_wdma_addr(test);
}

static void camerapp_hw_lme_update_ctx(struct lme_ctx *ctx, bool is_first)
{
	int width = 1024;
	int height = 768;
	u64 addr = 0x87654321f;

	ctx->s_frame.width = width;
	ctx->s_frame.height = height;
	ctx->s_frame.lme_fmt = &lme_formats[0];
	ctx->s_frame.pixelformat = V4L2_PIX_FMT_GREY;
	ctx->s_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->s_frame.addr.curr_in = addr;
	ctx->s_frame.addr.prev_in = addr;

	ctx->d_frame.width = width / 2;
	ctx->d_frame.height = height / 2;
	ctx->d_frame.lme_fmt = &lme_formats[0];
	ctx->d_frame.pixelformat = V4L2_PIX_FMT_GREY;
	ctx->d_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->d_frame.addr.mv_out = addr;
	ctx->d_frame.addr.sad_out = addr;

	/* pre_control */
	ctx->pre_control_params.op_mode = LME_OP_MODE_TNR;
	ctx->pre_control_params.scenario = LME_SCENARIO_PROCESSING;
	ctx->pre_control_params.sps_mode = LME_SPS_8X4;

	/* post_control */
	ctx->post_control_params.is_first = is_first;
	ctx->post_control_params.curr_roi_width = test_ctx.width;
	ctx->post_control_params.curr_roi_height = test_ctx.height;

	/* mbmv */
}

static void camerapp_hw_lme_update_param_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	struct lme_ctx *ctx;
	struct c_loader_buffer clb;

	ctx = kunit_kzalloc(test, sizeof(struct lme_ctx), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* for first frame */
	camerapp_hw_lme_update_ctx(ctx, true);

	clb.header_dva = 0;
	clb.num_of_headers = 0;
	lme->current_ctx = ctx;

	/* run test*/
	lme->variant = camerapp_hw_lme_get_size_constraints(lme->pmio);
	camerapp_hw_lme_update_param(lme->pmio, lme, &clb);

	/* check */
	camerapp_hw_lme_check_block_reg(test);
	camerapp_hw_lme_check_set_mvct(test);
	camerapp_hw_lme_check_size_regs(test);
	camerapp_hw_lme_check_rdma(test);
	camerapp_hw_lme_check_wdma(test);

	kunit_kfree(test, ctx);
}

static void camerapp_hw_lme_init_pmio_config_kunit_test(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;

	camerapp_hw_lme_init_pmio_config(lme);
}

static void camerapp_hw_lme_get_reg_cnt_kunit_test(struct kunit *test)
{
	u32 reg_cnt;

	reg_cnt = camerapp_hw_lme_get_reg_cnt();
	KUNIT_EXPECT_EQ(test, reg_cnt, (u32)LME_REG_CNT);
}

static void __lme_init_pmio(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;
	int ret;

	lme->regs_base = tctx->addr;

	camerapp_hw_lme_init_pmio_config(lme);
	lme->pmio_config.cache_type = PMIO_CACHE_NONE;

	lme->pmio = pmio_init(NULL, NULL, &lme->pmio_config);

	ret = pmio_field_bulk_alloc(
		lme->pmio, &lme->pmio_fields, lme->pmio_config.fields, lme->pmio_config.num_fields);
	if (ret)
		return;
}

static void __lme_exit_pmio(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;
	struct lme_dev *lme = tctx->lme;

	if (lme->pmio) {
		if (lme->pmio_fields)
			pmio_field_bulk_free(lme->pmio, lme->pmio_fields);

		pmio_exit(lme->pmio);
	}
}

static struct kunit_case camerapp_hw_api_lme_kunit_test_cases[] = {
	KUNIT_CASE(camerapp_hw_api_lme_get_size_constraints_kunit_test),
	KUNIT_CASE(camerapp_hw_api_lme_sfr_dump_kunit_test),
	KUNIT_CASE(camerapp_hw_api_lme_start_kunit_test),
	KUNIT_CASE(camerapp_hw_api_lme_stop_kunit_test),
	KUNIT_CASE(camerapp_hw_api_lme_sw_reset_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_get_intr_status_and_clear_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_get_fs_fe_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_get_output_size_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_get_mbmv_size_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_set_initialization_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_update_param_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_init_pmio_config_kunit_test),
	KUNIT_CASE(camerapp_hw_lme_get_reg_cnt_kunit_test),
	{},
};

static int camerapp_hw_api_lme_kunit_test_init(struct kunit *test)
{
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);

	test_ctx.width = 320;
	test_ctx.height = 240;
	test_ctx.stride = 320;

	test_ctx.lme = kunit_kzalloc(test, sizeof(struct lme_dev), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.lme);

	test_ctx.lme->regs_base = test_ctx.addr;
	test_ctx.lme->regs_rsc = &test_ctx.rsc;

	test->priv = &test_ctx;

	__lme_init_pmio(test);

	return 0;
}

static void camerapp_hw_api_lme_kunit_test_exit(struct kunit *test)
{
	struct camerapp_hw_api_lme_kunit_test_ctx *tctx = test->priv;

	__lme_exit_pmio(test);

	kunit_kfree(test, tctx->addr);
	kunit_kfree(test, tctx->lme);
}

struct kunit_suite camerapp_hw_api_lme_kunit_test_suite = {
	.name = "pablo-hw-api-lme-v1300-kunit-test",
	.init = camerapp_hw_api_lme_kunit_test_init,
	.exit = camerapp_hw_api_lme_kunit_test_exit,
	.test_cases = camerapp_hw_api_lme_kunit_test_cases,
};
define_pablo_kunit_test_suites(&camerapp_hw_api_lme_kunit_test_suite);

MODULE_LICENSE("GPL");
