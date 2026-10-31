/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo video node functions
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_FRAME_MGR_CFG_H
#define IS_FRAME_MGR_CFG_H

#include "is-param.h"
#include "exynos-is-sensor.h"

#define MAX_FRAME_INFO (4)

#define MAX_OUT_LOGICAL_NODE (1)
#define MAX_CAP_LOGICAL_NODE (4)

enum is_frame_info_index { INFO_FRAME_START, INFO_CONFIG_LOCK, INFO_FRAME_END_PROC };

struct is_frame_info {
	int cpu;
	int pid;
	unsigned long long when;
};

struct is_stripe_size {
	/* Horizontal pixel ratio how much stripe processing is done. */
	u32 h_pix_ratio;
	/* x offset position for sub-frame */
	u32 offset_x;
	/* crop x position for sub-frame */
	u32 crop_x;
	/* stripe width for sub-frame */
	u32 crop_width;
	/* left margin for sub-frame */
	u32 left_margin;
	/* right margin for sub-frame */
	u32 right_margin;
};

enum pablo_repeat_scenario {
	PABLO_REPEAT_NO,
	PABLO_REPEAT_YUVP_CLAHE,
};

struct pablo_repeat_info {
	u32 num;
	u32 idx;
	enum pablo_repeat_scenario scenario;
};

struct is_stripe_info {
	/* Region index. */
	u32 region_id;
	/* Total region num. */
	u32 region_num;
	/* Horizontal pixel count which stripe processing is done for. */
	u32 h_pix_num[MAX_STRIPE_REGION_NUM];
	/* stripe start position for sub-frame */
	u32 start_pos_x;
	/* stripe width for sub-frame */
	u32 stripe_width;
	/* sub-frame size for incrop/otcrop */
	struct is_stripe_size in;
	struct is_stripe_size out;
#ifdef USE_KERNEL_VFS_READ_WRITE
	/* For image dump */
	ulong kva[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
	size_t size[MAX_STRIPE_REGION_NUM][IS_MAX_PLANES];
#endif
};

struct is_sub_frame {
	/* Target address for all input node
	 * 0: invalid address, DMA should be disabled
	 * any value: valid address
	 */
	u32 id;
	u32 num_planes;
	u32 hw_pixeltype;
	ulong kva[IS_MAX_PLANES];
	dma_addr_t dva[IS_MAX_PLANES];
};

struct is_sub_node {
	u32 hw_id;
	struct is_sub_frame sframe[CAPTURE_NODE_MAX];
};

/**
 * struct is_frame_time_cfg - frame time configuration
 * @vvalid: sensor vvalid time (usec)
 * @line_ratio: Proportion of line_time on vvalid (%)
 * @post_frame_gap: required time for CMDQ post_frame_gap (usec)
 */
struct is_frame_time_cfg {
	u32 vvalid;
	u32 line_ratio;
	u32 post_frame_gap;
};

struct is_frame {
	struct list_head list;
	struct kthread_work work;
	struct kthread_delayed_work dwork;
	void *group;
	void *subdev; /* is_subdev */
	u32 hw_slot_id[HW_SLOT_MAX];
	u32 ldr_hw_slot_id;

	/* group leader use */
	struct camera2_shot_ext *shot_ext;
	struct camera2_shot *shot;
	size_t shot_size;

	struct is_vb2_buf *vbuf;

	/* dva for ICPU */
	dma_addr_t shot_dva;

	/* stream use */
	struct camera2_stream *stream;

	/* common use */
	u32 planes; /* total planes include multi-buffers */
	struct is_priv_buf *pb_output;
	dma_addr_t dvaddr_buffer[IS_MAX_PLANES];
	ulong kvaddr_buffer[IS_MAX_PLANES];
	size_t size[IS_MAX_PLANES];

	/*
	 * target address for capture node
	 * [0] invalid address, stop
	 * [others] valid address
	 */
	dma_addr_t dva_ssvc[CSI_VIRTUAL_CH_MAX][IS_MAX_PLANES]; /* Sensor LVN */

	dma_addr_t dva_mlsc_yuv444[IS_MAX_PLANES]; /* MLSC YUV WDMA */
	dma_addr_t dva_mlsc_glpg[MLSC_GLPG_NUM][IS_MAX_PLANES]; /* MLSC GLPG WDMA */
	dma_addr_t dva_mlsc_svhist[IS_MAX_PLANES]; /* MLSC SVHIST WDMA */
	dma_addr_t dva_mlsc_lmeds[IS_MAX_PLANES]; /* MLSC LMEDS WDMA */
	dma_addr_t dva_mlsc_fdpig[IS_MAX_PLANES]; /* MLSC FDPIG WDMA */
	dma_addr_t dva_mlsc_cav[IS_MAX_PLANES]; /* MLSC CAV WDMA */

	dma_addr_t lmesTargetAddress[IS_MAX_PLANES]; /* LME prev IN DMA */
	u64 lmeskTargetAddress[IS_MAX_PLANES]; /* LME prev IN KVA */
	dma_addr_t lmecTargetAddress[IS_MAX_PLANES]; /* LME MV OUT DMA */
	dma_addr_t lmesadTargetAddress[IS_MAX_PLANES]; /* LME SAD OUT DMA */
	dma_addr_t ixscmapTargetAddress[IS_MAX_PLANES]; /* DNS SC MAP RDMA */
	dma_addr_t ypnrdsTargetAddress[IS_MAX_PLANES]; /* YPP Y/UV L2 RDMA */
	dma_addr_t ypdgaTargetAddress[IS_MAX_PLANES]; /* YPP DRC GAIN RDMA */
	dma_addr_t ypclaheTargetAddress[IS_MAX_PLANES]; /* YPP CLAHE RDMA */
	dma_addr_t sc0TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc1TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc2TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc3TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc4TargetAddress[IS_MAX_PLANES];
	dma_addr_t sc5TargetAddress[IS_MAX_PLANES];

	dma_addr_t dva_mtnr1_out_cur_l1_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_cur_l2_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_cur_l3_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_cur_l4_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr0_out_prev_l0_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_prev_l1_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_prev_l2_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_prev_l3_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_prev_l4_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr0_out_mv_geomatch[IS_MAX_PLANES];
	dma_addr_t dva_mtnr0_out_prev_l0_wgt[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_prev_l1_wgt[IS_MAX_PLANES];
	dma_addr_t dva_mtnr0_out_seg_l0[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_seg_l1[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_seg_l2[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_seg_l3[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_out_seg_l4[IS_MAX_PLANES];
	dma_addr_t dva_mtnr0_cap_l0_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_cap_l1_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_cap_l2_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_cap_l3_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_cap_l4_yuv[IS_MAX_PLANES];
	dma_addr_t dva_mtnr0_cap_l0_wgt[IS_MAX_PLANES];
	dma_addr_t dva_mtnr1_cap_l1_wgt[IS_MAX_PLANES];

	dma_addr_t dva_msnr_cap_lme[IS_MAX_PLANES];

	dma_addr_t dva_rgbp_hist[IS_MAX_PLANES]; /* RGBP HIST WDMA*/
	dma_addr_t dva_rgbp_awb[IS_MAX_PLANES]; /* RGBP AWB WDMA */
	dma_addr_t dva_rgbp_drc[IS_MAX_PLANES]; /* RGBP DRC WDMA*/
	dma_addr_t dva_rgbp_sat[IS_MAX_PLANES]; /* RGBP SAT WDMA */
	dma_addr_t dva_byrp_byr[IS_MAX_PLANES]; /* BYRP DNG WDMA */
	dma_addr_t dva_byrp_hdr[IS_MAX_PLANES]; /* BYRP HDR RDMA */
	dma_addr_t dva_shrp_hf[IS_MAX_PLANES]; /* SHRP HF RDMA */
	dma_addr_t dva_shrp_seg[IS_MAX_PLANES]; /* SHRP SEG CONF RDMA */

	dma_addr_t dva_yuvp_out_pcchist[IS_MAX_PLANES]; /* YPP PCC HIST RDMA */
	dma_addr_t dva_yuvp_cap_pcchist[IS_MAX_PLANES]; /* YPP PCC HIST WDMA */

	u64 kva_mtnr0_rta_info[IS_MAX_PLANES];
	u64 kva_mtnr1_rta_info[IS_MAX_PLANES];
	u64 kva_msnr_rta_info[IS_MAX_PLANES];
	u64 kva_yuvp_rta_info[IS_MAX_PLANES];
	u64 kva_shrp_rta_info[IS_MAX_PLANES];
	u64 kva_lme_rta_info[IS_MAX_PLANES];
	u64 kva_mcsc_rta_info[IS_MAX_PLANES];

	/* Logical node information */
	struct is_sub_node cap_node;

	/* multi-buffer use */
	/* total number of buffers per frame */
	u32 num_buffers;
	/* current processed buffer index */
	u32 cur_buf_index;

	/* internal use */
	unsigned long mem_state;
	u32 state;
	u32 fcount;
	u32 rcount;
	u32 index;
	u32 result;
	unsigned long out_flag;
	IS_DECLARE_PMAP(pmap);
	struct is_param_region *parameter;

	struct is_frame_info frame_info[MAX_FRAME_INFO];
	u32 instance; /* device instance */
	u32 type;
	unsigned long core_flag;

#ifdef MEASURE_TIME
	/* time measure externally */
	struct timespec64 *tzone;
	/* time measure internally */
	struct is_monitor mpoint[TMS_END];
#endif

	/* for draw digit */
	u32 width;
	u32 height;

	struct pablo_repeat_info repeat_info;
	struct is_stripe_info stripe_info;

	struct is_frame_time_cfg time_cfg;

	u32 sensor_rms_crop_ratio;
};

#endif
