/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 */

#ifndef EXYNOS_HWDECOMP_H
#define EXYNOS_HWDECOMP_H

typedef int (*vendor_hw_decomp_fn)(const unsigned char *src, size_t src_len,
					unsigned char *dst, struct page *page);

vendor_hw_decomp_fn register_vendor_hw_decomp(void);

#endif
