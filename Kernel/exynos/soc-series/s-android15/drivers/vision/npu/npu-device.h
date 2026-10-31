/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_DEVICE_H_
#define _NPU_DEVICE_H_

#include <linux/completion.h>

#include "npu-log.h"
#include "npu-debug.h"
#include "npu-vertex.h"
#include "npu-system.h"
#include "npu-syscall.h"
#include "npu-protodrv.h"
#include "npu-sessionmgr.h"

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
#include <soc/samsung/exynos/exynos-itmon.h>
#endif

#include "npu-scheduler.h"
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
#include "dsp-kernel.h"
#endif
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "npu-governor.h"
#endif
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>
#endif

#define NPU_DEVICE_NAME	"npu-turing"

#define NPU_MAX_GNET 2

enum npu_device_state {
	NPU_DEVICE_STATE_OPEN,
	NPU_DEVICE_STATE_START
};

enum npu_device_mode {
	NPU_DEVICE_MODE_NORMAL,
	NPU_DEVICE_MODE_TEST
};

enum npu_device_err_state {
	NPU_DEVICE_ERR_STATE_EMERGENCY
};

struct npu_hw_device;

struct npu_device {
	struct device *dev;
	unsigned long state;
	unsigned long err_state;
	struct npu_system system;
	struct npu_hw_device **hdevs;
	struct npu_vertex vertex;
	struct completion my_completion;
	struct npu_debug debug;
	// struct npu_proto_drv *proto_drv;
	struct npu_sessionmgr sessionmgr;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	struct dsp_kernel_manager kmgr;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
	struct notifier_block itmon_nb;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	struct s2mpu_notifier_block s2mpu_nb;
#endif
	struct npu_scheduler_info *sched;
	int magic;
	struct mutex start_stop_lock;
	u32 is_secure;
	u32 active_non_secure_sessions;

	struct workqueue_struct		*npu_log_wq;
	struct delayed_work		npu_log_work;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct cmdq_table_info cmdq_table_info;
#endif
	u32 is_first;
	struct npu_session *first_session;
#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION)
	atomic_t cam_on_count;
#endif
	atomic_t num_open_gnet;
	u64 gnet[NPU_MAX_GNET];
	atomic_t gnet_ref[NPU_MAX_GNET];
	struct mutex gnet_glock;
};

bool npu_device_check_and_lock(void);
void npu_device_unlock(bool skip);

int npu_device_open(struct npu_device *device);
int npu_device_close(struct npu_device *device);
int npu_device_start(struct npu_device *device);
int npu_device_stop(struct npu_device *device);
int npu_device_bootup(struct npu_device *device);
int npu_device_shutdown(struct npu_device *device);
int check_emergency(struct npu_device *dev);

//int npu_device_emergency_recover(struct npu_device *device);
void npu_device_set_emergency_err(struct npu_device *device);
int npu_device_is_emergency_err(struct npu_device *device);
int npu_device_recovery_close(struct npu_device *device);
#endif // _NPU_DEVICE_H_
