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

#include "pablo-hw-api-dof.h"
#include "is-hw-common-dma.h"
#include "is-hw-control.h"

#include "pablo-hw-reg-dof-v13_0_0.h"
#include "pmio.h"

#define DOF_SET_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define DOF_SET_R(base, R, val) PMIO_SET_R(base, R, val)
#define DOF_SET_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)
#define DOF_GET_F(base, R, F) PMIO_GET_F(base, R, F)
#define DOF_GET_R(base, R) PMIO_GET_R(base, R)

static const struct dof_variant dof_variant[] = {
	{
		.limit_input = {
			.min_w		= 64,
			.min_h		= 64,
			.max_w		= 1024,
			.max_h		= 768,
		},
		.version		= IP_VERSION,
	},
};

const struct dof_variant *camerapp_hw_dof_get_size_constraints(struct pablo_mmio *pmio)
{
	return dof_variant;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_get_size_constraints);

u32 camerapp_hw_dof_get_ver(struct pablo_mmio *pmio)
{
	return DOF_GET_R(pmio, DOF_R_IP_VERSION);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_get_ver);

void camerapp_hw_dof_set_cmdq(struct pablo_mmio *pmio, dma_addr_t clh, u32 noh)
{
	dof_dbg("[DOF] clh(%llu), noh(%d)\n", clh, noh);

	if (clh && noh) {
		DOF_SET_F(pmio, DOF_R_CMDQ_QUE_CMD_H, DOF_F_CMDQ_QUE_CMD_BASE_ADDR,
			DVA_36BIT_HIGH(clh));
		DOF_SET_F(pmio, DOF_R_CMDQ_QUE_CMD_M, DOF_F_CMDQ_QUE_CMD_HEADER_NUM, noh);
		DOF_SET_F(pmio, DOF_R_CMDQ_QUE_CMD_M, DOF_F_CMDQ_QUE_CMD_SETTING_MODE, 1);
	} else {
		DOF_SET_F(pmio, DOF_R_CMDQ_QUE_CMD_M, DOF_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	}

	DOF_SET_R(pmio, DOF_R_CMDQ_ADD_TO_QUEUE_0, 1);
	DOF_SET_R(pmio, DOF_R_CMDQ_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_set_cmdq);

void camerapp_hw_dof_start(struct pablo_mmio *pmio, struct c_loader_buffer *clb)
{
	dof_dbg("[DOF]\n");
	camerapp_hw_dof_set_cmdq(pmio, clb->header_dva, clb->num_of_headers);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_start);

void camerapp_hw_dof_stop(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_stop);

void camerapp_hw_dof_clear_intr_all(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");
	DOF_SET_F(pmio, DOF_R_INT_REQ_INT0_CLEAR, DOF_F_INT_REQ_INT0_CLEAR, DOF_INT_EN_MASK);
	/* INT1 is enabled if it necessary */
	/* DOF_SET_F(pmio, DOF_R_INT_REQ_INT1_CLEAR, DOF_F_INT_REQ_INT1_CLEAR, DOF_INT1_EN); */
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_clear_intr_all);

u32 camerapp_hw_dof_get_intr_status_and_clear(struct pablo_mmio *pmio)
{
	u32 int0_status, int1_status;

	dof_dbg("[DOF]\n");
	int0_status = DOF_GET_R(pmio, DOF_R_INT_REQ_INT0);
	DOF_SET_R(pmio, DOF_R_INT_REQ_INT0_CLEAR, int0_status);

	int1_status = DOF_GET_R(pmio, DOF_R_INT_REQ_INT1);
	DOF_SET_R(pmio, DOF_R_INT_REQ_INT1_CLEAR, int1_status);

	dof_dbg("[DOF]int0(0x%x), int1(0x%x)\n", int0_status, int1_status);

	return int0_status;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_get_intr_status_and_clear);

u32 camerapp_hw_dof_get_int_frame_start(void)
{
	return DOF_INT_FRAME_START;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_get_int_frame_start);

u32 camerapp_hw_dof_get_int_frame_end(void)
{
	return DOF_INT_FRAME_END;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_get_int_frame_end);

u32 camerapp_hw_dof_get_int_err(void)
{
	return DOF_INT_ERR;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_get_int_err);

u32 camerapp_hw_dof_dma_reset(struct pablo_mmio *pmio)
{
	u32 reset_count = 0;
	u32 temp;

	dof_dbg("[DOF]\n");

	DOF_SET_R(pmio, DOF_R_TRANS_STOP_REQ, 0x1);

	while (1) {
		temp = DOF_GET_R(pmio, DOF_R_TRANS_STOP_REQ_RDY);
		if (temp == 1) {
			dof_dbg("[DOF] %s done.\n", __func__);
			return 0;
		}
		if (reset_count > DOF_TRY_COUNT)
			return reset_count;
		reset_count++;
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_dma_reset);

u32 camerapp_hw_dof_core_reset(struct pablo_mmio *pmio)
{
	u32 reset_count = 0;
	u32 temp;

	dof_dbg("[DOF]\n");

	/* request to dof hw */
	DOF_SET_F(pmio, DOF_R_SW_RESET, DOF_F_SW_RESET, 1);

	/* wait reset complete */
	while (1) {
		temp = DOF_GET_R(pmio, DOF_R_SW_RESET);
		if (temp == 0) {
			dof_dbg("[DOF] %s done.\n", __func__);
			return 0;
		}
		if (reset_count > DOF_TRY_COUNT)
			return reset_count;
		reset_count++;
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_core_reset);

u32 camerapp_hw_dof_sw_reset(struct pablo_mmio *pmio)
{
	int ret_dma, ret_core;

	dof_dbg("[DOF]\n");
	ret_dma = camerapp_hw_dof_dma_reset(pmio);
	if (ret_dma)
		dof_info("[DOF][ERR] sw dma reset fail(%d)", ret_dma);

	ret_core = camerapp_hw_dof_core_reset(pmio);
	if (ret_core)
		dof_info("[DOF][ERR] sw core reset fail(%d)", ret_core);

	return ret_dma + ret_core;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_sw_reset);

void camerapp_hw_dof_set_clock(struct pablo_mmio *pmio, bool on)
{
	dof_dbg("[DOF] clock %s\n", on ? "on" : "off");
	DOF_SET_F(pmio, DOF_R_IP_PROCESSING, DOF_F_IP_PROCESSING, on);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_set_clock);

void camerapp_hw_dof_set_init(struct pablo_mmio *pmio)
{
#ifdef USE_VELOCE
	/* SYSMMU_S0_DOF_S2 */
	void __iomem *reg;

	dof_dbg("[DOF][VELOCE] S2MPU disable (SYSMMU_S0_DOF_S2)\n");
	reg = ioremap(0x1F0C0054, 0x4);
	writel(0xFF, reg);
	iounmap(reg);
#endif

	dof_dbg("[DOF]\n");
	DOF_SET_F(pmio, DOF_R_CMDQ_VHD_CONTROL, DOF_F_CMDQ_VHD_VBLANK_QRUN_ENABLE, 1);
	DOF_SET_F(pmio, DOF_R_CMDQ_VHD_CONTROL, DOF_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);
	DOF_SET_F(pmio, DOF_R_DEBUG_CLOCK_ENABLE, DOF_F_DEBUG_CLOCK_ENABLE, 0); /* for debugging */

	DOF_SET_R(pmio, DOF_R_C_LOADER_ENABLE, 1);
	DOF_SET_R(pmio, DOF_R_STAT_RDMACL_EN, 1);

	/* Interrupt group enable for one frame */
	DOF_SET_F(pmio, DOF_R_CMDQ_QUE_CMD_L, DOF_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE,
		DOF_INT_GRP_EN_MASK);
	/* 1: DMA preloading, 2: COREX, 3: APB Direct */
	DOF_SET_F(pmio, DOF_R_CMDQ_QUE_CMD_M, DOF_F_CMDQ_QUE_CMD_SETTING_MODE, 3);
	DOF_SET_R(pmio, DOF_R_CMDQ_ENABLE, 1);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_set_init);

int camerapp_hw_dof_wait_idle(struct pablo_mmio *pmio)
{
	int ret = SET_SUCCESS;
	u32 idle;
	u32 int_all;
	u32 try_cnt = 0;

	dof_dbg("[DOF]\n");
	idle = DOF_GET_F(pmio, DOF_R_IDLENESS_STATUS, DOF_F_IDLENESS_STATUS);
	int_all = DOF_GET_R(pmio, DOF_R_INT_REQ_INT0_STATUS);

	dof_info("[DOF] idle status before disable (idle:%d, int1:0x%X)\n", idle, int_all);

	while (!idle) {
		idle = DOF_GET_F(pmio, DOF_R_IDLENESS_STATUS, DOF_F_IDLENESS_STATUS);

		try_cnt++;
		if (try_cnt >= DOF_TRY_COUNT) {
			err_hw("[DOF] timeout waiting idle - disable fail");
#ifndef USE_VELOCE
			camerapp_hw_dof_sfr_dump(pmio);
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

	int_all = DOF_GET_R(pmio, DOF_R_INT_REQ_INT0_STATUS);

	dof_info("[DOF] idle status after disable (idle:%d, int1:0x%X)\n", idle, int_all);

	return ret;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_wait_idle);

static void camerapp_hw_dof_set_common(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");
	/* 0: RDMA, 1: OTF */
	DOF_SET_R(pmio, DOF_R_IP_USE_OTF_PATH_01, 0x0);
	DOF_SET_R(pmio, DOF_R_IP_USE_OTF_PATH_23, 0x0);
	DOF_SET_R(pmio, DOF_R_IP_USE_OTF_PATH_45, 0x0);
	DOF_SET_R(pmio, DOF_R_IP_USE_OTF_PATH_67, 0x0);

	/* 0: start frame asap, 1; start frame upon cinfifo vvalid rise */
	DOF_SET_F(pmio, DOF_R_IP_USE_CINFIFO_NEW_FRAME_IN, DOF_F_IP_USE_CINFIFO_NEW_FRAME_IN, 0x0);
}

static void camerapp_hw_dof_set_int_mask(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");
	DOF_SET_F(pmio, DOF_R_INT_REQ_INT0_ENABLE, DOF_F_INT_REQ_INT0_ENABLE, DOF_INT_EN_MASK);
}

static void camerapp_hw_dof_set_secure_id(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");
	/* Set Paramer Value - scenario 0: Non-secure,  1: Secure */
	DOF_SET_F(pmio, DOF_R_SECU_CTRL_SEQID, DOF_F_SECU_CTRL_SEQID, 0x0);
}

static void camerapp_hw_dof_set_block_crc(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");

	/* This mask register default value is 00001000 = 0x00000008 */
	DOF_SET_F(pmio, DOF_R_CFG_MASK, DOF_F_CFG_BUFFER_MASK_CRC, 0x8);
	DOF_SET_F(pmio, DOF_R_CFG_MASK, DOF_F_CFG_MASK_STREAM_STATISTICS, 0x0);
}

void camerapp_hw_dof_set_core(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");
	camerapp_hw_dof_set_common(pmio);
	camerapp_hw_dof_set_int_mask(pmio);
	camerapp_hw_dof_set_secure_id(pmio);
	camerapp_hw_dof_set_block_crc(pmio);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_set_core);

void camerapp_hw_dof_set_initialization(struct pablo_mmio *pmio)
{
	dof_dbg("[DOF]\n");
	camerapp_hw_dof_set_clock(pmio, true);
	camerapp_hw_dof_set_init(pmio);
	camerapp_hw_dof_set_core(pmio);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_set_initialization);

void camerapp_hw_dof_sfr_dump(struct pablo_mmio *pmio)
{
	void __iomem *base_addr;
	u32 reg_value;
	u32 i;

	base_addr = pmio->mmio_base;
	for (i = 0; i < DOF_REG_CNT; i++) {
		reg_value = readl(base_addr + dof_regs[i].sfr_offset);
		pr_info("[@][SFR][DUMP] reg:[%s][0x%04X], value:[0x%08X]\n", dof_regs[i].reg_name,
			dof_regs[i].sfr_offset, reg_value);
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_sfr_dump);

void camerapp_hw_dof_update_debug_info(struct pablo_mmio *pmio, struct dof_debug_info *debug_info,
	u32 buf_index, enum dof_debug_status status)
{
	void __iomem *base_addr;
	int i;

	dof_dbg("[DOF]\n");
	base_addr = pmio->mmio_base;
	debug_info->buffer_index = buf_index;
	debug_info->device_status = status;
	for (i = 0; i < debug_info->regs.num_reg; i++) {
		/* range check : range for SNP_TOP */
		if ((debug_info->regs.reg_data[i].addr < DOF_R_CFG_BYPASS_PREFETCH) ||
			(debug_info->regs.reg_data[i].addr > DOF_R_SRC_VERSION))
			continue;

		debug_info->regs.reg_data[i].value =
			readl(base_addr + debug_info->regs.reg_data[i].addr);
	}

	dof_dbg("[DOF] print debug register (num:%d)\n", debug_info->regs.num_reg);
	for (i = 0; i < debug_info->regs.num_reg; i++) {
		pr_info("[@][DOF][%02d] reg:[0x%04X], value:[0x%08X]\n", i,
			debug_info->regs.reg_data[i].addr, debug_info->regs.reg_data[i].value);
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_update_debug_info);

u32 camerapp_hw_dof_get_reg_cnt(void)
{
	return DOF_REG_CNT;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_get_reg_cnt);

void camerapp_hw_dof_init_pmio_config(struct dof_dev *dof)
{
	struct pmio_config *pcfg;

	dof_dbg("[DOF]\n");
	pcfg = &dof->pmio_config;
	pcfg->name = "dof";
	pcfg->mmio_base = dof->regs_base;

	pcfg->volatile_table = &dof_volatile_table;
	pcfg->wr_noinc_table = &dof_wr_noinc_table;

	pcfg->max_register = DOF_R_IP_CFG;
	pcfg->num_reg_defaults_raw = (DOF_R_IP_CFG >> 2) + 1;
	pcfg->phys_base = dof->regs_rsc->start;
	pcfg->dma_addr_shift = 4;

	pcfg->ranges = dof_range_cfgs;
	pcfg->num_ranges = ARRAY_SIZE(dof_range_cfgs);

	pcfg->fields = dof_field_descs;
	pcfg->num_fields = ARRAY_SIZE(dof_field_descs);

	pcfg->cache_type = PMIO_CACHE_FLAT;

	dof_info("[DOF] pmio cache_type(%d:%s)", pcfg->cache_type,
		pcfg->cache_type == PMIO_CACHE_FLAT ?
			"PMIO_CACHE_FLAT" :
			(pcfg->cache_type == PMIO_CACHE_NONE ? "PMIO_CACHE_NONE" : "UNKNOWN"));
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_init_pmio_config);

void camerapp_hw_dof_update_block_reg(struct pablo_mmio *pmio, struct dof_dev *dof)
{
	int debug_rdmo;
	int debug_wrmo;
	int rdmo = dof->default_rdmo;
	int wrmo = dof->default_wrmo;

	dof_dbg("[DOF]\n");
	DOF_SET_F(pmio, DOF_R_IP_CFG, DOF_F_Y_DOF_IP_ENABLE, 0x1);

	DOF_SET_F(pmio, DOF_R_CFG_BYPASS_PREFETCH, DOF_F_CFG_ZERO_SKIP_BYPASS, 0x0);
	DOF_SET_F(pmio, DOF_R_CFG_BYPASS_PREFETCH, DOF_F_CFG_PREFETCH, 0x1);
	DOF_SET_F(pmio, DOF_R_CFG_BYPASS_PREFETCH, DOF_F_CFG_PREFETCH_SIZE, 0x10);

	/* Swapped */
	DOF_SET_F(pmio, DOF_R_CFG_DATA_SWAP, DOF_F_CFG_RD_DATA_INPUT0, 0x0);
	DOF_SET_F(pmio, DOF_R_CFG_DATA_SWAP, DOF_F_CFG_RD_DATA_INPUT1, 0x0);
	DOF_SET_F(pmio, DOF_R_CFG_DATA_SWAP, DOF_F_CFG_RD_DATA_OUTPUT, 0x0);

	/* Outstanding */
	debug_rdmo = dof_get_debug_rdmo();
	if (debug_rdmo)
		rdmo = debug_rdmo >= 128 ? 0 : debug_rdmo;

	debug_wrmo = dof_get_debug_wrmo();
	if (debug_wrmo)
		wrmo = debug_wrmo >= 128 ? 0 : debug_wrmo;

	dof_dbg("[DOF] set rdmo:%d, wrmo:%d\n", rdmo, wrmo);
	DOF_SET_F(pmio, DOF_R_CFG_MAX_OS, DOF_F_CFG_MAX_RD_OS, rdmo);
	DOF_SET_F(pmio, DOF_R_CFG_MAX_OS, DOF_F_CFG_MAX_WR_OS, wrmo);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_update_block_reg);

void camerapp_hw_dof_print_dma_address(
	struct dof_frame *s_frame, struct dof_frame *d_frame, struct dof_model_addr *model_addr)
{
	dof_info("[DOF][IN0] 0x%llx~0x%llx (0x%x)\n", s_frame->addr.curr_in,
		s_frame->addr.curr_in + s_frame->addr.curr_in_size, s_frame->addr.curr_in_size);
	dof_info("[DOF][IN1] 0x%llx~0x%llx (0x%x)\n", s_frame->addr.prev_in,
		s_frame->addr.prev_in + s_frame->addr.prev_in_size, s_frame->addr.prev_in_size);
	dof_info("[DOF][OUT] 0x%llx~0x%llx (0x%x)\n", d_frame->addr.output,
		d_frame->addr.output + d_frame->addr.output_size, d_frame->addr.output_size);
	dof_info("[DOF][TMP] 0x%llx~0x%llx (0x%x)\n", model_addr->dva_temporary,
		model_addr->dva_temporary + model_addr->temporary_size, model_addr->temporary_size);
	dof_info("[DOF][CON] 0x%llx~0x%llx (0x%x) 0x%llx\n", model_addr->dva_constant,
		model_addr->dva_constant + model_addr->constant_size, model_addr->constant_size,
		model_addr->dva_constant_with_offset);
	dof_info("[DOF][INS] 0x%llx~0x%llx (0x%x) 0x%llx\n", model_addr->dva_instruction,
		model_addr->dva_instruction + model_addr->instruction_size,
		model_addr->instruction_size, model_addr->dva_instruction_with_offset);
	dof_info("[DOF][PST] 0x%llx~0x%llx (0x%x)\n", s_frame->addr.prev_state,
		s_frame->addr.prev_state + s_frame->addr.prev_state_size,
		s_frame->addr.prev_state_size);
	dof_info("[DOF][NST] 0x%llx~0x%llx (0x%x)\n", d_frame->addr.next_state,
		d_frame->addr.next_state + d_frame->addr.next_state_size,
		d_frame->addr.next_state_size);
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_print_dma_address);

void camerapp_hw_dof_set_dma_address(struct pablo_mmio *pmio, struct dof_frame *s_frame,
	struct dof_frame *d_frame, struct dof_model_addr *model_addr)
{
	int n;
	int fro_number = 1;

	dof_dbg("[DOF] fro_number:%d\n", fro_number);
	if (dof_get_debug_level())
		camerapp_hw_dof_print_dma_address(s_frame, d_frame, model_addr);

	for (n = 0; n < fro_number; n++) {
		/* input */
		DOF_SET_R(pmio, DOF_R_CFG_INPUT0_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(s_frame->addr.curr_in));
		DOF_SET_R(pmio, DOF_R_CFG_INPUT0_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(s_frame->addr.curr_in));
		DOF_SET_R(pmio, DOF_R_CFG_INPUT1_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(s_frame->addr.prev_in));
		DOF_SET_R(pmio, DOF_R_CFG_INPUT1_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(s_frame->addr.prev_in));
		DOF_SET_R(pmio, DOF_R_CFG_PSTATE_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(s_frame->addr.prev_state));
		DOF_SET_R(pmio, DOF_R_CFG_PSTATE_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(s_frame->addr.prev_state));

		/* output */
		DOF_SET_R(pmio, DOF_R_CFG_OUTPUT_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(d_frame->addr.output));
		DOF_SET_R(pmio, DOF_R_CFG_OUTPUT_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(d_frame->addr.output));
		DOF_SET_R(pmio, DOF_R_CFG_NSTATE_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(d_frame->addr.next_state));
		DOF_SET_R(pmio, DOF_R_CFG_NSTATE_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(d_frame->addr.next_state));

		/* model */
		DOF_SET_R(pmio, DOF_R_CFG_INST_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(model_addr->dva_instruction_with_offset));
		DOF_SET_R(pmio, DOF_R_CFG_INST_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(model_addr->dva_instruction_with_offset));
		DOF_SET_R(pmio, DOF_R_CFG_CONST_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(model_addr->dva_constant_with_offset));
		DOF_SET_R(pmio, DOF_R_CFG_CONST_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(model_addr->dva_constant_with_offset));
		DOF_SET_R(pmio, DOF_R_CFG_TEMP_BASE_ADDR_0_MSB + n * 0x40,
			DOF_MSB(model_addr->dva_temporary));
		DOF_SET_R(pmio, DOF_R_CFG_TEMP_BASE_ADDR_0_LSB + n * 0x40,
			DOF_LSB(model_addr->dva_temporary));
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_set_dma_address);

int camerapp_hw_dof_update_param(struct pablo_mmio *pmio, struct dof_ctx *current_ctx)
{
	struct dof_frame *s_frame, *d_frame;
	struct dof_model_addr *model_addr;
	struct dof_dev *dof_dev;
	int ret = 0;

	dof_dbg("[DOF]\n");

	s_frame = &current_ctx->s_frame;
	d_frame = &current_ctx->d_frame;
	model_addr = &current_ctx->model_addr;
	dof_dev = current_ctx->dof_dev;

	camerapp_hw_dof_update_block_reg(pmio, dof_dev);
	camerapp_hw_dof_set_dma_address(pmio, s_frame, d_frame, model_addr);

	return ret;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_update_param);

void camerapp_hw_dof_update_default_mo(struct dof_dev *dof)
{
	if (!dof->enable_sw_workaround) {
		dof->default_rdmo = 0;
		dof->default_wrmo = 1;
	} else {
		dof->default_rdmo = 8;
		dof->default_wrmo = 8;
	}
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_update_default_mo);

int camerapp_hw_dof_prepare(struct dof_dev *dof)
{
	void __iomem *sysreg_dof_base;
	void __iomem *llcaid_d_dof_base;
	void __iomem *sci_2025ap_base;
	u32 value = 0;

	camerapp_hw_dof_update_default_mo(dof);

	if (!dof->enable_sw_workaround)
		return 0;

	sysreg_dof_base = devm_ioremap(dof->dev, 0x1F020000, 0x4000);
	if (IS_ERR_OR_NULL(sysreg_dof_base)) {
		dev_err(dof->dev, "Failed to ioremap for sysreg_dof_base\n");
		return -ENOMEM;
	}

	value = is_hw_get_reg(sysreg_dof_base, &sysreg_dof_regs[DOF_USER_DOF_AXCACHE]);
	dof_info("[DOF] E DOF_USER_DOF_AXCACHE : 0x%x", value);

	is_hw_set_field(sysreg_dof_base, &sysreg_dof_regs[DOF_USER_DOF_AXCACHE],
			&sysreg_dof_fields[ARCACHE_DOF_M0_OVERRIDE_EN], 0x1);
	is_hw_set_field(sysreg_dof_base, &sysreg_dof_regs[DOF_USER_DOF_AXCACHE],
			&sysreg_dof_fields[AWCACHE_DOF_M0_OVERRIDE_EN], 0x1);
	is_hw_set_field(sysreg_dof_base, &sysreg_dof_regs[DOF_USER_DOF_AXCACHE],
			&sysreg_dof_fields[ARCACHE_DOF_M0], 0x3);
	is_hw_set_field(sysreg_dof_base, &sysreg_dof_regs[DOF_USER_DOF_AXCACHE],
			&sysreg_dof_fields[AWCACHE_DOF_M0], 0x3);

	value = is_hw_get_reg(sysreg_dof_base, &sysreg_dof_regs[DOF_USER_DOF_AXCACHE]);
	dof_info("[DOF] X DOF_USER_DOF_AXCACHE : 0x%x", value);

	devm_iounmap(dof->dev, sysreg_dof_base);

	llcaid_d_dof_base = devm_ioremap(dof->dev, 0x1F090000, 0x4000);
	if (IS_ERR_OR_NULL(llcaid_d_dof_base)) {
		dev_err(dof->dev, "Failed to ioremap for llcaid_d_dof_base\n");
		return -ENOMEM;
	}

	value = is_hw_get_reg(llcaid_d_dof_base,
			      &llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_ENABLE]);
	dof_info("[DOF] E MASTERID_CFG0_STREAMID_CFG0_STREAM_ENABLE : 0x%x", value);
	value = is_hw_get_reg(llcaid_d_dof_base,
			      &llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_RD_CTRL]);
	dof_info("[DOF] E MASTERID_CFG0_STREAMID_CFG0_STREAM_RD_CTRL : 0x%x", value);
	value = is_hw_get_reg(llcaid_d_dof_base,
			      &llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_WR_CTRL]);
	dof_info("[DOF] E MASTERID_CFG0_STREAMID_CFG0_STREAM_WR_CTRL : 0x%x", value);

	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_ENABLE],
			&llcaid_d_dof_fields[ENABLE], 0x1);
	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_ENABLE],
			&llcaid_d_dof_fields[TARGET_STREAMID_MASK], 0x2);
	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_ENABLE],
			&llcaid_d_dof_fields[TARGET_STREAMID_VAL], 0x2);

	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_RD_CTRL],
			&llcaid_d_dof_fields[RD_HINT_OVERRIDE_MODE], 0x2);
	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_RD_CTRL],
			&llcaid_d_dof_fields[RD_STARIC_HINT_VAL], 0x1);
	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_RD_CTRL],
			&llcaid_d_dof_fields[RD_LLC_ID], 0x3f);

	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_WR_CTRL],
			&llcaid_d_dof_fields[WR_HINT_OVERRIDE_MODE], 0x2);
	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_WR_CTRL],
			&llcaid_d_dof_fields[WR_STARIC_HINT_VAL], 0x1);
	is_hw_set_field(llcaid_d_dof_base,
			&llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_WR_CTRL],
			&llcaid_d_dof_fields[WR_LLC_ID], 0x3f);

	value = is_hw_get_reg(llcaid_d_dof_base,
			      &llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_ENABLE]);
	dof_info("[DOF] X MASTERID_CFG0_STREAMID_CFG0_STREAM_ENABLE : 0x%x", value);
	value = is_hw_get_reg(llcaid_d_dof_base,
			      &llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_RD_CTRL]);
	dof_info("[DOF] X MASTERID_CFG0_STREAMID_CFG0_STREAM_RD_CTRL : 0x%x", value);
	value = is_hw_get_reg(llcaid_d_dof_base,
			      &llcaid_d_dof_regs[MASTERID_CFG0_STREAMID_CFG0_STREAM_WR_CTRL]);
	dof_info("[DOF] X MASTERID_CFG0_STREAMID_CFG0_STREAM_WR_CTRL : 0x%x", value);

	devm_iounmap(dof->dev, llcaid_d_dof_base);

	sci_2025ap_base = devm_ioremap(dof->dev, 0x2A060000, 0x4000);
	if (IS_ERR_OR_NULL(sci_2025ap_base)) {
		dev_err(dof->dev, "Failed to ioremap for sci_2025ap_base\n");
		return -ENOMEM;
	}

	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONBASEADDR_0]);
	dof_info("[DOF] E REGIONBASEADDR_0 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONTOPADDR_0]);
	dof_info("[DOF] E REGIONTOPADDR_0 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONBASEADDR_1]);
	dof_info("[DOF] E REGIONBASEADDR_1 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONTOPADDR_1]);
	dof_info("[DOF] E REGIONTOPADDR_1 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONBASEADDR_2]);
	dof_info("[DOF] E REGIONBASEADDR_2 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONTOPADDR_2]);
	dof_info("[DOF] E REGIONTOPADDR_2 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONBASEADDR_3]);
	dof_info("[DOF] E REGIONBASEADDR_3 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONTOPADDR_3]);
	dof_info("[DOF] E REGIONTOPADDR_3 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONBASEADDR_4]);
	dof_info("[DOF] E REGIONBASEADDR_4 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONTOPADDR_4]);
	dof_info("[DOF] E REGIONTOPADDR_4 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONBASEADDR_5]);
	dof_info("[DOF] E REGIONBASEADDR_5 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONTOPADDR_5]);
	dof_info("[DOF] E REGIONTOPADDR_5 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONBASEADDR_6]);
	dof_info("[DOF] E REGIONBASEADDR_6 : 0x%x", value);
	value = is_hw_get_reg(sci_2025ap_base, &sci_2025ap_regs[REGIONTOPADDR_6]);
	dof_info("[DOF] E REGIONTOPADDR_6 : 0x%x", value);

	devm_iounmap(dof->dev, sci_2025ap_base);

	return 0;
}
KUNIT_EXPORT_SYMBOL(camerapp_hw_dof_prepare);

static struct pablo_camerapp_hw_dof hw_dof_ops = {
	.sw_reset = camerapp_hw_dof_sw_reset,
	.wait_idle = camerapp_hw_dof_wait_idle,
	.set_initialization = camerapp_hw_dof_set_initialization,
	.update_param = camerapp_hw_dof_update_param,
	.start = camerapp_hw_dof_start,
	.sfr_dump = camerapp_hw_dof_sfr_dump,
	.prepare = camerapp_hw_dof_prepare,
};

struct pablo_camerapp_hw_dof *pablo_get_hw_dof_ops(void)
{
	return &hw_dof_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_hw_dof_ops);
