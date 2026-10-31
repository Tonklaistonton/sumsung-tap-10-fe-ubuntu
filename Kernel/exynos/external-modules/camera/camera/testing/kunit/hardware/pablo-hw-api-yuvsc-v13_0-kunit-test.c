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
#include "pmio.h"
#include "is-hw-common-dma.h"
#include "hardware/api/pablo-hw-api-yuvsc.h"
#include "hardware/sfr/pablo-sfr-yuvsc-v13_0.h"

#define CALL_HWAPI_OPS(op, args...) test_ctx.ops->op(args)

/* PMIO MACRO */
#define SET_CR(base, R, val) PMIO_SET_R(base, R, val)
#define SET_CR_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define SET_CR_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)

#define GET_CR(base, R) PMIO_GET_R(base, R)
#define GET_CR_F(base, R, F) PMIO_GET_F(base, R, F)

/* LOG MACRO */
#define HW_NAME "YUVSC"
#define yuvsc_info(fmt, args...) info_hw("[%s]" fmt "\n", HW_NAME, ##args)
#define yuvsc_dbg(level, fmt, args...) dbg_hw(level, "[%s]" fmt "\n", HW_NAME, ##args)
#define yuvsc_warn(fmt, args...) warn_hw("[%s]" fmt, HW_NAME, ##args)
#define yuvsc_err(fmt, args...) err_hw("[%s]" fmt, HW_NAME, ##args)

static struct pablo_hw_yuvsc_api_kunit_test_ctx {
	void *base;
	const struct yuvsc_hw_ops *ops;
	struct pmio_config pmio_config;
	struct pablo_mmio *pmio;
	struct pmio_field *pmio_fields;
	struct yuvsc_param_set p_set;
} test_ctx;

static void pablo_hw_api_yuvsc_hw_reset_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(reset, pmio);
}

static void pablo_hw_api_yuvsc_hw_init_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(init, pmio);

	/* s_otf */
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_RGB_CINFIFO_ENABLE), 1);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_CONFIG, YUVSC_F_RGB_CINFIFO_DEBUG_EN), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_YUV_COUTFIFO_ENABLE), 1);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_YUV_COUTFIFO_CONFIG, YUVSC_F_YUV_COUTFIFO_DEBUG_EN), 1);

	/* s_cloader */
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_STAT_RDMACL_EN), 1);
}

static void pablo_hw_api_yuvsc_hw_s_core_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct yuvsc_param_set *p_set;
	u32 w, h, val;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;

	/* TC#1. YUVSC Chain configuration. */
	w = __LINE__;
	h = __LINE__;
	p_set->otf_input.width = p_set->otf_output.width = w;
	p_set->otf_input.height = p_set->otf_output.height = h;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	val = (w << 16) | h;
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_CHAIN_SRC_IMG_SIZE), val);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_CHAIN_SCALED_IMG_SIZE), val);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_CHAIN_DST_IMG_SIZE), val);

	/* TC#2. Set to YUVSC Scale up */
	w = __LINE__;
	h = __LINE__;
	p_set->otf_output.width = w;
	p_set->otf_output.height = h;

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	/* Not support scale up */
	val = (p_set->otf_input.width << 16) | p_set->otf_input.height;
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_CHAIN_SRC_IMG_SIZE), val);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_CHAIN_SCALED_IMG_SIZE), val);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_CHAIN_DST_IMG_SIZE), val);

	/* TC#3. Set CRC seed with 0. */
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_STREAM_CRC, YUVSC_F_RGB_CINFIFO_CRC_SEED), 0);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_YUV_COUTFIFO_STREAM_CRC, YUVSC_F_YUV_COUTFIFO_CRC_SEED), 0);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_RGB_RGBTOYUV_STREAM_CRC, YUVSC_F_RGB_RGBTOYUV_CRC_SEED), 0);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(
			pmio, YUVSC_R_YUV_YUV444TO422_STREAM_CRC, YUVSC_F_YUV_YUV444TO422_CRC_SEED),
		0);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_YUV_SCALER_STREAM_CRC, YUVSC_F_YUV_SCALER_CRC_SEED), 0);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, YUVSC_R_RGB_GAMMA_STREAM_CRC, YUVSC_F_RGB_GAMMA_CRC_SEED), 0);
	KUNIT_EXPECT_EQ(
		test, GET_CR_F(pmio, YUVSC_R_YUV_CROP_STREAM_CRC, YUVSC_F_YUV_CROP_CRC_SEED), 0);

	/* TC#4. Set CRC seed with non-zero. */
	val = __LINE__;
	is_set_debug_param(IS_DEBUG_PARAM_CRC_SEED, val);

	CALL_HWAPI_OPS(s_core, pmio, p_set);

	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_STREAM_CRC, YUVSC_F_RGB_CINFIFO_CRC_SEED),
		(u8)val);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_YUV_COUTFIFO_STREAM_CRC, YUVSC_F_YUV_COUTFIFO_CRC_SEED),
		(u8)val);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_RGB_RGBTOYUV_STREAM_CRC, YUVSC_F_RGB_RGBTOYUV_CRC_SEED),
		(u8)val);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(
			pmio, YUVSC_R_YUV_YUV444TO422_STREAM_CRC, YUVSC_F_YUV_YUV444TO422_CRC_SEED),
		(u8)val);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_YUV_SCALER_STREAM_CRC, YUVSC_F_YUV_SCALER_CRC_SEED),
		(u8)val);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_RGB_GAMMA_STREAM_CRC, YUVSC_F_RGB_GAMMA_CRC_SEED), (u8)val);
	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_YUV_CROP_STREAM_CRC, YUVSC_F_YUV_CROP_CRC_SEED), (u8)val);
}

static void pablo_hw_api_yuvsc_hw_s_path_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	struct yuvsc_param_set *p_set;
	struct pablo_common_ctrl_frame_cfg frame_cfg;

	pmio = test_ctx.pmio;
	p_set = &test_ctx.p_set;

	/* TC#1. Enable every path. */
	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));
	p_set->otf_input.cmd = 1;
	p_set->otf_output.cmd = 1;

	CALL_HWAPI_OPS(s_path, pmio, p_set, &frame_cfg);

	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_in_en, 0x1);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_out_en, 0x1);

	/* TC#2. Disable every path. */
	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));
	p_set->otf_input.cmd = 0;
	p_set->otf_output.cmd = 0;

	CALL_HWAPI_OPS(s_path, pmio, p_set, &frame_cfg);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_in_en, 0x0);
	KUNIT_EXPECT_EQ(test, frame_cfg.cotf_out_en, 0x0);
}

#define YUVSC_LIC_CH_CNT 4
struct yuvsc_line_buffer {
	u32 offset[YUVSC_LIC_CH_CNT];
};

/* EVT0 : assume that ctx3 is used for reprocessing */
static struct yuvsc_line_buffer lb_offset_evt0 = {
	.offset = { 0, 4096, 8192, 12288 }, /* offset < 26000 & 16px aligned (8ppc) */
};
/* EVT1.1~ : assume that ctx0 is used for reprocessing */
static struct yuvsc_line_buffer lb_offset = {
	.offset = { 0, 13712, 17808, 21904 }, /* offset < 26000 & 16px aligned (8ppc) */
};

static void pablo_hw_api_yuvsc_hw_s_lbctrl_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int lic_ch;
	struct yuvsc_line_buffer lb;

	if (exynos_soc_info.main_rev >= 1 &&
		exynos_soc_info.sub_rev >= 1)
		lb = lb_offset;
	else
		lb = lb_offset_evt0;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(s_lbctrl, pmio);

	for (lic_ch = 0; lic_ch < YUVSC_LIC_CH_CNT; lic_ch++) {
		KUNIT_EXPECT_EQ(test,
			GET_CR_F(pmio, YUVSC_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x10 * lic_ch),
				YUVSC_F_CHAIN_LBCTRL_OFFSET_GRP0_C0 + lic_ch),
			lb.offset[lic_ch]);
	}
}

static void pablo_hw_api_yuvsc_hw_g_int_en_kunit_test(struct kunit *test)
{
	u32 int_en[PCC_INT_ID_NUM] = { 0 };

	CALL_HWAPI_OPS(g_int_en, int_en);

	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_0], INT0_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_INT_1], INT1_EN_MASK);
	KUNIT_EXPECT_EQ(test, int_en[PCC_CMDQ_INT], 0);
	KUNIT_EXPECT_EQ(test, int_en[PCC_COREX_INT], 0);
}

#define YUVSC_INT_GRP_EN_MASK                                                                      \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
static void pablo_hw_api_yuvsc_hw_g_int_grp_en_kunit_test(struct kunit *test)
{
	u32 int_grp_en;

	int_grp_en = CALL_HWAPI_OPS(g_int_grp_en);

	KUNIT_EXPECT_EQ(test, int_grp_en, YUVSC_INT_GRP_EN_MASK);
}

static void pablo_hw_api_yuvsc_hw_wait_idle_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;
	int ret;

	pmio = test_ctx.pmio;

	/* TC#1. Timeout to wait idleness. */
	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);

	/* TC#2. Succeed to wait idleness. */
	SET_CR(pmio, YUVSC_R_IDLENESS_STATUS, 1);

	ret = CALL_HWAPI_OPS(wait_idle, pmio);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_yuvsc_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_CR);
	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_DBG_STATE);
	CALL_HWAPI_OPS(dump, pmio, HW_DUMP_MODE_NUM);
}

static void pablo_hw_api_yuvsc_hw_is_occurred_kunit_test(struct kunit *test)
{
	bool occur;
	u32 status;
	ulong type;

	/* TC#1. No interrupt. */
	status = 0;
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	/* TC#2. Test each interrupt. */
	status = BIT_MASK(INTR0_YUVSC_FRAME_START_INT);
	type = BIT_MASK(INT_FRAME_START);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_YUVSC_FRAME_END_INT);
	type = BIT_MASK(INT_FRAME_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_YUVSC_COREX_END_INT_0);
	type = BIT_MASK(INT_COREX_END);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_YUVSC_SETTING_DONE_INT);
	type = BIT_MASK(INT_SETTING_DONE);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR0_YUVSC_CMDQ_ERROR_INT);
	type = BIT_MASK(INT_ERR0);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	status = BIT_MASK(INTR1_YUVSC_LIC_LITE_DBG_CNT_ERR);
	type = BIT_MASK(INT_ERR1);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);

	/* TC#3. Test interrupt ovarlapping. */
	status = BIT_MASK(INTR0_YUVSC_FRAME_START_INT);
	type = BIT_MASK(INTR0_YUVSC_FRAME_START_INT) | BIT_MASK(INTR0_YUVSC_FRAME_END_INT);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, false);

	status = BIT_MASK(INTR0_YUVSC_FRAME_START_INT) | BIT_MASK(INTR0_YUVSC_FRAME_END_INT);
	occur = CALL_HWAPI_OPS(is_occurred, status, type);
	KUNIT_EXPECT_EQ(test, occur, true);
}

static void pablo_hw_api_yuvsc_hw_s_strgen_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio;

	pmio = test_ctx.pmio;

	CALL_HWAPI_OPS(s_strgen, pmio);

	KUNIT_EXPECT_EQ(test,
		GET_CR_F(pmio, YUVSC_R_RGB_CINFIFO_CONFIG, YUVSC_F_RGB_CINFIFO_STRGEN_MODE_EN), 1);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_IP_USE_CINFIFO_NEW_FRAME_IN), PCC_ASAP);
}

static void pablo_hw_api_yuvsc_hw_clr_cotf_err_kunit_test(struct kunit *test)
{
	struct pablo_mmio *pmio = test_ctx.pmio;
	const u32 val = __LINE__;

	SET_CR(pmio, YUVSC_R_RGB_CINFIFO_INT, val);
	SET_CR(pmio, YUVSC_R_YUV_COUTFIFO_INT, val);

	CALL_HWAPI_OPS(clr_cotf_err, pmio);

	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_RGB_CINFIFO_INT_CLEAR), val);
	KUNIT_EXPECT_EQ(test, GET_CR(pmio, YUVSC_R_YUV_COUTFIFO_INT_CLEAR), val);
}

static struct kunit_case pablo_hw_api_yuvsc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_s_lbctrl_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_g_int_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_g_int_grp_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvsc_hw_clr_cotf_err_kunit_test),
	{},
};

static struct pablo_mmio *pablo_hw_api_yuvsc_pmio_init(void)
{
	struct pmio_config *pcfg;
	struct pablo_mmio *pmio;

	pcfg = &test_ctx.pmio_config;

	pcfg->name = "YUVSC";
	pcfg->mmio_base = test_ctx.base;
	pcfg->cache_type = PMIO_CACHE_NONE;

	yuvsc_hw_g_pmio_cfg(pcfg);

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

static int pablo_hw_api_yuvsc_kunit_test_init(struct kunit *test)
{
	memset(&test_ctx, 0, sizeof(test_ctx));

	test_ctx.base = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.base);

	test_ctx.ops = yuvsc_hw_g_ops();

	test_ctx.pmio = pablo_hw_api_yuvsc_pmio_init();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.pmio);

	return 0;
}

static void pablo_hw_api_yuvsc_pmio_deinit(void)
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

static void pablo_hw_api_yuvsc_kunit_test_exit(struct kunit *test)
{
	pablo_hw_api_yuvsc_pmio_deinit();
	kunit_kfree(test, test_ctx.base);

	memset(&test_ctx, 0, sizeof(test_ctx));
}

struct kunit_suite pablo_hw_api_yuvsc_kunit_test_suite = {
	.name = "pablo-hw-api-yuvsc-v13_0-kunit-test",
	.init = pablo_hw_api_yuvsc_kunit_test_init,
	.exit = pablo_hw_api_yuvsc_kunit_test_exit,
	.test_cases = pablo_hw_api_yuvsc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_yuvsc_kunit_test_suite);

MODULE_LICENSE("GPL");
