// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[@][PCC(v1.4)] " fmt

#include <linux/string.h>
#include <linux/module.h>
#include <linux/of_platform.h>

#include "sfr/pablo-sfr-common-ctrl-v1_4.h"
#include "pmio.h"
#include "is-hw-common-dma.h"
#include "pablo-hw-api-common-ctrl.h"

#define PCC_TRY_COUNT 1000000 /* 1s */
#define MAX_COTF_IN_NUM 8
#define MAX_COTF_OUT_NUM 6
#define INT_HIST_NUM 8
#define PCC_IRQ_HANDLER_NUM 32 /* 32 bits per 1 IRQ*/
#define PCC_DUMMY_CMD_HEADER_NUM 1
#define MAX_EXT_CR_NUM 100

#define PCC_INT_GRP_MASK_FRO_BASE                                                                  \
	((0) | BIT_MASK(PCC_INT_GRP_ERR_CRPT) | BIT_MASK(PCC_INT_GRP_CMDQ_HOLD))
#define PCC_INT_GRP_MASK_FRO_FIRST ((0) | BIT_MASK(PCC_INT_GRP_FRAME_START))
#define PCC_INT_GRP_MASK_FRO_MID ((0) | BIT_MASK(PCC_INT_GRP_DEBUG)) /* CINROW */
#define PCC_INT_GRP_MASK_FRO_LAST                                                                  \
	((0) | BIT_MASK(PCC_INT_GRP_FRAME_END) | BIT_MASK(PCC_INT_GRP_SETTING_DONE))

#define PCC_PERF_MON_MASK                                                                          \
	((0) | BIT_MASK(CMN_CTRL_PERF_MON_INT_DELAY_START) |                                       \
		BIT_MASK(CMN_CTRL_PERF_MON_INT_DELAY_END) |                                        \
		BIT_MASK(CMN_CTRL_PERF_MON_INT_DELAY_USER) |                                       \
		BIT_MASK(CMN_CTRL_PERF_MON_PROCESS_PRE_CONFIG) |                                   \
		BIT_MASK(CMN_CTRL_PERF_MON_PROCESS_FRAME))

/* PMIO MACRO */
#define SET_CR(base, R, val) PMIO_SET_R(base, R, val)
#define SET_CR_F(base, R, F, val) PMIO_SET_F(base, R, F, val)
#define SET_CR_V(base, reg_val, F, val) PMIO_SET_V(base, reg_val, F, val)

#define GET_CR(base, R) PMIO_GET_R(base, R)
#define GET_CR_F(base, R, F) PMIO_GET_F(base, R, F)
#define GET_CR_V(base, F, val) _get_pmio_field_val(base, F, val)

#define GET_CMDQ_STATE(pcc)                                                                        \
	GET_CR_F((pcc)->pmio, CMN_CTRL_R_CMDQ_DEBUG_STATUS, CMN_CTRL_F_CMDQ_DEBUG_PROCESS)

/* LOG MACRO */
#define pcc_err(fmt, pcc, args...)                                                                 \
	pr_err("[%s][ERR]%s:%d:" fmt "\n", pcc->name, __func__, __LINE__, ##args)
#define pcc_warn(fmt, pcc, args...)                                                                \
	pr_warn("[%s][WRN]%s:%d:" fmt "\n", pcc->name, __func__, __LINE__, ##args)
#define pcc_info(fmt, pcc, args...)                                                                \
	pr_info("[%s]%s:%d:" fmt "\n", pcc->name, __func__, __LINE__, ##args)
#define pcc_dump(fmt, pcc, args...) pr_info("[%s][DUMP] " fmt "\n", pcc->name, ##args)

enum sw_reset_mode {
	SW_RESET_ALL,
	SW_RESET_CORE, /* not used */
	SW_RESET_APB,
	SW_RESET_MODE_NUM,
};

struct cr_field_map {
	u32 cr_offset;
	u32 field_id;
};

struct int_cr_map {
	struct cr_field_map src;
	struct cr_field_map clr;
};

struct cmdq_queue_status {
	u32 fullness;
	u32 wptr;
	u32 rptr;
};

struct cmdq_frame_id {
	u32 cmd_id;
	u32 frame_id;
};

struct cmdq_dbg_info {
	u32 state;
	u32 cmd_cnt;
	struct cmdq_queue_status queue;
	struct cmdq_frame_id pre;
	struct cmdq_frame_id cur;
	struct cmdq_frame_id next;
	bool charged;
};

typedef void (*int_handler)(struct pablo_common_ctrl *pcc);
struct irq_handler {
	int_handler func;
};

typedef void (*dump_handler)(struct pablo_common_ctrl *pcc);
struct dump_dbg_handler {
	dump_handler func;
};

extern struct pablo_common_ctrl_dbg_param_info dbg_param_info;

/* HW specified constants */
static const struct cr_field_map cotf_in_cr[MAX_COTF_IN_NUM] = {
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_0,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_1,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_2,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_3,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_4,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_5,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_6,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_67,
		.field_id = CMN_CTRL_F_IP_USE_OTF_IN_FOR_PATH_7,
	},
};

static const struct cr_field_map cotf_out_cr[MAX_COTF_OUT_NUM] = {
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_0,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_01,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_1,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_2,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_23,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_3,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_4,
	},
	{
		.cr_offset = CMN_CTRL_R_IP_USE_OTF_PATH_45,
		.field_id = CMN_CTRL_F_IP_USE_OTF_OUT_FOR_PATH_5,
	},
};

static const struct cr_field_map sw_reset_cr[SW_RESET_MODE_NUM] = {
	[SW_RESET_ALL] = {
		.cr_offset = CMN_CTRL_R_SW_RESET,
		.field_id = CMN_CTRL_F_SW_RESET,
	},
	[SW_RESET_CORE] = {
		.cr_offset = CMN_CTRL_R_SW_CORE_RESET,
		.field_id = CMN_CTRL_F_SW_CORE_RESET,
	},
	[SW_RESET_APB] = {
		.cr_offset = CMN_CTRL_R_SW_APB_RESET,
		.field_id = CMN_CTRL_R_SW_APB_RESET,
	},
};

static const struct int_cr_map int_cr_maps[PCC_INT_ID_NUM] = {
	[PCC_INT_0] = {
		.src = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT0,
			.field_id = CMN_CTRL_F_INT_REQ_INT0,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT0_CLEAR,
			.field_id = CMN_CTRL_F_INT_REQ_INT0_CLEAR,
		},
	},
	[PCC_INT_1] = {
		.src = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT1,
			.field_id = CMN_CTRL_F_INT_REQ_INT1,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_INT_REQ_INT1_CLEAR,
			.field_id = CMN_CTRL_F_INT_REQ_INT1_CLEAR,
		},
	},
	[PCC_CMDQ_INT] = {
		.src = {
			.cr_offset = CMN_CTRL_R_CMDQ_INT,
			.field_id = CMN_CTRL_F_CMDQ_INT,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_CMDQ_INT_CLEAR,
			.field_id = CMN_CTRL_F_CMDQ_INT_CLEAR,
		},
	},
	[PCC_COREX_INT] = {
		.src = {
			.cr_offset = CMN_CTRL_R_COREX_INT,
			.field_id = CMN_CTRL_F_COREX_INT,
		},
		.clr = {
			.cr_offset = CMN_CTRL_R_COREX_INT_CLEAR,
			.field_id = CMN_CTRL_F_COREX_INT_CLEAR,
		},
	},
};

#define CMDQ_STATE_STR_LENGTH 20
static const char cmdq_state_str[CMDQ_STATE_NUM][CMDQ_STATE_STR_LENGTH] = {
	"IDLE",
	"ACTIVE_CMD_POPPED",
	"PRE_CONFIG",
	"PRE_START",
	"FRAME",
	"POST_FRAME",
};

#define CMD_ID_STR_LENGTH 3
static const char cmd_id_str[][CMD_ID_STR_LENGTH] = {
	"NOR",
	"DUM",
};

/* PCC internal functions */
static inline u32 _get_pmio_field_val(struct pablo_mmio *pmio, u32 F, u32 val)
{
	struct pmio_field *field;

	field = pmio_get_field(pmio, F);
	if (!field)
		return 0;

	val &= field->mask;
	val >>= field->shift;

	return val;
}

static int _init_pmio(struct pablo_common_ctrl *pcc, struct pablo_mmio *hw_pmio)
{
	int ret;
	struct pmio_config *pcfg;
	struct pablo_mmio *pcc_pmio;

	if (pcc->pmio) {
		pcc_err("It already has PMIO.", pcc);
		goto err_init;
	}

	pcfg = &pcc->pmio_config;
	memset(pcfg, 0, sizeof(struct pmio_config));

	pcfg->name = pcc->name;
	pcfg->phys_base = hw_pmio->phys_base;
	pcfg->mmio_base = hw_pmio->mmio_base;
	pcfg->cache_type = PMIO_CACHE_FLAT;
	pcfg->dma_addr_shift = LSB_BIT;
	pcfg->ignore_phys_base = hw_pmio->ignore_phys_base;

	pcfg->max_register = CMN_CTRL_R_FREEZE_CORRUPTED_ENABLE;
	pcfg->num_reg_defaults_raw = (pcfg->max_register / PMIO_REG_STRIDE) + 1;
	pcfg->fields = cmn_ctrl_field_descs;
	pcfg->num_fields = ARRAY_SIZE(cmn_ctrl_field_descs);

	pcfg->rd_table = &cmn_ctrl_rd_ranges_table;
	pcfg->volatile_table = &cmn_ctrl_volatile_ranges_table;

	pcc_pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pcc_pmio)) {
		pcc_err("Failed to init PMIO. ret(%ld)", pcc, PTR_ERR(pcc_pmio));
		goto err_init;
	}

	ret = pmio_field_bulk_alloc(pcc_pmio, &pcc->fields, pcfg->fields, pcfg->num_fields);
	if (ret) {
		pcc_err("Failed to alloc PMIO field bulk. ret(%d)", pcc, ret);
		goto err_field_bulk_alloc;
	}

	/**
	 * Use APB_DIRECT for PCC APIs by default
	 *
	 * Because PCC user generates a CMDQ cmd with its own cloader buffer,
	 * PCC should not use another cloader buffer by default.
	 * The PCC cloader buffer is only for CMDQ dummy cmd.
	 */
	pmio_cache_set_bypass(pcc_pmio, true);

	pcc->pmio = pcc_pmio;

	return 0;

err_field_bulk_alloc:
	pmio_exit(pcc_pmio);
err_init:
	return -EINVAL;
}

static int _init_cloader(struct pablo_common_ctrl *pcc, struct is_mem *mem)
{
	struct pablo_internal_subdev *pis;
	int ret;
	u32 reg_num, reg_size, header_size;

	if (!mem) {
		pcc_err("mem is NULL.", pcc);
		return -EINVAL;
	}

	pis = &pcc->subdev_cloader;
	ret = pablo_internal_subdev_probe(pis, 0, mem, "CLOADER");
	if (ret) {
		pcc_err("Failed to probe internal sub-device for CLOADER. ret(%d)", pcc, ret);
		return ret;
	}

	reg_num = (pcc->pmio->max_register / PMIO_REG_STRIDE) + 1;
	reg_num += MAX_EXT_CR_NUM;
	reg_size = reg_num * PMIO_REG_STRIDE;
	pcc->header_size = header_size = ALIGN(reg_num, 16);

	pis->width = header_size + reg_size;
	pis->height = 1;
	pis->num_planes = 1;
	pis->num_batch = 1;
	pis->num_buffers = 1;
	pis->bits_per_pixel = BITS_PER_BYTE;
	pis->memory_bitwidth = BITS_PER_BYTE;
	pis->size[0] = ALIGN(DIV_ROUND_UP(pis->width * pis->memory_bitwidth, BITS_PER_BYTE), 32) *
		       pis->height;

	ret = CALL_I_SUBDEV_OPS(pis, alloc, pis);
	if (ret) {
		pcc_err("Failed to alloc internal sub-device for CLOADER. ret(%d)", pcc, ret);
		return ret;
	}

	return 0;
}

static void _pcc_set_qch(struct pablo_common_ctrl *pcc, bool on)
{
	struct pablo_mmio *pmio = pcc->pmio;
	unsigned long irq_flags;

	spin_lock_irqsave(&pcc->qch_lock, irq_flags);

	if (on && (pcc->qch_ref_cnt++ == 0))
		SET_CR(pmio, CMN_CTRL_R_IP_PROCESSING, 1);
	else if (!on && --pcc->qch_ref_cnt == 0)
		SET_CR(pmio, CMN_CTRL_R_IP_PROCESSING, 0);

	if (pcc->qch_ref_cnt < 0)
		pcc->qch_ref_cnt = 0;

	spin_unlock_irqrestore(&pcc->qch_lock, irq_flags);
}

struct sfr_access_log {
	u32 sfr_ofs;
	u32 sfr_range;
};

#define SFR_ACCESS_LOG_NUM 4
static struct sfr_access_log sfr_access_logs[SFR_ACCESS_LOG_NUM] = {
	[0] = {
		.sfr_ofs = 0x0,
		.sfr_range = 0x0,
	},
	[1] = {
		.sfr_ofs = 0x0,
		.sfr_range = 0x0,
	},
	[2] = {
		.sfr_ofs = 0x0,
		.sfr_range = 0x0,
	},
	[3] = {
		.sfr_ofs = 0x0,
		.sfr_range = 0x0,
	},
};

/* Values defined by HW */
#define INCREMENT_SHIFT 0
#define INCREMENT_BIT_NUM 6
#define INCREMENT_MAX (1 << INCREMENT_BIT_NUM)
#define MULTIPLE_SHIFT INCREMENT_BIT_NUM
#define MULTIPLE_BIT_NUM 2
#define MULTIPLE_NUM 4
#define APB_PADDR_MASK GENMASK(27, 0)

static const u32 sfr_access_log_multiples[MULTIPLE_NUM] = { 1, 4, 16, 256 };

static u32 _get_sfr_access_log_adjust_range(u32 range)
{
	u32 num_cr = range >> 2;
	u32 i, multiple, increment;
	u32 ret = 0;

	if (!num_cr)
		return 0;

	for (i = 0; i < MULTIPLE_NUM; i++) {
		multiple = sfr_access_log_multiples[i];
		if (num_cr > (INCREMENT_MAX * multiple))
			continue;

		increment = (ALIGN(num_cr, multiple) / multiple) - 1;

		ret = (increment << INCREMENT_SHIFT);
		ret |= (i << MULTIPLE_SHIFT);
		break;
	}

	return ret;
}

static void _pcc_set_sfr_log(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 i, range, paddr;
	bool enable = false;

	for (i = 0; i < SFR_ACCESS_LOG_NUM; i++) {
		range = sfr_access_logs[i].sfr_range;
		if (!range)
			continue;

		range = _get_sfr_access_log_adjust_range(sfr_access_logs[i].sfr_range);
		SET_CR_F(pmio, CMN_CTRL_R_SFR_ACCESS_LOG_0 + (0x8 * i),
			CMN_CTRL_F_SFR_ACCESS_LOG_0_ADJUST_RANGE, range);

		paddr = (pmio->phys_base + sfr_access_logs[i].sfr_ofs) & APB_PADDR_MASK;
		SET_CR(pmio, CMN_CTRL_R_SFR_ACCESS_LOG_0_ADDRESS + (0x8 * i), paddr);

		enable = true;
	}

	SET_CR(pmio, CMN_CTRL_R_SFR_ACCESS_LOG_ENABLE, enable);
}

#define PCC_PERF_MON_USER_INT_ID PCC_INT_0 /* INT0, INT1 only */
#define PCC_PERF_MON_USER_INT_BIT CMN_CTRL_INT0_FRAME_END_INT
static void _pcc_set_perf_mon(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 val = 0;

	val = SET_CR_V(
		pmio, val, CMN_CTRL_F_PERF_MONITOR_INT_USER_SEL_INT_ID, PCC_PERF_MON_USER_INT_ID);
	val = SET_CR_V(pmio, val, CMN_CTRL_F_PERF_MONITOR_INT_USER_SEL, PCC_PERF_MON_USER_INT_BIT);
	SET_CR(pmio, CMN_CTRL_R_PERF_MONITOR_INT_USER_SEL, val);

	SET_CR_F(pmio, CMN_CTRL_R_PERF_MONITOR_ENABLE, CMN_CTRL_F_PERF_MONITOR_ENABLE,
		PCC_PERF_MON_MASK);
}

static int _wait_sw_reset(struct pablo_common_ctrl *pcc, enum sw_reset_mode mode)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 retry = PCC_TRY_COUNT;

	pcc_info("mode(%d) E", pcc, mode);

	while (GET_CR(pmio, sw_reset_cr[mode].cr_offset) && --retry)
		udelay(1);

	if (!retry) {
		pcc_err("sw_reset timeout", pcc);
		return -ETIME;
	}

	pcc_info("mode(%d) X", pcc, mode);

	return 0;
}

static void _pcc_enable(struct pablo_common_ctrl *pcc, const struct pablo_common_ctrl_cfg *cfg)
{
	struct pablo_mmio *pmio = pcc->pmio;

	/* Enable QCH to get IP clock. */
	_pcc_set_qch(pcc, true);

	/* Enable Debugging features */
	_pcc_set_sfr_log(pcc);
	_pcc_set_perf_mon(pcc);
	SET_CR_F(pmio, CMN_CTRL_R_DEBUG_CLOCK_ENABLE, CMN_CTRL_F_DEBUG_CLOCK_ENABLE, 1);

	/* Assert FS event when there is actual VValid signal input. */
	SET_CR(pmio, CMN_CTRL_R_IP_USE_CINFIFO_NEW_FRAME_IN, cfg->fs_mode);

	/**
	 * When HW is in clock-gating state,
	 * send input stall signal for the VHD valid signal from input.
	 */
	SET_CR_F(pmio, CMN_CTRL_R_CMDQ_VHD_CONTROL, CMN_CTRL_F_CMDQ_VHD_STALL_ON_QSTOP_ENABLE, 1);

	SET_CR_F(pmio, CMN_CTRL_R_C_LOADER_LOGICAL_OFFSET_EN, CMN_CTRL_F_C_LOADER_LOGICAL_OFFSET_EN,
		pmio->ignore_phys_base);

	/* Enable C_Loader. The actual C_Loader operation follows the CMDQ setting mode. */
	SET_CR(pmio, CMN_CTRL_R_C_LOADER_ENABLE, 1);

	/* Enable interrupts */
	SET_CR(pmio, CMN_CTRL_R_INT_REQ_INT0_ENABLE, cfg->int_en[PCC_INT_0]);
	SET_CR(pmio, CMN_CTRL_R_INT_REQ_INT1_ENABLE, cfg->int_en[PCC_INT_1]);
	SET_CR(pmio, CMN_CTRL_R_CMDQ_INT_ENABLE, cfg->int_en[PCC_CMDQ_INT]);
	SET_CR(pmio, CMN_CTRL_R_COREX_INT_ENABLE, cfg->int_en[PCC_COREX_INT]);
	SET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT0_ENABLE, 0xffffffff);
	SET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT1_ENABLE, 0xffffffff);

	/* Enable CMDQ */
	SET_CR(pmio, CMN_CTRL_R_CMDQ_ENABLE, 1);
}

static int _wait_cmdq_idle(struct pablo_common_ctrl *pcc)
{
	u32 retry = PCC_TRY_COUNT;
	u32 val;
	bool busy;

	do {
		val = GET_CMDQ_STATE(pcc);
		busy = val & CMDQ_BUSY_STATE;
		if (busy)
			udelay(1);
		else
			break;
	} while (--retry);

	if (busy) {
		pcc_warn("timeout(0x%x)", pcc, val);
		return -ETIME;
	}

	return 0;
}

static inline void set_cmdq_lock(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 val = 0;

	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_POP_LOCK, 1);
	val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_RELOAD_LOCK, 1);

	SET_CR(pmio, CMN_CTRL_R_CMDQ_LOCK, val);
}

static void acquire_cmdq_lock(struct pablo_common_ctrl *pcc)
{
	spin_lock_irqsave(&pcc->cmdq_lock, pcc->irq_flags);

	set_cmdq_lock(pcc);
	_wait_cmdq_idle(pcc);
}

static inline void release_cmdq_lock(struct pablo_common_ctrl *pcc)
{
	SET_CR(pcc->pmio, CMN_CTRL_R_CMDQ_LOCK, 0);

	spin_unlock_irqrestore(&pcc->cmdq_lock, pcc->irq_flags);
}

static void _sw_reset(struct pablo_common_ctrl *pcc, enum sw_reset_mode mode)
{
	struct pablo_mmio *pmio = pcc->pmio;
	unsigned long irq_flags;

	set_cmdq_lock(pcc);

	pcc_info("mode(%d)", pcc, mode);

	SET_CR(pmio, sw_reset_cr[mode].cr_offset, 1);

	if (mode != SW_RESET_CORE) {
		/* It also reset IP_PROCESSING CR. */
		spin_lock_irqsave(&pcc->qch_lock, irq_flags);
		pcc->qch_ref_cnt = 0;
		spin_unlock_irqrestore(&pcc->qch_lock, irq_flags);
	}
}

static void _s_cotf(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 pos, val;

	/* COTF_IN */
	for (pos = 0; pos < MAX_COTF_IN_NUM; pos++) {
		val = (frame_cfg->cotf_in_en & BIT_MASK(pos)) ? 1 : 0;
		SET_CR_F(pmio, cotf_in_cr[pos].cr_offset, cotf_in_cr[pos].field_id, val);
	}

	/* COTF_OUT */
	for (pos = 0; pos < MAX_COTF_OUT_NUM; pos++) {
		val = (frame_cfg->cotf_out_en & BIT_MASK(pos)) ? 1 : 0;
		SET_CR_F(pmio, cotf_out_cr[pos].cr_offset, cotf_out_cr[pos].field_id, val);
	}
}

static void _s_post_frame_gap(struct pablo_common_ctrl *pcc, u32 delay)
{
	u32 cur_delay =
		GET_CR_F(pcc->pmio, CMN_CTRL_R_IP_POST_FRAME_GAP, CMN_CTRL_F_IP_POST_FRAME_GAP);

	if (delay < MIN_POST_FRAME_GAP)
		delay = MIN_POST_FRAME_GAP;

	if (cur_delay == delay)
		return;

	pcc_info("%s:%d -> %d", pcc, __func__, cur_delay, delay);
	SET_CR_F(pcc->pmio, CMN_CTRL_R_IP_POST_FRAME_GAP, CMN_CTRL_F_IP_POST_FRAME_GAP, delay);
}

static void _get_cmdq_queue_status(struct pablo_common_ctrl *pcc, struct cmdq_queue_status *status)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 val;

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_QUEUE_0_INFO);
	status->fullness = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_FULLNESS, val);
	status->wptr = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_WPTR, val);
	status->rptr = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_RPTR, val);
}

static u32 _get_cmdq_queue_fullness(struct pablo_common_ctrl *pcc)
{
	struct cmdq_queue_status status = {
		0,
	};

	_get_cmdq_queue_status(pcc, &status);

	return status.fullness;
}

static void _flush_cmdq_queue(struct pablo_common_ctrl *pcc)
{
	if (!_get_cmdq_queue_fullness(pcc))
		return;

	/*
	 * This reset below CR values, too.
	 *  - cmd_h/m/l
	 *  - queue_wptr
	 *  - queue_rptr
	 *  - queue_fullness
	 */
	SET_CR_F(pcc->pmio, CMN_CTRL_R_CMDQ_FLUSH_QUEUE_0, CMN_CTRL_F_CMDQ_FLUSH_QUEUE_0, 1);
}

static void _s_ext_cr(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_cr_set *cr_set,
	struct c_loader_buffer *clb)
{
	if (!cr_set->cr || !cr_set->size)
		return;

	if (cr_set->size > MAX_EXT_CR_NUM) {
		pcc_err("Too many ext_cr_set size(%u)", pcc, cr_set->size);
		return;
	}

	pmio_cache_fsync_ext(pcc->pmio, (void *)clb, (void *)cr_set->cr, cr_set->size);
}

static int _s_cmd(struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct pablo_mmio *pmio = pcc->pmio;
	struct pablo_common_ctrl_cmd *cmd = &frame_cfg->cmd;
	u32 set_mode = cmd->set_mode, header_num = 0;
	u32 base_addr, val, int_grp_en, fro_id;
	u32 num_buffers = frame_cfg->num_buffers;

	/* C-loader configuration */
	switch (set_mode) {
	case PCC_DMA_PRELOADING:
	case PCC_DMA_DIRECT:
		base_addr = DVA_36BIT_HIGH(cmd->base_addr);
		if (base_addr) {
			header_num = cmd->header_num;
		} else {
			pcc_warn("Missing base_addr. Force APB_DIRECT mode.", pcc);
			set_mode = PCC_APB_DIRECT;
		}
		break;
	case PCC_COREX:
	case PCC_APB_DIRECT:
		base_addr = 0;
		header_num = 0;
		break;
	default:
		pcc_err("Invalid CMDQ setting mode(%d)", pcc, set_mode);
		return -EINVAL;
	}

	if (!num_buffers) {
		pcc_warn("Invalid num_buffers(%d)", pcc, num_buffers);
		num_buffers = 1;
	}

	for (fro_id = 0; fro_id < num_buffers; fro_id++) {
		int_grp_en = 0;
		if (num_buffers > 1) {
			if (fro_id > 0)
				set_mode = PCC_APB_DIRECT;

			int_grp_en = PCC_INT_GRP_MASK_FRO_BASE;
			if (fro_id == 0)
				int_grp_en |= PCC_INT_GRP_MASK_FRO_FIRST;
			if (fro_id == ((num_buffers - 1) / 2))
				int_grp_en |= PCC_INT_GRP_MASK_FRO_MID;
			if (fro_id == num_buffers - 1)
				int_grp_en |= PCC_INT_GRP_MASK_FRO_LAST;
		} else {
			int_grp_en = cmd->int_grp_en;
		}

		SET_CR_F(pmio, CMN_CTRL_R_CMDQ_QUE_CMD_H, CMN_CTRL_F_CMDQ_QUE_CMD_BASE_ADDR,
			base_addr);

		val = 0;
		val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_HEADER_NUM, header_num);
		val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_SETTING_MODE, set_mode);
		val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_HOLD_MODE, 0); /* not used */
		val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_CMD_ID, cmd->cmd_id);
		val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_FRAME_ID, cmd->fcount);
		SET_CR(pmio, CMN_CTRL_R_CMDQ_QUE_CMD_M, val);

		val = 0;
		val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, int_grp_en);
		val = SET_CR_V(pmio, val, CMN_CTRL_F_CMDQ_QUE_CMD_FRO_INDEX, fro_id);
		SET_CR(pmio, CMN_CTRL_R_CMDQ_QUE_CMD_L, val);

		/* This triggers HW frame processing. */
		SET_CR(pcc->pmio, CMN_CTRL_R_CMDQ_ADD_TO_QUEUE_0, 1);
	}

	return 0;
}

static void _get_cmdq_last_cmd(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	struct pmio_field *field;
	struct pablo_common_ctrl_cmd *last_cmd;
	u32 last_rptr, val;

	/* Get CMDQ previous read pointer with the way of circular buffer. */
	field = pmio_get_field(pmio, CMN_CTRL_F_CMDQ_QUEUE_0_RPTR_FOR_DEBUG);

	last_rptr = GET_CR_F(pmio, CMN_CTRL_R_CMDQ_QUEUE_0_INFO, CMN_CTRL_F_CMDQ_QUEUE_0_RPTR);
	last_rptr = (last_rptr + (field->mask >> field->shift)) & field->mask;
	pcc->dbg.last_rptr = last_rptr;

	/* Move CMDQ debug read pointer to last read pointer */
	SET_CR_F(pmio, CMN_CTRL_R_CMDQ_QUEUE_0_RPTR_FOR_DEBUG,
		CMN_CTRL_F_CMDQ_QUEUE_0_RPTR_FOR_DEBUG, last_rptr);

	last_cmd = &pcc->dbg.last_cmd;
	memset(last_cmd, 0, sizeof(struct pablo_common_ctrl_cmd));

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_H);
	last_cmd->base_addr = (dma_addr_t)GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_BASE_ADDR, val)
			      << LSB_BIT;

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_M);
	last_cmd->header_num = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_HEADER_NUM, val);
	last_cmd->set_mode = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_SETTING_MODE, val);
	last_cmd->cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_CMD_ID, val);
	last_cmd->fcount = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_FRAME_ID, val);

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_QUE_0_CMD_L);
	last_cmd->int_grp_en = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE, val);
	last_cmd->fro_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_QUE_CMD_FRO_INDEX, val);
}

static void _prepare_dummy_cmd(
	struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct pablo_common_ctrl_frame_cfg dummy_cfg = {
		0,
	};
	struct c_loader_buffer clb = {
		0,
	};
	struct pablo_mmio *pmio = pcc->pmio;
	struct c_loader_buffer *p_clb = NULL;
	struct pablo_common_ctrl_cmd *dummy_cmd, *cmd;
	struct is_framemgr *framemgr;
	struct is_frame *cl_frame;

	/* Get Cloader buffer */
	framemgr = GET_SUBDEV_I_FRAMEMGR(&pcc->subdev_cloader);
	cl_frame = peek_frame(framemgr, FS_FREE);
	if (likely(cl_frame)) {
		clb.header_dva = cl_frame->dvaddr_buffer[0];
		clb.payload_dva = cl_frame->dvaddr_buffer[0] + pcc->header_size;
		clb.clh = (struct c_loader_header *)cl_frame->kvaddr_buffer[0];
		clb.clp =
			(struct c_loader_payload *)(cl_frame->kvaddr_buffer[0] + pcc->header_size);

		p_clb = &clb;
	}

	dummy_cfg.num_buffers = frame_cfg->num_buffers;

	dummy_cmd = &dummy_cfg.cmd;
	cmd = &frame_cfg->cmd;

	dummy_cmd->set_mode = PCC_APB_DIRECT;
	dummy_cmd->cmd_id = CMD_DUMMY;
	dummy_cmd->fcount = cmd->fcount;
	dummy_cmd->int_grp_en = cmd->int_grp_en;

	if (likely(p_clb)) {
		struct is_priv_buf *pb;

		_s_ext_cr(pcc, &frame_cfg->ext_cr_set, p_clb);

		dummy_cmd->base_addr = p_clb->header_dva;
		dummy_cmd->header_num = p_clb->num_of_headers + PCC_DUMMY_CMD_HEADER_NUM;
		dummy_cmd->set_mode = PCC_DMA_DIRECT;

		/* Update CR memory only into Cloader buffer */
		pmio_cache_set_bypass(pmio, false);
		pmio_cache_set_only(pmio, true);
		_s_cmd(pcc, &dummy_cfg);
		pmio_cache_set_only(pmio, false);
		pmio_cache_set_bypass(pmio, true);

		/* Get Cloader header & payload data from PMIO for dummy_cmd */
		pmio_cache_fsync(pmio, (void *)p_clb, PMIO_FORMATTER_PAIR);
		if (p_clb->num_of_pairs > 0)
			p_clb->num_of_headers++;

		/* Flush host CPU cache */
		pb = cl_frame->pb_output;
		CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);

		if (p_clb->num_of_headers != dummy_cmd->header_num) {
			pcc_err("Invalid header of dummy_cmd! clb_header_num(%d) cmd_header_num(%d)",
				pcc, p_clb->num_of_headers, dummy_cmd->header_num);
			dummy_cmd->set_mode = PCC_APB_DIRECT;
		}
	}

	/* Add dummy_cmd into CMDQ queue */
	_s_cmd(pcc, &dummy_cfg);
}

static int _prepare_real_cmd(
	struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	int ret;

	if (!GET_CR(pcc->pmio, CMN_CTRL_R_CMDQ_ENABLE)) {
		pcc_err("CMDQ is being disabled!!", pcc);
		return -EPERM;
	}

	frame_cfg->cmd.cmd_id = CMD_NORMAL;

	/**
	 * Don't change COTF configuration for CMDQ PRE_START state.
	 *
	 * CMDQ PRE_START is the specific timing as below.
	 *  - PRE_CONFIG: CMDQ finished to update HW CRs for next frame.
	 *  - PRE_START: CMDQ is waiting the frame_start event of next frame.
	 *  - FRAME: It starts frame processing.
	 *
	 * So, PRE_START state means it finished to update HW CR of next frame,
	 * and it's waiting frame_start event.
	 * The COTF configuration, however, affects the frame_start event from COTF.
	 * Since it's changed before frame_start event, the original HW CR setting of COTF
	 * would be overwritten & might occur HW mal-function.
	 */
	if (GET_CMDQ_STATE(pcc) != CMDQ_PRE_START)
		_s_cotf(pcc, frame_cfg);

	/* TODO: Care about FRO */
	_flush_cmdq_queue(pcc);

	ret = _s_cmd(pcc, frame_cfg);

	if (pcc->mode == PCC_OTF)
		_prepare_dummy_cmd(pcc, frame_cfg);

	return ret;
}

static void _get_cmdq_frame_id(
	struct pablo_common_ctrl *pcc, struct cmdq_frame_id *pre, struct cmdq_frame_id *cur)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 val;

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_FRAME_ID);

	pre->cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_PRE_CMD_ID, val);
	pre->frame_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_PRE_FRAME_ID, val);
	cur->cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CURRENT_CMD_ID, val);
	cur->frame_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CURRENT_FRAME_ID, val);
}

static void _frame_start_handler(struct pablo_common_ctrl *pcc)
{
	struct cmdq_dbg_info dbg;

	_get_cmdq_frame_id(pcc, &dbg.pre, &dbg.cur);

	pcc->dbg.last_fcount = dbg.cur.frame_id;
}

static void _setting_done_otf_handler(struct pablo_common_ctrl *pcc)
{
	struct pablo_common_ctrl_frame_cfg *frame_cfg;

	_get_cmdq_last_cmd(pcc);

	frame_cfg = &pcc->frame_cfg;

	if (frame_cfg->num_buffers < 2)
		return;
	else if (frame_cfg->cmd.fcount <= pcc->dbg.last_cmd.fcount)
		return;

	acquire_cmdq_lock(pcc);

	if (_get_cmdq_queue_fullness(pcc) % frame_cfg->num_buffers != 0) {
		release_cmdq_lock(pcc);
		return;
	}

	/* add_to_queue real_cmd for FRO scenario */
	_prepare_real_cmd(pcc, frame_cfg);

	release_cmdq_lock(pcc);
}

static void _setting_done_m2m_handler(struct pablo_common_ctrl *pcc)
{
	_get_cmdq_last_cmd(pcc);
}

static struct irq_handler irq_handlers[PCC_INT_ID_NUM][PCC_IRQ_HANDLER_NUM][PCC_MODE_NUM] = {
	[PCC_INT_0] = {
		[CMN_CTRL_INT0_FRAME_START_INT] = {
			[PCC_OTF] = {
				.func = _frame_start_handler,
			},
			[PCC_M2M] = {
				.func = _frame_start_handler,
			},
			[PCC_OTF_NO_DUMMY] = {
				.func = _frame_start_handler,
			},
		},
		[CMN_CTRL_INT0_SETTING_DONE_INT] = {
			[PCC_OTF] = {
				.func = _setting_done_otf_handler,
			},
			[PCC_M2M] = {
				.func = _setting_done_m2m_handler,
			},
			[PCC_OTF_NO_DUMMY] = {
				.func = _setting_done_otf_handler,
			},
		},
	},
	[PCC_INT_1] = {
		/* Nothing to do */
	},
	[PCC_CMDQ_INT] = {
		/* Nothing to do */
	},
	[PCC_COREX_INT] = {
		/* Nothing to do */
	},
};

static u32 _get_int_status(struct pablo_common_ctrl *pcc, u32 src_cr, u32 clr_cr, bool clear)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 status;

	status = GET_CR(pmio, src_cr);

	if (clear)
		SET_CR(pmio, clr_cr, status);

	return status;
}

static void _dump_cotf(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 i;

	pcc_dump("COTF IN *********************", pcc);
	for (i = 0; i < MAX_COTF_IN_NUM; i++) {
		if (GET_CR_F(pmio, cotf_in_cr[i].cr_offset, cotf_in_cr[i].field_id))
			pcc_dump("[IN%d] ON", pcc, i);
	}

	pcc_dump("COTF OUT ********************", pcc);
	for (i = 0; i < MAX_COTF_OUT_NUM; i++) {
		if (GET_CR_F(pmio, cotf_out_cr[i].cr_offset, cotf_out_cr[i].field_id))
			pcc_dump("[OUT%d] ON", pcc, i);
	}
}

static void _get_cmdq_state_str(const u32 state, char *str)
{
	u32 i;
	const ulong state_bits = state;

	if (!state) {
		strncpy(str, cmdq_state_str[0], CMDQ_STATE_STR_LENGTH);
		str += CMDQ_STATE_STR_LENGTH;
	}

	for_each_set_bit (i, (const ulong *)&state_bits, CMDQ_STATE_NUM - 1) {
		strncpy(str, cmdq_state_str[i + 1], CMDQ_STATE_STR_LENGTH);
		str += CMDQ_STATE_STR_LENGTH;
	}

	*str = '\0';
}

static void _dump_cmdq(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	struct cmdq_dbg_info dbg;
	struct pablo_common_ctrl_cmd *last_cmd;
	char *str;
	u32 val;

	acquire_cmdq_lock(pcc);

	dbg.state = GET_CMDQ_STATE(pcc);
	dbg.cmd_cnt = GET_CR_F(pmio, CMN_CTRL_R_CMDQ_FRAME_COUNTER, CMN_CTRL_F_CMDQ_FRAME_COUNTER);

	_get_cmdq_queue_status(pcc, &dbg.queue);
	_get_cmdq_frame_id(pcc, &dbg.pre, &dbg.cur);

	val = GET_CR(pmio, CMN_CTRL_R_CMDQ_DEBUG_STATUS_PRE_LOAD);
	dbg.next.cmd_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CHARGED_CMD_ID, val);
	dbg.next.frame_id = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CHARGED_FRAME_ID, val);
	dbg.charged = GET_CR_V(pmio, CMN_CTRL_F_CMDQ_CHARGED_FOR_NEXT_FRAME, val);

	last_cmd = &pcc->dbg.last_cmd;

	release_cmdq_lock(pcc);

	str = __getname();
	if (!str)
		return;

	_get_cmdq_state_str(dbg.state, str);

	pcc_dump("CMDQ ************************", pcc);
	pcc_dump("STATE: %s", pcc, str);
	pcc_dump("CMD_CNT: %d", pcc, dbg.cmd_cnt);
	pcc_dump("QUEUE: fullness %d last_rptr %d rptr %d wptr %d", pcc, dbg.queue.fullness,
		pcc->dbg.last_rptr, dbg.queue.rptr, dbg.queue.wptr);
	pcc_dump("FRAME_ID: PRE[%.3s][F%d] -> CUR[%.3s][F%d] -> NEXT[%.3s][F%d]", pcc,
		cmd_id_str[dbg.pre.cmd_id], dbg.pre.frame_id, cmd_id_str[dbg.cur.cmd_id],
		dbg.cur.frame_id, dbg.charged ? cmd_id_str[dbg.next.cmd_id] : "",
		dbg.charged ? dbg.next.frame_id : -1);
	pcc_dump("LAST_CMD_H: dva 0x%llx", pcc, last_cmd->base_addr);
	pcc_dump("LAST_CMD_M: header_num %d set_mode %d cmd_id %d fcount %d", pcc,
		last_cmd->header_num, last_cmd->set_mode, last_cmd->cmd_id, last_cmd->fcount);
	pcc_dump("LAST_CMD_L: int_grp_en 0x%x fro_id %d", pcc, last_cmd->int_grp_en,
		last_cmd->fro_id);
	pcc_dump("POST_FRAME_GAP: %u", pcc,
		GET_CR_F(pmio, CMN_CTRL_R_IP_POST_FRAME_GAP, CMN_CTRL_F_IP_POST_FRAME_GAP));

	__putname(str);
}

static void _dump_cloader(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	bool en, busy;

	en = GET_CR_F(pmio, CMN_CTRL_R_C_LOADER_ENABLE, CMN_CTRL_F_C_LOADER_ENABLE);
	busy = GET_CR_F(pmio, CMN_CTRL_R_C_LOADER_DEBUG_STATUS, CMN_CTRL_F_C_LOADER_BUSY);

	pcc_dump("CLOADER *********************", pcc);
	pcc_dump("ENABLE: %s", pcc, GET_EN_STR(en));
	pcc_dump("STATE: %s", pcc, busy ? "BUSY" : "IDLE");
	pcc_dump("HEADER_CNT[DMA] header %d payload %d", pcc,
		GET_CR_F(pmio, CMN_CTRL_R_C_LOADER_DEBUG_HEADER_REQ_COUNTER,
			CMN_CTRL_F_C_LOADER_NUM_OF_HEADER_TO_REQ),
		GET_CR_F(pmio, CMN_CTRL_R_C_LOADER_DEBUG_HEADER_REQ_COUNTER,
			CMN_CTRL_F_C_LOADER_NUM_OF_HEADER_REQED));
	pcc_dump("HEADER_CNT[APB] set %d skip %d", pcc,
		GET_CR_F(pmio, CMN_CTRL_R_C_LOADER_DEBUG_HEADER_APB_COUNTER,
			CMN_CTRL_F_C_LOADER_NUM_OF_HEADER_APBED),
		GET_CR_F(pmio, CMN_CTRL_R_C_LOADER_DEBUG_HEADER_APB_COUNTER,
			CMN_CTRL_F_C_LOADER_NUM_OF_HEADER_SKIPED));
}

static void _dump_int_hist(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 i, cr_offset, val;

	pcc_dump("INT_HIST ********************", pcc);
	pcc_dump("[CUR] 0x%08x 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT0),
		GET_CR(pmio, CMN_CTRL_R_INT_HIST_CURINT1));

	for (i = 0, cr_offset = 0; i < INT_HIST_NUM; i++, cr_offset += (PMIO_REG_STRIDE * 3)) {
		val = GET_CR(pmio, CMN_CTRL_R_INT_HIST_00_FRAME_ID + cr_offset);

		pcc_dump("[%.3s][F%d] 0x%08x 0x%08x", pcc,
			cmd_id_str[GET_CR_V(pmio, CMN_CTRL_F_INT_HIST_00_CMD_ID, val)],
			GET_CR_V(pmio, CMN_CTRL_F_INT_HIST_00_FRAME_ID, val),
			GET_CR(pmio, CMN_CTRL_R_INT_HIST_00_INT0 + cr_offset),
			GET_CR(pmio, CMN_CTRL_R_INT_HIST_00_INT1 + cr_offset));
	}
}

static void _dump_dbg_status(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;

	pcc_dump("DBG_STATUS ******************", pcc);
	pcc_dump("STATUS: qch 0x%x chain_idle %d idle %d", pcc,
		GET_CR_F(pmio, CMN_CTRL_R_QCH_STATUS, CMN_CTRL_F_QCH_STATUS),
		GET_CR_F(pmio, CMN_CTRL_R_IDLENESS_STATUS, CMN_CTRL_F_CHAIN_IDLENESS_STATUS),
		GET_CR_F(pmio, CMN_CTRL_R_IDLENESS_STATUS, CMN_CTRL_F_IDLENESS_STATUS));
	pcc_dump("BUSY_MON[0] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_0));
	pcc_dump("BUSY_MON[1] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_1));
	pcc_dump("BUSY_MON[2] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_2));
	pcc_dump("BUSY_MON[3] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_BUSY_MONITOR_3));
	pcc_dump("STAL_OUT[0] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_0));
	pcc_dump("STAL_OUT[1] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_1));
	pcc_dump("STAL_OUT[2] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_2));
	pcc_dump("STAL_OUT[3] 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_IP_STALL_OUT_STATUS_3));
}

#define GET_LOG(log, ofs) ((log >> ofs) & 0xF)
static void _dump_sfr_access_log(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 i, range, paddr, increment, multiple, log;

	if (!GET_CR(pmio, CMN_CTRL_R_SFR_ACCESS_LOG_ENABLE))
		return;

	pcc_dump("SFR_ACCESS_LOG **************", pcc);

	for (i = 0; i < SFR_ACCESS_LOG_NUM; i++) {
		log = GET_CR(pmio, CMN_CTRL_R_SFR_ACCESS_LOG_0 + (0x8 * i));
		paddr = GET_CR(pmio, CMN_CTRL_R_SFR_ACCESS_LOG_0_ADDRESS + (0x8 * i));

		range = GET_CR_V(pmio, CMN_CTRL_F_SFR_ACCESS_LOG_0_ADJUST_RANGE, log);
		increment = ((range >> INCREMENT_SHIFT) & GENMASK(INCREMENT_BIT_NUM - 1, 0)) + 1;
		multiple = (range >> MULTIPLE_SHIFT) & GENMASK(MULTIPLE_BIT_NUM - 1, 0);
		multiple = sfr_access_log_multiples[multiple];
		range = (increment << 2) * multiple;

		pcc_dump("LOG[%d] 0x%08x--0x%08x", pcc, i, paddr, paddr + range);
		pcc_dump("LOG[%d] APB: POST/IDLE(%d) PRE_CONFIG(%d) PRE_START(%d) FRAME(%d)", pcc,
			i, GET_LOG(log, 28), GET_LOG(log, 24), GET_LOG(log, 20), GET_LOG(log, 16));
		pcc_dump("LOG[%d] C_LOADER: PRE_CONFIG(%d) FRAME(%d)", pcc, i, GET_LOG(log, 12),
			GET_LOG(log, 8));
	}
}

static void _dump_perf_mon(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;

	if (!GET_CR(pmio, CMN_CTRL_R_PERF_MONITOR_ENABLE))
		return;

	pcc_dump("PERF_MONITOR ****************", pcc);
	pcc_dump("INT%d: 0x%08x", pcc,
		GET_CR_F(pmio, CMN_CTRL_R_PERF_MONITOR_INT_USER_SEL,
			CMN_CTRL_F_PERF_MONITOR_INT_USER_SEL_INT_ID),
		(u32)BIT_MASK(GET_CR_F(pmio, CMN_CTRL_R_PERF_MONITOR_INT_USER_SEL,
			CMN_CTRL_F_PERF_MONITOR_INT_USER_SEL)));
	pcc_dump("INT_START 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_PERF_MONITOR_INT_START));
	pcc_dump("INT_END 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_PERF_MONITOR_INT_END));
	pcc_dump("INT_USER 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_PERF_MONITOR_INT_USER));
	pcc_dump(
		"PRE_CONFIG 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_PERF_MONITOR_PROCESS_PRE_CONFIG));
	pcc_dump("FRAME 0x%08x", pcc, GET_CR(pmio, CMN_CTRL_R_PERF_MONITOR_PROCESS_FRAME));
}

static void _dump_dbg_full(struct pablo_common_ctrl *pcc)
{
	struct pablo_mmio *pmio = pcc->pmio;
	u32 version;

	version = GET_CR(pmio, CMN_CTRL_R_COMMON_CTRL_VERSION);

	pcc_dump("v%02x.%02x.%02x ====================", pcc,
		 GET_CR_V(pmio, CMN_CTRL_F_CTRL_MAJOR, version),
		 GET_CR_V(pmio, CMN_CTRL_F_CTRL_MINOR, version),
		 GET_CR_V(pmio, CMN_CTRL_F_CTRL_MICRO, version));

	_dump_cotf(pcc);
	_dump_cmdq(pcc);
	_dump_cloader(pcc);
	_dump_int_hist(pcc);
	_dump_dbg_status(pcc);
	_dump_sfr_access_log(pcc);
	_dump_perf_mon(pcc);

	pcc_dump("==============================", pcc);
}

static void _dump_dbg_light(struct pablo_common_ctrl *pcc)
{
	_dump_cotf(pcc);
	_dump_cmdq(pcc);
	_dump_int_hist(pcc);

	pcc_dump("==============================", pcc);
}

static struct dump_dbg_handler dump_dbg_handlers[PCC_DUMP_MODE_NUM] = {
	[PCC_DUMP_FULL] = {
		.func = _dump_dbg_full,
	},
	[PCC_DUMP_LIGHT] = {
		.func = _dump_dbg_light,
	},
};

static void _dbg_dump_handler(struct pablo_common_ctrl *pcc, u32 int_id, u32 status)
{
	const unsigned long mode_msk = (unsigned long)dbg_param_info.mode_msk;

	if (!test_bit(pcc->mode, &mode_msk))
		return;
	else if (!dbg_param_info.int_msk[int_id])
		return;
	else if (!(status & dbg_param_info.int_msk[int_id]))
		return;

	pcc_info("[INT%d] 0x%08x", pcc, int_id, status);
	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
}

/* PCC API functions */
static int pcc_enable(struct pablo_common_ctrl *pcc, const struct pablo_common_ctrl_cfg *cfg)
{
	int ret;

	/**
	 * W/A: Check sw_reset state.
	 *
	 * It guarantees that sw_reset triggered by previous streamoff sequence is finished.
	 * After that, it can set new CR settings for streamon sequence.
	 */
	ret = _wait_sw_reset(pcc, SW_RESET_ALL);
	if (ret)
		return ret;

	_pcc_enable(pcc, cfg);

	pcc->cfg = *cfg;

	pcc_info("fs_mode(%d) int0(0x%08x) int1(0x%08x) cmdq_int(0x%08x) corex_int(0x%08x)", pcc,
		cfg->fs_mode, cfg->int_en[PCC_INT_0], cfg->int_en[PCC_INT_1],
		cfg->int_en[PCC_CMDQ_INT], cfg->int_en[PCC_COREX_INT]);

	return 0;
}

static void pcc_otf_disable(struct pablo_common_ctrl *pcc)
{
	/* Flush CMDQ */
	acquire_cmdq_lock(pcc);
	_flush_cmdq_queue(pcc);
	release_cmdq_lock(pcc);

	/* Disable QCH for HWACG */
	_pcc_set_qch(pcc, false);

	/* Reset CMDQ */
	_sw_reset(pcc, SW_RESET_APB);
	_wait_sw_reset(pcc, SW_RESET_APB);

	pcc_info("", pcc);
}

static void pcc_m2m_disable(struct pablo_common_ctrl *pcc)
{
	/* Flush CMDQ */
	acquire_cmdq_lock(pcc);
	_flush_cmdq_queue(pcc);
	release_cmdq_lock(pcc);

	/* Disable QCH for HWACG */
	_pcc_set_qch(pcc, false);

	pcc_info("", pcc);
}

static int pcc_reset(struct pablo_common_ctrl *pcc)
{
	_sw_reset(pcc, SW_RESET_ALL);

	return _wait_sw_reset(pcc, SW_RESET_ALL);
}

static int pcc_otf_shot(
	struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	int ret;
	memcpy(&pcc->frame_cfg, frame_cfg, sizeof(struct pablo_common_ctrl_frame_cfg));

	acquire_cmdq_lock(pcc);

	/**
	 * Do deferred shot for FRO scenario.
	 * Because '_s_cmd()' flushs the CMDQ, it should be run after setting the last cmd into CMDQ.
	 */
	if (frame_cfg->num_buffers > 1 &&
	    _get_cmdq_queue_fullness(pcc) % frame_cfg->num_buffers != 0) {
		release_cmdq_lock(pcc);
		return 0;
	}

	ret = _prepare_real_cmd(pcc, frame_cfg);
	release_cmdq_lock(pcc);

	return ret;
}

static int pcc_m2m_shot(
	struct pablo_common_ctrl *pcc, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	int ret;

	acquire_cmdq_lock(pcc);
	ret = _prepare_real_cmd(pcc, frame_cfg);
	release_cmdq_lock(pcc);

	return ret;
}

static u32 pcc_get_int_status(struct pablo_common_ctrl *pcc, u32 int_id, bool clear)
{
	u32 int_cr, clr_cr, i;
	ulong status;
	struct irq_handler *handler;

	if (int_id >= PCC_INT_ID_NUM) {
		pcc_err("Invalid INT_ID(%d)", pcc, int_id);
		return 0;
	}

	int_cr = int_cr_maps[int_id].src.cr_offset;
	clr_cr = int_cr_maps[int_id].clr.cr_offset;

	status = _get_int_status(pcc, int_cr, clr_cr, clear);

	for_each_set_bit (i, &status, PCC_IRQ_HANDLER_NUM) {
		handler = &irq_handlers[int_id][i][pcc->mode];

		if (handler->func)
			handler->func(pcc);
	}

	if (dbg_param_info.en[PCC_DBG_DUMP])
		_dbg_dump_handler(pcc, int_id, status);

	if (int_id == PCC_PERF_MON_USER_INT_ID && test_bit(PCC_PERF_MON_USER_INT_BIT, &status))
		SET_CR(pcc->pmio, CMN_CTRL_R_PERF_MONITOR_CLEAR, PCC_PERF_MON_MASK);

	return status;
}

static int pcc_cmp_fcount(struct pablo_common_ctrl *pcc, const u32 fcount)
{
	struct pmio_field *field;
	u32 bitmask;

	field = pmio_get_field(pcc->pmio, CMN_CTRL_F_CMDQ_CURRENT_FRAME_ID);
	bitmask = field->mask >> field->shift;

	return (fcount & bitmask) - pcc->dbg.last_fcount;
}

static void pcc_dump_dbg(struct pablo_common_ctrl *pcc, enum pablo_common_ctrl_dump_mode mode)
{
	dump_dbg_handlers[mode].func(pcc);
}

static void pcc_set_qch(struct pablo_common_ctrl *pcc, bool on)
{
	_pcc_set_qch(pcc, on);
}

static void pcc_set_delay(struct pablo_common_ctrl *pcc, const u32 delay)
{
	_s_post_frame_gap(pcc, delay);
}

static int pcc_recover_otf(struct pablo_common_ctrl *pcc, const u32 fcount)
{
	struct pablo_common_ctrl_frame_cfg *frame_cfg;

	frame_cfg = &pcc->frame_cfg;

	if (frame_cfg->num_buffers > 1)
		return 0;

	/* Flush CMDQ */
	acquire_cmdq_lock(pcc);
	_flush_cmdq_queue(pcc);
	_prepare_dummy_cmd(pcc, &pcc->frame_cfg);
	release_cmdq_lock(pcc);

	return 0;
}

static int pcc_recover_otf_no_dummy(struct pablo_common_ctrl *pcc, const u32 fcount)
{
	struct pablo_common_ctrl_frame_cfg *frame_cfg;
	int ret = 0;

	frame_cfg = &pcc->frame_cfg;

	if (frame_cfg->num_buffers > 1)
		return ret;

	acquire_cmdq_lock(pcc);
	if (GET_CMDQ_STATE(pcc) == CMDQ_PRE_START) {
		pcc_info("%s:%d", pcc, __func__, fcount);
		/* Reset CMDQ */
		_sw_reset(pcc, SW_RESET_APB);
		_wait_sw_reset(pcc, SW_RESET_APB);

		ret = 1;
	}

	release_cmdq_lock(pcc);

	return ret;
}

static const struct pablo_common_ctrl_ops pcc_ops[PCC_MODE_NUM] = {
	/* PCC_OTF */
	{
		.enable = pcc_enable,
		.disable = pcc_otf_disable,
		.reset = pcc_reset,
		.shot = pcc_otf_shot,
		.get_int_status = pcc_get_int_status,
		.cmp_fcount = pcc_cmp_fcount,
		.dump = pcc_dump_dbg,
		.set_qch = NULL,
		.set_delay = pcc_set_delay,
		.recover = pcc_recover_otf,
	},
	/* PCC_M2M */
	{
		.enable = pcc_enable,
		.disable = pcc_m2m_disable,
		.reset = pcc_reset,
		.shot = pcc_m2m_shot,
		.get_int_status = pcc_get_int_status,
		.cmp_fcount = pcc_cmp_fcount,
		.dump = pcc_dump_dbg,
		.set_qch = pcc_set_qch,
		.set_delay = NULL,
	},
	/* PCC_OTF_NO_DUMMY */
	{
		.enable = pcc_enable,
		.disable = pcc_otf_disable,
		.reset = pcc_reset,
		.shot = pcc_otf_shot,
		.get_int_status = pcc_get_int_status,
		.cmp_fcount = pcc_cmp_fcount,
		.dump = pcc_dump_dbg,
		.set_qch = NULL,
		.set_delay = pcc_set_delay,
		.recover = pcc_recover_otf_no_dummy,
	},
};

/* PCC HW functions */
static int pablo_common_ctrl_init(struct pablo_common_ctrl *pcc, struct pablo_mmio *pmio,
	const char *name, enum pablo_common_ctrl_mode mode, struct is_mem *mem)
{
	if (!pcc) {
		pr_err("[@][PCC][ERR]%s:%d:pcc is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!pmio) {
		pcc_err("pmio is NULL", pcc);
		return -EINVAL;
	}

	if (!name) {
		pcc_err("name is NULL", pcc);
		return -EINVAL;
	}

	if (mode >= PCC_MODE_NUM) {
		pcc_err("mode(%d) is out-of-range", pcc, mode);
		return -EINVAL;
	}

	strncpy(pcc->name, name, (sizeof(pcc->name) - 1));
	pcc->mode = mode;
	pcc->ops = &pcc_ops[mode];

	spin_lock_init(&pcc->cmdq_lock);

	spin_lock_init(&pcc->qch_lock);
	pcc->qch_ref_cnt = 0;

	if (_init_pmio(pcc, pmio))
		return -EINVAL;

	switch (mode) {
	case PCC_OTF:
		if (_init_cloader(pcc, mem)) {
			CALL_PCC_HW_OPS(pcc, deinit, pcc);
			return -EINVAL;
		}
		break;
	default:
		/* Nothing to do */
		break;
	};

	pcc_info("mode(%d)", pcc, mode);

	return 0;
}

/* PCC HW functions */
static int pablo_common_ctrl_set_mode(
	struct pablo_common_ctrl *pcc, enum pablo_common_ctrl_mode mode)
{
	if (!pcc) {
		pr_err("[@][PCC][ERR]%s:%d:pcc is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (mode >= PCC_MODE_NUM) {
		pcc_err("mode(%d) is out-of-range", pcc, mode);
		return -EINVAL;
	}

	if (pcc->mode == mode)
		return 0;

	pcc->mode = mode;
	pcc->ops = &pcc_ops[mode];

	pcc_info("mode(%d)", pcc, mode);

	return 0;
}

static void pablo_common_ctrl_deinit(struct pablo_common_ctrl *pcc)
{
	struct pablo_internal_subdev *pis;

	if (!pcc)
		return;

	pcc_info("E", pcc);

	if (pcc->qch_ref_cnt) {
		pcc_warn("remaining qch_ref_cnt(%d)", pcc, pcc->qch_ref_cnt);
		pcc->qch_ref_cnt = 1;
		_pcc_set_qch(pcc, false);
	}

	pis = &pcc->subdev_cloader;
	if (test_bit(PABLO_SUBDEV_ALLOC, &pis->state))
		CALL_I_SUBDEV_OPS(pis, free, pis);

	if (pcc->fields) {
		pmio_field_bulk_free(pcc->pmio, pcc->fields);
		pcc->fields = NULL;
	}

	if (pcc->pmio) {
		pmio_exit(pcc->pmio);
		pcc->pmio = NULL;
	}
}

static const struct pablo_common_ctrl_hw_ops pcc_hw_ops = {
	.init = pablo_common_ctrl_init,
	.set_mode = pablo_common_ctrl_set_mode,
	.deinit = pablo_common_ctrl_deinit,
};

/* Driver functions */
static int pablo_common_ctrl_api_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pablo_common_ctrl *pcc;

	pcc = (struct pablo_common_ctrl *)platform_get_drvdata(pdev);
	if (!pcc) {
		dev_err(dev, "Failed to get pcc\n");
		return -ENODEV;
	}

	pcc->hw_ops = &pcc_hw_ops;

	dev_info(dev, "%s done\n", __func__);

	return 0;
}

static int pablo_common_ctrl_api_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id pablo_common_ctrl_api_of_table_v1_4[] = {
	{
		.name = "pablo_common_ctrl_v1.4",
		.compatible = "samsung,pablo-common-ctrl-v1.4",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pablo_common_ctrl_api_of_table_v1_4);

static struct platform_driver pablo_common_ctrl_api_driver = {
	.probe = pablo_common_ctrl_api_probe,
	.remove = pablo_common_ctrl_api_remove,
	.driver = {
		.name = "pablo_common_ctrl_v1.4",
		.owner = THIS_MODULE,
		.of_match_table = pablo_common_ctrl_api_of_table_v1_4,
	},
};

struct platform_driver *pablo_common_ctrl_api_get_platform_driver_v1_4(void)
{
	return &pablo_common_ctrl_api_driver;
}
KUNIT_EXPORT_SYMBOL(pablo_common_ctrl_api_get_platform_driver_v1_4);

#ifndef MODULE
static int __init pablo_common_ctrl_api_init(void)
{
	int ret;

	ret = platform_driver_probe(&pablo_common_ctrl_api_driver, pablo_common_ctrl_api_probe);
	if (ret)
		pr_err("%s: platform_driver_probe is failed(%d)", __func__, ret);

	return ret;
}
device_initcall_sync(pablo_common_ctrl_api_init);
#endif
