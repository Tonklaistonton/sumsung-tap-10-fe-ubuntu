// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DPU profiler file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <exynos_drm_profiler.h>

#define PROFILER_PERIOD_NSEC	100000000UL
#define FPS_UNIT_NSEC		1000000000UL	/* 1sec */

struct exynos_drm_profiler {
	u64 frame_vsync_cnt;
	ktime_t frame_vsync_cnt_time;
	u64 ems_frame_cnt;
	ktime_t ems_frame_cnt_time;
	u64 ems_fence_cnt;
	ktime_t ems_fence_cnt_time;
};

/* only valid for LCD display crtc */
static struct exynos_drm_crtc *primary_exynos_crtc;

static struct exynos_profiler_gpudev_fn *profiler_gpudev_fn;
static struct exynos_profiler_drmdev_fn profiler_drmdev_fn = {
	.get_vsync_cnt = &get_frame_vsync_cnt,
	.get_frame_cnt = &get_ems_frame_cnt,
	.get_fence_cnt = &get_ems_fence_cnt,
};

static inline bool is_profiler_registed(struct exynos_drm_crtc *exynos_crtc)
{
	if (!exynos_crtc || !exynos_crtc->profiler)
		return false;

	return true;
}

void get_frame_vsync_cnt(u64 *cnt, ktime_t *time)
{
	const struct exynos_drm_profiler *profiler;
	struct drm_crtc *crtc;
	u64 lcd_fps;
	u64 vsync_period_nsec;
	static u64 vsync_cnt;
	static ktime_t prev_ktime;
	ktime_t vsync_time;
	ktime_t cur_ktime = ktime_get();
	ktime_t ktime_diff;

	if (unlikely(!is_profiler_registed(primary_exynos_crtc))) {
		pr_info("%s: dpu profiler is unregisted.\n", __func__);
		return;
	}

	crtc = &primary_exynos_crtc->base;
	drm_modeset_lock(&crtc->mutex, NULL);
	if (unlikely(!crtc->state)) {
		pr_info("%s: crtc state is not yet reset.\n", __func__);
		goto out;
	}

	lcd_fps = drm_mode_vrefresh(&crtc->state->mode);
	if (!lcd_fps) {
		pr_info("%s: there is no vrefresh\n", __func__);
		goto out;
	}
	vsync_period_nsec = FPS_UNIT_NSEC / lcd_fps;

	profiler = primary_exynos_crtc->profiler;
	if (vsync_cnt) {
		if (prev_ktime) {
			ktime_diff = cur_ktime - prev_ktime;
			vsync_cnt += DIV_ROUND_CLOSEST(lcd_fps * ktime_diff, FPS_UNIT_NSEC);
		} else
			vsync_cnt += lcd_fps * PROFILER_PERIOD_NSEC / FPS_UNIT_NSEC;
	} else
		vsync_cnt = profiler->frame_vsync_cnt;

	if ((cur_ktime - profiler->frame_vsync_cnt_time) > vsync_period_nsec)
		vsync_time = profiler->frame_vsync_cnt_time + vsync_period_nsec;
	else
		vsync_time = profiler->frame_vsync_cnt_time;

	*cnt = vsync_cnt;
	*time = vsync_time;
	prev_ktime = cur_ktime;
out:
	drm_modeset_unlock(&crtc->mutex);
}

void get_ems_frame_cnt(u64 *cnt, ktime_t *time)
{
	const struct exynos_drm_profiler *profiler;

	if (unlikely(!is_profiler_registed(primary_exynos_crtc))) {
		pr_info("%s: dpu profiler is unregisted.\n", __func__);
		return;
	}

	profiler = primary_exynos_crtc->profiler;
	*cnt = profiler->ems_frame_cnt;
	*time = profiler->ems_frame_cnt_time;
}

void get_ems_fence_cnt(u64 *cnt, ktime_t *time)
{
	const struct exynos_drm_profiler *profiler;

	if (unlikely(!is_profiler_registed(primary_exynos_crtc))) {
		pr_info("%s: dpu profiler is unregisted.\n", __func__);
		return;
	}

	profiler = primary_exynos_crtc->profiler;
	*cnt = profiler->ems_fence_cnt;
	*time = profiler->ems_fence_cnt_time;
}

void exynos_drm_profiler_update_vsync_cnt(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_drm_profiler *profiler;

	if (unlikely(!is_profiler_registed(exynos_crtc)))
		return;

	profiler = exynos_crtc->profiler;
	profiler->frame_vsync_cnt++;
	profiler->frame_vsync_cnt_time = ktime_get();
	if(profiler_gpudev_fn) {
		profiler_gpudev_fn->set_vsync(ktime_get_real() / 1000UL);
	}
}

void exynos_drm_profiler_update_ems_frame_cnt(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_drm_profiler *profiler;

	if (unlikely(!is_profiler_registed(exynos_crtc)))
		return;

	profiler = exynos_crtc->profiler;
	profiler->ems_frame_cnt++;
	profiler->ems_frame_cnt_time = ktime_get();
}

void exynos_drm_profiler_update_ems_fence_cnt(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_drm_profiler *profiler;

	if (unlikely(!is_profiler_registed(exynos_crtc)))
		return;

	profiler = exynos_crtc->profiler;
	profiler->ems_fence_cnt++;
	profiler->ems_fence_cnt_time = ktime_get();
}

void *exynos_drm_profiler_register(struct exynos_drm_crtc *exynos_crtc)
{
	struct exynos_drm_profiler *profiler;
	struct drm_crtc *crtc = &exynos_crtc->base;

	/* profiler support only lcd crtc(index == 0) */
	if (!IS_ENABLED(CONFIG_EXYNOS_PROFILER_GPU) || crtc->index)
		return NULL;

	profiler = drmm_kzalloc(crtc->dev, sizeof(struct exynos_drm_profiler), GFP_KERNEL);

	primary_exynos_crtc = exynos_crtc;

	profiler_gpudev_fn = sgpu_profiler_get_gpudev_fn();
	exynos_profiler_register_drmdev_fn(&profiler_drmdev_fn);

	pr_info("%s[%d]: profiler supported\n", crtc->name, crtc->index);

	return profiler;
}
