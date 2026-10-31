// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MLSC HW control APIs
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw.h"
#include "pmio.h"
#include "sfr/pablo-sfr-mlsc-v13_0.h"
#include "pablo-hw-api-mlsc.h"

/* PMIO MACRO */
#define SET_CR(pmio, R, val) PMIO_SET_R(pmio, R, val)
#define SET_CR_F(pmio, R, F, val) PMIO_SET_F(pmio, R, F, val)
#define SET_CR_V(pmio, reg_val, F, val) PMIO_SET_V(pmio, reg_val, F, val)

#define GET_CR(pmio, R) PMIO_GET_R(pmio, R)
#define GET_CR_F(pmio, R, F) PMIO_GET_F(pmio, R, F)

/* LOG MACRO */
#define HW_NAME "MLSC"
#define mlsc_info(fmt, args...) info_hw("[%s]" fmt "\n", HW_NAME, ##args)
#define mlsc_warn(fmt, args...) warn_hw("[%s]" fmt, HW_NAME, ##args)
#define mlsc_err(fmt, args...) err_hw("[%s]" fmt, HW_NAME, ##args)

/* Tuning Parameters */
#define HBLANK_CYCLE 0x20 /* Reset value */

/* Guided value */
#define USE_MLSC_LIC_RECOVERY 1
#define LIC_HBLALK_CYCLE 50

/* Constraints */
#define MLSC_SRAM_MAX_SIZE 26000
#define MLSC_BIT_MODE 2 /* YUV422 */
#define MLSC_LMEDS_OUT_MAX_W 1664
#define MLSC_LMEDS_OUT_MAX_H 1248
#define MLSC_FDPIG_OUT_MAX_W 640
#define MLSC_FDPIG_OUT_MAX_H 480
#define MLSC_CAV_OUT_MAX_W 640
#define MLSC_CAV_OUT_MAX_H 480
#define MLSC_SVHIST_WDMA_W 1024
#define MLSC_LIC_CH_CNT 4

enum mlsc_cinfifo_new_frame_in {
	NEW_FRAME_ASAP = 0,
	NEW_FRAME_VVALID_RISE = 1,
};

enum mlsc_cotf_in_id {
	MLSC_COTF_IN_YUV,
};

enum mlsc_ds_out_format {
	MLSC_DS_OUT_RGB = 0,
	MLSC_DS_OUT_YUV422 = 1,
	MLSC_DS_OUT_YUV420 = 2,
};

struct mlsc_dma_cfg {
	pdma_addr_t *addrs;
	u32 num_planes;
	u32 num_buffers;
	u32 buf_idx;
	u32 p_size; /* Payload size of each plane */
	u32 sbwc_en;
};

struct mlsc_hw_dma {
	enum mlsc_dma_id dma_id;
	unsigned int sfr_offset;
	unsigned int field_offset;
	unsigned int size_sfr_offset;
	unsigned int size_field_offset;
	char *name;
};

static const struct mlsc_hw_dma mlsc_dmas[MLSC_DMA_NUM] = {
	{ MLSC_DMA_NONE, MLSC_REG_CNT, 0, 0, 0, "DMA_NONE" },
	{ MLSC_R_CL, MLSC_R_YUV_RDMACLOAD_EN, MLSC_F_YUV_RDMACLOAD_EN, 0, 0, "R_CL" },
	{ MLSC_R_Y, MLSC_R_YUV_RDMAY_EN, MLSC_F_YUV_RDMAY_EN, 0, 0, "R_Y" },
	{ MLSC_R_UV, MLSC_R_YUV_RDMAUV_EN, MLSC_F_YUV_RDMAUV_EN, 0, 0, "R_UV" },
	{ MLSC_W_YUV444_U, MLSC_R_YUV_WDMAYUV444Y_EN, MLSC_F_YUV_WDMAYUV444Y_EN, 0, 0,
		"W_YUV444_Y" },
	{ MLSC_W_YUV444_V, MLSC_R_YUV_WDMAYUV444U_EN, MLSC_F_YUV_WDMAYUV444U_EN, 0, 0,
		"W_YUV444_U" },
	{ MLSC_W_YUV444_Y, MLSC_R_YUV_WDMAYUV444V_EN, MLSC_F_YUV_WDMAYUV444V_EN, 0, 0,
		"W_YUV444_V" },
	{ MLSC_W_GLPG0_Y, MLSC_R_Y_WDMAGLPGOUTL0_EN, MLSC_F_Y_WDMAGLPGOUTL0_EN, 0, 0, "W_GLPG0_Y" },
	{ MLSC_W_GLPG1_Y, MLSC_R_YUV_WDMAGLPGOUTL1Y_EN, MLSC_F_YUV_WDMAGLPGOUTL1Y_EN,
		MLSC_R_GLPG_L1_IMG_SIZE, MLSC_F_GLPG_L1_IMG_WIDTH, "W_GLPG1_Y" },
	{ MLSC_W_GLPG1_U, MLSC_R_YUV_WDMAGLPGOUTL1U_EN, MLSC_F_YUV_WDMAGLPGOUTL1U_EN, 0, 0,
		"W_GLPG1_U" },
	{ MLSC_W_GLPG1_V, MLSC_R_YUV_WDMAGLPGOUTL1V_EN, MLSC_F_YUV_WDMAGLPGOUTL1V_EN, 0, 0,
		"W_GLPG1_V" },
	{ MLSC_W_GLPG2_Y, MLSC_R_YUV_WDMAGLPGOUTL2Y_EN, MLSC_F_YUV_WDMAGLPGOUTL2Y_EN,
		MLSC_R_GLPG_L2_IMG_SIZE, MLSC_F_GLPG_L2_IMG_WIDTH, "W_GLPG2_Y" },
	{ MLSC_W_GLPG2_U, MLSC_R_YUV_WDMAGLPGOUTL2U_EN, MLSC_F_YUV_WDMAGLPGOUTL2U_EN, 0, 0,
		"W_GLPG2_U" },
	{ MLSC_W_GLPG2_V, MLSC_R_YUV_WDMAGLPGOUTL2V_EN, MLSC_F_YUV_WDMAGLPGOUTL2V_EN, 0, 0,
		"W_GLPG2_V" },
	{ MLSC_W_GLPG3_Y, MLSC_R_YUV_WDMAGLPGOUTL3Y_EN, MLSC_F_YUV_WDMAGLPGOUTL3Y_EN,
		MLSC_R_GLPG_L3_IMG_SIZE, MLSC_F_GLPG_L3_IMG_WIDTH, "W_GLPG3_Y" },
	{ MLSC_W_GLPG3_U, MLSC_R_YUV_WDMAGLPGOUTL3U_EN, MLSC_F_YUV_WDMAGLPGOUTL3U_EN, 0, 0,
		"W_GLPG3_U" },
	{ MLSC_W_GLPG3_V, MLSC_R_YUV_WDMAGLPGOUTL3V_EN, MLSC_F_YUV_WDMAGLPGOUTL3V_EN, 0, 0,
		"W_GLPG3_V" },
	{ MLSC_W_GLPG4_Y, MLSC_R_YUV_WDMAGLPGOUTG4Y_EN, MLSC_F_YUV_WDMAGLPGOUTG4Y_EN,
		MLSC_R_GLPG_G4_IMG_SIZE, MLSC_F_GLPG_G4_IMG_WIDTH, "W_GLPG4_Y" },
	{ MLSC_W_GLPG4_U, MLSC_R_YUV_WDMAGLPGOUTG4U_EN, MLSC_F_YUV_WDMAGLPGOUTG4U_EN, 0, 0,
		"W_GLPG4_U" },
	{ MLSC_W_GLPG4_V, MLSC_R_YUV_WDMAGLPGOUTG4V_EN, MLSC_F_YUV_WDMAGLPGOUTG4V_EN, 0, 0,
		"W_GLPG4_V" },
	{ MLSC_W_SVHIST, MLSC_R_STAT_WDMASVHIST_EN, MLSC_F_STAT_WDMASVHIST_EN, 0, 0, "W_SVHIST" },
	{ MLSC_W_FDPIG, MLSC_R_YUV_WDMAFDPIG_EN, MLSC_F_YUV_WDMAFDPIG_EN, MLSC_R_FDPIG_IMG_SIZE,
		MLSC_F_FDPIG_IMG_WIDTH, "W_FDPIG" },
	{ MLSC_W_LMEDS, MLSC_R_Y_WDMALME_EN, MLSC_F_Y_WDMALME_EN, MLSC_R_LME_IMG_SIZE,
		MLSC_F_LME_IMG_WIDTH, "W_DSLME" },
	{ MLSC_W_CAV, MLSC_R_YUV_WDMACAV_EN, MLSC_F_YUV_WDMACAV_EN, MLSC_R_CAV_IMG_SIZE,
		MLSC_F_CAV_IMG_WIDTH, "W_CAV" },
};

struct mlsc_ds_cfg {
	u32 scale_x; /* @5.12 */
	u32 scale_y; /* @5.12 */
	u32 inv_scale_x; /* @0.14 */
	u32 inv_scale_y; /* @0.14 */
	u32 inv_shift_x; /* 26-31 */
	u32 inv_shift_y; /* 26-31 */
	struct is_crop in_crop; /* Crop */
	struct is_crop ot_crop; /* Scale down */
	u32 format; /* Scale down */
};

struct mlsc_line_buffer {
	u32 offset[MLSC_LIC_CH_CNT];
};

/* for image max width : 4080 */
/* FIXME: need to be set for multiple context */
static struct mlsc_line_buffer lb_offset_evt0[] = {
	[0].offset = { 0, 4096, 8192, 12288 }, /* offset < 26000 & 32px aligned (8ppc) */
	[1].offset = { 0, 1664, 3328, 4992 }, /* offset < 1664 & 16px aligned (8ppc) */
};
/* EVT1.1~ : assume that ctx0 is used for reprocessing */
static struct mlsc_line_buffer lb_offset[] = {
	[0].offset = { 0, 13712, 17808, 21904 }, /* offset < 26000 & 32px aligned (8ppc) */
	[1].offset = { 0, 1664, 3328, 4992 }, /* offset < 1664 & 16px aligned (8ppc) */
};

void mlsc_hw_s_lbctrl(struct pablo_mmio *pmio)
{
	int lic_ch;
	struct mlsc_line_buffer *lb;

	if (exynos_soc_info.main_rev >= 1 &&
		exynos_soc_info.sub_rev >= 1)
		lb = lb_offset;
	else
		lb = lb_offset_evt0;

	for (lic_ch = 0; lic_ch < MLSC_LIC_CH_CNT; lic_ch++) {
		SET_CR_F(pmio, MLSC_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x4 * lic_ch),
			MLSC_F_CHAIN_LBCTRL_OFFSET_GRP0_C0 + (lic_ch * 2), lb[0].offset[lic_ch]);
		SET_CR_F(pmio, MLSC_R_CHAIN_LBCTRL_OFFSET_GRP0TO1_C0 + (0x4 * lic_ch),
			MLSC_F_CHAIN_LBCTRL_OFFSET_GRP1_C0 + (lic_ch * 2), lb[1].offset[lic_ch]);
	}
}

static void _mlsc_hw_s_otf(struct pablo_mmio *pmio, bool en)
{
	if (en) {
		SET_CR(pmio, MLSC_R_YUV_CINFIFO_ENABLE, 1);
		SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_CONFIG,
			MLSC_F_YUV_CINFIFO_STALL_BEFORE_FRAME_START_EN, 1);
		SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_CONFIG, MLSC_F_YUV_CINFIFO_AUTO_RECOVERY_EN, 1);
		SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_CONFIG, MLSC_F_YUV_CINFIFO_DEBUG_EN, 1);
		SET_CR(pmio, MLSC_R_YUV_CINFIFO_INT_ENABLE, 0xF);
		SET_CR(pmio, MLSC_R_YUV_CINFIFO_ROL_SELECT, 0xF);

		SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_INTERVALS,
				MLSC_F_YUV_CINFIFO_INTERVAL_HBLANK, HBLANK_CYCLE);
	} else {
		SET_CR(pmio, MLSC_R_YUV_CINFIFO_ENABLE, 0);
	}
}

static void _mlsc_hw_s_crc(struct pablo_mmio *pmio, u8 seed)
{
	SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_STREAM_CRC, MLSC_F_YUV_CINFIFO_CRC_SEED, seed);
}

static inline u32 _mlsc_hw_g_int_state(
	struct pablo_mmio *pmio, u32 int_src, u32 int_clr, bool clear)
{
	u32 int_state;

	int_state = GET_CR(pmio, int_src);

	if (clear)
		SET_CR(pmio, int_clr, int_state);

	return int_state;
}

static inline u32 _mlsc_hw_g_idleness_state(struct pablo_mmio *pmio)
{
	return GET_CR_F(pmio, MLSC_R_IDLENESS_STATUS, MLSC_F_IDLENESS_STATUS);
}

static int _mlsc_hw_wait_idleness(struct pablo_mmio *pmio)
{
	u32 retry = 0;

	while (!_mlsc_hw_g_idleness_state(pmio)) {
		if (retry++ > MLSC_TRY_COUNT) {
			mlsc_err("Failed to wait IDLENESS. retry %u", retry);
			return -ETIME;
		}

		usleep_range(3, 4);
	}

	return 0;
}

static void _mlsc_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	/* TOP */
	mlsc_info("= TOP ================================");
	mlsc_info("IDLENESS:\t0x%08x", GET_CR(pmio, MLSC_R_IDLENESS_STATUS));
	mlsc_info("BUSY:\t0x%08x", GET_CR(pmio, MLSC_R_IP_BUSY_MONITOR_0));
	mlsc_info("STALL_OUT:\t0x%08x", GET_CR(pmio, MLSC_R_IP_STALL_OUT_STATUS_0));

	mlsc_info("INT0:\t0x%08x/ HIST: 0x%08x", GET_CR(pmio, MLSC_R_INT_REQ_INT0),
		GET_CR(pmio, MLSC_R_INT_HIST_CURINT0));
	mlsc_info("INT1:\t0x%08x/ HIST: 0x%08x", GET_CR(pmio, MLSC_R_INT_REQ_INT1),
		GET_CR(pmio, MLSC_R_INT_HIST_CURINT1));

	/* CTRL */
	mlsc_info("= CTRL ===============================");
	mlsc_info("CMDQ_DBG:\t0x%08x", GET_CR(pmio, MLSC_R_CMDQ_DEBUG_STATUS));
	mlsc_info("CMDQ_INT:\t0x%08x", GET_CR(pmio, MLSC_R_CMDQ_INT));
	mlsc_info("COREX_INT:\t0x%08x", GET_CR(pmio, MLSC_R_COREX_INT));

	/* PATH */
	mlsc_info("= OTF ================================");
	mlsc_info("OTF_PATH:\t0x%08x", GET_CR(pmio, MLSC_R_IP_USE_OTF_PATH_01));

	/* CINFIFO0 */
	mlsc_info("= CIN0 ===============================");
	mlsc_info("CIN0_CFG:\t0x%08x", GET_CR(pmio, MLSC_R_YUV_CINFIFO_CONFIG));
	mlsc_info("CIN0_INT:\t0x%08x", GET_CR(pmio, MLSC_R_YUV_CINFIFO_INT));
	mlsc_info("CIN0_CNT:\t0x%08x", GET_CR(pmio, MLSC_R_YUV_CINFIFO_INPUT_CNT));
	mlsc_info("CIN0_STL:\t0x%08x", GET_CR(pmio, MLSC_R_YUV_CINFIFO_STALL_CNT));
	mlsc_info("CIN0_FUL:\t0x%08x", GET_CR(pmio, MLSC_R_YUV_CINFIFO_FIFO_FULLNESS));

	mlsc_info("======================================");
}

int mlsc_hw_s_otf(struct pablo_mmio *pmio, bool en)
{
	_mlsc_hw_s_otf(pmio, en);

	return 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_otf);

static void _mlsc_hw_s_cloader(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, MLSC_R_YUV_RDMACLOAD_EN, MLSC_F_YUV_RDMACLOAD_EN, 1);
}

void mlsc_hw_s_sr(struct pablo_mmio *pmio, bool enable)
{
	u32 val = 0;

	if (!enable) {
		/**
		 * Before releasing the DMA of Shared Resource,
		 * the module must be disabled in advance.
		 */
		pmio_cache_set_only(pmio, false);
		SET_CR(pmio, MLSC_R_YUV_RDMAY_EN, 0);
		SET_CR(pmio, MLSC_R_YUV_RDMAUV_EN, 0);
		SET_CR(pmio, MLSC_R_CHAIN_INPUT_0_SELECT, 0);
		pmio_cache_set_only(pmio, true);

		SET_CR(pmio, MLSC_R_ALLOC_SR_ENABLE, enable);
		return;
	}

	SET_CR_F(pmio, MLSC_R_ALLOC_SR_GRP_0TO3, MLSC_F_ALLOC_SR_GRP0, val);
	SET_CR_F(pmio, MLSC_R_ALLOC_SR_GRP_0TO3, MLSC_F_ALLOC_SR_GRP1, val);

	SET_CR_F(pmio, MLSC_R_ALLOC_SR_GRP_4TO7, MLSC_F_ALLOC_SR_GRP4, val);
	SET_CR_F(pmio, MLSC_R_ALLOC_SR_GRP_4TO7, MLSC_F_ALLOC_SR_GRP5, val);

	SET_CR_F(pmio, MLSC_R_ALLOC_SR_GRP_8TO11, MLSC_F_ALLOC_SR_GRP8, val);
	SET_CR_F(pmio, MLSC_R_ALLOC_SR_GRP_8TO11, MLSC_F_ALLOC_SR_GRP9, val);

	SET_CR(pmio, MLSC_R_ALLOC_SR_ENABLE, enable);
}

void mlsc_hw_init(struct pablo_mmio *pmio)
{
	_mlsc_hw_s_otf(pmio, true);
	_mlsc_hw_s_cloader(pmio);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_init);

void mlsc_hw_s_core(struct pablo_mmio *pmio, u32 in_w, u32 in_h)
{
	int val;
	u32 seed;

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_CHAIN_IMG_WIDTH, in_w);
	val = SET_CR_V(pmio, val, MLSC_F_CHAIN_IMG_HEIGHT, in_h);
	SET_CR(pmio, MLSC_R_CHAIN_IMG_SIZE, val);

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		_mlsc_hw_s_crc(pmio, seed);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_core);

int mlsc_hw_s_path(struct pablo_mmio *pmio, enum mlsc_input_path input,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	u32 cinfifo_en, chain_in;
	struct pablo_common_ctrl_cr_set *ext_cr_set;

	switch (input) {
	case OTF:
		cinfifo_en = 1;
		chain_in = OTF;
		break;
	case DMA:
	case VOTF:
		cinfifo_en = 0;
		chain_in = DMA;
		break;
	default:
		err_hw("[MLSC] Invalid s_input %d", input);
		return -ERANGE;
	}

	/* chain input0 select
	 * 0: CINFIFO
	 * 1: RDMA
	 */
	SET_CR(pmio, MLSC_R_CHAIN_INPUT_0_SELECT, chain_in);

	frame_cfg->cotf_in_en = cinfifo_en ? BIT_MASK(MLSC_COTF_IN_YUV) : 0;

	SET_CR_F(pmio, MLSC_R_OTF_PLATFORM_INPUT_MUX_0TO3, MLSC_F_OTF_PLATFORM_INPUT_0_MUX,
		cinfifo_en);

	/*TODO*/
	SET_CR_F(pmio, MLSC_R_CHAIN_MUX_SELECT, MLSC_F_LMEDS_INPUT_SELECT, 0);

	SET_CR(pmio, MLSC_R_IP_USE_CINFIFO_NEW_FRAME_IN, cinfifo_en);

	_mlsc_hw_s_otf(pmio, cinfifo_en);

	ext_cr_set = &frame_cfg->ext_cr_set;
	ext_cr_set->cr = mlsc_ext_cr_set;
	ext_cr_set->size = ARRAY_SIZE(mlsc_ext_cr_set);

	return 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_path);

struct mlsc_int_mask {
	ulong bitmask;
	bool partial;
};

static struct mlsc_int_mask int_mask[INT_TYPE_NUM] = {
	[INT_FRAME_START] = { BIT_MASK(INTR0_MLSC_FRAME_START_INT), false },
	[INT_FRAME_END] = { BIT_MASK(INTR0_MLSC_FRAME_END_INT), false },
	[INT_COREX_END] = { BIT_MASK(INTR0_MLSC_COREX_END_INT_0), false },
	[INT_SETTING_DONE] = { BIT_MASK(INTR0_MLSC_SETTING_DONE_INT), true },
	[INT_ERR0] = { INT0_ERR_MASK, true },
	[INT_WARN0] = { INT0_WARN_MASK, true },
	[INT_ERR1] = { INT1_ERR_MASK, true },
};

bool mlsc_hw_is_occurred(u32 status, ulong type)
{
	ulong i;
	u32 bitmask = 0, partial_mask = 0;
	bool occur = true;

	for_each_set_bit(i, &type, INT_TYPE_NUM) {
		if (int_mask[i].partial)
			partial_mask |= int_mask[i].bitmask;
		else
			bitmask |= int_mask[i].bitmask;
	}

	if (bitmask)
		occur = occur && ((status & bitmask) == bitmask);

	if (partial_mask)
		occur = occur && (status & partial_mask);

	return occur;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_is_occurred);

void mlsc_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = INT0_EN_MASK;
	int_en[PCC_INT_1] = INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_g_int_en);

u32 mlsc_hw_g_int_grp_en(void)
{
	return MLSC_INT_GRP_EN_MASK;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_g_int_grp_en);

void mlsc_hw_clr_cotf_err(struct pablo_mmio *pmio)
{
	u32 val;

	val = GET_CR(pmio, MLSC_R_YUV_CINFIFO_INT);
	SET_CR(pmio, MLSC_R_YUV_CINFIFO_INT_CLEAR, val);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_clr_cotf_err);

int mlsc_hw_wait_idle(struct pablo_mmio *pmio)
{
	u32 idle, int0_state, int1_state;
	int ret;

	idle = _mlsc_hw_g_idleness_state(pmio);
	int0_state = _mlsc_hw_g_int_state(pmio, MLSC_R_INT_REQ_INT0, 0, false);
	int1_state = _mlsc_hw_g_int_state(pmio, MLSC_R_INT_REQ_INT1, 0, false);

	mlsc_info("Wait IDLENESS start. idleness 0x%x int0 0x%x int1 0x%x", idle, int0_state,
		int1_state);

	ret = _mlsc_hw_wait_idleness(pmio);

	idle = _mlsc_hw_g_idleness_state(pmio);
	int0_state = _mlsc_hw_g_int_state(pmio, MLSC_R_INT_REQ_INT0, 0, false);
	int1_state = _mlsc_hw_g_int_state(pmio, MLSC_R_INT_REQ_INT1, 0, false);

	mlsc_info("Wait IDLENESS done. idleness 0x%x int0 0x%x int1 0x%x", idle, int0_state,
		int1_state);

	return ret;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_wait_idle);

int mlsc_hw_s_reset(struct pablo_mmio *pmio)
{
	int ret;
	u32 val = 0;
	u32 retry = MLSC_TRY_COUNT;

	SET_CR(pmio, MLSC_R_SW_RESET, 1);
	do {
		val = GET_CR(pmio, MLSC_R_SW_RESET);
		if (val)
			udelay(1);
		else
			break;
	} while (--retry);

	if (val) {
		err_hw("[MLSC] sw reset timeout(%#x)", val);
		return -ETIME;
	}

	ret = mlsc_hw_wait_idle(pmio);

	return ret;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_reset);

void mlsc_hw_s_lic_cfg(struct pablo_mmio *pmio, struct mlsc_lic_cfg *cfg)
{
	u32 val;
	u32 rdma_en;

	rdma_en = (cfg->input_path == OTF) ? 0 : 1;

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_LIC_BYPASS, cfg->bypass);
	val = SET_CR_V(pmio, val, MLSC_F_LIC_DEBUG_ON, 1); //TODO
	val = SET_CR_V(pmio, val, MLSC_F_LIC_FAKE_GEN_ON, USE_MLSC_LIC_RECOVERY);
	SET_CR(pmio, MLSC_R_LIC_INPUT_MODE, val);

	val = SET_CR_F(pmio, MLSC_R_LIC_INPUT_CONFIG_0, MLSC_F_LIC_BIT_MODE, MLSC_BIT_MODE);
	val = SET_CR_F(pmio, MLSC_R_LIC_INPUT_CONFIG_0, MLSC_F_LIC_RDMA_EN, rdma_en);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_LIC_IN_HBLANK_CYCLE, LIC_HBLALK_CYCLE);
	val = SET_CR_V(pmio, val, MLSC_F_LIC_OUT_HBLANK_CYCLE, LIC_HBLALK_CYCLE);
	SET_CR(pmio, MLSC_R_LIC_INPUT_BLANK, val);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_lic_cfg);

static void _mlsc_hw_g_fmt_map(
	enum mlsc_dma_id dma_id, ulong *byr_fmt_map, ulong *yuv_fmt_map, ulong *rgb_fmt_map)
{
	switch (dma_id) {
	case MLSC_R_CL:
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK)) & IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case MLSC_R_Y:
	case MLSC_R_UV:
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_PACK) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_PACK) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)) &
			       IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case MLSC_W_YUV444_Y:
	case MLSC_W_YUV444_U:
	case MLSC_W_YUV444_V:
	case MLSC_W_GLPG0_Y:
	case MLSC_W_GLPG1_Y:
	case MLSC_W_GLPG1_U:
	case MLSC_W_GLPG1_V:
	case MLSC_W_GLPG2_Y:
	case MLSC_W_GLPG2_U:
	case MLSC_W_GLPG2_V:
	case MLSC_W_GLPG3_Y:
	case MLSC_W_GLPG3_U:
	case MLSC_W_GLPG3_V:
	case MLSC_W_GLPG4_Y:
	case MLSC_W_GLPG4_U:
	case MLSC_W_GLPG4_V:
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U10BIT_PACK) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_PACK) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)) &
			       IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case MLSC_W_SVHIST:
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK)) & IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case MLSC_W_LMEDS:
		*byr_fmt_map = (0 | BIT_MASK(DMA_FMT_U8BIT_PACK) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_LSB_ZERO) |
				       BIT_MASK(DMA_FMT_U8BIT_UNPACK_MSB_ZERO)) &
			       IS_BAYER_FORMAT_MASK;
		*yuv_fmt_map = 0;
		*rgb_fmt_map = 0;
		break;
	case MLSC_W_FDPIG:
		*byr_fmt_map = 0;
		*yuv_fmt_map =
			(0 | BIT_MASK(DMA_FMT_YUV422_2P_UFIRST) |
				BIT_MASK(DMA_FMT_YUV420_2P_UFIRST) |
				BIT_MASK(DMA_FMT_YUV420_2P_VFIRST) | BIT_MASK(DMA_FMT_YUV444_1P)) &
			IS_YUV_FORMAT_MASK;
		*rgb_fmt_map =
			(0 | BIT_MASK(DMA_FMT_RGB_RGBA8888) | BIT_MASK(DMA_FMT_RGB_ARGB8888) |
				BIT_MASK(DMA_FMT_RGB_ABGR8888) | BIT_MASK(DMA_FMT_RGB_BGRA8888)) &
			IS_RGB_FORMAT_MASK;
		break;
	case MLSC_W_CAV:
		*byr_fmt_map = 0;
		*yuv_fmt_map =
			(0 | BIT_MASK(DMA_FMT_YUV422_2P_UFIRST) |
				BIT_MASK(DMA_FMT_YUV420_2P_UFIRST) |
				BIT_MASK(DMA_FMT_YUV420_2P_VFIRST) | BIT_MASK(DMA_FMT_YUV444_1P)) &
			IS_YUV_FORMAT_MASK;
		*rgb_fmt_map =
			(0 | BIT_MASK(DMA_FMT_RGB_RGBA8888) | BIT_MASK(DMA_FMT_RGB_ABGR8888) |
				BIT_MASK(DMA_FMT_RGB_ARGB8888) | BIT_MASK(DMA_FMT_RGB_BGRA8888)) &
			IS_RGB_FORMAT_MASK;
		break;
	default:
		err_hw("[MLSC] Invalid DMA id %d", dma_id);
		break;
	};
}

int mlsc_hw_create_dma(struct pablo_mmio *pmio, enum mlsc_dma_id dma_id, struct is_common_dma *dma)
{
	int ret;
	ulong byr_fmt_map, yuv_fmt_map, rgb_fmt_map;

	if (dma_id >= MLSC_DMA_NUM) {
		err_hw("[MLSC] Invalid dma_id %d", dma_id);
		return -EINVAL;
	} else if (mlsc_dmas[dma_id].dma_id == MLSC_DMA_NONE) {
		/* Not existing DMA */
		return 0;
	}

	dma->reg_ofs = mlsc_dmas[dma_id].sfr_offset;
	dma->field_ofs = mlsc_dmas[dma_id].field_offset;

	_mlsc_hw_g_fmt_map(dma_id, &byr_fmt_map, &yuv_fmt_map, &rgb_fmt_map);

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(
		dma, pmio, dma_id, mlsc_dmas[dma_id].name, byr_fmt_map, yuv_fmt_map, rgb_fmt_map);

	CALL_DMA_OPS(dma, dma_set_corex_id, COREX_DIRECT);

	dbg_hw(2, "[MLSC][%s] created. id %d BYR 0x%lX YUV 0x%lX RGB 0x%lX\n",
		mlsc_dmas[dma_id].name, dma_id, byr_fmt_map, yuv_fmt_map, rgb_fmt_map);

	return 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_create_dma);

static int _mlsc_hw_s_dma_addrs(struct is_common_dma *dma, struct mlsc_dma_cfg *cfg)
{
	u32 b, p, i;
	int ret = 0;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MLSC_R_Y:
	case MLSC_W_YUV444_Y:
	case MLSC_W_GLPG0_Y:
	case MLSC_W_GLPG1_Y:
	case MLSC_W_GLPG2_Y:
	case MLSC_W_GLPG3_Y:
	case MLSC_W_GLPG4_Y:
	case MLSC_W_SVHIST:
	case MLSC_W_LMEDS:
		for (i = 0; i < cfg->num_buffers; i++)
			address[i] = (dma_addr_t) * (cfg->addrs + (cfg->num_planes * i));
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, address, 0, cfg->buf_idx, cfg->num_buffers);
		break;
	case MLSC_R_UV:
	case MLSC_W_YUV444_U:
	case MLSC_W_GLPG1_U:
	case MLSC_W_GLPG2_U:
	case MLSC_W_GLPG3_U:
	case MLSC_W_GLPG4_U:
		for (i = 0; i < cfg->num_buffers; i++)
			address[i] = (dma_addr_t) * (cfg->addrs + (cfg->num_planes * i + 1));
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, address, 0, cfg->buf_idx, cfg->num_buffers);
		break;
	case MLSC_W_YUV444_V:
	case MLSC_W_GLPG1_V:
	case MLSC_W_GLPG2_V:
	case MLSC_W_GLPG3_V:
	case MLSC_W_GLPG4_V:
		for (i = 0; i < cfg->num_buffers; i++)
			address[i] = (dma_addr_t) * (cfg->addrs + (cfg->num_planes * i + 2));
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, address, 0, cfg->buf_idx, cfg->num_buffers);
		break;
	case MLSC_W_CAV:
	case MLSC_W_FDPIG:
		for (p = 0; p < cfg->num_planes; p++) {
			for (b = 0; b < cfg->num_buffers; b++) {
				i = (b * cfg->num_planes) + p;
				address[b] = (dma_addr_t)cfg->addrs[i];
			}
			CALL_DMA_OPS(
				dma, dma_set_img_addr, address, p, cfg->buf_idx, cfg->num_buffers);
		}
		break;
	default:
		err_hw("[MLSC] invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if ((cfg->sbwc_en == COMP) || (cfg->sbwc_en == COMP_LOSS)) {
		for (b = 0; b < cfg->num_buffers; b++)
			hdr_addr[b] = address[b] + cfg->p_size;

		CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, 0, cfg->buf_idx, cfg->num_buffers);
	}

	return ret;
}

static void _mlsc_hw_s_conv_cfg(
	struct pablo_mmio *pmio, enum mlsc_dma_id dma_id, u32 hw_format, bool en)
{
	switch (dma_id) {
	case MLSC_R_Y:
		if (!en || hw_format == DMA_INOUT_FORMAT_YUV422)
			SET_CR(pmio, MLSC_R_YUV_YUV420TO422_BYPASS, 1);
		else
			SET_CR(pmio, MLSC_R_YUV_YUV420TO422_BYPASS, 0);
		break;
	case MLSC_W_FDPIG:
		if (!en || hw_format == DMA_INOUT_FORMAT_RGB)
			SET_CR(pmio, MLSC_R_RGB_RGBTOYUVFDPIG_BYPASS, 1);
		else
			SET_CR(pmio, MLSC_R_RGB_RGBTOYUVFDPIG_BYPASS, 0);
		break;
	case MLSC_W_CAV:
		if (!en || hw_format == DMA_INOUT_FORMAT_RGB)
			SET_CR(pmio, MLSC_R_RGB_RGBTOYUVCAV_BYPASS, 1);
		else
			SET_CR(pmio, MLSC_R_RGB_RGBTOYUVCAV_BYPASS, 0);
		break;
	default:
		return;
	}
}

static void _mlsc_hw_s_img_size_cfg(
	struct pablo_mmio *pmio, enum mlsc_dma_id dma_id, u32 width, u32 height)
{
	int val = 0;

	if (!mlsc_dmas[dma_id].size_sfr_offset)
		return;

	val = SET_CR_V(pmio, val, mlsc_dmas[dma_id].size_field_offset, width);
	val = SET_CR_V(pmio, val, mlsc_dmas[dma_id].size_field_offset + 1, height);
	SET_CR(pmio, mlsc_dmas[dma_id].size_sfr_offset, val);
}

static void _mlsc_hw_s_ds_lme(struct pablo_mmio *pmio, bool en, struct mlsc_ds_cfg *cfg)
{
	u32 val;

	if (!en) {
		SET_CR(pmio, MLSC_R_Y_DSLME_BYPASS, 1);
		return;
	}

	/* Scale configuration */
	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_Y_DSLME_OUT_W, cfg->ot_crop.w);
	val = SET_CR_V(pmio, val, MLSC_F_Y_DSLME_OUT_H, cfg->ot_crop.h);
	SET_CR(pmio, MLSC_R_Y_DSLME_OUT, val);

	/* Scale factor configuration */
	SET_CR_F(pmio, MLSC_R_Y_DSLME_X_SCALE, MLSC_F_Y_DSLME_SCALE_FACTOR_X, cfg->scale_x);
	SET_CR_F(pmio, MLSC_R_Y_DSLME_Y_SCALE, MLSC_F_Y_DSLME_SCALE_FACTOR_Y, cfg->scale_y);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_Y_DSLME_INV_SCALE_X, cfg->inv_scale_x);
	val = SET_CR_V(pmio, val, MLSC_F_Y_DSLME_INV_SCALE_Y, cfg->inv_scale_y);
	SET_CR(pmio, MLSC_R_Y_DSLME_INV_SCALE, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_Y_DSLME_INV_SHIFT_X, cfg->inv_shift_x);
	val = SET_CR_V(pmio, val, MLSC_F_Y_DSLME_INV_SHIFT_Y, cfg->inv_shift_y);
	SET_CR(pmio, MLSC_R_Y_DSLME_INV_SHIFT, val);

	SET_CR(pmio, MLSC_R_Y_DSLME_BYPASS, 0);
}

static void _mlsc_hw_s_ds_fdpig(struct pablo_mmio *pmio, bool en, struct mlsc_ds_cfg *cfg)
{
	u32 val;

	if (!en) {
		SET_CR_F(pmio, MLSC_R_YUV_DSFDPIG_CROP_EN, MLSC_F_YUV_DSFDPIG_CROP_EN, 0);
		SET_CR_F(pmio, MLSC_R_YUV_DSFDPIG_BYPASS, MLSC_F_YUV_DSFDPIG_BYPASS, 1);

		return;
	}

	/* Crop configuration */
	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_CROP_START_X, cfg->in_crop.x);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_CROP_START_Y, cfg->in_crop.y);
	SET_CR(pmio, MLSC_R_YUV_DSFDPIG_CROP_START, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_CROP_SIZE_X, cfg->in_crop.w);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_CROP_SIZE_Y, cfg->in_crop.h);
	SET_CR(pmio, MLSC_R_YUV_DSFDPIG_CROP_SIZE, val);

	SET_CR_F(pmio, MLSC_R_YUV_DSFDPIG_CROP_EN, MLSC_F_YUV_DSFDPIG_CROP_EN, 1);

	/* Scale configuration */
	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_OUTPUT_W, cfg->ot_crop.w);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_OUTPUT_H, cfg->ot_crop.h);
	SET_CR(pmio, MLSC_R_YUV_DSFDPIG_OUTPUT_SIZE, val);

	/* Scale factor configuration */
	SET_CR_F(pmio, MLSC_R_YUV_DSFDPIG_SCALE_X, MLSC_F_YUV_DSFDPIG_SCALE_X, cfg->scale_x);
	SET_CR_F(pmio, MLSC_R_YUV_DSFDPIG_SCALE_Y, MLSC_F_YUV_DSFDPIG_SCALE_Y, cfg->scale_y);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_INV_SCALE_X, cfg->inv_scale_x);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_INV_SCALE_Y, cfg->inv_scale_y);
	SET_CR(pmio, MLSC_R_YUV_DSFDPIG_INV_SCALE, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_INV_SHIFT_X, cfg->inv_shift_x);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_INV_SHIFT_Y, cfg->inv_shift_y);
	SET_CR(pmio, MLSC_R_YUV_DSFDPIG_INV_SHIFT, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_BYPASS, 0);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSFDPIG_OUT_FORMAT_TYPE, cfg->format);
	SET_CR(pmio, MLSC_R_YUV_DSFDPIG_BYPASS, val);
}

static void _mlsc_hw_s_ds_cav(struct pablo_mmio *pmio, bool en, struct mlsc_ds_cfg *cfg)
{
	u32 val;

	if (!en) {
		SET_CR_F(pmio, MLSC_R_YUV_DSCAV_CROP_EN, MLSC_F_YUV_DSCAV_CROP_EN, 0);
		SET_CR_F(pmio, MLSC_R_YUV_DSCAV_BYPASS, MLSC_F_YUV_DSCAV_BYPASS, 1);

		return;
	}

	/* Crop configuration */
	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_CROP_START_X, cfg->in_crop.x);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_CROP_START_Y, cfg->in_crop.y);
	SET_CR(pmio, MLSC_R_YUV_DSCAV_CROP_START, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_CROP_SIZE_X, cfg->in_crop.w);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_CROP_SIZE_Y, cfg->in_crop.h);
	SET_CR(pmio, MLSC_R_YUV_DSCAV_CROP_SIZE, val);

	SET_CR_F(pmio, MLSC_R_YUV_DSCAV_CROP_EN, MLSC_F_YUV_DSCAV_CROP_EN, 1);

	/* Scale configuration */
	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_OUTPUT_W, cfg->ot_crop.w);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_OUTPUT_H, cfg->ot_crop.h);
	SET_CR(pmio, MLSC_R_YUV_DSCAV_OUTPUT_SIZE, val);

	/* Scale factor configuration */
	SET_CR_F(pmio, MLSC_R_YUV_DSCAV_SCALE_X, MLSC_F_YUV_DSCAV_SCALE_X, cfg->scale_x);
	SET_CR_F(pmio, MLSC_R_YUV_DSCAV_SCALE_Y, MLSC_F_YUV_DSCAV_SCALE_Y, cfg->scale_y);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_INV_SCALE_X, cfg->inv_scale_x);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_INV_SCALE_Y, cfg->inv_scale_y);
	SET_CR(pmio, MLSC_R_YUV_DSCAV_INV_SCALE, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_INV_SHIFT_X, cfg->inv_shift_x);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_INV_SHIFT_Y, cfg->inv_shift_y);
	SET_CR(pmio, MLSC_R_YUV_DSCAV_INV_SHIFT, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_BYPASS, 0);
	val = SET_CR_V(pmio, val, MLSC_F_YUV_DSCAV_OUT_FORMAT_TYPE, cfg->format);
	SET_CR(pmio, MLSC_R_YUV_DSCAV_BYPASS, val);
}

void mlsc_hw_s_menr_cfg(
	struct pablo_mmio *pmio, struct mlsc_radial_cfg *radial_cfg, struct is_rectangle *lmeds_dst)
{
	u32 val;
	u32 sensor_binning_x, sensor_binning_y, csis_binning_x, csis_binning_y;
	u32 scale_ratio_x, scale_ratio_y;
	u32 sensor_crop_x, sensor_crop_y, rgbp_crop_offset_x, rgbp_crop_offset_y;
	u32 xbin, ybin, crop_x, crop_y;

	sensor_binning_x = radial_cfg->sensor_binning_x;
	sensor_binning_y = radial_cfg->sensor_binning_y;
	csis_binning_x = radial_cfg->csis_binning_x;
	csis_binning_y = radial_cfg->csis_binning_y;

	/* scale_ratio_x = yuvsc scale ratio * lmeds ratio */
	scale_ratio_x = radial_cfg->rgbp_crop_w * 1000 / lmeds_dst->w;
	scale_ratio_y = radial_cfg->rgbp_crop_h * 1000 / lmeds_dst->h;

	xbin = 1024ULL * sensor_binning_x * csis_binning_x * scale_ratio_x / 1000 / 1000 / 1000;
	ybin = 1024ULL * sensor_binning_y * csis_binning_y * scale_ratio_y / 1000 / 1000 / 1000;

	rgbp_crop_offset_x = radial_cfg->rgbp_crop_offset_x;
	rgbp_crop_offset_y = radial_cfg->rgbp_crop_offset_y;
	sensor_crop_x = radial_cfg->sensor_crop_x;
	sensor_crop_y = radial_cfg->sensor_crop_y;

	crop_x = sensor_binning_x * (sensor_crop_x + (csis_binning_x * rgbp_crop_offset_x / 1000)) /
		 1000;
	crop_y = sensor_binning_y * (sensor_crop_y + (csis_binning_y * rgbp_crop_offset_y / 1000)) /
		 1000;

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_Y_MENR_CROPX, crop_x);
	val = SET_CR_V(pmio, val, MLSC_F_Y_MENR_CROPY, crop_y);
	SET_CR(pmio, MLSC_R_Y_MENR_CROP, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_Y_MENR_X_IMAGE_SIZE, lmeds_dst->w);
	val = SET_CR_V(pmio, val, MLSC_F_Y_MENR_XBIN, xbin);
	SET_CR(pmio, MLSC_R_Y_MENR_X_IMAGE_SIZE, val);

	val = 0;
	val = SET_CR_V(pmio, val, MLSC_F_Y_MENR_Y_IMAGE_SIZE, lmeds_dst->h);
	val = SET_CR_V(pmio, val, MLSC_F_Y_MENR_YBIN, ybin);
	SET_CR(pmio, MLSC_R_Y_MENR_Y_IMAGE_SIZE, val);
}

static void _mlsc_hw_s_svhist_bypass(struct pablo_mmio *pmio, u32 bypass)
{
	SET_CR(pmio, MLSC_R_RGB_SVHIST_BYPASS, bypass);
}

static void _mlsc_hw_s_edgescore_bypass(struct pablo_mmio *pmio, u32 bypass)
{
	SET_CR(pmio, MLSC_R_Y_EDGESCORE_BYPASS, bypass);
}

void mlsc_hw_s_config(struct pablo_mmio *pmio, struct mlsc_size_cfg *size,
	struct mlsc_param_set *p_set, struct is_mlsc_config *conf)
{
	if (p_set->dma_output_svhist.cmd && conf->svhist_bypass) {
		p_set->dma_output_svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		dbg_hw(1, "[%d][MLSC][F%d] bypass SVHIST", p_set->instance_id, p_set->fcount);
	} else if (!conf->svhist_bypass) {
		p_set->dma_output_svhist.width = MLSC_SVHIST_WDMA_W;
		p_set->dma_output_svhist.height = conf->svhist_grid_num;
		dbg_hw(3, "[%d][MLSC]set_config-%s:[F%d] svhist_grid_num %d\n", p_set->instance_id,
			__func__, p_set->fcount, conf->svhist_grid_num);
	}

	_mlsc_hw_s_svhist_bypass(pmio, conf->svhist_bypass);

	if (p_set->dma_output_lme_ds.cmd && conf->lmeds_bypass) {
		p_set->dma_output_lme_ds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		dbg_hw(1, "[%d][MLSC][F%d] bypass LMEDS0", p_set->instance_id, p_set->fcount);
	} else if (!conf->lmeds_bypass) {
		p_set->dma_output_lme_ds.width = conf->lmeds_w;
		p_set->dma_output_lme_ds.height = conf->lmeds_h;
		p_set->dma_output_lme_ds.stride_plane0 = conf->lmeds_stride;

		if (!p_set->dma_output_lme_ds.width || !p_set->dma_output_lme_ds.height) {
			p_set->dma_output_lme_ds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			warn_hw("[%d][MLSC][F%d] Invalid LMEDS0 size %dx%d", p_set->instance_id,
				p_set->fcount, p_set->dma_output_lme_ds.width,
				p_set->dma_output_lme_ds.height);
		}

		dbg_hw(3, "[%d][MLSC]set_config-%s:[F%d] lmeds %dx%d stride %d\n",
			p_set->instance_id, __func__, p_set->fcount, conf->lmeds_w, conf->lmeds_h,
			conf->lmeds_stride);
	}

	_mlsc_hw_s_edgescore_bypass(pmio, conf->lmeds_bypass);
}

static inline int _mlsc_hw_g_inv_shift(u32 scale)
{
	u32 shift_num;

	/* 12 fractional bit calculation */
	if (scale == (1 << 12)) /* x1.0 */
		shift_num = 26;
	else if (scale <= (2 << 12)) /* x2.0 */
		shift_num = 27;
	else if (scale <= (4 << 12)) /* x4.0 */
		shift_num = 28;
	else if (scale <= (8 << 12)) /* x8.0 */
		shift_num = 29;
	else if (scale <= (16 << 12)) /* x16.0 */
		shift_num = 30;
	else
		shift_num = 31;

	return shift_num;
}

int mlsc_hw_s_ds_cfg(struct pablo_mmio *pmio, enum mlsc_dma_id dma_id,
	struct mlsc_size_cfg *size_cfg, struct is_mlsc_config *conf, struct mlsc_param_set *p_set)
{
	struct param_dma_output *dma_out;
	struct mlsc_ds_cfg cfg;
	void (*func_ds_cfg)(struct pablo_mmio *pmio, bool en, struct mlsc_ds_cfg *cfg);
	u32 in_w, in_h, ds_w, ds_h, total_w, total_h, max_w, max_h;
	bool ds_en;

	in_w = ds_w = size_cfg->input_w;
	in_h = ds_h = size_cfg->input_h;

	switch (dma_id) {
	case MLSC_W_LMEDS:
		dma_out = &p_set->dma_output_lme_ds;
		func_ds_cfg = _mlsc_hw_s_ds_lme;
		ds_en = (!conf->lmeds_bypass) ? true : false;
		if (ds_en) {
			size_cfg->lmeds_dst.w = conf->lmeds_w;
			size_cfg->lmeds_dst.h = conf->lmeds_h;
		} else {
			size_cfg->lmeds_dst.w = size_cfg->input_w;
			size_cfg->lmeds_dst.h = size_cfg->input_h;
		}
		ds_w = size_cfg->lmeds_dst.w;
		ds_h = size_cfg->lmeds_dst.h;
		max_w = MLSC_LMEDS_OUT_MAX_W;
		max_h = MLSC_LMEDS_OUT_MAX_H;
		break;
	case MLSC_W_FDPIG:
		dma_out = &p_set->dma_output_fdpig;
		func_ds_cfg = _mlsc_hw_s_ds_fdpig;
		ds_en = dma_out->cmd ? true : false;
		max_w = MLSC_FDPIG_OUT_MAX_W;
		max_h = MLSC_FDPIG_OUT_MAX_H;
		break;
	case MLSC_W_CAV:
		dma_out = &p_set->dma_output_cav;
		func_ds_cfg = _mlsc_hw_s_ds_cav;
		ds_en = dma_out->cmd ? true : false;
		max_w = MLSC_CAV_OUT_MAX_W;
		max_h = MLSC_CAV_OUT_MAX_H;
		break;
	default:
		/* Other DMA doesn't have DS or doesn't be controlled by driver. */
		return 0;
	}

	if (!dma_out->cmd)
		_mlsc_hw_s_img_size_cfg(pmio, dma_id, ds_w, ds_h);

	/* Update DS input crop */
	switch (dma_id) {
	case MLSC_W_LMEDS:
		/* No crop */
		dma_out->dma_crop_offset_x = 0;
		dma_out->dma_crop_offset_y = 0;
		dma_out->dma_crop_width = in_w;
		dma_out->dma_crop_height = in_h;
		break;
	case MLSC_W_FDPIG:
	case MLSC_W_CAV:
		/* User controls crop */
		if (size_cfg->rms_crop_ratio) {
			dma_out->dma_crop_offset_x =
				dma_out->dma_crop_offset_x * size_cfg->rms_crop_ratio / 10;
			dma_out->dma_crop_offset_y =
				dma_out->dma_crop_offset_y * size_cfg->rms_crop_ratio / 10;
			dma_out->dma_crop_width =
				dma_out->dma_crop_width * size_cfg->rms_crop_ratio / 10;
			dma_out->dma_crop_height =
				dma_out->dma_crop_height * size_cfg->rms_crop_ratio / 10;
		} else {
			warn_hw("[MLSC][%s] Invalid rms_crop_ratio(%d)", mlsc_dmas[dma_id].name,
				size_cfg->rms_crop_ratio);
		}
		break;
	default:
		return 0;
	}

	if (!ds_en) {
		func_ds_cfg(pmio, false, NULL);
		return 0;
	}

	/* Check incrop boundary */
	total_w = dma_out->dma_crop_offset_x + dma_out->dma_crop_width;
	total_h = dma_out->dma_crop_offset_y + dma_out->dma_crop_height;
	if (total_w > in_w || total_h > in_h) {
		if (size_cfg->rms_crop_ratio > 10) {
			dbg_hw(2, "[MLSC][%s] Invalid incrop %dx%d -> %d,%d %dx%d",
				mlsc_dmas[dma_id].name, in_w, in_h, dma_out->dma_crop_offset_x,
				dma_out->dma_crop_offset_y, dma_out->dma_crop_width,
				dma_out->dma_crop_height);

			dma_out->dma_crop_offset_x = 0;
			dma_out->dma_crop_offset_y = 0;
			dma_out->dma_crop_width = in_w;
			dma_out->dma_crop_height = in_h;
		} else {
			err_hw("[MLSC][%s] Invalid incrop %dx%d -> %d,%d %dx%d",
				mlsc_dmas[dma_id].name, in_w, in_h, dma_out->dma_crop_offset_x,
				dma_out->dma_crop_offset_y, dma_out->dma_crop_width,
				dma_out->dma_crop_height);

			return -EINVAL;
		}
	}

	/* Check otcrop boundary */
	total_w = MIN(max_w, dma_out->dma_crop_width);
	total_h = MIN(max_h, dma_out->dma_crop_height);
	if (total_w < dma_out->width || total_h < dma_out->height) {
		err_hw("[MLSC][%s] Invalid otcrop %dx%d -> %dx%d", mlsc_dmas[dma_id].name, total_w,
			total_h, dma_out->width, dma_out->height);

		return -EINVAL;
	}

	cfg.in_crop.x = dma_out->dma_crop_offset_x;
	cfg.in_crop.y = dma_out->dma_crop_offset_y;
	cfg.in_crop.w = dma_out->dma_crop_width;
	cfg.in_crop.h = dma_out->dma_crop_height;
	cfg.ot_crop.x = 0;
	cfg.ot_crop.y = 0;
	cfg.ot_crop.w = dma_out->width;
	cfg.ot_crop.h = dma_out->height;

	if (dma_out->format == DMA_INOUT_FORMAT_RGB)
		cfg.format = MLSC_DS_OUT_RGB;
	else if (dma_out->format == DMA_INOUT_FORMAT_YUV422)
		cfg.format = MLSC_DS_OUT_YUV422;
	else
		cfg.format = MLSC_DS_OUT_YUV420;

	/* Apply the modified ds out size to DMA */
	dma_out->width = cfg.ot_crop.w;
	dma_out->height = cfg.ot_crop.h;

	cfg.scale_x = (cfg.in_crop.w << 12) / cfg.ot_crop.w;
	cfg.scale_y = (cfg.in_crop.h << 12) / cfg.ot_crop.h;
	cfg.inv_shift_x = _mlsc_hw_g_inv_shift(cfg.scale_x);
	cfg.inv_shift_y = _mlsc_hw_g_inv_shift(cfg.scale_y);
	cfg.inv_scale_x = (1 << cfg.inv_shift_x) / cfg.scale_x;
	cfg.inv_scale_y = (1 << cfg.inv_shift_y) / cfg.scale_y;

	func_ds_cfg(pmio, true, &cfg);

	dbg_hw(2, "[MLSC][%s]DS: %d,%d %dx%d -> %dx%d\n", mlsc_dmas[dma_id].name, cfg.in_crop.x,
		cfg.in_crop.y, cfg.in_crop.w, cfg.in_crop.h, cfg.ot_crop.w, cfg.ot_crop.h);

	return 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_ds_cfg);

static void _mlsc_hw_s_sbwc_32x4_ctrl(
	struct pablo_mmio *pmio, enum mlsc_dma_id dma_id, u32 sbwc_type)
{
	u32 val = sbwc_type ? 1 : 0;

	switch (dma_id) {
	case MLSC_R_Y:
		SET_CR_F(pmio, MLSC_R_SBWC_32X4_CTRL_0, MLSC_F_TILE_TO_OTF_ENABLE, val);
		break;
	case MLSC_W_GLPG0_Y:
		SET_CR_F(pmio, MLSC_R_SBWC_32X4_CTRL_0, MLSC_F_OTF_TO_TILE_ENABLE_L0, val);
		break;
	case MLSC_W_GLPG1_Y:
		SET_CR_F(pmio, MLSC_R_SBWC_32X4_CTRL_0, MLSC_F_OTF_TO_TILE_ENABLE_L1, val);
		break;
	case MLSC_W_GLPG2_Y:
		SET_CR_F(pmio, MLSC_R_SBWC_32X4_CTRL_0, MLSC_F_OTF_TO_TILE_ENABLE_L2, val);
		break;
	default:
		break;
	}
}

int mlsc_hw_s_rdma_cfg(struct is_common_dma *dma, struct mlsc_param_set *param, u32 num_buffers)
{
	int ret = 0;
	struct param_dma_input *dma_in;
	struct mlsc_dma_cfg cfg;
	u32 sbwc_type, quality_control = 0;
	u32 hw_format = 0, bit_width, pixel_size;
	int format;
	u32 width, height;
	u32 en_votf, order;
	u32 comp_64b_align, stride_1p, hdr_stride_1p = 0, align = 16;
	enum format_type dma_type;

	switch (dma->id) {
	case MLSC_R_Y:
	case MLSC_R_UV:
		dma_in = &param->dma_input;
		cfg.addrs = param->input_dva;
		dma_type = DMA_FMT_BAYER;
		break;
	case MLSC_DMA_NONE:
		/* Not existing DMA */
		return 0;
	case MLSC_R_CL:
	default:
		warn_hw("[MLSC][%s] NOT supported DMA", dma->name);
		return 0;
	}

	if (dma_in->cmd == DMA_INPUT_COMMAND_DISABLE)
		goto skip_dma;

	sbwc_type = dma_in->sbwc_type;
	hw_format = dma_in->format;
	bit_width = dma_in->bitwidth;
	pixel_size = dma_in->msb + 1;
	width = dma_in->width;
	height = dma_in->height;
	if (dma->id == MLSC_R_UV)
		height = height / 2;

	en_votf = dma_in->v_otf_enable;
	order = dma_in->order;

	if (hw_format == DMA_INOUT_FORMAT_YUV420)
		SET_CR_F(dma->base, MLSC_R_RDMA_IN_FORMAT, MLSC_F_RDMA_IN_YUV_FORMAT,
			0); /* 0: YUV420, 1: YUV422 */

	if (order == DMA_INOUT_ORDER_CbCr)
		SET_CR_F(dma->base, MLSC_R_RDMA_IN_FORMAT, MLSC_F_RDMA_IN_UV_ALIGN,
			0); /* 0: U first, 1: V first */

	_mlsc_hw_s_conv_cfg(dma->base, dma->id, hw_format, true);
	_mlsc_hw_s_sbwc_32x4_ctrl(dma->base, dma->id, sbwc_type);

	cfg.sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);

	if (cfg.sbwc_en == COMP_LOSS)
		quality_control = LOSSY_COMP_FOOTPRINT;

	switch (cfg.sbwc_en) {
	case NONE: /* SBWC_DISABLE */
		stride_1p = is_hw_dma_get_img_stride(
			bit_width, pixel_size, hw_format, width, align, true);
		break;
	case COMP: /* SBWC_LOSSYLESS_32B/64B */
		hdr_stride_1p = is_hw_dma_get_header_stride(width, MLSC_COMP_BLOCK_WIDTH, align);
		fallthrough;
	case COMP_LOSS: /* SBWC_LOSSY_32B/64B */
		stride_1p =
			is_hw_dma_get_payload_stride(cfg.sbwc_en, pixel_size, width, comp_64b_align,
				quality_control, MLSC_COMP_BLOCK_WIDTH, MLSC_COMP_BLOCK_HEIGHT);
		break;
	default:
		err_hw("[MLSC][%s] Invalid SBWC mode. ret %d", dma->name, cfg.sbwc_en);
		goto skip_dma;
	}

	ret = is_hw_dma_get_bayer_format(
		bit_width, pixel_size, hw_format, cfg.sbwc_en, /* is_msb */ true, &format);
	if (ret || (stride_1p & 0x1)) {
		err_hw("[MLSC][%s] invalid rdma_cfg: format %d stride_1p %d", dma->name, format,
			stride_1p);
		goto skip_dma;
	}

	ret = CALL_DMA_OPS(dma, dma_set_format, format, dma_type);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, cfg.sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, en_votf, 0);
	/* ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info); */

	if (bit_width != DMA_INOUT_BIT_WIDTH_12BIT)
		ret |= CALL_DMA_OPS(dma, dma_set_msb_align, 0, 1);

	if (ret)
		goto skip_dma;

	switch (cfg.sbwc_en) {
	case COMP:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, hdr_stride_1p, 0);
		cfg.p_size = ALIGN(height, MLSC_COMP_BLOCK_HEIGHT) * stride_1p;
		break;
	case COMP_LOSS:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		break;
	default:
		break;
	}

	if (ret)
		goto skip_dma;

	cfg.num_planes = dma_in->plane;
	cfg.num_buffers = num_buffers;
	cfg.buf_idx = 0;

	_mlsc_hw_s_dma_addrs(dma, &cfg);

	dbg_hw(2, "[MLSC][%s]dma_cfg: size %dx%d format %d-%d plane %d votf %d\n", dma->name, width,
		height, dma_type, format, cfg.num_planes, en_votf);
	dbg_hw(2, "[MLSC][%s]stride_cfg: img %d hdr %d\n", dma->name, stride_1p, hdr_stride_1p);
	dbg_hw(2, "[MLSC][%s]sbwc_cfg: en %d 64b_align %d quality_control %d\n", dma->name,
		cfg.sbwc_en, comp_64b_align, quality_control);
	dbg_hw(2, "[MLSC][%s]dma_addr: img[0] 0x%llx\n", dma->name, cfg.addrs[0]);

	CALL_DMA_OPS(dma, dma_enable, 1);

	return 0;

skip_dma:
	dbg_hw(2, "[MLSC][%s]dma_cfg: OFF\n", dma->name);

	_mlsc_hw_s_conv_cfg(dma->base, dma->id, hw_format, false);
	CALL_DMA_OPS(dma, dma_enable, 0);

	return 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_rdma_cfg);

static inline bool _mlsc_hw_is_rgb(u32 hw_order)
{
	/* Common DMA RGB format always covers 4 color channels. */
	switch (hw_order) {
	case DMA_INOUT_ORDER_ARGB:
	case DMA_INOUT_ORDER_BGRA:
	case DMA_INOUT_ORDER_RGBA:
	case DMA_INOUT_ORDER_ABGR:
		return true;
	default:
		return false;
	}
}

static inline bool _mlsc_hw_is_yuv(u32 hw_format, u32 hw_order)
{
	switch (hw_format) {
	case DMA_INOUT_FORMAT_YUV444:
	case DMA_INOUT_FORMAT_YUV422:
	case DMA_INOUT_FORMAT_YUV420:
	case DMA_INOUT_FORMAT_YUV422_CHUNKER:
	case DMA_INOUT_FORMAT_YUV444_TRUNCATED:
	case DMA_INOUT_FORMAT_YUV422_TRUNCATED:
	case DMA_INOUT_FORMAT_YUV422_PACKED:
		return true;
	case DMA_INOUT_FORMAT_Y:
		/* 8bit y only format is handled as YUV420 2p format */
		return true;
	case DMA_INOUT_FORMAT_RGB:
		/* 8bit RGB format is handled as YUV444 format */
		if (hw_order == DMA_INOUT_ORDER_BGR)
			return true;
		else
			return false;
	default:
		return false;
	}
}

int mlsc_hw_s_wdma_cfg(
	struct is_common_dma *dma, struct mlsc_param_set *param, u32 num_buffers, int disable)
{
	int ret = 0;
	struct param_dma_output *dma_out = NULL;
	struct mlsc_dma_cfg cfg;
	u32 hw_format = 0, hw_order, bit_width, pixel_size, sbwc_type;
	int format;
	u32 width, height;
	u32 stride_1p, stride_2p = 0, stride_3p = 0, header_stride_1p = 0;
	u32 comp_64b_align = 1, quality_control = 0;
	enum format_type dma_type;
	bool img_flag = false;

	memset(&cfg, 0x00, sizeof(struct mlsc_dma_cfg));
	sbwc_type = DMA_INPUT_SBWC_DISABLE;

	switch (dma->id) {
	case MLSC_W_YUV444_Y:
	case MLSC_W_YUV444_U:
	case MLSC_W_YUV444_V:
		dma_out = &param->dma_output_yuv;
		cfg.addrs = param->output_dva_yuv;
		img_flag = true;
		break;
	case MLSC_W_GLPG0_Y:
		dma_out = &param->dma_output_glpg[0];
		cfg.addrs = param->output_dva_glpg[0];
		img_flag = true;
		sbwc_type = dma_out->sbwc_type;
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;
		break;
	case MLSC_W_GLPG1_Y:
		dma_out = &param->dma_output_glpg[1];
		cfg.addrs = param->output_dva_glpg[1];
		img_flag = true;
		sbwc_type = dma_out->sbwc_type;
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;
		break;
	case MLSC_W_GLPG1_U:
	case MLSC_W_GLPG1_V:
		dma_out = &param->dma_output_glpg[1];
		cfg.addrs = param->output_dva_glpg[1];
		img_flag = true;
		sbwc_type = dma_out->sbwc_type;
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;
		break;
	case MLSC_W_GLPG2_Y:
		dma_out = &param->dma_output_glpg[2];
		cfg.addrs = param->output_dva_glpg[2];
		img_flag = true;
		sbwc_type = dma_out->sbwc_type;
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;
		break;
	case MLSC_W_GLPG2_U:
	case MLSC_W_GLPG2_V:
		dma_out = &param->dma_output_glpg[2];
		cfg.addrs = param->output_dva_glpg[2];
		img_flag = true;
		sbwc_type = dma_out->sbwc_type;
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;
		break;
	case MLSC_W_GLPG3_Y:
	case MLSC_W_GLPG3_U:
	case MLSC_W_GLPG3_V:
		dma_out = &param->dma_output_glpg[3];
		cfg.addrs = param->output_dva_glpg[3];
		img_flag = true;
		break;
	case MLSC_W_GLPG4_Y:
	case MLSC_W_GLPG4_U:
	case MLSC_W_GLPG4_V:
		dma_out = &param->dma_output_glpg[4];
		cfg.addrs = param->output_dva_glpg[4];
		img_flag = true;
		break;
	case MLSC_W_SVHIST:
		dma_out = &param->dma_output_svhist;
		cfg.addrs = param->output_dva_svhist;
		cfg.num_buffers = 1;
		break;
	case MLSC_W_LMEDS:
		dma_out = &param->dma_output_lme_ds;
		cfg.addrs = param->output_dva_lme_ds;
		img_flag = true;
		break;
	case MLSC_W_FDPIG:
		dma_out = &param->dma_output_fdpig;
		cfg.addrs = param->output_dva_fdpig;
		img_flag = true;
		break;
	case MLSC_W_CAV:
		dma_out = &param->dma_output_cav;
		cfg.addrs = param->output_dva_cav;
		img_flag = true;
		break;
	case MLSC_DMA_NONE:
		/* Not existing DMA */
		return 0;
	default:
		warn_hw("[MLSC][%s] NOT supported DMA", dma->name);
		return 0;
	}

	if (disable || dma_out->cmd == DMA_OUTPUT_COMMAND_DISABLE)
		goto skip_dma;

	cfg.num_planes = dma_out->plane;
	width = dma_out->width;
	height = dma_out->height;
	hw_format = dma_out->format;

	_mlsc_hw_s_conv_cfg(dma->base, dma->id, hw_format, true);
	_mlsc_hw_s_img_size_cfg(dma->base, dma->id, width, height);
	_mlsc_hw_s_sbwc_32x4_ctrl(dma->base, dma->id, sbwc_type);

	hw_order = dma_out->order;
	bit_width = dma_out->bitwidth;
	pixel_size = dma_out->msb + 1;
	stride_1p = dma_out->stride_plane0 ? dma_out->stride_plane0 : width;
	stride_2p = dma_out->stride_plane1 ? dma_out->stride_plane1 : width;
	stride_3p = dma_out->stride_plane2 ? dma_out->stride_plane2 : width;

	cfg.sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);

	if (cfg.sbwc_en == COMP_LOSS)
		quality_control = LOSSY_COMP_FOOTPRINT;

	if (cfg.sbwc_en == NONE) {
		stride_1p = is_hw_dma_get_img_stride(
			bit_width, pixel_size, hw_format, stride_1p, 16, img_flag);
	} else if (cfg.sbwc_en == COMP || cfg.sbwc_en == COMP_LOSS) {
		stride_1p = is_hw_dma_get_payload_stride(cfg.sbwc_en, pixel_size, stride_1p,
			comp_64b_align, quality_control, MLSC_COMP_BLOCK_WIDTH,
			MLSC_COMP_BLOCK_HEIGHT);
		header_stride_1p =
			is_hw_dma_get_header_stride(width, MLSC_COMP_BLOCK_WIDTH, 16);
	} else {
		return -EINVAL;
	}

	if (dma->available_yuv_format_map && _mlsc_hw_is_yuv(hw_format, hw_order)) {
		dma_type = DMA_FMT_YUV;
		format = is_hw_dma_get_yuv_format(
			bit_width, hw_format, dma_out->plane, dma_out->order);
	} else if (dma->available_rgb_format_map && _mlsc_hw_is_rgb(hw_order)) {
		dma_type = DMA_FMT_RGB;
		format = is_hw_dma_get_rgb_format(bit_width, dma_out->plane, dma_out->order);
	} else if (dma->available_bayer_format_map) {
		dma_type = DMA_FMT_BAYER;
		ret = is_hw_dma_get_bayer_format(
			bit_width, pixel_size, hw_format, cfg.sbwc_en, true, &format);
	} else {
		ret = -EINVAL;
	}

	if (ret || (width & 0x1)) {
		err_hw("[MLSC][%s] invalid wdma_cfg: format %d order %d width %d", dma->name,
			hw_format, hw_order, width);
		goto skip_dma;
	}

	ret = CALL_DMA_OPS(dma, dma_set_format, format, dma_type);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, cfg.sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, stride_2p, stride_3p);

	switch (cfg.sbwc_en) {
	case COMP:
	case COMP_LOSS:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		cfg.p_size = ((height + MLSC_COMP_BLOCK_HEIGHT - 1) / MLSC_COMP_BLOCK_HEIGHT) *
			     stride_1p;
		break;
	default:
		break;
	}

	if (ret)
		goto skip_dma;

	if (cfg.num_buffers == 0)
		cfg.num_buffers = num_buffers;
	cfg.buf_idx = 0;

	_mlsc_hw_s_dma_addrs(dma, &cfg);

	dbg_hw(2, "[MLSC][%s]dma_cfg: size %dx%d format %d-%d plane %d, sbwc %d\n", dma->name, width, height,
		dma_type, format, cfg.num_planes, cfg.sbwc_en);
	dbg_hw(2, "[MLSC][%s]stride_cfg: img %d bit_width %d/%d  hdr %d\n", dma->name, stride_1p,
		pixel_size, bit_width, header_stride_1p);
	dbg_hw(2, "[MLSC][%s]sbwc_cfg: en %d 64b_align %d quality_control %d, payload_size %d\n",
		dma->name, cfg.sbwc_en, comp_64b_align, quality_control, cfg.p_size);
	dbg_hw(2, "[MLSC][%s]dma_addr: img[0] 0x%llx\n", dma->name, cfg.addrs[0]);

	CALL_DMA_OPS(dma, dma_enable, 1);

	return 0;

skip_dma:
	dbg_hw(2, "[MLSC][%s]dma_cfg: OFF\n", dma->name);

	_mlsc_hw_s_conv_cfg(dma->base, dma->id, hw_format, false);
	CALL_DMA_OPS(dma, dma_enable, 0);

	return 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_wdma_cfg);

#define DMA_AXI_DEBUG_CONTROL_OFFSET 0x1e4

void mlsc_hw_s_dma_debug(struct pablo_mmio *pmio, enum mlsc_dma_id dma_id)
{
	SET_CR(pmio, mlsc_dmas[dma_id].sfr_offset + DMA_AXI_DEBUG_CONTROL_OFFSET, 0x3);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_dma_debug);

int mlsc_hw_s_glpg(
	struct pablo_mmio *pmio, struct mlsc_param_set *p_set, struct mlsc_size_cfg *size)
{
	int val = 0, i;
	u32 glpg_reg_offset = MLSC_R_YUV_GLPGL1_BYPASS - MLSC_R_YUV_GLPGL0_BYPASS;

	SET_CR(pmio, MLSC_R_GLPGBYPASS_OUT_FORMAT, 0); /* 0: 12bit, 1: 10bit */

	/* GLPG L0 ~ L3 setting */
	for (i = 0; i < MLSC_GLPG_NUM - 1; i++) {
		if (!p_set->dma_output_glpg[i].cmd) {
			SET_CR_F(pmio, MLSC_R_YUV_GLPGL0_BYPASS + glpg_reg_offset * i,
				MLSC_F_YUV_GLPGL0_BYPASS, 1);
			continue;
		}

		if (p_set->dma_output_glpg[i].width & 1 || p_set->dma_output_glpg[i].height & 1 ||
			size->input_w < p_set->dma_output_glpg[i].width ||
			size->input_h < p_set->dma_output_glpg[i].height) {
			mlsc_err("The resolution of glpg%d size is wrong. %dx%d", i,
				p_set->dma_output_glpg[i].width, p_set->dma_output_glpg[i].height);
			return -EINVAL;
		}

		val = 0;
		val = SET_CR_V(pmio, val, MLSC_F_YUV_GLPGL0_BYPASS, 0);
		if (p_set->dma_output_glpg[i].bitwidth == DMA_INOUT_BIT_WIDTH_10BIT)
			val = SET_CR_V(pmio, val, MLSC_F_YUV_GLPGL0_BIT_10_MODE_EN, 1);
		SET_CR(pmio, MLSC_R_YUV_GLPGL0_BYPASS + glpg_reg_offset * i, val);

		val = 0;
		val = SET_CR_V(pmio, val, MLSC_F_YUV_GLPGL0_DOWNSCALER_SRC_VSIZE,
			p_set->dma_output_glpg[i].height);
		val = SET_CR_V(pmio, val, MLSC_F_YUV_GLPGL0_DOWNSCALER_SRC_HSIZE,
			p_set->dma_output_glpg[i].width);
		SET_CR(pmio, MLSC_R_YUV_GLPGL0_DOWNSCALER_SRC_SIZE + glpg_reg_offset * i, val);

		val = 0;
		val = SET_CR_V(pmio, val, MLSC_F_YUV_GLPGL0_DOWNSCALER_DST_VSIZE,
			p_set->dma_output_glpg[i + 1].height);
		val = SET_CR_V(pmio, val, MLSC_F_YUV_GLPGL0_DOWNSCALER_DST_HSIZE,
			p_set->dma_output_glpg[i + 1].width);
		SET_CR(pmio, MLSC_R_YUV_GLPGL0_DOWNSCALER_DST_SIZE + glpg_reg_offset * i, val);
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_glpg);

u32 mlsc_hw_g_edge_score(struct pablo_mmio *pmio)
{
	return GET_CR(pmio, MLSC_R_Y_EDGESCORE_EDGE_SCORE_ACCUM);
}

void mlsc_hw_s_bypass(struct pablo_mmio *pmio)
{
	SET_CR(pmio, MLSC_R_YUV_YUV420TO422_BYPASS, 1);
	SET_CR(pmio, MLSC_R_RGB_INVGAMMARGB_BYPASS, 1);
	SET_CR_F(pmio, MLSC_R_RGB_INVCCM33_CONFIG, MLSC_F_RGB_INVCCM33_BYPASS, 1);
	SET_CR(pmio, MLSC_R_RGB_BLCSTAT_BYPASS, 1);
	SET_CR(pmio, MLSC_R_RGB_SDRC_BYPASS, 1);
	SET_CR(pmio, MLSC_R_RGB_CCM9_BYPASS, 1);
	SET_CR(pmio, MLSC_R_RGB_GAMMARGB_BYPASS, 1);
	SET_CR(pmio, MLSC_R_RGB_SVHIST_BYPASS, 1);
	SET_CR(pmio, MLSC_R_RGB_RGB2Y_BYPASS, 1);
	SET_CR(pmio, MLSC_R_Y_MENR_BYPASS, 1);
	SET_CR(pmio, MLSC_R_Y_EDGESCORE_BYPASS, 1);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_bypass);

void mlsc_hw_s_dtp(struct pablo_mmio *pmio)
{
	dbg_hw(1, "[API][%s] dtp color bar pattern is enabled!\n", __func__);

	SET_CR_F(pmio, MLSC_R_YUV_DTP_BYPASS, MLSC_F_YUV_DTP_BYPASS, 0);
	SET_CR_F(pmio, MLSC_R_YUV_DTP_TEST_PATTERN_MODE, MLSC_F_YUV_DTP_TEST_PATTERN_MODE,
		2); /* color bar pattern */
}

void mlsc_hw_s_strgen(struct pablo_mmio *pmio)
{
	SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_CONFIG, MLSC_F_YUV_CINFIFO_STRGEN_MODE_EN, 1);
	SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_CONFIG, MLSC_F_YUV_CINFIFO_STRGEN_MODE_DATA_TYPE, 0);
	SET_CR_F(pmio, MLSC_R_YUV_CINFIFO_CONFIG, MLSC_F_YUV_CINFIFO_STRGEN_MODE_DATA, 128);
	SET_CR(pmio, MLSC_R_IP_USE_CINFIFO_NEW_FRAME_IN, 0);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_s_strgen);

void mlsc_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		mlsc_info("DUMP CR");
		is_hw_dump_regs(pmio_get_base(pmio), mlsc_regs, MLSC_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		mlsc_info("DUMP DBG_STATE");
		_mlsc_hw_dump_dbg_state(pmio);
		break;
	default:
		mlsc_err("Not supported dump_mode %d", mode);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_dump);

u32 mlsc_hw_g_reg_cnt(void)
{
	return MLSC_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_g_reg_cnt);

void mlsc_hw_g_pmio_cfg(struct pmio_config *pcfg)
{
	pcfg->rd_table = &mlsc_rd_ranges_table;
	pcfg->volatile_table = &mlsc_volatile_ranges_table;

	pcfg->max_register = MLSC_R_YUV_GLPGL3_LINE_CTRL_PIPE;
	pcfg->num_reg_defaults_raw = (MLSC_R_YUV_GLPGL3_LINE_CTRL_PIPE >> 2) + 1;
	pcfg->dma_addr_shift = LSB_BIT;

	pcfg->fields = mlsc_field_descs;
	pcfg->num_fields = ARRAY_SIZE(mlsc_field_descs);
}
KUNIT_EXPORT_SYMBOL(mlsc_hw_g_pmio_cfg);
