// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-type.h"
#include "pablo-hw-api-common.h"
#include "pablo-hw-api-common-ctrl.h"
#include "hardware/api/pablo-hw-api-byrp-v4_0.h"
#include "hardware/sfr/pablo-sfr-byrp-v4_0.h"
#include "pablo-crta-interface.h"

#define BYRP_LUT_REG_CNT 1650 /* DRC */

#define BYRP_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define BYRP_SET_F_COREX(base, R, F, val) PMIO_SET_F_COREX(base, R, F, val)
#define BYRP_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define BYRP_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define BYRP_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define BYRP_GET_R(base, R) PMIO_GET_R(base, R)

#define BYRP_SIZE_TEST_SET 3

static struct byrp_test_ctx {
	void *test_addr;
	struct is_byrp_config config;
	struct pmio_config pmio_config;
	struct pablo_mmio *pmio;
	struct pmio_field *pmio_fields;
	struct pmio_reg_seq *pmio_reg_seqs;
	struct is_frame frame;
	struct byrp_param_set param_set;
	struct is_param_region param_region;
} test_ctx;

static struct is_rectangle byrp_size_test_config[BYRP_SIZE_TEST_SET] = {
	{ 32, 16 }, /* min size */
	{ 8192, 15600 }, /* max size */
	{ 4032, 3024 }, /* customized size */
};

struct reg_set_test {
	u32 reg_name;
	u32 field_name;
	u32 value;
};

static struct reg_set_test byrp_bypass_test_list[] = { { BYRP_R_BYR_BITMASK0_BYPASS,
							       BYRP_F_BYR_BITMASK0_BYPASS, 0x0 },
	{ BYRP_R_BYR_GAMMASENSOR_BYPASS, BYRP_F_BYR_GAMMASENSOR_BYPASS, 0x1 },
	{ BYRP_R_BYR_BLCBYR_BYPASS, BYRP_F_BYR_BLCBYR_BYPASS, 0x1 },
	{ BYRP_R_BYR_AFIDENTBPC_BYPASS, BYRP_F_BYR_AFIDENTBPC_BYPASS, 0x1 },
	{ BYRP_R_BYR_BPCSUSPMAP_BYPASS, BYRP_F_BYR_BPCSUSPMAP_BYPASS, 0x1 },
	{ BYRP_R_BYR_BPCGGC_BYPASS, BYRP_F_BYR_BPCGGC_BYPASS, 0x1 },
	{ BYRP_R_BYR_BPCFLATDETECTOR_BYPASS, BYRP_F_BYR_BPCFLATDETECTOR_BYPASS, 0x1 },
	{ BYRP_R_BYR_BPCDIRDETECTOR_BYPASS, BYRP_F_BYR_BPCDIRDETECTOR_BYPASS, 0x1 },
	{ BYRP_R_BYR_DISPARITY_BYPASS, BYRP_F_BYR_DISPARITY_BYPASS, 0x1 },

	{ BYRP_R_BYR_PREDNS0_BYPASS, BYRP_F_BYR_PREDNS0_BYPASS, 0x1 },
	{ BYRP_R_BYR_BYRHDR_BYPASS, BYRP_F_BYR_BYRHDR_BYPASS, 0x1 },

	{ BYRP_R_BYR_CGRAS_BYPASS_REG, BYRP_F_BYR_CGRAS_BYPASS, 0x1 },
	{ BYRP_R_BYR_WBGDNG_BYPASS, BYRP_F_BYR_WBGDNG_BYPASS, 0x1 },
	{ BYRP_R_BYR_BLCDNG_BYPASS, BYRP_F_BYR_BLCDNG_BYPASS, 0x1 },

	{ BYRP_R_TETRA_SMCB0_CTRL, BYRP_F_TETRA_SMCB0_BYPASS, 0x1 },
	{ BYRP_R_BYR_THSTATPRE_BYPASS, BYRP_F_BYR_THSTATPRE_BYPASS, 0x1 },
	{ BYRP_R_TETRA_SMCB1_CTRL, BYRP_F_TETRA_SMCB1_BYPASS, 0x1 },
	{ BYRP_R_BYR_CDAF_BYPASS, BYRP_F_BYR_CDAF_BYPASS, 0x1 },
	{ BYRP_R_BYR_RGBYHIST_BYPASS, BYRP_F_BYR_RGBYHIST_BYPASS, 0x1 },
	{ BYRP_R_BYR_THSTATAE_BYPASS, BYRP_F_BYR_THSTATAE_BYPASS, 0x1 },
	{ BYRP_R_BYR_THSTATAWB_BYPASS, BYRP_F_BYR_THSTATAWB_BYPASS, 0x1 } };

static struct param_dma_output test_stat_output = {
	.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	.width = 320,
	.height = 240,
	.format = DMA_INOUT_FORMAT_Y,
	.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	.plane = DMA_INOUT_PLANE_2,
	.msb = 11,
};

static void pablo_hw_api_byrp_hw_dump_kunit_test(struct kunit *test)
{
	byrp_hw_dump(test_ctx.pmio, HW_DUMP_CR);
	byrp_hw_dump(test_ctx.pmio, HW_DUMP_DBG_STATE);
	byrp_hw_dump(test_ctx.pmio, HW_DUMP_MODE_NUM);
}

static void pablo_hw_api_byrp_hw_g_int_en_kunit_test(struct kunit *test)
{
	u32 int_en[PCC_INT_ID_NUM] = { 0 };

	byrp_hw_g_int_en(int_en);

	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_0], BYRP_INT0_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_1], BYRP_INT1_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_CMDQ_INT], 0);
	KUNIT_EXPECT_EQ(test, int_en[PCC_COREX_INT], 0);
}

#define BYRP_INT_GRP_EN_MASK                                                                       \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
static void pablo_hw_api_byrp_hw_g_int_grp_en_kunit_test(struct kunit *test)
{
	u32 int_grp_en;

	int_grp_en = byrp_hw_g_int_grp_en();

	KUNIT_EXPECT_EQ(test, int_grp_en, BYRP_INT_GRP_EN_MASK);
}

static void pablo_hw_api_byrp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 ret;
	int test_idx;

	byrp_hw_s_block_bypass(test_ctx.pmio);

	for (test_idx = 0; test_idx < ARRAY_SIZE(byrp_bypass_test_list); test_idx++) {
		ret = BYRP_GET_F(test_ctx.pmio, byrp_bypass_test_list[test_idx].reg_name,
			byrp_bypass_test_list[test_idx].field_name);
		KUNIT_EXPECT_EQ(test, ret, byrp_bypass_test_list[test_idx].value);
	}
}

static void pablo_hw_api_byrp_hw_wdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = BYRP_WDMA_BYR; test_idx < BYRP_WDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = byrp_hw_wdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* dma id err test */
	dma_id = -1;
	ret = byrp_hw_wdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_byrp_hw_rdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = BYRP_RDMA_IMG; test_idx < BYRP_RDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = byrp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* dma id err test */
	dma_id = -1;
	ret = byrp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_byrp_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	byrp_hw_s_strgen(test_ctx.pmio);

	/* STRGEN setting */
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_CINFIFO_CONFIG);
	expected_val = 0xff0060;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void __s_fro_kunit_test(struct kunit *test, u32 expected_val)
{
	KUNIT_EXPECT_EQ(test,
		BYRP_GET_F(test_ctx.pmio, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_THSTAT_PRE),
		expected_val);
	KUNIT_EXPECT_EQ(test,
		BYRP_GET_F(test_ctx.pmio, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_CDAF),
		expected_val);
	KUNIT_EXPECT_EQ(test,
		BYRP_GET_F(test_ctx.pmio, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_RGBYHIST),
		expected_val);
	KUNIT_EXPECT_EQ(test,
		BYRP_GET_F(test_ctx.pmio, BYRP_R_FRO_FRAME_NUM, BYRP_F_FRO_FRAME_NUM_THSTAT),
		expected_val);
}

static void pablo_hw_api_byrp_hw_s_core_kunit_test(struct kunit *test)
{
	u32 num_buffers, expected_val;
	struct byrp_param_set *param_set = &test_ctx.param_set;

	param_set->dma_input.msb = 10;

	num_buffers = 0;
	expected_val = 0;
	byrp_hw_s_core(test_ctx.pmio, num_buffers, param_set);
	__s_fro_kunit_test(test, expected_val);

	num_buffers = 1;
	expected_val = 0;
	byrp_hw_s_core(test_ctx.pmio, num_buffers, param_set);
	__s_fro_kunit_test(test, expected_val);

	num_buffers = 2;
	expected_val = 0;
	byrp_hw_s_core(test_ctx.pmio, num_buffers, param_set);
	__s_fro_kunit_test(test, expected_val);

	num_buffers = 3;
	expected_val = 1;
	byrp_hw_s_core(test_ctx.pmio, num_buffers, param_set);
	__s_fro_kunit_test(test, expected_val);
}

static void pablo_hw_api_byrp_hw_s_bitmask_kunit_test(struct kunit *test)
{
	u32 bit_in = 10;
	u32 bit_out = 14;
	u32 set_val;

	byrp_hw_s_bitmask(test_ctx.pmio, bit_in, bit_out);

	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_BITMASK0_BITTAGEIN);
	KUNIT_EXPECT_EQ(test, set_val, bit_in);
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_BITMASK0_BITTAGEOUT);
	KUNIT_EXPECT_EQ(test, set_val, bit_out);
}

static void pablo_hw_api_byrp_hw_is_occurred_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt0_list[] = { INTR0_BYRP_CMDQ_ERROR_INT, INTR0_BYRP_COREX_ERROR_INT,
				      INTR0_BYRP_VOTF_GLOBAL_ERROR_INT,
				      INTR0_BYRP_VOTF_LOST_CONNECTION_INT,
				      INTR0_BYRP_OTF_SEQ_ID_ERROR_INT };

	u32 warn_int0_list[] = { INTR0_BYRP_CINFIFO_0_ERROR_INT,  INTR0_BYRP_CINFIFO_1_ERROR_INT,
				 INTR0_BYRP_CINFIFO_2_ERROR_INT,  INTR0_BYRP_CINFIFO_3_ERROR_INT,
				 INTR0_BYRP_CINFIFO_4_ERROR_INT,  INTR0_BYRP_CINFIFO_5_ERROR_INT,
				 INTR0_BYRP_CINFIFO_6_ERROR_INT,  INTR0_BYRP_CINFIFO_7_ERROR_INT,
				 INTR0_BYRP_COUTFIFO_0_ERROR_INT, INTR0_BYRP_COUTFIFO_1_ERROR_INT,
				 INTR0_BYRP_COUTFIFO_2_ERROR_INT, INTR0_BYRP_COUTFIFO_3_ERROR_INT,
				 INTR0_BYRP_COUTFIFO_4_ERROR_INT, INTR0_BYRP_COUTFIFO_5_ERROR_INT };

	u32 err_interrupt1_list[] = { INTR1_BYRP_VOTFLOSTFLUSH, INTR1_BYRP_SBWCERROR,
		INTR1_BYRP_CPRDBGERRORLIC_DBG_CNT_ERR, INTR1_BYRP_CROPIN_DBG_CNT_ERR,
		INTR1_BYRP_DISTRIBUTOR_DBG_CNT_ERR, INTR1_BYRP_MERGE_POSTBPC_DBG_CNT_ERR,
		INTR1_BYRP_SPLIT_POST_BPC_DBG_CNT_ERR, INTR1_BYRP_SYNC4HDR_DBG_CNT_ERR,
		INTR1_BYRP_BYRHDR_DBG_CNT_ERR, INTR1_BYRP_SPLIT_CGRAS_STAT_DBG_CNT_ERR,
		INTR1_BYRP_CROPDNG_DBG_CNT_ERR, INTR1_BYRP_SMCB0_DCG_CNT_ERR,
		INTR1_BYRP_THSTATPRE_DBG_CNT_ERR, INTR1_BYRP_SPLIT_SMCB1_DBG_CNT_ERR,
		INTR1_BYRP_CDAF_DCG_CNT_ERR, INTR1_BYRP_RGBYHIST_DBG_CNT_ERR,
		INTR1_BYRP_THSTAT_DNG_CNT_ERR, INTR1_BYRP_LIC_INT_OVF, INTR1_BYRP_LIC_INT_ERR };

	kunit_err(test, "%d", __LINE__);
	status = 1 << INTR0_BYRP_FRAME_START_INT;
	ret = byrp_hw_is_occurred(status, INTR_FRAME_START);
	KUNIT_EXPECT_EQ(test, ret, status);
	kunit_err(test, "%d", __LINE__);

	status = 1 << INTR0_BYRP_ROW_COL_INT;
	ret = byrp_hw_is_occurred(status, INTR_FRAME_CINROW);
	KUNIT_EXPECT_EQ(test, ret, status);
	kunit_err(test, "%d", __LINE__);

	status = 1 << INTR0_BYRP_FRAME_END_INT;
	ret = byrp_hw_is_occurred(status, INTR_FRAME_END);
	KUNIT_EXPECT_EQ(test, ret, status);
	kunit_err(test, "%d", __LINE__);

	status = 1 << INTR0_BYRP_COREX_END_INT_0;
	ret = byrp_hw_is_occurred(status, INTR_COREX_END_0);
	KUNIT_EXPECT_EQ(test, ret, status);
	kunit_err(test, "%d", __LINE__);

	status = 1 << INTR0_BYRP_COREX_END_INT_1;
	ret = byrp_hw_is_occurred(status, INTR_COREX_END_1);
	KUNIT_EXPECT_EQ(test, ret, status);
	kunit_err(test, "%d", __LINE__);

	for (test_idx = 0; test_idx < ARRAY_SIZE(err_interrupt0_list); test_idx++) {
		status = 1 << err_interrupt0_list[test_idx];
		ret = byrp_hw_is_occurred(status, INTR_ERR0);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
	kunit_err(test, "%d", __LINE__);

	for (test_idx = 0; test_idx < ARRAY_SIZE(warn_int0_list); test_idx++) {
		status = 1 << warn_int0_list[test_idx];
		ret = byrp_hw_is_occurred(status, INTR_WARN0);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
	kunit_err(test, "%d", __LINE__);

	for (test_idx = 0; test_idx < ARRAY_SIZE(err_interrupt1_list); test_idx++) {
		status = 1 << err_interrupt1_list[test_idx];
		ret = byrp_hw_is_occurred(status, INTR_ERR1);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_byrp_hw_s_init_kunit_test(struct kunit *test)
{
	int ret;
	u32 ch;

	for (ch = 0; ch < 5; ch++) {
		ret = byrp_hw_s_init(test_ctx.pmio, ch);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}

static void pablo_hw_api_byrp_hw_wait_idle_kunit_test(struct kunit *test)
{
	int ret;

	ret = byrp_hw_wait_idle(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	BYRP_SET_F(test_ctx.pmio, BYRP_R_IDLENESS_STATUS, BYRP_F_IDLENESS_STATUS, 1);
	ret = byrp_hw_wait_idle(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_byrp_hw_s_grid_cfg_kunit_test(struct kunit *test)
{
	struct byrp_grid_cfg grid_cfg = {
		0,
	};

	byrp_hw_s_grid_cfg(test_ctx.pmio, &grid_cfg);
}

static void pablo_hw_api_byrp_hw_s_mcb_size_kunit_test(struct kunit *test)
{
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < BYRP_SIZE_TEST_SET; test_idx++) {
		width = byrp_size_test_config[test_idx].w;
		height = byrp_size_test_config[test_idx].h;

		byrp_hw_s_mcb_size(test_ctx.pmio, width, height);
	}
}

static void pablo_hw_api_byrp_hw_s_bcrop_size_kunit_test(struct kunit *test)
{
	u32 width, height;
	int test_idx, test_idx2;

	for (test_idx = 0; test_idx < BYRP_SIZE_TEST_SET; test_idx++) {
		width = byrp_size_test_config[test_idx].w;
		height = byrp_size_test_config[test_idx].h;

		for (test_idx2 = 0; test_idx2 < BYRP_BCROP_MAX; test_idx2++)
			byrp_hw_s_bcrop_size(test_ctx.pmio, test_idx2, 0, 0, width, height);
	}
}

static void pablo_hw_api_byrp_hw_s_chain_size_kunit_test(struct kunit *test)
{
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < BYRP_SIZE_TEST_SET; test_idx++) {
		width = byrp_size_test_config[test_idx].w;
		height = byrp_size_test_config[test_idx].h;

		byrp_hw_s_chain_size(test_ctx.pmio, width, height);
	}
}

static void pablo_hw_api_byrp_hw_s_path_kunit_test(struct kunit *test)
{
	struct byrp_param_set *param_set = &test_ctx.param_set;
	struct pablo_common_ctrl_frame_cfg frame_cfg;

	param_set->otf_input.cmd = OTF_INPUT_COMMAND_DISABLE;
	param_set->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	param_set->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	byrp_hw_s_path(test_ctx.pmio, param_set, &frame_cfg);
	KUNIT_EXPECT_EQ(test, BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_CINFIFO_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, BYRP_GET_R(test_ctx.pmio, BYRP_R_OTF_PLATFORM_INPUT_MUX_0TO3), 0);
	KUNIT_EXPECT_EQ(test, BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_COUTFIFO_ENABLE), 1);

	param_set->otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
	param_set->dma_input.cmd = DMA_INPUT_COMMAND_DISABLE;
	param_set->otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE;
	byrp_hw_s_path(test_ctx.pmio, param_set, &frame_cfg);
	KUNIT_EXPECT_EQ(test, BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_CINFIFO_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, BYRP_GET_R(test_ctx.pmio, BYRP_R_OTF_PLATFORM_INPUT_MUX_0TO3), 1);
	KUNIT_EXPECT_EQ(test, BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_COUTFIFO_ENABLE), 1);
}

static void pablo_hw_api_byrp_hw_g_dma_param_ptr_kunit_test(struct kunit *test)
{
	int ret;
	u32 rdma_param_max_cnt, wdma_param_max_cnt;
	char *name;
	struct param_dma_input *pdi;
	struct param_dma_output *pdo;
	dma_addr_t *dma_frame_dva;
	pdma_addr_t *param_set_dva;
	struct is_frame *dma_frame = &test_ctx.frame;
	struct byrp_param_set *param_set = &test_ctx.param_set;

	name = __getname();

	/* RDMA */
	rdma_param_max_cnt = byrp_hw_g_rdma_cfg_max_cnt();
	KUNIT_EXPECT_EQ(test, rdma_param_max_cnt, BYRP_RDMA_CFG_MAX);

	ret = byrp_hw_g_rdma_param_ptr(BYRP_RDMA_CFG_IMG, dma_frame, param_set, name,
		&dma_frame_dva, &pdi, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(
		test, (dma_addr_t *)dma_frame_dva, (dma_addr_t *)dma_frame->dvaddr_buffer);
	KUNIT_EXPECT_PTR_EQ(test, (struct param_dma_input *)pdi,
		(struct param_dma_input *)&param_set->dma_input);
	KUNIT_EXPECT_PTR_EQ(
		test, (pdma_addr_t *)param_set_dva, (pdma_addr_t *)param_set->input_dva);

	ret = byrp_hw_g_rdma_param_ptr(BYRP_RDMA_CFG_MAX, dma_frame, param_set, name,
		&dma_frame_dva, &pdi, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* WDMA */
	wdma_param_max_cnt = byrp_hw_g_wdma_cfg_max_cnt();
	KUNIT_EXPECT_EQ(test, wdma_param_max_cnt, BYRP_WDMA_CFG_MAX);

	ret = byrp_hw_g_wdma_param_ptr(BYRP_WDMA_CFG_BYR, dma_frame, param_set, name,
		&dma_frame_dva, &pdo, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(
		test, (dma_addr_t *)dma_frame_dva, (dma_addr_t *)dma_frame->dva_byrp_byr);
	KUNIT_EXPECT_PTR_EQ(test, (struct param_dma_output *)pdo,
		(struct param_dma_output *)&param_set->dma_output_byr);
	KUNIT_EXPECT_PTR_EQ(
		test, (pdma_addr_t *)param_set_dva, (pdma_addr_t *)param_set->output_dva_byr);

	ret = byrp_hw_g_wdma_param_ptr(BYRP_WDMA_CFG_MAX, dma_frame, param_set, name,
		&dma_frame_dva, &pdo, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__putname(name);
}

static void pablo_hw_api_byrp_hw_s_external_shot_kunit_test(struct kunit *test)
{
	struct byrp_param_set *byrp_param_set = &test_ctx.param_set;
	struct byrp_param *byrp_param = &test_ctx.param_region.byrp;

	IS_DECLARE_PMAP(pmap);

	byrp_param->otf_input.cmd = 1;
	byrp_param->dma_input.cmd = 1;
	byrp_param->otf_output.cmd = 1;
	byrp_param->dma_output_byr.cmd = 1;

	byrp_hw_s_external_shot(&test_ctx.param_region, pmap, byrp_param_set);
	KUNIT_EXPECT_NE(test, byrp_param_set->otf_input.cmd, byrp_param->otf_input.cmd);
	KUNIT_EXPECT_NE(test, byrp_param_set->dma_input.cmd, byrp_param->dma_input.cmd);
	KUNIT_EXPECT_NE(test, byrp_param_set->otf_output.cmd, byrp_param->otf_output.cmd);
	KUNIT_EXPECT_NE(test, byrp_param_set->dma_output_byr.cmd, byrp_param->dma_output_byr.cmd);

	set_bit(PARAM_BYRP_OTF_INPUT, pmap);
	set_bit(PARAM_BYRP_DMA_INPUT, pmap);
	set_bit(PARAM_BYRP_OTF_OUTPUT, pmap);
	set_bit(PARAM_BYRP_BYR, pmap);

	byrp_hw_s_external_shot(&test_ctx.param_region, pmap, byrp_param_set);
	KUNIT_EXPECT_EQ(test, byrp_param_set->otf_input.cmd, byrp_param->otf_input.cmd);
	KUNIT_EXPECT_EQ(test, byrp_param_set->dma_input.cmd, byrp_param->dma_input.cmd);
	KUNIT_EXPECT_EQ(test, byrp_param_set->otf_output.cmd, byrp_param->otf_output.cmd);
	KUNIT_EXPECT_EQ(test, byrp_param_set->dma_output_byr.cmd, byrp_param->dma_output_byr.cmd);
}

static void pablo_hw_api_byrp_hw_cotf_error_handle_kunit_test(struct kunit *test)
{
	u32 input = 0xFF;
	u32 output;

	BYRP_SET_R(test_ctx.pmio, BYRP_R_BYR_CINFIFO_INT, input);
	BYRP_SET_R(test_ctx.pmio, BYRP_R_BYR_COUTFIFO_INT, input);

	byrp_hw_cotf_error_handle(test_ctx.pmio);

	output = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_CINFIFO_INT_CLEAR);
	KUNIT_EXPECT_EQ(test, output, input);
	output = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_COUTFIFO_INT_CLEAR);
	KUNIT_EXPECT_EQ(test, output, input);
}

static bool __byrp_hw_get_stat_wdma(
	struct is_common_dma *dma, struct byrp_param_set *param_set, u32 id, struct kunit *test)
{
	u32 enable = 0, width = 0, height = 0;
	struct is_frame *frame = &test_ctx.frame;
	bool flag;

	dma->id = id;
	switch (dma->id) {
	case BYRP_WDMA_RGBYHIST:
		param_set->dma_output_rgby = test_stat_output;
		break;
	case BYRP_WDMA_THSTAT_AWB:
		param_set->dma_output_awb = test_stat_output;
		break;
	}

	byrp_hw_s_wdma_init(dma, param_set, frame->num_buffers);

	CALL_DMA_OPS(dma, dma_get_enable, &enable);
	flag = enable == DMA_OUTPUT_COMMAND_ENABLE;
	KUNIT_EXPECT_TRUE(test, flag);

	CALL_DMA_OPS(dma, dma_get_size, &width, &height);
	flag = flag && (test_stat_output.width == width) && (test_stat_output.height == height);
	KUNIT_EXPECT_TRUE(test, flag);

	return flag;
}

static void pablo_hw_api_byrp_hw_s_wdma_init_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	struct byrp_param_set *param_set = &test_ctx.param_set;
	bool utc_flag;

	/* init dma*/
	dma.base = test_ctx.pmio;
	byrp_hw_wdma_create(&dma, test_ctx.pmio, BYRP_WDMA_BYR);
	dma.set_id = COREX_DIRECT;

	/* TC0: case rgbyhist*/
	utc_flag = __byrp_hw_get_stat_wdma(&dma, param_set, BYRP_WDMA_RGBYHIST, test);

	/* TC1: case rgbyhist*/
	utc_flag |= __byrp_hw_get_stat_wdma(&dma, param_set, BYRP_WDMA_THSTAT_AWB, test);

	set_utc_result(KUTC_BYRP_STAT_WDMA, UTC_ID_BYRP_STAT_WDMA, utc_flag);
}

static struct kunit_case pablo_hw_api_byrp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_byrp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_int_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_int_grp_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_wdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_bitmask_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_grid_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_mcb_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_bcrop_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_chain_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_dma_param_ptr_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_external_shot_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_cotf_error_handle_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_wdma_init_kunit_test),
	{},
};

static int __pablo_hw_api_byrp_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "byrp";

	test_ctx.pmio_config.mmio_base = test_ctx.test_addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	byrp_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);
	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init byrp PMIO: %ld", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
		test_ctx.pmio_config.fields, test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc byrp PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;
	}

	test_ctx.pmio_reg_seqs =
		kunit_kzalloc(test, sizeof(struct pmio_reg_seq) * byrp_hw_g_reg_cnt(), 0);
	if (!test_ctx.pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		pmio_exit(test_ctx.pmio);
		return -ENOMEM;
	}

	return ret;
}

static void __pablo_hw_api_byrp_pmio_deinit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pmio_reg_seqs);
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_byrp_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	ret = __pablo_hw_api_byrp_pmio_init(test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_api_byrp_kunit_test_exit(struct kunit *test)
{
	__pablo_hw_api_byrp_pmio_deinit(test);

	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(struct byrp_test_ctx));
}

struct kunit_suite pablo_hw_api_byrp_kunit_test_suite = {
	.name = "pablo-hw-api-byrp-v4_0-kunit-test",
	.init = pablo_hw_api_byrp_kunit_test_init,
	.exit = pablo_hw_api_byrp_kunit_test_exit,
	.test_cases = pablo_hw_api_byrp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_byrp_kunit_test_suite);

MODULE_LICENSE("GPL");
