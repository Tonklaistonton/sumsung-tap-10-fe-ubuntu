/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS Scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "scaler.h"

#define CREATE_TRACE_POINTS
#include "mscl_trace.h"

static void sc_show_crop(struct v4l2_rect *rect)
{
	print_sc(SC_INFO, "   - crop: L,T,W,H: %d, %d, %d, %d\n",
			rect->left, rect->top, rect->width, rect->height);
}

static void sc_show_addr(struct sc_addr *addr)
{
	int i;

	for (i = 0; i < SC_MAX_PLANES; i++)
		print_sc(SC_INFO, "  * plane%d. addr: %#lx, size: %#x\n",
			i, (unsigned long)addr->ioaddr[i], addr->size[i]);
}

static void sc_show_frame(char *name, struct sc_frame *frame)
{
	print_sc(SC_INFO, "  * %s) WxH : %d x %d\n", name, frame->width, frame->height);
	sc_show_crop(&frame->crop);
	sc_show_addr(&frame->addr);
	if (frame->sc_fmt)
		print_sc(SC_INFO, "  * Name : %s\n", frame->sc_fmt->name);

}

void sc_ctx_dump(struct sc_ctx *ctx)
{
	int i;
	print_sc(SC_INFO, "> scaler context info <\n");
	sc_show_frame("source", &ctx->s_frame);
	sc_show_frame("dest", &ctx->d_frame);
	print_sc(SC_INFO, "  - h_ratio: %#x, v_ratio: %#x\n", ctx->h_ratio, ctx->v_ratio);
	print_sc(SC_INFO, "  - flip_rot_cfg: %#x\n", ctx->flip_rot_cfg);
	print_sc(SC_INFO, "  - flags: %#lx\n", ctx->flags);
	print_sc(SC_INFO, "  - %s\n", ctx->cp_enabled ? "secure" : "non-secure");
	print_sc(SC_INFO, "  - src_buf memory %d", ctx->src_buf_info.memory);
	print_sc(SC_INFO, "  - src_buf plane_num %d", ctx->src_buf_info.plane_num);
	for (i = 0; i < ctx->src_buf_info.plane_num; i++)
		print_sc(SC_INFO, "  - planes[%d] length %d, ", i, ctx->src_buf_info.plane_len[i]);
	print_sc(SC_INFO, "  - dst_buf memory %d", ctx->dst_buf_info.memory);
	print_sc(SC_INFO, "  - dst_buf plane_num %d", ctx->dst_buf_info.plane_num);
	for (i = 0; i < ctx->dst_buf_info.plane_num; i++)
		print_sc(SC_INFO, "  - planes[%d] length %d, ", i, ctx->dst_buf_info.plane_len[i]);

	if (test_bit(CTX_INT_FRAME, &ctx->flags)) {
		int i;

		for (i = 0; i < ctx->num_int_frame; i++)
			sc_show_frame("Internal", &ctx->i_frame[i]->frame);
	}
}

void sc_tracing_mark_write(struct sc_ctx *ctx, char trace_id, const char *str,
			   int en)
{
	if (!ctx->pid)
		return;

	if (trace_id != 'B' && trace_id != 'E' && trace_id != 'C') {
		print_sc(SC_ERR,
			"%c is invalid arg for systrace\n", trace_id);
		return;
	}

	trace_tracing_mark_write(trace_id, ctx->pid, str, en);
}
