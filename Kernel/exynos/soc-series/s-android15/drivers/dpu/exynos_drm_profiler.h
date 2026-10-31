/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for DPU PROFILER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_PROFILER_H__
#define __EXYNOS_DRM_PROFILER_H__

#include <linux/ktime.h>
#include <linux/types.h>
#include <soc/samsung/profiler/exynos-profiler-extif.h>

#include <drm/drm_managed.h>
#include <exynos_drm_crtc.h>
struct exynos_drm_crtc;
void get_frame_vsync_cnt(u64 *cnt, ktime_t *time);
void get_ems_frame_cnt(u64 *cnt, ktime_t *time);
void get_ems_fence_cnt(u64 *cnt, ktime_t *time);
#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GPU)
void exynos_drm_profiler_update_vsync_cnt(struct exynos_drm_crtc *exynos_crtc);
void exynos_drm_profiler_update_ems_frame_cnt(struct exynos_drm_crtc *exynos_crtc);
void exynos_drm_profiler_update_ems_fence_cnt(struct exynos_drm_crtc *exynos_crtc);
void *exynos_drm_profiler_register(struct exynos_drm_crtc *exynos_crtc);
#else
static inline void exynos_drm_profiler_update_vsync_cnt(struct exynos_drm_crtc *exynos_crtc) {}
static inline void exynos_drm_profiler_update_ems_frame_cnt(struct exynos_drm_crtc *exynos_crtc) {}
static inline void exynos_drm_profiler_update_ems_fence_cnt(struct exynos_drm_crtc *exynos_crtc) {}
static inline void *exynos_drm_profiler_register(struct exynos_drm_crtc *exynos_crtc) { return NULL; }
#endif

#endif /* __EXYNOS_DRM_PROFILER_H__ */
