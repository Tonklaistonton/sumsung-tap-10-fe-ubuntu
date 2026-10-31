 /* Samsung Exynos5 SoC series Sensor driver
  *
  *
  * Copyright (c) 2024 Samsung Electronics Co., Ltd
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
 */

#ifndef IS_CIS_IMX823_H
#define IS_CIS_IMX823_H

#include "is-cis.h"

enum sensor_imx823_mode_enum {
	SENSOR_IMX823_MODE_4000x3000_30FPS = 0,
	SENSOR_IMX823_MODE_4000x2256_30FPS,
	SENSOR_IMX823_MODE_3184x2388_30FPS,
	SENSOR_IMX823_MODE_3184x1792_30FPS,
	SENSOR_IMX823_MODE_3472x2388_30FPS,
	SENSOR_IMX823_MODE_2000x1128_120FPS,
	SENSOR_IMX823_MODE_2000x1496_120FPS,
	SENSOR_IMX823_MODE_4000x3000_30FPS_R12,
	SENSOR_IMX823_MODE_4000x2256_30FPS_R12,
	SENSOR_IMX823_MODE_2000x1128_60FPS,
	SENSOR_IMX823_MODE_2000x1496_30FPS,

	/* Seamless mode */
	SENSOR_IMX823_MODE_4000X3000_10FPS_LN4 = 11,
	SENSOR_IMX823_MODE_4000X2256_10FPS_LN4,
	SENSOR_IMX823_MODE_3184X2388_10FPS_LN4,
	SENSOR_IMX823_MODE_3184X1792_10FPS_LN4,
	SENSOR_IMX823_MODE_MAX,
};

struct sensor_imx823_private_data {
	const struct sensor_regs global;
};

#define MODE_GROUP_NONE (-1)
enum sensor_imx823_mode_group_enum {
	SENSOR_IMX823_MODE_NORMAL,
	SENSOR_IMX823_MODE_LN4,
	SENSOR_IMX823_MODE_DSG,
	SENSOR_IMX823_MODE_MODE_GROUP_MAX
};
u32 sensor_imx823_mode_groups[SENSOR_IMX823_MODE_MODE_GROUP_MAX];

static const struct sensor_reg_addr sensor_imx823_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0,
	.cit = 0x0202,
	.cit_shifter = 0, /* not supported */
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};
#endif