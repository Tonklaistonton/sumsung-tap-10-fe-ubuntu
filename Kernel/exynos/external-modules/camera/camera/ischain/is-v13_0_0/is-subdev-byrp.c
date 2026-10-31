// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
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
#include "is-hw-chain.h"
#include "is-hw.h"

static int __ischain_byrp_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_LVN_BYRP0_BYR:
	case IS_LVN_BYRP1_BYR:
	case IS_LVN_BYRP2_BYR:
	case IS_LVN_BYRP3_BYR:
	case IS_LVN_BYRP4_BYR:
		*pindex = PARAM_BYRP_BYR;
		ret = 2;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __byrp_otf_in_cfg(struct is_device_ischain *device, struct is_group *group,
	struct is_frame *ldr_frame, struct camera2_node *node, enum is_param pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_device_sensor *sensor;
	struct param_sensor_config *ss_cfg;
	struct param_otf_input *otf_in;
	struct param_otf_output *otf_out;
	u32 cmd;

	sensor = device->sensor;
	ss_cfg = is_itf_g_param(device, NULL, PARAM_SENSOR_CONFIG);
	otf_in = is_itf_g_param(device, ldr_frame, pindex);

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state) ||
		test_bit(IS_GROUP_VOTF_INPUT, &group->state))
		cmd = OTF_INPUT_COMMAND_DISABLE;
	else
		cmd = OTF_INPUT_COMMAND_ENABLE;

	if (otf_in->cmd != cmd)
		mlvinfo("[F%d] OTF enable: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			otf_in->cmd, cmd);

	otf_in->cmd = cmd;
	set_bit(pindex, pmap);

	if (!otf_in->cmd)
		return 0;

	otf_in->width = is_sensor_g_bns_width(sensor);
	otf_in->height = is_sensor_g_bns_height(sensor);
	otf_in->offset_x = 0;
	otf_in->offset_y = 0;
	otf_in->physical_width = otf_in->width;
	otf_in->physical_height = otf_in->height;
	otf_in->format = OTF_INPUT_FORMAT_BAYER;
	otf_in->order = ss_cfg->pixel_order;
	otf_in->bayer_crop_offset_x = 0;
	otf_in->bayer_crop_offset_y = 0;
	otf_in->bayer_crop_width = otf_in->width;
	otf_in->bayer_crop_height = otf_in->height;

	if (sensor->cfg[sensor->nfi_toggle] &&
		sensor->cfg[sensor->nfi_toggle]->input[CSI_VIRTUAL_CH_0].hwformat ==
			HW_FORMAT_RAW12)
		otf_in->bitwidth = OTF_INPUT_BIT_WIDTH_14BIT;
	else
		otf_in->bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;

	otf_out = is_itf_g_param(device, ldr_frame, PARAM_BYRP_OTF_OUTPUT);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state) &&
		!test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))
		otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_out->cmd = OTF_OUTPUT_COMMAND_DISABLE;

	otf_out->width = otf_in->width;
	otf_out->height = otf_in->height;

	set_bit(PARAM_BYRP_OTF_OUTPUT, pmap);

	return 0;
}

static int __byrp_dma_in_cfg(struct is_device_ischain *device, struct is_subdev *leader,
	struct is_frame *ldr_frame, struct camera2_node *node, u32 pindex, IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_input *dma_input;
	struct param_otf_output *otf_out;
	struct is_group *group;
	struct is_param_region *param_region;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_msb;
	u32 hw_order, flag_extra, flag_pixel_size;
	struct is_crop in_cfg;

	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);

	group = container_of(leader, struct is_group, leader);
	dma_input = is_itf_g_param(device, ldr_frame, pindex);

	if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_input->v_otf_enable = DMA_INPUT_VOTF_ENABLE;
	} else if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
		dma_input->v_otf_enable = DMA_INPUT_VOTF_DISABLE;
	} else {
		if (dma_input->cmd != node->request)
			mlvinfo("[F%d] RDMA enable: %d -> %d\n", device, node->vid,
				ldr_frame->fcount, dma_input->cmd, node->request);
		dma_input->cmd = node->request;
		dma_input->v_otf_enable = DMA_INPUT_VOTF_DISABLE;
	}

	set_bit(pindex, pmap);

	if (!dma_input->cmd)
		return 0;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	hw_bitwidth = fmt->hw_bitwidth;
	hw_msb = fmt->bitsperpixel[0] - 1;
	param_region = &device->is_region->parameter;
	hw_order = param_region->sensor.config.pixel_order;

	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = node->flags & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	if (hw_format == DMA_INOUT_FORMAT_BAYER)
		mdbgs_ischain(
			3, "in_crop[unpack bitwidth: %d, msb: %d]\n", device, hw_bitwidth, hw_msb);
	else if (hw_format == DMA_INOUT_FORMAT_BAYER_PACKED)
		mdbgs_ischain(
			3, "in_crop[packed bitwidth: %d, msb: %d]\n", device, hw_bitwidth, hw_msb);

	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_OFF | flag_extra);

	in_cfg.x = 0;
	in_cfg.y = 0;
	if (node->width && node->height) {
		in_cfg.w = node->width;
		in_cfg.h = node->height;
	} else {
		in_cfg.w = leader->input.width;
		in_cfg.h = leader->input.height;
	}

	if (dma_input->format != hw_format)
		mlvinfo("[F%d]RDMA format: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			dma_input->format, hw_format);
	if (dma_input->bitwidth != hw_bitwidth)
		mlvinfo("[F%d]RDMA bitwidth: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			dma_input->bitwidth, hw_bitwidth);
	if (dma_input->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]RDMA sbwc_type: %d -> %d\n", device, node->vid, ldr_frame->fcount,
			dma_input->sbwc_type, hw_sbwc);
	if ((dma_input->width != in_cfg.w) || (dma_input->height != in_cfg.h))
		mlvinfo("[F%d]RDMA in[%d, %d]\n", device, node->vid, ldr_frame->fcount, in_cfg.w,
			in_cfg.h);

	dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->order = hw_order;
	dma_input->plane = fmt->hw_plane;
	dma_input->width = in_cfg.w;
	dma_input->height = in_cfg.h;

	/* Not support DMA crop */
	dma_input->dma_crop_offset = 0;
	dma_input->dma_crop_width = in_cfg.w;
	dma_input->dma_crop_height = in_cfg.h;

	/* Not support BAYER crop */
	dma_input->bayer_crop_offset_x = 0;
	dma_input->bayer_crop_offset_y = 0;
	dma_input->bayer_crop_width = in_cfg.w;
	dma_input->bayer_crop_height = in_cfg.h;
	dma_input->stride_plane0 = in_cfg.w;
	dma_input->stride_plane1 = in_cfg.w;
	dma_input->stride_plane2 = in_cfg.w;

	/* VOTF of slave in is always disabled */
	dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;

	otf_out = is_itf_g_param(device, ldr_frame, PARAM_BYRP_OTF_OUTPUT);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state) &&
		!test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))
		otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_out->cmd = OTF_OUTPUT_COMMAND_DISABLE;

	otf_out->width = dma_input->bayer_crop_width;
	otf_out->height = dma_input->bayer_crop_height;

	set_bit(PARAM_BYRP_OTF_OUTPUT, pmap);

	return 0;
}

static int __byrp_dma_out_cfg(struct is_device_ischain *device, struct is_subdev *leader,
	struct is_frame *frame, struct camera2_node *node,
	u32 pindex, IS_DECLARE_PMAP(pmap), int index)
{
	struct is_fmt *fmt;
	struct param_dma_output *dma_output;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	struct is_crop otcrop_cfg;
	u32 hw_format = DMA_INOUT_FORMAT_BAYER;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_plane, hw_msb, hw_order, flag_extra, flag_pixel_size;
	u32 width;
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

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[F%d][%d] otcrop is NULL (%d, %d, %d, %d), disable DMA", device, node->vid,
			frame->fcount, pindex, otcrop->x, otcrop->y, otcrop->w, otcrop->h);
		otcrop->x = otcrop->y = 0;
		otcrop->w = leader->input.width;
		otcrop->h = leader->input.height;
		dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}
	otcrop_cfg = *otcrop;
	width = otcrop_cfg.w;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[F%d][%d] incrop is NULL (%d, %d, %d, %d), disable DMA", device, node->vid,
			frame->fcount, pindex, incrop->x, incrop->y, incrop->w, incrop->h);
		incrop->x = 0;
		incrop->y = 0;
		incrop->w = leader->input.width;
		incrop->h = leader->input.height;
		dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}

	fmt = is_find_format(node->pixelformat, node->flags);

	if (!fmt) {
		merr("pixelformat(%c%c%c%c) is not found", device,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}
	hw_format = fmt->hw_format;
	hw_bitwidth = fmt->hw_bitwidth;
	hw_msb = fmt->bitsperpixel[0] - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	/* pixel type [0:5] : pixel size, [6:7] : extra */
	flag_pixel_size = node->flags & PIXEL_TYPE_SIZE_MASK;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK_LLC_OFF | flag_extra);

	/* WDMA */
	switch (node->vid) {
	case IS_LVN_BYRP0_BYR:
	case IS_LVN_BYRP1_BYR:
	case IS_LVN_BYRP2_BYR:
	case IS_LVN_BYRP3_BYR:
		/* flag_pixel_size is always CAMERA_PIXEL_SIZE_16BIT */
		if (hw_format == DMA_INOUT_FORMAT_BAYER) {
			hw_bitwidth =
				DMA_INOUT_BIT_WIDTH_16BIT; /* unpack uses bit width 16b only */
			mdbgs_ischain(3, "wdma(%d) unpack (bitwidth: %d, msb: %d)\n", device,
				node->vid, hw_bitwidth, hw_msb);
		} else {
			merr("Undefined wdma(%d) format(%d)", device, node->vid, hw_format);
		}
		break;
	default:
		merr("wdma(%d) is not found", device, node->vid);
		break;
	}

	if (dma_output->format != hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device, node->vid, frame->fcount,
			dma_output->format, hw_format);
	if (dma_output->bitwidth != hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device, node->vid, frame->fcount,
			dma_output->bitwidth, hw_bitwidth);
	if (dma_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]WDMA sbwc_type: %d -> %d\n", device, node->vid, frame->fcount,
			dma_output->sbwc_type, hw_sbwc);
	if (((dma_output->width != otcrop->w) || (dma_output->height != otcrop->h)))
		mlvinfo("[F%d]WDMA otcrop[%d, %d, %d, %d]\n", device, node->vid, frame->fcount,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;

	group = container_of(leader, struct is_group, leader);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_OUTPUT, &group->state))
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_ENABLE;
		else
			dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE;
	} else {
		dma_output->v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE;
	}

	dma_output->format = hw_format;
	dma_output->bitwidth = hw_bitwidth;
	dma_output->msb = hw_msb;
	dma_output->sbwc_type = hw_sbwc;
	dma_output->order = hw_order;
	dma_output->plane = hw_plane;
	dma_output->width = otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;
	dma_output->dma_crop_offset_x = 0;
	dma_output->dma_crop_offset_y = 0;
	dma_output->dma_crop_width = otcrop_cfg.w;
	dma_output->dma_crop_height = otcrop_cfg.h;

	return ret;
}

static int is_ischain_byrp_tag(struct is_subdev *subdev, void *device_data, struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct byrp_param *byrp_param;
	struct is_crop *incrop, *otcrop;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	IS_DECLARE_PMAP(pmap);
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_param_region *param_region;
	u32 pindex = 0;
	int i, dma_type;
	struct param_control *control;
	u32 calibrated_width, calibrated_height;
	u32 sensor_binning_ratio_x, sensor_binning_ratio_y;
	u32 bns_binning_ratio_x, bns_binning_ratio_y;
	u32 binned_sensor_width, binned_sensor_height;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "BYRP TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = container_of(subdev, struct is_group, leader);
	IS_INIT_PMAP(pmap);
	byrp_param = &device->is_region->parameter.byrp;

	control = is_itf_g_param(device, frame, PARAM_BYRP_CONTROL);
	if (test_bit(IS_GROUP_STRGEN_INPUT, &group->state))
		control->strgen = CONTROL_COMMAND_START;
	else
		control->strgen = CONTROL_COMMAND_STOP;

	set_bit(PARAM_BYRP_CONTROL, pmap);

	out_node = &frame->shot_ext->node_group.leader;
	ret = __byrp_otf_in_cfg(device, group, frame, out_node, PARAM_BYRP_OTF_INPUT, pmap);
	if (ret) {
		mlverr("[F%d] otf_in_cfg fail. ret %d", device, subdev->vid, frame->fcount, ret);
		goto p_err;
	}

	ret = __byrp_dma_in_cfg(device, subdev, frame, out_node, PARAM_BYRP_DMA_INPUT, pmap);
	if (ret) {
		mlverr("[F%d] dma_in_cfg error\n", device, subdev->vid, frame->fcount);
		goto p_err;
	}

	out_node->result = 1;

	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		cap_node = &frame->shot_ext->node_group.capture[i];

		if (!cap_node->vid)
			continue;

		dma_type = __ischain_byrp_slot(cap_node, &pindex);
		if (dma_type == 2) {
			ret = __byrp_dma_out_cfg(device, subdev, frame, cap_node, pindex, pmap, i);

			if (ret) {
				mlverr("[F%d] dma_out_cfg error\n", device, cap_node->vid,
					frame->fcount);
				continue;
			}
		} else {
			continue;
		}

		if (!cap_node->request)
			continue;

		cap_node->result = 1;
	}

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	param_region = &device->is_region->parameter;
	sensor = device->sensor;
	if (!test_bit(IS_GROUP_REPROCESSING, &group->state)) {
		calibrated_width = param_region->sensor.config.calibrated_width;
		calibrated_height = param_region->sensor.config.calibrated_height;
		sensor_binning_ratio_x = param_region->sensor.config.sensor_binning_ratio_x;
		sensor_binning_ratio_y = param_region->sensor.config.sensor_binning_ratio_y;
		binned_sensor_width = calibrated_width * 1000 / sensor_binning_ratio_x;
		binned_sensor_height = calibrated_height * 1000 / sensor_binning_ratio_y;
		bns_binning_ratio_x = param_region->sensor.config.bns_binning_ratio_x;
		bns_binning_ratio_y = param_region->sensor.config.bns_binning_ratio_y;

		frame->shot->udm.frame_info.sensor_size[0] = calibrated_width;
		frame->shot->udm.frame_info.sensor_size[1] = calibrated_height;
		frame->shot->udm.frame_info.sensor_binning[0] = sensor_binning_ratio_x;
		frame->shot->udm.frame_info.sensor_binning[1] = sensor_binning_ratio_y;
		frame->shot->udm.frame_info.bns_binning[0] = bns_binning_ratio_x;
		frame->shot->udm.frame_info.bns_binning[1] = bns_binning_ratio_y;

		/* freeform - use physical sensor crop offset */
		if (param_region->sensor.config.freeform_sensor_crop_enable == true) {
			frame->shot->udm.frame_info.sensor_crop_region[0] =
				(param_region->sensor.config.freeform_sensor_crop_offset_x) &
				(~0x1);
			frame->shot->udm.frame_info.sensor_crop_region[1] =
				(param_region->sensor.config.freeform_sensor_crop_offset_y) &
				(~0x1);
		} else { /* center-aglign - use logical sensor crop offset */
			frame->shot->udm.frame_info.sensor_crop_region[0] =
				((binned_sensor_width - param_region->sensor.config.width) >> 1) &
				(~0x1);
			frame->shot->udm.frame_info.sensor_crop_region[1] =
				((binned_sensor_height - param_region->sensor.config.height) >> 1) &
				(~0x1);
		}
		frame->shot->udm.frame_info.sensor_crop_region[2] =
			param_region->sensor.config.width;
		frame->shot->udm.frame_info.sensor_crop_region[3] =
			param_region->sensor.config.height;
	} else {
		if (sensor->cfg[sensor->nfi_toggle]->special_mode == IS_SPECIAL_MODE_REMOSAIC &&
			sensor->cfg[sensor->nfi_toggle]->hw_bns > HW_BNS_1_0) {
			bns_binning_ratio_x = param_region->sensor.config.bns_binning_ratio_x;
			bns_binning_ratio_y = param_region->sensor.config.bns_binning_ratio_y;
			frame->shot->udm.frame_info.bns_binning[0] = bns_binning_ratio_x;
			frame->shot->udm.frame_info.bns_binning[1] = bns_binning_ratio_y;
		}
	}

	return 0;

p_err:
	return ret;
}

static const struct is_subdev_ops is_subdev_byrp_ops = {
	.bypass = NULL,
	.cfg = NULL,
	.tag = is_ischain_byrp_tag,
};

const struct is_subdev_ops *pablo_get_is_subdev_byrp_ops(void)
{
	return &is_subdev_byrp_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_byrp_ops);
