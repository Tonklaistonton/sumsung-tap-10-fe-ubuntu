/*
 * Samsung Exynos5 SoC series Actuator driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_DEVICE_DW9808_H
#define IS_DEVICE_DW9808_H

#define DW9808_POS_SIZE_BIT		ACTUATOR_POS_SIZE_10BIT
#define DW9808_POS_MAX_SIZE		((1 << DW9808_POS_SIZE_BIT) - 1)
#define DW9808_POS_DIRECTION		ACTUATOR_RANGE_INF_TO_MAC
#define RINGING_CONTROL			0x2F /* 11ms */
#define PRESCALER				0x01 /* Tvib x 1 */

#define PWR_ON_DELAY			5000 /* DW9808 need delay for 5msec after power-on */

enum HW_SOFTLANDING_STATE {
	HW_SOFTLANDING_PASS = 0,
	HW_SOFTLANDING_FAIL = -200,
};

struct dw9808_cal_data {
	u8 control_mode;
	u8 prescale;
	u8 resonance;
	bool cal_data_available;
};

struct dw9808_cal_data_addr {
	u32 control_mode_addr;
	u32 resonance_addr;
};

struct dw9808_cal_data cal_data;
struct dw9808_cal_data_addr cal_data_addr;

#endif
