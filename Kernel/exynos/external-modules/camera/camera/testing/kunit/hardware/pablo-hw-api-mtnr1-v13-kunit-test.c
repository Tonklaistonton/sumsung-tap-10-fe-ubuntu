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

#include "hardware/api/is-hw-api-mtnr1-v13.h"
#include "hardware/sfr/is-sfr-mtnr1-v13_0.h"

#define MTNR1_GET_F(base, R, F)		PMIO_GET_F(base, R, F)

static struct pablo_hw_api_mtnr1_kunit_test_ctx {
	struct is_common_dma dma;
	struct param_dma_input dma_input;
	struct param_stripe_input stripe_input;
	u32 width, height;
	u32 sbwc_en, payload_size, strip_offset, header_offset;
	struct is_mtnr1_config config;
	void *addr;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field *pmio_fields;
} test_ctx;

static void pablo_hw_api_mtnr1_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mtnr1_kunit_test_ctx *tctx = test->priv;

	mtnr1_hw_dump(tctx->pmio, HW_DUMP_CR);
	mtnr1_hw_dump(tctx->pmio, HW_DUMP_DBG_STATE);
	mtnr1_hw_dump(tctx->pmio, HW_DUMP_MODE_NUM);
}

static void pablo_hw_api_mtnr1_hw_s_rdma_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mtnr1_kunit_test_ctx *tctx = test->priv;
	int ret;

	tctx->dma.id = MTNR1_RDMA_CUR_L1_Y;
	tctx->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	tctx->dma_input.format = DMA_INOUT_FORMAT_YUV422;
	tctx->dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	tctx->dma_input.sbwc_type = DMA_INPUT_SBWC_DISABLE;
	tctx->dma_input.msb = tctx->dma_input.bitwidth - 1;
	ret = mtnr1_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	KUNIT_EXPECT_EQ(test, 0, ret);

	tctx->dma.id = MTNR1_RDMA_CUR_L4_Y;
	ret = mtnr1_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mtnr1_hw_rdma_create_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mtnr1_kunit_test_ctx *tctx = test->priv;
	int ret;

	tctx->dma.id = MTNR1_RDMA_CUR_L1_Y;
	tctx->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	tctx->dma_input.format = DMA_INOUT_FORMAT_YUV422;
	tctx->dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	tctx->dma_input.sbwc_type = DMA_INPUT_SBWC_DISABLE;
	tctx->dma_input.msb = tctx->dma_input.bitwidth - 1;
	ret = mtnr1_hw_rdma_create(&tctx->dma, tctx->addr, MTNR1_RDMA_CUR_L1_Y);
	KUNIT_EXPECT_EQ(test, 0, ret);

	tctx->dma.id = MTNR1_RDMA_CUR_L4_Y;
	ret = mtnr1_hw_rdma_create(&tctx->dma, tctx->addr, MTNR1_RDMA_CUR_L4_Y);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mtnr1_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mtnr1_kunit_test_ctx *tctx = test->priv;
	int ret;

	tctx->dma.id = MTNR1_RDMA_CUR_L1_Y;
	tctx->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	tctx->dma_input.format = DMA_INOUT_FORMAT_YUV422;
	tctx->dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	tctx->dma_input.sbwc_type = DMA_INPUT_SBWC_DISABLE;
	tctx->dma_input.msb = tctx->dma_input.bitwidth - 1;
	ret = mtnr1_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	ret = mtnr1_hw_s_rdma_addr(&tctx->dma, tctx->addr, 0, 1, 0,
		tctx->sbwc_en, tctx->payload_size, tctx->strip_offset, tctx->header_offset);
	KUNIT_EXPECT_EQ(test, 0, ret);

	tctx->dma.id = MTNR1_RDMA_CUR_L4_Y;
	ret = mtnr1_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	ret = mtnr1_hw_s_rdma_addr(&tctx->dma, tctx->addr, 0, 1, 0,
		tctx->sbwc_en, tctx->payload_size, tctx->strip_offset, tctx->header_offset);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mtnr1_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	struct pablo_hw_api_mtnr1_kunit_test_ctx *tctx = test->priv;

	mtnr1_hw_s_strgen(tctx->pmio, 0);

	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
			MTNR1_F_STAT_CINFIFODLFEWGT_STRGEN_MODE_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
			MTNR1_F_STAT_CINFIFODLFEWGT_STRGEN_MODE_DATA_TYPE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
			MTNR1_F_STAT_CINFIFODLFEWGT_STRGEN_MODE_DATA);
	expected_val = 255;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_IP_USE_CINFIFO_NEW_FRAME_IN,
			MTNR1_F_IP_USE_CINFIFO_NEW_FRAME_IN);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

#define CONV_INT10_T(_v) (_v & 0x3FF)
static void pablo_hw_api_mtnr1_hw_s_mvf_resize_offset_kunit_test(struct kunit *test)
{
	int set_val, expected_val;
	struct pablo_hw_api_mtnr1_kunit_test_ctx *tctx = test->priv;

	struct _size {
		u32 w;
		u32 h;
	} in = { 128, 80, }, out = { 255, 120 };
	u32 pos;

	/* TC1: 6.1 8160x6120 */
	/* strip #1 */
	in.w = 128;
	out.w = 255;
	pos = 0;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = -63;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_Y);
	expected_val = -42;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* strip #2 */
	pos = 3584;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 49;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* TC2: 6.2 12000x9000 */
	/* strip #1 */
	in.w = 125;
	out.w = 375;
	pos = 0;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = -85;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* strip #2 */
	pos = 2560;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 59;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* strip #3 */
	pos = 5632;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 27;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* strip #4 */
	pos = 8704;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 251;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* TC3: not resized */
	/* strip #1 */
	in.w = 125;
	out.w = 125;
	pos = 0;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* strip #2 */
	pos = 2560;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* strip #3 */
	pos = 5632;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));

	/* strip #4 */
	pos = 8704;

	mtnr1_hw_s_mvf_resize_offset(tctx->pmio, 0, in.w, in.h, out.w, out.h, pos);
	set_val = MTNR1_GET_F(tctx->pmio, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, CONV_INT10_T(expected_val));
}

static int __pablo_hw_api_mtnr1_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "mtnr1";

	test_ctx.pmio_config.mmio_base = test_ctx.addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	mtnr1_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);

	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init mtnr1 PMIO: %ld", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
			test_ctx.pmio_config.fields,
			test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc mtnr1 PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;

	}

	return ret;
}

static void __pablo_hw_api_mtnr1_pmio_deinit(struct kunit *test)
{
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_mtnr1_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.width = 320;
	test_ctx.height = 240;
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);
	test->priv = &test_ctx;

	ret = __pablo_hw_api_mtnr1_pmio_init(test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_api_mtnr1_kunit_test_exit(struct kunit *test)
{
	__pablo_hw_api_mtnr1_pmio_deinit(test);

	kunit_kfree(test, test_ctx.addr);
	memset(&test_ctx, 0, sizeof(struct pablo_hw_api_mtnr1_kunit_test_ctx));
}

static struct kunit_case pablo_hw_api_mtnr1_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_mtnr1_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_mtnr1_hw_s_rdma_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_mtnr1_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_mtnr1_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_mtnr1_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_mtnr1_hw_s_mvf_resize_offset_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_mtnr1_kunit_test_suite = {
	.name = "pablo-hw-api-mtnr1-v13-kunit-test",
	.init = pablo_hw_api_mtnr1_kunit_test_init,
	.exit = pablo_hw_api_mtnr1_kunit_test_exit,
	.test_cases = pablo_hw_api_mtnr1_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_mtnr1_kunit_test_suite);

MODULE_LICENSE("GPL");
