// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung EXYNOS CAMERA PostProcessing lme driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-api-lme.h"
#include "is-hw-common-dma.h"
#include "is-hw-control.h"

#include "pablo-hw-reg-lme-v13_0_0.h"

#include "pmio.h"

#define LME_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define LME_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define LME_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define LME_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define LME_GET_R(base, R) PMIO_GET_R(base, R)

static const struct lme_variant lme_variant[] = {
	{
		.limit_input = {
			.min_w		= 90,
			.min_h		= 60,
			.max_w		= 1664,
			.max_h		= 1248,
		},
		.version		= IP_VERSION,
	},
};

const struct lme_variant *camerapp_hw_lme_get_size_constraints(struct pablo_mmio *pmio)
{
	return lme_variant;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_size_constraints);

u32 camerapp_hw_lme_get_ver(struct pablo_mmio *pmio)
{
	return LME_GET_R(pmio, LME_R_IP_VERSION);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_ver);

void camerapp_hw_lme_set_cmdq(struct pablo_mmio *pmio, dma_addr_t clh, u32 noh)
{
	lme_dbg("[LME] clh(%llu), noh(%d)\n", clh, noh);

	if (clh && noh) {
		LME_SET_F(pmio, LME_R_CMDQ_QUE_CMD_H, LME_F_CMDQ_QUE_CMD_BASE_ADDR,
			DVA_36BIT_HIGH(clh));
		LME_SET_F(pmio, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_HEADER_NUM, noh);
		LME_SET_F(pmio, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE, 1);
	} else {
		LME_SET_F(pmio, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	}

	LME_SET_R(pmio, LME_R_CMDQ_ADD_TO_QUEUE_0, 1);
	LME_SET_R(pmio, LME_R_CMDQ_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_cmdq);

void camerapp_hw_lme_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb)
{
	lme_dbg("[LME]\n");
	camerapp_hw_lme_set_cmdq(pmio, clb->header_dva, clb->num_of_headers);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_start);

void camerapp_hw_lme_stop(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_stop);

void camerapp_hw_lme_clear_intr_all(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
	LME_SET_F(pmio, LME_R_INT_REQ_INT0_CLEAR, LME_F_INT_REQ_INT0_CLEAR, LME_INT_EN_MASK);
	/* INT1 is enabled if it necessary */
	/* LME_SET_F(pmio, LME_R_INT_REQ_INT1_CLEAR, LME_F_INT_REQ_INT1_CLEAR, LME_INT1_EN); */
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_clear_intr_all);

u32 camerapp_hw_lme_get_intr_status_and_clear(struct pablo_mmio *pmio)
{
	u32 int0_status, int1_status;
	lme_dbg("[LME]\n");

	int0_status = LME_GET_R(pmio, LME_R_INT_REQ_INT0);
	LME_SET_R(pmio, LME_R_INT_REQ_INT0_CLEAR, int0_status);

	int1_status = LME_GET_R(pmio, LME_R_INT_REQ_INT1);
	LME_SET_R(pmio, LME_R_INT_REQ_INT1_CLEAR, int1_status);

	lme_dbg("[LME]int0(0x%x), int1(0x%x)\n", int0_status, int1_status);

	return int0_status;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_intr_status_and_clear);

u32 camerapp_hw_lme_get_int_frame_start(void)
{
	return LME_INT_FRAME_START;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_int_frame_start);

u32 camerapp_hw_lme_get_int_frame_end(void)
{
	return LME_INT_FRAME_END;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_int_frame_end);

u32 camerapp_hw_lme_get_int_err(void)
{
	return LME_INT_ERR;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_int_err);

int camerapp_hw_lme_get_output_size(int width, int height, int *total_width, int *line_count,
				    enum lme_sps_mode sps_mode, enum lme_wdma_index type)
{
	int divider_width = (sps_mode == LME_SPS_2X2) ? 2 : 8;
	int divider_height = (sps_mode == LME_SPS_2X2) ? 2 : 4;
	int bytes_per_pixel;

	/* [byte_format info] 0: mv out (4B), 1: sad out (3B) */
	if (type == LME_WDMA_MV_OUT) {
		bytes_per_pixel = 4;
	} else if (type == LME_WDMA_SAD_OUT) {
		bytes_per_pixel = 3;
	} else if (type == LME_WDMA_MBMV_OUT) {
		return SET_SUCCESS;
	} else {
		err_hw("[LME] not support type(%d)", type);
		return SET_ERROR;
	}

	if (width == 0 || height == 0) {
		err_hw("[LME] input is NULL(%dx%d)", width, height);
		return SET_ERROR;
	}

	*total_width = DIV_ROUND_UP(width, divider_width) * (bytes_per_pixel);
	*total_width = ALIGN(*total_width, 16); /* root, solomon HW constraint : WDMA */
	*line_count = DIV_ROUND_UP(height, divider_height);

	lme_dbg("[LME] size(%dx%d->%dx%d), sps_mode(%s:%dx%d), type(%d:%dB)\n", width, height,
		*total_width, *line_count, sps_mode == LME_SPS_2X2 ? "SPS_2X2" : "SPS_8X4",
		divider_width, divider_height, type, bytes_per_pixel);

	if (*total_width == 0 || *line_count == 0) {
		err_hw("[LME] output is NULL(total_width:%d,line_count:%d)", *total_width,
		       *line_count);
		return SET_ERROR;
	}

	return SET_SUCCESS;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_output_size);

void camerapp_hw_lme_get_mbmv_size(int *width, int *height)
{
	int divider_width = 16;
	int divider_height = 16;
	int bytes_per_pixel = 2;

	lme_info("[LME] size(%dx%d), divider(%dx%d)\n", *width, *height, divider_width,
		divider_height);

	*width = DIV_ROUND_UP(*width, divider_width) * (bytes_per_pixel);
	*height = DIV_ROUND_UP(*height, divider_height);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_mbmv_size);

int camerapp_hw_lme_dma_reset(struct pablo_mmio *pmio)
{
	u32 reset_count = 0;
	u32 temp;

	lme_dbg("[LME]\n");

	LME_SET_R(pmio, LME_R_TRANS_STOP_REQ, 0x1);

	while (1) {
		temp = LME_GET_R(pmio, LME_R_TRANS_STOP_REQ_RDY);
		if (temp == 1)
			break;
		if (reset_count > LME_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	lme_dbg("[LME] %s done.\n", __func__);

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_dma_reset);

u32 camerapp_hw_lme_sw_reset(struct pablo_mmio *pmio)
{
	u32 reset_count = 0;
	u32 temp;
	int ret = 0;

	lme_dbg("[LME]\n");

	ret = camerapp_hw_lme_dma_reset(pmio);
	if (ret) {
		err_hw("[LME] sw dma reset fail");
		return ret;
	}

	/* request to lme hw */
	LME_SET_F(pmio, LME_R_SW_RESET, LME_F_SW_RESET, 1);

	/* wait reset complete */
	while (1) {
		temp = LME_GET_R(pmio, LME_R_SW_RESET);
		if (temp == 0)
			break;
		if (reset_count > LME_TRY_COUNT)
			return reset_count;
		reset_count++;
	}

	lme_dbg("[LME] %s done.\n", __func__);

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_sw_reset);

void camerapp_hw_lme_set_clock(struct pablo_mmio *pmio, bool on)
{
	if (!lme_get_use_timeout_wa()) {
		lme_dbg("[LME] clock %s\n", on ? "on" : "off");
		LME_SET_F(pmio, LME_R_IP_PROCESSING, LME_F_IP_PROCESSING, on);
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_clock);

void camerapp_hw_lme_set_init(struct pablo_mmio *pmio)
{
#ifdef USE_VELOCE
	/* SYSMMU_D1_LME_S2 */
	void __iomem *reg;
	lme_dbg("[LME] S2MPU disable (SYSMMU_S0_LME_S2) \n");
	reg = ioremap(0x1D0C0054, 0x4);
	writel(0xFF, reg);
	iounmap(reg);
#endif

	lme_dbg("[LME]\n");

	LME_SET_F(pmio, LME_R_CMDQ_VHD_CONTROL, LME_F_CMDQ_VHD_VBLANK_QRUN_ENABLE, 1);
	LME_SET_F(pmio, LME_R_CMDQ_VHD_CONTROL, LME_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);
	LME_SET_F(pmio, LME_R_DEBUG_CLOCK_ENABLE, LME_F_DEBUG_CLOCK_ENABLE, 0);

	LME_SET_R(pmio, LME_R_C_LOADER_ENABLE, 1);
	LME_SET_R(pmio, LME_R_STAT_RDMACL_EN, 1);

	/* Interrupt group enable for one frame */
	LME_SET_F(pmio, LME_R_CMDQ_QUE_CMD_L, LME_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
		LME_INT_GRP_EN_MASK);
	/* 1: DMA preloading, 2: COREX, 3: APB Direct */
	LME_SET_F(pmio, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	LME_SET_R(pmio, LME_R_CMDQ_ENABLE, 1);

	if (lme_get_use_timeout_wa()) {
		LME_SET_R(pmio, LME_R_IP_PROCESSING, 1);
		LME_SET_R(pmio, LME_R_FORCE_INTERNAL_CLOCK, 1);
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_init);

void camerapp_hw_lme_dump(struct pablo_mmio *pmio)
{
	lme_info("[LME] SFR DUMP (v13.0)\n");
	is_hw_dump_regs(pmio, lme_regs, LME_REG_CNT);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_dump);

int camerapp_hw_lme_wait_idle(struct pablo_mmio *pmio)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	lme_dbg("[LME]\n");

	idle = LME_GET_F(pmio, LME_R_IDLENESS_STATUS, LME_F_IDLENESS_STATUS);
	int_all = LME_GET_R(pmio, LME_R_INT_REQ_INT0_STATUS);

	lme_dbg("[LME] idle status before disable (idle:%d, int1:0x%X)\n", idle, int_all);

	while (!idle) {
		idle = LME_GET_F(pmio, LME_R_IDLENESS_STATUS, LME_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= LME_TRY_COUNT) {
			err_hw("[LME] timeout waiting idle - disable fail");
#ifndef USE_VELOCE
			camerapp_hw_lme_dump(pmio);
#endif
			ret = -ETIME;
			break;
		}

#ifdef USE_VELOCE
		usleep_range(100, 101);
#else
		usleep_range(3, 4);
#endif
	};

	int_all = LME_GET_R(pmio, LME_R_INT_REQ_INT0_STATUS);

	lme_dbg("[LME] idle status after disable (idle:%d, int1:0x%X)\n", idle, int_all);

	return ret;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_wait_idle);

void camerapp_hw_lme_set_common(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");

	/* 0: RDMA, 1: OTF */
	LME_SET_R(pmio, LME_R_IP_USE_OTF_PATH_01, 0x0);
	LME_SET_R(pmio, LME_R_IP_USE_OTF_PATH_23, 0x0);
	LME_SET_R(pmio, LME_R_IP_USE_OTF_PATH_45, 0x0);
	LME_SET_R(pmio, LME_R_IP_USE_OTF_PATH_67, 0x0);

	/* 0: start frame asap, 1; start frame upon cinfifo vvalid rise */
	LME_SET_F(pmio, LME_R_IP_USE_CINFIFO_NEW_FRAME_IN, LME_F_IP_USE_CINFIFO_NEW_FRAME_IN, 0x0);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_common);

void camerapp_hw_lme_set_int_mask(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
	LME_SET_F(pmio, LME_R_INT_REQ_INT0_ENABLE, LME_F_INT_REQ_INT0_ENABLE, LME_INT_EN_MASK);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_int_mask);

void camerapp_hw_lme_set_secure_id(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
	/* Set Paramer Value - scenario 0: Non-secure,  1: Secure */
	LME_SET_F(pmio, LME_R_SECU_CTRL_SEQID, LME_F_SECU_CTRL_SEQID, 0x0);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_secure_id);

void camerapp_hw_lme_set_block_crc(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
	/* Nothing to config except AXI CRC */
	LME_SET_F(pmio, LME_R_AXICRC_SIPULME13P0P0_CNTX0_SEED_0,
		LME_F_AXICRC_SIPULME13P0P0_CNTX0_SEED_0, 0x37);
	LME_SET_F(pmio, LME_R_AXICRC_SIPULME13P0P0_CNTX0_SEED_0,
		LME_F_AXICRC_SIPULME13P0P0_CNTX0_SEED_1, 0x37);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_block_crc);

void camerapp_hw_lme_set_core(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
	camerapp_hw_lme_set_common(pmio);
	camerapp_hw_lme_set_int_mask(pmio);
	camerapp_hw_lme_set_secure_id(pmio);
	camerapp_hw_lme_set_block_crc(pmio);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_core);

void camerapp_hw_lme_set_initialization(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
	/* lme_dbg("[LME] mmio_base:[0x%08X]\n", pmio->mmio_base); */

	camerapp_hw_lme_set_clock(pmio, true);
	camerapp_hw_lme_set_init(pmio);
	camerapp_hw_lme_set_core(pmio);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_initialization);

int camerapp_hw_lme_rdma_init(struct pablo_mmio *pmio, struct lme_frame *s_frame,
	struct lme_mbmv *mbmv, bool enable, u32 id)
{
	u32 line_count, total_width;
	u32 val = 0;

	lme_dbg("[LME] enable(%d), id(%d), size(%dx%d)\n", enable, id, s_frame->width,
		s_frame->height);

	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_CACHE_IN_CLIENT_ENABLE, enable);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_CACHE_IN_GEOM_BURST_LENGTH, 0xf);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_CACHE_IN_BURST_ALIGNMENT, 0x0);

		LME_SET_R(pmio, LME_R_CACHE_IN_RDMAYIN_EN, enable);
		break;
	case LME_RDMA_CACHE_IN_1:
		break;
	case LME_RDMA_MBMV_IN: /* MBMV IN */
		lme_dbg("[LME]LME_RDMA_MBMV_IN\n");
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_CLIENT_ENABLE, enable);

		LME_SET_R(pmio, LME_R_MBMV_IN_RDMAYIN_EN, enable);

		if (enable == 0)
			return 0;

		total_width = mbmv->width;
		line_count = mbmv->height;
		lme_dbg("[LME]-total_width(%d->%d), line_count(%d)\n", total_width,
			mbmv->width_align, line_count);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BURST_LENGTH, 0x2);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LWIDTH, total_width);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_COUNT, line_count);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_TOTAL_WIDTH, mbmv->width_align);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_DIRECTION_HW, 0x1);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_LWIDTH, total_width);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_LINEGAP, 0x14);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PREGAP, 0x1);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_POSTGAP, 0x14);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PIXELGAP, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_STALLGAP, 0x1);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_PACKING, 0x8);

		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BPAD_SET, 0x0);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_IN_FRMT_BSHIFT_SET, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_MNM, val);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_FRMT_CH_MIX_1, 0x1);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_BURST_ALIGNMENT, 0x0);
		break;
	default:
		err_hw("[LME] invalid dma_id[%d]", id);
		break;
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_rdma_init);

int camerapp_hw_lme_wdma_init(struct pablo_mmio *pmio, struct lme_frame *s_frame,
	struct lme_mbmv *mbmv, bool enable, u32 id, enum lme_sps_mode sps_mode)
{
	u32 line_count, total_width;
	u32 val = 0;

	lme_dbg("[LME] enable(%d), id(%d), size(%dx%d), sps_mode(%d)\n", enable, id, s_frame->width,
		s_frame->height, sps_mode);

	switch (id) {
	case LME_WDMA_MV_OUT: /* MV OUT */
		lme_dbg("[LME]LME_WDMA_MV_OUT\n");
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_CLIENT_ENABLE, enable);

		LME_SET_R(pmio, LME_R_SPS_MV_OUT_WDMAMV_EN, enable);

		if (enable == 0)
			return 0;

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BURST_LENGTH, 0x3);

		camerapp_hw_lme_get_output_size(s_frame->width, s_frame->height, &total_width,
			&line_count, sps_mode, LME_WDMA_MV_OUT);

		lme_dbg("[LME]-total_width(%d), line_count(%d), stride(%d)\n", total_width,
			line_count, total_width);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH, total_width);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT, line_count);
		LME_SET_F(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_TOTAL_WIDTH,
			LME_F_SPS_MV_OUT_WDMAMV_IMG_STRIDE_1P, total_width);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_DIRECTION_HW, 0x1);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_PACKING, 16);

		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BPAD_SET, 0x4);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_BSHIFT_SET, 0x4);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_MNM, val);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_FRMT_CH_MIX_1, 0x1);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_BURST_ALIGNMENT, 0x0);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_SELF_HW_FLUSH_ENABLE, 0x0);
		break;

	case LME_WDMA_SAD_OUT: /* SAD OUT */
		lme_dbg("[LME]LME_WDMA_SAD_OUT\n");
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_CLIENT_ENABLE, enable);
		LME_SET_R(pmio, LME_R_SAD_OUT_WDMA_EN, enable);

		if (enable == 0)
			return 0;

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BURST_LENGTH, 0x3);

		camerapp_hw_lme_get_output_size(s_frame->width, s_frame->height, &total_width,
			&line_count, sps_mode, LME_WDMA_SAD_OUT);

		lme_dbg("[LME]-total_width(%d), line_count(%d), stride(%d)\n", total_width,
			line_count, total_width);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LWIDTH, total_width);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LINE_COUNT, line_count);
		LME_SET_F(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_TOTAL_WIDTH,
			LME_F_SAD_OUT_WDMA_IMG_STRIDE_1P, total_width);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_LINE_DIRECTION_HW, 0x1);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_PACKING, 24);

		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BPAD_SET, 0x8);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_SAD_OUT_FRMT_BSHIFT_SET, 0x8);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_FRMT_MNM, val);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_BURST_ALIGNMENT, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_SELF_HW_FLUSH_ENABLE, 0x0);
		break;

	case LME_WDMA_MBMV_OUT: /* MBMV OUT */
		lme_dbg("[LME]LME_WDMA_MBMV_OUT\n");
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_CLIENT_ENABLE, enable);

		LME_SET_R(pmio, LME_R_MBMV_OUT_WDMAMV_EN, enable);

		if (enable == 0)
			return 0;

		total_width = mbmv->width;
		line_count = mbmv->height;
		lme_dbg("[LME]-total_width(%d->%d), line_count(%d)\n", total_width,
			mbmv->width_align, line_count);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BURST_LENGTH, 0x3);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LWIDTH, total_width);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LINE_COUNT, line_count);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_TOTAL_WIDTH, mbmv->width_align);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_LINE_DIRECTION_HW, 0x1);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_PACKING, 0x8);

		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BPAD_SET, 0x0);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BPAD_TYPE, 0x0);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_OUT_FRMT_BSHIFT_SET, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_MNM, val);

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_CH_MIX_0, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_FRMT_CH_MIX_1, 0x1);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_BURST_ALIGNMENT, 0x0);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_CLIENT_FLUSH, 0);
		break;
	default:
		break;
	}
	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_wdma_init);

int camerapp_hw_lme_rdma_addr(struct pablo_mmio *pmio, dma_addr_t *addr, u32 id)
{
	int ret = SET_SUCCESS;
	int val = 0;

	lme_dbg("[LME]id:%d addr:%pad/%pad\n", id, &addr[0], &addr[1]);

	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		LME_SET_R(pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_LSB, DVA_36BIT_LOW(addr[0]));

		LME_SET_R(pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_MSB, DVA_36BIT_HIGH(addr[0]));
		break;
	case LME_RDMA_CACHE_IN_1:
		LME_SET_R(pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_1_LSB, DVA_36BIT_LOW(addr[0]));

		LME_SET_R(pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_1_MSB, DVA_36BIT_HIGH(addr[0]));
		break;
	case LME_RDMA_MBMV_IN:
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_EN_0, 1);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_EN_1, 1);
		val = LME_SET_V(
			pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_SIZE, 1);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_CONF, val);

		LME_SET_F(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB_0, DVA_36BIT_LOW(addr[0]));

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_0,
			DVA_36BIT_HIGH(addr[0]));

		LME_SET_F(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_LSB_1, DVA_36BIT_LOW(addr[1]));

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_1,
			DVA_36BIT_HIGH(addr[1]));
		break;
	default:
		err_hw("[LME] invalid dma_id[%d]", id);
		break;
	}

	return ret;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_rdma_addr);

int camerapp_hw_lme_wdma_addr(struct pablo_mmio *pmio, struct lme_frame *s_frame,
	struct lme_frame *d_frame, u32 id, struct lme_pre_control_params *pre_control_params,
	struct lme_mbmv *mbmv)
{
	int val = 0;
	u32 total_width = 0;
	u32 line_count = 0;
	u32 buffer_size = 0;
	enum lme_op_mode op_mode = pre_control_params->op_mode;
	enum lme_sps_mode sps_mode = pre_control_params->sps_mode;

	lme_dbg("[LME] id(%d), size(%dx%d), op_mode(%d)\n", id, s_frame->width, s_frame->height,
		op_mode);

	if (camerapp_hw_lme_get_output_size(s_frame->width, s_frame->height, &total_width,
					    &line_count, sps_mode, (enum lme_wdma_index)id)) {
		err_hw("[LME] get_output_size is failed(id:%d,size:%dx%d,op_mode:%d)\n", id,
		       s_frame->width, s_frame->height, op_mode);
		return SET_ERROR;
	}
	buffer_size = total_width * ((line_count == 0) ? 0 : (line_count - 1));

	switch (id) {
	case LME_WDMA_MV_OUT:
		d_frame->addr.actual_mv_out = d_frame->addr.mv_out + buffer_size;

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_LSB,
			DVA_36BIT_LOW(d_frame->addr.actual_mv_out));

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_0,
			DVA_36BIT_HIGH(d_frame->addr.actual_mv_out));
		break;
	case LME_WDMA_MBMV_OUT:
		if (op_mode == LME_OP_MODE_TNR) {
			if (mbmv->width_align == 0 || mbmv->height == 0) {
				err_hw("[LME] mbmv is NULL(%dx%d)\n", mbmv->width_align,
				       mbmv->height);
				return SET_ERROR;
			}
			total_width = mbmv->width_align;
			line_count = mbmv->height;
			buffer_size = total_width * ((line_count == 0) ? 0 : (line_count - 1));

			mbmv->actual_mbmv_out_0 = mbmv->dva_mbmv1;
			mbmv->actual_mbmv_out_1 = mbmv->dva_mbmv0 + buffer_size;
		} else {
			err_hw("[LME] NOT SUPPORT op_mode in DD -> need to add!");
		}

		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_EN_0, 1);
		val = LME_SET_V(pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_EN_1, 1);
		val = LME_SET_V(
			pmio, val, LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_ROTATION_SIZE, 1);
		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_CONF, val);

		LME_SET_F(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB_0,
			DVA_36BIT_LOW(mbmv->actual_mbmv_out_0));

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_0,
			(DVA_36BIT_HIGH(mbmv->actual_mbmv_out_0)));

		LME_SET_F(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB,
			LME_F_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_LSB_1,
			DVA_36BIT_LOW(mbmv->actual_mbmv_out_1));

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_MBMV_OUT_GEOM_BASE_ADDR_1,
			DVA_36BIT_HIGH(mbmv->actual_mbmv_out_1));
		break;
	case LME_WDMA_SAD_OUT:
		d_frame->addr.actual_sad_out = d_frame->addr.sad_out + buffer_size;

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BASE_ADDR_LSB,
			DVA_36BIT_LOW(d_frame->addr.actual_sad_out));

		LME_SET_R(pmio, LME_R_DMACLIENT_CNTX0_SAD_OUT_GEOM_BASE_ADDR_0,
			DVA_36BIT_HIGH(d_frame->addr.actual_sad_out));
		break;
	default:
		err_hw("[LME] invalid dma_id[%d]", id);
		return SET_ERROR;
	}

	lme_dbg("[LME] total_width(%d), line_count(%d), stride(%d), buffer_size(%d)\n", total_width,
		line_count, total_width, buffer_size);

	return SET_SUCCESS;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_wdma_addr);

void camerapp_hw_lme_set_cache(struct pablo_mmio *pmio, bool enable, u32 prev_width, u32 cur_height)
{
	u32 val = 0;
	u32 ignore_prefetch;
	u32 prev_pix_gain, cur_pix_gain;
	u32 prev_pix_offset, cur_pix_offset;

	lme_dbg("[LME] enable(%d), prev_width(%d), cur_height(%d)\n", enable, prev_width,
		cur_height);

	/* Common */
	enable = 1;
	LME_SET_F(pmio, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_Y_LME_BYPASS, !enable);

	ignore_prefetch = 0; /* use prefetch for performance */
	val = LME_SET_V(pmio, val, LME_F_CACHE_8BIT_IGNORE_PREFETCH, ignore_prefetch);
	val = LME_SET_V(pmio, val, LME_F_CACHE_8BIT_DATA_REQ_CNT_EN, 1);
	val = LME_SET_V(pmio, val, LME_F_CACHE_8BIT_PRE_REQ_CNT_EN, 1);
	val = LME_SET_V(pmio, val, LME_F_CACHE_8BIT_CACHE_CADDR_OFFSET, 0x8);
	LME_SET_R(pmio, LME_R_CACHE_8BIT_LME_BYPASS, val);

	prev_pix_gain = 0x40;
	prev_pix_offset = 0;

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_PRVCMGAIN, prev_pix_gain);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_PRVIMGHEIGHT, prev_pix_offset);
	LME_SET_R(pmio, LME_R_CACHE_8BIT_PIX_CONFIG_0, val);

	cur_pix_gain = 0x40;
	cur_pix_offset = 0;

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_CURCMGAIN, cur_pix_gain);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_CURCMOFFSET, cur_pix_offset);
	LME_SET_R(pmio, LME_R_CACHE_8BIT_PIX_CONFIG_1, val);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_cache);

void camerapp_hw_lme_cache_size(struct pablo_mmio *pmio, u32 prev_width, u32 prev_height,
	u32 cur_width, u32 cur_height, u32 stride)
{
	u32 val = 0;
	u32 prev_crop_start_x, prev_crop_start_y;
	u32 cur_crop_start_x, cur_crop_start_y;
	u32 prev_width_jump, cur_width_jump;

	lme_dbg("[LME] prev(%dx%d), cur(%dx%d), stride:%d\n", prev_width, prev_height, cur_width,
		cur_height, stride);

	/* previous frame */
	val = LME_SET_V(pmio, val, LME_F_Y_LME_PRVIMGWIDTH, prev_width);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_PRVIMGHEIGHT, prev_height);
	LME_SET_R(pmio, LME_R_CACHE_8BIT_IMAGE0_CONFIG, val);

	prev_crop_start_x = 0; /* Not use crop */
	prev_crop_start_y = 0;

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_PRVROISTARTX, prev_crop_start_x);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_PRVROISTARTY, prev_crop_start_y);
	LME_SET_R(pmio, LME_R_CACHE_8BIT_CROP_CONFIG_START_0, val);

	prev_width_jump = stride;
	LME_SET_R(pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_0, prev_width_jump);

	/* current frame */
	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_CURIMGWIDTH, cur_width);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_CURIMGHEIGHT, cur_height);
	LME_SET_R(pmio, LME_R_CACHE_8BIT_IMAGE1_CONFIG, val);

	/* Not use crop */
	cur_crop_start_x = 0;
	cur_crop_start_y = 0;

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_CURROISTARTX, cur_crop_start_x);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_CURROISTARTY, cur_crop_start_y);
	LME_SET_R(pmio, LME_R_CACHE_8BIT_CROP_CONFIG_START_1, val);

	cur_width_jump = stride;
	LME_SET_R(pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_JUMP_1, cur_width_jump);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_cache_size);

void camerapp_hw_lme_mvct_size(struct pablo_mmio *pmio, u32 width, u32 height)
{
	u32 val = 0;
	u32 prefetch_gap;
	u32 prefetch_en;
	const u32 cell_width = 16;

	lme_dbg("[LME]\n");
	prefetch_gap = DIV_ROUND_UP(width * 8 / 100, cell_width);
	prefetch_en = 1;
	lme_dbg("[LME] width(%d), cell_width(%d), prefetch_gap(%d)\n", width, cell_width,
		prefetch_gap);

	val = LME_SET_V(pmio, val, LME_F_MVCT_8BIT_LME_PREFETCH_GAP, prefetch_gap);
	val = LME_SET_V(pmio, val, LME_F_MVCT_8BIT_LME_PREFETCH_EN, prefetch_en);
	LME_SET_R(pmio, LME_R_MVCT_8BIT_LME_PREFETCH, val);

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_ROISIZEX, width);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_ROISIZEY, height);
	LME_SET_R(pmio, LME_R_MVCT_8BIT_IMAGE_DIMENTIONS, val);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_mvct_size);

void camerapp_hw_lme_set_mvct(struct pablo_mmio *pmio)
{
	u32 val = 0;
	const u32 sr_x = 128; /* search range: 16 ~ 128 */
	const u32 sr_y = 128;

	lme_dbg("[LME]\n");
	/* 0: fusion, 1: TNR */
	val = LME_SET_V(pmio, val, LME_F_Y_LME_MODE, 0x1);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_FIRSTFRAME, 0x0);
	val = LME_SET_V(pmio, val, LME_F_MVCT_8BIT_LME_FW_FRAME_ONLY, 0x0);
	LME_SET_R(pmio, LME_R_MVCT_8BIT_LME_CONFIG, val);

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_USEAD, 0x1);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_USESAD, 0x0);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_USECT, 0x1);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_USEZSAD, 0x1);
	LME_SET_R(pmio, LME_R_MVCT_8BIT_MVE_CONFIG, val);

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_WEIGHTCT, 1);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_WEIGHTAD, 5);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_WEIGHTSAD, 1);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_WEIGHTZSAD, 1);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_NOISELEVEL, 3);
	LME_SET_R(pmio, LME_R_MVCT_8BIT_MVE_WEIGHT, val);

	val = 0;
	val = LME_SET_V(pmio, val, LME_F_Y_LME_MPSSRCHRANGEX, sr_x);
	val = LME_SET_V(pmio, val, LME_F_Y_LME_MPSSRCHRANGEY, sr_y);
	LME_SET_R(pmio, LME_R_MVCT_8BIT_MV_SR, val);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_mvct);

void camerapp_hw_lme_set_first_frame(struct pablo_mmio *pmio, bool first_frame)
{
	lme_dbg("[LME] first_frame(%d)\n", first_frame);

	LME_SET_F(pmio, LME_R_MVCT_8BIT_LME_CONFIG, LME_F_Y_LME_FIRSTFRAME, first_frame);

	if (lme_get_use_timeout_wa() || first_frame) {
		LME_SET_F(pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET, 0);
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_first_frame);

void camerapp_hw_lme_set_sps_out_mode(struct pablo_mmio *pmio, enum lme_sps_mode sps_mode)
{
	lme_dbg("[LME] sps_mode(%d)\n", sps_mode);
	LME_SET_F(pmio, LME_R_MVCT_8BIT_LME_CONFIG, LME_F_Y_LME_LME_8X8SEARCH, sps_mode);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_sps_out_mode);

void camerapp_hw_lme_set_crc(struct pablo_mmio *pmio, u32 seed)
{
	lme_dbg("[LME] seed(%d)\n", seed);
	LME_SET_F(pmio, LME_R_AXICRC_SIPULME13P0P0_CNTX0_SEED_0,
		LME_F_AXICRC_SIPULME13P0P0_CNTX0_SEED_0, seed);
	LME_SET_F(pmio, LME_R_AXICRC_SIPULME13P0P0_CNTX0_SEED_1,
		LME_F_AXICRC_SIPULME13P0P0_CNTX0_SEED_1, seed);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_crc);

void camerapp_hw_lme_set_block_bypass(struct pablo_mmio *pmio)
{
	lme_dbg("[LME]\n");
	LME_SET_F(pmio, LME_R_CACHE_8BIT_LME_BYPASS, LME_F_Y_LME_BYPASS, 0x0);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_block_bypass);

void camerapp_lme_sfr_dump(void __iomem *base_addr)
{
	u32 i;
	u32 reg_value;

	lme_info("[LME] v13.0\n");
	for (i = 0; i < LME_REG_CNT; i++) {
		reg_value = readl(base_addr + lme_regs[i].sfr_offset);
		pr_info("[@][SFR][DUMP] reg:[%s][0x%04X], value:[0x%08X]\n", lme_regs[i].reg_name,
			lme_regs[i].sfr_offset, reg_value);
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_lme_sfr_dump);

u32 camerapp_hw_lme_get_reg_cnt(void)
{
	return LME_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_get_reg_cnt);

void camerapp_hw_lme_init_pmio_config(struct lme_dev *lme)
{
	struct pmio_config *cfg = &lme->pmio_config;

	lme_dbg("[LME]\n");

	cfg->name = "lme";
	cfg->mmio_base = lme->regs_base;

	cfg->num_corexs = 2;
	cfg->corex_stride = 0x8000;

	cfg->volatile_table = &lme_volatile_table;
	cfg->wr_noinc_table = &lme_wr_noinc_table;

	cfg->max_register = LME_R_Y_LME_SADOUT_CRC;
	cfg->num_reg_defaults_raw = (LME_R_Y_LME_SADOUT_CRC >> 2) + 1;
	cfg->phys_base = lme->regs_rsc->start;
	cfg->dma_addr_shift = 4;

	cfg->ranges = lme_range_cfgs;
	cfg->num_ranges = ARRAY_SIZE(lme_range_cfgs);

	cfg->fields = lme_field_descs;
	cfg->num_fields = ARRAY_SIZE(lme_field_descs);

	if (lme_get_use_timeout_wa())
		cfg->cache_type = PMIO_CACHE_NONE;
	else
		cfg->cache_type = PMIO_CACHE_FLAT;

	lme_info("[LME] use_timeout_wa(%d), pmio cache_type(%s)", lme_get_use_timeout_wa(),
		 cfg->cache_type == PMIO_CACHE_FLAT ?
			 "PMIO_CACHE_FLAT" :
			 (cfg->cache_type == PMIO_CACHE_NONE ? "PMIO_CACHE_NONE" : "UNKNOWN"));

	lme->pmio_en = true;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_init_pmio_config);

void camerapp_hw_lme_update_block_reg(struct pablo_mmio *pmio, struct lme_frame *s_frame)
{
	u32 cache_en = 1;
	u32 prev_width = s_frame->width;
	u32 cur_height = s_frame->height;

	lme_dbg("[LME]\n");
	camerapp_hw_lme_set_cache(pmio, cache_en, prev_width, cur_height);
	camerapp_hw_lme_set_block_bypass(pmio);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_update_block_reg);

void camerapp_hw_lme_set_size_regs(struct pablo_mmio *pmio, struct lme_frame *s_frame,
	struct lme_pre_control_params *pre_control_params,
	struct lme_post_control_params *post_control_params)
{
	u32 prev_width, prev_height;
	u32 cur_width, cur_height;

	lme_dbg("[LME] is_first:%d\n", post_control_params->is_first);

	prev_width = s_frame->width;
	prev_height = s_frame->height;
	cur_width = s_frame->width;
	cur_height = s_frame->height;

	camerapp_hw_lme_cache_size(
		pmio, prev_width, prev_height, cur_width, cur_height, s_frame->stride);
	camerapp_hw_lme_mvct_size(pmio, cur_width, cur_height);

	camerapp_hw_lme_set_first_frame(pmio, post_control_params->is_first);

	camerapp_hw_lme_set_sps_out_mode(pmio, pre_control_params->sps_mode);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_size_regs);

void lme_print_rdma_info(struct lme_frame *s_frame, struct lme_mbmv *mbmv)
{
	lme_info("cache_in: %pad, %pad\n", &s_frame->addr.actual_cache_in_0,
		&s_frame->addr.actual_cache_in_1);
	lme_info("mbmv_in: %pad, %pad\n", &mbmv->actual_mbmv_in_0, &mbmv->actual_mbmv_in_1);
}

void lme_print_wdma_info(struct lme_frame *d_frame, struct lme_mbmv *mbmv)
{
	lme_info("mv_out: %pad, sad_out: %pad\n", &d_frame->addr.actual_mv_out,
		&d_frame->addr.actual_sad_out);
	lme_info("mbmv_out: %pad, %pad\n", &mbmv->actual_mbmv_out_0, &mbmv->actual_mbmv_out_1);
}

void camerapp_hw_lme_print_dma_address(
	struct lme_frame *s_frame, struct lme_frame *d_frame, struct lme_mbmv *mbmv)
{
	lme_print_rdma_info(s_frame, mbmv);
	lme_print_wdma_info(d_frame, mbmv);
}

int camerapp_hw_lme_set_rdma(struct pablo_mmio *pmio, struct lme_frame *s_frame,
	struct lme_pre_control_params *pre_control_params,
	struct lme_post_control_params *post_control_params, struct lme_mbmv *mbmv, u32 id)
{
	dma_addr_t *input_dva = NULL;
	u32 cmd = DMA_INPUT_COMMAND_ENABLE;
	dma_addr_t mbmv_dva[2] = { 0, 0 };
	u32 total_width, line_count, buffer_size;
	enum lme_scenario scenario = pre_control_params->scenario;
	enum lme_op_mode op_mode = pre_control_params->op_mode;
	bool is_first = post_control_params->is_first;

	lme_dbg("[LME]\n");
	switch (id) {
	case LME_RDMA_CACHE_IN_0:
		if (scenario == LME_SCENARIO_REPROCESSING) {
			s_frame->addr.actual_cache_in_0 = s_frame->addr.curr_in;
		} else {
			s_frame->addr.actual_cache_in_0 = s_frame->addr.prev_in;
		}
		input_dva = &s_frame->addr.actual_cache_in_0;
		lme_dbg("[LME] scenario: %d, cache_in_0: %pad\n", scenario,
			&s_frame->addr.actual_cache_in_0);
		break;
	case LME_RDMA_CACHE_IN_1:
		if (scenario == LME_SCENARIO_REPROCESSING) {
			s_frame->addr.actual_cache_in_1 = s_frame->addr.prev_in;
		} else {
			s_frame->addr.actual_cache_in_1 = s_frame->addr.curr_in;
		}
		input_dva = &s_frame->addr.actual_cache_in_1;
		lme_dbg("[LME] scenario: %d, cache_in_1: %pad\n", scenario,
			&s_frame->addr.actual_cache_in_1);
		break;
	case LME_RDMA_MBMV_IN:
		input_dva = mbmv_dva;
		if (op_mode == LME_OP_MODE_TNR) {
			total_width = mbmv->width_align;
			line_count = mbmv->height;
			buffer_size = total_width * ((line_count == 0) ? 0 : (line_count - 1));

			if (lme_get_use_timeout_wa() && !is_first) {
				mbmv->actual_mbmv_in_0 = mbmv->dva_mbmv0;
				mbmv->actual_mbmv_in_1 = mbmv->dva_mbmv1 + buffer_size;
			} else {
				mbmv->actual_mbmv_in_0 = mbmv->dva_mbmv1 + buffer_size;
				mbmv->actual_mbmv_in_1 = mbmv->dva_mbmv0;
			}
			input_dva[0] = mbmv->actual_mbmv_in_0;
			input_dva[1] = mbmv->actual_mbmv_in_1;
			lme_dbg("[LME] use_timeout_wa:%d, is_first:%d, mbmv0:%pad, mbmv1:%pad\n",
				lme_get_use_timeout_wa(), is_first, &mbmv->actual_mbmv_in_0,
				&mbmv->actual_mbmv_in_1);
		} else {
			err_hw("[LME] not supprt op_mode(%d)", op_mode);
		}
		break;
	default:
		err_hw("[LME] invalid ID (%d)", id);
		return -EINVAL;
	}

	if (camerapp_hw_lme_rdma_init(pmio, s_frame, mbmv, cmd, id)) {
		err_hw("[LME] failed to initialize LME_RDMA(%d)", id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		if (camerapp_hw_lme_rdma_addr(pmio, input_dva, id)) {
			err_hw("[LME] failed to set LME_RDMA(%d) address", id);
			return -EINVAL;
		}
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_rdma);

int camerapp_hw_lme_set_wdma(struct pablo_mmio *pmio, struct lme_frame *s_frame,
	struct lme_frame *d_frame, struct lme_pre_control_params *pre_control_params,
	struct lme_mbmv *mbmv, u32 id)
{
	u32 cmd = DMA_INPUT_COMMAND_ENABLE;
	int ret = 0;
	enum lme_sps_mode sps_mode;

	lme_dbg("[LME]\n");
	sps_mode = pre_control_params->sps_mode;
	ret = camerapp_hw_lme_wdma_init(pmio, s_frame, mbmv, cmd, id, sps_mode);
	if (ret) {
		err_hw("[LME] failed to initializ LME_WDMA(%d)", id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = camerapp_hw_lme_wdma_addr(
			pmio, s_frame, d_frame, id, pre_control_params, mbmv);
		if (ret) {
			err_hw("[LME] failed to set LME_WDMA(%d) address", id);
			return -EINVAL;
		}
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_wdma);

/* modify : pablo_hw_helper_set_rta_regs in pablo-hw-helper-v2.c */
int camerapp_hw_lme_set_rta_regs(struct pablo_mmio *pmio, struct lme_dev *lme)
{
	struct lme_ctx *ctx;
	struct size_cr_set *scs;
	int i = 0;

	lme_dbg("[LME]");
	ctx = lme->current_ctx;
	scs = (struct size_cr_set *)ctx->kvaddr[LME_BUF_CR];
	if (!scs) {
		err_hw("[LME] cr_set is NULL");
		return -EINVAL;
	}

	if (scs->size == 0 || scs->size > MAX_CR_SET) {
		err_hw("[LME] size of cr_set is out of range (%d)", scs->size);
		return -EINVAL;
	}

	lme_dbg("[LME] scs->size(%d)\n", scs->size);
	for (i = 0; i < scs->size; i++) {
		lme_dbg("[LME][%d] addr=0x%X, data=0x%X", i, scs->cr[i].reg_addr,
			scs->cr[i].reg_data);
		/* set reg operation, insead of helper sync */
		LME_SET_R(pmio, scs->cr[i].reg_addr, scs->cr[i].reg_data);
	}

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_set_rta_regs);

int camerapp_hw_lme_update_param(
	struct pablo_mmio *pmio, struct lme_dev *lme, struct c_loader_buffer *clb)
{
	struct lme_frame *s_frame, *d_frame;
	struct lme_pre_control_params *pre_control_params;
	struct lme_post_control_params *post_control_params;
	struct lme_mbmv *mbmv;
	const struct lme_size_limit *limit;
	int ret_set_rta_regs = 0;
	int ret = 0;
	int i = 0;

	lme_dbg("[LME]\n");
	limit = &lme->variant->limit_input;
	s_frame = &lme->current_ctx->s_frame;
	d_frame = &lme->current_ctx->d_frame;
	pre_control_params = &lme->current_ctx->pre_control_params;
	post_control_params = &lme->current_ctx->post_control_params;
	mbmv = &lme->current_ctx->mbmv;

	if ((s_frame->width != post_control_params->curr_roi_width) ||
		(s_frame->height != post_control_params->curr_roi_height)) {
		lme_info("[LME] update (w:%d,h:%d,s:%d) -> (w:%d,h:%d,s:%d)\n", s_frame->width,
			s_frame->height, s_frame->stride, post_control_params->curr_roi_width,
			post_control_params->curr_roi_height, post_control_params->curr_roi_stride);

		if ((limit->max_w < post_control_params->curr_roi_width) ||
			(limit->max_h < post_control_params->curr_roi_height)) {
			err_hw("[LME] wrong update: too large! max size(%dx%d) < update size(%dx%d)",
				limit->max_w, limit->max_h, post_control_params->curr_roi_width,
				post_control_params->curr_roi_height);
			return -EINVAL;
		}

		if ((limit->min_w > post_control_params->curr_roi_width) ||
			(limit->min_h > post_control_params->curr_roi_height)) {
			err_hw("[LME] wrong update: too small! min size(%dx%d) > update size(%dx%d)",
				limit->min_w, limit->min_h, post_control_params->curr_roi_width,
				post_control_params->curr_roi_height);
			return -EINVAL;
		}

		s_frame->width = post_control_params->curr_roi_width;
		s_frame->height = post_control_params->curr_roi_height;
		s_frame->stride = post_control_params->curr_roi_stride;

		/* update mbmv info */
		mbmv->width = s_frame->width;
		mbmv->height = s_frame->height;
		camerapp_hw_lme_get_mbmv_size(&(mbmv->width), &(mbmv->height));
		mbmv->width_align = ALIGN(mbmv->width, 16);

		lme_info("[LME] input size(%dx%d) -> mbmv(%d(->%d)x%d)\n", s_frame->width,
			s_frame->height, mbmv->width, mbmv->width_align, mbmv->height);
	}

	/* defense code */
	if (!s_frame->stride)
		s_frame->stride = s_frame->width;

	camerapp_hw_lme_update_block_reg(pmio, s_frame);

	ret_set_rta_regs = camerapp_hw_lme_set_rta_regs(pmio, lme);
	if (ret_set_rta_regs) {
		lme_info("[LME] there is no info in cr (%d) -> default setting.\n",
			ret_set_rta_regs);
		camerapp_hw_lme_set_mvct(pmio);
	}

	camerapp_hw_lme_set_size_regs(pmio, s_frame, pre_control_params, post_control_params);

	for (i = LME_RDMA_CACHE_IN_0; i < LME_RDMA_MAX; i++) {
		ret = camerapp_hw_lme_set_rdma(
			pmio, s_frame, pre_control_params, post_control_params, mbmv, i);
		if (ret) {
			err_hw("[LME] camerapp_hw_lme_set_rdma is fail");
			return ret;
		}
	}

	if (lme_get_debug_level())
		lme_print_rdma_info(s_frame, mbmv);

	for (i = LME_WDMA_MV_OUT; i < LME_WDMA_MAX; i++) {
		ret = camerapp_hw_lme_set_wdma(pmio, s_frame, d_frame, pre_control_params, mbmv, i);
		if (ret) {
			err_hw("[LME] camerapp_hw_lme_set_wdma is fail");
			return ret;
		}
	};

	if (lme_get_debug_level())
		lme_print_wdma_info(d_frame, mbmv);

	return ret;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_lme_update_param);

static struct pablo_camerapp_hw_lme hw_lme_ops = {
	.sw_reset = camerapp_hw_lme_sw_reset,
	.wait_idle = camerapp_hw_lme_wait_idle,
	.set_initialization = camerapp_hw_lme_set_initialization,
	.update_param = camerapp_hw_lme_update_param,
	.start = camerapp_hw_lme_start,
	.sfr_dump = camerapp_lme_sfr_dump,
};

struct pablo_camerapp_hw_lme *pablo_get_hw_lme_ops(void)
{
	return &hw_lme_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_hw_lme_ops);
