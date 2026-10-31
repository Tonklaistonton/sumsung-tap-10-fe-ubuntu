// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * MTNR1 HW control APIs
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include "is-hw-api-mtnr1-v13.h"
#include "is-hw-common-dma.h"
#include "is-hw.h"
#include "is-hw-control.h"
#include "sfr/is-sfr-mtnr1-v13_0.h"
#include "pmio.h"
#include "pablo-hw-api-common-ctrl.h"

#define MTNR1_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define MTNR1_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define MTNR1_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define MTNR1_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define MTNR1_GET_R(base, R) PMIO_GET_R(base, R)

#define VBLANK_CYCLE 0xA
#define HBLANK_CYCLE 0x2E
#define PBLANK_CYCLE 0

#define info_mtnr(fmt, args...) info_common("[HW][MTNR1]", fmt, ##args)
#define info_mtnr1_ver(fmt, args...) info_common("[HW][MTNR1](v13.0)", fmt, ##args)
#define err_mtnr(fmt, args...)                                                                     \
	err_common("[HW][MTNR1][ERR]%s:%d:", fmt "\n", __func__, __LINE__, ##args)
#define dbg_mtnr(level, fmt, args...)                                                              \
	dbg_common((is_get_debug_param(IS_DEBUG_PARAM_HW)) >= (level), "[HW][MTNR1]", fmt, ##args)

/* CMDQ Interrupt group mask */
#define MTNR1_INT_GRP_EN_MASK                                                                      \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_START) | BIT_MASK(PCC_INT_GRP_FRAME_END) |               \
		BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD) |                 \
		BIT_MASK(PCC_INT_GRP_SETTING_DONE) | BIT_MASK(PCC_INT_GRP_DEBUG) |                 \
		BIT_MASK(PCC_INT_GRP_ENABLE_ALL))
#define MTNR1_INT_GRP_EN_MASK_FRO_FIRST BIT_MASK(PCC_INT_GRP_FRAME_START)
#define MTNR1_INT_GRP_EN_MASK_FRO_MIDDLE 0
#define MTNR1_INT_GRP_EN_MASK_FRO_LAST BIT_MASK(PCC_INT_GRP_FRAME_END)

enum mtnr1_cotf_in_id {
	MTNR1_COTF_IN_DLFE_WGT,
};

enum mtnr1_cotf_out_id {
	MTNR1_COTF_OUT_MTNR0_WGT,
	MTNR1_COTF_OUT_MSNR_L1,
	MTNR1_COTF_OUT_MSNR_L2,
	MTNR1_COTF_OUT_MSNR_L3,
	MTNR1_COTF_OUT_MSNR_L4,
	MTNR1_COTF_OUT_DLFE_CUR,
	MTNR1_COTF_OUT_DLFE_PREV,
};

static const char *const is_hw_mtnr1_rdma_name[] = {
	"MTNR1_RDMA_C_L1_Y",
	"MTNR1_RDMA_C_L1_U",
	"MTNR1_RDMA_C_L1_V",
	"MTNR1_RDMA_C_L2_Y",
	"MTNR1_RDMA_C_L2_U",
	"MTNR1_RDMA_C_L2_V",
	"MTNR1_RDMA_C_L3_Y",
	"MTNR1_RDMA_C_L3_U",
	"MTNR1_RDMA_C_L3_V",
	"MTNR1_RDMA_C_L4_Y",
	"MTNR1_RDMA_C_L4_U",
	"MTNR1_RDMA_C_L4_V",
	"MTNR1_RDMA_P_L1_Y",
	"MTNR1_RDMA_P_L1_U",
	"MTNR1_RDMA_P_L1_V",
	"MTNR1_RDMA_P_L2_Y",
	"MTNR1_RDMA_P_L2_U",
	"MTNR1_RDMA_P_L2_V",
	"MTNR1_RDMA_P_L3_Y",
	"MTNR1_RDMA_P_L3_U",
	"MTNR1_RDMA_P_L3_V",
	"MTNR1_RDMA_P_L4_Y",
	"MTNR1_RDMA_P_L4_U",
	"MTNR1_RDMA_P_L4_V",
	"MTNR1_RDMA_P_L1_WGT",
	"MTNR1_RDMA_SEG_L1",
	"MTNR1_RDMA_SEG_L2",
	"MTNR1_RDMA_SEG_L3",
	"MTNR1_RDMA_SEG_L4",
	"MTNR1_RDMA_MV_GEOMATCH",
};

static const char *const is_hw_mtnr1_wdma_name[] = {
	"MTNR1_WDMA_P_L1_Y",
	"MTNR1_WDMA_P_L1_U",
	"MTNR1_WDMA_P_L1_V",
	"MTNR1_WDMA_P_L2_Y",
	"MTNR1_WDMA_P_L2_U",
	"MTNR1_WDMA_P_L2_V",
	"MTNR1_WDMA_P_L3_Y",
	"MTNR1_WDMA_P_L3_U",
	"MTNR1_WDMA_P_L3_V",
	"MTNR1_WDMA_P_L4_Y",
	"MTNR1_WDMA_P_L4_U",
	"MTNR1_WDMA_P_L4_V",
	"MTNR1_WDMA_P_L1_WGT",
};

static void __mtnr1_hw_s_secure_id(struct pablo_mmio *base, u32 set_id)
{
	MTNR1_SET_F(base, MTNR1_R_SECU_CTRL_SEQID, MTNR1_F_SECU_CTRL_SEQID,
		0); /* TODO: get secure scenario */
}

u32 mtnr1_hw_is_occurred(unsigned int status, enum mtnr1_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_FRAME_START:
		mask = 1 << INTR0_MTNR1_FRAME_START_INT;
		break;
	case INTR_FRAME_END:
		mask = 1 << INTR0_MTNR1_FRAME_END_INT;
		break;
	case INTR_COREX_END_0:
		mask = 1 << INTR0_MTNR1_COREX_END_INT_0;
		break;
	case INTR_COREX_END_1:
		mask = 1 << INTR0_MTNR1_COREX_END_INT_1;
		break;
	case INTR_SETTING_DONE:
		mask = 1 << INTR0_MTNR1_SETTING_DONE_INT;
		break;
	case INTR_ERR:
		mask = MTNR1_INT0_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

u32 mtnr1_hw_is_occurred1(unsigned int status, enum mtnr1_event_type type)
{
	u32 mask;

	switch (type) {
	case INTR_ERR:
		mask = MTNR1_INT1_ERR_MASK;
		break;
	default:
		return 0;
	}

	return status & mask;
}

int mtnr1_hw_wait_idle(struct pablo_mmio *base)
{
	int ret = 0;
	u32 idle;
	u32 int0_all, int1_all;
	u32 try_cnt = 0;

	idle = MTNR1_GET_F(base, MTNR1_R_IDLENESS_STATUS, MTNR1_F_IDLENESS_STATUS);
	int0_all = MTNR1_GET_R(base, MTNR1_R_INT_REQ_INT0);
	int1_all = MTNR1_GET_R(base, MTNR1_R_INT_REQ_INT1);

	info_mtnr(
		"idle status before disable (idle:%d, int:0x%X, 0x%X)\n", idle, int0_all, int1_all);

	while (!idle) {
		idle = MTNR1_GET_F(base, MTNR1_R_IDLENESS_STATUS, MTNR1_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= MTNR1_TRY_COUNT) {
			err_mtnr("timeout waiting idle - disable fail");
			mtnr1_hw_dump(base, HW_DUMP_CR);
			ret = -ETIME;
			break;
		}

		usleep_range(3, 4);
	};

	int0_all = MTNR1_GET_R(base, MTNR1_R_INT_REQ_INT0);

	info_mtnr(
		"idle status after disable (idle:%d, int:0x%X, 0x%X)\n", idle, int0_all, int1_all);

	return ret;
}

void mtnr1_hw_s_core(struct pablo_mmio *base, u32 set_id)
{
	MTNR1_SET_R(base, MTNR1_R_YUV_RDMACL_EN, 1);
	__mtnr1_hw_s_secure_id(base, set_id);
}

static const struct is_reg mtnr1_dbg_cr[] = {
	/* The order of DBG_CR should match with the DBG_CR parser. */
	/* Chain Size */
	{ 0x0204, "YUV_MAIN_CTRL_OTF_SEG_EN" },
	{ 0x0208, "YUV_MAIN_CTRL_STILL_LAST_FRAME_EN" },
	{ 0x0210, "YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L1" },
	{ 0x0214, "YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L2" },
	{ 0x0218, "YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L3" },
	{ 0x021c, "YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L4" },
	/* CINFIFO 0 Status */
	{ 0x0e00, "YUV_CINFIFODLFEWGT_ENABLE" },
	{ 0x0e14, "YUV_CINFIFODLFEWGT_STATUS" },
	{ 0x0e18, "YUV_CINFIFODLFEWGT_INPUT_CNT" },
	{ 0x0e1c, "YUV_CINFIFODLFEWGT_STALL_CNT" },
	{ 0x0e20, "YUV_CINFIFODLFEWGT_FIFO_FULLNESS" },
	{ 0x0e40, "YUV_CINFIFODLFEWGT_INT" },
	/* COUTFIFO 0 Status */
	{ 0x0f00, "YUV_COUTFIFOMTNR0WGT_ENABLE" },
	{ 0x0f14, "YUV_COUTFIFOMTNR0WGT_STATUS" },
	{ 0x0f18, "YUV_COUTFIFOMTNR0WGT_INPUT_CNT" },
	{ 0x0f1c, "YUV_COUTFIFOMTNR0WGT_STALL_CNT" },
	{ 0x0f20, "YUV_COUTFIFOMTNR0WGT_FIFO_FULLNESS" },
	{ 0x0f40, "YUV_COUTFIFOMTNR0WGT_INT" },
	/* COUTFIFO 1 Status */
	{ 0x0f80, "YUV_COUTFIFOMSNRL1_ENABLE" },
	{ 0x0f94, "YUV_COUTFIFOMSNRL1_STATUS" },
	{ 0x0f98, "YUV_COUTFIFOMSNRL1_INPUT_CNT" },
	{ 0x0f9c, "YUV_COUTFIFOMSNRL1_STALL_CNT" },
	{ 0x0fa0, "YUV_COUTFIFOMSNRL1_FIFO_FULLNESS" },
	{ 0x0fc0, "YUV_COUTFIFOMSNRL1_INT" },
	/* COUTFIFO 2 Status */
	{ 0x1100, "YUV_COUTFIFOMSNRL2_ENABLE" },
	{ 0x1114, "YUV_COUTFIFOMSNRL2_STATUS" },
	{ 0x1118, "YUV_COUTFIFOMSNRL2_INPUT_CNT" },
	{ 0x111c, "YUV_COUTFIFOMSNRL2_STALL_CNT" },
	{ 0x1120, "YUV_COUTFIFOMSNRL2_FIFO_FULLNESS" },
	{ 0x1140, "YUV_COUTFIFOMSNRL2_INT" },
	/* COUTFIFO 3 Status */
	{ 0x1180, "YUV_COUTFIFOMSNRL3_ENABLE" },
	{ 0x1194, "YUV_COUTFIFOMSNRL3_STATUS" },
	{ 0x1198, "YUV_COUTFIFOMSNRL3_INPUT_CNT" },
	{ 0x119c, "YUV_COUTFIFOMSNRL3_STALL_CNT" },
	{ 0x11a0, "YUV_COUTFIFOMSNRL3_FIFO_FULLNESS" },
	{ 0x11c0, "YUV_COUTFIFOMSNRL3_INT" },
	/* COUTFIFO 4 Status */
	{ 0x1300, "YUV_COUTFIFOMSNRL4_ENABLE" },
	{ 0x1314, "YUV_COUTFIFOMSNRL4_STATUS" },
	{ 0x1318, "YUV_COUTFIFOMSNRL4_INPUT_CNT" },
	{ 0x131c, "YUV_COUTFIFOMSNRL4_STALL_CNT" },
	{ 0x1320, "YUV_COUTFIFOMSNRL4_FIFO_FULLNESS" },
	{ 0x1340, "YUV_COUTFIFOMSNRL4_INT" },
	/* COUTFIFO 5 Status */
	{ 0x1380, "YUV_COUTFIFODLFECUR_ENABLE" },
	{ 0x1394, "YUV_COUTFIFODLFECUR_STATUS" },
	{ 0x1398, "YUV_COUTFIFODLFECUR_INPUT_CNT" },
	{ 0x139c, "YUV_COUTFIFODLFECUR_STALL_CNT" },
	{ 0x13a0, "YUV_COUTFIFODLFECUR_FIFO_FULLNESS" },
	{ 0x13c0, "YUV_COUTFIFODLFECUR_INT" },
	/* COUTFIFO 6 Status */
	{ 0x1500, "YUV_COUTFIFOMSNRL4_ENABLE" },
	{ 0x1514, "YUV_COUTFIFOMSNRL4_STATUS" },
	{ 0x1518, "YUV_COUTFIFOMSNRL4_INPUT_CNT" },
	{ 0x151c, "YUV_COUTFIFOMSNRL4_STALL_CNT" },
	{ 0x1520, "YUV_COUTFIFOMSNRL4_FIFO_FULLNESS" },
	{ 0x1540, "YUV_COUTFIFOMSNRL4_INT" },
	/* ETC */
	{ 0x6e00, "YUV_GEOMATCHL1_EN" },
	{ 0x6e04, "YUV_GEOMATCHL1_BYPASS" },
	{ 0x6e08, "YUV_GEOMATCHL1_MATCH_ENABLE" },
	{ 0x6e0c, "YUV_GEOMATCHL1_MC_LMC_TNR_MODE" },
	{ 0x6e10, "YUV_GEOMATCHL1_TNR_WGT_EN" },
	{ 0x6e14, "YUV_GEOMATCHL1_TNR_WGT_BYPASS" },
	{ 0x6e18, "YUV_GEOMATCHL1_TNR_SFR_EN" },
	{ 0x6e1c, "YUV_GEOMATCHL1_TNR_SFR" },
	{ 0x6e20, "YUV_GEOMATCHL1_REF_IMG_SIZE" },
	{ 0x6e24, "YUV_GEOMATCHL1_REF_ROI_START" },
	{ 0x6e28, "YUV_GEOMATCHL1_ROI_SIZE" },
	{ 0x6e2c, "YUV_GEOMATCHL1_SCH_IMG_SIZE" },
	{ 0x6e30, "YUV_GEOMATCHL1_SCH_ACTIVE_START" },
	{ 0x6e34, "YUV_GEOMATCHL1_SCH_ACTIVE_SIZE" },
	{ 0x6e38, "YUV_GEOMATCHL1_SCH_ROI_START" },
	{ 0x6e3c, "YUV_GEOMATCHL1_MV_SIZE" },
	{ 0x6e40, "YUV_GEOMATCHL1_MV_BLOCK_SIZE" },
	{ 0x6e80, "YUV_GEOMATCHL2_EN" },
	{ 0x6e84, "YUV_GEOMATCHL2_BYPASS" },
	{ 0x6e88, "YUV_GEOMATCHL2_MATCH_ENABLE" },
	{ 0x6e8c, "YUV_GEOMATCHL2_MC_LMC_TNR_MODE" },
	{ 0x6e90, "YUV_GEOMATCHL2_TNR_WGT_EN" },
	{ 0x6e94, "YUV_GEOMATCHL2_TNR_WGT_BYPASS" },
	{ 0x6e98, "YUV_GEOMATCHL2_TNR_SFR_EN" },
	{ 0x6e9c, "YUV_GEOMATCHL2_TNR_SFR" },
	{ 0x6ea0, "YUV_GEOMATCHL2_REF_IMG_SIZE" },
	{ 0x6ea4, "YUV_GEOMATCHL2_REF_ROI_START" },
	{ 0x6ea8, "YUV_GEOMATCHL2_ROI_SIZE" },
	{ 0x6eac, "YUV_GEOMATCHL2_SCH_IMG_SIZE" },
	{ 0x6eb0, "YUV_GEOMATCHL2_SCH_ACTIVE_START" },
	{ 0x6eb4, "YUV_GEOMATCHL2_SCH_ACTIVE_SIZE" },
	{ 0x6eb8, "YUV_GEOMATCHL2_SCH_ROI_START" },
	{ 0x6ebc, "YUV_GEOMATCHL2_MV_SIZE" },
	{ 0x6ec0, "YUV_GEOMATCHL2_MV_BLOCK_SIZE" },
	{ 0x6f00, "YUV_GEOMATCHL3_EN" },
	{ 0x6f04, "YUV_GEOMATCHL3_BYPASS" },
	{ 0x6f08, "YUV_GEOMATCHL3_MATCH_ENABLE" },
	{ 0x6f0c, "YUV_GEOMATCHL3_MC_LMC_TNR_MODE" },
	{ 0x6f10, "YUV_GEOMATCHL3_TNR_WGT_EN" },
	{ 0x6f14, "YUV_GEOMATCHL3_TNR_WGT_BYPASS" },
	{ 0x6f18, "YUV_GEOMATCHL3_TNR_SFR_EN" },
	{ 0x6f1c, "YUV_GEOMATCHL3_TNR_SFR" },
	{ 0x6f20, "YUV_GEOMATCHL3_REF_IMG_SIZE" },
	{ 0x6f24, "YUV_GEOMATCHL3_REF_ROI_START" },
	{ 0x6f28, "YUV_GEOMATCHL3_ROI_SIZE" },
	{ 0x6f2c, "YUV_GEOMATCHL3_SCH_IMG_SIZE" },
	{ 0x6f30, "YUV_GEOMATCHL3_SCH_ACTIVE_START" },
	{ 0x6f34, "YUV_GEOMATCHL3_SCH_ACTIVE_SIZE" },
	{ 0x6f38, "YUV_GEOMATCHL3_SCH_ROI_START" },
	{ 0x6f3c, "YUV_GEOMATCHL3_MV_SIZE" },
	{ 0x6f40, "YUV_GEOMATCHL3_MV_BLOCK_SIZE" },
	{ 0x6f80, "YUV_GEOMATCHL4_EN" },
	{ 0x6f84, "YUV_GEOMATCHL4_BYPASS" },
	{ 0x6f88, "YUV_GEOMATCHL4_MATCH_ENABLE" },
	{ 0x6f8c, "YUV_GEOMATCHL4_MC_LMC_TNR_MODE" },
	{ 0x6f90, "YUV_GEOMATCHL4_TNR_WGT_EN" },
	{ 0x6f94, "YUV_GEOMATCHL4_TNR_WGT_BYPASS" },
	{ 0x6f98, "YUV_GEOMATCHL4_TNR_SFR_EN" },
	{ 0x6f9c, "YUV_GEOMATCHL4_TNR_SFR" },
	{ 0x6fa0, "YUV_GEOMATCHL4_REF_IMG_SIZE" },
	{ 0x6fa4, "YUV_GEOMATCHL4_REF_ROI_START" },
	{ 0x6fa8, "YUV_GEOMATCHL4_ROI_SIZE" },
	{ 0x6fac, "YUV_GEOMATCHL4_SCH_IMG_SIZE" },
	{ 0x6fb0, "YUV_GEOMATCHL4_SCH_ACTIVE_START" },
	{ 0x6fb4, "YUV_GEOMATCHL4_SCH_ACTIVE_SIZE" },
	{ 0x6fb8, "YUV_GEOMATCHL4_SCH_ROI_START" },
	{ 0x6fbc, "YUV_GEOMATCHL4_MV_SIZE" },
	{ 0x6fc0, "YUV_GEOMATCHL4_MV_BLOCK_SIZE" },
	{ 0x7000, "STAT_MVCONTROLLER_ENABLE" },
	{ 0x7004, "STAT_MVCONTROLLER_MV_IN_SIZE" },
	{ 0x7008, "STAT_MVCONTROLLER_MV_OUT_SIZE" },
	{ 0x700c, "STAT_MVCONTROLLER_MVF_RESIZE_EN" },
	{ 0x7600, "YUV_MIXERL1_ENABLE" },
	{ 0x7604, "YUV_MIXERL1_STILL_EN" },
	{ 0x7608, "YUV_MIXERL1_WGT_UPDATE_EN" },
	{ 0x760c, "YUV_MIXERL1_MODE" },
	{ 0x7800, "YUV_MIXERL2_ENABLE" },
	{ 0x7804, "YUV_MIXERL2_STILL_EN" },
	{ 0x7808, "YUV_MIXERL2_WGT_UPDATE_EN" },
	{ 0x780c, "YUV_MIXERL2_MODE" },
	{ 0x7a00, "YUV_MIXERL3_ENABLE" },
	{ 0x7a04, "YUV_MIXERL3_STILL_EN" },
	{ 0x7a08, "YUV_MIXERL3_WGT_UPDATE_EN" },
	{ 0x7a0c, "YUV_MIXERL3_MODE" },
	{ 0x7c00, "YUV_MIXERL4_ENABLE" },
	{ 0x7c04, "YUV_MIXERL4_STILL_EN" },
	{ 0x7c08, "YUV_MIXERL4_WGT_UPDATE_EN" },
	{ 0x7c0c, "YUV_MIXERL4_MODE" },
};

static void mtnr1_hw_dump_dbg_state(struct pablo_mmio *pmio)
{
	void *ctx;
	const struct is_reg *cr;
	u32 i, val;

	ctx = pmio->ctx ? pmio->ctx : (void *)pmio;
	pmio->reg_read(ctx, MTNR1_R_IP_VERSION, &val);

	is_dbg("[HW:%s] v%02u.%02u.%02u ======================================\n", pmio->name,
		(val >> 24) & 0xff, (val >> 16) & 0xff, val & 0xffff);
	for (i = 0; i < ARRAY_SIZE(mtnr1_dbg_cr); i++) {
		cr = &mtnr1_dbg_cr[i];

		pmio->reg_read(ctx, cr->sfr_offset, &val);
		is_dbg("[HW:%s]%40s %08x\n", pmio->name, cr->reg_name, val);
	}
	is_dbg("[HW:%s]=================================================\n", pmio->name);
}

void mtnr1_hw_dump(struct pablo_mmio *pmio, u32 mode)
{
	switch (mode) {
	case HW_DUMP_CR:
		info_mtnr1_ver("DUMP CR\n");
		is_hw_dump_regs(pmio_get_base(pmio), mtnr1_regs, MTNR1_REG_CNT);
		break;
	case HW_DUMP_DBG_STATE:
		info_mtnr1_ver("DUMP DBG_STATE\n");
		mtnr1_hw_dump_dbg_state(pmio);
		break;
	default:
		err_mtnr("%s:Not supported dump_mode %d", __FILENAME__, mode);
		break;
	}
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_dump);

void mtnr1_hw_dma_dump(struct is_common_dma *dma)
{
	CALL_DMA_OPS(dma, dma_print_info, 0);
}

static u32 __mtnr1_get_rdma_hwformat(struct is_common_dma *dma, u32 format,
			u32 sbwc_type, struct is_mtnr1_config *config)
{
	u32 new_format = format;

	switch (dma->id) {
	case MTNR1_RDMA_CUR_L2_Y:
		if (config->mixerL2_mode == 2
			&& config->mixerL2_still_en
			&& !sbwc_type)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	case MTNR1_RDMA_CUR_L3_Y:
		if (config->mixerL3_mode == 2
			&& config->mixerL3_still_en
			&& !sbwc_type)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	case MTNR1_RDMA_CUR_L4_Y:
		if (config->mixerL4_mode == 2
			&& config->mixerL4_still_en
			&& !sbwc_type)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	case MTNR1_RDMA_PREV_L2_Y:
		if (config->mixerL2_mode == 2
				&& !config->mixerL2_still_en)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	case MTNR1_RDMA_PREV_L3_Y:
		if (config->mixerL3_mode == 2
				&& !config->mixerL3_still_en)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	case MTNR1_RDMA_PREV_L4_Y:
		if (config->mixerL4_mode == 2
				&& !config->mixerL4_still_en)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	default:
		break;
	}

	dbg_mtnr(3, "%s: [%s] format(%d->%d), mixer_mode[%d %d %d %d], still_en[%d %d %d %d], sbwc(%d)",
			__func__, dma->name, format, new_format,
			config->mixerL1_mode, config->mixerL2_mode,
			config->mixerL3_mode, config->mixerL4_mode,
			config->mixerL1_still_en, config->mixerL2_still_en,
			config->mixerL3_still_en, config->mixerL4_still_en, sbwc_type);

	return new_format;
}

static u32 __mtnr1_get_wdma_hwformat(struct is_common_dma *dma, u32 format, struct is_mtnr1_config *config)
{
	u32 new_format = format;

	switch (dma->id) {
	case MTNR1_WDMA_PREV_L2_Y:
		if (config->mixerL2_mode)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	case MTNR1_WDMA_PREV_L3_Y:
		if (config->mixerL3_mode)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	case MTNR1_WDMA_PREV_L4_Y:
		if (config->mixerL4_mode)
			new_format = DMA_INOUT_FORMAT_IMG_WITH_WGT;
		break;
	default:
		break;
	}

	dbg_mtnr(3, "%s: [%s] format(%d->%d), mixer_mode[%d %d %d %d]",
			__func__, dma->name, format, new_format,
			config->mixerL1_mode, config->mixerL2_mode,
			config->mixerL3_mode, config->mixerL4_mode);

	return new_format;
}

static u32 __mtnr1_get_rdma_msb_align(u32 dma_id)
{
	u32 msb_align = 0;

	switch (dma_id) {
	/* Current */
	case MTNR1_RDMA_CUR_L1_Y:
	case MTNR1_RDMA_CUR_L1_U:
	case MTNR1_RDMA_CUR_L1_V:
	case MTNR1_RDMA_CUR_L2_U:
	case MTNR1_RDMA_CUR_L2_V:
	case MTNR1_RDMA_CUR_L3_U:
	case MTNR1_RDMA_CUR_L3_V:
	case MTNR1_RDMA_CUR_L4_U:
	case MTNR1_RDMA_CUR_L4_V:
	/* Previous */
	case MTNR1_RDMA_PREV_L1_Y:
	case MTNR1_RDMA_PREV_L1_U:
	case MTNR1_RDMA_PREV_L1_V:
	case MTNR1_RDMA_PREV_L2_U:
	case MTNR1_RDMA_PREV_L2_V:
	case MTNR1_RDMA_PREV_L3_U:
	case MTNR1_RDMA_PREV_L3_V:
	case MTNR1_RDMA_PREV_L4_U:
	case MTNR1_RDMA_PREV_L4_V:
		msb_align = 1;
		break;

	default:
		break;
	}

	return msb_align;
}

int mtnr1_hw_s_rdma_init(struct is_common_dma *dma, struct param_dma_input *dma_input,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_mtnr1_config *config)
{
	int ret = 0;
	u32 comp_sbwc_en = 0, comp_64b_align = 0;
	u32 quality_control = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_frame_width, full_dma_width;
	u32 format;
	u32 strip_offset_in_pixel = 0, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 strip_index = stripe_input->index;
	u32 strip_start_pos = (strip_index) ? (stripe_input->start_pos_x) : 0;
	u32 mv_width, mv_height;
	u32 bus_info = 0;
	u32 en_32b_pa = 0;
	bool is_image = true;
	u32 full_frame_width_layer[5] = { 0, };
	u32 level = 0;

	ret = CALL_DMA_OPS(dma, dma_enable, dma_input->cmd);

	if (dma_input->cmd == 0)
		return 0;

	full_frame_width = (strip_enable) ? stripe_input->full_width : frame_width;
	full_frame_width_layer[1] = (strip_enable) ? ALIGN(full_frame_width / 2, 2) : frame_width;
	full_frame_width_layer[2] = (strip_enable) ? ALIGN(full_frame_width_layer[1] / 2, 2) : frame_width;
	full_frame_width_layer[3] = (strip_enable) ? ALIGN(full_frame_width_layer[2] / 2, 2) : frame_width;
	full_frame_width_layer[4] = (strip_enable) ? ALIGN(full_frame_width_layer[3] / 2, 2) : frame_width;

	hwformat = dma_input->format;
	sbwc_type = dma_input->sbwc_type;
	memory_bitwidth = dma_input->bitwidth;
	pixelsize = dma_input->msb + 1;

	switch (dma->id) {
	case MTNR1_RDMA_CUR_L1_Y:
	case MTNR1_RDMA_CUR_L2_Y:
	case MTNR1_RDMA_CUR_L3_Y:
	case MTNR1_RDMA_CUR_L4_Y:
		strip_offset_in_pixel = dma_input->dma_crop_offset;
		width = dma_input->width;

		hwformat = __mtnr1_get_rdma_hwformat(dma, DMA_INOUT_FORMAT_Y,
					sbwc_type, config);

		height = frame_height;
		level = (dma->id - MTNR1_RDMA_CUR_L1_Y) / 3 + 1;
		full_dma_width = full_frame_width_layer[level];

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;

		break;
	case MTNR1_RDMA_CUR_L1_U:
	case MTNR1_RDMA_CUR_L2_U:
	case MTNR1_RDMA_CUR_L3_U:
	case MTNR1_RDMA_CUR_L4_U:
		strip_offset_in_pixel = dma_input->dma_crop_offset;
		width = dma_input->width;
		height = frame_height;
		level = (dma->id - MTNR1_RDMA_CUR_L1_U) / 3 + 1;
		full_dma_width = full_frame_width_layer[level];

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;

		break;
	case MTNR1_RDMA_CUR_L1_V:
	case MTNR1_RDMA_CUR_L2_V:
	case MTNR1_RDMA_CUR_L3_V:
	case MTNR1_RDMA_CUR_L4_V:
		strip_offset_in_pixel = dma_input->dma_crop_offset;
		width = dma_input->width;
		height = frame_height;
		level = (dma->id - MTNR1_RDMA_CUR_L1_V) / 3 + 1;
		full_dma_width = full_frame_width_layer[level];

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;

		break;
	case MTNR1_RDMA_PREV_L1_Y:
	case MTNR1_RDMA_PREV_L2_Y:
	case MTNR1_RDMA_PREV_L3_Y:
	case MTNR1_RDMA_PREV_L4_Y:
		width = full_frame_width;
		height = frame_height;

		hwformat = __mtnr1_get_rdma_hwformat(dma, DMA_INOUT_FORMAT_Y,
					sbwc_type, config);

		memory_bitwidth = config->imgL1_bit;
		pixelsize = config->imgL1_bit;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type)
			en_32b_pa = 1;

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSY_64B :
					DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;

		level = (dma->id - MTNR1_RDMA_PREV_L1_Y) / 3 + 1;
		full_dma_width = full_frame_width_layer[level];
		break;
	case MTNR1_RDMA_PREV_L1_U:
	case MTNR1_RDMA_PREV_L2_U:
	case MTNR1_RDMA_PREV_L3_U:
	case MTNR1_RDMA_PREV_L4_U:
		width = full_frame_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_YUV422;
		memory_bitwidth = config->imgL1_bit;
		pixelsize = config->imgL1_bit;
		bus_info = IS_LLC_CACHE_HINT_CACHE_ALLOC_TYPE << IS_LLC_CACHE_HINT_SHIFT;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type)
			en_32b_pa = 1;

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSYLESS_64B :
					DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;

		level = (dma->id - MTNR1_RDMA_PREV_L1_U) / 3 + 1;
		full_dma_width = full_frame_width_layer[level];
		break;

	case MTNR1_RDMA_PREV_L1_V:
	case MTNR1_RDMA_PREV_L2_V:
	case MTNR1_RDMA_PREV_L3_V:
	case MTNR1_RDMA_PREV_L4_V:
		width = full_frame_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_YUV422;
		memory_bitwidth = config->imgL1_bit;
		pixelsize = config->imgL1_bit;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type)
			en_32b_pa = 1;

		/* Y lossy, UV lossless when sbwc_type is LOSSY_CUSTOM */
		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSYLESS_64B :
					DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;

		level = (dma->id - MTNR1_RDMA_PREV_L1_V) / 3 + 1;
		full_dma_width = full_frame_width_layer[level];
		break;
	case MTNR1_RDMA_PREV_L1_WGT:
		width = ((full_frame_width / 2 + 15) / 16) * 16 * 2;
		height = (frame_height / 2 + 1) / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		en_32b_pa = 0;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		full_dma_width = width;
		break;
	case MTNR1_RDMA_MV_GEOMATCH:
		mv_width = config->mvc_in_w * 4;
		mv_height = config->mvc_in_h;
		width = mv_width;
		height = mv_height;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		strip_offset_in_byte = (strip_start_pos / mv_width) * mv_width / 8;
		full_dma_width = mv_width;
		is_image = false;
		break;
	case MTNR1_RDMA_SEG_L1:
	case MTNR1_RDMA_SEG_L2:
	case MTNR1_RDMA_SEG_L3:
	case MTNR1_RDMA_SEG_L4:
		width = frame_width;
		height = frame_height;
		full_dma_width = full_frame_width;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if (hwformat == DMA_INOUT_FORMAT_IMG_WITH_WGT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (en_32b_pa)
		comp_64b_align = 1;

	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 2)
		quality_control = LOSSY_COMP_FOOTPRINT;

	if (comp_sbwc_en == 0) {
		stride_1p = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, full_dma_width, 16, is_image);
		if (strip_enable && strip_offset_in_pixel)
			strip_offset_in_byte = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
				hwformat, strip_offset_in_pixel, 16, true);
	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_dma_width,
			comp_64b_align, quality_control, MTNR1_COMP_BLOCK_WIDTH,
			MTNR1_COMP_BLOCK_HEIGHT);
		header_stride_1p =
			is_hw_dma_get_header_stride(full_dma_width, MTNR1_COMP_BLOCK_WIDTH, 16);
		if (strip_enable && strip_offset_in_pixel) {
			strip_offset_in_byte = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
				strip_offset_in_pixel, comp_64b_align, quality_control,
				MTNR1_COMP_BLOCK_WIDTH, MTNR1_COMP_BLOCK_HEIGHT);
			strip_header_offset_in_byte = is_hw_dma_get_header_stride(
				strip_offset_in_pixel, MTNR1_COMP_BLOCK_WIDTH, 0);
		}
	} else {
		return -EINVAL;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);
	ret |= CALL_DMA_OPS(dma, dma_set_cache_32b_pa, en_32b_pa);
	ret |= CALL_DMA_OPS(dma, dma_set_msb_align, 0, __mtnr1_get_rdma_msb_align(dma->id));

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + MTNR1_COMP_BLOCK_HEIGHT - 1) / MTNR1_COMP_BLOCK_HEIGHT) *
				stride_1p;
		break;
	default:
		break;
	}

	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	dbg_mtnr(3,
		"%s : dma_id %d, sbwc(%d), 64b_align(%d), quality(%d), st_ofs %d, width %d, height %d, img_ofs %d, header_ofs %d, stride (%d, %d)\n",
		dma->name, dma->id,
		comp_sbwc_en, comp_64b_align, quality_control, strip_start_pos, width, height, *strip_offset,
		*header_offset, stride_1p, header_stride_1p);
	dbg_mtnr(3,
		"%s : dma_id %d, payload_size(%d), format(%d/%d), bit(%d) pix(%d)\n",
		dma->name, dma->id, *payload_size, hwformat, format, memory_bitwidth, pixelsize);

	return ret;
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_s_rdma_init);

int mtnr1_hw_s_wdma_init(struct is_common_dma *dma, struct param_dma_output *dma_output,
	struct param_stripe_input *stripe_input, u32 frame_width, u32 frame_height, u32 *sbwc_en,
	u32 *payload_size, u32 *strip_offset, u32 *header_offset, struct is_mtnr1_config *config)
{
	int ret = 0;
	u32 comp_sbwc_en = 0, comp_64b_align = 0;
	u32 quality_control = 0;
	u32 stride_1p = 0, header_stride_1p = 0;
	u32 hwformat, memory_bitwidth, pixelsize, sbwc_type;
	u32 width, height;
	u32 full_frame_width, full_dma_width = 0;
	u32 format;
	u32 strip_offset_in_pixel, strip_offset_in_byte = 0, strip_header_offset_in_byte = 0;
	u32 strip_enable = (stripe_input->total_count < 2) ? 0 : 1;
	u32 bus_info = 0;
	u32 en_32b_pa = 0;
	u32 full_frame_width_layer[5], strip_offset_layer[5];
	u32 level = 0;

	ret = CALL_DMA_OPS(dma, dma_enable, dma_output->cmd);
	if (dma_output->cmd == 0) {
		ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, DMA_OUTPUT_SBWC_DISABLE);
		return ret;
	}

	full_frame_width = (strip_enable) ? stripe_input->full_width : frame_width;
	hwformat = dma_output->format;
	sbwc_type = dma_output->sbwc_type;
	memory_bitwidth = dma_output->bitwidth;
	pixelsize = dma_output->msb + 1;

	full_frame_width_layer[1] = (strip_enable) ? ALIGN(full_frame_width / 2, 2) : full_frame_width;
	full_frame_width_layer[2] = (strip_enable) ? ALIGN(full_frame_width_layer[1] / 2, 2) : full_frame_width;
	full_frame_width_layer[3] = (strip_enable) ? ALIGN(full_frame_width_layer[2] / 2, 2) : full_frame_width;
	full_frame_width_layer[4] = (strip_enable) ? ALIGN(full_frame_width_layer[3] / 2, 2) : full_frame_width;

	strip_offset_layer[1] = (strip_enable) ? ALIGN(dma_output->dma_crop_offset_x / 2, 2) : 0;
	strip_offset_layer[2] = (strip_enable) ? ALIGN(strip_offset_layer[1] / 2, 2) : 0;
	strip_offset_layer[3] = (strip_enable) ? ALIGN(strip_offset_layer[2] / 2, 2) : 0;
	strip_offset_layer[4] = (strip_enable) ? ALIGN(strip_offset_layer[3] / 2, 2) : 0;

	switch (dma->id) {
	case MTNR1_WDMA_PREV_L1_Y:
	case MTNR1_WDMA_PREV_L2_Y:
	case MTNR1_WDMA_PREV_L3_Y:
	case MTNR1_WDMA_PREV_L4_Y:
		level = (dma->id - MTNR1_WDMA_PREV_L1_Y) / 3 + 1;
		strip_offset_in_pixel = strip_offset_layer[level];
		width = dma_output->dma_crop_width;
		height = frame_height;

		hwformat = __mtnr1_get_wdma_hwformat(dma, DMA_INOUT_FORMAT_Y, config);

		memory_bitwidth = config->imgL1_bit;
		pixelsize = config->imgL1_bit;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type) {
			bus_info |= 1 << IS_32B_WRITE_ALLOC_SHIFT;
			en_32b_pa = 1;
		}

		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSY_64B :
					DMA_INPUT_SBWC_LOSSY_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;

		full_dma_width = full_frame_width_layer[level];
		break;
	case MTNR1_WDMA_PREV_L1_U:
	case MTNR1_WDMA_PREV_L2_U:
	case MTNR1_WDMA_PREV_L3_U:
	case MTNR1_WDMA_PREV_L4_U:
		level = (dma->id - MTNR1_WDMA_PREV_L1_U) / 3 + 1;
		strip_offset_in_pixel = strip_offset_layer[level];
		width = dma_output->dma_crop_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_YUV422;
		memory_bitwidth = config->imgL1_bit;
		pixelsize = config->imgL1_bit;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type) {
			bus_info |= 1 << IS_32B_WRITE_ALLOC_SHIFT;
			en_32b_pa = 1;
		}

		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSYLESS_64B :
					DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;

		full_dma_width = full_frame_width_layer[level];
		break;
	case MTNR1_WDMA_PREV_L1_V:
	case MTNR1_WDMA_PREV_L2_V:
	case MTNR1_WDMA_PREV_L3_V:
	case MTNR1_WDMA_PREV_L4_V:
		level = (dma->id - MTNR1_WDMA_PREV_L1_V) / 3 + 1;
		strip_offset_in_pixel = strip_offset_layer[level];
		width = dma_output->dma_crop_width;
		height = frame_height;
		hwformat = DMA_INOUT_FORMAT_YUV422;
		memory_bitwidth = config->imgL1_bit;
		pixelsize = config->imgL1_bit;
		bus_info = IS_LLC_CACHE_HINT_CACHE_NOALLOC_TYPE << IS_LLC_CACHE_HINT_SHIFT;

		if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_type) {
			bus_info |= 1 << IS_32B_WRITE_ALLOC_SHIFT;
			en_32b_pa = 1;
		}

		if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
			sbwc_type = en_32b_pa ?
					DMA_INPUT_SBWC_LOSSYLESS_64B :
					DMA_INPUT_SBWC_LOSSYLESS_32B;
		else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
			sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;

		full_dma_width = full_frame_width_layer[level];
		break;
	case MTNR1_WDMA_PREV_L1_WGT:
		width = ((dma_output->dma_crop_width / 2 + 15) / 16) * 16 * 2;
		full_dma_width = (((full_frame_width / 2) / 2 + 15) / 16) * 16 * 2;
		strip_offset_in_pixel = dma_output->dma_crop_offset_x >> 1;
		height = (frame_height / 2 + 1) / 2;
		hwformat = DMA_INOUT_FORMAT_Y;
		sbwc_type = DMA_INPUT_SBWC_DISABLE;
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		pixelsize = DMA_INOUT_BIT_WIDTH_8BIT;
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if (hwformat == DMA_INOUT_FORMAT_IMG_WITH_WGT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;

	*sbwc_en = comp_sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	if (en_32b_pa)
		comp_64b_align = 1;

	if (!is_hw_dma_get_bayer_format(
		    memory_bitwidth, pixelsize, hwformat, comp_sbwc_en, true, &format))
		ret |= CALL_DMA_OPS(dma, dma_set_format, format, DMA_FMT_BAYER);
	else
		ret |= DMA_OPS_ERROR;

	if (comp_sbwc_en == 2)
		quality_control = LOSSY_COMP_FOOTPRINT;

	if (comp_sbwc_en == 0) {
		stride_1p = is_hw_dma_get_img_stride(
			memory_bitwidth, pixelsize, hwformat, full_dma_width, 16, true);
		if (strip_enable)
			strip_offset_in_byte = is_hw_dma_get_img_stride(memory_bitwidth, pixelsize,
				hwformat, strip_offset_in_pixel, 16, true);

	} else if (comp_sbwc_en == 1 || comp_sbwc_en == 2) {
		stride_1p = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize, full_dma_width,
			comp_64b_align, quality_control, MTNR1_COMP_BLOCK_WIDTH,
			MTNR1_COMP_BLOCK_HEIGHT);
		header_stride_1p =
			is_hw_dma_get_header_stride(full_dma_width, MTNR1_COMP_BLOCK_WIDTH, 16);

		if (strip_enable) {
			strip_offset_in_byte = is_hw_dma_get_payload_stride(comp_sbwc_en, pixelsize,
				strip_offset_in_pixel, comp_64b_align, quality_control,
				MTNR1_COMP_BLOCK_WIDTH, MTNR1_COMP_BLOCK_HEIGHT);
			strip_header_offset_in_byte = is_hw_dma_get_header_stride(
				strip_offset_in_pixel, MTNR1_COMP_BLOCK_WIDTH, 0);
		}
	} else {
		return -EINVAL;
	}

	ret |= CALL_DMA_OPS(dma, dma_set_comp_sbwc_en, comp_sbwc_en);
	ret |= CALL_DMA_OPS(dma, dma_set_comp_quality, quality_control);
	ret |= CALL_DMA_OPS(dma, dma_set_size, width, height);
	ret |= CALL_DMA_OPS(dma, dma_set_img_stride, stride_1p, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_votf_enable, 0, 0);
	ret |= CALL_DMA_OPS(dma, dma_set_bus_info, bus_info);
	ret |= CALL_DMA_OPS(dma, dma_set_cache_32b_pa, en_32b_pa);

	*payload_size = 0;
	switch (comp_sbwc_en) {
	case 1:
	case 2:
		ret |= CALL_DMA_OPS(dma, dma_set_comp_64b_align, comp_64b_align);
		ret |= CALL_DMA_OPS(dma, dma_set_header_stride, header_stride_1p, 0);
		*payload_size = ((height + MTNR1_COMP_BLOCK_HEIGHT - 1) / MTNR1_COMP_BLOCK_HEIGHT) *
				stride_1p;
		break;
	default:
		break;
	}

	*strip_offset = strip_offset_in_byte;
	*header_offset = strip_header_offset_in_byte;

	dbg_mtnr(3, "%s : dma_id %d, width %d, height %d, format(%d/%d), bit(%d)\
		 strip_offset_in_pixel %d, \
		strip_offset_in_byte %d, strip_header_offset_in_byte %d, payload_size %d\n",
		dma->name, dma->id, width, height, hwformat, format, memory_bitwidth,
		strip_offset_in_pixel, strip_offset_in_byte,
		strip_header_offset_in_byte, *payload_size);
	dbg_mtnr(3, "%s: comp_sbwc_en %d, pixelsize %d, comp_64b_align %d, quality_control %d, \
		stride_1p %d, header_stride_1p %d\n",
		dma->name,
		comp_sbwc_en, pixelsize, comp_64b_align, quality_control, stride_1p,
		header_stride_1p);

	return ret;
}

static int mtnr1_hw_rdma_create_pmio(struct is_common_dma *dma, void *base, u32 input_id)
{
	int ret = 0;
	ulong available_bayer_format_map;
	const char *name;

	switch (input_id) {
	case MTNR1_RDMA_CUR_L1_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL1Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL1Y_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L1_Y];
		break;
	case MTNR1_RDMA_CUR_L1_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL1U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL1U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L1_U];
		break;
	case MTNR1_RDMA_CUR_L1_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL1V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL1V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L1_V];
		break;
	case MTNR1_RDMA_CUR_L2_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL2Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL2Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L2_Y];
		break;
	case MTNR1_RDMA_CUR_L2_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL2U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL2U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L2_U];
		break;
	case MTNR1_RDMA_CUR_L2_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL2V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL2V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L2_V];
		break;
	case MTNR1_RDMA_CUR_L3_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL3Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL3Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L3_Y];
		break;
	case MTNR1_RDMA_CUR_L3_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL3U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL3U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L3_U];
		break;
	case MTNR1_RDMA_CUR_L3_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL3V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL3V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L3_V];
		break;
	case MTNR1_RDMA_CUR_L4_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL4Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL4Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L4_Y];
		break;
	case MTNR1_RDMA_CUR_L4_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL4U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL4U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L4_U];
		break;
	case MTNR1_RDMA_CUR_L4_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMACURRINL4V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMACURRINL4V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_CUR_L4_V];
		break;

	case MTNR1_RDMA_PREV_L1_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL1Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL1Y_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L1_Y];
		break;
	case MTNR1_RDMA_PREV_L1_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL1U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL1U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L1_U];
		break;
	case MTNR1_RDMA_PREV_L1_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL1V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL1V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L1_V];
		break;
	case MTNR1_RDMA_PREV_L2_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL2Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL2Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L2_Y];
		break;
	case MTNR1_RDMA_PREV_L2_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL2U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL2U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L2_U];
		break;
	case MTNR1_RDMA_PREV_L2_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL2V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL2V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L2_V];
		break;
	case MTNR1_RDMA_PREV_L3_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL3Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL3Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L3_Y];
		break;
	case MTNR1_RDMA_PREV_L3_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL3U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL3U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L3_U];
		break;
	case MTNR1_RDMA_PREV_L3_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL3V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL3V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L3_V];
		break;
	case MTNR1_RDMA_PREV_L4_Y:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL4Y_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL4Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L4_Y];
		break;
	case MTNR1_RDMA_PREV_L4_U:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL4U_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL4U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L4_U];
		break;
	case MTNR1_RDMA_PREV_L4_V:
		dma->reg_ofs = MTNR1_R_YUV_RDMAPREVINL4V_EN;
		dma->field_ofs = MTNR1_F_YUV_RDMAPREVINL4V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L4_V];
		break;

	case MTNR1_RDMA_PREV_L1_WGT:
		dma->reg_ofs = MTNR1_R_STAT_RDMAPREVWGTINL1_EN;
		dma->field_ofs = MTNR1_F_STAT_RDMAPREVWGTINL1_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_PREV_L1_WGT];
		break;
	case MTNR1_RDMA_SEG_L1:
		dma->reg_ofs = MTNR1_R_STAT_RDMASEGL1_EN;
		dma->field_ofs = MTNR1_F_STAT_RDMASEGL1_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_SEG_L1];
		break;
	case MTNR1_RDMA_SEG_L2:
		dma->reg_ofs = MTNR1_R_STAT_RDMASEGL2_EN;
		dma->field_ofs = MTNR1_F_STAT_RDMASEGL2_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_SEG_L2];
		break;
	case MTNR1_RDMA_SEG_L3:
		dma->reg_ofs = MTNR1_R_STAT_RDMASEGL3_EN;
		dma->field_ofs = MTNR1_F_STAT_RDMASEGL3_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_SEG_L3];
		break;
	case MTNR1_RDMA_SEG_L4:
		dma->reg_ofs = MTNR1_R_STAT_RDMASEGL4_EN;
		dma->field_ofs = MTNR1_F_STAT_RDMASEGL4_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_SEG_L4];
		break;
	case MTNR1_RDMA_MV_GEOMATCH:
		dma->reg_ofs = MTNR1_R_STAT_RDMAMVGEOMATCH_EN;
		dma->field_ofs = MTNR1_F_STAT_RDMAMVGEOMATCH_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_rdma_name[MTNR1_RDMA_MV_GEOMATCH];
		break;
	default:
		err_mtnr("invalid input_id[%d]", input_id);
		return -EINVAL;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

	return ret;
}

int mtnr1_hw_rdma_create(struct is_common_dma *dma, void *base, u32 dma_id)
{
	return mtnr1_hw_rdma_create_pmio(dma, base, dma_id);
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_rdma_create);

static int mtnr1_hw_wdma_create_pmio(struct is_common_dma *dma, void *base, u32 input_id)
{
	int ret = 0;
	ulong available_bayer_format_map;
	const char *name;

	switch (input_id) {
	case MTNR1_WDMA_PREV_L1_Y:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL1Y_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL1Y_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L1_Y];
		break;
	case MTNR1_WDMA_PREV_L1_U:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL1U_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL1U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L1_U];
		break;
	case MTNR1_WDMA_PREV_L1_V:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL1V_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL1V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L1_V];
		break;
	case MTNR1_WDMA_PREV_L2_Y:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL2Y_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL2Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L2_Y];
		break;
	case MTNR1_WDMA_PREV_L2_U:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL2U_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL2U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L2_U];
		break;
	case MTNR1_WDMA_PREV_L2_V:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL2V_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL2V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L2_V];
		break;
	case MTNR1_WDMA_PREV_L3_Y:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL3Y_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL3Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L3_Y];
		break;
	case MTNR1_WDMA_PREV_L3_U:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL3U_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL3U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L3_U];
		break;
	case MTNR1_WDMA_PREV_L3_V:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL3V_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL3V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L3_V];
		break;
	case MTNR1_WDMA_PREV_L4_Y:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL4Y_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL4Y_EN;
		available_bayer_format_map = 0x8777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L4_Y];
		break;
	case MTNR1_WDMA_PREV_L4_U:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL4U_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL4U_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L4_U];
		break;
	case MTNR1_WDMA_PREV_L4_V:
		dma->reg_ofs = MTNR1_R_YUV_WDMAPREVOUTL4V_EN;
		dma->field_ofs = MTNR1_F_YUV_WDMAPREVOUTL4V_EN;
		available_bayer_format_map = 0x777 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L4_V];
		break;

	case MTNR1_WDMA_PREV_L1_WGT:
		dma->reg_ofs = MTNR1_R_STAT_WDMAPREVWGTOUTL1_EN;
		dma->field_ofs = MTNR1_F_STAT_WDMAPREVWGTOUTL1_EN;
		available_bayer_format_map = 0x1 & IS_BAYER_FORMAT_MASK;
		name = is_hw_mtnr1_wdma_name[MTNR1_WDMA_PREV_L1_WGT];
		break;
	default:
		err_mtnr("invalid input_id[%d]", input_id);
		return -EINVAL;
	}

	ret = pmio_dma_set_ops(dma);
	ret |= pmio_dma_create(dma, base, input_id, name, available_bayer_format_map, 0, 0);

	return ret;
}

int mtnr1_hw_wdma_create(struct is_common_dma *dma, void *base, u32 dma_id)
{
	return mtnr1_hw_wdma_create_pmio(dma, base, dma_id);
}

void mtnr1_hw_s_dma_corex_id(struct is_common_dma *dma, u32 set_id)
{
	CALL_DMA_OPS(dma, dma_set_corex_id, set_id);
}

int mtnr1_hw_s_rdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset)
{
	int ret = 0, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MTNR1_RDMA_CUR_L1_Y:
	case MTNR1_RDMA_CUR_L2_Y:
	case MTNR1_RDMA_CUR_L3_Y:
	case MTNR1_RDMA_CUR_L4_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_RDMA_CUR_L1_U:
	case MTNR1_RDMA_CUR_L2_U:
	case MTNR1_RDMA_CUR_L3_U:
	case MTNR1_RDMA_CUR_L4_U:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 1] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_RDMA_CUR_L1_V:
	case MTNR1_RDMA_CUR_L2_V:
	case MTNR1_RDMA_CUR_L3_V:
	case MTNR1_RDMA_CUR_L4_V:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 2] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_RDMA_PREV_L1_Y:
	case MTNR1_RDMA_PREV_L2_Y:
	case MTNR1_RDMA_PREV_L3_Y:
	case MTNR1_RDMA_PREV_L4_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i];
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_RDMA_PREV_L1_U:
	case MTNR1_RDMA_PREV_L2_U:
	case MTNR1_RDMA_PREV_L3_U:
	case MTNR1_RDMA_PREV_L4_U:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 1];
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_RDMA_PREV_L1_V:
	case MTNR1_RDMA_PREV_L2_V:
	case MTNR1_RDMA_PREV_L3_V:
	case MTNR1_RDMA_PREV_L4_V:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 2];
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_RDMA_PREV_L1_WGT:
	case MTNR1_RDMA_MV_GEOMATCH:
	case MTNR1_RDMA_SEG_L1:
	case MTNR1_RDMA_SEG_L2:
	case MTNR1_RDMA_SEG_L3:
	case MTNR1_RDMA_SEG_L4:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if ((comp_sbwc_en == 1) || (comp_sbwc_en == 2)) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case MTNR1_RDMA_CUR_L1_Y:
		case MTNR1_RDMA_CUR_L2_Y:
		case MTNR1_RDMA_PREV_L1_Y:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[3 * i] + payload_size +
					      header_offset;
			break;
		case MTNR1_RDMA_CUR_L1_U:
		case MTNR1_RDMA_CUR_L2_U:
		case MTNR1_RDMA_PREV_L1_U:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[3 * i + 1] + payload_size +
					      header_offset;
			break;
		case MTNR1_RDMA_CUR_L1_V:
		case MTNR1_RDMA_CUR_L2_V:
		case MTNR1_RDMA_PREV_L1_V:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[3 * i + 2] + payload_size +
					      header_offset;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, plane, buf_idx, num_buffers);
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_s_rdma_addr);

int mtnr1_hw_s_wdma_addr(struct is_common_dma *dma, pdma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size, u32 strip_offset, u32 header_offset)
{
	int ret = 0, i;
	dma_addr_t address[IS_MAX_FRO];
	dma_addr_t hdr_addr[IS_MAX_FRO];

	switch (dma->id) {
	case MTNR1_WDMA_PREV_L1_Y:
	case MTNR1_WDMA_PREV_L2_Y:
	case MTNR1_WDMA_PREV_L3_Y:
	case MTNR1_WDMA_PREV_L4_Y:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_WDMA_PREV_L1_U:
	case MTNR1_WDMA_PREV_L2_U:
	case MTNR1_WDMA_PREV_L3_U:
	case MTNR1_WDMA_PREV_L4_U:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 1] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_WDMA_PREV_L1_V:
	case MTNR1_WDMA_PREV_L2_V:
	case MTNR1_WDMA_PREV_L3_V:
	case MTNR1_WDMA_PREV_L4_V:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[3 * i + 2] + strip_offset;
		ret = CALL_DMA_OPS(dma, dma_set_img_addr, address, plane, buf_idx, num_buffers);
		break;
	case MTNR1_WDMA_PREV_L1_WGT:
		for (i = 0; i < num_buffers; i++)
			address[i] = (dma_addr_t)addr[i] + strip_offset;
		ret = CALL_DMA_OPS(
			dma, dma_set_img_addr, (dma_addr_t *)address, plane, buf_idx, num_buffers);
		break;
	default:
		err_mtnr("invalid dma_id[%d]", dma->id);
		return -EINVAL;
	}

	if ((comp_sbwc_en == 1) || (comp_sbwc_en == 2)) {
		/* Lossless, Lossy need to set header base address */
		switch (dma->id) {
		case MTNR1_WDMA_PREV_L1_Y:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[3 * i] + payload_size +
					      header_offset;
			break;
		case MTNR1_WDMA_PREV_L1_U:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[3 * i + 1] + payload_size +
					      header_offset;
			break;
		case MTNR1_WDMA_PREV_L1_V:
			for (i = 0; i < num_buffers; i++)
				hdr_addr[i] = (dma_addr_t)addr[3 * i + 2] + payload_size +
					      header_offset;
			break;
		default:
			break;
		}

		ret = CALL_DMA_OPS(dma, dma_set_header_addr, hdr_addr, plane, buf_idx, num_buffers);
	}

	return ret;
}

void mtnr1_hw_g_int_en(u32 *int_en)
{
	int_en[PCC_INT_0] = MTNR1_INT0_EN_MASK;
	int_en[PCC_INT_1] = MTNR1_INT1_EN_MASK;
	/* Not used */
	int_en[PCC_CMDQ_INT] = 0;
	int_en[PCC_COREX_INT] = 0;
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_g_int_en);

u32 mtnr1_hw_g_int_grp_en(void)
{
	return MTNR1_INT_GRP_EN_MASK;
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_g_int_grp_en);

void mtnr1_hw_s_block_bypass(struct pablo_mmio *base, u32 set_id)
{
	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_OTF_SEG_EN, MTNR1_F_YUV_OTF_SEG_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_EN, MTNR1_F_YUV_GEOMATCHL1_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_BYPASS, MTNR1_F_YUV_GEOMATCHL1_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_EN, MTNR1_F_YUV_GEOMATCHL2_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_BYPASS, MTNR1_F_YUV_GEOMATCHL2_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_EN, MTNR1_F_YUV_GEOMATCHL3_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_BYPASS, MTNR1_F_YUV_GEOMATCHL3_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_EN, MTNR1_F_YUV_GEOMATCHL4_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_BYPASS, MTNR1_F_YUV_GEOMATCHL4_BYPASS, 1);

	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL1_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL1_MATCH_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_TNR_WGT_EN, MTNR1_F_YUV_GEOMATCHL1_TNR_WGT_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_TNR_WGT_BYPASS,
		MTNR1_F_YUV_GEOMATCHL1_TNR_WGT_BYPASS, 1);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL2_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL2_MATCH_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_TNR_WGT_EN, MTNR1_F_YUV_GEOMATCHL2_TNR_WGT_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_TNR_WGT_BYPASS,
		MTNR1_F_YUV_GEOMATCHL2_TNR_WGT_BYPASS, 1);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL3_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL3_MATCH_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_TNR_WGT_EN, MTNR1_F_YUV_GEOMATCHL3_TNR_WGT_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_TNR_WGT_BYPASS,
		MTNR1_F_YUV_GEOMATCHL3_TNR_WGT_BYPASS, 1);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL4_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL4_MATCH_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_TNR_WGT_EN, MTNR1_F_YUV_GEOMATCHL4_TNR_WGT_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_TNR_WGT_BYPASS,
		MTNR1_F_YUV_GEOMATCHL4_TNR_WGT_BYPASS, 1);

	MTNR1_SET_F(base, MTNR1_R_STAT_MVCONTROLLER_ENABLE, MTNR1_F_STAT_MVCONTROLLER_ENABLE, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_ENABLE, MTNR1_F_YUV_MIXERL1_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_ENABLE, MTNR1_F_YUV_MIXERL1_MERGE_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_STILL_EN, MTNR1_F_YUV_MIXERL1_STILL_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_ENABLE, MTNR1_F_YUV_MIXERL2_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_ENABLE, MTNR1_F_YUV_MIXERL2_MERGE_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_STILL_EN, MTNR1_F_YUV_MIXERL2_STILL_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_ENABLE, MTNR1_F_YUV_MIXERL3_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_ENABLE, MTNR1_F_YUV_MIXERL3_MERGE_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_STILL_EN, MTNR1_F_YUV_MIXERL3_STILL_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_ENABLE, MTNR1_F_YUV_MIXERL4_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_ENABLE, MTNR1_F_YUV_MIXERL4_MERGE_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_STILL_EN, MTNR1_F_YUV_MIXERL4_STILL_EN, 0);

	MTNR1_SET_F(base, MTNR1_R_STAT_SEGMAPPING_BYPASS, MTNR1_F_STAT_SEGMAPPING_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL1_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL1_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL2_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL2_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL3_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL3_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL4_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL4_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL1_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL1_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL2_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL2_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL3_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL3_BYPASS, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL4_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL4_BYPASS, 1);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPWEIGHTDMAL1_BYPASS, MTNR1_F_YUV_CROPWEIGHTDMAL1_BYPASS, 1);
}

void mtnr1_hw_s_otf_input_dlfe_wgt(struct pablo_mmio *base, u32 set_id, u32 enable,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	dbg_mtnr(4, "%s enable(%d)\n", __func__, enable);

	if (enable)
		frame_cfg->cotf_in_en |= BIT_MASK(MTNR1_COTF_IN_DLFE_WGT);

	MTNR1_SET_R(base, MTNR1_R_STAT_CINFIFODLFEWGT_ENABLE, enable);
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
		MTNR1_F_STAT_CINFIFODLFEWGT_STALL_BEFORE_FRAME_START_EN, enable);
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
		MTNR1_F_STAT_CINFIFODLFEWGT_AUTO_RECOVERY_EN, 0);
	MTNR1_SET_F(
		base, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG, MTNR1_F_STAT_CINFIFODLFEWGT_DEBUG_EN, 1);
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_INTERVALS,
		MTNR1_F_STAT_CINFIFODLFEWGT_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_INTERVALS,
		MTNR1_F_STAT_CINFIFODLFEWGT_INTERVAL_PIXEL, PBLANK_CYCLE);
}

void mtnr1_hw_s_otf_output_mtnr0_wgt(struct pablo_mmio *base, u32 set_id, u32 enable,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)

{
	dbg_mtnr(4, "%s enable(%d)\n", __func__, enable);

	if (enable)
		frame_cfg->cotf_out_en |= BIT_MASK(MTNR1_COTF_OUT_MTNR0_WGT);

	/* MTNR0 WGT */
	MTNR1_SET_R(base, MTNR1_R_STAT_COUTFIFOMTNR0WGT_ENABLE, enable);
	MTNR1_SET_F(base, MTNR1_R_STAT_COUTFIFOMTNR0WGT_CONFIG,
		MTNR1_F_STAT_COUTFIFOMTNR0WGT_VVALID_RISE_AT_FIRST_DATA_EN, 0);
	MTNR1_SET_F(base, MTNR1_R_STAT_COUTFIFOMTNR0WGT_CONFIG,
		MTNR1_F_STAT_COUTFIFOMTNR0WGT_DEBUG_EN, 1);
	MTNR1_SET_F(base, MTNR1_R_STAT_COUTFIFOMTNR0WGT_INTERVAL_VBLANK,
		MTNR1_F_STAT_COUTFIFOMTNR0WGT_INTERVAL_VBLANK, VBLANK_CYCLE);

	MTNR1_SET_F(base, MTNR1_R_STAT_COUTFIFOMTNR0WGT_INTERVALS,
		MTNR1_F_STAT_COUTFIFOMTNR0WGT_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR1_SET_F(base, MTNR1_R_STAT_COUTFIFOMTNR0WGT_INTERVALS,
		MTNR1_F_STAT_COUTFIFOMTNR0WGT_INTERVAL_PIXEL, PBLANK_CYCLE);
}

void mtnr1_hw_s_otf_output_msnr_ls(struct pablo_mmio *base, u32 set_id, u32 enable,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)

{
	dbg_mtnr(4, "%s enable(%d)\n", __func__, enable);

	if (enable) {
		frame_cfg->cotf_out_en |= BIT_MASK(MTNR1_COTF_OUT_MSNR_L1);
		frame_cfg->cotf_out_en |= BIT_MASK(MTNR1_COTF_OUT_MSNR_L2);
		frame_cfg->cotf_out_en |= BIT_MASK(MTNR1_COTF_OUT_MSNR_L3);
		frame_cfg->cotf_out_en |= BIT_MASK(MTNR1_COTF_OUT_MSNR_L4);
	}

	/* MSNR L1 */
	MTNR1_SET_R(base, MTNR1_R_YUV_COUTFIFOMSNRL1_ENABLE, enable);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL1_CONFIG,
		MTNR1_F_YUV_COUTFIFOMSNRL1_VVALID_RISE_AT_FIRST_DATA_EN, 0);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_COUTFIFOMSNRL1_CONFIG, MTNR1_F_YUV_COUTFIFOMSNRL1_DEBUG_EN, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL1_INTERVAL_VBLANK,
		MTNR1_F_YUV_COUTFIFOMSNRL1_INTERVAL_VBLANK, VBLANK_CYCLE);

	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL1_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL1_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL1_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL1_INTERVAL_PIXEL, PBLANK_CYCLE);

	/* MSNR L2 */
	MTNR1_SET_R(base, MTNR1_R_YUV_COUTFIFOMSNRL2_ENABLE, enable);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL2_CONFIG,
		MTNR1_F_YUV_COUTFIFOMSNRL2_VVALID_RISE_AT_FIRST_DATA_EN, 0);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_COUTFIFOMSNRL2_CONFIG, MTNR1_F_YUV_COUTFIFOMSNRL2_DEBUG_EN, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL2_INTERVAL_VBLANK,
		MTNR1_F_YUV_COUTFIFOMSNRL2_INTERVAL_VBLANK, VBLANK_CYCLE);

	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL2_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL2_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL2_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL2_INTERVAL_PIXEL, PBLANK_CYCLE);

	/* MSNR L3 */
	MTNR1_SET_R(base, MTNR1_R_YUV_COUTFIFOMSNRL3_ENABLE, enable);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL3_CONFIG,
		MTNR1_F_YUV_COUTFIFOMSNRL3_VVALID_RISE_AT_FIRST_DATA_EN, 0);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_COUTFIFOMSNRL3_CONFIG, MTNR1_F_YUV_COUTFIFOMSNRL3_DEBUG_EN, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL3_INTERVAL_VBLANK,
		MTNR1_F_YUV_COUTFIFOMSNRL3_INTERVAL_VBLANK, VBLANK_CYCLE);

	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL3_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL3_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL3_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL3_INTERVAL_PIXEL, PBLANK_CYCLE);

	/* MSNR L4 */
	MTNR1_SET_R(base, MTNR1_R_YUV_COUTFIFOMSNRL4_ENABLE, enable);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL4_CONFIG,
		MTNR1_F_YUV_COUTFIFOMSNRL4_VVALID_RISE_AT_FIRST_DATA_EN, 0);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_COUTFIFOMSNRL4_CONFIG, MTNR1_F_YUV_COUTFIFOMSNRL4_DEBUG_EN, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL4_INTERVAL_VBLANK,
		MTNR1_F_YUV_COUTFIFOMSNRL4_INTERVAL_VBLANK, VBLANK_CYCLE);

	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL4_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL4_INTERVAL_HBLANK, HBLANK_CYCLE);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL4_INTERVALS,
		MTNR1_F_YUV_COUTFIFOMSNRL4_INTERVAL_PIXEL, PBLANK_CYCLE);
}

void mtnr1_hw_s_input_size_l1(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	dbg_mtnr(4, "%s (%dx%d)\n", __func__, width, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L1, MTNR1_F_YUV_IN_IMG_SZ_WIDTH_L1,
		width);
	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L1, MTNR1_F_YUV_IN_IMG_SZ_HEIGHT_L1,
		height);
}

void mtnr1_hw_s_input_size_l2(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	dbg_mtnr(4, "%s (%dx%d)\n", __func__, width, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L2, MTNR1_F_YUV_IN_IMG_SZ_WIDTH_L2,
		width);
	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L2, MTNR1_F_YUV_IN_IMG_SZ_HEIGHT_L2,
		height);
}

void mtnr1_hw_s_input_size_l3(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	dbg_mtnr(4, "%s (%dx%d)\n", __func__, width, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L3, MTNR1_F_YUV_IN_IMG_SZ_WIDTH_L3,
		width);
	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L3, MTNR1_F_YUV_IN_IMG_SZ_HEIGHT_L3,
		height);
}

void mtnr1_hw_s_input_size_l4(struct pablo_mmio *base, u32 set_id, u32 width, u32 height)
{
	dbg_mtnr(4, "%s (%dx%d)\n", __func__, width, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L4, MTNR1_F_YUV_IN_IMG_SZ_WIDTH_L4,
		width);
	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_IN_IMG_SZ_WIDTH_L4, MTNR1_F_YUV_IN_IMG_SZ_HEIGHT_L4,
		height);
}

void mtnr1_hw_s_geomatch_size_l1(struct pablo_mmio *base, u32 set_id, u32 frame_width,
	u32 dma_width, u32 height, bool strip_enable, u32 strip_start_pos,
	struct is_mtnr1_config *mtnr_config)
{
	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_REF_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL1_REF_IMG_WIDTH,
		dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_REF_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL1_REF_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL1_REF_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL1_REF_ROI_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL1_ROI_SIZE_X,
		frame_width);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL1_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL1_ROI_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL1_SCH_IMG_WIDTH,
		frame_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL1_SCH_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL1_SCH_ACTIVE_START_X, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL1_SCH_ACTIVE_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL1_SCH_ACTIVE_SIZE_X, dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL1_SCH_ACTIVE_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL1_SCH_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL1_SCH_ROI_START_Y, 0);
}

void mtnr1_hw_s_geomatch_size_l2(struct pablo_mmio *base, u32 set_id, u32 frame_width,
	u32 dma_width, u32 height, bool strip_enable, u32 strip_start_pos,
	struct is_mtnr1_config *mtnr_config)
{
	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_REF_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL2_REF_IMG_WIDTH,
		dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_REF_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL2_REF_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL2_REF_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL2_REF_ROI_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL2_ROI_SIZE_X,
		frame_width);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL2_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL2_ROI_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL2_SCH_IMG_WIDTH,
		frame_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL2_SCH_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL2_SCH_ACTIVE_START_X, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL2_SCH_ACTIVE_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL2_SCH_ACTIVE_SIZE_X, dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL2_SCH_ACTIVE_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL2_SCH_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL2_SCH_ROI_START_Y, 0);
}

void mtnr1_hw_s_geomatch_size_l3(struct pablo_mmio *base, u32 set_id, u32 frame_width,
	u32 dma_width, u32 height, bool strip_enable, u32 strip_start_pos,
	struct is_mtnr1_config *mtnr_config)
{
	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_REF_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL3_REF_IMG_WIDTH,
		dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_REF_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL3_REF_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL3_REF_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL3_REF_ROI_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL3_ROI_SIZE_X,
		frame_width);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL3_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL3_ROI_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL3_SCH_IMG_WIDTH,
		frame_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL3_SCH_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL3_SCH_ACTIVE_START_X, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL3_SCH_ACTIVE_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL3_SCH_ACTIVE_SIZE_X, dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL3_SCH_ACTIVE_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL3_SCH_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL3_SCH_ROI_START_Y, 0);
}

void mtnr1_hw_s_geomatch_size_l4(struct pablo_mmio *base, u32 set_id, u32 frame_width,
	u32 dma_width, u32 height, bool strip_enable, u32 strip_start_pos,
	struct is_mtnr1_config *mtnr_config)
{
	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_REF_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL4_REF_IMG_WIDTH,
		dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_REF_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL4_REF_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL4_REF_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_REF_ROI_START,
		MTNR1_F_YUV_GEOMATCHL4_REF_ROI_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL4_ROI_SIZE_X,
		frame_width);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL4_ROI_SIZE, MTNR1_F_YUV_GEOMATCHL4_ROI_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_IMG_SIZE, MTNR1_F_YUV_GEOMATCHL4_SCH_IMG_WIDTH,
		frame_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_IMG_SIZE,
		MTNR1_F_YUV_GEOMATCHL4_SCH_IMG_HEIGHT, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL4_SCH_ACTIVE_START_X, strip_start_pos);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_ACTIVE_START,
		MTNR1_F_YUV_GEOMATCHL4_SCH_ACTIVE_START_Y, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL4_SCH_ACTIVE_SIZE_X, dma_width);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_ACTIVE_SIZE,
		MTNR1_F_YUV_GEOMATCHL4_SCH_ACTIVE_SIZE_Y, height);

	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL4_SCH_ROI_START_X, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_SCH_ROI_START,
		MTNR1_F_YUV_GEOMATCHL4_SCH_ROI_START_Y, 0);
}

void mtnr1_hw_s_mixer_size_l1(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct mtnr1_radial_cfg *radial_cfg,
	struct is_mtnr1_config *mtnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x *
		    radial_cfg->sw_binning_x * 1024ULL * radial_cfg->rgbp_crop_w / frame_width
			 / 1000 / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y *
		    radial_cfg->sw_binning_y * 1024ULL * radial_cfg->rgbp_crop_h / height
			 / 1000 / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
		   (radial_cfg->bns_binning_x * (radial_cfg->rgbp_crop_offset_x + strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
		   (radial_cfg->bns_binning_y * radial_cfg->rgbp_crop_offset_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_mtnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_BINNING_X, MTNR1_F_YUV_MIXERL1_BINNING_X, binning_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_BINNING_X, MTNR1_F_YUV_MIXERL1_BINNING_Y, binning_y);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL1_RADIAL_CENTER_X,
		radial_center_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL1_RADIAL_CENTER_Y,
		radial_center_y);
}

void mtnr1_hw_s_mixer_size_l2(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct mtnr1_radial_cfg *radial_cfg,
	struct is_mtnr1_config *mtnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x *
		    radial_cfg->sw_binning_x * 1024ULL * radial_cfg->rgbp_crop_w / frame_width
			 / 1000 / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y *
		    radial_cfg->sw_binning_y * 1024ULL * radial_cfg->rgbp_crop_h / height
			 / 1000 / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
		   (radial_cfg->bns_binning_x * (radial_cfg->rgbp_crop_offset_x + strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
		   (radial_cfg->bns_binning_y * radial_cfg->rgbp_crop_offset_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_mtnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_BINNING_X, MTNR1_F_YUV_MIXERL2_BINNING_X, binning_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_BINNING_X, MTNR1_F_YUV_MIXERL2_BINNING_Y, binning_y);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL2_RADIAL_CENTER_X,
		radial_center_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL2_RADIAL_CENTER_Y,
		radial_center_y);
}

void mtnr1_hw_s_mixer_size_l3(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct mtnr1_radial_cfg *radial_cfg,
	struct is_mtnr1_config *mtnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x *
		    radial_cfg->sw_binning_x * 1024ULL * radial_cfg->rgbp_crop_w / frame_width
			 / 1000 / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y *
		    radial_cfg->sw_binning_y * 1024ULL * radial_cfg->rgbp_crop_h / height
			 / 1000 / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
		   (radial_cfg->bns_binning_x * (radial_cfg->rgbp_crop_offset_x + strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
		   (radial_cfg->bns_binning_y * radial_cfg->rgbp_crop_offset_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_mtnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_BINNING_X, MTNR1_F_YUV_MIXERL3_BINNING_X, binning_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_BINNING_X, MTNR1_F_YUV_MIXERL3_BINNING_Y, binning_y);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL3_RADIAL_CENTER_X,
		radial_center_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL3_RADIAL_CENTER_Y,
		radial_center_y);
}

void mtnr1_hw_s_mixer_size_l4(struct pablo_mmio *base, u32 set_id, u32 frame_width, u32 dma_width,
	u32 height, bool strip_enable, u32 strip_start_pos, struct mtnr1_radial_cfg *radial_cfg,
	struct is_mtnr1_config *mtnr_config)
{
	int binning_x, binning_y;
	u32 sensor_center_x, sensor_center_y;
	int radial_center_x, radial_center_y;
	u32 offset_x, offset_y;

	dbg_mtnr(4, "%s size(%d, %dx%d), strip(%d, %d)\n", __func__, frame_width, dma_width, height,
		strip_enable, strip_start_pos);

	binning_x = radial_cfg->sensor_binning_x * radial_cfg->bns_binning_x *
		    radial_cfg->sw_binning_x * 1024ULL * radial_cfg->rgbp_crop_w / frame_width
			 / 1000 / 1000 / 1000;
	binning_y = radial_cfg->sensor_binning_y * radial_cfg->bns_binning_y *
		    radial_cfg->sw_binning_y * 1024ULL * radial_cfg->rgbp_crop_h / height
			 / 1000 / 1000 / 1000;

	sensor_center_x = (radial_cfg->sensor_full_width >> 1) & (~0x1);
	sensor_center_y = (radial_cfg->sensor_full_height >> 1) & (~0x1);

	offset_x = radial_cfg->sensor_crop_x +
		   (radial_cfg->bns_binning_x * (radial_cfg->rgbp_crop_offset_x + strip_start_pos) / 1000);
	offset_y = radial_cfg->sensor_crop_y +
		   (radial_cfg->bns_binning_y * radial_cfg->rgbp_crop_offset_y / 1000);

	radial_center_x = -sensor_center_x + radial_cfg->sensor_binning_x * offset_x / 1000;
	radial_center_y = -sensor_center_y + radial_cfg->sensor_binning_y * offset_y / 1000;

	dbg_mtnr(4, "%s: binning(%d,%d), radial_center(%d,%d)", __func__, binning_x, binning_y,
		radial_center_x, radial_center_y);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_BINNING_X, MTNR1_F_YUV_MIXERL4_BINNING_X, binning_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_BINNING_X, MTNR1_F_YUV_MIXERL4_BINNING_Y, binning_y);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL4_RADIAL_CENTER_X,
		radial_center_x);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_RADIAL_CENTER_X, MTNR1_F_YUV_MIXERL4_RADIAL_CENTER_Y,
		radial_center_y);
}

void mtnr1_hw_s_crop_clean_img_otf(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass)
{
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANOTFL1_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL1_BYPASS, bypass);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANOTFL2_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL2_BYPASS, bypass);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANOTFL3_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL3_BYPASS, bypass);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANOTFL4_BYPASS, MTNR1_F_YUV_CROPCLEANOTFL4_BYPASS, bypass);

	if (!bypass) {
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL1_START,
			MTNR1_F_YUV_CROPCLEANOTFL1_START_X, start_x);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL1_START,
			MTNR1_F_YUV_CROPCLEANOTFL1_START_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL1_SIZE,
			MTNR1_F_YUV_CROPCLEANOTFL1_SIZE_X, width);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANOTFL1_SIZE,
			MTNR1_F_YUV_CROPCLEANOTFL1_SIZE_Y, height);

		/* TODO: L2~L4 */
	}
}

void mtnr1_hw_s_crop_wgt_otf(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass)
{
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_BYPASS, MTNR1_F_YUV_CROPWEIGHTDMAL1_BYPASS,
		bypass);

	if (!bypass) {
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_START,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_START_X, start_x);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_START,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_START_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_SIZE,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_SIZE_X, width);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_SIZE,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_SIZE_Y, height);
	}
}

void mtnr1_hw_s_crop_clean_img_dma(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass)
{
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANDMAL1_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL1_BYPASS, bypass);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANDMAL2_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL2_BYPASS, bypass);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANDMAL3_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL3_BYPASS, bypass);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_CROPCLEANDMAL4_BYPASS, MTNR1_F_YUV_CROPCLEANDMAL4_BYPASS, bypass);

	if (!bypass) {
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL1_START,
			MTNR1_F_YUV_CROPCLEANDMAL1_START_X, start_x);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL1_START,
			MTNR1_F_YUV_CROPCLEANDMAL1_START_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL1_SIZE,
			MTNR1_F_YUV_CROPCLEANDMAL1_SIZE_X, width);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPCLEANDMAL1_SIZE,
			MTNR1_F_YUV_CROPCLEANDMAL1_SIZE_Y, height);

		/* TODO: L2 ~ L4 */
	}
}

void mtnr1_hw_s_crop_wgt_dma(
	struct pablo_mmio *base, u32 set_id, u32 start_x, u32 width, u32 height, bool bypass)
{
	MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_BYPASS, MTNR1_F_YUV_CROPWEIGHTDMAL1_BYPASS,
		bypass);

	if (!bypass) {
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_START,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_START_X, start_x);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_START,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_START_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_SIZE,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_SIZE_X, width);
		MTNR1_SET_F(base, MTNR1_R_YUV_CROPWEIGHTDMAL1_SIZE,
			MTNR1_F_YUV_CROPWEIGHTDMAL1_SIZE_Y, height);
	}
}

void mtnr1_hw_s_img_bitshift(struct pablo_mmio *base, u32 set_id, u32 img_shift_bit)
{
	if (img_shift_bit) {
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_LSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_LSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_LSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_OFFSET_V, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_RSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_RSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_RSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_OFFSET_V, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_LSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_LSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_LSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_OFFSET_V, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_RSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_RSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_RSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_OFFSET_V, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_LSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_LSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_LSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_OFFSET_V, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_RSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_RSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_RSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_OFFSET_V, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_LSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_LSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_LSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_OFFSET_V, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_RSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_RSHIFT_U, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_RSHIFT_V, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_OFFSET_Y, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_OFFSET_U, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_OFFSET_V, 0);

		/* CURR */ /* TODO: PREV and CUR are same ?? */
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL2_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL2_LSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL2_OFFSET_Y, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL3_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL3_LSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL3_OFFSET_Y, 0);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL4_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL4_LSHIFT_Y, img_shift_bit);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL4_OFFSET_Y, 0);
	} else {
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL1_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL1_BYPASS, 1);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL2_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL2_BYPASS, 1);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL3_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL3_BYPASS, 1);

		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERPREVRDMAL4_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERWDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERWDMAL4_BYPASS, 1);

		/* CURR */ /* TODO: PREV and CUR are same ?? */
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL2_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL2_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL3_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL3_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERCURRRDMAL4_CTRL,
			MTNR1_F_YUV_DATASHIFTERCURRRDMAL4_BYPASS, 1);
	}
}

void mtnr1_hw_s_wgt_bitshift(struct pablo_mmio *base, u32 set_id, u32 wgt_shift_bit)
{
}

void mtnr1_hw_g_img_bitshift(struct pablo_mmio *base, u32 set_id, u32 *shift, u32 *shift_chroma,
	u32 *offset, u32 *offset_chroma)
{
	*shift = MTNR1_GET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
		MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_LSHIFT_Y);
	*shift_chroma = MTNR1_GET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
		MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_LSHIFT_U);

	*offset = MTNR1_GET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
		MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_OFFSET_Y);
	*offset_chroma = MTNR1_GET_F(base, MTNR1_R_YUV_MAIN_CTRL_DATASHIFTERRDMAL1_CTRL,
		MTNR1_F_YUV_DATASHIFTERPREVRDMAL1_OFFSET_U);

	/* TODO: L2 ~ L4 or remove it */
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_g_img_bitshift);

void mtnr1_hw_g_wgt_bitshift(struct pablo_mmio *base, u32 set_id, u32 *shift)
{
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_g_wgt_bitshift);

void mtnr1_hw_s_mono_mode(struct pablo_mmio *base, u32 set_id, bool enable)
{
}

void mtnr1_hw_s_mvf_resize_offset(struct pablo_mmio *base, u32 set_id,
		u32 in_w, u32 in_h, u32 out_w, u32 out_h, u32 pos)
{
	int offset_x;
	int offset_y;

	int inverse_scale_x = (((1 << 8) * in_w + (out_w / 2)) / out_w);
	int resized_offset_x = (((inverse_scale_x - (1 << 8)) + 1) / 2);

	int inverse_scale_y = (((1 << 8) * in_h + (out_h / 2)) / out_h);
	int resized_offset_y = (((inverse_scale_y - (1 << 8)) + 1) / 2);

	int M = (pos / 32) * inverse_scale_x + resized_offset_x;

	offset_x = M % (1 << 8);
	offset_y = resized_offset_y;

	dbg_mtnr(4, "%s in(%dx%d), out(%dx%d), offset(%d, %d)\n", __func__,
			in_w, in_h, out_w, out_h, offset_x, offset_y);

	MTNR1_SET_F(base, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_X, offset_x);
	MTNR1_SET_F(base, MTNR1_R_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET,
			MTNR1_F_STAT_MVCONTROLLER_MVF_RESIZE_OFFSET_Y, offset_y);
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_s_mvf_resize_offset);

void mtnr1_hw_s_crc(struct pablo_mmio *base, u32 seed)
{
	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_CRC_EN, MTNR1_F_YUV_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_DTPL1_STREAM_CRC, MTNR1_F_YUV_DTPL1_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_DTPL2_STREAM_CRC, MTNR1_F_YUV_DTPL2_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_DTPL3_STREAM_CRC, MTNR1_F_YUV_DTPL3_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_DTPL4_STREAM_CRC, MTNR1_F_YUV_DTPL4_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_STREAM_CRC,
		MTNR1_F_STAT_CINFIFODLFEWGT_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_STAT_COUTFIFOMTNR0WGT_STREAM_CRC,
		MTNR1_F_STAT_COUTFIFOMTNR0WGT_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL1_STREAM_CRC,
		MTNR1_F_YUV_COUTFIFOMSNRL1_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL2_STREAM_CRC,
		MTNR1_F_YUV_COUTFIFOMSNRL2_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL3_STREAM_CRC,
		MTNR1_F_YUV_COUTFIFOMSNRL3_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFOMSNRL4_STREAM_CRC,
		MTNR1_F_YUV_COUTFIFOMSNRL4_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFODLFECURR_STREAM_CRC,
		MTNR1_F_YUV_COUTFIFODLFECURR_CRC_SEED, seed);
	MTNR1_SET_F(base, MTNR1_R_YUV_COUTFIFODLFEPREV_STREAM_CRC,
		MTNR1_F_YUV_COUTFIFODLFEPREV_CRC_SEED, seed);
}

void mtnr1_hw_s_dtp(struct pablo_mmio *base, u32 enable, enum mtnr1_dtp_type type, u32 y, u32 u,
	u32 v, enum mtnr1_dtp_color_bar cb)
{
	dbg_mtnr(4, "%s %d\n", __func__, enable);

	if (enable) {
		MTNR1_SET_F(
			base, MTNR1_R_YUV_DTPL1_CTRL, MTNR1_F_YUV_DTPL1_TEST_PATTERN_MODE, type);
		MTNR1_SET_F(
			base, MTNR1_R_YUV_DTPL2_CTRL, MTNR1_F_YUV_DTPL2_TEST_PATTERN_MODE, type);
		MTNR1_SET_F(
			base, MTNR1_R_YUV_DTPL3_CTRL, MTNR1_F_YUV_DTPL3_TEST_PATTERN_MODE, type);
		MTNR1_SET_F(
			base, MTNR1_R_YUV_DTPL4_CTRL, MTNR1_F_YUV_DTPL4_TEST_PATTERN_MODE, type);
		if (type == MTNR1_DTP_SOLID_IMAGE) {
			MTNR1_SET_F(base, MTNR1_R_YUV_DTPL1_CTRL, MTNR1_F_YUV_DTPL1_TEST_DATA_Y, y);
			MTNR1_SET_F(base, MTNR1_R_YUV_DTPL2_CTRL, MTNR1_F_YUV_DTPL2_TEST_DATA_Y, y);
			MTNR1_SET_F(base, MTNR1_R_YUV_DTPL3_CTRL, MTNR1_F_YUV_DTPL3_TEST_DATA_Y, y);
			MTNR1_SET_F(base, MTNR1_R_YUV_DTPL4_CTRL, MTNR1_F_YUV_DTPL4_TEST_DATA_Y, y);
		} else {
			MTNR1_SET_F(
				base, MTNR1_R_YUV_DTPL1_CTRL, MTNR1_F_YUV_DTPL1_YUV_STANDARD, cb);
			MTNR1_SET_F(
				base, MTNR1_R_YUV_DTPL2_CTRL, MTNR1_F_YUV_DTPL2_YUV_STANDARD, cb);
			MTNR1_SET_F(
				base, MTNR1_R_YUV_DTPL3_CTRL, MTNR1_F_YUV_DTPL3_YUV_STANDARD, cb);
			MTNR1_SET_F(
				base, MTNR1_R_YUV_DTPL4_CTRL, MTNR1_F_YUV_DTPL4_YUV_STANDARD, cb);
		}
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL1_CTRL, MTNR1_F_YUV_DTPL1_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL2_CTRL, MTNR1_F_YUV_DTPL2_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL3_CTRL, MTNR1_F_YUV_DTPL3_BYPASS, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL4_CTRL, MTNR1_F_YUV_DTPL4_BYPASS, 0);
	} else {
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL1_CTRL, MTNR1_F_YUV_DTPL1_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL2_CTRL, MTNR1_F_YUV_DTPL2_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL3_CTRL, MTNR1_F_YUV_DTPL3_BYPASS, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_DTPL4_CTRL, MTNR1_F_YUV_DTPL4_BYPASS, 1);
	}
}

void mtnr1_hw_debug_s_geomatch_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode)
{
	if (tnr_mode == MTNR1_TNR_MODE_PREPARE) {
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_EN, MTNR1_F_YUV_GEOMATCHL1_EN, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_EN, MTNR1_F_YUV_GEOMATCHL2_EN, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_EN, MTNR1_F_YUV_GEOMATCHL3_EN, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_EN, MTNR1_F_YUV_GEOMATCHL4_EN, 0);
	} else {
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_EN, MTNR1_F_YUV_GEOMATCHL1_EN, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_EN, MTNR1_F_YUV_GEOMATCHL2_EN, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_EN, MTNR1_F_YUV_GEOMATCHL3_EN, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_EN, MTNR1_F_YUV_GEOMATCHL4_EN, 1);
	}

	/* TODO: FIXME: why MATCH_ENABE always set to 0?? */
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL1_MC_LMC_TNR_MODE,
		MTNR1_F_YUV_GEOMATCHL1_MC_LMC_TNR_MODE, tnr_mode);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL1_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL1_MATCH_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL2_MC_LMC_TNR_MODE,
		MTNR1_F_YUV_GEOMATCHL2_MC_LMC_TNR_MODE, tnr_mode);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL2_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL2_MATCH_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL3_MC_LMC_TNR_MODE,
		MTNR1_F_YUV_GEOMATCHL3_MC_LMC_TNR_MODE, tnr_mode);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL3_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL3_MATCH_ENABLE, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_GEOMATCHL4_MC_LMC_TNR_MODE,
		MTNR1_F_YUV_GEOMATCHL4_MC_LMC_TNR_MODE, tnr_mode);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_GEOMATCHL4_MATCH_ENABLE, MTNR1_F_YUV_GEOMATCHL4_MATCH_ENABLE, 0);
}

void mtnr1_hw_debug_s_mixer_mode(struct pablo_mmio *base, u32 set_id, u32 tnr_mode)
{
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_ENABLE, MTNR1_F_YUV_MIXERL1_ENABLE, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_MODE, MTNR1_F_YUV_MIXERL1_MODE, tnr_mode);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_ENABLE, MTNR1_F_YUV_MIXERL2_ENABLE, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_MODE, MTNR1_F_YUV_MIXERL2_MODE, tnr_mode);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_ENABLE, MTNR1_F_YUV_MIXERL3_ENABLE, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_MODE, MTNR1_F_YUV_MIXERL3_MODE, tnr_mode);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_ENABLE, MTNR1_F_YUV_MIXERL4_ENABLE, 1);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_MODE, MTNR1_F_YUV_MIXERL4_MODE, tnr_mode);

	/* avoid rule checker assertion*/
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
		MTNR1_F_STAT_CINFIFODLFEWGT_ROL_RESET_ON_FRAME_START, 0);

	/* L1 */
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_0, 127);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_1, 84);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_2, 101);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_3, 138);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_4, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_5, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_6, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL1_THRESH_SLOPE_0_7, 309);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_0, MTNR1_F_YUV_MIXERL1_THRESH_0_0, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_0, MTNR1_F_YUV_MIXERL1_THRESH_0_1, 96);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_2, MTNR1_F_YUV_MIXERL1_THRESH_0_2, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_2, MTNR1_F_YUV_MIXERL1_THRESH_0_3, 43);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_4, MTNR1_F_YUV_MIXERL1_THRESH_0_4, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_4, MTNR1_F_YUV_MIXERL1_THRESH_0_5, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_6, MTNR1_F_YUV_MIXERL1_THRESH_0_6, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_THRESH_0_6, MTNR1_F_YUV_MIXERL1_THRESH_0_7, 21);

	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_0, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_1, 96);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_2, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_3, 43);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_4, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_5, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_6, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL1_SUB_THRESH_0_7, 21);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_0, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_1, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_2, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_3, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_4, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_5, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_6, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_WIDTH_0_7, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_0, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_1, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_2, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_3, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_4, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_5, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_6, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL1_SUB_THRESH_SLOPE_0_7, 16383);

	/* L2 */
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_0, 127);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_1, 84);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_2, 101);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_3, 138);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_4, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_5, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_6, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL2_THRESH_SLOPE_0_7, 309);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_0, MTNR1_F_YUV_MIXERL2_THRESH_0_0, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_0, MTNR1_F_YUV_MIXERL2_THRESH_0_1, 96);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_2, MTNR1_F_YUV_MIXERL2_THRESH_0_2, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_2, MTNR1_F_YUV_MIXERL2_THRESH_0_3, 43);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_4, MTNR1_F_YUV_MIXERL2_THRESH_0_4, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_4, MTNR1_F_YUV_MIXERL2_THRESH_0_5, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_6, MTNR1_F_YUV_MIXERL2_THRESH_0_6, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_THRESH_0_6, MTNR1_F_YUV_MIXERL2_THRESH_0_7, 21);

	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_0, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_1, 96);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_2, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_3, 43);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_4, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_5, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_6, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL2_SUB_THRESH_0_7, 21);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_0, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_1, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_2, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_3, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_4, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_5, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_6, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_WIDTH_0_7, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_0, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_1, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_2, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_3, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_4, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_5, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_6, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL2_SUB_THRESH_SLOPE_0_7, 16383);

	/* L3 */
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_0, 127);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_1, 84);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_2, 101);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_3, 138);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_4, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_5, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_6, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL3_THRESH_SLOPE_0_7, 309);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_0, MTNR1_F_YUV_MIXERL3_THRESH_0_0, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_0, MTNR1_F_YUV_MIXERL3_THRESH_0_1, 96);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_2, MTNR1_F_YUV_MIXERL3_THRESH_0_2, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_2, MTNR1_F_YUV_MIXERL3_THRESH_0_3, 43);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_4, MTNR1_F_YUV_MIXERL3_THRESH_0_4, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_4, MTNR1_F_YUV_MIXERL3_THRESH_0_5, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_6, MTNR1_F_YUV_MIXERL3_THRESH_0_6, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_THRESH_0_6, MTNR1_F_YUV_MIXERL3_THRESH_0_7, 21);

	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_0, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_1, 96);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_2, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_3, 43);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_4, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_5, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_6, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL3_SUB_THRESH_0_7, 21);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_0, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_1, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_2, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_3, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_4, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_5, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_6, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_WIDTH_0_7, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_0, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_1, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_2, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_3, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_4, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_5, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_6, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL3_SUB_THRESH_SLOPE_0_7, 16383);

	/* L4 */
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_0, 127);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_1, 84);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_2, 101);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_3, 138);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_4, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_5, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_6, 309);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL4_THRESH_SLOPE_0_7, 309);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_0, MTNR1_F_YUV_MIXERL4_THRESH_0_0, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_0, MTNR1_F_YUV_MIXERL4_THRESH_0_1, 96);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_2, MTNR1_F_YUV_MIXERL4_THRESH_0_2, 86);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_2, MTNR1_F_YUV_MIXERL4_THRESH_0_3, 43);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_4, MTNR1_F_YUV_MIXERL4_THRESH_0_4, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_4, MTNR1_F_YUV_MIXERL4_THRESH_0_5, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_6, MTNR1_F_YUV_MIXERL4_THRESH_0_6, 21);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_THRESH_0_6, MTNR1_F_YUV_MIXERL4_THRESH_0_7, 21);

	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_0, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_0, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_1, 96);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_2, 86);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_2, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_3, 43);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_4, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_4, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_5, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_6, 21);
	MTNR1_SET_F(
		base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_0_6, MTNR1_F_YUV_MIXERL4_SUB_THRESH_0_7, 21);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_0, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_0,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_1, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_2, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_2,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_3, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_4, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_4,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_5, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_6, 0);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_WIDTH_0_6,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_WIDTH_0_7, 0);

	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_0, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_0,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_1, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_2, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_2,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_3, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_4, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_4,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_5, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_6, 16383);
	MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_SUB_THRESH_SLOPE_0_6,
		MTNR1_F_YUV_MIXERL4_SUB_THRESH_SLOPE_0_7, 16383);

	if ((tnr_mode == MTNR1_TNR_MODE_PREPARE) || (tnr_mode == MTNR1_TNR_MODE_FUSION)) {
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL1_WGT_UPDATE_EN, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL2_WGT_UPDATE_EN, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL3_WGT_UPDATE_EN, 0);
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL4_WGT_UPDATE_EN, 0);
	} else {
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL1_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL1_WGT_UPDATE_EN, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL2_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL2_WGT_UPDATE_EN, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL3_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL3_WGT_UPDATE_EN, 1);
		MTNR1_SET_F(base, MTNR1_R_YUV_MIXERL4_WGT_UPDATE_EN,
			MTNR1_F_YUV_MIXERL4_WGT_UPDATE_EN, 1);
	}
}

void mtnr1_hw_s_strgen(struct pablo_mmio *base, u32 set_id)
{
	/* STRGEN setting */
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
		MTNR1_F_STAT_CINFIFODLFEWGT_STRGEN_MODE_EN, 1);
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
		MTNR1_F_STAT_CINFIFODLFEWGT_STRGEN_MODE_DATA_TYPE, 1);
	MTNR1_SET_F(base, MTNR1_R_STAT_CINFIFODLFEWGT_CONFIG,
		MTNR1_F_STAT_CINFIFODLFEWGT_STRGEN_MODE_DATA, 255);
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_s_strgen);

void mtnr1_hw_s_seg_otf_to_msnr(struct pablo_mmio *base, u32 en)
{
	dbg_mtnr(4, "%s %d\n", __func__, en);

	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_OTF_SEG_EN, MTNR1_F_YUV_OTF_SEG_EN, en);
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_s_seg_otf_to_msnr);

void mtnr1_hw_s_still_last_frame_en(struct pablo_mmio *base, u32 en)
{
	MTNR1_SET_F(base, MTNR1_R_YUV_MAIN_CTRL_STILL_LAST_FRAME_EN,
		MTNR1_F_YUV_MAIN_CTRL_STILL_LAST_FRAME_EN, en);
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_s_still_last_frame_en);

u32 mtnr1_hw_g_reg_cnt(void)
{
	return MTNR1_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_g_reg_cnt);

const struct pmio_field_desc *mtnr1_hw_g_field_descs(void)
{
	return mtnr1_field_descs;
}

unsigned int mtnr1_hw_g_num_field_descs(void)
{
	return ARRAY_SIZE(mtnr1_field_descs);
}

const struct pmio_access_table *mtnr1_hw_g_access_table(int type)
{
	switch (type) {
	case 0:
		return &mtnr1_volatile_ranges_table;
	case 1:
		return &mtnr1_wr_noinc_ranges_table;
	default:
		return NULL;
	};

	return NULL;
}

void mtnr1_hw_init_pmio_config(struct pmio_config *cfg)
{
	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &mtnr1_volatile_ranges_table;
	cfg->wr_noinc_table = &mtnr1_wr_noinc_ranges_table;

	cfg->max_register = MTNR1_R_YUV_CRC_L4_WDMA_IN;
	cfg->num_reg_defaults_raw = (MTNR1_R_YUV_CRC_L4_WDMA_IN >> 2) + 1;
	cfg->dma_addr_shift = 4;

	cfg->ranges = mtnr1_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(mtnr1_range_cfgs);

	cfg->fields = mtnr1_field_descs;
	cfg->num_fields = ARRAY_SIZE(mtnr1_field_descs);
}
KUNIT_EXPORT_SYMBOL(mtnr1_hw_init_pmio_config);
