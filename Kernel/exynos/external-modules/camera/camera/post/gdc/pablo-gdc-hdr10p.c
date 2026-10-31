// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS ISPP GDC driver
 * (FIMC-IS PostProcessing Generic Distortion Correction driver)
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "pablo-gdc.h"
#include "pablo-gdc-hdr10p.h"

static int gdc_hdr10p_wdma_buf_map(void *gdc_ctx, const u32 param_index)
{
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;
	struct gdc_hdr10p_ctx *hdr10p_ctx = &ctx->hdr10p_ctx[param_index];
	int dmafd = hdr10p_ctx->param.stat_buf_fd;
	int err = 0;

	hdr10p_ctx->dmabuf = dma_buf_get(dmafd);

	if (IS_ERR(hdr10p_ctx->dmabuf)) {
		err = PTR_ERR(hdr10p_ctx->dmabuf);
		gdc_dev_info(ctx->gdc_dev->dev,
			"[ERROR] Failed to 1) get %d (fd: %d)\n", err, dmafd);
		goto err_buf_get;
	}

	gdc_dev_dbg(ctx->gdc_dev->dev,
		"dvaddr - fd(%d) addr = %p (%s)\n",
		dmafd, hdr10p_ctx->dmabuf, hdr10p_ctx->dmabuf->exp_name);

	hdr10p_ctx->attachment = dma_buf_attach(hdr10p_ctx->dmabuf, ctx->gdc_dev->dev);
	gdc_dev_dbg(ctx->gdc_dev->dev, "hdr10p_ctx->attachment:%p\n", hdr10p_ctx->attachment);

	if (IS_ERR(hdr10p_ctx->attachment)) {
		err = PTR_ERR(hdr10p_ctx->attachment);
		gdc_dev_info(ctx->gdc_dev->dev,
			"[ERROR] Failed to 2) attach %d (fd: %d)\n", err, dmafd);
		goto err_buf_attach;
	}

	hdr10p_ctx->sgt = pkv_dma_buf_map_attachment(hdr10p_ctx->attachment, DMA_TO_DEVICE);
	gdc_dev_dbg(ctx->gdc_dev->dev, "hdr10p_ctx->sgt:%p\n", hdr10p_ctx->sgt);

	if (IS_ERR(hdr10p_ctx->sgt)) {
		err = PTR_ERR(hdr10p_ctx->sgt);
		gdc_dev_info(ctx->gdc_dev->dev,
			"[ERROR] Failed to 3) map_attachment %d (fd: %d)\n", err, dmafd);
		goto err_buf_attachment;
	}

	gdc_dev_dbg(ctx->gdc_dev->dev, "end normally.\n");
	return 0;

err_buf_attachment:
	dma_buf_detach(hdr10p_ctx->dmabuf, hdr10p_ctx->attachment);
err_buf_attach:
	dma_buf_put(hdr10p_ctx->dmabuf);
err_buf_get:
	hdr10p_ctx->dmabuf = NULL;
	hdr10p_ctx->attachment = NULL;
	hdr10p_ctx->sgt = NULL;

	return err;
}

static void gdc_hdr10p_wdma_buf_unmap(void *gdc_ctx, const u32 param_index)
{
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;
	struct gdc_hdr10p_ctx *hdr10p_ctx = &ctx->hdr10p_ctx[param_index];

	if (unlikely(!hdr10p_ctx->dmabuf)) {
		gdc_dev_info(ctx->gdc_dev->dev, "[ERROR] hdr10p_ctx->dmabuf is null.\n");
		return;
	}

	pkv_dma_buf_unmap_attachment(hdr10p_ctx->attachment, hdr10p_ctx->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(hdr10p_ctx->dmabuf, hdr10p_ctx->attachment);
	dma_buf_put(hdr10p_ctx->dmabuf);

	hdr10p_ctx->sgt = NULL;
	hdr10p_ctx->attachment = NULL;
	hdr10p_ctx->dmabuf = NULL;
}

#if IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
static void gdc_vb2_buf_sync(struct gdc_ctx *ctx,
			     int (*cpu_access_operation)(struct dma_buf *dma_buf,
							 enum dma_data_direction dir),
			     u32 param_index)
{
	struct gdc_hdr10p_ctx *hdr10p_ctx = &ctx->hdr10p_ctx[param_index];
	unsigned int flags = hdr10p_ctx->flags;
	bool cache_clean = !(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
	bool cache_invalidate = !(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);

	if (cache_clean == false && cache_invalidate == false)
		return;

	if (cache_clean)
		cpu_access_operation(hdr10p_ctx->dmabuf, DMA_TO_DEVICE);

	if (cache_invalidate)
		cpu_access_operation(hdr10p_ctx->dmabuf, DMA_FROM_DEVICE);
}
#endif

int gdc_hdr10p_buf_prepare(void *gdc_ctx, u32 buffer_index)
{
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;
	u32 param_index = GDC_BUF_IDX_TO_PARAM_IDX(buffer_index);
	int ret = 0;

	gdc_dev_dbg(ctx->gdc_dev->dev,
		"buffer_index:%u, param_index:%u\n", buffer_index, param_index);

	if (!ctx->gdc_dev->has_hdr10p) {
		gdc_dev_dbg(ctx->gdc_dev->dev, "GDC node does not have HDR10p.\n");
		return 0;
	}

	if (!ctx->hdr10p_ctx[param_index].param.en) {
		gdc_dev_dbg(ctx->gdc_dev->dev, "HDR10p is disabled.\n");
		return 0;
	}

	ret = gdc_hdr10p_wdma_buf_map(ctx, param_index);
	if (ret) {
		gdc_dev_info(ctx->gdc_dev->dev, "[ERROR] fail gdc_hdr10p_wdma_buf_map.(%d)\n", ret);
		ctx->hdr10p_ctx[param_index].param.en = 0;
		return ret;
	}
#if IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	gdc_vb2_buf_sync(ctx, dma_buf_end_cpu_access, param_index);
#endif

	return 0;
}

void gdc_hdr10p_buf_finish(void *gdc_ctx, u32 buffer_index)
{
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;
	u32 param_index = GDC_BUF_IDX_TO_PARAM_IDX(buffer_index);

	gdc_dev_dbg(ctx->gdc_dev->dev,
		"buffer_index:%u, param_index:%u\n", buffer_index, param_index);

	if (!ctx->gdc_dev->has_hdr10p) {
		gdc_dev_dbg(ctx->gdc_dev->dev, "GDC node does not have HDR10p.\n");
		return;
	}

	if (!ctx->hdr10p_ctx[param_index].param.en) {
		gdc_dev_dbg(ctx->gdc_dev->dev, "HDR10p is disabled.\n");
		return;
	}

#if IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	gdc_vb2_buf_sync(ctx, dma_buf_begin_cpu_access, param_index);
#endif
	gdc_hdr10p_wdma_buf_unmap(ctx, param_index);
}

void gdc_hdr10p_set_ext_ctrls(void *gdc_ctx, struct v4l2_ext_control *ext_ctrl)
{
	int ret = 0;
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;
	struct gdc_dev *gdc = ctx->gdc_dev;
	struct gdc_hdr10p_param *hdr10p_param_temp_save;
	struct gdc_hdr10p_param *hdr10p_param_real_save;
	u32 param_index;

	if (!gdc->has_hdr10p) {
		gdc_dev_dbg(gdc->dev, "GDC node does not have HDR10p.\n");
		return;
	}

	hdr10p_param_temp_save = &ctx->hdr10p_ctx[0].param;
	ret = gdc->sys_ops->copy_from_user(hdr10p_param_temp_save, ext_ctrl->ptr,
					   sizeof(struct gdc_hdr10p_param));
	param_index = GDC_BUF_IDX_TO_PARAM_IDX(hdr10p_param_temp_save->buf_index);
	gdc_dev_dbg(gdc->dev,
		"buf_index=%u, param_index=%u\n",
		hdr10p_param_temp_save->buf_index, param_index);

	/*
	 * Copy memory to keep the data for multi-buffer
	 * To support both of multi-buffer and legacy,
	 * hdr10p_param[buffer index + 1] will be used.
	 * e.g.
	 * (multi-buffer) buffer index is 0...n and use hdr10p_param[1...n+1]
	 * (not multi-buffer) buffer index is 0 and use hdr10p_param[1]
	 */
	hdr10p_param_real_save = &ctx->hdr10p_ctx[param_index].param;
	memcpy(hdr10p_param_real_save, hdr10p_param_temp_save, sizeof(struct gdc_hdr10p_param));

	gdc_dev_dbg(gdc->dev, "hdr10p_param : (en:%u, config:%u, fd:%u, size:%u, buf_index:%u)\n",
		hdr10p_param_real_save->en, hdr10p_param_real_save->config,
		hdr10p_param_real_save->stat_buf_fd, hdr10p_param_real_save->stat_buf_size,
		hdr10p_param_real_save->buf_index);
}

void gdc_hdr10p_fill_curr_frame(void *gdc_dev, void *gdc_ctx, u32 param_index)
{
	struct gdc_dev *gdc = (struct gdc_dev *)gdc_dev;
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;
	struct gdc_hdr10p_ctx *hdr10p_ctx = &ctx->hdr10p_ctx[param_index];
	struct gdc_hdr10p_param *hdr10p_param = &hdr10p_ctx->param;

	gdc_dev_dbg(gdc->dev, "param_index:%u, hdr10p_ctx:%p\n", param_index, hdr10p_ctx);

	if (!gdc->has_hdr10p) {
		gdc_dev_dbg(gdc->dev, "GDC node does not have HDR10p.\n");
		return;
	}

	if (!hdr10p_param->en) {
		gdc_dev_dbg(gdc->dev, "HDR10p is disabled.\n");
		return;
	}

	if (unlikely(!hdr10p_ctx->sgt)) {
		gdc_dev_info(gdc->dev, "[ERROR] hdr10p_ctx->sgt is null.\n");
		hdr10p_param->en = 0;
		return;
	}

	hdr10p_ctx->hdr10p_buf = (dma_addr_t)sg_dma_address(hdr10p_ctx->sgt->sgl);
	hdr10p_ctx->hdr10p_buf_size = hdr10p_param->stat_buf_size;

	gdc_dev_dbg(gdc->dev,
		"[hdr10p_ctx->hdr10p_buf] addr %pa, size %#x\n",
		&hdr10p_ctx->hdr10p_buf, hdr10p_ctx->hdr10p_buf_size);
}
