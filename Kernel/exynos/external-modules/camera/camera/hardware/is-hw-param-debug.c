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

#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

const char *otf_format_str[] = {
	"[0]FORMAT_BAYER",
	"[1]FORMAT_YUV444",
	"[2]FORMAT_YUV422",
	"[3]FORMAT_YUV420",
	"[4]FORMAT_RGB",
	"[5]FORMAT_YUV444_TRUNCATED",
	"[6]FORMAT_YUV422_TRUNCATED",
	/* input format */
	"[7]FORMAT_Y",
	/* not defined */
	"[8]FORMAT_UNKNOWN",
	/* output format */
	"[9]FORMAT_Y",
	"[10]FORMAT_BAYER_PLUS",
	"[11]FORMAT_BAYER_COMP",
	"[12]FORMAT_BAYER_COMP_LOSSY",
};

const char *dma_format_str[] = {
	"[0]FORMAT_BAYER",
	"[1]FORMAT_YUV444",
	"[2]FORMAT_YUV422",
	"[3]FORMAT_YUV420",
	"[4]FORMAT_RGB",
	"[5]FORMAT_BAYER_PACKED",
	"[6]FORMAT_YUV422_CHUNKER",
	"[7]FORMAT_YUV444_TRUNCATED",
	"[8]FORMAT_YUV422_TRUNCATED",
	"[9]FORMAT_Y",
	"[10]FORMAT_BAYER_PLUS",
	"[11]FORMAT_BAYER_COMP",
	"[12]FORMAT_BAYER_COMP_LOSSY",
	"[13]FORMAT_UNKNOWN",
	"[14]FORMAT_UNKNOWN",
	"[15]FORMAT_UNKNOWN",
	"[16]FORMAT_UNKNOWN",
	"[17]FORMAT_YUV422_PACKED",
	"[18]FORMAT_UNKNOWN",
	"[19]FORMAT_SEGCONF",
	"[20]FORMAT_HF",
	"[21]FORMAT_DRCGAIN",
	"[22]FORMAT_NOISE",
	"[23]FORMAT_SVHIST",
	"[24]FORMAT_DRCGRID",
	"[25]FORMAT_ANDROID",
};

const char *sbwc_type_str[] = {
	"[0]SBWC_DISABLE",
	"[1]SBWC_LOSSYLESS_32B",
	"[2]SBWC_LOSSY_32B",
	"[3]SBWC_LOSSY_CUSTOM_32B",
	"[4]SBWC_UNKNOWN",
	"[5]SBWC_LOSSYLESS_64B",
	"[6]SBWC_LOSSY_64B",
	"[7]SBWC_LOSSY_CUSTOM_64B",
};

char *dump_param_control(char *buf, const char *name, struct param_control *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "cmd", param->cmd, rem);
	p = pablo_json_uint(p, "bypass", param->bypass, rem);
	p = pablo_json_uint(p, "strgen", param->strgen, rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_control);

char *dump_param_sensor_config(
	char *buf, const char *name, struct param_sensor_config *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "frametime", param->frametime, rem);
	p = pablo_json_uint(p, "min_target_fps", param->min_target_fps, rem);
	p = pablo_json_uint(p, "max_target_fps", param->max_target_fps, rem);
	p = pablo_json_uint(p, "width", param->width, rem);
	p = pablo_json_uint(p, "height", param->height, rem);
	p = pablo_json_uint(p, "sensor_binning_ratio_x", param->sensor_binning_ratio_x, rem);
	p = pablo_json_uint(p, "sensor_binning_ratio_y", param->sensor_binning_ratio_y, rem);
	p = pablo_json_uint(p, "bns_binning_ratio_x", param->bns_binning_ratio_x, rem);
	p = pablo_json_uint(p, "bns_binning_ratio_y", param->bns_binning_ratio_y, rem);
	p = pablo_json_uint(p, "bns_margin_left", param->bns_margin_left, rem);
	p = pablo_json_uint(p, "bns_margin_top", param->bns_margin_top, rem);
	p = pablo_json_uint(p, "bns_output_width", param->bns_output_width, rem);
	p = pablo_json_uint(p, "bns_output_height", param->bns_output_height, rem);
	p = pablo_json_uint(p, "calibrated_width", param->calibrated_width, rem);
	p = pablo_json_uint(p, "calibrated_height", param->calibrated_height, rem);
	p = pablo_json_uint(
		p, "freeform_sensor_crop_enable", param->freeform_sensor_crop_enable, rem);
	p = pablo_json_uint(
		p, "freeform_sensor_crop_offset_x", param->freeform_sensor_crop_offset_x, rem);
	p = pablo_json_uint(
		p, "freeform_sensor_crop_offset_y", param->freeform_sensor_crop_offset_y, rem);
	p = pablo_json_uint(p, "pixel_order", param->pixel_order, rem);
	p = pablo_json_uint(p, "vvalid_time", param->vvalid_time, rem);
	p = pablo_json_uint(p, "req_vvalid_time", param->req_vvalid_time, rem);
	p = pablo_json_uint(p, "scenario", param->scenario, rem);
	p = pablo_json_uint(p, "mono_mode", param->mono_mode, rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_sensor_config);

char *dump_param_otf_input(char *buf, const char *name, struct param_otf_input *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "cmd", param->cmd, rem);
	p = pablo_json_nstr(p, "format", otf_format_str[param->format],
		strlen(otf_format_str[param->format]), rem);
	p = pablo_json_uint(p, "bitwidth", param->bitwidth, rem);
	p = pablo_json_uint(p, "order", param->order, rem);
	p = pablo_json_uint(p, "width", param->width, rem);
	p = pablo_json_uint(p, "height", param->height, rem);
	p = pablo_json_uint(p, "bayer_crop_offset_x", param->bayer_crop_offset_x, rem);
	p = pablo_json_uint(p, "bayer_crop_offset_y", param->bayer_crop_offset_y, rem);
	p = pablo_json_uint(p, "bayer_crop_width", param->bayer_crop_width, rem);
	p = pablo_json_uint(p, "bayer_crop_height", param->bayer_crop_height, rem);
	p = pablo_json_uint(p, "source", param->source, rem);
	p = pablo_json_uint(p, "physical_width", param->physical_width, rem);
	p = pablo_json_uint(p, "physical_height", param->physical_height, rem);
	p = pablo_json_uint(p, "offset_x", param->offset_x, rem);
	p = pablo_json_uint(p, "offset_y", param->offset_y, rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_otf_input);

char *dump_param_otf_output(
	char *buf, const char *name, struct param_otf_output *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "cmd", param->cmd, rem);
	p = pablo_json_nstr(p, "format", otf_format_str[param->format],
		strlen(otf_format_str[param->format]), rem);
	p = pablo_json_uint(p, "bitwidth", param->bitwidth, rem);
	p = pablo_json_uint(p, "order", param->order, rem);
	p = pablo_json_uint(p, "width", param->width, rem);
	p = pablo_json_uint(p, "height", param->height, rem);
	p = pablo_json_uint(p, "crop_offset_x", param->crop_offset_x, rem);
	p = pablo_json_uint(p, "crop_offset_y", param->crop_offset_y, rem);
	p = pablo_json_uint(p, "crop_width", param->crop_width, rem);
	p = pablo_json_uint(p, "crop_height", param->crop_height, rem);
	p = pablo_json_uint(p, "crop_enable", param->crop_enable, rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_otf_output);

char *dump_param_dma_input(char *buf, const char *name, struct param_dma_input *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "cmd", param->cmd, rem);
	p = pablo_json_nstr(p, "format", dma_format_str[param->format],
		strlen(dma_format_str[param->format]), rem);
	p = pablo_json_uint(p, "bitwidth", param->bitwidth, rem);
	p = pablo_json_uint(p, "order", param->order, rem);
	p = pablo_json_uint(p, "plane", param->plane, rem);
	p = pablo_json_uint(p, "width", param->width, rem);
	p = pablo_json_uint(p, "height", param->height, rem);
	p = pablo_json_uint(p, "dma_crop_offset", param->dma_crop_offset, rem);
	p = pablo_json_uint(p, "dma_crop_width", param->dma_crop_width, rem);
	p = pablo_json_uint(p, "dma_crop_height", param->dma_crop_height, rem);
	p = pablo_json_uint(p, "bayer_crop_offset_x", param->bayer_crop_offset_x, rem);
	p = pablo_json_uint(p, "bayer_crop_offset_y", param->bayer_crop_offset_y, rem);
	p = pablo_json_uint(p, "bayer_crop_width", param->bayer_crop_width, rem);
	p = pablo_json_uint(p, "bayer_crop_height", param->bayer_crop_height, rem);
	p = pablo_json_uint(p, "scene_mode", param->scene_mode, rem);
	p = pablo_json_uint(p, "stride_plane0", param->stride_plane0, rem);
	p = pablo_json_uint(p, "stride_plane1", param->stride_plane1, rem);
	p = pablo_json_uint(p, "stride_plane2", param->stride_plane2, rem);
	p = pablo_json_uint(p, "v_otf_enable", param->v_otf_enable, rem);
	p = pablo_json_uint(p, "orientation", param->orientation, rem);
	p = pablo_json_uint(p, "overlab_width", param->overlab_width, rem);
	p = pablo_json_uint(p, "strip_count", param->strip_count, rem);
	p = pablo_json_uint(p, "strip_max_count", param->strip_max_count, rem);
	p = pablo_json_uint(p, "sequence_id", param->sequence_id, rem);
	p = pablo_json_nstr(p, "sbwc_type", sbwc_type_str[param->sbwc_type],
		strlen(sbwc_type_str[param->sbwc_type]), rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_dma_input);

char *dump_param_dma_output(
	char *buf, const char *name, struct param_dma_output *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "cmd", param->cmd, rem);
	p = pablo_json_nstr(p, "format", dma_format_str[param->format],
		strlen(dma_format_str[param->format]), rem);
	p = pablo_json_uint(p, "bitwidth", param->bitwidth, rem);
	p = pablo_json_uint(p, "order", param->order, rem);
	p = pablo_json_uint(p, "plane", param->plane, rem);
	p = pablo_json_uint(p, "width", param->width, rem);
	p = pablo_json_uint(p, "height", param->height, rem);
	p = pablo_json_uint(p, "dma_crop_offset_x", param->dma_crop_offset_x, rem);
	p = pablo_json_uint(p, "dma_crop_offset_y", param->dma_crop_offset_y, rem);
	p = pablo_json_uint(p, "dma_crop_width", param->dma_crop_width, rem);
	p = pablo_json_uint(p, "dma_crop_height", param->dma_crop_height, rem);
	p = pablo_json_uint(p, "crop_enable", param->crop_enable, rem);
	p = pablo_json_uint(p, "msb", param->msb, rem);
	p = pablo_json_uint(p, "stride_plane0", param->stride_plane0, rem);
	p = pablo_json_uint(p, "stride_plane1", param->stride_plane1, rem);
	p = pablo_json_uint(p, "stride_plane2", param->stride_plane2, rem);
	p = pablo_json_uint(p, "v_otf_enable", param->v_otf_enable, rem);
	p = pablo_json_nstr(p, "sbwc_type", sbwc_type_str[param->sbwc_type],
		strlen(sbwc_type_str[param->sbwc_type]), rem);
	p = pablo_json_uint(p, "channel", param->channel, rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_dma_output);

char *dump_param_stripe_input(
	char *buf, const char *name, struct param_stripe_input *param, size_t *rem)
{
	int i;
	char *p = buf;
	char index_str[32];

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "index", param->index, rem);
	p = pablo_json_uint(p, "total_count", param->total_count, rem);
	p = pablo_json_uint(p, "left_margin", param->left_margin, rem);
	p = pablo_json_uint(p, "right_margin", param->right_margin, rem);
	p = pablo_json_uint(p, "full_width", param->full_width, rem);
	p = pablo_json_uint(p, "full_height", param->full_height, rem);
	p = pablo_json_uint(p, "full_incrop_width", param->full_incrop_width, rem);
	p = pablo_json_uint(p, "full_incrop_height", param->full_incrop_height, rem);
	p = pablo_json_uint(p, "scaled_left_margin", param->scaled_left_margin, rem);
	p = pablo_json_uint(p, "scaled_right_margin", param->scaled_right_margin, rem);
	p = pablo_json_uint(p, "scaled_full_width", param->scaled_full_width, rem);
	p = pablo_json_uint(p, "scaled_full_height", param->scaled_full_height, rem);
	for (i = 0; i < MAX_STRIPE_REGION_NUM; i++) {
		snprintf(index_str, sizeof(index_str), "stripe_roi_start_pos_x[%d]", i);
		p = pablo_json_uint(p, index_str, param->stripe_roi_start_pos_x[i], rem);
	}
	p = pablo_json_uint(p, "start_pos_x", param->start_pos_x, rem);
	p = pablo_json_uint(p, "repeat_idx", param->repeat_idx, rem);
	p = pablo_json_uint(p, "repeat_num", param->repeat_num, rem);
	p = pablo_json_uint(p, "repeat_scenario", param->repeat_scenario, rem);
	p = pablo_json_uint(p, "error", param->error, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_stripe_input);

char *dump_param_mcs_input(char *buf, const char *name, struct param_mcs_input *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "otf_cmd", param->otf_cmd, rem);
	p = pablo_json_nstr(p, "format", otf_format_str[param->otf_format],
		strlen(otf_format_str[param->otf_format]), rem);
	p = pablo_json_uint(p, "otf_bitwidth", param->otf_bitwidth, rem);
	p = pablo_json_uint(p, "otf_order", param->otf_order, rem);
	p = pablo_json_uint(p, "dma_cmd", param->dma_cmd, rem);
	p = pablo_json_nstr(p, "format", dma_format_str[param->dma_format],
		strlen(dma_format_str[param->dma_format]), rem);
	p = pablo_json_uint(p, "dma_bitwidth", param->dma_bitwidth, rem);
	p = pablo_json_uint(p, "dma_order", param->dma_order, rem);
	p = pablo_json_uint(p, "plane", param->plane, rem);
	p = pablo_json_uint(p, "width", param->width, rem);
	p = pablo_json_uint(p, "height", param->height, rem);
	p = pablo_json_uint(p, "dma_stride_y", param->dma_stride_y, rem);
	p = pablo_json_uint(p, "dma_stride_c", param->dma_stride_c, rem);
	p = pablo_json_uint(p, "dma_crop_offset_x", param->dma_crop_offset_x, rem);
	p = pablo_json_uint(p, "dma_crop_offset_y", param->dma_crop_offset_y, rem);
	p = pablo_json_uint(p, "dma_crop_width", param->dma_crop_width, rem);
	p = pablo_json_uint(p, "dma_crop_height", param->dma_crop_height, rem);
	p = pablo_json_uint(p, "djag_out_width", param->djag_out_width, rem);
	p = pablo_json_uint(p, "djag_out_height", param->djag_out_height, rem);
	p = pablo_json_uint(p, "stripe_in_start_pos_x", param->stripe_in_start_pos_x, rem);
	p = pablo_json_uint(p, "stripe_in_end_pos_x", param->stripe_in_end_pos_x, rem);
	p = pablo_json_uint(p, "stripe_roi_start_pos_x", param->stripe_roi_start_pos_x, rem);
	p = pablo_json_uint(p, "stripe_roi_end_pos_x", param->stripe_roi_end_pos_x, rem);
	p = pablo_json_nstr(p, "sbwc_type", sbwc_type_str[param->sbwc_type],
		strlen(sbwc_type_str[param->sbwc_type]), rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_mcs_input);

char *dump_param_mcs_output(
	char *buf, const char *name, struct param_mcs_output *param, size_t *rem)
{
	char *p = buf;

	p = pablo_json_object_open(p, name, rem);
	p = pablo_json_uint(p, "otf_cmd", param->otf_cmd, rem);
	p = pablo_json_nstr(p, "format", otf_format_str[param->otf_format],
		strlen(otf_format_str[param->otf_format]), rem);
	p = pablo_json_uint(p, "otf_bitwidth", param->otf_bitwidth, rem);
	p = pablo_json_uint(p, "otf_order", param->otf_order, rem);
	p = pablo_json_uint(p, "dma_cmd", param->dma_cmd, rem);
	p = pablo_json_nstr(p, "format", dma_format_str[param->dma_format],
		strlen(dma_format_str[param->dma_format]), rem);
	p = pablo_json_uint(p, "dma_bitwidth", param->dma_bitwidth, rem);
	p = pablo_json_uint(p, "dma_order", param->dma_order, rem);
	p = pablo_json_uint(p, "plane", param->plane, rem);
	p = pablo_json_uint(p, "crop_offset_x", param->crop_offset_x, rem);
	p = pablo_json_uint(p, "crop_offset_y", param->crop_offset_y, rem);
	p = pablo_json_uint(p, "crop_width", param->crop_width, rem);
	p = pablo_json_uint(p, "crop_height", param->crop_height, rem);
	p = pablo_json_uint(p, "width", param->width, rem);
	p = pablo_json_uint(p, "height", param->height, rem);
	p = pablo_json_uint(p, "dma_stride_y", param->dma_stride_y, rem);
	p = pablo_json_uint(p, "dma_stride_c", param->dma_stride_c, rem);
	p = pablo_json_uint(p, "yuv_range", param->yuv_range, rem);
	p = pablo_json_uint(p, "flip", param->flip, rem);
	p = pablo_json_uint(p, "hwfc", param->hwfc, rem);
	p = pablo_json_uint(p, "offset_x", param->offset_x, rem);
	p = pablo_json_uint(p, "offset_y", param->offset_y, rem);
	p = pablo_json_uint(p, "cmd", param->cmd, rem);
	p = pablo_json_uint(p, "stripe_in_start_pos_x", param->stripe_in_start_pos_x, rem);
	p = pablo_json_uint(p, "stripe_roi_start_pos_x", param->stripe_roi_start_pos_x, rem);
	p = pablo_json_uint(p, "stripe_roi_end_pos_x", param->stripe_roi_end_pos_x, rem);
	p = pablo_json_uint(p, "full_input_width", param->full_input_width, rem);
	p = pablo_json_uint(p, "full_output_width", param->full_output_width, rem);
	p = pablo_json_nstr(p, "sbwc_type", sbwc_type_str[param->sbwc_type],
		strlen(sbwc_type_str[param->sbwc_type]), rem);
	p = pablo_json_uint(p, "bitsperpixel", param->bitsperpixel, rem);
	p = pablo_json_uint(p, "err", param->err, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_mcs_output);

char *dump_param_hw_ip(char *buf, const char *name, const u32 id, size_t *rem)
{
	char *p = buf;

	p = pablo_json_nstr(p, "hw name", name, strlen(name), rem);
	p = pablo_json_uint(p, "hw id", id, rem);

	return p;
}
EXPORT_SYMBOL_GPL(dump_param_hw_ip);
