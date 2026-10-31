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

#include "punit-test-hw-ip.h"
#include "punit-test-file-io.h"
#include "punit-hw-mlsc-self-test.h"

static int pst_set_hw_mlsc(const char *val, const struct kernel_param *kp);
static int pst_get_hw_mlsc(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_mlsc = {
	.set = pst_set_hw_mlsc,
	.get = pst_get_hw_mlsc,
};

#define MLSC_PARAM_IDX(param) (param - PARAM_MLSC_CONTROL)

module_param_cb(test_hw_mlsc, &pablo_param_ops_hw_mlsc, NULL, 0644);

static struct is_frame *frame_mlsc;
static u32 mlsc_param[NUM_OF_MLSC_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_MLSC_PARAM];
static struct size_cr_set mlsc_cr_set;

static const enum pst_blob_node param_to_blob_mlsc[NUM_OF_MLSC_PARAM] = {
	[0 ... NUM_OF_MLSC_PARAM - 1] = PST_BLOB_NODE_MAX,
	[MLSC_PARAM_IDX(PARAM_MLSC_DMA_INPUT)] = PST_BLOB_MLSC_DMA_INPUT,
	[MLSC_PARAM_IDX(PARAM_MLSC_YUV444)] = PST_BLOB_MLSC_YUV444,
	[MLSC_PARAM_IDX(PARAM_MLSC_GLPG0)] = PST_BLOB_MLSC_GLPG0,
	[MLSC_PARAM_IDX(PARAM_MLSC_GLPG1)] = PST_BLOB_MLSC_GLPG1,
	[MLSC_PARAM_IDX(PARAM_MLSC_GLPG2)] = PST_BLOB_MLSC_GLPG2,
	[MLSC_PARAM_IDX(PARAM_MLSC_GLPG3)] = PST_BLOB_MLSC_GLPG3,
	[MLSC_PARAM_IDX(PARAM_MLSC_GLPG4)] = PST_BLOB_MLSC_GLPG4,
	[MLSC_PARAM_IDX(PARAM_MLSC_SVHIST)] = PST_BLOB_MLSC_SVHIST,
	[MLSC_PARAM_IDX(PARAM_MLSC_LMEDS)] = PST_BLOB_MLSC_LME_DS,
	[MLSC_PARAM_IDX(PARAM_MLSC_FDPIG)] = PST_BLOB_MLSC_FDPIG,
	[MLSC_PARAM_IDX(PARAM_MLSC_CAV)] = PST_BLOB_MLSC_CAV,
};

static const struct mlsc_param mlsc_param_preset[] = {
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].otf_input.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].otf_input.format = DMA_INOUT_FORMAT_BAYER,
	[0].otf_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].otf_input.order = 0,
	[0].otf_input.width = 320,
	[0].otf_input.height = 240,
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 320,
	[0].otf_input.bayer_crop_height = 240,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 320,
	[0].otf_input.physical_height = 240,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].dma_input.format = DMA_INOUT_FORMAT_YUV422_PACKED,
	[0].dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_10BIT,
	[0].dma_input.order = 1,
	[0].dma_input.plane = 2,
	[0].dma_input.width = 320,
	[0].dma_input.height = 240,
	[0].dma_input.dma_crop_offset = 0,
	[0].dma_input.dma_crop_width = 320,
	[0].dma_input.dma_crop_height = 240,
	[0].dma_input.bayer_crop_offset_x = 0,
	[0].dma_input.bayer_crop_offset_y = 0,
	[0].dma_input.bayer_crop_width = 320,
	[0].dma_input.bayer_crop_height = 240,
	[0].dma_input.scene_mode = 0,
	[0].dma_input.msb = DMA_INOUT_BIT_WIDTH_10BIT - 1,
	[0].dma_input.stride_plane0 = 320,
	[0].dma_input.stride_plane1 = 320,
	[0].dma_input.stride_plane2 = 320,
	[0].dma_input.v_otf_enable = 0,
	[0].dma_input.orientation = 0,
	[0].dma_input.strip_mode = 0,
	[0].dma_input.overlab_width = 0,
	[0].dma_input.strip_count = 0,
	[0].dma_input.strip_max_count = 0,
	[0].dma_input.sequence_id = 0,
	[0].dma_input.sbwc_type = 0,

	[0].yuv.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].yuv.format = DMA_INOUT_FORMAT_YUV444,
	[0].yuv.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].yuv.order = 1,
	[0].yuv.plane = 3,
	[0].yuv.width = 320,
	[0].yuv.height = 240,
	[0].yuv.dma_crop_offset_x = 0,
	[0].yuv.dma_crop_offset_y = 0,
	[0].yuv.dma_crop_width = 320,
	[0].yuv.dma_crop_height = 240,
	[0].yuv.crop_enable = 0,
	[0].yuv.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].yuv.stride_plane0 = 320,
	[0].yuv.stride_plane1 = 320,
	[0].yuv.stride_plane2 = 320,
	[0].yuv.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].yuv.sbwc_type = NONE,

	[0].glpg[0].cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].glpg[0].format = DMA_INOUT_FORMAT_Y,
	[0].glpg[0].bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].glpg[0].order = 1,
	[0].glpg[0].plane = 1,
	[0].glpg[0].width = 320,
	[0].glpg[0].height = 240,
	[0].glpg[0].dma_crop_offset_x = 0,
	[0].glpg[0].dma_crop_offset_y = 0,
	[0].glpg[0].dma_crop_width = 320,
	[0].glpg[0].dma_crop_height = 240,
	[0].glpg[0].crop_enable = 0,
	[0].glpg[0].msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].glpg[0].stride_plane0 = 320,
	[0].glpg[0].stride_plane1 = 320,
	[0].glpg[0].stride_plane2 = 320,
	[0].glpg[0].v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].glpg[0].sbwc_type = NONE,

	[0].glpg[1].cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].glpg[1].format = DMA_INOUT_FORMAT_YUV444,
	[0].glpg[1].bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].glpg[1].order = 1,
	[0].glpg[1].plane = 3,
	[0].glpg[1].width = 160,
	[0].glpg[1].height = 120,
	[0].glpg[1].dma_crop_offset_x = 0,
	[0].glpg[1].dma_crop_offset_y = 0,
	[0].glpg[1].dma_crop_width = 160,
	[0].glpg[1].dma_crop_height = 120,
	[0].glpg[1].crop_enable = 0,
	[0].glpg[1].msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].glpg[1].stride_plane0 = 160,
	[0].glpg[1].stride_plane1 = 160,
	[0].glpg[1].stride_plane2 = 160,
	[0].glpg[1].v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].glpg[1].sbwc_type = NONE,

	[0].glpg[2].cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].glpg[2].format = DMA_INOUT_FORMAT_YUV444,
	[0].glpg[2].bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].glpg[2].order = 1,
	[0].glpg[2].plane = 3,
	[0].glpg[2].width = 80,
	[0].glpg[2].height = 60,
	[0].glpg[2].dma_crop_offset_x = 0,
	[0].glpg[2].dma_crop_offset_y = 0,
	[0].glpg[2].dma_crop_width = 80,
	[0].glpg[2].dma_crop_height = 60,
	[0].glpg[2].crop_enable = 0,
	[0].glpg[2].msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].glpg[2].stride_plane0 = 80,
	[0].glpg[2].stride_plane1 = 80,
	[0].glpg[2].stride_plane2 = 80,
	[0].glpg[2].v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].glpg[2].sbwc_type = NONE,

	[0].glpg[3].cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].glpg[3].format = DMA_INOUT_FORMAT_YUV444,
	[0].glpg[3].bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].glpg[3].order = 1,
	[0].glpg[3].plane = 3,
	[0].glpg[3].width = 40,
	[0].glpg[3].height = 30,
	[0].glpg[3].dma_crop_offset_x = 0,
	[0].glpg[3].dma_crop_offset_y = 0,
	[0].glpg[3].dma_crop_width = 40,
	[0].glpg[3].dma_crop_height = 30,
	[0].glpg[3].crop_enable = 0,
	[0].glpg[3].msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].glpg[3].stride_plane0 = 40,
	[0].glpg[3].stride_plane1 = 40,
	[0].glpg[3].stride_plane2 = 40,
	[0].glpg[3].v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].glpg[3].sbwc_type = NONE,

	[0].glpg[4].cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].glpg[4].format = DMA_INOUT_FORMAT_YUV444,
	[0].glpg[4].bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[0].glpg[4].order = 1,
	[0].glpg[4].plane = 3,
	[0].glpg[4].width = 20,
	[0].glpg[4].height = 16,
	[0].glpg[4].dma_crop_offset_x = 0,
	[0].glpg[4].dma_crop_offset_y = 0,
	[0].glpg[4].dma_crop_width = 20,
	[0].glpg[4].dma_crop_height = 16,
	[0].glpg[4].crop_enable = 0,
	[0].glpg[4].msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[0].glpg[4].stride_plane0 = 20,
	[0].glpg[4].stride_plane1 = 20,
	[0].glpg[4].stride_plane2 = 20,
	[0].glpg[4].v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].glpg[4].sbwc_type = NONE,

	[0].svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	[0].svhist.format = DMA_INOUT_FORMAT_SVHIST,
	[0].svhist.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].svhist.order = 1,
	[0].svhist.plane = 1,
	[0].svhist.width = 320,
	[0].svhist.height = 240,
	[0].svhist.dma_crop_offset_x = 0,
	[0].svhist.dma_crop_offset_y = 0,
	[0].svhist.dma_crop_width = 320,
	[0].svhist.dma_crop_height = 240,
	[0].svhist.crop_enable = 0,
	[0].svhist.msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
	[0].svhist.stride_plane0 = 320,
	[0].svhist.stride_plane1 = 320,
	[0].svhist.stride_plane2 = 320,
	[0].svhist.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].svhist.sbwc_type = NONE,

	[0].lme_ds.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].lme_ds.format = DMA_INOUT_FORMAT_Y,
	[0].lme_ds.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].lme_ds.order = 1,
	[0].lme_ds.plane = 1,
	[0].lme_ds.width = 320,
	[0].lme_ds.height = 240,
	[0].lme_ds.dma_crop_offset_x = 0,
	[0].lme_ds.dma_crop_offset_y = 0,
	[0].lme_ds.dma_crop_width = 320,
	[0].lme_ds.dma_crop_height = 240,
	[0].lme_ds.crop_enable = 0,
	[0].lme_ds.msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
	[0].lme_ds.stride_plane0 = 320,
	[0].lme_ds.stride_plane1 = 320,
	[0].lme_ds.stride_plane2 = 320,
	[0].lme_ds.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].lme_ds.sbwc_type = NONE,

	[0].fdpig.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	[0].fdpig.format = DMA_INOUT_FORMAT_YUV420,
	[0].fdpig.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].fdpig.order = 1,
	[0].fdpig.plane = 2,
	[0].fdpig.width = 320,
	[0].fdpig.height = 240,
	[0].fdpig.dma_crop_offset_x = 0,
	[0].fdpig.dma_crop_offset_y = 0,
	[0].fdpig.dma_crop_width = 320,
	[0].fdpig.dma_crop_height = 240,
	[0].fdpig.crop_enable = 0,
	[0].fdpig.msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
	[0].fdpig.stride_plane0 = 320,
	[0].fdpig.stride_plane1 = 320,
	[0].fdpig.stride_plane2 = 320,
	[0].fdpig.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].fdpig.sbwc_type = NONE,

	[0].cav.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	[0].cav.format = DMA_INOUT_FORMAT_YUV420,
	[0].cav.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].cav.order = 1,
	[0].cav.plane = 2,
	[0].cav.width = 320,
	[0].cav.height = 240,
	[0].cav.dma_crop_offset_x = 0,
	[0].cav.dma_crop_offset_y = 0,
	[0].cav.dma_crop_width = 320,
	[0].cav.dma_crop_height = 240,
	[0].cav.crop_enable = 0,
	[0].cav.msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
	[0].cav.stride_plane0 = 320,
	[0].cav.stride_plane1 = 320,
	[0].cav.stride_plane2 = 320,
	[0].cav.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cav.sbwc_type = NONE,
};

static DECLARE_BITMAP(preset_test_result, ARRAY_SIZE(mlsc_param_preset));

size_t pst_get_preset_test_size_mlsc(void)
{
	return ARRAY_SIZE(mlsc_param_preset);
}

unsigned long *pst_get_preset_test_result_buf_mlsc(void)
{
	return preset_test_result;
}

const enum pst_blob_node *pst_get_blob_node_mlsc(void)
{
	return param_to_blob_mlsc;
}

static void pst_set_buf_mlsc(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 16;
	u32 block_w = MLSC_COMP_BLOCK_WIDTH;
	u32 block_h = MLSC_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_MLSC_CONTROL + param_idx) {
	case PARAM_MLSC_DMA_INPUT:
		dva = frame->dvaddr_buffer;
		pst_get_size_of_dma_input(&mlsc_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MLSC_YUV444:
		dva = frame->dva_mlsc_yuv444;
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_GLPG0:
		dva = frame->dva_mlsc_glpg[0];
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_GLPG1:
		dva = frame->dva_mlsc_glpg[1];
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_GLPG2:
		dva = frame->dva_mlsc_glpg[2];
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_GLPG3:
		dva = frame->dva_mlsc_glpg[3];
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_GLPG4:
		dva = frame->dva_mlsc_glpg[4];
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_SVHIST:
		dva = frame->dva_mlsc_svhist;
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_LMEDS:
		dva = frame->dva_mlsc_lmeds;
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_FDPIG:
		dva = frame->dva_mlsc_fdpig;
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	case PARAM_MLSC_CAV:
		dva = frame->dva_mlsc_cav;
		pst_get_size_of_dma_output(&mlsc_param[param_idx], align, size);
		break;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_MLSC0);
		pst_blob_inject(param_to_blob_mlsc[param_idx], pb[param_idx]);
	}
}

void pst_init_param_mlsc(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;

	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].otf_input, PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].dma_input, PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].yuv, PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].glpg[0], PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].glpg[1], PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].glpg[2], PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].glpg[3], PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].glpg[4], PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].svhist, PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].lme_ds, PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].fdpig, PARAMETER_MAX_SIZE);
	memcpy(mlsc_param[i++], (u32 *)&mlsc_param_preset[index].cav, PARAMETER_MAX_SIZE);
}

static void pst_set_param_mlsc(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MLSC_PARAM; i++) {
		pst_set_param(frame, mlsc_param[i], PARAM_MLSC_CONTROL + i);
		pst_set_buf_mlsc(frame, i);
	}
}

static void pst_clr_param_mlsc(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MLSC_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(param_to_blob_mlsc[i], pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_mlsc(struct is_frame *frame, struct size_cr_set *cr_set)
{
}

static const struct pst_callback_ops pst_cb_mlsc = {
	.init_param = pst_init_param_mlsc,
	.set_param = pst_set_param_mlsc,
	.clr_param = pst_clr_param_mlsc,
	.set_rta_info = pst_set_rta_info_mlsc,
};

const struct pst_callback_ops *pst_get_hw_mlsc_cb(void)
{
	return &pst_cb_mlsc;
}

static int pst_set_hw_mlsc(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val, DEV_HW_MLSC0, frame_mlsc, mlsc_param, &mlsc_cr_set,
		ARRAY_SIZE(mlsc_param_preset), preset_test_result, &pst_cb_mlsc);
}

static int pst_get_hw_mlsc(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "MLSC", ARRAY_SIZE(mlsc_param_preset), preset_test_result);
}
