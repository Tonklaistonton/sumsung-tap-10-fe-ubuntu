// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/uaccess.h>

unsigned long pkaw_copy_from_user(void *to, const void *from, unsigned long n)
{
	if (likely(access_ok(from, n)))
		return copy_from_user(to, from, n);

	memcpy(to, from, n);
	return 0;
}
EXPORT_SYMBOL_GPL(pkaw_copy_from_user);

unsigned long pkaw_copy_to_user(void *to, const void *from, unsigned long n)
{
	if (likely(access_ok(to, n)))
		return copy_to_user(to, from, n);

	memcpy(to, from, n);
	return 0;
}
EXPORT_SYMBOL_GPL(pkaw_copy_to_user);
