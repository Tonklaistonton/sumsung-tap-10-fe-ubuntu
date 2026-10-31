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
#include "pablo-hw-rgbp.h"
#include "pablo-icpu-adapter.h"

#define STREAM_ID(id) (id % IS_STREAM_COUNT)

static struct pablo_hw_rgbp_kunit_test_ctx {
	void *base;
	struct is_interface_ischain itfc;
	struct is_frame frame;
	struct is_framemgr framemgr;
	struct is_hardware hardware;
	struct pablo_rta_frame_info prfi;
	struct is_mem mem;
	struct is_region region;

	struct pablo_icpu_adt_msg_ops icpu_msg_ops;
	const struct pablo_icpu_adt_msg_ops *org_icpu_msg_ops;
} test_ctx;

static int __register_response_msg_cb_stub(struct pablo_icpu_adt *icpu_adt, u32 instance,
	enum pablo_hic_cmd_id msg, pablo_response_msg_cb cb)
{
	return 0;
}

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
static void pablo_hw_rgbp_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	struct pablo_hw_rgbp *hw;
	u32 instance = STREAM_ID(__LINE__);

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_OPEN, &hw_ip->state));

	KUNIT_ASSERT_PTR_NE(test, hw_ip->priv_info, NULL);

	hw = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	KUNIT_EXPECT_EQ(test, hw->subdev_cloader.instance, instance);
	KUNIT_EXPECT_EQ(test, hw->subdev_cloader.num_buffers, 2);
	KUNIT_EXPECT_NE(test, hw->subdev_cloader.size[0], 0);

	KUNIT_EXPECT_STREQ(test, hw->pcc->name, hw_ip->name);
	KUNIT_EXPECT_PTR_NE(test, hw->pcc->ops, NULL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip[2];
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0L;

	hw_ip[0] = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip[0]);

	ret = CALL_HWIP_OPS(hw_ip[0], open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip[0], enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &hw_ip[0]->state));

	set_bit(hw_ip[0]->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip[0], enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &hw_ip[0]->state);
	ret = CALL_HWIP_OPS(hw_ip[0], enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip[1] = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP1);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip[1]);
	ret = CALL_HWIP_OPS(hw_ip[1], open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip[1]->state);
	ret = CALL_HWIP_OPS(hw_ip[0], enable, instance, hw_map);
	clear_bit(HW_RUN, &hw_ip[1]->state);

	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip[0]->state));

	ret = CALL_HWIP_OPS(hw_ip[1], close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip[0], enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip[0], close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_disable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	u32 instance = STREAM_ID(__LINE__);
	ulong hw_map = 0UL;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	atomic_set(&hw_ip->instance, instance + 1);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);
	set_bit(HW_CONFIG, &hw_ip->state);

	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &hw_map);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_INIT, &hw_ip->state);
	atomic_set(&hw_ip->status.Vvalid, V_VALID);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_FALSE(test, test_bit(HW_CONFIG, &hw_ip->state));

	set_bit(HW_RUN, &hw_ip->state);
	set_bit(HW_CONFIG, &hw_ip->state);
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(instance, &hw_ip->run_rsc_state);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_TRUE(test, test_bit(HW_CONFIG, &hw_ip->state));

	atomic_set(&hw_ip->instance, instance);
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_FALSE(test, test_bit(HW_CONFIG, &hw_ip->state));

	set_bit(HW_CONFIG, &hw_ip->state);
	hw_ip->run_rsc_state = 0UL;
	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_FALSE(test, test_bit(HW_RUN, &hw_ip->state));
	KUNIT_EXPECT_FALSE(test, test_bit(HW_CONFIG, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_shot_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	struct is_frame *frame = &test_ctx.frame;
	struct cr_set cr = { 0x4, 0x12345678 };
	struct pablo_hw_rgbp_iq *iq_set;
	u32 instance = STREAM_ID(__LINE__);
	u32 fcount = __LINE__;
	ulong hw_map = 0;
	u32 chain_id = 0;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

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

	iq_set = &((struct pablo_hw_rgbp *)hw_ip->priv_info)->iq_set;

	ret = CALL_HWIP_OPS(hw_ip, set_regs, chain_id, instance, fcount, &cr, 1);
	KUNIT_EXPECT_EQ(test, iq_set->size, 1);
	KUNIT_EXPECT_EQ(test, iq_set->regs->reg_data, cr.reg_data);
	KUNIT_EXPECT_EQ(test, iq_set->regs->reg_addr, cr.reg_addr);

	frame->instance = instance;
	hw_ip->region[instance] = &test_ctx.region;

	ret = CALL_HWIP_OPS(hw_ip, shot, frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(HW_CONFIG, &hw_ip->state));

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_frame_ndone_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	struct is_frame *frame = &test_ctx.frame;
	enum ShotErrorType type = __LINE__ % SHOT_ERR_PERFRAME;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &frame->core_flag);
	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, type);
}

static void pablo_hw_rgbp_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	u32 instance = STREAM_ID(__LINE__);

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_reset_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	u32 instance = STREAM_ID(__LINE__);

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_restore_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	u32 instance = STREAM_ID(__LINE__);

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_change_chain_kunit_test(struct kunit *test)
{
	struct is_hw_ip *hw_ip[2];
	u32 instance = STREAM_ID(__LINE__);
	u32 next_id;
	int ret;

	hw_ip[0] = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip[0]);

	next_id = 0;
	ret = CALL_HWIP_OPS(hw_ip[0], change_chain, instance, next_id, hw_ip[0]->hardware);
	KUNIT_EXPECT_EQ(test, ret, 0);

	next_id = 1;
	ret = CALL_HWIP_OPS(hw_ip[0], change_chain, instance, next_id, hw_ip[0]->hardware);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ret = CALL_HWIP_OPS(hw_ip[0], open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip[0], change_chain, instance, next_id, hw_ip[0]->hardware);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	hw_ip[1] = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP1);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip[1]);

	ret = CALL_HWIP_OPS(hw_ip[1], open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_INIT, &hw_ip[1]->state);
	set_bit(HW_RUN, &hw_ip[1]->state);
	ret = CALL_HWIP_OPS(hw_ip[0], change_chain, instance, next_id, hw_ip[0]->hardware);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip[1], close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip[0], close, instance);
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
}

static const int reg_dump_type_result[] = {
	[IS_REG_DUMP_TO_ARRAY] = 0,
	[IS_REG_DUMP_TO_LOG] = 0,
	[IS_REG_DUMP_DMA] = 0,
};

static void pablo_hw_rgbp_dump_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	u32 instance = STREAM_ID(__LINE__);
	u32 fcount = __LINE__;
	u32 dump_type;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

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

static void pablo_hw_rgbp_set_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	struct is_rgbp_config conf, *hw_conf;
	u32 instance = STREAM_ID(__LINE__);
	u32 fcount = __LINE__;
	u32 chain_id = 0;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_conf = &((struct pablo_hw_rgbp *)hw_ip->priv_info)->config[instance];

	conf.magic = 0x12345678;
	ret = CALL_HWIP_OPS(hw_ip, set_config, chain_id, instance, fcount, &conf);
	KUNIT_EXPECT_EQ(test, hw_conf->magic, conf.magic);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_rgbp_cmp_fcount_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	u32 instance = STREAM_ID(__LINE__);
	u32 fcount = __LINE__;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

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

static void pablo_hw_rgbp_query_kunit_test(struct kunit *test)
{
	struct is_hw_ip *hw_ip = NULL;
	struct pablo_rta_frame_info *prfi = &test_ctx.prfi;
	struct is_frame *frame = &test_ctx.frame;
	struct is_param_region *p_region = &test_ctx.region.parameter;
	u32 instance = STREAM_ID(__LINE__);
	struct is_crop otf = { 0, 0, 1920, 1080 };

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	frame->parameter = p_region;
	p_region->rgbp.otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
	p_region->rgbp.otf_input.width = otf.w;
	p_region->rgbp.otf_input.height = otf.h;

	CALL_HWIP_OPS(hw_ip, query, instance, PABLO_QUERY_GET_PCFI, frame, prfi);
	KUNIT_EXPECT_EQ(test, prfi->rgbp_input_size.width, otf.w);
	KUNIT_EXPECT_EQ(test, prfi->rgbp_input_size.height, otf.h);

	p_region->rgbp.otf_input.cmd = OTF_INPUT_COMMAND_DISABLE;
	CALL_HWIP_OPS(hw_ip, query, instance, PABLO_QUERY_GET_PCFI, frame, prfi);
	KUNIT_EXPECT_EQ(test, prfi->rgbp_input_size.width, 0);
	KUNIT_EXPECT_EQ(test, prfi->rgbp_input_size.height, 0);
}

static void pablo_hw_rgbp_handle_interrupt_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	struct is_interface_ischain *itfc = &test_ctx.itfc;
	enum is_hardware_id hw_id = DEV_HW_RGBP0;
	u32 instance = 0;
	int hw_slot;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, hw_id);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler);

	/* not opened */
	ret = itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* not run */
	ret = itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);
	*(u32 *)(test_ctx.base + 0x0800) = 0xFFFFFFFF;
	*(u32 *)(test_ctx.base + 0x0804) = 0xFFFFFFFF;
	ret = itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static struct kunit_case pablo_hw_rgbp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_rgbp_open_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_enable_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_disable_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_shot_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_frame_ndone_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_notify_timeout_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_reset_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_restore_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_change_chain_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_dump_regs_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_set_config_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_cmp_fcount_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_query_kunit_test),
	KUNIT_CASE(pablo_hw_rgbp_handle_interrupt_kunit_test),
	{},
};

static void setup_hw_ip(struct kunit *test, struct is_hw_ip *hw_ip, u32 id)
{
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	init_waitqueue_head(&hw_ip->status.wait_queue);
	hw_ip->locomotive = hw_ip;
	hw_ip->id = id;
	hw_ip->pmio_fields = NULL;
	hw_ip->hw_ops = &hw_ops;
	hw_ip->regs[REG_SETA] = test_ctx.base;
	hw_ip->hardware = &test_ctx.hardware;
	hw_ip->framemgr = &test_ctx.framemgr;
	hw_ip->ip_num = 2;
}

static int pablo_hw_rgbp_kunit_test_init(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = NULL;
	struct is_interface *itf = NULL;
	struct is_interface_ischain *itfc = &test_ctx.itfc;
	struct pablo_icpu_adt *icpu_adt;

	memset(&test_ctx, 0, sizeof(test_ctx));
	test_ctx.base = kunit_kzalloc(test, 0x8000, 0);
	test_ctx.hardware = is_get_is_core()->hardware;

	ret = is_mem_init(&test_ctx.mem, is_get_is_core()->pdev);
	KUNIT_ASSERT_EQ(test, ret, 0);

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP0);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	setup_hw_ip(test, hw_ip, DEV_HW_RGBP0);

	ret = pablo_hw_rgbp_probe(hw_ip, itf, itfc, DEV_HW_RGBP0, "RGBP");
	KUNIT_ASSERT_EQ(test, ret, 0);

	hw_ip = CALL_HW_CHAIN_INFO_OPS(&test_ctx.hardware, get_hw_ip, DEV_HW_RGBP1);
	KUNIT_ASSERT_NOT_NULL(test, hw_ip);

	setup_hw_ip(test, hw_ip, DEV_HW_RGBP1);

	ret = pablo_hw_rgbp_probe(hw_ip, itf, itfc, DEV_HW_RGBP1, "RGBP");
	KUNIT_ASSERT_EQ(test, ret, 0);

	icpu_adt = pablo_get_icpu_adt();
	test_ctx.org_icpu_msg_ops = icpu_adt->msg_ops;
	test_ctx.icpu_msg_ops = *(icpu_adt->msg_ops);
	test_ctx.icpu_msg_ops.register_response_msg_cb = __register_response_msg_cb_stub;
	icpu_adt->msg_ops = &test_ctx.icpu_msg_ops;

	return 0;
}

static void pablo_hw_rgbp_kunit_test_exit(struct kunit *test)
{
	struct pablo_icpu_adt *icpu_adt;

	icpu_adt = pablo_get_icpu_adt();
	icpu_adt->msg_ops = test_ctx.org_icpu_msg_ops;

	kunit_kfree(test, test_ctx.base);

	memset(&test_ctx, 0, sizeof(test_ctx));
}

struct kunit_suite pablo_hw_rgbp_kunit_test_suite = {
	.name = "pablo-hw-rgbp-kunit-test",
	.init = pablo_hw_rgbp_kunit_test_init,
	.exit = pablo_hw_rgbp_kunit_test_exit,
	.test_cases = pablo_hw_rgbp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_rgbp_kunit_test_suite);

MODULE_LICENSE("GPL");
