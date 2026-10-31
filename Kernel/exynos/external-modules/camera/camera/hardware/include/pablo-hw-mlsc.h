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

#ifndef PABLO_HW_MLSC_H
#define PABLO_HW_MLSC_H

#include "pablo-hw-api-mlsc.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"
#include "pablo-crta-interface.h"

enum is_hw_mlsc_event_type {
	MLSC_INIT,
	/* INT1 */
	MLSC_FS,
	MLSC_FR,
	MLSC_FE,
	MLSC_SETTING_DONE,
};

enum pablo_hw_mlsc_nr_mode {
	MLSC_1NR,
	MLSC_2NR,
};

struct pablo_hw_mlsc_iq {
	struct cr_set *regs;
	u32 size;
	spinlock_t slock;

	u32 fcount;
	unsigned long state;
};

struct pablo_hw_mlsc {
	struct mlsc_param_set param_set[IS_STREAM_COUNT];
	struct is_mlsc_config config;
	struct is_common_dma dma[MLSC_DMA_NUM];
	enum mlsc_input_path input;
	struct mlsc_size_cfg size;
	struct mlsc_radial_cfg radial_cfg;
	bool hw_fro_en;

	struct pablo_internal_subdev subdev_cloader;
	u32 header_size;

	struct pablo_common_ctrl *pcc;

	/* for frame count management */
	atomic_t start_fcount;

	struct pablo_hw_mlsc_iq iq_set;
	struct pablo_hw_mlsc_iq cur_iq_set;
	unsigned long event_state;

	bool is_reprocessing;
};

#endif /* PABLO_HW_MLSC_H */
