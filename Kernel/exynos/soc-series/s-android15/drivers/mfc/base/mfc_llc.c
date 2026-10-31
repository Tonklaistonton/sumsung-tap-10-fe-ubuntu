/*
 * drivers/media/platform/exynos/mfc/base/mfc_llc.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#if IS_ENABLED(CONFIG_MFC_USES_LLC)

#include <linux/module.h>
#include <soc/samsung/exynos-sci.h>

#include "mfc_llc.h"

static void __mfc_llc_alloc(struct mfc_core *core, enum exynos_sci_llc_region_index region,
		int enable, int way)
{
	unsigned int region_index = region + core->id;
	/* region_index, enable */
	llc_qpd_on(region_index, enable);
	/* region_index, enable, cache way */
	llc_region_alloc(region_index, enable, way);

	/* 1way = 1MB */
	mfc_core_debug(2, "[LLC] %s %s %dMB\n", enable ? "alloc" : "free",
			region == LLC_REGION_MFC0_INT ? "Internal" : "DPB", way);
	MFC_TRACE_CORE("[LLC] %s %s %dMB\n", enable ? "alloc" : "free",
			region == LLC_REGION_MFC0_INT ? "Internal" : "DPB", way);
}

void __mfc_llc_init(struct mfc_core *core)
{
	int llcaid_idx, sfr_idx;
	for (llcaid_idx = 0; llcaid_idx < core->llcaid_cnt; llcaid_idx++) {
		for (sfr_idx = 0; sfr_idx < core->llcaid_info[llcaid_idx].num_sfrs; sfr_idx++) {
			writel(core->llcaid_info[llcaid_idx].sfrs[sfr_idx].data,
				core->llcaid_info[llcaid_idx].base +
				core->llcaid_info[llcaid_idx].sfrs[sfr_idx].offset);
		}
	}
}

void mfc_llc_enable(struct mfc_core *core)
{
	/* default 1way(1MB) per region */
	int way = 1;

	mfc_core_debug_enter();

	if (core->dev->debugfs.llc_disable)
		return;

	__mfc_llc_init(core);

	__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 1, way);
	if (!core->llc_int_only)
		__mfc_llc_alloc(core, LLC_REGION_MFC0_DPB, 1, way);

	core->llc_on_status = 1;
	mfc_core_info("[LLC] enabled\n");
	MFC_TRACE_CORE("[LLC] enabled\n");

	mfc_core_debug_leave();
}

void mfc_llc_disable(struct mfc_core *core)
{
	mfc_core_debug_enter();

	__mfc_llc_alloc(core, LLC_REGION_MFC0_INT, 0, 0);
	if (!core->llc_int_only)
		__mfc_llc_alloc(core, LLC_REGION_MFC0_DPB, 0, 0);

	core->llc_on_status = 0;
	core->llc_int_only = 0;

	mfc_core_info("[LLC] disabled\n");
	MFC_TRACE_CORE("[LLC] disabled\n");

	mfc_core_debug_leave();
}

void mfc_llc_flush(struct mfc_core *core)
{
	mfc_core_debug_enter();

	if (core->dev->debugfs.llc_disable)
		return;

	if (!core->need_llc_flush)
		return;

	llc_flush(LLC_REGION_MFC0_INT + core->id);
	llc_flush(LLC_REGION_MFC0_DPB + core->id);

	mfc_core_debug(2, "[LLC] flushed\n");
	MFC_TRACE_CORE("[LLC] flushed\n");

	mfc_core_debug_leave();
}

void mfc_llc_handle_resol(struct mfc_core *core, struct mfc_ctx *ctx)
{
	if (!core->llc_on_status)
		mfc_llc_enable(core);

	if (core->num_inst == 1) {
		if (UNDER_1080P_RES(ctx)) {
			mfc_ctx_debug(2, "[LLC] disable LLC for under FHD (%dx%d)\n",
					ctx->crop_width, ctx->crop_height);
			mfc_llc_disable(core);
		} else if ((ctx->type == MFCINST_ENCODER) &&
				(!IS_MFC_HEAVY_PERF(ctx, ctx->operating_framerate / 1000))) {
			mfc_ctx_debug(2, "[LLC] disable LLC(MFC/D) for ENC under UHD60 (%dx%d, %lufps)\n",
					ctx->crop_width, ctx->crop_height, ctx->operating_framerate / 1000);
			mfc_llc_disable(core);
			core->llc_int_only = 1;
			mfc_llc_enable(core);
		}
	}
}
#endif
