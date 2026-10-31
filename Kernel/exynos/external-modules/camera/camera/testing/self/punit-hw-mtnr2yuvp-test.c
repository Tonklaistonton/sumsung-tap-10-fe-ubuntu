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

#include <linux/module.h>

#include "punit-test-hw-ip.h"
#include "is-core.h"

static int pst_set_hw_mtnr2yuvp(const char *val, const struct kernel_param *kp);
static int pst_get_hw_mtnr2yuvp(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_mtnr2yuvp = {
	.set = pst_set_hw_mtnr2yuvp,
	.get = pst_get_hw_mtnr2yuvp,
};
module_param_cb(test_hw_mtnr2yuvp, &pablo_param_ops_hw_mtnr2yuvp, NULL, 0644);

#define NUM_OF_B2M_PARAM (sizeof(struct m2y_param)/PARAMETER_MAX_SIZE)

static struct is_frame *frame_m2y;
static u32 m2y_param[NUM_OF_B2M_PARAM][PARAMETER_MAX_MEMBER];

static const struct m2y_param m2y_param_preset[] = {
	/* 12 MP full size */
	[0].sz_mtnr2yuvp.x_start = 0,
	[0].sz_mtnr2yuvp.y_start = 0,
	[0].sz_mtnr2yuvp.w_start = 1920,
	[0].sz_mtnr2yuvp.h_start = 1440,
	[0].sz_mtnr2yuvp.x_mtnr_l0 = 0,
	[0].sz_mtnr2yuvp.y_mtnr_l0 = 0,
	[0].sz_mtnr2yuvp.w_mtnr_l0 = 1920,
	[0].sz_mtnr2yuvp.h_mtnr_l0 = 1440,
	[0].sz_mtnr2yuvp.x_mtnr_l1 = 0,
	[0].sz_mtnr2yuvp.y_mtnr_l1 = 0,
	[0].sz_mtnr2yuvp.w_mtnr_l1 = 1920,
	[0].sz_mtnr2yuvp.h_mtnr_l1 = 1440,
	[0].sz_mtnr2yuvp.x_mtnr_l2 = 0,
	[0].sz_mtnr2yuvp.y_mtnr_l2 = 0,
	[0].sz_mtnr2yuvp.w_mtnr_l2 = 1920,
	[0].sz_mtnr2yuvp.h_mtnr_l2 = 1440,
	[0].sz_mtnr2yuvp.x_mtnr_l3 = 0,
	[0].sz_mtnr2yuvp.y_mtnr_l3 = 0,
	[0].sz_mtnr2yuvp.w_mtnr_l3 = 1920,
	[0].sz_mtnr2yuvp.h_mtnr_l3 = 1440,
	[0].sz_mtnr2yuvp.x_mtnr_l4 = 0,
	[0].sz_mtnr2yuvp.y_mtnr_l4 = 0,
	[0].sz_mtnr2yuvp.w_mtnr_l4 = 1920,
	[0].sz_mtnr2yuvp.h_mtnr_l4 = 1440,
	[0].sz_mtnr2yuvp.x_msnr = 0,
	[0].sz_mtnr2yuvp.y_msnr = 0,
	[0].sz_mtnr2yuvp.w_msnr = 1920,
	[0].sz_mtnr2yuvp.h_msnr = 1440,
	[0].sz_mtnr2yuvp.x_yuvp = 0,
	[0].sz_mtnr2yuvp.y_yuvp = 0,
	[0].sz_mtnr2yuvp.w_yuvp = 1920,
	[0].sz_mtnr2yuvp.h_yuvp = 1440,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(m2y_param_preset));

static void pst_init_param_mtnr2yuvp(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct m2y_param *preset = m2y_param_preset;
	const struct pst_callback_ops *pst_cb;

	memcpy(m2y_param[i++], (u32 *)&preset[index].sz_mtnr2yuvp, PARAMETER_MAX_SIZE);

	pst_cb = pst_get_hw_mtnr0_cb();
	CALL_PST_CB(pst_cb, init_param, 0, PST_HW_IP_GROUP);

	pst_cb = pst_get_hw_mtnr1_cb();
	CALL_PST_CB(pst_cb, init_param, 0, PST_HW_IP_GROUP);

	pst_cb = pst_get_hw_msnr_cb();
	CALL_PST_CB(pst_cb, init_param, 0, PST_HW_IP_GROUP);

	pst_cb = pst_get_hw_yuvp_cb();
	CALL_PST_CB(pst_cb, init_param, 0, PST_HW_IP_GROUP);
}

static void pst_set_param_mtnr2yuvp(struct is_frame *frame)
{
	int i = 0;
	struct m2y_param *p = (struct m2y_param *)m2y_param;
	const struct pst_callback_ops *pst_cb;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;

	struct mtnr_param *param = &frame->parameter->mtnr;

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_mtnr0_cb();
	CALL_PST_CB(pst_cb, set_size, &p->sz_mtnr2yuvp, &p->sz_mtnr2yuvp);
	CALL_PST_CB(pst_cb, set_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_mtnr1_cb();
	CALL_PST_CB(pst_cb, set_size, &p->sz_mtnr2yuvp, &p->sz_mtnr2yuvp);
	CALL_PST_CB(pst_cb, set_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_msnr_cb();
	CALL_PST_CB(pst_cb, set_size, &p->sz_mtnr2yuvp, &p->sz_mtnr2yuvp);
	CALL_PST_CB(pst_cb, set_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_yuvp_cb();
	CALL_PST_CB(pst_cb, set_size, &p->sz_mtnr2yuvp, &p->sz_mtnr2yuvp);
	CALL_PST_CB(pst_cb, set_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	memset(frame->hw_slot_id, HW_SLOT_MAX, sizeof(frame->hw_slot_id));
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MTNR0);
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MTNR1);
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MSNR);
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_YPP);
}

static void pst_clr_param_mtnr2yuvp(struct is_frame *frame)
{
	const struct pst_callback_ops *pst_cb;
	struct mtnr_param *param = &frame->parameter->mtnr;

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_mtnr0_cb();
	CALL_PST_CB(pst_cb, clr_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_mtnr1_cb();
	CALL_PST_CB(pst_cb, clr_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_msnr_cb();
	CALL_PST_CB(pst_cb, clr_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

	pst_cb = pst_get_hw_yuvp_cb();
	CALL_PST_CB(pst_cb, clr_param, frame);

	param = &frame->parameter->mtnr;
	pr_err("TEST: %s:%d: cmd(%d %d %d %d)", __func__, __LINE__,
			param->rdma_cur_l1.cmd,
			param->rdma_cur_l2.cmd,
			param->rdma_cur_l3.cmd,
			param->rdma_cur_l4.cmd
	      );

}

static void pst_set_rta_info_mtnr2yuvp(struct is_frame *frame, struct size_cr_set *cr_set)
{
}

static const struct pst_callback_ops pst_cb_m2y = {
	.init_param = pst_init_param_mtnr2yuvp,
	.set_param = pst_set_param_mtnr2yuvp,
	.clr_param = pst_clr_param_mtnr2yuvp,
	.set_rta_info = pst_set_rta_info_mtnr2yuvp,
};

static int pst_set_hw_mtnr2yuvp(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_END,
			frame_m2y,
			m2y_param,
			NULL,
			ARRAY_SIZE(m2y_param_preset),
			result,
			&pst_cb_m2y);
}

static int pst_get_hw_mtnr2yuvp(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "MTNR2YUVP", ARRAY_SIZE(m2y_param_preset), result);
}
