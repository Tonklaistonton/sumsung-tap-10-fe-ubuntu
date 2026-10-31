/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung EXYNOS CAMERA PostProcessing dof driver
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_HW_API_DOF_H
#define CAMERAPP_HW_API_DOF_H

#include "pablo-dof.h"
#include "pablo-hw-api-common.h"

/* format related */
#define DOF_CFG_FMT_GREY (0x1 << 0)

/* DOF is different with other HW IP */
#define DOF_MSB(x) ((x) >> 32)
#define DOF_LSB(x) ((x)&0xFFFFFFFF)

#define DOF_IS_MULTIPLE_OF_256(n) (((n)&0xFF) == 0)

void camerapp_hw_dof_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
void camerapp_hw_dof_stop(struct pablo_mmio *pmio);
void camerapp_hw_dof_sw_init(struct pablo_mmio *pmio);
u32 camerapp_hw_dof_dma_reset(struct pablo_mmio *pmio);
u32 camerapp_hw_dof_core_reset(struct pablo_mmio *pmio);
u32 camerapp_hw_dof_sw_reset(struct pablo_mmio *pmio);
int camerapp_hw_dof_update_param(struct pablo_mmio *pmio, struct dof_ctx *current_ctx);
void camerapp_hw_dof_status_read(struct pablo_mmio *pmio);
void camerapp_hw_dof_sfr_dump(struct pablo_mmio *pmio);
void camerapp_hw_dof_update_debug_info(struct pablo_mmio *pmio, struct dof_debug_info *debug_info,
	u32 buf_index, enum dof_debug_status status);
void camerapp_hw_dof_interrupt_disable(struct pablo_mmio *pmio);
void camerapp_hw_dof_clear_intr_all(struct pablo_mmio *pmio);
u32 camerapp_hw_dof_get_intr_status_and_clear(struct pablo_mmio *pmio);
u32 camerapp_hw_dof_get_int_frame_start(void);
u32 camerapp_hw_dof_get_int_frame_end(void);
u32 camerapp_hw_dof_get_int_err(void);
u32 camerapp_hw_dof_get_ver(struct pablo_mmio *pmio);
const struct dof_variant *camerapp_hw_dof_get_size_constraints(struct pablo_mmio *pmio);
void camerapp_hw_dof_init_pmio_config(struct dof_dev *dof);
void camerapp_hw_dof_print_dma_address(
	struct dof_frame *s_frame, struct dof_frame *d_frame, struct dof_model_addr *model_addr);
int camerapp_hw_dof_wait_idle(struct pablo_mmio *pmio);
void camerapp_hw_dof_set_initialization(struct pablo_mmio *pmio);
u32 camerapp_hw_dof_get_reg_cnt(void);
int camerapp_hw_dof_prepare(struct dof_dev *dof);

struct pablo_camerapp_hw_dof {
	u32 (*sw_reset)(struct pablo_mmio *pmio);
	int (*wait_idle)(struct pablo_mmio *pmio);
	void (*set_initialization)(struct pablo_mmio *pmio);
	int (*update_param)(struct pablo_mmio *pmio, struct dof_ctx *current_ctx);
	void (*start)(struct pablo_mmio *pmio, struct c_loader_buffer *clb);
	void (*sfr_dump)(struct pablo_mmio *pmio);
	int (*prepare)(struct dof_dev *dof);
};

struct pablo_camerapp_hw_dof *pablo_get_hw_dof_ops(void);

#endif
