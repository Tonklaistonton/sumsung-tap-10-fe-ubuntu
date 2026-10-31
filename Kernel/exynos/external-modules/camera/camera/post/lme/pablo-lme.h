// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS CAMERA PostProcessing LME driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_LME__H_
#define CAMERAPP_LME__H_

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include "videodev2_exynos_camera.h"
#include <linux/io.h>
#include <linux/dma-buf.h>
#include <linux/version.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>
#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
#include <media/videobuf2-dma-sg.h>
#endif
#include "pablo-kernel-variant.h"
#include "pablo-mmio.h"
#include "pablo-mem.h"

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#define lme_pm_qos_request exynos_pm_qos_request
#define lme_pm_qos_add_request exynos_pm_qos_add_request
#define lme_pm_qos_update_request exynos_pm_qos_update_request
#define lme_pm_qos_remove_request exynos_pm_qos_remove_request
#else
#define lme_pm_qos_request dev_pm_qos_request
#define lme_pm_qos_add_request (arg...)
#define lme_pm_qos_update_request (arg...)
#define lme_pm_qos_remove_request (arg...)
#endif

enum LME_DEBUG_MODE {
	LME_DEBUG_OFF,
	LME_DEBUG_LOG,
};

enum LME_TIMEOUT_WA_MODE {
	LME_TIMEOUT_WA_NOT_USE,
	LME_TIMEOUT_WA_USE,
};

int lme_get_debug_level(void);
uint lme_get_use_timeout_wa(void);
void lme_set_use_timeout_wa(enum LME_TIMEOUT_WA_MODE mode);
#define lme_dbg(fmt, args...)                                                                      \
	do {                                                                                       \
		if (lme_get_debug_level())                                                         \
			pr_info("[%s:%d] " fmt, __func__, __LINE__, ##args);                       \
	} while (0)

#define lme_info(fmt, args...) pr_info("[%s:%d] " fmt, __func__, __LINE__, ##args);

#define call_bufop(q, op, args...)                                                                 \
	({                                                                                         \
		int ret = 0;                                                                       \
		if (q && q->buf_ops && q->buf_ops->op)                                             \
			ret = q->buf_ops->op(args);                                                \
		ret;                                                                               \
	})

/* lme test */
#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
#define USE_VELOCE
#endif
/* #define USE_CLOCK_INFO */

#define LME_MODULE_NAME "camerapp-lme"
#ifdef USE_VELOCE
/* default in veloce : 2 sec * CNT(10) =  20 sec */
#define LME_TIMEOUT_MSEC (2000)
#define LME_TIMEOUT msecs_to_jiffies(LME_TIMEOUT_MSEC)
#define LME_WDT_CNT (10)
#else
/* default : 100ms * CNT(10) = 1000 msec = 1 sec */
#define LME_TIMEOUT_MSEC (100)
#define LME_TIMEOUT msecs_to_jiffies(LME_TIMEOUT_MSEC)
#define LME_WDT_CNT (10)
#endif

#define QUOTIENT_TO_100(num) ((num % 100 == 0) ? (num / 100) : ((num / 100) + 1))

#define LME_MAX_PLANES 3
#define LME_MAX_BUFS VB2_MAX_FRAME

enum lme_rdma_index {
	LME_RDMA_CACHE_IN_0, /* video (0: prev, 1:cur) still (0: cur, 1: ref)*/
	LME_RDMA_CACHE_IN_1,
	LME_RDMA_MBMV_IN,
	LME_RDMA_MAX
};

enum lme_wdma_index { LME_WDMA_MV_OUT, LME_WDMA_SAD_OUT, LME_WDMA_MBMV_OUT, LME_WDMA_MAX };

enum set_status { SET_SUCCESS, SET_ERROR };

#define LME_META_PLANE 1

/* LME hardware device state */
#define DEV_RUN 1
#define DEV_SUSPEND 2

#define LME_BUF_PREPARE 0
#define LME_BUF_FINISH 1

enum LME_BUF_TYPE {
	LME_BUF_CURR_IN,
	LME_BUF_PREV_IN,
	LME_BUF_MV_OUT,
	LME_BUF_SAD_OUT,
	LME_BUF_CR,
	LME_BUF_TOTAL_COUNT
};

#define LME_DMA_COUNT (LME_BUF_TOTAL_COUNT)

enum LME_IRQ_DONE_TYPE {
	LME_IRQ_FRAME_END,
	LME_IRQ_ERROR,
};

/* LME context state */
#define CTX_PARAMS 1
#define CTX_STREAMING 2
#define CTX_RUN 3
#define CTX_ABORT 4
#define CTX_SRC_FMT 5
#define CTX_DST_FMT 6
#define CTX_INT_FRAME 7 /* intermediate frame available */
#define CTX_DEV_READY 8

#define fh_to_lme_ctx(__fh) container_of(__fh, struct lme_ctx, fh)
#define lme_fmt_is_rgb888(x) ((x == V4L2_PIX_FMT_RGB32) || (x == V4L2_PIX_FMT_BGR32))
#define lme_fmt_is_yuv422(x)                                                                       \
	((x == V4L2_PIX_FMT_YUYV) || (x == V4L2_PIX_FMT_UYVY) || (x == V4L2_PIX_FMT_YVYU) ||       \
		(x == V4L2_PIX_FMT_YUV422P) || (x == V4L2_PIX_FMT_NV16) ||                         \
		(x == V4L2_PIX_FMT_NV61))
#define lme_fmt_is_yuv420(x)                                                                       \
	((x == V4L2_PIX_FMT_YUV420) || (x == V4L2_PIX_FMT_YVU420) || (x == V4L2_PIX_FMT_NV12) ||   \
		(x == V4L2_PIX_FMT_NV21) || (x == V4L2_PIX_FMT_NV12M) ||                           \
		(x == V4L2_PIX_FMT_NV21M) || (x == V4L2_PIX_FMT_YUV420M) ||                        \
		(x == V4L2_PIX_FMT_YVU420M) || (x == V4L2_PIX_FMT_NV12MT_16X16))
#define lme_fmt_is_ayv12(x) ((x) == V4L2_PIX_FMT_YVU420)

/* Get index of param_set  */
#define GET_LME_IDX(x) ((x) + 1)

enum lme_dbg_mode {
	LME_DBG_DUMP_REG = 0,
	LME_DBG_DUMP_REG_ONCE = 1,
	LME_DBG_DUMP_S2D = 2,
	LME_DBG_TIME = 3,
	LME_DBG_PMIO_MODE = 4,
	LME_DBG_DUMP_PMIO_CACHE = 5,
};
/*
 * struct lme_size_limit - lme variant size information
 *
 * @min_w: minimum pixel width size
 * @min_h: minimum pixel height size
 * @max_w: maximum pixel width size
 * @max_h: maximum pixel height size
 */
struct lme_size_limit {
	u16 min_w;
	u16 min_h;
	u16 max_w;
	u16 max_h;
};

struct lme_variant {
	struct lme_size_limit limit_input;
	struct lme_size_limit limit_output;
	u32 version;
};

/*
 * struct lme_fmt - the driver's internal color format data
 * @name: format description
 * @pixelformat: the fourcc code for this format, 0 if not applicable
 * @num_planes: number of physically non-contiguous data planes
 * @num_comp: number of color components(ex. RGB, Y, Cb, Cr)
 * @h_div: horizontal division value of C against Y for crop
 * @v_div: vertical division value of C against Y for crop
 * @bitperpixel: bits per pixel
 * @color: the corresponding lme_color_fmt
 */
struct lme_fmt {
	char *name;
	u32 pixelformat;
	u32 cfg_val;
	u8 bitperpixel[LME_MAX_PLANES];
	u8 num_planes : 2; /* num of buffer */
	u8 num_comp : 2; /* num of hw_plane */
	u8 h_shift : 1;
	u8 v_shift : 1;
};

struct lme_addr {
	dma_addr_t curr_in;
	dma_addr_t prev_in;
	dma_addr_t mv_out;
	dma_addr_t sad_out;

	dma_addr_t actual_cache_in_0;
	dma_addr_t actual_cache_in_1;
	dma_addr_t actual_mv_out;
	dma_addr_t actual_sad_out;

	unsigned int curr_in_size;
	unsigned int prev_in_size;
	unsigned int mv_out_size;
	unsigned int sad_out_size;
};

struct lme_mbmv {
	struct is_priv_buf *pb_mbmv0;
	struct is_priv_buf *pb_mbmv1;
	unsigned int mbmv0_size;
	unsigned int mbmv1_size;
	ulong kva_mbmv0;
	ulong kva_mbmv1;
	dma_addr_t dva_mbmv0;
	dma_addr_t dva_mbmv1;

	dma_addr_t actual_mbmv_in_0;
	dma_addr_t actual_mbmv_in_1;
	dma_addr_t actual_mbmv_out_0;
	dma_addr_t actual_mbmv_out_1;

	unsigned int width;
	unsigned int width_align;
	unsigned int height;
};

/*
 * struct lme_frame - source/target frame properties
 * @fmt:	buffer format(like virtual screen)
 * @crop:	image size / position
 * @addr:	buffer start address(access using LME_ADDR_XXX)
 * @bytesused:	image size in bytes (w x h x bpp)
 */
struct lme_frame {
	const struct lme_fmt *lme_fmt;
	unsigned short width;
	unsigned short height;
	unsigned short stride;
	__u32 pixelformat;
	struct lme_addr addr;
	__u32 bytesused[LME_MAX_PLANES];
	__u32 pixel_size;
	__u32 num_planes;
	__u32 flags;
	int fd[LME_MAX_BUFS][LME_MAX_PLANES];
};

/*
 * struct lme_m2m_device - v4l2 memory-to-memory device data
 * @v4l2_dev: v4l2 device
 * @vfd: the video device node
 * @m2m_dev: v4l2 memory-to-memory device data
 * @in_use: the open count
 */
struct lme_m2m_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
	struct v4l2_m2m_dev *m2m_dev;
	atomic_t in_use;
	atomic_t in_streamon;
	struct mutex lock;
};

struct lme_wdt {
	struct timer_list timer;
	atomic_t cnt;
};

enum lme_op_mode { LME_OP_MODE_FUSION, LME_OP_MODE_TNR };

enum lme_scenario {
	LME_SCENARIO_PROCESSING,
	LME_SCENARIO_REPROCESSING,
};

enum lme_sps_mode {
	LME_SPS_8X4,
	LME_SPS_2X2,
};

struct lme_pre_control_params {
	enum lme_op_mode op_mode;
	enum lme_scenario scenario;
	enum lme_sps_mode sps_mode;
	uint32_t time_out;
	uint32_t buffer_queue_size;
	uint32_t reserved[5];
};

struct lme_post_control_params {
	bool is_first;
	uint32_t curr_roi_width;
	uint32_t curr_roi_height;
	uint32_t curr_roi_stride;
	int32_t prev_buf_fd;
	uint32_t prev_buf_size;
	int32_t cr_buf_fd;
	uint32_t cr_buf_size;
	int32_t sad_buf_fd;
	uint32_t sad_buf_size;
	uint32_t reserved[2];
};

struct lme_time_dbg {
	ktime_t shot_time;
	u64 shot_time_stamp;
	ktime_t hw_time;
	u64 hw_time_stamp;
};

/*
 * lme_ctx - the abstration for lme open context
 * @node:		list to be added to lme_dev.context_list
 * @lme_dev:		the LME device this context applies to
 * @m2m_ctx:		memory-to-memory device context
 * @s_frame:		source frame properties
 * @d_frame:		destination frame properties
 * @fh:			v4l2 file handle
 * @flags:		context state flags
 * @model_param:		model information from user
 */
struct lme_ctx {
	struct list_head node;
	struct lme_dev *lme_dev;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct lme_frame s_frame;
	struct lme_frame d_frame;
	struct v4l2_fh fh;
	struct lme_time_dbg time_dbg;
	unsigned long flags;
	struct lme_pre_control_params pre_control_params;
	struct lme_post_control_params post_control_params;
	struct lme_addr addr;
	struct lme_mbmv mbmv;

	struct dma_buf *dmabuf[LME_DMA_COUNT];
	struct dma_buf_attachment *attachment[LME_DMA_COUNT];
	struct sg_table *sgt[LME_DMA_COUNT];
	ulong kvaddr[LME_DMA_COUNT];
};

struct lme_frame_count {
	atomic_t fs;
	atomic_t fe;
};

/*
 * struct lme_dev - the abstraction for lme device
 * @dev:	pointer to the lme device
 * @variant:	the IP variant information
 * @m2m:	memory-to-memory V4L2 device information
 * @aclk:	aclk required for lme operation
 * @regs_base:	the mapped hardware registers
 * @regs_rsc:	the resource claimed for IO registers
 * @wait:	interrupt handler waitqueue
 * @ws:		work struct
 * @state:	device state flags
 * @alloc_ctx:	videobuf2 memory allocator context
 * @slock:	the spinlock pscecting this data structure
 * @lock:	the mutex pscecting this data structure
 * @wdt:	watchdog timer information
 * @version:	IP version number
 */
struct lme_dev {
	struct device *dev;
	const struct lme_variant *variant;
	struct lme_m2m_device m2m;
	struct clk *aclk;
	struct clk *pclk;
	struct clk *clk_chld;
	struct clk *clk_parn;
	void __iomem *regs_base;
	struct resource *regs_rsc;
	wait_queue_head_t wait;
	unsigned long state;
	struct vb2_alloc_ctx *alloc_ctx;
	spinlock_t slock;
	struct mutex lock;
	struct lme_wdt wdt;
	struct lme_frame_count frame_cnt;
	spinlock_t ctxlist_lock;
	struct lme_ctx *current_ctx;
	struct list_head context_list;
	unsigned int irq;
	int dev_id;
	u32 version;

	bool use_hw_cache_operation;
	bool use_cloader_iommu_group;
	u32 cloader_iommu_group_id;

	struct is_mem mem;
	struct pmio_config pmio_config;
	struct pablo_mmio *pmio;
	struct pmio_field *pmio_fields;
	bool pmio_en;

	struct is_priv_buf *pb_c_loader_payload;
	ulong kva_c_loader_payload;
	dma_addr_t dva_c_loader_payload;
	struct is_priv_buf *pb_c_loader_header;
	ulong kva_c_loader_header;
	dma_addr_t dva_c_loader_header;

	struct pablo_lme_v4l2_ops *v4l2_ops;
	struct pablo_lme_sys_ops *sys_ops;
	struct pablo_camerapp_hw_lme *hw_lme_ops;
};

static inline struct lme_frame *ctx_get_frame(struct lme_ctx *ctx, enum v4l2_buf_type type)
{
	struct lme_frame *frame;

	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			frame = &ctx->s_frame;
		else
			frame = &ctx->d_frame;
	} else {
		dev_err(ctx->lme_dev->dev, "Wrong V4L2 buffer type %d\n", type);
		return ERR_PTR(-EINVAL);
	}

	return frame;
}

extern void is_debug_s2d(bool en_s2d, const char *fmt, ...);

#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
static inline dma_addr_t lme_get_dma_address(struct vb2_buffer *vb2_buf, u32 plane)
{
	struct sg_table *sgt;
	sgt = vb2_dma_sg_plane_desc(vb2_buf, plane);

	return (dma_addr_t)sg_dma_address(sgt->sgl);
}

static inline void *lme_get_kvaddr(struct vb2_buffer *vb2_buf, u32 plane)
{
	return vb2_plane_vaddr(vb2_buf, plane);
}
#else
static inline dma_addr_t lme_get_dma_address(struct vb2_buffer *vb2_buf, u32 plane)
{
	return vb2_dma_sg_plane_dma_addr(vb2_buf, plane);
}

static inline void *lme_get_kvaddr(struct vb2_buffer *vb2_buf, u32 plane)
{
	return vb2_plane_vaddr(vb2_buf, plane);
}
#endif
#else
static inline dma_addr_t lme_get_dma_address(void *cookie, dma_addr_t *addr)
{
	return NULL;
}

static inline void *lme_get_kernel_address(void *cookie)
{
	return NULL;
}
#endif

struct pablo_lme_v4l2_ops {
	int (*m2m_streamon)(
		struct file *file, struct v4l2_m2m_ctx *m2m_ctx, enum v4l2_buf_type type);
	int (*m2m_streamoff)(
		struct file *file, struct v4l2_m2m_ctx *m2m_ctx, enum v4l2_buf_type type);
};

struct pablo_lme_sys_ops {
	unsigned long (*copy_from_user)(void *dst, const void *src, unsigned long size);
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define KUNIT_EXPORT_SYMBOL(x) EXPORT_SYMBOL_GPL(x)
struct pkt_lme_ops {
	int (*get_log_level)(void);
	void (*set_log_level)(int level);
	int (*get_debug)(char *buffer, const struct kernel_param *kp);
	const struct lme_fmt *(*find_format)(u32 pixfmt);
	int (*v4l2_querycap)(struct file *file, void *fh, struct v4l2_capability *cap);
	int (*v4l2_g_fmt_mplane)(struct file *file, void *fh, struct v4l2_format *f);
	int (*v4l2_try_fmt_mplane)(struct file *file, void *fh, struct v4l2_format *f);
	int (*image_bound_check)(
		struct lme_ctx *ctx, enum v4l2_buf_type type, struct v4l2_pix_format_mplane *pixm);
	int (*v4l2_s_fmt_mplane)(struct file *file, void *fh, struct v4l2_format *f);
	int (*v4l2_reqbufs)(struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs);
	int (*v4l2_querybuf)(struct file *file, void *fh, struct v4l2_buffer *buf);
	int (*vb2_qbuf)(struct vb2_queue *q, struct v4l2_buffer *b);
	int (*check_qbuf)(struct file *file, struct v4l2_m2m_ctx *m2m_ctx, struct v4l2_buffer *buf);
	int (*v4l2_qbuf)(struct file *file, void *fh, struct v4l2_buffer *buf);
	int (*v4l2_dqbuf)(struct file *file, void *fh, struct v4l2_buffer *buf);
	int (*power_clk_enable)(struct lme_dev *lme);
	void (*power_clk_disable)(struct lme_dev *lme);
	int (*v4l2_streamon)(struct file *file, void *fh, enum v4l2_buf_type type);
	int (*v4l2_streamoff)(struct file *file, void *fh, enum v4l2_buf_type type);
	int (*v4l2_s_ctrl)(struct file *file, void *priv, struct v4l2_control *ctrl);
	int (*v4l2_s_ext_ctrls)(struct file *file, void *priv, struct v4l2_ext_controls *ctrls);
	struct v4l2_m2m_dev *(*v4l2_m2m_init)(void);
	void (*v4l2_m2m_release)(struct v4l2_m2m_dev *m2m_dev);
	int (*ctx_stop_req)(struct lme_ctx *ctx);
	int (*vb2_queue_setup)(struct vb2_queue *vq, unsigned int *num_buffers,
		unsigned int *num_planes, unsigned int sizes[], struct device *alloc_devs[]);
	int (*vb2_buf_prepare)(struct vb2_buffer *vb);
	void (*vb2_buf_finish)(struct vb2_buffer *vb);
	void (*vb2_buf_queue)(struct vb2_buffer *vb);
	void (*vb2_lock)(struct vb2_queue *vq);
	void (*vb2_unlock)(struct vb2_queue *vq);
	void (*cleanup_queue)(struct lme_ctx *ctx);
	int (*vb2_start_streaming)(struct vb2_queue *vq, unsigned int count);
	void (*vb2_stop_streaming)(struct vb2_queue *vq);
	int (*queue_init)(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq);
	int (*pmio_init)(struct lme_dev *lme);
	void (*pmio_exit)(struct lme_dev *lme);
	void (*pmio_config)(struct lme_dev *lme, struct c_loader_buffer *clb);
	int (*run_next_job)(struct lme_dev *lme);
	int (*add_context_and_run)(struct lme_dev *lme, struct lme_ctx *ctx);
	void (*m2m_device_run)(void *priv);
	void (*m2m_job_abort)(void *priv);
	int (*clk_get)(struct lme_dev *lme);
	void (*clk_put)(struct lme_dev *lme);
	int (*sysmmu_fault_handler)(struct iommu_fault *fault, void *data);
	void (*shutdown)(struct platform_device *pdev);
	int (*suspend)(struct device *dev);
	int (*runtime_resume)(struct device *dev);
	int (*runtime_suspend)(struct device *dev);
	int (*alloc_pmio_mem)(struct lme_dev *lme);
	void (*free_pmio_mem)(struct lme_dev *lme);
	void (*job_finish)(struct lme_dev *lme, struct lme_ctx *ctx);
	int (*register_m2m_device)(struct lme_dev *lme, int dev_id);
};
struct pkt_lme_ops *pablo_kunit_get_lme(void);
ulong pablo_get_dbg_lme(void);
void pablo_set_dbg_lme(ulong dbg);
#else
#define KUNIT_EXPORT_SYMBOL(x)
#endif

#endif /* CAMERAPP_LME__H_ */
