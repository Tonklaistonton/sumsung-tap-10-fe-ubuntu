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

#define pr_fmt(fmt) "[" KBUILD_MODNAME "] V4L2-ITF-UTC-Test: " fmt

#include <linux/completion.h>
#include <linux/refcount.h>
#include <linux/delay.h>
#include "is-video.h"
#include "is-device-sensor.h"
#include "videodev2_exynos_camera.h"
#include "pablo-icpu-adapter.h"
#include "is-core.h"
#include "is-hw.h"
#include "pablo-mem.h"
#include "pablo-utc.h"
#include "punit-test-hw-ip.h"
#include <kunit/test.h>

MODULE_IMPORT_NS(DMA_BUF);
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>

#define NODE_PREFIX		"dev/video"
#define SENSOR_NODE_BASE	100
#define TEST_PLANES_NUM		3
#define TEST_BUF_NUM		8
#define TEST_PRE_SHOT		2

enum sensor_pos {
	PST_POS_SS0,
	PST_POS_SS1,
};

enum stream_index {
	PREVIEW_STREAM,
	REPROCESSING_STREAM,
};

enum v4l2_utc_cam_node {
	PST_NODE_SSX,
	PST_NODE_BYRP,
	PST_NODE_RGBP,
	PST_NODE_MCFP,
	PST_NODE_YUVP,
#if IS_ENABLED(CONFIG_PABLO_V12_0_0) || IS_ENABLED(CONFIG_PABLO_V12_1_0)
	PST_NODE_SHRP,
#endif
	PST_NODE_MCSC,
	PST_NODE_MAX,
};

enum putc_v4l2_itf_cmd {
	PST_V4L2_CSI_INIT,
	PST_V4L2_CSI_PWR,
	PST_V4L2_CSI_SFMT,
	PST_V4L2_CSI_S_STREAM,
	PST_DBM_FIFO_PROCESS_QBUF,
	PST_DBM_CHK_CUR_USE_BUF,
	PST_DBM_BUF_NO_OVERWRITE,
	PST_DBM_ONE_BUFFING_METHOD,
	PST_V4L2_NODE_LIST,
	PST_V4L2_NODE_OPEN_CLOSE,
	PST_V4L2_STM_CFG,
	PST_V4L2_S_FMT_SIZE,
	PST_V4L2_SUP_UN_OR_PACK_FMT,
	PST_V4L2_S_BUF_DVA,
	PST_V4L2_S_STEAM_ON,
	PST_V4L2_S_SENSOR_FPS,
	PST_V4L2_S_STEAM_ON_OFF,
	PST_V4L2_NO_DEP_CFG_PREVIEWW_OR_REPROCESS,
	PST_V4L2_STREAM_OFF_WITHIN_2_FRAME,
	PST_V4L2_S_SETFILE,
	PST_V4L2_LOG_SYNC,
	PST_V4L2_NOTIFY_STM_SET,
	PST_V4L2_FORCE_DONE,
	PST_V4L2_SEL_SENSOR_MODE,
	PST_DBM_ALLOC_INTERNAL_BUF,
	PST_DBM_VB2_Q_DQ,
	PST_DBM_ONLY_ALLOC_YUV_BUF_AT_UPPER_LAYER,
	PST_DBM_GET_CSI_BUF_DVA,
	PST_DBM_GET_BUF_KVA,
	PST_DBM_CSI_OUT_BUF_BIT_WIDTH_SUP,
	PST_S_CTL_CHG_STREAM_PATH_AFTER_STREAM_OFF,
	PUNIT_UTC_MAX,
};

static struct putc_info putc_v4l2_itf_list[] = {
	[PST_V4L2_CSI_INIT] = { 920, P_NO_RUN },
	[PST_V4L2_CSI_PWR] = { 921, P_NO_RUN },
	[PST_V4L2_CSI_SFMT] = { 922, P_NO_RUN },
	[PST_V4L2_CSI_S_STREAM] = { 923, P_NO_RUN },
	[PST_DBM_FIFO_PROCESS_QBUF] = { 929, P_NO_RUN },
	[PST_DBM_CHK_CUR_USE_BUF] = { 930, P_NO_RUN },
	[PST_DBM_BUF_NO_OVERWRITE] = { 931, P_NO_RUN },
	[PST_DBM_ONE_BUFFING_METHOD] = { 934, P_NO_RUN },
	[PST_V4L2_NODE_LIST] = { 945, P_NO_RUN },
	[PST_V4L2_NODE_OPEN_CLOSE] = { 947, P_NO_RUN },
	[PST_V4L2_STM_CFG] = { 948, P_NO_RUN },
	[PST_V4L2_S_FMT_SIZE] = { 949, P_NO_RUN },
	[PST_V4L2_SUP_UN_OR_PACK_FMT] = { 950, P_NO_RUN },
	[PST_V4L2_S_BUF_DVA] = { 951, P_NO_RUN },
	[PST_V4L2_S_STEAM_ON] = { 952, P_NO_RUN },
	[PST_V4L2_S_SENSOR_FPS] = { 953, P_NO_RUN },
	[PST_V4L2_S_STEAM_ON_OFF] = { 954, P_NO_RUN },
	[PST_V4L2_NO_DEP_CFG_PREVIEWW_OR_REPROCESS] = { 955, P_NO_RUN },
	[PST_V4L2_STREAM_OFF_WITHIN_2_FRAME] = { 956, P_NO_RUN },
	[PST_V4L2_S_SETFILE] = { 957, P_NO_RUN },
	[PST_V4L2_LOG_SYNC] = { 958, P_NO_RUN },
	[PST_V4L2_NOTIFY_STM_SET] = { 959, P_NO_RUN },
	[PST_V4L2_FORCE_DONE] = { 960, P_NO_RUN },
	[PST_V4L2_SEL_SENSOR_MODE] = { 961, P_NO_RUN },
	[PST_DBM_ALLOC_INTERNAL_BUF] = { 964, P_NO_RUN },
	[PST_DBM_VB2_Q_DQ] = { 965, P_NO_RUN },
	[PST_DBM_ONLY_ALLOC_YUV_BUF_AT_UPPER_LAYER] = { 966, P_NO_RUN },
	[PST_DBM_GET_CSI_BUF_DVA] = { 967, P_NO_RUN },
	[PST_DBM_GET_BUF_KVA] = { 968, P_NO_RUN },
	[PST_DBM_CSI_OUT_BUF_BIT_WIDTH_SUP] = { 969, P_NO_RUN },
	[PST_S_CTL_CHG_STREAM_PATH_AFTER_STREAM_OFF] = { 977, P_NO_RUN },
};

struct v4l2_utc_sr_info {
	u32	position;
	u32	width;
	u32	height;
	u32	fps;
};

struct v4l2_utc_frame_buffer {
	struct is_priv_buf *buf[TEST_PLANES_NUM];
	struct v4l2_plane planes[TEST_PLANES_NUM];
};

struct cam_node {
	char dev_name[20];
	u32 intype;
	enum is_device_type dev_type;
	enum is_video_dev_num video_id;
	struct file *fd;
	struct is_device_sensor *ids;
	struct v4l2_utc_sr_info sr_info;
};

struct pst_qdq_test_work_info {
	struct kthread_work work;
	struct semaphore qdq_resource;
	u32 instance;
	u32 slot;
};

struct pst_qdq_test_ctx {
	struct task_struct *task_qdq[PST_NODE_MAX];
	struct kthread_worker worker[PST_NODE_MAX];
	struct pst_qdq_test_work_info work_info[PST_NODE_MAX];
};

static struct is_priv_buf *v4l2_utc_pb;
static struct v4l2_plane v4l2_utc_vp[TEST_PLANES_NUM];
static struct v4l2_plane v4l2_utc_vp_img[TEST_PLANES_NUM];
static struct is_priv_buf *yuvp_rta_info[TEST_PLANES_NUM];
static struct v4l2_plane v4l2_utc_yuvp_rta_info[TEST_PLANES_NUM];
struct v4l2_utc_frame_buffer frame_buffer[TEST_BUF_NUM];
static refcount_t v4l2_utc_pb_ref = REFCOUNT_INIT(0);
struct cam_node cam_node[PST_NODE_MAX], r_cam_node[PST_NODE_MAX];
static struct pablo_icpu_adt pia_fake;
static struct pablo_icpu_adt *pia_orig;
static DEFINE_MUTEX(run_get_mutex);
static struct pst_qdq_test_ctx test_qdq_ctx[2];

static int pst_set_csi_v4l2_interface(const char *val, const struct kernel_param *kp);
static int pst_get_csi_v4l2_interface(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_csi_v4l2 = {
	.set = pst_set_csi_v4l2_interface,
	.get = pst_get_csi_v4l2_interface,
};
module_param_cb(test_utc_v4l2_itf, &pablo_param_ops_csi_v4l2, NULL, 0644);

static int v4l2_utc_sensor_s_input(struct cam_node *node, u32 width, u32 height, u32 position)
{
	struct file *file = node->fd;
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_video *iv = video_drvdata(file);
	struct is_device_sensor *ids;
	unsigned int s;

	if (!iv->vd.ioctl_ops->vidioc_s_input)
		return -EINVAL;

	ids = (struct is_device_sensor *)ivc->device;
	ids->sensor_width = width;
	ids->sensor_height = height;
	ids->position = position;

	s = (1 << INPUT_LEADER_SHIFT) & INPUT_LEADER_MASK;
	s |= (SENSOR_SCENARIO_NORMAL << SENSOR_SCN_SHIFT) & SENSOR_SCN_MASK;
	s |= (ids->position << INPUT_POSITION_SHIFT) & INPUT_POSITION_MASK;

	return iv->vd.ioctl_ops->vidioc_s_input(file, NULL, s);
}

static int v4l2_utc_ischain_s_input(struct cam_node *node, u32 stream, u32 position)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);
	unsigned int leader, s_input_index;

	if (!iv->vd.ioctl_ops->vidioc_s_input)
		return -EINVAL;

	if (iv->id == IS_VIDEO_BYRP)
		leader = stream ? 1 : 0;
	else
		leader = 0;

	s_input_index = (stream << INPUT_STREAM_SHIFT) & INPUT_STREAM_MASK;
	s_input_index |= (leader << INPUT_LEADER_SHIFT) & INPUT_LEADER_MASK;
	s_input_index |= (position << INPUT_POSITION_SHIFT) & INPUT_POSITION_MASK;
	s_input_index |= (node->intype << INPUT_INTYPE_SHIFT) & INPUT_INTYPE_MASK;

	return iv->vd.ioctl_ops->vidioc_s_input(file, NULL, s_input_index);
}

static int v4l2_utc_s_parm(struct cam_node *node, u32 framerate)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);
	struct v4l2_streamparm param;

	if (!iv->vd.ioctl_ops->vidioc_s_parm)
		return -EINVAL;

	param.parm.capture.timeperframe.denominator = framerate;
	param.parm.capture.timeperframe.numerator = 1;
	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	return iv->vd.ioctl_ops->vidioc_s_parm(file, NULL, &param);
}

static int v4l2_utc_s_fmt_mplane(struct cam_node *node, u32 pixelformat, u32 width, u32 height)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);
	struct v4l2_format f;

	if (!iv->vd.ioctl_ops->vidioc_s_fmt_vid_out_mplane)
		return -EINVAL;

	f.fmt.pix_mp.pixelformat = pixelformat;
	f.fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;
	f.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
	f.fmt.pix_mp.width = width;
	f.fmt.pix_mp.height = height;
	f.fmt.pix_mp.flags = CAMERA_PIXEL_SIZE_8BIT;

	return iv->vd.ioctl_ops->vidioc_s_fmt_vid_out_mplane(file, NULL, &f);
}

static int v4l2_utc_reqbufs(struct cam_node *node, u32 count)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);
	struct v4l2_requestbuffers b;

	if (!iv->vd.ioctl_ops->vidioc_reqbufs)
		return -EINVAL;

	b.count = count;
	b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	b.memory = V4L2_MEMORY_DMABUF;

	return iv->vd.ioctl_ops->vidioc_reqbufs(file, NULL, &b);
}

static int v4l2_utc_streamon(struct cam_node *node)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);

	if (!iv->vd.ioctl_ops->vidioc_streamon)
		return -EINVAL;

	return iv->vd.ioctl_ops->vidioc_streamon(file, NULL,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

static int v4l2_utc_streamoff(struct cam_node *node)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);

	if (!iv->vd.ioctl_ops->vidioc_streamoff)
		return -EINVAL;

	return iv->vd.ioctl_ops->vidioc_streamoff(file, NULL,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
}

void v4l2_utc_test_pre(struct cam_node *node, u32 width, u32 height)
{
	struct file *file = node->fd;
	struct is_core *core = is_get_is_core();
	struct is_video *iv = video_drvdata(file);
	struct is_mem *mem = iv->mem;
	u32 i, j;

	if (strncmp(current->comm, "sh", TASK_COMM_LEN))
		return;

	if (refcount_read(&v4l2_utc_pb_ref) == 0) {
		refcount_set(&v4l2_utc_pb_ref, 1);
		if (!mem) {
			core = is_get_is_core();
			mem = &core->resourcemgr.mem;
		}

		v4l2_utc_pb = CALL_PTR_MEMOP(mem, alloc, mem->priv,
				width * height * 2, NULL, 0);
		if (IS_ERR(v4l2_utc_pb))
			v4l2_utc_pb = NULL;

		for (i = 0; i < TEST_BUF_NUM; i++) {
			for (j = 0; j < TEST_PLANES_NUM; j++) {
				frame_buffer[i].buf[j] = CALL_PTR_MEMOP(mem, alloc, mem->priv,
						width * height * 2, NULL, 0);
				if (IS_ERR(frame_buffer[i].buf[j]))
					frame_buffer[i].buf[j] = NULL;
			}
		}

		yuvp_rta_info[0] = CALL_PTR_MEMOP(mem, alloc, mem->priv,
						sizeof(struct size_cr_set), NULL, 0);
		yuvp_rta_info[1] = CALL_PTR_MEMOP(mem, alloc, mem->priv,
							sizeof(struct is_yuvp_config), NULL, 0);
		yuvp_rta_info[2] = CALL_PTR_MEMOP(mem, alloc, mem->priv,
							SIZE_OF_META_PLANE, NULL, 0);

		for (i = 0; i < TEST_PLANES_NUM; i++) {
			if (IS_ERR(yuvp_rta_info[i]))
				yuvp_rta_info[i] = NULL;
		}
	} else {
		refcount_inc(&v4l2_utc_pb_ref);
	}
}

void v4l2_utc_test_post(void)
{
	u32 i, j;

	if (strncmp(current->comm, "sh", TASK_COMM_LEN))
		return;

	if (refcount_dec_and_test(&v4l2_utc_pb_ref)) {
		if (v4l2_utc_pb) {
			CALL_VOID_BUFOP(v4l2_utc_pb, free, v4l2_utc_pb);
			v4l2_utc_pb = NULL;
		}

		for (i = 0; i < TEST_BUF_NUM; i++) {
			for (j = 0; j < TEST_PLANES_NUM; j++) {
				CALL_VOID_BUFOP(frame_buffer[i].buf[j], free, frame_buffer[i].buf[j]);
				frame_buffer[i].buf[j] = NULL;
			}
		}

		for (i = 0; i < TEST_PLANES_NUM; i++) {
			CALL_VOID_BUFOP(yuvp_rta_info[i], free, yuvp_rta_info[i]);
			yuvp_rta_info[i] = NULL;
		}
	}
}

static int v4l2_utc_s_ctrl(struct cam_node *node, u32 id, s32 value)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);
	struct v4l2_control c;

	if (!iv->vd.ioctl_ops->vidioc_s_ctrl)
		return -EINVAL;

	c.id = id;
	c.value = value;

	return iv->vd.ioctl_ops->vidioc_s_ctrl(file, NULL, &c);
}

static int v4l2_utc_qbuf(struct cam_node *node, struct v4l2_utc_sr_info *sr_info, int idx)
{
	struct file *file = node->fd;
	struct is_video *iv = video_drvdata(file);
	struct v4l2_buffer buf;
	struct camera2_shot_ext *shot_ext;
	struct camera2_node *leader, *capture;
	int i, ret;

	if (!iv->vd.ioctl_ops->vidioc_qbuf)
		return -EINVAL;

	buf.index = idx;
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.bytesused = 0;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.m.planes = v4l2_utc_vp;
	buf.length = TEST_PLANES_NUM;

	shot_ext = (struct camera2_shot_ext *)CALL_BUFOP(v4l2_utc_pb, kvaddr, v4l2_utc_pb);
	shot_ext->shot.magicNumber = SHOT_MAGIC_NUMBER;

#if !IS_ENABLED(CONFIG_PABLO_V13_0_0)
	if ((iv->id == IS_VIDEO_BYRP) || (iv->id == IS_VIDEO_SS0_NUM)) {
#else
	if ((iv->id == IS_VIDEO_BYRP0) || (iv->id == IS_VIDEO_SS0_NUM)) {
#endif
		leader = &shot_ext->node_group.leader;
		leader->request = 1;
		leader->vid = iv->id;
		leader->pixelformat = V4L2_PIX_FMT_SBGGR10P;
		leader->input.cropRegion[0] = 0;
		leader->input.cropRegion[1] = 0;
		leader->input.cropRegion[2] = sr_info->width;
		leader->input.cropRegion[3] = sr_info->height;
		leader->output.cropRegion[0] = 0;
		leader->output.cropRegion[1] = 0;
		leader->output.cropRegion[2] = sr_info->width;
		leader->output.cropRegion[3] = sr_info->height;

		capture = &shot_ext->node_group.capture[0];

		if (iv->id == IS_VIDEO_SS0_NUM)
			capture->vid = IS_LVN_SS0_VC0;
		else
			capture->vid = IS_LVN_MCSC_P0;

		capture->request = 1;
		capture->pixelformat = V4L2_PIX_FMT_NV21M;
		capture->input.cropRegion[0] = 0;
		capture->input.cropRegion[1] = 0;
		capture->input.cropRegion[2] = sr_info->width;
		capture->input.cropRegion[3] = sr_info->height;
		capture->output.cropRegion[0] = 0;
		capture->output.cropRegion[1] = 0;
		capture->output.cropRegion[2] = sr_info->width;
		capture->output.cropRegion[3] = sr_info->height;

		capture->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		capture->buf.memory = V4L2_MEMORY_DMABUF;
		capture->buf.index = idx;
		capture->buf.m.planes = v4l2_utc_vp_img;
		capture->buf.length = TEST_PLANES_NUM;

		for (i = 0; i < TEST_PLANES_NUM; i++) {
			frame_buffer[idx].planes[i].m.fd = dma_buf_fd(frame_buffer[idx].buf[i]->dma_buf, 0);
			capture->buf.m.planes[i].m.fd = frame_buffer[idx].planes[i].m.fd;
			capture->buf.m.planes[i].length = TEST_PLANES_NUM;
		}

		capture = &shot_ext->node_group.capture[1];

		if (iv->id == IS_VIDEO_BYRP) {
			capture->vid = IS_LVN_YUVP_RTA_INFO;
			capture->request = 1;
			capture->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			capture->buf.memory = V4L2_MEMORY_DMABUF;
			capture->buf.index = idx;
			capture->buf.m.planes = v4l2_utc_yuvp_rta_info;
			capture->buf.length = TEST_PLANES_NUM;

			for (i = 0; i < TEST_PLANES_NUM; i++) {
				v4l2_utc_yuvp_rta_info[i].m.fd = dma_buf_fd(yuvp_rta_info[i]->dma_buf, 0);
				capture->buf.m.planes[i].m.fd = v4l2_utc_yuvp_rta_info[i].m.fd;
				capture->buf.m.planes[i].length = TEST_PLANES_NUM;
			}
		}
	}

	for (i = 0; i < TEST_PLANES_NUM; i++)
		v4l2_utc_vp[i].m.fd = dma_buf_fd(v4l2_utc_pb->dma_buf, 0);

	ret = iv->vd.ioctl_ops->vidioc_qbuf(file, NULL, &buf);

	for (i = 0; i < TEST_PLANES_NUM; i++) {
		put_unused_fd(v4l2_utc_vp[i].m.fd);
		put_unused_fd(frame_buffer[idx].planes[i].m.fd);

		if (iv->id == IS_VIDEO_BYRP)
			put_unused_fd(v4l2_utc_yuvp_rta_info[i].m.fd);
	}

	return ret;
}

static int v4l2_utc_dqbuf(struct cam_node *node, int idx)
{
	struct file *file = node->fd;
	struct v4l2_buffer buf;
	struct is_video *iv = video_drvdata(file);
	int ret;

	if (!iv->vd.ioctl_ops->vidioc_dqbuf)
		return -EINVAL;

	buf.index = idx;
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.bytesused = 0;
	buf.memory = V4L2_MEMORY_DMABUF;
	buf.m.planes = v4l2_utc_vp;
	buf.length = TEST_PLANES_NUM;

	ret = iv->vd.ioctl_ops->vidioc_dqbuf(file, NULL, &buf);

	return ret;
}

static int get_sensor_mode(struct cam_node *node, int pos)
{
	int mode, ret;
	struct is_module_enum *module;
	struct v4l2_utc_sr_info *sr_info = &(node->sr_info);
	struct is_video_ctx *ivc = (struct is_video_ctx *)(node->fd)->private_data;

	node->ids = (struct is_device_sensor *)ivc->device;
	ret = is_search_sensor_module_with_position(node->ids, pos, &module);
	if (ret) {
		err("get sensor mode failed");
		return ret;
	}

	mode = 0;
	sr_info->position = pos;
	sr_info->width = module->cfg[mode].width;
	sr_info->height = module->cfg[mode].height;
	sr_info->fps = module->cfg[mode].max_fps;

	return ret;
}

static int init_cam_node(struct cam_node *node, int index, int position)
{
#ifdef USE_KERNEL_VFS_READ_WRITE
	struct is_video *iv;
#endif
	node->intype = 1;

	switch (index) {
	case PST_NODE_SSX:
		node->video_id = IS_VIDEO_SS0_NUM + position;
		break;
	case PST_NODE_BYRP:
		node->video_id = IS_VIDEO_BYRP;
		node->intype = 0;
		break;
	case PST_NODE_RGBP:
		node->video_id = IS_VIDEO_RGBP;
		break;
	case PST_NODE_MCFP:
		node->video_id = IS_VIDEO_MCFP;
		break;
	case PST_NODE_YUVP:
		node->video_id = IS_VIDEO_YUVP;
		break;
#if (IS_ENABLED(CONFIG_PABLO_V12_0_0) || IS_ENABLED(CONFIG_PABLO_V12_1_0))
	case PST_NODE_SHRP:
		node->video_id = IS_VIDEO_SHRP;
		break;
#endif
	case PST_NODE_MCSC:
		node->video_id = IS_VIDEO_MCSC0;
		break;
	default:
		err("[@]cam node index is err");
		return -EINVAL;
	}

	scnprintf(node->dev_name, sizeof(node->dev_name), "%s%d", NODE_PREFIX,
		node->video_id + SENSOR_NODE_BASE);

#ifdef USE_KERNEL_VFS_READ_WRITE
	node->fd = filp_open(node->dev_name, O_WRONLY | O_TRUNC | O_SYNC, 0666);
	if (IS_ERR(node->fd)) {
		err("[@]can't run v4l2 itf test, fp is err");
		return -EINVAL;
	}

	iv = video_drvdata(node->fd);
	node->dev_type = iv->device_type;
#endif

	return 0;
}

static void pst_dq_work_fn(struct kthread_work *work)
{
	struct pst_qdq_test_work_info *work_info =
			container_of(work, struct pst_qdq_test_work_info, work);
	struct cam_node *node;

	if (work_info->slot == PST_NODE_BYRP)
		node = &cam_node[PST_NODE_BYRP];
	else
		node = &cam_node[PST_NODE_SSX];

	v4l2_utc_dqbuf(node, 0);

	up(&work_info->qdq_resource);
}

static int pst_make_dq_thread(u32 instance, u32 slot)
{
	struct pst_qdq_test_ctx *t_ctx = &test_qdq_ctx[instance];

	kthread_init_worker(&t_ctx->worker[slot]);
	t_ctx->task_qdq[slot] = kthread_run(kthread_worker_fn, &t_ctx->worker[slot],
		"pst-qdq-slot%d", slot);
	if (IS_ERR(t_ctx->task_qdq[slot])) {
		err("failed to create kthread");
		t_ctx->task_qdq[slot] = NULL;
		return -EINVAL;
	}

	t_ctx->work_info[slot].instance = instance;
	t_ctx->work_info[slot].slot = slot;
	kthread_init_work(&t_ctx->work_info[slot].work, pst_dq_work_fn);

	return 0;
}

static int run_reprocessing_stream_test(int node_index)
{
	int i, ret, width, height;

	for (i = 0; i < PST_NODE_MAX; i++) {
		ret = init_cam_node(&r_cam_node[i], i, PST_POS_SS1);
		if (ret)
			return  ret;
	}

	ret = get_sensor_mode(&r_cam_node[PST_NODE_SSX], PST_POS_SS1);
	if (ret)
		return  ret;

	width = r_cam_node[PST_NODE_SSX].sr_info.width;
	height = r_cam_node[PST_NODE_SSX].sr_info.height;

	ret |= v4l2_utc_sensor_s_input(&r_cam_node[PST_NODE_SSX], width, height, PST_POS_SS1);

	ret |= v4l2_utc_s_fmt_mplane(&r_cam_node[PST_NODE_SSX], V4L2_PIX_FMT_SBGGR10P,
		width, height);

	ret |= v4l2_utc_s_ctrl(&r_cam_node[PST_NODE_BYRP], V4L2_CID_IS_END_OF_STREAM, 1);

	for (i = node_index; i < PST_NODE_MAX; i++)
		ret |= v4l2_utc_ischain_s_input(&r_cam_node[i], REPROCESSING_STREAM, PST_POS_SS1);

	for (i = node_index; i < PST_NODE_MAX; i++) {
		ret |= v4l2_utc_s_fmt_mplane(&r_cam_node[i], V4L2_PIX_FMT_SBGGR10P, width, height);
		ret |= v4l2_utc_reqbufs(&r_cam_node[i], 8);
	}

	for (i = PST_NODE_MCSC; i >= node_index; i--)
		ret |= v4l2_utc_streamon(&r_cam_node[i]);

	for (i = node_index; i < PST_NODE_MAX; i++)
		ret |= v4l2_utc_streamoff(&r_cam_node[i]);

	for (i = 0; i < PST_NODE_MAX; i++) {
		if (!IS_ERR(r_cam_node[i].fd))
			filp_close(r_cam_node[i].fd, NULL);
	}

	return ret;
}

static int do_sensor_node_pre_shot(struct v4l2_utc_sr_info *sr_info, int instance, int ret)
{
	int i;
	struct semaphore *qdq_resource;

	qdq_resource = &test_qdq_ctx[instance].work_info[PST_NODE_SSX].qdq_resource;
	sema_init(qdq_resource, TEST_PRE_SHOT);

	for (i = 0; i < TEST_PRE_SHOT; i++) {
		ret |= v4l2_utc_qbuf(&cam_node[PST_NODE_SSX], sr_info, i);

		ret |= down_interruptible(qdq_resource);
	}
	putc_v4l2_itf_list[PST_V4L2_S_BUF_DVA].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_GET_CSI_BUF_DVA].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_ONE_BUFFING_METHOD].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_ONLY_ALLOC_YUV_BUF_AT_UPPER_LAYER].result =
		(!ret) ? P_PASS : P_FAIL;

	return ret;
}

static int check_cur_using_buf(struct is_framemgr *framemgr, struct is_frame *pre_frame)
{
	int process_cnt, flag = 0;
	struct is_frame *cur_frame;

	frame_manager_print_queues(framemgr);

	process_cnt = framemgr->queued_count[FS_PROCESS];

	if (process_cnt) {
		cur_frame = peek_frame(framemgr, FS_PROCESS);

		if (pre_frame == NULL)
			flag |= (process_cnt == 1) ? 0 : P_FAIL;
		else if ((process_cnt <= 1) && cur_frame)
			flag |= (cur_frame->index != pre_frame->index) ? 0 : P_FAIL;
		else
			flag |= P_FAIL;
	}

	return flag;
}

static int do_sensor_node_qdq(struct v4l2_utc_sr_info *sr_info, int instance, int ret)
{
	int i, process_cnt, overwrite_flag = 0, uni_use_buf_flag = 0;
	struct pst_qdq_test_ctx *t_ctx;
	struct semaphore *qdq_resource;
	struct is_video_ctx *ivc;
	struct is_framemgr *framemgr;
	struct is_frame *pre_frame;

	t_ctx = &test_qdq_ctx[instance];
	qdq_resource = &t_ctx->work_info[PST_NODE_SSX].qdq_resource;

	pst_make_dq_thread(instance, PST_NODE_SSX);

	ivc = (struct is_video_ctx *)cam_node[PST_NODE_SSX].fd->private_data;
	framemgr = GET_FRAMEMGR(ivc);

	for (i = 0; i < TEST_BUF_NUM; i++) {
		kthread_queue_work(&t_ctx->worker[PST_NODE_SSX],
			&t_ctx->work_info[PST_NODE_SSX].work);

		ret |= down_interruptible(qdq_resource);
		ret |= v4l2_utc_qbuf(&cam_node[PST_NODE_SSX], sr_info,
			(i + TEST_PRE_SHOT) % TEST_BUF_NUM);

		if (i > 1)
			overwrite_flag |= check_cur_using_buf(framemgr, pre_frame);

		process_cnt = framemgr->queued_count[FS_PROCESS];
		uni_use_buf_flag |= (process_cnt <= 1) ? 0 : P_FAIL;

		pre_frame = peek_frame(framemgr, FS_PROCESS);
	}
	putc_v4l2_itf_list[PST_DBM_CHK_CUR_USE_BUF].result = (!uni_use_buf_flag) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_BUF_NO_OVERWRITE].result = (!overwrite_flag) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_FIFO_PROCESS_QBUF].result = (!ret) ? P_PASS : P_FAIL;

	kthread_stop(test_qdq_ctx[0].task_qdq[PST_NODE_SSX]);

	return ret;
}

static int run_preview_stream_test(void)
{
	int i, fcnt_s, fcnt_e, width, height, position, fps, ret = 0;
	struct v4l2_utc_sr_info *sr_info;
	struct is_device_sensor *ids;

	for (i = 0; i < PST_NODE_MAX; i++)
		ret |= init_cam_node(&cam_node[i], i, PST_POS_SS0);

	putc_v4l2_itf_list[PST_V4L2_NODE_LIST].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_V4L2_NODE_OPEN_CLOSE].result = (!ret) ? P_PASS : P_FAIL;

	if (ret)
		goto err_init_cam_node;

	ret = get_sensor_mode(&cam_node[PST_NODE_SSX], PST_POS_SS0);
	if (ret)
		goto err_g_sensor_mode;

	sr_info = &(cam_node[PST_NODE_SSX].sr_info);
	width = sr_info->width;
	height = sr_info->height;
	position = sr_info->position;
	fps = sr_info->fps;

	ids = cam_node[PST_NODE_SSX].ids;

	info("start sensor(position %d: %d x %d @ %d fps)\n", position, width, height, fps);

	v4l2_utc_test_pre(&cam_node[PST_NODE_SSX], width, height);

	for (i = 0; i < PST_NODE_MAX; i++) {
		if (cam_node[i].dev_type == IS_DEVICE_SENSOR) {
			ret = v4l2_utc_s_ctrl(&cam_node[i], V4L2_CID_IS_END_OF_STREAM, 1);

			ret |= v4l2_utc_sensor_s_input(&cam_node[i], width, height, position);
			putc_v4l2_itf_list[PST_V4L2_CSI_INIT].result = (!ret) ? P_PASS : P_FAIL;
			putc_v4l2_itf_list[PST_V4L2_CSI_PWR].result = (!ret) ? P_PASS : P_FAIL;

			ret |= v4l2_utc_s_parm(&cam_node[i], fps);
			putc_v4l2_itf_list[PST_V4L2_S_SENSOR_FPS].result = (!ret) ? P_PASS : P_FAIL;

			ret |= v4l2_utc_s_ctrl(&cam_node[PST_NODE_SSX], V4L2_CID_IS_S_SENSOR_SIZE,
				(width << SENSOR_SIZE_WIDTH_SHIFT) | height);
		}

		if (cam_node[i].dev_type == IS_DEVICE_ISCHAIN)
			ret |= v4l2_utc_ischain_s_input(&cam_node[i], PREVIEW_STREAM, position);

		if ((i == PST_NODE_SSX) || (i == PST_NODE_BYRP)) {
			ret |= v4l2_utc_s_fmt_mplane(&cam_node[i], V4L2_PIX_FMT_SBGGR10P, width,
					height);
			ret |= v4l2_utc_reqbufs(&cam_node[i], 8);
		}
	}

	putc_v4l2_itf_list[PST_V4L2_STM_CFG].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_V4L2_NOTIFY_STM_SET].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_V4L2_S_FMT_SIZE].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_V4L2_SEL_SENSOR_MODE].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_ALLOC_INTERNAL_BUF].result = (!ret) ? P_PASS : P_FAIL;

	ret |= v4l2_utc_s_ctrl(&cam_node[PST_NODE_SSX], V4L2_CID_IS_SET_SETFILE,
		ISS_SUB_SCENARIO_STILL_PREVIEW);
	putc_v4l2_itf_list[PST_V4L2_S_SETFILE].result = (!ret) ? P_PASS : P_FAIL;

	ret |= v4l2_utc_s_ctrl(&cam_node[PST_NODE_SSX], V4L2_CID_IS_DEBUG_SYNC_LOG, 1);
	putc_v4l2_itf_list[PST_V4L2_LOG_SYNC].result = (!ret) ? P_PASS : P_FAIL;

	for (i = PST_NODE_BYRP; i >= 0; i--)
		ret |= v4l2_utc_streamon(&cam_node[i]);

	putc_v4l2_itf_list[PST_V4L2_S_STEAM_ON].result = (!ret) ? P_PASS : P_FAIL;

	ret |= do_sensor_node_pre_shot(sr_info, PREVIEW_STREAM, ret);

	ret |= v4l2_utc_s_ctrl(&cam_node[PST_NODE_SSX], V4L2_CID_IS_S_STREAM, 1);
	putc_v4l2_itf_list[PST_V4L2_CSI_SFMT].result = (!ret) ? P_PASS : P_FAIL;

	ret |= do_sensor_node_qdq(sr_info, PREVIEW_STREAM, ret);

	ret |= v4l2_utc_qbuf(&cam_node[PST_NODE_BYRP], sr_info, 0);
	ret |= v4l2_utc_dqbuf(&cam_node[PST_NODE_BYRP], 0);
	putc_v4l2_itf_list[PST_V4L2_SUP_UN_OR_PACK_FMT].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_VB2_Q_DQ].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_CSI_OUT_BUF_BIT_WIDTH_SUP].result = (!ret) ? P_PASS : P_FAIL;
	putc_v4l2_itf_list[PST_DBM_GET_BUF_KVA].result = (!ret) ? P_PASS : P_FAIL;

	usleep_range(60000, 70000);

	fcnt_s = ids->fcount;

	ret |= v4l2_utc_s_ctrl(&cam_node[PST_NODE_SSX], V4L2_CID_IS_S_STREAM, 0);
	putc_v4l2_itf_list[PST_V4L2_CSI_S_STREAM].result = (!ret) ? P_PASS : P_FAIL;

	fcnt_e = ids->fcount;
	if (ret == 0) {
		info("stream off within %d frames", fcnt_e - fcnt_s);
		putc_v4l2_itf_list[PST_V4L2_STREAM_OFF_WITHIN_2_FRAME].result =
			(fcnt_e - fcnt_s) <= 2 ? P_PASS : P_FAIL;
	}

	ret |= v4l2_utc_s_ctrl(&cam_node[PST_NODE_SSX], V4L2_CID_IS_FORCE_DONE, 0);
	putc_v4l2_itf_list[PST_V4L2_FORCE_DONE].result = (!ret) ? P_PASS : P_FAIL;

	for (i = 0; i <= PST_NODE_BYRP; i++)
		ret |= v4l2_utc_streamoff(&cam_node[i]);

	putc_v4l2_itf_list[PST_V4L2_S_STEAM_ON_OFF].result = (!ret) ? P_PASS : P_FAIL;

	v4l2_utc_test_post();

err_g_sensor_mode:
err_init_cam_node:
	for (i = 0; i < PST_NODE_MAX; i++) {
		if (!IS_ERR(cam_node[i].fd))
			filp_close(cam_node[i].fd, NULL);
	}

	return ret;
}

static int pst_set_csi_v4l2_interface(const char *val, const struct kernel_param *kp)
{
	int ret;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;

	guard(mutex)(&run_get_mutex);
	pia_orig = pablo_get_icpu_adt();
	pablo_set_icpu_adt(&pia_fake);
	hw->icpu_adt = &pia_fake;

	ret = run_reprocessing_stream_test(PST_NODE_BYRP);
	if (ret)
		err("[@] run reprocessing stream failed %d", ret);
	putc_v4l2_itf_list[PST_V4L2_NO_DEP_CFG_PREVIEWW_OR_REPROCESS].result =
		(!ret) ? P_PASS : P_FAIL;

	ret = run_preview_stream_test();
	if (ret)
		err("[@] v4l2 itf test failed %d", ret);

	putc_v4l2_itf_list[PST_S_CTL_CHG_STREAM_PATH_AFTER_STREAM_OFF].result =
		(!ret) ? P_PASS : P_FAIL;

	pablo_set_icpu_adt(pia_orig);
	hw->icpu_adt = pia_orig;

	return ret;
}

static int pst_get_csi_v4l2_interface(char *buffer, const struct kernel_param *kp)
{
	int i, ret;
	unsigned long result_v4l2 = 0;

	guard(mutex)(&run_get_mutex);
	for (i = 0; i < ARRAY_SIZE(putc_v4l2_itf_list); i++)
		(putc_v4l2_itf_list[i].result == P_PASS) ? set_bit(i, &result_v4l2) : 0;

	ret = scnprintf(buffer, PAGE_SIZE, "%lu %lx ", ARRAY_SIZE(putc_v4l2_itf_list), result_v4l2);
	ret += putc_get_result(buffer + ret, PAGE_SIZE - ret, putc_v4l2_itf_list,
		ARRAY_SIZE(putc_v4l2_itf_list));

	return ret;
}
