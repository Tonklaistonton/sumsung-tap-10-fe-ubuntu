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

#ifndef IS_PARAMS_H
#define IS_PARAMS_H

#include <linux/spinlock.h>
#include <linux/bitmap.h>
#include "is-common-enum.h"

#define PARAMETER_MAX_SIZE 128 /* bytes */
#define PARAMETER_MAX_MEMBER (PARAMETER_MAX_SIZE >> 2)

#define INC_BIT(bit) (bit << 1)
#define INC_NUM(bit) (bit + 1)

#define MLSC_PARAM_NUM (PARAM_MLSC_MAX - PARAM_MLSC_CONTROL + 1)
#define GET_MLSC_CTX_PINDEX(p, c) ((p) + ((c) * MLSC_PARAM_NUM))
#define MLSC_GLPG_NUM 5

#define MCSC_OUTPUT_DS MCSC_OUTPUT5
#define MCSC_INPUT_HF MCSC_OUTPUT5

#define MAX_STRIPE_REGION_NUM (8)

#define MAX_DJAG_SCALING_RATIO_INDEX 4
#define MAX_SCALER_SCALING_RATIO_INDEX 7
#define MAX_MODE_INDEX 2

typedef dma_addr_t pdma_addr_t;

enum is_param {
	PARAM_SENSOR_CONFIG,

	PARAM_MLSC_CONTROL,
	PARAM_MLSC_OTF_INPUT,
	PARAM_MLSC_DMA_INPUT,
	PARAM_MLSC_YUV444,
	PARAM_MLSC_GLPG0,
	PARAM_MLSC_GLPG1,
	PARAM_MLSC_GLPG2,
	PARAM_MLSC_GLPG3,
	PARAM_MLSC_GLPG4,
	PARAM_MLSC_SVHIST,
	PARAM_MLSC_LMEDS,
	PARAM_MLSC_FDPIG,
	PARAM_MLSC_CAV,
	PARAM_MLSC_MAX = PARAM_MLSC_CAV,

	PARAM_BYRP_CONTROL,
	PARAM_BYRP_OTF_INPUT,
	PARAM_BYRP_DMA_INPUT,
	PARAM_BYRP_OTF_OUTPUT,
	PARAM_BYRP_BYR,

	PARAM_RGBP_CONTROL,
	PARAM_RGBP_OTF_INPUT,
	PARAM_RGBP_OTF_OUTPUT,
	PARAM_RGBP_DMA_INPUT,
	PARAM_RGBP_HIST,
	PARAM_RGBP_AWB,
	PARAM_RGBP_DRC,
	PARAM_RGBP_SAT,

	PARAM_YUVSC_CONTROL,
	PARAM_YUVSC_OTF_INPUT,
	PARAM_YUVSC_OTF_OUTPUT,

	PARAM_MTNR_CONTROL,
	PARAM_MTNR_STRIPE_INPUT,
	/* MTNR0 */
	PARAM_MTNR_CIN_MTNR1_WGT,
	PARAM_MTNR_COUT_MSNR_L0,
	PARAM_MTNR_RDMA_CUR_L0,
	PARAM_MTNR_RDMA_CUR_L4,
	PARAM_MTNR_RDMA_PREV_L0,
	PARAM_MTNR_RDMA_PREV_L0_WGT,
	PARAM_MTNR_RDMA_MV_GEOMATCH,
	PARAM_MTNR_RDMA_SEG_L0,
	PARAM_MTNR_WDMA_PREV_L0,
	PARAM_MTNR_WDMA_PREV_L0_WGT,
	/* MTNR1 */
	PARAM_MTNR_CIN_DLFE_WGT,
	PARAM_MTNR_COUT_MSNR_L1,
	PARAM_MTNR_COUT_MSNR_L2,
	PARAM_MTNR_COUT_MSNR_L3,
	PARAM_MTNR_COUT_MSNR_L4,
	PARAM_MTNR_COUT_MTNR0_WGT,
	PARAM_MTNR_COUT_DLFE_CUR,
	PARAM_MTNR_COUT_DLFE_PREV,
	PARAM_MTNR_RDMA_CUR_L1,
	PARAM_MTNR_RDMA_CUR_L2,
	PARAM_MTNR_RDMA_CUR_L3,
	PARAM_MTNR_RDMA_PREV_L1,
	PARAM_MTNR_RDMA_PREV_L2,
	PARAM_MTNR_RDMA_PREV_L3,
	PARAM_MTNR_RDMA_PREV_L4,
	PARAM_MTNR_RDMA_PREV_L1_WGT,
	PARAM_MTNR_RDMA_SEG_L1,
	PARAM_MTNR_RDMA_SEG_L2,
	PARAM_MTNR_RDMA_SEG_L3,
	PARAM_MTNR_RDMA_SEG_L4,
	PARAM_MTNR_WDMA_PREV_L1,
	PARAM_MTNR_WDMA_PREV_L2,
	PARAM_MTNR_WDMA_PREV_L3,
	PARAM_MTNR_WDMA_PREV_L4,
	PARAM_MTNR_WDMA_PREV_L1_WGT,

	PARAM_MSNR_CONTROL,
	PARAM_MSNR_STRIPE_INPUT,
	PARAM_MSNR_CIN_L0,
	PARAM_MSNR_CIN_L1,
	PARAM_MSNR_CIN_L2,
	PARAM_MSNR_CIN_L3,
	PARAM_MSNR_CIN_L4,
	PARAM_MSNR_COUT_YUV,
	PARAM_MSNR_COUT_STAT,
	PARAM_MSNR_WDMA_LME,

	PARAM_YUVP_CONTROL,
	PARAM_YUVP_OTF_INPUT,
	PARAM_YUVP_OTF_OUTPUT,
	PARAM_YUVP_DMA_INPUT,
	PARAM_YUVP_FTO_OUTPUT,
	PARAM_YUVP_STRIPE_INPUT,
	PARAM_YUVP_SEG,
	PARAM_YUVP_DRC,
	PARAM_YUVP_CLAHE,
	PARAM_YUVP_PCCHIST_R,
	PARAM_YUVP_PCCHIST_W,

	PARAM_MCS_CONTROL,
	PARAM_MCS_INPUT,
	PARAM_MCS_OUTPUT0,
	PARAM_MCS_OUTPUT1,
	PARAM_MCS_OUTPUT2,
	PARAM_MCS_OUTPUT3,
	PARAM_MCS_OUTPUT4,
	PARAM_MCS_OUTPUT5,
	PARAM_MCS_STRIPE_INPUT,

	PARAM_PAF_CONTROL,
	PARAM_PAF_DMA_INPUT,
	PARAM_PAF_OTF_OUTPUT, /* deprecated, only for compatibility */
	PARAM_PAF_DMA_OUTPUT,

	IS_PARAM_NUM,
};

#define PARAM_MCSC_CONTROL PARAM_MCS_CONTROL
#define PARAM_MCSC_OTF_INPUT PARAM_MCS_INPUT
#define PARAM_MCSC_P0 PARAM_MCS_OUTPUT0
#define PARAM_MCSC_P1 PARAM_MCS_OUTPUT1
#define PARAM_MCSC_P2 PARAM_MCS_OUTPUT2
#define PARAM_MCSC_P3 PARAM_MCS_OUTPUT3
#define PARAM_MCSC_P4 PARAM_MCS_OUTPUT4
#define PARAM_MCSC_P5 PARAM_MCS_OUTPUT5

#define IS_DECLARE_PMAP(name) DECLARE_BITMAP(name, IS_PARAM_NUM)
#define IS_INIT_PMAP(name) bitmap_zero(name, IS_PARAM_NUM)
#define IS_FILL_PMAP(name) bitmap_fill(name, IS_PARAM_NUM)
#define IS_COPY_PMAP(dst, src) bitmap_copy(dst, src, IS_PARAM_NUM)
#define IS_AND_PMAP(dst, src) bitmap_and(dst, dst, src, IS_PARAM_NUM)
#define IS_OR_PMAP(dst, src) bitmap_or(dst, dst, src, IS_PARAM_NUM)
#define IS_EMPTY_PMAP(name) bitmap_empty(name, IS_PARAM_NUM)

#define IS_DECLARE_TUNECNT_MAP(name) DECLARE_BITMAP(name, IS_MAX_SETFILE)

enum mcsc_output_index {
	MCSC_OUTPUT0 = 0,
	MCSC_OUTPUT1 = 1,
	MCSC_OUTPUT2 = 2,
	MCSC_OUTPUT3 = 3,
	MCSC_OUTPUT4 = 4,
	MCSC_OUTPUT5 = 5,
	MCSC_OUTPUT_MAX
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo parameter types                                           *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
struct param_sensor_config {
	u32 frametime; /* max exposure time(us) */
	u32 min_target_fps;
	u32 max_target_fps;
	u32 width;
	u32 height;
	u32 sensor_binning_ratio_x;
	u32 sensor_binning_ratio_y;
	u32 bns_binning_ratio_x;
	u32 bns_binning_ratio_y;
	u32 bns_margin_left;
	u32 bns_margin_top;
	u32 bns_output_width; /* Active scaled image width */
	u32 bns_output_height; /* Active scaled image height */
	u32 calibrated_width; /* sensor cal size */
	u32 calibrated_height;
	u32 freeform_sensor_crop_enable; /* freeform sensor crop, not center aligned */
	u32 freeform_sensor_crop_offset_x;
	u32 freeform_sensor_crop_offset_y;
	u32 pixel_order;
	u32 vvalid_time; /* Actual sensor vvalid time */
	u32 req_vvalid_time; /* Required vvalid time for OTF input IPs */
	u32 scenario;
	u32 mono_mode; /* use mono sensor */
	u32 reserved[PARAMETER_MAX_MEMBER - 24];
	u32 err;
};

struct param_control {
	u32 cmd;
	u32 bypass;
	u32 strgen;
	u32 reserved[PARAMETER_MAX_MEMBER - 4];
	u32 err;
};

struct param_otf_input {
	u32 cmd;
	u32 format;
	u32 bitwidth;
	u32 order;
	u32 width; /* with margin */
	u32 height; /* with margine */
	u32 bayer_crop_offset_x;
	u32 bayer_crop_offset_y;
	u32 bayer_crop_width;
	u32 bayer_crop_height;
	u32 source;
	u32 physical_width; /* dummy + valid image */
	u32 physical_height; /* dummy + valid image */
	u32 offset_x; /* BCrop0: for removing dummy region */
	u32 offset_y; /* BCrop0: for removing dummy region */
	u32 reserved[PARAMETER_MAX_MEMBER - 16];
	u32 err;
};

struct param_dma_input {
	u32 cmd;
	u32 format;
	u32 bitwidth;
	u32 order;
	u32 plane;
	u32 width;
	u32 height;
	u32 dma_crop_offset; /* uiDmaCropOffset[31:16] : X offset, uiDmaCropOffset[15:0] : Y offset */
	u32 dma_crop_width;
	u32 dma_crop_height;
	u32 bayer_crop_offset_x;
	u32 bayer_crop_offset_y;
	u32 bayer_crop_width;
	u32 bayer_crop_height;
	u32 scene_mode; /* for AE envelop */
	u32 msb; /* last bit of data in memory size */
	u32 stride_plane0;
	u32 stride_plane1;
	u32 stride_plane2;
	u32 v_otf_enable;
	u32 orientation;
	u32 strip_mode;
	u32 overlab_width;
	u32 strip_count;
	u32 strip_max_count;
	u32 sequence_id;
	u32 sbwc_type; /* COMP_OFF = 0, COMP_LOSSLESS = 1, COMP_LOSSY =2 */
	u32 reserved[PARAMETER_MAX_MEMBER - 28];
	u32 err;
};

struct param_stripe_input {
	u32 index; /* Stripe region index */
	u32 total_count; /* Total stripe region num */
	u32 left_margin; /* Overlapped left horizontal region */
	u32 right_margin; /* Overlapped right horizontal region */
	u32 full_width; /* Original image width */
	u32 full_height; /* Original image height */
	u32 full_incrop_width; /* Original incrop image width */
	u32 full_incrop_height; /* Original incrop image height */
	u32 scaled_left_margin; /* for scaler */
	u32 scaled_right_margin; /* for scaler */
	u32 scaled_full_width; /* for scaler */
	u32 scaled_full_height; /* for scaler */
	u32 stripe_roi_start_pos_x[MAX_STRIPE_REGION_NUM + 1]; /* Start X Position w/o Margin */
	u32 start_pos_x;
	u32 repeat_idx;
	u32 repeat_num;
	u32 repeat_scenario;
	u32 reserved[PARAMETER_MAX_MEMBER - 26];
	u32 error; /* Error code */
};

struct param_otf_output {
	u32 cmd;
	u32 format;
	u32 bitwidth;
	u32 order;
	u32 width; /* BDS output width */
	u32 height; /* BDS output height */
	u32 crop_offset_x;
	u32 crop_offset_y;
	u32 crop_width;
	u32 crop_height;
	u32 crop_enable; /* 0: use crop before bds, 1: use crop after bds */
	u32 reserved[PARAMETER_MAX_MEMBER - 12];
	u32 err;
};

struct param_dma_output {
	u32 cmd;
	u32 format;
	u32 bitwidth;
	u32 order;
	u32 plane;
	u32 width; /* BDS output width */
	u32 height; /* BDS output height */
	u32 dma_crop_offset_x;
	u32 dma_crop_offset_y;
	u32 dma_crop_width;
	u32 dma_crop_height;
	u32 crop_enable; /* 0: use crop before bds, 1: use crop after bds */
	u32 msb; /* last bit of data in memory size */
	u32 stride_plane0;
	u32 stride_plane1;
	u32 stride_plane2;
	u32 v_otf_enable;
	u32 sbwc_type; /* COMP_OFF = 0, COMP_LOSSLESS = 1, COMP_LOSSY =2 */
	u32 channel; /* 3AA channel for HF */
	u32 reserved[PARAMETER_MAX_MEMBER - 20];
	u32 err;
};
struct param_mcs_input {
	u32 otf_cmd; /* DISABLE or ENABLE */
	u32 otf_format;
	u32 otf_bitwidth;
	u32 otf_order;
	u32 dma_cmd; /* DISABLE or ENABLE */
	u32 dma_format;
	u32 dma_bitwidth;
	u32 dma_order;
	u32 plane;
	u32 width;
	u32 height;
	u32 dma_stride_y;
	u32 dma_stride_c;
	u32 dma_crop_offset_x;
	u32 dma_crop_offset_y;
	u32 dma_crop_width;
	u32 dma_crop_height;
	u32 djag_out_width;
	u32 djag_out_height;
	u32 stripe_in_start_pos_x; /* Start X Position w/ Margin For STRIPE */
	u32 stripe_in_end_pos_x; /* End X Position w/ Margin For STRIPE */
	u32 stripe_roi_start_pos_x; /* Start X Position w/o Margin For STRIPE */
	u32 stripe_roi_end_pos_x; /* End X Position w/o Margin For STRIPE */
	u32 sbwc_type;
	u32 shrp_width;
	u32 shrp_height;
	u32 reserved[PARAMETER_MAX_MEMBER - 27];
	u32 err;
};

struct param_mcs_output {
	u32 otf_cmd; /* DISABLE or ENABLE */
	u32 otf_format;
	u32 otf_bitwidth;
	u32 otf_order;
	u32 dma_cmd; /* DISABLE or ENABLE */
	u32 dma_format;
	u32 dma_bitwidth;
	u32 dma_order;
	u32 plane;
	u32 crop_offset_x;
	u32 crop_offset_y;
	u32 crop_width;
	u32 crop_height;
	u32 width;
	u32 height;
	u32 dma_stride_y;
	u32 dma_stride_c;
	u32 yuv_range; /* FULL or NARROW */
	u32 flip; /* NORMAL(0) or X-MIRROR(1) or Y- MIRROR(2) or XY- MIRROR(3) */
	u32 hwfc; /* DISABLE or ENABLE */
	u32 offset_x;
	u32 offset_y;

	/*
	 * cmd
	 * [BIT 0]: output crop(0: disable, 1: enable)
	 * [BIT 1]: crop type(0: center, 1: freeform)
	 */
	u32 cmd;
	u32 stripe_in_start_pos_x; /* Start X Position w/ Margin For STRIPE */
	u32 stripe_roi_start_pos_x; /* Start X Position w/o Margin For STRIPE */
	u32 stripe_roi_end_pos_x; /* End X Position w/o Margin For STRIPE */
	u32 full_input_width; /* FULL WIDTH For STRIPE */
	u32 full_output_width;
	u32 sbwc_type;
	u32 bitsperpixel; /* bitsperpixel[23:16]: plane2, [15:8]: plane1,  [7:0]: plane0 */
	u32 reserved[PARAMETER_MAX_MEMBER - 31];
	u32 err;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Pablo parameter definitions					   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
struct sensor_param {
	struct param_sensor_config config;
};

struct mlsc_param {
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_dma_input dma_input;
	struct param_dma_output yuv;
	struct param_dma_output glpg[MLSC_GLPG_NUM];
	struct param_dma_output svhist;
	struct param_dma_output lme_ds;
	struct param_dma_output fdpig;
	struct param_dma_output cav;
};

struct byrp_param {
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_dma_input dma_input;
	struct param_otf_output otf_output;
	struct param_dma_output dma_output_byr;
};

struct rgbp_param {
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_otf_output otf_output;
	struct param_dma_input dma_input;
	struct param_dma_output hist;
	struct param_dma_output awb;
	struct param_dma_output drc;
	struct param_dma_output sat;
};

struct yuvsc_param {
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_otf_output otf_output;
};

struct mtnr_param {
	struct param_control control;
	struct param_stripe_input stripe_input;
	/* mtnr0 */
	struct param_otf_input cin_mtnr1_wgt;
	struct param_otf_output cout_msnr_l0;
	struct param_dma_input rdma_cur_l0;
	struct param_dma_input rdma_cur_l4;
	struct param_dma_input rdma_prev_l0;
	struct param_dma_input rdma_prev_l0_wgt;
	struct param_dma_input rdma_mv_geomatch;
	struct param_dma_input rdma_seg_l0;
	struct param_dma_output wdma_prev_l0;
	struct param_dma_output wdma_prev_l0_wgt;
	/* mtnr1 */
	struct param_otf_input cin_dlfe_wgt;
	struct param_otf_output cout_msnr_l1;
	struct param_otf_output cout_msnr_l2;
	struct param_otf_output cout_msnr_l3;
	struct param_otf_output cout_msnr_l4;
	struct param_otf_output cout_mtnr0_wgt;
	struct param_otf_output cout_dlfe_cur;
	struct param_otf_output cout_dlfe_prev;
	struct param_dma_input rdma_cur_l1;
	struct param_dma_input rdma_cur_l2;
	struct param_dma_input rdma_cur_l3;
	struct param_dma_input rdma_prev_l1;
	struct param_dma_input rdma_prev_l2;
	struct param_dma_input rdma_prev_l3;
	struct param_dma_input rdma_prev_l4;
	struct param_dma_input rdma_prev_l1_wgt;
	struct param_dma_input rdma_seg_l1;
	struct param_dma_input rdma_seg_l2;
	struct param_dma_input rdma_seg_l3;
	struct param_dma_input rdma_seg_l4;
	struct param_dma_output wdma_prev_l1;
	struct param_dma_output wdma_prev_l2;
	struct param_dma_output wdma_prev_l3;
	struct param_dma_output wdma_prev_l4;
	struct param_dma_output wdma_prev_l1_wgt;
};

struct msnr_param {
	struct param_control control;
	struct param_stripe_input stripe_input;
	struct param_otf_input cin_msnr_l0;
	struct param_otf_input cin_msnr_l1;
	struct param_otf_input cin_msnr_l2;
	struct param_otf_input cin_msnr_l3;
	struct param_otf_input cin_msnr_l4;
	struct param_otf_output cout_msnr_yuv;
	struct param_otf_output cout_msnr_stat;
	struct param_dma_output wdma_lme;
};

struct yuvp_param {
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_otf_output otf_output;
	struct param_dma_input dma_input;
	struct param_otf_output fto_output;
	struct param_stripe_input stripe_input;
	struct param_dma_input seg;
	struct param_dma_input drc;
	struct param_dma_input clahe;
	struct param_dma_input pcchist_in;
	struct param_dma_output pcchist_out;
};

struct mcs_param {
	struct param_control control;
	struct param_mcs_input input;
	struct param_mcs_output output[MCSC_OUTPUT_MAX];
	struct param_stripe_input stripe_input;
};

struct paf_rdma_param {
	struct param_control control;
	struct param_dma_input dma_input;
	struct param_otf_output otf_output;
	struct param_dma_output dma_output;
};

struct vra_param {
	struct param_control control;
};

struct is_param_region {
	struct sensor_param sensor;
	struct mlsc_param mlsc;
	struct byrp_param byrp;
	struct rgbp_param rgbp;
	struct yuvsc_param yuvsc;
	struct mtnr_param mtnr;
	struct msnr_param msnr;
	struct yuvp_param yuvp;
	struct mcs_param mcs;
	struct paf_rdma_param paf;
};

/////////////////////////////////////////////////////////////////////

struct is_fd_rect {
	u32 offset_x;
	u32 offset_y;
	u32 width;
	u32 height;
};

#define MAX_FACE_COUNT 16

struct is_fdae_info {
	u32 id[MAX_FACE_COUNT];
	u32 score[MAX_FACE_COUNT];
	struct is_fd_rect rect[MAX_FACE_COUNT];
	u32 is_rot[MAX_FACE_COUNT];
	u32 rot[MAX_FACE_COUNT];
	u32 face_num;
	u32 frame_count;
	spinlock_t slock;
};

/////////////////////////////////////////////////////////////////////

struct fd_rect {
	float offset_x;
	float offset_y;
	float width;
	float height;
};

struct nfd_info {
	int face_num;
	u32 id[MAX_FACE_COUNT];
	struct fd_rect rect[MAX_FACE_COUNT];
	float score[MAX_FACE_COUNT];
	u32 frame_count;
	float rot[MAX_FACE_COUNT]; /* for NFD v3.0 */
	float yaw[MAX_FACE_COUNT];
	float pitch[MAX_FACE_COUNT];
	u32 width; /* Original width size */
	u32 height; /* Original height size */
	u32 crop_x; /* crop image offset x */
	u32 crop_y; /* crop image offset y */
	u32 crop_w; /* crop width size */
	u32 crop_h; /* crop height size */
	bool ismask[MAX_FACE_COUNT];
	u32 iseye[MAX_FACE_COUNT];
	struct fd_rect eyerect[MAX_FACE_COUNT][2];
	struct fd_rect maskrect[MAX_FACE_COUNT];
};

enum vpl_object_type {
	VPL_OBJECT_BACKGROUND,
	VPL_OBJECT_FACE,
	VPL_OBJECT_TORSO,
	VPL_OBJECT_PERSON,
	VPL_OBJECT_ANIMAL,
	VPL_OBJECT_FOOD,
	VPL_OBJECT_PET_FACE,
	VPL_OBJECT_CHART,
	VPL_OBJECT_MASK,
	VPL_OBJECT_EYE,
	VPL_OBJECT_CLASS_COUNT
};

struct is_od_rect {
	float offset_x;
	float offset_y;
	float width;
	float height;
};

struct od_info {
	int od_num;
	unsigned int id[MAX_FACE_COUNT];
	struct is_od_rect rect[MAX_FACE_COUNT];
	enum vpl_object_type type[MAX_FACE_COUNT];
	float score[MAX_FACE_COUNT];
};

/////////////////////////////////////////////////////////////////////

struct is_region {
	struct is_param_region parameter;
	struct is_fdae_info fdae_info;
	struct nfd_info fd_info;
	struct od_info od_info; /* Object Detect */
	spinlock_t fd_info_slock;
};

struct byrp_param_set {
	struct param_sensor_config sensor_config;
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_dma_input dma_input;
	struct param_otf_output otf_output;
	struct param_dma_output dma_output_byr;

	struct param_dma_output dma_output_pre;
	struct param_dma_output dma_output_cdaf;
	struct param_dma_output dma_output_rgby;
	struct param_dma_output dma_output_ae;
	struct param_dma_output dma_output_awb;

	pdma_addr_t input_dva[IS_MAX_PLANES];
	pdma_addr_t output_dva_byr[IS_MAX_PLANES];

	pdma_addr_t output_dva_pre;
	pdma_addr_t output_dva_cdaf;
	pdma_addr_t output_dva_rgby;
	pdma_addr_t output_dva_ae;
	pdma_addr_t output_dva_awb;

	u32 instance_id;
	u32 fcount;
	u32 mono_mode; /* use mono sensor */
	bool reprocessing;
};

struct rgbp_param_set {
	struct param_sensor_config sensor_config;
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_otf_output otf_output;
	struct param_dma_input dma_input;
	struct param_dma_output dma_output_hist;
	struct param_dma_output dma_output_awb;
	struct param_dma_output dma_output_drc;
	struct param_dma_output dma_output_sat;
	pdma_addr_t input_dva[IS_MAX_PLANES];
	pdma_addr_t output_dva_hist[IS_MAX_PLANES];
	pdma_addr_t output_dva_awb[IS_MAX_PLANES];
	pdma_addr_t output_dva_drc[IS_MAX_PLANES];
	pdma_addr_t output_dva_sat[IS_MAX_PLANES];

	u32 instance;
	u32 fcount;
	u32 tnr_mode;
	u32 mono_mode; /* use mono sensor */
	bool reprocessing;
};

struct yuvsc_param_set {
	struct param_control control;
	struct param_otf_input otf_input;
	struct param_otf_output otf_output;

	u32 instance;
	u32 fcount;
	bool reprocessing;
};

struct mtnr_param_set {
	struct rgbp_param_set *rgbp_param;
	struct param_stripe_input stripe_input;
	/* mtnr0 */
	struct param_otf_input cin_mtnr1_wgt;
	struct param_otf_output cout_msnr_l0;
	struct param_dma_input rdma_cur_l0;
	struct param_dma_input rdma_cur_l4;
	struct param_dma_input rdma_prev_l0;
	struct param_dma_input rdma_prev_l0_wgt;
	struct param_dma_input rdma_mv_geomatch;
	struct param_dma_input rdma_seg_l0;
	struct param_dma_output wdma_prev_l0;
	struct param_dma_output wdma_prev_l0_wgt;
	/* mtnr1 */
	struct param_otf_input cin_dlfe_wgt;
	struct param_otf_output cout_msnr_l1;
	struct param_otf_output cout_msnr_l2;
	struct param_otf_output cout_msnr_l3;
	struct param_otf_output cout_msnr_l4;
	struct param_otf_output cout_mtnr0_wgt;
	struct param_otf_output cout_dlfe_cur;
	struct param_otf_output cout_dlfe_prev;
	struct param_dma_input rdma_cur_l1;
	struct param_dma_input rdma_cur_l2;
	struct param_dma_input rdma_cur_l3;
	struct param_dma_input rdma_prev_l1;
	struct param_dma_input rdma_prev_l2;
	struct param_dma_input rdma_prev_l3;
	struct param_dma_input rdma_prev_l4;
	struct param_dma_input rdma_prev_l1_wgt;
	struct param_dma_input rdma_seg_l1;
	struct param_dma_input rdma_seg_l2;
	struct param_dma_input rdma_seg_l3;
	struct param_dma_input rdma_seg_l4;
	struct param_dma_output wdma_prev_l1;
	struct param_dma_output wdma_prev_l2;
	struct param_dma_output wdma_prev_l3;
	struct param_dma_output wdma_prev_l4;
	struct param_dma_output wdma_prev_l1_wgt;

	pdma_addr_t input_dva_cur_l0[IS_MAX_PLANES];
	pdma_addr_t input_dva_cur_l1[IS_MAX_PLANES];
	pdma_addr_t input_dva_cur_l2[IS_MAX_PLANES];
	pdma_addr_t input_dva_cur_l3[IS_MAX_PLANES];
	pdma_addr_t input_dva_cur_l4[IS_MAX_PLANES];
	pdma_addr_t input_dva_prev_l0[IS_MAX_PLANES];
	pdma_addr_t input_dva_prev_l1[IS_MAX_PLANES];
	pdma_addr_t input_dva_prev_l2[IS_MAX_PLANES];
	pdma_addr_t input_dva_prev_l3[IS_MAX_PLANES];
	pdma_addr_t input_dva_prev_l4[IS_MAX_PLANES];
	pdma_addr_t input_dva_prev_l0_wgt[IS_MAX_PLANES];
	pdma_addr_t input_dva_prev_l1_wgt[IS_MAX_PLANES];
	pdma_addr_t input_dva_mv_geomatch[IS_MAX_PLANES];
	pdma_addr_t input_dva_seg_l0[IS_MAX_PLANES];
	pdma_addr_t input_dva_seg_l1[IS_MAX_PLANES];
	pdma_addr_t input_dva_seg_l2[IS_MAX_PLANES];
	pdma_addr_t input_dva_seg_l3[IS_MAX_PLANES];
	pdma_addr_t input_dva_seg_l4[IS_MAX_PLANES];
	pdma_addr_t output_dva_prev_l0[IS_MAX_PLANES];
	pdma_addr_t output_dva_prev_l1[IS_MAX_PLANES];
	pdma_addr_t output_dva_prev_l2[IS_MAX_PLANES];
	pdma_addr_t output_dva_prev_l3[IS_MAX_PLANES];
	pdma_addr_t output_dva_prev_l4[IS_MAX_PLANES];
	pdma_addr_t output_dva_prev_l0_wgt[IS_MAX_PLANES];
	pdma_addr_t output_dva_prev_l1_wgt[IS_MAX_PLANES];

	u32 instance_id;
	u32 fcount;
	u32 tnr_mode;
	u32 mono_mode; /* use mono sensor */
	bool reprocessing;
};

struct msnr_param_set {
	struct param_stripe_input stripe_input;
	struct param_otf_input cin_msnr_l0;
	struct param_otf_input cin_msnr_l1;
	struct param_otf_input cin_msnr_l2;
	struct param_otf_input cin_msnr_l3;
	struct param_otf_input cin_msnr_l4;
	struct param_otf_output cout_msnr_yuv;
	struct param_otf_output cout_msnr_stat;
	struct param_dma_output wdma_lme;

	pdma_addr_t output_dva_lme[IS_MAX_PLANES];

	u32 instance_id;
	u32 fcount;
	u32 tnr_mode;
	u32 mono_mode; /* use mono sensor */
	bool reprocessing;
};

struct dlfe_param_set {
	struct param_otf_input curr_in;
	struct param_otf_input prev_in;
	struct param_otf_output wgt_out;

	u32 instance;
	u32 fcount;
	u32 crc_seed;
};

struct yuvp_param_set {
	struct mtnr_param_set *mtnr_param;
	struct param_otf_input otf_input;
	struct param_otf_output otf_output;
	struct param_dma_input dma_input;
	struct param_otf_output fto_output;
	struct param_stripe_input stripe_input;
	struct param_dma_input dma_input_seg;
	struct param_dma_input dma_input_drc;
	struct param_dma_input dma_input_clahe;
	struct param_dma_input dma_input_pcchist;
	struct param_dma_output dma_output_pcchist;
	pdma_addr_t input_dva[IS_MAX_PLANES];
	pdma_addr_t input_dva_seg[IS_MAX_PLANES];
	pdma_addr_t input_dva_drc[IS_MAX_PLANES];
	pdma_addr_t input_dva_clahe[IS_MAX_PLANES];
	pdma_addr_t input_dva_pcchist[IS_MAX_PLANES];
	pdma_addr_t output_dva_pcchist[IS_MAX_PLANES];

	u32 instance_id;
	u32 fcount;
	u32 tnr_mode;
	u32 mono_mode; /* use mono sensor */
	bool reprocessing;
};

struct mlsc_param_set {
	struct param_sensor_config sensor_config;
	struct param_otf_input rgbp_otf_input;

	struct param_otf_input otf_input;
	struct param_dma_input dma_input;

	struct param_dma_output dma_output_yuv;
	struct param_dma_output dma_output_glpg[MLSC_GLPG_NUM];
	struct param_dma_output dma_output_svhist;
	struct param_dma_output dma_output_lme_ds;
	struct param_dma_output dma_output_fdpig;
	struct param_dma_output dma_output_cav;

	pdma_addr_t input_dva[IS_MAX_PLANES];
	pdma_addr_t output_dva_yuv[IS_MAX_PLANES];
	pdma_addr_t output_dva_glpg[MLSC_GLPG_NUM][IS_MAX_PLANES];
	pdma_addr_t output_dva_svhist[IS_MAX_PLANES];
	pdma_addr_t output_dva_lme_ds[IS_MAX_PLANES];
	pdma_addr_t output_dva_fdpig[IS_MAX_PLANES];
	pdma_addr_t output_dva_cav[IS_MAX_PLANES];

	u32 instance_id;
	u32 fcount;
	bool reprocessing;
};

struct is_yuvp_config {
	u32 clahe_bypass;
	u32 clahe_grid_num;
	u32 drc_bypass;
	u32 drc_grid_w;
	u32 drc_grid_h;
	u32 drc_grid_enabled_num; // 0~2
	u32 pcchist_bypass;

	u32 drc_contents_aware_isp_en;
	u32 ccm_contents_aware_isp_en;
	u32 pcc_contents_aware_isp_en;
	u32 sharpen_contents_aware_isp_en;

	u32 magic;
};

struct is_mtnr0_config {
	u32 skip_wdma;

	u32 still_last_frame_en;
	u32 L0_bypass;

	u32 mvc_en;
	u32 mvc_in_w;
	u32 mvc_in_h;
	u32 mvc_out_w;
	u32 mvc_out_h;

	u32 geomatchL0_en;
	u32 mixerL0_en;
	u32 mixerL0_mode;
	u32 mixerL0_still_en;
	u32 imgL0_bit;
	u32 wgtL0_bit;

	u32 mixerL0_contents_aware_isp_en;

	u32 magic;
};

struct is_mtnr1_config {
	u32 skip_wdma;

	u32 still_last_frame_en;

	u32 mvc_en;
	u32 mvc_in_w;
	u32 mvc_in_h;
	u32 mvc_out_w;
	u32 mvc_out_h;

	u32 geomatchL1_en;
	u32 mixerL1_en;
	u32 mixerL1_mode;
	u32 mixerL1_still_en;
	u32 imgL1_bit;
	u32 wgtL1_bit;

	u32 geomatchL2_en;
	u32 mixerL2_en;
	u32 mixerL2_mode;
	u32 mixerL2_still_en;
	u32 imgL2_bit;
	u32 wgtL2_bit;

	u32 geomatchL3_en;
	u32 mixerL3_en;
	u32 mixerL3_mode;
	u32 mixerL3_still_en;
	u32 imgL3_bit;
	u32 wgtL3_bit;

	u32 geomatchL4_en;
	u32 mixerL4_en;
	u32 mixerL4_mode;
	u32 mixerL4_still_en;
	u32 imgL4_bit;
	u32 wgtL4_bit;

	u32 mixerL1_contents_aware_isp_en;
	u32 mixerL2_contents_aware_isp_en;
	u32 mixerL3_contents_aware_isp_en;
	u32 mixerL4_contents_aware_isp_en;

	u32 magic;
};

struct is_msnr_config {
	u32 decomp_en;
	u32 decomp_low_power_en;

	u32 lmeds_input_select; // 0: PrmdRecon Out Y L0, 1: MSNR Img Out Y L0
	u32 lmeds_bypass;
	u32 lmeds_stride;
	u32 lmeds_w;
	u32 lmeds_h;

	u32 msnrL0_contents_aware_isp_en;
	u32 msnrL1_contents_aware_isp_en;
	u32 msnrL2_contents_aware_isp_en;
	u32 msnrL3_contents_aware_isp_en;
	u32 msnrL4_contents_aware_isp_en;

	u32 magic;
};

struct djag_xfilter_dejagging_coeff_config {
	u32 xfilter_dejagging_weight0;
	u32 xfilter_dejagging_weight1;
	u32 xfilter_hf_boost_weight;
	u32 center_hf_boost_weight;
	u32 diagonal_hf_boost_weight;
	u32 center_weighted_mean_weight;
};

struct djag_thres_1x5_matching_config {
	u32 thres_1x5_matching_sad;
	u32 thres_1x5_abshf;
};

struct djag_thres_shooting_detect_config {
	u32 thres_shooting_llcrr;
	u32 thres_shooting_lcr;
	u32 thres_shooting_neighbor;
	u32 thres_shooting_uucdd;
	u32 thres_shooting_ucd;
	u32 min_max_weight;
};

struct djag_lfsr_seed_config {
	u32 lfsr_seed_0;
	u32 lfsr_seed_1;
	u32 lfsr_seed_2;
};

struct djag_dither_config {
	u32 dither_value[9];
	u32 sat_ctrl;
	u32 dither_sat_thres;
	u32 dither_thres;
	u32 dither_wb_thres;
	u32 dither_black_level;
	u32 dither_white_level;
};

struct djag_cp_config {
	u32 cp_hf_thres;
	u32 cp_arbi_max_cov_offset;
	u32 cp_arbi_max_cov_shift;
	u32 cp_arbi_denom;
	u32 cp_arbi_mode;
};

struct djag_harris_det_config {
	u32 harris_k;
	u32 harris_th;
};

struct djag_harris_bilateral_c_config {
	u32 bilateral_c;
};

struct predjag_config {
	bool djag_en;

	/* Setfile tuning parameters for DJAG
	 * 0 : Scaling ratio = x1.0
	 * 1 : Scaling ratio = x1.1~x1.4
	 * 2 : Scaling ratio = x1.5~x2.0
	 * 3 : Scaling ratio = x2.1~
	 */

	struct djag_xfilter_dejagging_coeff_config
		xfilter_dejagging_coeff_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_thres_1x5_matching_config thres_1x5_matching_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_thres_shooting_detect_config
		thres_shooting_detect_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_lfsr_seed_config lfsr_seed_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_dither_config dither_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_cp_config cp_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_harris_det_config harris_det_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_harris_bilateral_c_config bilateral_c_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
};

struct djag_config {
	bool djag_en;

	/* Setfile tuning parameters for DJAG
	 * 0 : Scaling ratio = x1.0
	 * 1 : Scaling ratio = x1.1~x1.4
	 * 2 : Scaling ratio = x1.5~x2.0
	 * 3 : Scaling ratio = x2.1~
	 */

	struct djag_xfilter_dejagging_coeff_config
		xfilter_dejagging_coeff_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_thres_1x5_matching_config thres_1x5_matching_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_thres_shooting_detect_config
		thres_shooting_detect_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_lfsr_seed_config lfsr_seed_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_dither_config dither_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
	struct djag_cp_config cp_cfg[MAX_DJAG_SCALING_RATIO_INDEX];
};

struct hf_config {
	u32 recomp_bypass;

	u32 hf_weight;
	u32 biquad_factor_a;
	u32 biquad_factor_b;
	bool gain_enable;
	bool gain_increment;
	u32 gain_shift_adder;
};

struct scaler_filter_hcoef {
	int h_coef_a[9];
	int h_coef_b[9];
	int h_coef_c[9];
	int h_coef_d[9];
	int h_coef_e[9];
	int h_coef_f[9];
	int h_coef_g[9];
	int h_coef_h[9];
};

struct scaler_filter_vcoef {
	int v_coef_a[9];
	int v_coef_b[9];
	int v_coef_c[9];
	int v_coef_d[9];
};

struct scaler_coef_config {
	/* Setfile tuning parameters for sc_coef
	 * 0 : x8/8, Ratio <= 1048576 (~8:8)
	 * 1 : x7/8, 1048576 < Ratio <= 1198373 (~7/8)
	 * 2 : x6/8, 1198373 < Ratio <= 1398101 (~6/8)
	 * 3 : x5/8, 1398101 < Ratio <= 1677722 (~5/8)
	 * 4 : x4/8, 1677722 < Ratio <= 2097152 (~4/8)
	 * 5 : x3/8, 2097152 < Ratio <= 2796203 (~3/8)
	 * 6 : x2/8, 2796203 < Ratio <= 4194304 (~2/8)
	 */
	struct scaler_filter_hcoef poly_sc_h_coef[MAX_SCALER_SCALING_RATIO_INDEX];
	struct scaler_filter_vcoef poly_sc_v_coef[MAX_SCALER_SCALING_RATIO_INDEX];
	struct scaler_filter_hcoef post_sc_h_coef[MAX_SCALER_SCALING_RATIO_INDEX];
	struct scaler_filter_vcoef post_sc_v_coef[MAX_SCALER_SCALING_RATIO_INDEX];
};

struct scaler_bchs_config {
	/* contents for Full/Narrow mode
	 * 0 : SCALER_OUTPUT_YUV_RANGE_FULL
	 * 1 : SCALER_OUTPUT_YUV_RANGE_NARROW
	 */
	/* Brightness/Contrast control param */
	u32 y_offset[MAX_MODE_INDEX];
	u32 y_gain[MAX_MODE_INDEX];
	/* Hue/Saturation control param */
	u32 c_gain00[MAX_MODE_INDEX];
	u32 c_gain01[MAX_MODE_INDEX];
	u32 c_gain10[MAX_MODE_INDEX];
	u32 c_gain11[MAX_MODE_INDEX];
	/* clamp param */
	u32 y_max[MAX_MODE_INDEX];
	u32 y_min[MAX_MODE_INDEX];
	u32 c_max[MAX_MODE_INDEX];
	u32 c_min[MAX_MODE_INDEX];
};

struct is_mcsc_config {
	struct predjag_config predjag;
	struct djag_config djag;

	struct hf_config hf;

	struct scaler_coef_config sc_coef;
	struct scaler_bchs_config sc_bchs;

	u32 magic;
};

#endif /* IS_PARAMS_H */
