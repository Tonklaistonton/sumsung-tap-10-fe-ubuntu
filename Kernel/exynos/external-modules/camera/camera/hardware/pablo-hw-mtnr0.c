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
#include "pablo-hw-mtnr0.h"
#include "api/is-hw-api-mtnr0-v13.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

#define GET_HW(hw_ip) ((struct is_hw_mtnr0 *)hw_ip->priv_info)

static inline void *get_base(struct is_hw_ip *hw_ip)
{
	return hw_ip->pmio;
}

static int param_debug_mtnr0_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] bit value, MTNR0 debug features\n"
				"\tb[0] : dump sfr\n"
				"\tb[1] : dump sfr once\n"
				"\tb[2] : s2d\n"
				"\tb[3] : skip ddk\n"
				"\tb[4] : bypass\n"
				"\tb[5] : DTP\n"
				"\tb[6] : TNR\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static struct pablo_debug_param debug_mtnr0 = {
	.type = IS_DEBUG_PARAM_TYPE_BIT,
	.max_value = 0x7F,
	.ops.usage = param_debug_mtnr0_usage,
};

module_param_cb(debug_mtnr0, &pablo_debug_param_ops, &debug_mtnr0, 0644);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
const struct kernel_param *is_hw_mtnr0_get_debug_kernel_param_kunit_wrapper(void)
{
	return G_KERNEL_PARAM(debug_mtnr0);
}
KUNIT_EXPORT_SYMBOL(is_hw_mtnr0_get_debug_kernel_param_kunit_wrapper);
#endif

void is_hw_mtnr0_s_debug_type(int type)
{
	set_bit(type, &debug_mtnr0.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_mtnr0_s_debug_type);

bool is_hw_mtnr0_check_debug_type(int type)
{
	return test_bit(type, &debug_mtnr0.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_mtnr0_check_debug_type);

void is_hw_mtnr0_c_debug_type(int type)
{
	clear_bit(type, &debug_mtnr0.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_mtnr0_c_debug_type);

static int is_hw_mtnr0_handle_interrupt(u32 id, void *context)
{
	struct is_hardware *hardware;
	struct is_hw_ip *hw_ip;
	struct is_hw_mtnr0 *hw_mtnr;
	struct pablo_common_ctrl *pcc;
	u32 status, instance, hw_fcount, hl = 0, vl = 0;
	u32 f_err;

	hw_ip = (struct is_hw_ip *)context;

	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		mserr_hw("invalid interrupt, hw_ip state(0x%lx)", instance, hw_ip, hw_ip->state);
		return 0;
	}

	hw_mtnr = GET_HW(hw_ip);
	pcc = hw_mtnr->pcc;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	hw_mtnr->irq_state[id] = status = CALL_PCC_OPS(pcc, get_int_status, pcc, id, true);

	msdbg_hw(2, "MTNR0 interrupt%d status(0x%x)\n", instance, hw_ip, id, status);

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

	if (mtnr0_hw_is_occurred(status, INTR_SETTING_DONE))
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

	if (mtnr0_hw_is_occurred(status, INTR_FRAME_START) &&
		mtnr0_hw_is_occurred(status, INTR_FRAME_END))
		mswarn_hw("start/end overlapped!! (0x%x)", instance, hw_ip, status);

	if (mtnr0_hw_is_occurred(status, INTR_FRAME_START)) {
		atomic_add(hw_ip->num_buffers, &hw_ip->count.fs);
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_FRAME_START);
		if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

		CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
	}

	if (mtnr0_hw_is_occurred(status, INTR_FRAME_END)) {
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

		if (unlikely(is_hw_mtnr0_check_debug_type(MTNR0_DBG_S2D)))
			is_debug_s2d(true, "MTNR0_DBG_S2D");
	}

	if (id == 0)
		f_err = mtnr0_hw_is_occurred(status, INTR_ERR);
	else
		f_err = mtnr0_hw_is_occurred1(status, INTR_ERR);

	if (f_err) {
		msinfo_hw("[F:%d] Ocurred error interrupt%d (%d,%d) status(0x%x)\n", instance,
			hw_ip, hw_fcount, id, hl, vl, status);

		if (id == 0) {
			mtnr0_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
			mtnr0_hw_dump(hw_ip->pmio, HW_DUMP_CR);
		}
	}

exit:
	CALL_PCC_OPS(pcc, set_qch, pcc, false);

	return 0;
}

static int is_hw_mtnr0_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mtnr0 *hw;
	struct pablo_common_ctrl *pcc;
	int i;

	for (i = 0; i < COREX_MAX; i++)
		hw_ip->cur_hw_iq_set[i].size = 0;

	hw = GET_HW(hw_ip);
	pcc = hw->pcc;

	return CALL_PCC_OPS(pcc, reset, pcc);
}

static int is_hw_mtnr0_wait_idle(struct is_hw_ip *hw_ip, u32 instance)
{
	return mtnr0_hw_wait_idle(get_base(hw_ip));
}

static int __is_hw_mtnr0_s_common_reg(struct is_hw_ip *hw_ip, u32 instance)
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
		mtnr0_hw_s_crc(get_base(hw_ip), seed);

	msinfo_hw("clear interrupt\n", instance, hw_ip);

	return 0;
}

static int __is_hw_mtnr0_clear_common(struct is_hw_ip *hw_ip, u32 instance)
{
	int res;
	struct is_hw_mtnr0 *hw_mtnr;

	hw_mtnr = GET_HW(hw_ip);

	if (CALL_HWIP_OPS(hw_ip, reset, instance)) {
		mserr_hw("sw reset fail", instance, hw_ip);
		return -ENODEV;
	}

	res = CALL_HWIP_OPS(hw_ip, wait_idle, instance);
	if (res)
		mserr_hw("failed to mtnr0_hw_wait_idle", instance, hw_ip);

	msinfo_hw("final finished mtnr\n", instance, hw_ip);

	return res;
}

static int is_hw_mtnr0_get_y_sbwc_type(u32 sbwc_type)
{
	if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
		return DMA_INPUT_SBWC_LOSSY_32B;
	else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
		return DMA_INPUT_SBWC_LOSSY_64B;
	else
		return sbwc_type;
}

static int is_hw_mtnr0_get_uv_sbwc_type(u32 sbwc_type)
{
	if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_32B)
		return DMA_INPUT_SBWC_LOSSYLESS_32B;
	else if (sbwc_type == DMA_INPUT_SBWC_LOSSY_CUSTOM_64B)
		return DMA_INPUT_SBWC_LOSSYLESS_64B;
	else
		return sbwc_type;
}

static int is_hw_mtnr0_get_buf_info(enum is_hw_mtnr0_subdev buf_id,
	struct pablo_internal_subdev *sd, struct mtnr_param_set *param_set,
	struct is_mtnr0_config *config, u32 scenario)
{
	u32 width = param_set->wdma_prev_l0.width;
	u32 height = param_set->wdma_prev_l0.height;
	u32 sbwc_type = param_set->wdma_prev_l0.sbwc_type;
	u32 i, sbwc_t, sbwc_en, comp_64b_align = 1, quality_control = 0;
	u32 payload_size, header_size;

	switch (buf_id) {
	case MTNR0_SUBDEV_PREV_YUV:
	case MTNR0_SUBDEV_YUV:
	case MTNR0_SUBDEV_PREV_YUV_2NR:
	case MTNR0_SUBDEV_YUV_2NR:
		if (config->mixerL0_still_en)
			return -EINVAL;

		sd->width = width;
		sd->height = height;
		sd->num_planes = 1;
		sd->num_batch = 1;
		sd->num_buffers = 1;
		sd->bits_per_pixel = config->imgL0_bit;
		sd->memory_bitwidth = config->imgL0_bit;

		for (i = 0; i < sd->num_planes; i++) {
			sbwc_t = (i == 0) ? is_hw_mtnr0_get_y_sbwc_type(sbwc_type) :
					    is_hw_mtnr0_get_uv_sbwc_type(sbwc_type);

			sbwc_en = is_hw_dma_get_comp_sbwc_en(sbwc_t, &comp_64b_align);

			if (IS_ENABLED(CONFIG_MTNR_32B_PA_ENABLE) && sbwc_en)
				comp_64b_align = 1;

			if (sbwc_en == COMP_LOSS)
				quality_control = SBWC_QUALITY_MODE;

			if (sbwc_en == NONE) {
				payload_size =
					is_hw_dma_get_img_stride(sd->bits_per_pixel,
						sd->bits_per_pixel, DMA_INOUT_FORMAT_Y,
						ALIGN(width, MTNR0_COMP_BLOCK_WIDTH), 16, true) *
					ALIGN(height, MTNR0_COMP_BLOCK_HEIGHT);
				header_size = 0;
			} else {
				payload_size =
					is_hw_dma_get_payload_stride(sbwc_en, sd->bits_per_pixel,
						width, comp_64b_align, quality_control,
						MTNR0_COMP_BLOCK_WIDTH, MTNR0_COMP_BLOCK_HEIGHT) *
					DIV_ROUND_UP(height, MTNR0_COMP_BLOCK_HEIGHT);

				header_size = is_hw_dma_get_header_stride(width,
								  MTNR0_COMP_BLOCK_WIDTH, 16) *
								  DIV_ROUND_UP(height,
									  MTNR0_COMP_BLOCK_HEIGHT);
			}

			sd->size[i] = payload_size + header_size;
		}

		sd->secure = (scenario == IS_SCENARIO_SECURE) ? true : false;
		break;
	case MTNR0_SUBDEV_PREV_W:
	case MTNR0_SUBDEV_W:
	case MTNR0_SUBDEV_PREV_W_2NR:
	case MTNR0_SUBDEV_W_2NR:
		sd->width = width;
		sd->height = height;
		sd->num_planes = 1;
		sd->num_batch = 1;
		sd->num_buffers = 1;
		sd->bits_per_pixel = 8;
		sd->memory_bitwidth = 8;
		sd->size[0] = ALIGN(width / 2, 16) * ((height / 2) + 4);
		break;
	default:
		mserr("invalid buf_id(%d)", sd, sd, buf_id);
		return -EINVAL;
	}

	return 0;
}

static void __is_hw_mtnr0_free_buffer(u32 instance, struct is_hw_ip *hw_ip)
{
	enum is_hw_mtnr0_subdev buf_id;
	struct is_hw_mtnr0 *hw_mtnr = GET_HW(hw_ip);
	struct pablo_internal_subdev *sd;

	for (buf_id = 0; buf_id < MTNR0_SUBDEV_END; buf_id++) {
		sd = &hw_mtnr->subdev[instance][buf_id];

		if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		if (CALL_I_SUBDEV_OPS(sd, free, sd))
			mserr_hw("[%s] failed to free", instance, hw_ip, sd->name);
	}
}

static int __is_hw_mtnr0_alloc_buffer(u32 instance, struct is_hw_ip *hw_ip,
	struct mtnr_param_set *param_set, struct is_mtnr0_config *config)
{
	int ret;
	int buf_id;
	struct pablo_internal_subdev *sd;
	struct is_hw_mtnr0 *hw = GET_HW(hw_ip);
	u32 scenario = hw_ip->region[instance]->parameter.sensor.config.scenario;

	for (buf_id = 0; buf_id < MTNR0_SUBDEV_END; buf_id++) {
		sd = &hw->subdev[instance][buf_id];

		if (test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		if (is_hw_mtnr0_get_buf_info(buf_id, sd, param_set, config, scenario))
			continue;

		ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
		if (ret) {
			mserr_hw("[%s] failed to alloc(%d)", instance, hw_ip, sd->name, ret);
			goto err_sd;
		}
	}

	return 0;

err_sd:
	while (buf_id-- > 0) {
		sd = &hw->subdev[instance][buf_id];
		if (CALL_I_SUBDEV_OPS(sd, free, sd))
			mserr_hw("[%s] failed to free", instance, hw_ip, sd->name);
	}

	return ret;
}

static void __is_hw_mtnr0_free_buffer_2nr(u32 instance, struct is_hw_ip *hw_ip)
{
	enum is_hw_mtnr0_subdev buf_id;
	struct is_hw_mtnr0 *hw_mtnr = GET_HW(hw_ip);
	struct pablo_internal_subdev *sd;

	for (buf_id = MTNR0_SUBDEV_PREV_YUV_2NR; buf_id < MTNR0_SUBDEV_MAX; buf_id++) {
		sd = &hw_mtnr->subdev[instance][buf_id];

		if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		if (CALL_I_SUBDEV_OPS(sd, free, sd))
			mserr_hw("[%s] failed to free", instance, hw_ip, sd->name);
	}
}

static int __is_hw_mtnr0_alloc_buffer_2nr(u32 instance, struct is_hw_ip *hw_ip,
	struct mtnr_param_set *param_set, struct is_mtnr0_config *config)
{
	int ret;
	enum is_hw_mtnr0_subdev buf_id;
	struct pablo_internal_subdev *sd;
	struct is_hw_mtnr0 *hw = GET_HW(hw_ip);
	u32 scenario = hw_ip->region[instance]->parameter.sensor.config.scenario;

	for (buf_id = MTNR0_SUBDEV_PREV_YUV_2NR; buf_id < MTNR0_SUBDEV_MAX; buf_id++) {
		sd = &hw->subdev[instance][buf_id];

		if (test_bit(PABLO_SUBDEV_ALLOC, &sd->state))
			continue;

		if (is_hw_mtnr0_get_buf_info(buf_id, sd, param_set, config, scenario))
			continue;

		ret = CALL_I_SUBDEV_OPS(sd, alloc, sd);
		if (ret) {
			mserr_hw("[%s] failed to alloc(%d)", instance, hw_ip, sd->name, ret);
			goto err_sd;
		}
	}

	return 0;

err_sd:
	__is_hw_mtnr0_free_buffer_2nr(instance, hw_ip);

	return ret;
}

static int __nocfi is_hw_mtnr0_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_mtnr0 *hw_mtnr;
	struct is_mem *mem;

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, "HWMTNR");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME, false);

	hw_ip->priv_info = pablo_zalloc(sizeof(struct is_hw_mtnr0), GFP_KERNEL);
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_mtnr = GET_HW(hw_ip);
	hw_mtnr->instance = instance;
	hw_mtnr->pcc = pablo_common_ctrl_hw_get_pcc(&hw_ip->pmio_config);

	ret = CALL_PCC_HW_OPS(
		hw_mtnr->pcc, init, hw_mtnr->pcc, hw_ip->pmio, hw_ip->name, PCC_M2M, NULL);
	if (ret) {
		mserr_hw("failed to pcc init. ret %d", instance, hw_ip, ret);
		goto err_pcc_init;
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_MTNR);

	hw_mtnr->pb_c_loader_payload = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x8000, NULL, 0);
	if (IS_ERR_OR_NULL(hw_mtnr->pb_c_loader_payload)) {
		hw_mtnr->pb_c_loader_payload = NULL;
		err("failed to allocate buffer for c-loader payload");
		ret = -ENOMEM;
		goto err_chain_create;
	}
	hw_mtnr->kva_c_loader_payload =
		CALL_BUFOP(hw_mtnr->pb_c_loader_payload, kvaddr, hw_mtnr->pb_c_loader_payload);
	hw_mtnr->dva_c_loader_payload =
		CALL_BUFOP(hw_mtnr->pb_c_loader_payload, dvaddr, hw_mtnr->pb_c_loader_payload);

	hw_mtnr->pb_c_loader_header = CALL_PTR_MEMOP(mem, alloc, mem->priv, 0x2000, NULL, 0);
	if (IS_ERR_OR_NULL(hw_mtnr->pb_c_loader_header)) {
		hw_mtnr->pb_c_loader_header = NULL;
		err("failed to allocate buffer for c-loader header");
		ret = -ENOMEM;
		goto err_chain_create;
	}
	hw_mtnr->kva_c_loader_header =
		CALL_BUFOP(hw_mtnr->pb_c_loader_header, kvaddr, hw_mtnr->pb_c_loader_header);
	hw_mtnr->dva_c_loader_header =
		CALL_BUFOP(hw_mtnr->pb_c_loader_header, dvaddr, hw_mtnr->pb_c_loader_header);

	msdbg_hw(2, "c_loader_header kva(%lx), dva(%llx)", instance, hw_ip,
		hw_mtnr->kva_c_loader_header, hw_mtnr->dva_c_loader_header);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_chain_create:
	CALL_PCC_HW_OPS(hw_mtnr->pcc, deinit, hw_mtnr->pcc);
err_pcc_init:
	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_mtnr0_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	int ret;
	struct is_hw_mtnr0 *hw_mtnr;
	u32 input_id;
	enum is_hw_mtnr0_subdev buf_id;
	struct is_mem *mem;

	hw_mtnr = GET_HW(hw_ip);
	if (!hw_mtnr) {
		mserr_hw("hw_mtnr is null ", instance, hw_ip);
		ret = -ENODATA;
		goto err;
	}

	for (input_id = MTNR0_RDMA_CUR_L0_Y; input_id < MTNR0_RDMA_MAX; input_id++) {
		ret = mtnr0_hw_rdma_create(&hw_mtnr->rdma[input_id], get_base(hw_ip), input_id);
		if (ret) {
			mserr_hw("mtnr0_hw_rdma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	for (input_id = MTNR0_WDMA_PREV_L0_Y; input_id < MTNR0_WDMA_MAX; input_id++) {
		ret = mtnr0_hw_wdma_create(&hw_mtnr->wdma[input_id], get_base(hw_ip), input_id);
		if (ret) {
			mserr_hw("mtnr0_hw_wdma_create error[%d]", instance, hw_ip, input_id);
			ret = -ENODATA;
			goto err;
		}
	}

	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_MTNR);
	for (buf_id = 0; buf_id < MTNR0_SUBDEV_MAX; buf_id++)
		pablo_internal_subdev_probe(&hw_mtnr->subdev[instance][buf_id], instance, mem,
			mtnr0_internal_buf_name[buf_id]);

	set_bit(HW_INIT, &hw_ip->state);
	return 0;

err:
	return ret;
}

static int is_hw_mtnr0_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	return 0;
}

static int is_hw_mtnr0_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mtnr0 *hw_mtnr = NULL;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_mtnr = GET_HW(hw_ip);

	__is_hw_mtnr0_clear_common(hw_ip, instance);

	CALL_BUFOP(hw_mtnr->pb_c_loader_payload, free, hw_mtnr->pb_c_loader_payload);
	CALL_BUFOP(hw_mtnr->pb_c_loader_header, free, hw_mtnr->pb_c_loader_header);

	CALL_PCC_HW_OPS(hw_mtnr->pcc, deinit, hw_mtnr->pcc);

	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return 0;
}

static int is_hw_mtnr0_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_mmio *pmio;
	struct is_hw_mtnr0 *hw_mtnr;
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

	hw_mtnr = GET_HW(hw_ip);
	pcc = hw_mtnr->pcc;
	pmio = hw_ip->pmio;

	hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
	if (pmio_reinit_cache(pmio, &hw_ip->pmio_config)) {
		pmio_cache_set_bypass(pmio, true);
		err("failed to reinit PMIO cache, set bypass");
		return -EINVAL;
	}
	__is_hw_mtnr0_s_common_reg(hw_ip, instance);

	cfg.fs_mode = PCC_ASAP;
	mtnr0_hw_g_int_en(cfg.int_en);
	CALL_PCC_OPS(pcc, enable, pcc, &cfg);

	pmio_cache_set_only(pmio, true);

	memset(&hw_mtnr->config[instance], 0x0, sizeof(struct is_mtnr0_config));
	memset(&hw_mtnr->param_set[instance], 0x0, sizeof(struct mtnr_param_set));

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

static int is_hw_mtnr0_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	long timetowait;
	struct is_hw_mtnr0 *hw_mtnr;
	struct pablo_common_ctrl *pcc;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw(
		"mtnr0_disable: Vvalid(%d)\n", instance, hw_ip, atomic_read(&hw_ip->status.Vvalid));

	hw_mtnr = GET_HW(hw_ip);

	timetowait = wait_event_timeout(
		hw_ip->status.wait_queue, !atomic_read(&hw_ip->status.Vvalid), IS_HW_STOP_TIMEOUT);

	if (!timetowait)
		mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip, timetowait);

	__is_hw_mtnr0_free_buffer(instance, hw_ip);
	__is_hw_mtnr0_free_buffer_2nr(instance, hw_ip);

	if (hw_ip->run_rsc_state)
		return 0;

	pcc = hw_mtnr->pcc;
	CALL_PCC_OPS(pcc, disable, pcc);

	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);

	return ret;
}

static void __is_hw_mtnr0_set_prev_dma_cmd(
	struct mtnr_param_set *param_set, u32 instance, struct is_mtnr0_config *conf)
{
	if (conf->mixerL0_en) {
		if (conf->mixerL0_still_en) {
			/* MF-Still */
			param_set->rdma_prev_l0_wgt.cmd = DMA_INPUT_COMMAND_ENABLE;
			param_set->wdma_prev_l0_wgt.cmd = DMA_OUTPUT_COMMAND_ENABLE;
		} else {
			/* Video TNR */
			param_set->wdma_prev_l0.cmd = DMA_OUTPUT_COMMAND_ENABLE;
			switch (conf->mixerL0_mode) {
			case MTNR0_TNR_MODE_PREPARE:
				param_set->rdma_mv_geomatch.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->rdma_prev_l0.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->rdma_prev_l0_wgt.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->wdma_prev_l0_wgt.cmd = DMA_OUTPUT_COMMAND_DISABLE;
				break;
			case MTNR0_TNR_MODE_FIRST:
				param_set->rdma_prev_l0.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->rdma_prev_l0_wgt.cmd = DMA_INPUT_COMMAND_DISABLE;
				param_set->wdma_prev_l0_wgt.cmd = DMA_OUTPUT_COMMAND_ENABLE;
				break;
			case MTNR0_TNR_MODE_NORMAL:
				param_set->rdma_prev_l0.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->rdma_prev_l0_wgt.cmd = DMA_INPUT_COMMAND_ENABLE;
				param_set->wdma_prev_l0_wgt.cmd = DMA_OUTPUT_COMMAND_ENABLE;
				break;
			default:
				break;
			}

			/* 1/2 dma */
			if (conf->skip_wdma && (conf->mixerL0_mode == MTNR0_TNR_MODE_NORMAL)) {
				param_set->wdma_prev_l0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
				param_set->wdma_prev_l0_wgt.cmd = DMA_OUTPUT_COMMAND_DISABLE;
			}
		}
	} else {
		param_set->rdma_prev_l0.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->rdma_prev_l0_wgt.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->rdma_mv_geomatch.cmd = DMA_INPUT_COMMAND_DISABLE;
		param_set->wdma_prev_l0.cmd = DMA_OUTPUT_COMMAND_DISABLE;
		param_set->wdma_prev_l0_wgt.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	}
}

static void __is_hw_mtnr0_set_cur_dma_cmd(
	struct mtnr_param_set *param_set, u32 instance, struct is_mtnr0_config *conf)
{
	if (conf->mixerL0_en) {
		if (conf->mixerL0_still_en) {
			/* MF-Still */
			param_set->rdma_cur_l4.cmd = DMA_INPUT_COMMAND_ENABLE;
		} else {
			/* Video TNR */
			switch (conf->mixerL0_mode) {
			case MTNR0_TNR_MODE_PREPARE:
				param_set->rdma_cur_l4.cmd = DMA_INPUT_COMMAND_DISABLE;
				break;
			case MTNR0_TNR_MODE_FIRST:
				param_set->rdma_cur_l4.cmd = DMA_INPUT_COMMAND_ENABLE;
				break;
			case MTNR0_TNR_MODE_NORMAL:
				param_set->rdma_cur_l4.cmd = DMA_INPUT_COMMAND_ENABLE;
				break;
			default:
				break;
			}
		}
	} else {
		param_set->rdma_cur_l4.cmd = DMA_INPUT_COMMAND_DISABLE;
	}
}

static void __is_hw_mtnr0_set_cin_cmd(
	struct mtnr_param_set *param_set, u32 instance, struct is_mtnr0_config *conf)
{
	if (conf->mixerL0_en) {
		if (conf->mixerL0_still_en) {
			/* MF-Still */
			param_set->cin_mtnr1_wgt.cmd = OTF_INPUT_COMMAND_ENABLE;
		} else {
			/* Video TNR */
			switch (conf->mixerL0_mode) {
			case MTNR0_TNR_MODE_PREPARE:
				param_set->cin_mtnr1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE;
				break;
			case MTNR0_TNR_MODE_FIRST:
				param_set->cin_mtnr1_wgt.cmd = OTF_INPUT_COMMAND_ENABLE;
				break;
			case MTNR0_TNR_MODE_NORMAL:
				param_set->cin_mtnr1_wgt.cmd = OTF_INPUT_COMMAND_ENABLE;
				break;
			default:
				break;
			}
		}
	} else {
		param_set->cin_mtnr1_wgt.cmd = OTF_INPUT_COMMAND_DISABLE;
	}
}

static int __is_hw_mtnr0_set_rdma(struct is_hw_ip *hw_ip, struct is_hw_mtnr0 *hw_mtnr,
	struct mtnr_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	pdma_addr_t *input_dva;
	u32 comp_sbwc_en, payload_size, strip_offset = 0, header_offset = 0;
	struct param_dma_input *dma_input;
	u32 frame_width, frame_height;
	int ret;

	frame_width = param_set->rdma_cur_l0.width;
	frame_height = param_set->rdma_cur_l0.height;

	switch (id) {
	case MTNR0_RDMA_CUR_L0_Y:
		input_dva = param_set->input_dva_cur_l0;
		dma_input = &param_set->rdma_cur_l0;
		break;
	case MTNR0_RDMA_CUR_L4_Y:
		input_dva = param_set->input_dva_cur_l4;
		dma_input = &param_set->rdma_cur_l4;
		frame_width = param_set->rdma_cur_l4.width;
		frame_height = param_set->rdma_cur_l4.height;
		break;
	case MTNR0_RDMA_PREV_L0_Y:
	case MTNR0_RDMA_PREV_L0_Y_1:
		input_dva = param_set->input_dva_prev_l0;
		dma_input = &param_set->rdma_prev_l0;
		break;
	case MTNR0_RDMA_PREV_L0_WGT:
		input_dva = param_set->input_dva_prev_l0_wgt;
		dma_input = &param_set->rdma_prev_l0_wgt;
		break;
	case MTNR0_RDMA_MV_GEOMATCH:
		input_dva = param_set->input_dva_mv_geomatch;
		dma_input = &param_set->rdma_mv_geomatch;
		break;
	case MTNR0_RDMA_SEG_L0:
		input_dva = param_set->input_dva_seg_l0;
		dma_input = &param_set->rdma_seg_l0;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_mtnr->rdma[id].name, dma_input->cmd);

	mtnr0_hw_s_dma_corex_id(&hw_mtnr->rdma[id], set_id);

	ret = mtnr0_hw_s_rdma_init(&hw_mtnr->rdma[id], dma_input, &param_set->stripe_input,
		frame_width, frame_height, &comp_sbwc_en, &payload_size, &strip_offset,
		&header_offset, &hw_mtnr->config[instance]);
	if (ret) {
		mserr_hw("failed to initialize MTNR0_RDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (dma_input->cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = mtnr0_hw_s_rdma_addr(&hw_mtnr->rdma[id], input_dva, 0, hw_ip->num_buffers, 0,
			comp_sbwc_en, payload_size, strip_offset, header_offset);
		if (ret) {
			mserr_hw("failed to set MTNR0_RDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_mtnr0_set_wdma(struct is_hw_ip *hw_ip, struct is_hw_mtnr0 *hw_mtnr,
	struct mtnr_param_set *param_set, u32 instance, u32 id, u32 set_id)
{
	pdma_addr_t *output_dva;
	u32 comp_sbwc_en, payload_size, strip_offset = 0, header_offset = 0;
	struct param_dma_output *dma_output;
	u32 frame_width, frame_height;
	int ret;

	frame_width = param_set->rdma_cur_l0.width;
	frame_height = param_set->rdma_cur_l0.height;

	switch (id) {
	case MTNR0_WDMA_PREV_L0_Y:
		output_dva = param_set->output_dva_prev_l0;
		dma_output = &param_set->wdma_prev_l0;
		break;
	case MTNR0_WDMA_PREV_L0_WGT:
		output_dva = param_set->output_dva_prev_l0_wgt;
		dma_output = &param_set->wdma_prev_l0_wgt;
		break;
	default:
		merr_hw("invalid ID (%d)", instance, id);
		return -EINVAL;
	}

	msdbg_hw(2, "%s: %d\n", instance, hw_ip, hw_mtnr->wdma[id].name, dma_output->cmd);

	mtnr0_hw_s_dma_corex_id(&hw_mtnr->wdma[id], set_id);

	ret = mtnr0_hw_s_wdma_init(&hw_mtnr->wdma[id], dma_output, &param_set->stripe_input,
		frame_width, frame_height, &comp_sbwc_en, &payload_size, &strip_offset,
		&header_offset, &hw_mtnr->config[instance]);
	if (ret) {
		mserr_hw("failed to initialize MTNR0_WDMA(%d)", instance, hw_ip, id);
		return -EINVAL;
	}

	if (dma_output->cmd == DMA_INPUT_COMMAND_ENABLE) {
		ret = mtnr0_hw_s_wdma_addr(&hw_mtnr->wdma[id], output_dva, 0, hw_ip->num_buffers, 0,
			comp_sbwc_en, payload_size, strip_offset, header_offset);
		if (ret) {
			mserr_hw("failed to set MTNR0_WDMA(%d) address", instance, hw_ip, id);
			return -EINVAL;
		}
	}

	return 0;
}

static int __is_hw_mtnr0_bypass(struct is_hw_ip *hw_ip, u32 set_id)
{
	mtnr0_hw_s_block_bypass(get_base(hw_ip), set_id);

	return 0;
}

static int __is_hw_mtnr0_update_block_reg(
	struct is_hw_ip *hw_ip, struct mtnr_param_set *param_set, u32 instance, u32 set_id)
{
	struct is_hw_mtnr0 *hw_mtnr;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	hw_mtnr = GET_HW(hw_ip);

	if (hw_mtnr->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n", instance, hw_ip,
			hw_mtnr->instance);
		hw_mtnr->instance = instance;
	}

	__is_hw_mtnr0_bypass(hw_ip, set_id);

	if (unlikely(is_hw_mtnr0_check_debug_type(MTNR0_DBG_TNR))) {
		mtnr0_hw_debug_s_geomatch_mode(
			get_base(hw_ip), set_id, hw_mtnr->config[instance].mixerL0_mode);
		mtnr0_hw_debug_s_mixer_mode(
			get_base(hw_ip), set_id, hw_mtnr->config[instance].mixerL0_mode);
		info("[TNR] set geomatch & mixer\n");
	}

	return 0;
}

static void __is_hw_mtnr0_update_param(struct is_hw_ip *hw_ip, struct is_param_region *p_region,
	struct mtnr_param_set *param_set, IS_DECLARE_PMAP(pmap), u32 instance)
{
	struct mtnr_param *param;

	param = &p_region->mtnr;
	param_set->instance_id = instance;

	param_set->mono_mode = hw_ip->region[instance]->parameter.sensor.config.mono_mode;

	if (test_bit(PARAM_MTNR_CIN_MTNR1_WGT, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_CIN_MTNR1_WGT\n", instance, hw_ip);
		memcpy(&param_set->cin_mtnr1_wgt, &param->cin_mtnr1_wgt,
			sizeof(struct param_otf_input));
	}

	if (test_bit(PARAM_MTNR_COUT_MSNR_L0, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_COUT_MSNR_L0\n", instance, hw_ip);
		memcpy(&param_set->cout_msnr_l0, &param->cout_msnr_l0,
			sizeof(struct param_otf_output));
	}

	if (test_bit(PARAM_MTNR_RDMA_CUR_L0, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_RDMA_CUR_L0\n", instance, hw_ip);
		memcpy(&param_set->rdma_cur_l0, &param->rdma_cur_l0,
			sizeof(struct param_dma_input));
	}

	if (test_bit(PARAM_MTNR_RDMA_CUR_L1, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_RDMA_CUR_L1\n", instance, hw_ip);
		memcpy(&param_set->rdma_cur_l1, &param->rdma_cur_l1,
			sizeof(struct param_dma_input));
	}

	if (test_bit(PARAM_MTNR_RDMA_CUR_L4, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_RDMA_CUR_L4\n", instance, hw_ip);
		memcpy(&param_set->rdma_cur_l4, &param->rdma_cur_l4,
			sizeof(struct param_dma_input));
	}

	if (test_bit(PARAM_MTNR_RDMA_PREV_L0, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_RDMA_PREV_L0\n", instance, hw_ip);
		memcpy(&param_set->rdma_prev_l0, &param->rdma_prev_l0,
			sizeof(struct param_dma_input));
	}

	if (test_bit(PARAM_MTNR_RDMA_PREV_L0_WGT, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_RDMA_PREV_L0_WGT\n", instance, hw_ip);
		memcpy(&param_set->rdma_prev_l0_wgt, &param->rdma_prev_l0_wgt,
			sizeof(struct param_dma_input));
	}

	if (test_bit(PARAM_MTNR_RDMA_MV_GEOMATCH, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_RDMA_MV_GEOMATCH\n", instance, hw_ip);
		memcpy(&param_set->rdma_mv_geomatch, &param->rdma_mv_geomatch,
			sizeof(struct param_dma_input));
	}

	if (test_bit(PARAM_MTNR_RDMA_SEG_L0, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_RDMA_SEG_L0\n", instance, hw_ip);
		memcpy(&param_set->rdma_seg_l0, &param->rdma_seg_l0,
			sizeof(struct param_dma_input));
	}

	if (test_bit(PARAM_MTNR_WDMA_PREV_L0, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_WDMA_PREV_L0\n", instance, hw_ip);
		memcpy(&param_set->wdma_prev_l0, &param->wdma_prev_l0,
			sizeof(struct param_dma_output));
	}

	if (test_bit(PARAM_MTNR_WDMA_PREV_L0_WGT, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_WDMA_PREV_L0_WGT\n", instance, hw_ip);
		memcpy(&param_set->wdma_prev_l0_wgt, &param->wdma_prev_l0_wgt,
			sizeof(struct param_dma_output));
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && test_bit(PARAM_MTNR_STRIPE_INPUT, pmap)) {
		msdbg_hw(2, "PARAM_MTNR_STRIPE_INPUT\n", instance, hw_ip);
		memcpy(&param_set->stripe_input, &param->stripe_input,
			sizeof(struct param_stripe_input));
	}
}

static int is_hw_mtnr0_set_param(struct is_hw_ip *hw_ip, struct is_region *region,
	IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map)
{
	struct is_hw_mtnr0 *hw_mtnr;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;

	hw_mtnr = GET_HW(hw_ip);
	hw_mtnr->instance = IS_STREAM_COUNT;

	return 0;
}

#define DIV_ALIGN_UP(_n, _d) ALIGN(DIV_ROUND_UP(_n, _d), _d)
#define __is_hw_mtnr0_verify_size(_p, _c)                                                          \
	({                                                                                         \
		u32 _ew = DIV_ALIGN_UP(_p.w, 2);                                                   \
		u32 _eh = DIV_ALIGN_UP(_p.h, 2);                                                   \
		int _ret = 0;                                                                      \
		if (_ew != _c.w || _eh != _c.h) {                                                  \
			mserr_hw("invalid size: " #_p "(%dx%d), " #_c                              \
				 "(%dx%d) vs expected (%dx%d)",                                    \
				instance, hw_ip, _p.w, _p.h, _c.w, _c.h, _ew, _eh);                \
			_ret = -EINVAL;                                                            \
		}                                                                                  \
		_ret;                                                                              \
	})

static int __is_hw_mtnr0_set_size_regs(struct is_hw_ip *hw_ip, struct mtnr_param_set *param_set,
	u32 instance, const struct is_frame *frame, u32 set_id,
	struct pablo_common_ctrl_frame_cfg *frame_cfg)
{
	struct is_hw_mtnr0 *hw_mtnr;
	struct is_mtnr0_config *mtnr_config;
	u32 strip_enable;
	u32 crop_img_width, crop_wgt_width;
	u32 crop_img_x, crop_wgt_x;
	u32 crop_img_start_x;
	u32 crop_margin_for_align;
	struct {
		struct is_rectangle l0;
		struct is_rectangle l1;
		struct is_rectangle l4;
	} dma_size = {
		0,
	};
	u32 frame_width;
	u32 strip_start_pos;
	u32 stripe_index;
	u32 region_id;
	u32 img_shift_bit;
	u32 wgt_shift_bit;
	struct mtnr0_radial_cfg radial_cfg;
	bool mono_mode_en;
	u32 otf_crop_bypass = 1;

	hw_mtnr = GET_HW(hw_ip);
	mtnr_config = &hw_mtnr->config[instance];

	strip_enable = (param_set->stripe_input.total_count < 2) ? 0 : 1;
	mono_mode_en = param_set->mono_mode;

	region_id = param_set->stripe_input.index;
	frame_width = param_set->stripe_input.full_width;
	dma_size.l0.w = param_set->rdma_cur_l0.width;
	dma_size.l0.h = param_set->rdma_cur_l0.height;
	dma_size.l1.w = param_set->rdma_cur_l1.width;
	dma_size.l1.h = param_set->rdma_cur_l1.height;
	dma_size.l4.w = param_set->rdma_cur_l4.width;
	dma_size.l4.h = param_set->rdma_cur_l4.height;
	frame_width = (strip_enable) ? param_set->stripe_input.full_width : dma_size.l0.w;
	stripe_index = param_set->stripe_input.index;
	strip_start_pos = (stripe_index) ? (param_set->stripe_input.start_pos_x) : 0;

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

	mtnr0_hw_s_otf_input_mtnr1_wgt(
		get_base(hw_ip), set_id, param_set->cin_mtnr1_wgt.cmd, frame_cfg);
	mtnr0_hw_s_otf_output_msnr_l0(
		get_base(hw_ip), set_id, param_set->cout_msnr_l0.cmd, frame_cfg);

	mtnr0_hw_s_input_size_l0(get_base(hw_ip), set_id, dma_size.l0.w, dma_size.l0.h);
	if (__is_hw_mtnr0_verify_size(dma_size.l0, dma_size.l1))
		return -EINVAL;
	mtnr0_hw_s_input_size_l1(get_base(hw_ip), set_id, dma_size.l1.w, dma_size.l1.h);
	mtnr0_hw_s_input_size_l4(get_base(hw_ip), set_id, dma_size.l4.w, dma_size.l4.h);
	mtnr0_hw_s_geomatch_size(get_base(hw_ip), set_id, frame_width, dma_size.l0.w, dma_size.l0.h,
		strip_enable, strip_start_pos, mtnr_config);
	mtnr0_hw_s_mixer_size(get_base(hw_ip), set_id, frame_width, dma_size.l0.w, dma_size.l0.h,
		strip_enable, strip_start_pos, &radial_cfg, mtnr_config);

	/* set crop info for non-strip processing case */
	crop_img_start_x = 0;
	crop_img_x = 0;
	crop_img_width = dma_size.l0.w;
	crop_wgt_x = 0;
	crop_wgt_width = crop_img_width >> 1;

#if defined(MTNR0_STRIP_OTF_MARGIN_CROP)
	if (strip_enable) {
		otf_crop_bypass = 0;

		if (param_set->stripe_input.left_margin > 0 &&
			crop_img_width > MTNR0_STRIP_OTF_MARGIN_CROP) {
			crop_img_x = MTNR0_STRIP_OTF_MARGIN_CROP;
			crop_img_width -= MTNR0_STRIP_OTF_MARGIN_CROP;
		}

		if (param_set->stripe_input.right_margin > 0 &&
			crop_img_width > MTNR0_STRIP_OTF_MARGIN_CROP)
			crop_img_width -= MTNR0_STRIP_OTF_MARGIN_CROP;

		crop_wgt_x = crop_img_x >> 1;
		crop_wgt_width = crop_img_width >> 1;
	}

	msdbgs_hw(3, "[otf] in_crop_width %d, crop_img_x %d, crop_img_width %d\n", instance, hw_ip,
		param_set->rdma_cur_l0.dma_crop_width, crop_img_x, crop_img_width);
#endif

	mtnr0_hw_s_crop_clean_img_otf(get_base(hw_ip), set_id, crop_img_x, crop_img_width,
		dma_size.l0.h, otf_crop_bypass);
	if (strip_enable) {
		crop_img_start_x = ALIGN_DOWN(strip_start_pos + param_set->stripe_input.left_margin,
			MTNR0_COMP_BLOCK_WIDTH * 2);
		crop_margin_for_align = ZERO_IF_NEG(strip_start_pos +
				param_set->stripe_input.left_margin - crop_img_start_x);
		crop_img_x = ZERO_IF_NEG(param_set->stripe_input.left_margin - crop_margin_for_align);
		crop_img_width = ZERO_IF_NEG(dma_size.l0.w - param_set->stripe_input.left_margin -
				 param_set->stripe_input.right_margin + crop_margin_for_align);

		crop_wgt_x = crop_img_x >> 1;
		crop_wgt_width = crop_img_width >> 1;
	}

	param_set->wdma_prev_l0.dma_crop_offset_x = crop_img_start_x;
	param_set->wdma_prev_l0.dma_crop_width = crop_img_width;

	param_set->wdma_prev_l0_wgt.dma_crop_offset_x = crop_img_start_x;
	param_set->wdma_prev_l0_wgt.dma_crop_width = crop_img_width;

	param_set->rdma_cur_l0.dma_crop_offset = strip_start_pos;
	param_set->rdma_cur_l0.dma_crop_width = crop_img_width +
						param_set->stripe_input.left_margin +
						param_set->stripe_input.right_margin;

	msdbgs_hw(3, "in_crop(ofs:%d, width:%d), out_crop(ofs:%d,%d, width:%d), margin(%d, %d)\n",
		instance, hw_ip, strip_start_pos, param_set->rdma_cur_l0.dma_crop_width, crop_img_x,
		crop_img_start_x, crop_img_width, param_set->stripe_input.left_margin,
		param_set->stripe_input.right_margin);

	mtnr0_hw_s_crop_clean_img_dma(
		get_base(hw_ip), set_id, crop_img_x, crop_img_width, dma_size.l0.h, !strip_enable);
	mtnr0_hw_s_crop_wgt_dma(get_base(hw_ip), set_id, crop_wgt_x, crop_wgt_width,
		dma_size.l0.h >> 1, !strip_enable);

	img_shift_bit = ZERO_IF_NEG(DMA_INOUT_BIT_WIDTH_12BIT - mtnr_config->imgL0_bit);
	wgt_shift_bit = ZERO_IF_NEG(DMA_INOUT_BIT_WIDTH_8BIT - mtnr_config->wgtL0_bit);
	mtnr0_hw_s_img_bitshift(get_base(hw_ip), set_id, img_shift_bit);
	mtnr0_hw_s_mono_mode(get_base(hw_ip), set_id, mono_mode_en);

	mtnr0_hw_s_l0_bypass(get_base(hw_ip), mtnr_config->L0_bypass);

	mtnr0_hw_s_mvf_resize_offset(get_base(hw_ip), set_id,
			mtnr_config->mvc_in_w, mtnr_config->mvc_in_h,
			mtnr_config->mvc_out_w, mtnr_config->mvc_out_h, strip_start_pos);

	return 0;
}

static void is_hw_mtnr0_set_wgt_i_buffer(int instance, struct is_hw_ip *hw_ip,
	struct is_hw_mtnr0 *hw_mtnr, struct is_frame *frame, bool swap_frame, bool swap_fro,
	u32 p_idx, u32 c_idx)
{
	struct is_frame *i_frame_p, *i_frame_c;
	struct is_framemgr *i_fmgr_p, *i_fmgr_c;
	struct mtnr_param_set *param_set = &hw_mtnr->param_set[instance];

	i_fmgr_p = GET_SUBDEV_I_FRAMEMGR(&hw_mtnr->subdev[instance][p_idx]);
	i_fmgr_c = GET_SUBDEV_I_FRAMEMGR(&hw_mtnr->subdev[instance][c_idx]);

	i_frame_p = get_frame(i_fmgr_p, FS_FREE);
	i_frame_c = get_frame(i_fmgr_c, FS_FREE);
	if (!i_frame_p || !i_frame_c) {
		mswarn_hw("There is no FREE frames. pre(%d), cur(%d)\n", instance, hw_ip,
			i_fmgr_p->queued_count[FS_FREE], i_fmgr_c->queued_count[FS_FREE]);
		return;
	}

	if (swap_frame) {
		put_frame(i_fmgr_c, i_frame_p, FS_FREE);
		put_frame(i_fmgr_p, i_frame_c, FS_FREE);
	} else {
		put_frame(i_fmgr_p, i_frame_p, FS_FREE);
		put_frame(i_fmgr_c, i_frame_c, FS_FREE);
	}

	if (swap_fro) {
		/* HW FRO */
		SET_MTNR0_MUTLI_BUFFER_ADDR_SWAP(frame->num_buffers, i_frame_p->planes,
			i_frame_p->dvaddr_buffer, i_frame_c->dvaddr_buffer);
	} else {
		SET_MTNR0_MUTLI_BUFFER_ADDR(
			frame->num_buffers, i_frame_p->planes, i_frame_p->dvaddr_buffer);
		SET_MTNR0_MUTLI_BUFFER_ADDR(
			frame->num_buffers, i_frame_c->planes, i_frame_c->dvaddr_buffer);
	}

	/* prev wgt plane */
	param_set->rdma_prev_l0_wgt.plane = i_frame_p->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "prev_wgt_i", hw_ip, frame, 0, frame->num_buffers,
		&param_set->rdma_prev_l0_wgt.cmd, param_set->rdma_prev_l0_wgt.plane,
		param_set->input_dva_prev_l0_wgt, i_frame_p->dvaddr_buffer);

	/* out wgt */
	param_set->wdma_prev_l0_wgt.plane = i_frame_c->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "wgt_out_i", hw_ip, frame, 0, frame->num_buffers,
		&param_set->wdma_prev_l0_wgt.cmd, param_set->wdma_prev_l0_wgt.plane,
		param_set->output_dva_prev_l0_wgt, i_frame_c->dvaddr_buffer);
}

static void is_hw_mtnr0_set_yuv_i_buffer(int instance, struct is_hw_ip *hw_ip,
	struct is_hw_mtnr0 *hw_mtnr, struct is_frame *frame, bool swap_frame, bool swap_fro,
	u32 p_idx, u32 c_idx)
{
	struct is_frame *i_frame_p, *i_frame_c;
	struct is_framemgr *i_fmgr_p, *i_fmgr_c;
	struct mtnr_param_set *param_set = &hw_mtnr->param_set[instance];

	i_fmgr_p = GET_SUBDEV_I_FRAMEMGR(&hw_mtnr->subdev[instance][p_idx]);
	i_fmgr_c = GET_SUBDEV_I_FRAMEMGR(&hw_mtnr->subdev[instance][c_idx]);

	i_frame_p = get_frame(i_fmgr_p, FS_FREE);
	i_frame_c = get_frame(i_fmgr_c, FS_FREE);
	if (!i_frame_p || !i_frame_c) {
		mswarn_hw("There is no FREE frames. pre(%d), cur(%d)\n", instance, hw_ip,
			i_fmgr_p->queued_count[FS_FREE], i_fmgr_c->queued_count[FS_FREE]);
		return;
	}

	if (swap_frame) {
		put_frame(i_fmgr_c, i_frame_p, FS_FREE);
		put_frame(i_fmgr_p, i_frame_c, FS_FREE);
	} else {
		put_frame(i_fmgr_p, i_frame_p, FS_FREE);
		put_frame(i_fmgr_c, i_frame_c, FS_FREE);
	}

	if (swap_fro) {
		/* HW FRO */
		SET_MTNR0_MUTLI_BUFFER_ADDR_SWAP(frame->num_buffers, i_frame_p->planes,
			i_frame_p->dvaddr_buffer, i_frame_c->dvaddr_buffer);
	} else {
		SET_MTNR0_MUTLI_BUFFER_ADDR(
			frame->num_buffers, i_frame_p->planes, i_frame_p->dvaddr_buffer);
		SET_MTNR0_MUTLI_BUFFER_ADDR(
			frame->num_buffers, i_frame_c->planes, i_frame_c->dvaddr_buffer);
	}

	param_set->rdma_prev_l0.sbwc_type = param_set->wdma_prev_l0.sbwc_type;
	/* prev yuv */
	param_set->rdma_prev_l0.plane = i_frame_p->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "prev_yuv_i", hw_ip, frame, 0, frame->num_buffers,
		&param_set->rdma_prev_l0.cmd, param_set->rdma_prev_l0.plane,
		param_set->input_dva_prev_l0, i_frame_p->dvaddr_buffer);

	/* output yuv */
	param_set->wdma_prev_l0.plane = i_frame_c->planes;
	CALL_HW_OPS(hw_ip, dma_cfg, "yuv_out_i", hw_ip, frame, 0, frame->num_buffers,
		&param_set->wdma_prev_l0.cmd, param_set->wdma_prev_l0.plane,
		param_set->output_dva_prev_l0, i_frame_c->dvaddr_buffer);
}

static inline int __check_recursive_nr_2nd_iter(struct is_frame *frame)
{
	if (!frame->shot_ext)
		return 0;

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
	return frame->shot_ext->node_group.leader.recursiveNrType == NODE_RECURSIVE_NR_TYPE_2ND;
#else
	return 0;
#endif
}

static int __config_mixer_buffers(struct is_hw_ip *hw_ip, struct is_frame *frame,
	struct is_mtnr0_config *config, u32 cmd_prev_l0_wgt_in, u32 cmd_prev_l0_in,
	u32 cmd_prev_l0_out, u32 cmd_prev_l0_wgt_out)
{
	int ret;
	bool swap_frame, swap_fro;
	u32 instance = frame->instance;
	struct is_hw_mtnr0 *hw_mtnr = GET_HW(hw_ip);
	struct mtnr_param_set *param_set = &hw_mtnr->param_set[instance];
	struct param_stripe_input *stripe_input = &param_set->stripe_input;
	u32 cur_idx = frame->cur_buf_index;

	if (config->mixerL0_en) {
		ret = __is_hw_mtnr0_alloc_buffer(instance, hw_ip, param_set, config);
		if (ret) {
			mserr_hw("__is_hw_mtnr0_alloc_buffer is fail", instance, hw_ip);
			return ret;
		}

		swap_frame = (SKIP_MIX(config) || PARTIAL(stripe_input) || EVEN_BATCH(frame)) ?
				     false :
				     true;
		swap_fro = (SKIP_MIX(config) || PARTIAL(stripe_input)) ? false : true;

		if (cmd_prev_l0_in && cmd_prev_l0_out) {
			CALL_HW_OPS(hw_ip, dma_cfg, "prev_yuv", hw_ip, frame, cur_idx,
				frame->num_buffers, &param_set->rdma_prev_l0.cmd,
				param_set->rdma_prev_l0.plane, param_set->input_dva_prev_l0,
				frame->dva_mtnr0_out_prev_l0_yuv);
			CALL_HW_OPS(hw_ip, dma_cfg, "yuv_out", hw_ip, frame, cur_idx,
				frame->num_buffers, &param_set->wdma_prev_l0.cmd,
				param_set->wdma_prev_l0.plane, param_set->output_dva_prev_l0,
				frame->dva_mtnr0_cap_l0_yuv);
		} else {
			is_hw_mtnr0_set_yuv_i_buffer(instance, hw_ip, hw_mtnr, frame, swap_frame,
				swap_fro, MTNR0_SUBDEV_PREV_YUV, MTNR0_SUBDEV_YUV);
		}

		if (cmd_prev_l0_wgt_in && cmd_prev_l0_wgt_out) {
			CALL_HW_OPS(hw_ip, dma_cfg, "prev_wgt", hw_ip, frame, cur_idx,
				frame->num_buffers, &param_set->rdma_prev_l0_wgt.cmd,
				param_set->rdma_prev_l0_wgt.plane, param_set->input_dva_prev_l0_wgt,
				frame->dva_mtnr0_out_prev_l0_wgt);
			CALL_HW_OPS(hw_ip, dma_cfg, "wgt_out", hw_ip, frame, cur_idx,
				frame->num_buffers, &param_set->wdma_prev_l0_wgt.cmd,
				param_set->wdma_prev_l0_wgt.plane,
				param_set->output_dva_prev_l0_wgt, frame->dva_mtnr0_cap_l0_wgt);
		} else {
			is_hw_mtnr0_set_wgt_i_buffer(instance, hw_ip, hw_mtnr, frame, swap_frame,
				swap_fro, MTNR0_SUBDEV_PREV_W, MTNR0_SUBDEV_W);
		}
	} else {
		__is_hw_mtnr0_free_buffer(instance, hw_ip);
	}

	return 0;
}

static int __config_mixer_buffers_2nr(
	struct is_hw_ip *hw_ip, struct is_frame *frame, struct is_mtnr0_config *config)
{
	int ret;
	bool swap_frame, swap_fro;
	u32 instance = frame->instance;
	struct is_hw_mtnr0 *hw_mtnr = GET_HW(hw_ip);
	struct mtnr_param_set *param_set = &hw_mtnr->param_set[instance];
	struct param_stripe_input *stripe_input = &param_set->stripe_input;

	if (config->mixerL0_en) {
		ret = __is_hw_mtnr0_alloc_buffer_2nr(instance, hw_ip, param_set, config);
		if (ret)
			return ret;

		swap_frame = (SKIP_MIX(config) || PARTIAL(stripe_input) || EVEN_BATCH(frame)) ?
				     false :
				     true;
		swap_fro = (SKIP_MIX(config) || PARTIAL(stripe_input)) ? false : true;

		is_hw_mtnr0_set_yuv_i_buffer(instance, hw_ip, hw_mtnr, frame, swap_frame, swap_fro,
			MTNR0_SUBDEV_PREV_YUV_2NR, MTNR0_SUBDEV_YUV_2NR);

		is_hw_mtnr0_set_wgt_i_buffer(instance, hw_ip, hw_mtnr, frame, swap_frame, swap_fro,
			MTNR0_SUBDEV_PREV_W_2NR, MTNR0_SUBDEV_W_2NR);
	} else {
		__is_hw_mtnr0_free_buffer_2nr(instance, hw_ip);
	}

	return 0;
}

static void _is_hw_mtnr0_s_cmd(struct is_hw_mtnr0 *hw, struct c_loader_buffer *clb, u32 fcount,
	struct pablo_common_ctrl_cmd *cmd)
{
	cmd->set_mode = PCC_APB_DIRECT;
	cmd->fcount = fcount;
	cmd->int_grp_en = mtnr0_hw_g_int_grp_en();

	if (!clb)
		return;

	cmd->base_addr = clb->header_dva;
	cmd->header_num = clb->num_of_headers;
	cmd->set_mode = PCC_DMA_DIRECT;
}

static int is_hw_mtnr0_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct is_hw_mtnr0 *hw_mtnr;
	struct is_param_region *param_region;
	struct mtnr_param_set *param_set;
	u32 fcount, instance;
	u32 cur_idx;
	u32 set_id;
	int ret, i;
	u32 cmd_input, cmd_input_l4, cmd_mv_geomatch_in;
	u32 cmd_seg_l0_in, cmd_prev_l0_in, cmd_prev_l0_wgt_in;
	u32 cmd_prev_l0_out, cmd_prev_l0_wgt_out;
	u32 strip_index, strip_total_count, strip_repeat_num, strip_repeat_idx;
	struct is_mtnr0_config *config;
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

	hw_mtnr = GET_HW(hw_ip);
	pcc = hw_mtnr->pcc;
	param_set = &hw_mtnr->param_set[instance];
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

	__is_hw_mtnr0_update_param(hw_ip, param_region, param_set, frame->pmap, instance);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;

	cmd_input = CALL_HW_OPS(hw_ip, dma_cfg, "mtnr0_l0", hw_ip, frame, cur_idx,
		frame->num_buffers, &param_set->rdma_cur_l0.cmd, param_set->rdma_cur_l0.plane,
		param_set->input_dva_cur_l0, frame->dvaddr_buffer);
	cmd_input_l4 = CALL_HW_OPS(hw_ip, dma_cfg, "mtnr0_l4", hw_ip, frame, cur_idx,
		frame->num_buffers, &param_set->rdma_cur_l4.cmd, param_set->rdma_cur_l4.plane,
		param_set->input_dva_cur_l4, frame->dva_mtnr1_out_cur_l4_yuv);

	cmd_mv_geomatch_in = CALL_HW_OPS(hw_ip, dma_cfg, "mtnr0_mv_geomatch", hw_ip, frame, cur_idx,
		frame->num_buffers, &param_set->rdma_mv_geomatch.cmd,
		param_set->rdma_mv_geomatch.plane, param_set->input_dva_mv_geomatch,
		frame->dva_mtnr0_out_mv_geomatch);

	cmd_seg_l0_in = CALL_HW_OPS(hw_ip, dma_cfg, "seg_l0", hw_ip, frame, cur_idx,
		frame->num_buffers, &param_set->rdma_seg_l0.cmd, param_set->rdma_seg_l0.plane,
		param_set->input_dva_seg_l0, frame->dva_mtnr0_out_seg_l0);

	cmd_prev_l0_wgt_in = param_set->rdma_prev_l0_wgt.cmd;
	cmd_prev_l0_in = param_set->rdma_prev_l0.cmd;
	cmd_prev_l0_out = param_set->wdma_prev_l0.cmd;
	cmd_prev_l0_wgt_out = param_set->wdma_prev_l0_wgt.cmd;

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
		if (hw_mtnr->repeat_instance != instance)
			hw_mtnr->repeat_state = 0;

		if (frame_cfg.num_buffers > 1 || strip_total_count > 1 || strip_repeat_num > 1)
			hw_mtnr->repeat_state++;
		else
			hw_mtnr->repeat_state = 0;

		if (hw_mtnr->repeat_state > 1 &&
			(!pablo_is_first_shot(frame_cfg.num_buffers, cur_idx) ||
				!pablo_is_first_shot(strip_total_count, strip_index) ||
				!pablo_is_first_shot(strip_repeat_num, strip_repeat_idx)))
			skip = true;

		hw_mtnr->repeat_instance = instance;
	}

	msdbgs_hw(2,
		"[F:%d] repeat_state(%d), batch(%d, %d), strip(%d, %d), repeat(%d, %d), skip(%d)",
		instance, hw_ip, frame->fcount, hw_mtnr->repeat_state, frame_cfg.num_buffers,
		cur_idx, strip_total_count, strip_index, strip_repeat_num, strip_repeat_idx, skip);

	if (!IS_ENABLED(IRTA_CALL)) {
		hw_mtnr->config[instance].imgL0_bit = 12;
		hw_mtnr->config[instance].wgtL0_bit = 8;
	}

	if (unlikely(is_hw_mtnr0_check_debug_type(MTNR0_DBG_TNR))) {
		hw_mtnr->config[instance].mixerL0_still_en = 0;
		info("[TNR] mixerL0_mode %d\n", hw_mtnr->config[instance].mixerL0_mode);
		hw_mtnr->config[instance].mixerL0_en = 1;

		param_set->wdma_prev_l0.width = param_set->rdma_cur_l0.width;
		param_set->wdma_prev_l0.height = param_set->rdma_cur_l0.width;
		param_set->wdma_prev_l0.sbwc_type = param_set->rdma_cur_l0.sbwc_type;
	}

	/* temp direct set */
	set_id = COREX_DIRECT;

	/* reset CLD buffer */
	clb.num_of_headers = 0;
	clb.num_of_values = 0;
	clb.num_of_pairs = 0;

	clb.header_dva = hw_mtnr->dva_c_loader_header;
	clb.payload_dva = hw_mtnr->dva_c_loader_payload;

	clb.clh = (struct c_loader_header *)hw_mtnr->kva_c_loader_header;
	clb.clp = (struct c_loader_payload *)hw_mtnr->kva_c_loader_payload;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	mtnr0_hw_s_core(get_base(hw_ip), set_id);

	config = &hw_mtnr->config[instance];

	do_blk_cfg = CALL_HW_HELPER_OPS(
		hw_ip, set_rta_regs, instance, set_id, skip, frame, (void *)&clb);
	if (unlikely(do_blk_cfg))
		__is_hw_mtnr0_update_block_reg(hw_ip, param_set, instance, set_id);

	__is_hw_mtnr0_set_prev_dma_cmd(param_set, instance, config);

	__is_hw_mtnr0_set_cur_dma_cmd(param_set, instance, config);
	__is_hw_mtnr0_set_cin_cmd(param_set, instance, config);

	if (config->mvc_en && !cmd_mv_geomatch_in) {
		mswarn_hw("MVC is enabled, but mv buffer is disabled", instance, hw_ip);
		ret = -EINVAL;
		goto shot_fail_recovery;
	}

	ret = __is_hw_mtnr0_set_size_regs(hw_ip, param_set, instance, frame, set_id, &frame_cfg);
	if (ret) {
		mserr_hw("__is_hw_mtnr0_set_size_regs is fail", instance, hw_ip);
		goto shot_fail_recovery;
	}

	if (unlikely(is_hw_mtnr0_check_debug_type(MTNR0_DBG_DTP)))
		mtnr0_hw_s_dtp(get_base(hw_ip), 1, MTNR0_DTP_COLOR_BAR, 0, 0, 0,
			MTNR0_DTP_COLOR_BAR_BT601);

	if (__check_recursive_nr_2nd_iter(frame)) {
		ret = __config_mixer_buffers_2nr(hw_ip, frame, config);
		if (ret)
			goto shot_fail_recovery;
	} else {
		ret = __config_mixer_buffers(hw_ip, frame, config, cmd_prev_l0_wgt_in,
			cmd_prev_l0_in, cmd_prev_l0_out, cmd_prev_l0_wgt_out);
		if (ret)
			goto shot_fail_recovery;
	}

	/* TODO: Need to check MSNR want to get seg by otf */
	mtnr0_hw_s_seg_otf_to_msnr(get_base(hw_ip), cmd_seg_l0_in);

	mtnr0_hw_s_still_last_frame_en(get_base(hw_ip), config->still_last_frame_en);

	for (i = MTNR0_RDMA_CUR_L0_Y; i < MTNR0_RDMA_MAX; i++) {
		ret = __is_hw_mtnr0_set_rdma(hw_ip, hw_mtnr, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_mtnr0_set_rdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	for (i = MTNR0_WDMA_PREV_L0_Y; i < MTNR0_WDMA_MAX; i++) {
		ret = __is_hw_mtnr0_set_wdma(hw_ip, hw_mtnr, param_set, instance, i, set_id);
		if (ret) {
			mserr_hw("__is_hw_mtnr0_set_wdma is fail", instance, hw_ip);
			goto shot_fail_recovery;
		}
	}

	if (config->L0_bypass &&
		(config->mixerL0_mode || config->mixerL0_en || config->geomatchL0_en ||
			cmd_prev_l0_in || cmd_prev_l0_out))
		mserr_hw("L0_bypass setting mismatched, [%d, %d, %d, %d, %d]\n", instance, hw_ip,
			config->mixerL0_mode, config->mixerL0_en, config->geomatchL0_en,
			cmd_prev_l0_in, cmd_prev_l0_out);

	if (param_region->mtnr.control.strgen == CONTROL_COMMAND_START) {
		msdbg_hw(2, "STRGEN input\n", instance, hw_ip);
		mtnr0_hw_s_strgen(get_base(hw_ip), set_id);
	}

	pmio_cache_fsync(hw_ip->pmio, (void *)&clb, PMIO_FORMATTER_PAIR);

	if (clb.num_of_pairs > 0)
		clb.num_of_headers++;

	CALL_BUFOP(hw_mtnr->pb_c_loader_payload, sync_for_device, hw_mtnr->pb_c_loader_payload, 0,
		hw_mtnr->pb_c_loader_payload->size, DMA_TO_DEVICE);
	CALL_BUFOP(hw_mtnr->pb_c_loader_header, sync_for_device, hw_mtnr->pb_c_loader_header, 0,
		hw_mtnr->pb_c_loader_header->size, DMA_TO_DEVICE);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);

	_is_hw_mtnr0_s_cmd(hw_mtnr, &clb, fcount, &frame_cfg.cmd);

	if (unlikely(is_hw_mtnr0_check_debug_type(MTNR0_DBG_TNR))) {
		hw_mtnr->config[instance].mixerL0_mode++;
		hw_mtnr->config[instance].mixerL0_mode %= 3;
	}

	if (unlikely(is_hw_mtnr0_check_debug_type(MTNR0_DBG_DUMP_REG) ||
		     is_hw_mtnr0_check_debug_type(MTNR0_DBG_DUMP_REG_ONCE))) {
		mtnr0_hw_dump(hw_ip->pmio, HW_DUMP_CR);

		if (is_hw_mtnr0_check_debug_type(MTNR0_DBG_DUMP_REG_ONCE))
			is_hw_mtnr0_c_debug_type(MTNR0_DBG_DUMP_REG_ONCE);
	}

	set_bit(HW_CONFIG, &hw_ip->state);

shot_fail_recovery:
	/* Restore CMDs in param_set. */
	param_set->rdma_prev_l0.cmd = cmd_prev_l0_in;
	param_set->rdma_mv_geomatch.cmd = cmd_mv_geomatch_in;
	param_set->rdma_prev_l0_wgt.cmd = cmd_prev_l0_wgt_in;
	param_set->wdma_prev_l0.cmd = cmd_prev_l0_out;
	param_set->wdma_prev_l0_wgt.cmd = cmd_prev_l0_wgt_out;
	param_set->rdma_cur_l4.cmd = cmd_input_l4;

	if (!ret)
		CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);

	CALL_PCC_OPS(pcc, set_qch, pcc, false);

	return ret;
}

static int is_hw_mtnr0_get_meta(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	return 0;
}

static int is_hw_mtnr0_get_cap_meta(
	struct is_hw_ip *hw_ip, ulong hw_map, u32 instance, u32 fcount, u32 size, ulong addr)
{
	return 0;
}

static int is_hw_mtnr0_frame_ndone(
	struct is_hw_ip *hw_ip, struct is_frame *frame, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1, output_id, done_type, false);

	return ret;
}

static int is_hw_mtnr0_load_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_mtnr0_apply_setfile(
	struct is_hw_ip *hw_ip, u32 scenario, u32 instance, ulong hw_map)
{
	return 0;
}

static int is_hw_mtnr0_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	return 0;
}

int is_hw_mtnr0_restore(struct is_hw_ip *hw_ip, u32 instance)
{
	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	if (__is_hw_mtnr0_s_common_reg(hw_ip, instance)) {
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

static int is_hw_mtnr0_set_regs(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size)
{
	return 0;
}

int is_hw_mtnr0_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs,
	u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct is_hw_mtnr0 *hw_mtnr = GET_HW(hw_ip);
	struct pablo_common_ctrl *pcc = hw_mtnr->pcc;
	u32 i;
	int ret = 0;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		mtnr0_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
		mtnr0_hw_dump(hw_ip->pmio, HW_DUMP_CR);
		break;
	case IS_REG_DUMP_DMA:
		for (i = MTNR0_RDMA_CUR_L0_Y; i < MTNR0_RDMA_MAX; i++) {
			dma = &hw_mtnr->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = MTNR0_WDMA_PREV_L0_Y; i < MTNR0_WDMA_MAX; i++) {
			dma = &hw_mtnr->wdma[i];
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
			msinfo_hw("[F:%d] MTNR0 " #f ": %d -> %d", instance, hw_ip, fcount, old.f, \
				new->f);                                                           \
	}

#define CMP_CONFIG_SIZE(old, new, f1, f2)                                                          \
	{                                                                                          \
		if (old.f1 != new->f1 || old.f2 != new->f2                                         \
				 || is_get_debug_param(IS_DEBUG_PARAM_HW) >= 2)                    \
			msinfo_hw("[F:%d] MTNR0 " #f1 " x " #f2 ": %dx%d -> %dx%d", instance, hw_ip,\
				fcount, old.f1, old.f2, new->f1, new->f2);                         \
	}

static int is_hw_mtnr0_set_config(
	struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf)
{
	struct is_hw_mtnr0 *hw_mtnr;
	struct is_mtnr0_config *mtnr_config = (struct is_mtnr0_config *)conf;

	FIMC_BUG(!conf);

	hw_mtnr = GET_HW(hw_ip);

	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, still_last_frame_en);
	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, L0_bypass);

	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, mvc_en);
	CMP_CONFIG_SIZE(hw_mtnr->config[instance], mtnr_config, mvc_in_w, mvc_in_h);
	CMP_CONFIG_SIZE(hw_mtnr->config[instance], mtnr_config, mvc_out_w, mvc_out_h);

	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, geomatchL0_en);
	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, mixerL0_en);
	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, mixerL0_mode);
	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, mixerL0_still_en);
	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, imgL0_bit);
	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, wgtL0_bit);

	CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, mixerL0_contents_aware_isp_en);

	if (mtnr_config->mixerL0_still_en)
		mtnr_config->imgL0_bit = 12;
	else
		CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, imgL0_bit);

	if (mtnr_config->imgL0_bit != 8 && mtnr_config->imgL0_bit != 10 &&
		mtnr_config->imgL0_bit != 12) {
		mswarn_hw("[F:%d] imgL0_bit(%d) is not valid", instance, hw_ip, fcount,
			mtnr_config->imgL0_bit);
		mtnr_config->imgL0_bit = 12;
	}

	if (mtnr_config->mixerL0_still_en)
		mtnr_config->wgtL0_bit = 8;
	else
		CMP_CONFIG(hw_mtnr->config[instance], mtnr_config, wgtL0_bit);

	if (mtnr_config->wgtL0_bit != 4 && mtnr_config->wgtL0_bit != 8) {
		mswarn_hw("[F:%d] wgtL0_bit(%d) is not valid", instance, hw_ip, fcount,
			mtnr_config->wgtL0_bit);
		mtnr_config->wgtL0_bit = 8;
	}

	msdbg_hw(2, "[F:%d] skip_wdma(%d)\n", instance, hw_ip, fcount, mtnr_config->skip_wdma);

	memcpy(&hw_mtnr->config[instance], conf, sizeof(struct is_mtnr0_config));

	return 0;
}

static int is_hw_mtnr0_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_mtnr0 *hw_mtnr;
	struct pablo_common_ctrl *pcc;

	hw_mtnr = GET_HW(hw_ip);
	if (!hw_mtnr) {
		mserr_hw("failed to get HW MTNR0", instance, hw_ip);
		return -ENODEV;
	}

	pcc = hw_mtnr->pcc;

	CALL_PCC_OPS(pcc, set_qch, pcc, true);

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
	mtnr0_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);

	CALL_PCC_OPS(pcc, set_qch, pcc, false);

	return 0;
}

static size_t is_hw_mtnr0_dump_params(struct is_hw_ip *hw_ip, u32 instance, char *buf, size_t size)
{
	struct is_hw_mtnr0 *hw_mtnr = GET_HW(hw_ip);
	struct mtnr_param_set *param;
	size_t rem = size;
	char *p = buf;

	param = &hw_mtnr->param_set[instance];

	p = pablo_json_nstr(p, "hw name", hw_ip->name, strlen(hw_ip->name), &rem);
	p = pablo_json_uint(p, "hw id", hw_ip->id, &rem);

	p = dump_param_stripe_input(p, "stripe_input", &param->stripe_input, &rem);
	p = dump_param_otf_input(p, "cin_mtnr1_wgt", &param->cin_mtnr1_wgt, &rem);
	p = dump_param_otf_output(p, "cout_msnr_l0", &param->cout_msnr_l0, &rem);
	p = dump_param_dma_input(p, "rdma_cur_l0", &param->rdma_cur_l0, &rem);
	p = dump_param_dma_input(p, "rdma_cur_l4", &param->rdma_cur_l4, &rem);
	p = dump_param_dma_input(p, "rdma_prev_l0", &param->rdma_prev_l0, &rem);
	p = dump_param_dma_input(p, "rdma_prev_l0_wgt", &param->rdma_prev_l0_wgt, &rem);
	p = dump_param_dma_input(p, "rdma_mv_geomatch", &param->rdma_mv_geomatch, &rem);
	p = dump_param_dma_input(p, "rdma_seg_l0", &param->rdma_seg_l0, &rem);
	p = dump_param_dma_output(p, "wdma_prev_l0", &param->wdma_prev_l0, &rem);
	p = dump_param_dma_output(p, "wdma_prev_l0_wgt", &param->wdma_prev_l0_wgt, &rem);

	p = pablo_json_uint(p, "instance_id", param->instance_id, &rem);
	p = pablo_json_uint(p, "fcount", param->fcount, &rem);
	p = pablo_json_uint(p, "tnr_mode", param->tnr_mode, &rem);
	p = pablo_json_uint(p, "mono_mode", param->mono_mode, &rem);
	p = pablo_json_bool(p, "reprocessing", param->reprocessing, &rem);

	return WRITTEN(size, rem);
}

const struct is_hw_ip_ops is_hw_mtnr0_ops = {
	.open = is_hw_mtnr0_open,
	.init = is_hw_mtnr0_init,
	.deinit = is_hw_mtnr0_deinit,
	.close = is_hw_mtnr0_close,
	.enable = is_hw_mtnr0_enable,
	.disable = is_hw_mtnr0_disable,
	.shot = is_hw_mtnr0_shot,
	.set_param = is_hw_mtnr0_set_param,
	.get_meta = is_hw_mtnr0_get_meta,
	.get_cap_meta = is_hw_mtnr0_get_cap_meta,
	.frame_ndone = is_hw_mtnr0_frame_ndone,
	.load_setfile = is_hw_mtnr0_load_setfile,
	.apply_setfile = is_hw_mtnr0_apply_setfile,
	.delete_setfile = is_hw_mtnr0_delete_setfile,
	.restore = is_hw_mtnr0_restore,
	.set_regs = is_hw_mtnr0_set_regs,
	.dump_regs = is_hw_mtnr0_dump_regs,
	.set_config = is_hw_mtnr0_set_config,
	.notify_timeout = is_hw_mtnr0_notify_timeout,
	.reset = is_hw_mtnr0_reset,
	.wait_idle = is_hw_mtnr0_wait_idle,
	.dump_params = is_hw_mtnr0_dump_params,
};

int pablo_hw_mtnr0_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;
	int ret = 0;

	hw_ip->ops = &is_hw_mtnr0_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_mtnr0_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_mtnr0_handle_interrupt;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.name = "mtnr0";

	hw_ip->pmio_config.mmio_base = hw_ip->regs[REG_SETA];
	hw_ip->pmio_config.phys_base = hw_ip->regs_start[REG_SETA];

	hw_ip->pmio_config.cache_type = PMIO_CACHE_NONE;

	mtnr0_hw_init_pmio_config(&hw_ip->pmio_config);

	hw_ip->pmio = pmio_init(NULL, NULL, &hw_ip->pmio_config);
	if (IS_ERR(hw_ip->pmio)) {
		err("failed to init mtnr0 PMIO: %ld", PTR_ERR(hw_ip->pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(hw_ip->pmio, &hw_ip->pmio_fields, hw_ip->pmio_config.fields,
		hw_ip->pmio_config.num_fields);
	if (ret) {
		err("failed to alloc mtnr0 PMIO fields: %d", ret);
		pmio_exit(hw_ip->pmio);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(pablo_hw_mtnr0_probe);

void pablo_hw_mtnr0_remove(struct is_hw_ip *hw_ip)
{
	struct is_interface_ischain *itfc = hw_ip->itfc;
	int id = hw_ip->id;
	int hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = false;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = false;

	pmio_field_bulk_free(hw_ip->pmio, hw_ip->pmio_fields);
	pmio_exit(hw_ip->pmio);
}
EXPORT_SYMBOL_GPL(pablo_hw_mtnr0_remove);
