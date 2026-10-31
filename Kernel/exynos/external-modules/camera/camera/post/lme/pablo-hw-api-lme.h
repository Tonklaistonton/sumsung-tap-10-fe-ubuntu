// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS CAMERA PostProcessing lme driver
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_HW_API_LME_H
#define CAMERAPP_HW_API_LME_H

#include "pablo-lme.h"
#include "pablo-hw-api-common.h"

/* format related */
#define LME_CFG_FMT_GREY (0x1 << 0)

void camerapp_hw_lme_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
void camerapp_hw_lme_stop(struct pablo_mmio *pmio);
void camerapp_hw_lme_sw_init(struct pablo_mmio *pmio);
int camerapp_hw_lme_dma_reset(struct pablo_mmio *pmio);
u32 camerapp_hw_lme_sw_reset(struct pablo_mmio *pmio);
int camerapp_hw_lme_update_param(
	struct pablo_mmio *pmio, struct lme_dev *lme, struct c_loader_buffer *clb);
void camerapp_hw_lme_status_read(struct pablo_mmio *pmio);
void camerapp_lme_sfr_dump(void __iomem *base_addr);
void camerapp_hw_lme_interrupt_disable(struct pablo_mmio *pmio);
u32 camerapp_hw_lme_get_intr_status_and_clear(struct pablo_mmio *pmio);
u32 camerapp_hw_lme_get_int_frame_start(void);
u32 camerapp_hw_lme_get_int_frame_end(void);
u32 camerapp_hw_lme_get_int_err(void);
u32 camerapp_hw_lme_get_ver(struct pablo_mmio *pmio);
const struct lme_variant *camerapp_hw_lme_get_size_constraints(struct pablo_mmio *pmio);
void camerapp_hw_lme_init_pmio_config(struct lme_dev *lme);
int camerapp_hw_lme_get_output_size(int width, int height, int *total_width, int *line_count,
				    enum lme_sps_mode sps_mode, enum lme_wdma_index type);
void camerapp_hw_lme_get_mbmv_size(int *width, int *height);
int camerapp_hw_lme_wait_idle(struct pablo_mmio *pmio);
void camerapp_hw_lme_set_initialization(struct pablo_mmio *pmio);
u32 camerapp_hw_lme_get_reg_cnt(void);
void camerapp_hw_lme_print_dma_address(
	struct lme_frame *s_frame, struct lme_frame *d_frame, struct lme_mbmv *mbmv);

struct pablo_camerapp_hw_lme {
	u32 (*sw_reset)(struct pablo_mmio *pmio);
	int (*wait_idle)(struct pablo_mmio *pmio);
	void (*set_initialization)(struct pablo_mmio *pmio);
	int (*update_param)(
		struct pablo_mmio *pmio, struct lme_dev *lme, struct c_loader_buffer *clb);
	void (*start)(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
	void (*sfr_dump)(void __iomem *base_addr);
};
struct pablo_camerapp_hw_lme *pablo_get_hw_lme_ops(void);

#endif
