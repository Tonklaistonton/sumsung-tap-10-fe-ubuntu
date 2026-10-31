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

#include "exynos-is.h"
#include "is-hw.h"
#include "pablo-icpu-adapter.h"
#include "pablo-crta-bufmgr.h"
#include "pablo-hw-api-rgbp.h"
#include "pablo-hw-rgbp.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

#define GET_HW(hw_ip) ((struct pablo_hw_rgbp *)hw_ip->priv_info)
#define CALL_HW_RGBP_OPS(hw, op, args...) ((hw && hw->ops) ? hw->ops->op(args) : 0)
#define GET_EN_STR(en) (en ? "ON" : "OFF")

static const enum pablo_crta_buf_type data_to_buf[PABLO_CRTA_CSTAT_END_MAX] = {
	PABLO_CRTA_BUF_PRE_THUMB, PABLO_CRTA_BUF_AE_THUMB, PABLO_CRTA_BUF_AWB_THUMB,
	PABLO_CRTA_BUF_RGBY_HIST, PABLO_CRTA_BUF_CDAF_MW, PABLO_CRTA_BUF_MAX, /* invalid type */
};

static DEFINE_MUTEX(cmn_reg_lock);

/* DEBUG module params */
enum pablo_hw_rgbp_dbg_type {
	RGBP_DBG_NONE = 0,
	RGBP_DBG_S_TRIGGER,
	RGBP_DBG_CR_DUMP,
	RGBP_DBG_STATE_DUMP,
	RGBP_DBG_RTA_DUMP,
	RGBP_DBG_S2D,
	RGBP_DBG_APB_DIRECT,
	RGBP_DBG_EN_DTP,
	RGBP_DBG_TYPE_NUM,
};

struct pablo_hw_rgbp_dbg_info {
	bool en[RGBP_DBG_TYPE_NUM];
	u32 instance;
	u32 fcount;
	u32 int_msk[2];
};

static struct pablo_hw_rgbp_dbg_info dbg_info = {
	.en = {false, },
	.instance = 0,
	.fcount = 0,
	.int_msk = {0, },
};

static struct is_rgbp_config default_config = {
	.luma_shading_bypass = 1,
	.drcclct_bypass = 1,
	.thstat_awb_hdr_bypass = 1,
	.rgbyhist_hdr_bypass = 1,
};

static int is_hw_rgbp_frame_start_callback(void *caller, void *ctx, void *rsp_msg);
static int is_hw_rgbp_frame_end_callback(void *caller, void *ctx, void *rsp_msg);

static int pablo_hw_rgbp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance,
	u32 fcount, void *conf);

static int _parse_dbg_s_trigger(char **argv, int argc)
{
	int ret;
	u32 instance, fcount, int_msk0, int_msk1;

	if (argc != 4) {
		err("[DBG_RGBP] Invalid arguments! %d", argc);
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &instance);
	if (ret) {
		err("[DBG_RGBP] Invalid instance %d ret %d", instance, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[1], 0, &fcount);
	if (ret) {
		err("[DBG_RGBP] Invalid fcount %d ret %d", fcount, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[2], 0, &int_msk0);
	if (ret) {
		err("[DBG_RGBP] Invalid int_msk0 %d ret %d", int_msk0, ret);
		return -EINVAL;
	}

	ret = kstrtouint(argv[3], 0, &int_msk1);
	if (ret) {
		err("[DBG_RGBP] Invalid int_msk1 %d ret %d", int_msk1, ret);
		return -EINVAL;
	}

	dbg_info.instance = instance;
	dbg_info.fcount = fcount;
	dbg_info.int_msk[0] = int_msk0;
	dbg_info.int_msk[1] = int_msk1;

	info("[DBG_RGBP] S_TRIGGER:[I%d][F%d] INT0 0x%08x INT1 0x%08x\n",
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

static struct dbg_parser_info dbg_parsers[RGBP_DBG_TYPE_NUM] = {
	[RGBP_DBG_S_TRIGGER] = {
		"S_TRIGGER",
		"<instance> <fcount> <int_msk0> <int_msk1> ",
		_parse_dbg_s_trigger,
	},
	[RGBP_DBG_CR_DUMP] = {
		"CR_DUMP",
		"",
		NULL,
	},
	[RGBP_DBG_STATE_DUMP] = {
		"STATE_DUMP",
		"",
		NULL,
	},
	[RGBP_DBG_RTA_DUMP] = {
		"RTA_DUMP",
		"",
		NULL,
	},
	[RGBP_DBG_S2D] = {
		"S2D",
		"",
		NULL,
	},
	[RGBP_DBG_APB_DIRECT] = {
		"APB_DIRECT",
		"",
		NULL,
	},
	[RGBP_DBG_EN_DTP] = {
		"EN_DTP",
		"",
		NULL,
	},
};

static int pablo_hw_rgbp_dbg_set(const char *val)
{
	int ret = 0, argc = 0;
	char **argv;
	u32 dbg_type, en, arg_i = 0;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("[DBG_RGBP] No argument!");
		return -EINVAL;
	} else if (argc < 2) {
		err("[DBG_RGBP] Too short argument!");
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &dbg_type);
	if (ret || !dbg_type || dbg_type >= RGBP_DBG_TYPE_NUM) {
		err("[DBG_RGBP] Invalid dbg_type %u ret %d", dbg_type, ret);
		goto func_exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &en);
	if (ret) {
		err("[DBG_RGBP] Invalid en %u ret %d", en, ret);
		goto func_exit;
	}

	dbg_info.en[dbg_type] = en;
	info("[DBG_RGBP] %s[%s]\n", dbg_parsers[dbg_type].name, GET_EN_STR(dbg_info.en[dbg_type]));

	argc = (argc > arg_i) ? (argc - arg_i) : 0;

	if (argc && dbg_parsers[dbg_type].parser && dbg_parsers[dbg_type].parser(&argv[arg_i], argc)) {
		err("[DBG_RGBP] Failed to %s", dbg_parsers[dbg_type].name);
		goto func_exit;
	}

func_exit:
	argv_free(argv);

	return ret;
}

static int pablo_hw_rgbp_dbg_get(char *buffer, const size_t buf_size)
{
	const char *get_msg = "= RGBP DEBUG Configuration =====================\n"
			      "  Current Trigger Point:\n"
			      "    - instance %d\n"
			      "    - fcount %d\n"
			      "    - int0_msk 0x%08x int1_msk 0x%08x\n"
			      "================================================\n";

	return scnprintf(buffer, buf_size, get_msg, dbg_info.instance, dbg_info.fcount,
		dbg_info.int_msk[0], dbg_info.int_msk[1]);
}

static int pablo_hw_rgbp_dbg_usage(char *buffer, const size_t buf_size)
{
	int ret;
	u32 dbg_type;

	ret = scnprintf(buffer, buf_size, "[value] string value, RGBP debug features\n");
	for (dbg_type = 1; dbg_type < RGBP_DBG_TYPE_NUM; dbg_type++)
		ret += scnprintf(buffer + ret, buf_size - ret,
			"  - %10s[%3s]: echo %d <en> %s> debug_rgbp\n", dbg_parsers[dbg_type].name,
			GET_EN_STR(dbg_info.en[dbg_type]), dbg_type, dbg_parsers[dbg_type].man);

	return ret;
}

static struct pablo_debug_param debug_rgbp = {
	.type = IS_DEBUG_PARAM_TYPE_STR,
	.ops.get = pablo_hw_rgbp_dbg_get,
	.ops.set = pablo_hw_rgbp_dbg_set,
	.ops.usage = pablo_hw_rgbp_dbg_usage,
};

module_param_cb(debug_rgbp, &pablo_debug_param_ops, &debug_rgbp, 0644);

static void _dbg_handler(struct is_hw_ip *hw_ip, u32 irq_id, u32 status)
{
	struct pablo_hw_rgbp *hw;
	struct pablo_common_ctrl *pcc;
	u32 instance, fcount;

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;
	instance = atomic_read(&hw_ip->instance);
	fcount = atomic_read(&hw_ip->fcount);

	if (dbg_info.instance && dbg_info.instance != instance)
		return;
	else if (dbg_info.fcount && dbg_info.fcount != fcount)
		return;
	else if (dbg_info.int_msk[irq_id] && !(dbg_info.int_msk[irq_id] & status))
		return;

	info("[DBG_RGBP] %s:[I%d][F%d] INT%d 0x%08x\n", __func__,
		instance, fcount, irq_id, status);

	if (dbg_info.en[RGBP_DBG_CR_DUMP])
		CALL_HW_RGBP_OPS(hw, dump, hw_ip->pmio, HW_DUMP_CR);

	if (dbg_info.en[RGBP_DBG_STATE_DUMP]) {
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		CALL_HW_RGBP_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	}

	if (dbg_info.en[RGBP_DBG_S2D])
		is_debug_s2d(true, "RGBP_DBG_S2D");
}

/* RGBP HW OPS */
static int _pablo_hw_rgbp_cloader_init(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw;
	struct is_mem *mem;
	struct pablo_internal_subdev *pis;
	int ret;
	enum base_reg_index reg_id;
	resource_size_t reg_size;
	u32 reg_num, hdr_size;

	hw = GET_HW(hw_ip);
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_RGBP);
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
	pis->size[0] = ALIGN(DIV_ROUND_UP(pis->width * pis->memory_bitwidth, BITS_PER_BYTE), 32) * pis->height;
	ret = CALL_I_SUBDEV_OPS(pis, alloc, pis);
	if (ret) {
		mserr_hw("failed to alloc internal sub-device for %s: %d", instance, hw_ip,
			 pis->name, ret);
		return ret;
	}

	return ret;
}

static int _pablo_hw_rgbp_pcc_init(struct is_hw_ip *hw_ip)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct is_mem *mem;
	enum pablo_common_ctrl_mode pcc_mode;

	pcc_mode = PCC_OTF;

	hw->pcc = pablo_common_ctrl_hw_get_pcc(&hw_ip->pmio_config);
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_RGBP);

	return CALL_PCC_HW_OPS(hw->pcc, init, hw->pcc, hw_ip->pmio, hw_ip->name, pcc_mode, mem);
}

static int _pablo_hw_rgbp_dma_init(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct pablo_mmio *pmio = hw_ip->pmio;
	int ret, idx;

	rgbp_hw_g_dma_cnt(&hw->rdma_cnt, &hw->rdma_cfg_cnt, RGBP_RDMA);
	hw->rdma = pablo_zalloc(sizeof(struct is_common_dma) * hw->rdma_cnt, GFP_KERNEL);
	if (!hw->rdma) {
		mserr_hw("Failed to alloc rdma", instance, hw_ip);
		return -ENOMEM;
	}

	for (idx = 0; idx < hw->rdma_cnt; idx++) {
		ret = rgbp_hw_create_dma(&hw->rdma[idx], pmio, idx, RGBP_RDMA);
		if (ret) {
			mserr_hw("Failed to create rdma[%d]", instance, hw_ip, idx);
			goto err_create_rdma;
		}
	}

	rgbp_hw_g_dma_cnt(&hw->wdma_cnt, &hw->wdma_cfg_cnt, RGBP_WDMA);
	hw->wdma = pablo_zalloc(sizeof(struct is_common_dma) * hw->wdma_cnt, GFP_KERNEL);
	if (!hw->wdma) {
		mserr_hw("Failed to alloc wdma", instance, hw_ip);
		ret = -ENOMEM;
		goto err_create_rdma;
	}

	for (idx = 0; idx < hw->wdma_cnt; idx++) {
		ret = rgbp_hw_create_dma(&hw->wdma[idx], pmio, idx, RGBP_WDMA);
		if (ret) {
			mserr_hw("Failed to create wdma[%d]", instance, hw_ip, idx);
			goto err_create_wdma;
		}
	}

	return 0;

err_create_wdma:
	pablo_free(hw->wdma);
	hw->wdma = NULL;

err_create_rdma:
	pablo_free(hw->rdma);
	hw->rdma = NULL;

	return ret;
}

static int is_hw_rgbp_prepare(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw;
	struct pablo_mmio *pmio;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_cfg cfg = {
		0,
	};

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;
	pmio = hw_ip->pmio;

	cfg.fs_mode = PCC_VVALID_RISE;
	CALL_HW_RGBP_OPS(hw, g_int_en, cfg.int_en);
	if (CALL_PCC_OPS(pcc, enable, pcc, &cfg)) {
		mserr_hw("failed to PCC enable", instance, hw_ip);
		return -EINVAL;
	}

	CALL_HW_RGBP_OPS(hw, init, pmio, hw_ip->ch);

	return 0;
}

static int pablo_hw_rgbp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw;
	int ret;
	u32 reg_cnt = rgbp_hw_g_reg_cnt();

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, "HWRGBP");

	hw_ip->priv_info = pablo_zalloc(sizeof(struct pablo_hw_rgbp), GFP_KERNEL);
	if (!hw_ip->priv_info) {
		mserr_hw("Failed to alloc pablo_hw_rgbp", instance, hw_ip);
		return -ENOMEM;
	}

	hw = GET_HW(hw_ip);
	hw->ops = rgbp_hw_g_ops();
	hw_ip->ch = hw_ip->id - DEV_HW_RGBP0;

	hw->icpu_adt = pablo_get_icpu_adt();

	/* Setup C-loader */
	ret = _pablo_hw_rgbp_cloader_init(hw_ip, instance);
	if (ret)
		goto err_cloader_init;

	/* Setup Common-CTRL */
	ret = _pablo_hw_rgbp_pcc_init(hw_ip);
	if (ret)
		goto err_pcc_init;

	ret = _pablo_hw_rgbp_dma_init(hw_ip, instance);
	if (ret)
		goto err_dma_init;

	hw->iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	hw->cur_iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	if (!hw->iq_set.regs || !hw->cur_iq_set.regs) {
		mserr_hw("Failed to alloc iq_set regs", instance, hw_ip);
		ret = -ENOMEM;

		goto err_alloc_iq_set;
	}

	clear_bit(CR_SET_CONFIG, &hw->iq_set.state);
	set_bit(CR_SET_EMPTY, &hw->iq_set.state);
	spin_lock_init(&hw->iq_set.slock);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open\n", instance, hw_ip);

	return 0;

err_alloc_iq_set:
	if (hw->iq_set.regs) {
		pablo_free(hw->iq_set.regs);
		hw->iq_set.regs = NULL;
	}

	if (hw->cur_iq_set.regs) {
		pablo_free(hw->cur_iq_set.regs);
		hw->cur_iq_set.regs = NULL;
	}

	if (hw->wdma) {
		pablo_free(hw->wdma);
		hw->wdma = NULL;
	}

	if (hw->rdma) {
		pablo_free(hw->rdma);
		hw->rdma = NULL;
	}

err_dma_init:
	CALL_PCC_HW_OPS(hw->pcc, deinit, hw->pcc);

err_pcc_init:
	CALL_I_SUBDEV_OPS(&hw->subdev_cloader, free, &hw->subdev_cloader);

err_cloader_init:
	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	return ret;
}

static int is_hw_rgbp_register_icpu_msg_cb(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	struct pablo_icpu_adt *icpu_adt = hw_rgbp->icpu_adt;
	enum pablo_hic_cmd_id cmd_id;
	int ret;

	cmd_id = PABLO_HIC_CSTAT_FRAME_START;
	ret = CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance, cmd_id,
		is_hw_rgbp_frame_start_callback);
	if (ret)
		goto exit;

	cmd_id = PABLO_HIC_CSTAT_FRAME_END;
	ret = CALL_ADT_MSG_OPS(icpu_adt, register_response_msg_cb, instance, cmd_id,
		is_hw_rgbp_frame_end_callback);
	if (ret)
		goto exit;

exit:
	if (ret)
		mserr_hw("icpu_adt register_response_msg_cb fail. cmd_id %d", instance, hw_ip,
			cmd_id);

	return ret;
}

static void is_hw_rgbp_unregister_icpu_msg_cb(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	struct pablo_icpu_adt *icpu_adt = hw_rgbp->icpu_adt;

	CALL_ADT_MSG_OPS(
		icpu_adt, register_response_msg_cb, instance, PABLO_HIC_CSTAT_FRAME_START, NULL);
	CALL_ADT_MSG_OPS(
		icpu_adt, register_response_msg_cb, instance, PABLO_HIC_CSTAT_FRAME_END, NULL);
}

static int pablo_hw_rgbp_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	int ret = 0;
	struct pablo_hw_rgbp *hw_rgbp = NULL;

	hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;

	hw_rgbp->param_set[instance].reprocessing = flag;

	ret = is_hw_rgbp_register_icpu_msg_cb(hw_ip, instance);
	if (ret)
		goto err_register_icpu_msg_cb;

	if (hw_ip->group[instance] && hw_ip->group[instance]->device)
		hw_rgbp->sensor_itf[instance] =
			is_sensor_get_sensor_interface(hw_ip->group[instance]->device->sensor);
	else
		hw_rgbp->sensor_itf[instance] = NULL;

	set_bit(HW_INIT, &hw_ip->state);

	msdbg_hw(2, "init\n", instance, hw_ip);

	return 0;

err_register_icpu_msg_cb:
	is_hw_rgbp_unregister_icpu_msg_cb(hw_ip, instance);

	return ret;
}

static int pablo_hw_rgbp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw;
	struct is_hardware *hardware;
	struct pablo_internal_subdev *pis;
	int ret;
	struct is_hw_ip *hw_ip_phys;
	int i, max_num;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw = GET_HW(hw_ip);

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	ret = CALL_HW_RGBP_OPS(hw, wait_idle, hw_ip->pmio);
	if (ret)
		mserr_hw("failed to wait_idle. ret %d", instance, hw_ip, ret);

	max_num = hw_ip->ip_num;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, DEV_HW_RGBP + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num) {
		for (i = max_num - 1; i >= 0; i--) {
			hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, DEV_HW_RGBP + i);
			CALL_HW_RGBP_OPS(hw, reset, hw_ip_phys->pmio);
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

	if (hw->rdma) {
		pablo_free(hw->rdma);
		hw->rdma = NULL;
	}

	if (hw->wdma) {
		pablo_free(hw->wdma);
		hw->wdma = NULL;
	}

	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int pablo_hw_rgbp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_mmio *pmio;
	struct pablo_hw_rgbp *hw;
	u32 i, max_num;

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
	hw->event_state = RGBP_INIT;
	max_num = hw_ip->ip_num;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		struct is_hw_ip *hw_ip_phys =
			CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip, DEV_HW_RGBP + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num)
		CALL_HW_RGBP_OPS(hw, s_lbctrl, hw_ip->locomotive->pmio);

	mutex_unlock(&cmn_reg_lock);

	hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
	if (pmio_reinit_cache(pmio, &hw_ip->pmio_config)) {
		mserr_hw("failed to reinit PMIO cache", instance, hw_ip);
		return -EINVAL;
	}

	if (unlikely(dbg_info.en[RGBP_DBG_APB_DIRECT])) {
		pmio_cache_set_only(pmio, false);
		pmio_cache_set_bypass(pmio, true);
	} else {
		pmio_cache_set_only(pmio, true);
	}

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static void pablo_hw_rgbp_clear(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);

	memset(&hw->config[instance], 0x00, sizeof(struct is_rgbp_config));
	hw->cur_iq_set.size = 0;
	hw->event_state = RGBP_INIT;
	frame_manager_flush(GET_SUBDEV_I_FRAMEMGR(&hw->subdev_cloader));
	clear_bit(HW_CONFIG, &hw_ip->state);
}


static int pablo_hw_rgbp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_hw_rgbp *hw;
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
	timetowait = wait_event_timeout(
		hw_ip->status.wait_queue, !atomic_read(&hw_ip->status.Vvalid), IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout. timetowait %uus", instance, hw_ip,
			jiffies_to_usecs(IS_HW_STOP_TIMEOUT));
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		CALL_HW_RGBP_OPS(hw, dump, pmio, HW_DUMP_DBG_STATE);
		ret = -ETIME;
	}

	pablo_hw_rgbp_clear(hw_ip, instance);

	/* Disable PMIO Cache */
	pmio_cache_set_only(pmio, false);
	pmio_cache_set_bypass(pmio, true);

	rgbp_hw_s_otf(hw_ip->pmio, false);
	CALL_PCC_OPS(pcc, disable, pcc);

	return ret;
}

static int pablo_hw_rgbp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);

	FIMC_BUG(!region);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw->instance = IS_STREAM_COUNT;

	return 0;
}

static void _pablo_hw_rgbp_update_param(struct is_hw_ip *hw_ip, u32 instance,
	struct is_frame *frame, struct rgbp_param_set *param_set)
{
	if (frame->type == SHOT_TYPE_INTERNAL) {
		hw_ip->internal_fcount[instance] = frame->fcount;
		rgbp_hw_s_internal_shot(param_set);
	} else {
		rgbp_hw_s_external_shot(frame->parameter, param_set, frame->pmap);
	}
}

static int _pablo_hw_rgbp_s_iq_regs(struct is_hw_ip *hw_ip, u32 instance, struct c_loader_buffer *clb)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct pablo_mmio *pmio;
	struct pablo_hw_rgbp_iq *iq_set;
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

		if (unlikely(dbg_info.en[RGBP_DBG_RTA_DUMP])) {
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

static void _pablo_hw_rgbp_s_cmd(struct pablo_hw_rgbp *hw, struct c_loader_buffer *clb, u32 fcount,
				 struct pablo_common_ctrl_cmd *cmd)
{
	cmd->set_mode = PCC_APB_DIRECT;
	cmd->fcount = fcount;
	cmd->int_grp_en = CALL_HW_RGBP_OPS(hw, g_int_grp_en);

	if (!clb || unlikely(dbg_info.en[RGBP_DBG_APB_DIRECT]))
		return;

	cmd->base_addr = clb->header_dva;
	cmd->header_num = clb->num_of_headers;
	cmd->set_mode = PCC_DMA_DIRECT;
}

static int _pablo_hw_rgbp_set_rdma(struct is_hw_ip *hw_ip, struct pablo_hw_rgbp *hw, struct rgbp_param_set *param_set,
		u32 instance, u32 dma_id)
{
	pdma_addr_t *input_dva = NULL;
	u32 cmd, comp_sbwc_en, payload_size;
	u32 cache_hint = IS_LLC_CACHE_HINT_LAST_ACCESS;
	int ret = 0;

	input_dva = rgbp_hw_g_input_dva(param_set, instance, dma_id, &cmd);

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw->rdma[dma_id].name, cmd);

	ret = rgbp_hw_s_rdma_cfg(&hw->rdma[dma_id], param_set, cmd, cache_hint,
		&comp_sbwc_en, &payload_size);
	if (ret) {
		mserr_hw("Failed to initialize RGBP_DMA(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = rgbp_hw_s_rdma_addr(&hw->rdma[dma_id], input_dva, 0, (hw_ip->num_buffers & 0xffff),
			0, comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("Failed to set RGBP_RDMA(%d) address", instance, hw_ip, dma_id);
			return -EINVAL;
		}
	}

	return 0;
}

static int _pablo_hw_rgbp_set_wdma(struct is_hw_ip *hw_ip, struct pablo_hw_rgbp *hw, struct rgbp_param_set *param_set,
		u32 instance, u32 dma_id)
{
	pdma_addr_t *output_dva = NULL;
	u32 cmd;
	u32 cache_hint = IS_LLC_CACHE_HINT_VOTF_TYPE;
	int ret = 0;
	struct rgbp_dma_cfg dma_cfg;

	output_dva = rgbp_hw_g_output_dva(param_set, instance, dma_id, &cmd);

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw->wdma[dma_id].name, cmd);

	dma_cfg.enable = cmd;
	dma_cfg.cache_hint = cache_hint;
	dma_cfg.num_buffers = hw_ip->num_buffers & 0xffff;

	ret = rgbp_hw_s_wdma_cfg(&hw->wdma[dma_id], hw_ip->pmio, param_set, output_dva, &dma_cfg);
	if (ret) {
		mserr_hw("Failed to initialize RGBP_DMA(%d)", instance, hw_ip, dma_id);
		return -EINVAL;
	}

	return 0;
}

static int _pablo_hw_rgbp_s_dma(struct is_hw_ip *hw_ip, struct is_frame *frame,
	struct rgbp_param_set *param_set, struct is_rgbp_config *cfg)
{
	struct pablo_hw_rgbp *hw;
	struct param_dma_input *pdi;
	struct param_dma_output *pdo;
	pdma_addr_t *param_set_dva;
	dma_addr_t *frame_dva;
	char *name;
	u32 cur_idx, id;
	int ret;

	hw = GET_HW(hw_ip);
	cur_idx = frame->cur_buf_index;

	name = __getname();
	if (!name) {
		mserr_hw("failed to get alloc for dma name", frame->instance, hw_ip);
		return -ENOMEM;
	}

	for (id = 0; id < hw->rdma_cfg_cnt; id++) {
		if (rgbp_hw_g_rdma_param(frame, &frame_dva, param_set, &param_set_dva, &pdi, name, id))
			continue;

		CALL_HW_OPS(hw_ip, dma_cfg, name, hw_ip, frame, cur_idx, frame->num_buffers,
			&pdi->cmd, pdi->plane, param_set_dva, frame_dva);
	}

	for (id = 0; id < hw->wdma_cfg_cnt; id++) {
		if (rgbp_hw_g_wdma_param(frame, &frame_dva, param_set, &param_set_dva, &pdo, name, id))
			continue;

		CALL_HW_OPS(hw_ip, dma_cfg, name, hw_ip, frame, cur_idx, frame->num_buffers,
			&pdo->cmd, pdo->plane, param_set_dva, frame_dva);
	}
	__putname(name);

	rgbp_hw_s_dma_cfg(param_set, cfg);

	for (id = 0; id < hw->rdma_cnt; id++) {
		ret = _pablo_hw_rgbp_set_rdma(hw_ip, hw, param_set, frame->instance, id);
		if (ret) {
			mserr_hw("_pablo_hw_rgbp_set_rdma is fail", frame->instance, hw_ip);
			return -EINVAL;
		}
	}

	for (id = 0; id < hw->wdma_cnt; id++) {
		ret = _pablo_hw_rgbp_set_wdma(hw_ip, hw, param_set, frame->instance, id);
		if (ret) {
			mserr_hw("_pablo_hw_rgbp_set_wdma is fail", frame->instance, hw_ip);
			return -EINVAL;
		}
	}

	return 0;
}

static void _pablo_hw_rgbp_s_col_row(
	struct is_hw_ip *hw_ip, struct is_frame *frame, bool en, u32 height)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	u32 line_ratio, row;

	if (!en) {
		CALL_HW_RGBP_OPS(hw, s_int_on_col_row, hw_ip->pmio, false, 0, 0);
		return;
	}

	line_ratio = frame->time_cfg.line_ratio;
	if (!line_ratio)
		line_ratio = 50;

	row = height * line_ratio / 100;

	/* row cannot exceed the rgbp input IMG height */
	if (row >= height)
		row = ZERO_IF_NEG(height - 1);

	msdbg_hw(2, "s_col_row: %d@%d (%d%%)\n", frame->instance, hw_ip, row, height, line_ratio);

	CALL_HW_RGBP_OPS(hw, s_int_on_col_row, hw_ip->pmio, true, 0, row);
}

static int pablo_hw_rgbp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct pablo_hw_rgbp *hw;
	struct pablo_mmio *pmio;
	struct rgbp_param_set *param_set = NULL;
	u32 instance, fcount, cur_idx;
	struct is_framemgr *framemgr;
	struct is_frame *cl_frame;
	struct is_priv_buf *pb;
	struct c_loader_buffer clb, *p_clb = NULL;
	struct is_rgbp_config *cfg;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_frame_cfg frame_cfg = {
		0,
	};
	int ret;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	enum pablo_common_ctrl_mode pcc_mode;

	instance = frame->instance;
	fcount = frame->fcount;
	cur_idx = frame->cur_buf_index;

	msdbgs_hw(2, "[F:%d]shot(%d)\n", instance, hw_ip, fcount, cur_idx);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw = GET_HW(hw_ip);
	pmio = hw_ip->pmio;
	pcc = hw->pcc;
	cfg = &hw->config[instance];

	/* HW parameter */
	param_set = &hw->param_set[instance];
	param_set->instance = instance;
	param_set->fcount = fcount;
	param_set->mono_mode = hw_ip->region[instance]->parameter.sensor.config.mono_mode;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		pmio_cache_set_bypass(pmio, false);
		pmio_cache_set_only(pmio, true);
		is_hw_rgbp_prepare(hw_ip, instance);
	}

	if (param_set->reprocessing)
		pcc_mode = PCC_M2M;
	else
		pcc_mode = PCC_OTF;
	CALL_PCC_HW_OPS(pcc, set_mode, pcc, pcc_mode);

	/* Prepare C-Loader buffer */
	framemgr = GET_SUBDEV_I_FRAMEMGR(&hw->subdev_cloader);
	cl_frame = peek_frame(framemgr, FS_FREE);
	if (likely(!dbg_info.en[RGBP_DBG_APB_DIRECT] && cl_frame)) {
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

	hw->post_frame_gap =
		test_bit(HW_CONFIG, &hw_ip->state) ? frame->time_cfg.post_frame_gap : 0;

	_pablo_hw_rgbp_update_param(hw_ip, instance, frame, param_set);

	if (is_debug_support_crta()) {
		if (likely(!test_bit(hw_ip->id, &debug_iq))) {
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);
			ret = _pablo_hw_rgbp_s_iq_regs(hw_ip, instance, p_clb);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);
			if (ret)
				mserr_hw("Failed to set iq regs", instance, hw_ip);
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		}
	}

	if (unlikely(!hw->cur_iq_set.size)) {
		pablo_hw_rgbp_set_config(hw_ip, 0, instance, frame->fcount, &default_config);
		CALL_HW_RGBP_OPS(hw, s_bypass, pmio);
	}

	/* FRO */
	hw_ip->num_buffers = frame->num_buffers;
	frame_cfg.num_buffers = frame->num_buffers;

	CALL_HW_RGBP_OPS(hw, s_core, pmio, param_set, cfg);
	CALL_HW_RGBP_OPS(hw, s_path, pmio, param_set, &frame_cfg, cfg);

	_pablo_hw_rgbp_s_dma(hw_ip, frame, param_set, cfg);

	if (frame->shot_ext) {
		if ((param_set->tnr_mode != frame->shot_ext->tnr_mode) &&
				!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
			msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n",
					instance, hw_ip, fcount,
					param_set->tnr_mode, frame->shot_ext->tnr_mode);
		param_set->tnr_mode = frame->shot_ext->tnr_mode;
	} else {
		mswarn_hw("[F%d]frame->shot_ext is null", instance, hw_ip, fcount);
		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
	}

	msdbgs_hw(2, "[F:%d] batch(%d, %d)", instance, hw_ip, frame->fcount, frame_cfg.num_buffers,
		cur_idx);

	if (param_set->control.strgen == CONTROL_COMMAND_START)
		CALL_HW_RGBP_OPS(hw, s_strgen, pmio);

	CALL_HW_RGBP_OPS(hw, s_dtp, pmio, param_set, dbg_info.en[RGBP_DBG_EN_DTP]);

	if (param_set->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE)
		_pablo_hw_rgbp_s_col_row(hw_ip, frame, true, param_set->otf_input.height);
	else
		_pablo_hw_rgbp_s_col_row(hw_ip, frame, false, 0);

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
	_pablo_hw_rgbp_s_cmd(hw, p_clb, fcount, &frame_cfg.cmd);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);

	set_bit(HW_CONFIG, &hw_ip->state);

	CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);

	return 0;
}

static int pablo_hw_rgbp_frame_ndone(struct is_hw_ip *hw_ip, struct is_frame *frame,
		enum ShotErrorType done_type)
{
	if (test_bit(hw_ip->id, &frame->core_flag)) {
		return CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1,
			IS_HW_CORE_END, done_type, false);
	}

	return 0;
}

static int pablo_hw_rgbp_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw->pcc;

	msinfo_hw("reset\n", instance, hw_ip);

	rgbp_hw_s_otf(hw_ip->pmio, false);
	return CALL_PCC_OPS(pcc, reset, pcc);
}

static int pablo_hw_rgbp_restore(struct is_hw_ip *hw_ip, u32 instance)
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

static int pablo_hw_rgbp_change_chain(struct is_hw_ip *hw_ip, u32 instance, u32 next_id,
	struct is_hardware *hardware)
{
	struct is_hw_ip *next_hw_ip;
	struct pablo_hw_rgbp *hw, *next_hw;
	u32 next_hw_id = DEV_HW_RGBP0 + next_id;
	u32 curr_id;
	int ret = 0;

	curr_id = hw_ip->ch;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip, curr_id, next_id);
		goto out;
	}

	hw = GET_HW(hw_ip);
	if (!hw) {
		mserr_hw("failed to get HW RGBP%d", instance, hw_ip, hw_ip->ch);
		return -ENODEV;
	}

	next_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, next_hw_id);
	if (!next_hw_ip) {
		merr_hw("[ID:%d]invalid next next_hw_id", instance, next_hw_id);
		return -EINVAL;
	}

	next_hw = GET_HW(next_hw_ip);
	if (!next_hw) {
		mserr_hw("failed to get next HW RGBP%d", instance, next_hw_ip, next_hw_id);
		return -ENODEV;
	}

	if (!test_and_clear_bit(instance, &hw_ip->run_rsc_state))
		mswarn_hw("try to disable disabled instance", instance, hw_ip);

	ret = pablo_hw_rgbp_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("failed to pablo_hw_rgbp_disable ret(%d)", instance, hw_ip, ret);
		if (ret != -EWOULDBLOCK)
			return -EINVAL;
	}

	/*
	 * Copy instance information.
	 * But do not clear current hw_ip,
	 * because logical(initial) HW must be referred at close time.
	 */
	next_hw->param_set[instance] = hw->param_set[instance];
	next_hw->sensor_itf[instance] = hw->sensor_itf[instance];

	next_hw_ip->group[instance] = hw_ip->group[instance];
	next_hw_ip->region[instance] = hw_ip->region[instance];
	next_hw_ip->stm[instance] = hw_ip->stm[instance];

	/* set & clear physical HW */
	set_bit(next_hw_id, &hardware->hw_map[instance]);
	clear_bit(hw_ip->id, &hardware->hw_map[instance]);

	if (test_and_set_bit(instance, &next_hw_ip->run_rsc_state))
		mswarn_hw("try to enable enabled instance", instance, next_hw_ip);

	ret = pablo_hw_rgbp_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("failed to pablo_hw_rgbp_enable", instance, next_hw_ip);
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
		msinfo_hw("decrease hw_ip rsccount(%d)", instance, hw_ip, atomic_read(&hw_ip->rsccount));
	}

	if (!test_bit(next_hw_ip->id, &hardware->logical_hw_map[instance])) {
		atomic_inc(&next_hw_ip->rsccount);
		msinfo_hw("increase next_hw_ip rsccount(%d)", instance, next_hw_ip, atomic_read(&next_hw_ip->rsccount));
	}

	msinfo_hw("change_chain done (state: curr(0x%lx) next(0x%lx))", instance, hw_ip,
		hw_ip->state, next_hw_ip->state);
out:
	return ret;
}

static int pablo_hw_rgbp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id,
	u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct pablo_hw_rgbp_iq *iq_set;
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

static int pablo_hw_rgbp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct is_common_dma *dma;
	u32 idx;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		CALL_HW_RGBP_OPS(hw, dump, hw_ip->pmio, HW_DUMP_CR);
		break;
	case IS_REG_DUMP_DMA:
		for (idx = 0; idx < hw->rdma_cnt; idx++) {
			dma = &hw->rdma[idx];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (idx = 0; idx < hw->wdma_cnt; idx++) {
			dma = &hw->wdma[idx];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int pablo_hw_rgbp_set_config(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance,
	u32 fcount, void *conf)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct is_rgbp_config *org_cfg = &hw->config[instance];
	struct is_rgbp_config *new_cfg = (struct is_rgbp_config *)conf;

	if (org_cfg->sensor_center_x != new_cfg->sensor_center_x ||
	    org_cfg->sensor_center_y != new_cfg->sensor_center_y)
		msinfo_hw("[F:%d] RGBP sensor center(%d x %d)\n", instance, hw_ip, fcount,
			new_cfg->sensor_center_x, new_cfg->sensor_center_y);

	if (org_cfg->lsc_step_x != new_cfg->lsc_step_x ||
	    org_cfg->lsc_step_y != new_cfg->lsc_step_y)
		msinfo_hw("[F:%d] RGBP lsc step(%d x %d)\n", instance, hw_ip, fcount,
			new_cfg->lsc_step_x, new_cfg->lsc_step_y);

	if (org_cfg->luma_shading_bypass != new_cfg->luma_shading_bypass)
		msinfo_hw("[F:%d] RGBP luma shading bypass(%d)\n", instance, hw_ip, fcount,
			new_cfg->luma_shading_bypass);

	if (org_cfg->drcclct_bypass != new_cfg->drcclct_bypass ||
	    org_cfg->drc_grid_w != new_cfg->drc_grid_w ||
	    org_cfg->drc_grid_h != new_cfg->drc_grid_h)
		msinfo_hw("[F:%d] RGBP drcclct bypass(%d), grid(%d x %d)\n", instance, hw_ip,
			fcount, new_cfg->drcclct_bypass, new_cfg->drc_grid_w,
			new_cfg->drc_grid_h);

	if (org_cfg->thstat_awb_hdr_bypass != new_cfg->thstat_awb_hdr_bypass ||
	    org_cfg->thstat_awb_hdr_grid_w != new_cfg->thstat_awb_hdr_grid_w ||
	    org_cfg->thstat_awb_hdr_grid_h != new_cfg->thstat_awb_hdr_grid_h)
		msinfo_hw("[F:%d] RGBP thstat awb hdr bypass(%d), grid(%d x %d)\n", instance, hw_ip,
			fcount, new_cfg->thstat_awb_hdr_bypass,
			new_cfg->thstat_awb_hdr_grid_w,	new_cfg->thstat_awb_hdr_grid_h);

	if (org_cfg->rgbyhist_hdr_bypass != new_cfg->rgbyhist_hdr_bypass ||
	    org_cfg->rgbyhist_hdr_bin_num != new_cfg->rgbyhist_hdr_bin_num ||
	    org_cfg->rgbyhist_hdr_hist_num != new_cfg->rgbyhist_hdr_hist_num)
		msinfo_hw("[F:%d] RGBP rgbyhist hdr bypass(%d), num(%d, %d)\n", instance, hw_ip,
			fcount, new_cfg->rgbyhist_hdr_bypass,
			new_cfg->rgbyhist_hdr_bin_num,	new_cfg->rgbyhist_hdr_hist_num);

	memcpy(org_cfg, new_cfg, sizeof(struct is_rgbp_config));

	return 0;
}

static int pablo_hw_rgbp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_LIGHT);
	CALL_HW_RGBP_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);

	return 0;
}

static void _pablo_hw_rgbp_g_pcfi(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame,
	struct pablo_rta_frame_info *prfi)
{
	struct pablo_area *sensor_crop = &prfi->sensor_crop;
	struct rgbp_param *rgbp_p = &frame->parameter->rgbp;

	if (frame->sensor_rms_crop_ratio && rgbp_p->otf_input.cmd) {
		struct pablo_size sensor_size;

		sensor_size.width =
			prfi->sensor_calibration_size.width * 1000 / prfi->sensor_binning.x;
		sensor_size.height =
			prfi->sensor_calibration_size.height * 1000 / prfi->sensor_binning.y;

		CALL_HW_RGBP_OPS(GET_HW(hw_ip), s_rms_crop, &sensor_size, sensor_crop, rgbp_p,
				 frame->sensor_rms_crop_ratio);
	}

	if (rgbp_p->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		prfi->rgbp_input_size.width = rgbp_p->otf_input.width;
		prfi->rgbp_input_size.height = rgbp_p->otf_input.height;
		prfi->rgbp_crop_dmsc.offset.x = rgbp_p->otf_input.bayer_crop_offset_x;
		prfi->rgbp_crop_dmsc.offset.y = rgbp_p->otf_input.bayer_crop_offset_y;
		prfi->rgbp_crop_dmsc.size.width = rgbp_p->otf_input.bayer_crop_width;
		prfi->rgbp_crop_dmsc.size.height = rgbp_p->otf_input.bayer_crop_height;
	} else {
		prfi->rgbp_input_size.width = rgbp_p->dma_input.width;
		prfi->rgbp_input_size.height = rgbp_p->dma_input.height;
		prfi->rgbp_crop_dmsc.offset.x = rgbp_p->dma_input.bayer_crop_offset_x;
		prfi->rgbp_crop_dmsc.offset.y = rgbp_p->dma_input.bayer_crop_offset_y;
		prfi->rgbp_crop_dmsc.size.width = rgbp_p->dma_input.bayer_crop_width;
		prfi->rgbp_crop_dmsc.size.height = rgbp_p->dma_input.bayer_crop_height;
	}

	prfi->rgbp_out_drcclct_buffer = rgbp_p->drc.cmd;
}

static void pablo_hw_rgbp_query(struct is_hw_ip *ip, u32 instance, u32 type, void *in, void *out)
{
	switch (type) {
	case PABLO_QUERY_GET_PCFI:
		_pablo_hw_rgbp_g_pcfi(
			ip, instance, (struct is_frame *)in, (struct pablo_rta_frame_info *)out);
		break;
	default:
		break;
	}
}

static int pablo_hw_rgbp_cmp_fcount(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc;

	if (!hw)
		return 0;

	pcc = hw->pcc;
	return CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount);
}

static int pablo_hw_rgbp_recover(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc;

	if (!hw)
		return 0;

	pcc = hw->pcc;
	return CALL_PCC_OPS(pcc, recover, pcc, fcount);
}

static size_t pablo_hw_rgbp_dump_params(
	struct is_hw_ip *hw_ip, u32 instance, char *buf, size_t size)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	struct rgbp_param_set *param;
	size_t rem = size;
	char *p = buf;

	param = &hw->param_set[instance];

	p = pablo_json_nstr(p, "hw name", hw_ip->name, strlen(hw_ip->name), &rem);
	p = pablo_json_uint(p, "hw id", hw_ip->id, &rem);

	p = dump_param_sensor_config(p, "sensor_config", &param->sensor_config, &rem);
	p = dump_param_control(p, "control", &param->control, &rem);
	p = dump_param_otf_input(p, "otf_input", &param->otf_input, &rem);
	p = dump_param_otf_output(p, "otf_output", &param->otf_output, &rem);
	p = dump_param_dma_input(p, "dma_input", &param->dma_input, &rem);
	p = dump_param_dma_output(p, "dma_output_hist", &param->dma_output_hist, &rem);
	p = dump_param_dma_output(p, "dma_output_awb", &param->dma_output_awb, &rem);
	p = dump_param_dma_output(p, "dma_output_drc", &param->dma_output_drc, &rem);
	p = dump_param_dma_output(p, "dma_output_sat", &param->dma_output_sat, &rem);

	p = pablo_json_uint(p, "instance", param->instance, &rem);
	p = pablo_json_uint(p, "fcount", param->fcount, &rem);
	p = pablo_json_uint(p, "tnr_mode", param->tnr_mode, &rem);
	p = pablo_json_bool(p, "reprocessing", param->reprocessing, &rem);

	return WRITTEN(size, rem);
}

const struct is_hw_ip_ops pablo_hw_rgbp_ops = {
	.open = pablo_hw_rgbp_open,
	.init = pablo_hw_rgbp_init,
	.close = pablo_hw_rgbp_close,
	.enable = pablo_hw_rgbp_enable,
	.disable = pablo_hw_rgbp_disable,
	.shot = pablo_hw_rgbp_shot,
	.set_param = pablo_hw_rgbp_set_param,
	.frame_ndone = pablo_hw_rgbp_frame_ndone,
	.reset = pablo_hw_rgbp_reset,
	.restore = pablo_hw_rgbp_restore,
	.change_chain = pablo_hw_rgbp_change_chain,
	.set_regs = pablo_hw_rgbp_set_regs,
	.dump_regs = pablo_hw_rgbp_dump_regs,
	.set_config = pablo_hw_rgbp_set_config,
	.notify_timeout = pablo_hw_rgbp_notify_timeout,
	.query = pablo_hw_rgbp_query,
	.cmp_fcount = pablo_hw_rgbp_cmp_fcount,
	.recover = pablo_hw_rgbp_recover,
	.dump_params = pablo_hw_rgbp_dump_params,
};

static void is_hw_rgbp_frame_config_lock(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
					 u32 status)
{
	struct is_hardware *hardware = hw_ip->hardware;

	u32 dbg_hw_lv =
		atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;

	msdbg_hw(dbg_hw_lv, "[F%d]FR\n", instance, hw_ip, fcount);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_CONFIG_LOCK_E);
	atomic_add(1, &hw_ip->count.cl);
	CALL_HW_OPS(hw_ip, config_lock, hw_ip, instance, fcount);
	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_CONFIG_LOCK_X);
}

static inline void is_hw_rgbp_frame_start(
	struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	int ret;
	struct is_hardware *hardware = hw_ip->hardware;
	struct pablo_hw_rgbp *hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	struct pablo_common_ctrl *pcc = hw_rgbp->pcc;
	u32 hw_fcount, delay;
	struct pablo_crta_buf_info pcsi_buf = {
		0,
	};
	struct pablo_crta_bufmgr *bufmgr;

	/* shot_fcount -> start_fcount */
	hw_fcount = atomic_read(&hw_ip->fcount);
	atomic_set(&hw_rgbp->start_fcount, hw_fcount);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
	atomic_add(1, &hw_ip->count.fs);

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, hw_fcount, __func__);

	if (unlikely(!atomic_read(&hardware->streaming[hardware->sensor_position[instance]])))
		msinfo_hw("[F%d]F.S\n", instance, hw_ip, hw_fcount);

	if (is_debug_support_crta()) {
		bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_PCSI, instance, hw_ip->ch);
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, hw_fcount, true, &pcsi_buf);
		if (ret)
			mserr_hw("[PCFI]icpu_adt get_free_buf fail", instance, hw_ip);

		ret = CALL_ADT_MSG_OPS(hw_rgbp->icpu_adt, send_msg_cstat_frame_start, instance,
			(void *)hw_ip, NULL, hw_fcount, &pcsi_buf);
		if (ret)
			mserr_hw("icpu_adt send_msg_cstat_frame_start fail", instance, hw_ip);
	}

	CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);

	if (test_bit(IS_SENSOR_AEB_SWITCHING, &hw_ip->group[instance]->device->sensor->aeb_state))
		delay = 0;
	else
		delay = hw_rgbp->post_frame_gap * hardware->dvfs_freq[IS_DVFS_CAM];

	CALL_PCC_OPS(pcc, set_delay, pcc, delay);
}

static int is_hw_rgbp_frame_start_callback(void *caller, void *ctx, void *rsp_msg)
{
	int ret;
	u32 instance, fcount;
	struct is_hw_ip *hw_ip;
	struct pablo_hw_rgbp *hw_rgbp;
	struct pablo_icpu_adt_rsp_msg *msg;
	struct pablo_crta_buf_info pcsi_buf = {
		0,
	};
	struct pablo_crta_bufmgr *bufmgr;

	if (!caller || !rsp_msg) {
		err_hw("invalid callback: caller(%p), msg(%p)", caller, rsp_msg);
		return -EINVAL;
	}

	hw_ip = (struct is_hw_ip *)caller;
	hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	msg = (struct pablo_icpu_adt_rsp_msg *)rsp_msg;
	instance = msg->instance;
	fcount = msg->fcount;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[F%d]Ignore start callback before HW config\n", instance, hw_ip, fcount);
		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[F%d]Ignore start callback invalid HW state(0x%lx)", instance, hw_ip,
			fcount, hw_ip->state);
		return 0;
	}

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, fcount, __func__);
	_is_hw_frame_dbg_ext_trace(hw_ip, fcount, DEBUG_POINT_FRAME_START, 0);

	if (msg->rsp)
		mserr_hw("frame_start fail from icpu: msg_ret(%d)", instance, hw_ip, msg->rsp);

	bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_PCSI, instance, hw_ip->ch);
	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, fcount, &pcsi_buf);
	if (!ret)
		CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &pcsi_buf);

	return 0;
}

static inline void is_hw_rgbp_frame_end(
	struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	int ret;
	struct is_hardware *hardware = hw_ip->hardware;
	struct pablo_hw_rgbp *hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	u32 start_fcount;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	unsigned long flags;
	u32 edge_score = 0, buf_idx, index;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info stat_buf[PABLO_CRTA_CSTAT_END_MAX] = {
		0,
	};
	struct pablo_crta_buf_info shot_buf = {
		0,
	};
	struct is_sensor_interface *sensor_itf;
	struct vc_buf_info_t vc_buf_info;
	u32 mlsc_hw_id;
	struct is_hw_ip *mlsc_hw_ip;

	start_fcount = atomic_read(&hw_rgbp->start_fcount);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, start_fcount, DEBUG_POINT_FRAME_END);
	atomic_add(1, &hw_ip->count.fe);

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, start_fcount, __func__);

	if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
		msinfo_hw("[F%d]F.E\n", instance, hw_ip, start_fcount);

	if (is_debug_support_crta()) {
		framemgr = hw_ip->framemgr;
		framemgr_e_barrier_common(framemgr, 0, flags);
		frame = find_frame(
			framemgr, FS_HW_WAIT_DONE, frame_fcount, (void *)(ulong)start_fcount);
		framemgr_x_barrier_common(framemgr, 0, flags);

		if (frame) {
			shot_buf.kva = frame->shot;
			shot_buf.dva = frame->shot_dva;
		} else {
			shot_buf.kva = NULL;
			shot_buf.dva = 0;
		}

		/* Drop Current frame and Skip stat data */
		if (test_and_clear_bit(HW_OVERFLOW_RECOVERY, &hw_ip->state)) {
			if (frame)
				frame->result = IS_SHOT_DROP;
			return;
		}

		if (!hw_rgbp->param_set[instance].reprocessing) {
			for (buf_idx = PABLO_CRTA_CSTAT_END_PRE_THUMB;
			     buf_idx < PABLO_CRTA_CSTAT_END_VPDAF; buf_idx++) {
				bufmgr = pablo_get_crta_bufmgr(data_to_buf[buf_idx], instance,
							       hw_ip->ch);
				CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, start_fcount,
						     &stat_buf[buf_idx]);
			}

			sensor_itf = hw_rgbp->sensor_itf[instance];
			if (sensor_itf) {
				ret = sensor_itf->csi_itf_ops.get_vc_dma_buf_info(
					sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT3, &vc_buf_info);
				if (!ret)
					get_vc_dma_buf_by_fcount(
						sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT3,
						start_fcount, &index,
						&stat_buf[PABLO_CRTA_CSTAT_END_VPDAF].dva);
			}

			mlsc_hw_id = DEV_HW_MLSC0 + hw_ip->ch;
			mlsc_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, mlsc_hw_id);
			CALL_HWIP_OPS(mlsc_hw_ip, query, instance, PABLO_QUERY_GET_EDGE_SCORE, NULL,
				      &edge_score);
		}

		if (frame && frame->shot)
			copy_ctrl_to_dm(frame->shot);

		ret = CALL_ADT_MSG_OPS(hw_rgbp->icpu_adt, send_msg_cstat_frame_end, instance,
				       (void *)hw_ip, NULL, start_fcount, &shot_buf, stat_buf,
				       edge_score);
		if (ret)
			mserr_hw("icpu_adt send_msg_cstat_frame_end fail", instance, hw_ip);
	} else {
		CALL_HW_OPS(
			hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe))
			mserr_hw("[F%d]fs %d fe %d", instance, hw_ip, start_fcount,
				atomic_read(&hw_ip->count.fs), atomic_read(&hw_ip->count.fe));

		wake_up(&hw_ip->status.wait_queue);
	}
}

static inline void is_hw_rgbp_put_vc_buf(
	enum itf_vc_buf_data_type data_type, struct is_hw_ip *hw_ip, u32 instance, u32 fcount)
{
	struct pablo_hw_rgbp *hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	struct is_sensor_interface *sensor_itf = hw_rgbp->sensor_itf[instance];
	struct vc_buf_info_t vc_buf_info;
	int ret;

	if (!sensor_itf) {
		mswarn_hw("[F%d] sensor_interface is NULL", instance, hw_ip, fcount);
		return;
	}

	ret = sensor_itf->csi_itf_ops.get_vc_dma_buf_info(sensor_itf, data_type, &vc_buf_info);
	if (!ret)
		put_vc_dma_buf_by_fcount(sensor_itf, data_type, fcount);
}

static bool check_dma_done(struct is_hw_ip *hw_ip, u32 instance_id, u32 fcount)
{
	bool ret = false;
	struct is_frame *frame;
	struct is_framemgr *framemgr;
	struct is_hardware *hardware;
	int output_id = 0;
	u32 hw_fcount;
	ulong flags = 0;
	u32 queued_count;

	framemgr = hw_ip->framemgr;
	hw_fcount = atomic_read(&hw_ip->fcount);
	hardware = hw_ip->hardware;

flush_wait_done_frame:
	framemgr_e_barrier_common(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
	queued_count = framemgr->queued_count[FS_HW_WAIT_DONE];
	framemgr_x_barrier_common(framemgr, 0, flags);

	if (frame) {
		if (frame->type == SHOT_TYPE_LATE) {
			msinfo_hw("[F:%d,FF:%d,HWF:%d][WD%d] flush LATE_SHOT\n", instance_id, hw_ip,
				fcount, frame->fcount, hw_fcount, queued_count);

			ret = CALL_HW_OPS(hw_ip, frame_ndone, hw_ip, frame, IS_SHOT_LATE_FRAME);
			if (ret) {
				mserr_hw("hardware_frame_ndone fail(LATE_SHOT)", frame->instance,
					hw_ip);
				return true;
			}

			goto flush_wait_done_frame;
		} else {
			if (unlikely(frame->fcount < fcount)) {
				/* Flush the old frame which is in HW_WAIT_DONE state & retry. */
				mswarn_hw("[F:%d,FF:%d,HWF:%d][WD%d] invalid frame(idx:%d)",
					instance_id, hw_ip, fcount, frame->fcount, hw_fcount,
					queued_count, frame->cur_buf_index);

				framemgr_e_barrier_common(framemgr, 0, flags);
				frame_manager_print_info_queues(framemgr);
				framemgr_x_barrier_common(framemgr, 0, flags);

				ret = CALL_HW_OPS(hw_ip, frame_ndone, hw_ip, frame,
					IS_SHOT_INVALID_FRAMENUMBER);
				if (ret) {
					mserr_hw("hardware_frame_ndone fail(old frame)",
						frame->instance, hw_ip);
					return true;
				}

				goto flush_wait_done_frame;
			} else if (unlikely(frame->fcount > fcount)) {
				mswarn_hw("[F:%d,FF:%d,HWF:%d][WD%d] Too early frame. Skip it.",
					instance_id, hw_ip, fcount, frame->fcount, hw_fcount,
					queued_count);

				framemgr_e_barrier_common(framemgr, 0, flags);
				frame_manager_print_info_queues(framemgr);
				framemgr_x_barrier_common(framemgr, 0, flags);

				return true;
			}
		}
	} else {
flush_config_frame:
		/* Flush the old frame which is in HW_CONFIGURE state & skip dma_done. */
		framemgr_e_barrier_common(framemgr, 0, flags);
		frame = peek_frame(framemgr, FS_HW_CONFIGURE);
		if (frame) {
			if (unlikely(frame->fcount < hw_fcount)) {
				trans_frame(framemgr, frame, FS_HW_WAIT_DONE);
				framemgr_x_barrier_common(framemgr, 0, flags);

				mserr_hw("[F:%d,FF:%d,HWF:%d] late config frame", instance_id,
					hw_ip, fcount, frame->fcount, hw_fcount);

				CALL_HW_OPS(hw_ip, frame_ndone, hw_ip, frame,
					IS_SHOT_INVALID_FRAMENUMBER);
				goto flush_config_frame;
			} else if (frame->fcount == hw_fcount) {
				framemgr_x_barrier_common(framemgr, 0, flags);
				msinfo_hw("[F:%d,FF:%d,HWF:%d] right config frame", instance_id,
					hw_ip, fcount, frame->fcount, hw_fcount);
				return true;
			}
		}
		framemgr_x_barrier_common(framemgr, 0, flags);
		mserr_hw("[F:%d,HWF:%d]%s: frame(null)!!", instance_id, hw_ip, fcount, hw_fcount,
			__func__);
		return true;
	}

	/*
	 * fcount: This value should be same value that is notified by host at shot time.
	 * In case of FRO or batch mode, this value also should be same between start and end.
	 */
	msdbg_hw(1, "check_dma [ddk:%d,hw:%d] frame(F:%d,idx:%d,num_buffers:%d)\n", instance_id,
		hw_ip, fcount, hw_fcount, frame->fcount, frame->cur_buf_index, frame->num_buffers);

	if (test_bit(hw_ip->id, &frame->core_flag))
		output_id = IS_HW_CORE_END;

	CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, output_id, IS_SHOT_SUCCESS, true);
	return ret;
}

static int is_hw_rgbp_frame_end_callback(void *caller, void *ctx, void *rsp_msg)
{
	int ret;
	u32 instance, fcount, buf_idx;
	ulong flags = 0;
	struct is_hw_ip *hw_ip;
	struct pablo_hw_rgbp *hw_rgbp;
	struct is_hardware *hardware;
	struct pablo_icpu_adt_rsp_msg *msg;
	struct pablo_crta_buf_info buf_info;
	struct pablo_crta_bufmgr *bufmgr;
	struct is_framemgr *framemgr;
	struct is_frame *frame;

	if (!caller || !rsp_msg) {
		err_hw("invalid callback: caller(%p), msg(%p)", caller, rsp_msg);
		return -EINVAL;
	}

	hw_ip = (struct is_hw_ip *)caller;
	hw_rgbp = (struct pablo_hw_rgbp *)hw_ip->priv_info;
	hardware = hw_ip->hardware;
	msg = (struct pablo_icpu_adt_rsp_msg *)rsp_msg;
	instance = msg->instance;
	fcount = msg->fcount;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[F%d]Ignore end callback before HW config\n", instance, hw_ip, fcount);
		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[F%d]Ignore end callback invalid HW state(0x%lx)", instance, hw_ip,
			fcount, hw_ip->state);
		return 0;
	}

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, fcount, __func__);
	_is_hw_frame_dbg_ext_trace(hw_ip, fcount, DEBUG_POINT_FRAME_END, 0);

	if (msg->rsp)
		mserr_hw("frame_end fail from icpu: msg_ret(%d)", instance, hw_ip, msg->rsp);

	if (!hw_rgbp->param_set[instance].reprocessing) {
		for (buf_idx = PABLO_CRTA_CSTAT_END_PRE_THUMB;
		     buf_idx < PABLO_CRTA_CSTAT_END_CDAF_MW; buf_idx++) {
			bufmgr = pablo_get_crta_bufmgr(data_to_buf[buf_idx], instance, hw_ip->ch);
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, fcount, &buf_info);
			if (!ret)
				CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		}

		is_hw_rgbp_put_vc_buf(VC_BUF_DATA_TYPE_SENSOR_STAT3, hw_ip, instance, fcount);
	}

	framemgr = hw_ip->framemgr;
	framemgr_e_barrier_common(framemgr, 0, flags);
	frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
	framemgr_x_barrier_common(framemgr, 0, flags);

	if (frame && frame->result) {
		if (unlikely(frame->fcount > fcount)) {
			msdbg_hw(1, "frame_ndone [F%d][DDK:F%d] Too early frame. Skip it.",
				atomic_read(&hw_ip->instance), hw_ip, frame->fcount, fcount);

		} else {
			CALL_HW_OPS(hw_ip, frame_ndone, hw_ip, frame, frame->result);
		}
	} else {
		check_dma_done(hw_ip, instance, fcount);
	}

	clear_bit(HW_END, &hw_ip->state);

	wake_up(&hw_ip->status.wait_queue);

	return 0;
}

static void int0_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_rgbp *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT0] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	CALL_HW_RGBP_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();
}

static void int0_warn_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_rgbp *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT0] HW Warning!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	CALL_HW_RGBP_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();

	CALL_HW_RGBP_OPS(hw, clr_cotf_err, hw_ip->pmio);
}

static void int1_err_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct pablo_hw_rgbp *hw;
	struct pablo_common_ctrl *pcc;

	is_debug_lock();

	mserr_hw("[F%d][INT1] HW Error!! status 0x%08x", instance, hw_ip, fcount, status);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	CALL_HW_RGBP_OPS(hw, dump, hw_ip->pmio, HW_DUMP_DBG_STATE);
	is_debug_unlock();
}

static int pablo_hw_rgbp_check_int(struct is_hw_ip *hw_ip, u32 status)
{
	struct pablo_hw_rgbp *hw = GET_HW(hw_ip);
	u32 hw_fcount = atomic_read(&hw_ip->fcount);
	u32 instance = atomic_read(&hw_ip->instance);
	u32 fs, fr, fe;
	u32 int_status;

	fs = CALL_HW_RGBP_OPS(hw, is_occurred, status, BIT_MASK(INT_FRAME_START)) ?
		     BIT_MASK(INT_FRAME_START) :
		     0;
	fr = CALL_HW_RGBP_OPS(hw, is_occurred, status, BIT_MASK(INT_FRAME_ROW)) ?
		     BIT_MASK(INT_FRAME_ROW) :
		     0;
	fe = CALL_HW_RGBP_OPS(hw, is_occurred, status, BIT_MASK(INT_FRAME_END)) ?
		     BIT_MASK(INT_FRAME_END) :
		     0;

	if (fs && fe)
		mswarn_hw("[F%d] start/end overlapped!!", instance, hw_ip, hw_fcount);

	if ((fs && fr) || (fr && fe))
		mswarn_hw("[F%d] line overlapped!!", instance, hw_ip, hw_fcount);

	int_status = fs | fr | fe;

	msdbg_hw(4, "[F%d] int_status 0x%08x fs 0x%08x fr 0x%08x fe 0x%08x\n", instance, hw_ip,
		hw_fcount, int_status, fs, fr, fe);

	while (int_status) {
		switch (hw->event_state) {
		case RGBP_FS:
			if (fr) {
				is_hw_rgbp_frame_config_lock(hw_ip, instance, hw_fcount, status);
				hw->event_state = RGBP_FR;
				int_status &= ~fr;
				fr = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case RGBP_FR:
			if (fe) {
				hw->event_state = RGBP_FE;
				is_hw_rgbp_frame_end(hw_ip, instance, hw_fcount, status);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case RGBP_FE:
			if (fs) {
				hw->event_state = RGBP_FS;
				is_hw_rgbp_frame_start(hw_ip, instance, hw_fcount, status);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case RGBP_INIT:
			if (fs) {
				hw->event_state = RGBP_FS;
				is_hw_rgbp_frame_start(hw_ip, instance, hw_fcount, status);
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

static int pablo_hw_rgbp_handler_int(u32 id, void *ctx)
{
	struct is_hw_ip *hw_ip;
	struct pablo_hw_rgbp *hw;
	struct pablo_common_ctrl *pcc;
	u32 instance, hw_fcount, status;
	u32 int_status;

	hw_ip = (struct is_hw_ip *)ctx;
	instance = atomic_read(&hw_ip->instance);
	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("Invalid INT%d. hw_state 0x%lx", instance, hw_ip, id, hw_ip->state);
		wake_up(&hw_ip->status.wait_queue);
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
		int_status = pablo_hw_rgbp_check_int(hw_ip, status);
		if (int_status)
			mswarn_hw("[F%d] invalid interrupt: event_state(%ld), int_status(%x)",
				instance, hw_ip, hw_fcount, hw->event_state, int_status);

		if (CALL_HW_RGBP_OPS(hw, is_occurred, status, BIT_MASK(INT_SETTING_DONE)))
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

		if (CALL_HW_RGBP_OPS(hw, is_occurred, status, BIT_MASK(INT_ERR0))) {
			int0_err_handler(hw_ip, instance, hw_fcount, status);

			if (CALL_HW_OPS(hw_ip, clear_crc_status, instance, hw_ip) > 0)
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
		}

		if (CALL_HW_RGBP_OPS(hw, is_occurred, status, BIT_MASK(INT_WARN0)))
			int0_warn_handler(hw_ip, instance, hw_fcount, status);

	} else {
		if (CALL_HW_RGBP_OPS(hw, is_occurred, status, BIT_MASK(INT_ERR1))) {
			int1_err_handler(hw_ip, instance, hw_fcount, status);

			if (CALL_HW_OPS(hw_ip, clear_crc_status, instance, hw_ip) > 0)
				set_bit(IS_SENSOR_ESD_RECOVERY,
					&hw_ip->group[instance]->device->sensor->state);
		}
	}

	if (unlikely(dbg_info.en[RGBP_DBG_S_TRIGGER]))
		_dbg_handler(hw_ip, id, status);

	return 0;
}

static struct pablo_mmio *_pablo_hw_rgbp_pmio_init(struct is_hw_ip *hw_ip)
{
	int ret;
	struct pablo_mmio *pmio;
	struct pmio_config *pcfg;

	pcfg = &hw_ip->pmio_config;

	pcfg->name = "RGBP";
	pcfg->mmio_base = hw_ip->mmio_base;
	pcfg->cache_type = PMIO_CACHE_NONE;
	pcfg->phys_base = hw_ip->regs_start[REG_SETA];

	rgbp_hw_g_pmio_cfg(pcfg);

	pmio = pmio_init(NULL, NULL, pcfg);
	if (IS_ERR(pmio))
		goto err_init;

	ret = pmio_field_bulk_alloc(pmio, &hw_ip->pmio_fields,
				pcfg->fields, pcfg->num_fields);
	if (ret) {
		serr_hw("Failed to alloc RGBP PMIO field_bulk. ret %d", hw_ip, ret);
		goto err_field_bulk_alloc;
	}

	return pmio;

err_field_bulk_alloc:
	pmio_exit(pmio);
	pmio = ERR_PTR(ret);

err_init:
	return pmio;
}

static void _pablo_hw_rgbp_pmio_deinit(struct is_hw_ip *hw_ip)
{
	struct pablo_mmio *pmio = hw_ip->pmio;

	pmio_field_bulk_free(pmio, hw_ip->pmio_fields);
	pmio_exit(pmio);
}

int pablo_hw_rgbp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	struct pablo_mmio *pmio;
	int hw_slot;

	hw_ip->ops  = &pablo_hw_rgbp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &pablo_hw_rgbp_handler_int;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &pablo_hw_rgbp_handler_int;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	pmio = _pablo_hw_rgbp_pmio_init(hw_ip);
	if (IS_ERR(pmio)) {
		serr_hw("Failed to rgbp pmio_init.", hw_ip);
		return -EINVAL;
	}

	hw_ip->pmio = pmio;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_hw_rgbp_probe);

void pablo_hw_rgbp_remove(struct is_hw_ip *hw_ip)
{
	struct is_interface_ischain *itfc = hw_ip->itfc;
	int id = hw_ip->id;
	int hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = false;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = false;

	_pablo_hw_rgbp_pmio_deinit(hw_ip);
}
EXPORT_SYMBOL_GPL(pablo_hw_rgbp_remove);
