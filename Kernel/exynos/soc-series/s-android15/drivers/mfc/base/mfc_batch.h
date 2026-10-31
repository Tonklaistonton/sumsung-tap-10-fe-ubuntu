/*
 * drivers/media/platform/exynos/mfc/mfc_batch.h
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_BATCH_H
#define __MFC_BATCH_H __FILE__

#include "mfc_common.h"
#include "mfc_rate_calculate.h"

/* The number of batch frame */
#define MFC_DEFAULT_BATCH_NUM	4

#define IS_BATCH(core)  ((core->nal_q_mode == MFC_NAL_Q_BATCH) ||   \
                        (core->nal_q_mode == MFC_NAL_Q_BATCH_HWAPG))
#define IS_HWAPG(core)	(core->nal_q_mode == MFC_NAL_Q_BATCH_HWAPG)
#define IS_DPB(core)    ((core->nal_q_mode == MFC_NAL_Q_DPB) ||     \
                        (core->nal_q_mode == MFC_NAL_Q_LL_DPB))
#define IS_LL(core)     ((core->nal_q_mode == MFC_NAL_Q_LL) ||     \
                        (core->nal_q_mode == MFC_NAL_Q_LL_DPB))


void mfc_batch_delete_timer(struct mfc_ctx *ctx);
void mfc_batch_update(struct mfc_ctx *ctx);
void mfc_batch_disable(struct mfc_core *core);
void mfc_batch_timeout(struct timer_list *t);

int mfc_batch_dec_ctx_ready_set_bit(struct mfc_core_ctx *core_ctx);

int mfc_batch_mode_check(struct mfc_core *core, struct mfc_ctx *ctx);
#endif /* __MFC_BATCH_H */