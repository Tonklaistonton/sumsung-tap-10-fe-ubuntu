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

#include <linux/kernel.h>

#include "is-hw-common-dma.h"
#include "pablo-utc.h"

#define DMA_DATA_FORMAT_OFFSET 0x10
#define DMA_WIDTH_OFFSET 0x20
#define DMA_HEIGHT_OFFSET 0x24

static int putc_check_dma_in(const void *expect, const void *acture)
{
	struct param_dma_input *exp_data = (struct param_dma_input *)expect;
	struct param_dma_input *act_data = (struct param_dma_input *)acture;
	u32 sbwc_en, comp_64b_align;
	u32 fmt;
	int ret = 0;

	if (!exp_data->cmd || !act_data->cmd || exp_data->cmd != act_data->cmd)
		return P_FAIL;

	sbwc_en = is_hw_dma_get_comp_sbwc_en(exp_data->sbwc_type, &comp_64b_align);
	ret = is_hw_dma_get_bayer_format(
	    exp_data->bitwidth, exp_data->msb + 1, exp_data->format, sbwc_en, true, &fmt);

	if (!ret) {
		if ((exp_data->width == act_data->width) &&
			(exp_data->height == act_data->height) && (fmt == act_data->format))
			return P_PASS;
		else
			return P_FAIL;
	} else {
		return P_FAIL;
	}

	return 0;
}

int putc_check_result(const enum putc_cmd utc_cmd, const void *expect, const void *acture)
{
	int ret = P_NO_RUN;

	if (!expect || !acture)
		return P_FAIL;

	switch (utc_cmd) {
	case P_B_RDMA:
		ret = putc_check_dma_in(expect, acture);
		break;
	default:
		break;
	}

	return ret;
}

int putc_get_result(char *buffer, size_t buf_size, struct putc_info *utc_list, int utc_num)
{
	int i, ret;

	ret = scnprintf(buffer, buf_size, "\n[UTC]\n");
	for (i = 0; i < utc_num; i++) {
		ret += scnprintf(buffer + ret, buf_size - ret, "%d:%d\n", utc_list[i].camdev_id,
			utc_list[i].result);
	}

	return ret;
}

int putc_get_dma_in(void *hardware_ip, void *dma, u32 dma_offset)
{
	struct is_hw_ip *hw_ip;
	void __iomem *base_addr;
	struct param_dma_input *act_dma;
	struct is_reg reg_data;

	pr_info("get dma_in of bayer type\n");
	if (!hardware_ip || !dma)
		return -EINVAL;

	hw_ip = (struct is_hw_ip *)hardware_ip;
	base_addr = hw_ip->regs[REG_SETA] + 0x8000;
	act_dma = (struct param_dma_input *)dma;

	reg_data.sfr_offset = dma_offset;
	act_dma->cmd = is_hw_get_reg(base_addr, &reg_data);

	reg_data.sfr_offset = dma_offset + DMA_DATA_FORMAT_OFFSET;
	act_dma->format = is_hw_get_reg(base_addr, &reg_data) & 0x1F;

	reg_data.sfr_offset = dma_offset + DMA_WIDTH_OFFSET;
	act_dma->width = is_hw_get_reg(base_addr, &reg_data);

	reg_data.sfr_offset = dma_offset + DMA_HEIGHT_OFFSET;
	act_dma->height = is_hw_get_reg(base_addr, &reg_data);

	return 0;
}
