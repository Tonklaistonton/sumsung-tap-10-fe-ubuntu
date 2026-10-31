/* SPDX-License-Identifier: GPL */
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

#ifndef PABLO_ICPU_HW_H
#define PABLO_ICPU_HW_H

struct icpu_mbox_tx_info {
	void __iomem *int_enable_reg;
	void __iomem *int_gen_reg;
	void __iomem *int_status_reg;
	void __iomem *data_reg;
	u32 data_max_len;
};

struct icpu_mbox_rx_info {
	void __iomem *int_gen_reg;
	void __iomem *int_status_reg;
	void __iomem *data_reg;
	u32 data_max_len;
	int irq;
};

/* only u32 member expected */
#define ICPU_IO_SEQUENCE_LEN (5)
struct icpu_io_step {
	u32 type; /* 0: write, 1: read, 2: delay */
	u32 addr;
	u32 mask;
	u32 val;
	u32 timeout; /* ms(for type 1), us(for type 2), 0: no timeout */
};

struct icpu_io_sequence {
	struct icpu_io_step *step;
	u32 num;
};

struct icpu_serial_dbg_reg_info {
	const char *name;
	u32 start;
	u32 num;
};

struct icpu_dbg_reg_info {
	u32 num_sdri;
	struct icpu_serial_dbg_reg_info *sdri;
};

#define ICPU_MAX_CORE_NUM 2
struct icpu_platform_data {
	void __iomem *mcuctl_core_reg_base;
	void __iomem *mcuctl_reg_base;
	void __iomem *sysctrl_reg_base;
	void __iomem *cstore_reg_base;
	struct regmap *sysreg_reset;
	struct regmap *sysreg_s2mpu;
	u32 sysreg_reset_bit[ICPU_MAX_CORE_NUM];

	struct clk *clk;

	u32 num_cores;

	u32 num_chans;
	u32 num_tx_mbox;
	struct icpu_mbox_tx_info *tx_infos;
	u32 num_rx_mbox;
	struct icpu_mbox_rx_info *rx_infos;

	struct icpu_io_sequence force_powerdown_seq;

	struct icpu_io_sequence sw_reset_seq;

	struct icpu_dbg_reg_info dbg_reg_info;
};

struct icpu_hw {
	int (*set_base_addr)(void __iomem *base_addr, u32 dst_addr);
	int (*set_aarch64)(void __iomem *base_addr, bool aarch64);
	int (*hw_misc_prepare)(struct regmap *reg_s2mpu);
	int (*release_reset)(void __iomem *sysctrl_base, struct regmap *reg_reset, u32 *bits,
		unsigned int on);
	int (*wait_wfi_state_timeout)(void __iomem *base_addr, u32 timeout_ms);
	void (*set_reg_sequence)(struct icpu_io_sequence *seq);
	void (*panic_handler)(void __iomem *mcuctl_core_base, void __iomem *sysctrl_base,
		struct icpu_dbg_reg_info *info);
	void (*set_debug_reg)(void __iomem *sysctrl_base, struct icpu_serial_dbg_reg_info *sdri,
		u32 index, u32 val);
	void (*print_debug_reg)(void __iomem *mcuctl_core_base, void __iomem *sysctrl_base,
		struct icpu_dbg_reg_info *info);
};

#define HW_OPS(f, ... ) \
	hw.f ? hw.f(__VA_ARGS__) : 0

void icpu_hw_init(struct icpu_hw *hw);

#endif
