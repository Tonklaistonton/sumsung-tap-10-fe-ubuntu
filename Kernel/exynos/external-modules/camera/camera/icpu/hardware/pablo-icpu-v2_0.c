// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/regmap.h>

#include "pablo-icpu.h"
#include "pablo-icpu-hw.h"
#include "pablo-debug.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-HW-V2_0]",
};

struct icpu_logger *get_icpu_hw_log(void)
{
	return &_log;
}

static struct io_ops {
	void (*write)(u32 addr, u32 val);
	u32 (*read)(u32 addr);
} __custom_io_ops;

#define ICPU_RESET_TIMEOUT			10000	/* 10ms */

/* HW CONFIGS */
/* PMU */
#define ICPU_CPU_CONFIGURATION_REG_OFFSET	0x0
#define ICPU_CPU_STATUS_REG_OFFSET		0x4

/* MCTCTL_CORE */
#define ICPU_CPU_PC_L				0x0
#define ICPU_CPU_PC_H				0x4
#define ICPU_CPU_ACTIVE_CNT_L			0x8
#define ICPU_CPU_ACTIVE_CNT_H			0xC
#define ICPU_CPU_STALL_CNT_L			0x10
#define ICPU_CPU_STALL_CNT_H			0x14
#define ICPU_CPU_MONITOR_ENABLE			0x1C
#define ICPU_CPU_PC_MONITOR_EN_BIT		0
#define ICPU_CPU_ACTIVE_CNT_MONITOR_EN_BIT	1
#define ICPU_CPU_STALL_CNT_MONITOR_EN_BIT	2


/* MCUCTL_PERI */
#define ICPU_CPU_REMAPS0_NS_REG_OFFSET		0x0
#define ICPU_CPU_REMAPS0_S_REG_OFFSET		0x4
#define ICPU_CPU_REMAPD0_NS_REG_OFFSET		0x8
#define ICPU_CPU_REMAPD0_S_REG_OFFSET		0xC
#define ICPU_CPU_WFI_STATUS			0xC4
#define ICPU_CPU_AA64nAA32			0x10C

/* SYSCTRL */
#define ICPU_SYSCTRL_QCH_DISABLE		0x04
#define ICPU_SYSCTRL_AXI_TRANSFER_BLOCK 0x24
#define ICPU_SYSCTRL_MASTER_IP_SW_RESETn 0x2C

/* Enables or releases the nCORERESET
 * 0 = Enables(Hold, Assert) reset
 * 1 = Releases reset
 */
static int __set_icpu_release_reset(
	void __iomem *sysctrl_base, struct regmap *reg_reset, u32 *bits, unsigned int on)
{
	int ret;
	unsigned int mask;
	unsigned int val;
	unsigned int bit = bits[0];

	if (!reg_reset) {
		ICPU_ERR("No sysreg remap");
		return 0;
	}

	writel(0x0, sysctrl_base + ICPU_SYSCTRL_AXI_TRANSFER_BLOCK);
	writel(0x3, sysctrl_base + ICPU_SYSCTRL_MASTER_IP_SW_RESETn);
	writel(0x0, sysctrl_base + ICPU_SYSCTRL_QCH_DISABLE);

	mask = BIT(bit);
	val = on ? BIT(bit) : 0;

	ret = regmap_update_bits(reg_reset, ICPU_CPU_CONFIGURATION_REG_OFFSET, mask, val);
	if (ret) {
		ICPU_ERR("Failed to icpu sysreg %s (reset bit=%d)", on ? "Release" : "Enable", bit);
	} else if (!IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ)) {
		ret = regmap_read_poll_timeout(reg_reset, ICPU_CPU_STATUS_REG_OFFSET,
			val, (val & BIT(bit)) >> bit == on,
			0, ICPU_RESET_TIMEOUT);
	}

	return ret;
}

static inline void __io_write(u32 addr, u32 val)
{
	void __iomem *reg;

	ICPU_INFO("addr: %x, val: %x", addr, val);

	if (__custom_io_ops.write) {
		__custom_io_ops.write(addr, val);
	} else {
		reg = ioremap(addr, 0x4);
		writel(val, reg);
		iounmap(reg);
	}
}

static inline u32 __io_read(u32 addr)
{
	void __iomem *reg;
	u32 val;

	if (__custom_io_ops.read) {
		val = __custom_io_ops.read(addr);
	} else {
		reg = ioremap(addr, 0x4);
		val = readl(reg);
		iounmap(reg);
	}
	ICPU_INFO("addr: %x, val: %x", addr, val);

	return val;
}

static int __wait_io_read(u32 addr, u32 mask, u32 val, u32 timeout_ms)
{
	const unsigned long duration = 150;
	u32 retry = timeout_ms * 1000 / duration / 2;

	while (((__io_read(addr) & mask) != val) && --retry)
		fsleep(duration);

	return retry ? 0 : -ETIMEDOUT;
}

#if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ))
static int __set_hw_cfg_for_veloce(struct regmap *reg_s2mpu)
{
	if (reg_s2mpu)
		regmap_write(reg_s2mpu, 0, 0xff);
	else
		ICPU_ERR("reg_s2mpu is NULL");

	return 0;
}
#endif

static int __set_base_addr(void __iomem *base_addr, u32 dst_addr)
{

	if (!base_addr)
		return -EINVAL;

	if (dst_addr == 0)
		return -EINVAL;

	/* source
	 * [31-1] : upper 31bit of source address. normally 0.
	 * [0] : remap enable
	 */
	dst_addr = dst_addr >> 4;

	writel(1, base_addr + ICPU_CPU_REMAPS0_NS_REG_OFFSET);
	writel(1, base_addr + ICPU_CPU_REMAPS0_S_REG_OFFSET);

	writel(dst_addr, base_addr + ICPU_CPU_REMAPD0_NS_REG_OFFSET);
	writel(dst_addr, base_addr + ICPU_CPU_REMAPD0_S_REG_OFFSET);

	return 0;
}

static int __set_aarch64(void __iomem *base_addr, bool aarch64)
{

	if (!base_addr)
		return -EINVAL;

	writel(aarch64, base_addr + ICPU_CPU_AA64nAA32);

	return 0;
}

static int __wait_wfi_state_timeout(void __iomem *base_addr, u32 timeout_ms)
{
	int ret = 0;
	u32 wait_time = 0;

	do {
		if (readl(base_addr + ICPU_CPU_WFI_STATUS) == 1) {
			ICPU_INFO("wait ICPU_CPU_WFI_STATUS for %d ms", wait_time);
			break;
		}

		usleep_range(10000, 11000);
		wait_time += 10;
	} while (wait_time <= timeout_ms);

	if (wait_time > timeout_ms) {
		ICPU_ERR("wait ICPU_CPU_WFI_STATUS timeout!!");
		ret = -ETIMEDOUT;
	}

	return ret;
}

static void __set_reg_sequence(struct icpu_io_sequence *seq)
{
	int i, ret;

	ICPU_INFO("E\n");

	for (i = 0; i < seq->num; i++) {
		if (seq->step[i].type == 0) {
			__io_write(seq->step[i].addr, seq->step[i].val);
		} else if (seq->step[i].type == 1) {
			ret = __wait_io_read(seq->step[i].addr, seq->step[i].mask, seq->step[i].val,
				seq->step[i].timeout);
			ICPU_ERR_IF(ret, "step[%d] Wait io read timeout!!, read val(0x%x)", i,
				__io_read(seq->step[i].addr));
		} else if (seq->step[i].type == 0xDEAD) {
			is_debug_s2d(true, "ICPU powerdown fail! panic requested!");
		} else {
			ICPU_ERR("step[%d] unknown type(%d)", i, seq->step[i].type);
		}
	}

	ICPU_INFO("X\n");
}

static void __print_debug_reg(void __iomem *mcuctl_core_base,
		void __iomem *sysctrl_base, struct icpu_dbg_reg_info *info)
{
	u32 i, j;
	struct icpu_serial_dbg_reg_info *sdri;

	/* enable icpu monitor */
	iowrite32(1, sysctrl_base + ICPU_SYSCTRL_QCH_DISABLE);
	iowrite32(BIT(ICPU_CPU_PC_MONITOR_EN_BIT) | BIT(ICPU_CPU_ACTIVE_CNT_MONITOR_EN_BIT) |
		BIT(ICPU_CPU_STALL_CNT_MONITOR_EN_BIT), mcuctl_core_base + ICPU_CPU_MONITOR_ENABLE);

	/* print icpu monitor reg */
	ICPU_INFO("PC H:0x%x, L:0x%x", ioread32(mcuctl_core_base + ICPU_CPU_PC_H),
		ioread32(mcuctl_core_base + ICPU_CPU_PC_L));
	ICPU_INFO("ACTIVE_CNT H:0x%x, L:0x%x", ioread32(mcuctl_core_base + ICPU_CPU_ACTIVE_CNT_H),
		ioread32(mcuctl_core_base + ICPU_CPU_ACTIVE_CNT_L));
	ICPU_INFO("STALL_CNT H:0x%x, L:0x%x", ioread32(mcuctl_core_base + ICPU_CPU_ACTIVE_CNT_H),
		ioread32(mcuctl_core_base + ICPU_CPU_ACTIVE_CNT_L));

	/* disable icpu monitor  */
	iowrite32(0, mcuctl_core_base + ICPU_CPU_MONITOR_ENABLE);
	iowrite32(0, sysctrl_base + ICPU_SYSCTRL_QCH_DISABLE);

	/* print icpu sysctrl dbg reg and mbox dbg reg */
	for (i = 0; i < info->num_sdri; i++) {
		sdri = &info->sdri[i];
		for (j = 0; j + 3 < sdri->num; j += 4) {
			ICPU_INFO("%s%d:0x%08x, %d:0x%08x, %d:0x%08x, %d:0x%08x", sdri->name,
				j, ioread32(sysctrl_base + sdri->start + (sizeof(u32) * j)),
				j + 1, ioread32(sysctrl_base + sdri->start + (sizeof(u32) * (j + 1))),
				j + 2, ioread32(sysctrl_base + sdri->start + (sizeof(u32) * (j + 2))),
				j + 3, ioread32(sysctrl_base + sdri->start + (sizeof(u32) * (j + 3))));
		}
	}
}

static void __panic_handler(void __iomem *mcuctl_core_base,
		void __iomem *sysctrl_base, struct icpu_dbg_reg_info *info)
{
	__print_debug_reg(mcuctl_core_base, sysctrl_base, info);
}

static void __set_debug_reg(void __iomem *sysctrl_base,
		struct icpu_serial_dbg_reg_info *sdri, u32 index, u32 val)
{
	if (!sdri) {
		ICPU_ERR("dbg reg info is NULL");
		return;
	}

	if (index >= sdri->num) {
		ICPU_ERR("invalid %s index(%d/%d)", sdri->name, index, sdri->num);
		return;
	}

	ICPU_INFO("%s index(%d), val(0x%x)", sdri->name, index, val);
	iowrite32(val, sysctrl_base + sdri->start + sizeof(u32) * index);
}

void icpu_hw_init(struct icpu_hw *hw)
{
	memset(hw, 0x0, sizeof(struct icpu_hw));

	hw->set_base_addr = __set_base_addr;
	hw->set_aarch64 = __set_aarch64;
#if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ))
	hw->hw_misc_prepare = __set_hw_cfg_for_veloce;
#endif
	hw->release_reset = __set_icpu_release_reset;
	hw->wait_wfi_state_timeout = __wait_wfi_state_timeout;
	hw->set_reg_sequence = __set_reg_sequence;
	hw->panic_handler = __panic_handler;
	hw->set_debug_reg = __set_debug_reg;
	hw->print_debug_reg = __print_debug_reg;

	__custom_io_ops.read = 0;
	__custom_io_ops.write = 0;
}
KUNIT_EXPORT_SYMBOL(icpu_hw_init);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
void icpu_hw_config_io_ops(void *write, void *read)
{
	__custom_io_ops.write = write;
	__custom_io_ops.read = read;
}
KUNIT_EXPORT_SYMBOL(icpu_hw_config_io_ops);

void icpu_hw_reset_io_ops(void)
{
	__custom_io_ops.read = 0;
	__custom_io_ops.write = 0;
}
KUNIT_EXPORT_SYMBOL(icpu_hw_reset_io_ops);
#endif
