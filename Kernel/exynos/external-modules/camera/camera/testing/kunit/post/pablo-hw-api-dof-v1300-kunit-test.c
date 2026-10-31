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

#include "dof/pablo-dof.h"
#include "dof/pablo-hw-api-dof.h"
#include "dof/pablo-hw-reg-dof-v13_0_0.h"
#include "is-common-enum.h"
#include "is-type.h"
#include "is-hw-common-dma.h"

#define DOF_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define DOF_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define DOF_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define DOF_GET_R(base, R) PMIO_GET_R(base, R)

static struct camerapp_hw_api_dof_kunit_test_ctx {
	void *test_addr;
	struct pablo_mmio *pmio;
	struct dof_ctx *ctx;
	struct dof_dev *dof;
	struct resource rsc;
} test_ctx;

const struct dof_variant dof_variant = {
	.limit_input = {
		.min_w		= 64,
		.min_h		= 64,
		.max_w		= 1024,
		.max_h		= 768,
	},
	.version		= 0x13000000,
};

static const struct dof_fmt dof_formats[] = {
	{
		.name = "GREY",
		.pixelformat = V4L2_PIX_FMT_GREY,
		.cfg_val = DOF_CFG_FMT_GREY,
		.bitperpixel = { 8 },
		.num_planes = 1,
		.num_comp = 1,
		.h_shift = 1,
		.v_shift = 1,
	},
};

#define DOF_TEST_DMA_ADDR 0xDEADDEADF

/* Define the test cases. */
static void camerapp_hw_api_dof_get_size_constraints_kunit_test(struct kunit *test)
{
	const struct dof_variant *constraints;
	u32 val;

	DOF_SET_R(test_ctx.pmio, DOF_R_IP_VERSION, dof_variant.version);

	constraints = camerapp_hw_dof_get_size_constraints(test_ctx.pmio);
	val = camerapp_hw_dof_get_ver(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_w, dof_variant.limit_input.min_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_h, dof_variant.limit_input.min_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_w, dof_variant.limit_input.max_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_h, dof_variant.limit_input.max_h);
	KUNIT_EXPECT_EQ(test, constraints->version, dof_variant.version);
	KUNIT_EXPECT_EQ(test, constraints->version, val);
}

static void camerapp_hw_api_dof_start_kunit_test(struct kunit *test)
{
	struct c_loader_buffer clb;
	u32 val;

	/* APB-DIRECT */
	camerapp_hw_dof_start(test_ctx.pmio, &clb);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_CMDQ_QUE_CMD_M, DOF_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)3);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	/* C-LOADER */
	clb.header_dva = 0xDEADDEAD;
	clb.num_of_headers = 5;
	camerapp_hw_dof_start(test_ctx.pmio, &clb);

	val = DOF_GET_R(test_ctx.pmio, DOF_R_CMDQ_QUE_CMD_H);
	KUNIT_EXPECT_EQ(test, val, (u32)(clb.header_dva >> 4));
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CMDQ_QUE_CMD_M);
	KUNIT_EXPECT_EQ(test, val, (u32)((1 << 12) | clb.num_of_headers));
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CMDQ_QUE_CMD_M, DOF_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_api_dof_stop_kunit_test(struct kunit *test)
{
	camerapp_hw_dof_stop(test_ctx.pmio);
}

static void camerapp_hw_api_dof_sw_reset_kunit_test(struct kunit *test)
{
	u32 try_count = DOF_TRY_COUNT + 1;
	u32 val;

	/* separate tc */
	val = camerapp_hw_dof_dma_reset(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, val, try_count);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_TRANS_STOP_REQ);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	val = camerapp_hw_dof_core_reset(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, val, try_count);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_SW_RESET);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	/* total tc */
	DOF_SET_R(test_ctx.pmio, DOF_R_TRANS_STOP_REQ_RDY, 1);
	val = camerapp_hw_dof_sw_reset(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, val, (u32)try_count);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_TRANS_STOP_REQ);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_SW_RESET);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_dof_clear_intr_all_kunit_test(struct kunit *test)
{
	u32 val;

	camerapp_hw_dof_clear_intr_all(test_ctx.pmio);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_INT_REQ_INT0_CLEAR, DOF_F_INT_REQ_INT0_CLEAR);
	KUNIT_EXPECT_EQ(test, val, DOF_INT_EN_MASK);
}

static void camerapp_hw_dof_get_intr_status_and_clear_kunit_test(struct kunit *test)
{
	u32 val0, c0, c1;
	u32 int0 = 0xAAAABBBB;
	u32 int1 = 0xCCCCDDDD;

	DOF_SET_R(test_ctx.pmio, DOF_R_INT_REQ_INT0, int0);
	DOF_SET_R(test_ctx.pmio, DOF_R_INT_REQ_INT1, int1);

	val0 = camerapp_hw_dof_get_intr_status_and_clear(test_ctx.pmio);

	c0 = DOF_GET_R(test_ctx.pmio, DOF_R_INT_REQ_INT0_CLEAR);
	c1 = DOF_GET_R(test_ctx.pmio, DOF_R_INT_REQ_INT1_CLEAR);

	KUNIT_EXPECT_EQ(test, val0, int0);
	KUNIT_EXPECT_EQ(test, c0, int0);
	KUNIT_EXPECT_EQ(test, c1, int1);
}

static void camerapp_hw_dof_get_fs_fe_err_kunit_test(struct kunit *test)
{
	u32 val1, val2, val3;

	val1 = camerapp_hw_dof_get_int_frame_start();
	val2 = camerapp_hw_dof_get_int_frame_end();
	val3 = camerapp_hw_dof_get_int_err();

	KUNIT_EXPECT_EQ(test, DOF_INT_FRAME_START, val1);
	KUNIT_EXPECT_EQ(test, DOF_INT_FRAME_END, val2);
	KUNIT_EXPECT_EQ(test, DOF_INT_ERR, val3);
}

static void camerapp_hw_dof_wait_idle_kunit_test(struct kunit *test)
{
	int val;

	val = camerapp_hw_dof_wait_idle(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, val, -ETIME);

	DOF_SET_F(test_ctx.pmio, DOF_R_IDLENESS_STATUS, DOF_F_IDLENESS_STATUS, 1);
	val = camerapp_hw_dof_wait_idle(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, val, 0);
}

static void camerapp_hw_dof_set_initialization_kunit_test(struct kunit *test)
{
	u32 val;

	camerapp_hw_dof_set_initialization(test_ctx.pmio);

	/* camerapp_hw_dof_set_clock */
	val = DOF_GET_R(test_ctx.pmio, DOF_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	/* camerapp_hw_dof_set_init */
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CMDQ_VHD_CONTROL, DOF_F_CMDQ_VHD_VBLANK_QRUN_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_F(
		test_ctx.pmio, DOF_R_CMDQ_VHD_CONTROL, DOF_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_DEBUG_CLOCK_ENABLE, DOF_F_DEBUG_CLOCK_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = DOF_GET_R(test_ctx.pmio, DOF_R_C_LOADER_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_STAT_RDMACL_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_CMDQ_QUE_CMD_L, DOF_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0xbf); /* DOF_INT_GRP_EN_MASK */
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CMDQ_QUE_CMD_M, DOF_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)3);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	/* camerapp_hw_dof_set_core */
	val = DOF_GET_R(test_ctx.pmio, DOF_R_IP_USE_OTF_PATH_01);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_IP_USE_OTF_PATH_23);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_IP_USE_OTF_PATH_45);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_IP_USE_OTF_PATH_67);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_IP_USE_CINFIFO_NEW_FRAME_IN,
		DOF_F_IP_USE_CINFIFO_NEW_FRAME_IN);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = DOF_GET_R(test_ctx.pmio, DOF_R_INT_REQ_INT0_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)DOF_INT_EN_MASK);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_SECU_CTRL_SEQID, DOF_F_SECU_CTRL_SEQID);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_MASK, DOF_F_CFG_BUFFER_MASK_CRC);
	KUNIT_EXPECT_EQ(test, val, (u32)8);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_MASK, DOF_F_CFG_MASK_STREAM_STATISTICS);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void camerapp_hw_api_dof_sfr_dump_kunit_test(struct kunit *test)
{
	camerapp_hw_dof_sfr_dump(test_ctx.pmio);
}

static void camerapp_hw_api_dof_update_debug_info_kunit_test(struct kunit *test)
{
	struct dof_debug_info *debug_info;
	u32 ref = 0x12345678;
	u32 val;
	int i;

	debug_info = &(test_ctx.ctx->model_param.debug_info);
	debug_info->regs.num_reg = 30;
	for (i = 0; i < debug_info->regs.num_reg; i++) {
		debug_info->regs.reg_data[i].addr = DOF_R_STS_CURRENT_LAYER + 4 * i;
		DOF_SET_R(test_ctx.pmio, debug_info->regs.reg_data[i].addr, ref);
	}
	camerapp_hw_dof_update_debug_info(test_ctx.pmio, debug_info, 1, DEVICE_STATUS_TIMEOUT);

	KUNIT_EXPECT_EQ(test, (u32)debug_info->buffer_index, (u32)1);
	KUNIT_EXPECT_EQ(test, (u32)debug_info->device_status, (u32)DEVICE_STATUS_TIMEOUT);
	for (i = 0; i < debug_info->regs.num_reg; i++) {
		val = DOF_GET_R(test_ctx.pmio, DOF_R_STS_CURRENT_LAYER + 4 * i);
		KUNIT_EXPECT_EQ(test, val, ref);
	}
}

static void camerapp_hw_dof_update_ctx(struct dof_ctx *ctx, bool is_first)
{
	ctx->s_frame.addr.curr_in = DOF_TEST_DMA_ADDR;
	ctx->s_frame.addr.prev_in = DOF_TEST_DMA_ADDR;
	ctx->s_frame.addr.prev_state = DOF_TEST_DMA_ADDR;
	ctx->d_frame.addr.output = DOF_TEST_DMA_ADDR;
	ctx->d_frame.addr.next_state = DOF_TEST_DMA_ADDR;
	ctx->model_addr.dva_instruction_with_offset = DOF_TEST_DMA_ADDR;
	ctx->model_addr.dva_constant_with_offset = DOF_TEST_DMA_ADDR;
	ctx->model_addr.dva_temporary = DOF_TEST_DMA_ADDR;
	ctx->dof_dev = test_ctx.dof;
}

static void camerapp_hw_dof_check_block_reg(struct kunit *test)
{
	int debug_rdmo = 0;
	int debug_wrmo = 0;
	int val;

	/* TODO : using dof_get_debug_rdmo,, dof_get_debug_wrmo*/

	val = DOF_GET_F(test_ctx.pmio, DOF_R_IP_CFG, DOF_F_Y_DOF_IP_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_BYPASS_PREFETCH, DOF_F_CFG_ZERO_SKIP_BYPASS);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_BYPASS_PREFETCH, DOF_F_CFG_PREFETCH);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_BYPASS_PREFETCH, DOF_F_CFG_PREFETCH_SIZE);
	KUNIT_EXPECT_EQ(test, val, (u32)0x10);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_DATA_SWAP, DOF_F_CFG_RD_DATA_INPUT0);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_DATA_SWAP, DOF_F_CFG_RD_DATA_INPUT1);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_DATA_SWAP, DOF_F_CFG_RD_DATA_OUTPUT);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_MAX_OS, DOF_F_CFG_MAX_RD_OS);
	KUNIT_EXPECT_EQ(test, val, debug_rdmo);
	val = DOF_GET_F(test_ctx.pmio, DOF_R_CFG_MAX_OS, DOF_F_CFG_MAX_WR_OS);
	KUNIT_EXPECT_EQ(test, val, debug_wrmo);
}

static void camerapp_hw_dof_check_dma_address(struct kunit *test)
{
	int val;
	u32 ref_msb = DOF_MSB(DOF_TEST_DMA_ADDR);
	u32 ref_lsb = DOF_LSB(DOF_TEST_DMA_ADDR);

	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_INPUT0_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_INPUT0_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_INPUT1_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_INPUT1_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_PSTATE_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_PSTATE_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_OUTPUT_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_OUTPUT_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_NSTATE_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_NSTATE_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_INST_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_INST_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_CONST_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_CONST_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_TEMP_BASE_ADDR_0_MSB);
	KUNIT_EXPECT_EQ(test, val, ref_msb);
	val = DOF_GET_R(test_ctx.pmio, DOF_R_CFG_TEMP_BASE_ADDR_0_LSB);
	KUNIT_EXPECT_EQ(test, val, ref_lsb);
}

static void camerapp_hw_dof_update_param_kunit_test(struct kunit *test)
{
	camerapp_hw_dof_update_ctx(test_ctx.ctx, true);

	/* run test */
	camerapp_hw_dof_update_param(test_ctx.pmio, test_ctx.ctx);

	/* check */
	camerapp_hw_dof_check_block_reg(test);
	camerapp_hw_dof_check_dma_address(test);
}

static void camerapp_hw_dof_init_pmio_config_kunit_test(struct kunit *test)
{
	camerapp_hw_dof_init_pmio_config(test_ctx.dof);
}

static void camerapp_hw_dof_get_reg_cnt_kunit_test(struct kunit *test)
{
	u32 reg_cnt;

	reg_cnt = camerapp_hw_dof_get_reg_cnt();
	KUNIT_EXPECT_EQ(test, reg_cnt, (u32)DOF_REG_CNT);
}

static struct pablo_mmio *pablo_hw_api_dof_pmio_init(struct kunit *test)
{
	int ret;
	struct pablo_mmio *pmio;
	struct pmio_config *pcfg;

	pcfg = &test_ctx.dof->pmio_config;
	camerapp_hw_dof_init_pmio_config(test_ctx.dof);
	pcfg->cache_type = PMIO_CACHE_NONE;

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	ret = pmio_field_bulk_alloc(
		pmio, &test_ctx.dof->pmio_fields, pcfg->fields, pcfg->num_fields);
	if (ret)
		goto err_field_bulk_alloc;

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
	pmio = ERR_PTR(ret);
err_init:
	return pmio;
}

static void pablo_hw_api_dof_pmio_deinit(struct kunit *test)
{
	if (test_ctx.dof->pmio_fields) {
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.dof->pmio_fields);
		test_ctx.dof->pmio_fields = NULL;
	}

	if (test_ctx.pmio) {
		pmio_exit(test_ctx.pmio);
		test_ctx.pmio = NULL;
	}
}

static struct kunit_case camerapp_hw_api_dof_kunit_test_cases[] = {
	KUNIT_CASE(camerapp_hw_api_dof_get_size_constraints_kunit_test),
	KUNIT_CASE(camerapp_hw_api_dof_start_kunit_test),
	KUNIT_CASE(camerapp_hw_api_dof_stop_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_clear_intr_all_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_get_intr_status_and_clear_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_get_fs_fe_err_kunit_test),
	KUNIT_CASE(camerapp_hw_api_dof_sw_reset_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_wait_idle_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_set_initialization_kunit_test),
	KUNIT_CASE(camerapp_hw_api_dof_sfr_dump_kunit_test),
	KUNIT_CASE(camerapp_hw_api_dof_update_debug_info_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_update_param_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_init_pmio_config_kunit_test),
	KUNIT_CASE(camerapp_hw_dof_get_reg_cnt_kunit_test),
	{},
};

static int camerapp_hw_api_dof_kunit_test_init(struct kunit *test)
{
	test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	test_ctx.dof = kunit_kzalloc(test, sizeof(struct dof_dev), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.dof);

	test_ctx.ctx = kunit_kzalloc(test, sizeof(struct dof_ctx), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.ctx);

	test_ctx.dof->regs_base = test_ctx.test_addr;
	test_ctx.dof->regs_rsc = &test_ctx.rsc;

	test_ctx.pmio = pablo_hw_api_dof_pmio_init(test);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.pmio);

	return 0;
}

static void camerapp_hw_api_dof_kunit_test_exit(struct kunit *test)
{
	pablo_hw_api_dof_pmio_deinit(test);
	kunit_kfree(test, test_ctx.ctx);
	kunit_kfree(test, test_ctx.dof);
	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(struct camerapp_hw_api_dof_kunit_test_ctx));
}

struct kunit_suite camerapp_hw_api_dof_kunit_test_suite = {
	.name = "pablo-hw-api-dof-v1300-kunit-test",
	.init = camerapp_hw_api_dof_kunit_test_init,
	.exit = camerapp_hw_api_dof_kunit_test_exit,
	.test_cases = camerapp_hw_api_dof_kunit_test_cases,
};
define_pablo_kunit_test_suites(&camerapp_hw_api_dof_kunit_test_suite);

MODULE_LICENSE("GPL");
