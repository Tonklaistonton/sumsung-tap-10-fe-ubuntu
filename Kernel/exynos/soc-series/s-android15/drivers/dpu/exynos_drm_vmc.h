// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_vmc.h
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __EXYNOS_DRM_VMC_H__
#define __EXYNOS_DRM_VMC_H__

struct decon_device;
struct drm_display_mode;
struct exynos_display_mode;
#if IS_ENABLED(CONFIG_EXYNOS_VMC)
bool is_vmc_still_blocked(void *ctx);
void vmc_still_block(void *ctx, bool cancel_work);
void vmc_still_unblock(void *ctx);
void vmc_still_on(void *ctx);
void vmc_still_off(void *ctx, bool sync_cmd);
int vmc_atomic_check(void *ctx, struct drm_atomic_state *state);
void vmc_dump(void *ctx);
void vmc_atomic_enable(void *ctx);
void vmc_atomic_disable(void *ctx);
void vmc_atomic_exit_hiber(void *ctx);
void vmc_atomic_enter_hiber(void *ctx);
void vmc_atomic_set_config(void *ctx,
				const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode);
void vmc_atomic_lock(void *ctx, bool lock);
void vmc_atomic_update(void *ctx, struct drm_atomic_state *old_state);
void vmc_atomic_switching_prepare(void *ctx, struct drm_atomic_state *old_state);
void vmc_atomic_switching(void *ctx, bool en);
void *vmc_register(struct decon_device *decon);
void vmc_set_ignore_rw(struct drm_crtc *crtc);
#else
static inline bool is_vmc_still_blocked(void *ctx) { return false; }
static inline void vmc_still_block(void *ctx, bool cancel_work) {}
static inline void vmc_still_unblock(void *ctx) {}
static inline void vmc_still_on(void *ctx) {}
static inline void vmc_still_off(void *ctx, bool sync_cmd) {};
static inline int vmc_atomic_check(void *ctx, struct drm_atomic_state *state) { return 0; }
static inline void vmc_dump(void *ctx) {}
static inline void vmc_atomic_enable(void *ctx) {}
static inline void vmc_atomic_disable(void *ctx) {}
static inline void vmc_atomic_exit_hiber(void *ctx) {}
static inline void vmc_atomic_enter_hiber(void *ctx) {}
static inline void vmc_atomic_set_config(void *ctx,
				const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode) {}
static inline void vmc_atomic_lock(void *ctx, bool lock) {}
static inline void vmc_atomic_update(void *ctx, struct drm_atomic_state *old_state) {}
static inline void *vmc_register(struct decon_device *decon) { return NULL; }
static inline void vmc_atomic_switching_prepare(void *ctx, struct drm_atomic_state *old_state) {}
static inline void vmc_atomic_switching(void *ctx, bool en) {}
static inline void vmc_set_ignore_rw(struct drm_crtc *crtc) {}
#endif

#endif /* __EXYNOS_DRM_VMC_H__ */

