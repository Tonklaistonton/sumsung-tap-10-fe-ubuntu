// SPDX-License-Identifier: GPL-2.0-only
/*
 * vmc_regs.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register access functions for Samsung EXYNOS Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <cal_config.h>
#include <vmc_cal.h>
#include <regs-vmc.h>

static struct cal_regs_desc regs_vmc;

#define vmc_read(offset)			\
	cal_read(&regs_vmc, offset)
#define vmc_write(offset, val)		\
	cal_write(&regs_vmc, offset, val)
#define vmc_read_mask(offset, mask)		\
	cal_read_mask(&regs_vmc, offset, mask)
#define vmc_write_mask(offset, val, mask)	\
	cal_write_mask(&regs_vmc, offset, val, mask)

#define MAIN_OSC_HZ	(76800000ULL)
#define RCO_HZ		(49200000ULL)

static void
vmc_reg_set_display_size_width(struct vmc_config *config)
{
	struct dpu_panel_timing *p_timing = &config->p_timing;
	u64 vclk,  vdo_clk;
	u64 hblank, vblank;
	/* hx_period: # of vdo_clk cycle */
	u64 hact_period, hsa_period, hbp_period, hfp_period;
	u64 htot_period;
	u32 hsync_wd0, hsync_wd1;
	u32 hsync_num;

	if (config->clk_sel == VMC_CLK_CLK1)
		vdo_clk = MAIN_OSC_HZ;
	else
		vdo_clk = RCO_HZ;

	hblank = p_timing->hsa + p_timing->hbp + p_timing->hfp;
	vblank = p_timing->vsa + p_timing->vbp + p_timing->vfp;

	vclk = (p_timing->hactive + hblank) * (p_timing->vactive + vblank) *
			p_timing->vrefresh;

	/* unit for horizontal is # of vdo_clk cycle */
	hact_period = DIV_ROUND_CLOSEST(p_timing->hactive * vdo_clk, vclk);
	hsa_period = DIV_ROUND_CLOSEST(p_timing->hsa * vdo_clk, vclk);
	hbp_period = DIV_ROUND_CLOSEST(p_timing->hbp * vdo_clk, vclk);
	hfp_period = DIV_ROUND_CLOSEST(p_timing->hfp * vdo_clk, vclk);

	vmc_write(DISP_RESOL, VRESOL(p_timing->vactive) | HRESOL(hact_period));
	vmc_write(DISP_HPORCH, HFP(hfp_period) | HBP(hbp_period - hsa_period));
	vmc_write(DISP_VPORCH, VFP(p_timing->vfp) | VBP(p_timing->vbp));
	vmc_write(DISP_SYNC_AREA, VSA(p_timing->vsa) | HSA(hsa_period * 2));

	htot_period = DIV_ROUND_CLOSEST((p_timing->hactive + hblank) * vdo_clk, vclk);
	hsync_wd0 = (u32) (htot_period * 20 / 100);
	hsync_wd1 = (u32) (htot_period * 80 / 100);

	vmc_write(HSYNC_WIDTH1, HSYNC_MAX_EVEN_HALF(hsa_period));
	vmc_write(HSYNC_WIDTH2, HSYNC_WD0(hsync_wd0) | HSYNC_WD1(hsync_wd1));
	vmc_write(VSYNC_WIDTH, VSYNC_WD(p_timing->vsa));

	vmc_write(EMISSION_NUM, NUM_EMIT(config->emission_num));

	hsync_num = (p_timing->vsa + p_timing->vbp + p_timing->vactive + p_timing->vfp)
		/ config->emission_num;
	vmc_write(VSYNC_PERIOD, NUM_HSYNC(hsync_num));
}

static void vmc_reg_set_lfr_timing(struct vmc_config *config)
{
	u32 val;

	val = DDI_HSYNC_OPT(0) | LFR_NUM(10) | LFR_UPDATE_ESYNC_NUM(0);
	vmc_write(LFR_UPDATE, val);
}

static void vmc_reg_set_lfr_update_enable(enum vmc_lfr_update lfr_mask)
{
	if (lfr_mask == VMC_LFR_MASK)
		vmc_write_mask(LFR_UPDATE, LFR_MASK(1), LFR_MASK(1));
	else
		vmc_write_mask(LFR_UPDATE, LFR_MASK(0), LFR_MASK(1));
}

static void vmc_reg_set_wakeup_timing(struct vmc_config *config)
{
	/*
	 * NUM_VSYNC_WU: Num of Esync, not cout of Esync
	 * NUM_HSYNC_WU: Num of Hsync
	 * Problem
	 *	: Esync Num is reset to 0 whenver VSYNC_0 to DECON is asserted
	 */
	// set to 10 temporarily
	vmc_write(DISP_WAKEUP, NUM_VSYNC_WU(10) | NUM_HSYNC_WU(0));
}

static void vmc_reg_set_control(struct vmc_config *config)
{
	u32 val;

	val = VSYNC_FRM_IRQ_MASK_CNT(1) | ESYNC_O_SEL_HSYNC |
		ESYNC_O_ENB(0) | ESYNC_O_ENB_VAL(0) |
		CLK_SEL(config->clk_sel) | VM_TYPE_AP_CENTRIC;

	vmc_write(VMC_CTRL, val);
}

static void vmc_reg_set_delay_control(struct vmc_config *config)
{
	if (config->clk_sel == VMC_CLK_CLK1)
		vmc_write(DLY_CTRL, DLY_CLK1(0));
	else
		vmc_write(DLY_CTRL, DLY_CLK2(0));
}


void vmc_regs_desc_init(void __iomem *regs, const char *name)
{
	regs_vmc.regs = regs;
	regs_vmc.name = name;

	cal_log_info(0, "VMC_ON_IRQ:%#x\n", vmc_read(VMC_ON_IRQ));
}

static void vmc_reg_set_trigger(enum vmc_trig trig)
{
	u32 val, mask;
	if (trig == VMC_TRIG_MASK) {
		val = MODULATION_SEL(0x2) | UPDATE_MASK(1) |
			IRQ_ENABLE(1) | IRQ_VSYNC_MASK(1) | IRQ_WU_MASK(1);
		mask = MODULATION_SEL_MASK | UPDATE_MASK(1) |
			IRQ_ENABLE(1) | IRQ_VSYNC_MASK(1) | IRQ_WU_MASK(1);
		vmc_write_mask(VMC_ON_IRQ, val, mask);
	} else {
		val =  IRQ_ENABLE(1) | IRQ_VSYNC_MASK(0) | IRQ_WU_MASK(1);
		mask =  IRQ_ENABLE(1) | IRQ_VSYNC_MASK(1) | IRQ_WU_MASK(1);
		vmc_write_mask(VMC_ON_IRQ, val, mask);
	}
}

void vmc_reg_set_interrupts(u32 en)
{
	u32 val = en ? 0 : ~0;
	u32 mask;

	mask = IRQ_VSYNC_FRM_MASK(1) | IRQ_VSYNC_MASK(1) | IRQ_WU_MASK(1);
	vmc_write_mask(VMC_ON_IRQ, val, mask);

	vmc_write_mask(VMC_ON_IRQ, en ? ~0 : 0, IRQ_ENABLE(1));
}

void vmc_reg_init(struct vmc_config *config)
{
	vmc_reg_set_trigger(VMC_TRIG_MASK);

	vmc_reg_set_control(config);
	vmc_reg_set_display_size_width(config);
	vmc_reg_set_lfr_timing(config);
	vmc_reg_set_wakeup_timing(config);
	vmc_reg_set_delay_control(config);

	vmc_reg_set_lfr_update_enable(VMC_LFR_UNMASK);
	vmc_reg_set_trigger(VMC_TRIG_UNMASK);
}

void vmc_reg_start(void)
{
	vmc_reg_set_interrupts(1);
	vmc_write_mask(VMC_ON_IRQ, VM_EN(1), VM_EN(1));
}

void vmc_reg_stop(void)
{
	vmc_reg_set_interrupts(0);
	vmc_write_mask(VMC_ON_IRQ, VM_EN(0), VM_EN(1));
}

u32 vmc_reg_get_interrupt_and_clear(void)
{
	u32 val;
	u32 mask;

	val = vmc_read(VMC_ON_IRQ);
	mask = VMC_IRQ_VSYNC_FRM_MOD | VMC_IRQ_VSYNC_FRM | VMC_IRQ_WU;

	vmc_write_mask(VMC_ON_IRQ, val, mask);

	return (val & mask);
}

void __vmc_dump(void __iomem *regs)
{
	cal_log_info(0, "\n=== VMC SFR DUMP ===\n");
	dpu_print_hex_dump(regs, regs + 0x0000, 0x54);

	cal_log_info(0, "\n=== VMC SHADOW SFR DUMP ===\n");
	dpu_print_hex_dump(regs, regs + SHADOW_OFFSET, 0x54);
}

