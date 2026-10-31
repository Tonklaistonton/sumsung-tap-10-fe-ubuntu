/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_YUVSC_H
#define PABLO_HW_YUVSC_H

#include "is-param.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"
#include "pablo-crta-interface.h"

enum is_hw_yuvsc_event_type {
	YUVSC_INIT,
	/* INT1 */
	YUVSC_FS,
	YUVSC_FR,
	YUVSC_FE,
	YUVSC_SETTING_DONE,
};

struct pablo_hw_yuvsc_iq {
	struct cr_set *regs;
	u32 size;
	spinlock_t slock;

	u32 fcount;
	unsigned long state;
};

struct pablo_hw_yuvsc {
	struct is_yuvsc_config config[IS_STREAM_COUNT];
	struct yuvsc_param_set param_set[IS_STREAM_COUNT];

	struct pablo_internal_subdev subdev_cloader;
	u32 header_size;

	struct pablo_common_ctrl *pcc;

	const struct yuvsc_hw_ops *ops;

	struct pablo_hw_yuvsc_iq iq_set;
	struct pablo_hw_yuvsc_iq cur_iq_set;
	unsigned long event_state;
};

#endif /* PABLO_HW_YUVSC_H */
