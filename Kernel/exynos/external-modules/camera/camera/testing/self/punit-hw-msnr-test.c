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

static int pst_set_hw_msnr(const char *val, const struct kernel_param *kp);
static int pst_get_hw_msnr(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_msnr = {
	.set = pst_set_hw_msnr,
	.get = pst_get_hw_msnr,
};
module_param_cb(test_hw_msnr, &pablo_param_ops_hw_msnr, NULL, 0644);

#define NUM_OF_MSNR_PARAM (PARAM_MSNR_WDMA_LME - PARAM_MSNR_CONTROL + 1)

static struct is_frame *frame_msnr;
static u32 msnr_param[NUM_OF_MSNR_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_MSNR_PARAM];
static struct size_cr_set msnr_cr_set;
static struct msnr_size_conf_set {
	u32 size;
	struct is_msnr_config conf;
} msnr_size_conf;

static const struct msnr_param msnr_param_preset_grp[] = {
	/* Param set[0]: cur DMA input */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].cin_msnr_l0.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l0.format = OTF_INPUT_FORMAT_Y,
	[0].cin_msnr_l0.bitwidth = 0,
	[0].cin_msnr_l0.order = 0,
	[0].cin_msnr_l0.width = 1920,
	[0].cin_msnr_l0.height = 1440,
	[0].cin_msnr_l0.bayer_crop_offset_x = 0,
	[0].cin_msnr_l0.bayer_crop_offset_y = 0,
	[0].cin_msnr_l0.bayer_crop_width = 1920,
	[0].cin_msnr_l0.bayer_crop_height = 1440,
	[0].cin_msnr_l0.source = 0,
	[0].cin_msnr_l0.physical_width = 0,
	[0].cin_msnr_l0.physical_height = 0,
	[0].cin_msnr_l0.offset_x = 0,
	[0].cin_msnr_l0.offset_y = 0,

	[0].cin_msnr_l1.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l1.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l1.bitwidth = 0,
	[0].cin_msnr_l1.order = 0,
	[0].cin_msnr_l1.width = 1920 / 2,
	[0].cin_msnr_l1.height = 1440 / 2,
	[0].cin_msnr_l1.bayer_crop_offset_x = 0,
	[0].cin_msnr_l1.bayer_crop_offset_y = 0,
	[0].cin_msnr_l1.bayer_crop_width = 1920 / 2,
	[0].cin_msnr_l1.bayer_crop_height = 1440 / 2,
	[0].cin_msnr_l1.source = 0,
	[0].cin_msnr_l1.physical_width = 0,
	[0].cin_msnr_l1.physical_height = 0,
	[0].cin_msnr_l1.offset_x = 0,
	[0].cin_msnr_l1.offset_y = 0,

	[0].cin_msnr_l2.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l2.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l2.bitwidth = 0,
	[0].cin_msnr_l2.order = 0,
	[0].cin_msnr_l2.width = 1920 / 4,
	[0].cin_msnr_l2.height = 1440 / 4,
	[0].cin_msnr_l2.bayer_crop_offset_x = 0,
	[0].cin_msnr_l2.bayer_crop_offset_y = 0,
	[0].cin_msnr_l2.bayer_crop_width = 1920 / 4,
	[0].cin_msnr_l2.bayer_crop_height = 1440 / 4,
	[0].cin_msnr_l2.source = 0,
	[0].cin_msnr_l2.physical_width = 0,
	[0].cin_msnr_l2.physical_height = 0,
	[0].cin_msnr_l2.offset_x = 0,
	[0].cin_msnr_l2.offset_y = 0,

	[0].cin_msnr_l3.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l3.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l3.bitwidth = 0,
	[0].cin_msnr_l3.order = 0,
	[0].cin_msnr_l3.width = 1920 / 8,
	[0].cin_msnr_l3.height = 1440 / 8,
	[0].cin_msnr_l3.bayer_crop_offset_x = 0,
	[0].cin_msnr_l3.bayer_crop_offset_y = 0,
	[0].cin_msnr_l3.bayer_crop_width = 1920 / 8,
	[0].cin_msnr_l3.bayer_crop_height = 1440 / 8,
	[0].cin_msnr_l3.source = 0,
	[0].cin_msnr_l3.physical_width = 0,
	[0].cin_msnr_l3.physical_height = 0,
	[0].cin_msnr_l3.offset_x = 0,
	[0].cin_msnr_l3.offset_y = 0,

	[0].cin_msnr_l4.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l4.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l4.bitwidth = 0,
	[0].cin_msnr_l4.order = 0,
	[0].cin_msnr_l4.width = 1920 / 16,
	[0].cin_msnr_l4.height = 1440 / 16,
	[0].cin_msnr_l4.bayer_crop_offset_x = 0,
	[0].cin_msnr_l4.bayer_crop_offset_y = 0,
	[0].cin_msnr_l4.bayer_crop_width = 1920 / 16,
	[0].cin_msnr_l4.bayer_crop_height = 1440 / 16,
	[0].cin_msnr_l4.source = 0,
	[0].cin_msnr_l4.physical_width = 0,
	[0].cin_msnr_l4.physical_height = 0,
	[0].cin_msnr_l4.offset_x = 0,
	[0].cin_msnr_l4.offset_y = 0,

	[0].cout_msnr_yuv.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_yuv.format = OTF_OUTPUT_FORMAT_YUV444,
	[0].cout_msnr_yuv.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_yuv.order = 2,
	[0].cout_msnr_yuv.width = 1920,
	[0].cout_msnr_yuv.height = 1440,
	[0].cout_msnr_yuv.crop_offset_x = 0,
	[0].cout_msnr_yuv.crop_offset_y = 0,
	[0].cout_msnr_yuv.crop_width = 0,
	[0].cout_msnr_yuv.crop_height = 0,
	[0].cout_msnr_yuv.crop_enable = 0,

	[0].cout_msnr_stat.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_stat.format = OTF_OUTPUT_FORMAT_Y,
	[0].cout_msnr_stat.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_stat.order = 2,
	[0].cout_msnr_stat.width = 1920,
	[0].cout_msnr_stat.height = 1440,
	[0].cout_msnr_stat.crop_offset_x = 0,
	[0].cout_msnr_stat.crop_offset_y = 0,
	[0].cout_msnr_stat.crop_width = 0,
	[0].cout_msnr_stat.crop_height = 0,
	[0].cout_msnr_stat.crop_enable = 0,

	[0].wdma_lme.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	/* Param set[1]:  */
};

static const struct msnr_param msnr_param_preset[] = {
	/* Param set[0]: cur DMA input */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_START,

	[0].cin_msnr_l0.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l0.format = OTF_INPUT_FORMAT_Y,
	[0].cin_msnr_l0.bitwidth = 0,
	[0].cin_msnr_l0.order = 0,
	[0].cin_msnr_l0.width = 1920,
	[0].cin_msnr_l0.height = 1440,
	[0].cin_msnr_l0.bayer_crop_offset_x = 0,
	[0].cin_msnr_l0.bayer_crop_offset_y = 0,
	[0].cin_msnr_l0.bayer_crop_width = 1920,
	[0].cin_msnr_l0.bayer_crop_height = 1440,
	[0].cin_msnr_l0.source = 0,
	[0].cin_msnr_l0.physical_width = 0,
	[0].cin_msnr_l0.physical_height = 0,
	[0].cin_msnr_l0.offset_x = 0,
	[0].cin_msnr_l0.offset_y = 0,

	[0].cin_msnr_l1.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l1.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l1.bitwidth = 0,
	[0].cin_msnr_l1.order = 0,
	[0].cin_msnr_l1.width = 1920 / 2,
	[0].cin_msnr_l1.height = 1440 / 2,
	[0].cin_msnr_l1.bayer_crop_offset_x = 0,
	[0].cin_msnr_l1.bayer_crop_offset_y = 0,
	[0].cin_msnr_l1.bayer_crop_width = 1920 / 2,
	[0].cin_msnr_l1.bayer_crop_height = 1440 / 2,
	[0].cin_msnr_l1.source = 0,
	[0].cin_msnr_l1.physical_width = 0,
	[0].cin_msnr_l1.physical_height = 0,
	[0].cin_msnr_l1.offset_x = 0,
	[0].cin_msnr_l1.offset_y = 0,

	[0].cin_msnr_l2.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l2.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l2.bitwidth = 0,
	[0].cin_msnr_l2.order = 0,
	[0].cin_msnr_l2.width = 1920 / 4,
	[0].cin_msnr_l2.height = 1440 / 4,
	[0].cin_msnr_l2.bayer_crop_offset_x = 0,
	[0].cin_msnr_l2.bayer_crop_offset_y = 0,
	[0].cin_msnr_l2.bayer_crop_width = 1920 / 4,
	[0].cin_msnr_l2.bayer_crop_height = 1440 / 4,
	[0].cin_msnr_l2.source = 0,
	[0].cin_msnr_l2.physical_width = 0,
	[0].cin_msnr_l2.physical_height = 0,
	[0].cin_msnr_l2.offset_x = 0,
	[0].cin_msnr_l2.offset_y = 0,

	[0].cin_msnr_l3.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l3.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l3.bitwidth = 0,
	[0].cin_msnr_l3.order = 0,
	[0].cin_msnr_l3.width = 1920 / 8,
	[0].cin_msnr_l3.height = 1440 / 8,
	[0].cin_msnr_l3.bayer_crop_offset_x = 0,
	[0].cin_msnr_l3.bayer_crop_offset_y = 0,
	[0].cin_msnr_l3.bayer_crop_width = 1920 / 8,
	[0].cin_msnr_l3.bayer_crop_height = 1440 / 8,
	[0].cin_msnr_l3.source = 0,
	[0].cin_msnr_l3.physical_width = 0,
	[0].cin_msnr_l3.physical_height = 0,
	[0].cin_msnr_l3.offset_x = 0,
	[0].cin_msnr_l3.offset_y = 0,

	[0].cin_msnr_l4.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].cin_msnr_l4.format = OTF_INPUT_FORMAT_YUV444,
	[0].cin_msnr_l4.bitwidth = 0,
	[0].cin_msnr_l4.order = 0,
	[0].cin_msnr_l4.width = 1920 / 16,
	[0].cin_msnr_l4.height = 1440 / 16,
	[0].cin_msnr_l4.bayer_crop_offset_x = 0,
	[0].cin_msnr_l4.bayer_crop_offset_y = 0,
	[0].cin_msnr_l4.bayer_crop_width = 1920 / 16,
	[0].cin_msnr_l4.bayer_crop_height = 1440 / 16,
	[0].cin_msnr_l4.source = 0,
	[0].cin_msnr_l4.physical_width = 0,
	[0].cin_msnr_l4.physical_height = 0,
	[0].cin_msnr_l4.offset_x = 0,
	[0].cin_msnr_l4.offset_y = 0,

	[0].cout_msnr_yuv.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].cout_msnr_yuv.format = OTF_OUTPUT_FORMAT_YUV444,
	[0].cout_msnr_yuv.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_yuv.order = 2,
	[0].cout_msnr_yuv.width = 1920,
	[0].cout_msnr_yuv.height = 1440,
	[0].cout_msnr_yuv.crop_offset_x = 0,
	[0].cout_msnr_yuv.crop_offset_y = 0,
	[0].cout_msnr_yuv.crop_width = 0,
	[0].cout_msnr_yuv.crop_height = 0,
	[0].cout_msnr_yuv.crop_enable = 0,

	[0].cout_msnr_stat.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cout_msnr_stat.format = OTF_OUTPUT_FORMAT_Y,
	[0].cout_msnr_stat.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].cout_msnr_stat.order = 2,
	[0].cout_msnr_stat.width = 1920,
	[0].cout_msnr_stat.height = 1440,
	[0].cout_msnr_stat.crop_offset_x = 0,
	[0].cout_msnr_stat.crop_offset_y = 0,
	[0].cout_msnr_stat.crop_width = 0,
	[0].cout_msnr_stat.crop_height = 0,
	[0].cout_msnr_stat.crop_enable = 0,

	[0].wdma_lme.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	/* Param set[1]:  */
};

static DECLARE_BITMAP(result, ARRAY_SIZE(msnr_param_preset));

static void pst_set_size_msnr(void *in_param, void *out_param)
{
	struct msnr_param *p = (struct msnr_param *)msnr_param;
	struct param_size_byrp2yuvp *in = (struct param_size_byrp2yuvp *)in_param;
	struct param_size_byrp2yuvp *out = (struct param_size_byrp2yuvp *)out_param;

	if (!in || !out)
		return;

	p->cin_msnr_l0.width = in->w_mcfp;
	p->cin_msnr_l0.height = in->h_mcfp;
	p->cin_msnr_l1.width = in->w_mcfp / 2;
	p->cin_msnr_l1.height = in->h_mcfp / 2;
	p->cin_msnr_l2.width = in->w_mcfp / 4;
	p->cin_msnr_l2.height = in->h_mcfp / 4;
	p->cin_msnr_l3.width = in->w_mcfp / 8;
	p->cin_msnr_l3.height = in->h_mcfp / 8;
	p->cin_msnr_l4.width = in->w_mcfp / 16;
	p->cin_msnr_l4.height = in->h_mcfp / 16;

	p->cout_msnr_yuv.width = out->w_mcfp;
	p->cout_msnr_yuv.height = out->h_mcfp;
}

static enum pst_blob_node pst_get_blob_node_msnr(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_MSNR_CONTROL + idx) {
	case PARAM_MSNR_WDMA_LME:
		bn = PST_BLOB_MSNR_LME;
		break;
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_msnr(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	u32 block_w = MSNR_COMP_BLOCK_WIDTH;
	u32 block_h = MSNR_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_MSNR_CONTROL + param_idx) {
	case PARAM_MSNR_WDMA_LME:
		dva = frame->dva_msnr_cap_lme;
		pst_get_size_of_dma_input(&msnr_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MSNR_CONTROL:
	case PARAM_MSNR_STRIPE_INPUT:
	case PARAM_MSNR_CIN_L0:
	case PARAM_MSNR_CIN_L1:
	case PARAM_MSNR_CIN_L2:
	case PARAM_MSNR_CIN_L3:
	case PARAM_MSNR_CIN_L4:
	case PARAM_MSNR_COUT_YUV:
	case PARAM_MSNR_COUT_STAT:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_MSNR);
		pst_blob_inject(pst_get_blob_node_msnr(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_msnr(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct msnr_param *preset;

	if (type == PST_HW_IP_SINGLE)
		preset = msnr_param_preset;
	else
		preset = msnr_param_preset_grp;

	memcpy(msnr_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].stripe_input, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].cin_msnr_l0, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].cin_msnr_l1, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].cin_msnr_l2, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].cin_msnr_l3, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].cin_msnr_l4, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].cout_msnr_yuv, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].cout_msnr_stat, PARAMETER_MAX_SIZE);
	memcpy(msnr_param[i++], (u32 *)&preset[index].wdma_lme, PARAMETER_MAX_SIZE);
}

static void pst_set_conf_msnr(struct msnr_param *param, struct is_frame *frame)
{
	frame->kva_msnr_rta_info[PLANE_INDEX_CONFIG] = (u64)&msnr_size_conf;
}

static void pst_set_param_msnr(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MSNR_PARAM; i++) {
		pst_set_param(frame, msnr_param[i], PARAM_MSNR_CONTROL + i);
		pst_set_buf_msnr(frame, i);
	}

	pst_set_conf_msnr((struct msnr_param *)msnr_param, frame);
}

static void pst_clr_param_msnr(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MSNR_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_msnr(i), pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_msnr(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_msnr_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_msnr = {
	.init_param = pst_init_param_msnr,
	.set_param = pst_set_param_msnr,
	.clr_param = pst_clr_param_msnr,
	.set_rta_info = pst_set_rta_info_msnr,
	.set_size = pst_set_size_msnr,
};

const struct pst_callback_ops *pst_get_hw_msnr_cb(void)
{
	return &pst_cb_msnr;
}

static int pst_set_hw_msnr(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val, DEV_HW_MSNR, frame_msnr, msnr_param, &msnr_cr_set,
		ARRAY_SIZE(msnr_param_preset), result, &pst_cb_msnr);
}

static int pst_get_hw_msnr(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "MSNR", ARRAY_SIZE(msnr_param_preset), result);
}
