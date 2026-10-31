/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung EXYNOS CAMERA PostProcessing dof driver
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-sg.h>
#include <media/v4l2-ioctl.h>

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include "pablo-dof.h"
#include "pablo-hw-api-dof.h"
#include "pablo-kernel-variant.h"
#include "is-common-enum.h"
#include "is-video.h"

#include "pmio.h"
#include "pablo-device-iommu-group.h"
#include "pablo-irq.h"

static int dof_dma_buf_map(
	struct dof_ctx *ctx, enum DOF_BUF_TYPE type, int dmafd, bool is_need_kva);
static void dof_dma_buf_unmap(struct dof_ctx *ctx, int type);

/* Flags that are set by us */
#define V4L2_BUFFER_MASK_FLAGS                                                                     \
	(V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR |  \
		V4L2_BUF_FLAG_PREPARED | V4L2_BUF_FLAG_IN_REQUEST | V4L2_BUF_FLAG_REQUEST_FD |     \
		V4L2_BUF_FLAG_TIMESTAMP_MASK)
/* Output buffer flags that should be passed on to the driver */
#define V4L2_BUFFER_OUT_FLAGS                                                                      \
	(V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME | V4L2_BUF_FLAG_KEYFRAME |                    \
		V4L2_BUF_FLAG_TIMECODE)

#define DOF_V4L2_DEVICE_CAPS (V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING)

dma_addr_t dof_get_dva_from_sg(struct dof_ctx *ctx, int type)
{
	if (!ctx->sgt[type])
		return 0;

	return (dma_addr_t)sg_dma_address(ctx->sgt[type]->sgl);
};

static dma_addr_t dof_get_dma_address(struct vb2_buffer *vb2_buf, u32 plane)
{
	struct sg_table *sgt;

	sgt = vb2_dma_sg_plane_desc(vb2_buf, plane);
	if (!sgt)
		return 0;

	return (dma_addr_t)sg_dma_address(sgt->sgl);
}

static struct pablo_dof_dma_buf_ops dof_dma_buf_ops = {
	.attach = dma_buf_attach,
	.detach = dma_buf_detach,
	.map_attachment = pkv_dma_buf_map_attachment,
	.unmap_attachment = pkv_dma_buf_unmap_attachment,
	.vmap = pkv_dma_buf_vmap,
	.vunmap = pkv_dma_buf_vunmap,
	.get_dva = dof_get_dva_from_sg,
	.get_dma_address = dof_get_dma_address,
};

static int dof_debug_level = DOF_DEBUG_OFF;
module_param_named(dof_debug_level, dof_debug_level, uint, 0644);

static uint dof_debug_rdmo;
module_param_named(dof_debug_rdmo, dof_debug_rdmo, uint, 0644);

static uint dof_debug_wrmo;
module_param_named(dof_debug_wrmo, dof_debug_wrmo, uint, 0644);

static ulong debug_dof;
static int param_get_debug_dof(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DOF debug features\n");
	ret += sprintf(buffer + ret, "\tb[0] : Dump SFR (0x1)\n");
	ret += sprintf(buffer + ret, "\tb[1] : Dump SFR Once (0x2)\n");
	ret += sprintf(buffer + ret, "\tb[2] : S2D (0x4)\n");
	ret += sprintf(buffer + ret, "\tb[3] : Shot & H/W latency (0x8)\n");
	ret += sprintf(buffer + ret, "\tb[4] : PMIO APB-DIRECT (0x10)\n");
	ret += sprintf(buffer + ret, "\tb[5] : Dump PMIO Cache Buffer (0x20)\n");
	ret += sprintf(buffer + ret, "\tcurrent value : 0x%lx\n", debug_dof);

	return ret;
}

static const struct kernel_param_ops param_ops_debug_dof = {
	.set = param_set_ulong,
	.get = param_get_debug_dof,
};

module_param_cb(debug_dof, &param_ops_debug_dof, &debug_dof, 0644);

static int dof_suspend(struct device *dev);
static int dof_resume(struct device *dev);

struct vb2_dof_buffer {
	struct v4l2_m2m_buffer mb;
	struct dof_ctx *ctx;
	ktime_t ktime;
};

static const struct dof_fmt dof_formats[] = {
	{
		.name = "GREY",
		.pixelformat = V4L2_PIX_FMT_GREY,
		.cfg_val = DOF_CFG_FMT_GREY,
		.bitperpixel = { 8, 0, 0 },
		.num_planes = 1,
		.num_comp = 1,
		.h_shift = 1,
		.v_shift = 1,
	},
};

int dof_get_debug_level(void)
{
	return dof_debug_level;
}

uint dof_get_debug_rdmo(void)
{
	return dof_debug_rdmo;
}

uint dof_get_debug_wrmo(void)
{
	return dof_debug_wrmo;
}

/* Find the matches format */
static const struct dof_fmt *dof_find_format(u32 pixfmt)
{
	const struct dof_fmt *dof_fmt;
	unsigned long i;

	dof_dbg("[DOF]\n");
	for (i = 0; i < ARRAY_SIZE(dof_formats); ++i) {
		dof_fmt = &dof_formats[i];
		if (dof_fmt->pixelformat == pixfmt)
			return &dof_formats[i];
	}

	return NULL;
}

static int dof_v4l2_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	dof_dbg("[DOF]\n");
	strscpy(cap->driver, DOF_MODULE_NAME, sizeof(cap->driver));
	strscpy(cap->card, DOF_MODULE_NAME, sizeof(cap->card));

	cap->capabilities =
		V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	cap->capabilities |= V4L2_CAP_DEVICE_CAPS;
	cap->device_caps = DOF_V4L2_DEVICE_CAPS;

	return 0;
}

static int dof_v4l2_g_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);
	const struct dof_fmt *dof_fmt;
	struct dof_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int i;

	dof_dbg("[DOF]\n");

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	dof_fmt = frame->dof_fmt;

	pixm->width = frame->width;
	pixm->height = frame->height;
	pixm->pixelformat = frame->pixelformat;
	pixm->field = V4L2_FIELD_NONE;
	pixm->num_planes = frame->dof_fmt->num_planes;
	pixm->colorspace = 0;

	for (i = 0; i < pixm->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width * dof_fmt->bitperpixel[i]) >> 3;

		pixm->plane_fmt[i].sizeimage = pixm->plane_fmt[i].bytesperline * pixm->height;

		dof_dbg("[DOF] [%d] plane: bytesperline %d, sizeimage %d\n", i,
			pixm->plane_fmt[i].bytesperline, pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int dof_v4l2_try_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);
	const struct dof_fmt *dof_fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct dof_frame *frame;
	int i;

	dof_dbg("[DOF]\n");

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		v4l2_err(&ctx->dof_dev->m2m.v4l2_dev, "not supported v4l2 type\n");
		return -EINVAL;
	}

	dof_fmt = dof_find_format(f->fmt.pix_mp.pixelformat);
	if (!dof_fmt) {
		v4l2_err(&ctx->dof_dev->m2m.v4l2_dev,
			"not supported format type, pixelformat(%c%c%c%c)\n",
			(char)((f->fmt.pix_mp.pixelformat >> 0) & 0xFF),
			(char)((f->fmt.pix_mp.pixelformat >> 8) & 0xFF),
			(char)((f->fmt.pix_mp.pixelformat >> 16) & 0xFF),
			(char)((f->fmt.pix_mp.pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	dof_dbg("[DOF] num_planes:%d", dof_fmt->num_planes);
	for (i = 0; i < dof_fmt->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width * dof_fmt->bitperpixel[i]) >> 3;
		pixm->plane_fmt[i].sizeimage = pixm->plane_fmt[i].bytesperline * pixm->height;

		dof_info("[DOF] type:(%d) %s [%d] plane: bytesperline %d, sizeimage %d\n", f->type,
			(V4L2_TYPE_IS_OUTPUT(f->type) ? "input" : "output"), i,
			pixm->plane_fmt[i].bytesperline, pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int dof_v4l2_s_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)

{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	struct dof_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	const struct dof_size_limit *limit = &ctx->dof_dev->variant->limit_input;
	int i, ret = 0;

	dof_dbg("[DOF]\n");

	if (vb2_is_streaming(vq)) {
		v4l2_err(&ctx->dof_dev->m2m.v4l2_dev, "device is busy\n");
		return -EBUSY;
	}

	ret = dof_v4l2_try_fmt_mplane(file, fh, f);
	if (ret < 0)
		return ret;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	set_bit(CTX_PARAMS, &ctx->flags);

	frame->dof_fmt = dof_find_format(f->fmt.pix_mp.pixelformat);
	if (!frame->dof_fmt) {
		v4l2_err(&ctx->dof_dev->m2m.v4l2_dev, "not supported format values\n");
		return -EINVAL;
	}

	frame->num_planes = (pixm->num_planes < DOF_MAX_PLANES) ? pixm->num_planes : DOF_MAX_PLANES;

	for (i = 0; i < frame->num_planes; i++)
		frame->bytesused[i] = pixm->plane_fmt[i].sizeimage;

	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		(pixm->width * pixm->height > limit->max_w * limit->max_h)) {
		v4l2_err(&ctx->dof_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too large(%dx%d)\n", pixm->width,
			pixm->height, limit->max_w, limit->max_h);
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		(pixm->width * pixm->height < limit->min_w * limit->min_h)) {
		v4l2_err(&ctx->dof_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too small(%dx%d)\n", pixm->width,
			pixm->height, limit->min_w, limit->min_h);
		return -EINVAL;
	}

	frame->width = pixm->width;
	frame->height = pixm->height;
	frame->pixelformat = pixm->pixelformat;

	/* dof dosn't have width, height info for s_fmt. so this is dof_dbg */
	dof_dbg("[DOF][%s] pixelformat(%c%c%c%c) size(%dx%d)\n",
		(V4L2_TYPE_IS_OUTPUT(f->type) ? "input" : "output"),
		(char)((frame->dof_fmt->pixelformat >> 0) & 0xFF),
		(char)((frame->dof_fmt->pixelformat >> 8) & 0xFF),
		(char)((frame->dof_fmt->pixelformat >> 16) & 0xFF),
		(char)((frame->dof_fmt->pixelformat >> 24) & 0xFF), frame->width, frame->height);

	dof_dbg("[DOF] num_planes(%d)\n", frame->num_planes);
	for (i = 0; i < frame->num_planes; i++)
		dof_dbg("[DOF] bytesused[%d] = %d\n", i, frame->bytesused[i]);

	return 0;
}

static int dof_v4l2_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);

	dof_dbg("[DOF] count(%d), type(%d), memory(%d)\n", reqbufs->count, reqbufs->type,
		reqbufs->memory);
	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int dof_v4l2_querybuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);

	dof_dbg("[DOF] index(%d), type(%d), memory(%d), m.planes(%p), length(%d)\n", buf->index,
		buf->type, buf->memory, buf->m.planes, buf->length);

	return v4l2_m2m_querybuf(file, ctx->m2m_ctx, buf);
}

static int dof_check_vb2_qbuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;
	struct vb2_plane planes[VB2_MAX_PLANES];
	struct dma_buf *dbuf;
	int plane;
	int ret = 0;

	dof_dbg("[DOF]\n");
	if (q->fileio) {
		dof_info("[ERR] file io in progress\n");
		ret = -EBUSY;
		goto q_err;
	}

	if (b->type != q->type) {
		dof_info("[ERR] buf type is invalid(%d != %d)\n", b->type, q->type);
		ret = -EINVAL;
		goto q_err;
	}

	if (b->index >= q->num_buffers) {
		dof_info("[ERR] buffer index out of range b_index(%d) q_num_buffers(%d)\n",
			b->index, q->num_buffers);
		ret = -EINVAL;
		goto q_err;
	}

	if (q->bufs[b->index] == NULL) {
		/* Should never happen */
		dof_info("[ERR] buffer is NULL\n");
		ret = -EINVAL;
		goto q_err;
	}

	if (b->memory != q->memory) {
		dof_info("[ERR] invalid memory type b_mem(%d) q_mem(%d)\n", b->memory, q->memory);
		ret = -EINVAL;
		goto q_err;
	}

	vb = q->bufs[b->index];
	if (!vb) {
		dof_info("[ERR] vb is NULL");
		ret = -EINVAL;
		goto q_err;
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(b->type)) {
		/* Is memory for copying plane information present? */
		if (b->m.planes == NULL) {
			dof_info(
				"[ERR] multi-planar buffer passed but planes array not provided\n");
			ret = -EINVAL;
			goto q_err;
		}

		if (b->length < vb->num_planes || b->length > VB2_MAX_PLANES) {
			dof_info("[ERR] incorrect planes array length, expected %d, got %d\n",
				vb->num_planes, b->length);
			ret = -EINVAL;
			goto q_err;
		}
	}

	if ((b->flags & V4L2_BUF_FLAG_REQUEST_FD) && vb->state != VB2_BUF_STATE_DEQUEUED) {
		dof_info("[ERR] buffer is not in dequeued state\n");
		ret = -EINVAL;
		goto q_err;
	}

	/* For detect vb2 framework err, operate some vb2 functions */
	memset(planes, 0, sizeof(planes[0]) * vb->num_planes);
	/* Copy relevant information provided by the userspace */
	ret = call_bufop(vb->vb2_queue, fill_vb2_buffer, vb, planes);
	if (ret) {
		dof_info("[ERR] vb2_fill_vb2_v4l2_buffer failed (%d)\n", ret);
		goto q_err;
	}

	for (plane = 0; plane < vb->num_planes; ++plane) {
		dbuf = dma_buf_get(planes[plane].m.fd);
		if (IS_ERR_OR_NULL(dbuf)) {
			dof_info("[ERR] invalid dmabuf fd(%d) for plane %d\n", planes[plane].m.fd,
				plane);
			ret = -EINVAL;
			goto q_err;
		}

		if (planes[plane].length == 0)
			planes[plane].length = (unsigned int)dbuf->size;

		if (planes[plane].length < vb->planes[plane].min_length) {
			dof_info(
				"[ERR] invalid dmabuf length %u for plane %d, minimum length %u length %u\n",
				planes[plane].length, plane, vb->planes[plane].min_length,
				vb->planes[plane].length);
			ret = -EINVAL;
			dma_buf_put(dbuf);
			goto q_err;
		}
		dma_buf_put(dbuf);
	}
q_err:
	return ret;
}

static int dof_check_qbuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx, struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;

	dof_dbg("[DOF]\n");
	vq = v4l2_m2m_get_vq(m2m_ctx, buf->type);
	if (!V4L2_TYPE_IS_OUTPUT(vq->type) && (buf->flags & V4L2_BUF_FLAG_REQUEST_FD)) {
		dof_info("[ERR] requests cannot be used with capture buffers\n");
		return -EPERM;
	}
	return dof_check_vb2_qbuf(vq, buf);
}

static int dof_v4l2_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);
	int ret;

	dof_dbg("[DOF] buf->type:%d, %s\n", buf->type,
		V4L2_TYPE_IS_OUTPUT(buf->type) ? "input" : "output");

	if (!ctx->dof_dev->use_hw_cache_operation) {
		/* Save flags for cache sync of V4L2_MEMORY_DMABUF */
		if (V4L2_TYPE_IS_OUTPUT(buf->type))
			ctx->s_frame.flags = buf->flags;
		else
			ctx->d_frame.flags = buf->flags;
	}

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
	if (ret) {
		dev_err(ctx->dof_dev->dev, "v4l2_m2m_qbuf failed ret(%d) check(%d)\n", ret,
			dof_check_qbuf(file, ctx->m2m_ctx, buf));
	}

	return ret;
}

static int dof_v4l2_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);

	dof_dbg("[DOF] buf->type:%d, %s\n", buf->type,
		V4L2_TYPE_IS_OUTPUT(buf->type) ? "input" : "output");

	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int dof_power_clk_enable(struct dof_dev *dof)
{
	int ret;

	dof_info("[DOF]\n");
	if (in_interrupt())
		ret = pm_runtime_get(dof->dev);
	else
		ret = pm_runtime_get_sync(dof->dev);

	if (ret < 0) {
		dev_err(dof->dev, "%s=%d: Failed to enable local power\n", __func__, ret);
		return ret;
	}

	if (!IS_ERR(dof->pclk)) {
		ret = clk_prepare_enable(dof->pclk);
		if (ret) {
			dev_err(dof->dev, "%s: Failed to enable PCLK (err %d)\n", __func__, ret);
			goto err_pclk;
		}
	}

	if (!IS_ERR(dof->aclk)) {
		ret = clk_enable(dof->aclk);
		if (ret) {
			dev_err(dof->dev, "%s: Failed to enable ACLK (err %d)\n", __func__, ret);
			goto err_aclk;
		}
	}

	return 0;
err_aclk:
	if (!IS_ERR(dof->pclk))
		clk_disable_unprepare(dof->pclk);
err_pclk:
	pm_runtime_put(dof->dev);

	return ret;
}

static void dof_power_clk_disable(struct dof_dev *dof)
{
	dof_info("[DOF]\n");
	camerapp_hw_dof_stop(dof->pmio);

	if (!IS_ERR(dof->aclk))
		clk_disable_unprepare(dof->aclk);

	if (!IS_ERR(dof->pclk))
		clk_disable_unprepare(dof->pclk);

	pm_runtime_put(dof->dev);
}

static struct pablo_dof_v4l2_ops dof_v4l2_ops = {
	.m2m_streamon = v4l2_m2m_streamon,
	.m2m_streamoff = v4l2_m2m_streamoff,
};

static int dof_v4l2_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);
	struct dof_dev *dof = ctx->dof_dev;
	struct dof_frame *frame;
	int ret;
	int i, j;

	dof_info("[DOF] type:%d, %s\n", type, V4L2_TYPE_IS_OUTPUT(type) ? "input" : "output");
	frame = ctx_get_frame(ctx, type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	for (i = 0; i < DOF_MAX_BUFS; ++i) {
		for (j = 0; j < DOF_MAX_PLANES; ++j)
			frame->fd[i][j] = -1;
	}

	if (!V4L2_TYPE_IS_OUTPUT(type)) {
		mutex_lock(&dof->m2m.lock);
		if (!test_bit(CTX_DEV_READY, &ctx->flags)) {
			if (!atomic_read(&dof->m2m.in_streamon)) {
				ret = dof_power_clk_enable(dof);
				if (ret) {
					dev_err(dof->dev, "dof_power_clk_enable fail\n");
					mutex_unlock(&dof->m2m.lock);
					return ret;
				}
				ret = dof->hw_dof_ops->prepare(dof);
				if (ret) {
					dev_err(dof->dev, "camerapp_hw_dof_prepare fail\n");
					dof_power_clk_disable(dof);
					mutex_unlock(&dof->m2m.lock);
					return ret;
				}
			}
			atomic_inc(&dof->m2m.in_streamon);
			set_bit(CTX_DEV_READY, &ctx->flags);
		}
		mutex_unlock(&dof->m2m.lock);
	}

	return dof->v4l2_ops->m2m_streamon(file, ctx->m2m_ctx, type);
}

static int dof_v4l2_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(fh);
	struct dof_dev *dof = ctx->dof_dev;

	dof_info("[DOF] type:%d, %s\n", type, V4L2_TYPE_IS_OUTPUT(type) ? "input" : "output");
	if (!V4L2_TYPE_IS_OUTPUT(type)) {
		mutex_lock(&dof->m2m.lock);
		if (test_bit(CTX_DEV_READY, &ctx->flags)) {
			atomic_dec(&dof->m2m.in_streamon);
			if (!atomic_read(&dof->m2m.in_streamon)) {
				if (dof->hw_dof_ops->sw_reset(dof->pmio))
					dev_err(dof->dev, "dof_sw_reset fail\n");

				if (dof->hw_dof_ops->wait_idle(dof->pmio))
					dev_err(dof->dev, "dof_hw_wait_idle fail\n");

				dof_power_clk_disable(dof);

				atomic_set(&dof->frame_cnt.fs, 0);
				atomic_set(&dof->frame_cnt.fe, 0);
			}
			clear_bit(CTX_DEV_READY, &ctx->flags);
		}
		mutex_unlock(&dof->m2m.lock);
	}

	return dof->v4l2_ops->m2m_streamoff(file, ctx->m2m_ctx, type);
}

static int dof_v4l2_s_ctrl(struct file *file, void *priv, struct v4l2_control *ctrl)
{
	int ret = 0;

	dof_dbg("[DOF] v4l2_s_ctrl = %d (%d)\n", ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_CAMERAPP_MODEL_CONTROL:
		break;
	case V4L2_CID_CAMERAPP_PERFRAME_CONTROL:
		break;
	default:
		ret = -EINVAL;

		dof_dbg("Err: Invalid ioctl id(%d)\n", ctrl->id);
		break;
	}

	return ret;
}

static int dof_dma_buf_map(struct dof_ctx *ctx, enum DOF_BUF_TYPE type, int dmafd, bool is_need_kva)
{
	struct dof_dev *dof = ctx->dof_dev;
	struct device *dev = ctx->dof_dev->dev;
	int err;

	dof_dbg("[DOF] type:%d, fd:%d\n", type, dmafd);
	ctx->dmabuf[type] = dma_buf_get(dmafd);
	if (IS_ERR(ctx->dmabuf[type])) {
		err = PTR_ERR(ctx->dmabuf[type]);
		dof_info("[ERR] Failed to 1) get %d (type: %d, fd: %d)\n", err, type, dmafd);
		goto err_buf_get;
	}

	dof_dbg("[DOF] dvaddr - fd(%d) type %d, addr = %p (%s)\n", dmafd, type, ctx->dmabuf[type],
		ctx->dmabuf[type]->exp_name);

	ctx->attachment[type] = dof->dma_buf_ops->attach(ctx->dmabuf[type], dev);
	if (IS_ERR(ctx->attachment[type])) {
		err = PTR_ERR(ctx->attachment[type]);
		dof_info("[ERR] Failed to 2) attach %d (type: %d, fd: %d)\n", err, type, dmafd);
		goto err_buf_attach;
	}

	ctx->sgt[type] = dof->dma_buf_ops->map_attachment(ctx->attachment[type], DMA_TO_DEVICE);
	if (IS_ERR(ctx->sgt[type])) {
		err = PTR_ERR(ctx->sgt[type]);
		dof_info("[ERR] Failed to 3) map_attachement %d (type: %d, fd: %d)\n", err, type,
			dmafd);
		goto err_buf_attachment;
	}

	if (is_need_kva == true) {
		ctx->kvaddr[type] = (ulong)dof->dma_buf_ops->vmap(ctx->dmabuf[type]);
		if (!ctx->kvaddr[type]) {
			err = -ENOMEM;
			dof_info("[ERR] Failed to 4) buf_vmap (type: %d, fd: %d)\n", type, dmafd);
			goto err_buf_vmap;
		}
		dof_dbg("kvaddr - fd(%d) type %d, addr = %#lx\n", dmafd, type, ctx->kvaddr[type]);
	}

	return 0;

err_buf_vmap:
	dof->dma_buf_ops->unmap_attachment(
		ctx->attachment[type], ctx->sgt[type], DMA_BIDIRECTIONAL);
err_buf_attachment:
	dof->dma_buf_ops->detach(ctx->dmabuf[type], ctx->attachment[type]);
err_buf_attach:
	dma_buf_put(ctx->dmabuf[type]);
err_buf_get:
	ctx->dmabuf[type] = NULL;
	ctx->attachment[type] = NULL;
	ctx->sgt[type] = NULL;

	return err;
}

static void dof_dma_buf_unmap(struct dof_ctx *ctx, int type)
{
	struct dof_dev *dof = ctx->dof_dev;

	dof_dbg("[DOF]\n");
	if (!ctx->dmabuf[type])
		return;

	if (ctx->kvaddr[type]) {
		dof->dma_buf_ops->vunmap(ctx->dmabuf[type], (void *)(ctx->kvaddr[type]));
		ctx->kvaddr[type] = 0;
	}

	dof->dma_buf_ops->unmap_attachment(
		ctx->attachment[type], ctx->sgt[type], DMA_BIDIRECTIONAL);
	dof->dma_buf_ops->detach(ctx->dmabuf[type], ctx->attachment[type]);
	dma_buf_put(ctx->dmabuf[type]);

	ctx->sgt[type] = NULL;
	ctx->attachment[type] = NULL;
	ctx->dmabuf[type] = NULL;

	return;
}

static void dof_dma_buf_unmap_all(struct dof_ctx *ctx)
{
	int type;

	dof_dbg("[DOF]\n");

	for (type = 0; type < DOF_DMA_COUNT; type++) {
		if (ctx->dmabuf[type])
			dof_dma_buf_unmap(ctx, type);
	}
}

static int dof_update_model_address(struct dof_ctx *ctx, struct dof_model_param *model_param)
{
	struct dof_dev *dof = ctx->dof_dev;
	int ret;

	dof_dbg("[DOF]\n");

	/* instruction */
	if (ctx->model_addr.dva_instruction) {
		dof_dma_buf_unmap(ctx, DOF_BUF_INSTRUCTION);
		ctx->model_addr.dva_instruction = 0;
	}
	ret = dof_dma_buf_map(ctx, DOF_BUF_INSTRUCTION, model_param->instruction.address, true);
	if (ret) {
		dof_info("[DOF][ERR] fail dof_dma_buf_map(INSTRUCTION). ret:%d\n", ret);
		ctx->model_addr.dva_instruction = 0;
	} else {
		ctx->model_addr.dva_instruction =
			dof->dma_buf_ops->get_dva(ctx, DOF_BUF_INSTRUCTION);
		ctx->model_addr.instruction_size = model_param->instruction.size;
	}

	/* constant */
	if (ctx->model_addr.dva_constant) {
		dof_dma_buf_unmap(ctx, DOF_BUF_CONSTANT);
		ctx->model_addr.dva_constant = 0;
	}
	ret = dof_dma_buf_map(ctx, DOF_BUF_CONSTANT, model_param->constant.address, false);
	if (ret) {
		dof_info("[DOF][ERR] fail dof_dma_buf_map(CONSTANT). ret:%d\n", ret);
		ctx->model_addr.dva_constant = 0;
	} else {
		ctx->model_addr.dva_constant = dof->dma_buf_ops->get_dva(ctx, DOF_BUF_CONSTANT);
		ctx->model_addr.constant_size = model_param->constant.size;
	}

	/* temporary */
	if (ctx->model_addr.dva_temporary) {
		dof_dma_buf_unmap(ctx, DOF_BUF_TEMPORARY);
		ctx->model_addr.dva_temporary = 0;
	}
	if (model_param->temporary.size) {
		ret = dof_dma_buf_map(
			ctx, DOF_BUF_TEMPORARY, model_param->temporary.address, false);
		if (ret) {
			dof_info("[DOF][ERR] fail dof_dma_buf_map(TEMP). ret:%d\n", ret);
			ctx->model_addr.dva_temporary = 0;
		} else {
			ctx->model_addr.dva_temporary =
				dof->dma_buf_ops->get_dva(ctx, DOF_BUF_TEMPORARY);
			ctx->model_addr.temporary_size = model_param->temporary.size;
		}
	} else {
		dof_info("[DOF] Temp buffer isn't used.\n");
		ctx->model_addr.dva_temporary = 0;
	}
	return ret;
}

static void dof_update_model_address_offset(struct dof_ctx *ctx)
{
	struct dof_model_addr *model_addr = &ctx->model_addr;
	struct dof_perframe_control_params *perframe_control_params = &ctx->perframe_control_params;

	dof_dbg("[DOF][BF] dva-inst:0x%llx, const:0x%llx\n", model_addr->dva_instruction,
		model_addr->dva_constant);

	dof_dbg("[DOF][ADD] offset-inst:%d, const:%d\n", perframe_control_params->inst_offset,
		perframe_control_params->const_offset);

	if (model_addr->dva_instruction) {
		model_addr->dva_instruction_with_offset =
			model_addr->dva_instruction + perframe_control_params->inst_offset;
	}

	if (model_addr->dva_constant) {
		model_addr->dva_constant_with_offset =
			model_addr->dva_constant + perframe_control_params->const_offset;
	}
	dof_dbg("[DOF][AF] dva-inst:0x%llx, const:0x%llx\n",
		model_addr->dva_instruction_with_offset, model_addr->dva_constant_with_offset);
}

static void dof_print_cache(struct dof_ctx *ctx)
{
	dof_info("[DOF] CACHE: only %s (%d)\n", ctx->dof_dev->use_hw_cache_operation ? "HW" : "SW",
		ctx->dof_dev->dev->dma_coherent);
}

static void dof_print_model_param(struct dof_model_param *model_param)
{
	dof_info("[DOF][model_%d] instruction(%d, %d), constant(%d, %d), temporary(%d, %d)\n",
		 model_param->model_id, model_param->instruction.address,
		 model_param->instruction.size, model_param->constant.address,
		 model_param->constant.size, model_param->temporary.address,
		 model_param->temporary.size);

	dof_info("[DOF] time_out(%dms x %d = %d) msec, clock_level(%d), clock_mode(%d)\n",
		DOF_TIMEOUT_MSEC, model_param->time_out, DOF_TIMEOUT_MSEC * model_param->time_out,
		model_param->clock_level, model_param->clock_mode);

	dof_info("[DOF] enable_reg_dump(%d), dof_debug_info(index:%d, status:%d, num_reg:%d)\n",
		model_param->enable_reg_dump, model_param->debug_info.buffer_index,
		model_param->debug_info.device_status, model_param->debug_info.regs.num_reg);
}

static void dof_print_perframe_control_param(
	struct dof_perframe_control_params *perframe_control_params)
{
	dof_info("[DOF][model%d][F%d] roi(%dx%d)\n", perframe_control_params->model_id,
		 perframe_control_params->frame_count, perframe_control_params->roi_width,
		 perframe_control_params->roi_height);

	dof_info("[DOF][model] const(size:%d, checksum:%d), inst(size:%d, checksum:%d)\n",
		 perframe_control_params->const_size, perframe_control_params->const_checksum,
		 perframe_control_params->inst_size, perframe_control_params->inst_checksum);

	dof_info("[DOF][model] const_offset(%d), inst_offset(%d), prev(fd:%d, size:%d)\n",
		 perframe_control_params->const_offset, perframe_control_params->inst_offset,
		 perframe_control_params->prev_fd, perframe_control_params->prev_size);

	dof_info("[DOF][OUT] pstate(fd:%d, size:%d), nstate(fd:%d, size:%d)\n",
		 perframe_control_params->pstate_fd, perframe_control_params->pstate_size,
		 perframe_control_params->nstate_fd, perframe_control_params->nstate_size);

	dof_info("[DOF] performance_mode(%d, %s), operation_mode(%d, %s)\n",
		 perframe_control_params->performance_mode,
		 perframe_control_params->performance_mode ? "Enable" : "Disable",
		 perframe_control_params->operation_mode,
		 GET_OP_MODE_STRING(perframe_control_params->operation_mode));
}

static void dof_print_fill_current_frame(struct dof_frame *s_frame, struct dof_frame *d_frame)
{
	dof_info("[curr_in] addr %pa, size %d\n", &s_frame->addr.curr_in,
		s_frame->addr.curr_in_size);
	dof_info("[prev_in] addr %pa, size %d\n", &s_frame->addr.prev_in,
		s_frame->addr.prev_in_size);
	dof_info("[prev_state] addr %pa, size %d\n", &s_frame->addr.prev_state,
		s_frame->addr.prev_state_size);
	dof_info("[output] addr %pa, size %d\n", &d_frame->addr.output, d_frame->addr.output_size);
	dof_info("[next_state] addr %pa, size %d\n", &d_frame->addr.next_state,
		d_frame->addr.next_state_size);
}

static void dof_print_instruction_info(struct dof_ctx *ctx)
{
	unsigned char *buffer;

	dof_info("[DOF] instruction_offset dva:%llx, inst_offset:%d",
		 ctx->model_addr.dva_instruction_with_offset,
		 ctx->perframe_control_params.inst_offset);

	buffer = (unsigned char *)ctx->kvaddr[DOF_BUF_INSTRUCTION] +
		 ctx->perframe_control_params.inst_offset;

	dof_info("[DOF] kva buffer:%p", buffer);

	dof_info("[DOF][ 0] %02x %02x %02x %02x %02x %02x %02x %02x\n", buffer[0], buffer[1],
		 buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);

	dof_info("[DOF][ 8] %02x %02x %02x %02x %02x %02x %02x %02x\n", buffer[8], buffer[9],
		 buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15]);

	dof_info("[DOF][16] %02x %02x %02x %02x %02x %02x %02x %02x\n", buffer[16], buffer[17],
		 buffer[18], buffer[19], buffer[20], buffer[21], buffer[22], buffer[23]);

	dof_info("[DOF][24] %02x %02x %02x %02x %02x %02x %02x %02x\n", buffer[24], buffer[25],
		 buffer[26], buffer[27], buffer[28], buffer[29], buffer[30], buffer[31]);
}

static void dof_print_debugging_log(struct dof_ctx *ctx)
{
	dof_info("[DOF][FS:%d][FE:%d] print debugging info\n",
		 atomic_read(&ctx->dof_dev->frame_cnt.fs),
		 atomic_read(&ctx->dof_dev->frame_cnt.fe));
	dof_print_cache(ctx);
	dof_print_model_param(&ctx->model_param);
	dof_print_perframe_control_param(&ctx->perframe_control_params);
	dof_print_fill_current_frame(&ctx->s_frame, &ctx->d_frame);
	camerapp_hw_dof_print_dma_address(&ctx->s_frame, &ctx->d_frame, &ctx->model_addr);
	dof_print_instruction_info(ctx);
}

static struct pablo_dof_sys_ops dof_sys_ops = {
	.copy_from_user = copy_from_user,
};

static int dof_v4l2_s_ext_ctrls(struct file *file, void *priv, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct dof_ctx *ctx = fh_to_dof_ctx(file->private_data);
	struct dof_dev *dof = ctx->dof_dev;
	struct dof_model_param *model_param;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;

	dof_dbg("[DOF]\n");

	BUG_ON(!ctx);

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		dof_dbg("ctrl ID:%d\n", ext_ctrl->id);
		switch (ext_ctrl->id) {
		case V4L2_CID_CAMERAPP_MODEL_CONTROL: /* videodev2_exynos_camera.h */
			dof_dbg("V4L2_CID_CAMERAPP_MODEL_CONTROL(%d) ptr %p, size %lu\n",
				V4L2_CID_CAMERAPP_MODEL_CONTROL, ext_ctrl->ptr,
				sizeof(struct dof_model_param));
			ret = dof->sys_ops->copy_from_user(
				&ctx->model_param, ext_ctrl->ptr, sizeof(struct dof_model_param));
			if (ret) {
				dev_err(dof->dev, "copy_from_user is fail(%d)\n", ret);
				goto p_err;
			}
			model_param = &ctx->model_param;

			if (model_param->time_out) {
				model_param->time_out = QUOTIENT_TO_500(model_param->time_out);
			} else {
				dof_info("[DOF] Use default time_out\n");
				model_param->time_out = DOF_WDT_CNT;
			}

			dof_print_model_param(&ctx->model_param);
			dof_update_model_address(ctx, model_param);
			dof_print_cache(ctx);

			dof->enable_sw_workaround = model_param->enable_sw_workaround;
			break;
		case V4L2_CID_CAMERAPP_PERFRAME_CONTROL:
			dof_dbg("V4L2_CID_CAMERAPP_PERFRAME_CONTROL(%d) ptr %p, size %lu\n",
				V4L2_CID_CAMERAPP_PERFRAME_CONTROL, ext_ctrl->ptr,
				sizeof(struct dof_perframe_control_params));
			ret = dof->sys_ops->copy_from_user(&ctx->perframe_control_params,
				ext_ctrl->ptr, sizeof(struct dof_perframe_control_params));
			if (ret) {
				dev_err(dof->dev, "copy_from_user is fail(%d)\n", ret);
				goto p_err;
			}

			if (dof_get_debug_level())
				dof_print_perframe_control_param(&ctx->perframe_control_params);
			break;
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = dof_v4l2_s_ctrl(file, ctx, &ctrl);
			if (ret) {
				dof_dbg("dof_v4l2_s_ctrl is fail(%d)\n", ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

static int dof_v4l2_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	int ret = 0;

	dof_dbg("[DOF] v4l2_g_ctrl = %d (%d)\n", ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_CAMERAPP_MODEL_CONTROL:
		break;
	case V4L2_CID_CAMERAPP_PERFRAME_CONTROL:
		break;
	default:
		ret = -EINVAL;

		dof_dbg("Err: Invalid ioctl id(%d)\n", ctrl->id);
		break;
	}
	return ret;
}

int dof_v4l2_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct dof_ctx *ctx = fh_to_dof_ctx(file->private_data);
	struct dof_dev *dof = ctx->dof_dev;
	struct dof_model_param *model_param;
	struct dof_debug_info *debug_info;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;

	dof_dbg("[DOF]\n");

	BUG_ON(!ctx);

	if (ctrls->which != V4L2_CTRL_CLASS_CAMERA) {
		dof_info("[dof][ERR]invalid control class(%d)\n", ctrls->which);
		return -EINVAL;
	}

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		dof_dbg("ctrl ID:%d\n", ext_ctrl->id);
		switch (ext_ctrl->id) {
		case V4L2_CID_CAMERAPP_PERFRAME_CONTROL: /* videodev2_exynos_camera.h */
			model_param = &ctx->model_param;
			dof_dbg("[DOF]enable_reg_dump(%d)\n", model_param->enable_reg_dump);

			debug_info = &model_param->debug_info;
			dof_dbg("V4L2_CID_CAMERAPP_PERFRAME_CONTROL(%d) ptr %p, size %lu\n",
				V4L2_CID_CAMERAPP_PERFRAME_CONTROL, ext_ctrl->ptr,
				sizeof(struct dof_debug_info));
			ret = copy_to_user(
				ext_ctrl->ptr, debug_info, sizeof(struct dof_debug_info));
			if (ret) {
				dev_err(dof->dev, "copy_to_user is fail(%d)\n", ret);
				goto p_err;
			}
			break;
		case V4L2_CID_CAMERAPP_MODEL_CONTROL:
			break;
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = dof_v4l2_g_ctrl(file, ctx, &ctrl);
			if (ret) {
				dof_dbg("dof_v4l2_g_ctrl is fail(%d)\n", ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

static const struct v4l2_ioctl_ops dof_v4l2_ioctl_ops = {
	.vidioc_querycap = dof_v4l2_querycap,

	.vidioc_g_fmt_vid_cap_mplane = dof_v4l2_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane = dof_v4l2_g_fmt_mplane,

	.vidioc_try_fmt_vid_cap_mplane = dof_v4l2_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane = dof_v4l2_try_fmt_mplane,

	.vidioc_s_fmt_vid_cap_mplane = dof_v4l2_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane = dof_v4l2_s_fmt_mplane,

	.vidioc_reqbufs = dof_v4l2_reqbufs,
	.vidioc_querybuf = dof_v4l2_querybuf,

	.vidioc_qbuf = dof_v4l2_qbuf,
	.vidioc_dqbuf = dof_v4l2_dqbuf,

	.vidioc_streamon = dof_v4l2_streamon,
	.vidioc_streamoff = dof_v4l2_streamoff,

	.vidioc_s_ctrl = dof_v4l2_s_ctrl,
	.vidioc_s_ext_ctrls = dof_v4l2_s_ext_ctrls,

	.vidioc_g_ctrl = dof_v4l2_g_ctrl,
	.vidioc_g_ext_ctrls = dof_v4l2_g_ext_ctrls,
};

static int dof_ctx_stop_req(struct dof_ctx *ctx)
{
	struct dof_ctx *curr_ctx;
	struct dof_dev *dof = ctx->dof_dev;
	int ret = 0;

	dof_dbg("[DOF]\n");
	curr_ctx = v4l2_m2m_get_curr_priv(dof->m2m.m2m_dev);
	if (!test_bit(CTX_RUN, &ctx->flags) || (curr_ctx != ctx))
		return 0;

	set_bit(CTX_ABORT, &ctx->flags);

	ret = wait_event_timeout(dof->wait, !test_bit(CTX_RUN, &ctx->flags), DOF_TIMEOUT);

	if (ret == 0) {
		dev_err(dof->dev, "device failed to stop request\n");
		ret = -EBUSY;
	}

	return ret;
}

static int dof_vb2_queue_setup(struct vb2_queue *vq, unsigned int *num_buffers,
	unsigned int *num_planes, unsigned int sizes[], struct device *alloc_devs[])
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vq);
	struct dof_frame *frame;
	int i;

	dof_dbg("[DOF]\n");
	frame = ctx_get_frame(ctx, vq->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	/* Get number of planes from format_list in driver */
	*num_planes = frame->num_planes;
	dof_dbg("[DOF] num_planes:%d\n", *num_planes);
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = frame->bytesused[i];
		alloc_devs[i] = ctx->dof_dev->dev;
	}

	return 0;
}

static void dof_vb2_buf_sync(struct vb2_buffer *vb, struct dof_frame *frame, int action)
{
	unsigned int flags = frame->flags;
	bool cache_clean = !(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
	bool cache_invalidate = !(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);
	int i, num_planes;

	dof_dbg("%s: action(%d), clean(%d), invalidate(%d)\n",
		V4L2_TYPE_IS_OUTPUT(vb->type) ? "input" : "output", action, cache_clean,
		cache_invalidate);

	if (cache_clean == false && cache_invalidate == false)
		return;

	num_planes = (vb->num_planes > DOF_MAX_PLANES) ? DOF_MAX_PLANES : vb->num_planes;
	for (i = 0; i < num_planes; ++i) {
		dof_dbg("index(%d), fd(%d), dbuf_mapped(%d)\n", vb->index, vb->planes[i].m.fd,
			vb->planes[i].dbuf_mapped);
		if (action == DOF_BUF_FINISH) {
			if (cache_clean)
				dma_buf_begin_cpu_access(vb->planes[i].dbuf, DMA_TO_DEVICE);
			if (cache_invalidate)
				dma_buf_begin_cpu_access(vb->planes[i].dbuf, DMA_FROM_DEVICE);
		} else {
			/* Check if it is a newly attached buffer */
			if (frame->fd[vb->index][i] != vb->planes[i].m.fd) {
				frame->fd[vb->index][i] = vb->planes[i].m.fd;
				continue;
			}

			if (cache_clean)
				dma_buf_end_cpu_access(vb->planes[i].dbuf, DMA_TO_DEVICE);
			if (cache_invalidate)
				dma_buf_end_cpu_access(vb->planes[i].dbuf, DMA_FROM_DEVICE);
		}
	}
}

static int dof_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct dof_frame *frame;
	struct dof_perframe_control_params *perframe_control_params;
	int i, ret = 0;

	dof_dbg("[DOF]\n");
	frame = ctx_get_frame(ctx, vb->vb2_queue->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		for (i = 0; i < frame->dof_fmt->num_planes; i++)
			vb2_set_plane_payload(vb, i, frame->bytesused[i]);
	} else {
		perframe_control_params = &ctx->perframe_control_params;
		if (perframe_control_params->prev_fd >= 0) {
			ret = dof_dma_buf_map(
				ctx, DOF_BUF_PREV_IN, perframe_control_params->prev_fd, false);
			if (ret) {
				dof_info("[DOF][ERR] fail dof_dma_buf_map(PREV_IN). ret:%d\n", ret);
				goto p_err;
			}
		}
		if (perframe_control_params->pstate_fd >= 0) {
			ret = dof_dma_buf_map(
				ctx, DOF_BUF_PREV_STATE, perframe_control_params->pstate_fd, false);
			if (ret) {
				dof_info("[DOF][ERR] fail dof_dma_buf_map(PREV_STATE). ret:%d\n",
					ret);
				goto p_err;
			}
		}
		if (perframe_control_params->nstate_fd >= 0) {
			ret = dof_dma_buf_map(
				ctx, DOF_BUF_NEXT_STATE, perframe_control_params->nstate_fd, false);
			if (ret) {
				dof_info("[DOF][ERR] fail dof_dma_buf_map(NEXT_STATE). ret:%d\n",
					ret);
				goto p_err;
			}
		}
	}

	if (!ctx->dof_dev->use_hw_cache_operation)
		dof_vb2_buf_sync(vb, frame, DOF_BUF_PREPARE);
p_err:
	return ret;
}

static void dof_vb2_buf_finish(struct vb2_buffer *vb)
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct dof_frame *frame;

	dof_dbg("[DOF]\n");
	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type) &&
		unlikely(test_bit(DOF_DBG_TIME, &debug_dof)))
		dof_info("index(%d) shot_time %lld us, hw_time %lld us\n", vb->index,
			ctx->time_dbg.shot_time_stamp, ctx->time_dbg.hw_time_stamp);

	frame = ctx_get_frame(ctx, vb->vb2_queue->type);
	if (!ctx->dof_dev->use_hw_cache_operation && frame)
		dof_vb2_buf_sync(vb, frame, DOF_BUF_FINISH);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		dof_dma_buf_unmap(ctx, DOF_BUF_PREV_IN);
		dof_dma_buf_unmap(ctx, DOF_BUF_PREV_STATE);
		dof_dma_buf_unmap(ctx, DOF_BUF_NEXT_STATE);
	}
}

static void dof_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	dof_dbg("[DOF]\n");
	if (ctx->m2m_ctx)
		v4l2_m2m_buf_queue(ctx->m2m_ctx, v4l2_buf);
}

static void dof_vb2_buf_cleanup(struct vb2_buffer *vb)
{
	dof_dbg("[DOF]\n");
	/* No operation */
}

static void dof_vb2_lock(struct vb2_queue *vq)
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vq);

	dof_dbg("[DOF]\n");
	mutex_lock(&ctx->dof_dev->lock);
}

static void dof_vb2_unlock(struct vb2_queue *vq)
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vq);

	dof_dbg("[DOF]\n");
	mutex_unlock(&ctx->dof_dev->lock);
}

static void dof_cleanup_queue(struct dof_ctx *ctx)
{
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	dof_dbg("[DOF]\n");
	while (v4l2_m2m_num_src_bufs_ready(ctx->m2m_ctx) > 0) {
		src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		dof_dbg("src_index(%d)\n", src_vb->vb2_buf.index);
	}

	while (v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) > 0) {
		dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);
		dof_dbg("dst_index(%d)\n", dst_vb->vb2_buf.index);
	}
}

static int dof_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vq);

	dof_dbg("[DOF]\n");
	set_bit(CTX_STREAMING, &ctx->flags);

	return 0;
}

static void dof_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct dof_ctx *ctx = vb2_get_drv_priv(vq);
	int ret;

	dof_dbg("[DOF]\n");
	ret = dof_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->dof_dev->dev, "wait timeout\n");

	clear_bit(CTX_STREAMING, &ctx->flags);

	/* release all queued buffers in multi-buffer scenario*/
	dof_cleanup_queue(ctx);
}

static const struct vb2_ops dof_vb2_ops = {
	.queue_setup = dof_vb2_queue_setup,
	.buf_prepare = dof_vb2_buf_prepare,
	.buf_finish = dof_vb2_buf_finish,
	.buf_queue = dof_vb2_buf_queue,
	.buf_cleanup = dof_vb2_buf_cleanup,
	.wait_finish = dof_vb2_lock,
	.wait_prepare = dof_vb2_unlock,
	.start_streaming = dof_vb2_start_streaming,
	.stop_streaming = dof_vb2_stop_streaming,
};

static int queue_init(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq)
{
	struct dof_ctx *ctx = priv;
	int ret;

	dof_dbg("[DOF]\n");
	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->ops = &dof_vb2_ops;
	src_vq->mem_ops = &vb2_dma_sg_memops;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct vb2_dof_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->ops = &dof_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_sg_memops;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct vb2_dof_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	return vb2_queue_init(dst_vq);
}

static struct is_mem *__get_iommu_mem(struct dof_dev *dof)
{
	struct is_mem *mem;
	struct pablo_device_iommu_group *iommu_group;

	if (dof->use_cloader_iommu_group) {
		iommu_group = pablo_iommu_group_get(dof->cloader_iommu_group_id);
		mem = &iommu_group->mem;
	} else {
		mem = &dof->mem;
	}

	return mem;
}

static int dof_alloc_pmio_mem(struct dof_dev *dof)
{
	struct is_mem *mem;

	if (atomic_read(&dof->m2m.in_use))
		return 0;

	mem = __get_iommu_mem(dof);

	dof->pb_c_loader_payload = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x8000, NULL, 0);
	if (IS_ERR_OR_NULL(dof->pb_c_loader_payload)) {
		dev_err(dof->dev, "failed to allocate buffer for c-loader payload");
		dof->pb_c_loader_payload = NULL;
		return -ENOMEM;
	}

	dof->kva_c_loader_payload =
		CALL_BUFOP(dof->pb_c_loader_payload, kvaddr, dof->pb_c_loader_payload);
	dof->dva_c_loader_payload =
		CALL_BUFOP(dof->pb_c_loader_payload, dvaddr, dof->pb_c_loader_payload);

	dof->pb_c_loader_header = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x2000, NULL, 0);
	if (IS_ERR_OR_NULL(dof->pb_c_loader_header)) {
		dev_err(dof->dev, "failed to allocate buffer for c-loader header");
		dof->pb_c_loader_header = NULL;
		CALL_BUFOP(dof->pb_c_loader_payload, free, dof->pb_c_loader_payload);
		return -ENOMEM;
	}

	dof->kva_c_loader_header =
		CALL_BUFOP(dof->pb_c_loader_header, kvaddr, dof->pb_c_loader_header);
	dof->dva_c_loader_header =
		CALL_BUFOP(dof->pb_c_loader_header, dvaddr, dof->pb_c_loader_header);

	dof_info("payload_dva(0x%llx) header_dva(0x%llx)\n", dof->dva_c_loader_payload,
		dof->dva_c_loader_header);

	return 0;
}

static void dof_free_pmio_mem(struct dof_dev *dof)
{
	if (!IS_ERR_OR_NULL(dof->pb_c_loader_payload))
		CALL_BUFOP(dof->pb_c_loader_payload, free, dof->pb_c_loader_payload);
	if (!IS_ERR_OR_NULL(dof->pb_c_loader_header))
		CALL_BUFOP(dof->pb_c_loader_header, free, dof->pb_c_loader_header);
}

static int dof_open(struct file *file)
{
	struct dof_dev *dof = video_drvdata(file);
	struct dof_ctx *ctx;
	int ret = 0;

	ctx = pablo_zalloc(sizeof(struct dof_ctx), GFP_KERNEL);
	if (!ctx) {
		dev_err(dof->dev, "fail to alloc for open context\n");
		return -ENOMEM;
	}

	mutex_lock(&dof->m2m.lock);

	dof_info("[DOF] open : in_use = %d in_streamon = %d", atomic_read(&dof->m2m.in_use),
		atomic_read(&dof->m2m.in_streamon));

	if (!atomic_read(&dof->m2m.in_use)) {
		ret = dof_alloc_pmio_mem(dof);
		if (ret) {
			dev_err(dof->dev, "PMIO mem alloc failed\n");
			mutex_unlock(&dof->m2m.lock);
			goto err_alloc_pmio_mem;
		}
	}

	atomic_inc(&dof->m2m.in_use);

	mutex_unlock(&dof->m2m.lock);

	INIT_LIST_HEAD(&ctx->node);
	ctx->dof_dev = dof;

	v4l2_fh_init(&ctx->fh, dof->m2m.vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	/* Default color format */
	ctx->s_frame.dof_fmt = &dof_formats[0];
	ctx->d_frame.dof_fmt = &dof_formats[0];

	if (!IS_ERR(dof->pclk)) {
		ret = clk_prepare(dof->pclk);
		if (ret) {
			dev_err(dof->dev, "%s: failed to prepare PCLK(err %d)\n", __func__, ret);
			goto err_pclk_prepare;
		}
	}

	if (!IS_ERR(dof->aclk)) {
		ret = clk_prepare(dof->aclk);
		if (ret) {
			dev_err(dof->dev, "%s: failed to prepare ACLK(err %d)\n", __func__, ret);
			goto err_aclk_prepare;
		}
	}

	/* Setup the device context for mem2mem mode. */
	ctx->m2m_ctx = v4l2_m2m_ctx_init(dof->m2m.m2m_dev, ctx, queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		ret = -EINVAL;
		goto err_ctx;
	}

	dof_info("X\n");
	return 0;

err_ctx:
	if (!IS_ERR(dof->aclk))
		clk_unprepare(dof->aclk);
err_aclk_prepare:
	if (!IS_ERR(dof->pclk))
		clk_unprepare(dof->pclk);
err_pclk_prepare:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	atomic_dec(&dof->m2m.in_use);
	dof_free_pmio_mem(dof);
err_alloc_pmio_mem:
	pablo_free(ctx);

	return ret;
}

static void dof_job_finish(struct dof_dev *dof, struct dof_ctx *ctx)
{
	unsigned long flags;
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	dof_dbg("[DOF]\n");
	spin_lock_irqsave(&dof->slock, flags);

	ctx = v4l2_m2m_get_curr_priv(dof->m2m.m2m_dev);
	if (!ctx || !ctx->m2m_ctx) {
		dev_err(dof->dev, "current ctx is NULL\n");
		spin_unlock_irqrestore(&dof->slock, flags);
		return;
	}
	clear_bit(CTX_RUN, &ctx->flags);

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	BUG_ON(!src_vb || !dst_vb);

	dof_print_debugging_log(ctx);

	camerapp_hw_dof_update_debug_info(dof->pmio, &(ctx->model_param.debug_info),
		src_vb->vb2_buf.index, DEVICE_STATUS_TIMEOUT);

	v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
	v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);

	v4l2_m2m_job_finish(dof->m2m.m2m_dev, ctx->m2m_ctx);

	spin_unlock_irqrestore(&dof->slock, flags);
}

static int dof_release(struct file *file)
{
	struct dof_ctx *ctx = fh_to_dof_ctx(file->private_data);
	struct dof_dev *dof = ctx->dof_dev;

	mutex_lock(&dof->m2m.lock);

	dof_info("[DOF] close : in_use = %d in_streamon = %d", atomic_read(&dof->m2m.in_use),
		atomic_read(&dof->m2m.in_streamon));

	atomic_dec(&dof->m2m.in_use);
	if (!atomic_read(&dof->m2m.in_use) && test_bit(DEV_RUN, &dof->state)) {
		dev_err(dof->dev, "%s, dof is still running\n", __func__);
		dof_suspend(dof->dev);
	}

	dof_dma_buf_unmap_all(ctx);

	if (test_bit(CTX_DEV_READY, &ctx->flags)) {
		atomic_dec(&dof->m2m.in_streamon);
		if (!atomic_read(&dof->m2m.in_streamon))
			dof_power_clk_disable(dof);
		clear_bit(CTX_DEV_READY, &ctx->flags);
	}

	if (!atomic_read(&dof->m2m.in_use))
		dof_free_pmio_mem(dof);

	mutex_unlock(&dof->m2m.lock);

	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	if (!IS_ERR(dof->aclk))
		clk_unprepare(dof->aclk);
	if (!IS_ERR(dof->pclk))
		clk_unprepare(dof->pclk);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	pablo_free(ctx);

	return 0;
}

static const struct v4l2_file_operations dof_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = dof_open,
	.release = dof_release,
	.unlocked_ioctl = video_ioctl2,
};
static void dof_watchdog(struct timer_list *t)
{
	struct dof_wdt *wdt = from_timer(wdt, t, timer);
	struct dof_dev *dof = container_of(wdt, typeof(*dof), wdt);
	struct dof_ctx *ctx;
	unsigned long flags;

	dof_dbg("[DOF]\n");
	if (!test_bit(DEV_RUN, &dof->state)) {
		dof_info("[ERR] dof is not running\n");
		return;
	}

	spin_lock_irqsave(&dof->ctxlist_lock, flags);
	ctx = dof->current_ctx;
	if (!ctx) {
		dof_info("[ERR] ctx is empty\n");
		spin_unlock_irqrestore(&dof->ctxlist_lock, flags);
		return;
	}

	if (atomic_read(&dof->wdt.cnt) >= ctx->model_param.time_out) {
		dof_info("[ERR] final time_out(cnt:%d)\n", ctx->model_param.time_out);
		is_debug_s2d(true, "dof watchdog s2d");
		if (camerapp_hw_dof_sw_reset(dof->pmio))
			dev_err(dof->dev, "dof sw reset fail\n");

		atomic_set(&dof->wdt.cnt, 0);
		clear_bit(DEV_RUN, &dof->state);
		dof->current_ctx = NULL;
		spin_unlock_irqrestore(&dof->ctxlist_lock, flags);
		dof_job_finish(dof, ctx);
		return;
	}

	spin_unlock_irqrestore(&dof->ctxlist_lock, flags);

	if (test_bit(DEV_RUN, &dof->state)) {
#ifndef USE_VELOCE
		if (!atomic_read(&dof->wdt.cnt))
#endif
		{
			camerapp_hw_dof_sfr_dump(dof->pmio);
			dof_print_debugging_log(ctx);
		}

		atomic_inc(&dof->wdt.cnt);
		dev_err(dof->dev, "dof is still running(cnt:%d)\n", atomic_read(&dof->wdt.cnt));
		mod_timer(&dof->wdt.timer, jiffies + DOF_TIMEOUT);
	} else {
		dof_dbg("dof finished job\n");
	}
}

static void dof_pmio_config(struct dof_dev *dof, struct c_loader_buffer *clb)
{
	dof_dbg("[DOF]\n");

	if (unlikely(test_bit(DOF_DBG_PMIO_MODE, &debug_dof))) {
		/* APB-DIRECT */
		pmio_cache_sync(dof->pmio);
		clb->clh = NULL;
		clb->num_of_headers = 0;
	} else {
		clb->num_of_headers = 0;
		clb->num_of_values = 0;
		clb->num_of_pairs = 0;
		clb->header_dva = dof->dva_c_loader_header;
		clb->payload_dva = dof->dva_c_loader_payload;
		clb->clh = (struct c_loader_header *)dof->kva_c_loader_header;
		clb->clp = (struct c_loader_payload *)dof->kva_c_loader_payload;

		pmio_cache_fsync(dof->pmio, (void *)clb, PMIO_FORMATTER_PAIR);

		if (clb->num_of_pairs > 0)
			clb->num_of_headers++;

		if (unlikely(test_bit(DOF_DBG_DUMP_PMIO_CACHE, &debug_dof))) {
			pr_info("payload_dva(%pad) header_dva(%pad)\n", &clb->payload_dva,
				&clb->header_dva);
			pr_info("number of headers: %d\n", clb->num_of_headers);
			pr_info("number of pairs: %d\n", clb->num_of_pairs);

			print_hex_dump(KERN_INFO, "header  ", DUMP_PREFIX_OFFSET, 16, 4, clb->clh,
				clb->num_of_headers * 16, true);

			print_hex_dump(KERN_INFO, "payload ", DUMP_PREFIX_OFFSET, 16, 4, clb->clp,
				clb->num_of_headers * 64, true);
		}
	}

	if (!dof->use_hw_cache_operation) {
		CALL_BUFOP(dof->pb_c_loader_payload, sync_for_device, dof->pb_c_loader_payload, 0,
			dof->pb_c_loader_payload->size, DMA_TO_DEVICE);
		CALL_BUFOP(dof->pb_c_loader_header, sync_for_device, dof->pb_c_loader_header, 0,
			dof->pb_c_loader_header->size, DMA_TO_DEVICE);
	}
}

static int dof_run_next_job(struct dof_dev *dof)
{
	unsigned long flags;
	struct dof_ctx *ctx;
	struct c_loader_buffer clb;
	int ret = 0;

	dof_dbg("[DOF]\n");
	spin_lock_irqsave(&dof->ctxlist_lock, flags);

	if (dof->current_ctx || list_empty(&dof->context_list)) {
		/* a job is currently being processed or no job is to run */
		spin_unlock_irqrestore(&dof->ctxlist_lock, flags);
		return 0;
	}

	ctx = list_first_entry(&dof->context_list, struct dof_ctx, node);

	list_del_init(&ctx->node);

	clb.header_dva = 0;
	clb.num_of_headers = 0;
	dof->current_ctx = ctx;

	spin_unlock_irqrestore(&dof->ctxlist_lock, flags);

	/*
	 * dof_run_next_job() must not reenter while dof->state is DEV_RUN.
	 * DEV_RUN is cleared when an operation is finished.
	 */
	BUG_ON(test_bit(DEV_RUN, &dof->state));

	pmio_cache_set_only(dof->pmio, false);

	dof_dbg("dof hw setting\n");
	ctx->time_dbg.shot_time = ktime_get();

	dof->hw_dof_ops->sw_reset(dof->pmio);
	dof_dbg("dof sw reset : done\n");

	dof->hw_dof_ops->set_initialization(dof->pmio);
	dof_dbg("dof initialization : done\n");

	pmio_reset_cache(dof->pmio);
	pmio_cache_set_only(dof->pmio, true);

	ret = dof->hw_dof_ops->update_param(dof->pmio, dof->current_ctx);
	if (ret) {
		dev_err(dof->dev, "%s: failed to update dof param(%d)", __func__, ret);
		spin_lock_irqsave(&dof->ctxlist_lock, flags);
		dof->current_ctx = NULL;
		spin_unlock_irqrestore(&dof->ctxlist_lock, flags);
		dof_job_finish(dof, ctx);
		return ret;
	}
	dof_dbg("dof param update : done\n");

	dof_pmio_config(dof, &clb);

	set_bit(DEV_RUN, &dof->state);
	set_bit(CTX_RUN, &ctx->flags);
	mod_timer(&dof->wdt.timer, jiffies + DOF_TIMEOUT);

	dof->hw_dof_ops->start(dof->pmio, &clb);

	ctx->time_dbg.shot_time_stamp = ktime_us_delta(ktime_get(), ctx->time_dbg.shot_time);

	if (unlikely(test_bit(DOF_DBG_DUMP_REG, &debug_dof) ||
		     test_and_clear_bit(DOF_DBG_DUMP_REG_ONCE, &debug_dof)))
		dof->hw_dof_ops->sfr_dump(dof->pmio);

	ctx->time_dbg.hw_time = ktime_get();

	return 0;
}

static int dof_add_context_and_run(struct dof_dev *dof, struct dof_ctx *ctx)
{
	unsigned long flags;

	dof_dbg("[DOF]\n");
	spin_lock_irqsave(&dof->ctxlist_lock, flags);
	list_add_tail(&ctx->node, &dof->context_list);
	spin_unlock_irqrestore(&dof->ctxlist_lock, flags);

	return dof_run_next_job(dof);
}

static void dof_irq_finish(struct dof_dev *dof, struct dof_ctx *ctx, enum DOF_IRQ_DONE_TYPE type)
{
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	dof_dbg("[DOF]\n");
	clear_bit(DEV_RUN, &dof->state);
	del_timer(&dof->wdt.timer);
	atomic_set(&dof->wdt.cnt, 0);

	clear_bit(CTX_RUN, &ctx->flags);

	BUG_ON(ctx != v4l2_m2m_get_curr_priv(dof->m2m.m2m_dev));

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	BUG_ON(!src_vb || !dst_vb);

	if (type == DOF_IRQ_ERROR) {
		dof_print_debugging_log(ctx);

		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);

		/* Interrupt disable to ignore error */
		camerapp_hw_dof_interrupt_disable(dof->pmio);
		camerapp_hw_dof_get_intr_status_and_clear(dof->pmio);
	} else if (type == DOF_IRQ_FRAME_END) {
		if (ctx->model_param.enable_reg_dump) {
			camerapp_hw_dof_update_debug_info(dof->pmio, &(ctx->model_param.debug_info),
				src_vb->vb2_buf.index, DEVICE_STATUS_NORMAL);
		}

		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);
	}

	/* Wake up from CTX_ABORT state */
	clear_bit(CTX_ABORT, &ctx->flags);

	spin_lock(&dof->ctxlist_lock);
	dof->current_ctx = NULL;
	spin_unlock(&dof->ctxlist_lock);

	v4l2_m2m_job_finish(dof->m2m.m2m_dev, ctx->m2m_ctx);

	dof_resume(dof->dev);
	wake_up(&dof->wait);
	if (unlikely(test_bit(DOF_DBG_DUMP_S2D, &debug_dof))) {
		camerapp_hw_dof_sfr_dump(dof->pmio);
		is_debug_s2d(true, "DOF_DBG_DUMP_S2D");
	}
}

static irqreturn_t dof_irq_handler(int irq, void *priv)
{
	struct dof_dev *dof = priv;
	struct dof_ctx *ctx;
	u32 irq_status;
	u32 fs, fe;

	dof_dbg("[DOF]\n");
	spin_lock(&dof->slock);
	irq_status = camerapp_hw_dof_get_intr_status_and_clear(dof->pmio);

	ctx = dof->current_ctx;
	BUG_ON(!ctx);

	fs = atomic_read(&dof->frame_cnt.fs);
	fe = atomic_read(&dof->frame_cnt.fe);

	if ((irq_status & camerapp_hw_dof_get_int_frame_start()) &&
		(irq_status & camerapp_hw_dof_get_int_frame_end())) {
		dof_info("[WARN][DOF][FS:%d][FE:%d] start/end overlapped!!(0x%x)\n", fs, fe,
			irq_status);
	}

	if (irq_status & camerapp_hw_dof_get_int_err()) {
		dof_info("[ERR][DOF][FS:%d][FE:%d] handle error interrupt (0x%x)\n", fs, fe,
			irq_status);
		camerapp_hw_dof_sfr_dump(dof->pmio);
		dof_print_perframe_control_param(&ctx->perframe_control_params);
	}

	if (irq_status & camerapp_hw_dof_get_int_frame_start()) {
		ctx->time_dbg.hw_time = ktime_get();

		atomic_inc(&dof->frame_cnt.fs);
		fs = atomic_read(&dof->frame_cnt.fs);
		dof_dbg("[DOF][FS:%d][FE:%d] FRAME_START (0x%x)\n", fs, fe, irq_status);
	}

	if (irq_status & camerapp_hw_dof_get_int_frame_end()) {
		ctx->time_dbg.hw_time_stamp = ktime_us_delta(ktime_get(), ctx->time_dbg.hw_time);

		atomic_inc(&dof->frame_cnt.fe);
		fe = atomic_read(&dof->frame_cnt.fe);
		dof_dbg("[DOF][FS:%d][FE:%d] FRAME_END(0x%x)\n", fs, fe, irq_status);
		dof_irq_finish(dof, ctx, DOF_IRQ_FRAME_END);
	}
	spin_unlock(&dof->slock);

	return IRQ_HANDLED;
}

static void dof_fill_curr_frame(struct dof_dev *dof, struct dof_ctx *ctx)
{
	struct dof_frame *s_frame, *d_frame;
	struct vb2_buffer *src_vb = (struct vb2_buffer *)v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	struct vb2_buffer *dst_vb = (struct vb2_buffer *)v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	struct dof_perframe_control_params perframe_control_params = ctx->perframe_control_params;
	struct dof_roi *roi;

	dof_dbg("[DOF]\n");
	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;
	roi = &ctx->roi;

	dof_dbg("s_frame: %dx%d -> d_frame: %dx%d\n", s_frame->width, s_frame->height,
		d_frame->width, d_frame->height);

	if ((roi->roi_width != perframe_control_params.roi_width) ||
	    (roi->roi_height != perframe_control_params.roi_height)) {
		dof_info("[DOF][model%d][F%d] update roi(%dx%d -> %dx%d)\n",
			 perframe_control_params.model_id, perframe_control_params.frame_count,
			 roi->roi_width, roi->roi_height, perframe_control_params.roi_width,
			 perframe_control_params.roi_height);
	}

	roi->roi_width = perframe_control_params.roi_width;
	roi->roi_height = perframe_control_params.roi_height;

	/* dva info */
	dof_dbg("num_planes(%d)\n", s_frame->dof_fmt->num_planes);
	s_frame->addr.curr_in = dof->dma_buf_ops->get_dma_address(src_vb, 0);
	d_frame->addr.output = dof->dma_buf_ops->get_dma_address(dst_vb, 0);

	if (!ctx->sgt[DOF_BUF_PREV_IN]) {
		if (perframe_control_params.operation_mode == DOF_OP_MODE_DUAL_FE) {
			dof_info(
				"[ERR] ctx->sgt[DOF_BUF_PREV_IN] is NULL. check prev_fd(%d), operation_mode(%d)\n",
				perframe_control_params.prev_fd,
				perframe_control_params.operation_mode);
		}
		s_frame->addr.prev_in = 0;
	} else {
		s_frame->addr.prev_in = dof->dma_buf_ops->get_dva(ctx, DOF_BUF_PREV_IN);
	}
	if (!ctx->sgt[DOF_BUF_PREV_STATE]) {
		if (perframe_control_params.operation_mode == DOF_OP_MODE_SINGLE_FE) {
			dof_info(
				"[ERR] ctx->sgt[DOF_BUF_PREV_STATE] is NULL. check pstate_fd(%d), operation_mode(%d)\n",
				perframe_control_params.pstate_fd,
				perframe_control_params.operation_mode);
		}
		s_frame->addr.prev_state = 0;
	} else {
		s_frame->addr.prev_state = dof->dma_buf_ops->get_dva(ctx, DOF_BUF_PREV_STATE);
	}
	if (!ctx->sgt[DOF_BUF_NEXT_STATE]) {
		if (perframe_control_params.operation_mode != DOF_OP_MODE_FE_ONLY) {
			dof_info(
				"[ERR] ctx->sgt[DOF_BUF_NEXT_STATE] is NULL. check nstate_fd(%d), operation_mode(%d)\n",
				perframe_control_params.nstate_fd,
				perframe_control_params.operation_mode);
		}
		d_frame->addr.next_state = 0;
	} else {
		d_frame->addr.next_state = dof->dma_buf_ops->get_dva(ctx, DOF_BUF_NEXT_STATE);
	}

	s_frame->addr.curr_in_size = s_frame->width * s_frame->height;
	s_frame->addr.prev_in_size = perframe_control_params.prev_size;
	s_frame->addr.prev_state_size = perframe_control_params.pstate_size;
	d_frame->addr.output_size = d_frame->width * d_frame->height;
	d_frame->addr.next_state_size = perframe_control_params.nstate_size;

	dof_update_model_address_offset(ctx);

	if (dof_get_debug_level())
		dof_print_fill_current_frame(s_frame, d_frame);
}

static void dof_m2m_device_run(void *priv)
{
	struct dof_ctx *ctx = priv;
	struct dof_dev *dof = ctx->dof_dev;

	dof_dbg("[DOF]\n");
	if (test_bit(DEV_SUSPEND, &dof->state)) {
		dev_err(dof->dev, "dof is in suspend state\n");
		return;
	}

	if (test_bit(CTX_ABORT, &ctx->flags)) {
		dev_err(dof->dev, "aborted dof device run\n");
		return;
	}

	dof_fill_curr_frame(dof, ctx);

	dof_add_context_and_run(dof, ctx);
}

static void dof_m2m_job_abort(void *priv)
{
	struct dof_ctx *ctx = priv;
	int ret;

	dof_dbg("[DOF]\n");
	ret = dof_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->dof_dev->dev, "wait timeout\n");
}

static struct v4l2_m2m_ops dof_m2m_ops = {
	.device_run = dof_m2m_device_run,
	.job_abort = dof_m2m_job_abort,
};

static void dof_unregister_m2m_device(struct dof_dev *dof)
{
	dof_dbg("[DOF]\n");
	video_unregister_device(dof->m2m.vfd);
	v4l2_m2m_release(dof->m2m.m2m_dev);
	v4l2_device_unregister(&dof->m2m.v4l2_dev);
}

static struct v4l2_m2m_dev *dof_v4l2_m2m_init(void)
{
	return v4l2_m2m_init(&dof_m2m_ops);
}

static void dof_v4l2_m2m_release(struct v4l2_m2m_dev *m2m_dev)
{
	v4l2_m2m_release(m2m_dev);
}

static int dof_register_m2m_device(struct dof_dev *dof, int dev_id)
{
	struct v4l2_device *v4l2_dev;
	struct device *dev;
	struct video_device *vfd;
	int ret = 0;

	dof_dbg("[DOF]\n");
	dev = dof->dev;
	v4l2_dev = &dof->m2m.v4l2_dev;

	scnprintf(v4l2_dev->name, sizeof(v4l2_dev->name), "%s.m2m", DOF_MODULE_NAME);

	ret = v4l2_device_register(dev, v4l2_dev);
	if (ret) {
		dev_err(dof->dev, "failed to register v4l2 device\n");
		return ret;
	}

	vfd = video_device_alloc();
	if (!vfd) {
		dev_err(dof->dev, "failed to allocate video device\n");
		goto err_v4l2_dev;
	}

	vfd->fops = &dof_v4l2_fops;
	vfd->ioctl_ops = &dof_v4l2_ioctl_ops;
	vfd->release = video_device_release;
	vfd->lock = &dof->lock;
	vfd->vfl_dir = VFL_DIR_M2M;
	vfd->v4l2_dev = v4l2_dev;
	vfd->device_caps = DOF_V4L2_DEVICE_CAPS;
	scnprintf(vfd->name, sizeof(vfd->name), "%s:m2m", DOF_MODULE_NAME);

	video_set_drvdata(vfd, dof);

	dof->m2m.vfd = vfd;
	dof->m2m.m2m_dev = dof_v4l2_m2m_init();
	if (IS_ERR(dof->m2m.m2m_dev)) {
		dev_err(dof->dev, "failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(dof->m2m.m2m_dev);
		goto err_dev_alloc;
	}

	ret = video_register_device(
		vfd, VFL_TYPE_PABLO, EXYNOS_VIDEONODE_CAMERAPP(CAMERAPP_VIDEONODE_DOF));
	if (ret) {
		dev_err(dof->dev, "failed to register video device(%d)\n",
			EXYNOS_VIDEONODE_CAMERAPP(CAMERAPP_VIDEONODE_DOF));
		goto err_m2m_dev;
	}

	dev_info(dof->dev, "video node register: %d\n",
		EXYNOS_VIDEONODE_CAMERAPP(CAMERAPP_VIDEONODE_DOF));

	return 0;

err_m2m_dev:
	dof_v4l2_m2m_release(dof->m2m.m2m_dev);
err_dev_alloc:
	video_device_release(dof->m2m.vfd);
err_v4l2_dev:
	v4l2_device_unregister(v4l2_dev);

	return ret;
}
#ifdef CONFIG_EXYNOS_IOVMM
static int __always_unused dof_sysmmu_fault_handler(
	struct iommu_domain *domain, struct device *dev, unsigned long iova, int flags, void *token)
{
	struct dof_dev *dof = dev_get_drvdata(dev);
#else
static int dof_sysmmu_fault_handler(struct iommu_fault *fault, void *data)
{
	struct dof_dev *dof = data;
	struct device *dev = dof->dev;
	unsigned long iova = fault->event.addr;
#endif

	dof_dbg("[DOF]\n");
	if (test_bit(DEV_RUN, &dof->state)) {
		dev_info(dev, "System MMU fault called for IOVA %#lx\n", iova);
		camerapp_hw_dof_sfr_dump(dof->pmio);
	}

	return 0;
}

static int dof_clk_get(struct dof_dev *dof)
{
	dof_dbg("[DOF]\n");
	dof->aclk = devm_clk_get(dof->dev, "gate");
	if (IS_ERR(dof->aclk)) {
		if (PTR_ERR(dof->aclk) != -ENOENT) {
			dev_err(dof->dev, "Failed to get 'gate' clock: %ld", PTR_ERR(dof->aclk));
			return PTR_ERR(dof->aclk);
		}
		dev_info(dof->dev, "'gate' clock is not present\n");
	}

	dof->pclk = devm_clk_get(dof->dev, "gate2");
	if (IS_ERR(dof->pclk)) {
		if (PTR_ERR(dof->pclk) != -ENOENT) {
			dev_err(dof->dev, "Failed to get 'gate2' clock: %ld", PTR_ERR(dof->pclk));
			return PTR_ERR(dof->pclk);
		}
		dev_info(dof->dev, "'gate2' clock is not present\n");
	}

	dof->clk_chld = devm_clk_get(dof->dev, "mux_user");
	if (IS_ERR(dof->clk_chld)) {
		if (PTR_ERR(dof->clk_chld) != -ENOENT) {
			dev_err(dof->dev, "Failed to get 'mux_user' clock: %ld",
				PTR_ERR(dof->clk_chld));
			return PTR_ERR(dof->clk_chld);
		}
		dev_info(dof->dev, "'mux_user' clock is not present\n");
	}

	if (!IS_ERR(dof->clk_chld)) {
		dof->clk_parn = devm_clk_get(dof->dev, "mux_src");
		if (IS_ERR(dof->clk_parn)) {
			dev_err(dof->dev, "Failed to get 'mux_src' clock: %ld",
				PTR_ERR(dof->clk_parn));
			return PTR_ERR(dof->clk_parn);
		}
	} else {
		dof->clk_parn = ERR_PTR(-ENOENT);
	}

	return 0;
}

static void dof_clk_put(struct dof_dev *dof)
{
	dof_dbg("[DOF]\n");
	if (!IS_ERR(dof->clk_parn))
		devm_clk_put(dof->dev, dof->clk_parn);

	if (!IS_ERR(dof->clk_chld))
		devm_clk_put(dof->dev, dof->clk_chld);

	if (!IS_ERR(dof->pclk))
		devm_clk_put(dof->dev, dof->pclk);

	if (!IS_ERR(dof->aclk))
		devm_clk_put(dof->dev, dof->aclk);
}

#ifdef CONFIG_PM_SLEEP
static int dof_suspend(struct device *dev)
{
	struct dof_dev *dof = dev_get_drvdata(dev);
	int ret;

	dof_dbg("[DOF]\n");
	set_bit(DEV_SUSPEND, &dof->state);

	ret = wait_event_timeout(
		dof->wait, !test_bit(DEV_RUN, &dof->state), DOF_TIMEOUT * 50); /* 2sec */
	if (ret == 0)
		dev_err(dof->dev, "wait timeout\n");

	return 0;
}

static int dof_resume(struct device *dev)
{
	struct dof_dev *dof = dev_get_drvdata(dev);

	dof_dbg("[DOF]\n");
	clear_bit(DEV_SUSPEND, &dof->state);

	return 0;
}
#endif

static int dof_runtime_resume(struct device *dev)
{
	struct dof_dev *dof = dev_get_drvdata(dev);

	if (!IS_ERR(dof->clk_chld) && !IS_ERR(dof->clk_parn)) {
		int ret = clk_set_parent(dof->clk_chld, dof->clk_parn);

		if (ret) {
			dev_err(dof->dev, "%s: Failed to setup MUX: %d\n", __func__, ret);
			return ret;
		}
	}

	return 0;
}

static int dof_runtime_suspend(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dof_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(dof_suspend, dof_resume)
		SET_RUNTIME_PM_OPS(dof_runtime_suspend, dof_runtime_resume, NULL) };

static int dof_pmio_init(struct dof_dev *dof)
{
	int ret;

	dof_info("[DOF]");
	camerapp_hw_dof_init_pmio_config(dof);

	dof->pmio = pmio_init(NULL, NULL, &dof->pmio_config);
	if (IS_ERR(dof->pmio)) {
		dev_err(dof->dev, "failed to init dof PMIO: %ld", PTR_ERR(dof->pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(
		dof->pmio, &dof->pmio_fields, dof->pmio_config.fields, dof->pmio_config.num_fields);
	if (ret) {
		dev_err(dof->dev, "failed to alloc dof PMIO fields: %d", ret);
		pmio_exit(dof->pmio);
		return ret;
	}

	return 0;
}

static void dof_pmio_exit(struct dof_dev *dof)
{
	dof_info("[DOF]");

	if (dof->pmio) {
		if (dof->pmio_fields)
			pmio_field_bulk_free(dof->pmio, dof->pmio_fields);

		pmio_exit(dof->pmio);
	}
}

static int dof_probe(struct platform_device *pdev)
{
	struct dof_dev *dof;
	struct resource *rsc;
	struct device_node *np;
	int ret = 0;

	dof_info("[DOF]\n");
	dof = devm_kzalloc(&pdev->dev, sizeof(struct dof_dev), GFP_KERNEL);
	if (!dof) {
		dev_err(&pdev->dev, "no memory for dof device\n");
		return -ENOMEM;
	}

	dof->dev = &pdev->dev;
	np = dof->dev->of_node;

	dev_set_drvdata(&pdev->dev, dof);

	spin_lock_init(&dof->ctxlist_lock);
	INIT_LIST_HEAD(&dof->context_list);
	spin_lock_init(&dof->slock);
	mutex_init(&dof->lock);
	init_waitqueue_head(&dof->wait);

	rsc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!rsc) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		ret = -ENOMEM;
		goto err_get_mem_res;
	}

	dof->regs_base = devm_ioremap(&pdev->dev, rsc->start, resource_size(rsc));
	if (IS_ERR_OR_NULL(dof->regs_base)) {
		dev_err(&pdev->dev, "Failed to ioremap for reg_base\n");
		ret = ENOMEM;
		goto err_get_mem_res;
	}
	dof->regs_rsc = rsc;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get IRQ-0 (%d)\n", ret);
		goto err_get_irq_res;
	}
	dof->irq0 = ret;

	ret = devm_request_irq(&pdev->dev, dof->irq0, dof_irq_handler, 0, "dof0", dof);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq0\n");
		goto err_get_irq_res;
	}

	ret = platform_get_irq(pdev, 1);
	if (ret < 0) {
		dev_info(&pdev->dev, "there's no IRQ-1 resource\n");
	} else {
		dof->irq1 = ret;
		ret = devm_request_irq(&pdev->dev, dof->irq1, dof_irq_handler, 0, "dof1", dof);
		if (ret) {
			dev_err(&pdev->dev, "failed to install irq1\n");
			goto err_req_irq1;
		}
	}

	pablo_set_affinity_irq(dof->irq0, true);

	dof->use_hw_cache_operation = of_property_read_bool(np, "dma-coherent");
	dof_info("[DOF] %s dma-coherent\n", dof->use_hw_cache_operation ? "use" : "not use");

	dof->use_cloader_iommu_group = of_property_read_bool(np, "iommu_group_for_cloader");
	if (dof->use_cloader_iommu_group) {
		ret = of_property_read_u32(
			np, "iommu_group_for_cloader", &dof->cloader_iommu_group_id);
		if (ret)
			dev_err(&pdev->dev, "fail to get iommu group id for cloader, ret(%d)", ret);
	}

	atomic_set(&dof->wdt.cnt, 0);
	timer_setup(&dof->wdt.timer, dof_watchdog, 0);

	atomic_set(&dof->frame_cnt.fs, 0);
	atomic_set(&dof->frame_cnt.fe, 0);

	ret = dof_clk_get(dof);
	if (ret)
		goto err_wq;

	if (pdev->dev.of_node)
		dof->dev_id = of_alias_get_id(pdev->dev.of_node, "camerapp-dof");
	else
		dof->dev_id = pdev->id;

	if (dof->dev_id < 0)
		dof->dev_id = 0;
	platform_set_drvdata(pdev, dof);

	pm_runtime_enable(&pdev->dev);

	ret = dof_register_m2m_device(dof, dof->dev_id);
	if (ret) {
		dev_err(&pdev->dev, "failed to register m2m device\n");
		goto err_reg_m2m_dev;
	}

#ifdef CONFIG_EXYNOS_IOVMM
	ret = iovmm_activate(dof->dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to attach iommu\n");
		goto err_iommu;
	}
#endif

	ret = dof_power_clk_enable(dof);
	if (ret)
		goto err_power_clk;

#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(&pdev->dev, dof_sysmmu_fault_handler, dof);
#else
	iommu_register_device_fault_handler(&pdev->dev, dof_sysmmu_fault_handler, dof);
#endif

	ret = is_mem_init(&dof->mem, pdev);
	if (ret) {
		pablo_mem_deinit(&dof->mem);
		dev_err(dof->dev, "dof_mem_probe is fail(%d)", ret);
		goto err_mem_init;
	}

	ret = dof_pmio_init(dof);
	if (ret) {
		dev_err(&pdev->dev, "%s: dof pmio initialization failed", __func__);
		goto err_pmio_init;
	}

	mutex_init(&dof->m2m.lock);

	dof->variant = camerapp_hw_dof_get_size_constraints(dof->pmio);
	dof->version = camerapp_hw_dof_get_ver(dof->pmio);
	dof->v4l2_ops = &dof_v4l2_ops;
	dof->sys_ops = &dof_sys_ops;
	dof->hw_dof_ops = pablo_get_hw_dof_ops();
	dof->dma_buf_ops = &dof_dma_buf_ops;

	dof_power_clk_disable(dof);

	dev_info(&pdev->dev, "Driver probed successfully(version: %08x)\n", dof->version);

	return 0;

err_pmio_init:
err_mem_init:
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(&pdev->dev, NULL, dof);
#else
	iommu_unregister_device_fault_handler(&pdev->dev);
#endif

	dof_power_clk_disable(dof);
err_power_clk:
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_deactivate(dof->dev);
err_iommu:
#endif
	dof_unregister_m2m_device(dof);
err_reg_m2m_dev:
	dof_clk_put(dof);
err_wq:
	if (dof->irq1)
		devm_free_irq(&pdev->dev, dof->irq1, dof);
err_req_irq1:
	pablo_set_affinity_irq(dof->irq0, false);
	devm_free_irq(&pdev->dev, dof->irq0, dof);
err_get_irq_res:
	devm_iounmap(&pdev->dev, dof->regs_base);
err_get_mem_res:
	devm_kfree(&pdev->dev, dof);

	return ret;
}

static int dof_remove(struct platform_device *pdev)
{
	struct dof_dev *dof = platform_get_drvdata(pdev);

	dof_dbg("[DOF]\n");
	dof_pmio_exit(dof);

	pablo_mem_deinit(&dof->mem);

#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(&pdev->dev, NULL, dof);
	iovmm_deactivate(dof->dev);
#else
	iommu_unregister_device_fault_handler(&pdev->dev);
#endif

	dof_unregister_m2m_device(dof);

	pm_runtime_disable(&pdev->dev);

	dof_clk_put(dof);

	if (timer_pending(&dof->wdt.timer))
		del_timer(&dof->wdt.timer);

	if (dof->irq1)
		devm_free_irq(&pdev->dev, dof->irq1, dof);

	pablo_set_affinity_irq(dof->irq0, false);
	devm_free_irq(&pdev->dev, dof->irq0, dof);
	devm_iounmap(&pdev->dev, dof->regs_base);
	devm_kfree(&pdev->dev, dof);

	return 0;
}

static void dof_shutdown(struct platform_device *pdev)
{
	struct dof_dev *dof = platform_get_drvdata(pdev);

	dof_dbg("[DOF]\n");
	set_bit(DEV_SUSPEND, &dof->state);

	wait_event(dof->wait, !test_bit(DEV_RUN, &dof->state));

#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_deactivate(dof->dev);
#endif
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static void dof_set_debug_level(int level)
{
	dof_debug_level = level;
}

static void dof_set_debug_rdmo(uint rdmo)
{
	dof_debug_rdmo = rdmo;
}

static void dof_set_debug_wrmo(uint wrmo)
{
	dof_debug_wrmo = wrmo;
}

static struct pkt_dof_ops dof_ops = {
	.get_log_level = dof_get_debug_level,
	.set_log_level = dof_set_debug_level,
	.get_rdmo = dof_get_debug_rdmo,
	.set_rdmo = dof_set_debug_rdmo,
	.get_wrmo = dof_get_debug_wrmo,
	.set_wrmo = dof_set_debug_wrmo,
	.get_debug = param_get_debug_dof,
	.find_format = dof_find_format,
	.v4l2_querycap = dof_v4l2_querycap,
	.v4l2_g_fmt_mplane = dof_v4l2_g_fmt_mplane,
	.v4l2_try_fmt_mplane = dof_v4l2_try_fmt_mplane,
	.v4l2_s_fmt_mplane = dof_v4l2_s_fmt_mplane,
	.v4l2_reqbufs = dof_v4l2_reqbufs,
	.v4l2_querybuf = dof_v4l2_querybuf,
	.vb2_qbuf = dof_check_vb2_qbuf,
	.check_qbuf = dof_check_qbuf,
	.v4l2_qbuf = dof_v4l2_qbuf,
	.v4l2_dqbuf = dof_v4l2_dqbuf,
	.power_clk_enable = dof_power_clk_enable,
	.power_clk_disable = dof_power_clk_disable,
	.v4l2_streamon = dof_v4l2_streamon,
	.v4l2_streamoff = dof_v4l2_streamoff,
	.v4l2_s_ctrl = dof_v4l2_s_ctrl,
	.v4l2_s_ext_ctrls = dof_v4l2_s_ext_ctrls,
	.v4l2_m2m_init = dof_v4l2_m2m_init,
	.v4l2_m2m_release = dof_v4l2_m2m_release,
	.ctx_stop_req = dof_ctx_stop_req,
	.vb2_queue_setup = dof_vb2_queue_setup,
	.vb2_buf_prepare = dof_vb2_buf_prepare,
	.vb2_buf_finish = dof_vb2_buf_finish,
	.vb2_buf_queue = dof_vb2_buf_queue,
	.vb2_lock = dof_vb2_lock,
	.vb2_unlock = dof_vb2_unlock,
	.cleanup_queue = dof_cleanup_queue,
	.vb2_start_streaming = dof_vb2_start_streaming,
	.vb2_stop_streaming = dof_vb2_stop_streaming,
	.queue_init = queue_init,
	.pmio_init = dof_pmio_init,
	.pmio_exit = dof_pmio_exit,
	.pmio_config = dof_pmio_config,
	.run_next_job = dof_run_next_job,
	.add_context_and_run = dof_add_context_and_run,
	.m2m_device_run = dof_m2m_device_run,
	.m2m_job_abort = dof_m2m_job_abort,
	.clk_get = dof_clk_get,
	.clk_put = dof_clk_put,
	.sysmmu_fault_handler = dof_sysmmu_fault_handler,
	.shutdown = dof_shutdown,
	.suspend = dof_suspend,
	.runtime_resume = dof_runtime_resume,
	.runtime_suspend = dof_runtime_suspend,
	.alloc_pmio_mem = dof_alloc_pmio_mem,
	.free_pmio_mem = dof_free_pmio_mem,
	.job_finish = dof_job_finish,
	.register_m2m_device = dof_register_m2m_device,
};

struct pkt_dof_ops *pablo_kunit_get_dof(void)
{
	return &dof_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_dof);

ulong pablo_get_dbg_dof(void)
{
	return debug_dof;
}
KUNIT_EXPORT_SYMBOL(pablo_get_dbg_dof);

void pablo_set_dbg_dof(ulong dbg)
{
	debug_dof = dbg;
}
KUNIT_EXPORT_SYMBOL(pablo_set_dbg_dof);
#endif

static const struct of_device_id exynos_dof_match[] = {
	{
		.compatible = "samsung,exynos-is-dof",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_dof_match);

static struct platform_driver dof_driver = {
	.probe = dof_probe,
	.remove = dof_remove,
	.shutdown = dof_shutdown,
	.driver = {
		.name = DOF_MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &dof_pm_ops,
		.of_match_table = of_match_ptr(exynos_dof_match),
	},
};
module_driver(dof_driver, platform_driver_register, platform_driver_unregister)

MODULE_AUTHOR("SamsungLSI Camera");
MODULE_DESCRIPTION("EXYNOS CameraPP DOF driver");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
