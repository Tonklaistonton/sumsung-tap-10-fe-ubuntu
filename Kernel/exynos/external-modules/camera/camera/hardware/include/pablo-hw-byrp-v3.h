/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef IS_HW_BYRP_V2_H
#define IS_HW_BYRP_V2_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "pablo-hw-api-common-ctrl.h"
#include "pablo-hw-api-byrp-v4_0.h"
#include "pablo-crta-interface.h"

enum is_hw_byrp_event_id {
	BYRP_EVENT_FRAME_START = 1,
	BYRP_EVENT_FRAME_END,
};

enum is_hw_byrp_dbg_mode {
	BYRP_DBG_DUMP_REG,
	BYRP_DBG_DTP,
	BYRP_DBG_DUMP_PMIO,
	BYRP_DBG_DUMP_RTA,
};

enum is_hw_byrp_event_type {
	BYRP_INIT,
	/* INT1 */
	BYRP_FS,
	BYRP_FR,
	BYRP_FE,
	BYRP_SETTING_DONE,
};

struct is_hw_byrp_iq {
	struct cr_set *regs;
	u32 size;
	spinlock_t slock;

	u32 fcount;
	unsigned long state;
};

struct is_hw_byrp {
	struct byrp_param_set param_set[IS_STREAM_COUNT];
	struct is_common_dma *rdma;
	struct is_common_dma *wdma;
	struct is_byrp_config config;
	struct pablo_common_ctrl *pcc;
	u32 irq_state[BYRP_INTR_MAX];
	u32 instance;
	u32 rdma_max_cnt;
	u32 wdma_max_cnt;
	u32 rdma_param_max_cnt;
	u32 wdma_param_max_cnt;
	unsigned long state;

	struct pablo_internal_subdev subdev_cloader;
	u32 header_size;

	struct pablo_common_ctrl_cfg pcc_cfg;

	atomic_t start_fcount;
	atomic_t isr_run_count;
	wait_queue_head_t isr_wait_queue;

	struct pablo_icpu_adt *icpu_adt;
	struct is_hw_byrp_iq iq_set;
	struct is_hw_byrp_iq cur_iq_set;

	struct is_sensor_interface *sensor_itf[IS_STREAM_COUNT];
	u32 post_frame_gap;
	unsigned long event_state;
};

void is_hw_byrp_dump(void);
int is_hw_byrp_test(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
void is_hw_byrp_s_debug_type(int type);
void is_hw_byrp_c_debug_type(int type);
#endif
