// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/videodev2_exynos_media.h>
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "pablo-hw-msnr.h"
#include "is-stripe.h"

static int __ischain_msnr_slot(
	struct is_device_ischain *device, struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_LVN_MSNR_CAPTURE_LMEDS:
		*pindex = PARAM_MSNR_WDMA_LME;
		ret = 2;
		break;
	case IS_LVN_MSNR_CR:
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static inline int __check_cropRegion(struct is_device_ischain *device, struct camera2_node *node)
{
	if (IS_NULL_CROP((struct is_crop *)node->input.cropRegion)) {
		mlverr("incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	if (IS_NULL_CROP((struct is_crop *)node->output.cropRegion)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	return 0;
}

static int __msnr_dma_out_cfg(struct is_device_ischain *device, struct is_subdev *leader,
	struct is_frame *frame, struct camera2_node *node, u32 pindex, IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_output *dma_output;
	struct is_crop *incrop, *otcrop;
	int ret = 0;

	FIMC_BUG(!frame);
	FIMC_BUG(!node);

	dma_output = is_itf_g_param(device, frame, pindex);

	if (dma_output->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device, node->vid, frame->fcount,
			dma_output->cmd, node->request);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	set_bit(pindex, pmap);

	if (!node->request)
		return 0;

	ret = __check_cropRegion(device, node);
	if (ret)
		return ret;

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	mdbgs_ischain(
		4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, otcrop->w, otcrop->h);

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	if (dma_output->format != fmt->hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device, node->vid, frame->fcount,
			dma_output->format, fmt->hw_format);
	if (dma_output->bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device, node->vid, frame->fcount,
			dma_output->bitwidth, fmt->hw_bitwidth);
	if (!frame->stripe_info.region_num &&
		((dma_output->width != otcrop->w) || (dma_output->height != otcrop->h)))
		mlvinfo("[F%d][%d] WDMA otcrop[%d, %d, %d, %d]\n", device, node->vid, frame->fcount,
			pindex, otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->bitwidth = fmt->hw_bitwidth;
	dma_output->msb = fmt->hw_bitwidth - 1;
	dma_output->format = fmt->hw_format;

	dma_output->dma_crop_offset_x = incrop->x;
	dma_output->dma_crop_offset_y = incrop->y;
	dma_output->dma_crop_width = incrop->w;
	dma_output->dma_crop_height = incrop->h;

	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;

	dma_output->stride_plane0 =
		max(node->bytesperline[0], (otcrop->w * fmt->bitsperpixel[0]) / BITS_PER_BYTE);
	dma_output->stride_plane1 =
		max(node->bytesperline[1], (otcrop->w * fmt->bitsperpixel[1]) / BITS_PER_BYTE);

	return ret;
}

static inline u32 __get_mtnr_param_index(u32 p)
{
	u32 ret;

	switch (p) {
	case PARAM_MSNR_CIN_L0:
		ret = PARAM_MTNR_COUT_MSNR_L0;
		break;
	case PARAM_MSNR_CIN_L1:
		ret = PARAM_MTNR_COUT_MSNR_L1;
		break;
	case PARAM_MSNR_CIN_L2:
		ret = PARAM_MTNR_COUT_MSNR_L2;
		break;
	case PARAM_MSNR_CIN_L3:
		ret = PARAM_MTNR_COUT_MSNR_L3;
		break;
	case PARAM_MSNR_CIN_L4:
		ret = PARAM_MTNR_COUT_MSNR_L4;
		break;
	default:
		ret = IS_PARAM_NUM;
		break;
	}

	return ret;
}

static int __msnr_cin_cfg(struct is_device_ischain *device, struct is_group *group,
	struct is_frame *ldr_frame, struct is_stripe_info *stripe_info, IS_DECLARE_PMAP(pmap),
	u32 pindex)
{
	struct param_otf_input *otf_in;
	struct param_otf_output *mtnr_out;
	u32 mtnr_pindex = __get_mtnr_param_index(pindex);
	u32 level = 0;

	if (mtnr_pindex == IS_PARAM_NUM) {
		mlverr("[F%d] invalid param index: %d\n", device, IS_VIDEO_MSNR, ldr_frame->fcount,
			pindex);
		return -EINVAL;
	}

	mtnr_out = is_itf_g_param(device, NULL, mtnr_pindex);
	otf_in = is_itf_g_param(device, ldr_frame, pindex);
	otf_in->cmd = OTF_INPUT_COMMAND_ENABLE;

	set_bit(pindex, pmap);

	switch (pindex) {
	case PARAM_MSNR_CIN_L1:
	case PARAM_MSNR_CIN_L2:
	case PARAM_MSNR_CIN_L3:
	case PARAM_MSNR_CIN_L4:
		level = pindex - PARAM_MSNR_CIN_L1 + 1;
		break;
	default:
		break;
	}

	if (!ldr_frame->stripe_info.region_num &&
		((otf_in->width != mtnr_out->width) || (otf_in->height != mtnr_out->height)))
		mlvinfo("[F%d] OTF input L%d [%d, %d]\n", device, IS_VIDEO_MSNR, ldr_frame->fcount,
			pindex - PARAM_MSNR_CIN_L0, mtnr_out->width, mtnr_out->height);

	/* TODO: strip crop_width change for L1 ~ L4 */
	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		otf_in->width = __get_glpg_size(ldr_frame->stripe_info.out.crop_width, level);
	else
		otf_in->width = mtnr_out->width;
	otf_in->height = mtnr_out->height;
	otf_in->offset_x = 0;
	otf_in->offset_y = 0;
	otf_in->physical_width = otf_in->width;
	otf_in->physical_height = otf_in->height;
	otf_in->format =
		(pindex == PARAM_MSNR_CIN_L0) ? OTF_INPUT_FORMAT_Y : OTF_INPUT_FORMAT_YUV444;
	otf_in->bayer_crop_offset_x = 0;
	otf_in->bayer_crop_offset_y = 0;
	otf_in->bayer_crop_width = otf_in->width;
	otf_in->bayer_crop_height = otf_in->height;

	mdbgs_ischain(
		4, "%s : otf_in (%d x %d)\n", device, __func__, otf_in->width, otf_in->height);

	return 0;
}

static int __msnr_cout_cfg(
	struct is_device_ischain *device, struct is_frame *ldr_frame, IS_DECLARE_PMAP(pmap))
{
	struct param_otf_input *otf_in;
	struct param_otf_output *otf_out;

	/* TODO: consider DLNR */
	otf_in = is_itf_g_param(device, NULL, PARAM_MSNR_CIN_L0);
	otf_out = is_itf_g_param(device, ldr_frame, PARAM_MSNR_COUT_YUV);
	otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_out->width = otf_in->width;
	otf_out->height = otf_in->height;
	set_bit(PARAM_MSNR_COUT_YUV, pmap);

	mdbgs_ischain(
		4, "%s : otf_out (%d x %d)\n", device, __func__, otf_out->width, otf_out->height);

	return 0;
}

static int __msnr_stripe_in_cfg(struct is_device_ischain *device, struct is_subdev *leader,
	struct is_frame *frame, struct camera2_node *node, u32 pindex, IS_DECLARE_PMAP(pmap))
{
	int ret;
	struct param_stripe_input *stripe_input;
	struct is_crop *otcrop;
	int i;

	set_bit(pindex, pmap);

	ret = __check_cropRegion(device, node);
	if (ret)
		return ret;

	otcrop = (struct is_crop *)node->output.cropRegion;

	stripe_input = is_itf_g_param(device, frame, pindex);
	if (frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		if (!frame->stripe_info.region_id) {
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] =
					frame->stripe_info.h_pix_num[i - 1];
		}

		stripe_input->left_margin = frame->stripe_info.out.left_margin;
		stripe_input->right_margin = frame->stripe_info.out.right_margin;
		stripe_input->full_width = otcrop->w;
		stripe_input->full_height = otcrop->h;
		stripe_input->start_pos_x =
			frame->stripe_info.out.crop_x + frame->stripe_info.out.offset_x;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}

	pablo_update_repeat_param(frame, stripe_input);

	return 0;
}

static void __msnr_control_cfg(struct is_device_ischain *device, struct is_group *group,
	struct is_frame *frame, IS_DECLARE_PMAP(pmap))
{
	struct param_control *control;

	control = is_itf_g_param(device, frame, PARAM_MSNR_CONTROL);
	if (test_bit(IS_GROUP_STRGEN_INPUT, &group->state))
		control->strgen = CONTROL_COMMAND_START;
	else
		control->strgen = CONTROL_COMMAND_STOP;

	set_bit(PARAM_MSNR_CONTROL, pmap);
}

static int is_ischain_msnr_tag(struct is_subdev *subdev, void *device_data, struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	IS_DECLARE_PMAP(pmap);
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	u32 n, pindex = 0;
	int dma_type;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = container_of(subdev, struct is_group, leader);
	leader = subdev->leader;
	IS_INIT_PMAP(pmap);

	__msnr_control_cfg(device, group, frame, pmap);

	out_node = &frame->shot_ext->node_group.leader;

	for (n = PARAM_MSNR_CIN_L0; n <= PARAM_MSNR_CIN_L4; ++n) {
		ret = __msnr_cin_cfg(device, group, frame, &frame->stripe_info, pmap, n);
		if (ret) {
			mlverr("[F%d] otf_in_cfg L%d fail. ret %d", device, node->vid,
				frame->fcount, n - PARAM_MSNR_CIN_L0, ret);
			return ret;
		}
	}

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	ret = __msnr_cout_cfg(device, frame, pmap);
	if (ret) {
		mlverr("[F%d] otf_out_cfg fail. ret %d", device, node->vid, frame->fcount, ret);
		return ret;
	}

	ret = __msnr_stripe_in_cfg(device, subdev, frame, out_node, PARAM_MSNR_STRIPE_INPUT, pmap);
	if (ret) {
		mlverr("[F%d] strip_in_cfg fail. ret %d", device, node->vid, frame->fcount, ret);
		return ret;
	}

	out_node->result = 1;

	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		cap_node = &frame->shot_ext->node_group.capture[n];

		if (!cap_node->vid)
			continue;

		dma_type = __ischain_msnr_slot(device, cap_node, &pindex);
		if (dma_type == 2)
			ret = __msnr_dma_out_cfg(device, subdev, frame, cap_node, pindex, pmap);
		else
			ret = dma_type;
		if (ret) {
			mlverr("[F%d] dma_out_cfg error\n", device, cap_node->vid, frame->fcount);
			return ret;
		}

		cap_node->result = 1;
	}

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	return 0;

p_err:
	return ret;
}

static int is_ischain_msnr_get(struct is_subdev *subdev, struct is_frame *frame,
	enum pablo_subdev_get_type type, void *result)
{
	struct camera2_node *node;
	struct is_crop *incrop, *outcrop;

	switch (type) {
	case PSGT_REGION_NUM:
		node = &frame->shot_ext->node_group.leader;
		incrop = (struct is_crop *)node->input.cropRegion;
		outcrop = (struct is_crop *)node->output.cropRegion;

		*(int *)result = is_calc_region_num(incrop, outcrop, subdev);
		break;
	default:
		break;
	}

	return 0;
}

static const struct is_subdev_ops is_subdev_msnr_ops = {
	.bypass = NULL,
	.cfg = NULL,
	.tag = is_ischain_msnr_tag,
	.get = is_ischain_msnr_get,
};

const struct is_subdev_ops *pablo_get_is_subdev_msnr_ops(void)
{
	return &is_subdev_msnr_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_msnr_ops);
