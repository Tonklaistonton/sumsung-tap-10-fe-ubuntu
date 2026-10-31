/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXYNOS_POWER_RAIL_H
#define __EXYNOS_POWER_RAIL_H __FILE__

#define EXYNOS_POWER_RAIL_DBG_PREFIX	"EXYNOS-POWER-RAIL-DBG: "

#define INFO_SIZE	(SZ_64K)
#define RGT_STEP_SIZE	(6250)
#define POWER_RAIL_SIZE	12

int exynos_power_rail_dbg_init(void);

struct exynos_power_rail_info {
	unsigned int* domain_list;
	unsigned int size;
};

#if IS_ENABLED(CONFIG_SOC_S5E9945) || IS_ENABLED(CONFIG_SOC_S5E9945_EVT0) || IS_ENABLED(CONFIG_SOC_S5E8845)
typedef struct max_volt {
	unsigned char req_id;
	unsigned char volt;
} IDLE_DATA;

struct acpm_idle_data {
	IDLE_DATA max_voltage[POWER_RAIL_SIZE];
};

struct exynos_power_rail_dbg_info {
	struct device *dev;
	struct acpm_idle_data* idle_data;
	struct exynos_power_rail_info* power_rail_info;
	struct dentry *d;
	struct file_operations fops;
};

#else
#define ID_IN_POWER_OFF						2
#define ID_IN_MIN_LOCK_OF_POWER_OFF			3

#if IS_ENABLED(CONFIG_SOC_S5E8855)
#define REQ_VOLT_SIZE		(20)
#else
#define REQ_VOLT_SIZE		(30)
#endif

struct req_volt {
	u8 type;
	u8 req_id;
	u8 volt;
	u8 rsvd;
};

struct regulator {
	u8 ch;
	u8 rsvd0;

	u16 vsel_reg;
	u8 vsel_mask;
	u8 rsvd1;

	u16 enable_reg;
	u16 mode_reg;
	u8 vol_base;
	u8 vol_offset;

	u8 cur_volt;
	u8 max_volt;
	u8 min_volt;

	u8 req_tail;
	struct req_volt voltages[REQ_VOLT_SIZE];

	u8 coldtemp_en;
	u8 coldtemp;
	u8 coldtemp_vthr;
	u8 rsvd3;

	u32 set_ema;
	u32 wa[3];
	u32 get_coldtemp;

	u32 ppc;
	u32 sync;
};

struct exynos_power_rail_dbg_info {
	struct device *dev;
	void __iomem *apm_sram_base;
	struct regulator *reg_ptr;
	struct req_volt *rail_info;
	struct exynos_power_rail_info *power_rail_info;
	struct dentry *d;
	struct file_operations fops;
};

#endif

#endif
