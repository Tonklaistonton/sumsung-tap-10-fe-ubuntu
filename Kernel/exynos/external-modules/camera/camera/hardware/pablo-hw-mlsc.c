// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw.h"
#include "is-core.h"
#include "pablo-hw-mlsc.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

#define GET_HW(hw_ip) ((struct pablo_hw_mlsc *)hw_ip->priv_info)
#define GET_EN_STR(en) (en ? "ON" : "OFF")

static DEFINE_MUTEX(cmn_reg_lock);

/* DEBUG module params */
enum pablo_hw_mlsc_dbg_type {
	MLSC_DBG_NONE = 0,
	MLSC_DBG_S_TRIGGER,
	MLSC_DBG_CR_DUMP,
	MLSC_DBG_STATE_DUMP,
	MLSC_DBG_S2D,
	MLSC_DBG_APB_DIRECT,
	MLSC_DBG_DTP,
	MLSC_DBG_RTA_DUMP,
	MLSC_DBG_TYPE_NUM,
};

struct pablo_hw_mlsc_dbg_info {
	bool en[MLSC_DBG_TYPE_NUM];
	u32 option[MLSC_DBG_TYPE_NUM];
	u32 instance;
	u32 fcount;
	u32 int_msk[2];
	ulong dma_id;
};

static struct pablo_hw_mlsc_dbg_info dbg_info = {
	.en = { false, },
	.instance = 0,
	.fcount = 0,
	.int_msk = { 0, },
};

static struct is_mlsc_config default_config = {
	.svhist_bypass = 1,
	.lmeds_bypass = 1,
};

static int _parse_dbg_s_trigger(char **argv, int argc)
{
	int ret;
	u32 instance, fcount, int_msk0, int_msk1;

	if (argc != 4) {
		err("[DBG_MLSC] Invalid arguments! %d", argc);
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &instance);
	if (ret) {
		err("[DBG_MLSC] Invalid instance %d ret %d", instance, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[1], 0, &fcount);
	if (ret) {
		err("[DBG_MLSC] Invalid fcount %d ret %d", fcount, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[2], 0, &int_msk0);
	if (ret) {
		err("[DBG_MLSC] Invalid int_msk0 %d ret %d", int_msk0, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[3], 0, &int_msk1);
	if (ret) {
		err("[DBG_MLSC] Invalid int_msk1 %d ret %d", int_msk1, ret);
		return -EINVAL;
	}

	dbg_info.instance = instance;
	dbg_info.fcount = fcount;
	dbg_info.int_msk[0] = int_msk0;
	dbg_info.int_msk[1] = int_msk1;

	info("[DBG_MLSC] S_TRIGGER:[I%d][F%d] INT0 0x%08x INT1 0x%08x\n", dbg_info.instance,
		dbg_info.fcount, dbg_info.int_msk[0], dbg_info.int_msk[1]);

	return 0;
}

typedef int (*dbg_parser)(char **argv, int argc);
struct dbg_parser_info {
	char name[NAME_MAX];
	char man[NAME_MAX];
	dbg_parser parser;
	u32 option_default;
};

static struct dbg_parser_info dbg_parsers[MLSC_DBG_TYPE_NUM] = {
	[MLSC_DBG_S_TRIGGER] = {
		"S_TRIGGER",
		"<instance> <fcount> <int_msk0> <int_msk1> ",
		_parse_dbg_s_trigger,
	},
	[MLSC_DBG_CR_DUMP] = {
		"CR_DUMP",
		"",
		NULL,
		1,
	},
	[MLSC_DBG_STATE_DUMP] = {
		"STATE_DUMP",
		"",
		NULL,
	},
	[MLSC_DBG_S2D] = {
		"S2D",
		"",
		NULL,
	},
	[MLSC_DBG_APB_DIRECT] = {
		"APB_DIRECT",
		"",
		NULL,
	},
	[MLSC_DBG_DTP] = {
		"DTP",
		"",
		NULL,
	},
	[MLSC_DBG_RTA_DUMP] = {
		"RTA_DUMP",
		"",
		NULL,
	},
};

static int pablo_hw_mlsc_dbg_set(const char *val)
{
	int ret = 0, argc = 0;
	char **argv;
	u32 dbg_type, en, arg_i = 0, option;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("[DBG_MLSC] No argument!");
		return -EINVAL;
	} else if (argc < 2) {
		err("[DBG_MLSC] Too short argument!");
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &dbg_type);
	if (ret || !dbg_type || dbg_type >= MLSC_DBG_TYPE_NUM) {
		err("[DBG_MLSC] Invalid dbg_type %u ret %d", dbg_type, ret);
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &en);
	if (ret) {
		err("[DBG_MLSC] Invalid en %u ret %d", en, ret);
		goto func_exit;
	}

	dbg_info.en[dbg_type] = en;

	dbg_info.option[dbg_type] = dbg_parsers[dbg_type].option_default;

	if (argc > 2) {
		ret = kstrtouint(argv[arg_i++], 0, &option);
		if (ret) {
			err("[DBG_MLSC] Invalid option %u ret %d", option, ret);
			goto func_exit;
		}
		dbg_info.option[dbg_type] = option;
	}

	info("[DBG_MLSC] %s[%s] option: %d\n", dbg_parsers[dbg_type].name,
		GET_EN_STR(dbg_info.en[dbg_type]), dbg_info.option[dbg_type]);

	argc = (argc > arg_i) ? (argc - arg_i) : 0;

	if (argc && dbg_parsers[dbg_type].parser &&
		dbg_parsers[dbg_type].parser(&argv[arg_i], argc)) {
		err("[DBG_MLSC] Failed to %s", dbg_parsers[dbg_type].name);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_hw_mlsc_dbg_get(char *buffer, const size_t buf_size)
{
	const char *get_msg = "= MLSC DEBUG Configuration =====================\n"
			      "  Current Trigger Point:\n"
			      "    - instance %d\n"
			      "    - fcount %d\n"
			      "    - int0_msk 0x%08x int1_msk 0x%08x\n"
			      "================================================\n";

	return scnprintf(buffer, buf_size, get_msg, dbg_info.instance, dbg_info.fcount,
		dbg_info.int_msk[0], dbg_info.int_msk[1]);
}

static int pablo_hw_mlsc_dbg_usage(char *buffer, const size_t buf_size)
{
	int ret;
	u32 dbg_type;

	ret = scnprintf(buffer, buf_size, "[value] string value, MLSC debug features\n");
	for (dbg_type = 1; dbg_type < MLSC_DBG_TYPE_NUM; dbg_type++)
		ret += scnprintf(buffer + ret, buf_size - ret,
			"  - %10s[%3s]: echo %d <en> %s> debug_mlsc\n", dbg_parsers[dbg_type].name,
			GET_EN_STR(dbg_info.en[dbg_type]), dbg_type, dbg_parsers[dbg_type].man);

	return ret;
}

static struct pablo_debug_param debug_mlsc = {
	.type = IS_DEBUG_PARAM_TYPE_STR,
	.ops.set = pablo_hw_mlsc_dbg_set,
	.ops.get = pablo_hw_mlsc_dbg_get,
	.ops.usage = pablo_hw_mlsc_dbg_usage,
};

module_param_cb(debug_mlsc, &pablo_debug_param_ops, &debug_mlsc, 0644);

static void _dbg_handler(struct is_hw_ip *hw_ip, u32 irq_id, u32 status)
{
	struct pablo_hw_mlsc *hw;
	struct pablo_common_ctrl *pcc;
	u32 instance, fcount;

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;
	instance = atomic_read(&hw_ip->instance);
	fcount = atomic_read(&hw_ip->fcount);

	if (dbg_info.instance && dbg_info.instance != instance)
		return;

	if (dbg_info.fcount && dbg_info.fcount != fcount)
		return;

	if (dbg_info.int_msk[irq_id] && !(dbg_info.int_msk[irq_id] & status))
		return;

	info("[DBG_MLSC] %s:[I%d][F%d] INT%d 0x%08x\n", __func__, instance, fcount, irq_id, status);

	if (dbg_info.en[MLSC_DBG_CR_DUMP])
		mlsc_hw_dump(hw_ip->pmio, HW_DUMP_CR);

	if (dbg_info.en[MLSC_DBG_STATE_DUMP]) {
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		mlsc_hw_dump(hw_ip->pmio, HW_DUMP_CR);
	}

	if (dbg_info.en[MLSC_DBG_S2D])
		is_debug_s2d(true, "MLSC_DBG_S2D");
}

/* MLSC HW OPS */
static int _pablo_hw_mlsc_cloader_init(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_mlsc *hw;
	struct is_mem *mem;
	struct pablo_internal_subdev *pis;
	int ret;
	enum base_reg_index reg_id;
	resource_size_t reg_size;
	u32 reg_num, hdr_size;

	hw = GET_HW(hw_ip);
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_MLSC0);
	pis = &hw->subdev_cloader;
	ret = pablo_internal_subdev_probe(pis, instance, mem, "CLOADER");
	if (ret) {
		mserr_hw("failed to probe internal sub-device for %s: %d", instance, hw_ip,
			pis->name, ret);
		return ret;
	}

	reg_id = REG_SETA;
	reg_size = hw_ip->regs_end[reg_id] - hw_ip->regs_start[reg_id] + 1;
	reg_num = DIV_ROUND_UP(reg_size, 4); /* 4 Bytes per 1 CR. */
	hw->header_size = hdr_size = ALIGN(reg_num, 16); /* Header: 16 Bytes per 16 CRs. */
	pis->width = hdr_size + reg_size;
	pis->height = 1;
	pis->num_planes = 1;
	pis->num_batch = 1;
	pis->num_buffers = 2;
	pis->bits_per_pixel = BITS_PER_BYTE;
	pis->memory_bitwidth = BITS_PER_BYTE;
	pis->size[0] = ALIGN(DIV_ROUND_UP(pis->width * pis->memory_bitwidth, BITS_PER_BYTE), 32) *
		       pis->height;
	ret = CALL_I_SUBDEV_OPS(pis, alloc, pis);
	if (ret) {
		mserr_hw("failed to alloc internal sub-device for %s: %d", instance, hw_ip,
			pis->name, ret);
		return ret;
	}

	return ret;
}

static inline int _pablo_hw_mlsc_pcc_init(struct is_hw_ip *hw_ip)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct is_mem *mem;
	enum pablo_common_ctrl_mode pcc_mode;

	hw->pcc = pablo_common_ctrl_hw_get_pcc(&hw_ip->pmio_config);
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_MLSC0);

	pcc_mode = PCC_OTF_NO_DUMMY;

	return CALL_PCC_HW_OPS(hw->pcc, init, hw->pcc, hw_ip->pmio, hw_ip->name, pcc_mode, mem);
}

static int is_hw_mlsc_prepare(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_mlsc *hw;
	struct pablo_mmio *pmio;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_cfg cfg = {
		0,
	};

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;
	pmio = hw_ip->pmio;

	cfg.fs_mode = PCC_VVALID_RISE;
	mlsc_hw_g_int_en(cfg.int_en);
	if (CALL_PCC_OPS(pcc, enable, pcc, &cfg)) {
		mserr_hw("failed to PCC enable", instance, hw_ip);
		return -EINVAL;
	}

	mlsc_hw_init(pmio);

	return 0;
}

static int pablo_hw_mlsc_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct pablo_hw_mlsc *hw;
	u32 reg_cnt = mlsc_hw_g_reg_cnt();

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, "HWMLSC");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME, false);

	hw_ip->priv_info = pablo_zalloc(sizeof(struct pablo_hw_mlsc), GFP_KERNEL);
	hw_ip->ch = hw_ip->id - DEV_HW_MLSC0;

	hw = GET_HW(hw_ip);
	if (!hw) {
		mserr_hw("Failed to alloc pablo_hw_mlsc", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw->iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	if (!hw->iq_set.regs) {
		mserr_hw("failed to alloc iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_regs_alloc;
	}

	hw->cur_iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	if (!hw->cur_iq_set.regs) {
		mserr_hw("failed to alloc cur_iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_cur_regs_alloc;
	}

	clear_bit(CR_SET_CONFIG, &hw->iq_set.state);
	set_bit(CR_SET_EMPTY, &hw->iq_set.state);
	spin_lock_init(&hw->iq_set.slock);

	/* Setup C-loader */
	ret = _pablo_hw_mlsc_cloader_init(hw_ip, instance);
	if (ret)
		return ret;

	/* Setup Common-CTRL */
	ret = _pablo_hw_mlsc_pcc_init(hw_ip);
	if (ret)
		return ret;

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open\n", instance, hw_ip);

	return 0;

err_cur_regs_alloc:
	pablo_free(hw->iq_set.regs);
	hw->iq_set.regs = NULL;

err_regs_alloc:
	pablo_free(hw);
	hw_ip->priv_info = NULL;

err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int pablo_hw_mlsc_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	int ret = 0;
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	u32 dma_id;

	for (dma_id = MLSC_R_CL; dma_id < MLSC_DMA_NUM; dma_id++) {
		if (mlsc_hw_create_dma(hw_ip->pmio, dma_id, &hw->dma[dma_id])) {
			mserr_hw("[D%d] create_dma error", instance, hw_ip, dma_id);
			ret = -ENODATA;
		}
	}

	hw->is_reprocessing = flag;

	set_bit(HW_INIT, &hw_ip->state);
	hw->param_set[instance].reprocessing = flag;

	msdbg_hw(2, "init\n", instance, hw_ip);

	return ret;
}

static int pablo_hw_mlsc_close(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct pablo_hw_mlsc *hw;
	struct pablo_internal_subdev *pis;
	struct is_hw_ip *hw_ip_phys;
	int i, max_num;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw = GET_HW(hw_ip);

	pablo_free(hw->iq_set.regs);
	hw->iq_set.regs = NULL;
	pablo_free(hw->cur_iq_set.regs);
	hw->cur_iq_set.regs = NULL;

	ret = mlsc_hw_wait_idle(hw_ip->pmio);
	if (ret)
		mserr_hw("failed to wait_idle. ret %d", instance, hw_ip, ret);

	max_num = hw_ip->ip_num;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip, DEV_HW_MLSC0 + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num) {
		for (i = max_num - 1; i >= 0; i--) {
			hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip,
							    DEV_HW_MLSC0 + i);
			mlsc_hw_s_reset(hw_ip_phys->pmio);
		}
	}
	mutex_unlock(&cmn_reg_lock);

	CALL_PCC_HW_OPS(hw->pcc, deinit, hw->pcc);

	pis = &hw->subdev_cloader;
	CALL_I_SUBDEV_OPS(pis, free, pis);

	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "close\n", instance, hw_ip);

	return ret;
}

static int pablo_hw_mlsc_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_mmio *pmio;
	struct pablo_hw_mlsc *hw;
	u32 i, max_num, dma_id;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw = GET_HW(hw_ip);
	pmio = hw_ip->pmio;
	hw->event_state = MLSC_INIT;
	max_num = hw_ip->ip_num;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		struct is_hw_ip *hw_ip_phys =
			CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip, DEV_HW_MLSC0 + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num) {
		mlsc_hw_s_lbctrl(hw_ip->locomotive->pmio);

		for (dma_id = MLSC_W_YUV444_Y; dma_id < MLSC_DMA_NUM; dma_id++)
			mlsc_hw_s_dma_debug(hw_ip->locomotive->pmio, dma_id);
	}
	mutex_unlock(&cmn_reg_lock);

	hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
	if (pmio_reinit_cache(pmio, &hw_ip->pmio_config)) {
		mserr_hw("failed to reinit PMIO cache", instance, hw_ip);
		return -EINVAL;
	}

	if (unlikely(dbg_info.en[MLSC_DBG_APB_DIRECT])) {
		pmio_cache_set_only(pmio, false);
		pmio_cache_set_bypass(pmio, true);
	} else {
		pmio_cache_set_only(pmio, true);
	}

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static void pablo_hw_mlsc_clear(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);

	memset(&hw->config, 0x00, sizeof(struct is_mlsc_config));
	hw->cur_iq_set.size = 0;
	hw->event_state = MLSC_INIT;
	frame_manager_flush(GET_SUBDEV_I_FRAMEMGR(&hw->subdev_cloader));
}

static int pablo_hw_mlsc_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct pablo_hw_mlsc *hw;
	struct pablo_mmio *pmio;
	struct pablo_common_ctrl *pcc;
	u32 hw_ip_instance;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw("disable: %s\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid) == V_VALID ? "V_VALID" : "V_BLANK");

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;
	pmio = hw_ip->pmio;
	hw_ip_instance = atomic_read(&hw_ip->instance);

	if (hw_ip->run_rsc_state) {
		if (hw->param_set[instance].reprocessing == false) {
			if ((hw_ip_instance == instance) && (test_bit(HW_CONFIG, &hw_ip->state))) {
				goto out;
			} else if (test_bit(hw_ip_instance, &hw_ip->run_rsc_state) &&
				   hw->param_set[hw_ip_instance].reprocessing == false) {
				mswarn_hw("Occupied by S%d", instance, hw_ip,
					atomic_read(&hw_ip->instance));
				return -EWOULDBLOCK;
			}
		}
		return ret;
	}

	clear_bit(HW_RUN, &hw_ip->state);
out:
	timetowait = wait_event_timeout(
		hw_ip->status.wait_queue, !atomic_read(&hw_ip->status.Vvalid), IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout. timetowait %uus", instance, hw_ip,
			jiffies_to_usecs(IS_HW_STOP_TIMEOUT));
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		mlsc_hw_dump(pmio, HW_DUMP_DBG_STATE);
		ret = -ETIME;
	}

	pablo_hw_mlsc_clear(hw_ip, instance);

	/* Disable PMIO Cache */
	pmio_cache_set_only(pmio, false);
	pmio_cache_set_bypass(pmio, true);

	mlsc_hw_s_otf(hw_ip->pmio, false);
	CALL_PCC_OPS(pcc, disable, pcc);

	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void _pablo_hw_mlsc_init_config(struct is_hw_ip *hw_ip)
{
	struct pablo_hw_mlsc *hw;

	hw = GET_HW(hw_ip);

	atomic_set(&hw->start_fcount, 0);

	if (unlikely(dbg_info.en[MLSC_DBG_DTP]))
		mlsc_hw_s_dtp(hw_ip->pmio);
}

static int _pablo_hw_mlsc_s_lic_config(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_mlsc *hw;
	struct mlsc_param_set *p_set;
	struct param_otf_input *otf_in;
	struct param_dma_input *dma_in;
	struct mlsc_lic_cfg lic_cfg;

	hw = GET_HW(hw_ip);
	p_set = &hw->param_set[instance];

	otf_in = &p_set->otf_input;
	dma_in = &p_set->dma_input;

	if (otf_in->cmd == OTF_INPUT_COMMAND_ENABLE) {
		hw->input = OTF;
	} else if (dma_in->v_otf_enable == DMA_INPUT_VOTF_ENABLE) {
		hw->input = VOTF;
	} else if (dma_in->cmd == DMA_INPUT_COMMAND_ENABLE) {
		hw->input = DMA;
	} else {
		mserr_hw("No input", instance, hw_ip);
		return -EINVAL;
	}

	lic_cfg.bypass = 0;
	lic_cfg.input_path = hw->input;

	mlsc_hw_s_lic_cfg(hw_ip->pmio, &lic_cfg);

	msdbg_hw(2, "lic_config: %s_IN\n", atomic_read(&hw_ip->instance), hw_ip,
		(hw->input == OTF) ? "OTF" : "DMA");

	return 0;
}

static int _pablo_hw_mlsc_s_ds(struct is_hw_ip *hw_ip, struct mlsc_param_set *p_set)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct mlsc_size_cfg *size_cfg;
	u32 dma_id;
	int ret = 0;

	size_cfg = &hw->size;

	/* TODO */
	size_cfg->rms_crop_ratio = 10; /* x1.0 */

	for (dma_id = MLSC_W_FDPIG; dma_id < MLSC_DMA_NUM; dma_id++)
		ret |= mlsc_hw_s_ds_cfg(hw_ip->pmio, dma_id, size_cfg, &hw->config, p_set);

	FIMC_BUG(ret);

	return 0;
}

static int _pablo_hw_mlsc_s_dma(struct is_hw_ip *hw_ip, struct mlsc_param_set *p_set)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	u32 dma_id;
	int disable;

	/* RDMA cfg */
	for (dma_id = MLSC_R_Y; dma_id < MLSC_RDMA_NUM; dma_id++) {
		if (mlsc_hw_s_rdma_cfg(&hw->dma[dma_id], p_set, (hw_ip->num_buffers & 0xffff)))
			return -EINVAL;
	}

	/* WDMA cfg */
	for (dma_id = MLSC_W_YUV444_Y; dma_id < MLSC_DMA_NUM; dma_id++) {
		disable = test_bit(dma_id, &dbg_info.dma_id);
		if (mlsc_hw_s_wdma_cfg(
			    &hw->dma[dma_id], p_set, (hw_ip->num_buffers & 0xffff), disable))
			return -EINVAL;
	}

	return 0;
}

static void pablo_hw_mlsc_internal_shot(
	struct is_hw_ip *hw_ip, struct is_frame *frame, struct mlsc_param_set *p_set)
{
	u32 instance = atomic_read(&hw_ip->instance);

	hw_ip->internal_fcount[instance] = frame->fcount;

	/* Disable ALL WDMAs */
	p_set->dma_output_yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_glpg[0].cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_glpg[1].cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_glpg[2].cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_glpg[3].cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_glpg[4].cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_lme_ds.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_fdpig.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	p_set->dma_output_cav.cmd = DMA_OUTPUT_COMMAND_DISABLE;
}

static void _pablo_hw_mlsc_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
	struct mlsc_param_set *p_set, IS_DECLARE_PMAP(pmap), u32 instance, u32 ctx)
{
	struct mlsc_param *param;
	u32 pindex;

	param = &p_region->mlsc;
	p_set->instance_id = instance;

	/* Sensor info CFG */
	pindex = PARAM_SENSOR_CONFIG;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->sensor_config, &p_region->sensor.config,
			sizeof(struct param_sensor_config));

	/* RGBP otf info CFG */
	pindex = PARAM_RGBP_OTF_INPUT;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->rgbp_otf_input, &p_region->rgbp.otf_input,
			sizeof(struct param_otf_input));

	/* Input CFG */
	pindex = PARAM_MLSC_OTF_INPUT;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->otf_input, &param->otf_input, sizeof(struct param_otf_input));

	pindex = PARAM_MLSC_DMA_INPUT;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_input, &param->dma_input, sizeof(struct param_dma_input));

	/* Output CFG */
	pindex = PARAM_MLSC_YUV444;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_yuv, &param->yuv, sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_GLPG0;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_glpg[0], &param->glpg[0],
			sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_GLPG1;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_glpg[1], &param->glpg[1],
			sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_GLPG2;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_glpg[2], &param->glpg[2],
			sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_GLPG3;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_glpg[3], &param->glpg[3],
			sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_GLPG4;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_glpg[4], &param->glpg[4],
			sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_SVHIST;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_svhist, &param->svhist, sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_LMEDS;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_lme_ds, &param->lme_ds, sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_FDPIG;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_fdpig, &param->fdpig, sizeof(struct param_dma_output));

	pindex = PARAM_MLSC_CAV;
	if (test_bit(pindex, pmap))
		memcpy(&p_set->dma_output_cav, &param->cav, sizeof(struct param_dma_output));
}

static void pablo_hw_mlsc_external_shot(
	struct is_hw_ip *hw_ip, struct is_frame *frame, struct mlsc_param_set *p_set)
{
	struct is_param_region *p_region;
	u32 b_idx = frame->cur_buf_index;
	u32 instance = atomic_read(&hw_ip->instance);
	u32 ctx_idx;

	FIMC_BUG_VOID(!frame->shot);

	hw_ip->internal_fcount[instance] = 0;
	/*
	 * Due to async shot,
	 * it should refer the param_region from the current frame,
	 * not from the device region.
	 */
	p_region = frame->parameter;

	/* For now, it only considers 2 contexts */
	if (hw_ip->frame_type == LIB_FRAME_HDR_SHORT)
		ctx_idx = 1;
	else
		ctx_idx = 0;

	_pablo_hw_mlsc_update_param(hw_ip, p_region, p_set, frame->pmap, instance, ctx_idx);

	/* DMA settings */
	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_byr_in", hw_ip, frame, b_idx, frame->num_buffers,
		&p_set->dma_input.cmd, p_set->dma_input.plane, p_set->input_dva,
		frame->dvaddr_buffer);

	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_yuv_out", hw_ip, frame, 0, frame->num_buffers,
		&p_set->dma_output_yuv.cmd, p_set->dma_output_yuv.plane, p_set->output_dva_yuv,
		frame->dva_mlsc_yuv444);

	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_glpg0", hw_ip, frame, 0, frame->num_buffers,
		&p_set->dma_output_glpg[0].cmd, p_set->dma_output_glpg[0].plane,
		p_set->output_dva_glpg[0], frame->dva_mlsc_glpg[0]);
	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_glpg1", hw_ip, frame, 0, frame->num_buffers,
		&p_set->dma_output_glpg[1].cmd, p_set->dma_output_glpg[1].plane,
		p_set->output_dva_glpg[1], frame->dva_mlsc_glpg[1]);
	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_glpg2", hw_ip, frame, 0, frame->num_buffers,
		&p_set->dma_output_glpg[2].cmd, p_set->dma_output_glpg[2].plane,
		p_set->output_dva_glpg[2], frame->dva_mlsc_glpg[2]);
	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_glpg3", hw_ip, frame, 0, frame->num_buffers,
		&p_set->dma_output_glpg[3].cmd, p_set->dma_output_glpg[3].plane,
		p_set->output_dva_glpg[3], frame->dva_mlsc_glpg[3]);
	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_glpg4", hw_ip, frame, 0, frame->num_buffers,
		&p_set->dma_output_glpg[4].cmd, p_set->dma_output_glpg[4].plane,
		p_set->output_dva_glpg[4], frame->dva_mlsc_glpg[4]);

	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_svhist", hw_ip, frame, 0, 1,
		&p_set->dma_output_svhist.cmd, p_set->dma_output_svhist.plane,
		p_set->output_dva_svhist, frame->dva_mlsc_svhist);

	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_lme_ds", hw_ip, frame, 0, 1,
		&p_set->dma_output_lme_ds.cmd, p_set->dma_output_lme_ds.plane,
		p_set->output_dva_lme_ds, frame->dva_mlsc_lmeds);

	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_fdpig", hw_ip, frame, 0, 1, &p_set->dma_output_fdpig.cmd,
		p_set->dma_output_fdpig.plane, p_set->output_dva_fdpig, frame->dva_mlsc_fdpig);

	CALL_HW_OPS(hw_ip, dma_cfg, "mlsc_cav", hw_ip, frame, 0, 1, &p_set->dma_output_cav.cmd,
		p_set->dma_output_cav.plane, p_set->output_dva_cav, frame->dva_mlsc_cav);
}

static int _pablo_hw_mlsc_s_iq_regs(
	struct is_hw_ip *hw_ip, u32 instance, struct c_loader_buffer *clb)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct pablo_mmio *pmio;
	struct pablo_hw_mlsc_iq *iq_set = NULL;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size, i;
	bool configured = false;

	pmio = hw_ip->pmio;
	iq_set = &hw->iq_set;

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;

		if (unlikely(dbg_info.en[MLSC_DBG_RTA_DUMP])) {
			msinfo_hw("RTA CR DUMP\n", instance, hw_ip);
			for (i = 0; i < regs_size; i++)
				msinfo_hw("reg:[0x%04X], value:[0x%08X]\n", instance, hw_ip,
					regs[i].reg_addr, regs[i].reg_data);
		}

		if (clb) {
			pmio_cache_fsync_ext(hw_ip->pmio, clb, regs, regs_size);
			if (clb->num_of_pairs > 0)
				clb->num_of_headers++;
		} else {
			for (i = 0; i < regs_size; i++)
				PMIO_SET_R(pmio, regs[i].reg_addr, regs[i].reg_data);
		}

		memcpy(hw->cur_iq_set.regs, regs, sizeof(struct cr_set) * regs_size);
		hw->cur_iq_set.size = regs_size;

		set_bit(CR_SET_EMPTY, &iq_set->state);
		configured = true;
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	if (!configured) {
		mswarn_hw("[F%d]iq_set is NOT configured. iq_set (%d/0x%lx) cur_iq_set %d",
			instance, hw_ip, atomic_read(&hw_ip->fcount), iq_set->fcount, iq_set->state,
			hw->cur_iq_set.fcount);
		return -EINVAL;
	}

	return 0;
}

static void _pablo_hw_mlsc_s_cmd(struct pablo_hw_mlsc *hw, struct c_loader_buffer *clb, u32 fcount,
	struct pablo_common_ctrl_cmd *cmd)
{
	cmd->set_mode = PCC_APB_DIRECT;
	cmd->fcount = fcount;
	cmd->int_grp_en = mlsc_hw_g_int_grp_en();

	if (!clb || unlikely(dbg_info.en[MLSC_DBG_APB_DIRECT]))
		return;

	cmd->base_addr = clb->header_dva;
	cmd->header_num = clb->num_of_headers;
	cmd->set_mode = PCC_DMA_DIRECT;
}

static int pablo_hw_mlsc_set_config(
	struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *config)
{
	struct pablo_hw_mlsc *hw_mlsc;
	struct is_mlsc_config *mlsc_config = (struct is_mlsc_config *)config;

	FIMC_BUG(!config);

	hw_mlsc = GET_HW(hw_ip);

	if (hw_mlsc->config.svhist_bypass != mlsc_config->svhist_bypass ||
		hw_mlsc->config.svhist_grid_num != mlsc_config->svhist_grid_num)
		msinfo_hw("[F:%d] MLSC svhist_bypass(%d) svhist_grid_num(%d)", instance, hw_ip,
			fcount, mlsc_config->svhist_bypass, mlsc_config->svhist_grid_num);

	if (hw_mlsc->config.lmeds_bypass != mlsc_config->lmeds_bypass ||
		hw_mlsc->config.lmeds_stride != mlsc_config->lmeds_stride ||
		hw_mlsc->config.lmeds_w != mlsc_config->lmeds_w ||
		hw_mlsc->config.lmeds_h != mlsc_config->lmeds_h)
		msinfo_hw("[F:%d] MLSC lmeds_bypass(%d), lmeds_stride(%d), lmeds(%d x %d)\n",
			instance, hw_ip, fcount, mlsc_config->lmeds_bypass,
			mlsc_config->lmeds_stride, mlsc_config->lmeds_w, mlsc_config->lmeds_h);

	if (hw_mlsc->config.nr_iteration_mode != mlsc_config->nr_iteration_mode)
		msinfo_hw("[F:%d] MLSC nr_iteration_mode(%d)", instance, hw_ip, fcount,
			  mlsc_config->nr_iteration_mode);

	memcpy(&hw_mlsc->config, config, sizeof(struct is_mlsc_config));

	return 0;
}

static void _pablo_hw_mlsc_save_rta_cfg(struct is_hw_ip *hw_ip, struct is_frame *frame,
					u32 instance)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct pablo_hw_mlsc_iq *iq_set = &hw->iq_set;
	struct size_cr_set *kva_scs;
	void *kva_config;

	msdbg_hw(2, "[F%d]%s RNR 1st stream. save rta cfg for 2nd stream\n", instance, hw_ip,
		 frame->fcount, __func__);

	/* Save CR_SET only when kva is valid */
	kva_scs = (struct size_cr_set *)frame->kva_mlsc_rta_info[PLANE_INDEX_CR_SET];
	if (kva_scs) {
		memcpy(kva_scs->cr, iq_set->regs, sizeof(struct cr_set) * iq_set->size);
		kva_scs->size = iq_set->size;
	}

	/* Save CONFIG only when kva is valid */
	kva_config = (struct is_mlsc_config *)frame->kva_mlsc_rta_info[PLANE_INDEX_CONFIG];
	if (kva_config)
		memcpy(kva_config + sizeof(u32), &hw->config, sizeof(struct is_mlsc_config));
}

static int pablo_hw_mlsc_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	u32 ret, instance, fcount, cur_idx;
	struct pablo_hw_mlsc *hw;
	struct pablo_mmio *pmio;
	struct mlsc_param_set *p_set;
	u32 b_idx;
	struct is_framemgr *framemgr;
	struct is_frame *cl_frame;
	struct is_priv_buf *pb;
	struct c_loader_buffer clb, *p_clb = NULL;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_frame_cfg frame_cfg;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	u32 binned_sensor_width, binned_sensor_height;
	enum pablo_common_ctrl_mode pcc_mode;
	bool do_blk_cfg;
	bool skip = false;
	struct is_group *group;

	instance = frame->instance;
	fcount = frame->fcount;
	cur_idx = frame->cur_buf_index;

	msdbgs_hw(2, "[F%d]shot(%d)\n", instance, hw_ip, fcount, cur_idx);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw = GET_HW(hw_ip);
	pmio = hw_ip->pmio;
	pcc = hw->pcc;
	group = hw_ip->group[instance];

	/* HW parameter */
	p_set = &hw->param_set[instance];
	p_set->fcount = fcount;
	b_idx = frame->cur_buf_index;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		pmio_cache_set_bypass(pmio, false);
		pmio_cache_set_only(pmio, true);
		is_hw_mlsc_prepare(hw_ip, instance);
	}

	if (!test_bit(HW_OTF, &hw_ip->state))
		pcc_mode = PCC_M2M;
	else
		pcc_mode = PCC_OTF_NO_DUMMY;

	CALL_PCC_HW_OPS(pcc, set_mode, pcc, pcc_mode);

	/* Prepare C-Loader buffer */
	framemgr = GET_SUBDEV_I_FRAMEMGR(&hw->subdev_cloader);
	cl_frame = peek_frame(framemgr, FS_FREE);
	if (likely(!dbg_info.en[MLSC_DBG_APB_DIRECT] && cl_frame)) {
		memset(&clb, 0x00, sizeof(clb));
		clb.header_dva = cl_frame->dvaddr_buffer[0];
		clb.payload_dva = cl_frame->dvaddr_buffer[0] + hw->header_size;
		clb.clh = (struct c_loader_header *)cl_frame->kvaddr_buffer[0];
		clb.clp = (struct c_loader_payload *)(cl_frame->kvaddr_buffer[0] + hw->header_size);

		p_clb = &clb;
		trans_frame(framemgr, cl_frame, FS_PROCESS);
	} else {
		mswarn_hw("[F%d]Failed to get cl_frame", instance, hw_ip, frame->fcount);
		frame_manager_print_queues(framemgr);
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		pablo_hw_mlsc_internal_shot(hw_ip, frame, p_set);
		hw->radial_cfg.sensor_binning_x = p_set->sensor_config.sensor_binning_ratio_x;
		hw->radial_cfg.sensor_binning_y = p_set->sensor_config.sensor_binning_ratio_y;
		/* Total_crop = unbinned_sensor_crop */
		if (p_set->sensor_config.freeform_sensor_crop_enable == 1) {
			hw->radial_cfg.sensor_crop_x =
				p_set->sensor_config.freeform_sensor_crop_offset_x;
			hw->radial_cfg.sensor_crop_y =
				p_set->sensor_config.freeform_sensor_crop_offset_y;
		} else {
			binned_sensor_width = p_set->sensor_config.calibrated_width * 1000 /
					      p_set->sensor_config.sensor_binning_ratio_x;
			binned_sensor_height = p_set->sensor_config.calibrated_height * 1000 /
					       p_set->sensor_config.sensor_binning_ratio_y;
			hw->radial_cfg.sensor_crop_x =
				((binned_sensor_width - p_set->sensor_config.width) >> 1) & (~0x1);
			hw->radial_cfg.sensor_crop_y =
				((binned_sensor_height - p_set->sensor_config.height) >> 1) &
				(~0x1);
		}
		hw->radial_cfg.csis_binning_x = p_set->sensor_config.bns_binning_ratio_x;
		hw->radial_cfg.csis_binning_y = p_set->sensor_config.bns_binning_ratio_y;
		hw->radial_cfg.rgbp_crop_offset_x = p_set->rgbp_otf_input.bayer_crop_offset_x;
		hw->radial_cfg.rgbp_crop_offset_y = p_set->rgbp_otf_input.bayer_crop_offset_y;
		hw->radial_cfg.rgbp_crop_w = p_set->rgbp_otf_input.bayer_crop_width;
		hw->radial_cfg.rgbp_crop_h = p_set->rgbp_otf_input.bayer_crop_height;
	} else {
		pablo_hw_mlsc_external_shot(hw_ip, frame, p_set);
		hw->radial_cfg.sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
		hw->radial_cfg.sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
		hw->radial_cfg.sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
		hw->radial_cfg.sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
		hw->radial_cfg.csis_binning_x = frame->shot->udm.frame_info.bns_binning[0];
		hw->radial_cfg.csis_binning_y = frame->shot->udm.frame_info.bns_binning[1];
		hw->radial_cfg.rgbp_crop_offset_x =
			frame->shot->udm.frame_info.taa_in_crop_region[0];
		hw->radial_cfg.rgbp_crop_offset_y =
			frame->shot->udm.frame_info.taa_in_crop_region[1];
		hw->radial_cfg.rgbp_crop_w = frame->shot->udm.frame_info.taa_in_crop_region[2];
		hw->radial_cfg.rgbp_crop_h = frame->shot->udm.frame_info.taa_in_crop_region[3];
	}

	if (!test_bit(HW_CONFIG, &hw_ip->state))
		_pablo_hw_mlsc_init_config(hw_ip);

	ret = _pablo_hw_mlsc_s_lic_config(hw_ip, instance);
	if (ret) {
		mserr_hw("lic_config fail", instance, hw_ip);
		return ret;
	}

	if (is_debug_support_crta()) {
		if (test_bit(IS_GROUP_RNR_2ND, &group->state)) {
			do_blk_cfg = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, instance, COREX_DIRECT,
							skip, frame, (void *)&clb);
			msdbg_hw(2, "[F%d]%s RNR 2nd stream.\n", instance, hw_ip, frame->fcount,
				 __func__);
		} else {
			if (likely(!test_bit(hw_ip->id, &debug_iq))) {
				CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount,
					    DEBUG_POINT_RTA_REGS_E);
				ret = _pablo_hw_mlsc_s_iq_regs(hw_ip, instance, p_clb);
				CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount,
					    DEBUG_POINT_RTA_REGS_X);
				if (ret)
					mserr_hw("_pablo_hw_mlsc_s_iq_regs is fail", instance,
						 hw_ip);
			} else {
				msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
			}

			/* Save CRTA cr_set and config in RNR 1st stream*/
			if (hw->config.nr_iteration_mode == MLSC_2NR &&
			    frame->type != SHOT_TYPE_INTERNAL)
				_pablo_hw_mlsc_save_rta_cfg(hw_ip, frame, instance);
		}
	}

	if (unlikely(!hw->cur_iq_set.size) && !test_bit(IS_GROUP_RNR_2ND, &group->state)) {
		pablo_hw_mlsc_set_config(hw_ip, 0, instance, frame->fcount, &default_config);
		mlsc_hw_s_bypass(hw_ip->pmio);
	}

	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));

	/* FRO */
	hw_ip->num_buffers = frame->num_buffers;
	frame_cfg.num_buffers = frame->num_buffers;

	mlsc_hw_s_path(hw_ip->pmio, hw->input, &frame_cfg);

	switch (hw->input) {
	case OTF:
		hw->size.input_w = p_set->otf_input.width;
		hw->size.input_h = p_set->otf_input.height;
		break;
	case DMA:
	case VOTF:
		mlsc_hw_s_sr(hw_ip->pmio, true);
		hw->size.input_w = p_set->dma_input.width;
		hw->size.input_h = p_set->dma_input.height;
		break;
	default:
		ret = -EINVAL;
		goto shot_fail_recovery;
	}
	mlsc_hw_s_core(hw_ip->pmio, hw->size.input_w, hw->size.input_h);

	ret = mlsc_hw_s_glpg(hw_ip->pmio, p_set, &hw->size);
	if (ret) {
		mserr_hw("s_glpg fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	mlsc_hw_s_config(hw_ip->pmio, &hw->size, p_set, &hw->config);

	ret = _pablo_hw_mlsc_s_ds(hw_ip, p_set);
	if (ret) {
		mserr_hw("s_ds fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	mlsc_hw_s_menr_cfg(pmio, &hw->radial_cfg, &hw->size.lmeds_dst);

	ret = _pablo_hw_mlsc_s_dma(hw_ip, p_set);
	if (ret) {
		mserr_hw("s_dma fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (likely(p_clb)) {
		/* Sync PMIO cache */
		pmio_cache_fsync(pmio, (void *)&clb, PMIO_FORMATTER_PAIR);

		/* Flush Host CPU cache */
		pb = cl_frame->pb_output;
		CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);

		if (clb.num_of_pairs > 0)
			clb.num_of_headers++;

		/*
		pr_info("header dva: 0x%08llx\n", ((dma_addr_t)clb.header_dva) >> 4);
		pr_info("number of headers: %d\n", clb.num_of_headers);
		pr_info("number of pairs: %d\n", clb.num_of_pairs);

		print_hex_dump(KERN_INFO, "header  ", DUMP_PREFIX_OFFSET, 16, 4, clb.clh,
			clb.num_of_headers * 16, false);
		print_hex_dump(KERN_INFO, "payload ", DUMP_PREFIX_OFFSET, 16, 4, clb.clp,
			clb.num_of_headers * 64, false);
		*/
	} else {
		pmio_cache_sync(pmio);
	}

	/* Prepare CMD for CMDQ */
	_pablo_hw_mlsc_s_cmd(hw, p_clb, fcount, &frame_cfg.cmd);

	set_bit(HW_CONFIG, &hw_ip->state);

	CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);

shot_fail_recovery:
	if (likely(p_clb))
		trans_frame(framemgr, cl_frame, FS_FREE);

	return ret;
}

static int pablo_hw_mlsc_frame_ndone(
	struct is_hw_ip *hw_ip, struct is_frame *frame, enum ShotErrorType done_type)
{
	if (test_bit(hw_ip->id, &frame->core_flag))
		return CALL_HW_OPS(
			hw_ip, frame_done, hw_ip, frame, -1, IS_HW_CORE_END, done_type, false);

	return 0;
}

static int pablo_hw_mlsc_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_LIGHT);
	mlsc_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);

	return 0;
}

static int pablo_hw_mlsc_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw->pcc;

	msinfo_hw("reset\n", instance, hw_ip);

	mlsc_hw_s_otf(hw_ip->pmio, false);

	return CALL_PCC_OPS(pcc, reset, pcc);
}

static int pablo_hw_mlsc_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("restore: Not opened", instance, hw_ip);
		return -EINVAL;
	}

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	return 0;
}

static int pablo_hw_mlsc_change_chain(
	struct is_hw_ip *hw_ip, u32 instance, u32 next_id, struct is_hardware *hardware)
{
	int ret = 0;
	struct pablo_hw_mlsc *hw, *next_hw;
	u32 curr_id;
	u32 next_hw_id = DEV_HW_MLSC0 + next_id;
	struct is_hw_ip *next_hw_ip;

	curr_id = hw_ip->ch;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip, curr_id, next_id);
		goto p_err;
	}

	hw = (struct pablo_hw_mlsc *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("failed to get HW MLSC", instance, hw_ip);
		return -ENODEV;
	}

	next_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, next_hw_id);
	if (!next_hw_ip) {
		merr_hw("[ID:%d]invalid next next_hw_id", instance, next_hw_id);
		return -EINVAL;
	}

	next_hw = (struct pablo_hw_mlsc *)next_hw_ip->priv_info;
	if (!next_hw) {
		mserr_hw("failed to get next HW MLSC", instance, next_hw_ip);
		return -ENODEV;
	}

	if (!test_and_clear_bit(instance, &hw_ip->run_rsc_state))
		mswarn_hw("try to disable disabled instance", instance, hw_ip);

	ret = pablo_hw_mlsc_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_mlsc_disable is fail ret(%d)", instance, hw_ip, ret);
		if (ret != -EWOULDBLOCK)
			return -EINVAL;
	}

	/*
	 * Copy instance information.
	 * But do not clear current hw_ip,
	 * because logical(initial) HW must be referred at close time.
	 */
	next_hw->param_set[instance] = hw->param_set[instance];

	next_hw_ip->group[instance] = hw_ip->group[instance];
	next_hw_ip->region[instance] = hw_ip->region[instance];
	next_hw_ip->stm[instance] = hw_ip->stm[instance];

	/* set & clear physical HW */
	set_bit(next_hw_id, &hardware->hw_map[instance]);
	clear_bit(hw_ip->id, &hardware->hw_map[instance]);

	if (test_and_set_bit(instance, &next_hw_ip->run_rsc_state))
		mswarn_hw("try to enable enabled instance", instance, next_hw_ip);

	ret = pablo_hw_mlsc_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("pablo_hw_mlsc_enable is fail", instance, next_hw_ip);
		return -EINVAL;
	}

	/*
	 * There is no change about rsccount when change_chain processed
	 * because there is no open/close operation.
	 * But if it isn't increased, abnormal situation can be occurred
	 * according to hw close order among instances.
	 */
	if (!test_bit(hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_dec(&hw_ip->rsccount);
		msinfo_hw("decrease hw_ip rsccount(%d)", instance, hw_ip,
			atomic_read(&hw_ip->rsccount));
	}

	if (!test_bit(next_hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_inc(&next_hw_ip->rsccount);
		msinfo_hw("increase next_hw_ip rsccount(%d)", instance, next_hw_ip,
			atomic_read(&next_hw_ip->rsccount));
	}

	msinfo_hw("change_chain done (state: curr(0x%lx) next(0x%lx))", instance, hw_ip,
		hw_ip->state, next_hw_ip->state);
p_err:
	return ret;
}

static int pablo_hw_mlsc_set_regs(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size)
{
	struct pablo_hw_mlsc *hw;
	struct pablo_hw_mlsc_iq *iq_set;
	ulong flag = 0;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!regs);

	hw = (struct pablo_hw_mlsc *)hw_ip->priv_info;
	iq_set = &hw->iq_set;

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	msdbg_hw(2, "[F%d]Store IQ regs set: %p, size(%d)\n", instance, hw_ip, fcount, regs,
		regs_size);

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	iq_set->fcount = fcount;
	memcpy((void *)iq_set->regs, (void *)regs, (sizeof(struct cr_set) * regs_size));

	set_bit(CR_SET_CONFIG, &iq_set->state);

	spin_unlock_irqrestore(&iq_set->slock, flag);

	return 0;
}

static int pablo_hw_mlsc_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct pablo_hw_mlsc *hw_mlsc = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		mlsc_hw_dump(hw_ip->pmio, HW_DUMP_CR);
		break;
	case IS_REG_DUMP_DMA:
		hw_mlsc = GET_HW(hw_ip);
		for (i = MLSC_R_CL; i < MLSC_RDMA_NUM; i++) {
			dma = &hw_mlsc->dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = MLSC_W_YUV444_Y; i < MLSC_DMA_NUM; i++) {
			dma = &hw_mlsc->dma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		break;
	}

	return 0;
}

static void _pablo_hw_mlsc_g_pcfi(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame,
	struct pablo_rta_frame_info *prfi)
{
	struct mlsc_param *mlsc_p = &frame->parameter->mlsc;

	prfi->mlsc_out_svhist_buffer = mlsc_p->svhist.cmd;
	prfi->mlsc_out_meds_buffer = mlsc_p->lme_ds.cmd;
}

static void _pablo_hw_mlsc_g_edge_score(struct is_hw_ip *ip, u32 *edge_score)
{
	*edge_score = mlsc_hw_g_edge_score(ip->pmio);
}

static void pablo_hw_mlsc_query(struct is_hw_ip *ip, u32 instance, u32 type, void *in, void *out)
{
	switch (type) {
	case PABLO_QUERY_GET_PCFI:
		_pablo_hw_mlsc_g_pcfi(
			ip, instance, (struct is_frame *)in, (struct pablo_rta_frame_info *)out);
		break;
	case PABLO_QUERY_GET_EDGE_SCORE:
		_pablo_hw_mlsc_g_edge_score(ip, (u32 *)out);
		break;
	default:
		break;
	}
}

static void fs_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct is_hardware *hardware = hw_ip->hardware;
	u32 dbg_lv = atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;

	msdbg_hw(dbg_lv, "[F%d]FS\n", instance, hw_ip, fcount);

	atomic_add(1, &hw_ip->count.fs);
	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_FRAME_START);
	CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
}

static void fe_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	u32 dbg_lv = atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;

	msdbg_hw(dbg_lv, "[F%d]FE\n", instance, hw_ip, fcount);

	if (hw->input == DMA)
		mlsc_hw_s_sr(hw_ip->pmio, false);

	atomic_add(1, &hw_ip->count.fe);
	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_FRAME_END);
	CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

	if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
		mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)", instance, hw_ip,
			atomic_read(&hw_ip->count.fs), atomic_read(&hw_ip->count.fe),
			atomic_read(&hw_ip->count.dma), status);
	}

	wake_up(&hw_ip->status.wait_queue);
}

static void int0_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_mlsc *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT0] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	mlsc_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();
}

static void int0_warn_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_mlsc *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT0] HW Warning!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	mlsc_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();

	mlsc_hw_clr_cotf_err(hw_ip->pmio);
}

static void int1_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_mlsc *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT1] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	mlsc_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();
}

static int pablo_hw_mlsc_check_int(struct is_hw_ip *hw_ip, u32 status)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	u32 hw_fcount = atomic_read(&hw_ip->fcount);
	u32 instance = atomic_read(&hw_ip->instance);
	u32 fs, fe;
	u32 int_status;

	fs = mlsc_hw_is_occurred(status, BIT_MASK(INT_FRAME_START)) ? BIT_MASK(INT_FRAME_START) : 0;
	fe = mlsc_hw_is_occurred(status, BIT_MASK(INT_FRAME_END)) ? BIT_MASK(INT_FRAME_END) : 0;

	if (fs && fe)
		mswarn_hw("[F%d] start/end overlapped!!", instance, hw_ip, hw_fcount);

	int_status = fs | fe;

	msdbg_hw(4, "[F%d] int_status 0x%08x fs 0x%08x fe 0x%08x\n", instance, hw_ip, hw_fcount,
		int_status, fs, fe);

	while (int_status) {
		switch (hw->event_state) {
		case MLSC_FS:
			if (fe) {
				hw->event_state = MLSC_FE;
				fe_handler(hw_ip, instance, hw_fcount, status);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case MLSC_FE:
			if (fs) {
				hw->event_state = MLSC_FS;
				fs_handler(hw_ip, instance, hw_fcount, status);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case MLSC_INIT:
			if (fs) {
				hw->event_state = MLSC_FS;
				fs_handler(hw_ip, instance, hw_fcount, status);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		}
	}
skip_isr_event:

	return int_status;
}

static int pablo_hw_mlsc_isr(u32 id, void *ctx)
{
	struct is_hw_ip *hw_ip;
	struct pablo_hw_mlsc *hw;
	struct pablo_common_ctrl *pcc;
	u32 instance, hw_fcount, status;
	u32 int_status;

	hw_ip = (struct is_hw_ip *)ctx;
	instance = atomic_read(&hw_ip->instance);
	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("Invalid INT%d. hw_state 0x%lx", instance, hw_ip, id, hw_ip->state);
		return 0;
	}

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;
	hw_fcount = atomic_read(&hw_ip->fcount);
	status = CALL_PCC_OPS(pcc, get_int_status, pcc, id, true);

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("Invalid INT%d. int_status 0x%08x hw_state 0x%lx", instance, hw_ip, id,
			status, hw_ip->state);
		return 0;
	}
	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("Ignore ISR before HW config interrupt status(0x%x)\n", instance, hw_ip,
			status);
		return 0;
	}

	msdbg_hw(2, "[F%d][INT%d] status 0x%08x\n", instance, hw_ip, hw_fcount, id, status);

	if (id == INTR_HWIP1) {
		int_status = pablo_hw_mlsc_check_int(hw_ip, status);
		if (int_status)
			mswarn_hw("[F%d] invalid interrupt: event_state(%ld), int_status(%x)",
				instance, hw_ip, hw_fcount, hw->event_state, int_status);

		if (mlsc_hw_is_occurred(status, BIT_MASK(INT_SETTING_DONE)))
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

		if (mlsc_hw_is_occurred(status, BIT_MASK(INT_ERR0))) {
			int0_err_handler(hw_ip, instance, hw_fcount, status);

			if (CALL_HW_OPS(hw_ip, clear_crc_status, instance, hw_ip) > 0)
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
		}

		if (mlsc_hw_is_occurred(status, BIT_MASK(INT_WARN0)))
			int0_warn_handler(hw_ip, instance, hw_fcount, status);
	} else {
		if (mlsc_hw_is_occurred(status, BIT_MASK(INT_ERR1))) {
			int1_err_handler(hw_ip, instance, hw_fcount, status);

			if (CALL_HW_OPS(hw_ip, clear_crc_status, instance, hw_ip) > 0)
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
		}
	}

	if (unlikely(dbg_info.en[MLSC_DBG_S_TRIGGER]))
		_dbg_handler(hw_ip, id, status);

	return 0;
}

static int pablo_hw_mlsc_cmp_fcount(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc;

	if (!hw)
		return 0;

	pcc = hw->pcc;
	return CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount);
}

static int pablo_hw_mlsc_recover(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc;
	int ret;

	if (!hw)
		return 0;

	pcc = hw->pcc;
	ret = CALL_PCC_OPS(pcc, recover, pcc, fcount);

	if (ret)
		clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static size_t pablo_hw_mlsc_dump_params(
	struct is_hw_ip *hw_ip, u32 instance, char *buf, size_t size)
{
	int i;
	struct pablo_hw_mlsc *hw = GET_HW(hw_ip);
	struct mlsc_param_set *param;
	size_t rem = size;
	char *p = buf;

	param = &hw->param_set[instance];

	p = pablo_json_nstr(p, "hw name", hw_ip->name, strlen(hw_ip->name), &rem);
	p = pablo_json_uint(p, "hw id", hw_ip->id, &rem);

	p = dump_param_otf_input(p, "otf_outpuotf_inputt", &param->otf_input, &rem);
	p = dump_param_dma_input(p, "dma_input", &param->dma_input, &rem);
	p = dump_param_dma_output(p, "dma_output_yuv", &param->dma_output_yuv, &rem);

	for (i = 0; i < MLSC_GLPG_NUM; i++) {
		char title[32];

		snprintf(title, sizeof(title), "dma_output_glpg[%d]", i);
		p = dump_param_dma_output(p, title, &param->dma_output_glpg[i], &rem);
	}

	p = dump_param_dma_output(p, "dma_output_svhist", &param->dma_output_svhist, &rem);
	p = dump_param_dma_output(p, "dma_output_lme_ds", &param->dma_output_lme_ds, &rem);
	p = dump_param_dma_output(p, "dma_output_fdpig", &param->dma_output_fdpig, &rem);
	p = dump_param_dma_output(p, "dma_output_cav", &param->dma_output_cav, &rem);

	p = pablo_json_uint(p, "instance_id", param->instance_id, &rem);
	p = pablo_json_uint(p, "fcount", param->fcount, &rem);

	return WRITTEN(size, rem);
}

const struct is_hw_ip_ops pablo_hw_mlsc_ops = {
	.open = pablo_hw_mlsc_open,
	.init = pablo_hw_mlsc_init,
	.close = pablo_hw_mlsc_close,
	.enable = pablo_hw_mlsc_enable,
	.disable = pablo_hw_mlsc_disable,
	.shot = pablo_hw_mlsc_shot,
	.frame_ndone = pablo_hw_mlsc_frame_ndone,
	.notify_timeout = pablo_hw_mlsc_notify_timeout,
	.reset = pablo_hw_mlsc_reset,
	.restore = pablo_hw_mlsc_restore,
	.change_chain = pablo_hw_mlsc_change_chain,
	.set_regs = pablo_hw_mlsc_set_regs,
	.set_config = pablo_hw_mlsc_set_config,
	.dump_regs = pablo_hw_mlsc_dump_regs,
	.query = pablo_hw_mlsc_query,
	.cmp_fcount = pablo_hw_mlsc_cmp_fcount,
	.recover = pablo_hw_mlsc_recover,
	.dump_params = pablo_hw_mlsc_dump_params,
};

static struct pablo_mmio *_pablo_hw_mlsc_pmio_init(struct is_hw_ip *hw_ip)
{
	int ret;
	struct pablo_mmio *pmio;
	struct pmio_config *pcfg;

	pcfg = &hw_ip->pmio_config;

	pcfg->name = "MLSC";
	pcfg->mmio_base = hw_ip->mmio_base;
	pcfg->cache_type = PMIO_CACHE_NONE;
	pcfg->phys_base = hw_ip->regs_start[REG_SETA];

	mlsc_hw_g_pmio_cfg(pcfg);

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	ret = pmio_field_bulk_alloc(pmio, &hw_ip->pmio_fields, pcfg->fields, pcfg->num_fields);
	if (ret) {
		serr_hw("Failed to alloc MLSC PMIO field_bulk. ret %d", hw_ip, ret);
		goto err_field_bulk_alloc;
	}

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
	pmio = ERR_PTR(ret);
err_init:
	return pmio;
}

static void _pablo_hw_mlsc_pmio_deinit(struct is_hw_ip *hw_ip)
{
	struct pablo_mmio *pmio = hw_ip->pmio;

	pmio_field_bulk_free(pmio, hw_ip->pmio_fields);
	pmio_exit(pmio);
}

int pablo_hw_mlsc_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	struct pablo_mmio *pmio;
	int hw_slot;

	hw_ip->ops = &pablo_hw_mlsc_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &pablo_hw_mlsc_isr;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &pablo_hw_mlsc_isr;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	pmio = _pablo_hw_mlsc_pmio_init(hw_ip);
	if (IS_ERR(pmio)) {
		serr_hw("Failed to mlsc pmio_init.", hw_ip);
		return -EINVAL;
	}

	hw_ip->pmio = pmio;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_hw_mlsc_probe);

void pablo_hw_mlsc_remove(struct is_hw_ip *hw_ip)
{
	struct is_interface_ischain *itfc = hw_ip->itfc;
	int id = hw_ip->id;
	int hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = false;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = false;

	_pablo_hw_mlsc_pmio_deinit(hw_ip);
}
EXPORT_SYMBOL_GPL(pablo_hw_mlsc_remove);
