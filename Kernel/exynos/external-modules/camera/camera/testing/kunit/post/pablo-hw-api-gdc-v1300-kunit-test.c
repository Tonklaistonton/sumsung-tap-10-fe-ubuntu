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
#include "gdc/pablo-gdc.h"
#include "gdc/pablo-hw-api-gdc.h"
#if IS_ENABLED(CONFIG_PABLO_V13_0_0)
#include "gdc/pablo-hw-reg-gdc-v1300.h"
#endif
#include "is-common-enum.h"
#include "is-type.h"

#define GDC_GET_F(base, R, F) PMIO_GET_F((base)->pmio, R, F)
#define GDC_GET_R(base, R) PMIO_GET_R((base)->pmio, R)

static struct cameraepp_hw_api_gdc_kunit_test_ctx {
	void *addr;
	struct gdc_dev *gdc;
	struct gdc_crop_param *crop_param;
	struct resource rsc;
} test_ctx;

static const struct gdc_variant gdc_variant = {
	.limit_input = {
		.min_w = 96,
		.min_h = 64,
		.max_w = 16384,
		.max_h = 12288,
	},
	.limit_output = {
		.min_w = 96,
		.min_h = 64,
		.max_w = 16384,
		.max_h = 12288,
	},
	.version = 0x13000000,
};

static u32 support_sbwc_fmt_array[] = {
	V4L2_PIX_FMT_NV12M_SBWCL_32_8B,	   V4L2_PIX_FMT_NV12M_SBWCL_32_10B,
	V4L2_PIX_FMT_NV12M_SBWCL_64_8B,	   V4L2_PIX_FMT_NV12M_SBWCL_64_10B,
	V4L2_PIX_FMT_NV12M_SBWCL_64_8B_FR, V4L2_PIX_FMT_NV12M_SBWCL_64_10B_FR,
	V4L2_PIX_FMT_NV12M_SBWC_8B,        V4L2_PIX_FMT_NV12M_SBWC_10B,
};

static const struct gdc_fmt gdc_formats[] = {
	{
		.name = "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat = V4L2_PIX_FMT_NV12M,
		.bitperpixel = { 8, 4 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "YUV 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat = V4L2_PIX_FMT_NV21M,
		.bitperpixel = { 8, 4 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "YUV 4:2:0 contiguous 2-planar, Y/CbCr",
		.pixelformat = V4L2_PIX_FMT_NV12,
		.bitperpixel = { 8 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "YUV 4:2:0 contiguous 2-planar, Y/CrCb",
		.pixelformat = V4L2_PIX_FMT_NV21,
		.bitperpixel = { 8 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "YUV 4:2:2 packed, YCrYCb",
		.pixelformat = V4L2_PIX_FMT_YVYU,
		.bitperpixel = { 16 },
		.num_planes = 1,
		.num_comp = 1,
		.h_shift = 1,
	}, {
		.name = "YUV 4:2:2 packed, YCbYCr",
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.bitperpixel = { 16 },
		.num_planes = 1,
		.num_comp = 1,
		.h_shift = 1,
	}, {
		.name = "YUV 4:2:2 contiguous 2-planar, Y/CbCr",
		.pixelformat = V4L2_PIX_FMT_NV16,
		.bitperpixel = { 8 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "YUV 4:2:2 contiguous 2-planar, Y/CrCb",
		.pixelformat = V4L2_PIX_FMT_NV61,
		.bitperpixel = { 8 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "YUV 4:2:2 non-contiguous 2-planar, Y/CbCr",
		.pixelformat = V4L2_PIX_FMT_NV16M,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "YUV 4:2:2 non-contiguous 2-planar, Y/CrCb",
		.pixelformat = V4L2_PIX_FMT_NV61M,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "P010_16B",
		.pixelformat = V4L2_PIX_FMT_NV12M_P010,
		.bitperpixel = { 16, 16 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "P010_16B",
		.pixelformat = V4L2_PIX_FMT_NV21M_P010,
		.bitperpixel = { 16, 16 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "P210_16B",
		.pixelformat = V4L2_PIX_FMT_NV16M_P210,
		.bitperpixel = { 16, 16 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "P210_16B",
		.pixelformat = V4L2_PIX_FMT_NV61M_P210,
		.bitperpixel = { 16, 16 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "YUV422 2P 10bit(8+2)",
		.pixelformat = V4L2_PIX_FMT_NV16M_S10B,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "YUV422 2P 10bit(8+2)",
		.pixelformat = V4L2_PIX_FMT_NV61M_S10B,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "YUV420 2P 10bit(8+2)",
		.pixelformat = V4L2_PIX_FMT_NV12M_S10B,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "YUV420 2P 10bit(8+2)",
		.pixelformat = V4L2_PIX_FMT_NV21M_S10B,
		.bitperpixel = { 8, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSY 8bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_8B,
		.bitperpixel = { 8, 4 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSY 10bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_10B,
		.bitperpixel = { 16, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSY 32B Align 8bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_32_8B,
		.bitperpixel = { 8, 4 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSY 32B Align 10bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_32_10B,
		.bitperpixel = { 16, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSY 64B Align 8bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_8B,
		.bitperpixel = { 8, 4 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSY 64B Align 10bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_10B,
		.bitperpixel = { 16, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M 8bit SBWC Lossy 64B align footprint reduction",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_8B_FR,
		.bitperpixel = { 8, 4 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M 10bit SBWC Lossy 64B align footprint reduction",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_10B_FR,
		.bitperpixel = { 16, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSLESS 8bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWC_8B,
		.bitperpixel = { 8, 4 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "NV12M SBWC LOSSLESS 10bit",
		.pixelformat = V4L2_PIX_FMT_NV12M_SBWC_10B,
		.bitperpixel = { 16, 8 },
		.num_planes = 2,
		.num_comp = 2,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "Y 8bit",
		.pixelformat = V4L2_PIX_FMT_GREY,
		.bitperpixel = { 8 },
		.num_planes = 1,
		.num_comp = 1,
		.h_shift = 1,
		.v_shift = 1,
	}, {
		.name = "P010_16B_contiguous",
		.pixelformat = V4L2_PIX_FMT_P010,
		.bitperpixel = { 16 },
		.num_planes = 1,
		.num_comp = 2,
		.h_shift = 1,
	},
};

/* Define the test cases. */
static void camerapp_hw_api_gdc_get_size_constraints_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	const struct gdc_variant *constraints;
	u32 val;

	*(u32 *)(tctx->addr + GDC_R_IP_VERSION) = gdc_variant.version;

	constraints = camerapp_hw_gdc_get_size_constraints(gdc->pmio);
	val = camerapp_hw_gdc_get_ver(gdc->pmio);

	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_w, gdc_variant.limit_input.min_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_h, gdc_variant.limit_input.min_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_w, gdc_variant.limit_input.max_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_h, gdc_variant.limit_input.max_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.min_w, gdc_variant.limit_output.min_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.min_h, gdc_variant.limit_output.min_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.max_w, gdc_variant.limit_output.max_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.max_h, gdc_variant.limit_output.max_h);
	KUNIT_EXPECT_EQ(test, constraints->version, gdc_variant.version);
	KUNIT_EXPECT_EQ(test, constraints->version, val);
}

static void camerapp_hw_api_gdc_sfr_dump_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	camerapp_gdc_sfr_dump(gdc->regs_base);
}

static void camerapp_hw_api_gdc_start_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct c_loader_buffer clb;
	u32 val;

	/* APB-DIRECT */
	*(u32 *)(tctx->addr + GDC_R_IP_PROCESSING) = 0x1;

	camerapp_hw_gdc_start(gdc->pmio, &clb);

	val = GDC_GET_R(gdc, GDC_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, 1);
	val = GDC_GET_R(gdc, GDC_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, 0);

	/* C-LOADER */
	*(u32 *)(tctx->addr + GDC_R_IP_PROCESSING) = 0x1;
	*(u32 *)(tctx->addr + GDC_R_CMDQ_ADD_TO_QUEUE_0) = 0x0;

	clb.header_dva = 0xDEADDEAD;
	clb.num_of_headers = 5;
	camerapp_hw_gdc_start(gdc->pmio, &clb);

	val = GDC_GET_R(gdc, GDC_R_CMDQ_QUE_CMD_H);
	KUNIT_EXPECT_EQ(test, val, clb.header_dva >> 4);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_QUE_CMD_M);
	KUNIT_EXPECT_EQ(test, val, (1 << 12) | clb.num_of_headers);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void camerapp_hw_api_gdc_stop_kunit_test(struct kunit *test)
{
}

static void camerapp_hw_api_gdc_sw_reset_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val1, val2;

	val1 = camerapp_hw_gdc_sw_reset(gdc->pmio);
	val2 = GDC_GET_R(gdc, GDC_R_SW_RESET);
	KUNIT_EXPECT_EQ(test, val1, (u32)10001);
	KUNIT_EXPECT_EQ(test, val2, (u32)1);
}

static void camerapp_hw_gdc_get_intr_status_and_clear_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val0, c0, c1;
	u32 int0 = 0xAAAABBBB;
	u32 int1 = 0xCCCCDDDD;

	*(u32 *)(tctx->addr + GDC_R_INT_REQ_INT0) = int0;
	*(u32 *)(tctx->addr + GDC_R_INT_REQ_INT1) = int1;

	val0 = camerapp_hw_gdc_get_intr_status_and_clear(gdc->pmio);

	c0 = GDC_GET_R(gdc, GDC_R_INT_REQ_INT0_CLEAR);
	c1 = GDC_GET_R(gdc, GDC_R_INT_REQ_INT1_CLEAR);

	KUNIT_EXPECT_EQ(test, val0, int0);
	KUNIT_EXPECT_EQ(test, c0, int0);
	KUNIT_EXPECT_EQ(test, c1, int1);
}

static void camerapp_hw_gdc_get_fs_fe_kunit_test(struct kunit *test)
{
	u32 fs = GDC_INT_FRAME_START, fe = GDC_INT_FRAME_END;
	u32 val1, val2;

	val1 = camerapp_hw_gdc_get_int_frame_start();
	val2 = camerapp_hw_gdc_get_int_frame_end();

	KUNIT_EXPECT_EQ(test, fs, val1);
	KUNIT_EXPECT_EQ(test, fe, val2);
}

static int __find_supported_sbwc_fmt(u32 fmt)
{
	int i, ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(support_sbwc_fmt_array); i++) {
		if (support_sbwc_fmt_array[i] == fmt) {
			ret = 0;
			break;
		}
	}

	return ret;
}

static void camerapp_hw_gdc_get_sbwc_const_kunit_test(struct kunit *test)
{
	u32 width, height, pixformat;
	int i, val, result;

	width = 320;
	height = 240;

	for (i = 0; i < ARRAY_SIZE(gdc_formats); ++i) {
		pixformat = gdc_formats[i].pixelformat;
		result = __find_supported_sbwc_fmt(pixformat);
		val = camerapp_hw_get_sbwc_constraint(pixformat, width, height, 0);
		KUNIT_EXPECT_EQ(test, result, val);
	}
}

static void camerapp_hw_gdc_has_comp_header_kunit_test(struct kunit *test)
{
	bool val;

	val = camerapp_hw_gdc_has_comp_header(0);

	KUNIT_EXPECT_EQ(test, val, (bool)true);
}

static void camerapp_hw_get_comp_buf_size_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_frame frame;
	struct gdc_dev *gdc = tctx->gdc;
	u32 width, height, pixfmt, plane;
	int val1, val2;

	width = 320;
	height = 240;
	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_32_8B;
	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_ALL);
		val2 = plane ? (u32)(SBWCL_32_CBCR_SIZE(width, height) +
				     SBWCL_CBCR_HEADER_SIZE(width, height)) :
			       (u32)(SBWCL_32_Y_SIZE(width, height) +
				     SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_64_8B;
	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_ALL);
		val2 = plane ? (u32)(SBWCL_64_CBCR_SIZE(width, height) +
				     SBWCL_CBCR_HEADER_SIZE(width, height)) :
			       (u32)(SBWCL_64_Y_SIZE(width, height) +
				     SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_PAYLOAD);
		val2 = plane ? (u32)(SBWCL_64_CBCR_SIZE(width, height)) :
			       (u32)(SBWCL_64_Y_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_HEADER);
		val2 = plane ? (u32)(SBWCL_CBCR_HEADER_SIZE(width, height)) :
			       (u32)(SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_8B;
	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_ALL);
		KUNIT_EXPECT_EQ(test, val1, (int)-EINVAL);
	}

	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_64_8B_FR;
	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_ALL);
		val2 = plane ? (u32)(SBWCL_64_CBCR_SIZE_FR(width, height) +
				     SBWCL_CBCR_HEADER_SIZE(width, height)) :
			       (u32)(SBWCL_64_Y_SIZE_FR(width, height) +
				     SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_PAYLOAD);
		val2 = plane ? (u32)(SBWCL_64_CBCR_SIZE_FR(width, height)) :
			       (u32)(SBWCL_64_Y_SIZE_FR(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame, width, height, pixfmt, plane,
						     GDC_SBWC_SIZE_HEADER);
		val2 = plane ? (u32)(SBWCL_CBCR_HEADER_SIZE(width, height)) :
			       (u32)(SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}
}

static void camerapp_hw_gdc_set_initialization_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	camerapp_hw_gdc_set_initialization(gdc->pmio);

	val = GDC_GET_R(gdc, GDC_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_C_LOADER_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_STAT_RDMACLOADER_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_F(gdc, GDC_R_CMDQ_QUE_CMD_L, GDC_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0xFF);
	val = GDC_GET_F(gdc, GDC_R_CMDQ_QUE_CMD_M, GDC_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)3);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_INT_REQ_INT0_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)GDC_INT0_EN);
	val = GDC_GET_R(gdc, GDC_R_INT_REQ_INT1_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)GDC_INT1_EN);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_gdc_check_scale_parameters(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 val1, val2;
	u32 crop_width, crop_height;
	u32 gdc_scale_x, gdc_scale_y, gdc_scale_width, gdc_scale_height;
	u32 scaleShifterX, scaleShifterY, imagewidth, imageheight, gdc_scale_shifter_x,
		gdc_scale_shifter_y;
	u32 gdc_inv_scale_x, gdc_inv_scale_y, out_scaled_width, out_scaled_height;

	if (crop_param->is_bypass_mode) {
		crop_width = ctx->s_frame.width;
		crop_height = ctx->s_frame.height;
		gdc_scale_width = ctx->d_frame.width;
		gdc_scale_height = ctx->d_frame.height;
		out_scaled_width = 8192;
		out_scaled_height = 8192;
	} else {
		crop_width = crop_param->crop_width;
		crop_height = crop_param->crop_height;
		gdc_scale_width = ctx->s_frame.width;
		gdc_scale_height = ctx->s_frame.height;
		out_scaled_width = 8192 * crop_param->crop_width / ctx->d_frame.width;
		out_scaled_height = 8192 * crop_param->crop_height / ctx->d_frame.height;
	}

	scaleShifterX = DS_SHIFTER_MAX;
	imagewidth = gdc_scale_width << 1;
	while ((imagewidth <= MAX_VIRTUAL_GRID_X) && (scaleShifterX > 0)) {
		imagewidth <<= 1;
		scaleShifterX--;
	}
	gdc_scale_shifter_x = scaleShifterX;

	scaleShifterY = DS_SHIFTER_MAX;
	imageheight = gdc_scale_height << 1;
	while ((imageheight <= MAX_VIRTUAL_GRID_Y) && (scaleShifterY > 0)) {
		imageheight <<= 1;
		scaleShifterY--;
	}
	gdc_scale_shifter_y = scaleShifterY;

	gdc_scale_x = MIN(65535, ((MAX_VIRTUAL_GRID_X << (DS_FRACTION_BITS + gdc_scale_shifter_x)) +
				  gdc_scale_width / 2) /
					 gdc_scale_width);
	gdc_scale_y = MIN(65535, ((MAX_VIRTUAL_GRID_Y << (DS_FRACTION_BITS + gdc_scale_shifter_y)) +
				  gdc_scale_height / 2) /
					 gdc_scale_height);

	gdc_inv_scale_x = ctx->s_frame.width;
	gdc_inv_scale_y = ((ctx->s_frame.height << 3) + 3) / 6;

	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_CONFIG);
	KUNIT_EXPECT_EQ(test, val1, (u32)0);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_ORG_SIZE);
	val2 = (ctx->s_frame.height << 16) | ctx->s_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_START);
	val2 = (crop_param->crop_start_y << 16) | crop_param->crop_start_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_SIZE);
	val2 = (crop_height << 16) | crop_width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE);
	val2 = (gdc_scale_y << 16) | gdc_scale_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE_SHIFTER);
	val2 = (gdc_scale_shifter_y << 4) | gdc_scale_shifter_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INV_SCALE);
	val2 = (gdc_inv_scale_y << 16) | gdc_inv_scale_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_START);
	val2 = (0x0 << 16) | 0x0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_SIZE);
	val2 = (ctx->d_frame.height << 16) | ctx->d_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_SCALE);
	val2 = (out_scaled_height << 16) | out_scaled_width;
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void camerapp_hw_gdc_check_grid_parameters(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 i, j;
	u32 cal_sfr_offset;
	u32 val1, val2;
	u32 sfr_offset = 0x0004;
	u32 sfr_start_x = GDC_R_YUV_GDC_GRID_DX_0_0;
	u32 sfr_start_y = GDC_R_YUV_GDC_GRID_DY_0_0;

	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);
			val1 = GDC_GET_R(gdc, sfr_start_x + cal_sfr_offset);
			val2 = GDC_GET_R(gdc, sfr_start_y + cal_sfr_offset);

			if (crop_param->use_calculated_grid) {
				KUNIT_EXPECT_EQ(test, val1, crop_param->calculated_grid_x[i][j]);
				KUNIT_EXPECT_EQ(test, val2, crop_param->calculated_grid_y[i][j]);
			} else {
				KUNIT_EXPECT_EQ(test, val1, 0);
				KUNIT_EXPECT_EQ(test, val2, 0);
			}
		}
	}
}

static void camerapp_hw_gdc_check_compressor(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_frame *d_frame, *s_frame;
	u32 val1, val2;
	u32 align_64b = 0;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	if (s_frame->extra && d_frame->extra) {
		if (s_frame->pixelformat == V4L2_PIX_FMT_NV12M_SBWCL_64_8B &&
		    d_frame->pixelformat == V4L2_PIX_FMT_NV12M_SBWCL_64_8B)
			align_64b = 1;

		val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_COMPRESSOR);
		val2 = (d_frame->extra << 8) | s_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_COMP_CONTROL);
		val2 = (align_64b << 3) | s_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_COMP_CONTROL);
		val2 = (align_64b << 3) | s_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_COMP_LOSSY_QUALITY_CONTROL);
		KUNIT_EXPECT_EQ(test, val1, 0);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_COMP_LOSSY_QUALITY_CONTROL);
		KUNIT_EXPECT_EQ(test, val1, 0);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_COMP_CONTROL);
		val2 = (align_64b << 3) | d_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_COMP_LOSSY_QUALITY_CONTROL);
		KUNIT_EXPECT_EQ(test, val1, 0);
	} else {
		val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_COMPRESSOR);
		KUNIT_EXPECT_EQ(test, val1, 0);
	}
}

static void camerapp_hw_gdc_check_format(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_YUV_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x800);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x800);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x800);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_MONO_MODE);
	KUNIT_EXPECT_EQ(test, val, 0x0);
}

static void camerapp_hw_gdc_check_dma_addr(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_frame *d_frame, *s_frame;
	u32 val;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	/* CORE */
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_EN);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_EN);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_EN);
	KUNIT_EXPECT_EQ(test, val, 0x1);

	/* WDMA */
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cb >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cb & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cbcr_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cbcr_2bit & 0xF);

	/* RDMA */
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit & 0xF);

	if (d_frame->io_mode == GDC_OUT_OTF || d_frame->io_mode == GDC_OUT_NONE) {
		val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_EN);
		KUNIT_EXPECT_EQ(test, val, 0x0);
	}
}

static void camerapp_hw_gdc_check_dma_size(struct kunit *test, u32 wdma_width, u32 wdma_height,
					   u32 rdma_y_width, u32 rdma_y_height, u32 rdma_uv_width,
					   u32 rdma_uv_height, u32 wdma_stride_1p,
					   u32 wdma_stride_2p, u32 wdma_stride_header_1p,
					   u32 wdma_stride_header_2p, u32 rdma_stride_1p,
					   u32 rdma_stride_2p, u32 rdma_stride_header_1p,
					   u32 rdma_stride_header_2p)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_WIDTH);
	KUNIT_EXPECT_EQ(test, val, wdma_width);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, wdma_height);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_WIDTH);
	KUNIT_EXPECT_EQ(test, val, rdma_y_width);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, rdma_y_height);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_WIDTH);
	KUNIT_EXPECT_EQ(test, val, rdma_uv_width);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, rdma_uv_height);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_STRIDE_2P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_2p);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_header_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_STRIDE_2P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_header_2p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_2p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_header_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_header_2p);
}

static void camerapp_hw_gdc_check_core_param(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 val;

	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_INTERPOLATION);
	KUNIT_EXPECT_EQ(test, val, 0x23);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_BOUNDARY_OPTION);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_GRID_MODE);
	KUNIT_EXPECT_EQ(test, val, crop_param->is_grid_mode);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_YUV_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_LUMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, 0xFF0000);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_CHROMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, 0xFF0000);
}

static void camerapp_hw_gdc_check_out_select(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 val1, val2;

	if (crop_param->out_mode == GDC_OUT_M2M) {
		val2 = 0x1;
	} else { /* OTF */
		/* HEVC codec type is default value in OTF interface*/
		val2 = 0x6;

		if (crop_param->codec_type == GDC_CODEC_TYPE_H264)
			val2 = 0xA;
	}

	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUTPUT_SELECT);
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void camerapp_hw_gdc_check_votf_param(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val1, val2;

	val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_VOTF_EN);
	val2 = 0x3;
	KUNIT_EXPECT_EQ(test, val1, val2);

	val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_BUSINFO);
	val2 = 0x440; /* IS_LLC_CACHE_HINT_VOTF_TYPE | IS_32B_WRITE_ALLOC_SHIFT */
	KUNIT_EXPECT_EQ(test, val1, val2);
}

#if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)
static void camerapp_hw_gdc_check_hdr10p_enable(struct kunit *test, u32 ref)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	KUNIT_EXPECT_EQ(test, GDC_GET_R(gdc, GDC_R_RGB_HDR10PLUSSTAT_ENABLE), ref);
}

static void camerapp_hw_gdc_check_hdr10p_config(struct kunit *test, u32 ref)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	KUNIT_EXPECT_EQ(test, GDC_GET_R(gdc, GDC_R_RGB_HDR10PLUSSTAT_CONFIG), ref);
}

static void camerapp_hw_gdc_check_hdr10p_wdma_addr(struct kunit *test, u32 ref)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	KUNIT_EXPECT_EQ(test, GDC_GET_F(gdc, GDC_R_RGB_HDR10PLUSSTAT_IMG_BASE_ADDR_FRO_0_1,
		GDC_F_RGB_HDR10PLUSSTAT_WDMA_BASE_ADDR_FRO_0_1), DVA_GET_MSB(ref));
	KUNIT_EXPECT_EQ(test, GDC_GET_F(gdc, GDC_R_RGB_HDR10PLUSSTAT_IMG_BASE_ADDR_FRO_0_0,
		GDC_F_RGB_HDR10PLUSSTAT_WDMA_BASE_ADDR_FRO_0_0), DVA_GET_LSB(ref));
}

static void camerapp_hw_gdc_check_hdr10p_wdma_enable(struct kunit *test, u32 ref)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	KUNIT_EXPECT_EQ(test, GDC_GET_F(gdc, GDC_R_STAT_WDMAHDR10PLUSSTAT,
		GDC_F_STAT_WDMAHDR10PLUSSTAT_EN), ref);
}
#endif // #if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)

static void camerapp_hw_gdc_update_param_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx;
	u32 i, j;
	u64 addr = 0xDEADDEADF;

	ctx = kunit_kzalloc(test, sizeof(struct gdc_ctx), 0);

	ctx->s_frame.width = 320;
	ctx->s_frame.height = 240;
	ctx->s_frame.gdc_fmt = &gdc_formats[1];
	ctx->s_frame.extra = NONE;
	ctx->s_frame.pixelformat = V4L2_PIX_FMT_NV21M;
	ctx->s_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->s_frame.addr.y = addr;
	ctx->s_frame.addr.cb = addr;
	ctx->s_frame.addr.y_2bit = addr;
	ctx->s_frame.addr.cbcr_2bit = addr;
	ctx->d_frame.width = 320;
	ctx->d_frame.height = 240;
	ctx->d_frame.gdc_fmt = &gdc_formats[1];
	ctx->d_frame.extra = NONE;
	ctx->d_frame.pixelformat = V4L2_PIX_FMT_NV21M;
	ctx->d_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->d_frame.addr.y = addr;
	ctx->d_frame.addr.cb = addr;
	ctx->d_frame.addr.y_2bit = addr;
	ctx->d_frame.addr.cbcr_2bit = addr;
	ctx->d_frame.io_mode = GDC_OUT_M2M;
	ctx->cur_index = 0;
	ctx->crop_param[ctx->cur_index].is_grid_mode = 0;
	ctx->crop_param[ctx->cur_index].is_bypass_mode = 0;
	ctx->crop_param[ctx->cur_index].use_calculated_grid = 0;
	ctx->crop_param[ctx->cur_index].crop_start_x = 0;
	ctx->crop_param[ctx->cur_index].crop_start_y = 0;
	ctx->crop_param[ctx->cur_index].crop_width = 320;
	ctx->crop_param[ctx->cur_index].crop_height = 240;
	ctx->crop_param[ctx->cur_index].src_bytesperline[0] = 0;
	ctx->crop_param[ctx->cur_index].src_bytesperline[1] = 0;
	ctx->crop_param[ctx->cur_index].src_bytesperline[2] = 0;
	ctx->crop_param[ctx->cur_index].dst_bytesperline[0] = 0;
	ctx->crop_param[ctx->cur_index].dst_bytesperline[1] = 0;
	ctx->crop_param[ctx->cur_index].dst_bytesperline[2] = 0;
#if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)
	ctx->hdr10p_ctx[ctx->cur_index].param.en = 0;
	ctx->hdr10p_ctx[ctx->cur_index].param.config = 0;
	ctx->hdr10p_ctx[ctx->cur_index].param.stat_buf_fd = 0;
	ctx->hdr10p_ctx[ctx->cur_index].param.stat_buf_size = 0;
	ctx->hdr10p_ctx[ctx->cur_index].param.buf_index = 0;
	ctx->hdr10p_ctx[ctx->cur_index].dmabuf = 0;
	ctx->hdr10p_ctx[ctx->cur_index].attachment = 0;
	ctx->hdr10p_ctx[ctx->cur_index].sgt = 0;
	ctx->hdr10p_ctx[ctx->cur_index].hdr10p_buf = 0;
	ctx->hdr10p_ctx[ctx->cur_index].hdr10p_buf_size = 0;
	ctx->hdr10p_ctx[ctx->cur_index].flags = 0;
	gdc->has_hdr10p = 0;
#endif
	gdc->current_ctx = ctx;


	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			ctx->crop_param[ctx->cur_index].calculated_grid_x[i][j] = 0xAA;
			ctx->crop_param[ctx->cur_index].calculated_grid_y[i][j] = 0xBB;
		}
	}

	/* normal operation */
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_scale_parameters(test);
	camerapp_hw_gdc_check_grid_parameters(test);
	camerapp_hw_gdc_check_compressor(test);
	camerapp_hw_gdc_check_format(test);
	camerapp_hw_gdc_check_dma_addr(test);
	camerapp_hw_gdc_check_dma_size(test, ctx->d_frame.width, ctx->d_frame.height,
				       ctx->s_frame.width, ctx->s_frame.height, ctx->s_frame.width,
				       ctx->s_frame.height / 2, ALIGN(ctx->s_frame.width, 16),
				       ALIGN(ctx->s_frame.width, 16), 0, 0,
				       ALIGN(ctx->crop_param[ctx->cur_index].crop_width, 16),
				       ALIGN(ctx->crop_param[ctx->cur_index].crop_width, 16), 0, 0);
	camerapp_hw_gdc_check_core_param(test);
	camerapp_hw_gdc_check_out_select(test);

	/* scale up 2x, use_grid, lossless */
	ctx->s_frame.width = 160;
	ctx->s_frame.height = 120;
	ctx->s_frame.extra = COMP;
	ctx->d_frame.extra = COMP;
	ctx->crop_param[ctx->cur_index].use_calculated_grid = 1;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_scale_parameters(test);
	camerapp_hw_gdc_check_grid_parameters(test);
	camerapp_hw_gdc_check_compressor(test);

	/* bypass & grid_mode, lossy, votf */
	ctx->crop_param[ctx->cur_index].is_grid_mode = 1;
	ctx->crop_param[ctx->cur_index].is_bypass_mode = 1;
	ctx->crop_param[ctx->cur_index].out_mode = GDC_OUT_VOTF;
	ctx->s_frame.extra = COMP_LOSS;
	ctx->d_frame.extra = COMP_LOSS;
	ctx->s_frame.gdc_fmt = &gdc_formats[22];
	ctx->s_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->s_frame.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_8B;
	ctx->d_frame.gdc_fmt = &gdc_formats[22];
	ctx->d_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->d_frame.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_8B;
	ctx->d_frame.io_mode = GDC_OUT_VOTF;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_scale_parameters(test);
	camerapp_hw_gdc_check_compressor(test);
	camerapp_hw_gdc_check_dma_size(
		test, ctx->d_frame.width, ctx->d_frame.height, ctx->s_frame.width,
		ctx->s_frame.height, ctx->s_frame.width, ctx->s_frame.height / 2,
		SBWCL_64_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_64_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_HEADER_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_HEADER_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_64_STRIDE(ctx->s_frame.width), SBWCL_64_STRIDE(ctx->s_frame.width),
		SBWCL_HEADER_STRIDE(ctx->s_frame.width), SBWCL_HEADER_STRIDE(ctx->s_frame.width));
	camerapp_hw_gdc_check_votf_param(test);

	/* otf */
	ctx->crop_param[ctx->cur_index].out_mode = GDC_OUT_OTF;
	ctx->crop_param[ctx->cur_index].codec_type = GDC_CODEC_TYPE_HEVC;
	ctx->d_frame.io_mode = GDC_OUT_OTF;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_out_select(test);

	ctx->crop_param[ctx->cur_index].codec_type = GDC_CODEC_TYPE_NONE;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_out_select(test);

#if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)
	/* hdr10p */
	/* check node without hdr10p stat engine (eg. GDC_M) */
	gdc->has_hdr10p = 0;
	/* The register values should not be changed. */
	ctx->hdr10p_ctx[ctx->cur_index].param.en = 1;
	ctx->hdr10p_ctx[ctx->cur_index].param.config = 1;
	ctx->hdr10p_ctx[ctx->cur_index].hdr10p_buf = 0xbabeface;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_hdr10p_enable(test, 0);
	camerapp_hw_gdc_check_hdr10p_config(test, 0);
	camerapp_hw_gdc_check_hdr10p_wdma_addr(test, 0);
	camerapp_hw_gdc_check_hdr10p_wdma_enable(test, 0);

	/* check node with hdr10p stat engine (eg. GDC_O) */
	gdc->has_hdr10p = 1;

	/* The register values should be changed when enable is 1 */
	ctx->hdr10p_ctx[ctx->cur_index].param.en = 1;
	ctx->hdr10p_ctx[ctx->cur_index].param.config = 1;
	ctx->hdr10p_ctx[ctx->cur_index].hdr10p_buf = 0xbabeface;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_hdr10p_enable(test, 1);
	camerapp_hw_gdc_check_hdr10p_config(test, 1);
	camerapp_hw_gdc_check_hdr10p_wdma_addr(test, 0xbabeface);
	camerapp_hw_gdc_check_hdr10p_wdma_enable(test, 1);

	/* The config register should be maintained when enable is 0 except enable flag */
	ctx->hdr10p_ctx[ctx->cur_index].param.en = 0;
	ctx->hdr10p_ctx[ctx->cur_index].param.config = 0;
	ctx->hdr10p_ctx[ctx->cur_index].hdr10p_buf = 0;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_hdr10p_enable(test, 0);
	camerapp_hw_gdc_check_hdr10p_config(test, 1);
	camerapp_hw_gdc_check_hdr10p_wdma_addr(test, 0xbabeface);
	camerapp_hw_gdc_check_hdr10p_wdma_enable(test, 0);
#endif // #if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)

	kunit_kfree(test, ctx);
}

static void camerapp_hw_gdc_init_pmio_config_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	camerapp_hw_gdc_init_pmio_config(gdc);
}

static void camerapp_hw_gdc_g_reg_cnt_kunit_test(struct kunit *test)
{
	u32 reg_cnt;

	reg_cnt = camerapp_hw_gdc_g_reg_cnt();
	KUNIT_EXPECT_EQ(test, reg_cnt, GDC_REG_CNT);
}

#if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)

#define FIELD_BIT_MASK(field) \
	((1<<(gdc_field_descs[field].msb - gdc_field_descs[field].lsb + 1)) - 1)

static void camerapp_hw_gdc_check_hdr10p_init(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 i;
	u32 addr;

	const int yuv2rgb_matrix_offset_ref[3] = { 64, 0, 0 };

	const int yuv2rgb_matrix_coeff_ref[3][3] = { { 256, 0, 359 },
						     { 256, -41, -143 },
						     { 256, 471, 0 } };

	const int eotf_table_x_ref[65] = { 0,	 128,  256,  320,  384, 448, 512, 544, 576,  608,
					   640,	 656,  672,  688,  704, 720, 736, 744, 752,  760,
					   768,	 776,  784,  792,  800, 808, 816, 824, 832,  840,
					   848,	 856,  864,  872,  880, 888, 896, 904, 912,  920,
					   928,	 932,  936,  940,  944, 948, 952, 956, 960,  964,
					   968,	 972,  976,  980,  984, 988, 992, 996, 1000, 1004,
					   1008, 1012, 1016, 1020, 4 };

	const int eotf_table_y_ref[65] = {
		0,	   15942,     138356,	 315170,    659470,    1303424,	  2476202,
		3375662,   4575481,   6172341,	 8294261,   9603216,   11111139,  12848153,
		14849055,  17154072,  19809755,	 21285624,  22870015,  24571018,  26397348,
		28358395,  30464279,  32725910,	 35155053,  37764399,  40567642,  43579559,
		46816107,  50294518,  54033406,	 58052888,  62374710,  67022388,  72021358,
		77399146,  83185550,  89412833,	 96115952,  103332784, 111104394, 115212015,
		119475320, 123900469, 128493881, 133262244, 138212527, 143351992, 148688209,
		154229068, 159982795, 165957966, 172163524, 178608793, 185303500, 192257791,
		199482248, 206987913, 214786307, 222889454, 231309901, 240060747, 249155666,
		258608933, 9826523
	};

	const int gamut_conv_matrix_ref[3][3] = { { 1376, -289, -63 },
						  { -67, 1102, -11 },
						  { 3, -20, 1041 } };

	const int oetf_table_x_ref[65] = {
		0,	   512,	      1024,	 2048,	    4096,      6144,	  8192,
		12288,	   16384,     24576,	 32768,	    49152,     65536,	  81920,
		98304,	   131072,    163840,	 196608,    262144,    327680,	  393216,
		458752,	   524288,    655360,	 786432,    917504,    1048576,	  1310720,
		1572864,   1835008,   2097152,	 2359296,   2621440,   3145728,	  3670016,
		4194304,   5242880,   6291456,	 7340032,   8388608,   9437184,	  10485760,
		12582912,  14680064,  16777216,	 18874368,  20971520,  25165824,  29360128,
		33554432,  37748736,  41943040,	 50331648,  58720256,  67108864,  75497472,
		83886080,  100663296, 117440512, 134217728, 150994944, 167772160, 201326592,
		234881024, 33554432
	};

	const int oetf_table_y_ref[65] = { 0,	30,  42,  57,  76,  89,	 100, 116, 129,	 149, 164,
					   187, 205, 220, 232, 252, 268, 282, 305, 323,	 339, 352,
					   363, 383, 400, 414, 427, 449, 466, 482, 495,	 507, 518,
					   537, 553, 567, 590, 610, 627, 641, 654, 666,	 686, 703,
					   718, 731, 742, 763, 780, 795, 808, 820, 840,	 857, 872,
					   885, 897, 917, 934, 949, 962, 973, 993, 1010, 14 };

	// HDR10+Stat YUV2RGB Y-Offset / U-Offset / V-Offset
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_OFFSET_0, GDC_F_YUV_HDRYUVTORGB_OFFSET_0_0),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_OFFSET_0_0) & yuv2rgb_matrix_offset_ref[0]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_OFFSET_1, GDC_F_YUV_HDRYUVTORGB_OFFSET_0_1),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_OFFSET_0_1) & yuv2rgb_matrix_offset_ref[1]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_OFFSET_1, GDC_F_YUV_HDRYUVTORGB_OFFSET_0_2),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_OFFSET_0_2) & yuv2rgb_matrix_offset_ref[2]);

	// HDR10+Stat YUV2RGB Color Conversion Matrix
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_0, GDC_F_YUV_HDRYUVTORGB_COEFF_0_0),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_0_0) & yuv2rgb_matrix_coeff_ref[0][0]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_0, GDC_F_YUV_HDRYUVTORGB_COEFF_0_1),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_0_1) & yuv2rgb_matrix_coeff_ref[0][1]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_1, GDC_F_YUV_HDRYUVTORGB_COEFF_0_2),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_0_2) & yuv2rgb_matrix_coeff_ref[0][2]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_1, GDC_F_YUV_HDRYUVTORGB_COEFF_1_0),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_1_0) & yuv2rgb_matrix_coeff_ref[1][0]);

	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_2, GDC_F_YUV_HDRYUVTORGB_COEFF_1_1),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_1_1) & yuv2rgb_matrix_coeff_ref[1][1]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_2, GDC_F_YUV_HDRYUVTORGB_COEFF_1_2),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_1_2) & yuv2rgb_matrix_coeff_ref[1][2]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_3, GDC_F_YUV_HDRYUVTORGB_COEFF_2_0),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_2_0) & yuv2rgb_matrix_coeff_ref[2][0]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_3, GDC_F_YUV_HDRYUVTORGB_COEFF_2_1),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_2_1) & yuv2rgb_matrix_coeff_ref[2][1]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_YUV_HDRYUVTORGB_COEFF_4, GDC_F_YUV_HDRYUVTORGB_COEFF_2_2),
			FIELD_BIT_MASK(GDC_F_YUV_HDRYUVTORGB_COEFF_2_2) & yuv2rgb_matrix_coeff_ref[2][2]);

	// Electro-Optical Transfer Function (EOTF) Table X
	for (i = 0; i < 65; i++) {
		addr = GDC_R_RGB_HDREOTF_X_PNTS_TBL_0 + 4 * i;
		KUNIT_EXPECT_EQ(test, GDC_GET_R(gdc, addr), eotf_table_x_ref[i]);
	}

	// Electro-Optical Transfer Function (EOTF) Table Y
	for (i = 0; i < 65; i++) {
		addr = GDC_R_RGB_HDREOTF_Y_PNTS_TBL_0 + 4 * i;
		KUNIT_EXPECT_EQ(test, GDC_GET_R(gdc, addr), eotf_table_y_ref[i]);
	}

	// Color Gamut Conversion Matrix
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_0, GDC_F_RGB_HDRGAMUTCONV_MATRIX_0_0),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_0_0) & gamut_conv_matrix_ref[0][0]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_0, GDC_F_RGB_HDRGAMUTCONV_MATRIX_0_1),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_0_1) & gamut_conv_matrix_ref[0][1]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_1, GDC_F_RGB_HDRGAMUTCONV_MATRIX_0_2),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_0_2) & gamut_conv_matrix_ref[0][2]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_1, GDC_F_RGB_HDRGAMUTCONV_MATRIX_1_0),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_1_0) & gamut_conv_matrix_ref[1][0]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_2, GDC_F_RGB_HDRGAMUTCONV_MATRIX_1_1),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_1_1) & gamut_conv_matrix_ref[1][1]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_2, GDC_F_RGB_HDRGAMUTCONV_MATRIX_1_2),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_1_2) & gamut_conv_matrix_ref[1][2]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_3, GDC_F_RGB_HDRGAMUTCONV_MATRIX_2_0),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_2_0) & gamut_conv_matrix_ref[2][0]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_3, GDC_F_RGB_HDRGAMUTCONV_MATRIX_2_1),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_2_1) & gamut_conv_matrix_ref[2][1]);
	KUNIT_EXPECT_EQ(test,
			GDC_GET_F(gdc, GDC_R_RGB_HDRGAMUTCONV_MATRIX_4, GDC_F_RGB_HDRGAMUTCONV_MATRIX_2_2),
			FIELD_BIT_MASK(GDC_F_RGB_HDRGAMUTCONV_MATRIX_2_2) & gamut_conv_matrix_ref[2][2]);

	// Optical-Electro Transfer Function (OETF) Table X
	for (i = 0; i < 65; i++) {
		addr = GDC_R_RGB_HDROETF_X_PNTS_TBL_0 + 4 * i;
		KUNIT_EXPECT_EQ(test, GDC_GET_R(gdc, addr), oetf_table_x_ref[i]);
	}

	// Optical-Electro Transfer Function (OETF) Table Y
	for (i = 0; i < 65; i++) {
		addr = GDC_R_RGB_HDROETF_Y_PNTS_TBL_0 + 4 * i;
		KUNIT_EXPECT_EQ(test, GDC_GET_R(gdc, addr), oetf_table_y_ref[i]);
	}
}

static void camerapp_hw_gdc_hdr10p_init_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	camerapp_hw_gdc_hdr10p_init(gdc->pmio);
	camerapp_hw_gdc_check_hdr10p_init(test);
}
#endif // #if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)

static void __gdc_init_pmio(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	int ret;
	u32 reg_cnt;

	gdc->regs_base = tctx->addr;

	camerapp_hw_gdc_init_pmio_config(gdc);
	gdc->pmio_config.cache_type = PMIO_CACHE_NONE; /* override for kunit test */

	gdc->pmio = pmio_init(NULL, NULL, &gdc->pmio_config);

	ret = pmio_field_bulk_alloc(gdc->pmio, &gdc->pmio_fields, gdc->pmio_config.fields,
				    gdc->pmio_config.num_fields);
	if (ret)
		return;

	reg_cnt = camerapp_hw_gdc_g_reg_cnt();
}

static void __gdc_exit_pmio(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	if (gdc->pmio) {
		if (gdc->pmio_fields)
			pmio_field_bulk_free(gdc->pmio, gdc->pmio_fields);

		pmio_exit(gdc->pmio);
	}
}

static struct kunit_case camerapp_hw_api_gdc_kunit_test_cases[] = {
	KUNIT_CASE(camerapp_hw_api_gdc_get_size_constraints_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_sfr_dump_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_start_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_stop_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_sw_reset_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_get_intr_status_and_clear_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_get_fs_fe_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_get_sbwc_const_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_has_comp_header_kunit_test),
	KUNIT_CASE(camerapp_hw_get_comp_buf_size_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_set_initialization_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_update_param_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_init_pmio_config_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_g_reg_cnt_kunit_test),
#if IS_ENABLED(CONFIG_CAMERA_PP_GDC_HAS_HDR10P)
	KUNIT_CASE(camerapp_hw_gdc_hdr10p_init_kunit_test),
#endif
	{},
};

static int camerapp_hw_api_gdc_kunit_test_init(struct kunit *test)
{
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);

	test_ctx.gdc = kunit_kzalloc(test, sizeof(struct gdc_dev), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.gdc);

	test_ctx.crop_param = kunit_kzalloc(test, sizeof(struct gdc_crop_param), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.crop_param);

	test_ctx.gdc->regs_base = test_ctx.addr;
	test_ctx.gdc->regs_rsc = &test_ctx.rsc;

	test->priv = &test_ctx;

	__gdc_init_pmio(test);

	return 0;
}

static void camerapp_hw_api_gdc_kunit_test_exit(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;

	__gdc_exit_pmio(test);

	kunit_kfree(test, tctx->crop_param);
	kunit_kfree(test, tctx->addr);
	kunit_kfree(test, tctx->gdc);
}

struct kunit_suite camerapp_hw_api_gdc_kunit_test_suite = {
	.name = "pablo-hw-api-gdc-v1300-kunit-test",
	.init = camerapp_hw_api_gdc_kunit_test_init,
	.exit = camerapp_hw_api_gdc_kunit_test_exit,
	.test_cases = camerapp_hw_api_gdc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&camerapp_hw_api_gdc_kunit_test_suite);

MODULE_LICENSE("GPL");
