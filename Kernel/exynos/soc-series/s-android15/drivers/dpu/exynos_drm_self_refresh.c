// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_self_refresh.c
 *
 * Copyright (C) 2024 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drm_vblank.h>
#include <drm/drm_vblank_work.h>

#include <dpu_trace.h>
#include <uapi/linux/sched/types.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <exynos_display_common.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_connector.h>
#include <exynos_drm_dpp.h>
#include <exynos_drm_self_refresh.h>
#include <exynos_drm_vmc.h>
#include <exynos_drm_hwapg.h>

static int dpu_sr_log_level = 6;
module_param(dpu_sr_log_level, int, 0600);
MODULE_PARM_DESC(dpu_sr_log_level, "log level for self-refresh [default : 6]");

static bool dpu_esync_duration_check;
module_param(dpu_esync_duration_check, bool, 0600);
MODULE_PARM_DESC(dpu_esync_duration_check, "dpu esync duration check [default : false]");

#define SR_NAME "exynos-self-refresh"
#define sr_info(fmt, ...)	\
	dpu_pr_info(SR_NAME, 0, dpu_sr_log_level, fmt, ##__VA_ARGS__)

#define sr_warn(fmt, ...)	\
	dpu_pr_warn(SR_NAME, 0, dpu_sr_log_level, fmt, ##__VA_ARGS__)

#define sr_err(fmt, ...)	\
	dpu_pr_err(SR_NAME, 0, dpu_sr_log_level, fmt, ##__VA_ARGS__)

#define sr_debug(fmt, ...)	\
	dpu_pr_debug(SR_NAME, 0, dpu_sr_log_level, fmt, ##__VA_ARGS__)

#define IN_RANGE(val, min, max)                                 \
	(((min) > 0 && (min) < (max) &&                         \
	  (val) >= (min) && (val) <= (max)) ? true : false)

#define ESYNC_HZ	240LL
#define ESYNC_NS	(HZ2NS(ESYNC_HZ))
#define HALF_ESYNC_NS	(ESYNC_NS >> 1)
#define HALF_ESYNC_US	(HALF_ESYNC_NS / NSEC_PER_USEC)
#define MIN_ESYNC	(ESYNC_NS - HALF_ESYNC_NS)
#define MAX_ESYNC	(ESYNC_NS + HALF_ESYNC_NS)

#define MIN_FRAME_INTERVAL_NS	(HZ2NS(120LL) - HALF_ESYNC_NS)
#define MAX_FRAME_INTERVAL_NS	(HZ2NS(10LL) + HALF_ESYNC_NS)

enum {
	SYNC_TYPE_ESYNC		= BIT(0),
	SYNC_TYPE_FRAMEINTERVAL	= BIT(1),
	SYNC_TYPE_GRAMSCAN	= BIT(2),
	SYNC_TYPE_ALL_MASK	= SYNC_TYPE_ESYNC | SYNC_TYPE_FRAMEINTERVAL | SYNC_TYPE_GRAMSCAN,
	SYNC_TYPE_MAX		= BIT(3),
};

enum {
	WORK_TYPE_PANEL_REFRESH	= BIT(0),
	WORK_TYPE_BR_DIMMMING	= BIT(1),
	WORK_TYPE_ALL_MASK	= WORK_TYPE_PANEL_REFRESH | WORK_TYPE_BR_DIMMMING,
};

enum {
	DPU_SELF_VHM,
	DPU_SELF_VIDEO,
	DPU_SELF_COMMAND,
	DPU_SELF_UNDEFINED,
};

struct dvrr_config {
	u32 frame_interval_ns;
	u64 adjusted_present_time_ns;
	u64 expected_present_time_ns;
	bool need_panel_refresh;
};

struct exynos_self_refresh {
	int type;
	struct exynos_drm_crtc *exynos_crtc;
	spinlock_t slock;
	ktime_t last_esync_ns;
	ktime_t last_vsync_ns;
	ktime_t last_gramscan_ns;
	ktime_t next_vsync_ns;
	u32	next_vsync_ecnt;

	struct dvrr_config current_config;
	struct dvrr_config pended_config;
	struct {
		bool initialized;
		struct {
			struct drm_vblank_work work;
		} still_off;

		struct {
			struct drm_vblank_work work;
		} panel_refresh;

		struct {
			struct drm_vblank_work work;
		} dpu_refresh;

		struct {
			struct drm_vblank_work work;
			u32 remain_bq_cnt;
			u32 full_bq_cnt;
		} br_dimming;

		struct {
			struct drm_vblank_work get_work;
			struct drm_vblank_work put_work;
			bool en;
			bool trigger_en;
		} trans_dimming;
	} works;
	struct exynos_drm_connector *exynos_conn;
};

#define to_ctx(_work, member) \
	container_of((_work), struct exynos_self_refresh, member)

static inline bool is_odd_frame_interval(u32 frame_interval_ns)
{
	u32 curr_frame_interval_cnt;

	curr_frame_interval_cnt = DIV_ROUND_CLOSEST(frame_interval_ns, ESYNC_NS);

	return curr_frame_interval_cnt & 1;
}

static inline void __check_esync_dur(int sync_type, ktime_t later, ktime_t earlier)
{
	ktime_t dur_ns;

	if (sync_type & SYNC_TYPE_GRAMSCAN)
		return;

	dur_ns = ktime_sub(later, earlier);
	if (IN_RANGE(dur_ns, MIN_ESYNC, MAX_ESYNC))
		return;

	sr_err("invalid esync dur"KTIME_MSEC_FMT"\n", KTIME_MSEC_ARG(dur_ns));
}

static bool __update_dvrr_config(struct exynos_self_refresh *sr, ktime_t curr_ns)
{
	ktime_t adj_ept;
	ktime_t new_frame_interval_ns;

	assert_spin_locked(&sr->slock);

	/* there is no pended config */
	if (!sr->pended_config.frame_interval_ns)
		return false;

	new_frame_interval_ns = sr->pended_config.frame_interval_ns;

	/* not yet reach to adj_ept */
	adj_ept = sr->pended_config.adjusted_present_time_ns;
	if (adj_ept && adj_ept > curr_ns) {
		sr_debug("return adj_ept_ns"KTIME_SEC_FMT" > curr_ns"KTIME_SEC_FMT"\n",
			KTIME_SEC_ARG(adj_ept), KTIME_SEC_ARG(curr_ns));
		return false;
	}

	sr->current_config = sr->pended_config;
	memset(&sr->pended_config, 0, sizeof(struct dvrr_config));

	sr_debug("curr"KTIME_SEC_FMT" adj_ept"KTIME_SEC_FMT"\n",
		KTIME_SEC_ARG(curr_ns), KTIME_SEC_ARG(adj_ept));
	sr_debug("frame_interval"KTIME_MSEC_FMT"\n", KTIME_MSEC_ARG(new_frame_interval_ns));
	DPU_EVENT_LOG("SELF_REFRESH_CONFIG", sr->exynos_crtc, 0,
			"adj_ept"KTIME_SEC_FMT" frame_interval"KTIME_MSEC_FMT,
			KTIME_SEC_ARG(adj_ept), KTIME_MSEC_ARG(new_frame_interval_ns));

	return true;
}

static inline bool __is_esync(struct exynos_self_refresh *sr, ktime_t curr_ns)
{
	ktime_t dur_ns = ktime_sub(curr_ns, sr->last_esync_ns);

	return IN_RANGE(dur_ns, MIN_ESYNC, MAX_ESYNC);
}

static inline bool __is_frame_interval(struct exynos_self_refresh *sr, ktime_t curr_ns)
{
	struct dvrr_config *config = &sr->current_config;
	ktime_t frame_interval_ns = config->frame_interval_ns;
	ktime_t dur_ns = ktime_sub(curr_ns, sr->last_vsync_ns);

	assert_spin_locked(&sr->slock);

	/* if EPT time for dvrr config is expires, this sync is vsync */
	if (__update_dvrr_config(sr, curr_ns))
		return true;

	/* if currunt frame interval time expires, this sync is vsync */
	if (IN_RANGE(dur_ns, frame_interval_ns - HALF_ESYNC_NS, frame_interval_ns + HALF_ESYNC_NS))
		return true;

	if (dur_ns > frame_interval_ns) {
		sr_debug("duration"KTIME_MSEC_FMT" is over frame interval"KTIME_MSEC_FMT"\n",
				KTIME_MSEC_ARG(dur_ns), KTIME_MSEC_ARG(frame_interval_ns));
		return true;
	}

	return false;
}

static inline int __get_sync_type(struct exynos_self_refresh *sr, ktime_t curr_ns)
{
	int sync_type;

	assert_spin_locked(&sr->slock);

	sync_type = __is_esync(sr, curr_ns) ? SYNC_TYPE_ESYNC : SYNC_TYPE_GRAMSCAN;
	if (__is_frame_interval(sr, curr_ns))
		sync_type |= SYNC_TYPE_FRAMEINTERVAL;

	return sync_type;
}

#define CLOSEST 0
#define ROUNDUP 1
static inline u32 __get_remain_esync_cnt(ktime_t later_ns, ktime_t curr_ns, int flag)
{
	ktime_t remain_time_ns;
	u32 remain_esync_cnt;

	remain_time_ns = ktime_sub(later_ns, curr_ns);
	if (flag == CLOSEST)
		remain_esync_cnt = DIV_ROUND_CLOSEST(remain_time_ns, ESYNC_NS);
	else
		remain_esync_cnt = DIV_ROUND_UP(remain_time_ns, ESYNC_NS);

	return remain_esync_cnt;
}

#define MAX_STR_SZ (128)
static inline void __print_sync_timeinfo(struct exynos_self_refresh *sr, int sync_type,
					 ktime_t curr_ns)
{
	char buf[MAX_STR_SZ];
	unsigned int len = 0;
	ktime_t period_ns;

	assert_spin_locked(&sr->slock);

	if (sync_type & SYNC_TYPE_ESYNC) {
		period_ns = ktime_sub(curr_ns, sr->last_esync_ns);
		len += scnprintf(buf + len, MAX_STR_SZ - len,
				"ESYNC"KTIME_MSEC_FMT, KTIME_MSEC_ARG(period_ns));
	} else {
		period_ns = ktime_sub(curr_ns, sr->last_gramscan_ns);
		len += scnprintf(buf + len, MAX_STR_SZ - len,
				"GRAMSCAN"KTIME_MSEC_FMT, KTIME_MSEC_ARG(period_ns));
		period_ns = ktime_sub(curr_ns, sr->last_esync_ns);
		len += scnprintf(buf + len, MAX_STR_SZ - len,
				"DUR"KTIME_MSEC_FMT, KTIME_MSEC_ARG(period_ns));
		if (period_ns > ESYNC_NS * 2 + HALF_ESYNC_NS)
			sr_err("invalid GRAMSCAN dur"KTIME_MSEC_FMT"\n",
					KTIME_MSEC_ARG(period_ns));
	}

	if (sync_type & SYNC_TYPE_FRAMEINTERVAL) {
		period_ns = ktime_sub(curr_ns, sr->last_vsync_ns);
		len += scnprintf(buf + len, MAX_STR_SZ - len,
				" VSYNC"KTIME_MSEC_FMT, KTIME_MSEC_ARG(period_ns));
	}

	sr_info("%s\n", buf);
}

static inline void __update_sync_timestamp(struct exynos_self_refresh *sr, int sync_type,
					 ktime_t curr_ns)
{
	assert_spin_locked(&sr->slock);

	sr->last_esync_ns = curr_ns;
	if (sync_type & SYNC_TYPE_FRAMEINTERVAL)
		sr->last_vsync_ns = curr_ns;
	if (sync_type & SYNC_TYPE_GRAMSCAN)
		sr->last_gramscan_ns = curr_ns;

	sr->next_vsync_ns = sr->last_vsync_ns + sr->current_config.frame_interval_ns;
	sr->next_vsync_ecnt = __get_remain_esync_cnt(sr->next_vsync_ns, curr_ns, CLOSEST);
}

static int __crtc_vblank_work_schedule(struct drm_vblank_work *work,
			     struct drm_crtc *crtc, bool nextonmiss)
{
	struct drm_vblank_crtc *vblank = work->vblank;
	struct drm_device *dev = vblank->dev;
	unsigned long irqflags;
	bool inmodeset, rescheduling = false, wake = false;
	u64 count = drm_crtc_accurate_vblank_count(crtc);
	int ret = 0;

	spin_lock_irqsave(&dev->event_lock, irqflags);
	if (work->cancelling)
		goto out;

	spin_lock(&dev->vbl_lock);
	inmodeset = vblank->inmodeset;
	spin_unlock(&dev->vbl_lock);
	if (inmodeset)
		goto out;

	if (list_empty(&work->node)) {
		ret = drm_crtc_vblank_get(crtc);
		if (ret < 0)
			goto out;
	} else if (work->count == count) {
		/* Already scheduled w/ same vbl count */
		goto out;
	} else {
		rescheduling = true;
	}

	work->count = count;
	if (!nextonmiss) {
		drm_crtc_vblank_put(crtc);
		ret = kthread_queue_work(vblank->worker, &work->base);

		if (rescheduling) {
			list_del_init(&work->node);
			wake = true;
		}
	} else {
		if (!rescheduling)
			list_add_tail(&work->node, &vblank->pending_work);
		ret = true;
	}

out:
	spin_unlock_irqrestore(&dev->event_lock, irqflags);
	if (wake)
		wake_up_all(&vblank->work_wait_queue);
	return ret;
}

static void __still_off_work(struct kthread_work *work)
{
	struct drm_vblank_work *vblank_work = to_drm_vblank_work(work);
	struct exynos_self_refresh *sr = to_ctx(vblank_work, works.still_off.work);
	struct exynos_drm_crtc *exynos_crtc = sr->exynos_crtc;

	vmc_still_off(exynos_crtc->vmc, true);
}

#define WAIT_COMMIT_USEC	(DIV_ROUND_CLOSEST(HALF_ESYNC_US, 100L) * 100L)
static void __dpu_refresh_work(struct kthread_work *work)
{
	struct drm_vblank_work *vblank_work = to_drm_vblank_work(work);
	struct exynos_self_refresh *sr = to_ctx(vblank_work, works.dpu_refresh.work);
	struct exynos_drm_crtc *exynos_crtc = sr->exynos_crtc;

	if (!exynos_crtc || !exynos_crtc->ops || !exynos_crtc->ops->update_request)
		return;

	/*
	 * If there is a user commit requested before the midpoint
	 * of the esync interval, it will be processed.
	 * If not, the refresh work will be handled.
	 *
	 * esync: ---|----------------------------------------|---
	 *            <-- user commit -->|<-- refresh work -->
	 */
	usleep_range(WAIT_COMMIT_USEC, WAIT_COMMIT_USEC + 100L);

	spin_lock_irq(&sr->slock);
	if (is_vmc_still_blocked(exynos_crtc->vmc))
		goto out;

	hibernation_trig_reset(exynos_crtc->hibernation, NULL);

	exynos_crtc->ops->update_request(exynos_crtc);

out:
	spin_unlock_irq(&sr->slock);
}


/**
 *                              DPU self-refresh timing in VHM
 *
 *[EVEN frameinterval]
 *                                                                     Next frame interval
 *                                                                               ^
 * frame interval(60Hz):     |---------------------------------------------------|-------------
 * esync(240Hz):             |------------|------------|------------|------------|------------|
 * br_dimming_work:                       ^<-------------3_esync-----------------|
 * br_dimming_work:                                                 ^<--1_esync--|
 * DPU HW(120Hz):            |.//////////////////////..|.//////////////////////..|.////////////
 *
 *
 *
 *[ODD frameinterval]
 *                                                                          Next frame interval
 *                                                                                            ^
 * frame interval(48Hz):     |----------------------------------------------------------------|
 * esync(240Hz):             |------------|------------|------------|------------|------------|
 * br_dimming_work:                       ^<-------------------4_esync------------------------|
 * br_dimming_work:                                                              ^<--1_esync--|
 * DPU HW(120Hz):            |.//////////////////////..|.//////////////////////..|............|
 */
static inline bool __schedule_brightness_dimming_work(struct exynos_self_refresh *sr)
{
	struct dvrr_config *config;

	config = &sr->current_config;
	if (!config->frame_interval_ns || sr->works.br_dimming.remain_bq_cnt <= 0)
		return false;

	if (sr->next_vsync_ecnt == 1)
		goto need_dpu_refresh;

	if (sr->works.br_dimming.full_bq_cnt == sr->works.br_dimming.remain_bq_cnt)
		return false;

	if (is_odd_frame_interval(config->frame_interval_ns)) {
		if (sr->type == DPU_SELF_VHM) {
			if (!(sr->next_vsync_ecnt & 1) && sr->next_vsync_ecnt >= 4) {
				goto need_dpu_refresh;
			}
		} else if (sr->type == DPU_SELF_COMMAND) {
			if ((sr->next_vsync_ecnt & 1) && sr->next_vsync_ecnt >= 5)
				goto need_dpu_refresh;
		}
	} else {
		if (sr->type == DPU_SELF_VHM) {
			if (sr->next_vsync_ecnt & 1 && sr->next_vsync_ecnt >= 3)
				goto need_dpu_refresh;
		} else if (sr->type == DPU_SELF_COMMAND) {
			if (!(sr->next_vsync_ecnt & 1) && sr->next_vsync_ecnt >= 2)
				goto need_dpu_refresh;
		}
	}

	return false;

need_dpu_refresh:
	__crtc_vblank_work_schedule(&sr->works.br_dimming.work,
			&sr->exynos_crtc->base, false);
	return true;
}

static void __brightness_dimming_work(struct kthread_work *work)
{
	struct drm_vblank_work *vblank_work = to_drm_vblank_work(work);
	struct exynos_self_refresh *sr = to_ctx(vblank_work, works.br_dimming.work);
	const struct exynos_drm_connector_funcs *funcs;
	struct exynos_drm_connector *exynos_conn = sr->exynos_conn;
	struct exynos_drm_crtc *exynos_crtc = sr->exynos_crtc;

	if (!exynos_conn || !exynos_conn->funcs || !exynos_conn->funcs->update_brightness)
		return;

	if (sr->type == DPU_SELF_COMMAND) {
		hibernation_trig_reset(exynos_crtc->hibernation, NULL);
		/* delay for the panel command to be clearly reflected in the next esync. */
		udelay(1000);
	}
	funcs =	exynos_conn->funcs;
	sr->works.br_dimming.remain_bq_cnt = funcs->update_brightness(exynos_conn, false);
	sr_debug("need dimming remain_bq_cnt(%d)!!!\n", sr->works.br_dimming.remain_bq_cnt);
}

static void __panel_refresh_work(struct kthread_work *work)
{
	struct drm_vblank_work *vblank_work = to_drm_vblank_work(work);
	struct exynos_self_refresh *sr = to_ctx(vblank_work, works.panel_refresh.work);
	struct exynos_drm_connector *exynos_conn = sr->exynos_conn;

	if (!exynos_conn || !exynos_conn->funcs || !exynos_conn->funcs->update_panel)
		return;

	exynos_conn->funcs->update_panel(exynos_conn);
}

static inline void __schedule_panel_refresh_work(struct exynos_self_refresh *sr, ktime_t curr_ns)
{
	struct dvrr_config *config;
	u32 remain_esync_cnt;

	if (sr->type != DPU_SELF_COMMAND)
		return;

	config = &sr->pended_config;
	if (!config->expected_present_time_ns)
		return;

	if (!config->need_panel_refresh)
		return;

	remain_esync_cnt = __get_remain_esync_cnt(config->expected_present_time_ns, curr_ns,
			CLOSEST);
	/* Panel refresh should be triggered to avoid collision at EPT */
	if (remain_esync_cnt == 4 ||
	/* Panel refresh should be triggered, since there can be no frame updates for EPT */
			remain_esync_cnt == 2 || remain_esync_cnt == 1) {
		/* delay for the panel command to be clearly reflected in the next esync. */
		udelay(1000);
		__crtc_vblank_work_schedule(&sr->works.panel_refresh.work,
				&sr->exynos_crtc->base,	false);
		sr_debug("remain esync cnt(%d) panel update trigger", remain_esync_cnt);
	}
}

static void __get_trigger_mask_work(struct kthread_work *work)
{
	struct drm_vblank_work *vblank_work = to_drm_vblank_work(work);
	struct exynos_self_refresh *sr = to_ctx(vblank_work, works.trans_dimming.get_work);
	struct exynos_drm_crtc *exynos_crtc = sr->exynos_crtc;
	const struct exynos_drm_crtc_ops *ops = exynos_crtc->ops;
	struct decon_device *decon = exynos_crtc->ctx;
	int fid;

	if (sr->works.trans_dimming.trigger_en == true || !ops->get_trigger_mask) {
		sr_debug("already triiger en(%d)\n", sr->works.trans_dimming.trigger_en);
		return;
	}

	/* NOTE: currently block all DPUF regardless of usage */
	for (fid = 0; fid < MAX_DPUF_CNT; fid++) {
		struct dpuf *dpuf_blk = get_dpuf_blkdata(fid);
		struct exynos_hwapg_context *hwapg_ctx =
			dpuf_blk->hwapg_ctx;
		if (hwapg_ctx) {
			const struct exynos_hwapg_ops *ops =
				hwapg_ctx->ops;
			ops->block(hwapg_ctx, sr_to_hwapg_src(decon->id));
		}
	}
	ops->get_trigger_mask(exynos_crtc);
	sr->works.trans_dimming.trigger_en = true;
}

static void __put_trigger_mask_work(struct kthread_work *work)
{
	struct drm_vblank_work *vblank_work = to_drm_vblank_work(work);
	struct exynos_self_refresh *sr = to_ctx(vblank_work, works.trans_dimming.put_work);
	struct exynos_drm_crtc *exynos_crtc = sr->exynos_crtc;
	const struct exynos_drm_crtc_ops *ops = exynos_crtc->ops;
	struct decon_device *decon = exynos_crtc->ctx;
	int fid;

	if (sr->works.trans_dimming.trigger_en == false || !ops->put_trigger_mask) {
		sr_debug("already triiger en(%d)\n", sr->works.trans_dimming.trigger_en);
		return;
	}

	ops->put_trigger_mask(exynos_crtc);

	/* NOTE: currently unblock all DPUF regardless of usage */
	for (fid = 0; fid < MAX_DPUF_CNT; fid++) {
		struct dpuf *dpuf_blk = get_dpuf_blkdata(fid);
		struct exynos_hwapg_context *hwapg_ctx =
			dpuf_blk->hwapg_ctx;
		if (hwapg_ctx && hwapg_ctx->enable) {
			const struct exynos_hwapg_ops *ops =
				hwapg_ctx->ops;
			ops->unblock(hwapg_ctx, sr_to_hwapg_src(decon->id));
		}
	}
	sr->works.trans_dimming.trigger_en = false;
}

static inline bool __schedule_trans_dimming_work(struct exynos_self_refresh *sr, ktime_t curr_ns)
{
	struct dvrr_config *config;

	config = &sr->current_config;
	if (!config->frame_interval_ns)
		return false;

	if (!sr->works.trans_dimming.en)
		return false;

	if (sr->next_vsync_ecnt == 1)
		return true;

	if (is_odd_frame_interval(config->frame_interval_ns)) {
		if (!(sr->next_vsync_ecnt & 1) && sr->next_vsync_ecnt >= 4)
			return true;
	} else {
		if (sr->next_vsync_ecnt & 1 && sr->next_vsync_ecnt >= 3)
			return true;
	}

	return false;
}

static void __vblank_work_init(struct drm_vblank_work *work, struct drm_crtc *crtc,
			  void (*func)(struct kthread_work *work))
{
	kthread_init_work(&work->base, func);
	INIT_LIST_HEAD(&work->node);
	work->vblank = &crtc->dev->vblank[drm_crtc_index(crtc)];
}

static inline void __init_works(struct exynos_self_refresh *sr)
{
	if (likely(sr->works.initialized))
		return;

	__vblank_work_init(&sr->works.still_off.work, &sr->exynos_crtc->base,
			__still_off_work);
	__vblank_work_init(&sr->works.panel_refresh.work, &sr->exynos_crtc->base,
			__panel_refresh_work);
	__vblank_work_init(&sr->works.dpu_refresh.work, &sr->exynos_crtc->base,
			__dpu_refresh_work);
	__vblank_work_init(&sr->works.br_dimming.work, &sr->exynos_crtc->base,
			__brightness_dimming_work);
	__vblank_work_init(&sr->works.trans_dimming.get_work, &sr->exynos_crtc->base,
			__get_trigger_mask_work);
	__vblank_work_init(&sr->works.trans_dimming.put_work, &sr->exynos_crtc->base,
			__put_trigger_mask_work);
	sr->works.initialized = true;
}

static int __get_sr_type(const struct drm_atomic_state *state,
                         const struct exynos_display_mode *exynos_mode)
{
	if (!exynos_mode)
		return DPU_SELF_UNDEFINED;
	if (exynos_mode->vhm)
		return DPU_SELF_VHM;
	if (exynos_mode->mode_flags & MIPI_DSI_MODE_VIDEO)
		return DPU_SELF_VIDEO;

	return DPU_SELF_COMMAND;
}

int exynos_self_refresh_atomic_check(void *ctx, struct drm_atomic_state *state)
{
	struct exynos_self_refresh *sr = ctx;
	struct exynos_drm_crtc_state *new_exynos_crtc_state;
	ktime_t frame_interval_ns;
	u32 esync_cnt;

	if (!sr)
		return 0;

	new_exynos_crtc_state = exynos_drm_atomic_get_new_crtc_state(state, sr->exynos_crtc);
	if (!new_exynos_crtc_state->frame_interval_ns)
		return 0;

	frame_interval_ns = new_exynos_crtc_state->frame_interval_ns;
	if (!IN_RANGE(frame_interval_ns, MIN_FRAME_INTERVAL_NS,
				MAX_FRAME_INTERVAL_NS)) {
		sr_err("invalid frame_interval"KTIME_MSEC_FMT" min"KTIME_MSEC_FMT" max"KTIME_MSEC_FMT"\n",
				KTIME_MSEC_ARG(frame_interval_ns),
				KTIME_MSEC_ARG(MIN_FRAME_INTERVAL_NS),
				KTIME_MSEC_ARG(MAX_FRAME_INTERVAL_NS));
		new_exynos_crtc_state->frame_interval_ns = 0;
	}

	esync_cnt = DIV_ROUND_CLOSEST(frame_interval_ns, ESYNC_NS);
	new_exynos_crtc_state->frame_interval_ns = esync_cnt * ESYNC_NS;

	sr_debug("frameinterval"KTIME_MSEC_FMT" adjusted to"KTIME_MSEC_FMT"\n",
		KTIME_MSEC_ARG(frame_interval_ns), KTIME_MSEC_ARG(esync_cnt * ESYNC_NS));

	return 0;
}

void exynos_self_refresh_atomic_queue_dvrr_config(void *ctx, struct drm_atomic_state *old_state)
{
	struct exynos_self_refresh *sr = ctx;
	struct drm_crtc_state *new_crtc_state;
	struct exynos_drm_crtc_state *new_exynos_crtc_state;
	bool in_notifycall;

	if (!sr)
		return;

	new_crtc_state = drm_atomic_get_new_crtc_state(old_state, &sr->exynos_crtc->base);
	new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
	in_notifycall = new_exynos_crtc_state->skip_hw_update;

	spin_lock_irq(&sr->slock);
	sr->exynos_conn = crtc_get_exynos_conn(new_crtc_state, DRM_MODE_CONNECTOR_DSI);
	sr->type = __get_sr_type(old_state, &new_exynos_crtc_state->exynos_mode);

	__init_works(sr);
	sr->pended_config.frame_interval_ns = new_exynos_crtc_state->frame_interval_ns;
	sr->pended_config.adjusted_present_time_ns =
		new_exynos_crtc_state->adjusted_present_time_ns;
	sr->pended_config.expected_present_time_ns =
		new_exynos_crtc_state->expected_present_time_ns;

	/* Panel refresh will be needed, only if dvrr config updated by notifycall */
	sr->pended_config.need_panel_refresh = in_notifycall ? true : false;
	if (in_notifycall) {
#if 0
		struct dvrr_config *config = &sr->pended_config;
		ktime_t curr_ns = ktime_get();
		u32 remain_esync_cnt = __get_remain_esync_cnt(config->expected_present_time_ns,
				curr_ns, ROUNDUP);

		__crtc_vblank_work_schedule(&sr->works.still_off.work,
				&sr->exynos_crtc->base,	false);
		sr_debug("remain_esync_cnt(%d) ept"KTIME_SEC_FMT" curr"KTIME_SEC_FMT"\n",
				remain_esync_cnt, KTIME_SEC_ARG(config->expected_present_time_ns),
				KTIME_SEC_ARG(curr_ns));
#endif
	}
	spin_unlock_irq(&sr->slock);
}

void exynos_self_refresh_update_esync(void *ctx)
{
	struct exynos_self_refresh *sr = ctx;
	ktime_t curr_ns;
	int sync_type;
	bool need_dpu_refresh = false;

	if (!sr)
		return;

	curr_ns = ktime_get();
	spin_lock(&sr->slock);
	if (sr->type != DPU_SELF_VHM && sr->type != DPU_SELF_COMMAND)
		goto out;

	if (sr->last_vsync_ns == 0) {
		__update_sync_timestamp(sr, SYNC_TYPE_ALL_MASK, curr_ns);
		goto out;
	}

	sync_type = __get_sync_type(sr, curr_ns);
	if (sync_type & SYNC_TYPE_FRAMEINTERVAL)
		DPU_ATRACE_TOGGLE("DPU FRAME INTERVAL", sr->exynos_crtc->thread->pid);
	if (unlikely(dpu_esync_duration_check))
		__print_sync_timeinfo(sr, sync_type, curr_ns);
	__update_sync_timestamp(sr, sync_type, curr_ns);

	__schedule_panel_refresh_work(sr, curr_ns);
	need_dpu_refresh |= __schedule_brightness_dimming_work(sr);
	need_dpu_refresh |= __schedule_trans_dimming_work(sr, curr_ns);
	if (need_dpu_refresh && sr->type == DPU_SELF_VHM) {
		__crtc_vblank_work_schedule(&sr->works.dpu_refresh.work,
				&sr->exynos_crtc->base, false);
	}
	sr_debug("curr"KTIME_SEC_FMT" next vsync"KTIME_SEC_FMT"\n",
			KTIME_SEC_ARG(sr->last_esync_ns), KTIME_SEC_ARG(sr->next_vsync_ns));
	sr_debug("remain esync cnt(%d), time"KTIME_MSEC_FMT"\n",
			sr->next_vsync_ecnt, KTIME_MSEC_ARG(ktime_sub(sr->next_vsync_ns,
					sr->last_esync_ns)));

out:
	spin_unlock(&sr->slock);

	return;
}

void *exynos_self_refresh_register(struct exynos_drm_crtc *exynos_crtc)
{
	struct decon_device *decon = exynos_crtc->ctx;
	struct device_node *np = decon->dev->of_node;
	struct exynos_self_refresh *sr;
	int ret, val;

	ret = of_property_read_u32(np, "self-refresh", &val);
	/* if ret == -EINVAL, property is not existed */
	if (ret == -EINVAL || (ret == 0 && val == 0)) {
		sr_info("dpu self refresh is not supported\n");
		return NULL;
	}

	sr = devm_kzalloc(decon->dev, sizeof(struct exynos_self_refresh), GFP_KERNEL);
	if (!sr)
		return NULL;

	sr->exynos_crtc = exynos_crtc;
	spin_lock_init(&sr->slock);
	sr->current_config.frame_interval_ns = ESYNC_NS;
	sr->type = DPU_SELF_UNDEFINED;

	sr_info("dpu self refresh is supported\n");

	return sr;
}

void exynos_self_refresh_atomic_clear_sync(void *ctx)
{
	struct exynos_self_refresh *sr = ctx;
	struct exynos_drm_crtc_state *new_exynos_crtc_state;

	if (!sr)
		return;

	new_exynos_crtc_state =	to_exynos_crtc_state(sr->exynos_crtc->base.state);
	new_exynos_crtc_state->frame_interval_ns = 0;

	spin_lock_irq(&sr->slock);
	if (sr->type != DPU_SELF_VHM && sr->type != DPU_SELF_COMMAND)
		goto out;

	__update_sync_timestamp(sr, SYNC_TYPE_ALL_MASK, 0);
	memset(&sr->current_config, 0, sizeof(struct dvrr_config));
	memset(&sr->pended_config, 0, sizeof(struct dvrr_config));
	sr->current_config.frame_interval_ns = ESYNC_NS;
out:
	spin_unlock_irq(&sr->slock);
}

void exynos_self_refresh_set_dimming_cnt(void *ctx, u32 remain_bq_cnt, u32 full_bq_cnt)
{
	struct exynos_self_refresh *sr = ctx;

	if (!sr)
		return;

	if (sr->type != DPU_SELF_VHM && sr->type != DPU_SELF_COMMAND)
		return;

	if (!remain_bq_cnt)
		return;

	sr_debug("full_bq_cnt:%u remain_bq_cnt:%u\n", full_bq_cnt, remain_bq_cnt);

	sr->works.br_dimming.remain_bq_cnt = remain_bq_cnt;
	sr->works.br_dimming.full_bq_cnt = full_bq_cnt;
}
EXPORT_SYMBOL(exynos_self_refresh_set_dimming_cnt);

void exynos_self_refresh_set_trans_dimming(void *ctx, enum trans_dimming_type type, bool en)
{
	struct exynos_self_refresh *sr = ctx;
	struct decon_device *decon;
	struct dvrr_config *config;

	if (!sr)
		return;

	if (sr->type >= DPU_SELF_UNDEFINED)
		return;

	config = &sr->current_config;
	if (!config->frame_interval_ns)
		return;

	decon = sr->exynos_crtc->ctx;
	if (en) {
		decon->dimming |= type;
		if (sr->type == DPU_SELF_COMMAND)
			__crtc_vblank_work_schedule(&sr->works.trans_dimming.get_work,
					&sr->exynos_crtc->base, false);
		sr->works.trans_dimming.en |= type;
	} else {
		decon->dimming &= ~type;
		if (sr->type == DPU_SELF_COMMAND)
			__crtc_vblank_work_schedule(&sr->works.trans_dimming.put_work,
					&sr->exynos_crtc->base, false);
		sr->works.trans_dimming.en &= ~type;
	}
}
EXPORT_SYMBOL(exynos_self_refresh_set_trans_dimming);
