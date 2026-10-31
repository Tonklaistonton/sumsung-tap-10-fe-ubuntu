// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS ISPP GDC driver
 * (FIMC-IS PostProcessing Generic Distortion Correction driver)
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-gdc.h"
#include "votf/pablo-votf.h"
#include "pablo-gdc-votf-gdc-api.h"

static struct device *device_for_mfc_votf;

static struct gdc_dev *get_gdc_dev_for_votf(void)
{
	if (device_for_mfc_votf == NULL)
		return NULL;

	return dev_get_drvdata(device_for_mfc_votf);
}

static unsigned long gdc_get_dst_inode_num(struct gdc_ctx *ctx)
{
	struct vb2_buffer *dst_vb;
	unsigned long inode_num;

	dst_vb = (struct vb2_buffer *)v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	inode_num = file_inode(dst_vb->planes[0].dbuf->file)->i_ino;

	gdc_dev_dbg(ctx->gdc_dev->dev,
		"fd(%d) inode_num(%lu)\n", dst_vb->planes[0].m.fd, inode_num);

	return inode_num;
}

static int gdc_check_received_buffer(struct gdc_ctx *ctx, unsigned long mfc_ino)
{
	struct vb2_buffer *vb2_buf;
	struct v4l2_m2m_buffer *v4l2_buf;
	unsigned long inode_num;
	int ret = GDC_NO_INO, idx = 0;

	v4l2_m2m_for_each_dst_buf(ctx->m2m_ctx, v4l2_buf) {
		vb2_buf = &v4l2_buf->vb.vb2_buf;
		inode_num = file_inode(vb2_buf->planes[0].dbuf->file)->i_ino;
		gdc_dev_dbg(ctx->gdc_dev->dev,
			"buffer[%d] gdc_inode(%lu), mfc_inode(%lu)\n", idx, inode_num, mfc_ino);

		if (inode_num == mfc_ino) {
			if (idx) {
				ret = GDC_DROP_INO;
				gdc_dev_info(ctx->gdc_dev->dev, "MFC drop (%d) frames\n", idx);
			} else
				ret = GDC_GOOD_INO;

			break;
		}
		idx++;
	}

	return ret;
}

void gdc_set_device_for_votf(struct device *dev)
{
	if (dev == NULL)
		return;

	if (device_for_mfc_votf != NULL) {
		gdc_dev_err(dev, "VOTF is busy. Already connected to MFC\n");
		return;
	}

	device_for_mfc_votf = dev;
}

int gdc_device_run(unsigned long i_ino)
{
	unsigned long flags;
	struct gdc_ctx *ctx;
	struct gdc_dev *gdc = get_gdc_dev_for_votf();
	int ret;

	if (!gdc) {
		gdc_info("no gdc_dev\n");
		return -ENOMEM;
	}

	if (!gdc->votf_ctx) {
		gdc_info("no votf_ctx\n");
		return -ENOMEM;
	}
	if (!i_ino) {
		gdc_dev_info(gdc->dev, "no mfc i_ino(%lu)\n ", i_ino);
		return -EINVAL;
	}
	gdc_dev_dbg(gdc->dev, "received inode : (%lu)\n", i_ino);

	spin_lock_irqsave(&gdc->ctxlist_lock, flags);
	ctx = gdc->votf_ctx;
	spin_unlock_irqrestore(&gdc->ctxlist_lock, flags);

	ret = gdc_check_received_buffer(ctx, i_ino);
	if (ret == GDC_GOOD_INO) {
		gdc_fill_curr_frame(gdc, ctx);
		ret = gdc_add_context_and_run(gdc, ctx);
	} else if (ret == GDC_DROP_INO) {
		gdc_job_finish(gdc, ctx);
	}
	/* else == GDC_NO_FD */
	return ret;
}
EXPORT_SYMBOL(gdc_device_run);

int gdc_register_ready_frame_cb(int (*gdc_ready_frame_cb)(unsigned long i_ino))
{
	unsigned long dst_i_ino;
	struct gdc_dev *gdc = get_gdc_dev_for_votf();

	if (!gdc) {
		gdc_info("no gdc_dev\n");
		return -ENOMEM;
	}

	if (!gdc_ready_frame_cb) {
		gdc_info("no gdc_ready_frame_cb\n");
		return -ENOMEM;
	}

	gdc->mfc_votf.gdc_frame_ready = gdc_ready_frame_cb;

	gdc_dev_dbg(gdc->dev, "done\n");

	if (atomic_read(&gdc->wait_mfc)) {
		/* first frame only */
		atomic_set(&gdc->wait_mfc, 0);
		dst_i_ino = gdc_get_dst_inode_num(gdc->votf_ctx);
		if (gdc->mfc_votf.gdc_frame_ready(dst_i_ino) < 0) {
			gdc_dev_info(gdc->dev, "MFC buffer is abnormal(i_ino:%lu)\n", dst_i_ino);
			gdc_job_finish(gdc, gdc->votf_ctx);
		}
	}

	return 0;
}
EXPORT_SYMBOL(gdc_register_ready_frame_cb);

void gdc_out_votf_otf(void *gdc_dev, void *gdc_ctx)
{
	struct gdc_dev *gdc = (struct gdc_dev *)gdc_dev;
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;

	unsigned long flags;
	unsigned long dst_i_ino;

	spin_lock_irqsave(&gdc->ctxlist_lock, flags);
	gdc->votf_ctx = ctx;
	spin_unlock_irqrestore(&gdc->ctxlist_lock, flags);

	/* if mfc is not ready, then wait. */
	if (!gdc->mfc_votf.gdc_frame_ready) {
		atomic_set(&gdc->wait_mfc, 1);
		return;
	}

	/* send dst buffer information to MFC DD */
	dst_i_ino = gdc_get_dst_inode_num(ctx);
	if (gdc->mfc_votf.gdc_frame_ready(dst_i_ino) < 0) {
		gdc_dev_info(gdc->dev, "MFC buffer is abnormal(i_ino:%lu)\n", dst_i_ino);
		gdc_job_finish(gdc, ctx);
	}
	gdc_dev_dbg(gdc->dev, "Waiting for MFC call(i_ino:%lu)\n", dst_i_ino);
}

void gdc_unregister_ready_frame_cb(void)
{
	struct gdc_dev *gdc = get_gdc_dev_for_votf();

	if (!gdc) {
		gdc_info("no gdc_dev\n");
		return;
	}

	gdc->mfc_votf.gdc_frame_ready = NULL;

	gdc_dev_dbg(gdc->dev, "done\n");
}
EXPORT_SYMBOL(gdc_unregister_ready_frame_cb);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pkt_gdc_votf_ops gdc_votf_ops = {
	.device_run = gdc_device_run,
	.out_votf_otf = gdc_out_votf_otf,
	.register_ready_frame_cb = gdc_register_ready_frame_cb,
	.unregister_ready_frame_cb = gdc_unregister_ready_frame_cb,
};

struct pkt_gdc_votf_ops *pablo_kunit_get_gdc_votf_ops(void)
{
	return &gdc_votf_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_gdc_votf_ops);

struct device *gdc_get_votf_device(void)
{
	return device_for_mfc_votf;
}
KUNIT_EXPORT_SYMBOL(gdc_get_votf_device);

void gdc_set_votf_device(struct device *data)
{
	gdc_set_device_for_votf(data);
}
KUNIT_EXPORT_SYMBOL(gdc_set_votf_device);
#endif
