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
#include "is-core.h"
#include "pablo-mem.h"
#include "pmio.h"
#include "pablo-hw-api-common-ctrl.h"
#include "sfr/pablo-sfr-common-ctrl-v2_0.h"

#define MAX_COTF_IN_NUM 8
#define MAX_COTF_OUT_NUM 8

#define SET_CR(R, value)                                                                           \
	do {                                                                                       \
		u32 *addr = (u32 *)(test_ctx.base + R);                                            \
		*addr = value;                                                                     \
	} while (0)
#define GET_CR(R) (*(u32 *)(test_ctx.base + R))

struct cr_val_map {
	u32 cr_offset;
	u32 val;
};

struct cmd_val_map {
	struct pablo_common_ctrl_cmd cmd;
	u32 cmd_h;
	u32 cmd_m;
	u32 cmd_l;
	u32 queue;
	int ret;
};

static struct pablo_kunit_test_ctx {
	struct platform_device pdev;
	struct pablo_common_ctrl pcc_m2m;
	struct pablo_common_ctrl pcc_otf;
	struct is_mem *mem;
	struct pablo_mmio *pmio;
	struct pmio_field *pmio_fields;
	void *base;
} test_ctx;

/* Define testcases */
static void pablo_hw_api_common_ctrl_m2m_init_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc;
	struct pablo_mmio *pmio = NULL;
	char name[PCC_NAME_LEN] = "PCC-KUNIT";
	u32 pcc_mode = PCC_MODE_NUM;

	pcc = &test_ctx.pcc_m2m;
	CALL_PCC_HW_OPS(pcc, deinit, pcc);

	ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, name, pcc_mode, NULL);
	KUNIT_EXPECT_NE(test, ret, 0);

	pmio = test_ctx.pmio;
	ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, NULL, pcc_mode, NULL);
	KUNIT_EXPECT_NE(test, ret, 0);

	ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, name, pcc_mode, NULL);
	KUNIT_EXPECT_NE(test, ret, 0);

	for (pcc_mode = 0; pcc_mode < PCC_MODE_NUM; pcc_mode++) {
		CALL_PCC_HW_OPS(pcc, deinit, pcc);

		ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, name, pcc_mode, NULL);
		KUNIT_EXPECT_STREQ(test, pcc->name, name);
		KUNIT_EXPECT_EQ(test, pcc->mode, pcc_mode);
		KUNIT_EXPECT_NOT_NULL(test, pcc->ops);
	}
}

static const struct cr_val_map apg_mode_cr[PCC_APG_MODE_NUM] = {
	[PCC_APG_OFF] = {
		.cr_offset = CMN_CTRL_R_IP_APG_MODE,
		.val = 0x0,
	},
	[PCC_HWAPG] = {
		.cr_offset = CMN_CTRL_R_IP_APG_MODE,
		.val = 0x1,
	},
	[PCC_ICTRL] = {
		.cr_offset = CMN_CTRL_R_IP_APG_MODE,
		.val = 0x3,
	},
};

static void pablo_hw_api_common_ctrl_m2m_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_m2m;
	struct pablo_common_ctrl_cfg cfg;
	u32 i;

	cfg.int_en[PCC_INT_0] = (u32)__LINE__;
	cfg.int_en[PCC_INT_1] = (u32)__LINE__;
	cfg.int_en[PCC_CMDQ_INT] = (u32)__LINE__;
	cfg.int_en[PCC_COREX_INT] = (u32)__LINE__;

	ret = CALL_PCC_OPS(pcc, enable, pcc, &cfg);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_VHD_CONTROL), BIT_MASK(24));
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_C_LOADER_ENABLE), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_INT_REQ_INT0_ENABLE), cfg.int_en[PCC_INT_0]);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_INT_REQ_INT1_ENABLE), cfg.int_en[PCC_INT_1]);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_INT_ENABLE), cfg.int_en[PCC_CMDQ_INT]);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_COREX_INT_ENABLE), cfg.int_en[PCC_COREX_INT]);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_ENABLE), 1);

	for (i = PCC_ASAP; i < PCC_FS_MODE_NUM; i++) {
		cfg.fs_mode = i;

		ret = CALL_PCC_OPS(pcc, enable, pcc, &cfg);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_USE_CINFIFO_NEW_FRAME_IN), i);
	}

	for (i = PCC_APG_OFF; i < PCC_APG_MODE_NUM; i++) {
		cfg.apg_mode = i;

		ret = CALL_PCC_OPS(pcc, enable, pcc, &cfg);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, GET_CR(apg_mode_cr[i].cr_offset), apg_mode_cr[i].val);
	}
}

static void pablo_hw_api_common_ctrl_m2m_reset_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_m2m;

	ret = CALL_PCC_OPS(pcc, reset, pcc);
	/* It always fail because the SW_RESET never be cleared. */
	KUNIT_ASSERT_EQ(test, ret, -ETIME);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_LOCK), 0x101);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_SW_RESET), 1);
}

static const struct cr_val_map cotf_in_cr[MAX_COTF_IN_NUM] = {
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.val = 0x0001,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.val = 0x0100,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.val = 0x0001,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.val = 0x0100,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.val = 0x0001,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.val = 0x0100,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.val = 0x0001,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.val = 0x0100,
	},
};

static const struct cr_val_map cotf_out_cr[MAX_COTF_OUT_NUM] = {
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.val = 0x00010000,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.val = 0x01000000,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.val = 0x00010000,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.val = 0x01000000,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.val = 0x00010000,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.val = 0x01000000,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.val = 0x00010000,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.val = 0x01000000,
	},
};

static void pablo_hw_api_common_ctrl_m2m_shot_cotf_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_m2m;
	struct pablo_common_ctrl_frame_cfg frame_cfg;
	u32 pos;

	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));

	SET_CR(CMN_CTRL_R_CMDQ_ENABLE, 1);

	/* Test COTF_IN */
	for (pos = 0; pos < MAX_COTF_IN_NUM; pos++) {
		frame_cfg.cotf_in_en = BIT_MASK(pos);
		ret = CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, GET_CR(cotf_in_cr[pos].cr_offset), cotf_in_cr[pos].val);
	}

	frame_cfg.cotf_in_en = 0;

	/* Test COTF_OUT */
	for (pos = 0; pos < MAX_COTF_OUT_NUM; pos++) {
		frame_cfg.cotf_out_en = BIT_MASK(pos);

		ret = CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, GET_CR(cotf_out_cr[pos].cr_offset), cotf_out_cr[pos].val);
	}
}

static const struct cmd_val_map cmd_tc[] = {
	/* TC 1. Normal DMA-preload mode. */
	{
		.cmd = {
			.base_addr = 0xffffffff0, /* 36bit */
			.header_num = 0xfff,
			.set_mode = PCC_DMA_PRELOADING,
			.fcount = 0xffff,
			.int_grp_en = 0xff,
		},
		.cmd_h = 0xffffffff,
		.cmd_m = 0x7fff0fff,
		.cmd_l = 0x0ff,
		.queue = 0x1,
		.ret = 0,
	},
	/* TC 2. Normal DMA-direct mode. */
	{
		.cmd = {
			.base_addr = 0xffffffff0, /* 36bit */
			.header_num = 0xfff,
			.set_mode = PCC_DMA_DIRECT,
			.fcount = 0xffff,
			.int_grp_en = 0xff,
		},
		.cmd_h = 0xffffffff,
		.cmd_m = 0x7fff1fff,
		.cmd_l = 0x0ff,
		.queue = 0x1,
		.ret = 0,
	},
	/* TC 3. Normal COREX mode. */
	{
		.cmd = {
			.base_addr = 0xffffffff0, /* 36bit */
			.header_num = 0xfff,
			.set_mode = PCC_COREX,
			.fcount = 0xffff,
			.int_grp_en = 0xff,
		},
		.cmd_h = 0x00000000,
		.cmd_m = 0x7fff2000,
		.cmd_l = 0x0ff,
		.queue = 0x1,
		.ret = 0,
	},
	/* TC 4. Normal APB-direct mode. */
	{
		.cmd = {
			.base_addr = 0xffffffff0, /* 36bit */
			.header_num = 0xfff,
			.set_mode = PCC_APB_DIRECT,
			.fcount = 0xffff,
			.int_grp_en = 0xff,
		},
		.cmd_h = 0x00000000,
		.cmd_m = 0x7fff3000,
		.cmd_l = 0x0ff,
		.queue = 0x1,
		.ret = 0,
	},
	/* TC 5. DMA-direct mode without base_addr */
	{
		.cmd = {
			.base_addr = 0x000000000, /* 36bit */
			.header_num = 0xfff,
			.set_mode = PCC_DMA_DIRECT,
			.fcount = 0xffff,
			.int_grp_en = 0xff,
		},
		.cmd_h = 0x00000000,
		.cmd_m = 0x7fff3000,
		.cmd_l = 0x0ff,
		.queue = 0x1,
		.ret = 0,
	},
	/* TC 6. Abnormal CMDQ setting mode */
	{
		.cmd = {
			.base_addr = 0xffffffff0, /* 36bit */
			.header_num = 0xfff,
			.set_mode = PCC_SETTING_MODE_NUM,
			.fcount = 0xffff,
			.int_grp_en = 0xff,
		},
		.cmd_h = 0x00000000,
		.cmd_m = 0x00000000,
		.cmd_l = 0x000,
		.queue = 0x0,
		.ret = -EINVAL,
	},
};

static void pablo_hw_api_common_ctrl_m2m_shot_cmd_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_m2m;
	struct pablo_common_ctrl_frame_cfg frame_cfg;
	u32 tc_id;

	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));

	ret = CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);
	KUNIT_EXPECT_EQ(test, ret, -EPERM);

	SET_CR(CMN_CTRL_R_CMDQ_ENABLE, 1);

	for (tc_id = 0; tc_id < ARRAY_SIZE(cmd_tc); tc_id++) {
		memcpy(&frame_cfg.cmd, &cmd_tc[tc_id].cmd, sizeof(struct pablo_common_ctrl_cmd));

		ret = CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);
		KUNIT_EXPECT_EQ(test, ret, cmd_tc[tc_id].ret);
		KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_H), cmd_tc[tc_id].cmd_h);
		KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_M), cmd_tc[tc_id].cmd_m);
		KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_L), cmd_tc[tc_id].cmd_l);
		KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_ADD_TO_QUEUE_0), cmd_tc[tc_id].queue);

		SET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_H, 0);
		SET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_M, 0);
		SET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_L, 0);
		SET_CR(CMN_CTRL_R_CMDQ_ADD_TO_QUEUE_0, 0);
	}
}

static const struct cr_val_map int_cr[PCC_INT_ID_NUM][2] = {
	{
		{
			.cr_offset = CMN_CTRL_R_INT_REQ_INT0,
			.val = __LINE__,
		},
		{
			.cr_offset = CMN_CTRL_R_INT_REQ_INT0_CLEAR,
		},
	},
	{
		{
			.cr_offset = CMN_CTRL_R_INT_REQ_INT1,
			.val = __LINE__,
		},
		{
			.cr_offset = CMN_CTRL_R_INT_REQ_INT1_CLEAR,
		},
	},
	{
		{
			.cr_offset = CMN_CTRL_R_CMDQ_INT,
			.val = __LINE__,
		},
		{
			.cr_offset = CMN_CTRL_R_CMDQ_INT_CLEAR,
		},
	},
	{
		{
			.cr_offset = CMN_CTRL_R_COREX_INT,
			.val = __LINE__,
		},
		{
			.cr_offset = CMN_CTRL_R_COREX_INT_CLEAR,
		},
	},
};

static void pablo_hw_api_common_ctrl_m2m_get_int_status_kunit_test(struct kunit *test)
{
	u32 int_id, status;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_m2m;

	for (int_id = 0; int_id < PCC_INT_ID_NUM; int_id++) {
		SET_CR(int_cr[int_id][0].cr_offset, int_cr[int_id][0].val);

		status = CALL_PCC_OPS(pcc, get_int_status, pcc, int_id, false);
		KUNIT_EXPECT_EQ(test, status, int_cr[int_id][0].val);
		KUNIT_EXPECT_EQ(test, GET_CR(int_cr[int_id][1].cr_offset), 0);

		status = CALL_PCC_OPS(pcc, get_int_status, pcc, int_id, true);
		KUNIT_EXPECT_EQ(test, GET_CR(int_cr[int_id][1].cr_offset), status);
	}

	status = CALL_PCC_OPS(pcc, get_int_status, pcc, int_id, false);
	KUNIT_EXPECT_EQ(test, status, 0);
}

static void pablo_hw_api_common_ctrl_m2m_dump_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_m2m;
	u32 mode;

	SET_CR(CMN_CTRL_R_IP_USE_OTF_PATH_01, __LINE__);
	SET_CR(CMN_CTRL_R_IP_USE_OTF_PATH_23, __LINE__);
	SET_CR(CMN_CTRL_R_IP_USE_OTF_PATH_45, __LINE__);
	SET_CR(CMN_CTRL_R_IP_USE_OTF_PATH_67, __LINE__);
	SET_CR(CMN_CTRL_R_CMDQ_DEBUG_STATUS, 0x1f);
	SET_CR(CMN_CTRL_R_CMDQ_FRAME_COUNTER, __LINE__);
	SET_CR(CMN_CTRL_R_CMDQ_QUEUE_0_INFO, __LINE__);
	SET_CR(CMN_CTRL_R_CMDQ_FRAME_ID, __LINE__);
	SET_CR(CMN_CTRL_R_CMDQ_DEBUG_STATUS_PRE_LOAD, __LINE__);
	SET_CR(CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_H, __LINE__);
	SET_CR(CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_M, __LINE__);
	SET_CR(CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_L, __LINE__);

	for (mode = 0; mode < PCC_DUMP_MODE_NUM; mode++)
		CALL_PCC_OPS(pcc, dump, pcc, mode);
}

static void pablo_hw_api_common_ctrl_m2m_setting_done_handler_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;
	u32 status, int_id = PCC_INT_0, rptr = 3;

	SET_CR(int_cr[int_id][0].cr_offset, (1 << CMN_CTRL_INT0_SETTING_DONE_INT));
	SET_CR(CMN_CTRL_R_CMDQ_QUEUE_0_INFO, (rptr << 16));

	status = CALL_PCC_OPS(pcc, get_int_status, pcc, int_id, false);
	KUNIT_EXPECT_EQ(test, status, (1 << CMN_CTRL_INT0_SETTING_DONE_INT));
	KUNIT_EXPECT_EQ(test, pcc->dbg.last_rptr, rptr - 1);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_QUEUE_0_RPTR_FOR_DEBUG), pcc->dbg.last_rptr);
}

static void pablo_hw_api_common_ctrl_m2m_set_qch_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_m2m;

	/* 1st on */
	CALL_PCC_OPS(pcc, set_qch, pcc, true);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 1);

	/* 2nd on */
	CALL_PCC_OPS(pcc, set_qch, pcc, true);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 1);

	/* 1st off */
	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 1);

	/* 2nd off */
	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 0);

	/* 3rd off */
	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 0);

	/* 1st' on */
	CALL_PCC_OPS(pcc, set_qch, pcc, true);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 1);
}

static void pablo_hw_api_common_ctrl_otf_init_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;
	struct pablo_mmio *pmio = test_ctx.pmio;
	char name[PCC_NAME_LEN] = "PCC-KUNIT";
	u32 pcc_mode = PCC_OTF;

	CALL_PCC_HW_OPS(pcc, deinit, pcc);
	KUNIT_EXPECT_FALSE(test, test_bit(PABLO_SUBDEV_ALLOC, &pcc->subdev_cloader.state));

	ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, name, pcc_mode, NULL);
	KUNIT_EXPECT_NE(test, ret, 0);
	KUNIT_EXPECT_PTR_EQ(test, pcc->fields, NULL);
	KUNIT_EXPECT_PTR_EQ(test, pcc->pmio, NULL);

	ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, name, pcc_mode, test_ctx.mem);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, test_bit(PABLO_SUBDEV_ALLOC, &pcc->subdev_cloader.state));
}

static void pablo_hw_api_common_ctrl_otf_disable_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;

	CALL_PCC_OPS(pcc, disable, pcc);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_C_LOADER_ENABLE), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_VHD_CONTROL), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_PROCESSING), 0);
}

static void pablo_hw_api_common_ctrl_otf_shot_delay_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;
	const u32 delay = __LINE__;

	CALL_PCC_OPS(pcc, set_delay, pcc, 0);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_POST_FRAME_GAP), MIN_POST_FRAME_GAP);

	CALL_PCC_OPS(pcc, set_delay, pcc, delay);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_IP_POST_FRAME_GAP), delay);
}

static void pablo_hw_api_common_ctrl_otf_shot_dummy_cmd_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;
	struct pablo_common_ctrl_frame_cfg frame_cfg = {
		0,
	};
	u32 val = 0;

	ret = CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);
	KUNIT_EXPECT_EQ(test, ret, -EPERM);

	SET_CR(CMN_CTRL_R_CMDQ_ENABLE, 1);

	frame_cfg.cmd.fcount = __LINE__;
	val |= (1 << 31); /* CMD_ID */
	val |= (frame_cfg.cmd.fcount << 16); /* FRAME_ID */
	val |= (1 << 12); /* SETTING_MODE */
	val |= (1 << 0); /* HEADER_NUM */

	ret = CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, GET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_H), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_M), val);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_ADD_TO_QUEUE_0), 1);
}

static const struct cr_set ext_cr_set[] = {
	{ 0x4000, 0x0 },
	{ 0x4004, 0x0 },
	{ 0x4008, 0x0 },
	{ 0x400C, 0x0 },
};

static void pablo_hw_api_common_ctrl_otf_shot_ext_cr_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;
	struct pablo_common_ctrl_frame_cfg frame_cfg = {
		0,
	};
	u32 val = 0;

	SET_CR(CMN_CTRL_R_CMDQ_ENABLE, 1);

	frame_cfg.cmd.fcount = __LINE__;
	val |= (1 << 31); /* CMD_ID */
	val |= (frame_cfg.cmd.fcount << 16); /* FRAME_ID */
	val |= (1 << 12); /* SETTING_MODE */
	val |= (2 << 0); /* HEADER_NUM */
	frame_cfg.ext_cr_set.size = ARRAY_SIZE(ext_cr_set);
	frame_cfg.ext_cr_set.cr = ext_cr_set;

	ret = CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, GET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_H), 0);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_QUE_CMD_M), val);
	KUNIT_EXPECT_EQ(test, GET_CR(CMN_CTRL_R_CMDQ_ADD_TO_QUEUE_0), 1);
}

static void pablo_hw_api_common_ctrl_otf_frame_start_handler_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;
	u32 status, int_id = PCC_INT_0, fcount = __LINE__;

	SET_CR(int_cr[int_id][0].cr_offset, (1 << CMN_CTRL_INT0_FRAME_START_INT));
	SET_CR(CMN_CTRL_R_CMDQ_FRAME_ID, (fcount << 16));

	status = CALL_PCC_OPS(pcc, get_int_status, pcc, int_id, false);
	KUNIT_EXPECT_EQ(test, status, (1 << CMN_CTRL_INT0_FRAME_START_INT));
	KUNIT_EXPECT_EQ(test, pcc->dbg.last_fcount, (fcount & GENMASK(14, 0)));
}

static void pablo_hw_api_common_ctrl_otf_cmp_fcount_kunit_test(struct kunit *test)
{
	struct pablo_common_ctrl *pcc = &test_ctx.pcc_otf;
	u32 int_id = PCC_INT_0, fcount = __LINE__;
	int diff, result;

	/* Trigger to update PCC internal latest fcount by FS handler. */
	SET_CR(int_cr[int_id][0].cr_offset, (1 << CMN_CTRL_INT0_FRAME_START_INT));
	SET_CR(CMN_CTRL_R_CMDQ_FRAME_ID, (fcount << 16));

	CALL_PCC_OPS(pcc, get_int_status, pcc, int_id, false);

	diff = -3;
	result = CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount + diff);
	KUNIT_EXPECT_EQ(test, result, diff);

	diff = 3;
	result = CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount + diff);
	KUNIT_EXPECT_EQ(test, result, diff);

	result = CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount);
	KUNIT_EXPECT_EQ(test, result, 0);

	result = CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount + (0xffff << 15));
	KUNIT_EXPECT_EQ(test, result, 0);
}

static struct kunit_case pablo_hw_api_common_ctrl_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_shot_cotf_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_shot_cmd_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_get_int_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_setting_done_handler_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_m2m_set_qch_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_otf_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_otf_disable_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_otf_shot_delay_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_otf_shot_dummy_cmd_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_otf_shot_ext_cr_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_otf_frame_start_handler_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_ctrl_otf_cmp_fcount_kunit_test),
	{},
};

static struct pablo_mmio *setup_pmio(void)
{
	int ret;
	struct pmio_config pcfg;
	struct pablo_mmio *pmio;

	memset(&pcfg, 0, sizeof(struct pmio_config));

	pcfg.name = "PCC";
	pcfg.mmio_base = test_ctx.base;
	pcfg.cache_type = PMIO_CACHE_NONE;

	pcfg.max_register = CMN_CTRL_R_FREEZE_CORRUPTED_ENABLE;
	pcfg.num_reg_defaults_raw = (pcfg.max_register / PMIO_REG_STRIDE) + 1;
	pcfg.fields = cmn_ctrl_field_descs;
	pcfg.num_fields = ARRAY_SIZE(cmn_ctrl_field_descs);

	pmio = pmio_init(NULL, NULL, &pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	ret = pmio_field_bulk_alloc(pmio, &test_ctx.pmio_fields, pcfg.fields, pcfg.num_fields);
	if (ret)
		goto err_field_bulk_alloc;

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
err_init:
	return ERR_PTR(-EINVAL);
}

static int pablo_hw_api_common_ctrl_kunit_test_init(struct kunit *test)
{
	int ret;
	struct is_core *core;
	struct is_mem *mem;
	struct pablo_mmio *pmio;
	struct platform_driver *pdrv;
	struct platform_device *pdev = &test_ctx.pdev;
	struct pablo_common_ctrl *pcc;

	memset(&test_ctx, 0, sizeof(test_ctx));

	core = pablo_get_core_async();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	mem = &core->resourcemgr.mem;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, mem->is_mem_ops);

	test_ctx.mem = mem;
	test_ctx.base = kunit_kzalloc(test, 0x1000, 0);

	pmio = setup_pmio();
	KUNIT_ASSERT_FALSE(test, IS_ERR(pmio));

	pdrv = pablo_common_ctrl_api_get_platform_driver_v2_0();

	pcc = &test_ctx.pcc_m2m;
	platform_set_drvdata(pdev, pcc);
	pdrv->probe(pdev);

	ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, "PCC-KUNIT", PCC_M2M, NULL);
	KUNIT_ASSERT_EQ(test, ret, 0);

	pcc = &test_ctx.pcc_otf;
	platform_set_drvdata(pdev, pcc);
	pdrv->probe(pdev);

	ret = CALL_PCC_HW_OPS(pcc, init, pcc, pmio, "PCC-KUNIT", PCC_OTF, mem);
	KUNIT_ASSERT_EQ(test, ret, 0);

	test_ctx.pmio = pmio;

	return 0;
}

static void pablo_hw_api_common_ctrl_kunit_test_exit(struct kunit *test)
{
	struct pablo_common_ctrl *pcc;

	pcc = &test_ctx.pcc_m2m;
	CALL_PCC_HW_OPS(pcc, deinit, pcc);

	pcc = &test_ctx.pcc_otf;
	CALL_PCC_HW_OPS(pcc, deinit, pcc);

	if (test_ctx.pmio_fields) {
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		test_ctx.pmio_fields = NULL;
	}

	if (test_ctx.pmio) {
		pmio_exit(test_ctx.pmio);
		test_ctx.pmio = NULL;
	}

	kunit_kfree(test, test_ctx.base);
}

static struct kunit_suite pablo_hw_api_common_ctrl_v2_0_kunit_test_suite = {
	.name = "pablo-hw-api-common-ctrl-v2_0-kunit-test",
	.init = pablo_hw_api_common_ctrl_kunit_test_init,
	.exit = pablo_hw_api_common_ctrl_kunit_test_exit,
	.test_cases = pablo_hw_api_common_ctrl_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_common_ctrl_v2_0_kunit_test_suite);
