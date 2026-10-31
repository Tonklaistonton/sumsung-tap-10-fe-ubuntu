//SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos5 SoC series EEPROM driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_EEPROM_HM6_H
#define IS_EEPROM_HM6_H

#define EEPROM_DATA_PATH		"/data/vendor/camera/hm6_eeprom_data.bin"
#define EEPROM_DUAL_DATA_PATH		"/data/vendor/camera/dual_cal_dump.bin"
#define EEPROM_SERIAL_NUM_DATA_PATH	"/data/vendor/camera/serial_number.bin"

/* Total Cal data size */
#define EEPROM_DATA_SIZE		SZ_16K

#define EEPROM_HEADER_VERSION_START 0x6E
#define IS_HEADER_VER_SIZE      11
#define IS_MAX_CAL_SIZE (64 * 1024)

#endif
