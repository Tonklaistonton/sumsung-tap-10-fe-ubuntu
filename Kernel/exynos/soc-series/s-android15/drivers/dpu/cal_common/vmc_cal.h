/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for SAMSUNG VMC CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_VMC_CAL_H__
#define __SAMSUNG_VMC_CAL_H__

#include <exynos_panel.h>

enum vmc_trig {
	VMC_TRIG_MASK = 0,
	VMC_TRIG_UNMASK
};

enum vmc_lfr_update {
	VMC_LFR_MASK = 0,
	VMC_LFR_UNMASK,
};

enum vmc_clk_sel {
	VMC_CLK_CLK1 = 0, // Main OSC
	VMC_CLK_CLK2, // RCO
};

struct vmc_regs {
	void __iomem *regs;
};

struct vmc_config {
	struct dpu_panel_timing p_timing;
	enum vmc_clk_sel clk_sel;
	u32 emission_num;
};

void vmc_regs_desc_init(void __iomem *regs, const char *name);
void vmc_reg_init(struct vmc_config *config);
void vmc_reg_switching_init(struct vmc_config *config);
u32 vmc_reg_get_interrupt_and_clear(void);
void vmc_reg_start(void);
void vmc_reg_stop(void);
u32 vmc_reg_get_vmc_en(void);
u32 vmc_reg_get_mask_on(void);
void vmc_reg_set_mask_on_force_up(u32 on);

void __vmc_pmu_dump(void __iomem *regs);
void __vmc_dump(void __iomem *regs);
#endif /* __SAMSUNG_VMC_CAL_H__ */
