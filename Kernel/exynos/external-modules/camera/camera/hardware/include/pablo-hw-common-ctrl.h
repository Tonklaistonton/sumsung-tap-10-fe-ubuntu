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

#ifndef PABLO_HW_COMMON_CTRL_H
#define PABLO_HW_COMMON_CTRL_H

#include "pablo-mmio.h"

struct pablo_common_ctrl_hw {
	struct device *dev;
	u32 pcc_num;
	struct pablo_common_ctrl *pccs;
};

struct pablo_common_ctrl *pablo_common_ctrl_hw_get_pcc(struct pmio_config *cfg);
struct platform_driver *pablo_common_ctrl_hw_get_driver(void);

#endif /* PABLO_HW_COMMON_CTRL_H */
