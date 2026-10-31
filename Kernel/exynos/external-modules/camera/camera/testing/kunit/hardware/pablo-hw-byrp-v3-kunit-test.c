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
#include "is-hw-ip.h"
#include "is-core.h"
#include "pablo-hw-byrp-v3.h"
#include "pablo-icpu-adapter.h"
#include "pablo-hw-chain-info.h"

static struct pablo_hw_byrp_kunit_test_ctx {
	struct is_hw_ip hw_ip;
	struct is_hardware hardware;
	struct is_interface_ischain itfc;
	struct is_framemgr framemgr;
	struct is_frame frame;
	struct camera2_shot_ext shot_ext;
	struct camera2_shot shot;
	struct is_param_region parameter;
	struct is_mem mem;
	struct is_mem_ops memops;
	struct byrp_param byrp_param;
	struct is_region region;
	void *test_addr;
	struct is_hw_ip_ops hw_ops;
	struct is_hw_ip_ops *org_hw_ops;

	struct pablo_icpu_adt_msg_ops icpu_msg_ops;
	const struct pablo_icpu_adt_msg_ops *org_icpu_msg_ops;
} test_ctx;

static struct param_dma_input test_dma_input = {
	.cmd = DMA_INPUT_COMMAND_ENABLE,
	.format = DMA_INOUT_FORMAT_BAYER_PACKED,
	.bitwidth = DMA_INOUT_BIT_WIDTH_16BIT,
	.order = DMA_INOUT_ORDER_YYCbCr,
	.dma_crop_offset = 0,
	.msb = 7,
};

static int __register_response_msg_cb_stub(struct pablo_icpu_adt *icpu_adt, u32 instance,
	enum pablo_hic_cmd_id msg, pablo_response_msg_cb cb)
{
	return 0;
}

/* Define the test cases. */

static void pablo_hw_byrp_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_hw_byrp *hw;
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw = (struct is_hw_byrp *)hw_ip->priv_info;
	KUNIT_EXPECT_STREQ(test, hw->pcc->name, hw_ip->name);
	KUNIT_EXPECT_EQ(test, hw->pcc->mode, PCC_OTF);
	KUNIT_EXPECT_PTR_NE(test, hw->pcc->ops, NULL);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_handle_interrupt_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itfc->itf_ip[4].handler[INTR_HWIP1].handler);

	/* not opened */
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* overflow recorvery */
	set_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	clear_bit(HW_OVERFLOW_RECOVERY, &hw_ip->hardware->hw_recovery_flag);

	/* not run */
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);
	*(u32 *)(test_ctx.test_addr + 0x0800) = 0xFFFFFFFF;
	*(u32 *)(test_ctx.test_addr + 0x0804) = 0xFFFFFFFF;
	ret = itfc->itf_ip[4].handler[INTR_HWIP1].handler(0, hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static bool __set_rta_regs(struct is_hw_ip *hw_ip, u32 instance, u32 set_id, bool skip,
	struct is_frame *frame, void *buf)
{
	return true;
}

static int __reset_stub(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static int __wait_idle_stub(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static void __g_binning_size(
	struct byrp_param_set *param_set, struct is_frame *frame, u32 *binning_x, u32 *binning_y)
{
	u32 sensor_binning_ratio_x, sensor_binning_ratio_y;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		sensor_binning_ratio_x = param_set->sensor_config.sensor_binning_ratio_x;
		sensor_binning_ratio_y = param_set->sensor_config.sensor_binning_ratio_y;
	} else {
		sensor_binning_ratio_x = frame->shot->udm.frame_info.sensor_binning[0];
		sensor_binning_ratio_y = frame->shot->udm.frame_info.sensor_binning[1];
		param_set->sensor_config.sensor_binning_ratio_x = sensor_binning_ratio_x;
		param_set->sensor_config.sensor_binning_ratio_y = sensor_binning_ratio_y;
	}

	/* Total_binning = sensor_binning * csis_bns_binning */
	*binning_x =
		(1024ULL * sensor_binning_ratio_x * param_set->sensor_config.bns_binning_ratio_x) /
		1000 / 1000;
	*binning_y =
		(1024ULL * sensor_binning_ratio_y * param_set->sensor_config.bns_binning_ratio_y) /
		1000 / 1000;
}

static void pablo_hw_byrp_s_size_regs(struct is_hw_ip *hw_ip, ulong hw_map, struct kunit *test)
{
	int ret;
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct byrp_param_set *param_set = &hw_byrp->param_set[0];
	u32 binning_x, binning_y, g_binning_x, g_binning_y;
	u32 binning_ratio[] = {
		8000, /* 8192 - Binning 8 */
		4000, /* 4096 - Binning 4 */
		2000, /* 2048 - Binning 2 */
		1000, /* 1024 - No Binning */
	};
	long debug_val = is_get_debug_param(IS_DEBUG_PARAM_DISABLE_CRTA);
	int i;
	bool utc_flag;

	is_set_debug_param(IS_DEBUG_PARAM_DISABLE_CRTA, 1);
	for (i = 0; i < ARRAY_SIZE(binning_ratio); i++) {
		test_ctx.frame.shot->udm.frame_info.sensor_binning[0] = binning_ratio[i];
		test_ctx.frame.shot->udm.frame_info.sensor_binning[1] = binning_ratio[i];

		ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
		KUNIT_EXPECT_EQ(test, ret, 0);
		if (i == 0)
			utc_flag = ret == 0;
		else
			utc_flag = utc_flag && (ret == 0);

		__g_binning_size(param_set, &test_ctx.frame, &binning_x, &binning_y);
		byrp_hw_g_binning_size(hw_ip->pmio, &g_binning_x, &g_binning_y);
		KUNIT_EXPECT_EQ(test, g_binning_x, binning_x);
		utc_flag = utc_flag && (g_binning_x == binning_x);
		KUNIT_EXPECT_EQ(test, g_binning_y, binning_y);
		utc_flag = utc_flag && (g_binning_y == binning_y);
	}
	is_set_debug_param(IS_DEBUG_PARAM_DISABLE_CRTA, debug_val);

	set_utc_result(KUTC_BYRP_BINNING_RATIO, UTC_ID_BYRP_BINNING_RATIO, utc_flag);
}

static void pablo_hw_byrp_check_perframe(struct is_hw_ip *hw_ip, ulong hw_map, struct kunit *test)
{
	struct is_param_region *param_region = &test_ctx.parameter;
	u32 resolution[][2] = {
		{ 4000, 3000 },
		{ 2000, 1500 },
		{ 1000, 750 },
		{ 500, 375 },
	};
	u32 width, height;
	bool pmio_flag = pmio_cache_get_only(hw_ip->pmio);
	int ret, i;
	bool utc_flag;

	is_set_debug_param(IS_DEBUG_PARAM_DISABLE_CRTA, 1);
	pmio_cache_set_only(hw_ip->pmio, false);

	param_region->byrp.dma_input = test_dma_input;
	for (i = 0; i < ARRAY_SIZE(resolution); i++) {
		param_region->byrp.dma_input.width = resolution[i][0];
		param_region->byrp.dma_input.height = resolution[i][1];
		ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
		KUNIT_EXPECT_EQ(test, ret, 0);
		if (i == 0)
			utc_flag = ret == 0;
		else
			utc_flag = utc_flag && (ret == 0);
		byrp_hw_g_chain_size(hw_ip->pmio, COREX_DIRECT, &width, &height);
		ret = (width == resolution[i][0]) && (height == resolution[i][1]);
		KUNIT_EXPECT_TRUE(test, ret);
		utc_flag = utc_flag && ret;
	}
	is_set_debug_param(IS_DEBUG_PARAM_DISABLE_CRTA, 0);
	pmio_cache_set_only(hw_ip->pmio, pmio_flag);

	set_utc_result(KUTC_BYRP_PER_FRM_CTRL, UTC_ID_BYRP_PER_FRM_CTRL, utc_flag);
	set_utc_result(KUTC_BYRP_META_INF, UTC_ID_BYRP_META_INF, utc_flag);
}

static void pablo_hw_byrp_shot_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct pablo_hw_helper_ops ops;
	u32 instance = 0;
	ulong hw_map = 0;
	struct is_param_region *param_region;
	struct cr_set cr;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &hw_map);
	test_ctx.frame.shot = &test_ctx.shot;
	test_ctx.frame.shot_ext = &test_ctx.shot_ext;
	test_ctx.frame.parameter = &test_ctx.parameter;
	ops.set_rta_regs = __set_rta_regs;
	hw_ip->help_ops = &ops;
	hw_ip->region[instance] = &test_ctx.region;

	set_bit(PARAM_BYRP_OTF_INPUT, test_ctx.frame.pmap);
	set_bit(PARAM_BYRP_DMA_INPUT, test_ctx.frame.pmap);
	set_bit(PARAM_BYRP_BYR, test_ctx.frame.pmap);
	set_bit(PARAM_BYRP_OTF_OUTPUT, test_ctx.frame.pmap);

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	param_region = &test_ctx.parameter;
	param_region->byrp.dma_input.cmd = 1;
	param_region->byrp.dma_input.bitwidth = 16;
	param_region->byrp.dma_input.msb = 7;
	param_region->byrp.dma_output_byr.bitwidth = 16;
	param_region->byrp.dma_output_byr.cmd = 1;
	param_region->byrp.dma_output_byr.msb = 11;

	is_hw_byrp_s_debug_type(BYRP_DBG_DTP);

	ret = CALL_HWIP_OPS(hw_ip, set_regs, 0, instance, 0, &cr, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	is_hw_byrp_c_debug_type(BYRP_DBG_DTP);

	ret = CALL_HWIP_OPS(hw_ip, set_regs, 0, instance, 0, &cr, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, shot, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_byrp_s_size_regs(hw_ip, hw_map, test);

	pablo_hw_byrp_check_perframe(hw_ip, hw_map, test);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	ulong hw_map = 0;
	bool utc_flag;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(hw_ip->id, &hw_map);

	ret = CALL_HWIP_OPS(hw_ip, enable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	utc_flag = ret == 0;

	ret = CALL_HWIP_OPS(hw_ip, disable, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
	utc_flag = utc_flag ? ret == 0 : 0;

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
	set_utc_result(KUTC_BYRP_EN_DIS, UTC_ID_BYRP_EN_DIS, utc_flag);
}

static void pablo_hw_byrp_set_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 chain_id = 0;
	u32 instance = 0;
	u32 fcount = 0;
	struct is_byrp_config conf;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_config, chain_id, instance, fcount, &conf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_dump_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	u32 fcount = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_TO_LOG);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_DMA);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, dump_regs, instance, fcount, NULL, 0, IS_REG_DUMP_TO_ARRAY);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_set_regs_kunit_test(struct kunit *test)
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
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, notify_timeout, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_restore_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, restore, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_setfile_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 scenario = 0;
	u32 instance = 0;
	ulong hw_map = 0;

	ret = CALL_HWIP_OPS(hw_ip, load_setfile, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, apply_setfile, scenario, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, delete_setfile, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_get_meta_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	ulong hw_map = 0;

	ret = CALL_HWIP_OPS(hw_ip, get_meta, &test_ctx.frame, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_set_param_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;
	ulong hw_map = 0;
	IS_DECLARE_PMAP(pmap);

	set_bit(hw_ip->id, &hw_map);

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, init, instance, false, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, set_param, &test_ctx.region, pmap, instance, hw_map);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, deinit, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_frame_ndone_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	enum ShotErrorType type = IS_SHOT_UNKNOWN;

	ret = CALL_HWIP_OPS(hw_ip, frame_ndone, &test_ctx.frame, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_reset_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	u32 instance = 0;

	hw_ip->ops = test_ctx.org_hw_ops;

	ret = CALL_HWIP_OPS(hw_ip, open, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	ret = CALL_HWIP_OPS(hw_ip, close, instance);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static void pablo_hw_byrp_dma_cfg_kunit_test(struct kunit *test)
{
	struct is_hw_ip *hw_ip = &test_ctx.hardware.hw_ip[0];
	u32 dma_cmd = 1;
	u32 plane = DMA_INOUT_PLANE_1;
	pdma_addr_t dst_dva[IS_MAX_PLANES] = { 0 };
	dma_addr_t sc0TargetAddress[IS_MAX_PLANES];
	int ret;

	test_ctx.frame.num_buffers = 1;
	ret = is_hardware_dma_cfg("", hw_ip, &test_ctx.frame, 0, test_ctx.frame.num_buffers,
		&dma_cmd, plane, dst_dva, sc0TargetAddress);
	KUNIT_EXPECT_EQ(test, dma_cmd, 0);
	set_utc_result(KUTC_BYRP_DIS_DMA, UTC_ID_BYRP_DIS_DMA, dma_cmd == 0);
}

static struct kunit_case pablo_hw_byrp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_byrp_open_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_handle_interrupt_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_shot_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_enable_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_set_config_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_dump_regs_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_set_regs_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_notify_timeout_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_restore_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_setfile_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_get_meta_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_set_param_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_frame_ndone_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_reset_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_dma_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_byrp_dma_cfg_kunit_test),
	{},
};

static void __setup_hw_ip(struct kunit *test)
{
	int ret;
	enum is_hardware_id hw_id = DEV_HW_BYRP;
	struct is_interface *itf = NULL;
	struct is_hw_ip *hw_ip = &test_ctx.hw_ip;
	struct is_hw_ip *org_hw_ip;
	struct is_interface_ischain *itfc = &test_ctx.itfc;

	hw_ip->hardware = &test_ctx.hardware;

	org_hw_ip = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip, DEV_HW_BYRP);
	KUNIT_ASSERT_NOT_NULL(test, org_hw_ip);

	hw_ip->regs_start[REG_SETA] = org_hw_ip->regs_start[REG_SETA];
	hw_ip->regs_end[REG_SETA] = org_hw_ip->regs_end[REG_SETA];

	ret = is_hw_byrp_probe(hw_ip, itf, itfc, hw_id, "BYRP");
	KUNIT_ASSERT_EQ(test, ret, 0);

	hw_ip->locomotive = hw_ip;
	hw_ip->id = hw_id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "BYRP");
	hw_ip->itf = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);
	hw_ip->state = 0;
	set_bit(HW_OTF, &hw_ip->state);

	hw_ip->framemgr = &test_ctx.framemgr;

	test_ctx.org_hw_ops = (struct is_hw_ip_ops *)hw_ip->ops;
	test_ctx.hw_ops = *(hw_ip->ops);
	test_ctx.hw_ops.reset = __reset_stub;
	test_ctx.hw_ops.wait_idle = __wait_idle_stub;
	hw_ip->ops = &test_ctx.hw_ops;
}

static int pablo_hw_byrp_kunit_test_init(struct kunit *test)
{
	int ret;
	struct pablo_icpu_adt *icpu_adt;

	test_ctx.hardware = is_get_is_core()->hardware;

	test_ctx.test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	test_ctx.hw_ip.regs[REG_SETA] = test_ctx.test_addr;

	icpu_adt = pablo_get_icpu_adt();
	test_ctx.org_icpu_msg_ops = icpu_adt->msg_ops;
	test_ctx.icpu_msg_ops = *(icpu_adt->msg_ops);
	test_ctx.icpu_msg_ops.register_response_msg_cb = __register_response_msg_cb_stub;
	icpu_adt->msg_ops = &test_ctx.icpu_msg_ops;

	ret = is_mem_init(&test_ctx.mem, is_get_is_core()->pdev);
	KUNIT_ASSERT_EQ(test, ret, 0);

	__setup_hw_ip(test);

	return 0;
}

static void pablo_hw_byrp_kunit_test_exit(struct kunit *test)
{
	struct pablo_icpu_adt *icpu_adt;

	icpu_adt = pablo_get_icpu_adt();
	icpu_adt->msg_ops = test_ctx.org_icpu_msg_ops;

	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(test_ctx));
}

struct kunit_suite pablo_hw_byrp_kunit_test_suite = {
	.name = "pablo-hw-byrp-v3-kunit-test",
	.init = pablo_hw_byrp_kunit_test_init,
	.exit = pablo_hw_byrp_kunit_test_exit,
	.test_cases = pablo_hw_byrp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_byrp_kunit_test_suite);

MODULE_LICENSE("GPL");
