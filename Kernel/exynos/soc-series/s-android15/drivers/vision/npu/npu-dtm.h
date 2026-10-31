/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_DTM_H_
#define _NPU_DTM_H_

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/printk.h>
#include <linux/cpuidle.h>
#include <soc/samsung/bts.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos-devfreq.h>
#endif

#include "include/npu-config.h"
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-util-regs.h"
#include "npu-hw-device.h"

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
void npu_dtm_set(struct npu_scheduler_info *info);
bool npu_dtm_get_flag(void);
#endif

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
#define EMODE_START	(90000)
#define EMODE_RELEASE	(70000)

void npu_dtm_trigger_emode(struct npu_scheduler_info *info);
int npu_dtm_emode_probe(struct npu_device *device);
#endif

#if IS_ENABLED(CONFIG_NPU_USE_ESCA_DTM)
/* NPU-ESCA IPC Protocol */
enum npu_esca_event {
	UNKNOWN = 0xF,
	NPU_ESCA_START,
	NPU_ESCA_BOOST,
	NPU_ESCA_END,
};

struct npu_esca_mbox {
	u32 request;
	u32 receive;
	u64 reserved;
};

union npu_esca_mbox_align {
	u32 data[4];
	struct npu_esca_mbox mbox;
};

struct esca_ipc_channel {
       u32 ipc_ch;
       u32 size;
};

int npu_dtm_open(struct npu_device *device);
int npu_dtm_close(struct npu_device *device);
int npu_dtm_ipc_communicate(enum npu_esca_event data);
#endif
#endif /* _NPU_DTM_H_ */
