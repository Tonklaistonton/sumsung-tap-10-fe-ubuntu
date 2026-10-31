/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDOR_TEST_SENSOR_H
#define IS_VENDOR_TEST_SENSOR_H

#include <exynos-is-sensor.h>

enum is_vendor_device_type {
	SENSOR_TYPE = 0,
	ACTUATOR_TYPE,
	EEPROM_TYPE,
	DEVICE_TYPE_MAX,
};

#endif /* IS_VENDOR_TEST_SENSOR_H */
