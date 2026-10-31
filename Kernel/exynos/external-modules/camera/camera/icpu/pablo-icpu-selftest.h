/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_SELFTEST_H
#define PABLO_ICPU_SELFTEST_H

int pablo_icpu_selftest_get_param_ops(const struct kernel_param_ops **control_ops,
		const struct kernel_param_ops **msg_ops);
void pablo_icpu_selftest_rsp_cb(void *sender, void *cookie, u32 *data);
void pablo_icpu_selftest_register_test_itf(struct pablo_icpu_itf_api *itf);

#endif
