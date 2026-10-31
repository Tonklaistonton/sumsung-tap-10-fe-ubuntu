// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-devicemgr.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "is-hw.h"

static void mlsc_ctl_cfg(struct is_device_ischain *device, struct is_group *group,
	enum is_param pindex, IS_DECLARE_PMAP(pmap))
{
	struct param_control *control = is_itf_g_param(device, NULL, pindex);

	if (test_bit(IS_GROUP_START, &group->state)) {
		control->cmd = CONTROL_COMMAND_START;
		control->bypass = CONTROL_BYPASS_DISABLE;
	} else {
		control->cmd = CONTROL_COMMAND_STOP;
		control->bypass = CONTROL_BYPASS_DISABLE;
	}

	set_bit(pindex, pmap);
}

static int mlsc_otf_in_cfg(struct is_device_ischain *device, struct is_group *group,
	struct is_frame *ldr_frame, struct camera2_node *node, enum is_param pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct param_otf_input *otf_in;
	struct is_crop *incrop;

	FIMC_BUG(!node);

	otf_in = is_itf_g_param(device, ldr_frame, pindex);

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state) ||
		test_bit(IS_GROUP_VOTF_INPUT, &group->state))
		otf_in->cmd = OTF_INPUT_COMMAND_DISABLE;
	else
		otf_in->cmd = OTF_INPUT_COMMAND_ENABLE;

	set_bit(pindex, pmap);

	if (!otf_in->cmd)
		return 0;

	incrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d] incrop is NULL", device, node->vid, ldr_frame->fcount);
		return -EINVAL;
	}

	otf_in->width = incrop->w;
	otf_in->height = incrop->h;
	otf_in->offset_x = 0;
	otf_in->offset_y = 0;
	otf_in->physical_width = otf_in->width;
	otf_in->physical_height = otf_in->height;
	otf_in->format = OTF_INPUT_FORMAT_YUV422;
	otf_in->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	otf_in->bayer_crop_offset_x = incrop->x;
	otf_in->bayer_crop_offset_y = incrop->y;
	otf_in->bayer_crop_width = incrop->w;
	otf_in->bayer_crop_height = incrop->h;
	otf_in->bitwidth = OTF_INPUT_BIT_WIDTH_12BIT;

	return 0;
}

static int mlsc_dma_in_cfg(struct is_device_ischain *device, struct is_group *group,
	struct is_subdev *leader, struct is_frame *ldr_frame, struct camera2_node *node,
	enum is_param pindex, IS_DECLARE_PMAP(pmap))
{
	struct param_dma_input *dma_in;
	struct is_crop *incrop;
	struct is_fmt *fmt;
	u32 hw_bitwidth;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 flag_extra;

	dma_in = is_itf_g_param(device, ldr_frame, pindex);

	if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
		dma_in->cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_in->v_otf_enable = DMA_INPUT_VOTF_ENABLE;
	} else if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		dma_in->cmd = DMA_INPUT_COMMAND_DISABLE;
		dma_in->v_otf_enable = DMA_INPUT_VOTF_DISABLE;
	} else {
		if (dma_in->cmd != node->request)
			mlvinfo("[F%d] RDMA enable: %d -> %d\n", device, node->vid,
				ldr_frame ? ldr_frame->fcount : -1, dma_in->cmd, node->request);

		dma_in->cmd = node->request;
		dma_in->v_otf_enable = DMA_INPUT_VOTF_DISABLE;
	}

	set_bit(pindex, pmap);

	if (!dma_in->cmd)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d] incrop is NULL", device, node->vid,
			ldr_frame ? ldr_frame->fcount : -1);
		return -EINVAL;
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("[F%d] pixelformat(%c%c%c%c) is not found", device, node->vid,
			ldr_frame ? ldr_frame->fcount : -1, (char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}

	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	hw_bitwidth = fmt->hw_bitwidth;

	if (dma_in->format != fmt->hw_format)
		mlvinfo("[F%d] RDMA format: %d -> %d\n", device, node->vid,
			ldr_frame ? ldr_frame->fcount : -1, dma_in->format, fmt->hw_format);
	if (dma_in->bitwidth != hw_bitwidth)
		mlvinfo("[F%d] RDMA bitwidth: %d -> %d\n", device, node->vid,
			ldr_frame ? ldr_frame->fcount : -1, dma_in->bitwidth, hw_bitwidth);
	if (dma_in->sbwc_type != hw_sbwc)
		mlvinfo("[F%d] RDMA sbwc_type: %d -> %d\n", device, node->vid,
			ldr_frame ? ldr_frame->fcount : -1, dma_in->sbwc_type, hw_sbwc);
	if ((dma_in->dma_crop_width != incrop->w) || (dma_in->dma_crop_height != incrop->h))
		mlvinfo("[F%d] RDMA bcrop[%d, %d, %d, %d]\n", device, node->vid,
			ldr_frame ? ldr_frame->fcount : -1, incrop->x, incrop->y, incrop->w,
			incrop->h);

	dma_in->format = fmt->hw_format;
	dma_in->bitwidth = hw_bitwidth;
	dma_in->msb = fmt->bitsperpixel[0] - 1;
	dma_in->sbwc_type = hw_sbwc;
	dma_in->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	dma_in->plane = 2;

	dma_in->width = leader->input.width;
	dma_in->height = leader->input.height;
	dma_in->dma_crop_offset = 0;
	dma_in->dma_crop_width = leader->input.width;
	dma_in->dma_crop_height = leader->input.height;
	dma_in->bayer_crop_offset_x = incrop->x;
	dma_in->bayer_crop_offset_y = incrop->y;
	dma_in->bayer_crop_width = incrop->w;
	dma_in->bayer_crop_height = incrop->h;

	return 0;
}

static int mlsc_dma_out_cfg(struct is_device_ischain *device, struct is_subdev *leader,
	struct is_frame *ldr_frame, struct camera2_node *node, enum is_param pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct param_dma_output *dma_out;
	struct is_crop *incrop, *otcrop;
	struct is_fmt *fmt;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 flag_extra;

	dma_out = is_itf_g_param(device, ldr_frame, pindex);
	if (dma_out->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			dma_out->cmd, node->request);

	dma_out->cmd = node->request;

	set_bit(pindex, pmap);

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d] incrop is NULL", device, node->vid, ldr_frame->fcount);
		incrop->x = incrop->y = 0;
		incrop->w = leader->input.width;
		incrop->h = leader->input.height;
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[F%d] otcrop is NULL", device, node->vid, ldr_frame->fcount);
		otcrop->x = otcrop->y = 0;
		otcrop->w = leader->input.width;
		otcrop->h = leader->input.height;
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		return -EINVAL;
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("[F%d] pixelformat(%c%c%c%c) is not found", device, node->vid,
			ldr_frame->fcount, (char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		return -EINVAL;
	}

	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	mlvdbgs(4, "[F%d] flag %d, flag_extra %d, hw_sbwc %d\n", device, node->vid, ldr_frame->fcount,
			node->flags, flag_extra, hw_sbwc);

	if (dma_out->format != fmt->hw_format)
		mlvinfo("[F%d] WDMA format: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			dma_out->format, fmt->hw_format);
	if (dma_out->bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d] WDMA bitwidth: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			dma_out->bitwidth, fmt->hw_bitwidth);
	if (dma_out->sbwc_type != hw_sbwc)
		mlvinfo("[F%d] WDMA sbwc_type: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			dma_out->sbwc_type, hw_sbwc);
	if ((dma_out->dma_crop_width != incrop->w) || (dma_out->dma_crop_height != incrop->h))
		mlvinfo("[F%d] WDMA incrop[%d, %d, %d, %d]\n", device, node->vid, ldr_frame->fcount,
			incrop->x, incrop->y, incrop->w, incrop->h);
	if ((dma_out->width != otcrop->w) || (dma_out->height != otcrop->h)) {
		/* FDPIG usually changes its size between 320x240 and 640x480 */
		if ((node->vid == IS_LVN_MLSC0_FDPIG) || (node->vid == IS_LVN_MLSC1_FDPIG) ||
			(node->vid == IS_LVN_MLSC2_FDPIG) || (node->vid == IS_LVN_MLSC3_FDPIG)) {
			mlvdbgs(4, "[F%d] WDMA otcrop[%d, %d, %d, %d]\n", device, node->vid,
				ldr_frame->fcount, otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		} else {
			mlvinfo("[F%d] WDMA otcrop[%d, %d, %d, %d]\n", device, node->vid,
				ldr_frame->fcount, otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		}
	}

	dma_out->format = fmt->hw_format;
	dma_out->bitwidth = fmt->hw_bitwidth;
	dma_out->msb = fmt->bitsperpixel[0] - 1;
	dma_out->sbwc_type = hw_sbwc;
	dma_out->order = fmt->hw_order;
	dma_out->plane = fmt->hw_plane;
	dma_out->dma_crop_offset_x = incrop->x;
	dma_out->dma_crop_offset_y = incrop->y;
	dma_out->dma_crop_width = incrop->w;
	dma_out->dma_crop_height = incrop->h;
	dma_out->width = otcrop->w;
	dma_out->height = otcrop->h;
	dma_out->stride_plane0 = node->width;
	dma_out->stride_plane1 = node->width;
	dma_out->stride_plane2 = node->width;
	dma_out->crop_enable = 0;

	if (otcrop->x || otcrop->y)
		mlvwarn("[F%d] Ignore crop pos(%d, %d)", device, node->vid, ldr_frame->fcount,
			otcrop->x, otcrop->y);

	return 0;
}

static int is_ischain_mlsc_is_valid_vid(u32 l_vid, u32 c_vid)
{
	u32 min_vid, max_vid;

	switch (l_vid) {
	case IS_VIDEO_MLSC0:
		min_vid = IS_LVN_MLSC0_SVHIST;
		max_vid = IS_LVN_MLSC0_YUV444;
		break;
	case IS_VIDEO_MLSC1:
		min_vid = IS_LVN_MLSC1_SVHIST;
		max_vid = IS_LVN_MLSC1_YUV444;
		break;
	case IS_VIDEO_MLSC2:
		min_vid = IS_LVN_MLSC2_SVHIST;
		max_vid = IS_LVN_MLSC2_YUV444;
		break;
	case IS_VIDEO_MLSC3:
		min_vid = IS_LVN_MLSC3_SVHIST;
		max_vid = IS_LVN_MLSC3_YUV444;
		break;
	default:
		return 0;
	}

	return (c_vid >= min_vid && c_vid <= max_vid);
}

static int is_ischain_mlsc_g_pindex(u32 vid, u32 *pindex)
{
	switch (vid) {
	case IS_LVN_MLSC0_SVHIST:
	case IS_LVN_MLSC1_SVHIST:
	case IS_LVN_MLSC2_SVHIST:
	case IS_LVN_MLSC3_SVHIST:
		*pindex = PARAM_MLSC_SVHIST;
		return 2;
	case IS_LVN_MLSC0_FDPIG:
	case IS_LVN_MLSC1_FDPIG:
	case IS_LVN_MLSC2_FDPIG:
	case IS_LVN_MLSC3_FDPIG:
		*pindex = PARAM_MLSC_FDPIG;
		return 2;
	case IS_LVN_MLSC0_LMEDS:
	case IS_LVN_MLSC1_LMEDS:
	case IS_LVN_MLSC2_LMEDS:
	case IS_LVN_MLSC3_LMEDS:
		*pindex = PARAM_MLSC_LMEDS;
		return 2;
	case IS_LVN_MLSC0_CAV:
	case IS_LVN_MLSC1_CAV:
	case IS_LVN_MLSC2_CAV:
	case IS_LVN_MLSC3_CAV:
		*pindex = PARAM_MLSC_CAV;
		return 2;
	case IS_LVN_MLSC0_GLPG_L0:
	case IS_LVN_MLSC1_GLPG_L0:
	case IS_LVN_MLSC2_GLPG_L0:
	case IS_LVN_MLSC3_GLPG_L0:
		*pindex = PARAM_MLSC_GLPG0;
		return 2;
	case IS_LVN_MLSC0_GLPG_L1:
	case IS_LVN_MLSC1_GLPG_L1:
	case IS_LVN_MLSC2_GLPG_L1:
	case IS_LVN_MLSC3_GLPG_L1:
		*pindex = PARAM_MLSC_GLPG1;
		return 2;
	case IS_LVN_MLSC0_GLPG_L2:
	case IS_LVN_MLSC1_GLPG_L2:
	case IS_LVN_MLSC2_GLPG_L2:
	case IS_LVN_MLSC3_GLPG_L2:
		*pindex = PARAM_MLSC_GLPG2;
		return 2;
	case IS_LVN_MLSC0_GLPG_L3:
	case IS_LVN_MLSC1_GLPG_L3:
	case IS_LVN_MLSC2_GLPG_L3:
	case IS_LVN_MLSC3_GLPG_L3:
		*pindex = PARAM_MLSC_GLPG3;
		return 2;
	case IS_LVN_MLSC0_GLPG_G4:
	case IS_LVN_MLSC1_GLPG_G4:
	case IS_LVN_MLSC2_GLPG_G4:
	case IS_LVN_MLSC3_GLPG_G4:
		*pindex = PARAM_MLSC_GLPG4;
		return 2;
	case IS_LVN_MLSC0_YUV444:
	case IS_LVN_MLSC1_YUV444:
	case IS_LVN_MLSC2_YUV444:
	case IS_LVN_MLSC3_YUV444:
		*pindex = PARAM_MLSC_YUV444;
		return 2;
	default:
		*pindex = 0;
		return 0;
	};
}

static int is_ischain_mlsc_tag(struct is_subdev *subdev, void *device_data, struct is_frame *frame,
	struct camera2_node *l_node)
{
	int ret;
	struct is_device_ischain *device = (struct is_device_ischain *)device_data;
	struct is_group *group;
	struct camera2_node *node;
	enum is_param pindex;
	struct is_sub_frame *sframe;
	dma_addr_t *dva;
	u32 cap_i;
	IS_DECLARE_PMAP(pmap);

	group = container_of(subdev, struct is_group, leader);
	IS_INIT_PMAP(pmap);

	mdbgs_ischain(4, "MLSC TAG\n", device);

	/* control cfg */
	mlsc_ctl_cfg(device, group, PARAM_MLSC_CONTROL, pmap);

	/* OTF in cfg */
	node = &frame->shot_ext->node_group.leader;
	ret = mlsc_otf_in_cfg(device, group, frame, node, PARAM_MLSC_OTF_INPUT, pmap);
	if (ret) {
		mlverr("[F%d] dma_in_cfg fail. ret %d", device, node->vid, frame->fcount, ret);
		return ret;
	}

	/* DMA in cfg */
	ret = mlsc_dma_in_cfg(device, group, subdev, frame, node, PARAM_MLSC_DMA_INPUT, pmap);
	if (ret) {
		mlverr("[F%d] dma_in_cfg fail. ret %d", device, node->vid, frame->fcount, ret);
		return ret;
	}

	node->result = 1;

	/* DMA out cfg */
	for (cap_i = 0; cap_i < CAPTURE_NODE_MAX; cap_i++) {
		node = &frame->shot_ext->node_group.capture[cap_i];
		if (!is_ischain_mlsc_is_valid_vid(subdev->vid, node->vid) ||
			!is_ischain_mlsc_g_pindex(node->vid, &pindex))
			continue;

		if (mlsc_dma_out_cfg(device, subdev, frame, node, pindex, pmap))
			continue;

		if (!node->request)
			continue;

		node->result = 1;

		sframe = &frame->cap_node.sframe[cap_i];
		ret = is_hw_get_capture_slot(frame, &dva, NULL, sframe->id);
		if (ret) {
			mrerr("Invalid capture slot(%d)", group, frame, sframe->id);
			continue;
		}

		if (dva)
			memcpy(dva, sframe->dva, sizeof(dma_addr_t) * sframe->num_planes);
	}

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param fail. ret %d", device, frame, ret);
		return ret;
	}

	return 0;
}

static const struct is_subdev_ops is_subdev_mlsc_ops = {
	.bypass = NULL,
	.cfg = NULL,
	.tag = is_ischain_mlsc_tag,
};

const struct is_subdev_ops *pablo_get_is_subdev_mlsc_ops(void)
{
	return &is_subdev_mlsc_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_mlsc_ops);
