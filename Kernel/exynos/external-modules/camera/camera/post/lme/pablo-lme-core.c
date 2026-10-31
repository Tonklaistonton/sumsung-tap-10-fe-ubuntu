// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS CAMERA PostProcessing lme driver
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
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

#include "pablo-lme.h"
#include "pablo-hw-api-lme.h"
#include "pablo-kernel-variant.h"
#include "is-common-enum.h"
#include "is-video.h"

#include "pmio.h"
#include "pablo-device-iommu-group.h"
#include "pablo-irq.h"

static int lme_alloc_internal_mem(struct lme_ctx *ctx);
static void lme_free_internal_mem(struct lme_ctx *ctx);
static int lme_dma_buf_map(
	struct lme_ctx *ctx, enum LME_BUF_TYPE type, int dmafd, bool is_need_kva);
static void lme_dma_buf_unmap(struct lme_ctx *ctx, int type);

/* Flags that are set by us */
#define V4L2_BUFFER_MASK_FLAGS                                                                     \
	(V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED | V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR |  \
		V4L2_BUF_FLAG_PREPARED | V4L2_BUF_FLAG_IN_REQUEST | V4L2_BUF_FLAG_REQUEST_FD |     \
		V4L2_BUF_FLAG_TIMESTAMP_MASK)
/* Output buffer flags that should be passed on to the driver */
#define V4L2_BUFFER_OUT_FLAGS                                                                      \
	(V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME | V4L2_BUF_FLAG_KEYFRAME |                    \
		V4L2_BUF_FLAG_TIMECODE)

#define LME_V4L2_DEVICE_CAPS (V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING)

static int lme_debug_level = LME_DEBUG_OFF;
static uint lme_use_timeout_wa = LME_TIMEOUT_WA_USE;

module_param_named(lme_debug_level, lme_debug_level, uint, 0644);
module_param_named(lme_use_timeout_wa, lme_use_timeout_wa, uint, 0644);

static ulong debug_lme;
static int param_get_debug_lme(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "LME debug features\n");
	ret += sprintf(buffer + ret, "\tb[0] : Dump SFR (0x1)\n");
	ret += sprintf(buffer + ret, "\tb[1] : Dump SFR Once (0x2)\n");
	ret += sprintf(buffer + ret, "\tb[2] : S2D (0x4)\n");
	ret += sprintf(buffer + ret, "\tb[3] : Shot & H/W latency (0x8)\n");
	ret += sprintf(buffer + ret, "\tb[4] : PMIO APB-DIRECT (0x10)\n");
	ret += sprintf(buffer + ret, "\tb[5] : Dump PMIO Cache Buffer (0x20)\n");
	ret += sprintf(buffer + ret, "\tcurrent value : 0x%lx\n", debug_lme);

	return ret;
}

static const struct kernel_param_ops param_ops_debug_lme = {
	.set = param_set_ulong,
	.get = param_get_debug_lme,
};

module_param_cb(debug_lme, &param_ops_debug_lme, &debug_lme, S_IRUGO | S_IWUSR);

static int lme_suspend(struct device *dev);
static int lme_resume(struct device *dev);

struct vb2_lme_buffer {
	struct v4l2_m2m_buffer mb;
	struct lme_ctx *ctx;
	ktime_t ktime;
};

static const struct lme_fmt lme_formats[] = {
	{
		.name = "GREY",
		.pixelformat = V4L2_PIX_FMT_GREY,
		.cfg_val = LME_CFG_FMT_GREY,
		.bitperpixel = { 8, 0, 0 },
		.num_planes = 1,
		.num_comp = 1,
		.h_shift = 1,
		.v_shift = 1,
	},
};

int lme_get_debug_level(void)
{
	return lme_debug_level;
}

uint lme_get_use_timeout_wa(void)
{
	return lme_use_timeout_wa;
}

void lme_set_use_timeout_wa(enum LME_TIMEOUT_WA_MODE mode)
{
	lme_use_timeout_wa = mode;
}

/* Find the matches format */
static const struct lme_fmt *lme_find_format(u32 pixfmt)
{
	const struct lme_fmt *lme_fmt;
	unsigned long i;

	lme_dbg("[LME]\n");
	for (i = 0; i < ARRAY_SIZE(lme_formats); ++i) {
		lme_fmt = &lme_formats[i];
		if (lme_fmt->pixelformat == pixfmt) {
			return &lme_formats[i];
		}
	}

	return NULL;
}

static int lme_v4l2_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	lme_dbg("[LME]\n");
	strncpy(cap->driver, LME_MODULE_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, LME_MODULE_NAME, sizeof(cap->card) - 1);

	cap->capabilities =
		V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	cap->capabilities |= V4L2_CAP_DEVICE_CAPS;
	cap->device_caps = LME_V4L2_DEVICE_CAPS;

	return 0;
}

static int lme_v4l2_g_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);
	const struct lme_fmt *lme_fmt;
	struct lme_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int i;

	lme_dbg("[LME]\n");

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	lme_fmt = frame->lme_fmt;

	pixm->width = frame->width;
	pixm->height = frame->height;
	pixm->pixelformat = frame->pixelformat;
	pixm->field = V4L2_FIELD_NONE;
	pixm->num_planes = frame->lme_fmt->num_planes;
	pixm->colorspace = 0;

	for (i = 0; i < pixm->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width * lme_fmt->bitperpixel[i]) >> 3;
		if (lme_fmt_is_ayv12(lme_fmt->pixelformat)) {
			unsigned int y_size, c_span;
			y_size = pixm->width * pixm->height;
			c_span = ALIGN(pixm->width >> 1, 16);
			pixm->plane_fmt[i].sizeimage = y_size + (c_span * pixm->height >> 1) * 2;
		} else {
			pixm->plane_fmt[i].sizeimage =
				pixm->plane_fmt[i].bytesperline * pixm->height;
		}

		lme_dbg("[LME] [%d] plane: bytesperline %d, sizeimage %d\n", i,
			pixm->plane_fmt[i].bytesperline, pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int lme_v4l2_try_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);
	const struct lme_fmt *lme_fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct lme_frame *frame;
	int i;

	lme_dbg("[LME]\n");

	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		v4l2_err(&ctx->lme_dev->m2m.v4l2_dev, "not supported v4l2 type\n");
		return -EINVAL;
	}

	lme_fmt = lme_find_format(f->fmt.pix_mp.pixelformat);
	if (!lme_fmt) {
		v4l2_err(&ctx->lme_dev->m2m.v4l2_dev,
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

	lme_dbg("[LME] num_planes:%d", lme_fmt->num_planes);
	for (i = 0; i < lme_fmt->num_planes; ++i) {
		/* The pixm->plane_fmt[i].sizeimage for the plane which
		 * contains the src blend data has to be calculated as per the
		 * size of the actual width and actual height of the src blend
		 * buffer */
		pixm->plane_fmt[i].bytesperline = (pixm->width * lme_fmt->bitperpixel[i]) >> 3;
		pixm->plane_fmt[i].sizeimage = pixm->plane_fmt[i].bytesperline * pixm->height;

		lme_dbg("[LME] type:(%d) %s [%d] plane: bytesperline %d, sizeimage %d\n", f->type,
			(V4L2_TYPE_IS_OUTPUT(f->type) ? "input" : "output"), i,
			pixm->plane_fmt[i].bytesperline, pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int lme_v4l2_s_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)

{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	struct lme_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	const struct lme_size_limit *limit = &ctx->lme_dev->variant->limit_input;
	int i, ret = 0;

	lme_dbg("[LME]\n");

	if (vb2_is_streaming(vq)) {
		v4l2_err(&ctx->lme_dev->m2m.v4l2_dev, "device is busy\n");
		return -EBUSY;
	}

	ret = lme_v4l2_try_fmt_mplane(file, fh, f);
	if (ret < 0)
		return ret;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	set_bit(CTX_PARAMS, &ctx->flags);

	frame->lme_fmt = lme_find_format(f->fmt.pix_mp.pixelformat);
	if (!frame->lme_fmt) {
		v4l2_err(&ctx->lme_dev->m2m.v4l2_dev, "not supported format values\n");
		return -EINVAL;
	}

	frame->num_planes = (pixm->num_planes < LME_MAX_PLANES) ? pixm->num_planes : LME_MAX_PLANES;

	for (i = 0; i < frame->num_planes; i++) {
		frame->bytesused[i] = pixm->plane_fmt[i].sizeimage;
	}
	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width > limit->max_w) || (pixm->height > limit->max_h))) {
		v4l2_err(&ctx->lme_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too large\n", pixm->width,
			pixm->height);
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width < limit->min_w) || (pixm->height < limit->min_h))) {
		v4l2_err(&ctx->lme_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too small\n", pixm->width,
			pixm->height);
		return -EINVAL;
	}

	frame->width = pixm->width;
	frame->height = pixm->height;
	frame->pixelformat = pixm->pixelformat;

	lme_info("[LME][%s] pixelformat(%c%c%c%c) size(%dx%d)\n",
		(V4L2_TYPE_IS_OUTPUT(f->type) ? "input" : "output"),
		(char)((frame->lme_fmt->pixelformat >> 0) & 0xFF),
		(char)((frame->lme_fmt->pixelformat >> 8) & 0xFF),
		(char)((frame->lme_fmt->pixelformat >> 16) & 0xFF),
		(char)((frame->lme_fmt->pixelformat >> 24) & 0xFF), frame->width, frame->height);

	lme_dbg("[LME] num_planes(%d)\n", frame->num_planes);
	for (i = 0; i < frame->num_planes; i++) {
		lme_dbg("[LME] bytesused[%d] = %d\n", i, frame->bytesused[i]);
	}

	return 0;
}

static int lme_v4l2_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);

	lme_dbg("[LME] count(%d), type(%d), memory(%d)\n", reqbufs->count, reqbufs->type,
		reqbufs->memory);
	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int lme_v4l2_querybuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);
	lme_dbg("[LME] index(%d), type(%d), memory(%d), m.planes(%p), length(%d)\n", buf->index,
		buf->type, buf->memory, buf->m.planes, buf->length);

	return v4l2_m2m_querybuf(file, ctx->m2m_ctx, buf);
}

static int lme_check_vb2_qbuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;
	struct vb2_plane planes[VB2_MAX_PLANES];
	int plane;
	int ret = 0;

	lme_dbg("[LME]\n");
	if (q->fileio) {
		lme_info("[ERR] file io in progress\n");
		ret = -EBUSY;
		goto q_err;
	}

	if (b->type != q->type) {
		lme_info("[ERR] buf type is invalid(%d != %d)\n", b->type, q->type);
		ret = -EINVAL;
		goto q_err;
	}

	if (b->index >= q->num_buffers) {
		lme_info("[ERR] buffer index out of range b_index(%d) q_num_buffers(%d)\n",
			b->index, q->num_buffers);
		ret = -EINVAL;
		goto q_err;
	}

	if (q->bufs[b->index] == NULL) {
		/* Should never happen */
		lme_info("[ERR] buffer is NULL\n");
		ret = -EINVAL;
		goto q_err;
	}

	if (b->memory != q->memory) {
		lme_info("[ERR] invalid memory type b_mem(%d) q_mem(%d)\n", b->memory, q->memory);
		ret = -EINVAL;
		goto q_err;
	}

	vb = q->bufs[b->index];
	if (!vb) {
		lme_info("[ERR] vb is NULL");
		ret = -EINVAL;
		goto q_err;
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(b->type)) {
		/* Is memory for copying plane information present? */
		if (b->m.planes == NULL) {
			lme_info("[ERR] multi-planar buffer passed but "
				 "planes array not provided\n");
			ret = -EINVAL;
			goto q_err;
		}

		if (b->length < vb->num_planes || b->length > VB2_MAX_PLANES) {
			lme_info("[ERR] incorrect planes array length, "
				 "expected %d, got %d\n",
				vb->num_planes, b->length);
			ret = -EINVAL;
			goto q_err;
		}
	}

	if ((b->flags & V4L2_BUF_FLAG_REQUEST_FD) && vb->state != VB2_BUF_STATE_DEQUEUED) {
		lme_info("[ERR] buffer is not in dequeued state\n");
		ret = -EINVAL;
		goto q_err;
	}

	/* For detect vb2 framework err, operate some vb2 functions */
	memset(planes, 0, sizeof(planes[0]) * vb->num_planes);
	/* Copy relevant information provided by the userspace */
	ret = call_bufop(vb->vb2_queue, fill_vb2_buffer, vb, planes);
	if (ret) {
		lme_info("[ERR] vb2_fill_vb2_v4l2_buffer failed (%d)\n", ret);
		goto q_err;
	}

	for (plane = 0; plane < vb->num_planes; ++plane) {
		struct dma_buf *dbuf;

		dbuf = dma_buf_get(planes[plane].m.fd);
		if (IS_ERR_OR_NULL(dbuf)) {
			lme_info("[ERR] invalid dmabuf fd(%d) for plane %d\n", planes[plane].m.fd,
				plane);
			ret = -EINVAL;
			goto q_err;
		}

		if (planes[plane].length == 0)
			planes[plane].length = (unsigned int)dbuf->size;

		if (planes[plane].length < vb->planes[plane].min_length) {
			lme_info("[ERR] invalid dmabuf length %u for plane %d, "
				 "minimum length %u length %u\n",
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

static int lme_check_qbuf(struct file *file, struct v4l2_m2m_ctx *m2m_ctx, struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;

	lme_dbg("[LME]\n");
	vq = v4l2_m2m_get_vq(m2m_ctx, buf->type);
	if (!V4L2_TYPE_IS_OUTPUT(vq->type) && (buf->flags & V4L2_BUF_FLAG_REQUEST_FD)) {
		lme_info("[ERR] requests cannot be used with capture buffers\n");
		return -EPERM;
	}
	return lme_check_vb2_qbuf(vq, buf);
}

static int lme_v4l2_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);
	int ret;

	lme_dbg("[LME] buf->type:%d, %s\n", buf->type,
		V4L2_TYPE_IS_OUTPUT(buf->type) ? "input" : "output");

	if (!ctx->lme_dev->use_hw_cache_operation) {
		/* Save flags for cache sync of V4L2_MEMORY_DMABUF */
		if (V4L2_TYPE_IS_OUTPUT(buf->type))
			ctx->s_frame.flags = buf->flags;
		else
			ctx->d_frame.flags = buf->flags;
	}

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
	if (ret) {
		dev_err(ctx->lme_dev->dev, "v4l2_m2m_qbuf failed ret(%d) check(%d)\n", ret,
			lme_check_qbuf(file, ctx->m2m_ctx, buf));
	}

	return ret;
}

static int lme_v4l2_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);

	lme_dbg("[LME] buf->type:%d, %s\n", buf->type,
		V4L2_TYPE_IS_OUTPUT(buf->type) ? "input" : "output");

	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int lme_power_clk_enable(struct lme_dev *lme)
{
	int ret;

	lme_info("[LME]\n");
	if (in_interrupt())
		ret = pm_runtime_get(lme->dev);
	else
		ret = pm_runtime_get_sync(lme->dev);

	if (ret < 0) {
		dev_err(lme->dev, "%s=%d: Failed to enable local power\n", __func__, ret);
		return ret;
	}

	if (!IS_ERR(lme->pclk)) {
		ret = clk_prepare_enable(lme->pclk);
		if (ret) {
			dev_err(lme->dev, "%s: Failed to enable PCLK (err %d)\n", __func__, ret);
			goto err_pclk;
		}
	}

	if (!IS_ERR(lme->aclk)) {
		ret = clk_enable(lme->aclk);
		if (ret) {
			dev_err(lme->dev, "%s: Failed to enable ACLK (err %d)\n", __func__, ret);
			goto err_aclk;
		}
	}

	return 0;
err_aclk:
	if (!IS_ERR(lme->pclk))
		clk_disable_unprepare(lme->pclk);
err_pclk:
	pm_runtime_put(lme->dev);

	return ret;
}

static void lme_power_clk_disable(struct lme_dev *lme)
{
	lme_info("[LME]\n");
	camerapp_hw_lme_stop(lme->pmio);

	if (!IS_ERR(lme->aclk))
		clk_disable_unprepare(lme->aclk);

	if (!IS_ERR(lme->pclk))
		clk_disable_unprepare(lme->pclk);

	pm_runtime_put(lme->dev);
}

static struct pablo_lme_v4l2_ops lme_v4l2_ops = {
	.m2m_streamon = v4l2_m2m_streamon,
	.m2m_streamoff = v4l2_m2m_streamoff,
};

static int lme_v4l2_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);
	struct lme_dev *lme = ctx->lme_dev;
	struct lme_frame *frame;
	int ret;
	int i, j;

	lme_info("[LME] type:%d, %s\n", type, V4L2_TYPE_IS_OUTPUT(type) ? "input" : "output");
	frame = ctx_get_frame(ctx, type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	for (i = 0; i < LME_MAX_BUFS; ++i) {
		for (j = 0; j < LME_MAX_PLANES; ++j) {
			frame->fd[i][j] = -1;
		}
	}

	if (!V4L2_TYPE_IS_OUTPUT(type)) {
		mutex_lock(&lme->m2m.lock);
		if (!test_bit(CTX_DEV_READY, &ctx->flags)) {
			if (!atomic_read(&lme->m2m.in_streamon)) {
				ret = lme_power_clk_enable(lme);
				if (ret) {
					dev_err(lme->dev, "lme_power_clk_enable fail\n");
					mutex_unlock(&lme->m2m.lock);
					return ret;
				}
			}
			atomic_inc(&lme->m2m.in_streamon);
			set_bit(CTX_DEV_READY, &ctx->flags);
		}
		mutex_unlock(&lme->m2m.lock);

		lme_alloc_internal_mem(ctx);
	}

	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int lme_v4l2_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(fh);
	struct lme_dev *lme = ctx->lme_dev;

	lme_info("[LME] type:%d, %s\n", type, V4L2_TYPE_IS_OUTPUT(type) ? "input" : "output");
	if (!V4L2_TYPE_IS_OUTPUT(type)) {
		mutex_lock(&lme->m2m.lock);
		if (test_bit(CTX_DEV_READY, &ctx->flags)) {
			atomic_dec(&lme->m2m.in_streamon);
			if (!atomic_read(&lme->m2m.in_streamon)) {
				if (lme->hw_lme_ops->sw_reset(lme->pmio))
					dev_err(lme->dev, "lme_sw_reset fail\n");

				if (lme->hw_lme_ops->wait_idle(lme->pmio))
					dev_err(lme->dev, "lme_hw_wait_idle fail\n");

				lme_power_clk_disable(lme);

				atomic_set(&lme->frame_cnt.fs, 0);
				atomic_set(&lme->frame_cnt.fe, 0);
			}
			clear_bit(CTX_DEV_READY, &ctx->flags);
		}
		mutex_unlock(&lme->m2m.lock);

		lme_free_internal_mem(ctx);
	}

	return lme->v4l2_ops->m2m_streamoff(file, ctx->m2m_ctx, type);
}

static int lme_v4l2_s_ctrl(struct file *file, void *priv, struct v4l2_control *ctrl)
{
	int ret = 0;
	lme_dbg("[LME] v4l2_s_ctrl = %d (%d)\n", ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_CAMERAPP_PREINIT_CONTROL:
		break;
	case V4L2_CID_CAMERAPP_PERFRAME_CONTROL:
		break;
	default:
		ret = -EINVAL;

		lme_dbg("Err: Invalid ioctl id(%d)\n", ctrl->id);
		break;
	}

	return ret;
}

static int lme_dma_buf_map(struct lme_ctx *ctx, enum LME_BUF_TYPE type, int dmafd, bool is_need_kva)
{
	struct device *dev = ctx->lme_dev->dev;
	int err;

	lme_dbg("[LME] type:%d, fd:%d\n", type, dmafd);
	ctx->dmabuf[type] = dma_buf_get(dmafd);
	if (IS_ERR(ctx->dmabuf[type])) {
		err = PTR_ERR(ctx->dmabuf[type]);
		lme_info("[ERR] Failed to 1) get %d (type: %d, fd: %d)\n", err, type, dmafd);
		goto err_buf_get;
	}

	lme_dbg("[LME] dvaddr - fd(%d) type %d, addr = %p (%s)\n", dmafd, type, ctx->dmabuf[type],
		ctx->dmabuf[type]->exp_name);
	ctx->attachment[type] = dma_buf_attach(ctx->dmabuf[type], dev);
	if (IS_ERR(ctx->attachment[type])) {
		err = PTR_ERR(ctx->attachment[type]);
		lme_info("[ERR] Failed to 2) attach %d (type: %d, fd: %d)\n", err, type, dmafd);
		goto err_buf_attach;
	}

	ctx->sgt[type] = pkv_dma_buf_map_attachment(ctx->attachment[type], DMA_TO_DEVICE);
	if (IS_ERR(ctx->sgt[type])) {
		err = PTR_ERR(ctx->sgt[type]);
		lme_info("[ERR] Failed to 3) map_attachement %d (type: %d, fd: %d)\n", err, type,
			dmafd);
		goto err_buf_attachment;
	}

	if (is_need_kva == true) {
		ctx->kvaddr[type] = (ulong)pkv_dma_buf_vmap(ctx->dmabuf[type]);
		if (!ctx->kvaddr[type]) {
			err = -ENOMEM;
			lme_info("[ERR] Failed to 4) buf_vmap (type: %d, fd: %d)\n", type, dmafd);
			goto err_buf_vmap;
		}
		lme_dbg("kvaddr - fd(%d) type %d, addr = %#lx\n", dmafd, type, ctx->kvaddr[type]);
	}

	return 0;

err_buf_vmap:
	pkv_dma_buf_unmap_attachment(ctx->attachment[type], ctx->sgt[type], DMA_BIDIRECTIONAL);
err_buf_attachment:
	dma_buf_detach(ctx->dmabuf[type], ctx->attachment[type]);
err_buf_attach:
	dma_buf_put(ctx->dmabuf[type]);
err_buf_get:
	ctx->dmabuf[type] = NULL;
	ctx->attachment[type] = NULL;
	ctx->sgt[type] = NULL;

	return err;
}

static void lme_dma_buf_unmap(struct lme_ctx *ctx, int type)
{
	lme_dbg("[LME]\n");
	if (!ctx->dmabuf[type])
		return;

	if (ctx->kvaddr[type]) {
		pkv_dma_buf_vunmap(ctx->dmabuf[type], (void *)(ctx->kvaddr[type]));
		ctx->kvaddr[type] = 0;
	}

	pkv_dma_buf_unmap_attachment(ctx->attachment[type], ctx->sgt[type], DMA_BIDIRECTIONAL);
	dma_buf_detach(ctx->dmabuf[type], ctx->attachment[type]);
	dma_buf_put(ctx->dmabuf[type]);

	ctx->sgt[type] = NULL;
	ctx->attachment[type] = NULL;
	ctx->dmabuf[type] = NULL;

	return;
}

static void lme_dma_buf_unmap_all(struct lme_ctx *ctx)
{
	int type;
	lme_dbg("[LME]\n");

	for (type = 0; type < LME_DMA_COUNT; type++) {
		if (ctx->dmabuf[type])
			lme_dma_buf_unmap(ctx, type);
	}
}

static void lme_print_cache(struct lme_ctx *ctx)
{
	lme_info("[LME] CACHE: only %s (%d)\n", ctx->lme_dev->use_hw_cache_operation ? "HW" : "SW",
		ctx->lme_dev->dev->dma_coherent);
}

static void lme_print_pre_control_param(struct lme_pre_control_params *pre_control_params)
{
	lme_info(
		"[LME] op_mode(%d), scenario(%d), sps_mode(%d), time_out(%dms x %d = %d) msec, buffer_queue_size(%d)\n",
		pre_control_params->op_mode, pre_control_params->scenario,
		pre_control_params->sps_mode, LME_TIMEOUT_MSEC, pre_control_params->time_out,
		LME_TIMEOUT_MSEC * pre_control_params->time_out,
		pre_control_params->buffer_queue_size);
}

static void lme_print_perframe_control_param(struct lme_post_control_params *post_control_params)
{
	lme_info("[LME] is_first(%d), cr_fd(%d), cr_size(%d), size(%dx%d), stride(%d)\n",
		post_control_params->is_first, post_control_params->cr_buf_fd,
		post_control_params->cr_buf_size, post_control_params->curr_roi_width,
		post_control_params->curr_roi_height, post_control_params->curr_roi_stride);

	lme_info("[LME] prev_buf_fd(%d), prev_buf_size(%d), sad_buf_fd(%d), sad_buf_size(%d)\n",
		post_control_params->prev_buf_fd, post_control_params->prev_buf_size,
		post_control_params->sad_buf_fd, post_control_params->sad_buf_size);
}

static void lme_print_fill_current_frame(struct lme_frame *s_frame, struct lme_frame *d_frame)
{
	lme_info("[curr_in] addr %pad, size %d\n", &s_frame->addr.curr_in,
		s_frame->addr.curr_in_size);
	lme_info("[prev_in] addr %pad, size %d\n", &s_frame->addr.prev_in,
		s_frame->addr.prev_in_size);
	lme_info("[mv_out] addr %pad, size %d\n", &d_frame->addr.mv_out, d_frame->addr.mv_out_size);
	lme_info("[sad_out] addr %pad, size %d\n", &d_frame->addr.sad_out,
		d_frame->addr.sad_out_size);
}

static void lme_print_debugging_log(struct lme_ctx *ctx)
{
	lme_info("[LME] print debugging info\n");
	lme_print_cache(ctx);
	lme_print_pre_control_param(&ctx->pre_control_params);
	lme_print_perframe_control_param(&ctx->post_control_params);
	lme_print_fill_current_frame(&ctx->s_frame, &ctx->d_frame);
	camerapp_hw_lme_print_dma_address(&ctx->s_frame, &ctx->d_frame, &ctx->mbmv);
}

static struct pablo_lme_sys_ops lme_sys_ops = {
	.copy_from_user = copy_from_user,
};

static int lme_v4l2_s_ext_ctrls(struct file *file, void *priv, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct lme_ctx *ctx = fh_to_lme_ctx(file->private_data);
	struct lme_dev *lme = ctx->lme_dev;
	struct lme_pre_control_params *pre_control_params;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;

	lme_dbg("[LME]\n");

	BUG_ON(!ctx);

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		lme_dbg("ctrl ID:%d\n", ext_ctrl->id);
		switch (ext_ctrl->id) {
		case V4L2_CID_CAMERAPP_PREINIT_CONTROL: /* videodev2_exynos_camera.h */
			lme_dbg("V4L2_CID_CAMERAPP_PREINIT_CONTROL(%d) ptr %p, size %lu\n",
				V4L2_CID_CAMERAPP_PREINIT_CONTROL, ext_ctrl->ptr,
				sizeof(struct lme_pre_control_params));
			ret = lme->sys_ops->copy_from_user(&ctx->pre_control_params, ext_ctrl->ptr,
				sizeof(struct lme_pre_control_params));
			if (ret) {
				dev_err(lme->dev, "copy_from_user is fail(%d)\n", ret);
				goto p_err;
			}

			pre_control_params = &ctx->pre_control_params;

			if (pre_control_params->time_out) {
				pre_control_params->time_out =
					QUOTIENT_TO_100(pre_control_params->time_out);
			} else {
				lme_info("[LME] Use default time_out\n");
				pre_control_params->time_out = LME_WDT_CNT;
			}

			lme_print_pre_control_param(&ctx->pre_control_params);
			lme_print_cache(ctx);
			break;
		case V4L2_CID_CAMERAPP_PERFRAME_CONTROL:
			lme_dbg("V4L2_CID_CAMERAPP_PERFRAME_CONTROL(%d) ptr %p, size %lu\n",
				V4L2_CID_CAMERAPP_PERFRAME_CONTROL, ext_ctrl->ptr,
				sizeof(struct lme_post_control_params));
			ret = lme->sys_ops->copy_from_user(&ctx->post_control_params, ext_ctrl->ptr,
				sizeof(struct lme_post_control_params));
			if (ret) {
				dev_err(lme->dev, "copy_from_user is fail(%d)\n", ret);
				goto p_err;
			}

			if (lme_get_debug_level())
				lme_print_perframe_control_param(&ctx->post_control_params);
			break;
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = lme_v4l2_s_ctrl(file, ctx, &ctrl);
			if (ret) {
				lme_dbg("lme_v4l2_s_ctrl is fail(%d)\n", ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

static const struct v4l2_ioctl_ops lme_v4l2_ioctl_ops = {
	.vidioc_querycap = lme_v4l2_querycap,

	.vidioc_g_fmt_vid_cap_mplane = lme_v4l2_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane = lme_v4l2_g_fmt_mplane,

	.vidioc_try_fmt_vid_cap_mplane = lme_v4l2_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane = lme_v4l2_try_fmt_mplane,

	.vidioc_s_fmt_vid_cap_mplane = lme_v4l2_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane = lme_v4l2_s_fmt_mplane,

	.vidioc_reqbufs = lme_v4l2_reqbufs,
	.vidioc_querybuf = lme_v4l2_querybuf,

	.vidioc_qbuf = lme_v4l2_qbuf,
	.vidioc_dqbuf = lme_v4l2_dqbuf,

	.vidioc_streamon = lme_v4l2_streamon,
	.vidioc_streamoff = lme_v4l2_streamoff,

	.vidioc_s_ctrl = lme_v4l2_s_ctrl,
	.vidioc_s_ext_ctrls = lme_v4l2_s_ext_ctrls,
};

static int lme_ctx_stop_req(struct lme_ctx *ctx)
{
	struct lme_ctx *curr_ctx;
	struct lme_dev *lme = ctx->lme_dev;
	int ret = 0;

	lme_dbg("[LME]\n");
	curr_ctx = v4l2_m2m_get_curr_priv(lme->m2m.m2m_dev);
	if (!test_bit(CTX_RUN, &ctx->flags) || (curr_ctx != ctx))
		return 0;

	set_bit(CTX_ABORT, &ctx->flags);

	ret = wait_event_timeout(lme->wait, !test_bit(CTX_RUN, &ctx->flags), LME_TIMEOUT);

	if (ret == 0) {
		dev_err(lme->dev, "device failed to stop request\n");
		ret = -EBUSY;
	}

	return ret;
}

static int lme_vb2_queue_setup(struct vb2_queue *vq, unsigned int *num_buffers,
	unsigned int *num_planes, unsigned int sizes[], struct device *alloc_devs[])
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vq);
	struct lme_frame *frame;
	int i;

	lme_dbg("[LME]\n");
	frame = ctx_get_frame(ctx, vq->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	/* Get number of planes from format_list in driver */
	*num_planes = frame->num_planes;
	lme_dbg("[LME] num_planes:%d\n", *num_planes);
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = frame->bytesused[i];
		alloc_devs[i] = ctx->lme_dev->dev;
	}

	return 0;
}

static void lme_vb2_buf_sync(struct vb2_buffer *vb, struct lme_frame *frame, int action)
{
	unsigned int flags = frame->flags;
	bool cache_clean = !(flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN);
	bool cache_invalidate = !(flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE);
	int i, num_planes;

	lme_dbg("%s: action(%d), clean(%d), invalidate(%d)\n",
		V4L2_TYPE_IS_OUTPUT(vb->type) ? "input" : "output", action, cache_clean,
		cache_invalidate);

	if (cache_clean == false && cache_invalidate == false)
		return;

	num_planes = (vb->num_planes > LME_MAX_PLANES) ? LME_MAX_PLANES : vb->num_planes;
	for (i = 0; i < num_planes; ++i) {
		lme_dbg("index(%d), fd(%d), dbuf_mapped(%d)\n", vb->index, vb->planes[i].m.fd,
			vb->planes[i].dbuf_mapped);
		if (action == LME_BUF_FINISH) {
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

static int lme_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct lme_frame *frame;
	struct lme_post_control_params *post_control_params;
	int i, ret = 0;

	lme_dbg("[LME]\n");
	frame = ctx_get_frame(ctx, vb->vb2_queue->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		for (i = 0; i < frame->lme_fmt->num_planes; i++)
			vb2_set_plane_payload(vb, i, frame->bytesused[i]);
	}

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		post_control_params = &ctx->post_control_params;
		ret = lme_dma_buf_map(ctx, LME_BUF_CR, post_control_params->cr_buf_fd, true);
		if (ret) {
			lme_info("[LME][ERR] fail lme_dma_buf_map(CR). ret:%d\n", ret);
			goto p_err;
		}
		ret = lme_dma_buf_map(
			ctx, LME_BUF_PREV_IN, post_control_params->prev_buf_fd, false);
		if (ret) {
			lme_info("[LME][ERR] fail lme_dma_buf_map(PREV_IN). ret:%d\n", ret);
			goto p_err;
		}
		ret = lme_dma_buf_map(ctx, LME_BUF_SAD_OUT, post_control_params->sad_buf_fd, false);
		if (ret) {
			lme_info("[LME][ERR] fail lme_dma_buf_map(SAD_OUT). ret:%d\n", ret);
			goto p_err;
		}
	}

	if (!ctx->lme_dev->use_hw_cache_operation)
		lme_vb2_buf_sync(vb, frame, LME_BUF_PREPARE);

p_err:
	return ret;
}

static void lme_vb2_buf_finish(struct vb2_buffer *vb)
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct lme_frame *frame;

	lme_dbg("[LME]\n");
	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type) &&
		unlikely(test_bit(LME_DBG_TIME, &debug_lme)))
		lme_info("index(%d) shot_time %lld us, hw_time %lld us\n", vb->index,
			ctx->time_dbg.shot_time_stamp, ctx->time_dbg.hw_time_stamp);

	frame = ctx_get_frame(ctx, vb->vb2_queue->type);
	if (!ctx->lme_dev->use_hw_cache_operation && frame)
		lme_vb2_buf_sync(vb, frame, LME_BUF_FINISH);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		lme_dma_buf_unmap(ctx, LME_BUF_CR);
		lme_dma_buf_unmap(ctx, LME_BUF_PREV_IN);
		lme_dma_buf_unmap(ctx, LME_BUF_SAD_OUT);
	}
}

static void lme_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	lme_dbg("[LME]\n");
	if (ctx->m2m_ctx)
		v4l2_m2m_buf_queue(ctx->m2m_ctx, v4l2_buf);
}

static void lme_vb2_buf_cleanup(struct vb2_buffer *vb)
{
	lme_dbg("[LME]\n");
	/* No operation */
}

static void lme_vb2_lock(struct vb2_queue *vq)
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vq);

	lme_dbg("[LME]\n");
	mutex_lock(&ctx->lme_dev->lock);
}

static void lme_vb2_unlock(struct vb2_queue *vq)
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vq);

	lme_dbg("[LME]\n");
	mutex_unlock(&ctx->lme_dev->lock);
}

static void lme_cleanup_queue(struct lme_ctx *ctx)
{
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	lme_dbg("[LME]\n");
	while (v4l2_m2m_num_src_bufs_ready(ctx->m2m_ctx) > 0) {
		src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		lme_dbg("src_index(%d)\n", src_vb->vb2_buf.index);
	}

	while (v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) > 0) {
		dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);
		lme_dbg("dst_index(%d)\n", dst_vb->vb2_buf.index);
	}
}

static int lme_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vq);

	lme_dbg("[LME]\n");
	set_bit(CTX_STREAMING, &ctx->flags);

	return 0;
}

static void lme_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct lme_ctx *ctx = vb2_get_drv_priv(vq);
	int ret;

	lme_dbg("[LME]\n");
	ret = lme_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->lme_dev->dev, "wait timeout\n");

	clear_bit(CTX_STREAMING, &ctx->flags);

	/* release all queued buffers in multi-buffer scenario*/
	lme_cleanup_queue(ctx);
}

static struct vb2_ops lme_vb2_ops = {
	.queue_setup = lme_vb2_queue_setup,
	.buf_prepare = lme_vb2_buf_prepare,
	.buf_finish = lme_vb2_buf_finish,
	.buf_queue = lme_vb2_buf_queue,
	.buf_cleanup = lme_vb2_buf_cleanup,
	.wait_finish = lme_vb2_lock,
	.wait_prepare = lme_vb2_unlock,
	.start_streaming = lme_vb2_start_streaming,
	.stop_streaming = lme_vb2_stop_streaming,
};

static int queue_init(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq)
{
	struct lme_ctx *ctx = priv;
	int ret;

	lme_dbg("[LME]\n");
	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->ops = &lme_vb2_ops;
	src_vq->mem_ops = &vb2_dma_sg_memops;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = (unsigned int)sizeof(struct vb2_lme_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->ops = &lme_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_sg_memops;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = (unsigned int)sizeof(struct vb2_lme_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	return vb2_queue_init(dst_vq);
}

static struct is_mem *__get_iommu_mem(struct lme_dev *lme)
{
	struct is_mem *mem;
	struct pablo_device_iommu_group *iommu_group;

	if (lme->use_cloader_iommu_group) {
		iommu_group = pablo_iommu_group_get(lme->cloader_iommu_group_id);
		mem = &iommu_group->mem;
	} else {
		mem = &lme->mem;
	}

	return mem;
}

static void lme_free_internal_mem(struct lme_ctx *ctx)
{
	struct is_mem *mem;

	lme_dbg("[LME]\n");

	mem = __get_iommu_mem(ctx->lme_dev);

	if (!IS_ERR_OR_NULL(ctx->mbmv.pb_mbmv0)) {
		lme_info("[LME] Free mbmv0(%u)\n", ctx->mbmv.mbmv0_size);
		CALL_BUFOP(ctx->mbmv.pb_mbmv0, free, ctx->mbmv.pb_mbmv0);
		ctx->mbmv.pb_mbmv0 = NULL;
	}

	if (!IS_ERR_OR_NULL(ctx->mbmv.pb_mbmv1)) {
		lme_info("[LME] Free mbmv1(%u)\n", ctx->mbmv.mbmv1_size);
		CALL_BUFOP(ctx->mbmv.pb_mbmv1, free, ctx->mbmv.pb_mbmv1);
		ctx->mbmv.pb_mbmv1 = NULL;
	}
}

static int lme_alloc_internal_mem(struct lme_ctx *ctx)
{
	int ret = 0;
	struct is_mem *mem;
	size_t size;
	struct lme_frame *frame;
	frame = &ctx->s_frame;

	ctx->mbmv.width = frame->width;
	ctx->mbmv.height = frame->height;
	camerapp_hw_lme_get_mbmv_size(&(ctx->mbmv.width), &(ctx->mbmv.height));
	ctx->mbmv.width_align = ALIGN(ctx->mbmv.width, 16);

	ctx->mbmv.mbmv0_size = (ctx->mbmv.width_align) * (ctx->mbmv.height);
	ctx->mbmv.mbmv1_size = (ctx->mbmv.width_align) * (ctx->mbmv.height);
	lme_info("[LME] input size(%dx%d) -> mbmv(%d(->%d)x%d), mbmv0(%d), mbmv1(%d)\n",
		frame->width, frame->height, ctx->mbmv.width, ctx->mbmv.width_align,
		ctx->mbmv.height, ctx->mbmv.mbmv0_size, ctx->mbmv.mbmv1_size);

	lme_free_internal_mem(ctx);

	mem = __get_iommu_mem(ctx->lme_dev);

	/* alloc mbmv0 */
	size = ctx->mbmv.mbmv0_size;

	ctx->mbmv.pb_mbmv0 = CALL_PTR_MEMOP(mem, alloc, mem->priv, size, "system-uncached", 0);
	if (IS_ERR_OR_NULL(ctx->mbmv.pb_mbmv0)) {
		dev_err(ctx->lme_dev->dev, "failed to allocate buffer for mbmv0");
		ctx->mbmv.pb_mbmv0 = NULL;
		ret = -ENOMEM;
		goto func_exit;
	}

	ctx->mbmv.kva_mbmv0 = CALL_BUFOP(ctx->mbmv.pb_mbmv0, kvaddr, ctx->mbmv.pb_mbmv0);
	ctx->mbmv.dva_mbmv0 = CALL_BUFOP(ctx->mbmv.pb_mbmv0, dvaddr, ctx->mbmv.pb_mbmv0);
	lme_info("[LME] Allocated mbmv0: kva(%#lx), dva(%#llx), size(%zu)\n", ctx->mbmv.kva_mbmv0,
		ctx->mbmv.dva_mbmv0, size);

	/* alloc mbmv1 */
	size = ctx->mbmv.mbmv1_size;

	ctx->mbmv.pb_mbmv1 = CALL_PTR_MEMOP(mem, alloc, mem->priv, size, "system-uncached", 0);
	if (IS_ERR_OR_NULL(ctx->mbmv.pb_mbmv1)) {
		dev_err(ctx->lme_dev->dev, "failed to allocate buffer for mbmv1");
		ctx->mbmv.pb_mbmv1 = NULL;
		ret = -ENOMEM;
		goto err_addr_mbmv0;
	}

	ctx->mbmv.kva_mbmv1 = CALL_BUFOP(ctx->mbmv.pb_mbmv1, kvaddr, ctx->mbmv.pb_mbmv1);
	ctx->mbmv.dva_mbmv1 = CALL_BUFOP(ctx->mbmv.pb_mbmv1, dvaddr, ctx->mbmv.pb_mbmv1);
	lme_info("[LME] Allocated mbmv1: kva(%#lx), dva(%#llx), size(%zu)\n", ctx->mbmv.kva_mbmv1,
		ctx->mbmv.dva_mbmv1, size);

	return ret;

err_addr_mbmv0:
	lme_free_internal_mem(ctx);

func_exit:
	return ret;
}

static int lme_alloc_pmio_mem(struct lme_dev *lme)
{
	struct is_mem *mem;

	if (atomic_read(&lme->m2m.in_use))
		return 0;

	mem = __get_iommu_mem(lme);

	lme->pb_c_loader_payload = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x8000, NULL, 0);
	if (IS_ERR_OR_NULL(lme->pb_c_loader_payload)) {
		dev_err(lme->dev, "failed to allocate buffer for c-loader payload");
		lme->pb_c_loader_payload = NULL;
		return -ENOMEM;
	}

	lme->kva_c_loader_payload =
		CALL_BUFOP(lme->pb_c_loader_payload, kvaddr, lme->pb_c_loader_payload);
	lme->dva_c_loader_payload =
		CALL_BUFOP(lme->pb_c_loader_payload, dvaddr, lme->pb_c_loader_payload);

	lme->pb_c_loader_header = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x2000, NULL, 0);
	if (IS_ERR_OR_NULL(lme->pb_c_loader_header)) {
		dev_err(lme->dev, "failed to allocate buffer for c-loader header");
		lme->pb_c_loader_header = NULL;
		CALL_BUFOP(lme->pb_c_loader_payload, free, lme->pb_c_loader_payload);
		return -ENOMEM;
	}

	lme->kva_c_loader_header =
		CALL_BUFOP(lme->pb_c_loader_header, kvaddr, lme->pb_c_loader_header);
	lme->dva_c_loader_header =
		CALL_BUFOP(lme->pb_c_loader_header, dvaddr, lme->pb_c_loader_header);

	lme_info("payload_dva(0x%llx) header_dva(0x%llx)\n", lme->dva_c_loader_payload,
		lme->dva_c_loader_header);

	return 0;
}

static void lme_free_pmio_mem(struct lme_dev *lme)
{
	if (!IS_ERR_OR_NULL(lme->pb_c_loader_payload))
		CALL_BUFOP(lme->pb_c_loader_payload, free, lme->pb_c_loader_payload);
	if (!IS_ERR_OR_NULL(lme->pb_c_loader_header))
		CALL_BUFOP(lme->pb_c_loader_header, free, lme->pb_c_loader_header);
}

static int lme_open(struct file *file)
{
	struct lme_dev *lme = video_drvdata(file);
	struct lme_ctx *ctx;
	int ret = 0;

	ctx = pablo_zalloc(sizeof(struct lme_ctx), GFP_KERNEL);

	if (!ctx) {
		dev_err(lme->dev, "no memory for open context\n");
		return -ENOMEM;
	}

	mutex_lock(&lme->m2m.lock);

	lme_info("[LME] open : in_use = %d in_streamon = %d",
		atomic_read(&lme->m2m.in_use), atomic_read(&lme->m2m.in_streamon));

	if (!atomic_read(&lme->m2m.in_use) && lme->pmio_en) {
		ret = lme_alloc_pmio_mem(lme);
		if (ret) {
			dev_err(lme->dev, "PMIO mem alloc failed\n");
			mutex_unlock(&lme->m2m.lock);
			goto err_alloc_pmio_mem;
		}
	}

	atomic_inc(&lme->m2m.in_use);

	mutex_unlock(&lme->m2m.lock);

	INIT_LIST_HEAD(&ctx->node);
	ctx->lme_dev = lme;

	v4l2_fh_init(&ctx->fh, lme->m2m.vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	/* Default color format */
	ctx->s_frame.lme_fmt = &lme_formats[0];
	ctx->d_frame.lme_fmt = &lme_formats[0];

	if (!IS_ERR(lme->pclk)) {
		ret = clk_prepare(lme->pclk);
		if (ret) {
			dev_err(lme->dev, "%s: failed to prepare PCLK(err %d)\n", __func__, ret);
			goto err_pclk_prepare;
		}
	}

	if (!IS_ERR(lme->aclk)) {
		ret = clk_prepare(lme->aclk);
		if (ret) {
			dev_err(lme->dev, "%s: failed to prepare ACLK(err %d)\n", __func__, ret);
			goto err_aclk_prepare;
		}
	}

	/* Setup the device context for mem2mem mode. */
	ctx->m2m_ctx = v4l2_m2m_ctx_init(lme->m2m.m2m_dev, ctx, queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		ret = -EINVAL;
		goto err_ctx;
	}

	lme_info("X\n");
	return 0;

err_ctx:
	if (!IS_ERR(lme->aclk))
		clk_unprepare(lme->aclk);
err_aclk_prepare:
	if (!IS_ERR(lme->pclk))
		clk_unprepare(lme->pclk);
err_pclk_prepare:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	atomic_dec(&lme->m2m.in_use);
	lme_free_pmio_mem(lme);
err_alloc_pmio_mem:
	pablo_free(ctx);

	return ret;
}

static void lme_job_finish(struct lme_dev *lme, struct lme_ctx *ctx)
{
	unsigned long flags;
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	lme_dbg("[LME]\n");
	spin_lock_irqsave(&lme->slock, flags);

	ctx = v4l2_m2m_get_curr_priv(lme->m2m.m2m_dev);
	if (!ctx || !ctx->m2m_ctx) {
		dev_err(lme->dev, "current ctx is NULL\n");
		spin_unlock_irqrestore(&lme->slock, flags);
		return;
	}
	clear_bit(CTX_RUN, &ctx->flags);

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	BUG_ON(!src_vb || !dst_vb);

	lme_print_debugging_log(ctx);
	v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
	v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);

	v4l2_m2m_job_finish(lme->m2m.m2m_dev, ctx->m2m_ctx);

	spin_unlock_irqrestore(&lme->slock, flags);
}

static int lme_release(struct file *file)
{
	struct lme_ctx *ctx = fh_to_lme_ctx(file->private_data);
	struct lme_dev *lme = ctx->lme_dev;

	mutex_lock(&lme->m2m.lock);

	lme_info("[LME] close : in_use = %d in_streamon = %d",
		atomic_read(&lme->m2m.in_use), atomic_read(&lme->m2m.in_streamon));

	atomic_dec(&lme->m2m.in_use);
	if (!atomic_read(&lme->m2m.in_use) && test_bit(DEV_RUN, &lme->state)) {
		dev_err(lme->dev, "%s, lme is still running\n", __func__);
		lme_suspend(lme->dev);
	}

	lme_free_internal_mem(ctx);

	lme_dma_buf_unmap_all(ctx);

	if (test_bit(CTX_DEV_READY, &ctx->flags)) {
		atomic_dec(&lme->m2m.in_streamon);
		if (!atomic_read(&lme->m2m.in_streamon))
			lme_power_clk_disable(lme);
		clear_bit(CTX_DEV_READY, &ctx->flags);
	}

	if (!atomic_read(&lme->m2m.in_use) && lme->pmio_en) {
		lme_free_pmio_mem(lme);
	}

	mutex_unlock(&lme->m2m.lock);

	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	if (!IS_ERR(lme->aclk))
		clk_unprepare(lme->aclk);
	if (!IS_ERR(lme->pclk))
		clk_unprepare(lme->pclk);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	pablo_free(ctx);

	return 0;
}

static const struct v4l2_file_operations lme_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = lme_open,
	.release = lme_release,
	.unlocked_ioctl = video_ioctl2,
};

static void lme_watchdog(struct timer_list *t)
{
	struct lme_wdt *wdt = from_timer(wdt, t, timer);
	struct lme_dev *lme = container_of(wdt, typeof(*lme), wdt);
	struct lme_ctx *ctx;
	unsigned long flags;

	lme_dbg("[LME]\n");
	if (!test_bit(DEV_RUN, &lme->state)) {
		lme_info("[ERR] lme is not running\n");
		return;
	}

	spin_lock_irqsave(&lme->ctxlist_lock, flags);
	ctx = lme->current_ctx;
	if (!ctx) {
		lme_info("[ERR] ctx is empty\n");
		spin_unlock_irqrestore(&lme->ctxlist_lock, flags);
		return;
	}

	if (atomic_read(&lme->wdt.cnt) >= ctx->pre_control_params.time_out) {
		lme_info("[ERR] final time_out(cnt:%d)\n", ctx->pre_control_params.time_out);
		is_debug_s2d(true, "lme watchdog s2d");
		if (camerapp_hw_lme_sw_reset(lme->pmio))
			dev_err(lme->dev, "lme sw reset fail\n");

		atomic_set(&lme->wdt.cnt, 0);
		clear_bit(DEV_RUN, &lme->state);
		lme->current_ctx = NULL;
		spin_unlock_irqrestore(&lme->ctxlist_lock, flags);
		lme_job_finish(lme, ctx);
		return;
	}

	spin_unlock_irqrestore(&lme->ctxlist_lock, flags);

	if (test_bit(DEV_RUN, &lme->state)) {
#ifndef USE_VELOCE
		if (!atomic_read(&lme->wdt.cnt))
#endif
		{
			camerapp_lme_sfr_dump(lme->regs_base);
		}
		atomic_inc(&lme->wdt.cnt);
		dev_err(lme->dev, "lme is still running(cnt:%d)\n", atomic_read(&lme->wdt.cnt));
		mod_timer(&lme->wdt.timer, jiffies + LME_TIMEOUT);
	} else {
		lme_dbg("lme finished job\n");
	}
}

static void lme_pmio_config(struct lme_dev *lme, struct c_loader_buffer *clb)
{
	if (unlikely(test_bit(LME_DBG_PMIO_MODE, &debug_lme))) {
		/* APB-DIRECT */
		pmio_cache_sync(lme->pmio);
		clb->clh = NULL;
		clb->num_of_headers = 0;
	} else {
		/* C-LOADER */
		clb->num_of_headers = 0;
		clb->num_of_values = 0;
		clb->num_of_pairs = 0;
		clb->header_dva = lme->dva_c_loader_header;
		clb->payload_dva = lme->dva_c_loader_payload;
		clb->clh = (struct c_loader_header *)lme->kva_c_loader_header;
		clb->clp = (struct c_loader_payload *)lme->kva_c_loader_payload;

		pmio_cache_fsync(lme->pmio, (void *)clb, PMIO_FORMATTER_PAIR);

		if (clb->num_of_pairs > 0)
			clb->num_of_headers++;

		if (unlikely(test_bit(LME_DBG_DUMP_PMIO_CACHE, &debug_lme))) {
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

	CALL_BUFOP(lme->pb_c_loader_payload, sync_for_device, lme->pb_c_loader_payload, 0,
		lme->pb_c_loader_payload->size, DMA_TO_DEVICE);
	CALL_BUFOP(lme->pb_c_loader_header, sync_for_device, lme->pb_c_loader_header, 0,
		lme->pb_c_loader_header->size, DMA_TO_DEVICE);
}

static int lme_run_next_job(struct lme_dev *lme)
{
	unsigned long flags;
	struct lme_ctx *ctx;
	struct c_loader_buffer clb;
	int ret = 0;

	lme_dbg("[LME]\n");
	spin_lock_irqsave(&lme->ctxlist_lock, flags);

	if (lme->current_ctx || list_empty(&lme->context_list)) {
		/* a job is currently being processed or no job is to run */
		spin_unlock_irqrestore(&lme->ctxlist_lock, flags);
		return 0;
	}

	ctx = list_first_entry(&lme->context_list, struct lme_ctx, node);

	list_del_init(&ctx->node);

	clb.header_dva = 0;
	clb.num_of_headers = 0;
	lme->current_ctx = ctx;

	spin_unlock_irqrestore(&lme->ctxlist_lock, flags);

	/*
	 * lme_run_next_job() must not reenter while lme->state is DEV_RUN.
	 * DEV_RUN is cleared when an operation is finished.
	 */
	BUG_ON(test_bit(DEV_RUN, &lme->state));

	if (lme->pmio_en)
		pmio_cache_set_only(lme->pmio, false);

	lme_dbg("lme hw setting\n");
	ctx->time_dbg.shot_time = ktime_get();

	lme->hw_lme_ops->sw_reset(lme->pmio);
	lme_dbg("lme sw reset : done\n");

	lme->hw_lme_ops->set_initialization(lme->pmio);
	lme_dbg("lme initialization : done\n");

	if (lme->pmio_en) {
		pmio_reset_cache(lme->pmio);

		if (!lme_get_use_timeout_wa())
			pmio_cache_set_only(lme->pmio, true);
	}
	ret = lme->hw_lme_ops->update_param(lme->pmio, lme, &clb);
	if (ret) {
		dev_err(lme->dev, "%s: failed to update lme param(%d)", __func__, ret);
		spin_lock_irqsave(&lme->ctxlist_lock, flags);
		lme->current_ctx = NULL;
		spin_unlock_irqrestore(&lme->ctxlist_lock, flags);
		lme_job_finish(lme, ctx);
		return ret;
	}
	lme_dbg("lme param update : done\n");

	if (lme->pmio_en)
		lme_pmio_config(lme, &clb);

	set_bit(DEV_RUN, &lme->state);
	set_bit(CTX_RUN, &ctx->flags);
	mod_timer(&lme->wdt.timer, jiffies + LME_TIMEOUT);

	lme->hw_lme_ops->start(lme->pmio, &clb);

	ctx->time_dbg.shot_time_stamp = ktime_us_delta(ktime_get(), ctx->time_dbg.shot_time);

	if (unlikely(test_bit(LME_DBG_DUMP_REG, &debug_lme) ||
		     test_and_clear_bit(LME_DBG_DUMP_REG_ONCE, &debug_lme)))
		lme->hw_lme_ops->sfr_dump(lme->regs_base);

	ctx->time_dbg.hw_time = ktime_get();

	return 0;
}

static int lme_add_context_and_run(struct lme_dev *lme, struct lme_ctx *ctx)
{
	unsigned long flags;

	lme_dbg("[LME]\n");
	spin_lock_irqsave(&lme->ctxlist_lock, flags);
	list_add_tail(&ctx->node, &lme->context_list);
	spin_unlock_irqrestore(&lme->ctxlist_lock, flags);

	return lme_run_next_job(lme);
}

static void lme_irq_finish(struct lme_dev *lme, struct lme_ctx *ctx, enum LME_IRQ_DONE_TYPE type)
{
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	lme_dbg("[LME]\n");
	clear_bit(DEV_RUN, &lme->state);
	del_timer(&lme->wdt.timer);
	atomic_set(&lme->wdt.cnt, 0);

	clear_bit(CTX_RUN, &ctx->flags);

	BUG_ON(ctx != v4l2_m2m_get_curr_priv(lme->m2m.m2m_dev));

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	BUG_ON(!src_vb || !dst_vb);

	if (type == LME_IRQ_ERROR) {
		lme_print_debugging_log(ctx);
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);

		/* Interrupt disable to ignore error */
		camerapp_hw_lme_interrupt_disable(lme->pmio);
		camerapp_hw_lme_get_intr_status_and_clear(lme->pmio);
	} else if (type == LME_IRQ_FRAME_END) {
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);
	}

	/* Wake up from CTX_ABORT state */
	clear_bit(CTX_ABORT, &ctx->flags);

	spin_lock(&lme->ctxlist_lock);
	lme->current_ctx = NULL;
	spin_unlock(&lme->ctxlist_lock);

	v4l2_m2m_job_finish(lme->m2m.m2m_dev, ctx->m2m_ctx);

	lme_resume(lme->dev);
	wake_up(&lme->wait);
	if (unlikely(test_bit(LME_DBG_DUMP_S2D, &debug_lme)))
		is_debug_s2d(true, "LME_DBG_DUMP_S2D");
}

static irqreturn_t lme_irq_handler(int irq, void *priv)
{
	struct lme_dev *lme = priv;
	struct lme_ctx *ctx;
	u32 irq_status;
	u32 fs, fe;

	lme_dbg("[LME]\n");
	spin_lock(&lme->slock);
	irq_status = camerapp_hw_lme_get_intr_status_and_clear(lme->pmio);

	ctx = lme->current_ctx;
	BUG_ON(!ctx);

	fs = atomic_read(&lme->frame_cnt.fs);
	fe = atomic_read(&lme->frame_cnt.fe);

	if ((irq_status & camerapp_hw_lme_get_int_frame_start()) &&
		(irq_status & camerapp_hw_lme_get_int_frame_end())) {
		lme_info("[WARN][LME][FS:%d][FE:%d] start/end overlapped!!(0x%x)\n", fs, fe,
			irq_status);
	}

	if (irq_status & camerapp_hw_lme_get_int_err()) {
		lme_info("[ERR][LME][FS:%d][FE:%d] handle error interrupt (0x%x)\n", fs, fe,
			irq_status);
		camerapp_lme_sfr_dump(lme->regs_base);
	}

	if (irq_status & camerapp_hw_lme_get_int_frame_start()) {
		ctx->time_dbg.hw_time = ktime_get();

		atomic_inc(&lme->frame_cnt.fs);
		fs = atomic_read(&lme->frame_cnt.fs);
		lme_dbg("[LME][FS:%d][FE:%d] FRAME_START (0x%x)\n", fs, fe, irq_status);
	}

	if (irq_status & camerapp_hw_lme_get_int_frame_end()) {
		ctx->time_dbg.hw_time_stamp = ktime_us_delta(ktime_get(), ctx->time_dbg.hw_time);

		atomic_inc(&lme->frame_cnt.fe);
		fe = atomic_read(&lme->frame_cnt.fe);
		lme_dbg("[LME][FS:%d][FE:%d] FRAME_END(0x%x)\n", fs, fe, irq_status);
		lme_irq_finish(lme, ctx, LME_IRQ_FRAME_END);
	}
	spin_unlock(&lme->slock);

	return IRQ_HANDLED;
}

static void lme_fill_curr_frame(struct lme_dev *lme, struct lme_ctx *ctx)
{
	struct lme_frame *s_frame, *d_frame;
	struct vb2_buffer *src_vb = (struct vb2_buffer *)v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	struct vb2_buffer *dst_vb = (struct vb2_buffer *)v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	struct lme_post_control_params post_control_params = ctx->post_control_params;

	lme_dbg("[LME]\n");
	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;
	lme_dbg("s_frame: %dx%d -> d_frame: %dx%d\n", s_frame->width, s_frame->height,
		d_frame->width, d_frame->height);

	/* dva info */
	lme_dbg("num_planes(%d)\n", s_frame->lme_fmt->num_planes);
	s_frame->addr.curr_in = lme_get_dma_address(src_vb, 0);
	d_frame->addr.mv_out = lme_get_dma_address(dst_vb, 0);

	if (!ctx->sgt[LME_BUF_PREV_IN]) {
		lme_info(
			"[ERR]ctx->sgt[LME_BUF_PREV_IN] is NULL. check post_control_params->prev_buf_fd(%d)\n",
			post_control_params.prev_buf_fd);
	} else {
		s_frame->addr.prev_in = (dma_addr_t)sg_dma_address(ctx->sgt[LME_BUF_PREV_IN]->sgl);
	}

	if (!ctx->sgt[LME_BUF_SAD_OUT]) {
		lme_info(
			"[ERR]ctx->sgt[LME_BUF_SAD_OUT] is NULL. check post_control_params->sad_buf_fd(%d)\n",
			post_control_params.sad_buf_fd);
	} else {
		d_frame->addr.sad_out = (dma_addr_t)sg_dma_address(ctx->sgt[LME_BUF_SAD_OUT]->sgl);
	}

	s_frame->addr.curr_in_size = s_frame->width * s_frame->height;
	s_frame->addr.prev_in_size = post_control_params.prev_buf_size;

	d_frame->addr.mv_out_size = d_frame->width * d_frame->height;
	d_frame->addr.sad_out_size = post_control_params.sad_buf_size;

	if (lme_get_debug_level())
		lme_print_fill_current_frame(s_frame, d_frame);
}

static void lme_m2m_device_run(void *priv)
{
	struct lme_ctx *ctx = priv;
	struct lme_dev *lme = ctx->lme_dev;

	lme_dbg("[LME]\n");
	if (test_bit(DEV_SUSPEND, &lme->state)) {
		dev_err(lme->dev, "lme is in suspend state\n");
		return;
	}

	if (test_bit(CTX_ABORT, &ctx->flags)) {
		dev_err(lme->dev, "aborted lme device run\n");
		return;
	}

	lme_fill_curr_frame(lme, ctx);

	lme_add_context_and_run(lme, ctx);
}

static void lme_m2m_job_abort(void *priv)
{
	struct lme_ctx *ctx = priv;
	int ret;

	lme_dbg("[LME]\n");
	ret = lme_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->lme_dev->dev, "wait timeout\n");
}

static struct v4l2_m2m_ops lme_m2m_ops = {
	.device_run = lme_m2m_device_run,
	.job_abort = lme_m2m_job_abort,
};

static void lme_unregister_m2m_device(struct lme_dev *lme)
{
	lme_dbg("[LME]\n");
	video_unregister_device(lme->m2m.vfd);
	v4l2_m2m_release(lme->m2m.m2m_dev);
	v4l2_device_unregister(&lme->m2m.v4l2_dev);
}

static struct v4l2_m2m_dev *lme_v4l2_m2m_init(void)
{
	return v4l2_m2m_init(&lme_m2m_ops);
}

static void lme_v4l2_m2m_release(struct v4l2_m2m_dev *m2m_dev)
{
	v4l2_m2m_release(m2m_dev);
}

static int lme_register_m2m_device(struct lme_dev *lme, int dev_id)
{
	struct v4l2_device *v4l2_dev;
	struct device *dev;
	struct video_device *vfd;
	int ret = 0;

	lme_dbg("[LME]\n");
	dev = lme->dev;
	v4l2_dev = &lme->m2m.v4l2_dev;

	scnprintf(v4l2_dev->name, sizeof(v4l2_dev->name), "%s.m2m", LME_MODULE_NAME);

	ret = v4l2_device_register(dev, v4l2_dev);
	if (ret) {
		dev_err(lme->dev, "failed to register v4l2 device\n");
		return ret;
	}

	vfd = video_device_alloc();
	if (!vfd) {
		dev_err(lme->dev, "failed to allocate video device\n");
		goto err_v4l2_dev;
	}

	vfd->fops = &lme_v4l2_fops;
	vfd->ioctl_ops = &lme_v4l2_ioctl_ops;
	vfd->release = video_device_release;
	vfd->lock = &lme->lock;
	vfd->vfl_dir = VFL_DIR_M2M;
	vfd->v4l2_dev = v4l2_dev;
	vfd->device_caps = LME_V4L2_DEVICE_CAPS;
	scnprintf(vfd->name, sizeof(vfd->name), "%s:m2m", LME_MODULE_NAME);

	video_set_drvdata(vfd, lme);

	lme->m2m.vfd = vfd;
	lme->m2m.m2m_dev = lme_v4l2_m2m_init();
	if (IS_ERR(lme->m2m.m2m_dev)) {
		dev_err(lme->dev, "failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(lme->m2m.m2m_dev);
		goto err_dev_alloc;
	}

	ret = video_register_device(
		vfd, VFL_TYPE_PABLO, EXYNOS_VIDEONODE_CAMERAPP(CAMERAPP_VIDEONODE_LME));
	if (ret) {
		dev_err(lme->dev, "failed to register video device(%d)\n",
			EXYNOS_VIDEONODE_CAMERAPP(CAMERAPP_VIDEONODE_LME));
		goto err_m2m_dev;
	}

	dev_info(lme->dev, "video node register: %d\n",
		EXYNOS_VIDEONODE_CAMERAPP(CAMERAPP_VIDEONODE_LME));

	return 0;

err_m2m_dev:
	lme_v4l2_m2m_release(lme->m2m.m2m_dev);
err_dev_alloc:
	video_device_release(lme->m2m.vfd);
err_v4l2_dev:
	v4l2_device_unregister(v4l2_dev);

	return ret;
}
#ifdef CONFIG_EXYNOS_IOVMM
static int __attribute__((unused)) lme_sysmmu_fault_handler(
	struct iommu_domain *domain, struct device *dev, unsigned long iova, int flags, void *token)
{
	struct lme_dev *lme = dev_get_drvdata(dev);
#else
static int lme_sysmmu_fault_handler(struct iommu_fault *fault, void *data)
{
	struct lme_dev *lme = data;
	struct device *dev = lme->dev;
	unsigned long iova = fault->event.addr;
#endif

	lme_dbg("[LME]\n");
	if (test_bit(DEV_RUN, &lme->state)) {
		dev_info(dev, "System MMU fault called for IOVA %#lx\n", iova);
		camerapp_lme_sfr_dump(lme->regs_base);
	}

	return 0;
}

static int lme_clk_get(struct lme_dev *lme)
{
	lme_dbg("[LME]\n");
	lme->aclk = devm_clk_get(lme->dev, "gate");
	if (IS_ERR(lme->aclk)) {
		if (PTR_ERR(lme->aclk) != -ENOENT) {
			dev_err(lme->dev, "Failed to get 'gate' clock: %ld", PTR_ERR(lme->aclk));
			return PTR_ERR(lme->aclk);
		}
		dev_info(lme->dev, "'gate' clock is not present\n");
	}

	lme->pclk = devm_clk_get(lme->dev, "gate2");
	if (IS_ERR(lme->pclk)) {
		if (PTR_ERR(lme->pclk) != -ENOENT) {
			dev_err(lme->dev, "Failed to get 'gate2' clock: %ld", PTR_ERR(lme->pclk));
			return PTR_ERR(lme->pclk);
		}
		dev_info(lme->dev, "'gate2' clock is not present\n");
	}

	lme->clk_chld = devm_clk_get(lme->dev, "mux_user");
	if (IS_ERR(lme->clk_chld)) {
		if (PTR_ERR(lme->clk_chld) != -ENOENT) {
			dev_err(lme->dev, "Failed to get 'mux_user' clock: %ld",
				PTR_ERR(lme->clk_chld));
			return PTR_ERR(lme->clk_chld);
		}
		dev_info(lme->dev, "'mux_user' clock is not present\n");
	}

	if (!IS_ERR(lme->clk_chld)) {
		lme->clk_parn = devm_clk_get(lme->dev, "mux_src");
		if (IS_ERR(lme->clk_parn)) {
			dev_err(lme->dev, "Failed to get 'mux_src' clock: %ld",
				PTR_ERR(lme->clk_parn));
			return PTR_ERR(lme->clk_parn);
		}
	} else {
		lme->clk_parn = ERR_PTR(-ENOENT);
	}

	return 0;
}

static void lme_clk_put(struct lme_dev *lme)
{
	lme_dbg("[LME]\n");
	if (!IS_ERR(lme->clk_parn))
		devm_clk_put(lme->dev, lme->clk_parn);

	if (!IS_ERR(lme->clk_chld))
		devm_clk_put(lme->dev, lme->clk_chld);

	if (!IS_ERR(lme->pclk))
		devm_clk_put(lme->dev, lme->pclk);

	if (!IS_ERR(lme->aclk))
		devm_clk_put(lme->dev, lme->aclk);
}

#ifdef CONFIG_PM_SLEEP
static int lme_suspend(struct device *dev)
{
	struct lme_dev *lme = dev_get_drvdata(dev);
	int ret;

	lme_dbg("[LME]\n");
	set_bit(DEV_SUSPEND, &lme->state);

	ret = wait_event_timeout(
		lme->wait, !test_bit(DEV_RUN, &lme->state), LME_TIMEOUT * 50); /* 2sec */
	if (ret == 0)
		dev_err(lme->dev, "wait timeout\n");

	return 0;
}

static int lme_resume(struct device *dev)
{
	struct lme_dev *lme = dev_get_drvdata(dev);

	lme_dbg("[LME]\n");
	clear_bit(DEV_SUSPEND, &lme->state);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int lme_runtime_resume(struct device *dev)
{
	struct lme_dev *lme = dev_get_drvdata(dev);

	lme_info("[LME]");
	if (!IS_ERR(lme->clk_chld) && !IS_ERR(lme->clk_parn)) {
		int ret = clk_set_parent(lme->clk_chld, lme->clk_parn);
		if (ret) {
			dev_err(lme->dev, "%s: Failed to setup MUX: %d\n", __func__, ret);
			return ret;
		}
	}

	return 0;
}

static int lme_runtime_suspend(struct device *dev)
{
	lme_info("[LME]");
	return 0;
}
#endif

static const struct dev_pm_ops lme_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(lme_suspend, lme_resume)
		SET_RUNTIME_PM_OPS(lme_runtime_suspend, lme_runtime_resume, NULL) };

static int lme_pmio_init(struct lme_dev *lme)
{
	int ret;

	lme_info("[LME]");
	camerapp_hw_lme_init_pmio_config(lme);

	lme->pmio = pmio_init(NULL, NULL, &lme->pmio_config);
	if (IS_ERR(lme->pmio)) {
		dev_err(lme->dev, "failed to init lme PMIO: %ld", PTR_ERR(lme->pmio));
		return -ENOMEM;
	}

	if (!lme->pmio_en)
		return 0;

	ret = pmio_field_bulk_alloc(
		lme->pmio, &lme->pmio_fields, lme->pmio_config.fields, lme->pmio_config.num_fields);
	if (ret) {
		dev_err(lme->dev, "failed to alloc lme PMIO fields: %d", ret);
		pmio_exit(lme->pmio);
		return ret;
	}

	return 0;
}

static void lme_pmio_exit(struct lme_dev *lme)
{
	lme_info("[LME]");
	if (lme->pmio) {
		if (lme->pmio_fields)
			pmio_field_bulk_free(lme->pmio, lme->pmio_fields);

		pmio_exit(lme->pmio);
	}
}

static void lme_update_hw_config(void)
{
	lme_dbg("[LME]\n");
	if (exynos_soc_info.main_rev >= 1)
		lme_set_use_timeout_wa(LME_TIMEOUT_WA_NOT_USE);
	else
		lme_set_use_timeout_wa(LME_TIMEOUT_WA_USE);

	lme_info("[LME]soc_info(%d.%d), lme_use_timeout_wa(%d)\n", exynos_soc_info.main_rev,
		 exynos_soc_info.sub_rev, lme_get_use_timeout_wa());
}

static int lme_probe(struct platform_device *pdev)
{
	struct lme_dev *lme;
	struct resource *rsc;
	struct device_node *np;
	int ret = 0;

	lme_info("[LME]\n");
	lme = devm_kzalloc(&pdev->dev, sizeof(struct lme_dev), GFP_KERNEL);
	if (!lme) {
		dev_err(&pdev->dev, "no memory for lme device\n");
		return -ENOMEM;
	}

	lme->dev = &pdev->dev;
	np = lme->dev->of_node;

	dev_set_drvdata(&pdev->dev, lme);

	spin_lock_init(&lme->ctxlist_lock);
	INIT_LIST_HEAD(&lme->context_list);
	spin_lock_init(&lme->slock);
	mutex_init(&lme->lock);
	init_waitqueue_head(&lme->wait);

	rsc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!rsc) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		ret = -ENOMEM;
		goto err_get_mem_res;
	}

	lme->regs_base = devm_ioremap(&pdev->dev, rsc->start, resource_size(rsc));
	if (IS_ERR_OR_NULL(lme->regs_base)) {
		dev_err(&pdev->dev, "Failed to ioremap for reg_base\n");
		ret = ENOMEM;
		goto err_get_mem_res;
	}
	lme->regs_rsc = rsc;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get IRQ (%d)\n", ret);
		goto err_get_irq_res;
	}
	lme->irq = ret;

	ret = devm_request_irq(&pdev->dev, lme->irq, lme_irq_handler, 0, "lme0", lme);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq\n");
		goto err_get_irq_res;
	}

	pablo_set_affinity_irq(lme->irq, true);

	lme->use_hw_cache_operation = of_property_read_bool(np, "dma-coherent");
	lme_info("[LME] %s dma-coherent\n", lme->use_hw_cache_operation ? "use" : "not use");

	lme->use_cloader_iommu_group = of_property_read_bool(np, "iommu_group_for_cloader");
	if (lme->use_cloader_iommu_group) {
		ret = of_property_read_u32(
			np, "iommu_group_for_cloader", &lme->cloader_iommu_group_id);
		if (ret)
			dev_err(&pdev->dev, "fail to get iommu group id for cloader, ret(%d)", ret);
	}

	atomic_set(&lme->wdt.cnt, 0);
	timer_setup(&lme->wdt.timer, lme_watchdog, 0);

	atomic_set(&lme->frame_cnt.fs, 0);
	atomic_set(&lme->frame_cnt.fe, 0);

	ret = lme_clk_get(lme);
	if (ret)
		goto err_wq;

	if (pdev->dev.of_node)
		lme->dev_id = of_alias_get_id(pdev->dev.of_node, "camerapp-lme");
	else
		lme->dev_id = pdev->id;

	if (lme->dev_id < 0)
		lme->dev_id = 0;
	platform_set_drvdata(pdev, lme);

	pm_runtime_enable(&pdev->dev);

	ret = lme_register_m2m_device(lme, lme->dev_id);
	if (ret) {
		dev_err(&pdev->dev, "failed to register m2m device\n");
		goto err_reg_m2m_dev;
	}

#ifdef CONFIG_EXYNOS_IOVMM
	ret = iovmm_activate(lme->dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to attach iommu\n");
		goto err_iommu;
	}
#endif

	ret = lme_power_clk_enable(lme);
	if (ret)
		goto err_power_clk;

#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(&pdev->dev, lme_sysmmu_fault_handler, lme);
#else
	iommu_register_device_fault_handler(&pdev->dev, lme_sysmmu_fault_handler, lme);
#endif

	lme_update_hw_config();

	ret = is_mem_init(&lme->mem, pdev);
	if (ret) {
		dev_err(lme->dev, "lme_mem_probe is fail(%d)", ret);
		goto err_mem_init;
	}

	lme->pmio_en = false;
	ret = lme_pmio_init(lme);
	if (ret) {
		dev_err(&pdev->dev, "%s: lme pmio initialization failed", __func__);
		goto err_pmio_init;
	}

	mutex_init(&lme->m2m.lock);

	lme->variant = camerapp_hw_lme_get_size_constraints(lme->pmio);
	lme->version = camerapp_hw_lme_get_ver(lme->pmio);
	lme->v4l2_ops = &lme_v4l2_ops;
	lme->sys_ops = &lme_sys_ops;
	lme->hw_lme_ops = pablo_get_hw_lme_ops();

	lme_power_clk_disable(lme);

	dev_info(&pdev->dev, "Driver probed successfully(version: %08x)\n", lme->version);

	return 0;

err_pmio_init:
err_mem_init:
	pablo_mem_deinit(&lme->mem);
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(&pdev->dev, NULL, lme);
#else
	iommu_unregister_device_fault_handler(&pdev->dev);
#endif

	lme_power_clk_disable(lme);
err_power_clk:
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_deactivate(lme->dev);
err_iommu:
#endif
	lme_unregister_m2m_device(lme);
err_reg_m2m_dev:
	lme_clk_put(lme);
err_wq:
	if (lme->irq) {
		pablo_set_affinity_irq(lme->irq, false);
		devm_free_irq(&pdev->dev, lme->irq, lme);
	}
err_get_irq_res:
	devm_iounmap(&pdev->dev, lme->regs_base);
err_get_mem_res:
	devm_kfree(&pdev->dev, lme);

	return ret;
}

static int lme_remove(struct platform_device *pdev)
{
	struct lme_dev *lme = platform_get_drvdata(pdev);

	lme_dbg("[LME]\n");
	lme_pmio_exit(lme);

	pablo_mem_deinit(&lme->mem);

#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(&pdev->dev, NULL, lme);
	iovmm_deactivate(lme->dev);
#else
	iommu_unregister_device_fault_handler(&pdev->dev);
#endif

	lme_unregister_m2m_device(lme);

	pm_runtime_disable(&pdev->dev);

	lme_clk_put(lme);

	if (timer_pending(&lme->wdt.timer))
		del_timer(&lme->wdt.timer);

	pablo_set_affinity_irq(lme->irq, false);
	devm_free_irq(&pdev->dev, lme->irq, lme);
	devm_iounmap(&pdev->dev, lme->regs_base);
	devm_kfree(&pdev->dev, lme);

	return 0;
}

static void lme_shutdown(struct platform_device *pdev)
{
	struct lme_dev *lme = platform_get_drvdata(pdev);

	lme_dbg("[LME]\n");
	set_bit(DEV_SUSPEND, &lme->state);

	wait_event(lme->wait, !test_bit(DEV_RUN, &lme->state));

#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_deactivate(lme->dev);
#endif
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static void lme_set_debug_level(int level)
{
	lme_debug_level = level;
}

static struct pkt_lme_ops lme_ops = {
	.get_log_level = lme_get_debug_level,
	.set_log_level = lme_set_debug_level,
	.get_debug = param_get_debug_lme,
	.find_format = lme_find_format,
	.v4l2_querycap = lme_v4l2_querycap,
	.v4l2_g_fmt_mplane = lme_v4l2_g_fmt_mplane,
	.v4l2_try_fmt_mplane = lme_v4l2_try_fmt_mplane,
	.v4l2_s_fmt_mplane = lme_v4l2_s_fmt_mplane,
	.v4l2_reqbufs = lme_v4l2_reqbufs,
	.v4l2_querybuf = lme_v4l2_querybuf,
	.vb2_qbuf = lme_check_vb2_qbuf,
	.check_qbuf = lme_check_qbuf,
	.v4l2_qbuf = lme_v4l2_qbuf,
	.v4l2_dqbuf = lme_v4l2_dqbuf,
	.power_clk_enable = lme_power_clk_enable,
	.power_clk_disable = lme_power_clk_disable,
	.v4l2_streamon = lme_v4l2_streamon,
	.v4l2_streamoff = lme_v4l2_streamoff,
	.v4l2_s_ctrl = lme_v4l2_s_ctrl,
	.v4l2_s_ext_ctrls = lme_v4l2_s_ext_ctrls,
	.v4l2_m2m_init = lme_v4l2_m2m_init,
	.v4l2_m2m_release = lme_v4l2_m2m_release,
	.ctx_stop_req = lme_ctx_stop_req,
	.vb2_queue_setup = lme_vb2_queue_setup,
	.vb2_buf_prepare = lme_vb2_buf_prepare,
	.vb2_buf_finish = lme_vb2_buf_finish,
	.vb2_buf_queue = lme_vb2_buf_queue,
	.vb2_lock = lme_vb2_lock,
	.vb2_unlock = lme_vb2_unlock,
	.cleanup_queue = lme_cleanup_queue,
	.vb2_start_streaming = lme_vb2_start_streaming,
	.vb2_stop_streaming = lme_vb2_stop_streaming,
	.queue_init = queue_init,
	.pmio_init = lme_pmio_init,
	.pmio_exit = lme_pmio_exit,
	.pmio_config = lme_pmio_config,
	.run_next_job = lme_run_next_job,
	.add_context_and_run = lme_add_context_and_run,
	.m2m_device_run = lme_m2m_device_run,
	.m2m_job_abort = lme_m2m_job_abort,
	.clk_get = lme_clk_get,
	.clk_put = lme_clk_put,
	.sysmmu_fault_handler = lme_sysmmu_fault_handler,
	.shutdown = lme_shutdown,
	.suspend = lme_suspend,
	.runtime_resume = lme_runtime_resume,
	.runtime_suspend = lme_runtime_suspend,
	.alloc_pmio_mem = lme_alloc_pmio_mem,
	.free_pmio_mem = lme_free_pmio_mem,
	.job_finish = lme_job_finish,
	.register_m2m_device = lme_register_m2m_device,
};

struct pkt_lme_ops *pablo_kunit_get_lme(void)
{
	return &lme_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_lme);

ulong pablo_get_dbg_lme(void)
{
	return debug_lme;
}
KUNIT_EXPORT_SYMBOL(pablo_get_dbg_lme);

void pablo_set_dbg_lme(ulong dbg)
{
	debug_lme = dbg;
}
KUNIT_EXPORT_SYMBOL(pablo_set_dbg_lme);

#endif

static const struct of_device_id exynos_lme_match[] = {
	{
		.compatible = "samsung,exynos-is-lme",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_lme_match);

static struct platform_driver lme_driver = { .probe = lme_probe,
	.remove = lme_remove,
	.shutdown = lme_shutdown,
	.driver = {
		.name = LME_MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &lme_pm_ops,
		.of_match_table = of_match_ptr(exynos_lme_match),
	}
};
module_driver(lme_driver, platform_driver_register, platform_driver_unregister)

MODULE_AUTHOR("SamsungLSI Camera");
MODULE_DESCRIPTION("EXYNOS CameraPP LME driver");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
