// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include "punit-test-hw-ip.h"
#include "punit-test-file-io.h"
#include "../pablo-self-test-result.h"
#include "pablo-framemgr.h"
#include "is-hw.h"
#include "is-core.h"
#include "is-device-ischain.h"

static int pst_set_hw_mtnr1(const char *val, const struct kernel_param *kp);
static int pst_get_hw_mtnr1(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_mtnr1 = {
	.set = pst_set_hw_mtnr1,
	.get = pst_get_hw_mtnr1,
};
module_param_cb(test_hw_mtnr1, &pablo_param_ops_hw_mtnr1, NULL, 0644);

#define NUM_OF_MTNR_PARAM (PARAM_MTNR_WDMA_PREV_L1_WGT - PARAM_MTNR_CONTROL + 1)

static struct is_frame *frame_mtnr1;
static u32 mtnr1_param[NUM_OF_MTNR_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_MTNR_PARAM];
static struct size_cr_set mtnr1_cr_set;
static struct mtnr1_size_conf_set {
	u32 size;
	struct is_mtnr1_config conf;
} mtnr1_size_conf;

static const struct mtnr_param mtnr1_param_preset_grp[] = {
	/* Param set[0]: cur DMA input */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].cin_dlfe_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].cin_dlfe_wgt.format = OTF_INPUT_FORMAT_YUV422,
	[0].cin_dlfe_wgt.bitwidth = 0,
	[0].cin_dlfe_wgt.order = 0,
	[0].cin_dlfe_wgt.width = 1920,
	[0].cin_dlfe_wgt.height = 1440,
	[0].cin_dlfe_wgt.bayer_crop_offset_x = 0,
	[0].cin_dlfe_wgt.bayer_crop_offset_y = 0,
	[0].cin_dlfe_wgt.bayer_crop_width = 1920,
	[0].cin_dlfe_wgt.bayer_crop_height = 1440,
	[0].cin_dlfe_wgt.source = 0,
	[0].cin_dlfe_wgt.physical_width = 0,
	[0].cin_dlfe_wgt.physical_height = 0,
	[0].cin_dlfe_wgt.offset_x = 0,
	[0].cin_dlfe_wgt.offset_y = 0,

	[0].cout_msnr_l1.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_l1.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l1.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l1.order = 2,
	[0].cout_msnr_l1.width = 1920 / 2,
	[0].cout_msnr_l1.height = 1440 / 2,
	[0].cout_msnr_l1.crop_offset_x = 0,
	[0].cout_msnr_l1.crop_offset_y = 0,
	[0].cout_msnr_l1.crop_width = 0,
	[0].cout_msnr_l1.crop_height = 0,
	[0].cout_msnr_l1.crop_enable = 0,

	[0].cout_msnr_l2.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_l2.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l2.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l2.order = 2,
	[0].cout_msnr_l2.width = 1920 / 4,
	[0].cout_msnr_l2.height = 1440 / 4,
	[0].cout_msnr_l2.crop_offset_x = 0,
	[0].cout_msnr_l2.crop_offset_y = 0,
	[0].cout_msnr_l2.crop_width = 0,
	[0].cout_msnr_l2.crop_height = 0,
	[0].cout_msnr_l2.crop_enable = 0,

	[0].cout_msnr_l3.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_l3.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l3.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l3.order = 2,
	[0].cout_msnr_l3.width = 1920 / 8,
	[0].cout_msnr_l3.height = 1440 / 8,
	[0].cout_msnr_l3.crop_offset_x = 0,
	[0].cout_msnr_l3.crop_offset_y = 0,
	[0].cout_msnr_l3.crop_width = 0,
	[0].cout_msnr_l3.crop_height = 0,
	[0].cout_msnr_l3.crop_enable = 0,

	[0].cout_msnr_l4.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_l4.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l4.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l4.order = 2,
	[0].cout_msnr_l4.width = 1920 / 16,
	[0].cout_msnr_l4.height = 1440 / 16,
	[0].cout_msnr_l4.crop_offset_x = 0,
	[0].cout_msnr_l4.crop_offset_y = 0,
	[0].cout_msnr_l4.crop_width = 0,
	[0].cout_msnr_l4.crop_height = 0,
	[0].cout_msnr_l4.crop_enable = 0,

	[0].rdma_cur_l1.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l1.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l1.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l1.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l1.plane = 3,
	[0].rdma_cur_l1.width = 1920 / 2,
	[0].rdma_cur_l1.height = 1440 / 2,
	[0].rdma_cur_l1.dma_crop_offset = 0,
	[0].rdma_cur_l1.dma_crop_width = 1920 / 2,
	[0].rdma_cur_l1.dma_crop_height = 1440 / 2,
	[0].rdma_cur_l1.bayer_crop_offset_x = 0,
	[0].rdma_cur_l1.bayer_crop_offset_y = 0,
	[0].rdma_cur_l1.bayer_crop_width = 1920 / 2,
	[0].rdma_cur_l1.bayer_crop_height = 1440 / 2,
	[0].rdma_cur_l1.scene_mode = 0,
	[0].rdma_cur_l1.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l1.stride_plane0 = 1920 / 2,
	[0].rdma_cur_l1.stride_plane1 = 1920 / 2,
	[0].rdma_cur_l1.stride_plane2 = 1920 / 2,
	[0].rdma_cur_l1.v_otf_enable = 0,
	[0].rdma_cur_l1.orientation = 0,
	[0].rdma_cur_l1.strip_mode = 0,
	[0].rdma_cur_l1.overlab_width = 0,
	[0].rdma_cur_l1.strip_count = 0,
	[0].rdma_cur_l1.strip_max_count = 0,
	[0].rdma_cur_l1.sequence_id = 0,
	[0].rdma_cur_l1.sbwc_type = 0,

	[0].rdma_cur_l2.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l2.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l2.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l2.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l2.plane = 3,
	[0].rdma_cur_l2.width = 1920 / 4,
	[0].rdma_cur_l2.height = 1440 / 4,
	[0].rdma_cur_l2.dma_crop_offset = 0,
	[0].rdma_cur_l2.dma_crop_width = 1920 / 4,
	[0].rdma_cur_l2.dma_crop_height = 1440 / 4,
	[0].rdma_cur_l2.bayer_crop_offset_x = 0,
	[0].rdma_cur_l2.bayer_crop_offset_y = 0,
	[0].rdma_cur_l2.bayer_crop_width = 1920 / 4,
	[0].rdma_cur_l2.bayer_crop_height = 1440 / 4,
	[0].rdma_cur_l2.scene_mode = 0,
	[0].rdma_cur_l2.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l2.stride_plane0 = 1920 / 4,
	[0].rdma_cur_l2.stride_plane1 = 1920 / 4,
	[0].rdma_cur_l2.stride_plane2 = 1920 / 4,
	[0].rdma_cur_l2.v_otf_enable = 0,
	[0].rdma_cur_l2.orientation = 0,
	[0].rdma_cur_l2.strip_mode = 0,
	[0].rdma_cur_l2.overlab_width = 0,
	[0].rdma_cur_l2.strip_count = 0,
	[0].rdma_cur_l2.strip_max_count = 0,
	[0].rdma_cur_l2.sequence_id = 0,
	[0].rdma_cur_l2.sbwc_type = 0,

	[0].rdma_cur_l3.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l3.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l3.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l3.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l3.plane = 3,
	[0].rdma_cur_l3.width = 1920 / 8,
	[0].rdma_cur_l3.height = 1440 / 8,
	[0].rdma_cur_l3.dma_crop_offset = 0,
	[0].rdma_cur_l3.dma_crop_width = 1920 / 8,
	[0].rdma_cur_l3.dma_crop_height = 1440 / 8,
	[0].rdma_cur_l3.bayer_crop_offset_x = 0,
	[0].rdma_cur_l3.bayer_crop_offset_y = 0,
	[0].rdma_cur_l3.bayer_crop_width = 1920 / 8,
	[0].rdma_cur_l3.bayer_crop_height = 1440 / 8,
	[0].rdma_cur_l3.scene_mode = 0,
	[0].rdma_cur_l3.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l3.stride_plane0 = 1920 / 8,
	[0].rdma_cur_l3.stride_plane1 = 1920 / 8,
	[0].rdma_cur_l3.stride_plane2 = 1920 / 8,
	[0].rdma_cur_l3.v_otf_enable = 0,
	[0].rdma_cur_l3.orientation = 0,
	[0].rdma_cur_l3.strip_mode = 0,
	[0].rdma_cur_l3.overlab_width = 0,
	[0].rdma_cur_l3.strip_count = 0,
	[0].rdma_cur_l3.strip_max_count = 0,
	[0].rdma_cur_l3.sequence_id = 0,
	[0].rdma_cur_l3.sbwc_type = 0,

	[0].rdma_cur_l4.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l4.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l4.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l4.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l4.plane = 3,
	[0].rdma_cur_l4.width = 1920 / 16,
	[0].rdma_cur_l4.height = 1440 / 16,
	[0].rdma_cur_l4.dma_crop_offset = 0,
	[0].rdma_cur_l4.dma_crop_width = 1920 / 16,
	[0].rdma_cur_l4.dma_crop_height = 1440 / 16,
	[0].rdma_cur_l4.bayer_crop_offset_x = 0,
	[0].rdma_cur_l4.bayer_crop_offset_y = 0,
	[0].rdma_cur_l4.bayer_crop_width = 1920 / 16,
	[0].rdma_cur_l4.bayer_crop_height = 1440 / 16,
	[0].rdma_cur_l4.scene_mode = 0,
	[0].rdma_cur_l4.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l4.stride_plane0 = 1920 / 16,
	[0].rdma_cur_l4.stride_plane1 = 1920 / 16,
	[0].rdma_cur_l4.stride_plane2 = 1920 / 16,
	[0].rdma_cur_l4.v_otf_enable = 0,
	[0].rdma_cur_l4.orientation = 0,
	[0].rdma_cur_l4.strip_mode = 0,
	[0].rdma_cur_l4.overlab_width = 0,
	[0].rdma_cur_l4.strip_count = 0,
	[0].rdma_cur_l4.strip_max_count = 0,
	[0].rdma_cur_l4.sequence_id = 0,
	[0].rdma_cur_l4.sbwc_type = 0,

	[0].rdma_prev_l1.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l2.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l3.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l4.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l1.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l2.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l3.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l4.cmd = OTF_INPUT_COMMAND_DISABLE,

	[0].wdma_prev_l1.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l1.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l1.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l1.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l1.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l1.width = 1920 / 2,
	[0].wdma_prev_l1.height = 1440 / 2,
	[0].wdma_prev_l1.dma_crop_offset_x = 0,
	[0].wdma_prev_l1.dma_crop_offset_y = 0,
	[0].wdma_prev_l1.dma_crop_width = 1920 / 2,
	[0].wdma_prev_l1.dma_crop_height = 1440 / 2,
	[0].wdma_prev_l1.crop_enable = 0,
	[0].wdma_prev_l1.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l1.stride_plane0 = 1920 / 2,
	[0].wdma_prev_l1.stride_plane1 = 1920 / 2,
	[0].wdma_prev_l1.stride_plane2 = 1920 / 2,
	[0].wdma_prev_l1.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l1.sbwc_type = NONE,

	[0].wdma_prev_l2.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l2.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l2.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l2.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l2.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l2.width = 1920 / 4,
	[0].wdma_prev_l2.height = 1440 / 4,
	[0].wdma_prev_l2.dma_crop_offset_x = 0,
	[0].wdma_prev_l2.dma_crop_offset_y = 0,
	[0].wdma_prev_l2.dma_crop_width = 1920 / 4,
	[0].wdma_prev_l2.dma_crop_height = 1440 / 4,
	[0].wdma_prev_l2.crop_enable = 0,
	[0].wdma_prev_l2.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l2.stride_plane0 = 1920 / 4,
	[0].wdma_prev_l2.stride_plane1 = 1920 / 4,
	[0].wdma_prev_l2.stride_plane2 = 1920 / 4,
	[0].wdma_prev_l2.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l2.sbwc_type = NONE,

	[0].wdma_prev_l3.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l3.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l3.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l3.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l3.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l3.width = 1920 / 8,
	[0].wdma_prev_l3.height = 1440 / 8,
	[0].wdma_prev_l3.dma_crop_offset_x = 0,
	[0].wdma_prev_l3.dma_crop_offset_y = 0,
	[0].wdma_prev_l3.dma_crop_width = 1920 / 8,
	[0].wdma_prev_l3.dma_crop_height = 1440 / 8,
	[0].wdma_prev_l3.crop_enable = 0,
	[0].wdma_prev_l3.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l3.stride_plane0 = 1920 / 8,
	[0].wdma_prev_l3.stride_plane1 = 1920 / 8,
	[0].wdma_prev_l3.stride_plane2 = 1920 / 8,
	[0].wdma_prev_l3.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l3.sbwc_type = NONE,

	[0].wdma_prev_l4.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l4.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l4.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l4.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l4.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l4.width = 1920 / 16,
	[0].wdma_prev_l4.height = 1440 / 16,
	[0].wdma_prev_l4.dma_crop_offset_x = 0,
	[0].wdma_prev_l4.dma_crop_offset_y = 0,
	[0].wdma_prev_l4.dma_crop_width = 1920 / 16,
	[0].wdma_prev_l4.dma_crop_height = 1440 / 16,
	[0].wdma_prev_l4.crop_enable = 0,
	[0].wdma_prev_l4.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l4.stride_plane0 = 1920 / 16,
	[0].wdma_prev_l4.stride_plane1 = 1920 / 16,
	[0].wdma_prev_l4.stride_plane2 = 1920 / 16,
	[0].wdma_prev_l4.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l4.sbwc_type = NONE,

	[0].wdma_prev_l1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,

	/* Param set[1]:  */
};

static const struct mtnr_param mtnr1_param_preset[] = {
	/* Param set[0]: cur DMA input */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].cin_dlfe_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].cin_dlfe_wgt.format = OTF_INPUT_FORMAT_YUV422,
	[0].cin_dlfe_wgt.bitwidth = 0,
	[0].cin_dlfe_wgt.order = 0,
	[0].cin_dlfe_wgt.width = 1920,
	[0].cin_dlfe_wgt.height = 1440,
	[0].cin_dlfe_wgt.bayer_crop_offset_x = 0,
	[0].cin_dlfe_wgt.bayer_crop_offset_y = 0,
	[0].cin_dlfe_wgt.bayer_crop_width = 1920,
	[0].cin_dlfe_wgt.bayer_crop_height = 1440,
	[0].cin_dlfe_wgt.source = 0,
	[0].cin_dlfe_wgt.physical_width = 0,
	[0].cin_dlfe_wgt.physical_height = 0,
	[0].cin_dlfe_wgt.offset_x = 0,
	[0].cin_dlfe_wgt.offset_y = 0,

	[0].cout_msnr_l1.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cout_msnr_l1.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l1.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l1.order = 2,
	[0].cout_msnr_l1.width = 1920 / 2,
	[0].cout_msnr_l1.height = 1440 / 2,
	[0].cout_msnr_l1.crop_offset_x = 0,
	[0].cout_msnr_l1.crop_offset_y = 0,
	[0].cout_msnr_l1.crop_width = 0,
	[0].cout_msnr_l1.crop_height = 0,
	[0].cout_msnr_l1.crop_enable = 0,

	[0].cout_msnr_l2.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cout_msnr_l2.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l2.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l2.order = 2,
	[0].cout_msnr_l2.width = 1920 / 4,
	[0].cout_msnr_l2.height = 1440 / 4,
	[0].cout_msnr_l2.crop_offset_x = 0,
	[0].cout_msnr_l2.crop_offset_y = 0,
	[0].cout_msnr_l2.crop_width = 0,
	[0].cout_msnr_l2.crop_height = 0,
	[0].cout_msnr_l2.crop_enable = 0,

	[0].cout_msnr_l3.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cout_msnr_l3.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l3.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l3.order = 2,
	[0].cout_msnr_l3.width = 1920 / 8,
	[0].cout_msnr_l3.height = 1440 / 8,
	[0].cout_msnr_l3.crop_offset_x = 0,
	[0].cout_msnr_l3.crop_offset_y = 0,
	[0].cout_msnr_l3.crop_width = 0,
	[0].cout_msnr_l3.crop_height = 0,
	[0].cout_msnr_l3.crop_enable = 0,

	[0].cout_msnr_l4.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cout_msnr_l4.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l4.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l4.order = 2,
	[0].cout_msnr_l4.width = 1920 / 16,
	[0].cout_msnr_l4.height = 1440 / 16,
	[0].cout_msnr_l4.crop_offset_x = 0,
	[0].cout_msnr_l4.crop_offset_y = 0,
	[0].cout_msnr_l4.crop_width = 0,
	[0].cout_msnr_l4.crop_height = 0,
	[0].cout_msnr_l4.crop_enable = 0,

	[0].rdma_cur_l1.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l1.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l1.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l1.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l1.plane = 3,
	[0].rdma_cur_l1.width = 1920 / 2,
	[0].rdma_cur_l1.height = 1440 / 2,
	[0].rdma_cur_l1.dma_crop_offset = 0,
	[0].rdma_cur_l1.dma_crop_width = 1920 / 2,
	[0].rdma_cur_l1.dma_crop_height = 1440 / 2,
	[0].rdma_cur_l1.bayer_crop_offset_x = 0,
	[0].rdma_cur_l1.bayer_crop_offset_y = 0,
	[0].rdma_cur_l1.bayer_crop_width = 1920 / 2,
	[0].rdma_cur_l1.bayer_crop_height = 1440 / 2,
	[0].rdma_cur_l1.scene_mode = 0,
	[0].rdma_cur_l1.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l1.stride_plane0 = 1920 / 2,
	[0].rdma_cur_l1.stride_plane1 = 1920 / 2,
	[0].rdma_cur_l1.stride_plane2 = 1920 / 2,
	[0].rdma_cur_l1.v_otf_enable = 0,
	[0].rdma_cur_l1.orientation = 0,
	[0].rdma_cur_l1.strip_mode = 0,
	[0].rdma_cur_l1.overlab_width = 0,
	[0].rdma_cur_l1.strip_count = 0,
	[0].rdma_cur_l1.strip_max_count = 0,
	[0].rdma_cur_l1.sequence_id = 0,
	[0].rdma_cur_l1.sbwc_type = 0,

	[0].rdma_cur_l2.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l2.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l2.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l2.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l2.plane = 3,
	[0].rdma_cur_l2.width = 1920 / 4,
	[0].rdma_cur_l2.height = 1440 / 4,
	[0].rdma_cur_l2.dma_crop_offset = 0,
	[0].rdma_cur_l2.dma_crop_width = 1920 / 4,
	[0].rdma_cur_l2.dma_crop_height = 1440 / 4,
	[0].rdma_cur_l2.bayer_crop_offset_x = 0,
	[0].rdma_cur_l2.bayer_crop_offset_y = 0,
	[0].rdma_cur_l2.bayer_crop_width = 1920 / 4,
	[0].rdma_cur_l2.bayer_crop_height = 1440 / 4,
	[0].rdma_cur_l2.scene_mode = 0,
	[0].rdma_cur_l2.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l2.stride_plane0 = 1920 / 4,
	[0].rdma_cur_l2.stride_plane1 = 1920 / 4,
	[0].rdma_cur_l2.stride_plane2 = 1920 / 4,
	[0].rdma_cur_l2.v_otf_enable = 0,
	[0].rdma_cur_l2.orientation = 0,
	[0].rdma_cur_l2.strip_mode = 0,
	[0].rdma_cur_l2.overlab_width = 0,
	[0].rdma_cur_l2.strip_count = 0,
	[0].rdma_cur_l2.strip_max_count = 0,
	[0].rdma_cur_l2.sequence_id = 0,
	[0].rdma_cur_l2.sbwc_type = 0,

	[0].rdma_cur_l3.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l3.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l3.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l3.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l3.plane = 3,
	[0].rdma_cur_l3.width = 1920 / 8,
	[0].rdma_cur_l3.height = 1440 / 8,
	[0].rdma_cur_l3.dma_crop_offset = 0,
	[0].rdma_cur_l3.dma_crop_width = 1920 / 8,
	[0].rdma_cur_l3.dma_crop_height = 1440 / 8,
	[0].rdma_cur_l3.bayer_crop_offset_x = 0,
	[0].rdma_cur_l3.bayer_crop_offset_y = 0,
	[0].rdma_cur_l3.bayer_crop_width = 1920 / 8,
	[0].rdma_cur_l3.bayer_crop_height = 1440 / 8,
	[0].rdma_cur_l3.scene_mode = 0,
	[0].rdma_cur_l3.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l3.stride_plane0 = 1920 / 8,
	[0].rdma_cur_l3.stride_plane1 = 1920 / 8,
	[0].rdma_cur_l3.stride_plane2 = 1920 / 8,
	[0].rdma_cur_l3.v_otf_enable = 0,
	[0].rdma_cur_l3.orientation = 0,
	[0].rdma_cur_l3.strip_mode = 0,
	[0].rdma_cur_l3.overlab_width = 0,
	[0].rdma_cur_l3.strip_count = 0,
	[0].rdma_cur_l3.strip_max_count = 0,
	[0].rdma_cur_l3.sequence_id = 0,
	[0].rdma_cur_l3.sbwc_type = 0,

	[0].rdma_cur_l4.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l4.format = DMA_INOUT_FORMAT_YUV444,
	[0].rdma_cur_l4.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l4.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l4.plane = 3,
	[0].rdma_cur_l4.width = 1920 / 16,
	[0].rdma_cur_l4.height = 1440 / 16,
	[0].rdma_cur_l4.dma_crop_offset = 0,
	[0].rdma_cur_l4.dma_crop_width = 1920 / 16,
	[0].rdma_cur_l4.dma_crop_height = 1440 / 16,
	[0].rdma_cur_l4.bayer_crop_offset_x = 0,
	[0].rdma_cur_l4.bayer_crop_offset_y = 0,
	[0].rdma_cur_l4.bayer_crop_width = 1920 / 16,
	[0].rdma_cur_l4.bayer_crop_height = 1440 / 16,
	[0].rdma_cur_l4.scene_mode = 0,
	[0].rdma_cur_l4.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l4.stride_plane0 = 1920 / 16,
	[0].rdma_cur_l4.stride_plane1 = 1920 / 16,
	[0].rdma_cur_l4.stride_plane2 = 1920 / 16,
	[0].rdma_cur_l4.v_otf_enable = 0,
	[0].rdma_cur_l4.orientation = 0,
	[0].rdma_cur_l4.strip_mode = 0,
	[0].rdma_cur_l4.overlab_width = 0,
	[0].rdma_cur_l4.strip_count = 0,
	[0].rdma_cur_l4.strip_max_count = 0,
	[0].rdma_cur_l4.sequence_id = 0,
	[0].rdma_cur_l4.sbwc_type = 0,

	[0].rdma_prev_l1.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l2.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l3.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l4.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l1.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l2.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l3.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l4.cmd = OTF_INPUT_COMMAND_DISABLE,

	[0].wdma_prev_l1.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l1.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l1.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l1.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l1.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l1.width = 1920 / 2,
	[0].wdma_prev_l1.height = 1440 / 2,
	[0].wdma_prev_l1.dma_crop_offset_x = 0,
	[0].wdma_prev_l1.dma_crop_offset_y = 0,
	[0].wdma_prev_l1.dma_crop_width = 1920 / 2,
	[0].wdma_prev_l1.dma_crop_height = 1440 / 2,
	[0].wdma_prev_l1.crop_enable = 0,
	[0].wdma_prev_l1.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l1.stride_plane0 = 1920 / 2,
	[0].wdma_prev_l1.stride_plane1 = 1920 / 2,
	[0].wdma_prev_l1.stride_plane2 = 1920 / 2,
	[0].wdma_prev_l1.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l1.sbwc_type = NONE,

	[0].wdma_prev_l2.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l2.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l2.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l2.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l2.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l2.width = 1920 / 4,
	[0].wdma_prev_l2.height = 1440 / 4,
	[0].wdma_prev_l2.dma_crop_offset_x = 0,
	[0].wdma_prev_l2.dma_crop_offset_y = 0,
	[0].wdma_prev_l2.dma_crop_width = 1920 / 4,
	[0].wdma_prev_l2.dma_crop_height = 1440 / 4,
	[0].wdma_prev_l2.crop_enable = 0,
	[0].wdma_prev_l2.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l2.stride_plane0 = 1920 / 4,
	[0].wdma_prev_l2.stride_plane1 = 1920 / 4,
	[0].wdma_prev_l2.stride_plane2 = 1920 / 4,
	[0].wdma_prev_l2.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l2.sbwc_type = NONE,

	[0].wdma_prev_l3.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l3.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l3.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l3.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l3.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l3.width = 1920 / 8,
	[0].wdma_prev_l3.height = 1440 / 8,
	[0].wdma_prev_l3.dma_crop_offset_x = 0,
	[0].wdma_prev_l3.dma_crop_offset_y = 0,
	[0].wdma_prev_l3.dma_crop_width = 1920 / 8,
	[0].wdma_prev_l3.dma_crop_height = 1440 / 8,
	[0].wdma_prev_l3.crop_enable = 0,
	[0].wdma_prev_l3.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l3.stride_plane0 = 1920 / 8,
	[0].wdma_prev_l3.stride_plane1 = 1920 / 8,
	[0].wdma_prev_l3.stride_plane2 = 1920 / 8,
	[0].wdma_prev_l3.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l3.sbwc_type = NONE,

	[0].wdma_prev_l4.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l4.format = DMA_INOUT_FORMAT_YUV444,
	[0].wdma_prev_l4.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].wdma_prev_l4.order = DMA_INOUT_ORDER_CrCb,
	[0].wdma_prev_l4.plane = DMA_INOUT_PLANE_3,
	[0].wdma_prev_l4.width = 1920 / 16,
	[0].wdma_prev_l4.height = 1440 / 16,
	[0].wdma_prev_l4.dma_crop_offset_x = 0,
	[0].wdma_prev_l4.dma_crop_offset_y = 0,
	[0].wdma_prev_l4.dma_crop_width = 1920 / 16,
	[0].wdma_prev_l4.dma_crop_height = 1440 / 16,
	[0].wdma_prev_l4.crop_enable = 0,
	[0].wdma_prev_l4.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].wdma_prev_l4.stride_plane0 = 1920 / 16,
	[0].wdma_prev_l4.stride_plane1 = 1920 / 16,
	[0].wdma_prev_l4.stride_plane2 = 1920 / 16,
	[0].wdma_prev_l4.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].wdma_prev_l4.sbwc_type = NONE,

	[0].wdma_prev_l1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,

	/* Param set[1]:  */
};

static DECLARE_BITMAP(result, ARRAY_SIZE(mtnr1_param_preset));

static void pst_set_size_mtnr1(void *in_param, void *out_param)
{
	struct mtnr_param *p = (struct mtnr_param *)mtnr1_param;
	struct param_size_byrp2yuvp *in = (struct param_size_byrp2yuvp *)in_param;
	struct param_size_byrp2yuvp *out = (struct param_size_byrp2yuvp *)out_param;

	if (!in || !out)
		return;

	p->cout_msnr_l0.width = out->w_mcfp;
	p->cout_msnr_l0.height = out->h_mcfp;

	p->rdma_cur_l0.width = in->w_mcfp;
	p->rdma_cur_l0.height = in->h_mcfp;
	p->rdma_cur_l0.dma_crop_offset = 0;
	p->rdma_cur_l0.dma_crop_width = in->w_mcfp;
	p->rdma_cur_l0.dma_crop_height = in->h_mcfp;

	p->rdma_cur_l4.width = in->w_mcfp / 16;
	p->rdma_cur_l4.height = in->h_mcfp / 16;
	p->rdma_cur_l4.dma_crop_offset = 0;
	p->rdma_cur_l4.dma_crop_width = in->w_mcfp / 16;
	p->rdma_cur_l4.dma_crop_height = in->h_mcfp / 16;

	p->wdma_prev_l0.width = out->w_mcfp;
	p->wdma_prev_l0.height = out->h_mcfp;
	p->wdma_prev_l0.dma_crop_offset_x = out->x_mcfp;
	p->wdma_prev_l0.dma_crop_offset_y = out->y_mcfp;
	p->wdma_prev_l0.dma_crop_width = out->w_mcfp;
	p->wdma_prev_l0.dma_crop_height = out->h_mcfp;
}

static enum pst_blob_node pst_get_blob_node_mtnr1(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_MTNR_CONTROL + idx) {
	case PARAM_MTNR_WDMA_PREV_L1:
		bn = PST_BLOB_MTNR_L1;
		break;
	case PARAM_MTNR_WDMA_PREV_L2:
		bn = PST_BLOB_MTNR_L2;
		break;
	case PARAM_MTNR_WDMA_PREV_L3:
		bn = PST_BLOB_MTNR_L3;
		break;
	case PARAM_MTNR_WDMA_PREV_L4:
		bn = PST_BLOB_MTNR_L4;
		break;
	case PARAM_MTNR_WDMA_PREV_L1_WGT:
		bn = PST_BLOB_MTNR_L1_W;
		break;
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_mtnr1(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	u32 block_w = MTNR0_COMP_BLOCK_WIDTH;
	u32 block_h = MTNR0_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_MTNR_CONTROL + param_idx) {
	case PARAM_MTNR_RDMA_CUR_L1:
		dva = frame->dva_mtnr1_out_cur_l1_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_CUR_L2:
		dva = frame->dva_mtnr1_out_cur_l2_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_CUR_L3:
		dva = frame->dva_mtnr1_out_cur_l3_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_CUR_L4:
		dva = frame->dva_mtnr1_out_cur_l4_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_PREV_L1:
		dva = frame->dva_mtnr1_out_prev_l1_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_PREV_L1_WGT:
		dva = frame->dva_mtnr1_out_prev_l1_wgt;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_PREV_L2:
		dva = frame->dva_mtnr1_out_prev_l2_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_PREV_L3:
		dva = frame->dva_mtnr1_out_prev_l3_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_PREV_L4:
		dva = frame->dva_mtnr1_out_prev_l4_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_MV_GEOMATCH:
		dva = frame->dva_mtnr0_out_mv_geomatch;
		if (!dva[0])
			pst_get_size_of_dma_input(
				&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_SEG_L1:
		dva = frame->dva_mtnr1_out_seg_l1;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_SEG_L2:
		dva = frame->dva_mtnr1_out_seg_l2;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_SEG_L3:
		dva = frame->dva_mtnr1_out_seg_l3;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_SEG_L4:
		dva = frame->dva_mtnr1_out_seg_l4;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_WDMA_PREV_L1:
		dva = frame->dva_mtnr1_cap_l1_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_WDMA_PREV_L1_WGT:
		dva = frame->dva_mtnr1_cap_l1_wgt;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_WDMA_PREV_L2:
		dva = frame->dva_mtnr1_cap_l2_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_WDMA_PREV_L3:
		dva = frame->dva_mtnr1_cap_l3_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_WDMA_PREV_L4:
		dva = frame->dva_mtnr1_cap_l4_yuv;
		pst_get_size_of_dma_input(&mtnr1_param[param_idx], align, block_w, block_h, size);
		break;

	/* MTNR0 */
	case PARAM_MTNR_RDMA_CUR_L0:
	case PARAM_MTNR_RDMA_PREV_L0:
	case PARAM_MTNR_RDMA_PREV_L0_WGT:
	case PARAM_MTNR_RDMA_SEG_L0:
	case PARAM_MTNR_WDMA_PREV_L0:
	case PARAM_MTNR_WDMA_PREV_L0_WGT:
	case PARAM_MTNR_CONTROL:
	case PARAM_MTNR_STRIPE_INPUT:
	case PARAM_MTNR_CIN_MTNR1_WGT:
	case PARAM_MTNR_COUT_MSNR_L0:
	/* MTNR1 */
	case PARAM_MTNR_CIN_DLFE_WGT:
	case PARAM_MTNR_COUT_MSNR_L1:
	case PARAM_MTNR_COUT_MSNR_L2:
	case PARAM_MTNR_COUT_MSNR_L3:
	case PARAM_MTNR_COUT_MSNR_L4:
	case PARAM_MTNR_COUT_MTNR0_WGT:
	case PARAM_MTNR_COUT_DLFE_CUR:
	case PARAM_MTNR_COUT_DLFE_PREV:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_MTNR);
		pst_blob_inject(pst_get_blob_node_mtnr1(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_mtnr1(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct mtnr_param *preset;

	if (type == PST_HW_IP_SINGLE)
		preset = mtnr1_param_preset;
	else
		preset = mtnr1_param_preset_grp;

	memcpy(mtnr1_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].stripe_input, PARAMETER_MAX_SIZE);

	i = PARAM_MTNR_RDMA_CUR_L4 - PARAM_MTNR_CONTROL;
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_cur_l4, PARAMETER_MAX_SIZE);

	i = PARAM_MTNR_CIN_DLFE_WGT - PARAM_MTNR_CONTROL;
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cin_dlfe_wgt, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cout_msnr_l1, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cout_msnr_l2, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cout_msnr_l3, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cout_msnr_l4, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cout_mtnr0_wgt, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cout_dlfe_cur, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].cout_dlfe_prev, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_cur_l1, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_cur_l2, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_cur_l3, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_prev_l1, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_prev_l2, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_prev_l3, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_prev_l4, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_prev_l1_wgt, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_seg_l1, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_seg_l2, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_seg_l3, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].rdma_seg_l4, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].wdma_prev_l1, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].wdma_prev_l2, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].wdma_prev_l3, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].wdma_prev_l4, PARAMETER_MAX_SIZE);
	memcpy(mtnr1_param[i++], (u32 *)&preset[index].wdma_prev_l1_wgt, PARAMETER_MAX_SIZE);
}

static void pst_set_conf_mtnr1(struct mtnr_param *param, struct is_frame *frame)
{
	frame->kva_mtnr1_rta_info[PLANE_INDEX_CONFIG] = (u64)&mtnr1_size_conf;
}

static void pst_set_param_mtnr1(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MTNR_PARAM; i++) {
		switch (PARAM_MTNR_CONTROL + i) {
		case PARAM_MTNR_RDMA_CUR_L0:
		case PARAM_MTNR_RDMA_PREV_L0:
		case PARAM_MTNR_RDMA_PREV_L0_WGT:
		case PARAM_MTNR_RDMA_SEG_L0:
		case PARAM_MTNR_WDMA_PREV_L0:
		case PARAM_MTNR_WDMA_PREV_L0_WGT:
		case PARAM_MTNR_CONTROL:
		case PARAM_MTNR_STRIPE_INPUT:
		case PARAM_MTNR_CIN_MTNR1_WGT:
		case PARAM_MTNR_COUT_MSNR_L0:
			break;
		default:
			pst_set_param(frame, mtnr1_param[i], PARAM_MTNR_CONTROL + i);
			pst_set_buf_mtnr1(frame, i);
			break;
		}
	}

	pst_set_conf_mtnr1((struct mtnr_param *)mtnr1_param, frame);
}

static void pst_clr_param_mtnr1(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MTNR_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_mtnr1(i), pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_mtnr1(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_mtnr1_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_mtnr1 = {
	.init_param = pst_init_param_mtnr1,
	.set_param = pst_set_param_mtnr1,
	.clr_param = pst_clr_param_mtnr1,
	.set_rta_info = pst_set_rta_info_mtnr1,
	.set_size = pst_set_size_mtnr1,
};

const struct pst_callback_ops *pst_get_hw_mtnr1_cb(void)
{
	return &pst_cb_mtnr1;
}

static int pst_set_hw_mtnr1(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val, DEV_HW_MTNR1, frame_mtnr1, mtnr1_param, &mtnr1_cr_set,
		ARRAY_SIZE(mtnr1_param_preset), result, &pst_cb_mtnr1);
}

static int pst_get_hw_mtnr1(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "MTNR1", ARRAY_SIZE(mtnr1_param_preset), result);
}
