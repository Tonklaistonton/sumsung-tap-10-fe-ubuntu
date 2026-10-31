/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos CAMERA-PP GDC HDR10+ driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_GDC_HDR10P_H_
#define CAMERAPP_GDC_HDR10P_H_

#include <linux/videodev2.h>
#include <linux/dma-buf.h>
#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
#include <media/videobuf2-dma-sg.h>
#endif

struct gdc_hdr10p_param {
	u32 en;
	u32 config;
	u32 stat_buf_fd;
	u32 stat_buf_size;
	u32 buf_index;
};

struct gdc_hdr10p_ctx {
	struct gdc_hdr10p_param param;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t hdr10p_buf;
	unsigned int hdr10p_buf_size;
	__u32 flags;
};

int gdc_hdr10p_buf_prepare(void *gdc_ctx, u32 buffer_index);
void gdc_hdr10p_buf_finish(void *gdc_ctx, u32 buffer_index);
void gdc_hdr10p_set_ext_ctrls(void *gdc_ctx, struct v4l2_ext_control *ext_ctrl);
void gdc_hdr10p_fill_curr_frame(void *gdc_dev, void *gdc_ctx, u32 param_index);

#endif /* CAMERAPP_GDC_HDR10P_H_ */
