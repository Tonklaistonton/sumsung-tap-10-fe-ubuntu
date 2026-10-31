/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "npu-device.h"
#include "npu-system.h"

int npu_util_memdump_probe(struct npu_system *system);
int npu_util_memdump_open(struct npu_system *system);
int npu_util_memdump_close(struct npu_system *system);
int npu_util_dump_handle_error_k(struct npu_device *device);
void npu_util_dump_handle_nrespone(struct npu_system *system);
