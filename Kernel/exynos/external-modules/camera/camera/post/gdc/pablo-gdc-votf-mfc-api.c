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
#include "pablo-gdc-votf-mfc-api.h"

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

static int gdc_device_run(unsigned long i_ino)
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
		gdc_info("no mfc i_ino(%lu)\n ", i_ino);
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

static int mfc_ready(bool ready)
{
	unsigned long dst_i_ino;
	struct gdc_dev *gdc = get_gdc_dev_for_votf();

	if (!gdc) {
		gdc_info("no gdc_dev for votf\n");
		return -EINVAL;
	}

	gdc->mfc_votf.is_mfc_ready = ready;

	gdc_dev_dbg(gdc->dev, "is_mfc_ready:%d\n", ready);

	if (atomic_read(&gdc->wait_mfc)) {
		/* first frame only */
		atomic_set(&gdc->wait_mfc, 0);
		dst_i_ino = gdc_get_dst_inode_num(gdc->votf_ctx);
		if (mfc_core_votf_ready(dst_i_ino) < 0) {
			gdc_dev_info(gdc->dev, "MFC buffer is abnormal(i_ino:%lu)\n", dst_i_ino);
			gdc_job_finish(gdc, gdc->votf_ctx);
		}
		gdc_dev_dbg(gdc->dev, "Waiting for MFC call(i_ino:%lu)\n", dst_i_ino);
	}

	return 0;
}

static const struct mfc_votf_ops ops_to_mfc = {
	.mfc_ready = mfc_ready,
	.gdc_device_run = gdc_device_run,
};

static int register_votf_ops_to_mfc(void *gdc_dev)
{
	struct gdc_dev *gdc = (struct gdc_dev *)gdc_dev;

	if (gdc_dev == NULL)
		return -ENODEV;

	if (mfc_register_votf_cb(&ops_to_mfc) < 0) {
		gdc->mfc_votf.is_ops_registered = false;
		gdc_dev_err(gdc->dev,
			"mfc_register_votf_cb() failed. (mfc_ready:%p, gdc_device_run:%p)\n",
			mfc_ready, gdc_device_run);
		return -EINVAL;
	}
	gdc->mfc_votf.is_ops_registered = true;
	return 0;
}

void gdc_out_votf_otf(void *gdc_dev, void *gdc_ctx)
{
	struct gdc_dev *gdc = (struct gdc_dev *)gdc_dev;
	struct gdc_ctx *ctx = (struct gdc_ctx *)gdc_ctx;

	unsigned long flags;
	unsigned long dst_i_ino;

	if (gdc->mfc_votf.is_ops_registered == false) {
		if (register_votf_ops_to_mfc(gdc) < 0) {
			gdc_dev_err(gdc->dev, "register_votf_ops_to_mfc() failed\n");
			gdc_job_finish(gdc, ctx);
			return;
		}
		ctx->use_mfc_votf_ops = true;
		gdc_dev_info(gdc->dev, "gdc will use mfc APIs for votf\n");
	}

	spin_lock_irqsave(&gdc->ctxlist_lock, flags);
	gdc->votf_ctx = ctx;
	spin_unlock_irqrestore(&gdc->ctxlist_lock, flags);

	/* if mfc is not ready, then wait. */
	if (!gdc->mfc_votf.is_mfc_ready) {
		gdc_dev_dbg(gdc->dev, "is_mfc_ready is false. GDC will wait.\n");
		atomic_set(&gdc->wait_mfc, 1);
		return;
	}

	/* send dst buffer information to MFC DD */
	dst_i_ino = gdc_get_dst_inode_num(ctx);
	if (mfc_core_votf_ready(dst_i_ino) < 0) {
		gdc_dev_info(gdc->dev, "MFC buffer is abnormal(i_ino:%lu)\n", dst_i_ino);
		gdc_job_finish(gdc, ctx);
	}
	gdc_dev_dbg(gdc->dev, "Waiting for MFC call(i_ino:%lu)\n", dst_i_ino);
}

int gdc_clear_mfc_votf_ops(void *gdc_dev)
{
	struct gdc_dev *gdc = (struct gdc_dev *)gdc_dev;

	if (gdc_dev == NULL)
		return -ENODEV;

	if (gdc->mfc_votf.is_ops_registered)
		mfc_unregister_votf_cb();

	gdc->mfc_votf.is_ops_registered = false;
	gdc->mfc_votf.is_mfc_ready = false;

	return 0;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pkt_gdc_votf_ops gdc_votf_ops = {
	.device_run = gdc_device_run,
	.out_votf_otf = gdc_out_votf_otf,
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
	device_for_mfc_votf = NULL;
	gdc_set_device_for_votf(data);
}
KUNIT_EXPORT_SYMBOL(gdc_set_votf_device);

int mfc_register_votf_cb(const struct mfc_votf_ops *votf_ops)
{
	return 0;
}

void mfc_unregister_votf_cb(void)
{
}

int mfc_core_votf_ready(unsigned long i_ino)
{
	return 0;
}
#endif
