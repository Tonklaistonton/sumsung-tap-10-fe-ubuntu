// SPDX-License-Identifier: GPL
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

#include <linux/regmap.h>

#include "pablo-icpu.h"
#include "pablo-icpu-hw.h"
#include "pablo-debug.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-HW-V1_0]",
};

struct icpu_logger *get_icpu_hw_log(void)
{
	return &_log;
}

static struct io_ops {
	void (*write)(u32 addr, u32 val);
	u32 (*read)(u32 addr);
} __custom_io_ops;

/* HW CONFIGS */

#define ICPU_CPU_REMAPS0_NS_REG_OFFSET 0x0
#define ICPU_CPU_REMAPS0_S_REG_OFFSET 0x4
#define ICPU_CPU_REMAPD0_NS_REG_OFFSET 0x8
#define ICPU_CPU_REMAPD0_S_REG_OFFSET 0xC
#define ICPU_CPU_WFI_STATUS 0xC4

#define ICPU_SYSCTRL_AXI_TRANSFER_BLOCK 0x24
#define ICPU_SYSCTRL_NUM_ACTIVE_TRANSACTION 0x28

#define ICPU_CSTIRE_DMA_FLUSH 0x30
#define ICPU_CSTIRE_STATUS 0x34

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

	mask = BIT(bit);
	val = on ? BIT(bit) : 0;

	ret = regmap_update_bits(reg_reset, 0, mask, val);
	if (ret)
		ICPU_ERR("Failed to icpu sysreg %s (reset bit=%d)", on ? "Release" : "Enable", bit);

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
	void __iomem *reg;

	/* ICPU: s2mpu*/
	__io_write(0x18070054, 0xFF);

	/* TZPC control non-secure*/
	__io_write(0x18010204, 0xFFFFFFFF);

	/* TZPC control non-secure*/
	__io_write(0x18010214, 0xFFFFFFFF);

	/* TZPC AXPROT_SEL */
	__io_write(0x18020400, 0xF);

	/* TZPC AXPROT_VAL */
	__io_write(0x18020404, 0x3);

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
	int ret;
	int i;

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

static void __panic_handler(void __iomem *mcuctl_core_base,
		void __iomem *sysctrl_base, struct icpu_dbg_reg_info *info)
{
	u32 i, j;
	struct icpu_serial_dbg_reg_info *sdri;

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
#if (IS_ENABLED(CONFIG_CAMERA_CIS_ZEBU_OBJ))
	hw->hw_misc_prepare = __set_hw_cfg_for_veloce;
#endif
	hw->release_reset = __set_icpu_release_reset;
	hw->wait_wfi_state_timeout = __wait_wfi_state_timeout;
	hw->set_reg_sequence = __set_reg_sequence;
	hw->panic_handler = __panic_handler;
	hw->set_debug_reg = __set_debug_reg;

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
