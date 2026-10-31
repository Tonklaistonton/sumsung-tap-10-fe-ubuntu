// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-helper.h"
#include "is-hw-yuvp-v3.h"
#include "is-err.h"
#include "api/is-hw-api-yuvp-v3_0.h"
#include "is-votfmgr.h"
#include "is-votf-id-table.h"
#include "is-stripe.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

#define GET_HW(hw_ip) ((struct is_hw_yuvp *)hw_ip->priv_info)

static inline void *get_base(struct is_hw_ip *hw_ip)
{
	return hw_ip->pmio;
}

static struct is_yuvp_config config_default = {
	.clahe_bypass = 1,
	.clahe_grid_num = 0,
	.drc_bypass = 1,
	.drc_grid_w = 0,
	.drc_grid_h = 0,
	.drc_grid_enabled_num = 1,
	.pcchist_bypass = 1,
	.ccm_contents_aware_isp_en = 0,
	.sharpen_contents_aware_isp_en = 0,
	.pcc_contents_aware_isp_en = 0,
	.drc_contents_aware_isp_en = 0,
};

static int is_hw_yuvp_handle_interrupt0(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	struct pablo_common_ctrl *pcc;
	u32 status, instance, hw_fcount;
	bool f_err = false;

	hw_ip = (struct is_hw_ip *)context;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt, hw_ip state(0x%lx)", instance, hw_ip, hw_ip->state);
		return 0;
	}

	hw_yuvp = GET_HW(hw_ip);
	pcc = hw_yuvp->pcc;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	hw_yuvp->irq_state[id] = status = CALL_PCC_OPS(pcc, get_int_status, pcc, id, true);

	msdbg_hw(2, "YUVP0 interrupt status(0x%x)\n", instance, hw_ip, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt: 0x%x", instance, hw_ip, status);
		goto exit;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("During recovery : invalid interrupt", instance, hw_ip);
		goto exit;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		goto exit;
	}

	if (yuvp_hw_is_occurred0(status, INTR_SETTING_DONE))
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (yuvp_hw_is_occurred0(status, INTR_FRAME_START) &&
		yuvp_hw_is_occurred0(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (yuvp_hw_is_occurred0(status, INTR_FRAME_START)) {
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fs);
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
	}

	if (yuvp_hw_is_occurred0(status, INTR_FRAME_END)) {
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_END);
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fe);

		CALL_HW_OPS(
			hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);

		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.E\n", instance, hw_ip, hw_fcount);

		if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
			mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)", instance, hw_ip,
				atomic_read(&hw_ip->count.fs), atomic_read(&hw_ip->count.fe),
				atomic_read(&hw_ip->count.dma), status);
		}
		wake_up(&hw_ip->status.wait_queue);

		if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_YUVP)))
			yuvp_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
	}

	f_err = yuvp_hw_is_occurred0(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[YUVP0][F:%d] Ocurred error interrupt : status(0x%x)\n", instance, hw_ip,
			hw_fcount, status);
		yuvp_hw_dump(hw_ip->pmio, HW_DUMP_CR);
	}

exit:
	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	return 0;
}

static int is_hw_yuvp_handle_interrupt1(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_yuvp *hw_yuvp;
	struct pablo_common_ctrl *pcc;
	u32 status, instance, hw_fcount;
	bool f_err;

	hw_ip = (struct is_hw_ip *)context;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt, hw_ip state(0x%lx)", instance, hw_ip, hw_ip->state);
		return 0;
	}

	hw_yuvp = GET_HW(hw_ip);
	pcc = hw_yuvp->pcc;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	hw_yuvp->irq_state[id] = status = CALL_PCC_OPS(pcc, get_int_status, pcc, id, true);

	msdbg_hw(2, "YUVP1 interrupt status(0x%x)\n", instance, hw_ip, status);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt: 0x%x", instance, hw_ip, status);
		goto exit;
	}

	if (test_bit(HW_OVERFLOW_RECOVERY, &hardware->hw_recovery_flag)) {
		mserr_hw("During recovery : invalid interrupt", instance, hw_ip);
		goto exit;
	}

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		goto exit;
	}

	f_err = yuvp_hw_is_occurred1(status, INTR_ERR);
	if (f_err) {
		msinfo_hw("[YUVP1][F:%d] Ocurred error interrupt : status(0x%x)\n", instance, hw_ip,
			hw_fcount, status);
		yuvp_hw_dump(hw_ip->pmio, HW_DUMP_CR);
	}

exit:
	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	return 0;
}

static int is_hw_yuvp_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw;
	struct pablo_common_ctrl *pcc;

	msinfo_hw("reset\n", instance, hw_ip);

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	return CALL_PCC_OPS(pcc, reset, pcc);
}

static void is_hw_yuvp_prepare(struct is_hw_ip *hw_ip, u32 instance)
{
	u32 seed;

	seed = is_get_debug_param(IS_DEBUG_PARAM_CRC_SEED);
	if (unlikely(seed))
		yuvp_hw_s_crc(get_base(hw_ip), seed);
}

static int is_hw_yuvp_finish(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	ret = yuvp_hw_wait_idle(get_base(hw_ip));
	if (ret)
		mserr_hw("failed to yuvp_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished yuvp\n", instance, hw_ip);

	return ret;
}

static int __nocfi is_hw_yuvp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;
	u32 rdma_max_cnt, wdma_max_cnt;
	struct is_mem *mem;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, "HWYUVP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME, false);

	hw_ip->priv_info = pablo_zalloc(sizeof(struct is_hw_yuvp), GFP_KERNEL);
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_yuvp = GET_HW(hw_ip);
	hw_yuvp->instance = instance;
	hw_yuvp->pcc = pablo_common_ctrl_hw_get_pcc(&hw_ip->pmio_config);

	ret = CALL_PCC_HW_OPS(
		hw_yuvp->pcc, init, hw_yuvp->pcc, hw_ip->pmio, hw_ip->name, PCC_M2M, NULL);
	if (ret) {
		mserr_hw("failed to pcc init. ret %d", instance, hw_ip, ret);
		goto err_pcc_init;
	}

	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, &hw_yuvp->lib[instance], LIB_FUNC_YUVP);
	if (ret)
		goto err_chain_create;

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, yuvp_hw_g_reg_cnt());
	if (ret)
		goto err_iqset_alloc;

	rdma_max_cnt = yuvp_hw_g_rdma_max_cnt();
	hw_yuvp->rdma = pablo_zalloc(sizeof(struct is_common_dma) * rdma_max_cnt, GFP_KERNEL);
	if (!hw_yuvp->rdma) {
		mserr_hw("Failed to allocate rdma", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc_dma;
	}

	wdma_max_cnt = yuvp_hw_g_wdma_max_cnt();
	hw_yuvp->wdma = pablo_zalloc(sizeof(struct is_common_dma) * wdma_max_cnt, GFP_KERNEL);
	if (!hw_yuvp->wdma) {
		mserr_hw("Failed to allocate wdma", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc_dma;
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_YUVP);

	hw_yuvp->pb_c_loader_payload = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x16000, NULL, 0);
	if (IS_ERR_OR_NULL(hw_yuvp->pb_c_loader_payload)) {
		hw_yuvp->pb_c_loader_payload = NULL;
		err("failed to allocate buffer for c-loader payload");
		return -ENOMEM;
	}
	hw_yuvp->kva_c_loader_payload =
		CALL_BUFOP(hw_yuvp->pb_c_loader_payload, kvaddr, hw_yuvp->pb_c_loader_payload);
	hw_yuvp->dva_c_loader_payload =
		CALL_BUFOP(hw_yuvp->pb_c_loader_payload, dvaddr, hw_yuvp->pb_c_loader_payload);

	hw_yuvp->pb_c_loader_header = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x4000, NULL, 0);
	if (IS_ERR_OR_NULL(hw_yuvp->pb_c_loader_header)) {
		hw_yuvp->pb_c_loader_header = NULL;
		err("failed to allocate buffer for c-loader header");
		return -ENOMEM;
	}
	hw_yuvp->kva_c_loader_header =
		CALL_BUFOP(hw_yuvp->pb_c_loader_header, kvaddr, hw_yuvp->pb_c_loader_header);
	hw_yuvp->dva_c_loader_header =
		CALL_BUFOP(hw_yuvp->pb_c_loader_header, dvaddr, hw_yuvp->pb_c_loader_header);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_alloc_dma:
	if (hw_yuvp->rdma)
		pablo_free(hw_yuvp->rdma);

	if (hw_yuvp->wdma)
		pablo_free(hw_yuvp->wdma);

err_iqset_alloc:
	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_yuvp->lib[instance]);

err_chain_create:
	CALL_PCC_HW_OPS(hw_yuvp->pcc, deinit, hw_yuvp->pcc);
err_pcc_init:
	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_yuvp_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	int ret;
	struct is_hw_yuvp *hw_yuvp;
	u32 rmda_max_cnt, wdma_max_cnt;
	u32 dma_id;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = GET_HW(hw_ip);
	if (!hw_yuvp) {
		mserr_hw("hw_yuvp is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	ret = CALL_HW_HELPER_OPS(
		hw_ip, init, instance, &hw_yuvp->lib[instance], (u32)flag, f_type, LIB_FUNC_YUVP);
	if (ret)
		return ret;

	rmda_max_cnt = yuvp_hw_g_rdma_max_cnt();
	for (dma_id = 0; dma_id < rmda_max_cnt; dma_id++) {
		ret = yuvp_hw_rdma_create(&hw_yuvp->rdma[dma_id], get_base(hw_ip), dma_id);
		if (ret) {
			mserr_hw("yuvp_hw_rdma_create error[%d]", instance, hw_ip, dma_id);
			ret = -ENODATA;
			goto err;
		}
	}

	wdma_max_cnt = yuvp_hw_g_wdma_max_cnt();
	for (dma_id = 0; dma_id < wdma_max_cnt; dma_id++) {
		ret = yuvp_hw_wdma_create(&hw_yuvp->wdma[dma_id], get_base(hw_ip), dma_id);
		if (ret) {
			mserr_hw("yuvp_hw_wdma_create error[%d]", instance, hw_ip, dma_id);
			ret = -ENODATA;
			goto err;
		}
	}

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;
}

static int is_hw_yuvp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);

	return CALL_HW_HELPER_OPS(hw_ip, deinit, instance, &hw_yuvp->lib[instance]);
}

static int is_hw_yuvp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_yuvp = GET_HW(hw_ip);

	CALL_HW_HELPER_OPS(hw_ip, close, instance, &hw_yuvp->lib[instance]);

	is_hw_yuvp_finish(hw_ip, instance);

	CALL_BUFOP(hw_yuvp->pb_c_loader_payload, free, hw_yuvp->pb_c_loader_payload);
	CALL_BUFOP(hw_yuvp->pb_c_loader_header, free, hw_yuvp->pb_c_loader_header);

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);

	CALL_PCC_HW_OPS(hw_yuvp->pcc, deinit, hw_yuvp->pcc);

	pablo_free(hw_yuvp->rdma);
	pablo_free(hw_yuvp->wdma);

	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_yuvp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_mmio *pmio;
	struct is_hw_yuvp *hw_yuvp;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_cfg cfg = {
		0,
	};
	int ret;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	if (test_bit(HW_RUN, &hw_ip->state))
		return 0;

	msdbg_hw(2, "enable: start\n", instance, hw_ip);

	hw_yuvp = GET_HW(hw_ip);
	pcc = hw_yuvp->pcc;
	pmio = hw_ip->pmio;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
	if (pmio_reinit_cache(pmio, &hw_ip->pmio_config)) {
		pmio_cache_set_bypass(pmio, true);
		err("failed to reinit PMIO cache, set bypass");
		return -EINVAL;
	}

	is_hw_yuvp_prepare(hw_ip, instance);

	cfg.fs_mode = PCC_ASAP;
	yuvp_hw_g_int_en(cfg.int_en);
	CALL_PCC_OPS(pcc, enable, pcc, &cfg);

	pmio_cache_set_only(pmio, true);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_yuvp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_yuvp *hw_yuvp;
	struct yuvp_param_set *param_set;
	struct pablo_common_ctrl *pcc;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw(
		"yuvp_disable: Vvalid(%d)\n", instance, hw_ip, atomic_read(&hw_ip->status.Vvalid));

	hw_yuvp = GET_HW(hw_ip);

	timetowait = wait_event_timeout(
		hw_ip->status.wait_queue, !atomic_read(&hw_ip->status.Vvalid), IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip, timetowait);
		ret = -ETIME;
	}

	param_set = &hw_yuvp->param_set[instance];
	param_set->fcount = 0;

	CALL_HW_HELPER_OPS(hw_ip, disable, instance, &hw_yuvp->lib[instance]);

	if (hw_ip->run_rsc_state)
		return 0;

	pcc = hw_yuvp->pcc;
	CALL_PCC_OPS(pcc, disable, pcc);

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static int __is_hw_yuvp_set_rdma_cmd(
	struct yuvp_param_set *param_set, u32 instance, struct is_yuvp_config *conf)
{
	mdbg_hw(4, "%s\n", instance, __func__);

	if (conf->clahe_bypass & 0x1)
		param_set->dma_input_clahe.cmd = DMA_INPUT_COMMAND_DISABLE;

	if (conf->drc_bypass & 0x1)
		param_set->dma_input_drc.cmd = DMA_INPUT_COMMAND_DISABLE;

	if (!conf->ccm_contents_aware_isp_en && !conf->sharpen_contents_aware_isp_en &&
		!conf->drc_contents_aware_isp_en)
		param_set->dma_input_seg.cmd = DMA_INPUT_COMMAND_DISABLE;

	if (conf->pcchist_bypass & 0x1)
		param_set->dma_input_pcchist.cmd = DMA_INPUT_COMMAND_DISABLE;

	return 0;
}

static int __is_hw_yuvp_set_wdma_cmd(
	struct yuvp_param_set *param_set, u32 instance, struct is_yuvp_config *conf)
{
	mdbg_hw(4, "%s\n", instance, __func__);

	if (conf->pcchist_bypass & 0x1)
		param_set->dma_output_pcchist.cmd = DMA_OUTPUT_COMMAND_DISABLE;

	return 0;
}

static int __is_hw_yuvp_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_yuvp *hw_yuvp,
	struct yuvp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	pdma_addr_t *input_dva = NULL;
	struct is_yuvp_config *config = &hw_yuvp->config[instance];
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 strip_offset = 0, header_offset = 0;
	u32 in_crop_size_x = 0;
	u32 cache_hint;
	int ret;

	if (yuvp_hw_get_input_dva(
		    id, &cmd, &input_dva, param_set, config->drc_grid_enabled_num) < 0) {
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	if (yuvp_hw_get_rdma_cache_hint(id, &cache_hint) < 0) {
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %s(%d)\n", instance, hw_ip, __func__, hw_yuvp->rdma[id].name, cmd);

	yuvp_hw_s_rdma_corex_id(&hw_yuvp->rdma[id], set_id);

	ret = yuvp_hw_s_rdma_init(&hw_yuvp->rdma[id], param_set, cmd,
		config->clahe_grid_num, config->drc_grid_w,
		config->drc_grid_h, in_crop_size_x, cache_hint, &comp_sbwc_en,
		&payload_size, &strip_offset, &header_offset);
	if (ret) {
		mserr_hw("failed to initialize YUVPP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = yuvp_hw_s_rdma_addr(&hw_yuvp->rdma[id], input_dva, 0, hw_ip->num_buffers, 0,
			comp_sbwc_en, payload_size, strip_offset, header_offset);
		if (ret) {
			mserr_hw("failed to set YUVPP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_yuvp_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_yuvp *hw_yuvp,
	struct yuvp_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	pdma_addr_t *output_dva = NULL;
	struct is_yuvp_config *config = &hw_yuvp->config[instance];
	u32 cmd;
	u32 comp_sbwc_en, payload_size;
	u32 in_crop_size_x = 0;
	u32 cache_hint;
	int ret;

	if (yuvp_hw_get_output_dva(id, &cmd, &output_dva, param_set)) {
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	if (yuvp_hw_get_wdma_cache_hint(id, &cache_hint) < 0) {
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %s(%d)\n", instance, hw_ip, __func__, hw_yuvp->wdma[id].name, cmd);

	yuvp_hw_s_wdma_corex_id(&hw_yuvp->wdma[id], set_id);

	ret = yuvp_hw_s_wdma_init(&hw_yuvp->wdma[id], param_set, cmd, 0, config->drc_grid_w,
		config->drc_grid_h, in_crop_size_x, cache_hint, &comp_sbwc_en,
		&payload_size);
	if (ret) {
		mserr_hw("failed to initialize YUVPP_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = yuvp_hw_s_wdma_addr(&hw_yuvp->wdma[id], output_dva, 0, hw_ip->num_buffers, 0,
			comp_sbwc_en, payload_size);
		if (ret) {
			mserr_hw("failed to set YUVPP_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static void __is_hw_yuvp_check_size(
	struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set, u32 instance)
{
	struct param_dma_input *input = &param_set->dma_input;
	struct param_otf_output *output = &param_set->otf_output;

	msdbgs_hw(2, "hw_yuvp_check_size >>>\n", instance, hw_ip);
	msdbgs_hw(2, "dma_input: format(%d),crop_size(%dx%d)\n", instance, hw_ip, input->format,
		input->dma_crop_width, input->dma_crop_height);
	msdbgs_hw(2, "otf_output: otf_cmd(%d),format(%d)\n", instance, hw_ip, output->cmd,
		output->format);
	msdbgs_hw(2, "otf_output: pos(%d,%d),crop%dx%d),size(%dx%d)\n", instance, hw_ip,
		output->crop_offset_x, output->crop_offset_y, output->crop_width,
		output->crop_height, output->width, output->height);
	msdbgs_hw(2, "<<< hw_yuvp_check_size <<<\n", instance, hw_ip);
}

static int __is_hw_yuvp_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	sdbg_hw(4, "%s\n", hw_ip, __func__);

	yuvp_hw_s_block_bypass(get_base(hw_ip), set_id);

	return 0;
}

static int __is_hw_yuvp_update_block_reg(
	struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_yuvp = GET_HW(hw_ip);

	if (hw_yuvp->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n", instance, hw_ip,
			hw_yuvp->instance);
		hw_yuvp->instance = instance;
	}

	__is_hw_yuvp_check_size(hw_ip, param_set, instance);
	return __is_hw_yuvp_bypass(hw_ip, set_id);
}

static void __is_hw_yuvp_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
	struct yuvp_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct yuvp_param *param;

	param = &p_region->yuvp;
	param_set->instance_id = instance;

	param_set->mono_mode = hw_ip->region[instance]->parameter.sensor.config.mono_mode;

	/* check input */
	if (test_bit(PARAM_YUVP_OTF_INPUT, pmap)) {
		memcpy(&param_set->otf_input, &param->otf_input, sizeof(struct param_otf_input));
		msdbg_hw(4, "%s : PARAM_YUVP_OTF_INPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_DMA_INPUT, pmap)) {
		memcpy(&param_set->dma_input, &param->dma_input, sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_DMA_INPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_DRC, pmap)) {
		memcpy(&param_set->dma_input_drc, &param->drc, sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_DRC\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_CLAHE, pmap)) {
		memcpy(&param_set->dma_input_clahe, &param->clahe, sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_CLAHE\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_SEG, pmap)) {
		memcpy(&param_set->dma_input_seg, &param->seg, sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_SEG_MAP\n", instance, hw_ip, __func__);
	}

	/* check output */
	if (test_bit(PARAM_YUVP_OTF_OUTPUT, pmap)) {
		memcpy(&param_set->otf_output, &param->otf_output, sizeof(struct param_otf_output));
		msdbg_hw(4, "%s : PARAM_YUVP_OTF_OUTPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_STRIPE_INPUT, pmap)) {
		memcpy(&param_set->stripe_input, &param->stripe_input,
			sizeof(struct param_stripe_input));
		msdbg_hw(4, "%s : PARAM_YUVP_STRIPE_INPUT\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_PCCHIST_R, pmap)) {
		memcpy(&param_set->dma_input_pcchist, &param->pcchist_in,
			sizeof(struct param_dma_input));
		msdbg_hw(4, "%s : PARAM_YUVP_PCCHIST_R\n", instance, hw_ip, __func__);
	}

	if (test_bit(PARAM_YUVP_PCCHIST_W, pmap)) {
		memcpy(&param_set->dma_output_pcchist, &param->pcchist_out,
			sizeof(struct param_dma_output));
		msdbg_hw(4, "%s : PARAM_YUVP_PCCHIST_W\n", instance, hw_ip, __func__);
	}
}

static int is_hw_yuvp_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_yuvp = GET_HW(hw_ip);
	hw_yuvp->instance = IS_STREAM_COUNT;

	return 0;
}

static void __is_hw_yuvp_set_strip_regs(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	u32 enable, u32 start_pos, u32 set_id)
{
	u32 strip_index, strip_total_count;
	enum yuvp_strip_type strip_type = YUVP_STRIP_TYPE_NONE;

	if (enable) {
		strip_index = param_set->stripe_input.index;
		strip_total_count = param_set->stripe_input.total_count;

		if (!strip_index)
			strip_type = YUVP_STRIP_TYPE_FIRST;
		else if (strip_index == strip_total_count - 1)
			strip_type = YUVP_STRIP_TYPE_LAST;
		else
			strip_type = YUVP_STRIP_TYPE_MIDDLE;
	}
	yuvp_hw_s_strip(get_base(hw_ip), set_id, enable, start_pos, strip_type,
		param_set->stripe_input.left_margin, param_set->stripe_input.right_margin,
		param_set->stripe_input.full_width);
}

static int __is_hw_yuvp_check_dlnr_decomp(
	struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame)
{
	void *config;

	config = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_config, DEV_HW_MSNR, frame);
	if (!config) {
		mserr_hw("config plane is NULL", instance, hw_ip);
		return false;
	}

	return ((struct is_msnr_config *)(config + sizeof(u32)))->decomp_en;
}

static int __is_hw_yuvp_set_size_regs(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	u32 instance, u32 strip_start_pos, struct is_frame *frame, u32 set_id,
	enum yuvp_input_path_type input_path)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_yuvp_config *config;
	int ret;
	u32 strip_enable;
	u32 width, height;
	u32 frame_width;
	u32 region_id;
	struct yuvp_radial_cfg radial_cfg;
	bool mono_mode_en;
	bool dlnr_decomp;

	hw_yuvp = GET_HW(hw_ip);
	config = &hw_yuvp->config[instance];

	strip_enable = (param_set->stripe_input.total_count == 0) ? 0 : 1;
	mono_mode_en = param_set->mono_mode;

	region_id = param_set->stripe_input.index;

	dlnr_decomp = __is_hw_yuvp_check_dlnr_decomp(hw_ip, instance, frame);

	if (dlnr_decomp) {
		width = ALIGN(param_set->otf_input.width / 2, 2);
		height = ALIGN(param_set->otf_input.height / 2, 2);
	} else {
		width = param_set->otf_input.width;
		height = param_set->otf_input.height;
	}

	msdbg_hw(4, "%s : path %d, (w x h) = (%d x %d)\n", instance, hw_ip, __func__, input_path,
		width, height);

	frame_width = (strip_enable) ? param_set->stripe_input.full_width : width;

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

	ret = yuvp_hw_s_cnr_size(get_base(hw_ip), set_id, strip_start_pos, frame_width, width,
		height, strip_enable, &radial_cfg);
	if (ret) {
		mserr_hw("failed to set size regs for cnr", instance, hw_ip);
		goto err;
	}

	ret = yuvp_hw_s_sharpen_size(get_base(hw_ip), set_id, strip_start_pos, frame_width, width,
		height, strip_enable, &radial_cfg);
	if (ret) {
		mserr_hw("failed to set size regs for sharp adder", instance, hw_ip);
		goto err;
	}

	__is_hw_yuvp_set_strip_regs(hw_ip, param_set, strip_enable, strip_start_pos, set_id);

	yuvp_hw_s_size(get_base(hw_ip), set_id, width, height, strip_enable);
	yuvp_hw_s_mono_mode(get_base(hw_ip), set_id, mono_mode_en);
err:
	return ret;
}

static void __is_hw_yuvp_config_path(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	u32 instance, u32 set_id, enum yuvp_input_path_type *input_path,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);

	if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		*input_path = YUVP_INPUT_RDMA;
		yuvp_hw_s_mux_dtp(get_base(hw_ip), set_id, YUVP_MUX_DTP_RDMA_YUV);
	} else {
		*input_path = YUVP_INPUT_OTF;
		yuvp_hw_s_cinfifo(get_base(hw_ip), set_id);
		yuvp_hw_s_mux_dtp(get_base(hw_ip), set_id, YUVP_MUX_DTP_CINFIFO);
	}

	yuvp_hw_s_mux_cout_sel(get_base(hw_ip), set_id, param_set);

	/* MUX SEGCONF 0: CINFIFO 1: RDMA */
	if (param_set->dma_input_seg.cmd == DMA_INPUT_COMMAND_ENABLE)
		yuvp_hw_s_mux_segconf_sel(get_base(hw_ip), set_id, 1);
	else
		yuvp_hw_s_mux_segconf_sel(get_base(hw_ip), set_id, 0);

	yuvp_hw_s_input_path(get_base(hw_ip), set_id, *input_path, frame_cfg);

	if (param_set->otf_output.cmd == DMA_INPUT_COMMAND_ENABLE) {
		yuvp_hw_s_coutfifo(get_base(hw_ip), set_id);
		yuvp_hw_s_output_path(get_base(hw_ip), set_id, 1, frame_cfg);
	} else {
		yuvp_hw_s_output_path(get_base(hw_ip), set_id, 0, frame_cfg);
	}

	yuvp_hw_s_demux_enable(get_base(hw_ip), set_id, param_set, hw_yuvp->config[instance]);
}

static void __is_hw_yuvp_config_dma(struct is_hw_ip *hw_ip, struct yuvp_param_set *param_set,
	struct is_frame *frame, u32 fcount, u32 instance)
{
	u32 cur_idx;
	u32 cmd_input, cmd_input_drc, cmd_input_clahe, cmd_input_scmap;
	u32 cmd_input_pcchist, cmd_output_pcchist;

	cur_idx = frame->cur_buf_index;

	cmd_input = CALL_HW_OPS(hw_ip, dma_cfg, "yuvp", hw_ip, frame, cur_idx, frame->num_buffers,
		&param_set->dma_input.cmd, param_set->dma_input.plane, param_set->input_dva,
		frame->dvaddr_buffer);

	cmd_input_drc = CALL_HW_OPS(hw_ip, dma_cfg, "ypdga", hw_ip, frame, 0, frame->num_buffers,
				    &param_set->dma_input_drc.cmd, param_set->dma_input_drc.plane,
				    param_set->input_dva_drc, frame->ypdgaTargetAddress);

	/* only 1 SVHIST buffer is valid per 1 batch shot
	 * always use index 0 buffer for batch mode
	 */
	cmd_input_clahe =
		CALL_HW_OPS(hw_ip, dma_cfg, "ypclahe", hw_ip, frame, 0, frame->num_buffers,
			&param_set->dma_input_clahe.cmd, param_set->dma_input_clahe.plane,
			param_set->input_dva_clahe, frame->ypclaheTargetAddress);

	cmd_input_scmap = CALL_HW_OPS(hw_ip, dma_cfg, "scmap", hw_ip, frame, cur_idx,
		frame->num_buffers, &param_set->dma_input_seg.cmd, param_set->dma_input_seg.plane,
		param_set->input_dva_seg, frame->ixscmapTargetAddress);

	cmd_input_pcchist =
		CALL_HW_OPS(hw_ip, dma_cfg, "pcc_rdma", hw_ip, frame, 0, frame->num_buffers,
			&param_set->dma_input_pcchist.cmd, param_set->dma_input_pcchist.plane,
			param_set->input_dva_pcchist, frame->dva_yuvp_out_pcchist);

	cmd_output_pcchist =
		CALL_HW_OPS(hw_ip, dma_cfg, "pcc_wdma", hw_ip, frame, 0, frame->num_buffers,
			&param_set->dma_output_pcchist.cmd, param_set->dma_output_pcchist.plane,
			param_set->output_dva_pcchist, frame->dva_yuvp_cap_pcchist);

	param_set->instance_id = instance;
	param_set->fcount = fcount;
}

#define CMP_CONFIG(old, new, f)                                                                    \
	{                                                                                          \
		if (old.f != new->f)                                                               \
			msinfo_hw("[F:%d] YUVP " #f ": %d -> %d", instance, hw_ip, fcount, old.f,  \
				new->f);                                                           \
	}

#define CMP_CONFIG_SIZE(old, new, f1, f2)                                                          \
	{                                                                                          \
		if (old.f1 != new->f1 || old.f2 != new->f2)                                        \
			msinfo_hw("[F:%d] YUVP " #f1 " x " #f2 ": %dx%d -> %dx%d", instance, hw_ip,\
				fcount, old.f1, old.f2, new->f1, new->f2);                         \
	}

static int is_hw_yuvp_set_config(
	struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *config)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_yuvp_config *conf = (struct is_yuvp_config *)config;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!conf);

	hw_yuvp = GET_HW(hw_ip);

	CMP_CONFIG(hw_yuvp->config[instance], conf, clahe_bypass);
	CMP_CONFIG(hw_yuvp->config[instance], conf, clahe_grid_num);
	CMP_CONFIG(hw_yuvp->config[instance], conf, drc_bypass);
	CMP_CONFIG_SIZE(hw_yuvp->config[instance], conf, drc_grid_w, drc_grid_h);
	CMP_CONFIG(hw_yuvp->config[instance], conf, drc_grid_enabled_num);
	CMP_CONFIG(hw_yuvp->config[instance], conf, pcchist_bypass);

	CMP_CONFIG(hw_yuvp->config[instance], conf, drc_contents_aware_isp_en);
	CMP_CONFIG(hw_yuvp->config[instance], conf, ccm_contents_aware_isp_en);
	CMP_CONFIG(hw_yuvp->config[instance], conf, pcc_contents_aware_isp_en);
	CMP_CONFIG(hw_yuvp->config[instance], conf, sharpen_contents_aware_isp_en);

	hw_yuvp->config[instance] = *conf;

	return 0;
}

static void _is_hw_yuvp_s_cmd(struct is_hw_yuvp *hw, struct c_loader_buffer *clb, u32 fcount,
	struct pablo_common_ctrl_cmd *cmd)
{
	cmd->set_mode = PCC_APB_DIRECT;
	cmd->fcount = fcount;
	cmd->int_grp_en = yuvp_hw_g_int_grp_en();

	if (!clb)
		return;

	cmd->base_addr = clb->header_dva;
	cmd->header_num = clb->num_of_headers;
	cmd->set_mode = PCC_DMA_DIRECT;
}

static int is_hw_yuvp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp;
	struct is_param_region *param_region;
	struct yuvp_param_set *param_set;
	u32 fcount, instance;
	u32 cur_idx;
	u32 strip_start_pos;
	u32 set_id;
	int ret, i;
	u32 strip_index, strip_total_count, strip_repeat_num, strip_repeat_idx;
	enum yuvp_input_path_type input_path;
	bool do_blk_cfg;
	u32 cmd, bypass;
	bool skip = false;
	u32 rdma_max_cnt, wdma_max_cnt;
	struct c_loader_buffer clb;
	struct is_yuvp_config conf = {
		0,
	};
	struct pablo_common_ctrl_frame_cfg frame_cfg = {
		0,
	};
	struct pablo_common_ctrl *pcc;

	instance = frame->instance;
	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

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

	hw_yuvp = GET_HW(hw_ip);
	pcc = hw_yuvp->pcc;
	param_set = &hw_yuvp->param_set[instance];
	param_region = frame->parameter;

	atomic_set(&hw_yuvp->strip_index, frame->stripe_info.region_id);
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

	__is_hw_yuvp_update_param(hw_ip, param_region, param_set, frame->pmap, instance);
	__is_hw_yuvp_config_dma(hw_ip, param_set, frame, fcount, instance);

	/* FRO */
	cur_idx = frame->cur_buf_index;
	hw_ip->num_buffers = frame->num_buffers;
	frame_cfg.num_buffers = frame->num_buffers;

	strip_index = param_set->stripe_input.index;
	strip_total_count = param_set->stripe_input.total_count;
	strip_repeat_num = param_set->stripe_input.repeat_num;
	strip_repeat_idx = param_set->stripe_input.repeat_idx;
	strip_start_pos = (strip_index) ? (param_set->stripe_input.start_pos_x) : 0;

	if (IS_ENABLED(SKIP_ISP_SHOT_FOR_MULTI_SHOT)) {
		if (hw_yuvp->repeat_instance != instance)
			hw_yuvp->repeat_state = 0;

		if (frame_cfg.num_buffers > 1 || strip_total_count > 1 || strip_repeat_num > 1)
			hw_yuvp->repeat_state++;
		else
			hw_yuvp->repeat_state = 0;

		if (hw_yuvp->repeat_state > 1 &&
			(!pablo_is_first_shot(frame_cfg.num_buffers, cur_idx) ||
				!pablo_is_first_shot(strip_total_count, strip_index) ||
				!pablo_is_first_shot(strip_repeat_num, strip_repeat_idx)))
			skip = true;

		hw_yuvp->repeat_instance = instance;
	}

	msdbgs_hw(2,
		"[F:%d] repeat_state(%d), batch(%d, %d), strip(%d, %d), repeat(%d, %d), skip(%d)",
		instance, hw_ip, frame->fcount, hw_yuvp->repeat_state, frame_cfg.num_buffers,
		cur_idx, strip_total_count, strip_index, strip_repeat_num, strip_repeat_idx, skip);

	/* temp direct set*/
	set_id = COREX_DIRECT;

	/* reset CLD buffer */
	clb.num_of_headers = 0;
	clb.num_of_values = 0;
	clb.num_of_pairs = 0;

	clb.header_dva = hw_yuvp->dva_c_loader_header;
	clb.payload_dva = hw_yuvp->dva_c_loader_payload;

	clb.clh = (struct c_loader_header *)hw_yuvp->kva_c_loader_header;
	clb.clp = (struct c_loader_payload *)hw_yuvp->kva_c_loader_payload;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	yuvp_hw_s_core(get_base(hw_ip), frame->num_buffers, set_id);

	ret = CALL_HW_HELPER_OPS(
		hw_ip, lib_shot, instance, skip, frame, &hw_yuvp->lib[instance], param_set);
	if (ret)
		return ret;

	do_blk_cfg = CALL_HW_HELPER_OPS(
		hw_ip, set_rta_regs, instance, set_id, skip, frame, (void *)&clb);
	if (unlikely(do_blk_cfg)) {
		is_hw_yuvp_set_config(hw_ip, 0, instance, fcount, &config_default);
		__is_hw_yuvp_update_block_reg(hw_ip, param_set, instance, set_id);
	}

	conf = hw_yuvp->config[instance];

	__is_hw_yuvp_config_path(hw_ip, param_set, instance, set_id, &input_path, &frame_cfg);
	__is_hw_yuvp_set_rdma_cmd(param_set, instance, &conf);
	__is_hw_yuvp_set_wdma_cmd(param_set, instance, &conf);

	ret = __is_hw_yuvp_set_size_regs(
		hw_ip, param_set, instance, strip_start_pos, frame, set_id, input_path);
	if (ret) {
		mserr_hw("__is_hw_yuvp_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	rdma_max_cnt = yuvp_hw_g_rdma_max_cnt();
	for (i = 0; i < rdma_max_cnt; i++) {
		ret = __is_hw_yuvp_set_rdma(hw_ip, hw_yuvp, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_yuvp_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	cmd = param_set->dma_input_clahe.cmd;
	bypass = conf.clahe_bypass;
	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass && IS_ENABLED(IRTA_CALL))
		conf.clahe_bypass = 1;

	yuvp_hw_s_clahe_bypass(get_base(hw_ip), set_id, conf.clahe_bypass);

	cmd = param_set->dma_input_drc.cmd;
	bypass = conf.drc_bypass;
	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass && IS_ENABLED(IRTA_CALL)) {
		mserr_hw("drc rdma setting mismatched : rdma %d, bypass %d\n", instance, hw_ip, cmd,
			bypass);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

#if IS_ENABLED(CONFIG_USE_YUVP_SEGCONF_RDMA)
	cmd = param_set->dma_input_seg.cmd;
	bypass = (!conf.ccm_contents_aware_isp_en && !conf.sharpen_contents_aware_isp_en &&
		  !conf.drc_contents_aware_isp_en);

	if ((cmd == DMA_INPUT_COMMAND_DISABLE) && !bypass && IS_ENABLED(IRTA_CALL)) {
		mserr_hw("seg_conf rdma setting mismatched : rdma %d, bypass %d\n", instance, hw_ip,
			cmd, bypass);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}
#endif

	wdma_max_cnt = yuvp_hw_g_wdma_max_cnt();
	for (i = 0; i < wdma_max_cnt; i++) {
		ret = __is_hw_yuvp_set_wdma(hw_ip, hw_yuvp, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_yuvp_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	if (param_region->yuvp.control.strgen == CONTROL_COMMAND_START) {
		msdbg_hw(2, "[YUVP]STRGEN input\n", instance, hw_ip);
		yuvp_hw_s_strgen(get_base(hw_ip), set_id);
	}

	pmio_cache_fsync(hw_ip->pmio, (void *)&clb, PMIO_FORMATTER_PAIR);
	if (clb.num_of_pairs > 0)
		clb.num_of_headers++;

	CALL_BUFOP(hw_yuvp->pb_c_loader_payload, sync_for_device, hw_yuvp->pb_c_loader_payload, 0,
		hw_yuvp->pb_c_loader_payload->size, DMA_TO_DEVICE);
	CALL_BUFOP(hw_yuvp->pb_c_loader_header, sync_for_device, hw_yuvp->pb_c_loader_header, 0,
		hw_yuvp->pb_c_loader_header->size, DMA_TO_DEVICE);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);

	_is_hw_yuvp_s_cmd(hw_yuvp, &clb, fcount, &frame_cfg.cmd);

	CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);

	if (unlikely(is_get_debug_param(IS_DEBUG_PARAM_YUVP)))
		yuvp_hw_dump(hw_ip->pmio, HW_DUMP_CR);

	set_bit(HW_CONFIG, &hw_ip->state);

shot_fail_recovery:
	CALL_PCC_OPS(pcc, set_qch, pcc, false);

	return ret;
}

static int is_hw_yuvp_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);

	return CALL_HW_HELPER_OPS(
		hw_ip, get_meta, frame->instance, frame, &hw_yuvp->lib[frame->instance]);
}

static int is_hw_yuvp_frame_ndone(
	struct is_hw_ip *hw_ip, struct is_frame *frame, enum ShotErrorType done_type)
{
	int ret = 0;

	sdbg_hw(4, "%s\n", hw_ip, __func__);

	if (test_bit(hw_ip->id, &frame->core_flag)) {
		ret = CALL_HW_OPS(
			hw_ip, frame_done, hw_ip, frame, -1, IS_HW_CORE_END, done_type, false);
	}

	return ret;
}

static int is_hw_yuvp_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);

	return CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, &hw_yuvp->lib[instance]);
}

static int is_hw_yuvp_apply_setfile(
	struct is_hw_ip *hw_ip, u32 scenario, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);

	return CALL_HW_HELPER_OPS(
		hw_ip, apply_setfile, instance, &hw_yuvp->lib[instance], scenario);
}

static int is_hw_yuvp_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);

	return CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, &hw_yuvp->lib[instance]);
}

static int is_hw_yuvp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);
	int ret;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	ret = CALL_HWIP_OPS(hw_ip, reset, instance);
	if (ret)
		return ret;

	if (pmio_reinit_cache(hw_ip->pmio, &hw_ip->pmio_config)) {
		pmio_cache_set_bypass(hw_ip->pmio, true);
		err("failed to reinit PMIO cache, set bypass");
		return -EINVAL;
	}

	is_hw_yuvp_prepare(hw_ip, instance);

	return CALL_HW_HELPER_OPS(hw_ip, restore, instance, &hw_yuvp->lib[instance]);
}

static int is_hw_yuvp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size)
{
	return CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
}

static int is_hw_yuvp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw_yuvp->pcc;
	u32 rdma_max_cnt, wdma_max_cnt;
	u32 i;
	int ret = 0;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		yuvp_hw_dump(hw_ip->pmio, HW_DUMP_CR);
		break;
	case IS_REG_DUMP_DMA:
		rdma_max_cnt = yuvp_hw_g_rdma_max_cnt();
		for (i = 0; i < rdma_max_cnt; i++) {
			dma = &hw_yuvp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		wdma_max_cnt = yuvp_hw_g_wdma_max_cnt();
		for (i = 0; i < wdma_max_cnt; i++) {
			dma = &hw_yuvp->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		ret = -EINVAL;
	}

	CALL_PCC_OPS(pcc, set_qch, pcc, false);
	return ret;
}

static int is_hw_yuvp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_yuvp *hw_yuvp;
	struct pablo_common_ctrl *pcc;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);
	hw_yuvp = GET_HW(hw_ip);
	if (!hw_yuvp) {
		mserr_hw("failed to get HW YUVP", instance, hw_ip);
		return -ENODEV;
	}

	pcc = hw_yuvp->pcc;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	yuvp_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);

	CALL_PCC_OPS(pcc, set_qch, pcc, false);

	return CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, &hw_yuvp->lib[instance]);
}

static size_t is_hw_yuvp_dump_params(struct is_hw_ip *hw_ip, u32 instance, char *buf, size_t size)
{
	struct is_hw_yuvp *hw_yuvp = GET_HW(hw_ip);
	struct yuvp_param_set *param;
	size_t rem = size;
	char *p = buf;

	param = &hw_yuvp->param_set[instance];

	p = pablo_json_nstr(p, "hw name", hw_ip->name, strlen(hw_ip->name), &rem);
	p = pablo_json_uint(p, "hw id", hw_ip->id, &rem);

	p = dump_param_otf_input(p, "otf_input", &param->otf_input, &rem);
	p = dump_param_otf_output(p, "otf_output", &param->otf_output, &rem);
	p = dump_param_dma_input(p, "dma_input", &param->dma_input, &rem);
	p = dump_param_otf_output(p, "fto_output", &param->fto_output, &rem);
	p = dump_param_stripe_input(p, "stripe_input", &param->stripe_input, &rem);
	p = dump_param_dma_input(p, "dma_input_seg", &param->dma_input_seg, &rem);
	p = dump_param_dma_input(p, "dma_input_drc", &param->dma_input_drc, &rem);
	p = dump_param_dma_input(p, "dma_input_clahe", &param->dma_input_clahe, &rem);
	p = dump_param_dma_input(p, "dma_input_pcchist", &param->dma_input_pcchist, &rem);
	p = dump_param_dma_output(p, "dma_output_pcchist", &param->dma_output_pcchist, &rem);

	p = pablo_json_uint(p, "instance_id", param->instance_id, &rem);
	p = pablo_json_uint(p, "fcount", param->fcount, &rem);
	p = pablo_json_uint(p, "tnr_mode", param->tnr_mode, &rem);
	p = pablo_json_uint(p, "mono_mode", param->mono_mode, &rem);
	p = pablo_json_bool(p, "reprocessing", param->reprocessing, &rem);

	return WRITTEN(size, rem);
}

const struct is_hw_ip_ops is_hw_yuvp_ops = {
	.open = is_hw_yuvp_open,
	.init = is_hw_yuvp_init,
	.deinit = is_hw_yuvp_deinit,
	.close = is_hw_yuvp_close,
	.enable = is_hw_yuvp_enable,
	.disable = is_hw_yuvp_disable,
	.shot = is_hw_yuvp_shot,
	.set_param = is_hw_yuvp_set_param,
	.get_meta = is_hw_yuvp_get_meta,
	.frame_ndone = is_hw_yuvp_frame_ndone,
	.load_setfile = is_hw_yuvp_load_setfile,
	.apply_setfile = is_hw_yuvp_apply_setfile,
	.delete_setfile = is_hw_yuvp_delete_setfile,
	.restore = is_hw_yuvp_restore,
	.set_regs = is_hw_yuvp_set_regs,
	.dump_regs = is_hw_yuvp_dump_regs,
	.set_config = is_hw_yuvp_set_config,
	.notify_timeout = is_hw_yuvp_notify_timeout,
	.reset = is_hw_yuvp_reset,
	.dump_params = is_hw_yuvp_dump_params,
};

int is_hw_yuvp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;
	int ret = 0;

	hw_ip->ops = &is_hw_yuvp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_yuvp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_yuvp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.name = "yuvp";

	hw_ip->pmio_config.mmio_base = hw_ip->regs[REG_SETA];
	hw_ip->pmio_config.phys_base = hw_ip->regs_start[REG_SETA];

	hw_ip->pmio_config.cache_type = PMIO_CACHE_NONE;

	yuvp_hw_init_pmio_config(&hw_ip->pmio_config);

	hw_ip->pmio = pmio_init(NULL, NULL, &hw_ip->pmio_config);
	if (IS_ERR(hw_ip->pmio)) {
		err("failed to init yuvp PMIO: %ld", PTR_ERR(hw_ip->pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(hw_ip->pmio, &hw_ip->pmio_fields, hw_ip->pmio_config.fields,
		hw_ip->pmio_config.num_fields);
	if (ret) {
		err("failed to alloc yuvp PMIO fields: %d", ret);
		pmio_exit(hw_ip->pmio);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(is_hw_yuvp_probe);

void is_hw_yuvp_remove(struct is_hw_ip *hw_ip)
{
	struct is_interface_ischain *itfc = hw_ip->itfc;
	int id = hw_ip->id;
	int hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = false;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = false;

	pmio_field_bulk_free(hw_ip->pmio, hw_ip->pmio_fields);
	pmio_exit(hw_ip->pmio);
}
EXPORT_SYMBOL_GPL(is_hw_yuvp_remove);
