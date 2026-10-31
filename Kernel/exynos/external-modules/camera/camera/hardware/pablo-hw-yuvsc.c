// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-hw.h"
#include "pablo-hw-api-yuvsc.h"
#include "pablo-hw-yuvsc.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

#define GET_HW(hw_ip)				((struct pablo_hw_yuvsc *)hw_ip->priv_info)
#define CALL_HW_YUVSC_OPS(hw, op, args...)	((hw && hw->ops) ? hw->ops->op(args) : 0)
#define GET_EN_STR(en)				(en ? "ON" : "OFF")


static DEFINE_MUTEX(cmn_reg_lock);

/* DEBUG module params */
enum pablo_hw_yuvsc_dbg_type {
	YUVSC_DBG_NONE = 0,
	YUVSC_DBG_S_TRIGGER,
	YUVSC_DBG_CR_DUMP,
	YUVSC_DBG_STATE_DUMP,
	YUVSC_DBG_RTA_DUMP,
	YUVSC_DBG_S2D,
	YUVSC_DBG_APB_DIRECT,
	YUVSC_DBG_TYPE_NUM,
};

struct pablo_hw_yuvsc_dbg_info {
	bool en[YUVSC_DBG_TYPE_NUM];
	u32 instance;
	u32 fcount;
	u32 int_msk[2];
};

static struct pablo_hw_yuvsc_dbg_info dbg_info = {
	.en = { false, },
	.instance = 0,
	.fcount = 0,
	.int_msk = { 0, },
};

static int _parse_dbg_s_trigger(char **argv, int argc)
{
	int ret;
	u32 instance, fcount, int_msk0, int_msk1;

	if (argc != 4) {
		err("[DBG_YUVSC] Invalid arguments! %d", argc);
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &instance);
	if (ret) {
		err("[DBG_YUVSC] Invalid instance %d ret %d", instance, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[1], 0, &fcount);
	if (ret) {
		err("[DBG_YUVSC] Invalid fcount %d ret %d", fcount, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[2], 0, &int_msk0);
	if (ret) {
		err("[DBG_YUVSC] Invalid int_msk0 %d ret %d", int_msk0, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[3], 0, &int_msk1);
	if (ret) {
		err("[DBG_YUVSC] Invalid int_msk1 %d ret %d", int_msk1, ret);
		return -EINVAL;
	}

	dbg_info.instance = instance;
	dbg_info.fcount = fcount;
	dbg_info.int_msk[0] = int_msk0;
	dbg_info.int_msk[1] = int_msk1;

	info("[DBG_YUVSC] S_TRIGGER:[I%d][F%d] INT0 0x%08x INT1 0x%08x\n",
			dbg_info.instance,
			dbg_info.fcount,
			dbg_info.int_msk[0], dbg_info.int_msk[1]);

	return 0;
}

typedef int (*dbg_parser)(char **argv, int argc);
struct dbg_parser_info {
	char name[NAME_MAX];
	char man[NAME_MAX];
	dbg_parser parser;
};

static struct dbg_parser_info dbg_parsers[YUVSC_DBG_TYPE_NUM] = {
	[YUVSC_DBG_S_TRIGGER] = {
		"S_TRIGGER",
		"<instance> <fcount> <int_msk0> <int_msk1> ",
		_parse_dbg_s_trigger,
	},
	[YUVSC_DBG_CR_DUMP] = {
		"CR_DUMP",
		"",
		NULL,
	},
	[YUVSC_DBG_STATE_DUMP] = {
		"STATE_DUMP",
		"",
		NULL,
	},
	[YUVSC_DBG_RTA_DUMP] = {
		"RTA_DUMP",
		"",
		NULL,
	},
	[YUVSC_DBG_S2D] = {
		"S2D",
		"",
		NULL,
	},
	[YUVSC_DBG_APB_DIRECT] = {
		"APB_DIRECT",
		"",
		NULL,
	},
};

static int pablo_hw_yuvsc_dbg_set(const char *val)
{
	int ret = 0, argc = 0;
	char **argv;
	u32 dbg_type, en, arg_i = 0;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("[DBG_YUVSC] No argument!");
		return -EINVAL;
	} else if (argc < 2) {
		err("[DBG_YUVSC] Too short argument!");
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &dbg_type);
	if (ret || !dbg_type || dbg_type >= YUVSC_DBG_TYPE_NUM) {
		err("[DBG_YUVSC] Invalid dbg_type %u ret %d", dbg_type, ret);
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &en);
	if (ret) {
		err("[DBG_YUVSC] Invalid en %u ret %d", en, ret);
		goto func_exit;
	}

	dbg_info.en[dbg_type] = en;
	info("[DBG_YUVSC] %s[%s]\n", dbg_parsers[dbg_type].name, GET_EN_STR(dbg_info.en[dbg_type]));

	argc = (argc > arg_i) ? (argc - arg_i) : 0;

	if (argc && dbg_parsers[dbg_type].parser &&
		dbg_parsers[dbg_type].parser(&argv[arg_i], argc)) {
		err("[DBG_YUVSC] Failed to %s", dbg_parsers[dbg_type].name);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_hw_yuvsc_dbg_get(char *buffer, const size_t buf_size)
{
	const char *get_msg = "= YUVSC DEBUG Configuration =====================\n"
			      "  Current Trigger Point:\n"
			      "    - instance %d\n"
			      "    - fcount %d\n"
			      "    - int0_msk 0x%08x int1_msk 0x%08x\n"
			      "================================================\n";

	return scnprintf(buffer, buf_size, get_msg, dbg_info.instance, dbg_info.fcount,
		dbg_info.int_msk[0], dbg_info.int_msk[1]);
}

static int pablo_hw_yuvsc_dbg_usage(char *buffer, const size_t buf_size)
{
	int ret;
	u32 dbg_type;

	ret = scnprintf(buffer, buf_size, "[value] string value, YUVSC debug features\n");
	for (dbg_type = 1; dbg_type < YUVSC_DBG_TYPE_NUM; dbg_type++)
		ret += scnprintf(buffer + ret, buf_size - ret,
			"  - %10s[%3s]: echo %d <en> %s> debug_yuvsc\n", dbg_parsers[dbg_type].name,
			GET_EN_STR(dbg_info.en[dbg_type]), dbg_type, dbg_parsers[dbg_type].man);

	return ret;
}

static struct pablo_debug_param debug_yuvsc = {
	.type = IS_DEBUG_PARAM_TYPE_STR,
	.ops.set = pablo_hw_yuvsc_dbg_set,
	.ops.get = pablo_hw_yuvsc_dbg_get,
	.ops.usage = pablo_hw_yuvsc_dbg_usage,
};

module_param_cb(debug_yuvsc, &pablo_debug_param_ops, &debug_yuvsc, 0644);

static void _dbg_handler(struct is_hw_ip *hw_ip, u32 irq_id, u32 status)
{
	struct pablo_hw_yuvsc *hw;
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

	info("[DBG_YUVSC] %s:[I%d][F%d] INT%d 0x%08x\n", __func__,
		instance, fcount, irq_id, status);

	if (dbg_info.en[YUVSC_DBG_CR_DUMP])
		CALL_HW_YUVSC_OPS(hw, dump, hw_ip->pmio, HW_DUMP_CR);

	if (dbg_info.en[YUVSC_DBG_STATE_DUMP]) {
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		CALL_HW_YUVSC_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	}

	if (dbg_info.en[YUVSC_DBG_S2D])
		is_debug_s2d(true, "YUVSC_DBG_S2D");
}

/* YUVSC HW OPS */
static int _pablo_hw_yuvsc_cloader_init(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_yuvsc *hw;
	struct is_mem *mem;
	struct pablo_internal_subdev *pis;
	int ret;
	enum base_reg_index reg_id;
	resource_size_t reg_size;
	u32 reg_num, hdr_size;

	hw = GET_HW(hw_ip);
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_YUVSC0);
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

static inline int _pablo_hw_yuvsc_pcc_init(struct is_hw_ip *hw_ip)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct is_mem *mem;
	enum pablo_common_ctrl_mode pcc_mode;

	hw->pcc = pablo_common_ctrl_hw_get_pcc(&hw_ip->pmio_config);
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_YUVSC0);

	pcc_mode = PCC_OTF_NO_DUMMY;

	return CALL_PCC_HW_OPS(hw->pcc, init, hw->pcc, hw_ip->pmio, hw_ip->name, pcc_mode, mem);
}

static int is_hw_yuvsc_prepare(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_yuvsc *hw;
	struct pablo_mmio *pmio;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_cfg cfg = {
		0,
	};

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;
	pmio = hw_ip->pmio;

	cfg.fs_mode = PCC_VVALID_RISE;
	CALL_HW_YUVSC_OPS(hw, g_int_en, cfg.int_en);
	if (CALL_PCC_OPS(pcc, enable, pcc, &cfg)) {
		mserr_hw("failed to PCC enable", instance, hw_ip);
		return -EINVAL;
	}

	CALL_HW_YUVSC_OPS(hw, init, pmio);

	return 0;
}

static int pablo_hw_yuvsc_open(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_yuvsc *hw;
	int ret;
	u32 reg_cnt = yuvsc_hw_g_reg_cnt();

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, "HWYUVSC");

	hw_ip->priv_info = pablo_zalloc(sizeof(struct pablo_hw_yuvsc), GFP_KERNEL);
	if (!hw_ip->priv_info) {
		mserr_hw("Failed to alloc pablo_hw_yuvsc", instance, hw_ip);
		return -ENOMEM;
	}

	hw = GET_HW(hw_ip);
	hw->ops = yuvsc_hw_g_ops();
	hw_ip->ch = hw_ip->id - DEV_HW_YUVSC0;

	/* Setup C-loader */
	ret = _pablo_hw_yuvsc_cloader_init(hw_ip, instance);
	if (ret)
		return ret;

	/* Setup Common-CTRL */
	ret = _pablo_hw_yuvsc_pcc_init(hw_ip);
	if (ret)
		goto err_pcc_init;

	hw->iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	hw->cur_iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	if (!hw->iq_set.regs || !hw->cur_iq_set.regs) {
		mserr_hw("Failed to alloc iq_set regs", instance, hw_ip);
		ret = -ENOMEM;

		goto err_all_iq_set;
	}

	clear_bit(CR_SET_CONFIG, &hw->iq_set.state);
	set_bit(CR_SET_EMPTY, &hw->iq_set.state);
	spin_lock_init(&hw->iq_set.slock);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open\n", instance, hw_ip);

	return 0;

err_all_iq_set:
	if (hw->iq_set.regs) {
		pablo_free(hw->iq_set.regs);
		hw->iq_set.regs = NULL;
	}

	if (hw->cur_iq_set.regs) {
		pablo_free(hw->cur_iq_set.regs);
		hw->cur_iq_set.regs = NULL;
	}

	CALL_PCC_HW_OPS(hw->pcc, deinit, hw->pcc);

err_pcc_init:
	CALL_I_SUBDEV_OPS(&hw->subdev_cloader, free, &hw->subdev_cloader);

	return ret;
}

static int pablo_hw_yuvsc_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	struct pablo_hw_yuvsc *hw;

	set_bit(HW_INIT, &hw_ip->state);

	msdbg_hw(2, "init\n", instance, hw_ip);

	hw = GET_HW(hw_ip);

	hw->param_set[instance].reprocessing = flag;

	return 0;
}

static int pablo_hw_yuvsc_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_yuvsc *hw;
	struct pablo_internal_subdev *pis;
	int ret;
	struct is_hw_ip *hw_ip_phys;
	int i, max_num;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw = GET_HW(hw_ip);
	ret = CALL_HW_YUVSC_OPS(hw, wait_idle, hw_ip->pmio);
	if (ret)
		mserr_hw("failed to wait_idle. ret %d", instance, hw_ip, ret);

	max_num = hw_ip->ip_num;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip, DEV_HW_YUVSC0 + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num) {
		for (i = max_num - 1; i >= 0; i--) {
			hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip,
							    DEV_HW_YUVSC0 + i);
			CALL_HW_YUVSC_OPS(hw, reset, hw_ip_phys->pmio);
		}
	}

	mutex_unlock(&cmn_reg_lock);

	if (hw->iq_set.regs) {
		pablo_free(hw->iq_set.regs);
		hw->iq_set.regs = NULL;
	}

	if (hw->cur_iq_set.regs) {
		pablo_free(hw->cur_iq_set.regs);
		hw->cur_iq_set.regs = NULL;
	}

	CALL_PCC_HW_OPS(hw->pcc, deinit, hw->pcc);

	pis = &hw->subdev_cloader;
	CALL_I_SUBDEV_OPS(pis, free, pis);

	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	clear_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "close\n", instance, hw_ip);

	return 0;
}

static int pablo_hw_yuvsc_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_mmio *pmio;
	struct pablo_hw_yuvsc *hw;
	u32 i, ip_num;

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
	hw->event_state = YUVSC_INIT;
	ip_num = hw_ip->ip_num;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < ip_num; i++) {
		struct is_hw_ip *hw_ip_phys =
			CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip, DEV_HW_YUVSC0 + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == ip_num)
		CALL_HW_YUVSC_OPS(hw, s_lbctrl, hw_ip->locomotive->pmio);

	mutex_unlock(&cmn_reg_lock);

	hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
	if (pmio_reinit_cache(pmio, &hw_ip->pmio_config)) {
		mserr_hw("failed to reinit PMIO cache", instance, hw_ip);
		return -EINVAL;
	}

	if (unlikely(dbg_info.en[YUVSC_DBG_APB_DIRECT])) {
		pmio_cache_set_only(pmio, false);
		pmio_cache_set_bypass(pmio, true);
	} else {
		pmio_cache_set_only(pmio, true);
	}

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static void pablo_hw_yuvsc_clear(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);

	memset(&hw->config[instance], 0x00, sizeof(struct is_yuvsc_config));
	hw->cur_iq_set.size = 0;
	hw->event_state = YUVSC_INIT;
	frame_manager_flush(GET_SUBDEV_I_FRAMEMGR(&hw->subdev_cloader));
	clear_bit(HW_CONFIG, &hw_ip->state);
}

static int pablo_hw_yuvsc_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_hw_yuvsc *hw;
	struct pablo_mmio *pmio;
	struct pablo_common_ctrl *pcc;
	int ret = 0;
	long timetowait;
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
	timetowait = wait_event_timeout(hw_ip->status.wait_queue,
			!atomic_read(&hw_ip->status.Vvalid),
			IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout. timetowait %uus", instance, hw_ip,
				jiffies_to_usecs(IS_HW_STOP_TIMEOUT));
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		CALL_HW_YUVSC_OPS(hw, dump, pmio, HW_DUMP_DBG_STATE);
		ret = -ETIME;
	}

	pablo_hw_yuvsc_clear(hw_ip, instance);

	/* Disable PMIO Cache */
	pmio_cache_set_only(pmio, false);
	pmio_cache_set_bypass(pmio, true);

	yuvsc_hw_s_otf(hw_ip->pmio, false);
	CALL_PCC_OPS(pcc, disable, pcc);

	return ret;
}

static void _pablo_hw_yuvsc_update_param(struct is_hw_ip *hw_ip, struct is_frame *frame,
		struct yuvsc_param_set *p_set)
{
	struct is_param_region *p_region;
	struct yuvsc_param *param;

	p_region = frame->parameter;
	param = &p_region->yuvsc;

	if (test_bit(PARAM_YUVSC_CONTROL, frame->pmap))
		memcpy(&p_set->control, &param->control, sizeof(struct param_control));

	if (test_bit(PARAM_YUVSC_OTF_INPUT, frame->pmap))
		memcpy(&p_set->otf_input, &param->otf_input, sizeof(struct param_otf_input));

	if (test_bit(PARAM_YUVSC_OTF_OUTPUT, frame->pmap))
		memcpy(&p_set->otf_output, &param->otf_output, sizeof(struct param_otf_input));
}

static int _pablo_hw_yuvsc_s_iq_regs(struct is_hw_ip *hw_ip, u32 instance,
	struct c_loader_buffer *p_clb)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct pablo_mmio *pmio;
	struct pablo_hw_yuvsc_iq *iq_set;
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

		if (unlikely(dbg_info.en[YUVSC_DBG_RTA_DUMP])) {
			msinfo_hw("RTA CR DUMP\n", instance, hw_ip);
			for (i = 0; i < regs_size; i++)
				msinfo_hw("reg:[0x%04X], value:[0x%08X]\n", instance, hw_ip,
					regs[i].reg_addr, regs[i].reg_data);
		}

		if (p_clb) {
			pmio_cache_fsync_ext(hw_ip->pmio, p_clb, regs, regs_size);
			if (p_clb->num_of_pairs > 0)
				p_clb->num_of_headers++;
		} else {
			for (i = 0; i < regs_size; i++)
				PMIO_SET_R(pmio, regs[i].reg_addr, regs[i].reg_data);
		}

		memcpy(hw->cur_iq_set.regs, regs, sizeof(struct cr_set) * regs_size);
		hw->cur_iq_set.size = iq_set->size;

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

static void _pablo_hw_yuvsc_s_cmd(struct pablo_hw_yuvsc *hw, struct c_loader_buffer *clb,
		u32 fcount, struct pablo_common_ctrl_cmd *cmd)
{
	cmd->set_mode = PCC_APB_DIRECT;
	cmd->fcount = fcount;
	cmd->int_grp_en = CALL_HW_YUVSC_OPS(hw, g_int_grp_en);

	if (!clb || unlikely(dbg_info.en[YUVSC_DBG_APB_DIRECT]))
		return;

	cmd->base_addr = clb->header_dva;
	cmd->header_num = clb->num_of_headers;
	cmd->set_mode = PCC_DMA_DIRECT;
}

static int pablo_hw_yuvsc_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct pablo_hw_yuvsc *hw;
	struct pablo_mmio *pmio;
	struct yuvsc_param_set *p_set;
	struct is_framemgr *framemgr;
	struct is_frame *cl_frame;
	struct is_priv_buf *pb;
	struct c_loader_buffer clb, *p_clb = NULL;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_frame_cfg frame_cfg;
	u32 instance, fcount, cur_idx;
	int ret;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	enum pablo_common_ctrl_mode pcc_mode;

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

	/* HW parameter */
	p_set = &hw->param_set[instance];
	p_set->instance = frame->instance;
	p_set->fcount = frame->fcount;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		pmio_cache_set_bypass(pmio, false);
		pmio_cache_set_only(pmio, true);
		is_hw_yuvsc_prepare(hw_ip, instance);
	}

	if (p_set->reprocessing)
		pcc_mode = PCC_M2M;
	else
		pcc_mode = PCC_OTF_NO_DUMMY;

	CALL_PCC_HW_OPS(pcc, set_mode, pcc, pcc_mode);

	/* Prepare C-Loader buffer */
	framemgr = GET_SUBDEV_I_FRAMEMGR(&hw->subdev_cloader);
	cl_frame = peek_frame(framemgr, FS_FREE);
	if (likely(!dbg_info.en[YUVSC_DBG_APB_DIRECT] && cl_frame)) {
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
		hw_ip->internal_fcount[instance] = fcount;
	} else {
		hw_ip->internal_fcount[instance] = 0;

		_pablo_hw_yuvsc_update_param(hw_ip, frame, p_set);
	}

	if (is_debug_support_crta()) {
		if (likely(!test_bit(hw_ip->id, &debug_iq))) {
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);
			ret = _pablo_hw_yuvsc_s_iq_regs(hw_ip, instance, p_clb);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);
			if (ret)
				mserr_hw("Failed to set iq regs", instance, hw_ip);
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		}
	}

	memset(&frame_cfg, 0, sizeof(struct pablo_common_ctrl_frame_cfg));

	/* FRO */
	hw_ip->num_buffers = frame->num_buffers;
	frame_cfg.num_buffers = frame->num_buffers;

	CALL_HW_YUVSC_OPS(hw, s_core, pmio, p_set);
	CALL_HW_YUVSC_OPS(hw, s_path, pmio, p_set, &frame_cfg);

	if (p_set->control.strgen == CONTROL_COMMAND_START)
		CALL_HW_YUVSC_OPS(hw, s_strgen, pmio);

	if (likely(p_clb)) {
		/* Sync PMIO cache */
		pmio_cache_fsync(pmio, (void *)&clb, PMIO_FORMATTER_PAIR);

		/* Flush Host CPU cache */
		pb = cl_frame->pb_output;
		CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);

		if (clb.num_of_pairs > 0)
			clb.num_of_headers++;

		trans_frame(framemgr, cl_frame, FS_FREE);
	}

	/* Prepare CMD for CMDQ */
	_pablo_hw_yuvsc_s_cmd(hw, p_clb, fcount, &frame_cfg.cmd);

	set_bit(HW_CONFIG, &hw_ip->state);

	CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);

	return 0;
}

static int pablo_hw_yuvsc_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	if (test_bit(hw_ip->id, &frame->core_flag))
		return CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
				IS_HW_CORE_END, done_type, false);

	return 0;
}

static int pablo_hw_yuvsc_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_LIGHT);
	CALL_HW_YUVSC_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);

	return 0;
}

static int pablo_hw_yuvsc_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw->pcc;

	msinfo_hw("reset\n", instance, hw_ip);

	yuvsc_hw_s_otf(hw_ip->pmio, false);

	return CALL_PCC_OPS(pcc, reset, pcc);
}

static int pablo_hw_yuvsc_restore(struct is_hw_ip *hw_ip, u32 instance)
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

static int pablo_hw_yuvsc_change_chain(struct is_hw_ip *hw_ip, u32 instance, u32 next_id,
	struct is_hardware *hardware)
{
	struct is_hw_ip *next_hw_ip;
	struct pablo_hw_yuvsc *hw, *next_hw;
	u32 next_hw_id = DEV_HW_YUVSC0 + next_id;
	u32 curr_id;
	int ret = 0;

	curr_id = hw_ip->ch;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip, curr_id, next_id);
		goto p_err;
	}

	hw = GET_HW(hw_ip);
	if (!hw) {
		mserr_hw("failed to get HW YUVSC%d", instance, hw_ip, hw_ip->ch);
		return -ENODEV;
	}

	next_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, next_hw_id);
	if (!next_hw_ip) {
		merr_hw("[ID:%d]invalid next next_hw_id", instance, next_hw_id);
		return -EINVAL;
	}

	next_hw = GET_HW(next_hw_ip);
	if (!next_hw) {
		mserr_hw("failed to get next HW YUVSC%d", instance, next_hw_ip, next_hw_id);
		return -ENODEV;
	}

	if (!test_and_clear_bit(instance, &hw_ip->run_rsc_state))
		mswarn_hw("try to disable disabled instance", instance, hw_ip);

	ret = pablo_hw_yuvsc_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("failed to pablo_hw_yuvsc_disable ret(%d)", instance, hw_ip, ret);
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

	ret = pablo_hw_yuvsc_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("failed to pablo_hw_yuvsc_enable", instance, next_hw_ip);
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

static int pablo_hw_yuvsc_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct pablo_hw_yuvsc_iq *iq_set;
	ulong flag = 0;

	iq_set = &hw->iq_set;

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	msdbg_hw(2, "[F%d]Store IQ regs set: %p, size(%d)\n", instance, hw_ip,
		fcount, regs, regs_size);

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	iq_set->fcount = fcount;
	memcpy((void *)iq_set->regs, (void *)regs, (sizeof(struct cr_set) * regs_size));

	set_bit(CR_SET_CONFIG, &iq_set->state);

	spin_unlock_irqrestore(&iq_set->slock, flag);

	return 0;
}

static int pablo_hw_yuvsc_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
		struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		CALL_HW_YUVSC_OPS(hw, dump, hw_ip->pmio, HW_DUMP_CR);
		break;
	default:
		/* Do nothing */
		break;
	}

	return 0;
}

static int pablo_hw_yuvsc_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
		void *conf)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct is_yuvsc_config *org_cfg = &hw->config[instance];
	struct is_yuvsc_config *new_cfg = (struct is_yuvsc_config *)conf;

	FIMC_BUG(!new_cfg);

	memcpy(org_cfg, new_cfg, sizeof(struct is_yuvsc_config));

	return 0;
}

static int pablo_hw_yuvsc_cmp_fcount(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc;

	if (!hw)
		return 0;

	pcc = hw->pcc;
	return CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount);
}

static int pablo_hw_yuvsc_recover(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
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

static void _pablo_hw_yuvsc_g_pcfi(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame,
	struct pablo_rta_frame_info *prfi)
{
	struct yuvsc_param *yuvsc_p = &frame->parameter->yuvsc;

	if (frame->sensor_rms_crop_ratio) {
		struct pablo_size otf_in;
		struct rgbp_param *rgbp_p = &frame->parameter->rgbp;

		otf_in.width = rgbp_p->otf_output.width;
		otf_in.height = rgbp_p->otf_output.height;

		CALL_HW_YUVSC_OPS(GET_HW(hw_ip), s_rms_crop, &otf_in, yuvsc_p,
				  frame->sensor_rms_crop_ratio);
	}

	prfi->yuvsc_output_size.width = yuvsc_p->otf_output.width;
	prfi->yuvsc_output_size.height = yuvsc_p->otf_output.height;
}

static void pablo_hw_yuvsc_query(struct is_hw_ip *ip, u32 instance, u32 type, void *in, void *out)
{
	switch (type) {
	case PABLO_QUERY_GET_PCFI:
		_pablo_hw_yuvsc_g_pcfi(
			ip, instance, (struct is_frame *)in, (struct pablo_rta_frame_info *)out);
		break;
	default:
		break;
	}
}

static size_t pablo_hw_yuvsc_dump_params(
	struct is_hw_ip *hw_ip, u32 instance, char *buf, size_t size)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	struct yuvsc_param_set *param;
	size_t rem = size;
	char *p = buf;

	param = &hw->param_set[instance];

	p = pablo_json_nstr(p, "hw name", hw_ip->name, strlen(hw_ip->name), &rem);
	p = pablo_json_uint(p, "hw id", hw_ip->id, &rem);

	p = dump_param_control(p, "control", &param->control, &rem);
	p = dump_param_otf_input(p, "otf_input", &param->otf_input, &rem);
	p = dump_param_otf_output(p, "otf_output", &param->otf_output, &rem);

	p = pablo_json_uint(p, "instance", param->instance, &rem);
	p = pablo_json_uint(p, "fcount", param->fcount, &rem);

	return WRITTEN(size, rem);
}

const struct is_hw_ip_ops pablo_hw_yuvsc_ops = {
	.open = pablo_hw_yuvsc_open,
	.init = pablo_hw_yuvsc_init,
	.close = pablo_hw_yuvsc_close,
	.enable = pablo_hw_yuvsc_enable,
	.disable = pablo_hw_yuvsc_disable,
	.shot = pablo_hw_yuvsc_shot,
	.frame_ndone = pablo_hw_yuvsc_frame_ndone,
	.notify_timeout = pablo_hw_yuvsc_notify_timeout,
	.reset = pablo_hw_yuvsc_reset,
	.restore = pablo_hw_yuvsc_restore,
	.change_chain = pablo_hw_yuvsc_change_chain,
	.set_regs = pablo_hw_yuvsc_set_regs,
	.dump_regs = pablo_hw_yuvsc_dump_regs,
	.set_config = pablo_hw_yuvsc_set_config,
	.cmp_fcount = pablo_hw_yuvsc_cmp_fcount,
	.recover = pablo_hw_yuvsc_recover,
	.query = pablo_hw_yuvsc_query,
	.dump_params = pablo_hw_yuvsc_dump_params,
};

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
	u32 dbg_lv = atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;

	msdbg_hw(dbg_lv, "[F%d]FE\n", instance, hw_ip, fcount);

	atomic_add(1, &hw_ip->count.fe);
	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_FRAME_END);
	CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

	if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
		mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)",
				instance, hw_ip,
				atomic_read(&hw_ip->count.fs),
				atomic_read(&hw_ip->count.fe),
				atomic_read(&hw_ip->count.dma),
				status);
	}

	wake_up(&hw_ip->status.wait_queue);
}

static void int0_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_yuvsc *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT0] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	CALL_HW_YUVSC_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();
}

static void int0_warn_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_yuvsc *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT0] HW Warning!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	CALL_HW_YUVSC_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();

	CALL_HW_YUVSC_OPS(hw, clr_cotf_err, hw_ip->pmio);
}

static void int1_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_yuvsc *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT1] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	CALL_HW_YUVSC_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();
}

static int pablo_hw_yuvsc_check_int(struct is_hw_ip *hw_ip, u32 status)
{
	struct pablo_hw_yuvsc *hw = GET_HW(hw_ip);
	u32 hw_fcount = atomic_read(&hw_ip->fcount);
	u32 instance = atomic_read(&hw_ip->instance);
	u32 fs, fe;
	u32 int_status;

	fs = CALL_HW_YUVSC_OPS(hw, is_occurred, status, BIT_MASK(INT_FRAME_START)) ?
		     BIT_MASK(INT_FRAME_START) :
		     0;
	fe = CALL_HW_YUVSC_OPS(hw, is_occurred, status, BIT_MASK(INT_FRAME_END)) ?
		     BIT_MASK(INT_FRAME_END) :
		     0;

	if (fs && fe)
		mswarn_hw("[F%d] start/end overlapped!!", instance, hw_ip, hw_fcount);

	int_status = fs | fe;

	msdbg_hw(4, "[F%d] int_status 0x%08x fs 0x%08x fe 0x%08x\n", instance, hw_ip, hw_fcount,
		int_status, fs, fe);

	while (int_status) {
		switch (hw->event_state) {
		case YUVSC_FS:
			if (fe) {
				hw->event_state = YUVSC_FE;
				fe_handler(hw_ip, instance, hw_fcount, status);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case YUVSC_FE:
			if (fs) {
				hw->event_state = YUVSC_FS;
				fs_handler(hw_ip, instance, hw_fcount, status);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case YUVSC_INIT:
			if (fs) {
				hw->event_state = YUVSC_FS;
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

static int pablo_hw_yuvsc_handler_int(u32 id, void *ctx)
{
	struct is_hw_ip *hw_ip;
	struct pablo_hw_yuvsc *hw;
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
		int_status = pablo_hw_yuvsc_check_int(hw_ip, status);
		if (int_status)
			mswarn_hw("[F%d] invalid interrupt: event_state(%ld), int_status(%x)",
				instance, hw_ip, hw_fcount, hw->event_state, int_status);

		if (CALL_HW_YUVSC_OPS(hw, is_occurred, status, BIT_MASK(INT_SETTING_DONE)))
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

		if (CALL_HW_YUVSC_OPS(hw, is_occurred, status, BIT_MASK(INT_ERR0))) {
			int0_err_handler(hw_ip, instance, hw_fcount, status);

			if (CALL_HW_OPS(hw_ip, clear_crc_status, instance, hw_ip) > 0)
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
		}

		if (CALL_HW_YUVSC_OPS(hw, is_occurred, status, BIT_MASK(INT_WARN0)))
			int0_warn_handler(hw_ip, instance, hw_fcount, status);
	} else {
		if (CALL_HW_YUVSC_OPS(hw, is_occurred, status, BIT_MASK(INT_ERR1))) {
			int1_err_handler(hw_ip, instance, hw_fcount, status);

			if (CALL_HW_OPS(hw_ip, clear_crc_status, instance, hw_ip) > 0)
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
		}
	}
	if (unlikely(dbg_info.en[YUVSC_DBG_S_TRIGGER]))
		_dbg_handler(hw_ip, id, status);

	return 0;
}

static struct pablo_mmio *_pablo_hw_yuvsc_pmio_init(struct is_hw_ip *hw_ip)
{
	int ret;
	struct pablo_mmio *pmio;
	struct pmio_config *pcfg;

	pcfg = &hw_ip->pmio_config;

	pcfg->name = "YUVSC";
	pcfg->mmio_base = hw_ip->mmio_base;
	pcfg->cache_type = PMIO_CACHE_NONE;
	pcfg->phys_base = hw_ip->regs_start[REG_SETA];

	yuvsc_hw_g_pmio_cfg(pcfg);

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	ret = pmio_field_bulk_alloc(pmio, &hw_ip->pmio_fields,
				pcfg->fields, pcfg->num_fields);
	if (ret) {
		serr_hw("Failed to alloc YUVSC PMIO field_bulk. ret %d", hw_ip, ret);
		goto err_field_bulk_alloc;
	}

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
	pmio = ERR_PTR(ret);
err_init:
	return pmio;
}

static void _pablo_hw_yuvsc_pmio_deinit(struct is_hw_ip *hw_ip)
{
	struct pablo_mmio *pmio = hw_ip->pmio;

	pmio_field_bulk_free(pmio, hw_ip->pmio_fields);
	pmio_exit(pmio);
}

int pablo_hw_yuvsc_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
		struct is_interface_ischain *itfc, int id, const char *name)
{
	struct pablo_mmio *pmio;
	int hw_slot;

	hw_ip->ops = &pablo_hw_yuvsc_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("Invalid hw_slot %d", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &pablo_hw_yuvsc_handler_int;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &pablo_hw_yuvsc_handler_int;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	pmio = _pablo_hw_yuvsc_pmio_init(hw_ip);
	if (IS_ERR(pmio)) {
		serr_hw("Failed to yuvsc pmio_init.", hw_ip);
		return -EINVAL;
	}

	hw_ip->pmio = pmio;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_hw_yuvsc_probe);

void pablo_hw_yuvsc_remove(struct is_hw_ip *hw_ip)
{
	struct is_interface_ischain *itfc = hw_ip->itfc;
	int id = hw_ip->id;
	int hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = false;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = false;

	_pablo_hw_yuvsc_pmio_deinit(hw_ip);
}
EXPORT_SYMBOL_GPL(pablo_hw_yuvsc_remove);
