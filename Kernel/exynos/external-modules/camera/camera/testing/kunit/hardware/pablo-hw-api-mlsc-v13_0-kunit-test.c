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
#include "pablo-hw-api-common.h"
#include "pablo-hw-api-common-ctrl.h"
#include "hardware/api/pablo-hw-api-mlsc.h"
#include "hardware/sfr/pablo-sfr-mlsc-v13_0.h"
#include "pablo-crta-interface.h"

#define MLSC_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define MLSC_SET_F_COREX(base, R, F, val) PMIO_SET_F_COREX(base, R, F, val)
#define MLSC_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define MLSC_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define MLSC_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define MLSC_GET_R(base, R) PMIO_GET_R(base, R)

#define MLSC_BYR_RDMA_STRIDE_ALIGN 16
#define DMA_AXI_DEBUG_CONTROL_OFFSET 0x1e4

struct mlsc_hw_dma {
	enum mlsc_dma_id dma_id;
	unsigned int sfr_offset;
	unsigned int field_offset;
	unsigned int size_sfr_offset;
	unsigned int size_field_offset;
	char *name;
};

static const struct mlsc_hw_dma mlsc_dmas[MLSC_DMA_NUM] = {
	{ MLSC_DMA_NONE, MLSC_REG_CNT, 0, 0, 0, "DMA_NONE" },
	{ MLSC_R_CL, MLSC_R_YUV_RDMACLOAD_EN, MLSC_F_YUV_RDMACLOAD_EN, 0, 0, "R_CL" },
	{ MLSC_R_Y, MLSC_R_YUV_RDMAY_EN, MLSC_R_YUV_RDMAY_EN, 0, 0, "R_Y" },
	{ MLSC_R_UV, MLSC_R_YUV_RDMAUV_EN, MLSC_F_YUV_RDMAUV_EN, 0, 0, "R_UV" },
	{ MLSC_W_YUV444_U, MLSC_R_YUV_WDMAYUV444Y_EN, MLSC_F_YUV_WDMAYUV444Y_EN, 0, 0,
		"W_YUV444_Y" },
	{ MLSC_W_YUV444_V, MLSC_R_YUV_WDMAYUV444U_EN, MLSC_F_YUV_WDMAYUV444U_EN, 0, 0,
		"W_YUV444_U" },
	{ MLSC_W_YUV444_Y, MLSC_R_YUV_WDMAYUV444V_EN, MLSC_F_YUV_WDMAYUV444V_EN, 0, 0,
		"W_YUV444_V" },
	{ MLSC_W_GLPG0_Y, MLSC_R_Y_WDMAGLPGOUTL0_EN, MLSC_F_Y_WDMAGLPGOUTL0_EN, 0, 0, "W_GLPG0_Y" },
	{ MLSC_W_GLPG1_Y, MLSC_R_YUV_WDMAGLPGOUTL1Y_EN, MLSC_F_YUV_WDMAGLPGOUTL1Y_EN,
		MLSC_R_GLPG_L1_IMG_SIZE, MLSC_F_GLPG_L1_IMG_WIDTH, "W_GLPG1_Y" },
	{ MLSC_W_GLPG1_U, MLSC_R_YUV_WDMAGLPGOUTL1U_EN, MLSC_F_YUV_WDMAGLPGOUTL1U_EN, 0, 0,
		"W_GLPG1_U" },
	{ MLSC_W_GLPG1_V, MLSC_R_YUV_WDMAGLPGOUTL1V_EN, MLSC_F_YUV_WDMAGLPGOUTL1V_EN, 0, 0,
		"W_GLPG1_V" },
	{ MLSC_W_GLPG2_Y, MLSC_R_YUV_WDMAGLPGOUTL2Y_EN, MLSC_F_YUV_WDMAGLPGOUTL2Y_EN,
		MLSC_R_GLPG_L2_IMG_SIZE, MLSC_F_GLPG_L2_IMG_WIDTH, "W_GLPG2_Y" },
	{ MLSC_W_GLPG2_U, MLSC_R_YUV_WDMAGLPGOUTL2U_EN, MLSC_F_YUV_WDMAGLPGOUTL2U_EN, 0, 0,
		"W_GLPG2_U" },
	{ MLSC_W_GLPG2_V, MLSC_R_YUV_WDMAGLPGOUTL2V_EN, MLSC_F_YUV_WDMAGLPGOUTL2V_EN, 0, 0,
		"W_GLPG2_V" },
	{ MLSC_W_GLPG3_Y, MLSC_R_YUV_WDMAGLPGOUTL3Y_EN, MLSC_F_YUV_WDMAGLPGOUTL3Y_EN,
		MLSC_R_GLPG_L3_IMG_SIZE, MLSC_F_GLPG_L3_IMG_WIDTH, "W_GLPG3_Y" },
	{ MLSC_W_GLPG3_U, MLSC_R_YUV_WDMAGLPGOUTL3U_EN, MLSC_F_YUV_WDMAGLPGOUTL3U_EN, 0, 0,
		"W_GLPG3_U" },
	{ MLSC_W_GLPG3_V, MLSC_R_YUV_WDMAGLPGOUTL3V_EN, MLSC_F_YUV_WDMAGLPGOUTL3V_EN, 0, 0,
		"W_GLPG3_V" },
	{ MLSC_W_GLPG4_Y, MLSC_R_YUV_WDMAGLPGOUTG4Y_EN, MLSC_F_YUV_WDMAGLPGOUTG4Y_EN,
		MLSC_R_GLPG_G4_IMG_SIZE, MLSC_F_GLPG_G4_IMG_WIDTH, "W_GLPG4_Y" },
	{ MLSC_W_GLPG4_U, MLSC_R_YUV_WDMAGLPGOUTG4U_EN, MLSC_F_YUV_WDMAGLPGOUTG4U_EN, 0, 0,
		"W_GLPG4_U" },
	{ MLSC_W_GLPG4_V, MLSC_R_YUV_WDMAGLPGOUTG4V_EN, MLSC_F_YUV_WDMAGLPGOUTG4V_EN, 0, 0,
		"W_GLPG4_V" },
	{ MLSC_W_SVHIST, MLSC_R_STAT_WDMASVHIST_EN, MLSC_F_STAT_WDMASVHIST_EN, 0, 0, "W_SVHIST" },
	{ MLSC_W_FDPIG, MLSC_R_YUV_WDMAFDPIG_EN, MLSC_F_YUV_WDMAFDPIG_EN, MLSC_R_FDPIG_IMG_SIZE,
		MLSC_F_FDPIG_IMG_WIDTH, "W_FDPIG" },
	{ MLSC_W_LMEDS, MLSC_R_Y_WDMALME_EN, MLSC_F_Y_WDMALME_EN, MLSC_R_LME_IMG_SIZE,
		MLSC_F_LME_IMG_WIDTH, "W_DSLME" },
	{ MLSC_W_CAV, MLSC_R_YUV_WDMACAV_EN, MLSC_F_YUV_WDMACAV_EN, MLSC_R_CAV_IMG_SIZE,
		MLSC_F_CAV_IMG_WIDTH, "W_CAV" },
};

static struct mlsc_test_ctx {
	void *test_addr;
	struct is_mlsc_config config;
	struct pmio_config pmio_config;
	struct pablo_mmio *pmio;
	struct pmio_field *pmio_fields;
	struct pmio_reg_seq *pmio_reg_seqs;
	struct is_frame frame;
	struct mlsc_param_set param_set;
	struct is_param_region param_region;
} test_ctx;

#define MLSC_SIZE_TEST_SET 3

static struct is_rectangle mlsc_size_test_config[MLSC_SIZE_TEST_SET] = {
	{ 128, 128 }, /* min size */
	{ 20800, 15600 }, /* max size */
	{ 4032, 3024 }, /* customized size */
};

struct reg_set_test {
	u32 reg_name;
	u32 field_name;
	u32 value;
};

static struct reg_set_test mlsc_bypass_test_list[] = {
	{ MLSC_R_YUV_YUV420TO422_BYPASS, MLSC_F_YUV_YUV420TO422_BYPASS, 0x1 },
	{ MLSC_R_RGB_INVGAMMARGB_BYPASS, MLSC_F_RGB_INVGAMMARGB_BYPASS, 0x1 },
	{ MLSC_R_RGB_INVCCM33_CONFIG, MLSC_F_RGB_INVCCM33_BYPASS, 0x1 },
	{ MLSC_R_RGB_BLCSTAT_BYPASS, MLSC_F_RGB_BLCSTAT_BYPASS, 0x1 },
	{ MLSC_R_RGB_SDRC_BYPASS, MLSC_F_RGB_SDRC_BYPASS, 0x1 },
	{ MLSC_R_RGB_CCM9_BYPASS, MLSC_F_RGB_CCM9_BYPASS, 0x1 },
	{ MLSC_R_RGB_GAMMARGB_BYPASS, MLSC_F_RGB_GAMMARGB_BYPASS, 0x1 },
	{ MLSC_R_RGB_SVHIST_BYPASS, MLSC_F_RGB_SVHIST_BYPASS, 0x1 },
	{ MLSC_R_RGB_RGB2Y_BYPASS, MLSC_F_RGB_RGB2Y_BYPASS, 0x1 },
	{ MLSC_R_Y_MENR_BYPASS, MLSC_F_Y_MENR_BYPASS, 0x1 },
	{ MLSC_R_Y_EDGESCORE_BYPASS, MLSC_F_Y_EDGESCORE_BYPASS, 0x1 },
};

static void pablo_hw_api_mlsc_hw_dump_kunit_test(struct kunit *test)
{
	mlsc_hw_dump(test_ctx.pmio, HW_DUMP_CR);
	mlsc_hw_dump(test_ctx.pmio, HW_DUMP_DBG_STATE);
}

static void pablo_hw_api_mlsc_hw_g_int_en_kunit_test(struct kunit *test)
{
	u32 int_en[PCC_INT_ID_NUM] = { 0 };

	mlsc_hw_g_int_en(int_en);

	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_0], INT0_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_1], INT1_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_CMDQ_INT], 0);
	KUNIT_EXPECT_EQ(test, int_en[PCC_COREX_INT], 0);
}

#define MLSC_INT_GRP_EN_MASK                                                                       \
	((0) | (1 << INTR_GRP_MLSC_FRAME_START_INT) | (1 << INTR_GRP_MLSC_FRAME_END_INT) |         \
		(1 << INTR_GRP_MLSC_ERROR_CRPT_INT) | (1 << INTR_GRP_MLSC_CMDQ_HOLD_INT) |         \
		(1 << INTR_GRP_MLSC_SETTING_DONE_INT) | (1 << INTR_GRP_MLSC_DEBUG_INT) |           \
		(1 << INTR_GRP_MLSC_ENABLE_ALL_INT))
static void pablo_hw_api_mlsc_hw_g_int_grp_en_kunit_test(struct kunit *test)
{
	u32 int_grp_en;

	int_grp_en = mlsc_hw_g_int_grp_en();

	KUNIT_EXPECT_EQ(test, int_grp_en, MLSC_INT_GRP_EN_MASK);
}

static void pablo_hw_api_mlsc_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 ret;
	int test_idx;

	mlsc_hw_s_bypass(test_ctx.pmio);

	for (test_idx = 0; test_idx < ARRAY_SIZE(mlsc_bypass_test_list); test_idx++) {
		ret = MLSC_GET_F(test_ctx.pmio, mlsc_bypass_test_list[test_idx].reg_name,
			mlsc_bypass_test_list[test_idx].field_name);
		KUNIT_EXPECT_EQ(test, ret, mlsc_bypass_test_list[test_idx].value);
	}
}

static void pablo_hw_api_mlsc_hw_dma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = MLSC_R_CL; test_idx < MLSC_DMA_NUM; test_idx++) {
		dma_id = test_idx;

		ret = mlsc_hw_create_dma(test_ctx.pmio, dma_id, &dma);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* dma id err test */
	dma_id = MLSC_DMA_NUM;
	ret = mlsc_hw_create_dma(test_ctx.pmio, dma_id, &dma);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

struct pablo_hw_api_mlsc_test_config {
	struct is_rectangle input;
	struct is_rectangle yuv444;
	struct is_rectangle glpg[5];
	struct is_rectangle lmeds;
	struct is_rectangle fdpig;
	struct is_rectangle cav;
};

const static struct pablo_hw_api_mlsc_test_config test_config = { { 4000, 3000 }, { 4000, 3000 },
	{ { 4000, 3000 }, { 2000, 1500 }, { 1000, 750 }, { 500, 376 }, { 250, 188 } }, { 320, 240 },
	{ 320, 240 }, { 320, 240 } };

static void pablo_hw_api_mlsc_hw_s_rdma_cfg_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	enum mlsc_dma_id dma_id;
	struct param_dma_input *dma_input;
	int ret;
	ulong set_val, expected_val;

	/* set input */
	for (dma_id = MLSC_R_Y; dma_id < MLSC_RDMA_NUM; dma_id++) {
		/* run: mlsc_hw_create_dma */
		ret = mlsc_hw_create_dma(test_ctx.pmio, dma_id, &dma);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = dma.available_bayer_format_map;
		expected_val = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_PACK) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_PACK) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)) &
			       IS_BAYER_FORMAT_MASK;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = dma.available_yuv_format_map;
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = dma.available_rgb_format_map;
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* set input: 8bit Y only */
		dma_input = &test_ctx.param_set.dma_input;
		dma_input->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma_input->width = test_config.input.w;
		dma_input->height = test_config.input.h;
		dma_input->format = DMA_INOUT_FORMAT_YUV422;
		dma_input->order = DMA_INOUT_ORDER_CbCr;
		dma_input->bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
		dma_input->msb = dma_input->bitwidth - 1;
		dma_input->plane = DMA_INOUT_PLANE_1;
		dma_input->sbwc_type = DMA_INPUT_SBWC_DISABLE;
		dma_input->v_otf_enable = false;
		test_ctx.param_set.input_dva[0] = 0x19860218;

		ret = mlsc_hw_s_rdma_cfg(&dma, &test_ctx.param_set, 1);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMAY_EN, MLSC_F_YUV_RDMAY_EN);
		expected_val = dma_input->cmd;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMAY_DATA_FORMAT,
			MLSC_F_YUV_RDMAY_DATA_FORMAT_BAYER);
		expected_val = DMA_FMT_U12BIT_PACK;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = MLSC_GET_F(
			test_ctx.pmio, MLSC_R_YUV_RDMAY_COMP_CONTROL, MLSC_F_YUV_RDMAY_SBWC_EN);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMAY_WIDTH, MLSC_F_YUV_RDMAY_WIDTH);
		expected_val = dma_input->width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val =
			MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMAY_HEIGHT, MLSC_F_YUV_RDMAY_HEIGHT);
		expected_val = dma_input->height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMAY_IMG_STRIDE_1P,
			MLSC_F_YUV_RDMAY_IMG_STRIDE_1P);
		expected_val =
			ALIGN(DIV_ROUND_UP(dma_input->width * dma_input->bitwidth, BITS_PER_BYTE),
				MLSC_BYR_RDMA_STRIDE_ALIGN);
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* MSB */
		set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO0,
			MLSC_F_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO0);
		/* MSB << 4 | LSB */
		set_val = (set_val << 4) |
			  MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
				  MLSC_F_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

		expected_val = test_ctx.param_set.input_dva[0];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}
}

static void pablo_hw_api_mlsc_hw_s_wdma_cav_cfg_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	enum mlsc_dma_id dma_id;
	struct param_dma_output *dma_out;
	int ret;
	ulong set_val, expected_val;

	/* set input */
	dma_id = MLSC_W_CAV;

	/* run: mlsc_hw_create_dma */
	ret = mlsc_hw_create_dma(test_ctx.pmio, dma_id, &dma);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = dma.available_bayer_format_map;
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = dma.available_yuv_format_map;
	expected_val =
		(0 | BIT_MASK(DMA_FMT_YUV422_2P_UFIRST) | BIT_MASK(DMA_FMT_YUV420_2P_UFIRST) |
			BIT_MASK(DMA_FMT_YUV420_2P_VFIRST) | BIT_MASK(DMA_FMT_YUV444_1P)) &
		IS_YUV_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = dma.available_rgb_format_map;
	expected_val = (0 | BIT_MASK(DMA_FMT_RGB_RGBA8888) | BIT_MASK(DMA_FMT_RGB_ABGR8888) |
			       BIT_MASK(DMA_FMT_RGB_ARGB8888) | BIT_MASK(DMA_FMT_RGB_BGRA8888)) &
		       IS_RGB_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* set input: yuv420 2P 8bit */
	dma_out = &test_ctx.param_set.dma_output_cav;
	dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_out->width = test_config.cav.w;
	dma_out->height = test_config.cav.h;
	dma_out->format = DMA_INOUT_FORMAT_YUV420;
	dma_out->order = DMA_INOUT_ORDER_CbCr;
	dma_out->bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	dma_out->msb = dma_out->bitwidth - 1;
	dma_out->plane = DMA_INOUT_PLANE_2;
	test_ctx.param_set.output_dva_cav[0] = 0xdeadbeef;

	ret = mlsc_hw_s_wdma_cfg(&dma, &test_ctx.param_set, 1, 0);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_WDMACAV_EN, MLSC_F_YUV_WDMACAV_EN);
	expected_val = dma_out->cmd;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_YUV_WDMACAV_DATA_FORMAT, MLSC_F_YUV_WDMACAV_DATA_FORMAT_TYPE);
	expected_val = DMA_FMT_YUV;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_YUV_WDMACAV_DATA_FORMAT, MLSC_F_YUV_WDMACAV_DATA_FORMAT_YUV);
	expected_val = DMA_FMT_YUV420_2P_UFIRST;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_WDMACAV_WIDTH, MLSC_F_YUV_WDMACAV_WIDTH);
	expected_val = dma_out->width;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_WDMACAV_HEIGHT, MLSC_F_YUV_WDMACAV_HEIGHT);
	expected_val = dma_out->height;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_YUV_WDMACAV_IMG_STRIDE_1P, MLSC_F_YUV_WDMACAV_IMG_STRIDE_1P);
	expected_val = ALIGN(DIV_ROUND_UP(dma_out->width * dma_out->bitwidth, BITS_PER_BYTE), 16);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* MSB */
	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0,
		MLSC_F_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0);
	/* MSB << 4 | LSB */
	set_val = (set_val << 4) |
		  MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
			  MLSC_F_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

	expected_val = test_ctx.param_set.output_dva_cav[0];
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_RGB_RGBTOYUVCAV_BYPASS, MLSC_F_RGB_RGBTOYUVCAV_BYPASS);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_mlsc_hw_s_wdma_glpg0_cfg_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	enum mlsc_dma_id dma_id;
	struct param_dma_output *dma_out;
	int ret;
	ulong set_val, expected_val;

	/* set input */
	dma_id = MLSC_W_GLPG0_Y;

	/* run: mlsc_hw_create_dma */
	ret = mlsc_hw_create_dma(test_ctx.pmio, dma_id, &dma);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = dma.available_bayer_format_map;
	expected_val =
		(0 | BIT_MASK(DMA_FMT_U10BIT_PACK) | BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO) |
			BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO) | BIT_MASK(DMA_FMT_U12BIT_PACK) |
			BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO) |
			BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)) &
		IS_BAYER_FORMAT_MASK;

	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = dma.available_yuv_format_map;
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = dma.available_rgb_format_map;
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* set input: y 12bit */
	dma_out = &test_ctx.param_set.dma_output_glpg[0];
	dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_out->width = test_config.glpg[0].w;
	dma_out->height = test_config.glpg[0].h;
	dma_out->format = DMA_INOUT_FORMAT_Y;
	dma_out->order = DMA_INOUT_ORDER_NO;
	dma_out->bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	dma_out->msb = dma_out->bitwidth - 1;
	dma_out->plane = DMA_INOUT_PLANE_1;
	dma_out->sbwc_type = DMA_INPUT_SBWC_DISABLE;
	test_ctx.param_set.output_dva_glpg[0][0] = 0xdeadbeef;

	ret = mlsc_hw_s_wdma_cfg(&dma, &test_ctx.param_set, 1, 0);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_EN, MLSC_F_Y_WDMAGLPGOUTL0_EN);
	expected_val = dma_out->cmd;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_DATA_FORMAT,
		MLSC_F_Y_WDMAGLPGOUTL0_DATA_FORMAT_BAYER);
	expected_val = DMA_FMT_U12BIT_PACK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_WIDTH, MLSC_F_Y_WDMAGLPGOUTL0_WIDTH);
	expected_val = dma_out->width;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_HEIGHT, MLSC_F_Y_WDMAGLPGOUTL0_HEIGHT);
	expected_val = dma_out->height;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_IMG_STRIDE_1P,
		MLSC_F_Y_WDMAGLPGOUTL0_IMG_STRIDE_1P);
	expected_val = ALIGN(DIV_ROUND_UP(dma_out->width * dma_out->bitwidth, BITS_PER_BYTE), 16);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* MSB */
	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_IMG_BASE_ADDR_1P_FRO0,
		MLSC_F_Y_WDMAGLPGOUTL0_IMG_BASE_ADDR_1P_FRO0);
	/* MSB << 4 | LSB */
	set_val = (set_val << 4) |
		  MLSC_GET_F(test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
			  MLSC_F_Y_WDMAGLPGOUTL0_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

	expected_val = test_ctx.param_set.output_dva_glpg[0][0];
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_COMP_CONTROL, MLSC_F_Y_WDMAGLPGOUTL0_SBWC_EN);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* sbwc test */
	dma_out->sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
	ret = mlsc_hw_s_wdma_cfg(&dma, &test_ctx.param_set, 1, 0);

	set_val = MLSC_GET_F(
		test_ctx.pmio, MLSC_R_Y_WDMAGLPGOUTL0_COMP_CONTROL, MLSC_F_Y_WDMAGLPGOUTL0_SBWC_EN);
	expected_val = 2;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MLSC_GET_F(test_ctx.pmio, MLSC_R_SBWC_32X4_CTRL_0, MLSC_F_OTF_TO_TILE_ENABLE_L0);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_mlsc_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	mlsc_hw_s_strgen(test_ctx.pmio);

	/* STRGEN setting */
	set_val = MLSC_GET_R(test_ctx.pmio, MLSC_R_YUV_CINFIFO_CONFIG);
	expected_val = 0x800020;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_mlsc_hw_s_core_kunit_test(struct kunit *test)
{
	int test_idx;
	u32 width, height;

	for (test_idx = 0; test_idx < MLSC_SIZE_TEST_SET; test_idx++) {
		width = mlsc_size_test_config[test_idx].w;
		height = mlsc_size_test_config[test_idx].h;

		mlsc_hw_s_core(test_ctx.pmio, width, height);

		KUNIT_EXPECT_EQ(test,
			MLSC_GET_F(test_ctx.pmio, MLSC_R_CHAIN_IMG_SIZE, MLSC_F_CHAIN_IMG_WIDTH),
			width);
		KUNIT_EXPECT_EQ(test,
			MLSC_GET_F(test_ctx.pmio, MLSC_R_CHAIN_IMG_SIZE, MLSC_F_CHAIN_IMG_HEIGHT),
			height);
	}
}

static void pablo_hw_api_mlsc_hw_is_occurred_kunit_test(struct kunit *test)
{
	bool occur;
	u32 status;
	ulong type;

	/* TC#1. No interrupt. */
	status = 0;
	type = BIT_MASK(INT_FRAME_START);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	/* TC#2. Test each interrupt. */
	status = BIT_MASK(INTR0_MLSC_FRAME_START_INT);
	type = BIT_MASK(INT_FRAME_START);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_MLSC_FRAME_END_INT);
	type = BIT_MASK(INT_FRAME_END);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_MLSC_COREX_END_INT_0);
	type = BIT_MASK(INT_COREX_END);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_MLSC_SETTING_DONE_INT);
	type = BIT_MASK(INT_SETTING_DONE);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR1_MLSC_VOTF_LOST_FLUSH_INT);
	type = BIT_MASK(INT_ERR1);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	/* TC#3. Test interrupt ovarlapping. */
	status = BIT_MASK(INTR0_MLSC_FRAME_START_INT);
	type = BIT_MASK(INTR0_MLSC_FRAME_START_INT) | BIT_MASK(INTR0_MLSC_FRAME_END_INT);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	status = BIT_MASK(INTR0_MLSC_FRAME_START_INT) | BIT_MASK(INTR0_MLSC_FRAME_END_INT);
	occur = mlsc_hw_is_occurred(status, type);
	KUNIT_EXPECT_EQ(test, occur, true);
}

static void pablo_hw_api_mlsc_hw_s_init_kunit_test(struct kunit *test)
{
	mlsc_hw_init(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test,
		MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_CINFIFO_CONFIG,
			MLSC_F_YUV_CINFIFO_STALL_BEFORE_FRAME_START_EN),
		1);
	KUNIT_EXPECT_EQ(test,
		MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_CINFIFO_CONFIG, MLSC_F_YUV_CINFIFO_DEBUG_EN),
		1);
	KUNIT_EXPECT_EQ(test, MLSC_GET_R(test_ctx.pmio, MLSC_R_YUV_CINFIFO_ENABLE), 1);

	KUNIT_EXPECT_EQ(test, MLSC_GET_R(test_ctx.pmio, MLSC_R_YUV_CINFIFO_INT_ENABLE), 0xF);
	KUNIT_EXPECT_EQ(test, MLSC_GET_R(test_ctx.pmio, MLSC_R_YUV_CINFIFO_ROL_SELECT), 0xF);

	KUNIT_EXPECT_EQ(test,
		MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_RDMACLOAD_EN, MLSC_F_YUV_RDMACLOAD_EN), 1);
}

static void pablo_hw_api_mlsc_hw_wait_idle_kunit_test(struct kunit *test)
{
	int ret;

	ret = mlsc_hw_wait_idle(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	MLSC_SET_F(test_ctx.pmio, MLSC_R_IDLENESS_STATUS, MLSC_F_IDLENESS_STATUS, 1);
	ret = mlsc_hw_wait_idle(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

enum mlsc_cotf_in_id {
	MLSC_COTF_IN_YUV,
};

static void pablo_hw_api_mlsc_hw_s_path_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl_frame_cfg frame_cfg;
	enum mlsc_input_path input;

	input = DMA;
	mlsc_hw_s_path(test_ctx.pmio, input, &frame_cfg);
	KUNIT_EXPECT_EQ(test, MLSC_GET_R(test_ctx.pmio, MLSC_R_YUV_CINFIFO_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, MLSC_GET_R(test_ctx.pmio, MLSC_R_OTF_PLATFORM_INPUT_MUX_0TO3), 0);
	KUNIT_EXPECT_EQ(test, 0, frame_cfg.cotf_in_en);

	input = OTF;
	mlsc_hw_s_path(test_ctx.pmio, input, &frame_cfg);
	KUNIT_EXPECT_EQ(test, MLSC_GET_R(test_ctx.pmio, MLSC_R_YUV_CINFIFO_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, MLSC_GET_R(test_ctx.pmio, MLSC_R_OTF_PLATFORM_INPUT_MUX_0TO3), 1);
	KUNIT_EXPECT_EQ(test, BIT_MASK(MLSC_COTF_IN_YUV), frame_cfg.cotf_in_en);
}

static void pablo_hw_api_mlsc_hw_s_lic_cfg_kunit_test(struct kunit *test)
{
	struct mlsc_lic_cfg cfg;

	cfg.bypass = 0;

	cfg.input_path = OTF;
	mlsc_hw_s_lic_cfg(test_ctx.pmio, &cfg);
	KUNIT_EXPECT_EQ(
		test, MLSC_GET_F(test_ctx.pmio, MLSC_R_LIC_INPUT_MODE, MLSC_F_LIC_BYPASS), 0);
	KUNIT_EXPECT_EQ(test,
		MLSC_GET_F(test_ctx.pmio, MLSC_R_LIC_INPUT_CONFIG_0, MLSC_F_LIC_RDMA_EN), OTF);

	cfg.input_path = DMA;
	mlsc_hw_s_lic_cfg(test_ctx.pmio, &cfg);
	KUNIT_EXPECT_EQ(test,
		MLSC_GET_F(test_ctx.pmio, MLSC_R_LIC_INPUT_CONFIG_0, MLSC_F_LIC_RDMA_EN), DMA);
}

static void pablo_hw_api_mlsc_hw_s_ds_cfg_kunit_test(struct kunit *test)
{
	int ret;
	u32 dma_id;
	u32 set_val;
	u32 crop_reg, crop_field;
	struct mlsc_size_cfg size_cfg;
	struct param_dma_output *dma_out;
	struct is_mlsc_config *config = &test_ctx.config;

	size_cfg.rms_crop_ratio = 10; /* x1.0 */
	size_cfg.input_w = test_config.input.w;
	size_cfg.input_h = test_config.input.h;

	for (dma_id = MLSC_W_FDPIG; dma_id < MLSC_DMA_NUM; dma_id++) {
		crop_reg = MLSC_REG_CNT;

		switch (dma_id) {
		case MLSC_W_LMEDS:
			dma_out = &test_ctx.param_set.dma_output_lme_ds;
			config->lmeds_bypass = 0;
			config->lmeds_w = test_config.lmeds.w;
			config->lmeds_h = test_config.lmeds.h;
			break;
		case MLSC_W_FDPIG:
			dma_out = &test_ctx.param_set.dma_output_fdpig;
			dma_out->dma_crop_width = test_config.input.w;
			dma_out->dma_crop_height = test_config.input.h;
			dma_out->width = test_config.fdpig.w;
			dma_out->height = test_config.fdpig.h;
			crop_reg = MLSC_R_YUV_DSFDPIG_CROP_EN;
			crop_field = MLSC_F_YUV_DSFDPIG_CROP_EN;
			break;
		case MLSC_W_CAV:
			dma_out = &test_ctx.param_set.dma_output_cav;
			dma_out->dma_crop_width = test_config.input.w;
			dma_out->dma_crop_height = test_config.input.h;
			dma_out->width = test_config.cav.w;
			dma_out->height = test_config.cav.h;
			crop_reg = MLSC_R_YUV_DSCAV_CROP_EN;
			crop_field = MLSC_F_YUV_DSCAV_CROP_EN;
			break;
		default:
			continue;
		}

		dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		ret = mlsc_hw_s_ds_cfg(
			test_ctx.pmio, dma_id, &size_cfg, config, &test_ctx.param_set);
		KUNIT_EXPECT_EQ(test, ret, 0);

		if (crop_reg != MLSC_REG_CNT) {
			set_val = MLSC_GET_F(test_ctx.pmio, crop_reg, crop_field);
			KUNIT_EXPECT_EQ(test, set_val, 1);
		}

		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		mlsc_hw_s_ds_cfg(test_ctx.pmio, dma_id, &size_cfg, config, &test_ctx.param_set);
		KUNIT_EXPECT_EQ(test, ret, 0);

		if (crop_reg != MLSC_REG_CNT) {
			set_val = MLSC_GET_F(test_ctx.pmio, crop_reg, crop_field);
			KUNIT_EXPECT_EQ(test, set_val, 0);
		}
	}

	/* incrop error */
	dma_out = &test_ctx.param_set.dma_output_cav;
	dma_out->dma_crop_width = test_config.input.w + 1;
	dma_out->dma_crop_height = test_config.input.h + 1;

	dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	ret = mlsc_hw_s_ds_cfg(test_ctx.pmio, MLSC_W_CAV, &size_cfg, config, &test_ctx.param_set);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* outcrop error */
	dma_out = &test_ctx.param_set.dma_output_cav;
	dma_out->dma_crop_width = test_config.input.w;
	dma_out->dma_crop_height = test_config.input.h;
	dma_out->width = test_config.input.w + 1;
	dma_out->height = test_config.input.h + 1;

	dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	ret = mlsc_hw_s_ds_cfg(test_ctx.pmio, MLSC_W_CAV, &size_cfg, config, &test_ctx.param_set);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sensor remosaic crop scenario */
	size_cfg.rms_crop_ratio = 20;

	for (dma_id = MLSC_W_FDPIG; dma_id <= MLSC_W_CAV; dma_id++) {
		switch (dma_id) {
		case MLSC_W_FDPIG:
			dma_out = &test_ctx.param_set.dma_output_fdpig;
			dma_out->width = test_config.fdpig.w;
			dma_out->height = test_config.fdpig.h;
			break;
		case MLSC_W_CAV:
			dma_out = &test_ctx.param_set.dma_output_cav;
			dma_out->width = test_config.cav.w;
			dma_out->height = test_config.cav.w;
			break;
		default:
			continue;
		}

		dma_out->dma_crop_width = test_config.input.w;
		dma_out->dma_crop_height = test_config.input.h;
		dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;

		ret = mlsc_hw_s_ds_cfg(
			test_ctx.pmio, dma_id, &size_cfg, config, &test_ctx.param_set);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}

static void pablo_hw_api_mlsc_hw_s_glpg_kunit_test(struct kunit *test)
{
	int i, ret;
	struct mlsc_size_cfg size_cfg;
	u32 glpg_reg_offset = MLSC_R_YUV_GLPGL1_BYPASS - MLSC_R_YUV_GLPGL0_BYPASS;

	size_cfg.input_w = test_config.input.w;
	size_cfg.input_h = test_config.input.h;

	for (i = 0; i < MLSC_GLPG_NUM; i++) {
		test_ctx.param_set.dma_output_glpg[i].cmd = 1;
		test_ctx.param_set.dma_output_glpg[i].width = test_config.glpg[i].w;
		test_ctx.param_set.dma_output_glpg[i].height = test_config.glpg[i].h;
		test_ctx.param_set.dma_output_glpg[i].bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	}

	ret = mlsc_hw_s_glpg(test_ctx.pmio, &test_ctx.param_set, &size_cfg);

	for (i = 0; i < MLSC_GLPG_NUM - 1; i++) {
		KUNIT_EXPECT_EQ(test,
			MLSC_GET_F(test_ctx.pmio,
				MLSC_R_YUV_GLPGL0_DOWNSCALER_SRC_SIZE + glpg_reg_offset * i,
				MLSC_F_YUV_GLPGL0_DOWNSCALER_SRC_HSIZE),
			test_ctx.param_set.dma_output_glpg[i].width);
		KUNIT_EXPECT_EQ(test,
			MLSC_GET_F(test_ctx.pmio,
				MLSC_R_YUV_GLPGL0_DOWNSCALER_SRC_SIZE + glpg_reg_offset * i,
				MLSC_F_YUV_GLPGL0_DOWNSCALER_SRC_VSIZE),
			test_ctx.param_set.dma_output_glpg[i].height);

		KUNIT_EXPECT_EQ(test,
			MLSC_GET_F(test_ctx.pmio,
				MLSC_R_YUV_GLPGL0_DOWNSCALER_DST_SIZE + glpg_reg_offset * i,
				MLSC_F_YUV_GLPGL0_DOWNSCALER_DST_HSIZE),
			test_ctx.param_set.dma_output_glpg[i + 1].width);
		KUNIT_EXPECT_EQ(test,
			MLSC_GET_F(test_ctx.pmio,
				MLSC_R_YUV_GLPGL0_DOWNSCALER_DST_SIZE + glpg_reg_offset * i,
				MLSC_F_YUV_GLPGL0_DOWNSCALER_DST_VSIZE),
			test_ctx.param_set.dma_output_glpg[i + 1].height);

		KUNIT_EXPECT_EQ(test,
			MLSC_GET_F(test_ctx.pmio, MLSC_R_YUV_GLPGL0_BYPASS + glpg_reg_offset * i,
				MLSC_F_YUV_GLPGL0_BIT_10_MODE_EN),
			0);
	}
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* odd size error */
	test_ctx.param_set.dma_output_glpg[0].width = test_config.glpg[0].w - 1;
	ret = mlsc_hw_s_glpg(test_ctx.pmio, &test_ctx.param_set, &size_cfg);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* wrong size error (out size > input size) */
	test_ctx.param_set.dma_output_glpg[0].width = test_config.glpg[0].w + 2;
	ret = mlsc_hw_s_glpg(test_ctx.pmio, &test_ctx.param_set, &size_cfg);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_api_mlsc_hw_s_dma_debug_kunit_test(struct kunit *test)
{
	u32 dma_id, set_val;

	for (dma_id = MLSC_W_YUV444_Y; dma_id <= MLSC_W_CAV; dma_id++) {
		mlsc_hw_s_dma_debug(test_ctx.pmio, dma_id);
		set_val = MLSC_GET_R(
			test_ctx.pmio, mlsc_dmas[dma_id].sfr_offset + DMA_AXI_DEBUG_CONTROL_OFFSET);

		KUNIT_EXPECT_EQ(test, set_val, 0x3);
	}
}

static void pablo_hw_api_mlsc_hw_clr_cotf_err_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio = test_ctx.pmio;
	const u32 val = __LINE__;

	MLSC_SET_R(pmio, MLSC_R_YUV_CINFIFO_INT, val);

	mlsc_hw_clr_cotf_err(pmio);

	KUNIT_EXPECT_EQ(test, MLSC_GET_R(pmio, MLSC_R_YUV_CINFIFO_INT_CLEAR), val);
}

static struct kunit_case pablo_hw_api_mlsc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_mlsc_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_g_int_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_g_int_grp_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_dma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_rdma_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_wdma_cav_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_wdma_glpg0_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_lic_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_ds_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_glpg_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_s_dma_debug_kunit_test),
	KUNIT_CASE(pablo_hw_api_mlsc_hw_clr_cotf_err_kunit_test),
	{},
};

static struct pablo_mmio *pablo_hw_api_mlsc_pmio_init(void)
{
	struct pmio_config *pcfg;
	struct pablo_mmio *pmio;

	pcfg = &test_ctx.pmio_config;

	pcfg->name = "MLSC";
	pcfg->mmio_base = test_ctx.test_addr;
	pcfg->cache_type = PMIO_CACHE_NONE;

	mlsc_hw_g_pmio_cfg(pcfg);

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

static int pablo_hw_api_mlsc_kunit_test_init(struct kunit *test)
{
	test_ctx.test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	test_ctx.pmio = pablo_hw_api_mlsc_pmio_init();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.pmio);

	return 0;
}

static void pablo_hw_api_mlsc_pmio_deinit(void)
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

static void pablo_hw_api_mlsc_kunit_test_exit(struct kunit *test)
{
	pablo_hw_api_mlsc_pmio_deinit();
	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(struct mlsc_test_ctx));
}

struct kunit_suite pablo_hw_api_mlsc_kunit_test_suite = {
	.name = "pablo-hw-api-mlsc-v13_0-kunit-test",
	.init = pablo_hw_api_mlsc_kunit_test_init,
	.exit = pablo_hw_api_mlsc_kunit_test_exit,
	.test_cases = pablo_hw_api_mlsc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_mlsc_kunit_test_suite);

MODULE_LICENSE("GPL");
