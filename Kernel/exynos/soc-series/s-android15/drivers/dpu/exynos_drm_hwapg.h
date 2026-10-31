// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_hwapg.h
 *
 * Copyright (c) 2025 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __EXYNOS_DRM_HWAPG__
#define __EXYNOS_DRM_HWAPG__

#include "exynos_drm_dsim.h"
#include <linux/types.h>
#include <linux/spinlock.h>

enum dpu_hwapg_dest {
	DPU_HWAPG_DEST_DPUF0 = 0,
	DPU_HWAPG_DEST_DPUF1,
	DPU_HWAPG_DEST_MAX
};
#define dpuf_to_hwapg_dest(id) ((enum dpu_hwapg_dest)id + DPU_HWAPG_DEST_DPUF0)

enum dpu_hwapg_src {
	DPU_HWAPG_SOURCE_DECON0 = 0,
	DPU_HWAPG_SOURCE_DECON1,
	DPU_HWAPG_SOURCE_DECON2,
	DPU_HWAPG_SOURCE_DECON3,
	DPU_HWAPG_SOURCE_WB0,
	DPU_HWAPG_SOURCE_WB1,
	DPU_HWAPG_SOURCE_DSIMFC0,
	DPU_HWAPG_SOURCE_DSIMFC1,
	DPU_HWAPG_SOURCE_VMC,
	DPU_HWAPG_SOURCE_SR0,
	DPU_HWAPG_SOURCE_SR1,
	DPU_HWAPG_SOURCE_TUI,
	DPU_HWAPG_SOURCE_SFRDMA0,
	DPU_HWAPG_SOURCE_SFRDMA1,
	DPU_HWAPG_SOURCE_CGCDMA0,
	DPU_HWAPG_SOURCE_CGCDMA1,
	DPU_HWAPG_SOURCE_MAX
};
#define decon_to_hwapg_src(id) ((enum dpu_hwapg_src)id + DPU_HWAPG_SOURCE_DECON0)
#define dsimfc_to_hwapg_src(id) ((enum dpu_hwapg_src)id + DPU_HWAPG_SOURCE_DSIMFC0)
#define vmc_to_hwapg_src(id) ((enum dpu_hwapg_src)id + DPU_HWAPG_SOURCE_VMC)
#define wb_to_hwapg_src(id)  ((enum dpu_hwapg_src)id + DPU_HWAPG_SOURCE_WB0)
#define sr_to_hwapg_src(id)  ((enum dpu_hwapg_src)id + DPU_HWAPG_SOURCE_SR0)
#define sfrdma_to_hwapg_src(id)  ((enum dpu_hwapg_src)id + DPU_HWAPG_SOURCE_SFRDMA0)
#define cgc_to_hwapg_src(id)  ((enum dpu_hwapg_src)id + DPU_HWAPG_SOURCE_CGCDMA0)

struct exynos_hwapg_ctrl {
	uint32_t src_mask;
	spinlock_t *slock;
};

struct exynos_hwapg_context {
	bool enable;
	enum dpu_hwapg_dest dest;
	struct exynos_hwapg_ctrl ctrl;
	const struct exynos_hwapg_ops *ops;
};
struct exynos_hwapg_ops {
	void (*block)(struct exynos_hwapg_context *ctx, enum dpu_hwapg_src src);
	void (*unblock)(struct exynos_hwapg_context *ctx,
			enum dpu_hwapg_src src);
};

struct exynos_hwapg_context *
exynos_drm_get_hwapg_context(struct decon_device *decon,
			     enum dpu_hwapg_dest dest, bool enable);
void exynos_hwapg_unblock_all(enum dpu_hwapg_src src);
void exynos_hwapg_block_all(enum dpu_hwapg_src src);
void exynos_hwapg_dump_status(void);
void exynos_check_hwapg_blocked(void);
#endif /* __EXYNOS_DRM_HWAPG__ */
