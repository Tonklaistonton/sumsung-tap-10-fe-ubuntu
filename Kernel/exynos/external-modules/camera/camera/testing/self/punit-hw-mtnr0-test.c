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

static int pst_set_hw_mtnr0(const char *val, const struct kernel_param *kp);
static int pst_get_hw_mtnr0(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_mtnr0 = {
	.set = pst_set_hw_mtnr0,
	.get = pst_get_hw_mtnr0,
};
module_param_cb(test_hw_mtnr0, &pablo_param_ops_hw_mtnr0, NULL, 0644);

#define NUM_OF_MTNR_PARAM (PARAM_MTNR_WDMA_PREV_L1_WGT - PARAM_MTNR_CONTROL + 1)

static struct is_frame *frame_mtnr0;
static u32 mtnr0_param[NUM_OF_MTNR_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_MTNR_PARAM];
static struct size_cr_set mtnr0_cr_set;
static struct mtnr0_size_conf_set {
	u32 size;
	struct is_mtnr0_config conf;
} mtnr0_size_conf;

static const struct mtnr_param mtnr0_param_preset_grp[] = {
	/* Param set[0]: cur DMA input */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].cin_mtnr1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].cin_mtnr1_wgt.format = OTF_INPUT_FORMAT_YUV422,
	[0].cin_mtnr1_wgt.bitwidth = 0,
	[0].cin_mtnr1_wgt.order = 0,
	[0].cin_mtnr1_wgt.width = 1920,
	[0].cin_mtnr1_wgt.height = 1440,
	[0].cin_mtnr1_wgt.bayer_crop_offset_x = 0,
	[0].cin_mtnr1_wgt.bayer_crop_offset_y = 0,
	[0].cin_mtnr1_wgt.bayer_crop_width = 1920,
	[0].cin_mtnr1_wgt.bayer_crop_height = 1440,
	[0].cin_mtnr1_wgt.source = 0,
	[0].cin_mtnr1_wgt.physical_width = 0,
	[0].cin_mtnr1_wgt.physical_height = 0,
	[0].cin_mtnr1_wgt.offset_x = 0,
	[0].cin_mtnr1_wgt.offset_y = 0,

	[0].cout_msnr_l0.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_l0.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l0.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l0.order = 2,
	[0].cout_msnr_l0.width = 1920,
	[0].cout_msnr_l0.height = 1440,
	[0].cout_msnr_l0.crop_offset_x = 0,
	[0].cout_msnr_l0.crop_offset_y = 0,
	[0].cout_msnr_l0.crop_width = 0,
	[0].cout_msnr_l0.crop_height = 0,
	[0].cout_msnr_l0.crop_enable = 0,

	[0].rdma_cur_l0.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l0.format = DMA_INOUT_FORMAT_Y,
	[0].rdma_cur_l0.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l0.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l0.plane = 1,
	[0].rdma_cur_l0.width = 1920,
	[0].rdma_cur_l0.height = 1440,
	[0].rdma_cur_l0.dma_crop_offset = 0,
	[0].rdma_cur_l0.dma_crop_width = 1920,
	[0].rdma_cur_l0.dma_crop_height = 1440,
	[0].rdma_cur_l0.bayer_crop_offset_x = 0,
	[0].rdma_cur_l0.bayer_crop_offset_y = 0,
	[0].rdma_cur_l0.bayer_crop_width = 1920,
	[0].rdma_cur_l0.bayer_crop_height = 1440,
	[0].rdma_cur_l0.scene_mode = 0,
	[0].rdma_cur_l0.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l0.stride_plane0 = 1920,
	[0].rdma_cur_l0.stride_plane1 = 1920,
	[0].rdma_cur_l0.stride_plane2 = 1920,
	[0].rdma_cur_l0.v_otf_enable = 0,
	[0].rdma_cur_l0.orientation = 0,
	[0].rdma_cur_l0.strip_mode = 0,
	[0].rdma_cur_l0.overlab_width = 0,
	[0].rdma_cur_l0.strip_count = 0,
	[0].rdma_cur_l0.strip_max_count = 0,
	[0].rdma_cur_l0.sequence_id = 0,
	[0].rdma_cur_l0.sbwc_type = 0,

	[0].rdma_cur_l1.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].rdma_cur_l1.format = DMA_INOUT_FORMAT_Y,
	[0].rdma_cur_l1.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l1.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l1.plane = 1,
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

	[0].rdma_cur_l4.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].rdma_cur_l4.format = DMA_INOUT_FORMAT_Y,
	[0].rdma_cur_l4.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l4.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l4.plane = 1,
	[0].rdma_cur_l4.width = 120, /* 1920 / 16 */
	[0].rdma_cur_l4.height = 90, /* 1440 / 16 */
	[0].rdma_cur_l4.dma_crop_offset = 0,
	[0].rdma_cur_l4.dma_crop_width = 120,
	[0].rdma_cur_l4.dma_crop_height = 90,
	[0].rdma_cur_l4.bayer_crop_offset_x = 0,
	[0].rdma_cur_l4.bayer_crop_offset_y = 0,
	[0].rdma_cur_l4.bayer_crop_width = 120,
	[0].rdma_cur_l4.bayer_crop_height = 90,
	[0].rdma_cur_l4.scene_mode = 0,
	[0].rdma_cur_l4.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l4.stride_plane0 = 120,
	[0].rdma_cur_l4.stride_plane1 = 120,
	[0].rdma_cur_l4.stride_plane2 = 120,
	[0].rdma_cur_l4.v_otf_enable = 0,
	[0].rdma_cur_l4.orientation = 0,
	[0].rdma_cur_l4.strip_mode = 0,
	[0].rdma_cur_l4.overlab_width = 0,
	[0].rdma_cur_l4.strip_count = 0,
	[0].rdma_cur_l4.strip_max_count = 0,
	[0].rdma_cur_l4.sequence_id = 0,
	[0].rdma_cur_l4.sbwc_type = 0,

	[0].rdma_prev_l0.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l0_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_mv_geomatch.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l0.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l0.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l0_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,

	/* Param set[1]:  */
};

static const struct mtnr_param mtnr0_param_preset[] = {
	/* Param set[0]: cur DMA input */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].cin_mtnr1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].cin_mtnr1_wgt.format = OTF_INPUT_FORMAT_YUV422,
	[0].cin_mtnr1_wgt.bitwidth = 0,
	[0].cin_mtnr1_wgt.order = 0,
	[0].cin_mtnr1_wgt.width = 1920,
	[0].cin_mtnr1_wgt.height = 1440,
	[0].cin_mtnr1_wgt.bayer_crop_offset_x = 0,
	[0].cin_mtnr1_wgt.bayer_crop_offset_y = 0,
	[0].cin_mtnr1_wgt.bayer_crop_width = 1920,
	[0].cin_mtnr1_wgt.bayer_crop_height = 1440,
	[0].cin_mtnr1_wgt.source = 0,
	[0].cin_mtnr1_wgt.physical_width = 0,
	[0].cin_mtnr1_wgt.physical_height = 0,
	[0].cin_mtnr1_wgt.offset_x = 0,
	[0].cin_mtnr1_wgt.offset_y = 0,

	[0].cout_msnr_l0.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cout_msnr_l0.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].cout_msnr_l0.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_l0.order = 2,
	[0].cout_msnr_l0.width = 1920,
	[0].cout_msnr_l0.height = 1440,
	[0].cout_msnr_l0.crop_offset_x = 0,
	[0].cout_msnr_l0.crop_offset_y = 0,
	[0].cout_msnr_l0.crop_width = 0,
	[0].cout_msnr_l0.crop_height = 0,
	[0].cout_msnr_l0.crop_enable = 0,

	[0].rdma_cur_l0.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].rdma_cur_l0.format = DMA_INOUT_FORMAT_Y,
	[0].rdma_cur_l0.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l0.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l0.plane = 1,
	[0].rdma_cur_l0.width = 1920,
	[0].rdma_cur_l0.height = 1440,
	[0].rdma_cur_l0.dma_crop_offset = 0,
	[0].rdma_cur_l0.dma_crop_width = 1920,
	[0].rdma_cur_l0.dma_crop_height = 1440,
	[0].rdma_cur_l0.bayer_crop_offset_x = 0,
	[0].rdma_cur_l0.bayer_crop_offset_y = 0,
	[0].rdma_cur_l0.bayer_crop_width = 1920,
	[0].rdma_cur_l0.bayer_crop_height = 1440,
	[0].rdma_cur_l0.scene_mode = 0,
	[0].rdma_cur_l0.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l0.stride_plane0 = 1920,
	[0].rdma_cur_l0.stride_plane1 = 1920,
	[0].rdma_cur_l0.stride_plane2 = 1920,
	[0].rdma_cur_l0.v_otf_enable = 0,
	[0].rdma_cur_l0.orientation = 0,
	[0].rdma_cur_l0.strip_mode = 0,
	[0].rdma_cur_l0.overlab_width = 0,
	[0].rdma_cur_l0.strip_count = 0,
	[0].rdma_cur_l0.strip_max_count = 0,
	[0].rdma_cur_l0.sequence_id = 0,
	[0].rdma_cur_l0.sbwc_type = 0,

	[0].rdma_cur_l1.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].rdma_cur_l1.format = DMA_INOUT_FORMAT_Y,
	[0].rdma_cur_l1.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l1.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l1.plane = 1,
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

	[0].rdma_cur_l4.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].rdma_cur_l4.format = DMA_INOUT_FORMAT_Y,
	[0].rdma_cur_l4.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].rdma_cur_l4.order = DMA_INOUT_ORDER_CrCb,
	[0].rdma_cur_l4.plane = 1,
	[0].rdma_cur_l4.width = 120, /* 1920 / 16 */
	[0].rdma_cur_l4.height = 90, /* 1440 / 16 */
	[0].rdma_cur_l4.dma_crop_offset = 0,
	[0].rdma_cur_l4.dma_crop_width = 120,
	[0].rdma_cur_l4.dma_crop_height = 90,
	[0].rdma_cur_l4.bayer_crop_offset_x = 0,
	[0].rdma_cur_l4.bayer_crop_offset_y = 0,
	[0].rdma_cur_l4.bayer_crop_width = 120,
	[0].rdma_cur_l4.bayer_crop_height = 90,
	[0].rdma_cur_l4.scene_mode = 0,
	[0].rdma_cur_l4.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].rdma_cur_l4.stride_plane0 = 120,
	[0].rdma_cur_l4.stride_plane1 = 120,
	[0].rdma_cur_l4.stride_plane2 = 120,
	[0].rdma_cur_l4.v_otf_enable = 0,
	[0].rdma_cur_l4.orientation = 0,
	[0].rdma_cur_l4.strip_mode = 0,
	[0].rdma_cur_l4.overlab_width = 0,
	[0].rdma_cur_l4.strip_count = 0,
	[0].rdma_cur_l4.strip_max_count = 0,
	[0].rdma_cur_l4.sequence_id = 0,
	[0].rdma_cur_l4.sbwc_type = 0,

	[0].rdma_prev_l0.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_prev_l0_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_mv_geomatch.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].rdma_seg_l0.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l0.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].wdma_prev_l0_wgt.cmd = OTF_INPUT_COMMAND_DISABLE,

	/* Param set[1]:  */
};

static DECLARE_BITMAP(result, ARRAY_SIZE(mtnr0_param_preset));

static void pst_set_size_mtnr0(void *in_param, void *out_param)
{
	struct mtnr_param *p = (struct mtnr_param *)mtnr0_param;
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

static enum pst_blob_node pst_get_blob_node_mtnr0(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_MTNR_CONTROL + idx) {
	case PARAM_MTNR_WDMA_PREV_L0_WGT:
		bn = PST_BLOB_MTNR_L0_W;
		break;
	case PARAM_MTNR_WDMA_PREV_L0:
		bn = PST_BLOB_MTNR_L0;
		break;
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_mtnr0(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	u32 block_w = MTNR0_COMP_BLOCK_WIDTH;
	u32 block_h = MTNR0_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_MTNR_CONTROL + param_idx) {
	case PARAM_MTNR_RDMA_CUR_L0:
		dva = frame->dvaddr_buffer;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_CUR_L4:
		dva = frame->dva_mtnr1_out_cur_l4_yuv;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_PREV_L0:
		dva = frame->dva_mtnr0_out_prev_l0_yuv;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_PREV_L0_WGT:
		dva = frame->dva_mtnr0_out_prev_l0_wgt;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_MV_GEOMATCH:
		dva = frame->dva_mtnr0_out_mv_geomatch;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_RDMA_SEG_L0:
		dva = frame->dva_mtnr0_out_seg_l0;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_WDMA_PREV_L0:
		dva = frame->dva_mtnr0_cap_l0_yuv;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_WDMA_PREV_L0_WGT:
		dva = frame->dva_mtnr0_cap_l0_wgt;
		pst_get_size_of_dma_input(&mtnr0_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MTNR_CONTROL:
	case PARAM_MTNR_STRIPE_INPUT:
	case PARAM_MTNR_CIN_MTNR1_WGT:
	case PARAM_MTNR_COUT_MSNR_L0:
	case PARAM_MTNR_RDMA_CUR_L1:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_MTNR);
		pst_blob_inject(pst_get_blob_node_mtnr0(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_mtnr0(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct mtnr_param *preset;

	if (type == PST_HW_IP_SINGLE)
		preset = mtnr0_param_preset;
	else
		preset = mtnr0_param_preset_grp;

	memcpy(mtnr0_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].stripe_input, PARAMETER_MAX_SIZE);

	memcpy(mtnr0_param[i++], (u32 *)&preset[index].cin_mtnr1_wgt, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].cout_msnr_l0, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].rdma_cur_l0, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].rdma_cur_l4, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].rdma_prev_l0, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].rdma_prev_l0_wgt, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].rdma_mv_geomatch, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].rdma_seg_l0, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].wdma_prev_l0, PARAMETER_MAX_SIZE);
	memcpy(mtnr0_param[i++], (u32 *)&preset[index].wdma_prev_l0_wgt, PARAMETER_MAX_SIZE);

	i = PARAM_MTNR_RDMA_CUR_L1 - PARAM_MTNR_CONTROL;
	memcpy(mtnr0_param[i], (u32 *)&preset[index].rdma_cur_l1, PARAMETER_MAX_SIZE);
}

static void pst_set_conf_mtnr0(struct mtnr_param *param, struct is_frame *frame)
{
	frame->kva_mtnr0_rta_info[PLANE_INDEX_CONFIG] = (u64)&mtnr0_size_conf;
}

static void pst_set_param_mtnr0(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MTNR_PARAM; i++) {
		switch (PARAM_MTNR_CONTROL + i) {
		/* MTNR0 param */
		case PARAM_MTNR_RDMA_CUR_L0:
		case PARAM_MTNR_RDMA_CUR_L4:
		case PARAM_MTNR_RDMA_PREV_L0:
		case PARAM_MTNR_RDMA_PREV_L0_WGT:
		case PARAM_MTNR_RDMA_MV_GEOMATCH:
		case PARAM_MTNR_RDMA_SEG_L0:
		case PARAM_MTNR_WDMA_PREV_L0:
		case PARAM_MTNR_WDMA_PREV_L0_WGT:
		case PARAM_MTNR_CONTROL:
		case PARAM_MTNR_STRIPE_INPUT:
		case PARAM_MTNR_CIN_MTNR1_WGT:
		case PARAM_MTNR_COUT_MSNR_L0:
			pst_set_param(frame, mtnr0_param[i], PARAM_MTNR_CONTROL + i);
			pst_set_buf_mtnr0(frame, i);
			break;
		/* MTNR1 param */
		case PARAM_MTNR_RDMA_CUR_L1:
			pst_set_param(frame, mtnr0_param[i], PARAM_MTNR_CONTROL + i);
			pst_set_buf_mtnr0(frame, i);
			break;
		default:
			break;
		}
	}

	pst_set_conf_mtnr0((struct mtnr_param *)mtnr0_param, frame);
}

static void pst_clr_param_mtnr0(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MTNR_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_mtnr0(i), pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_mtnr0(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_mtnr0_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_mtnr0 = {
	.init_param = pst_init_param_mtnr0,
	.set_param = pst_set_param_mtnr0,
	.clr_param = pst_clr_param_mtnr0,
	.set_rta_info = pst_set_rta_info_mtnr0,
	.set_size = pst_set_size_mtnr0,
};

const struct pst_callback_ops *pst_get_hw_mtnr0_cb(void)
{
	return &pst_cb_mtnr0;
}

static int pst_set_hw_mtnr0(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val, DEV_HW_MTNR0, frame_mtnr0, mtnr0_param, &mtnr0_cr_set,
		ARRAY_SIZE(mtnr0_param_preset), result, &pst_cb_mtnr0);
}

static int pst_get_hw_mtnr0(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "MTNR", ARRAY_SIZE(mtnr0_param_preset), result);
}
