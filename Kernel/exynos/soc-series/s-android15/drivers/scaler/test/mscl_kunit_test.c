/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Kunit of Samsung EXYNOS Scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <kunit/test.h>
#include <kunit/visibility.h>

#include "../scaler.h"
#include "../scaler-regs.h"
#include "mscl_kunit_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct sc_ctx scaler_ctx;
struct sc_dev scaler_dev;
struct sc_tws scaler_tws;
struct sc_votf_target scaler_votf_table[10];
char scaler_virtual_base_sfr[0x3000];
char scaler_virtual_votf_sfr[0x10000];
char scaler_virtual_votf_target_sfr[0x1000];
struct sc_qos_table scaler_qos_table[3];
struct sc_ctx scaler_ctx_prio[4];

static int mscl_test_init(struct kunit *test)
{
	int i = 0;
	kunit_info(test, "%s", __func__);

	memset(&scaler_ctx, 0, sizeof(scaler_ctx));
	memset(&scaler_dev, 0, sizeof(scaler_dev));
	memset(&scaler_tws, 0, sizeof(scaler_tws));
	scaler_dev.regs = scaler_virtual_base_sfr;
	scaler_dev.votf_regs = scaler_virtual_votf_sfr;
	scaler_dev.current_ctx = &scaler_ctx;
	scaler_dev.votf_table = scaler_votf_table;
	for (i = 0; i < 10; i++) {
		scaler_votf_table[i].regs = scaler_virtual_votf_target_sfr;
	}
	scaler_ctx.tws = &scaler_tws;
	scaler_tws.sc_dev = &scaler_dev;
	return 0;
}

static void mscl_test_exit(struct kunit *test)
{
	kunit_info(test, "%s", __func__);
}

static void mscl_test_ext_buf_size(struct kunit *test)
{
	u32 result = 0;
	int width_aligned, width_unaligned;

	kunit_info(test, "%s", __func__);

	width_aligned = 128;
	width_unaligned = 100;

	result = sc_ext_buf_size(width_aligned);
	KUNIT_EXPECT_EQ(test, 0, result);

	result = sc_ext_buf_size(width_unaligned);
	KUNIT_EXPECT_EQ(test, 512, result);
}

static void mscl_test_get_blend_value(struct kunit *test)
{
	unsigned int cfg = 0;

	kunit_info(test, "%s", __func__);

	get_blend_value(&cfg, 0xff, false);
	KUNIT_EXPECT_EQ(test, cfg, 0x90000000);

	cfg = SCALER_SEL_INV_MASK | SCALER_SEL_MASK;
	get_blend_value(&cfg, 0x5, false);
	KUNIT_EXPECT_EQ(test, cfg, 0x45000000);

	cfg = SCALER_OP_SEL_INV_MASK | SCALER_OP_SEL_MASK;
	get_blend_value(&cfg, 0x11, true);
	KUNIT_EXPECT_EQ(test, cfg, 0x31000000);
}

static void mscl_test_sc_coef_adjust(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	KUNIT_EXPECT_EQ(test, sc_coef_adjust(0x01BC0038), 0x006F000E);
}

static void mscl_test_sc_hwset_blend(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	writel(SCALER_CFG_FILL_EN, scaler_dev.regs + SCALER_CFG);

	sc_hwset_blend(&scaler_dev, BL_OP_SRC, true, 0x11);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_CFG), 0x1010000);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_SRC_BLEND_COLOR), 0x25000000);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_SRC_BLEND_ALPHA), 0x25000011);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_DST_BLEND_COLOR), 0x97000000);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_DST_BLEND_ALPHA), 0x97000000);

	sc_hwset_blend(&scaler_dev, BL_OP_DST_OVER, false, 0xff);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_CFG), 0x1030000);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_SRC_BLEND_COLOR), 0x73000000);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_SRC_BLEND_ALPHA), 0x20000011);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_DST_BLEND_COLOR), 0x40000000);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_DST_BLEND_ALPHA), 0x31000000);
}

static void mscl_test_sc_hwset_clear_votf_clock_en(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	writel(0x1, scaler_dev.votf_regs + MSCL_VOTF_RING_CLOCK_EN);
	KUNIT_EXPECT_EQ(test, sc_hwset_clear_votf_clock_en(&scaler_tws), true);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_RING_CLOCK_EN), 0x1);

	atomic_set(&scaler_dev.votf_ref_count, 1);
	KUNIT_EXPECT_EQ(test, sc_hwset_clear_votf_clock_en(&scaler_tws), true);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_RING_CLOCK_EN), 0x0);

	writel(0x1, scaler_dev.votf_regs + MSCL_VOTF_TWS_BUSY);
	KUNIT_EXPECT_EQ(test, sc_hwset_clear_votf_clock_en(&scaler_tws), false);
}

static void mscl_test_sc_hwset_color_fill(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	writel(SCALER_CFG_CSC_Y_OFFSET_SRC, scaler_dev.regs + SCALER_CFG);

	sc_hwset_color_fill(&scaler_dev, 0x55);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_FILL_COLOR), 0x55);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_CFG),
			SCALER_CFG_CSC_Y_OFFSET_SRC | SCALER_CFG_FILL_EN);
}

static void mscl_test_sc_hwset_init_votf(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	scaler_dev.votf_base_pa = 0x12345678;
	scaler_dev.current_ctx->tws->idx = 1;

	sc_hwset_init_votf(&scaler_dev);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_RING_EN), MSCL_VOTF_EN_VAL);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_LOCAL_IP), 0x1234);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_IMMEDIATE_MODE), MSCL_VOTF_EN_VAL);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_SET_A), MSCL_VOTF_EN_VAL);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_TWS_OFFSET + MSCL_VOTF_TWS_ENABLE), MSCL_VOTF_EN_VAL);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_TWS_OFFSET + MSCL_VOTF_TWS_LIMIT), 0xff);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_TWS_OFFSET + MSCL_VOTF_TWS_LINES_IN_TOKEN), 0x1);
}

static void mscl_test_sc_hwset_tws_flush(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	scaler_dev.current_ctx->tws->idx = 2;

	sc_hwset_tws_flush(&scaler_tws);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_TWS_OFFSET * 2 + MSCL_VOTF_TWS_FLUSH), 0x1);
}

static void mscl_test_sc_hwset_votf(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	scaler_dev.current_ctx->tws->idx = 2;
	scaler_dev.version = SCALER_VERSION(7, 0, 1);
	scaler_tws.sink.buf_idx = 1;
	scaler_tws.sink.dpu_dma_idx = 2;
	scaler_dev.votf_table[2].votf_base_pa = 0x87654321;

	sc_hwset_votf(&scaler_dev, &scaler_tws);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_VOTF_MASTER_ID), 2);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_CONNECTION_MODE + 8),
			(1 << MSCL_VOTF_CONNECT_BUF_IDX_SHIFT) | MSCL_VOTF_HALF_CONNECTION);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs  + MSCL_VOTF_TWS_DEST_ID + MSCL_VOTF_TWS_OFFSET*2), 0x87658);

	scaler_dev.current_ctx->tws->idx = 1;
	scaler_dev.version = SCALER_VERSION(7, 0, 0);
	scaler_tws.sink.buf_idx = 2;
	scaler_tws.sink.dpu_dma_idx = 1;
	scaler_dev.votf_table[1].votf_base_pa = 0x12345678;

	sc_hwset_votf(&scaler_dev, &scaler_tws);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_VOTF_MASTER_ID), 1);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_CONNECTION_MODE), MSCL_VOTF_HALF_CONNECTION);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs  + MSCL_VOTF_TWS_DEST_ID + MSCL_VOTF_TWS_OFFSET), 0x12340);
	KUNIT_EXPECT_EQ(test, readl(scaler_votf_table[1].regs + SC_DPU_BUF_WRITE_COUNTER), 2);
}

static void mscl_test_sc_hwset_votf_en(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	writel(SCALER_CFG_FILL_EN, scaler_dev.regs + SCALER_CFG);

	sc_hwset_votf_en(&scaler_dev, true);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_CFG), SCALER_CFG_FILL_EN | SCALER_CFG_VOTF_EN);

	sc_hwset_votf_en(&scaler_dev, false);

	KUNIT_EXPECT_EQ(test, readl(scaler_dev.regs + SCALER_CFG), SCALER_CFG_FILL_EN);
}

static void mscl_test_sc_hwset_votf_ring_clk_en(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	atomic_set(&scaler_dev.votf_ref_count, 1);

	sc_hwset_votf_ring_clk_en(&scaler_dev);

	KUNIT_EXPECT_EQ(test, atomic_read(&scaler_dev.votf_ref_count), 2);
	KUNIT_EXPECT_EQ(test, readl(scaler_dev.votf_regs + MSCL_VOTF_RING_CLOCK_EN), MSCL_VOTF_EN_VAL);
}

static void mscl_test_sc_votf_read_reg_and_print(struct kunit *test)
{
	kunit_info(test, "%s", __func__);
	sc_votf_read_reg_and_print(&scaler_dev, 0);
}

static void mscl_test_sc_select_ctx(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	struct sc_ctx *temp_ctx;

	scaler_qos_table[0].freq_mscl = 400000000;
	scaler_qos_table[1].freq_mscl = 300000000;
	scaler_qos_table[2].freq_mscl = 200000000;
	scaler_dev.qos_table_cnt = 3;
	scaler_dev.qos_table = scaler_qos_table;

	INIT_LIST_HEAD(&scaler_dev.context_list);

	scaler_ctx_prio[0].pid = 1000;
	scaler_ctx_prio[1].pid = 1001;
	scaler_ctx_prio[2].pid = 1002;
	scaler_ctx_prio[3].pid = 1003;
	scaler_ctx_prio[0].priority = SC_CTX_PRIORITY_HIGH;
	scaler_ctx_prio[1].priority = SC_CTX_PRIORITY_HIGH;
	scaler_ctx_prio[2].priority = SC_CTX_PRIORITY_LOW;
	scaler_ctx_prio[3].priority = SC_CTX_PRIORITY_LOW;

	/* if target_clk is exceed max mif clk and there is high priority ctx,
	   high priority ctx is chosen first  */
	scaler_dev.target_clk = 500000000;
	list_add_tail(&scaler_ctx_prio[2].node, &scaler_dev.context_list);
	list_add_tail(&scaler_ctx_prio[0].node, &scaler_dev.context_list);
	list_add_tail(&scaler_ctx_prio[3].node, &scaler_dev.context_list);
	list_add_tail(&scaler_ctx_prio[1].node, &scaler_dev.context_list);

	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1000, temp_ctx->pid);
	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1001, temp_ctx->pid);

	/* if target_clk is exceed max mif clk and there is no high priority ctx,
	   low priority ctx can be chosen */
	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1002, temp_ctx->pid);
	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1003, temp_ctx->pid);

	/* if target_clk is not exceed max mif clk, low priority ctx can be chosen */
	scaler_dev.target_clk = 400000000;
	list_add_tail(&scaler_ctx_prio[2].node, &scaler_dev.context_list);
	list_add_tail(&scaler_ctx_prio[0].node, &scaler_dev.context_list);
	list_add_tail(&scaler_ctx_prio[3].node, &scaler_dev.context_list);
	list_add_tail(&scaler_ctx_prio[1].node, &scaler_dev.context_list);

	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1002, temp_ctx->pid);
	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1000, temp_ctx->pid);
	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1003, temp_ctx->pid);
	temp_ctx = sc_select_ctx(&scaler_dev);
	list_del_init(&temp_ctx->node);
	KUNIT_EXPECT_EQ(test, 1001, temp_ctx->pid);
}

static struct kunit_case mscl_test_cases[] = {
	KUNIT_CASE(mscl_test_ext_buf_size),
	KUNIT_CASE(mscl_test_get_blend_value),
	KUNIT_CASE(mscl_test_sc_coef_adjust),
	KUNIT_CASE(mscl_test_sc_hwset_blend),
	KUNIT_CASE(mscl_test_sc_hwset_clear_votf_clock_en),
	KUNIT_CASE(mscl_test_sc_hwset_color_fill),
	KUNIT_CASE(mscl_test_sc_hwset_init_votf),
	KUNIT_CASE(mscl_test_sc_hwset_tws_flush),
	KUNIT_CASE(mscl_test_sc_hwset_votf),
	KUNIT_CASE(mscl_test_sc_hwset_votf_en),
	KUNIT_CASE(mscl_test_sc_hwset_votf_ring_clk_en),
	KUNIT_CASE(mscl_test_sc_votf_read_reg_and_print),
	KUNIT_CASE(mscl_test_sc_select_ctx),
	{},
};

static struct kunit_suite mscl_test_suite = {
	.name = "mscl_exynos",
	.init = mscl_test_init,
	.exit = mscl_test_exit,
	.test_cases = mscl_test_cases,
};

kunit_test_suites(&mscl_test_suite);

MODULE_LICENSE("GPL");
