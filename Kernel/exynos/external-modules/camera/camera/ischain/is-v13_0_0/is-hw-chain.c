// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v9.1 specific functions
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif

#include "is-config.h"
#include "is-param.h"
#include "is-type.h"
#include "is-core.h"
#include "is-hw-chain.h"
#include "is-device-sensor.h"
#include "is-device-csi.h"
#include "is-device-ischain.h"
#include "is-hw.h"
#include "../../interface/is-interface-library.h"
#include "votf/pablo-votf.h"
#include "is-hw-dvfs.h"
#include "pablo-device-iommu-group.h"
#include "api/is-hw-api-csis_pdp_top.h"
#include "pablo-smc.h"
#include "pablo-crta-interface.h"
#include "pablo-crta-bufmgr.h"
#include "pablo-irq.h"
#include "is-hw-param-debug.h"

#define REQ_START_2_SHOT 2000 /* 2ms */
#define REQ_SHOT_2_END 10000 /* 10ms */
#define REQ_END_2_START 1000 /* 1ms */
#define REQ_CINROW_RATIO 15 /* 15% */
#define DVFS_LEVEL_RATIO 2 /* clock change ratio */
#define FAST_FPS 120 /* 120 fps */
#define REQ_CINROW_RATIO_AT_FAST_FPS 30 /* 30% */

#define CHK_HW_PDP(hw_id) (hw_id >= DEV_HW_PAF0 && hw_id <= DEV_HW_PAF_MAX)
#define CHK_HW_BYRP(hw_id) (hw_id >= DEV_HW_BYRP0 && hw_id <= DEV_HW_BYRP_MAX)
#define CHK_HW_RGBP(hw_id) (hw_id >= DEV_HW_RGBP0 && hw_id <= DEV_HW_RGBP_MAX)
#define CHK_HW_YUVSC(hw_id) (hw_id >= DEV_HW_YUVSC0 && hw_id <= DEV_HW_YUVSC_MAX)
#define CHK_HW_MLSC(hw_id) (hw_id >= DEV_HW_MLSC0 && hw_id <= DEV_HW_MLSC_MAX)

static const enum is_subdev_id subdev_id[GROUP_SLOT_MAX] = {
	[GROUP_SLOT_SENSOR] = ENTRY_SENSOR,
	[GROUP_SLOT_PAF] = ENTRY_PAF,
	[GROUP_SLOT_BYRP] = ENTRY_BYRP,
	[GROUP_SLOT_RGBP] = ENTRY_RGBP,
	[GROUP_SLOT_YUVSC] = ENTRY_YUVSC,
	[GROUP_SLOT_MLSC] = ENTRY_MLSC,
	[GROUP_SLOT_MTNR] = ENTRY_MTNR,
	[GROUP_SLOT_MSNR] = ENTRY_MSNR,
	[GROUP_SLOT_YUVP] = ENTRY_YUVP,
	[GROUP_SLOT_MCS] = ENTRY_MCS,
};

static const char *const subdev_id_name[GROUP_SLOT_MAX] = {
	[GROUP_SLOT_SENSOR] = "SSX",
	[GROUP_SLOT_PAF] = "PXS",
	[GROUP_SLOT_BYRP] = "BYRP",
	[GROUP_SLOT_RGBP] = "RGBP",
	[GROUP_SLOT_YUVSC] = "YUVSC",
	[GROUP_SLOT_MLSC] = "MLSC",
	[GROUP_SLOT_MTNR] = "MTNR",
	[GROUP_SLOT_MSNR] = "MSNR",
	[GROUP_SLOT_YUVP] = "YUVP",
	[GROUP_SLOT_MCS] = "MXS",
};

static const struct is_subdev_ops *(*subdev_ops[GROUP_SLOT_MAX])(void) = {
	[GROUP_SLOT_SENSOR] = pablo_get_is_subdev_sensor_ops,
	[GROUP_SLOT_PAF] = pablo_get_is_subdev_paf_ops,
	[GROUP_SLOT_BYRP] = pablo_get_is_subdev_byrp_ops,
	[GROUP_SLOT_RGBP] = pablo_get_is_subdev_rgbp_ops,
	[GROUP_SLOT_YUVSC] = pablo_get_is_subdev_yuvsc_ops,
	[GROUP_SLOT_MLSC] = pablo_get_is_subdev_mlsc_ops,
	[GROUP_SLOT_MTNR] = pablo_get_is_subdev_mtnr_ops,
	[GROUP_SLOT_MSNR] = pablo_get_is_subdev_msnr_ops,
	[GROUP_SLOT_YUVP] = pablo_get_is_subdev_yuvp_ops,
	[GROUP_SLOT_MCS] = pablo_get_is_subdev_mcs_ops,
};

static struct pablo_rta_frame_info __prfi[IS_STREAM_COUNT];

static const int ioresource_to_hw_id[IORESOURCE_MAX] = {
	[0 ... IORESOURCE_MAX - 1] = DEV_HW_END,
	[IORESOURCE_BYRP0] = DEV_HW_BYRP0,
	[IORESOURCE_BYRP1] = DEV_HW_BYRP1,
	[IORESOURCE_BYRP2] = DEV_HW_BYRP2,
	[IORESOURCE_BYRP3] = DEV_HW_BYRP3,
	[IORESOURCE_BYRP4] = DEV_HW_END, /* DEV_HW_BYRP4 */
	[IORESOURCE_RGBP0] = DEV_HW_RGBP0,
	[IORESOURCE_RGBP1] = DEV_HW_RGBP1,
	[IORESOURCE_RGBP2] = DEV_HW_RGBP2,
	[IORESOURCE_RGBP3] = DEV_HW_RGBP3,
	[IORESOURCE_YUVSC0] = DEV_HW_YUVSC0,
	[IORESOURCE_YUVSC1] = DEV_HW_YUVSC1,
	[IORESOURCE_YUVSC2] = DEV_HW_YUVSC2,
	[IORESOURCE_YUVSC3] = DEV_HW_YUVSC3,
	[IORESOURCE_MLSC0] = DEV_HW_MLSC0,
	[IORESOURCE_MLSC1] = DEV_HW_MLSC1,
	[IORESOURCE_MLSC2] = DEV_HW_MLSC2,
	[IORESOURCE_MLSC3] = DEV_HW_MLSC3,
	[IORESOURCE_MTNR0] = DEV_HW_MTNR0,
	[IORESOURCE_MTNR1] = DEV_HW_MTNR1,
	[IORESOURCE_MSNR] = DEV_HW_MSNR,
	[IORESOURCE_YUVP] = DEV_HW_YPP,
	[IORESOURCE_MCSC] = DEV_HW_MCSC0,
};

static const u32 s2mpu_address_table[SYSMMU_DMAX_S2] = {
	[SYSMMU_S0_BYRP_S2 ... SYSMMU_DMAX_S2 - 1] = 0,
	[SYSMMU_S0_BYRP_S2] = 0x23160054,
	[SYSMMU_S0_CSIS_S2] = 0x23A20054,
	[SYSMMU_S0_ICPU_S2] = 0x248D0054,
	[SYSMMU_S0_MCSC_S2] = 0x25890054,
	[SYSMMU_S0_MLSC_S2] = 0x278A0054,
	[SYSMMU_S0_MTNR_S2] = 0x26920054,
	[SYSMMU_RGBP_S0_S2] = 0x250A0054,
	[SYSMMU_S0_YUVP_S2] = 0x28890054,
};


const int *is_hw_get_ioresource_to_hw_id(void)
{
	return ioresource_to_hw_id;
}

static inline void __is_isr_host(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid)
		intr_hw->handler(intr_hw->id, (void *)itf_hw->hw_ip);
	else
		err_itfc("[ID:%d](1) empty handler!!", itf_hw->id);
}

/*
 * Interrupt handler definitions
 */
/* BYRP */
static irqreturn_t __is_isr1_byrp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp4(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp4(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* RGBP */
static irqreturn_t __is_isr1_rgbp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_rgbp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_rgbp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_rgbp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* YUVSC */
static irqreturn_t __is_isr1_yuvsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_yuvsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_yuvsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_yuvsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MLSC */
static irqreturn_t __is_isr1_mlsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mlsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mlsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mlsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MTNR */
static irqreturn_t __is_isr1_mtnr0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mtnr0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mtnr1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mtnr1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MSNR */
static irqreturn_t __is_isr1_msnr(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_msnr(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* YUVP */
static irqreturn_t __is_isr1_yuvp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MCSC */
static irqreturn_t __is_isr1_mcsc(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mcsc(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/*
 * HW group related functions
 */
void is_hw_get_subdev_info(u32 slot, u32 *id, const char **name, const struct is_subdev_ops **sops)
{
	*id = subdev_id[slot];
	*name = subdev_id_name[slot];
	*sops = subdev_ops[slot]();
}

int is_hw_group_open(void *group_data)
{
	int ret = 0;
	u32 group_id;
	struct is_subdev *leader;
	struct is_group *group;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = group_data;
	leader = &group->leader;
	device = group->device;
	group_id = group->id;

	switch (group_id) {
	case GROUP_ID_SS0:
	case GROUP_ID_SS1:
	case GROUP_ID_SS2:
	case GROUP_ID_SS3:
	case GROUP_ID_SS4:
	case GROUP_ID_SS5:
		leader->constraints_width = GROUP_SENSOR_MAX_WIDTH;
		leader->constraints_height = GROUP_SENSOR_MAX_HEIGHT;
		break;
	case GROUP_ID_PAF0:
	case GROUP_ID_PAF1:
	case GROUP_ID_PAF2:
	case GROUP_ID_PAF3:
		leader->constraints_width = GROUP_PDP_MAX_WIDTH;
		leader->constraints_height = GROUP_PDP_MAX_HEIGHT;
		break;
	case GROUP_ID_BYRP0:
	case GROUP_ID_BYRP1:
	case GROUP_ID_BYRP2:
	case GROUP_ID_BYRP3:
	case GROUP_ID_BYRP4:
	case GROUP_ID_RGBP0:
	case GROUP_ID_RGBP1:
	case GROUP_ID_RGBP2:
	case GROUP_ID_RGBP3:
	case GROUP_ID_YUVSC0:
	case GROUP_ID_YUVSC1:
	case GROUP_ID_YUVSC2:
	case GROUP_ID_YUVSC3:
	case GROUP_ID_MLSC0:
	case GROUP_ID_MLSC1:
	case GROUP_ID_MLSC2:
	case GROUP_ID_MLSC3:
		leader->constraints_width = GROUP_BYRP_MAX_WIDTH;
		leader->constraints_height = GROUP_BYRP_MAX_HEIGHT;
		break;
	case GROUP_ID_MTNR:
	case GROUP_ID_MSNR:
	case GROUP_ID_YUVP:
	case GROUP_ID_MCS0:
		leader->constraints_width = GROUP_MTNR_MAX_WIDTH;
		leader->constraints_height = GROUP_MTNR_MAX_HEIGHT;
		break;
	default:
		merr("(%s) is invalid", group, group_id_name[group_id]);
		break;
	}

	return ret;
}

int is_get_hw_list(int group_id, int *hw_list)
{
	int i;
	int hw_index = 0;

	/* initialization */
	for (i = 0; i < GROUP_HW_MAX; i++)
		hw_list[i] = -1;

	switch (group_id) {
	case GROUP_ID_PAF0:
		hw_list[hw_index] = DEV_HW_PAF0; hw_index++;
		break;
	case GROUP_ID_PAF1:
		hw_list[hw_index] = DEV_HW_PAF1; hw_index++;
		break;
	case GROUP_ID_PAF2:
		hw_list[hw_index] = DEV_HW_PAF2; hw_index++;
		break;
	case GROUP_ID_PAF3:
		hw_list[hw_index] = DEV_HW_PAF3; hw_index++;
		break;
	case GROUP_ID_BYRP0:
		hw_list[hw_index] = DEV_HW_BYRP0; hw_index++;
		break;
	case GROUP_ID_BYRP1:
		hw_list[hw_index] = DEV_HW_BYRP1; hw_index++;
		break;
	case GROUP_ID_BYRP2:
		hw_list[hw_index] = DEV_HW_BYRP2; hw_index++;
		break;
	case GROUP_ID_BYRP3:
		hw_list[hw_index] = DEV_HW_BYRP3; hw_index++;
		break;
	case GROUP_ID_BYRP4:
		hw_list[hw_index] = DEV_HW_BYRP4; hw_index++;
		break;
	case GROUP_ID_RGBP0:
		hw_list[hw_index] = DEV_HW_RGBP0; hw_index++;
		break;
	case GROUP_ID_RGBP1:
		hw_list[hw_index] = DEV_HW_RGBP1; hw_index++;
		break;
	case GROUP_ID_RGBP2:
		hw_list[hw_index] = DEV_HW_RGBP2; hw_index++;
		break;
	case GROUP_ID_RGBP3:
		hw_list[hw_index] = DEV_HW_RGBP3; hw_index++;
		break;
	case GROUP_ID_YUVSC0:
		hw_list[hw_index] = DEV_HW_YUVSC0; hw_index++;
		break;
	case GROUP_ID_YUVSC1:
		hw_list[hw_index] = DEV_HW_YUVSC1; hw_index++;
		break;
	case GROUP_ID_YUVSC2:
		hw_list[hw_index] = DEV_HW_YUVSC2; hw_index++;
		break;
	case GROUP_ID_YUVSC3:
		hw_list[hw_index] = DEV_HW_YUVSC3; hw_index++;
		break;
	case GROUP_ID_MLSC0:
		hw_list[hw_index] = DEV_HW_MLSC0; hw_index++;
		break;
	case GROUP_ID_MLSC1:
		hw_list[hw_index] = DEV_HW_MLSC1; hw_index++;
		break;
	case GROUP_ID_MLSC2:
		hw_list[hw_index] = DEV_HW_MLSC2; hw_index++;
		break;
	case GROUP_ID_MLSC3:
		hw_list[hw_index] = DEV_HW_MLSC3; hw_index++;
		break;
	case GROUP_ID_MTNR:
		hw_list[hw_index] = DEV_HW_MTNR0; hw_index++;
		hw_list[hw_index] = DEV_HW_MTNR1; hw_index++;
		break;
	case GROUP_ID_MSNR:
		hw_list[hw_index] = DEV_HW_MSNR; hw_index++;
		break;
	case GROUP_ID_YUVP:
		hw_list[hw_index] = DEV_HW_YPP; hw_index++;
		break;
	case GROUP_ID_MCS0:
		hw_list[hw_index] = DEV_HW_MCSC0; hw_index++;
		break;
	case GROUP_ID_MAX:
		break;
	default:
		err("Invalid group%d(%s)", group_id, group_id_name[group_id]);
		break;
	}

	return hw_index;
}
/*
 * System registers configurations
 */
static inline int __is_hw_get_address(struct platform_device *pdev,
				struct is_interface_hwip *itf_hwip,
				int hw_id, char *hw_name,
				u32 resource_id, enum base_reg_index reg_index,
				bool alloc_memlog)
{
	struct resource *mem_res = NULL;
	struct device *dev = &pdev->dev;
	phys_addr_t paddr = 0;
	u64 vaddr = 0;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, resource_id);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		return -EINVAL;
	}

	itf_hwip->hw_ip->regs_start[reg_index] = mem_res->start;
	itf_hwip->hw_ip->regs_end[reg_index] = mem_res->end;
	itf_hwip->hw_ip->regs[reg_index] =
		devm_ioremap(dev, mem_res->start, resource_size(mem_res));
	if (!itf_hwip->hw_ip->regs[reg_index]) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		return -EINVAL;
	}

	/* RT path IP doesn't support full-range APB access. */
	if ((hw_id >= DEV_HW_BYRP && hw_id <= DEV_HW_BYRP_MAX) ||
		(hw_id >= DEV_HW_RGBP && hw_id <= DEV_HW_RGBP_MAX) ||
		(hw_id >= DEV_HW_YUVSC0 && hw_id <= DEV_HW_YUVSC_MAX) ||
		(hw_id >= DEV_HW_MLSC0 && hw_id <= DEV_HW_MLSC_MAX))
		vaddr = (u64)&itf_hwip->hw_ip->sfr_dump[reg_index];
	else
		paddr = mem_res->start;

	if (alloc_memlog)
		is_debug_memlog_alloc_dump(paddr, vaddr, resource_size(mem_res), hw_name);

	info_itfc("[ID:%2d] %s VA(0x%lx)\n", hw_id, hw_name,
		(ulong)itf_hwip->hw_ip->regs[reg_index]);

	return 0;
}

int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id)
{
	struct platform_device *pdev = NULL;
	struct is_interface_hwip *itf_hwip = NULL;

	FIMC_BUG(!itfc_data);
	FIMC_BUG(!pdev_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	itf_hwip->hw_ip->dump_for_each_reg = NULL;
	itf_hwip->hw_ip->dump_reg_list_size = 0;

	switch (hw_id) {
	case DEV_HW_BYRP0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP0", IORESOURCE_BYRP0, REG_SETA, true);
		break;
	case DEV_HW_BYRP1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP1", IORESOURCE_BYRP1, REG_SETA, true);
		break;
	case DEV_HW_BYRP2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP2", IORESOURCE_BYRP2, REG_SETA, true);
		break;
	case DEV_HW_BYRP3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP3", IORESOURCE_BYRP3, REG_SETA, true);
		break;
	case DEV_HW_BYRP4:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP4", IORESOURCE_BYRP4, REG_SETA, true);
		break;
	case DEV_HW_RGBP0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP0", IORESOURCE_RGBP0, REG_SETA, true);
		break;
	case DEV_HW_RGBP1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP1", IORESOURCE_RGBP1, REG_SETA, true);
		break;
	case DEV_HW_RGBP2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP2", IORESOURCE_RGBP2, REG_SETA, true);
		break;
	case DEV_HW_RGBP3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP3", IORESOURCE_RGBP3, REG_SETA, true);
		break;
	case DEV_HW_YUVSC0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC0", IORESOURCE_YUVSC0, REG_SETA, true);
		break;
	case DEV_HW_YUVSC1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC1", IORESOURCE_YUVSC1, REG_SETA, true);
		break;
	case DEV_HW_YUVSC2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC2", IORESOURCE_YUVSC2, REG_SETA, true);
		break;
	case DEV_HW_YUVSC3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC3", IORESOURCE_YUVSC3, REG_SETA, true);
		break;
	case DEV_HW_MLSC0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC0", IORESOURCE_MLSC0, REG_SETA, true);
		break;
	case DEV_HW_MLSC1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC1", IORESOURCE_MLSC1, REG_SETA, true);
		break;
	case DEV_HW_MLSC2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC2", IORESOURCE_MLSC2, REG_SETA, true);
		break;
	case DEV_HW_MLSC3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC3", IORESOURCE_MLSC3, REG_SETA, true);
		break;
	case DEV_HW_MTNR0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MTNR0", IORESOURCE_MTNR0, REG_SETA, true);
		break;
	case DEV_HW_MTNR1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MTNR1", IORESOURCE_MTNR1, REG_SETA, true);
		break;
	case DEV_HW_MSNR:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MSNR", IORESOURCE_MSNR, REG_SETA, true);
		break;
	case DEV_HW_YPP:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVP", IORESOURCE_YUVP, REG_SETA, true);
		break;
	case DEV_HW_MCSC0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MCSC", IORESOURCE_MCSC, REG_SETA, true);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return 0;
}

int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_BYRP0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 0);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 1);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 2);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 3);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 4);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 5);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 6);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 7);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP4:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 8);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp4-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 9);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp4-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 10);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 11);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 12);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 13);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 14);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 15);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 16);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 17);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 18);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 19);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 20);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 21);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 22);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 23);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 24);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 25);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 26);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 27);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 28);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 29);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 30);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 31);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 32);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 33);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MTNR0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 34);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mtnr0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 35);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mtnr0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MTNR1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 36);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mtnr1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 37);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mtnr1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MSNR:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 38);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq msnr-0");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 39);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq msnr-1");
			return -EINVAL;
		}
		break;
	case DEV_HW_YPP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 40);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvp-0");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 41);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvp-1");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 42);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 43);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mcsc-1\n");
			return -EINVAL;
		}
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

static inline int __is_hw_request_irq(struct is_interface_hwip *itf_hwip,
	const char *name, int isr_num,
	unsigned int added_irq_flags,
	irqreturn_t (*func)(int, void *))
{
	size_t name_len = 0;
	int ret = 0;

	name_len = sizeof(itf_hwip->irq_name[isr_num]);
	snprintf(itf_hwip->irq_name[isr_num], name_len, "%s-%d", name, isr_num);

	ret = pablo_request_irq(itf_hwip->irq[isr_num], func,
		itf_hwip->irq_name[isr_num],
		added_irq_flags,
		itf_hwip);
	if (ret) {
		err_itfc("[HW:%s] request_irq [%d] fail", name, isr_num);
		return -EINVAL;
	}

	itf_hwip->handler[isr_num].id = isr_num;
	itf_hwip->handler[isr_num].valid = true;

	return ret;
}

int is_hw_request_irq(void *itfc_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;

	switch (hw_id) {
	case DEV_HW_BYRP0:
		ret = __is_hw_request_irq(itf_hwip, "byrp0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp0);
		ret = __is_hw_request_irq(itf_hwip, "byrp0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp0);
		break;
	case DEV_HW_BYRP1:
		ret = __is_hw_request_irq(itf_hwip, "byrp1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp1);
		ret = __is_hw_request_irq(itf_hwip, "byrp1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp1);
		break;
	case DEV_HW_BYRP2:
		ret = __is_hw_request_irq(itf_hwip, "byrp2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp2);
		ret = __is_hw_request_irq(itf_hwip, "byrp2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp2);
		break;
	case DEV_HW_BYRP3:
		ret = __is_hw_request_irq(itf_hwip, "byrp3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp3);
		ret = __is_hw_request_irq(itf_hwip, "byrp3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp3);
		break;
	case DEV_HW_BYRP4:
		ret = __is_hw_request_irq(itf_hwip, "byrp4", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp4);
		ret = __is_hw_request_irq(itf_hwip, "byrp4", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp4);
		break;
	case DEV_HW_RGBP0:
		ret = __is_hw_request_irq(itf_hwip, "rgbp0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp0);
		ret = __is_hw_request_irq(itf_hwip, "rgbp0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp0);
		break;
	case DEV_HW_RGBP1:
		ret = __is_hw_request_irq(itf_hwip, "rgbp1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp1);
		ret = __is_hw_request_irq(itf_hwip, "rgbp1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp1);
		break;
	case DEV_HW_RGBP2:
		ret = __is_hw_request_irq(itf_hwip, "rgbp2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp2);
		ret = __is_hw_request_irq(itf_hwip, "rgbp2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp2);
		break;
	case DEV_HW_RGBP3:
		ret = __is_hw_request_irq(itf_hwip, "rgbp3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp3);
		ret = __is_hw_request_irq(itf_hwip, "rgbp3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp3);
		break;
	case DEV_HW_YUVSC0:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc0);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc0);
		break;
	case DEV_HW_YUVSC1:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc1);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc1);
		break;
	case DEV_HW_YUVSC2:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc2);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc2);
		break;
	case DEV_HW_YUVSC3:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc3);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc3);
		break;
	case DEV_HW_MLSC0:
		ret = __is_hw_request_irq(itf_hwip, "mlsc0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc0);
		ret = __is_hw_request_irq(itf_hwip, "mlsc0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc0);
		break;
	case DEV_HW_MLSC1:
		ret = __is_hw_request_irq(itf_hwip, "mlsc1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc1);
		ret = __is_hw_request_irq(itf_hwip, "mlsc1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc1);
		break;
	case DEV_HW_MLSC2:
		ret = __is_hw_request_irq(itf_hwip, "mlsc2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc2);
		ret = __is_hw_request_irq(itf_hwip, "mlsc2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc2);
		break;
	case DEV_HW_MLSC3:
		ret = __is_hw_request_irq(itf_hwip, "mlsc3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc3);
		ret = __is_hw_request_irq(itf_hwip, "mlsc3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc3);
		break;
	case DEV_HW_MTNR0:
		ret = __is_hw_request_irq(itf_hwip, "mtnr0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mtnr0);
		ret = __is_hw_request_irq(itf_hwip, "mtnr0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mtnr0);
		break;
	case DEV_HW_MTNR1:
		ret = __is_hw_request_irq(itf_hwip, "mtnr1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mtnr1);
		ret = __is_hw_request_irq(itf_hwip, "mtnr1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mtnr1);
		break;
	case DEV_HW_MSNR:
		ret = __is_hw_request_irq(itf_hwip, "msnr", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_msnr);
		ret = __is_hw_request_irq(itf_hwip, "msnr", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_msnr);
		break;
	case DEV_HW_YPP:
		ret = __is_hw_request_irq(itf_hwip, "yuvp", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvp);
		ret = __is_hw_request_irq(itf_hwip, "yuvp", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvp);
		break;
	case DEV_HW_MCSC0:
		ret = __is_hw_request_irq(itf_hwip, "mcsc", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mcsc);
		ret = __is_hw_request_irq(itf_hwip, "mcsc", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mcsc);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

void is_hw_camif_init(void)
{
	const u32 ibuf_mux_base = 0x239D4000;
	const u32 mux_val = 0x3F; /* reset value */
	void __iomem *addr;
	u32 i, offset;

	addr = ioremap(ibuf_mux_base, sizeof(u32) * MAX_NUM_CSIS_OTF_CH);

	for (i = 0, offset = 0; i < MAX_NUM_CSIS_OTF_CH; i++, offset += 4)
		writel(mux_val, addr + offset);

	iounmap(addr);
}

#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
void is_hw_s2mpu_cfg(void)
{
	void __iomem *reg;
	int idx;

	pr_info("[DBG] S2MPU disable\n");

	for (idx = 0; idx < SYSMMU_DMAX_S2; idx++) {
		if (!s2mpu_address_table[idx])
			continue;

		reg = ioremap(s2mpu_address_table[idx], 0x4);
		writel(0xFF, reg);
		iounmap(reg);
	}

}
PST_EXPORT_SYMBOL(is_hw_s2mpu_cfg);
#endif

int is_hw_camif_cfg(void *sensor_data)
{
	return 0;
}

int is_hw_ischain_enable(struct is_core *core)
{
	int ret = 0;
	struct is_interface_hwip *itf_hwip = NULL;
	struct is_interface_ischain *itfc;
	struct is_hardware *hw;
	int hw_slot;
	u32 idx, i;

	itfc = &core->interface_ischain;
	hw = &core->hardware;

	/* irq affinity should be restored after S2R at gic600 */
	for (idx = 0; idx < IORESOURCE_MAX; idx++) {
		if (ioresource_to_hw_id[idx] >= DEV_HW_END)
			continue;

		hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, ioresource_to_hw_id[idx]);
		itf_hwip = &(itfc->itf_ip[hw_slot]);

		for (i = 0; i < INTR_HWIP_MAX; i++) {
			if (!itf_hwip->handler[i].valid)
				continue;

			pablo_set_affinity_irq(itf_hwip->irq[i], true);
		}
	}

	votfitf_disable_service();

#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
	is_hw_s2mpu_cfg();
#endif

	info("%s: complete\n", __func__);

	return ret;
}

#ifdef ENABLE_HWACG_CONTROL
#define NUM_OF_CSIS 7
void is_hw_csi_qchannel_enable_all(bool enable)
{
	phys_addr_t csis_cmn_ctrls[NUM_OF_CSIS] = {
		0x23880004,
		0x23890004,
		0x238A0004,
		0x238B0004,
		0x238C0004,
		0x238D0004,
		0x238E0004,
	};
	phys_addr_t csis_pdp_top = 0x239D0000;
	void __iomem *base;
	u32 val;
	int i;

	if (enable) {
		/* CSIS_LINK */
		for (i = 0; i < NUM_OF_CSIS; i++) {
			base = ioremap(csis_cmn_ctrls[i], SZ_4);
			val = readl(base);
			writel(val | (1 << 20), base);
			iounmap(base);
		}
	}

	/* CSIS_PDP_TOP */
	base = ioremap(csis_pdp_top, 0x20);
	csis_pdp_top_qch_cfg(base, enable);
	iounmap(base);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
static struct is_llc_way_num is_llc_way[IS_LLC_SN_END] = {
	/* default : VOTF 0MB, MTNR 0MB, ICPU 0MB*/
	[IS_LLC_SN_DEFAULT].votf = 0,
	[IS_LLC_SN_DEFAULT].mtnr = 0,
	[IS_LLC_SN_DEFAULT].icpu = 0,
	/* FHD scenario : VOTF 0MB, MTNR 1MB, ICPU 2MB */
	[IS_LLC_SN_FHD].votf = 0,
	[IS_LLC_SN_FHD].mtnr = 1,
	[IS_LLC_SN_FHD].icpu = 2,
	/* UHD scenario : VOTF 1MB, MTNR 1MB, ICPU 2MB */
	[IS_LLC_SN_UHD].votf = 1,
	[IS_LLC_SN_UHD].mtnr = 1,
	[IS_LLC_SN_UHD].icpu = 2,
	/* 8K scenario : VOTF 2MB, MTNR 1MB, ICPU 2MB */
	[IS_LLC_SN_8K].votf = 2,
	[IS_LLC_SN_8K].mtnr = 1,
	[IS_LLC_SN_8K].icpu = 2,
	/* preview scenario : VOTF 0MB, MTNR 1MB, ICPU 2MB */
	[IS_LLC_SN_PREVIEW].votf = 0,
	[IS_LLC_SN_PREVIEW].mtnr = 1,
	[IS_LLC_SN_PREVIEW].icpu = 2,
};
#endif

void is_hw_configure_llc(bool on, void *ischain, ulong *llc_state)
{
#if IS_ENABLED(CONFIG_EXYNOS_SCI)
	struct is_dvfs_scenario_param param;
	u32 votf, mtnr, icpu, size;
	struct is_device_ischain *device = (struct is_device_ischain *)ischain;

	/* way 1 means alloc 1M LLC */
	if (on) {
		is_hw_dvfs_get_scenario_param(device, 0, &param);

		if (param.mode == IS_DVFS_MODE_PHOTO) {
			votf = is_llc_way[IS_LLC_SN_PREVIEW].votf;
			mtnr = is_llc_way[IS_LLC_SN_PREVIEW].mtnr;
			icpu = is_llc_way[IS_LLC_SN_PREVIEW].icpu;
		} else if (param.mode == IS_DVFS_MODE_VIDEO) {
			switch (param.resol) {
			case IS_DVFS_RESOL_FHD:
				votf = is_llc_way[IS_LLC_SN_FHD].votf;
				mtnr = is_llc_way[IS_LLC_SN_FHD].mtnr;
				icpu = is_llc_way[IS_LLC_SN_FHD].icpu;
				break;
			case IS_DVFS_RESOL_UHD:
				votf = is_llc_way[IS_LLC_SN_UHD].votf;
				mtnr = is_llc_way[IS_LLC_SN_UHD].mtnr;
				icpu = is_llc_way[IS_LLC_SN_UHD].icpu;
				break;
			case IS_DVFS_RESOL_8K:
				votf = is_llc_way[IS_LLC_SN_8K].votf;
				mtnr = is_llc_way[IS_LLC_SN_8K].mtnr;
				icpu = is_llc_way[IS_LLC_SN_8K].icpu;
				break;
			default:
				votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
				mtnr = is_llc_way[IS_LLC_SN_DEFAULT].mtnr;
				icpu = is_llc_way[IS_LLC_SN_DEFAULT].icpu;
				break;
			}
		} else {
			votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
			mtnr = is_llc_way[IS_LLC_SN_DEFAULT].mtnr;
			icpu = is_llc_way[IS_LLC_SN_DEFAULT].icpu;
		}

		size = is_get_debug_param(IS_DEBUG_PARAM_LLC);
		if (size) {
			icpu = size / 10000;
			if (icpu > 16)
				icpu = 0;
			votf = (size / 100) % 100;
			if (votf > 16)
				votf = 0;
			mtnr = size % 100;
			if (mtnr > 16)
				mtnr = 0;
		}

		if (votf) {
			llc_region_alloc(LLC_REGION_CAM_CSIS, 1, votf);
			set_bit(LLC_REGION_CAM_CSIS, llc_state);
		}

		if (mtnr) {
			llc_region_alloc(LLC_REGION_CAM_MCFP, 1, mtnr);
			set_bit(LLC_REGION_CAM_MCFP, llc_state);
		}

		if (icpu) {
			llc_region_alloc(LLC_REGION_ICPU, 1, icpu);
			set_bit(LLC_REGION_ICPU, llc_state);
		}

		info("[LLC] alloc, VOTF:%dMB, MTNR:%dMB, ICPU:%dMB", votf, mtnr, icpu);
	} else {
		if (test_and_clear_bit(LLC_REGION_CAM_CSIS, llc_state))
			llc_region_alloc(LLC_REGION_CAM_CSIS, 0, 0);
		if (test_and_clear_bit(LLC_REGION_CAM_MCFP, llc_state))
			llc_region_alloc(LLC_REGION_CAM_MCFP, 0, 0);
		if (test_and_clear_bit(LLC_REGION_ICPU, llc_state))
			llc_region_alloc(LLC_REGION_ICPU, 0, 0);

		info("[LLC] release");
	}

	llc_enable(on);
#endif
}
KUNIT_EXPORT_SYMBOL(is_hw_configure_llc);

void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id)
{
	int bts_index = 0;

	switch (scenario_id) {
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K24:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K24_HF:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30_HF:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD120:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD240:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD480:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD120:
	case IS_DVFS_SN_REAR_SINGLE_SSM:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30_RECURSIVE:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30_RECURSIVE:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30_RECURSIVE:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_SUPERSTEADY:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30_RECURSIVE:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_UHD60:
	case IS_DVFS_SN_PIP_DUAL_PHOTO:
	case IS_DVFS_SN_PIP_DUAL_CAPTURE:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_UHD30:
	case IS_DVFS_SN_TRIPLE_PHOTO:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD60:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD60:
	case IS_DVFS_SN_TRIPLE_CAPTURE:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60:
		bts_index = 1;
		break;
	default:
		bts_index = 0;
		break;
	}

	/* If default scenario & specific scenario selected,
	 * off specific scenario first.
	 */
	if (resourcemgr->cur_bts_scen_idx && bts_index == 0)
		is_bts_scen(resourcemgr, resourcemgr->cur_bts_scen_idx, false);

	if (bts_index && bts_index != resourcemgr->cur_bts_scen_idx)
		is_bts_scen(resourcemgr, bts_index, true);
	resourcemgr->cur_bts_scen_idx = bts_index;
}

int is_hw_get_capture_slot(struct is_frame *frame, dma_addr_t **taddr, u64 **taddr_k, u32 vid)
{
	int ret = 0;
	dma_addr_t *p = NULL;
	u64 *kp = NULL;

	if (taddr)
		*taddr = NULL;
	if (taddr_k)
		*taddr_k = NULL;

	switch (vid) {
	/* BYRP */
	case IS_LVN_BYRP0_BYR:
	case IS_LVN_BYRP1_BYR:
	case IS_LVN_BYRP2_BYR:
	case IS_LVN_BYRP3_BYR:
	case IS_LVN_BYRP4_BYR:
		p = frame->dva_byrp_byr;
		break;
	case IS_LVN_BYRP0_HDR:
	case IS_LVN_BYRP1_HDR:
	case IS_LVN_BYRP2_HDR:
	case IS_LVN_BYRP3_HDR:
	case IS_LVN_BYRP4_HDR:
		p = frame->dva_byrp_hdr;
		break;
	/* RGBP */
	case IS_LVN_RGBP0_DRC:
	case IS_LVN_RGBP1_DRC:
	case IS_LVN_RGBP2_DRC:
	case IS_LVN_RGBP3_DRC:
		p = frame->dva_rgbp_drc;
		break;
	case IS_LVN_RGBP0_HIST:
	case IS_LVN_RGBP1_HIST:
	case IS_LVN_RGBP2_HIST:
	case IS_LVN_RGBP3_HIST:
		p = frame->dva_rgbp_hist;
		break;
	case IS_LVN_RGBP0_SAT:
	case IS_LVN_RGBP1_SAT:
	case IS_LVN_RGBP2_SAT:
	case IS_LVN_RGBP3_SAT:
		p = frame->dva_rgbp_sat;
		break;
	/* YUVP */
	case IS_LVN_YUVP_DRC:
		p = frame->ypdgaTargetAddress;
		break;
	case IS_LVN_YUVP_CLAHE:
		p = frame->ypclaheTargetAddress;
		break;
	case IS_LVN_YUVP_PCCHIST_R:
		p = frame->dva_yuvp_out_pcchist;
		break;
	case IS_LVN_YUVP_PCCHIST_W:
		p = frame->dva_yuvp_cap_pcchist;
		break;
	case IS_LVN_YUVP_SEG:
		p = frame->ixscmapTargetAddress;
		break;
	/* MLSC */
	case IS_LVN_MLSC0_YUV444:
	case IS_LVN_MLSC1_YUV444:
	case IS_LVN_MLSC2_YUV444:
	case IS_LVN_MLSC3_YUV444:
		p = frame->dva_mlsc_yuv444;
		break;
	case IS_LVN_MLSC0_SVHIST:
	case IS_LVN_MLSC1_SVHIST:
	case IS_LVN_MLSC2_SVHIST:
	case IS_LVN_MLSC3_SVHIST:
		p = frame->dva_mlsc_svhist;
		break;
	case IS_LVN_MLSC0_FDPIG:
	case IS_LVN_MLSC1_FDPIG:
	case IS_LVN_MLSC2_FDPIG:
	case IS_LVN_MLSC3_FDPIG:
		p = frame->dva_mlsc_fdpig;
		break;
	case IS_LVN_MLSC0_LMEDS:
	case IS_LVN_MLSC1_LMEDS:
	case IS_LVN_MLSC2_LMEDS:
	case IS_LVN_MLSC3_LMEDS:
		p = frame->dva_mlsc_lmeds;
		break;
	case IS_LVN_MLSC0_CAV:
	case IS_LVN_MLSC1_CAV:
	case IS_LVN_MLSC2_CAV:
	case IS_LVN_MLSC3_CAV:
		p = frame->dva_mlsc_cav;
		break;
	case IS_LVN_MLSC0_GLPG_L0:
	case IS_LVN_MLSC1_GLPG_L0:
	case IS_LVN_MLSC2_GLPG_L0:
	case IS_LVN_MLSC3_GLPG_L0:
		p = frame->dva_mlsc_glpg[0];
		break;
	case IS_LVN_MLSC0_GLPG_L1:
	case IS_LVN_MLSC1_GLPG_L1:
	case IS_LVN_MLSC2_GLPG_L1:
	case IS_LVN_MLSC3_GLPG_L1:
		p = frame->dva_mlsc_glpg[1];
		break;
	case IS_LVN_MLSC0_GLPG_L2:
	case IS_LVN_MLSC1_GLPG_L2:
	case IS_LVN_MLSC2_GLPG_L2:
	case IS_LVN_MLSC3_GLPG_L2:
		p = frame->dva_mlsc_glpg[2];
		break;
	case IS_LVN_MLSC0_GLPG_L3:
	case IS_LVN_MLSC1_GLPG_L3:
	case IS_LVN_MLSC2_GLPG_L3:
	case IS_LVN_MLSC3_GLPG_L3:
		p = frame->dva_mlsc_glpg[3];
		break;
	case IS_LVN_MLSC0_GLPG_G4:
	case IS_LVN_MLSC1_GLPG_G4:
	case IS_LVN_MLSC2_GLPG_G4:
	case IS_LVN_MLSC3_GLPG_G4:
		p = frame->dva_mlsc_glpg[4];
		break;
	/* MCSC */
	case IS_LVN_MCSC_P0:
		p = frame->sc0TargetAddress;
		break;
	case IS_LVN_MCSC_P1:
		p = frame->sc1TargetAddress;
		break;
	case IS_LVN_MCSC_P2:
		p = frame->sc2TargetAddress;
		break;
	case IS_LVN_MCSC_P3:
		p = frame->sc3TargetAddress;
		break;
	case IS_LVN_MCSC_P4:
		p = frame->sc4TargetAddress;
		break;
	case IS_LVN_MCSC_P5:
		p = frame->sc5TargetAddress;
		break;
	/* MTNR */
	case IS_LVN_MTNR_OUTPUT_CUR_L1:
		p = frame->dva_mtnr1_out_cur_l1_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_CUR_L2:
		p = frame->dva_mtnr1_out_cur_l2_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_CUR_L3:
		p = frame->dva_mtnr1_out_cur_l3_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_CUR_L4:
		p = frame->dva_mtnr1_out_cur_l4_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_PREV_L0:
		p = frame->dva_mtnr0_out_prev_l0_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_PREV_L1:
		p = frame->dva_mtnr1_out_prev_l1_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_PREV_L2:
		p = frame->dva_mtnr1_out_prev_l2_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_PREV_L3:
		p = frame->dva_mtnr1_out_prev_l3_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_PREV_L4:
		p = frame->dva_mtnr1_out_prev_l4_yuv;
		break;
	case IS_LVN_MTNR_OUTPUT_MV_GEOMATCH:
		p = frame->dva_mtnr0_out_mv_geomatch;
		break;
	case IS_LVN_MTNR_OUTPUT_PREV_L0_WGT:
		p = frame->dva_mtnr0_out_prev_l0_wgt;
		break;
	case IS_LVN_MTNR_OUTPUT_PREV_L1_WGT:
		p = frame->dva_mtnr1_out_prev_l1_wgt;
		break;
	case IS_LVN_MTNR_OUTPUT_SEG_L0:
		p = frame->dva_mtnr0_out_seg_l0;
		break;
	case IS_LVN_MTNR_OUTPUT_SEG_L1:
		p = frame->dva_mtnr1_out_seg_l1;
		break;
	case IS_LVN_MTNR_OUTPUT_SEG_L2:
		p = frame->dva_mtnr1_out_seg_l2;
		break;
	case IS_LVN_MTNR_OUTPUT_SEG_L3:
		p = frame->dva_mtnr1_out_seg_l3;
		break;
	case IS_LVN_MTNR_OUTPUT_SEG_L4:
		p = frame->dva_mtnr1_out_seg_l4;
		break;
	case IS_LVN_MTNR_CAPTURE_PREV_L0:
		p = frame->dva_mtnr0_cap_l0_yuv;
		break;
	case IS_LVN_MTNR_CAPTURE_PREV_L1:
		p = frame->dva_mtnr1_cap_l1_yuv;
		break;
	case IS_LVN_MTNR_CAPTURE_PREV_L2:
		p = frame->dva_mtnr1_cap_l2_yuv;
		break;
	case IS_LVN_MTNR_CAPTURE_PREV_L3:
		p = frame->dva_mtnr1_cap_l3_yuv;
		break;
	case IS_LVN_MTNR_CAPTURE_PREV_L4:
		p = frame->dva_mtnr1_cap_l4_yuv;
		break;
	case IS_LVN_MTNR_CAPTURE_PREV_L0_WGT:
		p = frame->dva_mtnr0_cap_l0_wgt;
		break;
	case IS_LVN_MTNR_CAPTURE_PREV_L1_WGT:
		p = frame->dva_mtnr1_cap_l1_wgt;
		break;
	case IS_LVN_MSNR_CAPTURE_LMEDS:
		p = frame->dva_msnr_cap_lme;
		break;
	case IS_LVN_MTNR0_CR:
		kp = frame->kva_mtnr0_rta_info;
		break;
	case IS_LVN_MTNR1_CR:
		kp = frame->kva_mtnr1_rta_info;
		break;
	case IS_LVN_MSNR_CR:
		kp = frame->kva_msnr_rta_info;
		break;
	case IS_LVN_YUVP_CR:
		kp = frame->kva_yuvp_rta_info;
		break;
	case IS_LVN_MCSC_CR:
		kp = frame->kva_mcsc_rta_info;
		break;
	case IS_LVN_MLSC0_CR:
	case IS_LVN_MLSC1_CR:
	case IS_LVN_MLSC2_CR:
	case IS_LVN_MLSC3_CR:
		kp = frame->kva_mlsc_rta_info;
		break;
	default:
		err_hw("Unsupported vid(%d)", vid);
		ret = -EINVAL;
	}

	if (taddr)
		*taddr = p;
	if (taddr_k)
		*taddr_k = kp;

	/* Clear subdev frame's target address before set */
	if (taddr && *taddr)
		memset(*taddr, 0x0, sizeof(typeof(**taddr)) * IS_MAX_PLANES);
	if (taddr_k && *taddr_k)
		memset(*taddr_k, 0x0, sizeof(typeof(**taddr_k)) * IS_MAX_PLANES);

	return ret;
}

void is_hw_fill_target_address(u32 hw_id, struct is_frame *dst, struct is_frame *src,
	bool reset)
{
	/* A previous address should not be cleared for sysmmu debugging. */
	reset = false;

	switch (hw_id) {
	case DEV_HW_PAF0:
	case DEV_HW_PAF1:
	case DEV_HW_PAF2:
	case DEV_HW_PAF3:
		break;
	case DEV_HW_LME0:
		TADDR_COPY(dst, src, lmesTargetAddress, reset);
		TADDR_COPY(dst, src, lmeskTargetAddress, reset);
		TADDR_COPY(dst, src, lmecTargetAddress, reset);
		TADDR_COPY(dst, src, lmesadTargetAddress, reset);
		TADDR_COPY(dst, src, kva_lme_rta_info, reset);
		break;
	case DEV_HW_BYRP0:
	case DEV_HW_BYRP1:
	case DEV_HW_BYRP2:
	case DEV_HW_BYRP3:
	case DEV_HW_BYRP4:
		TADDR_COPY(dst, src, dva_byrp_hdr, reset);
		TADDR_COPY(dst, src, dva_byrp_byr, reset);
		break;
	case DEV_HW_RGBP0:
	case DEV_HW_RGBP1:
	case DEV_HW_RGBP2:
	case DEV_HW_RGBP3:
		TADDR_COPY(dst, src, dva_rgbp_hist, reset);
		TADDR_COPY(dst, src, dva_rgbp_awb, reset);
		TADDR_COPY(dst, src, dva_rgbp_drc, reset);
		TADDR_COPY(dst, src, dva_rgbp_sat, reset);
		break;
	case DEV_HW_YUVSC0:
	case DEV_HW_YUVSC1:
	case DEV_HW_YUVSC2:
	case DEV_HW_YUVSC3:
		/* YUVSC use only one RDMA for c-loader */
		break;
	case DEV_HW_MLSC0:
	case DEV_HW_MLSC1:
	case DEV_HW_MLSC2:
	case DEV_HW_MLSC3:
		TADDR_COPY(dst, src, dva_mlsc_yuv444, reset);
		TADDR_COPY(dst, src, dva_mlsc_glpg[0], reset);
		TADDR_COPY(dst, src, dva_mlsc_glpg[1], reset);
		TADDR_COPY(dst, src, dva_mlsc_glpg[2], reset);
		TADDR_COPY(dst, src, dva_mlsc_glpg[3], reset);
		TADDR_COPY(dst, src, dva_mlsc_glpg[4], reset);
		TADDR_COPY(dst, src, dva_mlsc_svhist, reset);
		TADDR_COPY(dst, src, dva_mlsc_lmeds, reset);
		TADDR_COPY(dst, src, dva_mlsc_fdpig, reset);
		TADDR_COPY(dst, src, dva_mlsc_cav, reset);
		TADDR_COPY(dst, src, kva_mlsc_rta_info, reset);
		break;
	case DEV_HW_YPP:
		TADDR_COPY(dst, src, ixscmapTargetAddress, reset);

		TADDR_COPY(dst, src, ypnrdsTargetAddress, reset);
		TADDR_COPY(dst, src, ypdgaTargetAddress, reset);
		TADDR_COPY(dst, src, ypclaheTargetAddress, reset);
		TADDR_COPY(dst, src, dva_yuvp_out_pcchist, reset);
		TADDR_COPY(dst, src, dva_yuvp_cap_pcchist, reset);
		TADDR_COPY(dst, src, kva_yuvp_rta_info, reset);
		break;
	case DEV_HW_MTNR0:
		TADDR_COPY(dst, src, dva_mtnr0_out_prev_l0_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr0_out_mv_geomatch, reset);
		TADDR_COPY(dst, src, dva_mtnr0_out_prev_l0_wgt, reset);
		TADDR_COPY(dst, src, dva_mtnr0_out_seg_l0, reset);
		TADDR_COPY(dst, src, dva_mtnr0_cap_l0_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr0_cap_l0_wgt, reset);
		TADDR_COPY(dst, src, kva_mtnr0_rta_info, reset);
		break;
	case DEV_HW_MTNR1:
		TADDR_COPY(dst, src, dva_mtnr1_out_cur_l1_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_cur_l2_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_cur_l3_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_cur_l4_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_prev_l1_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_prev_l2_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_prev_l3_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_prev_l4_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_prev_l1_wgt, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_seg_l1, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_seg_l2, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_seg_l3, reset);
		TADDR_COPY(dst, src, dva_mtnr1_out_seg_l4, reset);
		TADDR_COPY(dst, src, dva_mtnr1_cap_l1_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_cap_l2_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_cap_l3_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_cap_l4_yuv, reset);
		TADDR_COPY(dst, src, dva_mtnr1_cap_l1_wgt, reset);
		TADDR_COPY(dst, src, kva_mtnr1_rta_info, reset);
		break;
	case DEV_HW_MSNR:
		TADDR_COPY(dst, src, dva_msnr_cap_lme, reset);
		TADDR_COPY(dst, src, kva_msnr_rta_info, reset);
		break;
	case DEV_HW_DLFE:
		/* DLFE shares the RTA_INFO buffer of MTNR */
		break;
	case DEV_HW_MCSC0:
		TADDR_COPY(dst, src, sc0TargetAddress, reset);
		TADDR_COPY(dst, src, sc1TargetAddress, reset);
		TADDR_COPY(dst, src, sc2TargetAddress, reset);
		TADDR_COPY(dst, src, sc3TargetAddress, reset);
		TADDR_COPY(dst, src, sc4TargetAddress, reset);
		TADDR_COPY(dst, src, sc5TargetAddress, reset);
		TADDR_COPY(dst, src, kva_mcsc_rta_info, reset);
		break;
	default:
		err("[%d] Invalid hw id(%d)", src->instance, hw_id);
		break;
	}
}

struct is_mem *is_hw_get_iommu_mem(u32 vid)
{
	struct is_core *core = is_get_is_core();
	struct pablo_device_iommu_group *iommu_group;

	switch (vid) {
	case IS_VIDEO_MCSC:
	case IS_LVN_MCSC_P0:
	case IS_LVN_MCSC_P1:
	case IS_LVN_MCSC_P2:
	case IS_LVN_MCSC_P3:
	case IS_LVN_MCSC_P4:
	case IS_LVN_MCSC_P5:
		iommu_group = pablo_iommu_group_get(0);
		return &iommu_group->mem;
	default:
		return &core->resourcemgr.mem;
	}
}

void is_hw_print_target_dva(struct is_frame *leader_frame, u32 instance)
{
	u32 i, j;

	for (i = 0; i < IS_MAX_PLANES; i++) {
		for (j = 0; j < CSI_VIRTUAL_CH_MAX; j++)
			IS_PRINT_MULTI_TARGET_DVA(dva_ssvc);

		IS_PRINT_TARGET_DVA(lmesTargetAddress);
		IS_PRINT_TARGET_DVA(lmecTargetAddress);
		IS_PRINT_TARGET_DVA(dva_byrp_hdr);
		IS_PRINT_TARGET_DVA(dva_byrp_byr);
		IS_PRINT_TARGET_DVA(sc0TargetAddress);
		IS_PRINT_TARGET_DVA(sc1TargetAddress);
		IS_PRINT_TARGET_DVA(sc2TargetAddress);
		IS_PRINT_TARGET_DVA(sc3TargetAddress);
		IS_PRINT_TARGET_DVA(sc4TargetAddress);
		IS_PRINT_TARGET_DVA(sc5TargetAddress);
	}
}

int is_hw_get_crta_buf_size(enum pablo_crta_buf_type buf_type, u32 *width, u32 *height, u32 *bpp)
{
	int ret = 0;

	switch (buf_type) {
	case PABLO_CRTA_BUF_PCFI:
		*width = (u32)sizeof(struct pablo_crta_frame_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_PCSI:
		*width = (u32)sizeof(struct pablo_crta_sensor_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_SS_CTRL:
		*width = (u32)sizeof(struct pablo_sensor_control_info);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_CDAF:
		*width = 211;
		*height = 1;
		*bpp = 32;
		break;
	case PABLO_CRTA_BUF_LAF:
		*width = (u32)sizeof(union itf_laser_af_data);
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_PRE_THUMB:
		*width = 98304;
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_AE_THUMB:
		*width = 65536;
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_AWB_THUMB:
		*width = 65536;
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_RGBY_HIST:
		*width = 5120;
		*height = 1;
		*bpp = 8;
		break;
	case PABLO_CRTA_BUF_CDAF_MW:
		*width = 13680;
		*height = 1;
		*bpp = 8;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

int is_hw_config(struct is_hw_ip *hw_ip, struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	u32 fcount, instance, cr_size = 0;
	struct pablo_crta_frame_info *pcfi;
	struct pablo_cr_set *cr_set = NULL;
	void *cfg = NULL;

	instance = atomic_read(&hw_ip->instance);
	fcount = buf_info->fcount;

	/* set results of crta */
	pcfi = (struct pablo_crta_frame_info *)buf_info->kva;
	if (CHK_HW_PDP(hw_ip->id)) {
		cr_set = pcfi->pdp_cr;
		cr_size = pcfi->pdp_cr_size;
	} else if (CHK_HW_BYRP(hw_ip->id)) {
		cr_set = pcfi->byrp_cr;
		cr_size = pcfi->byrp_cr_size;
		cfg = (void *)&pcfi->byrp_cfg;
	} else if (CHK_HW_RGBP(hw_ip->id)) {
		cr_set = pcfi->rgbp_cr;
		cr_size = pcfi->rgbp_cr_size;
		cfg = (void *)&pcfi->rgbp_cfg;
	} else if (CHK_HW_YUVSC(hw_ip->id)) {
		cr_set = pcfi->yuvsc_cr;
		cr_size = pcfi->yuvsc_cr_size;
		cfg = (void *)&pcfi->yuvsc_cfg;
	} else if (CHK_HW_MLSC(hw_ip->id)) {
		cr_set = pcfi->mlsc_cr;
		cr_size = pcfi->mlsc_cr_size;
		cfg = (void *)&pcfi->mlsc_cfg;
	}

	if (cfg) {
		ret = CALL_HWIP_OPS(hw_ip, set_config, 0, instance, fcount, cfg);
		if (ret)
			mserr_hw("set_config error (%d)", instance, hw_ip, ret);
	}

	if (cr_size) {
		ret = CALL_HWIP_OPS(
			hw_ip, set_regs, 0, instance, fcount, (struct cr_set *)cr_set, cr_size);
		if (ret)
			mserr_hw("set_regs error (%d)", instance, hw_ip, ret);
	}

	return ret;
}

void is_hw_update_pcfi(struct is_hardware *hardware, struct is_group *group,
			struct is_frame *frame, struct pablo_crta_buf_info *pcfi_buf)
{
	struct is_hw_ip *hw_ip;
	struct pablo_crta_frame_info *pcfi = (struct pablo_crta_frame_info *)pcfi_buf->kva;
	int hw_maxnum = 0;
	int hw_list[GROUP_HW_MAX], hw_index;
	u32 instance;

	if (!pcfi) {
		merr_adt("pcfi kva is null", frame->instance);
		return;
	}

	instance = group->instance;
	/* handle internal shot prfi */
	if (frame->type == SHOT_TYPE_INTERNAL) {
		memcpy(&pcfi->prfi, &__prfi[instance], sizeof(struct pablo_rta_frame_info));
		return;
	}

	while (group && (group->device_type == IS_DEVICE_ISCHAIN)) {
		hw_maxnum = is_get_hw_list(group->id, hw_list);

		for (hw_index = hw_maxnum - 1; hw_index >= 0; hw_index--) {
			hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, hw_list[hw_index]);
			if (!hw_ip) {
				merr_hw("invalid id (%d)", instance, hw_list[hw_index]);
				return;
			}

			CALL_HWIP_OPS(
				hw_ip, query, instance, PABLO_QUERY_GET_PCFI, frame, &pcfi->prfi);
		}
		group = group->child;
	}

	memcpy(&__prfi[instance], &pcfi->prfi, sizeof(struct pablo_rta_frame_info));

	mdbg_adt(2, "[SENSOR] (%dx%d) -> binning (%dx%d) -> crop (%d,%d,%dx%d)", frame->instance,
		pcfi->prfi.sensor_calibration_size.width, pcfi->prfi.sensor_calibration_size.height,
		pcfi->prfi.sensor_binning.x, pcfi->prfi.sensor_binning.y,
		pcfi->prfi.sensor_crop.offset.x, pcfi->prfi.sensor_crop.offset.y,
		pcfi->prfi.sensor_crop.size.width, pcfi->prfi.sensor_crop.size.height);

	mdbg_adt(2, "[BYRP] (%dx%d) -> crop_in (%d,%d,%dx%d)", frame->instance,
		pcfi->prfi.byrp_input_size.width, pcfi->prfi.byrp_input_size.height,
		pcfi->prfi.byrp_crop_in.offset.x, pcfi->prfi.byrp_crop_in.offset.y,
		pcfi->prfi.byrp_crop_in.size.width, pcfi->prfi.byrp_crop_in.size.height);

	mdbg_adt(2, "[RGBP] (%dx%d) -> crop_dmsc (%d,%d,%dx%d)", frame->instance,
		pcfi->prfi.rgbp_input_size.width, pcfi->prfi.rgbp_input_size.height,
		pcfi->prfi.rgbp_crop_dmsc.offset.x, pcfi->prfi.rgbp_crop_dmsc.offset.y,
		pcfi->prfi.rgbp_crop_dmsc.size.width, pcfi->prfi.rgbp_crop_dmsc.size.height);

	mdbg_adt(2, "[YUVSC] (%dx%d)", frame->instance, pcfi->prfi.yuvsc_output_size.width,
		pcfi->prfi.yuvsc_output_size.height);
}

void is_hw_update_frame_info(struct is_group *group, struct is_frame *frame)
{
	struct is_device_sensor *sensor = group->device->sensor;
	struct is_sensor_interface *sensor_itf = is_sensor_get_sensor_interface(sensor);
	struct is_frame_time_cfg *time_cfg = &frame->time_cfg;
	u32 vvalid, vblank, frame_rate, frame_duration;
	u32 post_frame_gap, line_ratio;
	const u32 min_vvalid = REQ_START_2_SHOT + REQ_SHOT_2_END;
	const u32 min_vblank = REQ_END_2_START;

	frame_rate = sensor->max_target_fps;
	frame_duration = 1000000U / frame_rate;

	sensor_itf->cis_itf_ops.get_sensor_frame_timing(sensor_itf, &vvalid, &vblank);
	if (!vvalid || !vblank) {
		post_frame_gap = 0;
		line_ratio = (frame_rate < FAST_FPS) ? 0 : REQ_CINROW_RATIO_AT_FAST_FPS;

		goto skip_calc;
	}

	/**
	 * Overwrite vblank gotten from CIS interface, since it's internal value of sensor
	 * that could have shorter size than actual vblank time.
	 */
	vblank = ZERO_IF_NEG(frame_duration - vvalid);

	/* Set post_frame_gap as much as possible */
	if (frame_duration > (vvalid + min_vblank))
		post_frame_gap = (frame_duration - (vvalid + min_vblank)) / DVFS_LEVEL_RATIO;
	else
		post_frame_gap = 0;

	/* Set line_time as fast as possible */
	if (min_vvalid > (vvalid + post_frame_gap))
		line_ratio = REQ_CINROW_RATIO_AT_FAST_FPS;
	else
		line_ratio = REQ_START_2_SHOT * 100 / vvalid;

	vblank -= post_frame_gap;

skip_calc:
	time_cfg->vvalid = vvalid;
	time_cfg->line_ratio = line_ratio;
	time_cfg->post_frame_gap = post_frame_gap;

	mdbg_adt(2, "[%s][F%d] vvalid(%d+%dus) line(%d%%) vblank(%uus) frame_duration(%dus)\n",
		frame->instance, group_id_name[group->id], frame->fcount, vvalid, post_frame_gap,
		line_ratio, vblank, frame_duration);
}

static inline struct is_group *__group_get_child(struct is_group *grp, u32 id)
{
	struct is_group *pos;

	for_each_group_child(pos, grp)
		if (pos->id == id)
			return pos;

	return NULL;
}

void is_hw_check_iteration_state(struct is_frame *frame,
	struct is_dvfs_ctrl *dvfs_ctrl, struct is_group *group)
{
	struct is_dvfs_iteration_mode *iter_mode = dvfs_ctrl->iter_mode;
	struct is_dvfs_rnr_mode *rnr_mode = &dvfs_ctrl->rnr_mode;
	u32 iter = frame->shot_ext->node_group.leader.iterationType;
	u32 rnr = frame->shot_ext->node_group.leader.recursiveNrType;

	if (!__group_get_child(group, GROUP_ID_MTNR))
		return;

	iter_mode->changed = 0;
	if (iter_mode->iter_svhist != iter) {
		mgrdbgs(1, "iteration state changed(%d -> %d)\n", group, group, frame,
			iter_mode->iter_svhist, iter);
		iter_mode->iter_svhist = iter;
		iter_mode->changed = 1;
	}

	rnr_mode->changed = 0;
	if (BOOL(rnr_mode->rnr) != BOOL(rnr)) {
		mgrdbgs(1, "recursiveNr state changed(%d -> %d)\n", group, group, frame,
			rnr_mode->rnr, rnr);
		rnr_mode->rnr = rnr;
		rnr_mode->changed = 1;
	}
}

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
void is_hw_restore_group_state_2nr(struct is_group *group)
{
/*	struct is_group *pos = __group_get_child(group, GROUP_ID_MTNR);

	if (!pos)
		return;

	if (pos->input_type == GROUP_INPUT_OTF || pos->input_type == GROUP_INPUT_VOTF)
		set_bit(IS_GROUP_OTF_INPUT, &pos->state);
*/
}

void is_hw_update_group_state_2nr(struct is_group *group, struct is_frame *frame)
{
#ifdef HACK_TO_AVOID_BUILD_ERR
	if (!pos)
		return;

	is_hw_restore_group_state_2nr(group);

	/* In case of recursiveNR, MCFP input type is M2M */
	if (frame->shot_ext->node_group.leader.recursiveNrType == NODE_RECURSIVE_NR_TYPE_2ND)
		clear_bit(IS_GROUP_OTF_INPUT, &pos->state);
#endif
}
#endif

size_t is_hw_get_param_dump(char *buf, size_t buf_size, struct is_param_region *param, u32 group_id)
{
	int i;
	struct is_hw_ip *hw_ip;
	char *p = buf;
	size_t rem = buf_size;

	hw_ip = is_get_hw_ip(group_id, &is_get_is_core()->hardware);

	switch (group_id) {
	case GROUP_ID_SS0:
	case GROUP_ID_SS1:
	case GROUP_ID_SS2:
	case GROUP_ID_SS3:
	case GROUP_ID_SS4:
	case GROUP_ID_SS5:
		p = dump_param_hw_ip(p, group_id_name[group_id] + strlen("G:"), 0, &rem);
		p = dump_param_sensor_config(p, "param_sensor_config", &param->sensor.config, &rem);
		break;
	case GROUP_ID_PAF0:
	case GROUP_ID_PAF1:
	case GROUP_ID_PAF2:
	case GROUP_ID_PAF3:
		if (hw_ip)
			p = dump_param_hw_ip(p, hw_ip->name, hw_ip->id, &rem);
		p = dump_param_control(p, "control", &param->paf.control, &rem);
		p = dump_param_dma_input(p, "dma_input", &param->paf.dma_input, &rem);
		p = dump_param_otf_output(p, "otf_output", &param->paf.otf_output, &rem);
		p = dump_param_dma_output(p, "dma_output", &param->paf.dma_output, &rem);
		break;
	case GROUP_ID_BYRP0:
	case GROUP_ID_BYRP1:
	case GROUP_ID_BYRP2:
	case GROUP_ID_BYRP3:
	case GROUP_ID_BYRP4:
	case GROUP_ID_RGBP0:
	case GROUP_ID_RGBP1:
	case GROUP_ID_RGBP2:
	case GROUP_ID_RGBP3:
	case GROUP_ID_YUVSC0:
	case GROUP_ID_YUVSC1:
	case GROUP_ID_YUVSC2:
	case GROUP_ID_YUVSC3:
	case GROUP_ID_MLSC0:
	case GROUP_ID_MLSC1:
	case GROUP_ID_MLSC2:
	case GROUP_ID_MLSC3:
	case GROUP_ID_MTNR:
	case GROUP_ID_MSNR:
	case GROUP_ID_YUVP:
		if (hw_ip)
			return CALL_HWIP_OPS(hw_ip, dump_params, 0, p, rem);
		break;
	case GROUP_ID_MCS0:
		if (hw_ip)
			p = dump_param_hw_ip(p, hw_ip->name, hw_ip->id, &rem);
		p = dump_param_control(p, "control", &param->mcs.control, &rem);
		p = dump_param_mcs_input(p, "input", &param->mcs.input, &rem);
		for (i = 0; i < MCSC_OUTPUT_MAX; i++) {
			char title[32];

			snprintf(title, sizeof(title), "output[%d]", i);
			p = dump_param_mcs_output(p, title, &param->mcs.output[i], &rem);
		}
		p = dump_param_stripe_input(p, "stripe_input", &param->mcs.stripe_input, &rem);
		break;
	default:
		err("%s(%d) is invalid", group_id_name[group_id], group_id);
		break;
	}

	return WRITTEN(buf_size, rem);
}
