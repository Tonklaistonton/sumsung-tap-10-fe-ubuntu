// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Exynos Pablo IS CSIS_PDP TOP HW control functions
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[CSIS_PDP_TOP]" fmt

#include "pablo-hw-api-common.h"
#include "is-common-config.h"
#include "exynos-is-sensor.h"
#include "pablo-smc.h"
#include "sfr/is-sfr-csi-pdp-top-v14_0.h"
#include "api/is-hw-api-csis_pdp_top.h"
#include "is-device-csi.h"

#define CONFIG_IBUF_ERR_INTR_AUTO_RESET 0
#define CONFIG_IBUF_ERR_CHECK_ON 0

enum csis_ibuf_mux_type {
	CSIS_IBUF_MUX_TYPE0, /* LC 0/1/2/3/4/5 */
	CSIS_IBUF_MUX_TYPE1, /* LC 6/7/8/9 */
	CSIS_IBUF_MUX_TYPE2, /* LC 0/1/4/5/8/9 */
	CSIS_IBUF_MUX_TYPE3, /* LC 2/3/6/7 */
};

enum csis_ibuf_ptrn_pd_ratio {
	PD_RATIO_1_TO_2,
	PD_RATIO_1_TO_4,
	PD_RATIO_1_TO_8,
	PD_RATIO_1_TO_16,
	PD_RATIO_NUM,
};

static u32 pd_ratio_div[PD_RATIO_NUM] = { [PD_RATIO_1_TO_2] = 2,
	[PD_RATIO_1_TO_4] = 4,
	[PD_RATIO_1_TO_8] = 8,
	[PD_RATIO_1_TO_16] = 16 };

struct csis_ibuf_err {
	bool err;
	bool overflow;
	bool stuck;
	bool protocol;
	bool wc_err;	/* different from calc_wc and in_wc */
	bool hsize_err_short;
	bool hsize_err_long;
	u8 lc;
	u8 mem_percent;
	u8 vsize_err_short;	/* vsize short according to vc [16]:vc0, [21]:vc5 */
	u8 vsize_err_long;	/* vsize long according to vc [16]:vc0, [21]:vc5 */
	u32 hcnt;
	u32 vcnt;
	u32 input_wc;
	u32 calc_wc;
};

static u32 _csis_ibuf_get_mux_val(u32 mux_type, u32 csi_ch)
{
	u32 val;

	switch (mux_type) {
	case CSIS_IBUF_MUX_TYPE0:
		val = csi_ch;
		break;
	case CSIS_IBUF_MUX_TYPE1:
		val = csi_ch | (1 << 3);
		break;
	case CSIS_IBUF_MUX_TYPE2:
		val = (1 << 4);
		val |= (csi_ch << 1);
		break;
	case CSIS_IBUF_MUX_TYPE3:
		val = (1 << 4) | 1;
		val |= (csi_ch << 1);
		break;
	default:
		val = 0x3f;
		break;
	}

	return val;
}

static const struct is_reg csis_pdp_top_dbg_cr[] = {
	/* STALL_CNT from PDPs */
	{ 0x0254, "STALL_CNT_PDP0" },
	{ 0x0258, "STALL_CNT_PDP1" },
	{ 0x025c, "STALL_CNT_PDP2" },
	{ 0x0260, "STALL_CNT_PDP3" },
	/* STALL_CNT from BYRPs */
	{ 0x0264, "STALL_CNT_BYRP0" },
	{ 0x0268, "STALL_CNT_BYRP1" },
	{ 0x026c, "STALL_CNT_BYRP2" },
	{ 0x0270, "STALL_CNT_BYRP3" },
	/* FRAME_CNT for CSISs */
	{ 0x0274, "DBG_FRAME_CNT_CSIS0" },
	{ 0x0278, "DBG_FRAME_CNT_CSIS1" },
	{ 0x027c, "DBG_FRAME_CNT_CSIS2" },
	{ 0x0280, "DBG_FRAME_CNT_CSIS3" },
	{ 0x0284, "DBG_FRAME_CNT_CSIS4" },
	{ 0x0288, "DBG_FRAME_CNT_CSIS5" },
	/* Overflow status by byrp stall, ciss wdma stall */
	{ 0x028c, "DBG_OVF_STATUS_BY_STALL" },
};

static void _csis_pdp_top_dump_dbg_state(void __iomem *base)
{
	u32 i;
	const struct is_reg *cr;

	info("[DUMP] CSIS_PDP_TOP DBG ==============\n");
	for (i = 0; i < ARRAY_SIZE(csis_pdp_top_dbg_cr); i++) {
		cr = &csis_pdp_top_dbg_cr[i];

		info("[DUMP] %20s %08x\n", cr->reg_name, is_hw_get_reg(base, cr));
	}
}

static void _csis_pdp_top_dump(void __iomem *base)
{
	info("[DUMP] CSIS_PDP_TOP ==================\n");
	is_hw_dump_regs(base, is_csis_pdp_top_regs, CSIS_PDP_TOP_REG_CNT);
}

static void _csis_pdp_top_ibuf_dump_dbg_state(void __iomem *base, u32 otf_ch)
{
	bool dbg_on;
	u32 dbg, in, out;
	u32 lc_a, lc_b;

	base += (otf_ch * CSIS_IBUF_CR_OFFSET);

	dbg = is_hw_get_reg(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_CNTL_0]);
	dbg_on = is_hw_get_field_value(dbg, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_ON]);
	if (!dbg_on)
		return;

	lc_a = is_hw_get_field_value(dbg, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_LOCH_A]);
	lc_b = is_hw_get_field_value(dbg, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_LOCH_B]);

	info("[DUMP] IBUF%d DBG =====================\n", otf_ch);
	info("[DUMP] STALL_CNT 0x%08x\n",
		is_hw_get_reg(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_STALL_CNT]));

	in = is_hw_get_reg(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_IN_CNT_A0]);
	out = is_hw_get_reg(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_OUT_CNT_A0]);

	/* 8ppc for HCNT */
	info("[DUMP][LC%d] %dx%d(%d) -> %dx%d(%d)\n", lc_a,
	     is_hw_get_field_value(in, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_HCNT]) * 8,
	     is_hw_get_field_value(in, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_VCNT]),
	     is_hw_get_field(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_IN_CNT_A1],
			     &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_FCNT]),
	     is_hw_get_field_value(out, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_HCNT]) * 8,
	     is_hw_get_field_value(out, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_VCNT]),
	     is_hw_get_field(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_OUT_CNT_A1],
			     &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_FCNT]));

	in = is_hw_get_reg(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_IN_CNT_B0]);
	out = is_hw_get_reg(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_OUT_CNT_B0]);

	/* 8ppc for HCNT */
	info("[DUMP][LC%d] %dx%d(%d) -> %dx%d(%d)\n", lc_b,
	     is_hw_get_field_value(in, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_HCNT]) * 8,
	     is_hw_get_field_value(in, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_VCNT]),
	     is_hw_get_field(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_IN_CNT_B1],
			     &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_FCNT]),
	     is_hw_get_field_value(out, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_HCNT]) * 8,
	     is_hw_get_field_value(out, &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_VCNT]),
	     is_hw_get_field(base, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_DEBUG_OUT_CNT_B1],
			     &is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_FCNT]));
}

static void _csis_pdp_top_ibuf_dump(void __iomem *base)
{
	void *cr_base;
	u32 cr_offset, ibuf_ch, lc;

	for (ibuf_ch = 0; ibuf_ch < CSIS_IBUF_CH_NUM; ibuf_ch++) {
		cr_offset = CSIS_IBUF_CR_OFFSET * ibuf_ch;
		cr_base = base + cr_offset;

		info("[DUMP] IBUF%d (0x%04X) ================\n", ibuf_ch, cr_offset);
		is_hw_dump_regs(cr_base, is_csis_ibuf_regs, CSIS_IBUF_REG_CNT);

		for (lc = 0; lc < CSIS_IBUF_LC_NUM; lc++) {
			info("[DUMP] LC%d ==================\n", lc);
			is_hw_dump_regs(cr_base, is_csis_ibuf_lc_regs, CSIS_IBUF_LC_REG_CNT);

			cr_base += CSIS_IBUF_LC_CR_OFFSET;
		}
	}

	info("[DUMP] IBUF_CMN =========================\n");
	is_hw_dump_regs(base, is_csis_ibuf_cmn_regs, CSIS_IBUF_CMN_REG_CNT);
}

static void _csis_pdp_top_bns_dump(void __iomem *base)
{
	info("[DUMP] BNS ===========================\n");
	is_hw_dump_regs(base, is_csis_bns_regs, CSIS_BNS_REG_CNT);
}

void csis_pdp_top_frame_id_en(struct is_device_csi *csi, struct is_fid_loc *fid_loc)
{
	struct pablo_camif_csis_pdp_top *top = csi->top;
	struct pablo_camif_otf_info *otf_info = &csi->otf_info;
	u32 val, reg_id;

	if (!top) {
		err("CSIS%d doesn't have top regs.\n", otf_info->csi_ch);
		return;
	}

	reg_id = CSIS_PDP_TOP_R_CSIS0_FRAME_ID_EN + otf_info->csi_ch;

	if (csi->f_id_dec) {
		if (!fid_loc->valid) {
			fid_loc->byte = 27;
			fid_loc->line = 0;
			warn("fid_loc is NOT properly set.\n");
		}

		val = is_hw_get_reg(top->regs, &is_csis_pdp_top_regs[reg_id]);
		val = is_hw_set_field_value(val,
			&is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_FID_LOC_BYTE], fid_loc->byte);
		val = is_hw_set_field_value(val,
			&is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_FID_LOC_LINE], fid_loc->line);
		val = is_hw_set_field_value(
			val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_FRAME_ID_EN_CSIS], 1);

		info("CSIS%d_FRAME_ID_EN:byte %d line %d\n", otf_info->csi_ch, fid_loc->byte,
			fid_loc->line);
	} else {
		val = 0;
	}

	is_hw_set_reg(top->regs, &is_csis_pdp_top_regs[reg_id], val);
}

u32 csis_pdp_top_get_frame_id_en(void __iomem *base_addr, struct is_device_csi *csi)
{
	return is_hw_get_reg(base_addr,
		&is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS0_FRAME_ID_EN + csi->otf_info.csi_ch]);
}

void csis_pdp_top_qch_cfg(void __iomem *base_reg, bool on)
{
	u32 val;

	val = 0;
	val = is_hw_set_field_value(val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_QACTIVE_ON], on);
	val = is_hw_set_field_value(val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_IP_PROCESSING], 1);
	is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS_CTRL], val);

	/* sw_reset */
	info("CSIS_PDP_TOP sw_reset\n");
	is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_SW_RESET_CORE],
		CSIS_TOP_SW_RESET_MASK);

	is_hw_set_field(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_MISC],
		&is_csis_pdp_top_fields[CSIS_PDP_TOP_F_DBG_ON], 1);
}

void csis_pdp_top_irq_msk(void __iomem *base_reg, bool on)
{
	const struct is_field *field = is_csis_pdp_top_fields;
	u32 val, msk;

	msk = on ? CSIS_TOP_IBUF_INTR_EN_MASK : 0;

	is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS_TOP_INT_ENABLE], msk);

	if (!IS_ENABLED(CONFIG_IBUF_ERR_INTR_AUTO_RESET)) {
		val = is_hw_get_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_MISC]);
		val = is_hw_set_field_value(
			val, &field[CSIS_PDP_TOP_F_IBUF_ERR_INTR_CLR_SW_CTRL_EN], 1);
		val = is_hw_set_field_value(
			val, &field[CSIS_PDP_TOP_F_IBUF_ERR_MODULE_SWRESET_AUTO_EN], 0);
		is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_MISC],
			val); /* bring up guide */
	}
}

void csis_pdp_top_s_link_vc_list(
	int *link_vc_list, u32 *mux_val_base, u32 max_vc_num, u32 otf_out_id)
{
	u32 lc, start_vc, end_vc;

	lc = start_vc = end_vc = 0;

	if (max_vc_num <= CSIS_OTF_CH_LC_NUM || otf_out_id == CSI_OTF_OUT_SINGLE) {
		*mux_val_base = CSIS_IBUF_MUX_TYPE0;
		start_vc = 0;
		end_vc = 5;
	} else if (otf_out_id == CSI_OTF_OUT_SHORT) {
		*mux_val_base = CSIS_IBUF_MUX_TYPE1;
		start_vc = 6;
		end_vc = 9;
	}

	while (start_vc <= end_vc)
		link_vc_list[lc++] = start_vc++;
}

void csis_pdp_top_s_otf_out_mux(
	void __iomem *regs, u32 csi_ch, u32 otf_ch, u32 img_vc, u32 mux_val_base, bool en)
{
	u32 mux_val;

	if (en)
		mux_val = _csis_ibuf_get_mux_val(mux_val_base, csi_ch);
	else
		mux_val = 0x3f; /* reset value */

	info("CSI%d -> OTF%d [%s] %s\n", csi_ch, otf_ch,
		(mux_val_base == CSIS_IBUF_MUX_TYPE0) ? "VC0-5" : "VC6-9", en ? "ON" : "OFF");

	is_hw_set_field(regs, &is_csis_ibuf_cmn_regs[CSIS_IBUF_CMN_R_IBUF_MUX0 + otf_ch],
		&is_csis_ibuf_fields[CSIS_IBUF_F_GLUEMUX_IBUF_MUX_SELX], mux_val);
}

void csis_pdp_top_s_otf_lc(void __iomem *regs, u32 otf_ch, u32 *lc)
{
	bool img_otf_out = false;
	u32 cr_offset = otf_ch * CSIS_IBUF_CR_OFFSET;
	u32 img_lc = lc[CAMIF_VC_IMG];

	/* IMG LC */
	is_hw_set_field(regs, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_OTF_IMG_SEL],
		&is_csis_pdp_top_fields[CSIS_PDP_TOP_F_OTF0_IMG_VC_SELECT + otf_ch], img_lc);
	is_hw_set_field(regs + cr_offset, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_CNTL_0],
		&is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_LOCH_A], img_lc);

	if (lc[CAMIF_VC_IMG] < CSIS_OTF_CH_LC_NUM) {
		info("OTF%d -> IMG_LC%d\n", otf_ch, img_lc);
		img_otf_out = true;
	}

	if (lc[CAMIF_VC_HPD] < CSIS_OTF_CH_LC_NUM)
		is_hw_set_field(regs + cr_offset,
			&is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_CNTL_0],
			&is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_LOCH_B], lc[CAMIF_VC_HPD]);

	is_hw_set_field(regs, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_MISC],
		&is_csis_pdp_top_fields[CSIS_PDP_TOP_F_BYRP_PATH0_EN_SEL + otf_ch], img_otf_out);
	is_hw_set_field(regs + cr_offset, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_CNTL_0],
		&is_csis_ibuf_fields[CSIS_IBUF_F_DEBUG_ON], img_otf_out);
}

void csi_pdp_top_dump(void __iomem *base_reg)
{
	u32 val;

	val = is_hw_get_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_VERSION_ID]);

	info("[DUMP] v%02u.%02u.%02u ======================================\n", (val >> 24) & 0xff,
		(val >> 16) & 0xff, val & 0xffff);

	_csis_pdp_top_dump_dbg_state(base_reg);
	_csis_pdp_top_dump(base_reg);
	_csis_pdp_top_ibuf_dump(base_reg);
	_csis_pdp_top_bns_dump(base_reg);

	info("[DUMP] =================================================\n");
}

static bool _get_ibuf_lc_en(u32 otf_lc[CAMIF_VC_ID_NUM], u32 lc)
{
	u32 i;

	for (i = 0; i < CAMIF_VC_ID_NUM; i++) {
		if (otf_lc[i] == lc)
			return true;
	}

	return false;
}

static u32 _get_ibuf_bitmode(u32 hwformat)
{
	u32 bitmode;

	switch (hwformat) {
	case HW_FORMAT_RAW8:
	case HW_FORMAT_EMBEDDED_8BIT:
		bitmode = IBUF_BITMODE_8BIT;
		break;
	case HW_FORMAT_RAW10:
		bitmode = IBUF_BITMODE_10BIT;
		break;
	case HW_FORMAT_RAW12:
		bitmode = IBUF_BITMODE_12BIT;
		break;
	case HW_FORMAT_RAW14:
		bitmode = IBUF_BITMODE_14BIT;
		break;
	default:
		bitmode = IBUF_BITMODE_MAX;
		break;
	}

	return bitmode;
}

void csis_pdp_top_set_ibuf(void __iomem *base_reg, struct pablo_camif_otf_info *otf_info,
	u32 otf_out_id, struct is_sensor_cfg *sensor_cfg, bool csi_potf)
{
	u32 val, cr_offset;
	u32 otf_ch = otf_info->otf_out_ch[otf_out_id];
	u32 width, height, bit_mode, lc;
	int link_vc;
	bool potf, user_emb, lc_en;
	u32 ibuf_enable = 0;

	cr_offset = otf_ch * CSIS_IBUF_CR_OFFSET;
	is_hw_set_field(base_reg + cr_offset,
		&is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_CNTL_0],
		&is_csis_ibuf_fields[CSIS_IBUF_F_STUCK_DETECT], 0); /* bring up guide */

	for (lc = 0; lc < CSIS_IBUF_LC_NUM; lc++) {
		cr_offset = (otf_ch * CSIS_IBUF_CR_OFFSET) + (lc * CSIS_IBUF_LC_CR_OFFSET);

		/**
		 * Disable IBUF err_check function by default.
		 * The reset value is 1(== enable).
		 */
		is_hw_set_field(base_reg + cr_offset,
			&is_csis_ibuf_lc_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_LCX_1],
			&is_csis_ibuf_fields[CSIS_IBUF_F_ERR_CHECK_ON_LCX], 0);

		link_vc = otf_info->link_vc_list[otf_out_id][lc];
		if (link_vc < 0)
			continue;

		cr_offset = (otf_ch * CSIS_IBUF_CR_OFFSET) + (lc * CSIS_IBUF_LC_CR_OFFSET);

		width = sensor_cfg->input[link_vc].width;
		height = sensor_cfg->input[link_vc].height;
		potf = (csi_potf || CHECK_POTF_EN(sensor_cfg->input[link_vc].extformat));
		user_emb = (potf && (sensor_cfg->input[link_vc].data == DATA_EMB));
		lc_en = true;

		bit_mode = _get_ibuf_bitmode(sensor_cfg->input[link_vc].hwformat);
		if (bit_mode >= IBUF_BITMODE_MAX) {
			lc_en = false;
			warn("[IBUF%d][VC%d-LC%d] Invalid data format 0x%x", otf_ch, link_vc, lc,
			     sensor_cfg->input[link_vc].hwformat);
			bit_mode = is_hw_get_field(
				base_reg + cr_offset,
				&is_csis_ibuf_lc_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_LCX_1],
				&is_csis_ibuf_fields[CSIS_IBUF_F_BITMODE_LCX]);
		}

		if (lc_en)
			ibuf_enable |= (1 << lc);
		else
			ibuf_enable &= ~(1 << lc);

		lc_en = lc_en && _get_ibuf_lc_en(otf_info->otf_lc[otf_out_id], lc);
		info("[CSI%d][IBUF%d][LC%d] %s\n", otf_info->csi_ch,
			otf_info->otf_out_ch[otf_out_id], lc, lc_en ? "ON" : "OFF");

		val = 0;
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_WIDTH_LCX], width);
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_HEIGHT_LCX], height);
		is_hw_set_reg(base_reg + cr_offset,
			&is_csis_ibuf_lc_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_LCX_0], val);

		val = 0;
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_OTF_EN_LCX], !potf);
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_BITMODE_LCX], bit_mode);
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_USER_EMB_LCX], user_emb);
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_OUT_BITMODE_LCX], bit_mode);

		if (IS_ENABLED(CONFIG_IBUF_ERR_CHECK_ON))
			val = is_hw_set_field_value(
				val, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_CHECK_ON_LCX], lc_en);

		is_hw_set_reg(base_reg + cr_offset,
			&is_csis_ibuf_lc_regs[CSIS_IBUF_R_IBUFX_INPUT_CONFIG_LCX_1], val);
	}

	is_hw_set_field(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_VC_CTRL_0],
		&is_csis_pdp_top_fields[CSIS_PDP_TOP_F_IBUF0 + otf_ch], ibuf_enable);
}
KUNIT_EXPORT_SYMBOL(csis_pdp_top_set_ibuf);

static u32 get_ibuf_ptrn_gen_pd_ratio(u32 img_width, u32 img_height, u32 pd_width, u32 pd_height)
{
	u32 i, div;

	for (i = PD_RATIO_1_TO_2; i < PD_RATIO_NUM; i++) {
		div = pd_ratio_div[i];
		if (((pd_width * div) == img_width) && ((pd_height * div) == img_height))
			break;
	}

	return i;
}

static void get_ibuf_ptrn_gen_vhd_blank(
	u32 width, u32 height, u32 fps, u32 clk, u32 *vblank, u32 *hblank, u32 *dblank)
{
	const struct is_field *h2h_field = &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_H2H_BLANK];
	const struct is_field *d2d_field = &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_D2D_BLANK];
	const u32 dvalid = 8; /* 8 cycle fixed */
	u32 hvalid, vvalid, line_length, frame_length; /* unit: cycle */
	u32 tmp_dblank, tmp_hblank; /* unit: cycle */
	u32 vvalid_mul;

	if (fps <= 30)
		vvalid_mul = 2;
	else if (fps <= 60)
		vvalid_mul = 3;
	else
		vvalid_mul = 5;

	frame_length = (clk * 1000 * 1000) / fps;
	hvalid = ALIGN(width, 8) / 8; /* 8ppc */
	vvalid = frame_length * vvalid_mul / 6;

	line_length = vvalid / height;

	if (hvalid >= line_length) {
		tmp_dblank = 0;
		tmp_hblank = 16; /* minimum value */
	} else if ((line_length - hvalid) >> h2h_field->bit_width) {
		tmp_hblank = GENMASK(h2h_field->bit_width - 1, 0); /* maximum value */
		tmp_dblank = (line_length - hvalid - tmp_hblank) / (width / 8); /* 8ppc */
		if (tmp_dblank >> d2d_field->bit_width)
			tmp_dblank = GENMASK(d2d_field->bit_width - 1, 0); /* maximum value */
	} else {
		tmp_dblank = 0;
		tmp_hblank = line_length - hvalid;
	}

	*vblank = frame_length - vvalid;
	*hblank = tmp_hblank;
	*dblank = tmp_dblank;

	info("[IBUF-PTR_GEN] V(%d/%d) H(%d/%d) D(%d/%d)\n", vvalid, *vblank, hvalid, *hblank,
		dvalid, *dblank);
}

void csis_pdp_top_enable_ibuf_ptrn_gen(void __iomem *base_reg, struct is_sensor_cfg *sensor_cfg,
				       struct pablo_camif_otf_info *otf_info, u32 clk, u32 fps,
				       bool on)
{
	u32 val;
	u32 width = sensor_cfg->input[CSI_VIRTUAL_CH_0].width;
	u32 height = sensor_cfg->input[CSI_VIRTUAL_CH_0].height;
	u32 hwformat = sensor_cfg->input[CSI_VIRTUAL_CH_0].hwformat;
	u32 pd_width = sensor_cfg->input[CSI_VIRTUAL_CH_1].width;
	u32 pd_height = sensor_cfg->input[CSI_VIRTUAL_CH_1].height;
	u32 otf_ch = otf_info->otf_out_ch[CSI_OTF_OUT_SINGLE];
	u32 dblank = 0, hblank = 256, vblank = 512;
	u32 bitmode, pd_ratio;

	base_reg += otf_ch * CSIS_IBUF_CR_OFFSET;

	bitmode = _get_ibuf_bitmode(hwformat);
	if (bitmode >= IBUF_BITMODE_MAX) {
		warn("IBUF%d][VC0-LC0] Invalid data format 0x%x", otf_ch, hwformat);
		bitmode = IBUF_BITMODE_10BIT;
	}

	/* Pattern_Gen configuration */
	val = 0;
	val = is_hw_set_field_value(val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_FRAME_CNT], 0);
	val = is_hw_set_field_value(val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_SENSOR_SYNC], 0);
	val = is_hw_set_field_value(
		val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_DATA_TYPE], 3); /* color bar */
	val = is_hw_set_field_value(val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_BITMODE], bitmode);
	is_hw_set_reg(base_reg, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_PTRN_CONFIG_1], val);

	/* PD configuration */
	val = 0;
	pd_ratio = get_ibuf_ptrn_gen_pd_ratio(width, height, pd_width, pd_height);
	if (pd_ratio < PD_RATIO_NUM) {
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_PD_LOCH_ID], 1);
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_PD_H], pd_ratio);
		val = is_hw_set_field_value(
			val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_PD_V], pd_ratio);
		val = is_hw_set_field_value(val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_PD_ON], 1);
	} else {
		val = is_hw_set_field_value(val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_PD_ON], 0);
	}

	is_hw_set_reg(base_reg, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_PTRN_CONFIG_2], val);

	/* IMG configuration */
	val = 0;
	val = is_hw_set_field_value(val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_WIDTH], width);
	val = is_hw_set_field_value(val, &is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_HEIGHT], height);
	is_hw_set_reg(base_reg, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_PTRN_CONFIG_3], val);

	if (!clk)
		clk = 533;

	/* VHD blank configuration */
	if (on)
		get_ibuf_ptrn_gen_vhd_blank(width, height, fps, clk, &vblank, &hblank, &dblank);

	is_hw_set_field(base_reg, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_PTRN_CONFIG_4],
		&is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_D2D_BLANK], dblank);
	is_hw_set_field(base_reg, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_PTRN_CONFIG_4],
		&is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_H2H_BLANK], hblank);
	is_hw_set_field(base_reg, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_PTRN_CONFIG_5],
		&is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_V2V_BLANK], vblank);

	is_hw_set_field(base_reg, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_PTRN_CONFIG_0],
		&is_csis_ibuf_fields[CSIS_IBUF_F_PTRN_GEN_ON], on);

	info("[IBUF%d] %s Pattern Generator (%dx%d@%d, %dMHz) pd_ratio(%d)\n", otf_ch,
		on ? "Enable" : "Disable", width, height, fps, clk, pd_ratio);
}
KUNIT_EXPORT_SYMBOL(csis_pdp_top_enable_ibuf_ptrn_gen);

int csis_pdp_top_irq_src(void __iomem *base_reg)
{
	int ret = 0;
	unsigned long src;
	struct csis_ibuf_err ibuf_err = {
		0,
	};
	u32 ibuf_ch, cr_offset;
	ulong err0, err1;
	bool overflow = false;
	ulong err_stat0, err_stat1;

	src = is_hw_get_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS_TOP_INTR_SOURCE]);

	for (ibuf_ch = 0; ibuf_ch < CSIS_IBUF_CH_NUM; ibuf_ch++) {
		if (!test_bit(INTR_CSIS_TOP_IBUF_CH0_ERR_INT + ibuf_ch, &src))
			continue;

		cr_offset = CSIS_IBUF_CR_OFFSET * ibuf_ch;

		ibuf_err.err = true;
		err0 = is_hw_get_reg(
			base_reg + cr_offset, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_ERR_CONFIG_0]);
		err1 = is_hw_get_reg(
			base_reg + cr_offset, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_ERR_CONFIG_1]);

		ibuf_err.overflow =
			is_hw_get_field_value(err0, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_OVERFLOW]);
		ibuf_err.stuck =
			is_hw_get_field_value(err0, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_STUCK]);
		ibuf_err.protocol =
			is_hw_get_field_value(err0, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_PROTOCOL]);
		ibuf_err.lc =
			is_hw_get_field_value(err0, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_LOCH]);
		ibuf_err.mem_percent =
			is_hw_get_field_value(err0, &is_csis_ibuf_fields[CSIS_IBUF_F_MEM_PERCENT]);

		ibuf_err.hcnt =
			is_hw_get_field_value(err1, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_HCNT]) *
			8; /* 8ppc */
		ibuf_err.vcnt =
			is_hw_get_field_value(err1, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_VCNT]);

		if (ibuf_err.overflow)
			overflow = true;

		err("[IBUF%d-LC%d] OVF(%d) STUCK(%d) PROTOCOL(%d) CNT(%dx%d) MEM(%d)", ibuf_ch,
			ibuf_err.lc, ibuf_err.overflow, ibuf_err.stuck, ibuf_err.protocol,
			ibuf_err.hcnt, ibuf_err.vcnt, ibuf_err.mem_percent);
		_csis_pdp_top_ibuf_dump_dbg_state(base_reg, ibuf_ch);

		err_stat0 = is_hw_get_reg(
			base_reg + cr_offset, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_ERR_STATUS_0]);
		err_stat1 = is_hw_get_reg(
			base_reg + cr_offset, &is_csis_ibuf_regs[CSIS_IBUF_R_IBUFX_ERR_STATUS_1]);

		ibuf_err.wc_err =
			is_hw_get_field_value(err_stat1, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_WC_ERR]);
		ibuf_err.calc_wc =
			is_hw_get_field_value(err_stat0, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_CALC_WC]);
		ibuf_err.input_wc =
			is_hw_get_field_value(err_stat0, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_IN_WC]);
		ibuf_err.hsize_err_short =
			is_hw_get_field_value(err_stat1, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_HSIZE_ERR_SHORT]);
		ibuf_err.vsize_err_short =
			is_hw_get_field_value(err_stat1, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_VSIZE_ERR_SHORT]);
		ibuf_err.hsize_err_long =
			is_hw_get_field_value(err_stat1, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_HSIZE_ERR_LONG]);
		ibuf_err.vsize_err_long =
			is_hw_get_field_value(err_stat1, &is_csis_ibuf_fields[CSIS_IBUF_F_ERR_VSIZE_ERR_LONG]);
		err("[IBUF%d] WC_ERR(%d) CALC(%d) : IN(%d), SHORT H/V(%d, %d) LONG H/V(%d, %d)",
			ibuf_ch, ibuf_err.wc_err, ibuf_err.calc_wc, ibuf_err.input_wc,
			ibuf_err.hsize_err_short, ibuf_err.vsize_err_short,
			ibuf_err.hsize_err_long, ibuf_err.vsize_err_long);
	}

	if (ibuf_err.err)
		_csis_pdp_top_dump_dbg_state(base_reg);

	if (!IS_ENABLED(CONFIG_IBUF_ERR_INTR_AUTO_RESET))
		is_hw_set_reg(
			base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS_TOP_INTR_SOURCE], src);

	if (overflow)
		ret = -EIO;

	return ret;
}

int csis_pdp_top_nfi_en(struct is_device_csi *csi, bool en, struct is_fid_loc *fid_loc)
{
	struct pablo_camif_csis_pdp_top *top = csi->top;
	struct pablo_camif_otf_info *otf_info = &csi->otf_info;
	u32 val = 0;

	if (!top) {
		err("CSIS%d doesn't have top regs.", otf_info->csi_ch);
		return -EINVAL;
	}

	if (!en) {
		is_hw_set_reg(top->regs,
			&is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS0_NFI_EN + otf_info->csi_ch], 0);
		return 0;
	}

	if (!fid_loc || !fid_loc->valid) {
		warn("fid_loc is NOT properly set.");
		return -EINVAL;
	}

	info("CSI%d FID:byte %d line %d, NFI:byte %d line %d\n", otf_info->csi_ch, fid_loc->byte,
		fid_loc->line, fid_loc->nfi_byte, fid_loc->nfi_line);

	val = is_hw_set_field_value(
		val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_FID_LOC_BYTE], fid_loc->byte);
	val = is_hw_set_field_value(
		val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_FID_LOC_LINE], fid_loc->line);
	is_hw_set_reg(top->regs,
		&is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS0_FRAME_ID_EN + otf_info->csi_ch], val);

	val = is_hw_set_field_value(
		val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_NFI_LOC_BYTE], fid_loc->nfi_byte);
	val = is_hw_set_field_value(
		val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_NFI_LOC_LINE], fid_loc->nfi_line);
	val = is_hw_set_field_value(
		val, &is_csis_pdp_top_fields[CSIS_PDP_TOP_F_CSISX_NFI_EN_CSIS], en);
	is_hw_set_reg(top->regs,
		&is_csis_pdp_top_regs[CSIS_PDP_TOP_R_CSIS0_NFI_EN + otf_info->csi_ch], val);

	return 0;
}

void csis_pdp_top_nfi_sw_trigger(struct is_device_csi *csi)
{
	struct pablo_camif_csis_pdp_top *top = csi->top;
	struct pablo_camif_otf_info *otf_info = &csi->otf_info;
	u32 val;

	if (!top) {
		err("CSIS%d doesn't have top regs.", otf_info->csi_ch);
		return;
	}

	val = 1 << otf_info->csi_ch;
	info("CSIS%d NFI SW Trigger", otf_info->csi_ch);

	is_hw_set_field(top->regs, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_CTRL_0],
		&is_csis_pdp_top_fields[CSIS_PDP_TOP_F_SW_TRIGGER], val);
}

static DEFINE_MUTEX(lock_nfi_irq_msk);
void csis_pdp_top_nfi_irq_msk(void __iomem *base_reg, u32 csi_ch, bool on)
{
	u64 l_msk, u_msk, concat_msk, ch_msk;

	mutex_lock(&lock_nfi_irq_msk);
	l_msk = is_hw_get_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_ENABLE_0]);
	u_msk = is_hw_get_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_ENABLE_1]);

	concat_msk = l_msk | (u_msk << 32);
	ch_msk = CSIS_NFI_INTR_EN_MASK << (csi_ch * INTR_NFI_MAX);

	if (on)
		concat_msk |= ch_msk;
	else
		concat_msk &= ~ch_msk;

	l_msk = concat_msk & 0xFFFFFFFF;
	u_msk = (concat_msk >> 32) & 0xFFFFFFFF;

	is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_ENABLE_0], l_msk);
	is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_ENABLE_1], u_msk);
	mutex_unlock(&lock_nfi_irq_msk);
}

void csis_pdp_top_nfi_irq_src(void __iomem *base_reg, ulong *str, ulong *end)
{
	u64 l_src, u_src, concat_src;
	u32 ch, ch_src;

	l_src = is_hw_get_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_SOURCE_0]);
	u_src = is_hw_get_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_SOURCE_1]);

	/* clear */
	is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_SOURCE_0], l_src);
	is_hw_set_reg(base_reg, &is_csis_pdp_top_regs[CSIS_PDP_TOP_R_NFI_INT_SOURCE_1], u_src);

	concat_src = l_src | (u_src << 32);

	for (ch = 0; ch < CSI_ID_MAX; ch++) {
		ch_src = (concat_src >> (ch * INTR_NFI_MAX)) & 0x7F;

		if (ch_src & BIT(INTR_NFI_1ST_FRAME_END))
			info("[NFI] CSI%d %s", ch, nfi_int_str[INTR_NFI_1ST_FRAME_END]);

		if (ch_src & BIT(INTR_NFI_1ST_FRAME_STR))
			info("[NFI] CSI%d %s", ch, nfi_int_str[INTR_NFI_1ST_FRAME_STR]);

		if (ch_src & BIT(INTR_NFI_2ND_FRAME_STR)) {
			info("[NFI] CSI%d %s", ch, nfi_int_str[INTR_NFI_2ND_FRAME_STR]);
			set_bit(ch, str);
		}

		if (ch_src & BIT(INTR_NFI_2ND_FRAME_END)) {
			info("[NFI] CSI%d %s", ch, nfi_int_str[INTR_NFI_2ND_FRAME_END]);
			set_bit(ch, end);
		}
	}
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_hw_csis_pdp_top_func pablo_kunit_hw_csis_pdp_top = {
	.frame_id_en = csis_pdp_top_frame_id_en,
	.get_frame_id_en = csis_pdp_top_get_frame_id_en,
	.qch_cfg = csis_pdp_top_qch_cfg,
	.s_link_vc_list = csis_pdp_top_s_link_vc_list,
	.s_otf_out_mux = csis_pdp_top_s_otf_out_mux,
	.s_otf_lc = csis_pdp_top_s_otf_lc,
	.irq_msk = csis_pdp_top_irq_msk,
	.irq_src = csis_pdp_top_irq_src,
	.nfi_irq_msk = csis_pdp_top_nfi_irq_msk,
	.nfi_irq_src = csis_pdp_top_nfi_irq_src,
};

struct pablo_kunit_hw_csis_pdp_top_func *pablo_kunit_get_hw_csis_pdp_top_test(void)
{
	return &pablo_kunit_hw_csis_pdp_top;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_hw_csis_pdp_top_test);
#endif
