// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_hwapg.c
 *
 * Copyright (c) 2025 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/io.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <exynos_drm_hwapg.h>
#include <exynos_drm_decon.h>
#include <soc/samsung/cal-if.h>

static struct exynos_hwapg_context *hwapg_ctx_ptr[DPU_HWAPG_DEST_MAX];
static DEFINE_SPINLOCK(shared_apg_lock);

static inline bool is_valid_dest(enum dpu_hwapg_dest dest)
{
	if (dest >= DPU_HWAPG_DEST_MAX) {
		pr_err("%s, invalid target(%d)\n", __func__, (uint32_t)dest);
		return false;
	}

	return true;
}

static inline bool is_valid_source(enum dpu_hwapg_src src)
{
	if (src >= DPU_HWAPG_SOURCE_MAX) {
		pr_err("%s, invalid source(%d)\n", __func__, (uint32_t)src);
		return false;
	}

	return true;
}

static inline u32 src_to_decon_id(enum dpu_hwapg_src src)
{
	return src > DPU_HWAPG_SOURCE_DECON3 ? 0 : (u32)src;
}

static void mask_apg_bycmu(enum dpu_hwapg_dest dest, bool en)
{
#if (DECON_VERSION_INFO == 0x07040000)
	uint32_t mask = (dest == DPU_HWAPG_DEST_DPUF1) ? BIT(22) : BIT(21);
	uint32_t val = en ? ~0 : 0;
	cal_set_forced_enable_0_ewr_cmu(mask, val);
#endif
}

#define CMU_DPUF_FORCED_EWR_ADDR 0x2A0A0A08
uint32_t cmu_get_apg_mask(enum dpu_hwapg_dest dest)
{
#if (DECON_VERSION_INFO == 0x07040000)
	static void __iomem *cmu_top_forced_ewr_addr = NULL;
	uint32_t mask = (dest == DPU_HWAPG_DEST_DPUF1) ? BIT(22) : BIT(21);
	uint32_t val;

	if (cmu_top_forced_ewr_addr == NULL)
		cmu_top_forced_ewr_addr =
			ioremap(CMU_DPUF_FORCED_EWR_ADDR, 0x8);

	val = readl(cmu_top_forced_ewr_addr);

	return val & mask;
#else
	return 0;
#endif
}

static void exynos_hwapg_block(struct exynos_hwapg_context *ctx,
			       enum dpu_hwapg_src src)
{
	struct exynos_hwapg_ctrl *ctrl = &ctx->ctrl;

	if (!is_valid_source(src))
		return;

	if (!ctx->enable)
		return;

	spin_lock(ctrl->slock);
	if (ctrl->src_mask == 0) {
		struct decon_device *decon =
			get_decon_drvdata(src_to_decon_id(src));
		struct exynos_drm_crtc *exynos_crtc = decon->crtc;
		mask_apg_bycmu(ctx->dest, true);
		DPU_EVENT_LOG(
			"APG_BLOCK", exynos_crtc, 0, "DST:%d, MASK:0x%08x(%s)",
			ctx->dest, ctrl->src_mask | (uint32_t)BIT(src),
			cmu_get_apg_mask(ctx->dest) ? "BLOCKED" : "UNBLOCKED");
	}
	ctrl->src_mask |= BIT(src);
	pr_debug("%s, src:%d, dst:%d, mask:0x%08x from [%ps]\n", __func__,
		(int)src, (int)ctx->dest, ctrl->src_mask,
		__builtin_return_address(0));
	spin_unlock(ctrl->slock);
}

static void exynos_hwapg_unblock(struct exynos_hwapg_context *ctx,
				 enum dpu_hwapg_src src)
{
	struct exynos_hwapg_ctrl *ctrl = &ctx->ctrl;

	if (!is_valid_source(src))
		return;

	if (!ctx->enable)
		return;

	spin_lock(ctrl->slock);

	if (ctrl->src_mask == 0)
		goto out;

	ctrl->src_mask &= ~BIT(src);
	if (ctrl->src_mask == 0) {
		struct decon_device *decon =
			get_decon_drvdata(src_to_decon_id(src));
		struct exynos_drm_crtc *exynos_crtc = decon->crtc;
		mask_apg_bycmu(ctx->dest, false);
		DPU_EVENT_LOG(
			"APG_UNBLOCK", exynos_crtc, 0,
			"DST:%d F0:0x%08x(%s) F1:0x%08x(%s)", ctx->dest,
			hwapg_ctx_ptr[0]->ctrl.src_mask,
			cmu_get_apg_mask(hwapg_ctx_ptr[0]->dest) ? "BLOCKED" :
								   "UNBLOCKED",
			hwapg_ctx_ptr[1]->ctrl.src_mask,
			cmu_get_apg_mask(hwapg_ctx_ptr[1]->dest) ? "BLOCKED" :
								   "UNBLOCKED");
	}
	pr_debug("%s, src:%d, dst:%d, mask:0x%08x, from [%ps]\n", __func__,
		 (int)src, (int)ctx->dest, ctrl->src_mask,
		 __builtin_return_address(0));

out:
	spin_unlock(ctrl->slock);
}

void exynos_hwapg_dump_status(void)
{
	int fid;

	pr_info("=== DPUF HWAPG status ===\n");

	spin_lock(&shared_apg_lock);
	for (fid = 0; fid < MAX_DPUF_CNT; fid++) {
		struct exynos_hwapg_context *ctx = hwapg_ctx_ptr[fid];
		if (ctx == NULL || !ctx->enable)
			continue;

		pr_info("DPUF%d - SRC_MASK:0x%08x, CMU STATUS:%s\n", fid,
			ctx->ctrl.src_mask,
			cmu_get_apg_mask(ctx->dest) ? "BLOCKED" : "UNBLOCKED");
	}
	spin_unlock(&shared_apg_lock);
}

static const struct exynos_hwapg_ops hwapg_ops = {
	.block = exynos_hwapg_block,
	.unblock = exynos_hwapg_unblock,
};
struct exynos_hwapg_context *
exynos_drm_get_hwapg_context(struct decon_device *decon,
			     enum dpu_hwapg_dest dest, bool enable)
{
	struct exynos_hwapg_context *hwapg_ctx;

	if (hwapg_ctx_ptr[dest])
		return hwapg_ctx_ptr[dest];

	hwapg_ctx = devm_kzalloc(
		decon->dev, sizeof(struct exynos_hwapg_context), GFP_KERNEL);
	if (!hwapg_ctx)
		return NULL;

	hwapg_ctx->ctrl.slock = &shared_apg_lock;
	hwapg_ctx->ops = &hwapg_ops;
	hwapg_ctx->dest = dest;
	hwapg_ctx->enable = enable;

	hwapg_ctx_ptr[dest] = hwapg_ctx;

	return hwapg_ctx;
}

void exynos_hwapg_unblock_all(enum dpu_hwapg_src src)
{
	int fid;

	for (fid = 0; fid < MAX_DPUF_CNT; fid++) {
		struct dpuf *dpuf_blk = get_dpuf_blkdata(fid);
		struct exynos_hwapg_context *hwapg_ctx = dpuf_blk->hwapg_ctx;
		if (hwapg_ctx && hwapg_ctx->enable) {
			const struct exynos_hwapg_ops *ops = hwapg_ctx->ops;
			ops->unblock(hwapg_ctx, src);
		}
	}
}

void exynos_hwapg_block_all(enum dpu_hwapg_src src)
{
	int fid;

	for (fid = 0; fid < MAX_DPUF_CNT; fid++) {
		struct dpuf *dpuf_blk = get_dpuf_blkdata(fid);
		struct exynos_hwapg_context *hwapg_ctx = dpuf_blk->hwapg_ctx;
		if (hwapg_ctx) {
			const struct exynos_hwapg_ops *ops = hwapg_ctx->ops;
			ops->block(hwapg_ctx, src);
		}
	}
}

void exynos_check_hwapg_blocked(void)
{
	int fid;

	spin_lock(&shared_apg_lock);
	for (fid = 0; fid < MAX_DPUF_CNT; fid++) {
		struct exynos_hwapg_context *ctx = hwapg_ctx_ptr[fid];
		if (ctx == NULL || !ctx->enable)
			continue;

		if (ctx->ctrl.src_mask && (!cmu_get_apg_mask(ctx->dest))) {
			pr_warn("%s, DPUF%d apg block state is unmatch(mask:0x%08x)\n",
				__func__, fid, ctx->ctrl.src_mask);
			mask_apg_bycmu(ctx->dest, true);
		}
	}
	spin_unlock(&shared_apg_lock);
}

