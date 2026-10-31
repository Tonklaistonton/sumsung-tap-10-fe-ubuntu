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
#include "is-hw-ip.h"
#include "pablo-hw-mlsc.h"

#define STREAM_ID(id) (id % IS_STREAM_COUNT)

static struct pablo_hw_mlsc_kunit_test_ctx {
	struct is_hw_ip hw_ip;
	struct is_interface_ischain itfc;
	struct is_frame frame;
	struct is_framemgr framemgr;
	struct is_hardware hardware;
	struct pablo_rta_frame_info prfi;
	void *base;
	struct is_mem mem;
	struct is_param_region p_region;
	struct camera2_shot shot;
	struct is_group group;
} test_ctx;

static int hw_frame_done(struct is_hw_ip *hw_ip, struct is_frame *frame, int wq_id, u32 output_id,
	enum ShotErrorType done_type, bool get_meta)
{
	/* This is dummy function */
	return (int)done_type;
}

static const struct is_hardware_ops hw_ops = {
	.frame_done = hw_frame_done,
};

/* Define testcases */
static void pablo_hw_mlsc_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct pablo_hw_mlsc *hw;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_OPEN, &hw_ip->state));

	KUNIT_ASSERT_PTR_NE(test, hw_ip->priv_info, NULL);

	hw = (struct pablo_hw_mlsc *)hw_ip->priv_info;
	KUNIT_EXPECT_EQ(test, hw->subdev_cloader.instance, instance);
	KUNIT_EXPECT_EQ(test, hw->subdev_cloader.num_buffers, 2);
	KUNIT_EXPECT_NE(test, hw->subdev_cloader.size[0], 0);
	KUNIT_EXPECT_STREQ(test, hw->pcc->name, hw_ip->name);
	KUNIT_EXPECT_EQ(test, hw->pcc->mode, PCC_OTF_NO_DUMMY);
	KUNIT_EXPECT_PTR_NE(test, hw->pcc->ops, NULL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_INIT, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, close, 0);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
	KUNIT_EXPECT_PTR_EQ(test, hw_ip->priv_info, NULL);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_OPEN, &hw_ip->state));
}

static void pablo_hw_mlsc_handle_interrupt_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itfc->itf_ip[17].handler[INTR_HWIP1].handler);

	/* not opened */
	ret = itfc->itf_ip[17].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* overflow recorvery */
	set_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);
	ret = itfc->itf_ip[17].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	clear_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);

	/* not run */
	ret = itfc->itf_ip[17].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);
	*(u32 *)(test_ctx.base + 0x0800) = 0xFFFFFFFF;
	*(u32 *)(test_ctx.base + 0x0804) = 0xFFFFFFFF;
	ret = itfc->itf_ip[17].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0L;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &hw_ip->state));

	set_bit(hw_ip->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &hw_ip->state);
	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_disable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0UL;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);
	set_bit(HW_CONFIG, &hw_ip->state);

	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_TRUE(test, test_bit(HW_CONFIG, &hw_ip->state));

	set_bit(hw_ip->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &hw_ip->state);
	atomic_set(&hw_ip->status.Vvalid, V_VALID);
	set_bit(instance, &hw_ip->run_rsc_state);
	atomic_set(&hw_ip->instance, instance);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_CONFIG, &hw_ip->state);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_FALSE(test, test_bit(HW_CONFIG, &hw_ip->state));

	hw_ip->run_rsc_state = 0UL;
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_FALSE(test, test_bit(HW_CONFIG, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_shot_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_frame *frame = &test_ctx.frame;
	struct is_param_region *p_region = &test_ctx.p_region;
	struct is_group *group = &test_ctx.group;
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0;

	frame->shot = &test_ctx.shot;

	/* TC#1. There is no HW id in hw_map. */
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* TC#2. HW is not initialized. */
	set_bit(hw_ip->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);

	frame->instance = instance;
	frame->parameter = p_region;

	/* TC#3. Run mlsc without input. */
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* TC#4. Run mlsc by otf input. */
	frame->shot = &test_ctx.shot;
	frame->type = SHOT_TYPE_EXTERNAL;
	p_region->mlsc.otf_input.cmd = 1;
	clear_bit(IS_GROUP_RNR_2ND, &group->state);
	hw_ip->group[instance] = group;
	set_bit(PARAM_MLSC_OTF_INPUT, frame->pmap);
	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_frame_ndone_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_frame *frame = &test_ctx.frame;
	enum ShotErrorType type = __LINE__ % SHOT_ERR_PERFRAME;

	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &frame->core_flag);
	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, type);
}

static void pablo_hw_mlsc_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);

	atomic_set(&hw_ip->status.Vvalid, V_VALID);
	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_reset_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_restore_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_set_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 chain_id = 0;
	u32 instance = 0;
	u32 fcount = 0;
	struct cr_set cr;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_regs, chain_id, instance, fcount, &cr, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_set_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 chain_id = 0;
	u32 instance = 0;
	u32 fcount = 0;
	struct is_mlsc_config conf;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_config, chain_id, instance, fcount, &conf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static const int reg_dump_type_result[] = {
	[IS_REG_DUMP_TO_ARRAY] = 0,
	[IS_REG_DUMP_TO_LOG] = 0,
	[IS_REG_DUMP_DMA] = 0,
};

static void pablo_hw_mlsc_dump_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);
	u32 fcount = __LINE__;
	u32 dump_type;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (dump_type = 0; dump_type < ARRAY_SIZE(reg_dump_type_result); dump_type++) {
		ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, dump_type);
		KUNIT_EXPECT_EQ(test, ret, reg_dump_type_result[dump_type]);
	}

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_mlsc_query_kunit_test(struct kunit *test)
{
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct pablo_rta_frame_info *prfi = &test_ctx.prfi;
	struct is_frame *frame = &test_ctx.frame;
	struct is_param_region *p_region = &test_ctx.p_region;
	u32 instance = STREAM_ID(__LINE__);
	u32 test_cmd = 1;

	frame->parameter = p_region;
	p_region->mlsc.svhist.cmd = test_cmd;
	p_region->mlsc.lme_ds.cmd = test_cmd;

	CALL_HWIP_OPS(hw_ip, query, instance, PABLO_QUERY_GET_PCFI, frame, prfi);
	KUNIT_EXPECT_EQ(test, prfi->mlsc_out_svhist_buffer, test_cmd);
	KUNIT_EXPECT_EQ(test, prfi->mlsc_out_meds_buffer, test_cmd);
}

static void pablo_hw_mlsc_cmp_fcount_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = STREAM_ID(__LINE__);
	u32 fcount = __LINE__;

	/* Run with hw null */
	ret = CALL_HWIP_OPS(hw_ip, cmp_fcount, fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, cmp_fcount, fcount);
	KUNIT_EXPECT_EQ(test, ret, fcount);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static struct kunit_case pablo_hw_mlsc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_mlsc_open_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_handle_interrupt_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_disable_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_shot_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_frame_ndone_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_notify_timeout_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_reset_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_restore_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_set_regs_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_set_config_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_dump_regs_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_query_kunit_test),
	KUNIT_CASE(pablo_hw_mlsc_cmp_fcount_kunit_test),
	{},
};

static void setup_hw_ip(struct kunit *test)
{
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_hw_ip *org_hw_ip;
	struct is_hardware *hardware = &test_ctx.hardware;

	org_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, DEV_HW_MLSC0);
	KUNIT_ASSERT_NOT_NULL(test, org_hw_ip);

	hw_ip->locomotive = hw_ip;
	hw_ip->id = DEV_HW_MLSC0;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	init_waitqueue_head(&hw_ip->status.wait_queue);
	hw_ip->hw_ops = &hw_ops;
	hw_ip->regs[REG_SETA] = test_ctx.base;
	hw_ip->regs_start[REG_SETA] = org_hw_ip->regs_start[REG_SETA];
	hw_ip->regs_end[REG_SETA] = org_hw_ip->regs_end[REG_SETA];
	hw_ip->hardware = &test_ctx.hardware;
	hw_ip->framemgr = &test_ctx.framemgr;
}

static int pablo_hw_mlsc_kunit_test_init(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_interface *itf = NULL;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	memset(&test_ctx, 0, sizeof(test_ctx));
	test_ctx.base = kunit_kzalloc(test, 0x8000, 0);
	test_ctx.hardware = is_get_is_core()->hardware;

	ret = is_mem_init(&test_ctx.mem, is_get_is_core()->pdev);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = pablo_hw_chain_info_probe(&test_ctx.hardware);
	KUNIT_ASSERT_EQ(test, ret, 0);

	setup_hw_ip(test);

	ret = pablo_hw_mlsc_probe(hw_ip, itf, itfc, DEV_HW_MLSC0, "MLSC");
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_mlsc_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.base);

	memset(&test_ctx, 0, sizeof(test_ctx));
}

struct kunit_suite pablo_hw_mlsc_kunit_test_suite = {
	.name = "pablo-hw-mlsc-kunit-test",
	.init = pablo_hw_mlsc_kunit_test_init,
	.exit = pablo_hw_mlsc_kunit_test_exit,
	.test_cases = pablo_hw_mlsc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_mlsc_kunit_test_suite);

MODULE_LICENSE("GPL");
