/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_KERNEL_API_WRAP_H
#define PABLO_KERNEL_API_WRAP_H

unsigned long pkaw_copy_from_user(void *to, const void *from, unsigned long n);
unsigned long pkaw_copy_to_user(void *to, const void *from, unsigned long n);

#endif /* PABLO_KERNEL_API_WRAP_H */
