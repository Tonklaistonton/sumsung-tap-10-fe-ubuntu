// SPDX-License-Identifier: GPL-2.0-only
/*
 * dsim_reg.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Jaehoe Yang <jaehoe.yang@samsung.com>
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * Register access functions for Samsung EXYNOS SoC MIPI-DSI Master driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mutex.h>
#include <linux/clk.h>
#include <regs-dsim.h>
#include <dsim_cal.h>
#include <decon_cal.h>
#include <cal_config.h>
#include <dsi_phy_con_val.h>
#include "../exynos_drm_dsim.h"
#include "../exynos_drm_freq_hop.h"
#include <soc/samsung/exynos/exynos-soc.h>

static struct cal_regs_desc regs_desc[REGS_DSIM_TYPE_MAX][MAX_DSI_CNT];

#define dsim_regs_desc(id)				\
	(&regs_desc[REGS_DSIM_DSI][id])
#define dsim_read(id, offset)				\
	cal_read(dsim_regs_desc(id), offset)
#define dsim_write(id, offset, val)			\
	cal_write(dsim_regs_desc(id), offset, val)
#define dsim_read_mask(id, offset, mask)		\
	cal_read_mask(dsim_regs_desc(id), offset, mask)
#define dsim_write_mask(id, offset, val, mask)		\
	cal_write_mask(dsim_regs_desc(id), offset, val, mask)

#define dphy_regs_desc(id)				\
	(&regs_desc[REGS_DSIM_PHY][id])
#define dsim_phy_read(id, offset)			\
	cal_read(dphy_regs_desc(id), offset)
#define dsim_phy_write(id, offset, val)			\
	cal_write(dphy_regs_desc(id), offset, val)
#define dsim_phy_read_mask(id, offset, mask)		\
	cal_read_mask(dphy_regs_desc(id), offset, mask)
#define dsim_phy_write_mask(id, offset, val, mask)	\
	cal_write_mask(dphy_regs_desc(id), offset, val, mask)

DEFINE_MUTEX(sys_reg_lock);
#define sys_regs_desc(id)				\
	(&regs_desc[REGS_DSIM_SYS][id])
#define dsim_sys_read(id, offset)			\
	cal_read(sys_regs_desc(id), offset)
#define dsim_sys_write(id, offset, val)			\
	cal_write(sys_regs_desc(id), offset, val)
#define dsim_sys_write_mask(id, offset, val, mask) do {         \
        mutex_lock(&sys_reg_lock);                              \
        cal_write_mask(sys_regs_desc(id), offset, val, mask);   \
        mutex_unlock(&sys_reg_lock);                            \
} while (0)

#ifndef AP_OSC_KHZ
#define AP_OSC_KHZ	(76800)
#endif

/* dsim version */
#define DSIM_VER_REF			0x02090400

#define SKEW_CAL_REF_CLOCK		1500 /* 1500 Mbps */

/* These definitions are need to guide from AP team */
#define DSIM_STOP_STATE_CNT		0xA
#define DSIM_BTA_TIMEOUT		0xff
#define DSIM_LP_RX_TIMEOUT		0xffff
#define DSIM_MULTI_PACKET_CNT		0xffff
#define DSIM_PLL_STABLE_TIME		0x682A
#define DSIM_FIFOCTRL_THRESHOLD		0x1 /* 1 ~ 32 */
#define DSIM_PH_FIFOCTRL_THRESHOLD	32 /* 1 ~ 32 */
#define DSIM_SYNC_FIFOCTRL_THRESHOLD	0x10 /* 1 ~ 32 */
#define DSIM_SYNC_PH_FIFOCTRL_THRESHOLD	32 /* 1 ~ 32 */

#define PLL_SLEEP_CNT_MULT		450
#define PLL_SLEEP_CNT_MARGIN_RATIO	0	/* 10% ~ 20% */
#define PLL_SLEEP_CNT_MARGIN		(PLL_SLEEP_CNT_MULT *	\
					PLL_SLEEP_CNT_MARGIN_RATIO / 100)

/* If below values depend on panel. These values wil be move to panel file.
 * And these values are valid in case of video mode only.
 */
#define DSIM_CMD_ALLOW_VALUE		4
#define DSIM_STABLE_VFP_VALUE		2
#define TE_MARGIN			2 /* 2% */

#define PLL_LOCK_CNT_MUL		500
#define PLL_LOCK_CNT_MARGIN_RATIO	0	/* 10% ~ 20% */
#define PLL_LOCK_CNT_MARGIN		(PLL_LOCK_CNT_MUL *	\
					PLL_LOCK_CNT_MARGIN_RATIO / 100)

#define NARROW_FRE_HOP_MARGIN		25 /* 25 per mille */

static u32 mask_value(u32 data, u32 mask, u32 shift)
{
	return ((data & mask) >> shift);
}

/***************************** DPHY CAL functions *******************************/
static u32 dsim_reg_get_version(u32 id)
{
	u32 version;

	version = dsim_read(id, DSIM_VERSION);

	return version;
}

static void dsim_reg_set_dphy_dither_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON2, val, DSIM_PHY_DITHER_EN);
}

#ifdef DPHY_LOOP
void dsim_reg_set_dphy_loop_back_test(u32 id)
{
	dsim_phy_write_mask(id, 0x0370, 1, (0x3 << 0));
	dsim_phy_write_mask(id, 0x0470, 1, (0x3 << 0));
	dsim_phy_write_mask(id, 0x0570, 1, (0x3 << 0));
	dsim_phy_write_mask(id, 0x0670, 1, (0x3 << 0));
	dsim_phy_write_mask(id, 0x0770, 1, (0x3 << 0));
}

static void dsim_reg_set_dphy_loop_test(u32 id)
{
	dsim_phy_write_mask(id, 0x0374, ~0, (1 << 3));
	dsim_phy_write_mask(id, 0x0474, ~0, (1 << 3));
	dsim_phy_write_mask(id, 0x0574, ~0, (1 << 3));
	dsim_phy_write_mask(id, 0x0674, ~0, (1 << 3));
	dsim_phy_write_mask(id, 0x0774, ~0, (1 << 3));

	dsim_phy_write_mask(id, 0x0374, 0x6, (0x7 << 0));
	dsim_phy_write_mask(id, 0x0474, 0x6, (0x7 << 0));
	dsim_phy_write_mask(id, 0x0574, 0x6, (0x7 << 0));
	dsim_phy_write_mask(id, 0x0674, 0x6, (0x7 << 0));
	dsim_phy_write_mask(id, 0x0774, 0x6, (0x7 << 0));

	dsim_phy_write_mask(id, 0x037c, 0x2, (0xffff << 0));
	dsim_phy_write_mask(id, 0x047c, 0x2, (0xffff << 0));
	dsim_phy_write_mask(id, 0x057c, 0x2, (0xffff << 0));
	dsim_phy_write_mask(id, 0x067c, 0x2, (0xffff << 0));
	dsim_phy_write_mask(id, 0x077c, 0x2, (0xffff << 0));
}
#endif

/*
 * ADAPTIVE_SYNC[27] bit of option suite register is always read '0'
 *  in evt0 due to h/w bug
 * This bug will be revised from evt1.
 */
static void dsim_write_option_suite_mask(u32 id, u32 org_val, u32 org_mask)
{
	u32 ext_vt_sync = DSIM_OPTION_SUITE_OPT_EXT_VT_SYNC_MASK;
	u32 val = org_val;
	u32 mask = org_mask;

	if ((dsim_read(id, DSIM_OPTION_SUITE) & ext_vt_sync)) {
		val |= DSIM_OPTION_SUITE_OPT_ADAPTIVE_SYNC_MASK;
		mask |= DSIM_OPTION_SUITE_OPT_ADAPTIVE_SYNC_MASK;
	}

	dsim_write_mask(id, DSIM_OPTION_SUITE, val, mask);
}

static void dsim_reg_set_dphy_wclk_buf_sft(u32 id, u32 cnt)
{
	u32 val = DSIM_PHY_WCLK_BUF_SFT_CNT(cnt);

	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON3, val,
			DSIM_PHY_WCLK_BUF_SFT_CNT_MASK);
}

/* DPHY setting */
static void dsim_reg_set_pll_freq(u32 id, u32 p, u32 m, u32 s, u32 k)
{
	u32 val, mask;

	/* F value */
	val = DSIM_PHY_PMS_F(k);
	dsim_phy_write(id, DSIM_PHY_PLL_CON6, val);

	/* P & S value */
	val = DSIM_PHY_PMS_P(p) | DSIM_PHY_PMS_S(s);
	mask = DSIM_PHY_PMS_P_MASK | DSIM_PHY_PMS_S_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON0, val, mask);

	/* M value */
	val = DSIM_PHY_PMS_M(m);
	mask = DSIM_PHY_PMS_M_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON1, val, mask);
}

static u32 dsim_reg_cal_pll_freq(u32 id, u32 p, u32 m, u32 s, u32 k)
{
	u64 pll_freq;

	pll_freq = AP_OSC_KHZ * (u64)((m << 16) + k / (1 <<16) - ((k >> 31) << 16));
	pll_freq = (pll_freq * 2 / p / (1 << s)) >> 16;
	pll_freq = (u32) DIV_ROUND_CLOSEST(pll_freq, 1000); /* Mhz */

	cal_log_debug(id, "[p m s k]: [%u %u %u %u] => %lluMhz", p, m, s, k,
			pll_freq);

	return pll_freq;
}

/* calculate hsclk according to pmsk values */
static u32 dsim_reg_get_pll_freq(u32 id)
{
	u32 val;
	u32 p, m, s, k;

	/* F value */
	val = dsim_phy_read(id, DSIM_PHY_PLL_CON6);
	k = DSIM_PHY_PMS_F_GET(val);

	/* P & S value */
	val = dsim_phy_read(id, DSIM_PHY_PLL_CON0);
	p = DSIM_PHY_PMS_P_GET(val) ?: 1;
	s = DSIM_PHY_PMS_S_GET(val);

	/* M value */
	val = dsim_phy_read(id, DSIM_PHY_PLL_CON1);
	m = DSIM_PHY_PMS_M_GET(val);

	return dsim_reg_cal_pll_freq(id, p, m, s, k);
}

static void dsim_reg_set_dphy_timing_values(u32 id,
			struct dphy_timing_value *t, u32 hsmode)
{
	u32 val;
	u32 hs_en, skewcal_en;
	u32 i;

	/* HS mode setting */
	if (hsmode) {
		/* under 1500Mbps : don't need SKEWCAL enable */
		hs_en = 1;
		skewcal_en = 0;
	} else {
		/* above 1500Mbps : need SKEWCAL enable */
		hs_en = 0;
		skewcal_en = 1;
	}

	/* clock lane setting */
	val = DSIM_PHY_ULPS_EXIT(t->b_dphyctl) | DSIM_PHY_WCLK_GATING_DISABLE(1);
	dsim_phy_write(id, DSIM_PHY_MC_TIME_CON2, val);

	val = DSIM_PHY_TCLK_PREPARE(t->clk_prepare) | DSIM_PHY_TCLK_ZERO(t->clk_zero) |
		DSIM_PHY_HSTX_CLK_SEL(hs_en) | DSIM_PHY_TLPX(t->lpx);
	dsim_phy_write(id, DSIM_PHY_MC_TIME_CON0, val);

	/* skew cal settings
	 * same values at [1500 ~ 2500]Mbps @ LN05LPE(DPHY v2.1)
	 */
	val = DSIM_PHY_SKEWCAL_INIT_WAIT_TIME(0xB) |
		DSIM_PHY_SKEWCAL_INIT_RUN_TIME(0xC) |
		DSIM_PHY_SKEWCAL_RUN_TIME(0x9) |
		skewcal_en;
	dsim_phy_write(id, DSIM_PHY_MC_DESKEW_CON0, val);
	/* add 'run|init_run|wait_run time' if skewcal is enabled */

	val = DSIM_PHY_TCLK_POST(t->clk_post) |
		DSIM_PHY_THS_EXIT(t->hs_exit) | DSIM_PHY_TCLK_TRAIL(t->clk_trail);
	dsim_phy_write(id, DSIM_PHY_MC_TIME_CON1, val);

	/* add other clock lane setting if necessary */

	/* data lane setting : D0 ~ D3 */
	for (i = 0; i < 4; i++) {
		val = DSIM_PHY_ULPS_EXIT(t->b_dphyctl);
		dsim_phy_write(id, DSIM_PHY_MD_TIME_CON2(i), val);


		val = DSIM_PHY_THS_PREPARE(t->hs_prepare) |
			DSIM_PHY_THS_ZERO(t->hs_zero) |
			DSIM_PHY_HSTX_CLK_SEL(hs_en) | DSIM_PHY_TLPX(t->lpx) |
			DSIM_PHY_TLP_EXIT_SKEW(0) | DSIM_PHY_TLP_ENTRY_SKEW(0);
		dsim_phy_write(id, DSIM_PHY_MD_TIME_CON0(i), val);

		val = DSIM_PHY_THS_EXIT(t->hs_exit) |
			DSIM_PHY_THS_TRAIL(t->hs_trail) |
			DSIM_PHY_TTA_GET(3) | DSIM_PHY_TTA_GO(0);
		dsim_phy_write(id, DSIM_PHY_MD_TIME_CON1(i), val);
		/* add other clock lane setting if necessary */
	}
}

static void dsim_reg_set_dphy_param_dither(u32 id, struct stdphy_pms *dphy_pms)
{
#if 0
	u32 val, mask;

	/* MFR & MRR*/
	val = DSIM_PHY_DITHER_MFR(dphy_pms->mfr)
		| DSIM_PHY_DITHER_MRR(dphy_pms->mrr);
	dsim_phy_write(id, DSIM_PHY_PLL_CON1, val);

	/* SEL_PF & ICP */
	val = DSIM_PHY_DITHER_SEL_PF(dphy_pms->sel_pf)
		| DSIM_PHY_DITHER_ICPC(dphy_pms->icp);
	mask = DSIM_PHY_DITHER_SEL_PF_MASK | DSIM_PHY_DITHER_ICPC_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON2, val, mask);

	/* AFC_ENB & EXTAFC & FSEL * RSEL*/
	val = (DSIM_PHY_DITHER_AFC_ENB((dphy_pms->afc_enb) ? ~0 : 0))
		| DSIM_PHY_DITHER_EXTAFC(dphy_pms->extafc)
		| DSIM_PHY_DITHER_FSEL(((dphy_pms->fsel) ? ~0 : 0))
		| DSIM_PHY_DITHER_RSEL(dphy_pms->rsel);
	mask = DSIM_PHY_DITHER_AFC_ENB_MASK | DSIM_PHY_DITHER_EXTAFC_MASK
		| DSIM_PHY_DITHER_FSEL_MASK | DSIM_PHY_DITHER_RSEL_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON2, val, mask);

	/* FEED_EN */
	val = ((dphy_pms->feed_en) ? ~0 : 0) | ((dphy_pms->fout_mask) ? ~0 : 0);
	mask = DSIM_PHY_DITHER_FEED_EN | DSIM_PHY_DITHER_FOUT_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON2, val, mask);
#endif
}

/* BIAS Block Control Register */
static void dsim_reg_set_bias_con(u32 id, const u32 *blk_ctl)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(DSIM_PHY_BIAS_CON_VAL); i++)
		dsim_phy_write(id, DSIM_PHY_BIAS_CON(i), blk_ctl[i]);
}

static void dsim_reg_set_hs_vod(u32 id, const u32 vod)
{
	if (vod != UINT_MAX)
		dsim_phy_write_mask(id, DSIM_PHY_BIAS_CON1,
					  DSIM_PHY_REG400M(vod),
					  DSIM_PHY_REG400M_MASK);
}

/* PLL Control Register */
static void dsim_reg_set_pll_con(u32 id, const u32 *blk_ctl)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(DSIM_PHY_PLL_CON_VAL); i++)
		dsim_phy_write(id, DSIM_PHY_PLL_CON(i), blk_ctl[i]);
}

/* Master Clock Lane General Control Register */
static void dsim_reg_set_mc_gnr_con(u32 id, const u32 *blk_ctl)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(DSIM_PHY_MC_GNR_CON_VAL); i++)
		dsim_phy_write(id, DSIM_PHY_MC_GNR_CON(i), blk_ctl[i]);
}

/* Master Clock Lane Analog Block Control Register */
static void dsim_reg_set_mc_ana_con(u32 id, const u32 *blk_ctl)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(DSIM_PHY_MC_ANA_CON_VAL); i++)
		dsim_phy_write(id, DSIM_PHY_MC_ANA_CON(i), blk_ctl[i]);
}

/* Master Data Lane General Control Register */
static void dsim_reg_set_md_gnr_con(u32 id, const u32 *blk_ctl)
{
	u32 i;

	for (i = 0; i < MAX_DSIM_DATALANE_CNT; i++)
		dsim_phy_write(id, DSIM_PHY_MD_GNR_CON0(i), blk_ctl[0]);
}

/* Master Data Lane Analog Block Control Register */
#define NUM_COMBO_PHY (3)
static void dsim_reg_set_md_ana_con(u32 id)
{
	u32 i;

	for (i = 0; i < NUM_COMBO_PHY; i++) {
		dsim_phy_write(id, DSIM_PHY_MD_ANA_CON0(i), DSIM_PHY_MD_ANA_CON_VAL[0]);
		dsim_phy_write(id, DSIM_PHY_MD_ANA_CON1(i), DSIM_PHY_MD_ANA_CON_VAL[1]);
	}

	dsim_phy_write(id, DSIM_PHY_MD_ANA_CON0(3), DSIM_PHY_MD_ANA_CON_VAL[0]);
}

#ifdef DPDN_INV_SWAP
void dsim_reg_set_inv_dpdn(u32 id, u32 inv_clk, u32 inv_data[4])
{
	u32 i;
	u32 val, mask;

	val = inv_clk ? (DSIM_PHY_CLK_INV) : 0;
	mask = DSIM_PHY_CLK_INV;
	dsim_phy_write_mask(id, DSIM_PHY_MC_DATA_CON0, val, mask);

	for (i = 0; i < MAX_DSIM_DATALANE_CNT; i++) {
		val = inv_data[i] ? (DSIM_PHY_DATA_INV) : 0;
		mask = DSIM_PHY_DATA_INV;
		dsim_phy_write_mask(id,  DSIM_PHY_MD_DATA_CON0(i), val, mask);
	}
}

static void dsim_reg_set_dpdn_swap(u32 id, u32 clk_swap)
{
	u32 val, mask;

	val = DSIM_PHY_DPDN_SWAP(clk_swap);
	mask = DSIM_PHY_DPDN_SWAP_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_MC_ANA_CON0, val, mask);
}
#endif

/******************* DSIM CAL functions *************************/
static void dsim_reg_sw_reset(u32 id)
{
	u32 val;
	int ret;

	dsim_write_mask(id, DSIM_SWRST, ~0, DSIM_SWRST_RESET);

	ret = readl_poll_timeout_atomic(dsim_regs_desc(id)->regs + DSIM_SWRST,
			val, !(val & DSIM_SWRST_RESET), 10, 2000);
	if (ret)
		cal_log_err(id, "%s is timeout.\n", __func__);
}

static void dsim_reg_set_config_options(u32 id, u32 lane,
		u32 eotp_en, u32 per_frame_read, u32 pix_format, u32 vc_id)
{
	u32 val, mask;

	val = DSIM_CONFIG_NUM_OF_DATA_LANE(lane) | DSIM_CONFIG_EOTP_EN(eotp_en)
		| DSIM_CONFIG_PER_FRAME_READ_EN(per_frame_read)
		| DSIM_CONFIG_PIXEL_FORMAT(pix_format)
		| DSIM_CONFIG_VC_ID(vc_id);

	mask = DSIM_CONFIG_NUM_OF_DATA_LANE_MASK | DSIM_CONFIG_EOTP_EN_MASK
		| DSIM_CONFIG_PER_FRAME_READ_EN_MASK
		| DSIM_CONFIG_PIXEL_FORMAT_MASK
		| DSIM_CONFIG_VC_ID_MASK;

	dsim_write_mask(id, DSIM_CONFIG, val, mask);
}

static void dsim_reg_enable_lane(u32 id, u32 lane, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_LANES_EN(lane));
}

static int dsim_reg_wait_phy_ready_clane(u32 id, bool en)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
			dphy_regs_desc(id)->regs + DSIM_PHY_MC_GNR_CON0,
			val, (en == DSIM_PHY_PHY_READY_GET(val)), 10, 2000);
	if (ret) {
		cal_log_err(id, "PHY clock lane is not ready[timeout]\n");
		return ret;
	}

	return 0;
}

static int dsim_reg_wait_phy_ready_dlane(u32 id, u32 lane_id, bool en)
{
	u32 val, reg_id;
	int ret;

	reg_id = DSIM_PHY_MD_GNR_CON0(lane_id);

	ret = readl_poll_timeout_atomic(dphy_regs_desc(id)->regs + reg_id, val,
			(en == DSIM_PHY_PHY_READY_GET(val)), 10, 2000);
	if (ret) {
		cal_log_err(id, "PHY clock lane is not ready[timeout]\n");
		return ret;
	}

	return 0;
}

static int dsim_reg_enable_lane_phy(u32 id, u32 lane, bool en)
{
	u32 i, lane_cnt = 0;
	u32 reg_id;
	u32 ret = 0;
	u32 val = en ? ~0 : 0;

	/* check enabled data lane count */
	lane_cnt = hweight32(lane);

	/*
	 * [step1] enable phy_enable
	 */

	/* (1.1) clock lane on|off */
	reg_id = DSIM_PHY_MC_GNR_CON0;
	dsim_phy_write_mask(id, reg_id, val, DSIM_PHY_PHY_ENABLE);

	/* (1.2) data lane on|off */
	for (i = 0; i < lane_cnt; i++) {
		reg_id = DSIM_PHY_MD_GNR_CON0(i);
		dsim_phy_write_mask(id, reg_id, val, DSIM_PHY_PHY_ENABLE);
	}

	/*
	 * [step2] wait for phy_ready
	 */

	/* (2.1) check ready of clock lane */
	if (dsim_reg_wait_phy_ready_clane(id, en))
		ret++;

	/* (2.2) check ready of data lanes (index : from '1') */
	for (i = 0; i < lane_cnt; i++)
		if (dsim_reg_wait_phy_ready_dlane(id, i, en))
			ret++;

	if (ret) {
		cal_log_err(id, "Error to enable PHY lane(err=%d)\n", ret);
		return -EBUSY;
	} else
		return 0;
}

static void dsim_reg_pll_stable_time(u32 id, u32 lock_cnt)
{
	u32 val;

	val = DSIM_PHY_PLL_LOCK_CNT(lock_cnt);
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON3, val, DSIM_PHY_PLL_LOCK_CNT_MASK);
}

static void dsim_reg_set_pll(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON0, val, DSIM_PHY_PLL_EN_MASK);
}

static bool dsim_reg_is_pll_stable(u32 id)
{
	u32 val, pll_lock;

	val = dsim_phy_read(id, DSIM_PHY_PLL_STAT0);
	pll_lock = DSIM_PHY_PLL_LOCK_GET(val);

	return pll_lock != 0;
}

static int dsim_reg_enable_pll(u32 id, u32 en)
{
	u32 val;
	int ret;

	if (en)
		dsim_reg_clear_int(id, DSIM_INTSRC_PLL_STABLE);

	dsim_reg_set_pll(id, en);

	ret = readl_poll_timeout_atomic(
			dphy_regs_desc(id)->regs + DSIM_PHY_PLL_STAT0,
			val, en == DSIM_PHY_PLL_LOCK_GET(val), 10, 2000);
	if (ret) {
		cal_log_err(id, "PHY %s failed[timeout]\n",
				en ? "enable" : "disable");
		return ret;
	}

	return 0;
}

static void dsim_reg_set_esc_clk_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_ESCCLK_EN);
}

static void dsim_reg_set_esc_clk_prescaler(u32 id, u32 en, u32 p)
{
	u32 val = en ? DSIM_CLK_CTRL_ESCCLK_EN : 0;
	u32 mask = DSIM_CLK_CTRL_ESCCLK_EN | DSIM_CLK_CTRL_ESC_PRESCALER_MASK;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (dsim_reg_get_version(id) < 0x02090200) {
		val |= DSIM_CLK_CTRL_ESC_PRESCALER(p);
		dsim_write_mask(id, DSIM_CLK_CTRL, val, mask);
	} else {
		if (!(dsim->res.oscclk_dsim))
			cal_log_err(id, "no oscclk_dsim resource");
		else if (en)
			if (clk_set_rate(dsim->res.oscclk_dsim, (AP_OSC_KHZ * 1000 / p)))
				cal_log_err(id, "cannot set oscclk_dsim\n");
	}
}

static void dsim_reg_set_esc_clk_on_lane(u32 id, u32 en, u32 lane)
{
	u32 val;

	lane = (lane >> 1) | (1 << 4);

	val = en ? DSIM_CLK_CTRL_LANE_ESCCLK_EN(lane) : 0;
	dsim_write_mask(id, DSIM_CLK_CTRL, val,
				DSIM_CLK_CTRL_LANE_ESCCLK_EN_MASK);
}

static void dsim_reg_set_stop_state_cnt(u32 id)
{
	u32 val = DSIM_ESCMODE_STOP_STATE_CNT(DSIM_STOP_STATE_CNT);

	dsim_write_mask(id, DSIM_ESCMODE, val,
				DSIM_ESCMODE_STOP_STATE_CNT_MASK);
}

static void dsim_reg_set_timeout(u32 id)
{
	u32 val = DSIM_TIMEOUT_BTA_TOUT(DSIM_BTA_TIMEOUT)
		| DSIM_TIMEOUT_LPRX_TOUT(DSIM_LP_RX_TIMEOUT);

	dsim_write(id, DSIM_TIMEOUT, val);
}

static void dsim_reg_disable_hsa(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HSA_DISABLE);
}

static void dsim_reg_disable_hbp(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HBP_DISABLE);
}

static void dsim_reg_disable_hfp(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HFP_DISABLE);
}

static void dsim_reg_disable_hse(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HSE_DISABLE);
}

static void dsim_reg_set_burst_mode(u32 id, u32 burst)
{
	u32 val = burst ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_BURST_MODE);
}

static void dsim_reg_set_sync_inform(u32 id, u32 inform)
{
	u32 val = inform ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_SYNC_INFORM);
}

#if defined(CONFIG_EXYNOS_PLL_SLEEP)
static bool __update_and_get_stop_pll(u32 id, u32 stop, bool with_update)
{
	static bool stopped[MAX_DSI_CNT];

	if (!with_update)
		return stopped[id];

	if (stop == stopped[id])
		return stopped[id];

	if (stop)
		dsim_reg_set_pll_sleep_block(id, stop);

	stopped[id] = stop;

	return stopped[id];
}

static void __update_stop_pll_sleep(u32 id, u32 stop)
{
#if !defined(CONFIG_EXYNOS_PLL_SLEEP)
	return;
#endif
	__update_and_get_stop_pll(id, stop, true);
}

void dsim_reg_stop_pll_sleep(u32 id, u32 stop)
{
	__update_stop_pll_sleep(id, stop);
}

void dsim_reg_set_pll_sleep_enable(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	if (__update_and_get_stop_pll(id, 0, false))
		val = 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_PLL_SLEEP);
}

static void dsim_reg_set_pll_sleep_self_ctrl(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	/* pll sleep auto-control is supported from 2.92 */
	if (dsim_reg_get_version(id) < 0x02090200)
		return;

	if ((exynos_soc_info.product_id == S5E9955_SOC_ID) &&
		(exynos_soc_info.main_rev == 0))
		dsim_write_option_suite_mask(id, val,
			DSIM_OPTION_SUITE_OPT_PLL_SLEEP_SELF_CTRL_MASK);
	else
		dsim_write_mask(id, DSIM_OPTION_SUITE, val,
			DSIM_OPTION_SUITE_OPT_PLL_SLEEP_SELF_CTRL_MASK);
}
#endif

void __decon_trig_control_for_dsim_shadow(u32 id, bool shadow_en)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	static bool trig_masked[MAX_DSI_CNT];

	if ((dsim->state == DSIM_STATE_ULPS || dsim->state == DSIM_STATE_SUSPEND)
		&& !trig_masked[id])
		return;

	if (!shadow_en) {
		if (!decon_reg_get_trigger_mask(id)) {
			decon_reg_set_trigger(id, DECON_HW_TRIG, DECON_TRIG_MASK);
			trig_masked[id] = true;
		} else
			trig_masked[id] = false;
	} else if (trig_masked[id]) {
		decon_reg_set_trigger(id, DECON_HW_TRIG, DECON_TRIG_UNMASK);
		trig_masked[id] = false;
	}
}

static void dsim_reg_enable_shadow(u32 id, u32 en);
void dsim_reg_set_pll_sleep_block(u32 id, u32 block)
{
#if !defined(CONFIG_EXYNOS_PLL_SLEEP)
	return;
#else
	if (__update_and_get_stop_pll(id, 0, false))
		return;
#endif

	__decon_trig_control_for_dsim_shadow(id, 0);
	dsim_reg_enable_shadow(id, 0);
	dsim_reg_set_pll_sleep_enable(id, !block);
	dsim_reg_enable_shadow(id, 1);
	__decon_trig_control_for_dsim_shadow(id, 1);

	if (block)
		dsim_reg_wait_exit_pll_sleep(id);

	cal_log_debug(id, "pll-sleep %s\n",block ? "block" : "unblock");
}

void dsim_reg_set_pll_sleep_block_release(u32 id)
{
	if (exynos_soc_info.product_id == S5E9955_SOC_ID)
		dsim_reg_set_pll_sleep_block(id, false);
}

/* 0=D-PHY, 1=C-PHY */
void dsim_reg_set_phy_selection(u32 id, u32 sel)
{
	u32 val = sel ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_PHY_SELECTION);
}

void dsim_reg_set_lp_force(u32 id, u32 en)
{
       u32 val = en ? ~0 : 0;

       dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_LP_FORCE_EN);
}

static void dsim_reg_set_vporch(u32 id, u32 vbp, u32 vfp)
{
	u32 val;

	val = DSIM_VPORCH_VFP_TOTAL(vfp) | DSIM_VPORCH_VBP(vbp);
	dsim_write(id, DSIM_VPORCH, val);
}

static void dsim_reg_set_vfp_detail(u32 id, u32 cmd_allow, u32 stable_vfp)
{
	u32 val = DSIM_VPORCH_VFP_CMD_ALLOW(cmd_allow)
		| DSIM_VPORCH_STABLE_VFP(stable_vfp);
	dsim_write(id, DSIM_VFP_DETAIL, val);
}

static void dsim_reg_set_hporch(u32 id, u32 hbp, u32 hfp)
{
	u32 val;

	val = DSIM_HPORCH_HFP(hfp) | DSIM_HPORCH_HBP(hbp);
	dsim_write(id, DSIM_HPORCH, val);
}

static void dsim_reg_set_sync_area(u32 id, u32 vsa, u32 hsa)
{
	u32 val = DSIM_SYNC_VSA(vsa) | DSIM_SYNC_HSA(hsa);

	dsim_write(id, DSIM_SYNC, val);
}

static void dsim_reg_set_resol(u32 id, u32 xres, u32 yres)
{
	u32 val;

	val = DSIM_RESOL_VRESOL(yres) | DSIM_RESOL_HRESOL(xres);
	dsim_write(id, DSIM_RESOL, val);
}

void dsim_reg_set_vresol(u32 id, u32 vresol)
{
	u32 val = DSIM_RESOL_VRESOL(vresol);

	dsim_write_mask(id, DSIM_RESOL, val, DSIM_RESOL_VRESOL_MASK);
}

void dsim_reg_set_hresol(u32 id, u32 hresol)
{
	u32 val = DSIM_RESOL_HRESOL(hresol);

	dsim_write_mask(id, DSIM_RESOL, val, DSIM_RESOL_HRESOL_MASK);
}

#define DSIM_VHM_CMD_ALLOW_MIN		4
#define DSIM_VHM_CMD_MASK_VAlUE		17 /* MASK_PERIOD_LINE_POINT + 2 of VMC */
#define DSIM_VHM_CMD_MASK_MIN		1

#define VHM_FH_VFP_START_POINT    	20 /* TODO: should find best value */
void dsim_reg_set_porch(u32 id, struct dsim_reg_config *config)
{
	u32 cmd_allow = config->p_timing.vfp - 2 * DSIM_STABLE_VFP_VALUE;

	dsim_reg_set_vfp_detail(id, cmd_allow, DSIM_STABLE_VFP_VALUE);

	if (config->mode == DSIM_VIDEO_MODE) {
		dsim_reg_set_vporch(id, config->p_timing.vbp,
				config->p_timing.vfp);
		if (config->vmc_en) {
			/*
			 * cmd_mask is set to higher than
			 * the mask_period_line_poinf of vmc
			 * in order to ensure
			 * that pre-frame sync cmd & frame are transmitted
			 * together as much as possible
			 */
			if (config->p_timing.vfp >=
				(DSIM_STABLE_VFP_VALUE +
				DSIM_VHM_CMD_ALLOW_MIN +
				DSIM_VHM_CMD_MASK_VAlUE))
				cmd_allow = config->p_timing.vfp -
					(DSIM_STABLE_VFP_VALUE + DSIM_VHM_CMD_MASK_VAlUE);
			else if (config->p_timing.vfp >=
				(DSIM_STABLE_VFP_VALUE +
				DSIM_VHM_CMD_ALLOW_MIN +
				DSIM_VHM_CMD_MASK_MIN))
				cmd_allow = DSIM_VHM_CMD_ALLOW_MIN;
		}
		dsim_reg_set_vfp_detail(id, cmd_allow,
				DSIM_STABLE_VFP_VALUE);
		dsim_reg_set_hporch(id, config->p_timing.hbp,
				config->p_timing.hfp);
		dsim_reg_set_sync_area(id, config->p_timing.vsa,
				config->p_timing.hsa);
	}
	if (config->vmc_en) {
		struct dsim_device *dsim = get_dsim_drvdata(id);
		u32 fh_start_point;
		u32 val, mask;

		if (!dsim->freq_hop)
			return;

		fh_start_point = config->p_timing.vfp > (VHM_FH_VFP_START_POINT + 1) ?
				VHM_FH_VFP_START_POINT : config->p_timing.vfp - 2;
		val = DSIM_FH_COMPENSATE_FH_START_POINT(fh_start_point);
		mask = DSIM_FH_COMPENSATE_FH_START_POINT_MASK;
		dsim_write_mask(id, DSIM_FH_COMPENSATE, val, mask);
		cal_log_debug(id, "fh_start_point(%d)", fh_start_point);
	}
}

static void dsim_reg_set_vt_htiming0(u32 id, u32 hsa_period, u32 hact_period)
{
	u32 val = DSIM_VT_HTIMING0_HSA_PERIOD(hsa_period)
		| DSIM_VT_HTIMING0_HACT_PERIOD(hact_period);

	dsim_write(id, DSIM_VT_HTIMING0, val);
}

static void dsim_reg_set_vt_htiming1(u32 id, u32 hfp_period, u32 hbp_period)
{
	u32 val = DSIM_VT_HTIMING1_HFP_PERIOD(hfp_period)
		| DSIM_VT_HTIMING1_HBP_PERIOD(hbp_period);

	dsim_write(id, DSIM_VT_HTIMING1, val);
}

static void dsim_reg_set_hperiod(u32 id, struct dsim_reg_config *config,
		u32 hs_clk)
{
	u64 vclk,  wclk;
	u64 hblank, vblank;
	u64 width, height;
	u64 hact_period, hsa_period, hbp_period, hfp_period;

	/*
	 * in case of vmc_en
	 * even if dsc compression is enabled, it is calculated using
	 * an uncompressed size to match the period timing with vmc.
	 */
	if (!(config->vmc_en) && config->dsc.enabled)
		width = get_comp_dsc_width(&config->dsc, config->bpc, 4) *
					config->dsc.slice_count;
	else
		width = config->p_timing.hactive;

	height = config->p_timing.vactive;

	if (config->mode == DSIM_VIDEO_MODE) {
		hblank = config->p_timing.hsa + config->p_timing.hbp +
			config->p_timing.hfp;
		vblank = config->p_timing.vsa + config->p_timing.vbp +
			config->p_timing.vfp;
		vclk = DIV_ROUND_CLOSEST((width + hblank) * (height + vblank) *
				config->p_timing.vrefresh, 1000);
#if defined(CONFIG_BOARD_EMULATOR)
		wclk = AP_OSC_KHZ;
#else
		wclk = DIV_ROUND_CLOSEST((u64)(hs_clk) * 1000, 16);
#endif

		/* round calculation to reduce fps error */
		hact_period = DIV_ROUND_CLOSEST(width * wclk, vclk);
		hsa_period = DIV_ROUND_CLOSEST(config->p_timing.hsa * wclk,
				vclk);
		hbp_period = DIV_ROUND_CLOSEST(config->p_timing.hbp * wclk,
				vclk);
		hfp_period = DIV_ROUND_CLOSEST(config->p_timing.hfp * wclk,
				vclk);

		dsim_reg_set_vt_htiming0(id, (u32)hsa_period, (u32)hact_period);
		dsim_reg_set_vt_htiming1(id, (u32)hfp_period, (u32)hbp_period);
	}
}

static void dsim_reg_set_video_mode(u32 id, u32 mode)
{
	u32 val = mode ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_VIDEO_MODE);
}

void dsim_reg_set_freq_hopp(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_FREQ_HOPP, val, DSIM_FREQ_HOPP_FH_EN_MASK);
}

int dsim_reg_wait_freq_hopp_timeout(u32 id, unsigned long timeout)
{
	u32 val;
	int ret = 0;

	cal_log_debug(id, "+\n");

	/*
	 * Because FH_EN was written when shadow_en=0,
	 * FH_EN should be checked under the condition
	 * that shadow_read_en is '0'.
	 */
	dsim_reg_enable_shadow_read(id, 0);
	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_FREQ_HOPP, val,
			!(val & DSIM_FREQ_HOPP_FH_EN_MASK), 10, timeout);
	dsim_reg_enable_shadow_read(id, 1);

	if (ret)
		cal_log_err(id, "timeout of waiting fh_en\n");

	cal_log_debug(id, "-\n");

	return ret;
}


static void dsim_reg_set_ext_vt_sync(u32 id, struct dsim_reg_config *config)
{
	u32 val, mask;
	u32 vtotal;
	u32 emission_num = 2;

	if (config->vmc_en) {
		vtotal = config->p_timing.vsa + config->p_timing.vbp +
			config->p_timing.vfp + config->p_timing.vactive;
		dsim_write(id, DSIM_ESYNC_CTRL, vtotal / emission_num);
	}

	val = config->vmc_en? ~0 : 0;
#if IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)
	val |= DSIM_OPTION_SUITE_OPT_SHADOW_SYNC_CMD_ALLOW_MASK;
#endif

	mask = DSIM_OPTION_SUITE_OPT_ADAPTIVE_SYNC_MASK |
#if IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)
		DSIM_OPTION_SUITE_OPT_SHADOW_SYNC_CMD_ALLOW_MASK |
#endif
		DSIM_OPTION_SUITE_OPT_EXT_VT_SYNC_MASK;
	dsim_write_mask(id, DSIM_OPTION_SUITE, val, mask);
}

static int dsim_reg_wait_idle_status(u32 id, u32 is_vm)
{
	u32 val;
	int ret;

	if (!is_vm)
		return 0;

	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_LINK_STATUS0, val,
			!DSIM_LINK_STATUS0_VIDEO_MODE_STATUS_GET(val), 10,
			33000);
	if (ret) {
		cal_log_err(id, "dsim%d wait timeout idle status\n", id);
		return ret;
	}

	return 0;
}

/* 0 = command, 1 = video mode */
u32 dsim_reg_get_display_mode(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_CONFIG);
	return DSIM_CONFIG_DISPLAY_MODE_GET(val);
}

static void dsim_reg_enable_dsc(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_CPRS_EN);
}

static void dsim_reg_set_cprl_ctrl(u32 id, struct dsim_reg_config *config)
{
	u32 multi_slice = 1;
	u32 val;

	/* if multi-slice(2~4 slices) DSC compression is used in video mode
	 * MULTI_SLICE_PACKET configuration must be matched
	 * to DDI's configuration
	 */
	if (config->mode == DSIM_COMMAND_MODE)
		multi_slice = 1;
	else if (config->mode == DSIM_VIDEO_MODE)
		multi_slice = config->dsc.slice_count > 1 ? 1 : 0;

	/* if MULTI_SLICE_PACKET is enabled,
	 * only one packet header is transferred
	 * for multi slice
	 */
	val = DSIM_CPRS_CTRL_MULI_SLICE_PACKET(multi_slice)
		| DSIM_CPRS_CTRL_NUM_OF_SLICE(config->dsc.slice_count);

	dsim_write(id, DSIM_CPRS_CTRL, val);
}

static void dsim_reg_set_num_of_slice(u32 id, u32 num_of_slice)
{
	u32 val = DSIM_CPRS_CTRL_NUM_OF_SLICE(num_of_slice);

	dsim_write_mask(id, DSIM_CPRS_CTRL, val,
				DSIM_CPRS_CTRL_NUM_OF_SLICE_MASK);
}

static void dsim_reg_get_num_of_slice(u32 id, u32 *num_of_slice)
{
	u32 val = dsim_read(id, DSIM_CPRS_CTRL);

	*num_of_slice = DSIM_CPRS_CTRL_NUM_OF_SLICE_GET(val);
}

static void dsim_reg_set_multi_slice(u32 id, struct dsim_reg_config *config)
{
	bool multi_slice = true;
	u32 val;

	/* if multi-slice(2~4 slices) DSC compression is used in video mode
	 * MULTI_SLICE_PACKET configuration must be matched
	 * to DDI's configuration
	 */
	if (config->mode == DSIM_VIDEO_MODE)
		multi_slice = config->dsc.slice_count > 1;

	/* if MULTI_SLICE_PACKET is enabled,
	 * only one packet header is transferred
	 * for multi slice
	 */
	val = multi_slice ? ~0 : 0;
	dsim_write_mask(id, DSIM_CPRS_CTRL, val,
				DSIM_CPRS_CTRL_MULI_SLICE_PACKET_MASK);
}

static void dsim_reg_set_size_of_slice(u32 id, struct dsim_reg_config *config)
{
	u32 slice_w = config->dsc.slice_width;
	u32 val_01 = 0;
	u32 val_23 = 0;

	slice_w = DIV_ROUND_UP(config->dsc.slice_width * config->bpc, 8);
	if (config->dsc.slice_count == 4) {
		val_01 = DSIM_SLICE01_SIZE_OF_SLICE1(slice_w) |
			DSIM_SLICE01_SIZE_OF_SLICE0(slice_w);
		val_23 = DSIM_SLICE23_SIZE_OF_SLICE3(slice_w) |
			DSIM_SLICE23_SIZE_OF_SLICE2(slice_w);

		dsim_write(id, DSIM_SLICE01, val_01);
		dsim_write(id, DSIM_SLICE23, val_23);
	} else if (config->dsc.slice_count == 2) {
		val_01 = DSIM_SLICE01_SIZE_OF_SLICE1(slice_w) |
			DSIM_SLICE01_SIZE_OF_SLICE0(slice_w);

		dsim_write(id, DSIM_SLICE01, val_01);
	} else if (config->dsc.slice_count == 1) {
		val_01 = DSIM_SLICE01_SIZE_OF_SLICE0(slice_w);

		dsim_write(id, DSIM_SLICE01, val_01);
	} else {
		cal_log_err(id, "not supported slice mode dsc(%d), slice(%d)\n",
				config->dsc.dsc_count, config->dsc.slice_count);
	}
}

static void dsim_reg_print_size_of_slice(u32 id)
{
	u32 val;
	u32 slice0_w, slice1_w, slice2_w, slice3_w;

	val = dsim_read(id, DSIM_SLICE01);
	slice0_w = DSIM_SLICE01_SIZE_OF_SLICE0_GET(val);
	slice1_w = DSIM_SLICE01_SIZE_OF_SLICE1_GET(val);

	val = dsim_read(id, DSIM_SLICE23);
	slice2_w = DSIM_SLICE23_SIZE_OF_SLICE2_GET(val);
	slice3_w = DSIM_SLICE23_SIZE_OF_SLICE3_GET(val);

	cal_log_debug(id, "slice0 w(%d) slice1 w(%d) slice2 w(%d) slice3(%d)\n",
			slice0_w, slice1_w, slice2_w, slice3_w);
}

static void dsim_reg_set_multi_packet_count(u32 id, u32 multipacketcnt)
{
	u32 val = DSIM_CMD_CONFIG_MULTI_PKT_CNT(multipacketcnt);

	dsim_write_mask(id, DSIM_CMD_CONFIG, val,
				DSIM_CMD_CONFIG_MULTI_PKT_CNT_MASK);
}

static void dsim_reg_set_cmd_te_ctrl0(u32 id, u32 stablevfp)
{
	u32 val = DSIM_CMD_TE_CTRL0_TIME_STABLE_VFP(stablevfp);

	dsim_write(id, DSIM_CMD_TE_CTRL0, val);
}

static void dsim_reg_set_cmd_te_ctrl1(u32 id, u32 teprotecton, u32 tetout)
{
	u32 val = DSIM_CMD_TE_CTRL1_TIME_TE_PROTECT_ON(teprotecton)
		| DSIM_CMD_TE_CTRL1_TIME_TE_TOUT(tetout);

	dsim_write(id, DSIM_CMD_TE_CTRL1, val);
}

#if IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)
#define TE_FRM_MASK_MARGIN			10 /* 10% */
static void dsim_reg_set_cmd_te_frm_mask(u32 id, unsigned int fps, u32 hs_clk)
{
	u32 te_frm_mask;

	te_frm_mask = hs_clk * (100 - TE_FRM_MASK_MARGIN - TE_MARGIN) * 100 / fps / 16;

	dsim_write_mask(id, DSIM_CMD_TE_CTRL0,
		DSIM_CMD_TE_CTRL0_TIME_TE_FRM_MASK(te_frm_mask),
		DSIM_CMD_TE_CTRL0_TIME_TE_FRM_MASK_MASK);
}
#endif

static void dsim_reg_get_cmd_timer(unsigned int fps, unsigned int *te_protect,
		unsigned int *te_timeout, u32 hs_clk)
{
	*te_protect = hs_clk * (100 - TE_MARGIN) * 100 / fps / 16;
	*te_timeout = hs_clk * (100 + TE_MARGIN * 2) * 100 / fps / 16;
}

void dsim_reg_set_cmd_ctrl(u32 id, struct dsim_reg_config *config,
						struct dsim_clks *clks)
{
	unsigned int time_stable_vfp;
	unsigned int time_te_protect_on;
	unsigned int time_te_tout;
	u32 pll_freq = dsim_reg_get_pll_freq(id) ?: clks->hs_clk;

	time_stable_vfp = config->p_timing.hactive * DSIM_STABLE_VFP_VALUE / 100;
	if (!config->dsc.enabled)
		time_stable_vfp *= 3;

	dsim_reg_get_cmd_timer(config->p_timing.vrefresh, &time_te_protect_on,
			       &time_te_tout, pll_freq);
	dsim_reg_set_cmd_te_ctrl0(id, time_stable_vfp);
	dsim_reg_set_cmd_te_ctrl1(id, time_te_protect_on, time_te_tout);
#if IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)
	if (config->mode == DSIM_COMMAND_MODE)
		dsim_reg_set_cmd_te_frm_mask(id, config->p_timing.vrefresh,
			pll_freq);
#endif
}

static void dsim_reg_enable_noncont_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val,
				DSIM_CLK_CTRL_NONCONT_CLOCK_LANE);
}

static void dsim_reg_enable_clocklane(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val,
				DSIM_CLK_CTRL_CLKLANE_ONOFF);
}

int dsim_reg_wait_phy_config_idle(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_LINK_STATUS3, val,
			(DSIM_LINK_STATUS3_PHY_CFG_STATUS_GET(val) ==
			DSIM_LINK_STATUS3_PHY_CFG_STATUS_IDLE),
			10, 1000);

	if (ret) {
		cal_log_err(id, "PHY configuration status is %d\n",
					DSIM_LINK_STATUS3_PHY_CFG_STATUS_GET(val));
		return ret;
	}

	return 0;
}

#if defined(CONFIG_EXYNOS_PLL_SLEEP) && defined(CONFIG_EXYNOS_BIAS_SLEEP)
bool dsim_reg_is_pll_sleep_state(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_LINK_STATUS3);
	val = DSIM_LINK_STATUS3_PLL_SLEEP_STATUS_GET(val);

	return (((val != DSIM_LINK_STATUS3_PLL_SLEEP_STATUS_IDLE) &&
			(val != DSIM_LINK_STATUS3_PLL_SLEEP_STATUS_WCLK)) ?
			true : false);
}

int dsim_reg_wait_exit_pll_sleep(u32 id)
{
	u32 val;
	int ret;

	if (__update_and_get_stop_pll(id, 0, false))
		return 0;

	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_LINK_STATUS3, val,
			(DSIM_LINK_STATUS3_PLL_SLEEP_STATUS_GET(val) ==
			DSIM_LINK_STATUS3_PLL_SLEEP_STATUS_IDLE),
			10, 2000);

	if (ret) {
		cal_log_err(id, "PLL sleep status is %d\n",
					DSIM_LINK_STATUS3_PLL_SLEEP_STATUS_GET(val));
		return ret;
	}

	/* add delay about 25usec to guarantee bias on */
	udelay(25);

	return 0;
}
#endif

void dsim_reg_enable_packetgo(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CMD_CONFIG, val,
				DSIM_CMD_CONFIG_PKT_GO_EN);
}

void dsim_reg_set_packetgo_ready(u32 id)
{
	dsim_write_mask(id, DSIM_CMD_CONFIG, DSIM_CMD_CONFIG_PKT_GO_RDY,
				DSIM_CMD_CONFIG_PKT_GO_RDY);
}

u32 dsim_reg_get_packetgo_ready(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_CMD_CONFIG);

	return DSIM_CMD_CONFIG_PKT_GO_RDY_GET(val);
}

bool dsim_reg_check_pkt_go_rdy(u32 id, u32 timeout)
{
	bool result = true;
	u32 val;
	int ret;

	if (dsim_reg_get_packetgo_ready(id)) {
		cal_log_info(id, "packetgo_ready is still high\n");
		ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_CMD_CONFIG, val,
			!DSIM_CMD_CONFIG_PKT_GO_RDY_GET(val), 10, timeout);
		if (ret) {
			cal_log_err(id, "timeout waiting for DSIM%d packetgo_ready to clear\n", id);
			result = false;
		} else
			cal_log_info(id, "packetgo_ready became low\n");
	}

	return result;
}


static void dsim_reg_enable_multi_cmd_packet(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CMD_CONFIG, val,
				DSIM_CMD_CONFIG_MULTI_CMD_PKT_EN);
}

/*
 * 0 = Disable shadow update (applied to Operating_SFR directly)
 * 1 = Enable shadow update (applied to Operating_SFR based on protocol)
 */
static void dsim_reg_enable_shadow(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_SFR_CTRL, val,
				DSIM_SFR_CTRL_SHADOW_EN);
}

static void dsim_reg_enable_shadow_vss_update(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_SHADOW_CONFIG, val,
			DSIM_SHADOW_CONFIG_SHADOW_VSS_UPDT_MASK);
}

static void dsim_reg_set_link_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;
	u32 mux_status;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_CLOCK_SEL);

	mux_status = en ? DSIM_LINK_STATUS3_MUX_STATUS_WORD_CLOCK :
				DSIM_LINK_STATUS3_MUX_STATUS_ALIVE_CLOCK;

	if (readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_LINK_STATUS3, val,
			(DSIM_LINK_STATUS3_MUX_STATUS_GET(val) == mux_status),
			10, 2000)) {
		cal_log_err(id, "failed to change link clock source to %s\n",
					(en ? "word clock" : "alive clock"));
		dsim_dump(get_dsim_drvdata(id));
	} else
		cal_log_debug(id, "succeeded to change link clock source to %s\n",
					(en ? "word clock" : "alive clock"));
}

int dsim_reg_get_linecount(u32 id, struct dsim_reg_config config)
{
	u32 val = 0;

	if (config.mode == DSIM_VIDEO_MODE) {
		val = dsim_read(id, DSIM_LINK_STATUS0);
		return DSIM_LINK_STATUS0_VM_LINE_CNT_GET(val);
	} else if (config.mode == DSIM_COMMAND_MODE) {
		val = dsim_read(id, DSIM_LINK_STATUS1);
		return DSIM_LINK_STATUS1_CMD_TRANSF_CNT_GET(val);
	}

	return -EINVAL;
}

static void dsim_reg_enable_hs_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_TX_REQUEST_HSCLK);
}

static void dsim_reg_enable_word_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_WORDCLK_EN);
}

static void dsim_reg_osc_gate_cond_sel(u32 id, u32 on)
{
	u32 val = on ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_OSC_GATE_CONDITION);
}

static int dsim_reg_wait_hs_clk_ready(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_DPHY_STATUS,
			val, (val & DSIM_DPHY_STATUS_TX_READY_HSCLK), 10,
			2000);
	if (ret) {
		cal_log_err(id, "DSI Master is not HS state.\n");
		return ret;
	}
	cal_log_debug(id, "DSI Master is HS state.\n");

	return 0;
}

static int dsim_reg_wait_hs_clk_disable(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_DPHY_STATUS,
			val, (val & DSIM_DPHY_STATUS_STOPSTATE_CLK), 10, 2000);
	if (ret) {
		cal_log_err(id, "DSI Master clock isn't disabled.\n");
		return ret;
	}

	return 0;
}

static void dsim_reg_enter_ulps(u32 id, u32 enter)
{
	u32 val = enter ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK | DSIM_ESCMODE_TX_ULPS_DATA;

	dsim_write_mask(id, DSIM_ESCMODE, val, mask);
}

static void dsim_reg_exit_ulps(u32 id, u32 exit)
{
	u32 val = exit ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK_EXIT |
			DSIM_ESCMODE_TX_ULPS_DATA_EXIT;

	dsim_write_mask(id, DSIM_ESCMODE, val, mask);
}

static void dsim_reg_set_num_of_transfer(u32 id, u32 num_of_transfer)
{
	u32 val = DSIM_NUM_OF_TRANSFER_PER_FRAME(num_of_transfer);

	dsim_write(id, DSIM_NUM_OF_TRANSFER, val);
}

static void dsim_reg_set_deskew_hw(u32 id, u32 interval, u32 position)
{
	u32 val = DSIM_DESKEW_CTRL_HW_INTERVAL(interval)
		| DSIM_DESKEW_CTRL_HW_POSITION(position);
	u32 mask = DSIM_DESKEW_CTRL_HW_INTERVAL_MASK
		| DSIM_DESKEW_CTRL_HW_POSITION_MASK;

	dsim_write_mask(id, DSIM_DESKEW_CTRL, val, mask);
}

static void dsim_reg_enable_deskew_hw_enable(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_DESKEW_CTRL, val, DSIM_DESKEW_CTRL_HW_EN);
}

void dsim_reg_set_cm_underrun_lp_ref(u32 id, u32 lp_ref)
{
	u32 val = DSIM_UNDERRUN_CTRL_CM_UNDERRUN_LP_REF(lp_ref);

	dsim_write(id, DSIM_UNDERRUN_CTRL, val);
}

static void dsim_reg_set_threshold(u32 id, u32 threshold)
{
	u32 val = DSIM_THRESHOLD_LEVEL(threshold);

	dsim_write(id, DSIM_THRESHOLD, val);

}

static void dsim_reg_set_vt_compensate(u32 id, u32 compensate)
{
	u32 val = DSIM_VIDEO_TIMER_COMPENSATE(compensate);

	if (dsim_reg_get_version(id) >= DSIM_VER_REF)
		return;

	dsim_write_mask(id, DSIM_VIDEO_TIMER, val,
			DSIM_VIDEO_TIMER_COMPENSATE_MASK);
}

static void dsim_reg_set_vstatus_int(u32 id, u32 vstatus)
{
	u32 val = DSIM_VIDEO_TIMER_VSTATUS_INTR_SEL(vstatus);

	dsim_write_mask(id, DSIM_VIDEO_TIMER, val,
			DSIM_VIDEO_TIMER_VSTATUS_INTR_SEL_MASK);
}

static void dsim_reg_set_bist_te_interval(u32 id, u32 interval)
{
	u32 val = DSIM_BIST_CTRL0_BIST_TE_INTERVAL(interval);

	dsim_write_mask(id, DSIM_BIST_CTRL0, val,
			DSIM_BIST_CTRL0_BIST_TE_INTERVAL_MASK);
}

static void dsim_reg_set_bist_mode(u32 id, u32 bist_mode)
{
	u32 val = DSIM_BIST_CTRL0_BIST_PTRN_MODE(bist_mode);

	dsim_write_mask(id, DSIM_BIST_CTRL0, val,
			DSIM_BIST_CTRL0_BIST_PTRN_MODE_MASK);
}

static void dsim_reg_enable_bist_pattern_move(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_BIST_CTRL0, val,
			DSIM_BIST_CTRL0_BIST_PTRN_MOVE_EN);
}

static void dsim_reg_enable_bist(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_BIST_CTRL0, val, DSIM_BIST_CTRL0_BIST_EN);
}

static void dsim_set_hw_deskew(u32 id, u32 en)
{
	u32 hw_interval = 1;

	if (en) {
		/* 0 : VBP first line, 1 : VFP last line*/
		dsim_reg_set_deskew_hw(id, hw_interval, 0);
		dsim_reg_enable_deskew_hw_enable(id, en);
	} else {
		dsim_reg_enable_deskew_hw_enable(id, en);
	}
}

static int dsim_reg_wait_enter_ulps_state(u32 id, u32 lanes)
{
	u32 val, data_lanes;
	int ret;

	data_lanes = lanes >> DSIM_LANE_CLOCK;
	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_DPHY_STATUS, val,
			(DSIM_DPHY_STATUS_ULPS_DATA_LANE_GET(val) == data_lanes)
			&& (val & DSIM_DPHY_STATUS_ULPS_CLK), 10, 2000);
	if (ret) {
		cal_log_debug(id, "DSI Master is ULPS state.\n");
		return ret;
	}

	return 0;
}

static int dsim_reg_wait_exit_ulps_state(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
			dsim_regs_desc(id)->regs + DSIM_DPHY_STATUS, val,
			!(DSIM_DPHY_STATUS_ULPS_DATA_LANE_GET(val)) &&
			!(val & DSIM_DPHY_STATUS_ULPS_CLK), 10, 2000);
	if (ret) {
		cal_log_err(id, "DSI Master is not stop state.\n");
		return ret;
	}

	return 0;
}

static int dsim_reg_get_dphy_timing(u32 id, u32 hs_clk, u32 esc_clk,
		struct dphy_timing_value *t)
{
	int i;

	i = ARRAY_SIZE(dphy_timing) - 1;

	for (; i >= 0; i--) {
		if ((dphy_timing[i][0] + 4) < hs_clk) {
			continue;
		} else {
			t->bps = hs_clk;
			t->clk_prepare = dphy_timing[i][1];
			t->clk_zero = dphy_timing[i][2];
			t->clk_post = dphy_timing[i][3];
			t->clk_trail = dphy_timing[i][4];
			t->hs_prepare = dphy_timing[i][5];
			t->hs_zero = dphy_timing[i][6];
			t->hs_trail = dphy_timing[i][7];
			t->lpx = dphy_timing[i][8];
			t->hs_exit = dphy_timing[i][9];
			break;
		}
	}

	if (i < 0) {
		cal_log_err(id, "can't find proper dphy timing(%u Mhz)\n",
				hs_clk);
		return -EINVAL;
	}

	cal_log_debug(id, "bps(%u) clk_prepare(%u) clk_zero(%u) clk_post(%u)\n",
			t->bps, t->clk_prepare, t->clk_zero, t->clk_post);
	cal_log_debug(id, "clk_trail(%u) hs_prepare(%u) hs_zero(%u)\n",
			t->clk_trail, t->hs_prepare, t->hs_zero);
	cal_log_debug(id, "hs_trail(%u) lpx(%u) hs_exit(%u)\n",
			t->hs_trail, t->lpx, t->hs_exit);

	if ((esc_clk > 20) || (esc_clk < 7)) {
		cal_log_err(id, "%u Mhz can't be used as escape clock\n",
				esc_clk);
		return -EINVAL;
	}

	t->b_dphyctl = b_dphyctl[esc_clk - 7];
	cal_log_debug(id, "b_dphyctl(%u)\n", t->b_dphyctl);

	return 0;
}

void dsim_reg_set_burst_cmd(u32 id, enum dsim_panel_mode mode)
{
	if (mode == DSIM_VIDEO_MODE) {
		dsim_reg_set_multi_packet_count(id, DSIM_PH_FIFOCTRL_THRESHOLD);
		dsim_reg_enable_multi_cmd_packet(id, 1);
	}

	dsim_reg_enable_packetgo(id, true);
}

/*
 * 0 = shadow update occus with frame start
 * 1 = registers below are shadow updated immediately
 *	DSIM_CONFIG[18](=video_mode)
 *	DSIM_CONFIG[31:16](=vfp_total)
 *	DSIM_VFP_DETAIL(=vfp_cmd_allow, vfp_stable)
 *	DSIM_OPTION_SUITE[27](=opt_adaptive_sync)
 *	DSIM_OPTION_SUITE[22](=opt_shadow_sync_cmd_allow)
 *	DSIM_ESYNC_CTRL[15:0](=esync_line)
 */
static void dsim_reg_enable_instant_shadow_update(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_SFR_CTRL, val,
			DSIM_SFR_CTRL_INSTANT_SHADOW_UPDATE_EN);
}

static void dsim_reg_set_config(u32 id, struct dsim_reg_config *config,
						struct dsim_clks *clks)
{
	u32 xres, yres;
	u32 threshold;
	u32 num_of_slice;
	u32 num_of_transfer;
	u32 overlap = 0; /* not yet supported */
	u32 version;
	u32 pll_freq = dsim_reg_get_pll_freq(id) ?: clks->hs_clk;

	/* shadow read disable */
	dsim_reg_enable_shadow_read(id, 1);

	if (config->mode == DSIM_VIDEO_MODE)
		dsim_reg_enable_clocklane(id, 0);
	else
		dsim_reg_enable_noncont_clock(id, 1);

	if (pll_freq >= SKEW_CAL_REF_CLOCK)
		dsim_set_hw_deskew(id, 1);
	else
		dsim_set_hw_deskew(id, 0);

	dsim_reg_enable_shadow_vss_update(id, config->mode == DSIM_VIDEO_MODE ? 1 : 0);

	/* set bta & lpdr timeout vlaues */
	dsim_reg_set_timeout(id);

	dsim_reg_set_cmd_transfer_mode(id, 0);
	dsim_reg_set_stop_state_cnt(id);

	if (config->mode == DSIM_COMMAND_MODE) {
		dsim_reg_set_cm_underrun_lp_ref(id, config->cmd_underrun_cnt);
	}

	yres = config->p_timing.vactive;
	/* threshold is availabe as 1H or 2H
	 * In case of DSC, 1H(compressed width) is allowed.
	 */
	if (config->dsc.enabled) {
		threshold = get_comp_dsc_width(&config->dsc, config->bpc, 4) *
							config->dsc.slice_count;
		xres = threshold;
		num_of_transfer = config->p_timing.vactive;
	} else {
		threshold = config->p_timing.hactive * 1 + overlap;
		xres = config->p_timing.hactive + overlap;
		num_of_transfer = (config->p_timing.hactive + overlap) *
				config->p_timing.vactive / threshold;
	}

	dsim_reg_set_threshold(id, threshold);
	dsim_reg_set_resol(id, xres, yres);
	dsim_reg_set_porch(id, config);
	dsim_reg_set_lp_force(id, config->lp_force_en);
	dsim_reg_set_num_of_transfer(id, num_of_transfer);

	/* set number of lanes, eotp enable, per_frame_read, pixformat, vc_id */
	dsim_reg_set_config_options(id, config->data_lane_cnt - 1, 1, 0,
			DSIM_PIXEL_FORMAT_RGB24, 0);

	/* CPSR & VIDEO MODE can be set when shadow enable on */
	/* shadow enable */
	dsim_reg_enable_shadow(id, 1);
	if (config->mode == DSIM_VIDEO_MODE) {
		dsim_reg_set_video_mode(id, 1);
		dsim_reg_set_hperiod(id, config, pll_freq);
		cal_log_debug(id, "%s: video mode set\n", __func__);
	} else {
		dsim_reg_set_video_mode(id, 0);
		cal_log_debug(id, "%s: command mode set\n", __func__);
	}

	dsim_reg_enable_dsc(id, config->dsc.enabled);

	if (config->mode == DSIM_VIDEO_MODE) {
		dsim_reg_disable_hsa(id, 0);
		dsim_reg_disable_hbp(id, 0);
		 /*
		  * non-burst to prevent hs-lp transition
		  * between lines during vactive period
		  */
		if (config->vmc_en) {
			dsim_reg_disable_hfp(id, 0);
			dsim_reg_disable_hse(id, 1);
			dsim_reg_set_burst_mode(id, 0);
		} else {
			dsim_reg_disable_hfp(id, 1);
			dsim_reg_disable_hse(id, 0);
			dsim_reg_set_burst_mode(id, 1);
		}
		dsim_reg_set_sync_inform(id, 0);
		dsim_reg_enable_clocklane(id, 0);
	}

	if (config->dsc.enabled) {
		cal_log_debug(id, "%s: dsc configuration is set\n", __func__);
		dsim_reg_set_cprl_ctrl(id, config);
		dsim_reg_set_size_of_slice(id, config);

		dsim_reg_get_num_of_slice(id, &num_of_slice);
		cal_log_debug(id, "number of DSC slice(%d)\n", num_of_slice);
		dsim_reg_print_size_of_slice(id);
	}

	if (config->burst_cmd_en) {
		dsim_reg_set_burst_cmd(id, config->mode);
	} else {
		dsim_reg_enable_packetgo(id, false);
	}

	if (config->mode == DSIM_COMMAND_MODE) {
		dsim_reg_set_cmd_ctrl(id, config, clks);
	} else if (config->mode == DSIM_VIDEO_MODE) {
		dsim_reg_set_vt_compensate(id, config->vt_compensation);
		if (config->vmc_en)
			dsim_reg_set_vstatus_int(id, DSIM_VSYNC);
		else
			dsim_reg_set_vstatus_int(id, DSIM_VFP);
	}

	if (config->version.major == 0 && config->version.minor == 0) {
		version = dsim_reg_get_version(id);
		config->version.major = HEXSTR2DEC(DSIM_VERSION_GET_MAJOR(version));
		config->version.minor = HEXSTR2DEC(DSIM_VERSION_GET_MINOR(version));
		cal_log_info(id, "dsim version : %02d.%02d.00\n",
				config->version.major, config->version.minor);
	}
	dsim_reg_set_ext_vt_sync(id, config);
	dsim_reg_enable_instant_shadow_update(id, 1);
}

static enum dsim_wide_freq_hop_range
dsim_check_wide_freq_hop_range(u32 center_clk, u32 hs_clk)
{
	u64 narrow_hs_clk_min = (u64)center_clk * (1000 - NARROW_FRE_HOP_MARGIN);
	u64 narrow_hs_clk_max = (u64)center_clk * (1000 + NARROW_FRE_HOP_MARGIN);

	if ((u64)hs_clk * 1000 < narrow_hs_clk_min)
		return DSIM_WIDE_FRE_HOP_LOW;
	else if ((u64)hs_clk * 1000 > narrow_hs_clk_max)
		return DSIM_WIDE_FRE_HOP_HIGH;
	else
		return DSIM_WIDE_FRE_HOP_MID;
}

/*
 * configure and set DPHY PLL, byte clock, escape clock and hs clock
 *	- PMS value have to be optained by using PMS Gen.
 *      tool (MSC_PLL_WIZARD2_00.exe)
 *	- PLL out is source clock of HS clock
 *	- byte clock = HS clock / 16
 *	- calculate divider of escape clock using requested escape clock
 *	  from driver
 *	- DPHY PLL, byte clock, escape clock are enabled.
 *	- HS clock will be enabled another function.
 *
 * Parameters
 *	- hs_clk : in/out parameter.
 *		in :  requested hs clock. out : calculated hs clock
 *	- esc_clk : in/out parameter.
 *		in : requested escape clock. out : calculated escape clock
 *	- word_clk : out parameter. byte clock = hs clock / 16
 */
static int dsim_reg_set_clocks(u32 id, struct dsim_clks *clks,
		struct dsim_reg_config *config, u32 en)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	unsigned int esc_div;
	struct dsim_pll_param pll;
	struct dphy_timing_value t;
	u32 pll_lock_cnt;
	u32 min_pll_lock_cnt;
	int ret = 0;
	u32 hsmode = 0;
#ifdef DPDN_INV_SWAP
	u32 inv_data[4] = {0, };
#endif
	u32 esc_inclk;
	u32 pll_freq, dphy_timing_hs_clk;
	enum dsim_wide_freq_hop_range hop_range;

	if (en) {
		struct stdphy_pms *dphy_pms = &config->dphy_pms;

		/*
		 * Do not need to set clocks related with PLL,
		 * if DPHY_PLL is already stabled because of LCD_ON_UBOOT.
		 */
		if (dsim_reg_is_pll_stable(id)) {
			cal_log_info(id, "DPHY PLL is already stable\n");
			return -EBUSY;
		}

		/*
		 * set p, m, s to DPHY PLL
		 * PMS value has to be optained by PMS calculation tool
		 * released to customer
		 */
		pll.p = dphy_pms->p;
		pll.m = dphy_pms->m;
		pll.s = dphy_pms->s;
		pll.k = dphy_pms->k;
		pll_freq = dsim_reg_cal_pll_freq(id, pll.p, pll.m, pll.s, pll.k);

		dphy_timing_hs_clk = clks->hs_clk;
		if (dsim->freq_hop && dsim->freq_hop->wide_enabled) {
			hop_range = dsim_check_wide_freq_hop_range(clks->hs_clk, pll_freq);
			if (hop_range != DSIM_WIDE_FRE_HOP_MID)
				dphy_timing_hs_clk = pll_freq;
		}

		/* get word clock */
		/* clks ->hs_clk is from DT */
		clks->word_clk = dphy_timing_hs_clk / 16;
		cal_log_debug(id, "word clock is %u MHz\n", clks->word_clk);

		/* requeseted escape clock */
		cal_log_debug(id, "requested escape clock %u MHz\n",
						clks->esc_clk);

		if (dsim_reg_get_version(id) < 0x02090200)
		    esc_inclk = clks->word_clk;
		else
		    esc_inclk = AP_OSC_KHZ / 1000;

		/* escape clock divider */
		esc_div = esc_inclk / clks->esc_clk;

		/* adjust escape clock */
		if ((esc_inclk / esc_div) > clks->esc_clk)
			esc_div += 1;

		/* adjusted escape clock */
		clks->esc_clk = esc_inclk / esc_div;
		cal_log_debug(id, "escape clock divider is 0x%x\n", esc_div);
		cal_log_debug(id, "escape clock is %u MHz\n", clks->esc_clk);
		dsim_reg_set_esc_clk_prescaler(id, 1, esc_div);

		/* set BIAS ctrl : default value */
		dsim_reg_set_bias_con(id, DSIM_PHY_BIAS_CON_VAL);
		dsim_reg_set_hs_vod(id, config->drive_strength);

		/* set PLL ctrl : default value */
		dsim_reg_set_pll_con(id, DSIM_PHY_PLL_CON_VAL);
#if 0
		if ((clks->hs_clk << pll.s) < 3000)
			dsim_phy_write_mask(id, DSIM_PHY_PLL_CON5,
						DSIM_PHY_DITHER_SEL_VCO(1),
						DSIM_PHY_DITHER_SEL_VCO_MASK);
#endif

		if (dphy_timing_hs_clk < SKEW_CAL_REF_CLOCK)
			hsmode = 1;

		/* get DPHY timing values using hs clock and escape clock */
		dsim_reg_get_dphy_timing(id, dphy_timing_hs_clk, clks->esc_clk, &t);
		dsim_reg_set_dphy_timing_values(id, &t, hsmode);
		/* check dither sequence */
		if (dphy_pms->dither_en) {
			dsim_reg_set_dphy_param_dither(id, dphy_pms);
			dsim_reg_set_dphy_dither_en(id, 1);
		}

		/* set clock lane General Control Register control */
		dsim_reg_set_mc_gnr_con(id, DSIM_PHY_MC_GNR_CON_VAL);

		/* set clock lane Analog Block Control Register control */
		dsim_reg_set_mc_ana_con(id, DSIM_PHY_MC_ANA_CON_VAL);

#ifdef DPDN_INV_SWAP
		dsim_reg_set_dpdn_swap(id, 1);
#endif

		/* set data lane General Control Register control */
		dsim_reg_set_md_gnr_con(id, DSIM_PHY_MD_GNR_CON_VAL);

#ifdef DPDN_INV_SWAP
		inv_data[0] = 0;
		inv_data[1] = 1;
		inv_data[2] = 1;
		inv_data[3] = 0;
		dsim_reg_set_inv_dpdn(id, 0, inv_data);
#endif

		/* set data lane Analog Block Control Register control */
		dsim_reg_set_md_ana_con(id);

		/* set PMSK on PHY */
		dsim_reg_set_pll_freq(id, pll.p, pll.m, pll.s, pll.k);
		cal_log_debug(id, "calculated hsclk is %u MHz\n", pll_freq);

		/*set wclk buf sft cnt */
		dsim_reg_set_dphy_wclk_buf_sft(id, 3);

		/* set PLL's lock time (lock_cnt)
		 * PLL lock cnt setting guide
		 * PLL_LOCK_CNT_MUL = 500
		 * PLL_LOCK_CNT_MARGIN = 10 (10%)
		 * PLL lock time = PLL_LOCK_CNT_MUL * Tp
		 * Tp = 1 / (OSC clk / pll_p)
		 * PLL lock cnt = PLL lock time * OSC clk
		 *
		 * recommended to measure the frequency assuming that the frequency is locked after 100us
		 * AP_OSC / 1000000 * 100 =  AP_OSC_KHZ / 10
		 */

		pll_lock_cnt = (PLL_LOCK_CNT_MUL + PLL_LOCK_CNT_MARGIN) * pll.p;
		min_pll_lock_cnt = AP_OSC_KHZ / 10;

		if (min_pll_lock_cnt > pll_lock_cnt)
			pll_lock_cnt = min_pll_lock_cnt;

		dsim_reg_pll_stable_time(id, pll_lock_cnt);

#ifdef DPHY_LOOP
		dsim_reg_set_dphy_loop_test(id);
#endif
		/* enable PLL */
		ret = dsim_reg_enable_pll(id, 1);

		if (!hsmode)
			usleep_range(100, 200);
	} else {
		/* check disable PHY timing */
		dsim_reg_set_esc_clk_prescaler(id, 0, 0xff);
		dsim_reg_enable_pll(id, 0);
	}

	return ret;
}

static int dsim_reg_set_lanes(u32 id, u32 lanes, u32 en)
{
	/* LINK lanes */
	dsim_reg_enable_lane(id, lanes, en);

	return 0;
}

static int dsim_reg_set_lanes_dphy(u32 id, u32 lanes, bool en)
{
	/* PHY lanes */
	if (dsim_reg_enable_lane_phy(id, (lanes >> 1), en))
		return -EBUSY;

	return 0;
}

static u32 dsim_reg_is_noncont_clk_enabled(u32 id)
{
	int ret;

	ret = dsim_read_mask(id, DSIM_CLK_CTRL,
					DSIM_CLK_CTRL_NONCONT_CLOCK_LANE);
	return ret;
}

static int dsim_reg_set_hs_clock(u32 id, u32 en)
{
	int reg = 0;
	int is_noncont = dsim_reg_is_noncont_clk_enabled(id);
	u32 val = dsim_read(id, DSIM_CLK_CTRL);

	val = DSIM_CLK_CTRL_TX_REQUEST_HSCLK_GET(val);
	if (val == en)
		return reg;

	if (en) {
		dsim_reg_enable_hs_clock(id, 1);
		if (!is_noncont)
			reg = dsim_reg_wait_hs_clk_ready(id);
	} else {
		dsim_reg_enable_hs_clock(id, 0);
		reg = dsim_reg_wait_hs_clk_disable(id);
	}
	return reg;
}

static void dsim_reg_set_int(u32 id, u32 en)
{
	u32 val = en ? 0 : ~0;
	u32 mask;

	/*
	 * TODO: underrun irq will be unmasked in the future.
	 * underrun irq(dsim_reg_set_config) is ignored in zebu emulator.
	 * it's not meaningful
	 *
	 * [20.03.11] remove DSIM_INTMSK_SW_RST_RELEASE
	 * When dsim_reg_sw_reset is performed at the dsim_reg_stop,
	 * sw_rst_release occurs, causing dsim_irq_handler to operate,
	 * At this time abnormal behavior happens
	 * similar to dpp_reg_set_irq_mask_all().
	 */
	mask = DSIM_INTMSK_SFR_PL_FIFO_EMPTY |
		DSIM_INTMSK_SFR_PH_FIFO_EMPTY |
		DSIM_INTMSK_FRAME_DONE | DSIM_INTMSK_INVALID_SFR_VALUE |
		DSIM_INTMSK_UNDER_RUN | DSIM_INTMSK_RX_DATA_DONE |
		DSIM_INTMSK_ERR_RX_ECC | DSIM_INTMSK_VT_STATUS;
#if IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)
	mask |= DSIM_INTMSK_SYNC_CMD_PH_FIFO_EMPTY;
#endif

	dsim_write_mask(id, DSIM_INTMSK, val, mask);
}

/*
 * enter or exit ulps mode
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : exit ULPS mode
 */
static int dsim_reg_set_ulps(u32 id, u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;

	} else {
		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(id, 1);

		ret = dsim_reg_wait_exit_ulps_state(id);
		if (ret)
			return ret;

		/* wait at least 1ms : Twakeup time for MARK1 state  */
		udelay(1000);

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(id, 0);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	}

	return ret;
}

/*
 * enter or exit ulps mode for LSI DDI
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : exit ULPS mode
 * assume that disp block power is off after ulps mode enter
 */
static int dsim_reg_set_smddi_ulps(u32 id, u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;
		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	} else {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;

		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(id, 1);

		ret = dsim_reg_wait_exit_ulps_state(id);
		if (ret)
			return ret;

		/* wait at least 1ms : Twakeup time for MARK1 state */
		udelay(100);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(id, 0);
	}

	return ret;
}

static int dsim_reg_set_ulps_by_ddi(u32 id, u32 ddi_type, u32 lanes, u32 en)
{
	int ret;

	switch (ddi_type) {
	case TYPE_OF_SM_DDI:
		ret = dsim_reg_set_smddi_ulps(id, en, lanes);
		break;
	case TYPE_OF_MAGNA_DDI:
		cal_log_err(id, "The ddi(%d) doesn't support ULPS\n", ddi_type);
		ret = -EINVAL;
		break;
	case TYPE_OF_NORMAL_DDI:
	default:
		ret = dsim_reg_set_ulps(id, en, lanes);
		break;
	}

	if (ret < 0) {
		cal_log_err(id, "failed to %s ULPS", en ? "enter" : "exit");
		dsim_dump(get_dsim_drvdata(id));
	}

	return ret;
}

static void dsim_reg_set_dphy_shadow_update_req(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	if ((exynos_soc_info.product_id == S5E9955_SOC_ID) &&
		(exynos_soc_info.main_rev == 0))
		dsim_write_option_suite_mask(id, val,
			DSIM_OPTION_SUITE_CFG_UPDT_EN_MASK);
	else
		dsim_write_mask(id, DSIM_OPTION_SUITE, val,
			DSIM_OPTION_SUITE_CFG_UPDT_EN_MASK);
}

void dsim_reg_set_cmd_allow(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	if ((exynos_soc_info.product_id == S5E9955_SOC_ID) &&
		(exynos_soc_info.main_rev == 0))
		dsim_write_option_suite_mask(id, val,
			DSIM_OPTION_SUITE_OPT_TE_ON_CMD_ALLOW_MASK);
	else
		dsim_write_mask(id, DSIM_OPTION_SUITE, val,
			DSIM_OPTION_SUITE_OPT_TE_ON_CMD_ALLOW_MASK);
}

static void dsim_reg_set_dphy_use_shadow(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON1, val,
			DSIM_PHY_USE_SDW_MASK);
}

static void dsim_reg_set_dphy_pll_stable_cnt(u32 id, u32 cnt)
{
	u32 val = DSIM_PHY_PLL_STB_CNT(cnt);

	dsim_phy_write_mask(id, DSIM_PHY_PLL_CON4, val,
			DSIM_PHY_PLL_STB_CNT_MASK);
}

void dsim_regs_desc_init(void __iomem *regs, const char *name,
		enum dsim_regs_type type, unsigned int id)
{
	cal_regs_desc_check(type, id, REGS_DSIM_TYPE_MAX, MAX_DSI_CNT);
	cal_regs_desc_set(regs_desc, regs, name, type, id);
}

static void dpu_sysreg_select_dphy_rst_control(u32 id, u32 sel)
{
	u32 phy_num = id;
	u32 val = sel ? ~0 : 0;
	u32 mask = SEL_RESET_DPHY_MASK(phy_num);

	dsim_sys_write_mask(id, DPU_MIPI_PHY_CON, val, mask);
	cal_log_debug(id, "%s: phy_con_sel val=0x%x", __func__,
			dsim_sys_read(id, DPU_MIPI_PHY_CON));
}

#if !defined(CONFIG_BOARD_EMULATOR)
static void dpu_sysreg_dphy_reset(u32 id, u32 rst)
{
	u32 val = rst ? ~0 : 0;
	u32 mask = id ? M_RESETN_M1_MASK : M_RESETN_M0_MASK;

	dsim_sys_write_mask(id, DPU_MIPI_PHY_CON, val, mask);
}
#endif

/******************** EXPORTED DSIM CAL APIs ********************/

void dsim_reg_init(u32 id, struct dsim_reg_config *config,
		struct dsim_clks *clks, bool panel_ctrl)
{
	u32 lanes;

	/* DPHY reset control from SYSREG(0) */
	dpu_sysreg_select_dphy_rst_control(id, 0);

	lanes = DSIM_LANE_CLOCK | GENMASK(config->data_lane_cnt, 1);

	/* choose OSC_CLK */
	dsim_reg_set_link_clock(id, 0);

	dsim_reg_sw_reset(id);

	dsim_reg_set_lanes(id, lanes, 1);

	dsim_reg_set_esc_clk_on_lane(id, 1, lanes);

	dsim_reg_enable_word_clock(id, 1);

	dsim_reg_osc_gate_cond_sel(id, 1);

#if !defined(CONFIG_BOARD_EMULATOR)
	/* Enable DPHY reset : DPHY reset start */
	dpu_sysreg_dphy_reset(id, 0);

	dsim_reg_set_clocks(id, clks, config, 1);

	dsim_reg_set_lanes_dphy(id, lanes, true);

	dpu_sysreg_dphy_reset(id, 1); /* Release DPHY reset */
#endif

	dsim_reg_set_link_clock(id, 1);	/* Selection to word clock */

	dsim_reg_set_config(id, config, clks);

#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	dsim_reg_set_pll_sleep_enable(id, true); /* PHY pll sleep enable */
	dsim_reg_set_pll_sleep_self_ctrl(id, true); /* PHY pll sleep auto-control */
#endif
}

/* Set clocks and lanes and HS ready */
void dsim_reg_start(u32 id)
{
	dsim_reg_set_hs_clock(id, 1);
	dsim_reg_set_int(id, 1);
	dsim_reg_clear_int(id, 0xffffffff);
}

/* Unset clocks and lanes and stop_state */
int dsim_reg_stop(u32 id, u32 lanes)
{
	int err = 0;
	u32 is_vm;

#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	bool pll_sleep_state = false;

	if (dsim_reg_is_pll_sleep_state(id)) {
		cal_log_debug(id, "pll is sleep state\n");
		pll_sleep_state = true;
	}
	dsim_reg_set_pll_sleep_block(id, true);
	if (pll_sleep_state)
		dsim_reg_wait_exit_pll_sleep(id);
#endif

	/* 0. wait the IDLE status */
	is_vm = dsim_reg_get_display_mode(id);
	err = dsim_reg_wait_idle_status(id, is_vm);
	if (err < 0)
		cal_log_err(id, "DSIM status is not IDLE!\n");

	dsim_reg_clear_int(id, 0xffffffff);
	/* disable interrupts */
	dsim_reg_set_int(id, 0);

	/* first disable HS clock */
	if (dsim_reg_set_hs_clock(id, 0) < 0)
		cal_log_err(id, "CLK lane doesn't be switched to LP mode\n");

	/* 1. clock selection : OSC */
	dsim_reg_set_link_clock(id, 0);

#if !defined(CONFIG_BOARD_EMULATOR)
	/* 2. master resetn */
	dpu_sysreg_dphy_reset(id, 0);
	/* 3. disable lane */
	dsim_reg_set_lanes_dphy(id, lanes, false);
#endif

	/* 4. turn off WORDCLK and ESCCLK */
	dsim_reg_set_esc_clk_on_lane(id, 0, lanes);
	dsim_reg_set_esc_clk_en(id, 0);

#if !defined(CONFIG_BOARD_EMULATOR)
	/* 5. disable PLL */
	dsim_reg_set_clocks(id, NULL, NULL, 0);
#endif

	if (err == 0)
		dsim_reg_sw_reset(id);

	return err;
}

void dsim_reg_recovery_process(u32 id)
{
	cal_log_debug(id, "+\n");

	/* 0. disable HS clock */
	if (dsim_reg_set_hs_clock(id, 0) < 0)
		cal_log_warn(id, "The CLK lane doesn't be switched to LP mode\n");

	/* 1. clock selection : OSC */
	dsim_reg_set_link_clock(id, 0);

	/* 2. reset & release */
#if !defined(CONFIG_BOARD_EMULATOR)
	dpu_sysreg_dphy_reset(id, 0);
#endif
	dsim_reg_function_reset(id);
#if !defined(CONFIG_BOARD_EMULATOR)
	dpu_sysreg_dphy_reset(id, 1);
#endif

	/* 3. clock selection : PLL */
	dsim_reg_set_link_clock(id, 1);

	/* 4. enable HS clock */
	dsim_reg_set_hs_clock(id, 1);

	cal_log_debug(id, "-\n");
}

/* Exit ULPS mode and set clocks and lanes */
int dsim_reg_exit_ulps_and_start(u32 id, u32 ddi_type, u32 lanes)
{
	int ret = 0;

	/* try to exit ULPS mode. The sequence is depends on DDI type */
	ret = dsim_reg_set_ulps_by_ddi(id, ddi_type, lanes, 0);
	dsim_reg_set_esc_clk_on_lane(id, 1, lanes);
	dsim_reg_start(id);
	return ret;
}

/* Unset clocks and lanes and enter ULPS mode */
int dsim_reg_stop_and_enter_ulps(u32 id, u32 ddi_type, u32 lanes)
{
	int ret = 0;
	int err = 0;
	u32 is_vm;

#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	bool pll_sleep_state = false;

	if (dsim_reg_is_pll_sleep_state(id)) {
		cal_log_debug(id, "pll is sleep state\n");
		pll_sleep_state = true;
	}
	dsim_reg_set_pll_sleep_block(id, true);
	if (pll_sleep_state)
		dsim_reg_wait_exit_pll_sleep(id);
#endif

	/* 0. wait the IDLE status */
	is_vm = dsim_reg_get_display_mode(id);
	err = dsim_reg_wait_idle_status(id, is_vm);
	if (err < 0)
		cal_log_err(id, "DSIM status is not IDLE!\n");

	dsim_reg_clear_int(id, 0xffffffff);
	/* disable interrupts */
	dsim_reg_set_int(id, 0);

	/* 1. turn off clk lane & wait for stopstate_clk */
	ret = dsim_reg_set_hs_clock(id, 0);
	if (ret < 0)
		cal_log_err(id, "CLK lane doesn't be switched to LP mode\n");

	/* 2. enter to ULPS & wait for ulps state of clk and data */
	dsim_reg_set_ulps_by_ddi(id, ddi_type, lanes, 1);

	/* 3. sequence for BLK_DPU off */
	/* 3.1 wait idle */
	is_vm = dsim_reg_get_display_mode(id);
	ret = dsim_reg_wait_idle_status(id, is_vm);
	if (ret < 0)
		cal_log_err(id, "%s : DSIM_status is not IDLE!\n", __func__);
	/* 3.2 OSC clock */
	dsim_reg_set_link_clock(id, 0);
	/* 3.3 off DPHY */
	dsim_reg_set_lanes_dphy(id, lanes, false);
	dsim_reg_set_clocks(id, NULL, NULL, 0);
	dsim_reg_set_esc_clk_on_lane(id, 0, lanes);

	/* 3.4 sw reset */
	dsim_reg_sw_reset(id);

	return ret;
}

int dsim_reg_get_int_and_clear(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_INTSRC);
	dsim_reg_clear_int(id, val);

	return val;
}

void dsim_reg_clear_int(u32 id, u32 int_src)
{
	dsim_write(id, DSIM_INTSRC, int_src);
}

int dsim_reg_get_int(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_INTSRC);

	return val;
}

u32 dsim_reg_dbg_arg_status(u32 id)
{
	u32 val;

	val = dsim_read(id, 0x018c);

	return val;
}

u32 dsim_reg_vt_vstatus(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_LINK_STATUS0);

	return ((val >> 13) & 0x1f) ;
}

void dsim_reg_wait_clear_int(u32 id, u32 int_num)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dsim_regs_desc(id)->regs + DSIM_INTSRC,
					val, !(val & int_num), 10, 30000);

	if (ret)
		cal_log_err(id,
			"timeout to wait for clear val(0x%08x) of int(0x%08x)\n",
			val, int_num);
}

int dsim_reg_get_link_clock(u32 id)
{
	int val = 0;

	val = dsim_read_mask(id, DSIM_CLK_CTRL, DSIM_CLK_CTRL_CLOCK_SEL);

	return val;
}

void dsim_reg_wr_tx_header(u32 id, u8 d_id, u8 d0, u8 d1, bool bta)
{
	u32 val = DSIM_PKTHDR_BTA_TYPE(bta) | DSIM_PKTHDR_ID(d_id) |
		DSIM_PKTHDR_DATA0(d0) | DSIM_PKTHDR_DATA1(d1);

	dsim_write_mask(id, DSIM_PKTHDR, val, DSIM_PKTHDR_DATA);
}

void dsim_reg_wr_tx_payload(u32 id, u32 payload)
{
	dsim_write(id, DSIM_PAYLOAD, payload);
}

u32 dsim_reg_header_fifo_is_empty(u32 id)
{
	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_PH_SFR);
}

bool dsim_reg_is_writable_fifo_state(u32 id)
{
	u32 val = dsim_read(id, DSIM_FIFOCTRL);
	bool ret;

	ret = DSIM_FIFOCTRL_NUMBER_OF_PH_SFR_GET(val) < DSIM_FIFOCTRL_THRESHOLD;

	return ret;
}

u32 dsim_reg_payload_fifo_is_empty(u32 id)
{

	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_PL_SFR);
}

int dsim_reg_wait_pl_fifo_is_empty_timeout(u32 id, unsigned long timeout_us)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dsim_regs_desc(id)->regs + DSIM_FIFOCTRL,
			val, (val & DSIM_FIFOCTRL_EMPTY_PL_SFR), 10, timeout_us);

	if (ret) {
		cal_log_err(id, "failed to flush tx-fifo of DSIM%d\n", id);
		return ret;
	}

	return 0;
}

bool dsim_reg_is_writable_ph_fifo_state(u32 id, u32 cmd_cnt)
{
	u32 val = dsim_read(id, DSIM_FIFOCTRL);

	val = DSIM_FIFOCTRL_NUMBER_OF_PH_SFR_GET(val);
	val += cmd_cnt;

	if (val < DSIM_PH_FIFOCTRL_THRESHOLD)
		return true;
	else
		return false;
}

void dsim_reg_wr_tx_sync_header(u32 id, u8 d_id, u8 d0, u8 d1, bool bta)
{
	u32 val = DSIM_PKTHDR_SYNC_BTA_TYPE(bta) | DSIM_PKTHDR_SYNC_ID(d_id) |
		DSIM_PKTHDR_SYNC_DATA0(d0) | DSIM_PKTHDR_SYNC_DATA1(d1);

	dsim_write_mask(id, DSIM_PKTHDR_SYNC, val, DSIM_PKTHDR_SYNC_DATA);
}

void dsim_reg_wr_tx_sync_payload(u32 id, u32 payload)
{
	dsim_write(id, DSIM_PAYLOAD_SYNC, payload);
}

u32 dsim_reg_sync_header_fifo_is_empty(u32 id)
{
	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_SYNC_PH_SFR);
}

bool dsim_reg_is_writable_sync_fifo_state(u32 id)
{
	u32 val = dsim_read(id, DSIM_FIFOCTRL);
	bool ret;

	ret = DSIM_FIFOCTRL_NUMBER_OF_SYNC_PH_SFR_GET(val) < DSIM_SYNC_FIFOCTRL_THRESHOLD;

	return ret;
}

u32 dsim_reg_sync_payload_fifo_is_empty(u32 id)
{

	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_SYNC_PL_SFR);
}

int dsim_reg_wait_sync_pl_fifo_is_empty_timeout(u32 id, unsigned long timeout_us)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dsim_regs_desc(id)->regs + DSIM_FIFOCTRL,
			val, (val & DSIM_FIFOCTRL_EMPTY_SYNC_PL_SFR), 10, timeout_us);

	if (ret) {
		cal_log_err(id, "failed to flush tx-sync-fifo of DSIM%d\n", id);
		return ret;
	}

	return 0;
}

bool dsim_reg_is_writable_sync_ph_fifo_state(u32 id, u32 cmd_cnt)
{
	u32 val = dsim_read(id, DSIM_FIFOCTRL);

	val = DSIM_FIFOCTRL_NUMBER_OF_SYNC_PH_SFR_GET(val);
	val += cmd_cnt;

	if (val < DSIM_SYNC_PH_FIFOCTRL_THRESHOLD)
		return true;
	else
		return false;
}

int dsim_reg_get_sync_pl_fifo_remain_bytes(u32 id)
{
	u32 val = dsim_read(id, DSIM_SYNC_FIFO_STATUS);

	/* register value unit: MEM_WIDTH-bit, 8bytes */
	return (DSIM_SYNC_FIFO_CMD_PL_FIFO_REMAIN_GET(val) * 8);
}

bool dsim_reg_get_frame_processing(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_MIPI_STATUS);

	return mask_value(val, DSIM_MIPI_STATUS_FRM_PROCESSING,
			DSIM_MIPI_STATUS_FRM_PROCESSING_SHIFT);
}

int dsim_reg_wait_frame_processing(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dsim_regs_desc(id)->regs + DSIM_MIPI_STATUS,
			val, (val & DSIM_MIPI_STATUS_FRM_PROCESSING), 10, 10000);

	if (ret) {
		cal_log_err(id, "failed to wait frame processing of DSIM%d\n", id);
		return ret;
	} else
		cal_log_debug(id, "succeeded to wait frame processing of DSIM%d\n", id);

	return 0;
}

int dsim_reg_wait_frame_idle(u32 id)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(dsim_regs_desc(id)->regs + DSIM_MIPI_STATUS,
			val, !(val & DSIM_MIPI_STATUS_FRM_PROCESSING), 10, 10000);

	if (ret) {
		cal_log_err(id, "failed to wait frame idle of DSIM%d\n", id);
		return ret;
	} else
		cal_log_debug(id, "succeeded to wait frame idle of DSIM%d\n", id);

	return 0;
}

u32 dsim_reg_get_rx_fifo(u32 id)
{
	return dsim_read(id, DSIM_RXFIFO);
}

u32 dsim_reg_rx_fifo_is_empty(u32 id)
{
	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_RX);
}

int dsim_reg_rx_err_handler(u32 id, u32 rx_fifo)
{
	int ret = 0;
	u32 err_bit = rx_fifo >> 8; /* Error_Range [23:8] */

	if ((err_bit & MIPI_DSI_ERR_BIT_MASK) == 0) {
		cal_log_debug(id, "Non error reporting format (rx_fifo=0x%x)\n",
				rx_fifo);
		return ret;
	}

	/* Parse error report bit*/
	if (err_bit & MIPI_DSI_ERR_SOT)
		cal_log_err(id, "SoT error!\n");
	if (err_bit & MIPI_DSI_ERR_SOT_SYNC)
		cal_log_err(id, "SoT sync error!\n");
	if (err_bit & MIPI_DSI_ERR_EOT_SYNC)
		cal_log_err(id, "EoT error!\n");
	if (err_bit & MIPI_DSI_ERR_ESCAPE_MODE_ENTRY_CMD)
		cal_log_err(id, "Escape mode entry command error!\n");
	if (err_bit & MIPI_DSI_ERR_LOW_POWER_TRANSMIT_SYNC)
		cal_log_err(id, "Low-power transmit sync error!\n");
	if (err_bit & MIPI_DSI_ERR_HS_RECEIVE_TIMEOUT)
		cal_log_err(id, "HS receive timeout error!\n");
	if (err_bit & MIPI_DSI_ERR_FALSE_CONTROL)
		cal_log_err(id, "False control error!\n");
	if (err_bit & MIPI_DSI_ERR_ECC_SINGLE_BIT)
		cal_log_err(id, "ECC error, single-bit(detect and correct)\n");
	if (err_bit & MIPI_DSI_ERR_ECC_MULTI_BIT)
		cal_log_err(id, "ECC error, multi-bit(detect, not correct)\n");
	if (err_bit & MIPI_DSI_ERR_CHECKSUM)
		cal_log_err(id, "Checksum error(long packet only)!\n");
	if (err_bit & MIPI_DSI_ERR_DATA_TYPE_NOT_RECOGNIZED)
		cal_log_err(id, "DSI data type not recognized!\n");
	if (err_bit & MIPI_DSI_ERR_VCHANNEL_ID_INVALID)
		cal_log_err(id, "DSI VC ID invalid!\n");
	if (err_bit & MIPI_DSI_ERR_INVALID_TRANSMIT_LENGTH)
		cal_log_err(id, "Invalid transmission length!\n");

	cal_log_err(id, "(rx_fifo=0x%x) Check DPHY values about HS clk.\n",
			rx_fifo);
	return -EINVAL;
}

/*
 * 0 = Updated Register : operating
 * 1 = Shadow Register  : programming
 */
void dsim_reg_enable_shadow_read(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_SFR_CTRL, val,
				DSIM_SFR_CTRL_SHADOW_REG_READ_EN);
}

void dsim_reg_function_reset(u32 id)
{
	u32 val;
	int ret;

	dsim_write_mask(id, DSIM_SWRST, ~0, DSIM_SWRST_FUNCRST);
	ret = readl_poll_timeout_atomic(dsim_regs_desc(id)->regs + DSIM_SWRST,
			val, !(val & DSIM_SWRST_FUNCRST), 10, 2000);
	if (ret)
		cal_log_err(id, "dsim%d function reset timeout\n", id);
}

/* Set porch and resolution to support Partial update */
void dsim_reg_set_partial_update(u32 id, struct dsim_reg_config *config)
{
	u32 threshold;
	u32 num_of_transfer;
	u32 xres, yres;
	u32 overlap = 0; /* not yet supported */

	yres = config->p_timing.vactive;
	if (config->dsc.enabled) {
		threshold = get_comp_dsc_width(&config->dsc, config->bpc, 4) *
							config->dsc.slice_count;
		xres = threshold;
		/* use 1-line transfer only */
		num_of_transfer = config->p_timing.vactive;
	} else {
		threshold = config->p_timing.hactive + overlap;
		xres = config->p_timing.hactive + overlap;
		num_of_transfer = (config->p_timing.hactive + overlap) *
			config->p_timing.vactive / threshold;
	}

	dsim_reg_set_threshold(id, threshold);
	dsim_reg_set_resol(id, xres, yres);
	dsim_reg_set_num_of_transfer(id, num_of_transfer);
	dsim_reg_set_cprl_ctrl(id, config);
}

void dsim_reg_set_mres(u32 id, struct dsim_reg_config *config)
{
	u32 threshold;
	u32 num_of_slice;
	u32 num_of_transfer;
	u32 xres, yres;
	u32 overlap = 0; /* not yet supported */

	if (config->mode == DSIM_COMMAND_MODE)
		dsim_reg_set_cm_underrun_lp_ref(id, config->cmd_underrun_cnt);

	yres = config->p_timing.vactive;
	if (config->dsc.enabled) {
		threshold = get_comp_dsc_width(&config->dsc, config->bpc, 4) *
							config->dsc.slice_count;
		xres = threshold;
		/* use 1-line transfer only */
		num_of_transfer = config->p_timing.vactive;
	} else {
		threshold = config->p_timing.hactive + overlap;
		xres = config->p_timing.hactive + overlap;
		num_of_transfer = (config->p_timing.hactive + overlap) *
			config->p_timing.vactive / threshold;
	}

	dsim_reg_set_threshold(id, threshold);
	dsim_reg_set_resol(id, xres, yres);
	dsim_reg_set_porch(id, config);
	dsim_reg_set_num_of_transfer(id, num_of_transfer);

	dsim_reg_enable_dsc(id, config->dsc.enabled);
	if (config->dsc.enabled) {
		cal_log_debug(id, "%s: dsc configuration is set\n", __func__);
		dsim_reg_set_num_of_slice(id, config->dsc.slice_count);
		dsim_reg_set_multi_slice(id, config); /* multi slice */
		dsim_reg_set_size_of_slice(id, config);

		dsim_reg_get_num_of_slice(id, &num_of_slice);
		cal_log_debug(id, "number of DSC slice(%d)\n", num_of_slice);
		dsim_reg_print_size_of_slice(id);
	}
}

void dsim_reg_set_bist(u32 id, bool en, u32 mode)
{
	if (en) {
		dsim_reg_set_bist_te_interval(id, 4505);
		dsim_reg_set_bist_mode(id, mode);
		dsim_reg_enable_bist_pattern_move(id, true);
	}

	dsim_reg_enable_bist(id, en);
}

void dsim_reg_set_cmd_transfer_mode(u32 id, u32 lp)
{
	u32 val = lp ? ~0 : 0;

	dsim_write_mask(id, DSIM_ESCMODE, val, DSIM_ESCMODE_CMD_LPDT);
}

u32 dsim_reg_get_cmd_tansfer_mode(u32 id)
{
	u32 val;

	val = dsim_read_mask(id, DSIM_ESCMODE, DSIM_ESCMODE_CMD_LPDT);
	return val ? 1 : 0;
}

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
void dsim_reg_set_cmd_access_mode(u32 id, u32 dma_en)
{
	u32 val = dma_en ? ~0 : 0;

	dsim_write_mask(id, DSIM_TRANS_MODE, val, DSIM_CMD_ACCESS_MODE_MASK);
}

u32  dsim_reg_get_cmd_access_mode(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_TRANS_MODE);
	return DSIM_CMD_ACCESS_MODE_GET(val);
}
#endif

static bool dsim_reg_check_clklane_stuck(u32 id, u32 link, u32 dphy)
{
	u32 data = 0x0;

	data = mask_value(link, DSIM_LINK_STATUS2_HSCLK_STATUS,
			DSIM_LINK_STATUS2_HSCLK_STATUS_SHIFT);

	if (data == 0x1) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_CLK,
				DSIM_DPHY_STATUS_STOPSTATE_CLK_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HSCLK,
				DSIM_DPHY_STATUS_TX_READY_HSCLK_SHIFT) != 0x0)
			goto exit;

		cal_log_info(id, ":recovery:matched=lp2hs\n");
	} else if (data == 0x05) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_CLK,
				DSIM_DPHY_STATUS_STOPSTATE_CLK_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HSCLK,
				DSIM_DPHY_STATUS_TX_READY_HSCLK_SHIFT) != 0x1)
			goto exit;

		cal_log_info(id, ":recovery:matched=hs2lp(included datalane)\n");
	} else {
		goto exit;
	}

	return true;
exit:
	cal_log_info(id, ":recovery: not matched\n");
	return false;
}

static bool dsim_reg_check_datalane_stuck(u32 id, u32 link, u32 dphy)
{
	u32 data = 0x0;

	data = mask_value(link, DSIM_LINK_STATUS2_HSDT_STATUS,
			DSIM_LINE_STATUS2_HSDT_STATUS_SHIFT);

	if (data == 0x02) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA,
				DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA_SHIFT) != 0x1)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_DATA,
				DSIM_DPHY_STATUS_STOPSTATE_DATA_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HS_DATA,
				DSIM_DPHY_STATUS_TX_READY_HS_DATA_SHIFT) != 0x0)
			goto exit;
		cal_log_info(id, "recovery:%s:matched=lp2hs\n", __func__);
	} else if (data == 0x03) {
		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA,
				DSIM_DPHY_STATUS_TX_REQUEST_HS_DATA_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_STOPSTATE_DATA,
				DSIM_DPHY_STATUS_STOPSTATE_DATA_SHIFT) != 0x0)
			goto exit;

		if (mask_value(dphy, DSIM_DPHY_STATUS_TX_READY_HS_DATA,
				DSIM_DPHY_STATUS_TX_READY_HS_DATA_SHIFT) != 0x1)
			goto exit;
		cal_log_info(id, "recovery:%s:matched=hs2lp\n", __func__);
	} else {
		goto exit;
	}

	return true;
exit:
	cal_log_info(id, "recovery:%s:not matched\n", __func__);
	return false;
}

bool dsim_reg_check_ppi_stuck(u32 id)
{
	u32 link = 0x0;
	u32 dphy = 0x0;
	bool ret = false;

	link = dsim_read(id, DSIM_LINK_STATUS2);
	dphy = dsim_read(id, DSIM_DPHY_STATUS);

	if (dsim_reg_check_clklane_stuck(id, link, dphy) ||
		dsim_reg_check_datalane_stuck(id, link, dphy))
		ret = true;

	return ret;
}

void dsim_reg_set_datalane_state(u32 id)
{
	u32 val, mask;

	val = DSIM_CSIS_LB_DBG_SW_P_DATA
		| DSIM_CSIS_LB_DBG_SW_P_ADDR;

	mask = DSIM_CSIS_LB_DBG_SW_P_DATA_MASK
		| DSIM_CSIS_LB_DBG_SW_P_ADDR_MASK;

	dsim_write_mask(id, DSIM_CSIS_LB, val, mask);
}

static void dsim_reg_update_dphy_timing(u32 id, u32 hs_clk, u32 hop_hs_clk)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct dphy_timing_value t;
	u32 hsmode = 0;
	enum dsim_wide_freq_hop_range old_range, new_range;
	u32 dphy_timing_hs_clk;

	old_range
		= dsim_check_wide_freq_hop_range(dsim->clk_param.hs_clk, hs_clk);
	new_range
		= dsim_check_wide_freq_hop_range(dsim->clk_param.hs_clk, hop_hs_clk);

	if (old_range == new_range)
		return;

	if (new_range == DSIM_WIDE_FRE_HOP_MID)
		dphy_timing_hs_clk = dsim->clk_param.hs_clk;
	else
		dphy_timing_hs_clk = hop_hs_clk;

	if (dphy_timing_hs_clk < SKEW_CAL_REF_CLOCK)
		hsmode = 1;

	/* get DPHY timing values using hs clock and escape clock */
	dsim_reg_get_dphy_timing(id, dphy_timing_hs_clk, dsim->clk_param.esc_clk, &t);
	dsim_reg_set_dphy_timing_values(id, &t, hsmode);

	cal_log_debug(id, "dphy timing values are changed for %uMhz\n",
			dphy_timing_hs_clk);
}

void dsim_reg_set_dphy_freq_hopping(u32 id, u32 p, u32 m, u32 k, u32 en)
{
	u32 val, mask;
	u32 pll_stable_cnt = (PLL_SLEEP_CNT_MULT + PLL_SLEEP_CNT_MARGIN) * p;
	u32 time_te_protect_on;
	u32 time_te_tout;
	u32 pll_freq, hop_pll_freq;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (en) {
		dsim_reg_set_pll_sleep_block(id, true);

		dsim_reg_set_dphy_use_shadow(id, 1);
		if (pll_stable_cnt < (AP_OSC_KHZ / 10))
			pll_stable_cnt = AP_OSC_KHZ / 10; /* recommended: 100us */
		dsim_reg_set_dphy_pll_stable_cnt(id, pll_stable_cnt);

		pll_freq = dsim_reg_get_pll_freq(id);

		/* M value */
		val = DSIM_PHY_PMS_M(m);
		mask = DSIM_PHY_PMS_M_MASK;
		dsim_phy_write_mask(id, DSIM_PHY_PLL_CON1, val, mask);

		/* F value */
		val = DSIM_PHY_PMS_F(k);
		mask = DSIM_PHY_PMS_F_MASK;
		dsim_phy_write_mask(id, DSIM_PHY_PLL_CON6, val, mask);

		hop_pll_freq = dsim_reg_get_pll_freq(id);
		dsim_reg_get_cmd_timer(dsim->config.p_timing.vrefresh,
				&time_te_protect_on,
				&time_te_tout,
				hop_pll_freq);
		dsim_reg_set_cmd_te_ctrl1(id, time_te_protect_on, time_te_tout);

		cal_log_debug(id, "old_hs_clk(%uMhz) -> new_hs_clk(%uMhz)\n",
				pll_freq, hop_pll_freq);

		if (dsim->freq_hop && dsim->freq_hop->wide_enabled)
			dsim_reg_update_dphy_timing(id, pll_freq, hop_pll_freq);

		if (!dsim->config.vmc_en)
			dsim_reg_set_dphy_shadow_update_req(id, 1);
	} else {
		dsim_reg_set_dphy_use_shadow(id, 0);
		dsim_reg_set_dphy_shadow_update_req(id, 0);
		dsim_reg_set_pll_sleep_block(id, false);
	}
}

int dsim_reg_vhm_hopping_pre_process(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	int ret = 0;
	u32 val, vt_state;

	if (!dsim->freq_hop->vhm_hopping)
		return 0;

	dsim->freq_hop->vhm_idle = false;

	spin_lock_irq(&dsim->slock);

	val = dsim_read(id, DSIM_LINK_STATUS0);
	vt_state = DSIM_LINK_STATUS0_VT_VSTATE_GET(val);

	switch (vt_state) {
	case DSIM_LINK_STATUS0_VT_VSTATE_VACT:
	case DSIM_LINK_STATUS0_VT_VSTATE_VSA:
	case DSIM_LINK_STATUS0_VT_VSTATE_VBP:
		__decon_trig_control_for_dsim_shadow(id, 0);
		dsim_reg_enable_shadow(id, 0);
		dsim_reg_set_freq_hopp(id, true);
		dsim_reg_enable_shadow(id, 1);
		__decon_trig_control_for_dsim_shadow(id, 1);
		break;
	case DSIM_LINK_STATUS0_VT_VSTATE_VFP:
		if (dsim_reg_get_frame_processing(id) &&
			dsim_reg_wait_frame_idle(id))
			dsim->freq_hop->vhm_hopping = false;
		fallthrough;
	case DSIM_LINK_STATUS0_VT_VSTATE_VIDLE:
		if (dsim->freq_hop->vhm_hopping) {
			dsim->freq_hop->vhm_idle = true;
			if (dsim_reg_set_hs_clock(id, 0) < 0) {
				cal_log_err(id, "CLK lane doesn't be switched to LP mode\n");
				dsim->freq_hop->vhm_hopping = false;
			}
		}
		break;
	default:
		cal_log_err(id, "undefined vtstate\n");
		dsim->freq_hop->vhm_hopping = false;
		break;
	}

	if (dsim->freq_hop->vhm_hopping) {
		dsim_reg_set_dphy_shadow_update_req(id, 1);
		dsim_reg_set_hperiod(id, &dsim->config,
				dsim_reg_get_pll_freq(id));
	} else
		cal_log_err(id, "timing that vhm_hop can't be performed\n");

	if (!dsim->freq_hop->vhm_idle)
		spin_unlock_irq(&dsim->slock);

	cal_log_debug(id, "%d: vhm_idle(%d), VSTATE(%x), linecount(%d)\n",
			__LINE__, dsim->freq_hop->vhm_idle, vt_state,
			dsim_reg_get_linecount(id, dsim->config));

	return ret;
}

int dsim_reg_vhm_hopping_post_process(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	int ret = 0;

	if (!dsim->freq_hop->vhm_hopping)
		return 0;

	if (dsim->freq_hop->vhm_idle) {
		ret = dsim_reg_wait_phy_config_idle(id); /* 1msec */
		dsim_reg_set_hs_clock(id, 1);
		spin_unlock_irq(&dsim->slock);
	} else {
		ret |= dsim_reg_wait_freq_hopp_timeout(id, 10000); /* 10msec */
		ret |= dsim_reg_wait_phy_config_idle(id); /* 1msec */
	}

	cal_log_debug(id, "%d: vhm_idle(%d), VSTATE(%x), linecount(%d)\n",
		__LINE__, dsim->freq_hop->vhm_idle,
		DSIM_LINK_STATUS0_VT_VSTATE_GET(dsim_read(id, DSIM_LINK_STATUS0)),
		dsim_reg_get_linecount(id, dsim->config));

	dsim->freq_hop->vhm_idle = false;

	if (ret)
		cal_log_err(id, "frequency hopping fail!!!\n");

	return ret;
}


static void dsim_reg_config_new_opmode(u32 id, struct dsim_reg_config *config,
		struct dsim_clks *clks)
{
	bool is_cmd = config->mode == DSIM_COMMAND_MODE ? true : false;

	dsim_reg_enable_noncont_clock(id, is_cmd);

	if (is_cmd) {
		dsim_reg_enable_deskew_hw_enable(id, 0);
	}
	dsim_reg_enable_shadow_vss_update(id, config->mode == DSIM_VIDEO_MODE ? 1 : 0);

	dsim_reg_enable_shadow(id, 1);
	if (is_cmd)
		dsim_reg_set_cm_underrun_lp_ref(id, config->cmd_underrun_cnt);
	else {
		dsim_reg_set_porch(id, config);
		dsim_reg_set_vfp_detail(id,
			config->p_timing.vfp - 2 * DSIM_STABLE_VFP_VALUE,
			DSIM_STABLE_VFP_VALUE);
	}
	dsim_reg_set_video_mode(id, !is_cmd);
	if (!is_cmd) {
		dsim_reg_set_hperiod(id, config, clks->hs_clk);
		dsim_reg_disable_hsa(id, 0);
		dsim_reg_disable_hbp(id, 0);
		if (config->vmc_en) {
			dsim_reg_disable_hfp(id, 0);
			dsim_reg_disable_hse(id, 1);
			dsim_reg_set_burst_mode(id, 0);
		} else {
			dsim_reg_disable_hfp(id, 1);
			dsim_reg_disable_hse(id, 0);
			dsim_reg_set_burst_mode(id, 1);
		}
		dsim_reg_set_sync_inform(id, 0);
		dsim_reg_set_vt_compensate(id, config->vt_compensation);
		if (config->vmc_en) {
			dsim_reg_set_vstatus_int(id, DSIM_VSYNC);
			dsim_reg_set_ext_vt_sync(id, config);
		} else
			dsim_reg_set_vstatus_int(id, DSIM_VFP);
	} else {
		dsim_reg_disable_hse(id, 1);
		dsim_reg_disable_hfp(id, 0);
		dsim_reg_set_cmd_ctrl(id, config, clks);
	}
}

void dsim_reg_opmode_switch(u32 id, struct dsim_reg_config *config, struct dsim_clks *clks)
{
	dsim_reg_config_new_opmode(id, config, clks);
}

#ifndef CONFIG_BOARD_EMULATOR
static void __dphy_dump(u32 id, struct dsim_regs *regs)
{
	int i;

	cal_log_info(id, "=== DSIM %d DPHY SFR DUMP ===\n", id);
	cal_log_info(id, "-[BIAS]-\n");
	dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + BIAS_OFFSET, 0xC);

	cal_log_info(id, "-[PLL]-\n");
	dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + PLL_OFFSET, 0x88);

	cal_log_info(id, "-[MC]-\n");
	dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + MC_OFFSET, 0x54);
	dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + DSIM_PHY_MC_DBG_STAT0, 0x10);

	for (i = 0; i < MAX_DSIM_DATALANE_CNT; i++) {
		cal_log_info(id, "-[CMD %d]-\n", i);
		dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + MD_OFFSET(i), 0x70);
		dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + DSIM_PHY_MD_DBG_STAT0(i),
				0x40);
	}

	cal_log_info(id, "-[MD 3]-\n");
	dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + MD_OFFSET(3), 0x70);
	dpu_print_hex_dump(regs->phy_regs, regs->phy_regs + DSIM_PHY_MD_DBG_STAT0(3), 0x20);
}
#else
static inline void __dphy_dump(u32 id, struct dsim_regs *regs) { }
#endif

void __dsim_dump(u32 id, struct dsim_regs *regs)
{
	/* change to updated register read mode (meaning: SHADOW in DECON) */
	cal_log_info(id, "=== DSIM %d LINK SFR DUMP ===\n", id);
	dsim_reg_enable_shadow_read(id, 0);
	dpu_print_hex_dump(regs->regs, regs->regs + 0x0000, 0x190);

#if IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)
	dpu_print_hex_dump(regs->regs, regs->regs + 0x0190, 0x4);
	dpu_print_hex_dump(regs->regs, regs->regs + 0x0250, 0x10);
#endif

	__dphy_dump(id, regs);

	/* restore to avoid size mismatch (possible config error at DECON) */
	dsim_reg_enable_shadow_read(id, 1);
}
