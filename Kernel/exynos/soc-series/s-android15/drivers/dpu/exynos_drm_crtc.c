// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_crtc.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <dpu_trace.h>
#include <uapi/linux/sched/types.h>
#include <uapi/drm/drm.h>
#include <linux/circ_buf.h>
#include <linux/dma-fence.h>
#include <linux/delay.h>

#include <drm/drm_crtc_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_color_mgmt.h>
#include <drm/drm_vblank.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_print.h>
#include <drm/drm_managed.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_mipi_dsi.h>
#include <samsung_drm.h>

#include <exynos_display_common.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_drv.h>
#include <exynos_drm_plane.h>
#include <exynos_drm_profiler.h>
#include <exynos_drm_format.h>
#include <exynos_drm_hibernation.h>
#include <exynos_drm_partial.h>
#include <exynos_drm_dqe.h>
#include <exynos_drm_vmc.h>
#include <exynos_drm_self_refresh.h>

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
#include <mcd_drm_helper.h>
#endif

static const struct drm_prop_enum_list color_mode_list[] = {
	{ HAL_COLOR_MODE_NATIVE, "Native" },
	{ HAL_COLOR_MODE_DCI_P3, "DCI-P3" },
	{ HAL_COLOR_MODE_SRGB, "SRGB" },
	{ HAL_COLOR_MODE_STANDARD_BT601_625, "BT601-625" },
	{ HAL_COLOR_MODE_STANDARD_BT601_625_UNADJUSTED, "BT601-625-UNADJUSTED" },
	{ HAL_COLOR_MODE_STANDARD_BT601_525, "BT601-525" },
	{ HAL_COLOR_MODE_STANDARD_BT601_525_UNADJUSTED, "BT601-525-UNADJUSTED" },
	{ HAL_COLOR_MODE_STANDARD_BT709, "BT709" },
	{ HAL_COLOR_MODE_ADOBE_RGB, "ADOBE-RGB" },
	{ HAL_COLOR_MODE_DISPLAY_P3, "DISPLAY-P3" },
	{ HAL_COLOR_MODE_BT2020, "BT2020" },
	{ HAL_COLOR_MODE_BT2100_PQ, "BT2100-PQ" },
	{ HAL_COLOR_MODE_BT2100_HLG, "BT2100-HLG" },
	{ HAL_COLOR_MODE_CUSTOM_0, "CUSTOM-0" },
	{ HAL_COLOR_MODE_CUSTOM_1, "CUSTOM-1" },
	{ HAL_COLOR_MODE_CUSTOM_2, "CUSTOM-2" },
	{ HAL_COLOR_MODE_CUSTOM_3, "CUSTOM-3" },
	{ HAL_COLOR_MODE_CUSTOM_4, "CUSTOM-4" },
	{ HAL_COLOR_MODE_CUSTOM_5, "CUSTOM-5" },
	{ HAL_COLOR_MODE_CUSTOM_6, "CUSTOM-6" },
	{ HAL_COLOR_MODE_CUSTOM_7, "CUSTOM-7" },
	{ HAL_COLOR_MODE_CUSTOM_8, "CUSTOM-8" },
	{ HAL_COLOR_MODE_CUSTOM_9, "CUSTOM-9" },
	{ HAL_COLOR_MODE_CUSTOM_10, "CUSTOM-10" },
	{ HAL_COLOR_MODE_CUSTOM_11, "CUSTOM-11" },
	{ HAL_COLOR_MODE_CUSTOM_12, "CUSTOM-12" },
	{ HAL_COLOR_MODE_CUSTOM_13, "CUSTOM-13" },
	{ HAL_COLOR_MODE_CUSTOM_14, "CUSTOM-14" },
	{ HAL_COLOR_MODE_CUSTOM_15, "CUSTOM-15" },
	{ HAL_COLOR_MODE_CUSTOM_16, "CUSTOM-16" },
	{ HAL_COLOR_MODE_CUSTOM_17, "CUSTOM-17" },
	{ HAL_COLOR_MODE_CUSTOM_18, "CUSTOM-18" },
	{ HAL_COLOR_MODE_CUSTOM_19, "CUSTOM-19" },
	{ HAL_COLOR_MODE_CUSTOM_20, "CUSTOM-20" },
	{ HAL_COLOR_MODE_CUSTOM_21, "CUSTOM-21" },
	{ HAL_COLOR_MODE_CUSTOM_22, "CUSTOM-22" },
	{ HAL_COLOR_MODE_CUSTOM_23, "CUSTOM-23" },
	{ HAL_COLOR_MODE_CUSTOM_24, "CUSTOM-24" },
	{ HAL_COLOR_MODE_CUSTOM_25, "CUSTOM-25" },
	{ HAL_COLOR_MODE_CUSTOM_26, "CUSTOM-26" },
	{ HAL_COLOR_MODE_CUSTOM_27, "CUSTOM-27" },
	{ HAL_COLOR_MODE_CUSTOM_28, "CUSTOM-28" },
	{ HAL_COLOR_MODE_CUSTOM_29, "CUSTOM-29" },
};

static const struct drm_prop_enum_list render_intent_list[] = {
	{ HAL_RENDER_INTENT_COLORIMETRIC, "Colorimetric" },
	{ HAL_RENDER_INTENT_ENHANCE, "Enhance" },
	{ HAL_RENDER_INTENT_TONE_MAP_COLORIMETRIC, "Tone Map Colorimetric" },
	{ HAL_RENDER_INTENT_TONE_MAP_ENHANCE, "Tone Map Enhance" },
	{ HAL_RENDER_INTENT_CUSTOM_0, "CUSTOM-0" },
	{ HAL_RENDER_INTENT_CUSTOM_1, "CUSTOM-1" },
	{ HAL_RENDER_INTENT_CUSTOM_2, "CUSTOM-2" },
	{ HAL_RENDER_INTENT_CUSTOM_3, "CUSTOM-3" },
	{ HAL_RENDER_INTENT_CUSTOM_4, "CUSTOM-4" },
	{ HAL_RENDER_INTENT_CUSTOM_5, "CUSTOM-5" },
	{ HAL_RENDER_INTENT_CUSTOM_6, "CUSTOM-6" },
	{ HAL_RENDER_INTENT_CUSTOM_7, "CUSTOM-7" },
	{ HAL_RENDER_INTENT_CUSTOM_8, "CUSTOM-8" },
	{ HAL_RENDER_INTENT_CUSTOM_9, "CUSTOM-9" },
	{ HAL_RENDER_INTENT_CUSTOM_10, "CUSTOM-10" },
	{ HAL_RENDER_INTENT_CUSTOM_11, "CUSTOM-11" },
	{ HAL_RENDER_INTENT_CUSTOM_12, "CUSTOM-12" },
	{ HAL_RENDER_INTENT_CUSTOM_13, "CUSTOM-13" },
	{ HAL_RENDER_INTENT_CUSTOM_14, "CUSTOM-14" },
	{ HAL_RENDER_INTENT_CUSTOM_15, "CUSTOM-15" },
	{ HAL_RENDER_INTENT_CUSTOM_16, "CUSTOM-16" },
	{ HAL_RENDER_INTENT_CUSTOM_17, "CUSTOM-17" },
	{ HAL_RENDER_INTENT_CUSTOM_18, "CUSTOM-18" },
	{ HAL_RENDER_INTENT_CUSTOM_19, "CUSTOM-19" },
	{ HAL_RENDER_INTENT_CUSTOM_20, "CUSTOM-20" },
	{ HAL_RENDER_INTENT_CUSTOM_21, "CUSTOM-21" },
	{ HAL_RENDER_INTENT_CUSTOM_22, "CUSTOM-22" },
	{ HAL_RENDER_INTENT_CUSTOM_23, "CUSTOM-23" },
	{ HAL_RENDER_INTENT_CUSTOM_24, "CUSTOM-24" },
	{ HAL_RENDER_INTENT_CUSTOM_25, "CUSTOM-25" },
	{ HAL_RENDER_INTENT_RESERVED_0, "RESERVED-0" },
	{ HAL_RENDER_INTENT_RESERVED_1, "RESERVED-1" },
	{ HAL_RENDER_INTENT_RESERVED_2, "RESERVED-2" },
	{ HAL_RENDER_INTENT_RESERVED_3, "RESERVED-3" },
	{ HAL_RENDER_INTENT_RESERVED_4, "RESERVED-4" },
	{ HAL_RENDER_INTENT_RESERVED_5, "RESERVED-5" },
	{ HAL_RENDER_INTENT_RESERVED_6, "RESERVED-6" },
	{ HAL_RENDER_INTENT_RESERVED_7, "RESERVED-7" },
	{ HAL_RENDER_INTENT_RESERVED_8, "RESERVED-8" },
	{ HAL_RENDER_INTENT_RESERVED_9, "RESERVED-9" },
	{ HAL_RENDER_INTENT_RESERVED_10, "RESERVED-10" },
	{ HAL_RENDER_INTENT_RESERVED_11, "RESERVED-11" },
	{ HAL_RENDER_INTENT_RESERVED_12, "RESERVED-12" },
	{ HAL_RENDER_INTENT_RESERVED_13, "RESERVED-13" },
	{ HAL_RENDER_INTENT_RESERVED_14, "RESERVED-14" },
	{ HAL_RENDER_INTENT_RESERVED_15, "RESERVED-15" },
	{ HAL_RENDER_INTENT_RESERVED_16, "RESERVED-16" },
	{ HAL_RENDER_INTENT_RESERVED_17, "RESERVED-17" },
	{ HAL_RENDER_INTENT_RESERVED_18, "RESERVED-18" },
	{ HAL_RENDER_INTENT_RESERVED_19, "RESERVED-19" },
};

static void exynos_drm_crtc_atomic_enable(struct drm_crtc *crtc,
					  struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	const struct exynos_drm_crtc_ops *ops = exynos_crtc->ops;
	bool exit_hiber;

	DPU_ATRACE_BEGIN(__func__);
	exit_hiber = is_crtc_psr_disabled(crtc, old_state);
	if (exit_hiber && ops->atomic_exit_hiber)
		ops->atomic_exit_hiber(exynos_crtc);
	else if (!exit_hiber && ops->enable) {
		drm_crtc_vblank_on(crtc);
		ops->enable(exynos_crtc, old_state);
	}

	if (IS_ENABLED(CONFIG_EXYNOS_BTS) && exynos_crtc->bts->ops->set_bus_qos)
		exynos_crtc->bts->ops->set_bus_qos(exynos_crtc);
	DPU_ATRACE_END(__func__);
}

static void exynos_drm_crtc_disable_affected_planes(struct drm_crtc *crtc,
						    struct drm_atomic_state *old_state)
{
	struct drm_crtc_state *old_crtc_state = drm_atomic_get_old_crtc_state(old_state, crtc);
	const struct drm_plane_helper_funcs *plane_funcs;
	struct drm_plane_state *old_plane_state;
	struct drm_plane *plane;

	/* disable planes on this crtc only when the previous state is active. */
	if (!old_crtc_state->active)
		return;

	drm_atomic_crtc_state_for_each_plane(plane, old_crtc_state) {
		plane_funcs = plane->helper_private;
		old_plane_state = drm_atomic_get_old_plane_state(old_state, plane);
		if (old_plane_state && plane_funcs->atomic_disable)
			plane_funcs->atomic_disable(plane, old_state);
	}
}

static void exynos_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					   struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	const struct exynos_drm_crtc_ops *ops = exynos_crtc->ops;
	bool enter_hiber;

	DPU_ATRACE_BEGIN(__func__);
	exynos_drm_crtc_disable_affected_planes(crtc, old_state);

	enter_hiber = is_crtc_psr_enabled(crtc, old_state);
	if (enter_hiber && ops->atomic_enter_hiber)
		ops->atomic_enter_hiber(exynos_crtc);
	else if (!enter_hiber && ops->disable)
		ops->disable(exynos_crtc);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}

	if (!enter_hiber && ops->disable) {
		drm_crtc_vblank_off(crtc);
		exynos_self_refresh_atomic_clear_sync(exynos_crtc->self_refresh);
	}

	DPU_ATRACE_END(__func__);
}

#define MAX_PRESENT_MARGIN_NS (4 * 1000000)
static int exynos_crtc_adjust_expected_present_time(struct drm_crtc *crtc,
						    struct drm_crtc_state *new_crtc_state)
{
	struct drm_device *drm_dev = crtc->dev;
	struct exynos_drm_crtc_state *new_exynos_state =
				to_exynos_crtc_state(new_crtc_state);
	const struct exynos_display_mode *exynos_mode;
	ktime_t adj_present_time_ns;
	ktime_t vsync_period_ns;
	ktime_t last_vblank_time;
	int fps;

	if (!new_exynos_state->expected_present_time_ns)
		return 0;

	exynos_mode = &new_exynos_state->exynos_mode;
	if (exynos_mode && exynos_mode->vhm)
		fps = 240;
	else
		fps = drm_mode_vrefresh(&new_exynos_state->base.adjusted_mode);
	if (!fps) {
		drm_err(crtc->dev, "invalid fps(%d)\n", fps);
		return -EINVAL;
	}

	vsync_period_ns = HZ2NS(fps);

	drm_crtc_vblank_count_and_time(crtc, &last_vblank_time);
	if (new_exynos_state->expected_present_time_ns <= last_vblank_time)
		return 0;

	/* align user-requested EPT with HW vsync */
	adj_present_time_ns = ktime_sub(new_exynos_state->expected_present_time_ns,
			last_vblank_time);
	adj_present_time_ns = DIV_ROUND_CLOSEST(adj_present_time_ns, vsync_period_ns);
	adj_present_time_ns = last_vblank_time + adj_present_time_ns * vsync_period_ns;

	drm_dbg(drm_dev, "adjust ept_time"KTIME_SEC_FMT" to "KTIME_SEC_FMT"\n",
			KTIME_SEC_ARG(new_exynos_state->expected_present_time_ns),
			KTIME_SEC_ARG(adj_present_time_ns));

	new_exynos_state->expected_present_time_ns = adj_present_time_ns;
	new_exynos_state->adjusted_present_time_ns =
		ktime_sub(adj_present_time_ns, MAX_PRESENT_MARGIN_NS);

	return 0;
}

static int exynos_crtc_atomic_check(struct drm_crtc *crtc,
				    struct drm_atomic_state *state)
{
	int ret = 0;
	struct drm_crtc_state *new_crtc_state =
				drm_atomic_get_new_crtc_state(state, crtc);
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	DRM_DEBUG("%s +\n", __func__);

	ret = exynos_self_refresh_atomic_check(exynos_crtc->self_refresh, state);
	if (ret)
		return ret;

	if (!new_crtc_state->enable)
		return ret;

	if (exynos_crtc->ops->atomic_check) {
		ret = exynos_crtc->ops->atomic_check(exynos_crtc, state);
		if (ret)
			return ret;
	}

	ret = vmc_atomic_check(exynos_crtc->vmc, state);
	if (ret)
		return ret;

	ret = exynos_crtc_adjust_expected_present_time(crtc, new_crtc_state);
	if (ret)
		return ret;

	DRM_DEBUG("%s -\n", __func__);

	return ret;
}

static void exynos_crtc_atomic_begin(struct drm_crtc *crtc,
				     struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	DPU_ATRACE_BEGIN(__func__);
	if (exynos_crtc->ops->atomic_begin)
		exynos_crtc->ops->atomic_begin(exynos_crtc, old_state);

	if (exynos_crtc->partial) {
		struct drm_crtc_state *old_crtc_state =
			drm_atomic_get_old_crtc_state(old_state, crtc);
		struct drm_crtc_state *new_crtc_state =
			drm_atomic_get_new_crtc_state(old_state, crtc);
		struct exynos_drm_crtc_state *new_exynos_state, *old_exynos_state;

		new_exynos_state = to_exynos_crtc_state(new_crtc_state);
		old_exynos_state = to_exynos_crtc_state(old_crtc_state);

		exynos_partial_update(exynos_crtc->partial,
				&old_exynos_state->partial_region,
				&new_exynos_state->partial_region);

		if (old_crtc_state->self_refresh_active &&
				!new_crtc_state->self_refresh_active)
			exynos_partial_restore(exynos_crtc->partial);
	}
	DPU_ATRACE_END(__func__);
}

#define EPT_TIMEOUT_US			(100 * USEC_PER_MSEC)
void
exynos_crtc_wait_present_time(struct exynos_drm_crtc_state *exynos_crtc_state)
{
	struct drm_crtc *crtc = exynos_crtc_state->base.crtc;
	struct drm_device *drm_dev = crtc->dev;
	ktime_t wait_time_ns;
	unsigned long wait_time_us;
	ktime_t curr, adj_present_time_ns;

	if (!exynos_crtc_state->adjusted_present_time_ns)
		return;

	adj_present_time_ns = exynos_crtc_state->adjusted_present_time_ns;
	curr = ktime_get();

	drm_dbg(drm_dev, "adjusted_ept_time"KTIME_SEC_FMT"\n",
			KTIME_SEC_ARG(adj_present_time_ns));
	drm_dbg(drm_dev, "curr_time"KTIME_SEC_FMT"\n", KTIME_SEC_ARG(curr));

	if (ktime_after(curr, adj_present_time_ns))
		return;

	DPU_ATRACE_BEGIN(__func__);
	wait_time_ns = ktime_sub(adj_present_time_ns, curr);
	drm_dbg(drm_dev, "wait_time"KTIME_MSEC_FMT"\n", KTIME_MSEC_ARG(wait_time_ns));
	wait_time_us = (unsigned long)ktime_to_us(wait_time_ns);
	wait_time_us = min_t(unsigned long, wait_time_us, EPT_TIMEOUT_US);
	usleep_range(wait_time_us, wait_time_us + 10);
	DPU_EVENT_LOG("PRESENT_TIME", to_exynos_crtc(crtc), 0,
		      " delayed[%ldus]", wait_time_us);
	DPU_ATRACE_END(__func__);
}

static void __set_freq_step(struct exynos_drm_crtc *exynos_crtc,
                               struct drm_atomic_state *old_state)
{
	struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_crtc_state *old_exynos_crtc_state, *new_exynos_crtc_state;
	struct exynos_drm_connector *exynos_conn;
	const struct exynos_drm_connector_funcs *funcs;
	u32 frame_interval_ns;

	new_exynos_crtc_state = exynos_drm_atomic_get_new_crtc_state(old_state, exynos_crtc);
	old_exynos_crtc_state = exynos_drm_atomic_get_old_crtc_state(old_state, exynos_crtc);

	frame_interval_ns = new_exynos_crtc_state->frame_interval_ns;
	if (old_exynos_crtc_state->frame_interval_ns == frame_interval_ns)
		return;

	new_crtc_state = drm_atomic_get_new_crtc_state(old_state, &exynos_crtc->base);
	exynos_conn = crtc_get_exynos_conn(new_crtc_state, DRM_MODE_CONNECTOR_DSI);
	if (!exynos_conn)
		return;

	funcs =	exynos_conn->funcs;
	if (funcs->set_freq_step)
		funcs->set_freq_step(exynos_conn, frame_interval_ns);
}

static void exynos_crtc_atomic_flush(struct drm_crtc *crtc,
				     struct drm_atomic_state *old_state)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	struct drm_crtc_state *new_crtc_state =
				drm_atomic_get_new_crtc_state(old_state, crtc);
	struct exynos_drm_crtc_state *new_exynos_state =
				to_exynos_crtc_state(new_crtc_state);
	ktime_t curr = ktime_get();

	DPU_ATRACE_BEGIN(__func__);

	exynos_crtc_arm_event(exynos_crtc);
	exynos_self_refresh_atomic_queue_dvrr_config(exynos_crtc->self_refresh, old_state);

	__set_freq_step(exynos_crtc, old_state);

	if (new_exynos_state->skip_hw_update) {
		drm_dbg(crtc->dev, "curr"KTIME_SEC_FMT"notifyEPT is called frame_interval_ns(%u) adj ept"KTIME_SEC_FMT"\n",
				KTIME_SEC_ARG(curr),
				new_exynos_state->frame_interval_ns,
				KTIME_SEC_ARG(new_exynos_state->adjusted_present_time_ns));
		goto out;
	}

	if (exynos_crtc->ops->atomic_flush)
		exynos_crtc->ops->atomic_flush(exynos_crtc, old_state);

out:
	DPU_ATRACE_END(__func__);
}

static enum drm_mode_status exynos_crtc_mode_valid(struct drm_crtc *crtc,
	const struct drm_display_mode *mode)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->mode_valid)
		return exynos_crtc->ops->mode_valid(exynos_crtc, mode);

	return MODE_OK;
}

static bool exynos_crtc_mode_fixup(struct drm_crtc *crtc,
				   const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->mode_fixup)
		return exynos_crtc->ops->mode_fixup(exynos_crtc, mode,
						    adjusted_mode);

	return true;
}

static void exynos_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	const struct drm_crtc_state *crtc_state = crtc->state;
	const struct exynos_drm_crtc_state *exynos_crtc_state =
					to_exynos_crtc_state(crtc_state);

	if (exynos_crtc->ops->mode_set)
		exynos_crtc->ops->mode_set(exynos_crtc, &crtc_state->mode,
				&crtc_state->adjusted_mode);

	if (exynos_crtc_state->bts_fps_ptr && exynos_crtc_state->exynos_mode.bts_fps) {
		u32 bts_fps = exynos_crtc_state->exynos_mode.bts_fps;

		if (put_user(bts_fps, exynos_crtc_state->bts_fps_ptr))
			pr_err("%s: failed to put bts_fps(%d) to user_ptr(%p)\n",
				__func__, bts_fps, exynos_crtc_state->bts_fps_ptr);
	}
}

static const struct drm_crtc_helper_funcs exynos_crtc_helper_funcs = {
	.mode_valid	= exynos_crtc_mode_valid,
	.mode_fixup	= exynos_crtc_mode_fixup,
	.mode_set_nofb	= exynos_crtc_mode_set_nofb,
	.atomic_check	= exynos_crtc_atomic_check,
	.atomic_begin	= exynos_crtc_atomic_begin,
	.atomic_flush	= exynos_crtc_atomic_flush,
	.atomic_enable	= exynos_drm_crtc_atomic_enable,
	.atomic_disable	= exynos_drm_crtc_atomic_disable,
};

static bool __is_recovery(struct exynos_drm_crtc *exynos_crtc)
{
	bool recovery = false;

	if (exynos_crtc->ops->is_recovering &&
			exynos_crtc->ops->is_recovering(exynos_crtc))
		recovery = true;

	return recovery;
}

void exynos_crtc_arm_event(struct exynos_drm_crtc *exynos_crtc)
{
	struct dma_fence *fence;
	struct drm_crtc *crtc = &exynos_crtc->base;
	struct drm_pending_vblank_event *event = crtc->state->event;
	unsigned long flags;
	bool recovery = false;

	if (!event)
		return;

	crtc->state->event = NULL;
	recovery = __is_recovery(exynos_crtc);

	if (recovery)
		drm_crtc_vblank_get(crtc);
	else
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (!recovery)
		WARN_ON(exynos_crtc->event != NULL);
	fence = event->base.fence;
	if (fence)
		DPU_EVENT_LOG("ARM_CRTC_OUT_FENCE", exynos_crtc, EVENT_FLAG_FENCE,
				FENCE_FMT, FENCE_ARG(fence));
	exynos_crtc->event = event;
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

void exynos_crtc_send_event(struct exynos_drm_crtc *exynos_crtc)
{
	struct drm_device *dev;
	unsigned long flags;

	if (!exynos_crtc)
		return;

	dev = exynos_crtc->base.dev;
	spin_lock_irqsave(&dev->event_lock, flags);

	if (exynos_crtc->event) {
		drm_send_event_locked(exynos_crtc->base.dev, &exynos_crtc->event->base);
		exynos_crtc->event = NULL;
		drm_crtc_vblank_put(&exynos_crtc->base);
		DPU_EVENT_LOG("SIGNAL_CRTC_OUT_FENCE", exynos_crtc,
				EVENT_FLAG_REPEAT | EVENT_FLAG_FENCE, NULL);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void exynos_drmm_crtc_destroy(struct drm_device *dev, void *ptr)
{
	struct exynos_drm_crtc *exynos_crtc = ptr;

	if (exynos_crtc->thread)
		kthread_stop(exynos_crtc->thread);
}

static int exynos_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->enable_vblank)
		return exynos_crtc->ops->enable_vblank(exynos_crtc);

	return 0;
}

static void exynos_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->disable_vblank)
		exynos_crtc->ops->disable_vblank(exynos_crtc);
}

static u32 exynos_drm_crtc_get_vblank_counter(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (exynos_crtc->ops->get_vblank_counter)
		return exynos_crtc->ops->get_vblank_counter(exynos_crtc);

	return 0;
}

static void exynos_drm_crtc_destroy_state(struct drm_crtc *crtc,
					struct drm_crtc_state *state)
{
	struct exynos_drm_crtc_state *exynos_crtc_state;

	exynos_crtc_state = to_exynos_crtc_state(state);
	drm_property_blob_put(exynos_crtc_state->partial);
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(exynos_crtc_state);
}

#define MAX_BPC (10)
static void exynos_drm_crtc_reset(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc_state *exynos_crtc_state;

	if (crtc->state) {
		exynos_drm_crtc_destroy_state(crtc, crtc->state);
		crtc->state = NULL;
	}

	exynos_crtc_state = kzalloc(sizeof(*exynos_crtc_state), GFP_KERNEL);
	if (exynos_crtc_state) {
		exynos_crtc_state->in_bpc = MAX_BPC;
		crtc->state = &exynos_crtc_state->base;
		crtc->state->crtc = crtc;
	} else {
		pr_err("failed to allocate exynos crtc state\n");
	}
}

static struct drm_crtc_state *
exynos_drm_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc_state *exynos_crtc_state;
	struct exynos_drm_crtc_state *copy;

	exynos_crtc_state = to_exynos_crtc_state(crtc->state);
	copy = kzalloc(sizeof(*copy), GFP_KERNEL);
	if (!copy)
		return NULL;

	memcpy(copy, exynos_crtc_state, sizeof(*copy));
	copy->wb_type = EXYNOS_WB_NONE;
	copy->freed_win_mask = 0;
	copy->skip_hw_update = false;
	copy->tui_changed = false;
	copy->hiber_retention_changed = false;
	copy->skip_frameupdate = false;
	copy->need_colormap = false;
	copy->bts_fps_ptr = NULL;
	copy->boost_bts_fps = 0;
	copy->expected_present_time_ns = 0;
	copy->adjusted_present_time_ns = 0;
	copy->seamless_modeset = 0;

	if (copy->partial)
		drm_property_blob_get(copy->partial);

	__drm_atomic_helper_crtc_duplicate_state(crtc, &copy->base);

	return &copy->base;
}

static int exynos_drm_crtc_set_property(struct drm_crtc *crtc,
					struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t val)
{
	const struct exynos_drm_properties *p;
	struct exynos_drm_crtc_state *exynos_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;
	int ret = 0;

	if (!crtc) {
		pr_err("%s: invalid param, crtc is null\n", __func__);
		return -EINVAL;
	}

	p = dev_get_exynos_props(crtc->dev);
	if (!p)
		return -EINVAL;

	exynos_crtc_state = to_exynos_crtc_state(state);
	exynos_crtc = to_exynos_crtc(crtc);

	if ((p->color_mode) && (property == p->color_mode)) {
		exynos_crtc_state->color_mode = val;
		state->color_mgmt_changed = true;
	} else if ((p->render_intent) && (property == p->render_intent)) {
		exynos_crtc_state->render_intent = val;
		state->color_mgmt_changed = true;
	} else if ((p->adjusted_vblank) && (property == p->adjusted_vblank)) {
		ret = 0; /* adj_vblank setting is not supported  */
	} else if ((p->dsr_status) && (property == p->dsr_status)) {
		exynos_crtc_state->dsr_status = val;
	} else if ((p->modeset_only) && (property == p->modeset_only)) {
		exynos_crtc_state->skip_frameupdate = val;
	} else if ((p->skip_hw_update) && (property == p->skip_hw_update)) {
		exynos_crtc_state->skip_hw_update = val;
	} else if ((p->skip_frameupdate) && (property == p->skip_frameupdate)) {
		exynos_crtc_state->skip_frameupdate = val;
	} else if ((p->partial) && (property == p->partial)) {
		ret = exynos_drm_replace_property_blob_from_id(crtc->dev,
				&exynos_crtc_state->partial,
				val, sizeof(struct drm_clip_rect));
	} else if ((p->dqe_fd) && (property == p->dqe_fd)) {
		exynos_crtc_state->color_fd_slot0 = U642I64(val);
		state->color_mgmt_changed = true;
	} else if ((p->color_fd_slot0) && (property == p->color_fd_slot0)) {
		exynos_crtc_state->color_fd_slot0 = U642I64(val);
		state->color_mgmt_changed = true;
	} else if ((p->color_fd_slot1) && (property == p->color_fd_slot1)) {
		exynos_crtc_state->color_fd_slot1 = U642I64(val);
		state->color_mgmt_changed = true;
	} else if ((p->bts_fps) && (property == p->bts_fps)) {
		exynos_crtc_state->bts_fps_ptr = u64_to_user_ptr(val);
	} else if ((p->expected_present_time_ns) &&
			(property == p->expected_present_time_ns)) {
		exynos_crtc_state->expected_present_time_ns = val;
	} else if ((p->frame_interval_ns) && (property == p->frame_interval_ns)) {
		exynos_crtc_state->frame_interval_ns = val;
	} else {
		ret = -EINVAL;
	}

	return ret;
}

#define NEXT_ADJ_VBLANK_OFFSET	1
static int get_next_adjusted_vblank_timestamp(struct drm_crtc *crtc, uint64_t *val)
{
	struct drm_vblank_crtc *vblank;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	int count = NEXT_ADJ_VBLANK_OFFSET;
	ktime_t timestamp, cur_time, diff;
#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	s64 elapsed_time;
	ktime_t s_time = ktime_get();
#endif

	if (drm_crtc_index(crtc) >= crtc->dev->num_crtcs) {
		pr_err("%s: invalid param, crtc index is out of range\n", __func__);
		return -EINVAL;
	}

	spin_lock(&crtc->commit_lock);
	if (!list_empty(&crtc->commit_list))
		count++;
	spin_unlock(&crtc->commit_lock);

#if IS_ENABLED(CONFIG_DRM_MCD_COMMON)
	elapsed_time = ktime_to_us(ktime_sub(ktime_get(), s_time));
	if ((count >= 10) || (elapsed_time > 50000))
		pr_info("%s: elapsed time: %lld, count: %d\n", __func__, elapsed_time, count);
#endif

	vblank = &crtc->dev->vblank[drm_crtc_index(crtc)];
	timestamp = vblank->time + count * vblank->framedur_ns;
	cur_time = ktime_get();
	if (cur_time > timestamp) {
		diff = cur_time - timestamp;
		timestamp += (DIV_ROUND_DOWN_ULL(diff, vblank->framedur_ns) +
				NEXT_ADJ_VBLANK_OFFSET) * vblank->framedur_ns;
	}
	*val = timestamp;

	DPU_EVENT_LOG("NEXT_ADJ_VBLANK", exynos_crtc, EVENT_FLAG_REPEAT,
			"timestamp(%lld)", timestamp);

	return 0;
}

static int exynos_drm_crtc_get_property(struct drm_crtc *crtc,
					const struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	const struct exynos_drm_properties *p;
	struct exynos_drm_crtc_state *exynos_crtc_state;
	struct exynos_drm_crtc *exynos_crtc;

	if (!crtc) {
		pr_err("%s: invalid param, crtc is null\n", __func__);
		return -EINVAL;
	}

	p = dev_get_exynos_props(crtc->dev);
	if (!p)
		return -EINVAL;

	exynos_crtc_state = to_exynos_crtc_state(state);
	exynos_crtc = to_exynos_crtc(crtc);

	if ((p->color_mode) && (property == p->color_mode))
		*val = exynos_crtc_state->color_mode;
	else if ((p->render_intent) && (property == p->render_intent))
		*val = exynos_crtc_state->render_intent;
	else if ((p->adjusted_vblank) && (property == p->adjusted_vblank))
		return get_next_adjusted_vblank_timestamp(crtc, val);
	else if ((p->dsr_status) && (property == p->dsr_status))
		*val = exynos_crtc_state->dsr_status;
	else if ((p->modeset_only) && (property == p->modeset_only))
		*val = exynos_crtc_state->skip_frameupdate;
	else if ((p->skip_hw_update) && (property == p->skip_hw_update))
		*val = exynos_crtc_state->skip_hw_update;
	else if ((p->skip_frameupdate) && (property == p->skip_frameupdate))
		*val = exynos_crtc_state->skip_frameupdate;
	else if ((p->partial) && (property == p->partial))
		*val = exynos_crtc_state->partial ?
			exynos_crtc_state->partial->base.id : 0;
	else if ((p->dqe_fd) && (property == p->dqe_fd))
		*val = I642U64(exynos_crtc_state->color_fd_slot0);
	else if ((p->color_fd_slot0) && (property == p->color_fd_slot0))
		*val = I642U64(exynos_crtc_state->color_fd_slot0);
	else if ((p->color_fd_slot1) && (property == p->color_fd_slot1))
		*val = I642U64(exynos_crtc_state->color_fd_slot1);
	else if ((p->bts_fps) && (property == p->bts_fps))
		*val = exynos_crtc->bts->fps;
	else if ((p->expected_present_time_ns) &&
			(property == p->expected_present_time_ns))
		*val = exynos_crtc_state->expected_present_time_ns;
	else if ((p->frame_interval_ns) && (property == p->frame_interval_ns))
		*val = exynos_crtc_state->frame_interval_ns;
	else
		return -EINVAL;

	return 0;
}

static void exynos_drm_crtc_print_state(struct drm_printer *p,
					const struct drm_crtc_state *state)
{
	const struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(state->crtc);
	const struct exynos_drm_crtc_state *exynos_crtc_state =
						to_exynos_crtc_state(state);
	const struct exynos_display_mode *exynos_mode;

	drm_printf(p, "\treserved_win_mask=0x%x\n",
			exynos_crtc_state->reserved_win_mask);
	drm_printf(p, "\tdsr_status=%d\n", exynos_crtc_state->dsr_status);
	drm_printf(p, "\tcolor_mode=%d\n", exynos_crtc_state->color_mode);
	drm_printf(p, "\trender_intent=%d\n", exynos_crtc_state->render_intent);
	drm_printf(p, "\tmodeset_only=%d\n", exynos_crtc_state->skip_frameupdate);
	drm_printf(p, "\tskip_hw_update=%d\n", exynos_crtc_state->skip_hw_update);
	drm_printf(p, "\tskip_frameupdate=%d\n", exynos_crtc_state->skip_frameupdate);
	drm_printf(p, "\tcolor_fd_slot0=%lld\n", exynos_crtc_state->color_fd_slot0);
	drm_printf(p, "\tcolor_fd_slot1=%lld\n", exynos_crtc_state->color_fd_slot1);
	drm_printf(p, "\texpected_present_time_ns=%lld\n",
			exynos_crtc_state->expected_present_time_ns);
	drm_printf(p, "\tframe_interval_ns=%u\n",
			exynos_crtc_state->frame_interval_ns);
	if (exynos_crtc_state->partial) {
		struct drm_clip_rect *partial_region =
			(struct drm_clip_rect *)exynos_crtc_state->partial->data;

		drm_printf(p, "\tblob(%d) partial region[%d %d %d %d]\n",
				exynos_crtc_state->partial->base.id,
				partial_region->x1, partial_region->y1,
				partial_region->x2 - partial_region->x1,
				partial_region->y2 - partial_region->y1);
	} else {
		drm_printf(p, "\tno partial region request\n");
	}

	if (exynos_crtc->ops->atomic_print_state)
		exynos_crtc->ops->atomic_print_state(p, exynos_crtc);

	exynos_mode = &exynos_crtc_state->exynos_mode;
	if (exynos_mode) {
		const struct exynos_display_dsc *dsc = &exynos_mode->dsc;

		drm_printf(p, "\tcurrent exynos_mode:\n");
		drm_printf(p, "\t\tdsc: en=%d dsc_cnt=%d slice_cnt=%d slice_h=%d\n",
				dsc->enabled, dsc->dsc_count, dsc->slice_count,
				dsc->slice_height);
		drm_printf(p, "\t\tout bpc: %d\n", exynos_mode->bpc);
		drm_printf(p, "\t\top_mode: %s %s\n", exynos_mode->mode_flags &
				MIPI_DSI_MODE_VIDEO ? "video" : "cmd",
				exynos_mode->vhm ? "hybrid" : "");
		drm_printf(p, "\t\tlp_mode_state: %d\n", exynos_mode->is_lp_mode);
		drm_printf(p, "\t\tbts_fps: %d\n", exynos_mode->bts_fps);
		drm_printf(p, "\t\tesync_hz: %d\n", exynos_mode->esync_hz);
	}
}

static int exynos_drm_crtc_late_register(struct drm_crtc *crtc)
{
	int ret = 0;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	ret = dpu_init_debug(exynos_crtc);
	if (ret)
		return ret;

	if (exynos_crtc->ops->late_register)
		ret = exynos_crtc->ops->late_register(exynos_crtc);

	return ret;
}

static void exysno_drm_crtc_early_unregister(struct drm_crtc *crtc)
{

}

static const char * const exynos_crc_source[] = {"auto"};
static int exynos_crtc_parse_crc_source(const char *source)
{
	if (!source)
		return 0;

	if (strcmp(source, "auto") == 0)
		return 1;

	return -EINVAL;
}

int exynos_drm_crtc_set_crc_source(struct drm_crtc *crtc, const char *source)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);

	if (!source) {
		DRM_ERROR("CRC source for crtc(%d) was not set\n", crtc->index);
		exynos_crtc_set_crc_state(exynos_crtc, EXYNOS_DRM_CRC_STOP);
		return 0;
	}

	if (exynos_crtc->crc_state == EXYNOS_DRM_CRC_STOP)
		exynos_crtc_set_crc_state(exynos_crtc, EXYNOS_DRM_CRC_REQ);

	return 0;
}

int exynos_drm_crtc_verify_crc_source(struct drm_crtc *crtc, const char *source,
				      size_t *values_cnt)
{
	int idx = exynos_crtc_parse_crc_source(source);

	if (idx < 0) {
		DRM_INFO("Unknown or invalid CRC source for CRTC%d\n", crtc->index);
		return -EINVAL;
	}

	*values_cnt = 3;
	return 0;
}

const char *const *exynos_drm_crtc_get_crc_sources(struct drm_crtc *crtc,
						   size_t *count)
{
	*count = ARRAY_SIZE(exynos_crc_source);
	return exynos_crc_source;
}

static const struct drm_crtc_funcs exynos_crtc_funcs = {
	.set_config		= drm_atomic_helper_set_config,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= exynos_drm_crtc_reset,
	.atomic_duplicate_state	= exynos_drm_crtc_duplicate_state,
	.atomic_destroy_state	= exynos_drm_crtc_destroy_state,
	.atomic_set_property	= exynos_drm_crtc_set_property,
	.atomic_get_property	= exynos_drm_crtc_get_property,
	.atomic_print_state     = exynos_drm_crtc_print_state,
	.enable_vblank		= exynos_drm_crtc_enable_vblank,
	.disable_vblank		= exynos_drm_crtc_disable_vblank,
	.get_vblank_counter	= exynos_drm_crtc_get_vblank_counter,
	.late_register		= exynos_drm_crtc_late_register,
	.early_unregister	= exysno_drm_crtc_early_unregister,
	.set_crc_source		= exynos_drm_crtc_set_crc_source,
	.verify_crc_source		= exynos_drm_crtc_verify_crc_source,
	.get_crc_sources		= exynos_drm_crtc_get_crc_sources,
};

int exynos_drm_crtc_create_properties(struct drm_device *drm_dev)
{
	struct exynos_drm_properties *p;

	p = dev_get_exynos_props(drm_dev);
	if (!p)
		return -EINVAL;

	p->color_mode = drm_property_create_enum(drm_dev, 0, "color mode",
			color_mode_list, ARRAY_SIZE(color_mode_list));
	if (!p->color_mode)
		return -ENOMEM;

	p->render_intent = drm_property_create_enum(drm_dev, 0, "render intent",
			render_intent_list, ARRAY_SIZE(render_intent_list));
	if (!p->render_intent)
		return -ENOMEM;

	p->adjusted_vblank = drm_property_create_signed_range(drm_dev, 0,
			"adjusted_vblank", 0, LONG_MAX);
	if (!p->adjusted_vblank)
		return -ENOMEM;

	/* decon self refresh for VR */
	p->dsr_status = drm_property_create_bool(drm_dev, 0, "dsr_status");
	if (!p->dsr_status)
		return -ENOMEM;

	p->modeset_only = drm_property_create_bool(drm_dev, 0, "modeset_only");
	if (!p->modeset_only)
		return -ENOMEM;

	p->skip_hw_update = drm_property_create_bool(drm_dev, 0, "skip_hw_update");
	if (!p->skip_hw_update)
		return -ENOMEM;

	p->skip_frameupdate = drm_property_create_bool(drm_dev, 0, "skip_frameupdate");
	if (!p->skip_frameupdate)
		return -ENOMEM;

	p->dqe_fd = drm_property_create_signed_range(drm_dev, 0, "DQE_FD", -1, 1023);
	if (!p->dqe_fd)
		return -ENOMEM;

	p->color_fd_slot0 = drm_property_create_signed_range(drm_dev, 0, "color_fd_slot0", -1, 1023);
	if (!p->color_fd_slot0)
		return -ENOMEM;

	p->color_fd_slot1 = drm_property_create_signed_range(drm_dev, 0, "color_fd_slot1", -1, 1023);
	if (!p->color_fd_slot1)
		return -ENOMEM;

	p->partial = drm_property_create(drm_dev, DRM_MODE_PROP_BLOB,
			"partial_region", 0);
	if (!p->partial)
		return -ENOMEM;

	p->bts_fps = drm_property_create_range(drm_dev, 0, "bts_fps", 0, U64_MAX);
	if (!p->bts_fps)
		return -ENOMEM;

	p->expected_present_time_ns = drm_property_create_range(drm_dev,
			0, "expected_present_time", 0, U64_MAX);
	if (!p->expected_present_time_ns)
		return -ENOMEM;

	p->frame_interval_ns = drm_property_create_range(drm_dev,
			0, "frame_interval_ns", 0, U32_MAX);
	if (!p->frame_interval_ns)
		return -ENOMEM;

	p->restrictions = drm_property_create(drm_dev,
			DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_BLOB,
			"restrictions", 0);
	if (!p->restrictions)
		return -ENOMEM;

	return 0;
}

static int exynos_drm_crtc_attach_restrictions_property(struct drm_device *drm_dev,
							struct drm_mode_object *obj,
							const struct decon_restrict *res)
{
	struct drm_property_blob *blob;
	struct exynos_drm_properties *p = dev_get_exynos_props(drm_dev);
	size_t size = SZ_2K;
	char *blob_data, *ptr;

	blob_data = kzalloc(size, GFP_KERNEL);
	if (!blob_data)
		return -ENOMEM;

	ptr = blob_data;
	dpu_res_add_u32(&ptr, &size, DPU_RES_ID, &res->id);
	dpu_res_add_u32(&ptr, &size, DPU_RES_DISP_CLK_MAX, &res->disp_max_clock);
	dpu_res_add_u32(&ptr, &size, DPU_RES_DISP_CLK_MARGIN_PCT, &res->disp_margin_pct);
	dpu_res_add_u32(&ptr, &size, DPU_RES_DISP_CLK_FACTOR_PCT, &res->disp_factor_pct);
	dpu_res_add_u32(&ptr, &size, DPU_RES_DISP_CLK_PPC, &res->ppc);

	if (res->dvrr_en) {
		u32 val;

		if (res->min_frame_interval_hz) {
			val = HZ2NS(res->min_frame_interval_hz);
			dpu_res_add_u32(&ptr, &size, DPU_RES_MIN_FRAME_INTERVAL_NS, &val);
		}
		if (res->vrr_timeout_ms) {
			val = res->vrr_timeout_ms * NSEC_PER_MSEC;
			dpu_res_add_u32(&ptr, &size, DPU_RES_VRR_TIMEOUT_NS, &val);
		}
		if (res->esyn_hz) {
			val = HZ2NS(res->esyn_hz);
			dpu_res_add_u32(&ptr, &size, DPU_RES_ESYN_PERIOD_NS, &val);
		}
	}

	blob = drm_property_create_blob(drm_dev, SZ_2K - size, blob_data);
	if (IS_ERR(blob))
		return PTR_ERR(blob);

	drm_object_attach_property(obj, p->restrictions, blob->base.id);

	kfree(blob_data);

	return 0;
}

struct exynos_drm_crtc *
exynos_drm_crtc_create(struct drm_device *drm_dev, unsigned int index,
		       struct exynos_drm_crtc_config *config,
		       const struct exynos_drm_crtc_ops *ops)
{
	struct exynos_drm_properties *p = dev_get_exynos_props(drm_dev);
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc *crtc;
	struct sched_param param = {
		.sched_priority = 20
	};
	int ret;

	exynos_crtc = drmm_crtc_alloc_with_planes(drm_dev, struct exynos_drm_crtc,
			base, config->default_plane, NULL, &exynos_crtc_funcs,
			"exynos-crtc-%d", index);
	if (IS_ERR(exynos_crtc)) {
		DRM_ERROR("failed to alloc exynos_crtc(%d)\n", index);
		return exynos_crtc;
	}

	exynos_crtc->possible_type = config->con_type;
	exynos_crtc->ops = ops;
	exynos_crtc->ctx = config->ctx;

	exynos_crtc->dqe = exynos_dqe_register(config->ctx);
	exynos_crtc->vmc = vmc_register(config->ctx);

	crtc = &exynos_crtc->base;

	drm_crtc_helper_add(crtc, &exynos_crtc_helper_funcs);

	/* valid only for LCD display */
	if (exynos_crtc->possible_type & EXYNOS_DISPLAY_TYPE_DSI) {
		drm_object_attach_property(&crtc->base, p->adjusted_vblank, 0);
		drm_object_attach_property(&crtc->base, p->dsr_status, 0);
		drm_object_attach_property(&crtc->base, p->skip_hw_update, 0);
		drm_object_attach_property(&crtc->base, p->skip_frameupdate, 0);
		drm_object_attach_property(&crtc->base, p->partial, 0);
		drm_object_attach_property(&crtc->base, p->bts_fps, 0);
		drm_object_attach_property(&crtc->base, p->expected_present_time_ns, 0);
		drm_object_attach_property(&crtc->base, p->frame_interval_ns, 0);
	}

	if (exynos_crtc->dqe) {
		drm_object_attach_property(&crtc->base, p->color_mode, HAL_COLOR_MODE_NATIVE);
		drm_object_attach_property(&crtc->base, p->render_intent,
				HAL_RENDER_INTENT_COLORIMETRIC);
		drm_object_attach_property(&crtc->base, p->dqe_fd, I642U64(-1));
		drm_object_attach_property(&crtc->base, p->color_fd_slot0, I642U64(-1));
		drm_object_attach_property(&crtc->base, p->color_fd_slot1, I642U64(-1));
	}

	exynos_drm_crtc_attach_restrictions_property(drm_dev, &crtc->base, config->res);

	drm_crtc_enable_color_mgmt(crtc, 0, false, 0);

	kthread_init_worker(&exynos_crtc->worker);
	exynos_crtc->thread = kthread_run(kthread_worker_fn, &exynos_crtc->worker,
			"crtc%d_kthread", crtc->index);
	if (IS_ERR(exynos_crtc->thread)) {
		DRM_ERROR("failed to run display thread of crtc(%d)\n", crtc->index);
		return (void *)exynos_crtc->thread;
	}
	sched_setscheduler_nocheck(exynos_crtc->thread, SCHED_FIFO, &param);

	ret = drmm_add_action_or_reset(drm_dev, exynos_drmm_crtc_destroy, exynos_crtc);
	if (ret)
		return ERR_PTR(ret);

	exynos_crtc->hibernation = exynos_hibernation_register(exynos_crtc);
	exynos_crtc->profiler = exynos_drm_profiler_register(exynos_crtc);
	exynos_crtc->self_refresh = exynos_self_refresh_register(exynos_crtc);

	return exynos_crtc;
}

struct exynos_drm_crtc *exynos_drm_crtc_get_by_type(struct drm_device *drm_dev,
				       enum exynos_drm_output_type out_type)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm_dev)
		if (to_exynos_crtc(crtc)->possible_type == out_type)
			return to_exynos_crtc(crtc);

	return ERR_PTR(-EPERM);
}

uint32_t exynos_drm_get_possible_crtcs(struct drm_encoder *encoder,
		enum exynos_drm_output_type out_type)
{
	struct exynos_drm_crtc *find_crtc;
	struct drm_crtc *crtc;
	uint32_t possible_crtcs = 0;

	drm_for_each_crtc(crtc, encoder->dev) {
		if (to_exynos_crtc(crtc)->possible_type & out_type) {
			find_crtc = to_exynos_crtc(crtc);
			possible_crtcs |= drm_crtc_mask(&find_crtc->base);
		}
	}

	return possible_crtcs;
}

void exynos_crtc_get_crc_data(struct exynos_drm_crtc *exynos_crtc)
{
	if (!exynos_crtc)
		return;

	if (exynos_crtc->crc_state == EXYNOS_DRM_CRC_PEND)
		if (exynos_crtc->ops->get_crc_data)
			exynos_crtc->ops->get_crc_data(exynos_crtc);

}

void exynos_crtc_set_crc_state(struct exynos_drm_crtc *exynos_crtc, int crc_state)
{
	if (!exynos_crtc)
		return;

	switch (crc_state) {
	case EXYNOS_DRM_CRC_STOP:
	case EXYNOS_DRM_CRC_REQ:
	case EXYNOS_DRM_CRC_START:
		exynos_crtc->crc_state = crc_state;
		break;
	case EXYNOS_DRM_CRC_PEND:
		if (exynos_crtc->crc_state == EXYNOS_DRM_CRC_START)
			exynos_crtc->crc_state = EXYNOS_DRM_CRC_PEND;
		break;
	default:
		break;
	}
}

struct drm_encoder*
crtc_get_encoder(const struct drm_crtc_state *crtc_state, u32 encoder_type)
{
	struct drm_encoder *encoder = NULL;
	struct drm_device *dev = crtc_state->crtc->dev;

	drm_for_each_encoder_mask(encoder, dev, crtc_state->encoder_mask)
		if (encoder->encoder_type == encoder_type)
			return encoder;

	return NULL;
}

struct drm_connector*
crtc_get_conn(const struct drm_crtc_state *crtc_state, u32 conn_type)
{
	struct drm_connector *conn, *ret = NULL;
	struct drm_connector_list_iter conn_iter;
	struct drm_device *dev = crtc_state->crtc->dev;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		if ((crtc_state->connector_mask & drm_connector_mask(conn))
				&& conn->connector_type == conn_type)
			ret = conn;
	}
	drm_connector_list_iter_end(&conn_iter);

	return ret;
}

struct exynos_drm_connector *
crtc_get_exynos_conn(const struct drm_crtc_state *crtc_state, int conn_type)
{
	struct drm_connector *conn = crtc_get_conn(crtc_state, conn_type);

	if (conn)
		return to_exynos_connector(conn);

	return NULL;
}

static bool dpu_vblank_duration_check;
module_param(dpu_vblank_duration_check, bool, 0600);
MODULE_PARM_DESC(dpu_vblank_duration_check, "dpu vblank duration check [default : false]");
void exynos_drm_crtc_handle_vblank(struct exynos_drm_crtc *exynos_crtc)
{
	ktime_t prev_vblank_time, curr_vblank_time;
	ktime_t period_nsec;
	u64 vblank_count;

	if (!exynos_crtc)
		return;

	exynos_hibernation_queue_enter_work(exynos_crtc);
	vblank_count = drm_crtc_vblank_count_and_time(&exynos_crtc->base, &prev_vblank_time);
	drm_crtc_handle_vblank(&exynos_crtc->base);
	vmc_still_on(exynos_crtc->vmc);
	exynos_self_refresh_update_esync(exynos_crtc->self_refresh);

	vblank_count = drm_crtc_vblank_count_and_time(&exynos_crtc->base, &curr_vblank_time);
	period_nsec = ktime_sub(curr_vblank_time, prev_vblank_time);
	DPU_ATRACE_TOGGLE("DPU VSYNC", exynos_crtc->thread->pid);
	DPU_EVENT_LOG("VBLANK", exynos_crtc, EVENT_FLAG_REPEAT,
			VBLANK_FMT, VBLANK_ARG(vblank_count, curr_vblank_time, period_nsec));
	if (dpu_vblank_duration_check)
		DRM_INFO(VBLANK_FMT, VBLANK_ARG(vblank_count, curr_vblank_time, period_nsec));

	exynos_drm_profiler_update_vsync_cnt(exynos_crtc);
}
