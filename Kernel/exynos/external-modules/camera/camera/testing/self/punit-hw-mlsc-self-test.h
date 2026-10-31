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

#ifndef PUNIT_HW_MLSC_SELF_TEST_H
#define PUNIT_HW_MLSC_SELF_TEST_H

#define NUM_OF_MLSC_PARAM (PARAM_MLSC_MAX - PARAM_MLSC_CONTROL + 1)

size_t pst_get_preset_test_size_mlsc(void);
unsigned long *pst_get_preset_test_result_buf_mlsc(void);
const enum pst_blob_node *pst_get_blob_node_mlsc(void);
void pst_init_param_mlsc(unsigned int index, enum pst_hw_ip_type type);

#endif /* PUNIT_HW_MLSC_SELF_TEST_H */
