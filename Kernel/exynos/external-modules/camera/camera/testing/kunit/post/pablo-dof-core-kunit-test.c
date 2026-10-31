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

#include <linux/dma-heap.h>

#include "pablo-kunit-test.h"
#include "dof/pablo-dof.h"
#include "dof/pablo-hw-api-dof.h"
#include "pmio.h"
#include "pablo-mem.h"

#define NUM_PLANES 1

struct pablo_vb2_fileio_buf {
	void *vaddr;
	unsigned int size;
	unsigned int pos;
	unsigned int queued : 1;
};

struct pablo_vb2_fileio_data {
	unsigned int count;
	unsigned int type;
	unsigned int memory;
	struct pablo_vb2_fileio_buf bufs[VB2_MAX_FRAME];
	unsigned int cur_index;
	unsigned int initial_index;
	unsigned int q_count;
	unsigned int dq_count;
	unsigned read_once : 1;
	unsigned write_immediately : 1;
};

static struct pablo_dof_test_ctx {
	struct dof_dev dof;
	struct dof_ctx ctx;
	struct device dev;
	struct dof_fmt g_fmt;
	struct v4l2_m2m_ctx m2m_ctx;
	struct vb2_queue vq;
	struct file file;
	struct inode f_inode;
	struct vb2_buffer vb;
	struct is_priv_buf payload;
	struct dma_buf dbuf;
	struct platform_device pdev;
	struct dof_frame frame;
} pkt_ctx;

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

static struct is_mem_ops pkt_mem_ops_ion = {
	.init = NULL,
	.cleanup = NULL,
	.alloc = NULL,
};

static struct is_priv_buf_ops pkt_buf_ops = {
	.kvaddr = NULL,
	.dvaddr = NULL,
};

static struct is_priv_buf *pkt_ion_alloc(
	void *ctx, size_t size, const char *heapname, unsigned int flags)
{
	struct is_priv_buf *payload = &pkt_ctx.payload;

	payload->ops = &pkt_buf_ops;

	return payload;
}

static struct pkt_dof_ops *pkt_ops;

int pkt_dof_v4l2_m2m_stream_onoff(
	struct file *file, struct v4l2_m2m_ctx *m2m_ctx, enum v4l2_buf_type type)
{
	if (!V4L2_TYPE_IS_OUTPUT(type))
		return -EINVAL;

	return 0;
}

static struct pablo_dof_v4l2_ops pkt_v4l2_ops = {
	.m2m_streamon = pkt_dof_v4l2_m2m_stream_onoff,
	.m2m_streamoff = pkt_dof_v4l2_m2m_stream_onoff,
};

static unsigned long pkt_copy_from_user(void *dst, const void *src, unsigned long size)
{
	return 0;
}

static struct pablo_dof_sys_ops pkt_dof_sys_ops = {
	.copy_from_user = pkt_copy_from_user,
};

static struct dma_buf_attachment *pkt_dma_buf_attach(struct dma_buf *dma_buf, struct device *dev)
{
	return 0;
}

static void pkt_dma_buf_detach(struct dma_buf *dma_buf, struct dma_buf_attachment *db_attach)
{
}

static struct sg_table *pkt_dma_buf_map_attachment(
	struct dma_buf_attachment *db_attach, enum dma_data_direction dir)
{
	return 0;
}

static void pkt_dma_buf_unmap_attachment(
	struct dma_buf_attachment *db_attach, struct sg_table *sgt, enum dma_data_direction dir)
{
}

static void *pkt_dma_buf_vmap(struct dma_buf *dma_buf)
{
	return 0;
}

static void pkt_dma_buf_vunmap(struct dma_buf *dbuf, void *kva)
{
}

static dma_addr_t pkt_get_dva_from_sg(struct dof_ctx *ctx, int type)
{
	return 0;
}

static dma_addr_t pkt_get_dma_address(struct vb2_buffer *vb2_buf, u32 plane)
{
	return 0;
}

static struct pablo_dof_dma_buf_ops pkt_dof_dma_buf_ops = {
	.attach = pkt_dma_buf_attach,
	.detach = pkt_dma_buf_detach,
	.map_attachment = pkt_dma_buf_map_attachment,
	.unmap_attachment = pkt_dma_buf_unmap_attachment,
	.vmap = pkt_dma_buf_vmap,
	.vunmap = pkt_dma_buf_vunmap,
	.get_dva = pkt_get_dva_from_sg,
	.get_dma_address = pkt_get_dma_address,
};

static u32 pkt_hw_dof_sw_reset(struct pablo_mmio *pmio)
{
	return 0;
}

static int pkt_hw_dof_wait_idle(struct pablo_mmio *pmio)
{
	return 0;
}

static void pkt_hw_dof_set_initialization(struct pablo_mmio *pmio)
{
}

int pkt_hw_dof_update_param(struct pablo_mmio *pmio, struct dof_ctx *current_ctx)
{
	if (!pmio)
		return -EINVAL;

	return 0;
}

void pkt_hw_dof_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb)
{
}

void pkt_dof_sfr_dump(struct pablo_mmio *pmio)
{
}

int pkt_dof_prepare(struct dof_dev *dof)
{
	return 0;
}

static struct pablo_camerapp_hw_dof pkt_hw_dof_ops = {
	.sw_reset = pkt_hw_dof_sw_reset,
	.wait_idle = pkt_hw_dof_wait_idle,
	.set_initialization = pkt_hw_dof_set_initialization,
	.update_param = pkt_hw_dof_update_param,
	.start = pkt_hw_dof_start,
	.sfr_dump = pkt_dof_sfr_dump,
	.prepare = pkt_dof_prepare,
};

static void pablo_dof_get_debug_dof_kunit_test(struct kunit *test)
{
	char *buffer;
	int test_result;

	buffer = __getname();

	test_result = pkt_ops->get_debug(buffer, NULL);
	KUNIT_EXPECT_GT(test, test_result, 0);

	__putname(buffer);
}

static void pablo_dof_find_format_kunit_test(struct kunit *test)
{
	const struct dof_fmt *fmt;
	u32 pixelformat = V4L2_PIX_FMT_GREY;

	fmt = pkt_ops->find_format(pixelformat);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fmt);
	KUNIT_EXPECT_EQ(test, fmt->bitperpixel[0], (u8)8);
	KUNIT_EXPECT_EQ(test, fmt->bitperpixel[1], (u8)0);
	KUNIT_EXPECT_EQ(test, fmt->bitperpixel[2], (u8)0);
	KUNIT_EXPECT_EQ(test, fmt->pixelformat, pixelformat);
}

static void pablo_dof_v4l2_querycap_kunit_test(struct kunit *test)
{
	struct v4l2_capability cap;
	u32 capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			   V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_DEVICE_CAPS;
	u32 device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	int test_result;

	test_result = pkt_ops->v4l2_querycap(NULL, NULL, &cap);

	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_EQ(test, cap.capabilities, capabilities);
	KUNIT_EXPECT_EQ(test, cap.device_caps, device_caps);
	KUNIT_EXPECT_EQ(test, strcmp(cap.driver, DOF_MODULE_NAME), 0);
	KUNIT_EXPECT_EQ(test, strcmp(cap.card, DOF_MODULE_NAME), 0);
}

static void pablo_dof_v4l2_g_fmt_mplane_kunit_test(struct kunit *test)
{
	struct v4l2_format v_fmt;
	int test_result;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	struct dof_fmt *g_fmt = &pkt_ctx.g_fmt;

	/* TC : Check frame error */
	test_result = pkt_ops->v4l2_g_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : pixelformat is not V4L2_PIX_FMT_GREY */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	pkt_ctx.ctx.d_frame.dof_fmt = g_fmt;
	test_result = pkt_ops->v4l2_g_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : pixelformat is V4L2_PIX_FMT_GREY */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	g_fmt->pixelformat = V4L2_PIX_FMT_GREY;
	pkt_ctx.ctx.s_frame.dof_fmt = g_fmt;
	test_result = pkt_ops->v4l2_g_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_v4l2_try_fmt_mplane_kunit_test(struct kunit *test)
{
	struct v4l2_format v_fmt;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	u32 pixelformat[] = { V4L2_PIX_FMT_GREY };
	int i;
	int test_result;

	/* TC : Check v4l2 fmt error */
	test_result = pkt_ops->v4l2_try_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : Check not support dof_fmt */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	test_result = pkt_ops->v4l2_try_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : Check not dof_fmt */
	pkt_ctx.dof.variant = camerapp_hw_dof_get_size_constraints(pkt_ctx.dof.pmio);
	for (i = 0; i < ARRAY_SIZE(pixelformat); ++i) {
		v_fmt.fmt.pix_mp.pixelformat = pixelformat[i];
		test_result = pkt_ops->v4l2_try_fmt_mplane(NULL, fh, &v_fmt);
		KUNIT_EXPECT_EQ(test, test_result, 0);
	}
}

static void pablo_dof_v4l2_s_fmt_mplane_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = &pkt_ctx.dof;
	struct v4l2_format v_fmt;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	v_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	/* TC : Check device is busy line:624*/
	pkt_ctx.m2m_ctx.out_q_ctx.q.streaming = true;
	test_result = pkt_ops->v4l2_s_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, -EBUSY);

	/* TC : Check normal process */
	v_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	pkt_ctx.m2m_ctx.out_q_ctx.q.streaming = false;
	dof->variant = camerapp_hw_dof_get_size_constraints(pkt_ctx.dof.pmio);
	v_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_GREY;
	test_result = pkt_ops->v4l2_s_fmt_mplane(NULL, fh, &v_fmt);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_v4l2_reqbufs_kunit_test(struct kunit *test)
{
	struct v4l2_requestbuffers reqbufs;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_reqbufs(NULL, fh, &reqbufs);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_dof_v4l2_querybuf_kunit_test(struct kunit *test)
{
	struct v4l2_buffer buf;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_querybuf(NULL, fh, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_dof_vb2_qbuf_kunit_test(struct kunit *test)
{
	struct vb2_queue *q = &pkt_ctx.vq;
	struct v4l2_buffer buf;
	int test_result;
	int i;

	/* TC : file io in progress */
	q->fileio = kunit_kzalloc(test, sizeof(struct pablo_vb2_fileio_data), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, q->fileio);
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EBUSY);

	/* TC : buf type is invalid */
	kunit_kfree(test, q->fileio);
	q->fileio = NULL;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : buffer index out of range */
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.index = 2;
	q->num_buffers = 1;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : invalid memory type */
	buf.index = 1;
	q->num_buffers = 2;
	buf.memory = VB2_MEMORY_DMABUF;
	q->memory = VB2_MEMORY_UNKNOWN;
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		q->bufs[i] = kunit_kzalloc(test, sizeof(struct vb2_buffer), GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, q->bufs[i]);
	}
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : planes array not provided */
	q->memory = VB2_MEMORY_DMABUF;
	buf.m.planes = NULL;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : incorrect planes array length */
	buf.m.planes = kunit_kzalloc(test, sizeof(struct v4l2_plane), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf.m.planes);
	buf.length = VB2_MAX_PLANES + 1;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : buffer is not in dequeued state */
	buf.length = VB2_MAX_PLANES - 2;
	buf.flags = V4L2_BUF_FLAG_REQUEST_FD;
	q->bufs[buf.index]->state = VB2_BUF_STATE_IN_REQUEST;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : Check normal process */
	buf.flags = VB2_BUF_STATE_IN_REQUEST;
	q->bufs[buf.index]->num_planes = 1;
	test_result = pkt_ops->vb2_qbuf(q, &buf);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, buf.m.planes);
	for (i = 0; i < VIDEO_MAX_FRAME; i++)
		kunit_kfree(test, q->bufs[i]);
}

static void pablo_dof_check_qbuf_kunit_test(struct kunit *test)
{
	struct v4l2_m2m_ctx *m2m_ctx = &pkt_ctx.m2m_ctx;
	struct v4l2_buffer buf;
	int test_result;

	buf.flags = V4L2_BUF_FLAG_REQUEST_FD;
	test_result = pkt_ops->check_qbuf(NULL, m2m_ctx, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EPERM);

	/* TODO : add successful TC */
}

static void pablo_dof_v4l2_qbuf_kunit_test(struct kunit *test)
{
	struct v4l2_buffer buf;
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_qbuf(file, fh, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_dof_v4l2_dqbuf_kunit_test(struct kunit *test)
{
	struct v4l2_buffer buf;
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	int test_result;

	test_result = pkt_ops->v4l2_dqbuf(file, fh, &buf);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_dof_power_clk_enable_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = &pkt_ctx.dof;
	int test_result;

	test_result = pkt_ops->power_clk_enable(dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_power_clk_disable_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = &pkt_ctx.dof;

	dof->pmio = kunit_kzalloc(test, sizeof(struct pablo_mmio), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dof->pmio);

	dof->pmio->mmio_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dof->pmio->mmio_base);

	pkt_ops->power_clk_disable(dof);

	kunit_kfree(test, dof->pmio->mmio_base);
	kunit_kfree(test, dof->pmio);
}

static void pablo_dof_v4l2_stream_on_kunit_test(struct kunit *test)
{
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	int test_result;

	dof->v4l2_ops = &pkt_v4l2_ops;
	dof->hw_dof_ops = &pkt_hw_dof_ops;

	/* TC : type is not output */
	test_result = pkt_ops->v4l2_streamon(file, fh, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : successful TC */
	test_result = pkt_ops->v4l2_streamon(file, fh, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_v4l2_stream_off_kunit_test(struct kunit *test)
{
	struct file *file = &pkt_ctx.file;
	struct v4l2_fh *fh = &pkt_ctx.ctx.fh;
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	struct platform_device *pdev = &pkt_ctx.pdev;
	int test_result;

	dof->v4l2_ops = &pkt_v4l2_ops;
	dof->hw_dof_ops = &pkt_hw_dof_ops;

	/* TC : type is not V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
	atomic_set(&dof->m2m.in_use, 1);
	test_result = pkt_ops->v4l2_streamoff(file, fh, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : no alloc internal mem */
	dof->use_cloader_iommu_group = 0;
	is_mem_init(&dof->mem, pdev);
	test_result = pkt_ops->v4l2_streamoff(file, fh, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_v4l2_s_ctrl_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct file *file = &pkt_ctx.file;
	struct v4l2_control ctrl;
	struct v4l2_fh *fh = &ctx->fh;
	int test_result;

	file->private_data = fh;

	/* TC : successful TC */
	ctrl.id = V4L2_CID_CAMERAPP_MODEL_CONTROL;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	ctrl.id = V4L2_CID_CAMERAPP_PERFRAME_CONTROL;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : default */
	ctrl.id = 0;
	test_result = pkt_ops->v4l2_s_ctrl(file, NULL, &ctrl);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_dof_v4l2_s_ext_ctrls_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct file *file = &pkt_ctx.file;
	struct v4l2_ext_controls *ctrls;
	int test_result;

	ctx->dof_dev->sys_ops = &pkt_dof_sys_ops;
	ctx->dof_dev->dma_buf_ops = &pkt_dof_dma_buf_ops;

	file->private_data = &ctx->fh;

	ctrls = kunit_kzalloc(test, sizeof(struct v4l2_ext_controls), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctrls);
	ctrls->count = 1;

	ctrls->controls = kunit_kzalloc(test, sizeof(struct v4l2_ext_control), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctrls->controls);

	/* TC : case V4L2_CID_CAMERAPP_MODEL_CONTROL */
	ctrls->controls->id = V4L2_CID_CAMERAPP_MODEL_CONTROL;
	ctrls->count = 1;
	test_result = pkt_ops->v4l2_s_ext_ctrls(file, NULL, ctrls);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : case V4L2_CID_CAMERAPP_PERFRAME_CONTROL */
	ctrls->controls->id = V4L2_CID_CAMERAPP_PERFRAME_CONTROL;
	test_result = pkt_ops->v4l2_s_ext_ctrls(file, NULL, ctrls);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : case default : dof_v4l2_s_ext_ctrls is fail */
	ctrls->controls->id = 0;
	test_result = pkt_ops->v4l2_s_ext_ctrls(file, NULL, ctrls);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	kunit_kfree(test, ctrls->controls);
	kunit_kfree(test, ctrls);
}

static void pablo_dof_ctx_stop_req_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	int test_result;

	ctx->dof_dev->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx->dof_dev->m2m.m2m_dev);
	test_result = pkt_ops->ctx_stop_req(ctx);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	pkt_ops->v4l2_m2m_release(ctx->dof_dev->m2m.m2m_dev);
}

static void pablo_dof_vb2_queue_setup_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_queue *q = &pkt_ctx.vq;
	unsigned int num_buffers;
	unsigned int num_planes = NUM_PLANES;
	unsigned int sizes[NUM_PLANES];
	struct device *alloc_devs[NUM_PLANES];
	int i;
	int test_result;

	for (i = 0; i < num_planes; i++)
		alloc_devs[i] = &pkt_ctx.dev;
	q->drv_priv = ctx;

	/* TC : frame error */
	test_result = pkt_ops->vb2_queue_setup(q, &num_buffers, &num_planes, sizes, alloc_devs);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : check normal process */
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	test_result = pkt_ops->vb2_queue_setup(q, &num_buffers, &num_planes, sizes, alloc_devs);
	KUNIT_EXPECT_EQ(test, num_planes, ctx->s_frame.num_planes);
	KUNIT_EXPECT_EQ(test, sizes[0], ctx->s_frame.bytesused[0]);
	KUNIT_EXPECT_EQ(test, (u64)alloc_devs[0], (u64)ctx->dof_dev->dev);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_vb2_buf_prepare_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_buffer *vb = &pkt_ctx.vb;
	int test_result;

	vb->vb2_queue = &pkt_ctx.vq;
	vb->vb2_queue->drv_priv = &pkt_ctx.ctx;

	ctx->dof_dev->dma_buf_ops = &pkt_dof_dma_buf_ops;
	ctx->s_frame.dof_fmt = &dof_formats[0];
	ctx->d_frame.dof_fmt = &dof_formats[0];

	/* TC : frame is NULL */
	test_result = pkt_ops->vb2_buf_prepare(vb);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* successful TC : V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE; */
	vb->vb2_queue->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	test_result = pkt_ops->vb2_buf_prepare(vb);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* successful TC : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
	vb->vb2_queue->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->dof_dev->use_hw_cache_operation = 1;
	test_result = pkt_ops->vb2_buf_prepare(vb);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* successful TC : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
	ctx->dof_dev->use_hw_cache_operation = 0;
	test_result = pkt_ops->vb2_buf_prepare(vb);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TODO : run dof_vb2_buf_sync */
}

static void pablo_dof_vb2_buf_finish_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_buffer *vb = &pkt_ctx.vb;

	vb->vb2_queue = &pkt_ctx.vq;
	vb->vb2_queue->drv_priv = &pkt_ctx.ctx;

	/* TC : successful TC : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
	vb->vb2_queue->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->dof_dev->use_hw_cache_operation = 1;
	pkt_ops->vb2_buf_finish(vb);

	/* TC : successful TC : V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
	ctx->dof_dev->use_hw_cache_operation = 0;
	pkt_ops->vb2_buf_finish(vb);

	/* TODO : run dof_vb2_buf_sync */
}

static void pablo_dof_vb2_buf_queue_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct v4l2_m2m_buffer m2m_buffer;
	struct vb2_v4l2_buffer *v4l2_buf = &m2m_buffer.vb;
	struct vb2_buffer *vb = &v4l2_buf->vb2_buf;
	struct list_head head;

	vb->vb2_queue = &pkt_ctx.vq;
	ctx->m2m_ctx->out_q_ctx.rdy_queue.prev = &head;
	ctx->m2m_ctx->out_q_ctx.rdy_queue.prev->next = &ctx->m2m_ctx->out_q_ctx.rdy_queue;
	vb->vb2_queue->drv_priv = ctx;
	vb->vb2_queue->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	pkt_ops->vb2_buf_queue(vb);
}

static void pablo_dof_vb2_lock_unlock_kunit_test(struct kunit *test)
{
	struct vb2_queue *q = &pkt_ctx.vq;
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;

	mutex_init(&dof->lock);

	q->drv_priv = &pkt_ctx.ctx;

	pkt_ops->vb2_lock(q);
	pkt_ops->vb2_unlock(q);
}

static void pablo_dof_cleanup_queue_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;

	pkt_ops->cleanup_queue(ctx);
}

static void pablo_dof_vb2_streaming_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_queue *q = &pkt_ctx.vq;
	int test_result;

	q->drv_priv = ctx;

	clear_bit(CTX_STREAMING, &ctx->flags);
	test_result = pkt_ops->vb2_start_streaming(q, 0);
	KUNIT_EXPECT_EQ(test, test_bit(CTX_STREAMING, &ctx->flags), 1);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	ctx->dof_dev->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();
	pkt_ops->vb2_stop_streaming(q);
	KUNIT_EXPECT_EQ(test, test_bit(CTX_STREAMING, &ctx->flags), 0);
	pkt_ops->v4l2_m2m_release(ctx->dof_dev->m2m.m2m_dev);
}

static void pablo_dof_queue_init_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct vb2_queue *src, *dst;
	int test_result;

	src = kunit_kzalloc(test, sizeof(struct vb2_queue), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, src);
	dst = kunit_kzalloc(test, sizeof(struct vb2_queue), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dst);

	test_result = pkt_ops->queue_init(ctx, src, dst);
	KUNIT_EXPECT_EQ(test, src->timestamp_flags, V4L2_BUF_FLAG_TIMESTAMP_COPY);
	KUNIT_EXPECT_EQ(test, src->type, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	KUNIT_EXPECT_EQ(test, dst->timestamp_flags, V4L2_BUF_FLAG_TIMESTAMP_COPY);
	KUNIT_EXPECT_EQ(test, dst->type, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, src);
	kunit_kfree(test, dst);
}

static void pablo_dof_pmio_init_exit_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	int test_result;

	dof->regs_base = kunit_kzalloc(test, 0x10000, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dof->regs_base);

	dof->regs_rsc = kunit_kzalloc(test, sizeof(struct resource), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dof->regs_rsc);

	test_result = pkt_ops->pmio_init(dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->pmio_exit(dof);

	kunit_kfree(test, dof->regs_rsc);
	kunit_kfree(test, dof->regs_base);
}

static void pablo_dof_pmio_config_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	struct c_loader_buffer clb;
	u32 num_of_headers;
	struct c_loader_header clh;
	ulong tmp_dbg_dof = pablo_get_dbg_dof();
	ulong set_dbg_dof = tmp_dbg_dof;

	dof->pb_c_loader_payload = &pkt_ctx.payload;
	dof->pb_c_loader_header = &pkt_ctx.payload;
	dof->pb_c_loader_payload->ops = &pkt_buf_ops;
	dof->pb_c_loader_header->ops = &pkt_buf_ops;
	dof->pmio = kunit_kzalloc(test, sizeof(struct pablo_mmio), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dof->pmio);

	/* TC : normal process (with hw cache : 0) */
	dof->use_hw_cache_operation = 0;
	clb.num_of_pairs = 1;
	num_of_headers = clb.num_of_headers;
	pkt_ops->pmio_config(dof, &clb);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, num_of_headers++);

	/* TC : normal process (with hw cache : 1) */
	dof->use_hw_cache_operation = 1;
	clb.num_of_pairs = 1;
	num_of_headers = clb.num_of_headers;
	pkt_ops->pmio_config(dof, &clb);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, num_of_headers++);

	/* TC : not DOF_DBG_PMIO_MODE */
	set_bit(DOF_DBG_PMIO_MODE, &set_dbg_dof);
	pablo_set_dbg_dof(set_dbg_dof);
	clb.clh = &clh;
	pkt_ops->pmio_config(dof, &clb);
	KUNIT_EXPECT_NULL(test, clb.clh);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, (u32)0);

	/* TC : not DOF_DBG_DUMP_PMIO_CACHE */
	clear_bit(DOF_DBG_PMIO_MODE, &set_dbg_dof);
	set_bit(DOF_DBG_DUMP_PMIO_CACHE, &set_dbg_dof);
	pablo_set_dbg_dof(set_dbg_dof);
	num_of_headers = clb.num_of_headers;
	pkt_ops->pmio_config(dof, &clb);
	KUNIT_EXPECT_EQ(test, clb.num_of_headers, num_of_headers++);

	pablo_set_dbg_dof(tmp_dbg_dof);
	kunit_kfree(test, dof->pmio);
}

static void dof_watchdog(struct timer_list *t)
{
}

static void pablo_dof_run_next_job_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	int test_result;

	INIT_LIST_HEAD(&dof->context_list);
	list_add_tail(&ctx->node, &dof->context_list);

	ctx->dof_dev->sys_ops = &pkt_dof_sys_ops;
	ctx->dof_dev->hw_dof_ops = &pkt_hw_dof_ops;

	timer_setup(&dof->wdt.timer, dof_watchdog, 0);

	dof->context_list.next = (struct list_head *)ctx;
	dof->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();

	/* TC : dof param update done */
	dof->pmio = kunit_kzalloc(test, sizeof(struct pablo_mmio), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dof->pmio);

	dof->pb_c_loader_payload = &pkt_ctx.payload;
	dof->pb_c_loader_header = &pkt_ctx.payload;
	dof->pb_c_loader_payload->ops = &pkt_buf_ops;
	dof->pb_c_loader_header->ops = &pkt_buf_ops;

	test_result = pkt_ops->run_next_job(dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	/* TC : a job is currently being processed or no job is to run */
	test_result = pkt_ops->run_next_job(dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->v4l2_m2m_release(dof->m2m.m2m_dev);
	kunit_kfree(test, dof->pmio);
	del_timer(&dof->wdt.timer);
}

static void pablo_dof_add_context_and_run_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	int test_result;

	dof->current_ctx = &pkt_ctx.ctx;
	INIT_LIST_HEAD(&dof->context_list);
	test_result = pkt_ops->add_context_and_run(dof, ctx);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_m2m_device_run_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	struct dof_ctx *ctx = &pkt_ctx.ctx;
	struct v4l2_m2m_buffer m2m_buffer;

	ctx->dof_dev->sys_ops = &pkt_dof_sys_ops;
	ctx->dof_dev->hw_dof_ops = &pkt_hw_dof_ops;
	ctx->dof_dev->dma_buf_ops = &pkt_dof_dma_buf_ops;

	ctx->s_frame.dof_fmt = &dof_formats[0];
	ctx->d_frame.dof_fmt = &dof_formats[0];

	/* TC : DOF is in suspend state */
	set_bit(DEV_SUSPEND, &ctx->dof_dev->state);
	pkt_ops->m2m_device_run(ctx);

	/* TC : aborted DOF device run */
	clear_bit(DEV_SUSPEND, &ctx->dof_dev->state);
	set_bit(CTX_ABORT, &ctx->flags);
	pkt_ops->m2m_device_run(ctx);

	/* TC : successful TC */
	clear_bit(CTX_ABORT, &ctx->flags);
	INIT_LIST_HEAD(&ctx->m2m_ctx->out_q_ctx.rdy_queue);
	list_add_tail(&m2m_buffer.list, &ctx->m2m_ctx->out_q_ctx.rdy_queue);

	dof->current_ctx = &pkt_ctx.ctx;
	INIT_LIST_HEAD(&dof->context_list);

	dof->pb_c_loader_payload = &pkt_ctx.payload;
	dof->pb_c_loader_header = &pkt_ctx.payload;
	dof->pb_c_loader_payload->ops = &pkt_buf_ops;
	dof->pb_c_loader_header->ops = &pkt_buf_ops;

	pkt_ops->m2m_device_run(ctx);

	/* TODO : successful TC with different perframe_control_params.operation_mode */
}

static void pablo_dof_m2m_job_abort_kunit_test(struct kunit *test)
{
	struct dof_ctx *ctx = &pkt_ctx.ctx;

	ctx->dof_dev->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();
	pkt_ops->m2m_job_abort(ctx);
	pkt_ops->v4l2_m2m_release(ctx->dof_dev->m2m.m2m_dev);
}

static void pablo_dof_clk_get_put_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = &pkt_ctx.dof;
	int test_result;

	test_result = pkt_ops->clk_get(dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->clk_put(dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_sysmmu_fault_handler_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = &pkt_ctx.dof;
	struct iommu_fault *fault;
	int test_result;

	fault = kunit_kzalloc(test, sizeof(struct iommu_fault), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fault);

	dof->dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dof->dev);

	test_result = pkt_ops->sysmmu_fault_handler(fault, dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, dof->dev);
	kunit_kfree(test, fault);
}

static void pablo_dof_shutdown_suspend_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = &pkt_ctx.dof;
	struct platform_device *pdev = &pkt_ctx.pdev;

	pdev->dev.driver_data = dof;
	pkt_ops->shutdown(pdev);
	KUNIT_EXPECT_EQ(test, (int)test_bit(DEV_SUSPEND, &dof->state), 1);

	pkt_ops->suspend(&pdev->dev);
	KUNIT_EXPECT_EQ(test, (int)test_bit(DEV_SUSPEND, &dof->state), 1);
}

static void pablo_dof_runtime_resume_suspend_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = &pkt_ctx.dof;
	struct platform_device *pdev = &pkt_ctx.pdev;
	int test_result;

	pdev->dev.driver_data = dof;
	test_result = pkt_ops->runtime_resume(&pdev->dev);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	test_result = pkt_ops->runtime_suspend(&pdev->dev);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_dof_alloc_free_pmio_mem_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;
	int test_result;

	dof->mem.is_mem_ops = &pkt_mem_ops_ion;

	/* TC : failed to allocate buffer for c-loader payload */
	test_result = pkt_ops->alloc_pmio_mem(dof);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	pkt_mem_ops_ion.alloc = pkt_ion_alloc;

	test_result = pkt_ops->alloc_pmio_mem(dof);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	pkt_ops->free_pmio_mem(dof);
}

static void pablo_dof_job_finish_kunit_test(struct kunit *test)
{
	struct dof_dev *dof = pkt_ctx.ctx.dof_dev;

	dof->m2m.m2m_dev = pkt_ops->v4l2_m2m_init();

	/* TC : ctx is NULL */
	pkt_ops->job_finish(dof, NULL);
	pkt_ops->v4l2_m2m_release(dof->m2m.m2m_dev);

	/* TODO : add successful TC */
}

static void pablo_dof_register_m2m_device_kunit_test(struct kunit *test)
{
	/* TODO : add successful TC */
	/* TODO : add unregister_m2m_device TC */
}

static int backup_log_level;
static int pablo_dof_kunit_test_init(struct kunit *test)
{
	memset(&pkt_ctx, 0, sizeof(pkt_ctx));

	pkt_ops = pablo_kunit_get_dof();

	backup_log_level = pkt_ops->get_log_level();
	pkt_ops->set_log_level(1);

	pkt_ctx.dof.dev = &pkt_ctx.dev;
	pkt_ctx.ctx.dof_dev = &pkt_ctx.dof;
	pkt_ctx.ctx.m2m_ctx = &pkt_ctx.m2m_ctx;
	pkt_ctx.g_fmt.num_planes = 1;
	pkt_ctx.file.f_inode = &pkt_ctx.f_inode;

	return 0;
}

static void pablo_dof_kunit_test_exit(struct kunit *test)
{
	pkt_ops->set_log_level(backup_log_level);
	pkt_ops = NULL;
}

static struct kunit_case pablo_dof_kunit_test_cases[] = {
	KUNIT_CASE(pablo_dof_get_debug_dof_kunit_test),
	KUNIT_CASE(pablo_dof_find_format_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_querycap_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_g_fmt_mplane_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_try_fmt_mplane_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_s_fmt_mplane_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_reqbufs_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_querybuf_kunit_test),
	KUNIT_CASE(pablo_dof_vb2_qbuf_kunit_test),
	KUNIT_CASE(pablo_dof_check_qbuf_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_qbuf_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_dqbuf_kunit_test),
	KUNIT_CASE(pablo_dof_power_clk_enable_kunit_test),
	KUNIT_CASE(pablo_dof_power_clk_disable_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_stream_on_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_stream_off_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_s_ctrl_kunit_test),
	KUNIT_CASE(pablo_dof_v4l2_s_ext_ctrls_kunit_test),
	KUNIT_CASE(pablo_dof_ctx_stop_req_kunit_test),
	KUNIT_CASE(pablo_dof_vb2_queue_setup_kunit_test),
	KUNIT_CASE(pablo_dof_vb2_buf_prepare_kunit_test),
	KUNIT_CASE(pablo_dof_vb2_buf_finish_kunit_test),
	KUNIT_CASE(pablo_dof_vb2_buf_queue_kunit_test),
	KUNIT_CASE(pablo_dof_vb2_lock_unlock_kunit_test),
	KUNIT_CASE(pablo_dof_cleanup_queue_kunit_test),
	KUNIT_CASE(pablo_dof_vb2_streaming_kunit_test),
	KUNIT_CASE(pablo_dof_queue_init_kunit_test),
	KUNIT_CASE(pablo_dof_pmio_init_exit_kunit_test),
	KUNIT_CASE(pablo_dof_pmio_config_kunit_test),
	KUNIT_CASE(pablo_dof_run_next_job_kunit_test),
	KUNIT_CASE(pablo_dof_add_context_and_run_kunit_test),
	KUNIT_CASE(pablo_dof_m2m_device_run_kunit_test),
	KUNIT_CASE(pablo_dof_m2m_job_abort_kunit_test),
	KUNIT_CASE(pablo_dof_clk_get_put_kunit_test),
	KUNIT_CASE(pablo_dof_sysmmu_fault_handler_kunit_test),
	KUNIT_CASE(pablo_dof_shutdown_suspend_kunit_test),
	KUNIT_CASE(pablo_dof_runtime_resume_suspend_kunit_test),
	KUNIT_CASE(pablo_dof_alloc_free_pmio_mem_kunit_test),
	KUNIT_CASE(pablo_dof_job_finish_kunit_test),
	KUNIT_CASE(pablo_dof_register_m2m_device_kunit_test),
	{},
};

struct kunit_suite pablo_dof_kunit_test_suite = {
	.name = "pablo-dof-kunit-test",
	.init = pablo_dof_kunit_test_init,
	.exit = pablo_dof_kunit_test_exit,
	.test_cases = pablo_dof_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_dof_kunit_test_suite);

MODULE_LICENSE("GPL");
