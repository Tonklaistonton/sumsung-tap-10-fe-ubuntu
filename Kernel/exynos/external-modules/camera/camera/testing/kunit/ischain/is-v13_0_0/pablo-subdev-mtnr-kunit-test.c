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
#include "is-subdev-ctrl.h"
#include "is-device-ischain.h"

const struct is_subdev_ops *pablo_get_is_subdev_mtnr_ops(void);

static struct test_ctx {
	struct is_group group;
	struct is_device_ischain idi;
	struct is_region is_region;
	struct is_frame frame;
	struct camera2_shot_ext	shot_ext;
	struct is_param_region parameter;
	const struct is_subdev_ops *ops;
} test_ctx;

static void __set_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 1920;
	dst[3] = 1080;
}

static void __clear_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 0;
	dst[3] = 0;
}

static void pablo_subdev_mtnr_get_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	enum pablo_subdev_get_type type;
	int result;

	type =  PSGT_REGION_NUM;

	ret = ops->get(subdev, frame, type, &result);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_mtnr_tag_basic_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_mtnr_tag_negative_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	/* crop is NULL */
	__clear_test_vector_cropRegion(node->input.cropRegion);
	__clear_test_vector_cropRegion(node->output.cropRegion);

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_subdev_mtnr_tag_otf_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	/* otf in */
	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_MTNR]->state);
	__set_test_vector_cropRegion(node->input.cropRegion);

	/* otf out */
	set_bit(IS_GROUP_OTF_OUTPUT, &idi->group[GROUP_SLOT_MTNR]->state);
	__set_test_vector_cropRegion(node->output.cropRegion);
	node->pixelformat = V4L2_PIX_FMT_NV21;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_mtnr_tag_otf_negative_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	/* otf in */
	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_MTNR]->state);
	__clear_test_vector_cropRegion(node->input.cropRegion);

	/* incrop is NULL */
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__set_test_vector_cropRegion(node->input.cropRegion);

	/* otf out */
	set_bit(IS_GROUP_OTF_OUTPUT, &idi->group[GROUP_SLOT_MTNR]->state);
	__clear_test_vector_cropRegion(node->output.cropRegion);

	/* otcrip is NULL */
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static enum is_video_dev_num _test_vector[] = {
	/* mtnr0 */
	IS_VIDEO_MTNR,
	IS_LVN_MTNR_OUTPUT_CUR_L4,
	IS_LVN_MTNR_OUTPUT_PREV_L0,
	IS_LVN_MTNR_OUTPUT_PREV_L0_WGT,
	IS_LVN_MTNR_OUTPUT_MV_GEOMATCH,
	IS_LVN_MTNR_OUTPUT_SEG_L0,
	IS_LVN_MTNR_CAPTURE_PREV_L0,
	IS_LVN_MTNR_CAPTURE_PREV_L0_WGT,

	/* mtnr1 */
	IS_LVN_MTNR_OUTPUT_CUR_L1,
	IS_LVN_MTNR_OUTPUT_CUR_L2,
	IS_LVN_MTNR_OUTPUT_CUR_L3,
	IS_LVN_MTNR_OUTPUT_PREV_L1,
	IS_LVN_MTNR_OUTPUT_PREV_L2,
	IS_LVN_MTNR_OUTPUT_PREV_L3,
	IS_LVN_MTNR_OUTPUT_PREV_L4,
	IS_LVN_MTNR_OUTPUT_PREV_L1_WGT,
	IS_LVN_MTNR_OUTPUT_SEG_L1,
	IS_LVN_MTNR_OUTPUT_SEG_L2,
	IS_LVN_MTNR_OUTPUT_SEG_L3,
	IS_LVN_MTNR_OUTPUT_SEG_L4,
	IS_LVN_MTNR_CAPTURE_PREV_L1,
	IS_LVN_MTNR_CAPTURE_PREV_L2,
	IS_LVN_MTNR_CAPTURE_PREV_L3,
	IS_LVN_MTNR_CAPTURE_PREV_L4,
	IS_LVN_MTNR_CAPTURE_PREV_L1_WGT,
};

static void pablo_subdev_mtnr_tag_dma_kunit_test(struct kunit *test)
{
	int ret, k;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;
	struct camera2_node *cap_node;

	for (k = 0; k < sizeof(_test_vector)/sizeof(enum is_video_dev_num); k++) {
		cap_node = &frame->shot_ext->node_group.capture[k];
		cap_node->vid = _test_vector[k];
		__set_test_vector_cropRegion(cap_node->input.cropRegion);
		__set_test_vector_cropRegion(cap_node->output.cropRegion);
		cap_node->pixelformat = V4L2_PIX_FMT_NV21;
	}

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_mtnr_tag_dma_negative_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	/* dma in */
	frame->shot_ext->node_group.capture[0].vid = IS_VIDEO_MTNR;

	/* request is not set */
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	frame->shot_ext->node_group.capture[0].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].output.cropRegion);

	/* pixel format is not found */
	frame->shot_ext->node_group.capture[0].pixelformat = 0;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	frame->shot_ext->node_group.capture[0].pixelformat = V4L2_PIX_FMT_NV21;

	/* dma out */
	frame->shot_ext->node_group.capture[1].vid = IS_LVN_MTNR_OUTPUT_PREV_L0;
	frame->shot_ext->node_group.capture[1].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].output.cropRegion);

	/* pixel format is not found */
	frame->shot_ext->node_group.capture[1].pixelformat = 0;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	frame->shot_ext->node_group.capture[1].pixelformat = V4L2_PIX_FMT_NV21;
}

static void pablo_subdev_mtnr_tag_sframe_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	frame->cap_node.sframe[0].id = IS_LVN_MTNR_OUTPUT_MV_GEOMATCH;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void *__g_param(struct is_device_ischain *device, struct is_frame *frame, u32 index)
{
	ulong dst_base, src_base, dst_param, src_param;

	if (frame) {
		dst_base = (ulong)frame->parameter;
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
		src_base = (ulong)&device->is_region->parameter;
		src_param = (src_base + (index * PARAMETER_MAX_SIZE));

		memcpy((ulong *)dst_param, (ulong *)src_param, PARAMETER_MAX_SIZE);
	} else {
		dst_base = (ulong)&device->is_region->parameter;
		dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));
	}

	return (void *)dst_param;
}

static void pablo_subdev_mtnr_tag_max_yuv_size_kunit_test(struct kunit *test)
{
	int ret, k;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;
	struct camera2_node *cap_node;
	struct { u32 w; u32 h; } size_ori = { subdev->input.width, subdev->input.height };
	struct { u32 w; u32 h; } yuv_max_ori = { idi->yuv_max_width, idi->yuv_max_height };

	struct param_dma_output *dma_output;

	for (k = 0; k < sizeof(_test_vector)/sizeof(enum is_video_dev_num); k++) {
		cap_node = &frame->shot_ext->node_group.capture[k];
		cap_node->vid = _test_vector[k];
		__set_test_vector_cropRegion(cap_node->input.cropRegion);
		__set_test_vector_cropRegion(cap_node->output.cropRegion);
		cap_node->pixelformat = V4L2_PIX_FMT_NV21;
		if (cap_node->vid == IS_LVN_MTNR_CAPTURE_PREV_L0 ||
			cap_node->vid == IS_LVN_MTNR_CAPTURE_PREV_L1 ||
			cap_node->vid == IS_LVN_MTNR_CAPTURE_PREV_L2 ||
			cap_node->vid == IS_LVN_MTNR_CAPTURE_PREV_L3 ||
			cap_node->vid == IS_LVN_MTNR_CAPTURE_PREV_L4)
			cap_node->request = 1;

	}

	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L0);
	KUNIT_EXPECT_EQ(test, dma_output->width, 0);
	KUNIT_EXPECT_EQ(test, dma_output->height, 0);
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L1);
	KUNIT_EXPECT_EQ(test, dma_output->width, 0);
	KUNIT_EXPECT_EQ(test, dma_output->height, 0);
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L2);
	KUNIT_EXPECT_EQ(test, dma_output->width, 0);
	KUNIT_EXPECT_EQ(test, dma_output->height, 0);
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L3);
	KUNIT_EXPECT_EQ(test, dma_output->width, 0);
	KUNIT_EXPECT_EQ(test, dma_output->height, 0);
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L4);
	KUNIT_EXPECT_EQ(test, dma_output->width, 0);
	KUNIT_EXPECT_EQ(test, dma_output->height, 0);

	/* set leader size */
	subdev->input.width = 1920;
	subdev->input.height = 1080;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L0);
	KUNIT_EXPECT_EQ(test, dma_output->width, subdev->input.width);
	KUNIT_EXPECT_EQ(test, dma_output->height, subdev->input.height);
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L1);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN(subdev->input.width / 2, 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN(subdev->input.height / 2, 2));
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L2);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN(subdev->input.width / 4, 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN(subdev->input.height / 4, 2));
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L3);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN(subdev->input.width / 8, 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN(subdev->input.height / 8, 2));
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L4);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN(subdev->input.width / 16, 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN(subdev->input.height / 16, 2));

	subdev->input.width = size_ori.w;
	subdev->input.height = size_ori.h;

	/* set max yuv size */
	idi->yuv_max_width = 320;
	idi->yuv_max_height = 240;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L0);
	KUNIT_EXPECT_EQ(test, dma_output->width, idi->yuv_max_width);
	KUNIT_EXPECT_EQ(test, dma_output->height, idi->yuv_max_height);
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L1);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN((idi->yuv_max_width / 2), 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN((idi->yuv_max_height / 2), 2));
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L2);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN((idi->yuv_max_width / 4), 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN((idi->yuv_max_height / 4), 2));
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L3);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN((idi->yuv_max_width / 8), 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN((idi->yuv_max_height / 8), 2));
	dma_output = __g_param(idi, frame, PARAM_MTNR_WDMA_PREV_L4);
	KUNIT_EXPECT_EQ(test, dma_output->width, ALIGN((idi->yuv_max_width / 16), 2));
	KUNIT_EXPECT_EQ(test, dma_output->height, ALIGN((idi->yuv_max_height / 16), 2));

	idi->yuv_max_width = yuv_max_ori.w;
	idi->yuv_max_height = yuv_max_ori.h;
}

static int pablo_subdev_mtnr_kunit_test_init(struct kunit *test)
{
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	test_ctx.ops = pablo_get_is_subdev_mtnr_ops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	idi->is_region = &test_ctx.is_region;
	idi->group[GROUP_SLOT_MTNR] = &test_ctx.group;
	subdev = &idi->group[GROUP_SLOT_MTNR]->leader;
	subdev->leader = subdev;
	node = &test_ctx.frame.shot_ext->node_group.leader;

	__set_test_vector_cropRegion(node->input.cropRegion);
	__set_test_vector_cropRegion(node->output.cropRegion);

	return 0;
}

static void pablo_subdev_mtnr_kunit_test_exit(struct kunit *test)
{
	struct camera2_node *node;

	node = &test_ctx.frame.shot_ext->node_group.leader;

	__clear_test_vector_cropRegion(node->input.cropRegion);
	__clear_test_vector_cropRegion(node->output.cropRegion);
}

static struct kunit_case pablo_subdev_mtnr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_mtnr_get_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_basic_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_otf_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_otf_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_dma_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_dma_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_sframe_kunit_test),
	KUNIT_CASE(pablo_subdev_mtnr_tag_max_yuv_size_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_mtnr_kunit_test_suite = {
	.name = "pablo-subdev-mtnr-kunit-test",
	.init = pablo_subdev_mtnr_kunit_test_init,
	.exit = pablo_subdev_mtnr_kunit_test_exit,
	.test_cases = pablo_subdev_mtnr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_mtnr_kunit_test_suite);

MODULE_LICENSE("GPL");
