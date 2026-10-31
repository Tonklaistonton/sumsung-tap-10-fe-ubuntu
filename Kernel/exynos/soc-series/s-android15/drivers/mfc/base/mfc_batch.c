/*
 * drivers/media/platform/exynos/mfc/base/mfc_batch.c
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_rm.h"

#include "mfc_batch.h"
#include "mfc_queue.h"

void __mfc_batch_update_timer(struct mfc_core *core, struct mfc_ctx *ctx)
{
	if (!core->batch_enable || !ctx->batch_wait_time)
		return;
	mfc_ctx_debug(2, "[BATCH] src update timer\n");
	mod_timer(&ctx->batch_timer, jiffies +
			msecs_to_jiffies(ctx->batch_wait_time));
}

void __mfc_batch_delete_timer(struct mfc_ctx *ctx)
{
	if (timer_pending(&ctx->batch_timer)) {
		mfc_ctx_debug(2, "[BATCH] src delete timer\n");
		del_timer(&ctx->batch_timer);
	}
}

void __mfc_batch_update(struct mfc_core *core, struct mfc_ctx *ctx)
{
	unsigned int batch_num = MFC_DEFAULT_BATCH_NUM;

	if (!core->batch_enable) {
		mfc_ctx_debug(2, "[BATCH] Not enabled\n");
		return;
	}

	if (!ctx->batch_queue_cnt && !ctx->batch_timeout
			&& (!timer_pending(&ctx->batch_timer))) {
		if (core->dev->debugfs.batch_num)
			batch_num = core->dev->debugfs.batch_num;
		else if (UNDER_FHD_RES(ctx) && core->dev->pdata->batch_num)
			batch_num = core->dev->pdata->batch_num;
		ctx->batch_wait_time = (1000000 * batch_num) \
				/ mfc_rate_get_rt_framerate(ctx, ctx->rt);
		__mfc_batch_update_timer(core, ctx);

		mfc_ctx_debug(2, "[BATCH] wait time: %d ms\n", ctx->batch_wait_time);
	}
}

void __mfc_batch_disable(struct mfc_core *core)
{
	struct mfc_ctx *ctx = core->dev->ctx[core->batch_index];

	if (!core->batch_enable) {
		mfc_core_debug(2, "[BATCH] already disabled\n");
		return;
	}

	mfc_core_debug(2, "[BATCH] disable\n");
	MFC_TRACE_CORE("[BATCH] disable\n");

	if (ctx) {
		ctx->batch_queue_cnt = 0;
		ctx->batch_wait_time = 0;
		ctx->batch_timeout = 0;
		__mfc_batch_delete_timer(ctx);
	} else {
		mfc_core_err("[BATCH] there is no ctx(%d)\n", core->batch_index);
	}

	core->batch_enable = 0;
	core->batch_index = 0;
}

void __mfc_batch_request_work(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;
	struct mfc_core *core = mfc_get_main_core(dev, ctx);
	struct mfc_core_ctx *core_ctx = NULL;

	if (!core) {
		mfc_ctx_err("[BATCH] There is no core\n");
		return;
	}

	core_ctx = core->core_ctx[ctx->num];
	if (!core_ctx) {
		mfc_ctx_err("[BATCH] There is no core_ctx\n");
		return;
	}

	/* qos_on should not be called here */
	if (!ctx->boosting_time) {
		core->sched->clear_work(core, core_ctx);
		core->sched->enqueue_work(core, core_ctx);
	} else {
		core->sched->set_work(core, core_ctx);
	}
	if (core->sched->is_work(core))
		core->sched->queue_work(core);

	return;
}

void mfc_batch_delete_timer(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;
	struct mfc_core *core = mfc_get_main_core(dev, ctx);
	unsigned long flags;

	if (!core) {
		mfc_ctx_err("[BATCH] There is no core\n");
		return;
	}

	spin_lock_irqsave(&core->batch_lock, flags);
	__mfc_batch_delete_timer(ctx);
	spin_unlock_irqrestore(&core->batch_lock, flags);
}

void mfc_batch_update(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;
	struct mfc_core *core = mfc_get_main_core(dev, ctx);
	unsigned long flags;

	if (!core) {
		mfc_ctx_err("[BATCH] There is no core\n");
		return;
	}

	spin_lock_irqsave(&core->batch_lock, flags);
	__mfc_batch_update(core, ctx);
	spin_unlock_irqrestore(&core->batch_lock, flags);
}

void mfc_batch_disable(struct mfc_core *core)
{
	unsigned long flags;

	spin_lock_irqsave(&core->batch_lock, flags);
	if (core->batch_enable) {
		mfc_core_info("[BATCH] disable Batch mode\n");
		MFC_TRACE_CORE("[BATCH] disable\n");
		__mfc_batch_disable(core);
	}
	core->nal_q_mode = MFC_NAL_Q_DISABLE;
	spin_unlock_irqrestore(&core->batch_lock, flags);
}

void mfc_batch_timeout(struct timer_list *t)
{
	unsigned long flags;

	struct mfc_ctx *ctx = from_timer(ctx, t, batch_timer);
	struct mfc_dev *dev = ctx->dev;
	struct mfc_core *core = mfc_get_main_core(dev, ctx);

	if (!core) {
		mfc_ctx_err("[BATCH] There is no core\n");
		return;
	}

	spin_lock_irqsave(&core->batch_lock, flags);

	mfc_core_debug(2, "[BATCH] buffer timeout\n");
	ctx->batch_timeout = 1;

	spin_unlock_irqrestore(&core->batch_lock, flags);

	__mfc_batch_request_work(ctx);
}

int mfc_batch_dec_ctx_ready_set_bit(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_core *core = core_ctx->core;
	struct mfc_dev *dev = ctx->dev;
	nal_queue_handle *nal_q_handle;
	int ret = 0;
	unsigned long flags;
	unsigned int src_buf_cnt, dst_buf_cnt;
	unsigned int batch_num = MFC_DEFAULT_BATCH_NUM;

	if (!core) {
		mfc_ctx_err("[BATCH] There is no core\n");
		return ret;
	}
	nal_q_handle = core->nal_q_handle;

	spin_lock_irqsave(&core->batch_lock, flags);

	if (nal_q_handle->nal_q_state == NAL_Q_STATE_BATCH_STARTED) {
		mfc_ctx_debug(2, "[BATCH] HW is running.\n");
		spin_unlock_irqrestore(&core->batch_lock, flags);
		return ret;
	}

	src_buf_cnt = mfc_get_queue_count(&ctx->buf_queue_lock, &core_ctx->src_buf_queue);
	dst_buf_cnt = mfc_get_available_dpb_count(core_ctx);

	mfc_ctx_debug(2, "[BATCH] src= %d, dst= %d, queue_cnt = %d, timeout= %d\n",
			src_buf_cnt, dst_buf_cnt, ctx->batch_queue_cnt, ctx->batch_timeout);

	if (mfc_check_buf_mb_flag_all(ctx, core_ctx, MFC_FLAG_LAST_FRAME) == 1) {
		batch_num = 1;
		mfc_ctx_debug(2, "[BATCH] There is a last frame\n");
	} else if (mfc_rate_get_rt_framerate(ctx, ctx->rt) < 25000) {
		batch_num = 1;
		mfc_ctx_debug(2, "[BATCH] frame rate is too low\n");
	} else if (dev->debugfs.batch_num) {
		batch_num = dev->debugfs.batch_num;
		mfc_ctx_debug(2, "[BATCH] frame count is changed for test(%u)\n", batch_num);
	} else if (UNDER_FHD_RES(ctx) && dev->pdata->batch_num) {
		batch_num = dev->pdata->batch_num;
		mfc_ctx_debug(2, "[BATCH] under FHD, wait %u buffers\n", batch_num);
	}

	mfc_ctx_debug(2, "[BATCH] src: %d/%d, dst: %d/%d\n",
		src_buf_cnt, batch_num, dst_buf_cnt, batch_num);

	if (src_buf_cnt && dst_buf_cnt && (ctx->batch_timeout
			|| ((src_buf_cnt >= batch_num) && (dst_buf_cnt >= batch_num)))) {
		if (src_buf_cnt > dst_buf_cnt)
			ctx->batch_queue_cnt = dst_buf_cnt;
		else
			ctx->batch_queue_cnt = src_buf_cnt;

		ret = 1;

		mfc_ctx_debug(2, "[BATCH] batch queue count: %d(src:%d, dst:%d)\n",
			ctx->batch_queue_cnt, src_buf_cnt, dst_buf_cnt);

		__mfc_batch_delete_timer(ctx);
	} else {
		ctx->batch_queue_cnt = 0;
	}

	spin_unlock_irqrestore(&core->batch_lock, flags);

	return ret;
}

int mfc_batch_mode_check(struct mfc_core *core, struct mfc_ctx *ctx)
{
	struct mfc_ctx *ctx_temp, *ctx_batch = NULL;
	struct mfc_core_ctx *core_ctx_temp;
	unsigned long flags;
	int i, ctx_cnt = 0;
	int nal_q_mode;

	spin_lock_irqsave(&core->batch_lock, flags);

	for (i = 0; i < MFC_NUM_CONTEXTS; i++) {
		ctx_temp = core->dev->ctx[i];
		core_ctx_temp = core->core_ctx[i];
		if (core_ctx_temp && ctx_temp) {
			if (ctx_temp->idle_mode == MFC_IDLE_MODE_NONE) {
				ctx_cnt++;
				ctx_batch = ctx_temp;
				mfc_core_debug(2, "[BATCH] there is an active instance(ctx%d)\n", i);
			} else {
				mfc_core_debug(2, "[BATCH] there is an idle instance(ctx%d)\n", i);
			}
		}
	}

	if ((core->sched_type == MFC_SCHED_PRIO) &&	core->dev->pdata->nal_q_ll)
		nal_q_mode = MFC_NAL_Q_LL;
	else
		nal_q_mode = MFC_NAL_Q_DEFAULT;

	mfc_core_debug(2, "[BATCH] ctx_cnt: %d, ctx->num: %d\n", ctx_cnt, ctx->num);
	mfc_core_debug(2, "[BATCH] plugin: %d, type: %s, rt: %d, boost: %d, framerate: %d\n",
				ctx->plugin_type, ctx->type == MFCINST_DECODER ? "DEC" : "ENC",
				ctx->rt, ctx->boosting_time ? 1 : 0, mfc_rate_get_op_framerate(ctx));
	mfc_core_debug(2, "[BATCH] src ts full: %d, dst_q ts full: %d, ctrl disable: %d, debugfs disable: %d\n",
				ctx->src_ts.ts_is_full, ctx->dst_q_ts.ts_is_full,
				ctx->batch_disable, core->dev->debugfs.batch_disable);

	if (core->dev->debugfs.nal_q_mode) {
		nal_q_mode = core->dev->debugfs.nal_q_mode;
		mfc_core_info("[BATCH] set nal_q_mode for test : %d\n", nal_q_mode);
	} else if (ctx_cnt == 1 && ctx_batch && ctx_batch->num == ctx->num && !ctx->plugin_type
				&& ctx->type == MFCINST_DECODER && core->dev->pdata->nal_q_batch_dec
				&& ctx->rt != MFC_NON_RT && ctx->rt != MFC_RT_UNDEFINED && !ctx->ktime_used
				&& !ctx->boosting_time && ctx->src_ts.ts_is_full && ctx->dst_q_ts.ts_is_full
				&& !ctx->batch_disable && !core->dev->debugfs.batch_disable
				&& !ctx->dec_priv->immediate_display && ctx->max_framerate < 60000) {
		if (UNDER_FHD_RES(ctx) && (mfc_rate_get_op_framerate(ctx) < 60000)) {
			if (core->dev->pdata->support_hwapg && !core->dev->debugfs.hwapg_disable)
				nal_q_mode = MFC_NAL_Q_BATCH_HWAPG;
			else
				nal_q_mode = MFC_NAL_Q_BATCH;
			mfc_core_debug(2, "[BATCH] BATCH %s (under FHD 60fps)\n",
					nal_q_mode == MFC_NAL_Q_BATCH_HWAPG ? "+ HWAPG" : "only");
		}
	}

	core->nal_q_mode = nal_q_mode;
	mfc_core_debug(2, "[BATCH] nal_q_mode is : %d\n", nal_q_mode);

	if (!core->batch_enable && IS_BATCH(core)) {
		mfc_core_info("[BATCH] enable Batch mode\n");
		MFC_TRACE_CTX("[BATCH] enable\n");
		core->batch_enable = 1;
		core->batch_index = ctx->num;
		__mfc_batch_update(core, ctx);
	} else if (core->batch_enable && !IS_BATCH(core)) {
		mfc_core_info("[BATCH] disable Batch mode\n");
		MFC_TRACE_CTX("[BATCH] disable\n");
		__mfc_batch_disable(core);
	} else if (core->batch_enable && IS_BATCH(core)
			&& (ctx_batch->num != core->batch_index)) {
		mfc_core_info("[BATCH] change Batch ctx(%d->%d)\n",
				core->batch_index, ctx->num);
		MFC_TRACE_CTX("[BATCH] change\n");
		__mfc_batch_disable(core);
		core->batch_enable = 1;
		core->batch_index = ctx->num;
		__mfc_batch_update(core, ctx);
	}

	spin_unlock_irqrestore(&core->batch_lock, flags);

	if (IS_BATCH(core))
		return 1;
	else
		return 0;
}
