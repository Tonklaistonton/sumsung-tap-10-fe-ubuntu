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

#include "pablo-hw-helper.h"
#include "is-err.h"
#include "is-stripe.h"
#include "pablo-hw-msnr.h"
#include "api/is-hw-api-msnr-v13.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

#define GET_HW(hw_ip) ((struct is_hw_msnr *)hw_ip->priv_info)

static inline void *get_base(struct is_hw_ip *hw_ip)
{
	return hw_ip->pmio;
}

static int param_debug_msnr_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] bit value, MSNR debug features\n"
				"\tb[0] : dump sfr\n"
				"\tb[1] : dump sfr once\n"
				"\tb[2] : s2d\n"
				"\tb[3] : skip ddk\n"
				"\tb[4] : bypass\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static struct pablo_debug_param debug_msnr = {
	.type = IS_DEBUG_PARAM_TYPE_BIT,
	.max_value = 0x1F,
	.ops.usage = param_debug_msnr_usage,
};

module_param_cb(debug_msnr, &pablo_debug_param_ops, &debug_msnr, 0644);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
const struct kernel_param *is_hw_msnr_get_debug_kernel_param_kunit_wrapper(void)
{
	return G_KERNEL_PARAM(debug_msnr);
}
KUNIT_EXPORT_SYMBOL(is_hw_msnr_get_debug_kernel_param_kunit_wrapper);
#endif

void is_hw_msnr_s_debug_type(int type)
{
	set_bit(type, &debug_msnr.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_msnr_s_debug_type);

bool is_hw_msnr_check_debug_type(int type)
{
	return test_bit(type, &debug_msnr.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_msnr_check_debug_type);

void is_hw_msnr_c_debug_type(int type)
{
	clear_bit(type, &debug_msnr.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_msnr_c_debug_type);

static int is_hw_msnr_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_msnr *hw_msnr;
	struct pablo_common_ctrl *pcc;
	u32 status, instance, hw_fcount, hl = 0, vl = 0;
	u32 f_err;

	hw_ip = (struct is_hw_ip *)context;

	hw_msnr = GET_HW(hw_ip);
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt, hw_ip state(0x%lx)", instance, hw_ip, hw_ip->state);
		return 0;
	}

	pcc = hw_msnr->pcc;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	hw_msnr->irq_state[id] = status = CALL_PCC_OPS(pcc, get_int_status, pcc, id, true);

	msdbg_hw(2, "MSNR interrupt%d status(0x%x)\n", instance, hw_ip, id, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt%d: 0x%x", instance, hw_ip, id, status);
		goto exit;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("During recovery : invalid interrupt%d", instance, hw_ip, id);
		goto exit;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt%d(0x%x)", instance, hw_ip, id, status);
		goto exit;
	}

	if (msnr_hw_is_occurred(status, INTR_SETTING_DONE))
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (msnr_hw_is_occurred(status, INTR_FRAME_START) &&
		msnr_hw_is_occurred(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (msnr_hw_is_occurred(status, INTR_FRAME_START)) {
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fs);
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
	}

	if (msnr_hw_is_occurred(status, INTR_FRAME_END)) {
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

		CALL_HW_OPS(
			hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

		atomic_set(&hw_ip->status.Vvalid, V_BLANK);
		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
			mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)", instance, hw_ip,
				atomic_read(&hw_ip->count.fs), atomic_read(&hw_ip->count.fe),
				atomic_read(&hw_ip->count.dma), status);
		}
		wake_up(&hw_ip->status.wait_queue);

		if (unlikely(is_hw_msnr_check_debug_type(MSNR_DBG_S2D)))
			is_debug_s2d(true, "MSNR_DBG_S2D");
	}

	if (id == 0)
		f_err = msnr_hw_is_occurred(status, INTR_ERR);
	else
		f_err = msnr_hw_is_occurred1(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt%d (%d,%d) status(0x%x)\n", instance,
			hw_ip, hw_fcount, id, hl, vl, status);

		if (id == 0) {
			msnr_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
			msnr_hw_dump(hw_ip->pmio, HW_DUMP_CR);
		}
	}

exit:
	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	return 0;
}

static int is_hw_msnr_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_msnr *hw;
	struct pablo_common_ctrl *pcc;
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_ip->cur_hw_iq_set[i].size = 0;

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	return CALL_PCC_OPS(pcc, reset, pcc);
}

static int is_hw_msnr_wait_idle(struct is_hw_ip *hw_ip, u32 instance)
{
	return msnr_hw_wait_idle(get_base(hw_ip));
}

static int __is_hw_msnr_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
{
	u32 seed;

	if (!hw_ip) {
		err("hw_ip is NULL");
		return -ENODEV;
	}

	msinfo_hw("reset\n", instance, hw_ip);
	if (CALL_HWIP_OPS(hw_ip, reset, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		msnr_hw_s_crc(get_base(hw_ip), seed);

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_msnr_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res;

	if (CALL_HWIP_OPS(hw_ip, reset, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	res = CALL_HWIP_OPS(hw_ip, wait_idle, instance);
	if (res)
		mserr_hw("failed to msnr_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished msnr\n", instance, hw_ip);

	return res;
}

static int __nocfi is_hw_msnr_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_msnr *hw_msnr;
	struct is_mem *mem;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, "HWMSNR");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME, false);

	hw_ip->priv_info = pablo_zalloc(sizeof(struct is_hw_msnr), GFP_KERNEL);
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_msnr = GET_HW(hw_ip);
	hw_msnr->instance = instance;
	hw_msnr->pcc = pablo_common_ctrl_hw_get_pcc(&hw_ip->pmio_config);

	ret = CALL_PCC_HW_OPS(
		hw_msnr->pcc, init, hw_msnr->pcc, hw_ip->pmio, hw_ip->name, PCC_M2M, NULL);
	if (ret) {
		mserr_hw("failed to pcc init. ret %d", instance, hw_ip, ret);
		goto err_pcc_init;
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_MSNR);

	hw_msnr->pb_c_loader_payload = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x8000, NULL, 0);
	if (IS_ERR_OR_NULL(hw_msnr->pb_c_loader_payload)) {
		hw_msnr->pb_c_loader_payload = NULL;
		err("failed to allocate buffer for c-loader payload");
		ret = -ENOMEM;
		goto err_chain_create;
	}
	hw_msnr->kva_c_loader_payload =
		CALL_BUFOP(hw_msnr->pb_c_loader_payload, kvaddr, hw_msnr->pb_c_loader_payload);
	hw_msnr->dva_c_loader_payload =
		CALL_BUFOP(hw_msnr->pb_c_loader_payload, dvaddr, hw_msnr->pb_c_loader_payload);

	hw_msnr->pb_c_loader_header = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x2000, NULL, 0);
	if (IS_ERR_OR_NULL(hw_msnr->pb_c_loader_header)) {
		hw_msnr->pb_c_loader_header = NULL;
		err("failed to allocate buffer for c-loader header");
		ret = -ENOMEM;
		goto err_chain_create;
	}
	hw_msnr->kva_c_loader_header =
		CALL_BUFOP(hw_msnr->pb_c_loader_header, kvaddr, hw_msnr->pb_c_loader_header);
	hw_msnr->dva_c_loader_header =
		CALL_BUFOP(hw_msnr->pb_c_loader_header, dvaddr, hw_msnr->pb_c_loader_header);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_chain_create:
	CALL_PCC_HW_OPS(hw_msnr->pcc, deinit, hw_msnr->pcc);
err_pcc_init:
	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_msnr_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	int ret;
	struct is_hw_msnr *hw_msnr;
	u32 input_id;

	hw_msnr = GET_HW(hw_ip);
	if (!hw_msnr) {
		mserr_hw("hw_msnr is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	for (input_id = MSNR_WDMA_LME; input_id < MSNR_WDMA_MAX; input_id++) {
		ret = msnr_hw_wdma_create(&hw_msnr->wdma[input_id], get_base(hw_ip), input_id);
		if (ret) {
			mserr_hw("msnr_hw_wdma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;
}

static int is_hw_msnr_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static int is_hw_msnr_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_msnr *hw_msnr = NULL;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_msnr = GET_HW(hw_ip);

	__is_hw_msnr_clear_common(hw_ip, instance);

	CALL_BUFOP(hw_msnr->pb_c_loader_payload, free, hw_msnr->pb_c_loader_payload);
	CALL_BUFOP(hw_msnr->pb_c_loader_header, free, hw_msnr->pb_c_loader_header);

	CALL_PCC_HW_OPS(hw_msnr->pcc, deinit, hw_msnr->pcc);

	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_msnr_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_mmio *pmio;
	struct is_hw_msnr *hw_msnr;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_cfg cfg = {
		0,
	};

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "enable: hw_map is not set!\n", instance, hw_ip);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_msnr = GET_HW(hw_ip);
	pcc = hw_msnr->pcc;
	pmio = hw_ip->pmio;

	hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
	if (pmio_reinit_cache(pmio, &hw_ip->pmio_config)) {
		pmio_cache_set_bypass(pmio, true);
		err("failed to reinit PMIO cache, set bypass");
		return -EINVAL;
	}
	__is_hw_msnr_s_common_reg(hw_ip, instance);

	cfg.fs_mode = PCC_ASAP;
	msnr_hw_g_int_en(cfg.int_en);
	CALL_PCC_OPS(pcc, enable, pcc, &cfg);

	pmio_cache_set_only(pmio, true);

	memset(&hw_msnr->config[instance], 0x0, sizeof(struct is_msnr_config));
	memset(&hw_msnr->param_set[instance], 0x0, sizeof(struct msnr_param_set));

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_msnr_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_msnr *hw_msnr;
	struct pablo_common_ctrl *pcc;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw(
		"msnr_disable: Vvalid(%d)\n", instance, hw_ip, atomic_read(&hw_ip->status.Vvalid));

	hw_msnr = GET_HW(hw_ip);

	timetowait = wait_event_timeout(
		hw_ip->status.wait_queue, !atomic_read(&hw_ip->status.Vvalid), IS_HW_STOP_TIMEOUT);

	if (!timetowait)
		mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip, timetowait);

	if (hw_ip->run_rsc_state)
		return 0;

	pcc = hw_msnr->pcc;
	CALL_PCC_OPS(pcc, disable, pcc);

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_msnr_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_msnr *hw_msnr,
	struct msnr_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	pdma_addr_t *output_dva;
	u32 comp_sbwc_en = 0, payload_size = 0;
	u32 strip_offset = 0, header_offset = 0;
	struct param_dma_output *dma_output;
	u32 frame_width, frame_height;
	int ret;

	frame_width = param_set->wdma_lme.width;
	frame_height = param_set->wdma_lme.height;

	switch (id) {
	case MSNR_WDMA_LME:
		output_dva = param_set->output_dva_lme;
		dma_output = &param_set->wdma_lme;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_msnr->wdma[id].name, dma_output->cmd);

	msnr_hw_s_dma_corex_id(&hw_msnr->wdma[id], set_id);

	ret = msnr_hw_s_wdma_init(&hw_msnr->wdma[id], dma_output, &param_set->stripe_input,
		frame_width, frame_height, &comp_sbwc_en, &payload_size, &strip_offset,
		&header_offset, &hw_msnr->config[instance]);
	if (ret) {
		mserr_hw("failed to initialize MSNR_WDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (dma_output->cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = msnr_hw_s_wdma_addr(&hw_msnr->wdma[id], output_dva, 0, hw_ip->num_buffers, 0,
			comp_sbwc_en, payload_size, strip_offset, header_offset);
		if (ret) {
			mserr_hw("failed to set MSNR_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_msnr_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	msnr_hw_s_block_bypass(get_base(hw_ip), set_id);

	return 0;
}

static int __is_hw_msnr_update_block_reg(
	struct is_hw_ip *hw_ip, struct msnr_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_msnr *hw_msnr;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_msnr = GET_HW(hw_ip);

	if (hw_msnr->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n", instance, hw_ip,
			hw_msnr->instance);
		hw_msnr->instance = instance;
	}

	param_set->wdma_lme.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	__is_hw_msnr_bypass(hw_ip, set_id);

	return 0;
}

static void __is_hw_msnr_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
	struct msnr_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct msnr_param *param;

	param = &p_region->msnr;
	param_set->instance_id = instance;

	param_set->mono_mode = hw_ip->region[instance]->parameter.sensor.config.mono_mode;

	if (test_bit(PARAM_MSNR_CIN_L0, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_CIN_L0\n", instance, hw_ip);
		memcpy(&param_set->cin_msnr_l0, &param->cin_msnr_l0,
			sizeof(struct param_otf_input));
	}

	if (test_bit(PARAM_MSNR_CIN_L1, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_CIN_L1\n", instance, hw_ip);
		memcpy(&param_set->cin_msnr_l1, &param->cin_msnr_l1,
			sizeof(struct param_otf_input));
	}

	if (test_bit(PARAM_MSNR_CIN_L2, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_CIN_L2\n", instance, hw_ip);
		memcpy(&param_set->cin_msnr_l2, &param->cin_msnr_l2,
			sizeof(struct param_otf_input));
	}

	if (test_bit(PARAM_MSNR_CIN_L3, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_CIN_L3\n", instance, hw_ip);
		memcpy(&param_set->cin_msnr_l3, &param->cin_msnr_l3,
			sizeof(struct param_otf_input));
	}

	if (test_bit(PARAM_MSNR_CIN_L4, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_CIN_L4\n", instance, hw_ip);
		memcpy(&param_set->cin_msnr_l4, &param->cin_msnr_l4,
			sizeof(struct param_otf_input));
	}

	if (test_bit(PARAM_MSNR_COUT_YUV, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_COUT_YUV\n", instance, hw_ip);
		memcpy(&param_set->cout_msnr_yuv, &param->cout_msnr_yuv,
			sizeof(struct param_otf_output));
	}

	if (test_bit(PARAM_MSNR_COUT_STAT, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_COUT_STAT\n", instance, hw_ip);
		memcpy(&param_set->cout_msnr_stat, &param->cout_msnr_stat,
			sizeof(struct param_otf_output));
	}

	if (test_bit(PARAM_MSNR_WDMA_LME, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_WDMA_LME\n", instance, hw_ip);
		memcpy(&param_set->wdma_lme, &param->wdma_lme, sizeof(struct param_dma_output));
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && test_bit(PARAM_MSNR_STRIPE_INPUT, pmap)) {
		msdbg_hw(2, "PARAM_MSNR_STRIPE_INPUT\n", instance, hw_ip);
		memcpy(&param_set->stripe_input, &param->stripe_input,
			sizeof(struct param_stripe_input));
	}
}

static int is_hw_msnr_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_msnr *hw_msnr;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_msnr = GET_HW(hw_ip);
	hw_msnr->instance = IS_STREAM_COUNT;

	return 0;
}

static int __is_hw_msnr_set_size_regs(struct is_hw_ip *hw_ip, struct msnr_param_set *param_set,
	u32 instance, const struct is_frame *frame, u32 set_id)

{
	struct is_hw_msnr *hw_msnr;
	struct is_msnr_config *msnr_config;
	struct {
		struct is_rectangle l0;
		struct is_rectangle l1;
		struct is_rectangle l2;
		struct is_rectangle l3;
		struct is_rectangle l4;
	} cin_size = {
		0,
	};
	struct {
		u32 enable;
		u32 region_id;
		enum msnr_strip_type type;
		u32 frame_width;
		u32 start_pos;
	} strip_info = {
		0,
	};
	struct msnr_radial_cfg radial_cfg;
	struct {
		u32 yuv_bypass;
		u32 hf_bypass;
		struct is_crop yuv;
		struct is_crop hf;
	} crop_info = {
		0,
	};
	struct is_multi_layer strip_start_pos = { 0, };
	struct is_multi_layer strip_frame_width = { 0, };

	hw_msnr = GET_HW(hw_ip);
	msnr_config = &hw_msnr->config[instance];

	strip_info.enable = (param_set->stripe_input.total_count < 2) ? 0 : 1;
	strip_info.region_id = param_set->stripe_input.index;
	if (strip_info.enable) {
		if (strip_info.region_id == 0)
			strip_info.type = MSNR_STRIP_FIRST;
		else if (strip_info.region_id ==
			 param_set->stripe_input.total_count - 1) /* TODO: check count */
			strip_info.type = MSNR_STRIP_LAST;
		else
			strip_info.type = MSNR_STRIP_MID;
	} else {
		strip_info.type = MSNR_STRIP_NONE;
	}

	cin_size.l0.w = param_set->cin_msnr_l0.width;
	cin_size.l0.h = param_set->cin_msnr_l0.height;
	cin_size.l1.w = param_set->cin_msnr_l1.width;
	cin_size.l1.h = param_set->cin_msnr_l1.height;
	cin_size.l2.w = param_set->cin_msnr_l2.width;
	cin_size.l2.h = param_set->cin_msnr_l2.height;
	cin_size.l3.w = param_set->cin_msnr_l3.width;
	cin_size.l3.h = param_set->cin_msnr_l3.height;
	cin_size.l4.w = param_set->cin_msnr_l4.width;
	cin_size.l4.h = param_set->cin_msnr_l4.height;

	strip_info.frame_width =
		(strip_info.enable) ? param_set->stripe_input.full_width : cin_size.l0.w;
	strip_frame_width.l1 =
		(strip_info.enable) ? ALIGN(param_set->stripe_input.full_width / 2, 2) : cin_size.l1.w;
	strip_frame_width.l2 = (strip_info.enable) ? ALIGN(strip_frame_width.l1 / 2, 2) : cin_size.l2.w;
	strip_frame_width.l3 = (strip_info.enable) ? ALIGN(strip_frame_width.l2 / 2, 2) : cin_size.l3.w;
	strip_frame_width.l4 = (strip_info.enable) ? ALIGN(strip_frame_width.l3 / 2, 2) : cin_size.l4.w;

	strip_info.start_pos = (strip_info.region_id) ? (param_set->stripe_input.start_pos_x) : 0;
	strip_start_pos.l1 = (strip_info.region_id) ? ALIGN(strip_info.start_pos / 2, 2) : 0;
	strip_start_pos.l2 = (strip_info.region_id) ? ALIGN(strip_start_pos.l1 / 2, 2) : 0;
	strip_start_pos.l3 = (strip_info.region_id) ? ALIGN(strip_start_pos.l2 / 2, 2) : 0;
	strip_start_pos.l4 = (strip_info.region_id) ? ALIGN(strip_start_pos.l3 / 2, 2) : 0;

	radial_cfg.sensor_full_width = frame->shot->udm.frame_info.sensor_size[0];
	radial_cfg.sensor_full_height = frame->shot->udm.frame_info.sensor_size[1];
	radial_cfg.sensor_crop_x = frame->shot->udm.frame_info.sensor_crop_region[0];
	radial_cfg.sensor_crop_y = frame->shot->udm.frame_info.sensor_crop_region[1];
	radial_cfg.rgbp_crop_offset_x = frame->shot->udm.frame_info.taa_in_crop_region[0];
	radial_cfg.rgbp_crop_offset_y = frame->shot->udm.frame_info.taa_in_crop_region[1];
	radial_cfg.rgbp_crop_w = frame->shot->udm.frame_info.taa_in_crop_region[2];
	radial_cfg.rgbp_crop_h = frame->shot->udm.frame_info.taa_in_crop_region[3];
	radial_cfg.sensor_binning_x = frame->shot->udm.frame_info.sensor_binning[0];
	radial_cfg.sensor_binning_y = frame->shot->udm.frame_info.sensor_binning[1];
	radial_cfg.bns_binning_x = frame->shot->udm.frame_info.bns_binning[0];
	radial_cfg.bns_binning_y = frame->shot->udm.frame_info.bns_binning[1];
	if (frame->shot_ext) {
		radial_cfg.sw_binning_x =
			frame->shot_ext->binning_ratio_x ? frame->shot_ext->binning_ratio_x : 1000;
		radial_cfg.sw_binning_y =
			frame->shot_ext->binning_ratio_y ? frame->shot_ext->binning_ratio_y : 1000;
	} else {
		radial_cfg.sw_binning_x = 1000;
		radial_cfg.sw_binning_y = 1000;
	}

	msnr_hw_s_chain_img_size_l0(get_base(hw_ip), cin_size.l0.w, cin_size.l0.h);
	msnr_hw_s_chain_img_size_l1(get_base(hw_ip), cin_size.l1.w, cin_size.l1.h);
	msnr_hw_s_chain_img_size_l2(get_base(hw_ip), cin_size.l2.w, cin_size.l2.h);
	msnr_hw_s_chain_img_size_l3(get_base(hw_ip), cin_size.l3.w, cin_size.l3.h);
	msnr_hw_s_chain_img_size_l4(get_base(hw_ip), cin_size.l4.w, cin_size.l4.h);

	/* TODO: set crop info */
	crop_info.yuv_bypass = 1;

	if (crop_info.yuv_bypass) {
		crop_info.yuv.x = 0;
		crop_info.yuv.y = 0;
		crop_info.yuv.w = cin_size.l0.w;
		crop_info.yuv.h = cin_size.l0.h;
	}
	msnr_hw_s_crop_yuv(get_base(hw_ip), crop_info.yuv_bypass, crop_info.yuv);

	crop_info.hf_bypass = 1;

	if (crop_info.hf_bypass) {
		crop_info.hf.x = 0;
		crop_info.hf.y = 0;
		crop_info.hf.w = cin_size.l0.w;
		crop_info.hf.h = cin_size.l0.h;
	}
	msnr_hw_s_crop_hf(get_base(hw_ip), crop_info.hf_bypass, crop_info.hf);

	msnr_hw_s_strip_size(
		get_base(hw_ip), strip_info.type, strip_info.start_pos, strip_info.frame_width);

	msnr_hw_s_radial_l0(get_base(hw_ip), set_id, strip_info.frame_width, cin_size.l0.h,
		strip_info.enable, strip_info.start_pos, &radial_cfg, msnr_config);
	msnr_hw_s_radial_l1(get_base(hw_ip), set_id, strip_frame_width.l1, cin_size.l1.h,
		strip_info.enable, strip_start_pos.l1, &radial_cfg, msnr_config);
	msnr_hw_s_radial_l2(get_base(hw_ip), set_id, strip_frame_width.l2, cin_size.l2.h,
		strip_info.enable, strip_start_pos.l2, &radial_cfg, msnr_config);
	msnr_hw_s_radial_l3(get_base(hw_ip), set_id, strip_frame_width.l3, cin_size.l3.h,
		strip_info.enable, strip_start_pos.l3, &radial_cfg, msnr_config);
	msnr_hw_s_radial_l4(get_base(hw_ip), set_id, strip_frame_width.l4, cin_size.l4.h,
		strip_info.enable, strip_start_pos.l4, &radial_cfg, msnr_config);

	return 0;
}

static void __is_hw_msnr_config_path(
	struct is_hw_ip *hw_ip, u32 instance, struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct is_hw_msnr *hw_msnr;
	struct is_msnr_config *msnr_config;
	u32 dlnr_enable;

	hw_msnr = GET_HW(hw_ip);
	msnr_config = &hw_msnr->config[instance];

	dlnr_enable = msnr_config->decomp_en;

	msnr_hw_s_otf_path(get_base(hw_ip), 1, frame_cfg);

	msnr_hw_s_otf_dlnr_path(get_base(hw_ip), dlnr_enable, frame_cfg);
}

static inline u32 __get_yuvp_contents_aware_en(struct is_hw_ip *hw_ip, struct is_frame *frame)
{
	u32 en = 0;
	void *addr = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_config, DEV_HW_YPP, frame);
	struct is_yuvp_config *cfg;

	if (addr) {
		cfg = (struct is_yuvp_config *)(addr + sizeof(u32));
		en = cfg->drc_contents_aware_isp_en ||
			cfg->ccm_contents_aware_isp_en ||
			cfg->pcc_contents_aware_isp_en ||
			cfg->sharpen_contents_aware_isp_en;
	}

	return en;
}

static void __is_hw_msnr_config_mux(struct is_hw_ip *hw_ip, u32 instance,
		struct is_frame *frame, struct msnr_param_set *param_set, u32 set_id)
{
	struct is_hw_msnr *hw_msnr;
	struct is_msnr_config *msnr_config;
	u32 dlnr_en;
	u32 cmd_lme;
	u32 lmeds_select;
	u32 lmeds_bypass;
	u32 lmeds_w;
	u32 lmeds_h;
	u32 segconf_en;
	u32 decomp_lowpower;

	hw_msnr = GET_HW(hw_ip);
	msnr_config = &hw_msnr->config[instance];

	/* DLNR mode */
	dlnr_en = msnr_config->decomp_en;
	decomp_lowpower = msnr_config->decomp_low_power_en;

	msnr_hw_s_dlnr_enable(get_base(hw_ip), dlnr_en);
	msnr_hw_s_decomp_lowpower(get_base(hw_ip), decomp_lowpower);

	/* DSLME input enable */
	cmd_lme = param_set->wdma_lme.cmd;
	msnr_hw_s_dslme_input_enable(get_base(hw_ip), cmd_lme);

	/* DSLME input select */
	lmeds_select = msnr_config->lmeds_input_select;
	lmeds_bypass = msnr_config->lmeds_bypass;
	lmeds_w = msnr_config->lmeds_w;
	lmeds_h = msnr_config->lmeds_h;
	msnr_hw_s_dslme_input_select(get_base(hw_ip), lmeds_select);
	msnr_hw_s_dslme_config(get_base(hw_ip), lmeds_bypass, lmeds_w, lmeds_h);

	/* SEGCONF output enable */
	segconf_en = __get_yuvp_contents_aware_en(hw_ip, frame);
	msnr_hw_s_segconf_output_enable(get_base(hw_ip), segconf_en);
}

static void _is_hw_msnr_s_cmd(struct is_hw_msnr *hw, struct c_loader_buffer *clb, u32 fcount,
	struct pablo_common_ctrl_cmd *cmd)
{
	cmd->set_mode = PCC_APB_DIRECT;
	cmd->fcount = fcount;
	cmd->int_grp_en = msnr_hw_g_int_grp_en();

	if (!clb)
		return;

	cmd->base_addr = clb->header_dva;
	cmd->header_num = clb->num_of_headers;
	cmd->set_mode = PCC_DMA_DIRECT;
}

static int is_hw_msnr_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct is_hw_msnr *hw_msnr;
	struct is_param_region *param_region;
	struct msnr_param_set *param_set;
	u32 fcount, instance;
	u32 cur_idx;
	u32 set_id;
	int ret, i;
	u32 cmd_lme_out;
	u32 strip_index, strip_total_count, strip_repeat_num, strip_repeat_idx;
	struct is_msnr_config *config;
	bool do_blk_cfg;
	bool skip = false;
	struct c_loader_buffer clb;
	struct pablo_common_ctrl_frame_cfg frame_cfg = {
		0,
	};
	struct pablo_common_ctrl *pcc;

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot(%d)\n", instance, hw_ip, frame->fcount, frame->cur_buf_index);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (!hw_ip->hardware) {
		mserr_hw("failed to get hardware", instance, hw_ip);
		return -EINVAL;
	}

	hw_msnr = GET_HW(hw_ip);
	pcc = hw_msnr->pcc;
	param_set = &hw_msnr->param_set[instance];
	param_region = frame->parameter;

	fcount = frame->fcount;

	if (hw_ip->internal_fcount[instance] != 0)
		hw_ip->internal_fcount[instance] = 0;

	if (frame->shot_ext) {
		if ((param_set->tnr_mode != frame->shot_ext->tnr_mode) &&
			!CHK_VIDEOHDR_MODE_CHANGE(param_set->tnr_mode, frame->shot_ext->tnr_mode))
			msinfo_hw("[F:%d] TNR mode is changed (%d -> %d)\n", instance, hw_ip,
				frame->fcount, param_set->tnr_mode, frame->shot_ext->tnr_mode);
		param_set->tnr_mode = frame->shot_ext->tnr_mode;
	} else {
		mswarn_hw("frame->shot_ext is null", instance, hw_ip);
		param_set->tnr_mode = TNR_PROCESSING_PREVIEW_POST_ON;
	}

	__is_hw_msnr_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	cmd_lme_out = CALL_HW_OPS(hw_ip, dma_cfg, "lme_out", hw_ip, frame, cur_idx,
		frame->num_buffers, &param_set->wdma_lme.cmd, param_set->wdma_lme.plane,
		param_set->output_dva_lme, frame->dva_msnr_cap_lme);

	param_set->instance_id = instance;
	param_set->fcount = fcount;

	/* FRO */
	hw_ip->num_buffers = frame->num_buffers;
	frame_cfg.num_buffers = frame->num_buffers;

	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;
	strip_repeat_num = param_set->stripe_input.repeat_num;
	strip_repeat_idx = param_set->stripe_input.repeat_idx;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (hw_msnr->repeat_instance != instance)
			hw_msnr->repeat_state = 0;

		if (frame_cfg.num_buffers > 1 || strip_total_count > 1 || strip_repeat_num > 1)
			hw_msnr->repeat_state++;
		else
			hw_msnr->repeat_state = 0;

		if (hw_msnr->repeat_state > 1 &&
			(!pablo_is_first_shot(frame_cfg.num_buffers, cur_idx) ||
				!pablo_is_first_shot(strip_total_count, strip_index) ||
				!pablo_is_first_shot(strip_repeat_num, strip_repeat_idx)))
			skip = true;

		hw_msnr->repeat_instance = instance;
	}

	msdbgs_hw(2,
		"[F:%d] repeat_state(%d), batch(%d, %d), strip(%d, %d), repeat(%d, %d), skip(%d)",
		instance, hw_ip, frame->fcount, hw_msnr->repeat_state, frame_cfg.num_buffers,
		cur_idx, strip_total_count, strip_index, strip_repeat_num, strip_repeat_idx, skip);

	/* temp direct set */
	set_id = COREX_DIRECT;

	/* reset CLD buffer */
	clb.num_of_headers = 0;
	clb.num_of_values = 0;
	clb.num_of_pairs = 0;

	clb.header_dva = hw_msnr->dva_c_loader_header;
	clb.payload_dva = hw_msnr->dva_c_loader_payload;

	clb.clh = (struct c_loader_header *)hw_msnr->kva_c_loader_header;
	clb.clp = (struct c_loader_payload *)hw_msnr->kva_c_loader_payload;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	msnr_hw_s_core(get_base(hw_ip), set_id);

	config = &hw_msnr->config[instance];

	do_blk_cfg = CALL_HW_HELPER_OPS(
		hw_ip, set_rta_regs, instance, set_id, skip, frame, (void *)&clb);
	if (unlikely(do_blk_cfg))
		__is_hw_msnr_update_block_reg(hw_ip, param_set, instance, set_id);

	__is_hw_msnr_config_path(hw_ip, instance, &frame_cfg);

	__is_hw_msnr_config_mux(hw_ip, instance, frame, param_set, set_id);

	ret = __is_hw_msnr_set_size_regs(hw_ip, param_set, instance, frame, set_id);
	if (ret) {
		mserr_hw("__is_hw_msnr_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	for (i = MSNR_WDMA_LME; i < MSNR_WDMA_MAX; i++) {
		ret = __is_hw_msnr_set_wdma(hw_ip, hw_msnr, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_msnr_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	if (param_region->msnr.control.strgen == CONTROL_COMMAND_START) {
		msdbg_hw(2, "STRGEN input\n", instance, hw_ip);
		msnr_hw_s_strgen(get_base(hw_ip), set_id);
	}

	pmio_cache_fsync(hw_ip->pmio, (void *)&clb, PMIO_FORMATTER_PAIR);

	if (clb.num_of_pairs > 0)
		clb.num_of_headers++;

	CALL_BUFOP(hw_msnr->pb_c_loader_payload, sync_for_device, hw_msnr->pb_c_loader_payload, 0,
		hw_msnr->pb_c_loader_payload->size, DMA_TO_DEVICE);
	CALL_BUFOP(hw_msnr->pb_c_loader_header, sync_for_device, hw_msnr->pb_c_loader_header, 0,
		hw_msnr->pb_c_loader_header->size, DMA_TO_DEVICE);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);

	_is_hw_msnr_s_cmd(hw_msnr, &clb, fcount, &frame_cfg.cmd);

	CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);

	if (unlikely(is_hw_msnr_check_debug_type(MSNR_DBG_DUMP_REG) ||
		     is_hw_msnr_check_debug_type(MSNR_DBG_DUMP_REG_ONCE))) {
		msnr_hw_dump(hw_ip->pmio, HW_DUMP_CR);

		if (is_hw_msnr_check_debug_type(MSNR_DBG_DUMP_REG_ONCE))
			is_hw_msnr_c_debug_type(MSNR_DBG_DUMP_REG_ONCE);
	}

	set_bit(HW_CONFIG, &hw_ip->state);

shot_fail_recovery:
	CALL_PCC_OPS(pcc, set_qch, pcc, false);

	return ret;
}

static int is_hw_msnr_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	return 0;
}

static int is_hw_msnr_get_cap_meta(
	struct is_hw_ip *hw_ip, ulong hw_map, u32 instance, u32 fcount, u32 size, ulong addr)
{
	return 0;
}

static int is_hw_msnr_frame_ndone(
	struct is_hw_ip *hw_ip, struct is_frame *frame, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1, output_id, done_type, false);

	return ret;
}

static int is_hw_msnr_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_msnr_apply_setfile(
	struct is_hw_ip *hw_ip, u32 scenario, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_msnr_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

int is_hw_msnr_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (__is_hw_msnr_s_common_reg(hw_ip, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	if (pmio_reinit_cache(hw_ip->pmio, &hw_ip->pmio_config)) {
		pmio_cache_set_bypass(hw_ip->pmio, true);
		err("failed to reinit PMIO cache, set bypass");
		return -EINVAL;
	}

	return 0;
}

static int is_hw_msnr_set_regs(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size)
{
	return 0;
}

int is_hw_msnr_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs,
	u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct is_hw_msnr *hw_msnr = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw_msnr->pcc;
	u32 i;
	int ret = 0;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		msnr_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
		msnr_hw_dump(hw_ip->pmio, HW_DUMP_CR);
		break;
	case IS_REG_DUMP_DMA:
		for (i = MSNR_WDMA_LME; i < MSNR_WDMA_MAX; i++) {
			dma = &hw_msnr->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		ret = -EINVAL;
	}

	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	return ret;
}

#define CMP_CONFIG(old, new, f)                                                                    \
	{                                                                                          \
		if (old.f != new->f || is_get_debug_param(IS_DEBUG_PARAM_HW) >= 2)                 \
			msinfo_hw("[F:%d] MSNR " #f ": %d -> %d", instance, hw_ip, fcount, old.f,  \
				new->f);                                                           \
	}

#define CMP_CONFIG_SIZE(old, new, f1, f2)                                                          \
	{                                                                                          \
		if (old.f1 != new->f1 || old.f2 != new->f2                                         \
				  || is_get_debug_param(IS_DEBUG_PARAM_HW) >= 2)                   \
			msinfo_hw("[F:%d] MSNR " #f1 " x " #f2 ": %dx%d -> %dx%d", instance, hw_ip,\
				fcount, old.f1, old.f2, new->f1, new->f2);                         \
	}

static int is_hw_msnr_set_config(
	struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_msnr *hw_msnr;
	struct is_msnr_config *msnr_config = (struct is_msnr_config *)conf;

	FIMC_BUG(!conf);

	hw_msnr = GET_HW(hw_ip);

	msdbg_hw(2, "[F:%d] magic(%d)\n", instance, hw_ip, fcount, msnr_config->magic);

	CMP_CONFIG(hw_msnr->config[instance], msnr_config, decomp_en);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, decomp_low_power_en);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, lmeds_input_select);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, lmeds_bypass);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, lmeds_stride);
	CMP_CONFIG_SIZE(hw_msnr->config[instance], msnr_config, lmeds_w, lmeds_h);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, msnrL0_contents_aware_isp_en);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, msnrL1_contents_aware_isp_en);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, msnrL2_contents_aware_isp_en);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, msnrL3_contents_aware_isp_en);
	CMP_CONFIG(hw_msnr->config[instance], msnr_config, msnrL4_contents_aware_isp_en);

	memcpy(&hw_msnr->config[instance], conf, sizeof(struct is_msnr_config));

	return 0;
}

static int is_hw_msnr_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_msnr *hw_msnr;
	struct pablo_common_ctrl *pcc;

	hw_msnr = GET_HW(hw_ip);
	if (!hw_msnr) {
		mserr_hw("failed to get HW MSNR", instance, hw_ip);
		return -ENODEV;
	}

	pcc = hw_msnr->pcc;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	msnr_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);

	CALL_PCC_OPS(pcc, set_qch, pcc, false);

	return 0;
}

static size_t is_hw_msnr_dump_params(struct is_hw_ip *hw_ip, u32 instance, char *buf, size_t size)
{
	struct is_hw_msnr *hw_msnr = GET_HW(hw_ip);
	struct msnr_param_set *param;
	size_t rem = size;
	char *p = buf;

	param = &hw_msnr->param_set[instance];

	p = pablo_json_nstr(p, "hw name", hw_ip->name, strlen(hw_ip->name), &rem);
	p = pablo_json_uint(p, "hw id", hw_ip->id, &rem);

	p = dump_param_stripe_input(p, "stripe_input", &param->stripe_input, &rem);
	p = dump_param_otf_input(p, "cin_msnr_l0", &param->cin_msnr_l0, &rem);
	p = dump_param_otf_input(p, "cin_msnr_l1", &param->cin_msnr_l1, &rem);
	p = dump_param_otf_input(p, "cin_msnr_l2", &param->cin_msnr_l2, &rem);
	p = dump_param_otf_input(p, "cin_msnr_l3", &param->cin_msnr_l3, &rem);
	p = dump_param_otf_input(p, "cin_msnr_l4", &param->cin_msnr_l4, &rem);
	p = dump_param_otf_output(p, "cout_msnr_yuv", &param->cout_msnr_yuv, &rem);
	p = dump_param_otf_output(p, "cout_msnr_stat", &param->cout_msnr_stat, &rem);
	p = dump_param_dma_output(p, "wdma_lme", &param->wdma_lme, &rem);

	p = pablo_json_uint(p, "instance_id", param->instance_id, &rem);
	p = pablo_json_uint(p, "fcount", param->fcount, &rem);
	p = pablo_json_uint(p, "tnr_mode", param->tnr_mode, &rem);
	p = pablo_json_uint(p, "mono_mode", param->mono_mode, &rem);
	p = pablo_json_bool(p, "reprocessing", param->reprocessing, &rem);

	return WRITTEN(size, rem);
}

const struct is_hw_ip_ops is_hw_msnr_ops = {
	.open = is_hw_msnr_open,
	.init = is_hw_msnr_init,
	.deinit = is_hw_msnr_deinit,
	.close = is_hw_msnr_close,
	.enable = is_hw_msnr_enable,
	.disable = is_hw_msnr_disable,
	.shot = is_hw_msnr_shot,
	.set_param = is_hw_msnr_set_param,
	.get_meta = is_hw_msnr_get_meta,
	.get_cap_meta = is_hw_msnr_get_cap_meta,
	.frame_ndone = is_hw_msnr_frame_ndone,
	.load_setfile = is_hw_msnr_load_setfile,
	.apply_setfile = is_hw_msnr_apply_setfile,
	.delete_setfile = is_hw_msnr_delete_setfile,
	.restore = is_hw_msnr_restore,
	.set_regs = is_hw_msnr_set_regs,
	.dump_regs = is_hw_msnr_dump_regs,
	.set_config = is_hw_msnr_set_config,
	.notify_timeout = is_hw_msnr_notify_timeout,
	.reset = is_hw_msnr_reset,
	.wait_idle = is_hw_msnr_wait_idle,
	.dump_params = is_hw_msnr_dump_params,
};

int pablo_hw_msnr_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;
	int ret = 0;

	hw_ip->ops = &is_hw_msnr_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_msnr_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_msnr_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.name = "msnr";

	hw_ip->pmio_config.mmio_base = hw_ip->regs[REG_SETA];
	hw_ip->pmio_config.phys_base = hw_ip->regs_start[REG_SETA];

	hw_ip->pmio_config.cache_type = PMIO_CACHE_NONE;

	msnr_hw_init_pmio_config(&hw_ip->pmio_config);

	hw_ip->pmio = pmio_init(NULL, NULL, &hw_ip->pmio_config);
	if (IS_ERR(hw_ip->pmio)) {
		err("failed to init msnr PMIO: %ld", PTR_ERR(hw_ip->pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(hw_ip->pmio, &hw_ip->pmio_fields, hw_ip->pmio_config.fields,
		hw_ip->pmio_config.num_fields);
	if (ret) {
		err("failed to alloc msnr PMIO fields: %d", ret);
		pmio_exit(hw_ip->pmio);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(pablo_hw_msnr_probe);

void pablo_hw_msnr_remove(struct is_hw_ip *hw_ip)
{
	struct is_interface_ischain *itfc = hw_ip->itfc;
	int id = hw_ip->id;
	int hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = false;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = false;

	pmio_field_bulk_free(hw_ip->pmio, hw_ip->pmio_fields);
	pmio_exit(hw_ip->pmio);
}
EXPORT_SYMBOL_GPL(pablo_hw_msnr_remove);
