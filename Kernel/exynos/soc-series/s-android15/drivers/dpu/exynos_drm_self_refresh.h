/* SPDX-License-Identifier: GPL-2.0-only
 *
 * exynos_drm_self_refresh.h
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for Display Hibernation Feature.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_SELF_REFRESH__
#define __EXYNOS_DRM_SELF_REFRESH__

#include <exynos_drm_crtc.h>

enum trans_dimming_type {
	TRANS_DIMMING_TYPE_DQE     = BIT(0),
	TRANS_DIMMING_TYPE_MDNIE   = BIT(1),
	TRANS_DIMMING_TYPE_OUTDOOR = BIT(2),
};

int exynos_self_refresh_atomic_check(void *ctx, struct drm_atomic_state *state);
void exynos_self_refresh_atomic_queue_dvrr_config(void *ctx, struct drm_atomic_state *old_state);
void exynos_self_refresh_update_esync(void *ctx);
void *exynos_self_refresh_register(struct exynos_drm_crtc *exynos_crtc);
void exynos_self_refresh_atomic_clear_sync(void *ctx);
void exynos_self_refresh_set_dimming_cnt(void *ctx, u32 remain_bq_cnt, u32 full_bq_cnt);
void exynos_self_refresh_set_trans_dimming(void *ctx, enum trans_dimming_type type, bool en);

#endif /* __EXYNOS_DRM_SELF_REFRESH__ */
