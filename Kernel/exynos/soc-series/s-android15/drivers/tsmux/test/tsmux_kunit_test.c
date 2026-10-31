// SPDX-License-Identifier: GPL-2.0
/*
 * tsmux_exynos_test.c - Samsung Exynos TSMUX Driver for Kunit
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>

#include <kunit/visibility.h>

#include "tsmux_dev.h"
#include "tsmux_kunit_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct tsmux_context ctx;
struct tsmux_device tsmux_dev;
char tsmux_virtual_sfr[1000];

static int tsmux_exynos_test_init(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	memset(&ctx, 0, sizeof(ctx));
	memset(&tsmux_dev, 0, sizeof(tsmux_dev));
	ctx.tsmux_dev = &tsmux_dev;
	tsmux_dev.ctx[0] = &ctx;
	atomic_set(&tsmux_dev.ctx_num, 0);
	tsmux_dev.regs_base = tsmux_virtual_sfr;
	init_waitqueue_head(&ctx.m2m_wait_queue);
	init_waitqueue_head(&ctx.otf_wait_queue);

    return 0;
}

static void tsmux_exynos_test_exit(struct kunit *test)
{
	kunit_info(test, "%s", __func__);
}

static void tsmux_exynos_is_otf_job_done_test(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	KUNIT_EXPECT_EQ(test, false, is_otf_job_done(&ctx));

	ctx.otf_outbuf_info[1].buf_state = BUF_PART_DONE;
	ctx.otf_cmd_queue.out_buf[1].partial_done = 1;

	KUNIT_EXPECT_EQ(test, true, is_otf_job_done(&ctx));

	ctx.otf_outbuf_info[1].buf_state = BUF_FREE;
	ctx.otf_cmd_queue.out_buf[1].partial_done = 0;
	ctx.otf_outbuf_info[0].buf_state = BUF_JOB_DONE;

	KUNIT_EXPECT_EQ(test, true, is_otf_job_done(&ctx));
}

static void tsmux_exynos_handle_otf_job_done_test(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	ctx.otf_outbuf_info[0].buf_state = BUF_Q;
	ctx.otf_cmd_queue.out_buf[0].actual_size = 200;
	tsmux_handle_otf_job_done(&tsmux_dev, 0);

	KUNIT_EXPECT_EQ(test, BUF_JOB_DONE, ctx.otf_outbuf_info[0].buf_state);
	KUNIT_EXPECT_EQ(test, 200, ctx.otf_cmd_queue.out_buf[0].offset);

	ctx.otf_outbuf_info[0].buf_state = BUF_PART_DONE;
	ctx.otf_cmd_queue.out_buf[0].partial_done = 1;
	ctx.otf_cmd_queue.out_buf[0].actual_size = 300;
	tsmux_handle_otf_job_done(&tsmux_dev, 0);
	KUNIT_EXPECT_EQ(test, BUF_JOB_DONE, ctx.otf_outbuf_info[0].buf_state);
	KUNIT_EXPECT_EQ(test, 200, ctx.otf_cmd_queue.out_buf[0].offset);
}

static void tsmux_exynos_handle_otf_partial_done_test(struct kunit *test)
{
	kunit_info(test, "%s", __func__);

	ctx.otf_outbuf_info[0].buf_state = BUF_Q;

	KUNIT_EXPECT_EQ(test, true, tsmux_handle_otf_partial_done(&tsmux_dev, 0));

	KUNIT_EXPECT_EQ(test, BUF_PART_DONE, ctx.otf_outbuf_info[0].buf_state);
	KUNIT_EXPECT_EQ(test, 1, ctx.otf_cmd_queue.out_buf[0].partial_done);
	KUNIT_EXPECT_EQ(test, 0, ctx.otf_cmd_queue.out_buf[0].offset);
	KUNIT_EXPECT_EQ(test, 39840, ctx.otf_cmd_queue.out_buf[0].actual_size);

	KUNIT_EXPECT_EQ(test, true, tsmux_handle_otf_partial_done(&tsmux_dev, 0));

	KUNIT_EXPECT_EQ(test, BUF_PART_DONE, ctx.otf_outbuf_info[0].buf_state);
	KUNIT_EXPECT_EQ(test, 1, ctx.otf_cmd_queue.out_buf[0].partial_done);
	KUNIT_EXPECT_EQ(test, 0, ctx.otf_cmd_queue.out_buf[0].offset);
	KUNIT_EXPECT_EQ(test, 79680, ctx.otf_cmd_queue.out_buf[0].actual_size);

	ctx.otf_cmd_queue.out_buf[0].partial_done = 0;
	KUNIT_EXPECT_EQ(test, true, tsmux_handle_otf_partial_done(&tsmux_dev, 0));
	KUNIT_EXPECT_EQ(test, 1, ctx.otf_cmd_queue.out_buf[0].partial_done);
	KUNIT_EXPECT_EQ(test, 79680, ctx.otf_cmd_queue.out_buf[0].offset);
	KUNIT_EXPECT_EQ(test, 39840, ctx.otf_cmd_queue.out_buf[0].actual_size);
}

static void tsmux_exynos_packetize_abnormal_case_test(struct kunit *test)
{
	struct packetizing_param param;

	kunit_info(test, "%s", __func__);

	KUNIT_EXPECT_EQ(test, -1, tsmux_packetize(&param));

	tsmux_set_global_tsmux_dev(&tsmux_dev);
	KUNIT_EXPECT_EQ(test, -1, tsmux_packetize(&param));

	ctx.otf_outbuf_info[0].buf_state = BUF_Q;
	KUNIT_EXPECT_EQ(test, -1, tsmux_packetize(&param));

	ctx.otf_outbuf_info[0].buf_state = BUF_FREE;
	KUNIT_EXPECT_EQ(test, -1, tsmux_packetize(&param));

	ctx.otf_buf_mapped = true;
	KUNIT_EXPECT_EQ(test, -1, tsmux_packetize(&param));

	ctx.otf_outbuf_info[0].dma_addr = 0x12ab;
	ctx.otf_cmd_queue.config.pkt_ctrl.mode = 1;
	KUNIT_EXPECT_EQ(test, 0, tsmux_packetize(&param));
}

static void tsmux_exynos_packetize_normal_case_test(struct kunit *test)
{
	struct packetizing_param param;

	kunit_info(test, "%s", __func__);

	tsmux_set_global_tsmux_dev(&tsmux_dev);
	ctx.otf_buf_mapped = true;
	ctx.otf_outbuf_info[0].dma_addr = 0x12ab;
	ctx.otf_cmd_queue.config.pkt_ctrl.mode = 1;
	param.time_stamp = 0x10101010;

	KUNIT_EXPECT_EQ(test, 0, tsmux_packetize(&param));
	KUNIT_EXPECT_EQ(test, TS_PKT_COUNT_PER_RTP, ctx.otf_cmd_queue.config.pkt_ctrl.rtp_size);
	KUNIT_EXPECT_EQ(test, 0x2105C9, ctx.otf_cmd_queue.config.pes_hdr.pts39_16);
	KUNIT_EXPECT_EQ(test, 0x2BD9, ctx.otf_cmd_queue.config.pes_hdr.pts15_0);
}

static struct kunit_case tsmux_exynos_test_cases[] = {
	KUNIT_CASE(tsmux_exynos_is_otf_job_done_test),
	KUNIT_CASE(tsmux_exynos_handle_otf_job_done_test),
	KUNIT_CASE(tsmux_exynos_handle_otf_partial_done_test),
	KUNIT_CASE(tsmux_exynos_packetize_abnormal_case_test),
	KUNIT_CASE(tsmux_exynos_packetize_normal_case_test),
	{},
};

static struct kunit_suite tsmux_exynos_test_suite = {
	.name = "tsmux_exynos",
	.init = tsmux_exynos_test_init,
	.exit = tsmux_exynos_test_exit,
	.test_cases = tsmux_exynos_test_cases,
};

kunit_test_suites(&tsmux_exynos_test_suite);

MODULE_LICENSE("GPL");
