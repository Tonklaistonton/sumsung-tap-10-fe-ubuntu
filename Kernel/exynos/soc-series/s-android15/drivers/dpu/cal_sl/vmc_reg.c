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
#include <soc/samsung/exynos/exynos-soc.h>

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
	u32 vsync_wd = 4;
	u64 vclk,  vdo_clk;
	u64 hblank, vblank;
	/* hx_period: # of vdo_clk cycle */
	u64 hact_period, hsa_period, hbp_period, hfp_period;
	u64 htot_period;
	u32 hsync_wd0, hsync_wd1;
	u64 target_frm_t, target_line_t, actual_hperiod, actual_line_t;

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

	target_frm_t = DIV_ROUND_CLOSEST(NSEC_PER_SEC, p_timing->vrefresh);
	target_line_t = DIV_ROUND_CLOSEST(target_frm_t, (p_timing->vactive + vblank));
	actual_hperiod = hact_period + hsa_period + hbp_period + hfp_period;
	actual_line_t = DIV_ROUND_CLOSEST(actual_hperiod * NSEC_PER_SEC, vdo_clk);
	cal_log_debug(0, "target_line_t(%llunsec), actual_line_t(%llunsec)\n",
			target_line_t, actual_line_t);
	/*
	 * If the actual fps is higher than the target value,
	 * hperiod correction is necessary.
	 */
	if (target_line_t > actual_line_t)
		hbp_period += 1;

	htot_period = DIV_ROUND_CLOSEST((p_timing->hactive + hblank) * vdo_clk, vclk);
	hsync_wd0 = (u32) (htot_period * 20 / 100);
	hsync_wd1 = (u32) (htot_period * 80 / 100);

	vmc_write(VMC_FRAME_RESOL, VRESOL(p_timing->vactive) | HRESOL(hact_period));
	vmc_write(VMC_FRAME_HPORCH, HFP(hfp_period) | HBP(hbp_period - hsa_period));
	vmc_write(VMC_FRAME_VPORCH, VFP(p_timing->vfp) | VBP(p_timing->vbp));
	vmc_write(VMC_FRAME_SYNC_AREA, VSA(p_timing->vsa) | HSA(hsa_period * 2));
	vmc_write(VMC_FRAME_ESYNC_NUM, NUM_EMIT(config->emission_num));
	vmc_write(VMC_ESYNC_PERIOD, NUM_HSYNC((p_timing->vactive + vblank) / config->emission_num));
	vmc_write(VMC_VSYNC_WD, VSYNC_WD(vsync_wd));	// 1 < vsync_wd < 63
	vmc_write(VMC_HSYNC_INT_WD, HSYNC_MAX_EVEN_HALF(hsa_period));
	vmc_write(VMC_HSYNC_DDI_WD, HSYNC_WD0(hsync_wd0) | HSYNC_WD1(hsync_wd1));

	vmc_write(VMC_DECON_IF_DLY, htot_period / 2);
}

#define LFR_VSYNC_FRM_NUM	100 /* TODO: should find best value */
static void vmc_reg_set_lfr_timing(struct vmc_config *config)
{
	u32 val, mask;

	val = LFR_UPDATE_HSYNC(0) | LFR_NUM(LFR_VSYNC_FRM_NUM) | LFR_UPDATE(0);
	mask = LFR_UPDATE_HSYNC(0xff) | LFR_NUM(0xff) | LFR_UPDATE(0x7f);
	vmc_write_mask(VMC_DECON_LFR_UPDATE, val, mask);
}

static void vmc_reg_set_lfr_update_enable(enum vmc_lfr_update lfr_mask)
{
	if (lfr_mask == VMC_LFR_MASK)
		vmc_write_mask(VMC_DECON_LFR_UPDATE, LFR_MASK(1), LFR_MASK(1));
	else
		vmc_write_mask(VMC_DECON_LFR_UPDATE, LFR_MASK(0), LFR_MASK(1));
}

static void vmc_reg_set_wakeup_timing(struct vmc_config *config)
{
	struct dpu_panel_timing *p_timing = &config->p_timing;
	u32 htotal;

	htotal = p_timing->hactive +  p_timing->hfp +
			p_timing->hsa +  p_timing->hbp;
	vmc_write_mask(VMC_WAKE_UP_H, NUM_HSYNC_WU(htotal), NUM_HSYNC_WU(0x7ffff));
	vmc_write_mask(VMC_WAKE_UP_V, NUM_VSYNC_WU(LFR_VSYNC_FRM_NUM), NUM_VSYNC_WU(0xff));
	vmc_write_mask(VMC_WAKE_UP_V, NUM_ESYNC_WU(0), NUM_ESYNC_WU(0x7f));
}

static void vmc_reg_set_control(struct vmc_config *config)
{
	u32 val, mask;

	val = DDI_HSYNC_OPT(0) | CLK_SEL(0) | SHD_UPDATE_MASK(0) | CMD_ALLOW_MASK(0) |
		FRM_UPDATE_MASK(0) | VSYNC_I_MASK(0) | VM_TYPE_AP_CENTRIC;
	mask = DDI_HSYNC_OPT(1) | CLK_SEL(1) | SHD_UPDATE_MASK(1) | CMD_ALLOW_MASK(1) |
		FRM_UPDATE_MASK(1) | VSYNC_I_MASK(1) | VM_TYPE(1);
	vmc_write_mask(VMC_MAIN_CTRL, val, mask);
}

static void vmc_reg_set_delay_control(struct vmc_config *config)
{
	if (config->clk_sel == VMC_CLK_CLK1)
		vmc_write(VMC_ESYNC_IF_DLY, DLY_CLK1(0));
	else
		vmc_write(VMC_ESYNC_IF_DLY, DLY_CLK2(0));
}

#define TMG_V_IDL_CNT		0 /* TODO: should find best value */
static void vmc_reg_set_idle_period_control(void)
{
	u32 val;

	val = TMG_V_IDLE_CNT_ON(1) | TMG_V_IDLE_CNT(TMG_V_IDL_CNT) |
		TMG_H_IDLE_CNT_ON(1) | TMG_H_IDLE_CNT(0);
	vmc_write(VMC_TMG_IDLE_CNT, val);
}

#define MASK_PERIOD_LINE_POINT	15 /* TODO: should find best value */
#define MASK_PERIOD_PIX_POINT	0 /* TODO: should find best value */
static void vmc_reg_set_esync_decon_control(void)
{
	u32 val;

	val = MASK_ON(1) | ESYNC_DSIM_UNMASK(0) | HSYNC_DECON_UNMASK(0) |
		 LINE_POINT(MASK_PERIOD_LINE_POINT) | PIX_POINT(MASK_PERIOD_PIX_POINT);
	vmc_write(VMC_TE_ESYNC_MASK, val);

	if ((exynos_soc_info.product_id == S5E9955_SOC_ID) &&
		(exynos_soc_info.main_rev != 0)) {
		/*
		 * from evt1, the VMC generates the ewr signal
		 * at the "next esync - mask_period" point
		 * and launches it to the CMU.
		 *
		 * @EWR=1: MASK_PERIOD OF VMC_TE_ESYNC_MASK is used
		 * @EWR=0: MASK_PERIOD OF VMC_MASK_POINT_L is used
		 */
		val = LINE_POINT(MASK_PERIOD_LINE_POINT) | PIX_POINT(MASK_PERIOD_PIX_POINT);
		vmc_write(VMC_MASK_POINT_L, val);
	}
}

static void vmc_reg_set_te_control(void)
{
	vmc_write(VMC_TE_CTRL, TE_ENABLE(1));

	vmc_write(VMC_TMG_EARLY_TE, 0);
}

static void vmc_reg_set_exit_te_control(void)
{
	vmc_write_mask(VMC_TE_CTRL, CMD_EXIT_TE(1) | TE_ENABLE(1),
				CMD_EXIT_TE(1) | TE_ENABLE(1));

	/* EARLY_TE_ON should be set in order to 1st shadow update */
	vmc_write(VMC_TMG_EARLY_TE, TE_WAIT_H_PERIOD(2) | EARLY_TE_ON(1));
}

void vmc_regs_desc_init(void __iomem *regs, const char *name)
{
	regs_vmc.regs = regs;
	regs_vmc.name = name;
}

static void vmc_reg_set_trigger(enum vmc_trig trig)
{
	u32 val, mask;
	if (trig == VMC_TRIG_MASK) {
		val = SHADOW_UPDATE_TIMING_SHD_UPDATE_DECON(0x1) |
			SHADOW_UPDATE_TIMING_CMD_ALLOW(0x1) |
			SHADOW_UPDATE_MODE(0xf) |
			SHADOW_UPDATE_MASK(0x1);
		mask = SHADOW_UPDATE_TIMING_SHD_UPDATE_DECON(0x3) |
			SHADOW_UPDATE_TIMING_CMD_ALLOW(0x3) |
			SHADOW_UPDATE_MODE(0xf) |
			SHADOW_UPDATE_MASK(0x1);
		vmc_write_mask(VMC_UPDATE_CTRL, val, mask);
	} else {
		val =  SHADOW_UPDATE_MASK(0);
		mask = SHADOW_UPDATE_MASK(1);
		vmc_write_mask(VMC_UPDATE_CTRL, val, mask);
	}
}

void vmc_reg_set_interrupts(u32 en)
{
	u32 val = en ? 0 : ~0;
	u32 mask;

	mask = INTREQ_VSYNC_FRM_CNT_MASK(1) | INTREQ_VSYNC_FRM_MASK(1) | INTREQ_WU_MASK(1);

	val |= INTREQ_VSYNC_FRM_ESYNC_DECON;
	mask |= INTREQ_VSYNC_FRM_SEL(1);

	vmc_write_mask(VMC_IRQ, val, mask);

	vmc_write_mask(VMC_IRQ, en ? ~0 : 0, IRQ_ENABLE(1));
}


void vmc_reg_init(struct vmc_config *config)
{
	vmc_reg_set_trigger(VMC_TRIG_MASK);

	vmc_reg_set_control(config);
	vmc_reg_set_display_size_width(config);
	vmc_reg_set_lfr_timing(config);
	vmc_reg_set_wakeup_timing(config);
	vmc_reg_set_delay_control(config);
	vmc_reg_set_idle_period_control();
	vmc_reg_set_esync_decon_control();
	vmc_reg_set_te_control();

	vmc_reg_set_lfr_update_enable(VMC_LFR_MASK);
	vmc_reg_set_trigger(VMC_TRIG_UNMASK);
}

void vmc_reg_start(void)
{
	vmc_reg_set_interrupts(1);
	vmc_write_mask(VMC_ON, VM_EN(1), VM_EN(1));
}

void vmc_reg_stop(void)
{
	vmc_reg_set_interrupts(0);
	vmc_write_mask(VMC_ON, VM_EN(0), VM_EN(1));
}

u32 vmc_reg_get_interrupt_and_clear(void)
{
	u32 val;
	u32 mask;

	val = vmc_read(VMC_IRQ);
	mask = VMC_IRQ_VSYNC_FRM_MOD | VMC_IRQ_VSYNC_FRM | VMC_IRQ_WU;

	vmc_write_mask(VMC_IRQ, val, mask);

	return (val & mask);
}

void vmc_reg_switching_init(struct vmc_config *config)
{
	vmc_reg_get_interrupt_and_clear();
	vmc_reg_set_trigger(VMC_TRIG_MASK);
	vmc_write(VMC_IRQ, 0x1);

	vmc_reg_set_control(config);
	vmc_reg_set_display_size_width(config);
	vmc_reg_set_lfr_timing(config);
	vmc_reg_set_lfr_update_enable(VMC_LFR_MASK);
	vmc_reg_set_wakeup_timing(config);
	vmc_reg_set_idle_period_control();
	vmc_reg_set_delay_control(config);
	vmc_reg_set_esync_decon_control();

	vmc_reg_set_exit_te_control();
	vmc_reg_set_trigger(VMC_TRIG_UNMASK);
}

u32 vmc_reg_get_vmc_en(void)
{
	return vmc_read_mask(VMC_ON, VM_EN(1));
}

u32 vmc_reg_get_mask_on(void)
{
	return vmc_read_mask(SHADOW_OFFSET + VMC_TE_ESYNC_MASK, MASK_ON(1));
}

void vmc_reg_set_mask_on_force_up(u32 on)
{
	u32 val, mask;

	val = MASK_ON(on ? 1 : 0) | ESYNC_DSIM_UNMASK(on ? 0 : 1);
	mask = MASK_ON(1) | ESYNC_DSIM_UNMASK(1);

	vmc_write_mask(VMC_TE_ESYNC_MASK, val, mask);
	vmc_write_mask(VMC_ON, ~0, FORCE_UP(1));

	cal_log_info(0, "S_VMC_TE_ESYNC_MASK = 0x%08X\n", vmc_read(VMC_TE_ESYNC_MASK));
}

void __vmc_pmu_dump(void __iomem *regs)
{
	if (!regs) {
		cal_log_err(0, "\nNo vmc-related regs in pmu!!!\n");
		return;
	}
	cal_log_info(0, "\n=== VMC PMU SFR DUMP ===\n");
	dpu_print_hex_dump(regs, regs + 0x0000, 0x08);
}

void __vmc_dump(void __iomem *regs)
{
	cal_log_info(0, "\n=== VMC SFR DUMP ===\n");
	dpu_print_hex_dump(regs, regs + 0x0000, 0x88);

	cal_log_info(0, "\n=== VMC SHADOW SFR DUMP ===\n");
	if ((exynos_soc_info.product_id == S5E9955_SOC_ID) &&
		(exynos_soc_info.main_rev == 0))
		dpu_print_hex_dump(regs, regs + SHADOW_OFFSET + 0x20, 0x58);
	else
		dpu_print_hex_dump(regs, regs + SHADOW_OFFSET + 0x20, 0x5C);
}

