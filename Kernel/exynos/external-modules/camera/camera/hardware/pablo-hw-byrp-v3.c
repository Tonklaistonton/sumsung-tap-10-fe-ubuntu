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
#include "pablo-hw-byrp-v3.h"
#include "is-err.h"
#include "pablo-icpu-adapter.h"
#include "pablo-crta-bufmgr.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-hw-param-debug.h"

static DEFINE_MUTEX(cmn_reg_lock);

static const enum pablo_crta_buf_type dma_to_crta_buf[BYRP_WDMA_MAX] = {
	PABLO_CRTA_BUF_MAX,
	PABLO_CRTA_BUF_PRE_THUMB,
	PABLO_CRTA_BUF_CDAF_MW,
	PABLO_CRTA_BUF_RGBY_HIST,
	PABLO_CRTA_BUF_AE_THUMB,
	PABLO_CRTA_BUF_AWB_THUMB,
};

static struct is_byrp_config conf_default = {
	.thstat_pre_bypass = true,
	.thstat_awb_bypass = true,
	.thstat_ae_bypass = true,
	.rgbyhist_bypass = true,
	.cdaf_bypass = true,
	.cdaf_mw_bypass = true,
};

static inline void *get_base(struct is_hw_ip *hw_ip)
{
	return hw_ip->pmio;
}

static int param_debug_byrp_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] bit value, BYRP debug features\n"
				"\tb[0] : dump sfr\n"
				"\tb[1] : enable DTP\n"
				"\tb[2] : dump pmio\n"
				"\tb[3] : dump rta\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static struct pablo_debug_param debug_byrp = {
	.type = IS_DEBUG_PARAM_TYPE_BIT,
	.max_value = 0xF,
	.ops.usage = param_debug_byrp_usage,
};

module_param_cb(debug_byrp, &pablo_debug_param_ops, &debug_byrp, 0644);

void is_hw_byrp_s_debug_type(int type)
{
	set_bit(type, &debug_byrp.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_byrp_s_debug_type);

void is_hw_byrp_c_debug_type(int type)
{
	clear_bit(type, &debug_byrp.value);
}
KUNIT_EXPORT_SYMBOL(is_hw_byrp_c_debug_type);

static void fs_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct is_hardware *hardware = hw_ip->hardware;
	struct is_hw_byrp *hw = (struct is_hw_byrp *)hw_ip->priv_info;
	struct pablo_common_ctrl *pcc = hw->pcc;
	u32 hw_fcount;
	u32 dbg_hw_lv =
		atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;
	u32 delay;

	/* shot_fcount -> start_fcount */
	hw_fcount = atomic_read(&hw_ip->fcount);
	atomic_set(&hw->start_fcount, hw_fcount);

	msdbg_hw(dbg_hw_lv, "[F%d]FS\n", instance, hw_ip, fcount);

	atomic_add(1, &hw_ip->count.fs);
	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_FRAME_START);
	CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);

	if (test_bit(IS_SENSOR_AEB_SWITCHING, &hw_ip->group[instance]->device->sensor->aeb_state))
		delay = 0;
	else
		delay = hw->post_frame_gap * hardware->dvfs_freq[IS_DVFS_CAM];

	CALL_PCC_OPS(pcc, set_delay, pcc, delay);
}

static void fe_handler(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, u32 status)
{
	struct is_hw_byrp *hw = (struct is_hw_byrp *)hw_ip->priv_info;
	struct is_hardware *hardware = hw_ip->hardware;
	u32 dbg_lv = atomic_read(&hardware->streaming[hardware->sensor_position[instance]]) ? 2 : 0;

	msdbg_hw(dbg_lv, "[F%d]FE\n", instance, hw_ip, fcount);

	if (hw->param_set[instance].dma_input.cmd)
		byrp_hw_s_sr(get_base(hw_ip), false, hw_ip->ch);

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

static int pablo_hw_byrp_check_int(struct is_hw_ip *hw_ip, u32 status)
{
	struct is_hw_byrp *hw = (struct is_hw_byrp *)hw_ip->priv_info;
	u32 hw_fcount = atomic_read(&hw_ip->fcount);
	u32 instance = atomic_read(&hw_ip->instance);
	u32 fs, fe, setting_done;
	u32 int_status;

	fs = byrp_hw_is_occurred(status, INTR_FRAME_START);
	fe = byrp_hw_is_occurred(status, INTR_FRAME_END);
	setting_done = byrp_hw_is_occurred(status, INTR_SETTING_DONE);

	if (fs && fe)
		mswarn_hw("[F%d] start/end overlapped!!", instance, hw_ip, hw_fcount);

	int_status = fs | fe;

	msdbg_hw(4, "[F%d] int_status 0x%08x fs 0x%08x fe 0x%08x setting_done 0x%08x\n", instance,
		hw_ip, hw_fcount, int_status, fs, fe, setting_done);

	while (int_status) {
		switch (hw->event_state) {
		case BYRP_FS:
			if (fe) {
				hw->event_state = BYRP_FE;
				fe_handler(hw_ip, instance, hw_fcount, status);
				int_status &= ~fe;
				fe = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case BYRP_FE:
			if (fs) {
				hw->event_state = BYRP_FS;
				fs_handler(hw_ip, instance, hw_fcount, status);
				int_status &= ~fs;
				fs = 0;
			} else {
				goto skip_isr_event;
			}
			break;
		case BYRP_INIT:
			if (fs) {
				hw->event_state = BYRP_FS;
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

static int is_hw_byrp_handle_interrupt0(u32 id, void *context)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_byrp *hw_byrp;
	struct pablo_common_ctrl *pcc;
	u32 status, instance, hw_fcount;
	u32 int_status;

	hw_ip = (struct is_hw_ip *)context;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	if (!hw_byrp) {
		msinfo_hw("[F%d][INT0] Ignore ISR not in open state", instance, hw_ip, hw_fcount);
		return 0;
	}

	atomic_inc(&hw_byrp->isr_run_count);

	pcc = hw_byrp->pcc;
	hw_byrp->irq_state[BYRP_INT0] = status = CALL_PCC_OPS(pcc, get_int_status, pcc, id, true);

	msdbg_hw(2, "[F%d][INT0] status 0x%08x\n", instance, hw_ip, hw_fcount, status);

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state) ||
		!test_bit(HW_CONFIG, &hw_ip->state)) {
		mserr_hw("[F%d][INT0] invalid HW state 0x%lx status 0x%x", instance, hw_ip,
			 hw_fcount, hw_ip->state, status);
		atomic_dec(&hw_byrp->isr_run_count);
		wake_up(&hw_byrp->isr_wait_queue);
		return 0;
	}

	int_status = pablo_hw_byrp_check_int(hw_ip, status);
	if (int_status)
		mswarn_hw("[F%d][INT0] invalid: event_state(%ld), int_status(%x)", instance, hw_ip,
			  hw_fcount, hw_byrp->event_state, int_status);

	if (byrp_hw_is_occurred(status, INTR_SETTING_DONE)) {
		CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, hw_fcount, DEBUG_POINT_SETTING_DONE);

		if (unlikely(test_bit(BYRP_DBG_DUMP_REG, &debug_byrp.value))) {
			byrp_hw_dump(hw_ip->pmio, HW_DUMP_CR);
			CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		}
	}

	if (byrp_hw_is_occurred(status, INTR_ERR0)) {
		is_debug_lock();
		mserr_hw("[F%d][INT0] HW Error!! status 0x%08x\n", instance, hw_ip, hw_fcount,
			 status);
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		byrp_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
		is_debug_unlock();

		if (IS_ENABLED(USE_SKIP_DUMP_LIC_OVERFLOW) &&
		    CALL_HW_OPS(hw_ip, clear_crc_status, instance, hw_ip) > 0) {
			set_bit(IS_SENSOR_ESD_RECOVERY,
				&hw_ip->group[instance]->device->sensor->state);
			mswarn_hw("skip to s2d dump", instance, hw_ip);
		} else {
			is_debug_s2d(true, "BYRP HW Error");
		}
	}

	if (byrp_hw_is_occurred(status, INTR_WARN0)) {
		mswarn_hw("[F%d][INT0] HW Warning!! status 0x%08x", instance, hw_ip, hw_fcount,
			  status);
		is_debug_lock();
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		byrp_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
		is_debug_unlock();

		byrp_hw_cotf_error_handle(get_base(hw_ip));
	}

	atomic_dec(&hw_byrp->isr_run_count);
	wake_up(&hw_byrp->isr_wait_queue);

	return 0;
}

static inline void is_hw_byrp_put_vc_buf(
	enum itf_vc_buf_data_type data_type, struct is_hw_ip *hw_ip, u32 instance, u32 fcount)
{
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct is_sensor_interface *sensor_itf = hw_byrp->sensor_itf[instance];
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

static inline void __is_hw_byrp_cdaf(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct pablo_crta_buf_info cdaf_buf[PABLO_CRTA_CDAF_DATA_MAX] = {
		0,
	};
	struct pablo_crta_bufmgr *bufmgr;
	struct is_sensor_interface *sensor_itf;
	u32 hw_fcount = atomic_read(&hw_byrp->start_fcount);
	u32 index, af_type;
	struct vc_buf_info_t vc_buf_info;

	/* Skip cdaf data*/
	if (test_bit(HW_OVERFLOW_RECOVERY, &hw_ip->state))
		return;

	if (!hw_byrp->param_set[instance].reprocessing) {
		bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_CDAF, instance, hw_ip->ch);
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, hw_fcount, true,
					   &cdaf_buf[PABLO_CRTA_CDAF_RAW]);

		if (ret || !cdaf_buf[PABLO_CRTA_CDAF_RAW].kva) {
			mserr_hw("[F%d] There is no valid CDAF buffer", instance, hw_ip, hw_fcount);
			return;
		}

		if (byrp_hw_g_cdaf_data(hw_ip->pmio, cdaf_buf[PABLO_CRTA_CDAF_RAW].kva)) {
			mserr_hw("[F%d] Failed to get CDAF stat", instance, hw_ip, hw_fcount);
			return;
		}

		sensor_itf = hw_byrp->sensor_itf[instance];
		if (!sensor_itf) {
			mserr_hw("[F%d] sensor_interface is NULL", instance, hw_ip, hw_fcount);
			return;
		}

		/* PDAF */
		ret = sensor_itf->csi_itf_ops.get_vc_dma_buf_info(
			sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT1, &vc_buf_info);
		if (!ret)
			get_vc_dma_buf_by_fcount(sensor_itf, VC_BUF_DATA_TYPE_SENSOR_STAT1,
						 hw_fcount, &index,
						 &cdaf_buf[PABLO_CRTA_PDAF_TAIL].dva);

		/*LAF*/
		if (sensor_itf->cis_itf_ops.is_laser_af_available(sensor_itf)) {
			bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_LAF, instance, hw_ip->ch);
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, hw_fcount, true,
						   &cdaf_buf[PABLO_CRTA_LASER_AF_DATA]);

			if (ret || !cdaf_buf[PABLO_CRTA_LASER_AF_DATA].kva) {
				mserr_hw("[F%d] There is no valid LAF buffer", instance, hw_ip,
					 hw_fcount);
				return;
			}

			sensor_itf->laser_af_itf_ops.get_distance(
				sensor_itf, &af_type, cdaf_buf[PABLO_CRTA_LASER_AF_DATA].kva);
		}

		ret = CALL_ADT_MSG_OPS(hw_byrp->icpu_adt, send_msg_cstat_cdaf_end, instance,
				       (void *)hw_ip, NULL, hw_fcount, cdaf_buf);
		if (ret)
			mserr_hw("icpu_adt send_msg_cstat_cdaf_end fail", instance, hw_ip);
	}
}

static int __is_hw_byrp_cdaf_callback(void *caller, void *ctx, void *rsp_msg)
{
	int ret;
	u32 instance, fcount;
	struct is_hw_ip *hw_ip;
	struct is_hw_byrp *hw_byrp;
	struct pablo_icpu_adt_rsp_msg *msg;
	struct pablo_crta_buf_info buf_info;
	struct pablo_crta_bufmgr *bufmgr;
	struct is_sensor_interface *sensor_itf;

	if (!caller || !rsp_msg) {
		err_hw("invalid callback: caller(%p), msg(%p)", caller, rsp_msg);
		return -EINVAL;
	}

	hw_ip = (struct is_hw_ip *)caller;
	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	msg = (struct pablo_icpu_adt_rsp_msg *)rsp_msg;
	instance = msg->instance;
	fcount = msg->fcount;

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		msinfo_hw("[F%d]Ignore cdaf callback before HW config\n", instance, hw_ip, fcount);
		return 0;
	}

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("[F%d]Ignore cdaf callback invalid HW state(0x%lx)", instance, hw_ip,
			fcount, hw_ip->state);
		return 0;
	}

	msdbg_hw(1, "[F%d]%s", instance, hw_ip, fcount, __func__);

	if (msg->rsp)
		mserr_hw("frame_end fail from icpu: msg_ret(%d)", instance, hw_ip, msg->rsp);

	if (!hw_byrp->param_set[instance].reprocessing) {
		bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_CDAF, instance, hw_ip->ch);
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, fcount, &buf_info);
		if (!ret)
			CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);

		sensor_itf = hw_byrp->sensor_itf[instance];
		if (!sensor_itf) {
			mserr_hw("[F%d] sensor_interface is NULL", instance, hw_ip, fcount);
			return -EINVAL;
		}

		is_hw_byrp_put_vc_buf(VC_BUF_DATA_TYPE_SENSOR_STAT1, hw_ip, instance, fcount);

		if (sensor_itf->cis_itf_ops.is_laser_af_available(sensor_itf)) {
			bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_LAF, instance, hw_ip->ch);
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, fcount, &buf_info);
			if (!ret)
				CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		}
	}
	return 0;
}

static int is_hw_byrp_handle_interrupt1(u32 id, void *context)
{
	struct is_hw_ip *hw_ip;
	struct is_hw_byrp *hw_byrp;
	struct pablo_common_ctrl *pcc;
	u32 status, instance, hw_fcount;
	bool f_err;

	hw_ip = (struct is_hw_ip *)context;
	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);

	if (!hw_byrp) {
		mserr_hw("[F%d][INT1] Ignore ISR not in open state", instance, hw_ip, hw_fcount);
		return 0;
	}

	atomic_inc(&hw_byrp->isr_run_count);

	pcc = hw_byrp->pcc;
	hw_byrp->irq_state[BYRP_INT1] = status = CALL_PCC_OPS(pcc, get_int_status, pcc, id, true);

	msdbg_hw(2, "[F%d][INT1] status 0x%08x\n", instance, hw_ip, hw_fcount, status);

	if (!test_bit(HW_OPEN, &hw_ip->state) || !test_bit(HW_RUN, &hw_ip->state) ||
		!test_bit(HW_CONFIG, &hw_ip->state)) {
		mserr_hw("[F%d][INT1] invalid HW state 0x%lx status 0x%x", instance, hw_ip,
			 hw_fcount, hw_ip->state, status);
		atomic_dec(&hw_byrp->isr_run_count);
		wake_up(&hw_byrp->isr_wait_queue);
		return 0;
	}

	if (byrp_hw_is_occurred(status, INTR_FRAME_CDAF))
		__is_hw_byrp_cdaf(hw_ip, instance);

	f_err = byrp_hw_is_occurred(status, INTR_ERR1);

	if (f_err) {
		is_debug_lock();
		msinfo_hw("[F:%d][INT1] HW Error!! status 0x%08x\n", instance, hw_ip, hw_fcount,
			  status);
		CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_FULL);
		byrp_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);
		is_debug_unlock();
	}

	atomic_dec(&hw_byrp->isr_run_count);
	wake_up(&hw_byrp->isr_wait_queue);

	return 0;
}

static int is_hw_byrp_reset(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp;
	struct pablo_common_ctrl *pcc;

	msinfo_hw("reset\n", instance, hw_ip);

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	pcc = hw_byrp->pcc;

	return CALL_PCC_OPS(pcc, reset, pcc);
}

static int is_hw_byrp_wait_idle(struct is_hw_ip *hw_ip, u32 instance)
{
	return byrp_hw_wait_idle(get_base(hw_ip));
}

static int is_hw_byrp_prepare(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp;
	struct pablo_common_ctrl *pcc;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	pcc = hw_byrp->pcc;

	byrp_hw_g_int_en(hw_byrp->pcc_cfg.int_en);
	if (CALL_PCC_OPS(pcc, enable, pcc, &hw_byrp->pcc_cfg)) {
		mserr_hw("failed to PCC enable", instance, hw_ip);
		return -EINVAL;
	}

	byrp_hw_s_init(get_base(hw_ip), hw_ip->ch);

	return 0;
}

static inline int __is_hw_byrp_init_pcc(struct is_hw_ip *hw_ip)
{
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct is_mem *mem;
	enum pablo_common_ctrl_mode pcc_mode;

	pcc_mode = PCC_OTF;
	hw_byrp->pcc_cfg.fs_mode = PCC_VVALID_RISE;

	hw_byrp->pcc = pablo_common_ctrl_hw_get_pcc(&hw_ip->pmio_config);
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_BYRP);

	return CALL_PCC_HW_OPS(
		hw_byrp->pcc, init, hw_byrp->pcc, hw_ip->pmio, hw_ip->name, pcc_mode, mem);
}

static int _pablo_hw_byrp_cloader_init(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret;
	struct is_hw_byrp *hw;
	struct is_mem *mem;
	struct pablo_internal_subdev *pis;
	enum base_reg_index reg_id;
	resource_size_t reg_size;
	u32 reg_num, header_size;

	hw = (struct is_hw_byrp *)hw_ip->priv_info;
	mem = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_iommu_mem, GROUP_ID_BYRP);
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
	hw->header_size = header_size = ALIGN(reg_num, 16); /* Header: 16 Bytes per 16 CRs. */

	pis->width = header_size + reg_size;
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

static inline int __is_hw_byrp_init_dma(struct is_hw_ip *hw_ip)
{
	int ret = 0;
	u32 i;
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	hw_byrp->rdma_max_cnt = byrp_hw_g_rdma_max_cnt();
	hw_byrp->wdma_max_cnt = byrp_hw_g_wdma_max_cnt();
	hw_byrp->rdma_param_max_cnt = byrp_hw_g_rdma_cfg_max_cnt();
	hw_byrp->wdma_param_max_cnt = byrp_hw_g_wdma_cfg_max_cnt();
	hw_byrp->rdma = pablo_zalloc(sizeof(struct is_common_dma) * hw_byrp->rdma_max_cnt, GFP_KERNEL);
	if (!hw_byrp->rdma) {
		serr_hw("hw_ip->rdma(null)", hw_ip);
		ret = -ENOMEM;
		goto err_dma_alloc;
	}
	hw_byrp->wdma = pablo_zalloc(sizeof(struct is_common_dma) * hw_byrp->wdma_max_cnt, GFP_KERNEL);
	if (!hw_byrp->wdma) {
		serr_hw("hw_ip->wdma(null)", hw_ip);
		ret = -ENOMEM;
		goto err_dma_alloc;
	}

	for (i = 0; i < hw_byrp->rdma_max_cnt; i++) {
		ret = byrp_hw_rdma_create(&hw_byrp->rdma[i], get_base(hw_ip), i);
		if (ret) {
			serr_hw("byrp_hw_rdma_create error[%d]", hw_ip, i);
			ret = -ENODATA;
			goto err_dma_alloc;
		}
	}

	for (i = 0; i < hw_byrp->wdma_max_cnt; i++) {
		ret = byrp_hw_wdma_create(&hw_byrp->wdma[i], get_base(hw_ip), i);
		if (ret) {
			serr_hw("byrp_hw_wdma_create error[%d]", hw_ip, i);
			ret = -ENODATA;
			goto err_dma_alloc;
		}
	}

	return ret;

err_dma_alloc:
	pablo_free(hw_byrp->rdma);
	hw_byrp->rdma = NULL;
	pablo_free(hw_byrp->wdma);
	hw_byrp->wdma = NULL;

	return ret;
}

static int __nocfi is_hw_byrp_open(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	struct is_hw_byrp *hw_byrp = NULL;
	u32 reg_cnt = byrp_hw_g_reg_cnt();

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_ip->ch = hw_ip->id - DEV_HW_BYRP0;

	frame_manager_probe(hw_ip->framemgr, "HWBYRP");
	frame_manager_open(hw_ip->framemgr, IS_MAX_HW_FRAME, false);

	hw_ip->priv_info = pablo_zalloc(sizeof(struct is_hw_byrp), GFP_KERNEL);
	if (!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	hw_byrp->icpu_adt = pablo_get_icpu_adt();

	hw_byrp->iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	if (!hw_byrp->iq_set.regs) {
		mserr_hw("failed to alloc iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_regs_alloc;
	}

	hw_byrp->cur_iq_set.regs = pablo_zalloc(sizeof(struct cr_set) * reg_cnt, GFP_KERNEL);
	if (!hw_byrp->cur_iq_set.regs) {
		mserr_hw("failed to alloc cur_iq_set.regs", instance, hw_ip);
		ret = -ENOMEM;
		goto err_cur_regs_alloc;
	}

	clear_bit(CR_SET_CONFIG, &hw_byrp->iq_set.state);
	set_bit(CR_SET_EMPTY, &hw_byrp->iq_set.state);
	spin_lock_init(&hw_byrp->iq_set.slock);

	ret = __is_hw_byrp_init_pcc(hw_ip);
	if (ret) {
		mserr_hw("failed to__is_hw_byrp_init_pcc: %d", instance, hw_ip, ret);
		goto err_pcc_init;
	}

	ret = _pablo_hw_byrp_cloader_init(hw_ip, instance);
	if (ret) {
		mserr_hw("failed _pablo_hw_byrp_cloader_init: %d", instance, hw_ip, ret);
		goto err_cloader_init;
	}

	ret = __is_hw_byrp_init_dma(hw_ip);
	if (ret) {
		mserr_hw("failed __is_hw_byrp_init_dma: %d", instance, hw_ip, ret);
		goto err_dma_alloc;
	}

	atomic_set(&hw_ip->status.Vvalid, V_BLANK);

	atomic_set(&hw_byrp->isr_run_count, 0);
	init_waitqueue_head(&hw_byrp->isr_wait_queue);

	set_bit(HW_OPEN, &hw_ip->state);

	msdbg_hw(2, "open: framemgr[%s]", instance, hw_ip, hw_ip->framemgr->name);

	return 0;

err_dma_alloc:
	CALL_I_SUBDEV_OPS(&hw_byrp->subdev_cloader, free, &hw_byrp->subdev_cloader);

err_cloader_init:
	CALL_PCC_HW_OPS(hw_byrp->pcc, deinit, hw_byrp->pcc);

err_pcc_init:
	pablo_free(hw_byrp->cur_iq_set.regs);
	hw_byrp->cur_iq_set.regs = NULL;

err_cur_regs_alloc:
	pablo_free(hw_byrp->iq_set.regs);
	hw_byrp->iq_set.regs = NULL;

err_regs_alloc:
	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

err_alloc:
	frame_manager_close(hw_ip->framemgr);
	return ret;
}

static int is_hw_byrp_register_icpu_msg_cb(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct pablo_icpu_adt *icpu_adt = hw_byrp->icpu_adt;
	enum pablo_hic_cmd_id cmd_id;
	int ret;

	cmd_id = PABLO_HIC_CSTAT_CDAF_END;
	ret = CALL_ADT_MSG_OPS(
		icpu_adt, register_response_msg_cb, instance, cmd_id, __is_hw_byrp_cdaf_callback);
	if (ret)
		goto exit;
exit:
	if (ret)
		mserr_hw("icpu_adt register_response_msg_cb fail. cmd_id %d", instance, hw_ip,
			cmd_id);

	return ret;
}

static void is_hw_byrp_unregister_icpu_msg_cb(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct pablo_icpu_adt *icpu_adt = hw_byrp->icpu_adt;

	CALL_ADT_MSG_OPS(
		icpu_adt, register_response_msg_cb, instance, PABLO_HIC_CSTAT_CDAF_END, NULL);
}

static int is_hw_byrp_init(struct is_hw_ip *hw_ip, u32 instance, bool flag, u32 f_type)
{
	int ret = 0;
	struct is_hw_byrp *hw_byrp = NULL;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	hw_byrp->param_set[instance].reprocessing = flag;

	ret = is_hw_byrp_register_icpu_msg_cb(hw_ip, instance);
	if (ret)
		goto err_register_icpu_msg_cb;

	if (hw_ip->group[instance] && hw_ip->group[instance]->device)
		hw_byrp->sensor_itf[instance] =
			is_sensor_get_sensor_interface(hw_ip->group[instance]->device->sensor);
	else
		hw_byrp->sensor_itf[instance] = NULL;

	set_bit(HW_INIT, &hw_ip->state);

	return 0;

err_register_icpu_msg_cb:
	is_hw_byrp_unregister_icpu_msg_cb(hw_ip, instance);

	return ret;
}

static int is_hw_byrp_deinit(struct is_hw_ip *hw_ip, u32 instance)
{
	is_hw_byrp_unregister_icpu_msg_cb(hw_ip, instance);

	if (IS_RUNNING_TUNING_SYSTEM())
		pablo_obte_deinit_3aa(instance);

	return 0;
}

static int is_hw_byrp_close(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp;
	struct is_hardware *hardware;
	int i, max_num;
	int ret = 0;
	struct is_hw_ip *hw_ip_phys;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	hardware = hw_ip->hardware;
	if (!hardware) {
		mserr_hw("hardware is null", instance, hw_ip);
		return -EINVAL;
	}

	max_num = hw_ip->ip_num;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, DEV_HW_BYRP + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num) {
		for (i = max_num - 1; i >= 0; i--) {
			hw_ip_phys = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, DEV_HW_BYRP + i);
			byrp_hw_s_reset(get_base(hw_ip_phys));
		}
	}

	mutex_unlock(&cmn_reg_lock);

	CALL_PCC_HW_OPS(hw_byrp->pcc, deinit, hw_byrp->pcc);

	CALL_I_SUBDEV_OPS(&hw_byrp->subdev_cloader, free, &hw_byrp->subdev_cloader);

	pablo_free(hw_byrp->rdma);
	hw_byrp->rdma = NULL;
	pablo_free(hw_byrp->wdma);
	hw_byrp->wdma = NULL;

	pablo_free(hw_byrp->iq_set.regs);
	hw_byrp->iq_set.regs = NULL;
	pablo_free(hw_byrp->cur_iq_set.regs);
	hw_byrp->cur_iq_set.regs = NULL;

	pablo_free(hw_ip->priv_info);
	hw_ip->priv_info = NULL;

	frame_manager_close(hw_ip->framemgr);
	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int is_hw_byrp_enable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	struct pablo_mmio *pmio;
	struct is_hw_byrp *hw_byrp;
	struct exynos_platform_is *pdata;
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

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	pmio = hw_ip->pmio;
	hw_byrp->event_state = BYRP_INIT;
	pdata = dev_get_platdata(is_get_is_dev());
	max_num = pdata->num_of_ip.byrp;

	mutex_lock(&cmn_reg_lock);
	for (i = 0; i < max_num; i++) {
		struct is_hw_ip *hw_ip_phys =
			CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_ip, DEV_HW_BYRP0 + i);

		if (hw_ip_phys && test_bit(HW_RUN, &hw_ip_phys->state))
			break;
	}

	if (i == max_num)
		byrp_hw_s_init_common(get_base(hw_ip->locomotive));

	mutex_unlock(&cmn_reg_lock);

	hw_ip->pmio_config.cache_type = PMIO_CACHE_FLAT;
	if (pmio_reinit_cache(pmio, &hw_ip->pmio_config)) {
		pmio_cache_set_bypass(pmio, true);
		err("failed to reinit PMIO cache, set bypass");
		return -EINVAL;
	}

	pmio_cache_set_only(pmio, true);

	set_bit(HW_RUN, &hw_ip->state);
	msdbg_hw(2, "enable: done\n", instance, hw_ip);

	return 0;
}

void is_hw_byrp_wait_isr_clear(struct is_hw_byrp *hw_byrp, u32 int_id)
{
	u32 try_cnt;
	u32 status;
	struct pablo_common_ctrl *pcc;

	pcc = hw_byrp->pcc;

	try_cnt = 0;
	while (hw_byrp->pcc_cfg.int_en[int_id] &
		(status = CALL_PCC_OPS(pcc, get_int_status, pcc, int_id, true))) {
		mdelay(1);

		if (++try_cnt >= 1000) {
			err_hw("[BYRP] Failed to wait int%d clear. 0x%x", int_id, status);
			break;
		}

		dbg_hw(2, "[BYRP]%s: try_cnt %d, status 0x%x\n", __func__, try_cnt, status);
	}
}

static void is_hw_byrp_wait_isr_done(struct is_hw_ip *hw_ip)
{
	long timetowait;
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	is_hw_byrp_wait_isr_clear(hw_byrp, PCC_INT_0);
	is_hw_byrp_wait_isr_clear(hw_byrp, PCC_INT_1);

	timetowait = wait_event_timeout(
		hw_byrp->isr_wait_queue, !atomic_read(&hw_byrp->isr_run_count), IS_HW_STOP_TIMEOUT);
	if (!timetowait)
		mserr_hw("wait isr done timeout (%ld)", atomic_read(&hw_ip->instance), hw_ip,
			timetowait);
}

static int is_hw_byrp_stop(struct is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	long timetowait;

	timetowait = wait_event_timeout(
		hw_ip->status.wait_queue, !atomic_read(&hw_ip->status.Vvalid), IS_HW_STOP_TIMEOUT);

	if (!timetowait) {
		mserr_hw("wait FRAME_END timeout (%ld)", instance, hw_ip, timetowait);
		ret = -ETIME;
	}

	is_hw_byrp_wait_isr_done(hw_ip);

	return ret;
}

static void is_hw_byrp_cleanup_stat_buf(struct is_hw_ip *hw_ip, u32 instance)
{
	struct pablo_crta_bufmgr *bufmgr;
	u32 buf_id;

	for (buf_id = PABLO_CRTA_BUF_BASE; buf_id < PABLO_CRTA_BUF_CDAF_MW; buf_id++) {
		bufmgr = pablo_get_crta_bufmgr(buf_id, instance, hw_ip->ch);
		CALL_CRTA_BUFMGR_OPS(bufmgr, flush_buf, 0);
	}
}

static void is_hw_byrp_clear(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw = (struct is_hw_byrp *)hw_ip->priv_info;

	is_hw_byrp_cleanup_stat_buf(hw_ip, instance);
	frame_manager_flush(GET_SUBDEV_I_FRAMEMGR(&hw->subdev_cloader));
	clear_bit(HW_CONFIG, &hw_ip->state);
	hw->event_state = BYRP_INIT;
}

static int is_hw_byrp_disable(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct is_hw_byrp *hw_byrp;
	struct pablo_mmio *pmio;
	struct pablo_common_ctrl *pcc;
	u32 hw_ip_instance;

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	msinfo_hw(
		"byrp_disable: Vvalid(%d)\n", instance, hw_ip, atomic_read(&hw_ip->status.Vvalid));

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	pcc = hw_byrp->pcc;
	pmio = hw_ip->pmio;
	hw_ip_instance = atomic_read(&hw_ip->instance);

	if (hw_ip->run_rsc_state) {
		if (hw_byrp->param_set[instance].reprocessing == false) {
			if ((hw_ip_instance == instance) && (test_bit(HW_CONFIG, &hw_ip->state))) {
				goto out;
			} else if (test_bit(hw_ip_instance, &hw_ip->run_rsc_state) &&
				   hw_byrp->param_set[hw_ip_instance].reprocessing == false) {
				mswarn_hw("Occupied by S%d", instance, hw_ip,
					atomic_read(&hw_ip->instance));
				return -EWOULDBLOCK;
			}
		}
		return ret;
	}

	clear_bit(HW_RUN, &hw_ip->state);
out:
	is_hw_byrp_stop(hw_ip, instance);

	is_hw_byrp_clear(hw_ip, instance);

	/* Disable PMIO Cache */
	pmio_cache_set_only(pmio, false);
	pmio_cache_set_bypass(pmio, true);

	byrp_hw_s_cinfifo(pmio, false);
	CALL_PCC_OPS(pcc, disable, pcc);

	return ret;
}

static int is_hw_byrp_set_config(
	struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *config)
{
	struct is_hw_byrp *hw_byrp;
	struct is_byrp_config *conf = (struct is_byrp_config *)config;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;

	if (hw_byrp->config.sensor_center_x != conf->sensor_center_x ||
		hw_byrp->config.sensor_center_y != conf->sensor_center_y)
		msinfo_hw("[F:%d] BYRP sensor_center(%d x %d)\n", instance, hw_ip, fcount,
			conf->sensor_center_x, conf->sensor_center_y);

	if (hw_byrp->config.thstat_pre_bypass != conf->thstat_pre_bypass)
		msinfo_hw("[F:%d] BYRP thstatpre bypass(%d), grid(%d x %d)\n", instance, hw_ip,
			fcount, conf->thstat_pre_bypass, conf->thstat_pre_grid_w,
			conf->thstat_pre_grid_h);

	if (hw_byrp->config.thstat_awb_bypass != conf->thstat_awb_bypass)
		msinfo_hw("[F:%d] BYRP thstatawb bypass(%d), grid(%d x %d)\n", instance, hw_ip,
			fcount, conf->thstat_awb_bypass, conf->thstat_awb_grid_w,
			conf->thstat_awb_grid_h);

	if (hw_byrp->config.thstat_ae_bypass != conf->thstat_ae_bypass)
		msinfo_hw("[F:%d] BYRP thstatae bypass(%d), grid(%d x %d)\n", instance, hw_ip,
			fcount, conf->thstat_ae_bypass, conf->thstat_ae_grid_w,
			conf->thstat_ae_grid_h);

	if (hw_byrp->config.rgbyhist_bypass != conf->rgbyhist_bypass)
		msinfo_hw("[F:%d] BYRP rgbyhist bypass(%d), bin(%d), hist(%d)\n", instance, hw_ip,
			fcount, conf->rgbyhist_bypass, conf->rgbyhist_bin_num,
			conf->rgbyhist_hist_num);

	if (hw_byrp->config.cdaf_bypass != conf->cdaf_bypass)
		msinfo_hw("[F:%d] BYRP cdaf bypass(%d)\n", instance, hw_ip, fcount,
			conf->cdaf_bypass);

	if (hw_byrp->config.cdaf_mw_bypass != conf->cdaf_mw_bypass)
		msinfo_hw("[F:%d] BYRP cdafmw bypass(%d), grid(%d x %d)\n", instance, hw_ip, fcount,
			conf->cdaf_mw_bypass, conf->cdaf_mw_x, conf->cdaf_mw_y);

	hw_byrp->config = *conf;

	return 0;
}

static int __is_hw_byrp_bypass(struct is_hw_ip *hw_ip)
{
	byrp_hw_s_block_bypass(get_base(hw_ip));

	return 0;
}

static int __is_hw_byrp_update_block_reg(
	struct is_hw_ip *hw_ip, struct byrp_param_set *param_set, u32 instance)
{
	u32 bit_in, bit_out;
	int ret = 0;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	if (param_set->dma_input.cmd)
		bit_in = param_set->dma_input.msb + 1;
	else
		bit_in = param_set->otf_input.bitwidth + 1;
	bit_out = 14;
	byrp_hw_s_bitmask(get_base(hw_ip), bit_in, bit_out);

	ret = __is_hw_byrp_bypass(hw_ip);

	return ret;
}

static void __is_hw_byrp_update_param(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame,
	struct byrp_param_set *param_set)
{
	struct byrp_param *param;
	struct is_param_region *param_region;

	param_set->instance_id = instance;
	param_set->mono_mode = param_set->sensor_config.mono_mode;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		hw_ip->internal_fcount[instance] = frame->fcount;
		byrp_hw_s_internal_shot(param_set);
	} else {
		param_region = frame->parameter;
		param = &param_region->byrp;
		byrp_hw_s_external_shot(param_region, frame->pmap, param_set);
	}
}

static void __is_hw_byrp_s_stat_param(
	struct is_hw_ip *hw_ip, u32 instance, struct byrp_param_set *p_set)
{
	int ret;
	struct pablo_crta_buf_info buf_info = {
		0,
	};
	struct pablo_crta_bufmgr *bufmgr;
	struct param_dma_output *dma_out;
	u32 dma_id;

	for (dma_id = 0; dma_id < BYRP_WDMA_MAX; dma_id++) {
		bufmgr = pablo_get_crta_bufmgr(dma_to_crta_buf[dma_id], instance, hw_ip->ch);

		if (!bufmgr)
			continue;

		buf_info.dva_cstat = 0;
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, p_set->fcount, true, &buf_info);

		if (ret || !buf_info.dva_cstat) {
			msdbg_hw(2, "Not enough frame(%d)", instance, hw_ip, dma_id);
			continue;
		}
		dma_out = byrp_hw_s_stat_cfg(dma_id, buf_info.dva_cstat, p_set);
		if (!dma_out)
			CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
	}
}

static int __is_hw_byrp_s_iq_regs(struct is_hw_ip *hw_ip, u32 instance, struct c_loader_buffer *clb)
{
	struct is_hw_byrp *hw_byrp = hw_ip->priv_info;
	struct is_hw_byrp_iq *iq_set = NULL;
	struct cr_set *regs;
	unsigned long flag;
	u32 regs_size, i;
	bool configured = false;

	iq_set = &hw_byrp->iq_set;

	spin_lock_irqsave(&iq_set->slock, flag);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		regs = iq_set->regs;
		regs_size = iq_set->size;

		if (unlikely(test_bit(BYRP_DBG_DUMP_RTA, &debug_byrp.value))) {
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
				PMIO_SET_R(hw_ip->pmio, regs[i].reg_addr, regs[i].reg_data);
		}

		memcpy(hw_byrp->cur_iq_set.regs, regs, sizeof(struct cr_set) * regs_size);
		hw_byrp->cur_iq_set.size = regs_size;

		set_bit(CR_SET_EMPTY, &iq_set->state);
		configured = true;
	}

	spin_unlock_irqrestore(&iq_set->slock, flag);

	if (!configured) {
		mswarn_hw("[F%d]iq_set is NOT configured. iq_set (%d/0x%lx) cur_iq_set %d",
			instance, hw_ip, atomic_read(&hw_ip->fcount), iq_set->fcount, iq_set->state,
			hw_byrp->cur_iq_set.fcount);
		return -EINVAL;
	}

	return 0;
}

static void is_hw_byrp_s_grid(struct is_hw_ip *hw_ip, struct byrp_param_set *param_set,
	u32 instance, const struct is_frame *frame)
{
	struct is_hw_byrp *hw_byrp;
	struct pablo_mmio *base;
	struct byrp_grid_cfg grid_cfg;
	struct is_byrp_config *cfg;
	u32 start_pos_x, start_pos_y;
	u32 sensor_binning_ratio_x, sensor_binning_ratio_y;
	u32 binned_sensor_width, binned_sensor_height;
	u32 sensor_crop_x, sensor_crop_y, sensor_center_x, sensor_center_y;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	base = get_base(hw_ip);
	cfg = &hw_byrp->config;

	/* GRID configuration for CGRAS */

	if (frame->type == SHOT_TYPE_INTERNAL) {
		sensor_binning_ratio_x = param_set->sensor_config.sensor_binning_ratio_x;
		sensor_binning_ratio_y = param_set->sensor_config.sensor_binning_ratio_y;
	} else {
		sensor_binning_ratio_x = frame->shot->udm.frame_info.sensor_binning[0];
		sensor_binning_ratio_y = frame->shot->udm.frame_info.sensor_binning[1];
		param_set->sensor_config.sensor_binning_ratio_x = sensor_binning_ratio_x;
		param_set->sensor_config.sensor_binning_ratio_y = sensor_binning_ratio_y;
	}

	sensor_center_x = cfg->sensor_center_x;
	sensor_center_y = cfg->sensor_center_y;
	if (sensor_center_x == 0 || sensor_center_y == 0) {
		sensor_center_x = (param_set->sensor_config.calibrated_width >> 1);
		sensor_center_y = (param_set->sensor_config.calibrated_height >> 1);
		msdbg_hw(2, "%s: cal_center(0,0) from DDK. Fix to (%d,%d)", param_set->instance_id,
			hw_ip, __func__, sensor_center_x, sensor_center_y);
	}

	/* Total_binning = sensor_binning * csis_bns_binning */
	grid_cfg.binning_x =
		(1024ULL * sensor_binning_ratio_x * param_set->sensor_config.bns_binning_ratio_x) /
		1000 / 1000;
	grid_cfg.binning_y =
		(1024ULL * sensor_binning_ratio_y * param_set->sensor_config.bns_binning_ratio_y) /
		1000 / 1000;

	/* Step */
	grid_cfg.step_x = cfg->lsc_step_x;
	grid_cfg.step_y = cfg->lsc_step_y;

	/* Total_crop = unbinned_sensor_crop */
	if (param_set->sensor_config.freeform_sensor_crop_enable == 1) {
		sensor_crop_x = param_set->sensor_config.freeform_sensor_crop_offset_x;
		sensor_crop_y = param_set->sensor_config.freeform_sensor_crop_offset_y;
	} else {
		binned_sensor_width =
			param_set->sensor_config.calibrated_width * 1000 / sensor_binning_ratio_x;
		binned_sensor_height =
			param_set->sensor_config.calibrated_height * 1000 / sensor_binning_ratio_y;
		sensor_crop_x =
			((binned_sensor_width - param_set->sensor_config.width) >> 1) & (~0x1);
		sensor_crop_y =
			((binned_sensor_height - param_set->sensor_config.height) >> 1) & (~0x1);
	}

	start_pos_x = (sensor_crop_x * sensor_binning_ratio_x) / 1000;
	start_pos_y = (sensor_crop_y * sensor_binning_ratio_y) / 1000;

	grid_cfg.crop_x = start_pos_x * grid_cfg.step_x;
	grid_cfg.crop_y = start_pos_y * grid_cfg.step_y;
	grid_cfg.crop_radial_x = (u16)((-1) * (sensor_center_x - start_pos_x));
	grid_cfg.crop_radial_y = (u16)((-1) * (sensor_center_y - start_pos_y));

	byrp_hw_s_grid_cfg(base, &grid_cfg);

	msdbg_hw(2,
		"%s:[CGR] dbg: calibrated_size(%dx%d), cal_center(%d,%d), sensor_crop(%d,%d), start_pos(%d,%d)\n",
		instance, hw_ip, __func__, param_set->sensor_config.calibrated_width,
		param_set->sensor_config.calibrated_height, cfg->sensor_center_x,
		cfg->sensor_center_y, sensor_crop_x, sensor_crop_y, start_pos_x, start_pos_y);

	msdbg_hw(2, "%s:[CGR] sfr: binning(%dx%d), step(%dx%d), crop(%d,%d), crop_radial(%d,%d)\n",
		instance, hw_ip, __func__, grid_cfg.binning_x, grid_cfg.binning_y, grid_cfg.step_x,
		grid_cfg.step_y, grid_cfg.crop_x, grid_cfg.crop_y, grid_cfg.crop_radial_x,
		grid_cfg.crop_radial_y);
}

static void __is_hw_byrp_set_size_regs(struct is_hw_ip *hw_ip, struct byrp_param_set *param_set,
	u32 instance, const struct is_frame *frame, struct is_rectangle *chain_size)
{
	struct param_sensor_config *sensor_config = &param_set->sensor_config;
	struct is_hw_size_config size_config;
	u32 binned_sensor_width, binned_sensor_height;

	if (param_set->dma_input.cmd == DMA_INPUT_COMMAND_ENABLE) {
		chain_size->w = param_set->dma_input.width;
		chain_size->h = param_set->dma_input.height;
	} else {
		chain_size->w = param_set->otf_input.width;
		chain_size->h = param_set->otf_input.height;
	}

	size_config.sensor_calibrated_width = sensor_config->calibrated_width;
	size_config.sensor_calibrated_height = sensor_config->calibrated_height;
	size_config.sensor_binning_x = sensor_config->sensor_binning_ratio_x;
	size_config.sensor_binning_y = sensor_config->sensor_binning_ratio_y;
	binned_sensor_width =
		size_config.sensor_calibrated_width * 1000 / size_config.sensor_binning_x;
	binned_sensor_height =
		size_config.sensor_calibrated_height * 1000 / size_config.sensor_binning_y;
	if (sensor_config->freeform_sensor_crop_enable == true) {
		size_config.sensor_crop_x = (sensor_config->freeform_sensor_crop_offset_x) & (~0x1);
		size_config.sensor_crop_y = (sensor_config->freeform_sensor_crop_offset_y) & (~0x1);
	} else { /* center-aglign - use logical sensor crop offset */
		size_config.sensor_crop_x =
			((binned_sensor_width - sensor_config->width) >> 1) & (~0x1);
		size_config.sensor_crop_y =
			((binned_sensor_height - sensor_config->height) >> 1) & (~0x1);
	}
	size_config.sensor_crop_width = sensor_config->width;
	size_config.sensor_crop_height = sensor_config->height;

	byrp_hw_s_chain_size(get_base(hw_ip), chain_size->w, chain_size->h);

	byrp_hw_s_bcrop_size(get_base(hw_ip), BYRP_BCROP_BYR, 0, 0, chain_size->w, chain_size->h);
	byrp_hw_s_bcrop_size(get_base(hw_ip), BYRP_BCROP_DNG, 0, 0, chain_size->w, chain_size->h);

	byrp_hw_s_mcb_size(get_base(hw_ip), chain_size->w, chain_size->h);
	byrp_hw_s_disparity_size(get_base(hw_ip), &size_config);
	is_hw_byrp_s_grid(hw_ip, param_set, instance, frame);
}

static int __is_hw_byrp_pmio_config(struct is_hw_ip *hw_ip, u32 instance,
	enum cmdq_setting_mode setting_mode, struct c_loader_buffer *clb)
{
	if (unlikely(!clb))
		setting_mode = CTRL_MODE_APB_DIRECT;

	switch (setting_mode) {
	case CTRL_MODE_DMA_DIRECT:
		pmio_cache_fsync(hw_ip->pmio, (void *)clb, PMIO_FORMATTER_PAIR);

		if (clb->num_of_pairs > 0)
			clb->num_of_headers++;

		if (unlikely(test_bit(BYRP_DBG_DUMP_PMIO, &debug_byrp.value))) {
			pr_info("header dva: 0x%08llx\n", ((dma_addr_t)clb->header_dva) >> 4);
			pr_info("number of headers: %d\n", clb->num_of_headers);
			pr_info("number of pairs: %d\n", clb->num_of_pairs);

			print_hex_dump(KERN_INFO, "header  ", DUMP_PREFIX_OFFSET, 16, 4, clb->clh,
				clb->num_of_headers * 16, false);
			print_hex_dump(KERN_INFO, "payload ", DUMP_PREFIX_OFFSET, 16, 4, clb->clp,
				clb->num_of_headers * 64, false);
		}
		break;
	case CTRL_MODE_APB_DIRECT:
		pmio_cache_sync(hw_ip->pmio);
		break;
	default:
		merr_hw("unsupport setting mode (%d)", instance, setting_mode);
		return -EINVAL;
	}

	return 0;
}

static void _is_hw_byrp_s_cmd(struct is_hw_byrp *hw, struct c_loader_buffer *clb, u32 fcount,
	struct pablo_common_ctrl_cmd *cmd)
{
	cmd->set_mode = PCC_APB_DIRECT;
	cmd->fcount = fcount;
	cmd->int_grp_en = byrp_hw_g_int_grp_en();

	if (!clb)
		return;

	cmd->base_addr = clb->header_dva;
	cmd->header_num = clb->num_of_headers;
	cmd->set_mode = PCC_DMA_DIRECT;
}

static int is_hw_byrp_shot(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map)
{
	struct is_hw_byrp *hw_byrp = NULL;
	struct pablo_mmio *pmio;
	struct byrp_param_set *param_set = NULL;
	struct is_frame *dma_frame;
	struct param_dma_input *pdi;
	struct param_dma_output *pdo;
	struct pablo_common_ctrl *pcc;
	struct pablo_common_ctrl_frame_cfg frame_cfg = {
		0,
	};
	u32 fcount, instance;
	u32 cur_idx;
	int ret = 0, i = 0;
	struct is_rectangle chain_size;
	struct is_framemgr *framemgr;
	struct is_frame *cl_frame;
	struct c_loader_buffer clb = {
		0,
	};
	struct c_loader_buffer *p_clb = NULL;
	enum cmdq_setting_mode setting_mode;
	pdma_addr_t *param_set_dva;
	dma_addr_t *dma_frame_dva;
	char *name;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	enum pablo_common_ctrl_mode pcc_mode;

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

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	pmio = hw_ip->pmio;
	pcc = hw_byrp->pcc;
	param_set = &hw_byrp->param_set[instance];

	if (!test_bit(HW_CONFIG, &hw_ip->state)) {
		pmio_cache_set_bypass(pmio, false);
		pmio_cache_set_only(pmio, true);
		is_hw_byrp_prepare(hw_ip, instance);
	}

	if (param_set->reprocessing)
		pcc_mode = PCC_M2M;
	else
		pcc_mode = PCC_OTF;
	CALL_PCC_HW_OPS(pcc, set_mode, pcc, pcc_mode);

	fcount = frame->fcount;
	dma_frame = frame;

	/* Prepare C_Loader buffer */
	framemgr = GET_SUBDEV_I_FRAMEMGR(&hw_byrp->subdev_cloader);
	cl_frame = peek_frame(framemgr, FS_FREE);
	if (likely(cl_frame)) {
		clb.header_dva = cl_frame->dvaddr_buffer[0];
		clb.payload_dva = cl_frame->dvaddr_buffer[0] + hw_byrp->header_size;
		clb.clh = (struct c_loader_header *)cl_frame->kvaddr_buffer[0];
		clb.clp = (struct c_loader_payload *)(cl_frame->kvaddr_buffer[0] +
						      hw_byrp->header_size);

		p_clb = &clb;
		trans_frame(framemgr, cl_frame, FS_PROCESS);
	} else {
		mswarn_hw("[F%d]Failed to get cl_frame", instance, hw_ip, frame->fcount);
		frame_manager_print_queues(framemgr);
	}

	__is_hw_byrp_update_param(hw_ip, instance, frame, param_set);

	if (param_set->dma_input.cmd)
		byrp_hw_s_sr(get_base(hw_ip), true, hw_ip->ch);

	/* DMA settings */
	cur_idx = frame->cur_buf_index;
	setting_mode = CTRL_MODE_DMA_DIRECT;
	name = __getname();
	if (!name) {
		mserr_hw("Failed to get name buffer", instance, hw_ip);
		return -ENOMEM;
	}

	for (i = 0; i < hw_byrp->rdma_param_max_cnt; i++) {
		if (byrp_hw_g_rdma_param_ptr(
			    i, dma_frame, param_set, name, &dma_frame_dva, &pdi, &param_set_dva))
			continue;

		CALL_HW_OPS(hw_ip, dma_cfg, name, hw_ip, frame, cur_idx, frame->num_buffers,
			&pdi->cmd, pdi->plane, param_set_dva, dma_frame_dva);
	}

	for (i = 0; i < hw_byrp->wdma_param_max_cnt; i++) {
		if (byrp_hw_g_wdma_param_ptr(
			    i, dma_frame, param_set, name, &dma_frame_dva, &pdo, &param_set_dva))
			continue;

		CALL_HW_OPS(hw_ip, dma_cfg, name, hw_ip, frame, cur_idx, frame->num_buffers,
			&pdo->cmd, pdo->plane, param_set_dva, dma_frame_dva);
	}
	__putname(name);

	param_set->instance_id = instance;
	param_set->fcount = fcount;

	/* FRO */
	hw_ip->num_buffers = frame->num_buffers;
	frame_cfg.num_buffers = frame->num_buffers;

	msdbgs_hw(2, "[F%d] batch(%d, %d)", instance, hw_ip, frame->fcount, frame_cfg.num_buffers,
		cur_idx);

	hw_byrp->post_frame_gap =
		test_bit(HW_CONFIG, &hw_ip->state) ? frame->time_cfg.post_frame_gap : 0;

	if (is_debug_support_crta()) {
		if (likely(!test_bit(hw_ip->id, &debug_iq))) {
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);
			ret = __is_hw_byrp_s_iq_regs(hw_ip, instance, p_clb);
			CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);
			if (ret)
				mserr_hw("__is_hw_byrp_s_iq_regs is fail", instance, hw_ip);
		} else {
			msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		}
	}

	if (unlikely(!hw_byrp->cur_iq_set.size)) {
		__is_hw_byrp_update_block_reg(hw_ip, param_set, instance);
		is_hw_byrp_set_config(hw_ip, 0, instance, frame->fcount, &conf_default);
	}

	byrp_hw_s_dma_cfg(param_set, &hw_byrp->config);
	__is_hw_byrp_s_stat_param(hw_ip, instance, param_set);

	byrp_hw_s_core(get_base(hw_ip), frame->num_buffers, param_set);

	byrp_hw_s_path(get_base(hw_ip), param_set, &frame_cfg);

	__is_hw_byrp_set_size_regs(hw_ip, param_set, instance, frame, &chain_size);

	for (i = 0; i < hw_byrp->rdma_max_cnt; i++)
		byrp_hw_s_rdma_init(&hw_byrp->rdma[i], param_set, hw_ip->num_buffers);

	for (i = 0; i < hw_byrp->wdma_max_cnt; i++)
		byrp_hw_s_wdma_init(&hw_byrp->wdma[i], param_set, hw_ip->num_buffers);

	if (unlikely(test_bit(BYRP_DBG_DTP, &debug_byrp.value)))
		byrp_hw_s_dtp(get_base(hw_ip), true, chain_size.w, chain_size.h);

	if (param_set->control.strgen == CONTROL_COMMAND_START) {
		msdbg_hw(2, "STRGEN input\n", instance, hw_ip);
		byrp_hw_s_strgen(get_base(hw_ip));
	}

	__is_hw_byrp_pmio_config(hw_ip, instance, setting_mode, p_clb);

	/* Flush Host CPU cache */
	if (likely(cl_frame)) {
		struct is_priv_buf *pb = cl_frame->pb_output;

		CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);
		trans_frame(framemgr, cl_frame, FS_FREE);
	}

	_is_hw_byrp_s_cmd(hw_byrp, p_clb, fcount, &frame_cfg.cmd);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, fcount, DEBUG_POINT_ADD_TO_CMDQ);

	set_bit(HW_CONFIG, &hw_ip->state);

	CALL_PCC_OPS(pcc, shot, pcc, &frame_cfg);

	return ret;
}

static int is_hw_byrp_frame_ndone(
	struct is_hw_ip *hw_ip, struct is_frame *frame, enum ShotErrorType done_type)
{
	int ret = 0;
	int output_id;

	output_id = IS_HW_CORE_END;
	if (test_bit(hw_ip->id, &frame->core_flag))
		ret = CALL_HW_OPS(hw_ip, frame_done, hw_ip, frame, -1, output_id, done_type, false);

	return ret;
}

int is_hw_byrp_restore(struct is_hw_ip *hw_ip, u32 instance)
{
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

	is_hw_byrp_prepare(hw_ip, instance);

	return 0;
}

int is_hw_byrp_dump_regs(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs,
	u32 regs_size, enum is_reg_dump_type dump_type)
{
	struct is_common_dma *dma;
	struct is_hw_byrp *hw_byrp = NULL;
	u32 i;

	switch (dump_type) {
	case IS_REG_DUMP_TO_LOG:
		byrp_hw_dump(hw_ip->pmio, HW_DUMP_CR);
		break;
	case IS_REG_DUMP_DMA:
		hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
		for (i = 0; i < hw_byrp->rdma_max_cnt; i++) {
			dma = &hw_byrp->rdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}

		for (i = 0; i < hw_byrp->wdma_max_cnt; i++) {
			dma = &hw_byrp->wdma[i];
			CALL_DMA_OPS(dma, dma_print_info, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int is_hw_byrp_set_regs(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount,
	struct cr_set *regs, u32 regs_size)
{
	struct is_hw_byrp *hw_byrp;
	struct is_hw_byrp_iq *iq_set;
	ulong flag = 0;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	iq_set = &hw_byrp->iq_set;

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

static int is_hw_byrp_change_chain(
	struct is_hw_ip *hw_ip, u32 instance, u32 next_id, struct is_hardware *hardware)
{
	int ret = 0;
	struct is_hw_byrp *hw, *next_hw;
	u32 curr_id;
	u32 next_hw_id = DEV_HW_BYRP0 + next_id;
	struct is_hw_ip *next_hw_ip;

	curr_id = hw_ip->ch;
	if (curr_id == next_id) {
		mswarn_hw("Same chain (curr:%d, next:%d)", instance, hw_ip, curr_id, next_id);
		goto p_err;
	}

	hw = (struct is_hw_byrp *)hw_ip->priv_info;
	if (!hw) {
		mserr_hw("failed to get HW BYRP", instance, hw_ip);
		return -ENODEV;
	}

	next_hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, next_hw_id);
	if (!next_hw_ip) {
		merr_hw("[ID:%d]invalid next next_hw_id", instance, next_hw_id);
		return -EINVAL;
	}

	next_hw = (struct is_hw_byrp *)next_hw_ip->priv_info;
	if (!next_hw) {
		mserr_hw("failed to get next HW CSTAT", instance, next_hw_ip);
		return -ENODEV;
	}

	if (!test_and_clear_bit(instance, &hw_ip->run_rsc_state))
		mswarn_hw("try to disable disabled instance", instance, hw_ip);

	ret = is_hw_byrp_disable(hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_cstat_disable is fail ret(%d)", instance, hw_ip, ret);
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

	ret = is_hw_byrp_enable(next_hw_ip, instance, hardware->hw_map[instance]);
	if (ret) {
		msinfo_hw("is_hw_byrp_enable is fail", instance, next_hw_ip);
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

static int is_hw_byrp_notify_timeout(struct is_hw_ip *hw_ip, u32 instance)
{
	struct is_hw_byrp *hw_byrp;
	struct pablo_common_ctrl *pcc;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	if (!hw_byrp) {
		mserr_hw("failed to get HW BYRP", instance, hw_ip);
		return -ENODEV;
	}

	pcc = hw_byrp->pcc;

	CALL_PCC_OPS(pcc, dump, pcc, PCC_DUMP_LIGHT);
	byrp_hw_dump(hw_ip->pmio, HW_DUMP_DBG_STATE);

	return 0;
}

static void __is_hw_byrp_hw_g_pcfi(struct is_hw_ip *hw_ip, u32 instance, struct is_frame *frame,
	struct pablo_rta_frame_info *prfi)
{
	struct is_hw_byrp *hw_byrp;
	struct byrp_param *byrp_p;

	hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	byrp_p = &frame->parameter->byrp;

	prfi->sensor_calibration_size.width = frame->shot->udm.frame_info.sensor_size[0];
	prfi->sensor_calibration_size.height = frame->shot->udm.frame_info.sensor_size[1];
	prfi->sensor_binning.x = frame->shot->udm.frame_info.sensor_binning[0];
	prfi->sensor_binning.y = frame->shot->udm.frame_info.sensor_binning[1];
	prfi->sensor_crop.offset.x = frame->shot->udm.frame_info.sensor_crop_region[0];
	prfi->sensor_crop.offset.y = frame->shot->udm.frame_info.sensor_crop_region[1];
	prfi->sensor_crop.size.width = frame->shot->udm.frame_info.sensor_crop_region[2];
	prfi->sensor_crop.size.height = frame->shot->udm.frame_info.sensor_crop_region[3];

	prfi->csis_bns_binning.x = frame->shot->udm.frame_info.bns_binning[0];
	prfi->csis_bns_binning.y = frame->shot->udm.frame_info.bns_binning[1];
	prfi->csis_mcb_binning.x = 1000;
	prfi->csis_mcb_binning.y = 1000;
	prfi->batch_num = frame->num_buffers;

	if (byrp_p->otf_input.cmd == OTF_INPUT_COMMAND_ENABLE) {
		prfi->byrp_input_bit = byrp_p->otf_input.bitwidth;
		prfi->byrp_input_size.width = byrp_p->otf_input.width;
		prfi->byrp_input_size.height = byrp_p->otf_input.height;
	} else {
		prfi->byrp_input_bit = byrp_p->dma_input.msb + 1;
		prfi->byrp_input_size.width = byrp_p->dma_input.width;
		prfi->byrp_input_size.height = byrp_p->dma_input.height;
	}

	if (byrp_p->dma_output_byr.cmd)
		prfi->byrpZslBit = byrp_p->dma_output_byr.msb + 1;

	prfi->byrp_crop_in.offset.x = 0;
	prfi->byrp_crop_in.offset.y = 0;
	prfi->byrp_crop_in.size.width = prfi->byrp_input_size.width;
	prfi->byrp_crop_in.size.height = prfi->byrp_input_size.height;

	prfi->magic = PABLO_CRTA_MAGIC_NUMBER;
}

static void is_hw_byrp_query(struct is_hw_ip *ip, u32 instance, u32 type, void *in, void *out)
{
	switch (type) {
	case PABLO_QUERY_GET_PCFI:
		__is_hw_byrp_hw_g_pcfi(
			ip, instance, (struct is_frame *)in, (struct pablo_rta_frame_info *)out);
		break;
	default:
		break;
	}
}

static int is_hw_byrp_cmp_fcount(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct pablo_common_ctrl *pcc;

	if (!hw_byrp)
		return 0;

	pcc = hw_byrp->pcc;
	return CALL_PCC_OPS(pcc, cmp_fcount, pcc, fcount);
}

static int is_hw_byrp_recover(struct is_hw_ip *hw_ip, u32 fcount)
{
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct pablo_common_ctrl *pcc;

	if (!hw_byrp)
		return 0;

	pcc = hw_byrp->pcc;
	return CALL_PCC_OPS(pcc, recover, pcc, fcount);
}

static size_t is_hw_byrp_dump_params(struct is_hw_ip *hw_ip, u32 instance, char *buf, size_t size)
{
	struct is_hw_byrp *hw_byrp = (struct is_hw_byrp *)hw_ip->priv_info;
	struct byrp_param_set *param;
	size_t rem = size;
	char *p = buf;

	param = &hw_byrp->param_set[instance];

	p = pablo_json_nstr(p, "hw name", hw_ip->name, strlen(hw_ip->name), &rem);
	p = pablo_json_uint(p, "hw id", hw_ip->id, &rem);

	p = dump_param_sensor_config(p, "sensor_config", &param->sensor_config, &rem);
	p = dump_param_otf_input(p, "otf_input", &param->otf_input, &rem);
	p = dump_param_dma_input(p, "dma_input", &param->dma_input, &rem);
	p = dump_param_otf_output(p, "otf_output", &param->otf_output, &rem);
	p = dump_param_dma_output(p, "dma_output_byr", &param->dma_output_byr, &rem);
	p = dump_param_dma_output(p, "dma_output_pre", &param->dma_output_pre, &rem);
	p = dump_param_dma_output(p, "dma_output_cdaf", &param->dma_output_cdaf, &rem);
	p = dump_param_dma_output(p, "dma_output_rgby", &param->dma_output_rgby, &rem);
	p = dump_param_dma_output(p, "dma_output_ae", &param->dma_output_ae, &rem);
	p = dump_param_dma_output(p, "dma_output_ae", &param->dma_output_awb, &rem);

	p = pablo_json_uint(p, "instance_id", param->instance_id, &rem);
	p = pablo_json_uint(p, "fcount", param->fcount, &rem);
	p = pablo_json_uint(p, "mono_mode", param->mono_mode, &rem);
	p = pablo_json_bool(p, "reprocessing", param->reprocessing, &rem);

	return WRITTEN(size, rem);
}

const struct is_hw_ip_ops is_hw_byrp_ops = {
	.open = is_hw_byrp_open,
	.init = is_hw_byrp_init,
	.deinit = is_hw_byrp_deinit,
	.close = is_hw_byrp_close,
	.enable = is_hw_byrp_enable,
	.disable = is_hw_byrp_disable,
	.shot = is_hw_byrp_shot,
	.frame_ndone = is_hw_byrp_frame_ndone,
	.restore = is_hw_byrp_restore,
	.dump_regs = is_hw_byrp_dump_regs,
	.set_regs = is_hw_byrp_set_regs,
	.set_config = is_hw_byrp_set_config,
	.change_chain = is_hw_byrp_change_chain,
	.notify_timeout = is_hw_byrp_notify_timeout,
	.reset = is_hw_byrp_reset,
	.wait_idle = is_hw_byrp_wait_idle,
	.query = is_hw_byrp_query,
	.cmp_fcount = is_hw_byrp_cmp_fcount,
	.recover = is_hw_byrp_recover,
	.dump_params = is_hw_byrp_dump_params,
};

int is_hw_byrp_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name)
{
	int hw_slot;
	int ret = 0;

	hw_ip->ops = &is_hw_byrp_ops;

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &is_hw_byrp_handle_interrupt0;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].id = INTR_HWIP1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = true;

	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].handler = &is_hw_byrp_handle_interrupt1;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].id = INTR_HWIP2;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = true;

	hw_ip->mmio_base = hw_ip->regs[REG_SETA];

	hw_ip->pmio_config.name = "byrp";

	hw_ip->pmio_config.mmio_base = hw_ip->regs[REG_SETA];
	hw_ip->pmio_config.phys_base = hw_ip->regs_start[REG_SETA];

	hw_ip->pmio_config.cache_type = PMIO_CACHE_NONE;
	hw_ip->pmio_config.ignore_phys_base = true;

	byrp_hw_init_pmio_config(&hw_ip->pmio_config);

	hw_ip->pmio = pmio_init(NULL, NULL, &hw_ip->pmio_config);
	if (IS_ERR(hw_ip->pmio)) {
		err("failed to init byrp PMIO: %ld", PTR_ERR(hw_ip->pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(hw_ip->pmio, &hw_ip->pmio_fields, hw_ip->pmio_config.fields,
		hw_ip->pmio_config.num_fields);
	if (ret) {
		err("failed to alloc byrp PMIO fields: %d", ret);
		pmio_exit(hw_ip->pmio);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(is_hw_byrp_probe);

void is_hw_byrp_remove(struct is_hw_ip *hw_ip)
{
	struct is_interface_ischain *itfc = hw_ip->itfc;
	int id = hw_ip->id;
	int hw_slot = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_hw_slot_id, id);

	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].valid = false;
	itfc->itf_ip[hw_slot].handler[INTR_HWIP2].valid = false;

	pmio_field_bulk_free(hw_ip->pmio, hw_ip->pmio_fields);
	pmio_exit(hw_ip->pmio);
}
EXPORT_SYMBOL_GPL(is_hw_byrp_remove);
