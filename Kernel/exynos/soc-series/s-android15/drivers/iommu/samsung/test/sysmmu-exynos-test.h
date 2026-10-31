/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#ifndef __SYSMMU_EXYNOS_TEST_H
#define __SYSMMU_EXYNOS_TEST_H

void samsung_iommu_deinit_log(struct samsung_iommu_log *log);
int samsung_iommu_init_log(struct samsung_iommu_log *log, int len);
#endif
