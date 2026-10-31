/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung EXYNOS CAMERA PostProcessing DOF driver
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_DOF__H_
#define CAMERAPP_DOF__H_

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
#define dof_pm_qos_request exynos_pm_qos_request
#define dof_pm_qos_add_request exynos_pm_qos_add_request
#define dof_pm_qos_update_request exynos_pm_qos_update_request
#define dof_pm_qos_remove_request exynos_pm_qos_remove_request
#else
#define dof_pm_qos_request dev_pm_qos_request
#define dof_pm_qos_add_request(args...)
#define dof_pm_qos_update_request(args...)
#define dof_pm_qos_remove_request(args...)
#endif

int dof_get_debug_level(void);
uint dof_get_debug_rdmo(void);
uint dof_get_debug_wrmo(void);
#define dof_dbg(fmt, args...)                                                                      \
	do {                                                                                       \
		if (dof_get_debug_level())                                                         \
			pr_info("[%s:%d] " fmt, __func__, __LINE__, ##args);                       \
	} while (0)

#define dof_info(fmt, args...) pr_info("[%s:%d] " fmt, __func__, __LINE__, ##args)

#define call_bufop(q, op, args...)                                                                 \
	({                                                                                         \
		int ret = 0;                                                                       \
		if (q && q->buf_ops && q->buf_ops->op)                                             \
			ret = q->buf_ops->op(args);                                                \
		ret;                                                                               \
	})

/* dof test */
#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
#define USE_VELOCE
#endif

#define DOF_MODULE_NAME "camerapp-dof"

#ifdef USE_VELOCE
/* default in veloce : 2 sec * CNT(10) =  20 sec */
#define DOF_TIMEOUT_MSEC (2000)
#define DOF_TIMEOUT (msecs_to_jiffies(DOF_TIMEOUT_MSEC))
#define DOF_WDT_CNT (10)
#else
/* default : 500ms * CNT(6) = 3000 msec = 3 sec */
#define DOF_TIMEOUT_MSEC (500)
#define DOF_TIMEOUT (msecs_to_jiffies(DOF_TIMEOUT_MSEC))
#define DOF_WDT_CNT (6)
#endif

#define QUOTIENT_TO_500(num) ((num % 500 == 0) ? (num / 500) : ((num / 500) + 1))

#define DOF_MAX_PLANES 3
#define DOF_MAX_BUFS VB2_MAX_FRAME

enum set_status { SET_SUCCESS, SET_ERROR };

#define DOF_META_PLANE 1

/* DOF hardware device state */
#define DEV_RUN 1
#define DEV_SUSPEND 2

#define DOF_BUF_PREPARE 0
#define DOF_BUF_FINISH 1

enum DOF_DEBUG_MODE {
	DOF_DEBUG_OFF,
	DOF_DEBUG_LOG,
};

enum DOF_BUF_TYPE {
	DOF_BUF_CURR_IN,
	DOF_BUF_PREV_IN,
	DOF_BUF_INSTRUCTION,
	DOF_BUF_CONSTANT,
	DOF_BUF_TEMPORARY,
	DOF_BUF_PREV_STATE,
	DOF_BUF_OUTPUT,
	DOF_BUF_NEXT_STATE,
	DOF_BUF_TOTAL_COUNT,
};

#define DOF_DMA_COUNT (DOF_BUF_TOTAL_COUNT)

enum DOF_IRQ_DONE_TYPE {
	DOF_IRQ_FRAME_END,
	DOF_IRQ_ERROR,
};

/* DOF context state */
#define CTX_PARAMS 1
#define CTX_STREAMING 2
#define CTX_RUN 3
#define CTX_ABORT 4
#define CTX_SRC_FMT 5
#define CTX_DST_FMT 6
#define CTX_INT_FRAME 7 /* intermediate frame available */
#define CTX_DEV_READY 8

#define fh_to_dof_ctx(__fh) container_of(__fh, struct dof_ctx, fh)

/* Get index of param_set  */
#define GET_DOF_IDX(x) ((x) + 1)

enum dof_dbg_mode {
	DOF_DBG_DUMP_REG = 0,
	DOF_DBG_DUMP_REG_ONCE = 1,
	DOF_DBG_DUMP_S2D = 2,
	DOF_DBG_TIME = 3,
	DOF_DBG_PMIO_MODE = 4,
	DOF_DBG_DUMP_PMIO_CACHE = 5,
};

enum dof_clk_status {
	DOF_CLK_ON,
	DOF_CLK_OFF,
};

enum dof_clocks { DOF_GATE_CLK, DOF_CHLD_CLK, DOF_PARN_CLK };

struct dof_size_limit {
	u16 min_w;
	u16 min_h;
	u16 max_w;
	u16 max_h;
};

struct dof_variant {
	struct dof_size_limit limit_input;
	struct dof_size_limit limit_output;
	u32 version;
};

struct dof_fmt {
	char *name;
	u32 pixelformat;
	u32 cfg_val;
	u8 bitperpixel[DOF_MAX_PLANES];
	u8 num_planes : 2; /* num of buffer */
	u8 num_comp : 2; /* num of hw_plane */
	u8 h_shift : 1;
	u8 v_shift : 1;
};

struct dof_addr {
	dma_addr_t curr_in;
	dma_addr_t prev_in;
	dma_addr_t prev_state;
	dma_addr_t output;
	dma_addr_t next_state;

	unsigned int curr_in_size;
	unsigned int prev_in_size;
	unsigned int prev_state_size;
	unsigned int output_size;
	unsigned int next_state_size;
};

struct dof_model_addr {
	dma_addr_t dva_instruction;
	dma_addr_t dva_constant;
	dma_addr_t dva_temporary;
	dma_addr_t dva_instruction_with_offset;
	dma_addr_t dva_constant_with_offset;

	unsigned int instruction_size;
	unsigned int constant_size;
	unsigned int temporary_size;
};

struct dof_frame {
	const struct dof_fmt *dof_fmt;
	unsigned int width;
	unsigned int height;
	unsigned int stride;
	__u32 pixelformat;
	struct dof_addr addr;
	__u32 bytesused[DOF_MAX_PLANES];
	__u32 pixel_size;
	__u32 num_planes;
	__u32 flags;
	int fd[DOF_MAX_BUFS][DOF_MAX_PLANES];
};

struct dof_m2m_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
	struct v4l2_m2m_dev *m2m_dev;
	atomic_t in_use;
	atomic_t in_streamon;
	struct mutex lock;
};

struct dof_wdt {
	struct timer_list timer;
	atomic_t cnt;
};

typedef uint32_t DmaAddress;

struct DmaBuffer {
	DmaAddress address;
	uint8_t *ptr;
	uint32_t size;
};

enum dof_performance_mode { DOF_PERF_MODE_DISABLE, DOF_PERF_MODE_ENABLE };

enum dof_op_mode { DOF_OP_MODE_SINGLE_FE, DOF_OP_MODE_DUAL_FE, DOF_OP_MODE_FE_ONLY };

#define GET_OP_MODE_STRING(x)                                                                      \
	((x == DOF_OP_MODE_SINGLE_FE) ? "SingleFE" :                                               \
					(x == DOF_OP_MODE_DUAL_FE) ?                               \
					"DualFE" :                                                 \
					(x == DOF_OP_MODE_FE_ONLY) ? "FeOnly" : "NONE")

#define DOF_DEBUG_REGISTER_NUM (50)

enum dof_debug_status { DEVICE_STATUS_NORMAL, DEVICE_STATUS_TIMEOUT };

struct reg_set {
	uint32_t addr;
	uint32_t value;
};

struct reg_req {
	uint32_t num_reg;
	struct reg_set reg_data[DOF_DEBUG_REGISTER_NUM];
};

struct dof_debug_info {
	uint32_t buffer_index;
	uint32_t device_status;
	struct reg_req regs;
};

/*
in ExynosCV\include\CvEngineInternalTypesDof.hpp
 - DofCnnParams
*/
struct dof_model_param {
	struct DmaBuffer instruction;
	struct DmaBuffer constant;
	struct DmaBuffer temporary;

	uint32_t time_out;
	uint32_t clock_level;
	uint32_t clock_mode;
	uint32_t enable_reg_dump;
	struct dof_debug_info debug_info;
	uint32_t enable_sw_workaround;
	uint32_t model_id;
	uint32_t reserved[4];
};

/*
in ExynosCV\include\CvEngineInternalTypesDof.hpp
 - DofPerFrameControlParams
*/
struct dof_perframe_control_params {
	uint32_t const_offset;
	uint32_t inst_offset;

	int32_t prev_fd;
	uint32_t prev_size;

	int32_t pstate_fd;
	uint32_t pstate_size;

	int32_t nstate_fd;
	uint32_t nstate_size;

	enum dof_performance_mode performance_mode;
	enum dof_op_mode operation_mode;

	uint32_t model_id;
	uint32_t roi_width;
	uint32_t roi_height;
	uint32_t frame_count;

	uint32_t const_size;
	uint32_t const_checksum;

	uint32_t inst_size;
	uint32_t inst_checksum;

	uint32_t reserved[6];
};

struct dof_time_dbg {
	ktime_t shot_time;
	u64 shot_time_stamp;
	ktime_t hw_time;
	u64 hw_time_stamp;
};

struct dof_roi {
	uint32_t roi_width;
	uint32_t roi_height;
};

struct dof_ctx {
	struct list_head node;
	struct dof_dev *dof_dev;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct dof_frame s_frame;
	struct dof_frame d_frame;
	struct dof_roi roi;
	struct v4l2_fh fh;
	struct dof_time_dbg time_dbg;
	unsigned long flags;
	struct dof_model_param model_param;
	struct dof_perframe_control_params perframe_control_params;
	struct dof_model_addr model_addr;

	struct dma_buf *dmabuf[DOF_DMA_COUNT];
	struct dma_buf_attachment *attachment[DOF_DMA_COUNT];
	struct sg_table *sgt[DOF_DMA_COUNT];
	ulong kvaddr[DOF_DMA_COUNT];
};

struct dof_frame_count {
	atomic_t fs;
	atomic_t fe;
};

struct dof_dev {
	struct device *dev;
	const struct dof_variant *variant;
	struct dof_m2m_device m2m;
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
	struct dof_wdt wdt;
	struct dof_frame_count frame_cnt;
	spinlock_t ctxlist_lock;
	struct dof_ctx *current_ctx;
	struct list_head context_list;
	unsigned int irq0, irq1;
	int dev_id;
	u32 version;

	bool use_hw_cache_operation;
	bool use_cloader_iommu_group;
	u32 cloader_iommu_group_id;

	struct is_mem mem;
	struct pmio_config pmio_config;
	struct pablo_mmio *pmio;
	struct pmio_field *pmio_fields;

	struct is_priv_buf *pb_c_loader_payload;
	ulong kva_c_loader_payload;
	dma_addr_t dva_c_loader_payload;
	struct is_priv_buf *pb_c_loader_header;
	ulong kva_c_loader_header;
	dma_addr_t dva_c_loader_header;

	struct pablo_dof_v4l2_ops *v4l2_ops;
	struct pablo_dof_sys_ops *sys_ops;
	struct pablo_camerapp_hw_dof *hw_dof_ops;
	struct pablo_dof_dma_buf_ops *dma_buf_ops;

	uint32_t enable_sw_workaround;
	uint32_t default_rdmo;
	uint32_t default_wrmo;
};

static inline struct dof_frame *ctx_get_frame(struct dof_ctx *ctx, enum v4l2_buf_type type)
{
	struct dof_frame *frame;

	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			frame = &ctx->s_frame;
		else
			frame = &ctx->d_frame;
	} else {
		dev_err(ctx->dof_dev->dev, "Wrong V4L2 buffer type %d\n", type);
		return ERR_PTR(-EINVAL);
	}

	return frame;
}

extern void is_debug_s2d(bool en_s2d, const char *fmt, ...);

static inline void *dof_get_kvaddr(struct vb2_buffer *vb2_buf, u32 plane)
{
	return vb2_plane_vaddr(vb2_buf, plane);
}

struct pablo_dof_v4l2_ops {
	int (*m2m_streamon)(
		struct file *file, struct v4l2_m2m_ctx *m2m_ctx, enum v4l2_buf_type type);
	int (*m2m_streamoff)(
		struct file *file, struct v4l2_m2m_ctx *m2m_ctx, enum v4l2_buf_type type);
};

struct pablo_dof_sys_ops {
	unsigned long (*copy_from_user)(void *dst, const void *src, unsigned long size);
};

struct pablo_dof_dma_buf_ops {
	struct dma_buf_attachment *(*attach)(struct dma_buf *dma_buf, struct device *dev);
	void (*detach)(struct dma_buf *dma_buf, struct dma_buf_attachment *db_attach);
	struct sg_table *(*map_attachment)(
		struct dma_buf_attachment *db_attach, enum dma_data_direction dir);
	void (*unmap_attachment)(struct dma_buf_attachment *db_attach, struct sg_table *sgt,
		enum dma_data_direction dir);
	void *(*vmap)(struct dma_buf *dma_buf);
	void (*vunmap)(struct dma_buf *dbuf, void *kva);
	dma_addr_t (*get_dva)(struct dof_ctx *ctx, int type);
	dma_addr_t (*get_dma_address)(struct vb2_buffer *vb2_buf, u32 plane);
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define KUNIT_EXPORT_SYMBOL(x) EXPORT_SYMBOL_GPL(x)
struct pkt_dof_ops {
	int (*get_log_level)(void);
	void (*set_log_level)(int level);
	uint (*get_rdmo)(void);
	void (*set_rdmo)(uint level);
	uint (*get_wrmo)(void);
	void (*set_wrmo)(uint level);
	int (*get_debug)(char *buffer, const struct kernel_param *kp);
	const struct dof_fmt *(*find_format)(u32 pixfmt);
	int (*v4l2_querycap)(struct file *file, void *fh, struct v4l2_capability *cap);
	int (*v4l2_g_fmt_mplane)(struct file *file, void *fh, struct v4l2_format *f);
	int (*v4l2_try_fmt_mplane)(struct file *file, void *fh, struct v4l2_format *f);
	int (*image_bound_check)(
		struct dof_ctx *ctx, enum v4l2_buf_type type, struct v4l2_pix_format_mplane *pixm);
	int (*v4l2_s_fmt_mplane)(struct file *file, void *fh, struct v4l2_format *f);
	int (*v4l2_reqbufs)(struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs);
	int (*v4l2_querybuf)(struct file *file, void *fh, struct v4l2_buffer *buf);
	int (*vb2_qbuf)(struct vb2_queue *q, struct v4l2_buffer *b);
	int (*check_qbuf)(struct file *file, struct v4l2_m2m_ctx *m2m_ctx, struct v4l2_buffer *buf);
	int (*v4l2_qbuf)(struct file *file, void *fh, struct v4l2_buffer *buf);
	int (*v4l2_dqbuf)(struct file *file, void *fh, struct v4l2_buffer *buf);
	int (*power_clk_enable)(struct dof_dev *dof);
	void (*power_clk_disable)(struct dof_dev *dof);
	int (*v4l2_streamon)(struct file *file, void *fh, enum v4l2_buf_type type);
	int (*v4l2_streamoff)(struct file *file, void *fh, enum v4l2_buf_type type);
	int (*v4l2_s_ctrl)(struct file *file, void *priv, struct v4l2_control *ctrl);
	int (*v4l2_s_ext_ctrls)(struct file *file, void *priv, struct v4l2_ext_controls *ctrls);
	struct v4l2_m2m_dev *(*v4l2_m2m_init)(void);
	void (*v4l2_m2m_release)(struct v4l2_m2m_dev *m2m_dev);
	int (*ctx_stop_req)(struct dof_ctx *ctx);
	int (*vb2_queue_setup)(struct vb2_queue *vq, unsigned int *num_buffers,
		unsigned int *num_planes, unsigned int sizes[], struct device *alloc_devs[]);
	int (*vb2_buf_prepare)(struct vb2_buffer *vb);
	void (*vb2_buf_finish)(struct vb2_buffer *vb);
	void (*vb2_buf_queue)(struct vb2_buffer *vb);
	void (*vb2_lock)(struct vb2_queue *vq);
	void (*vb2_unlock)(struct vb2_queue *vq);
	void (*cleanup_queue)(struct dof_ctx *ctx);
	int (*vb2_start_streaming)(struct vb2_queue *vq, unsigned int count);
	void (*vb2_stop_streaming)(struct vb2_queue *vq);
	int (*queue_init)(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq);
	int (*pmio_init)(struct dof_dev *dof);
	void (*pmio_exit)(struct dof_dev *dof);
	void (*pmio_config)(struct dof_dev *dof, struct c_loader_buffer *clb);
	int (*run_next_job)(struct dof_dev *dof);
	int (*add_context_and_run)(struct dof_dev *dof, struct dof_ctx *ctx);
	void (*m2m_device_run)(void *priv);
	void (*m2m_job_abort)(void *priv);
	int (*clk_get)(struct dof_dev *dof);
	void (*clk_put)(struct dof_dev *dof);
	int (*sysmmu_fault_handler)(struct iommu_fault *fault, void *data);
	void (*shutdown)(struct platform_device *pdev);
	int (*suspend)(struct device *dev);
	int (*runtime_resume)(struct device *dev);
	int (*runtime_suspend)(struct device *dev);
	int (*alloc_pmio_mem)(struct dof_dev *dof);
	void (*free_pmio_mem)(struct dof_dev *dof);
	void (*job_finish)(struct dof_dev *dof, struct dof_ctx *ctx);
	int (*register_m2m_device)(struct dof_dev *dof, int dev_id);
};
struct pkt_dof_ops *pablo_kunit_get_dof(void);
ulong pablo_get_dbg_dof(void);
void pablo_set_dbg_dof(ulong dbg);
#else
#define KUNIT_EXPORT_SYMBOL(x)
#endif

#endif /* CAMERAPP_DOF__H_ */
