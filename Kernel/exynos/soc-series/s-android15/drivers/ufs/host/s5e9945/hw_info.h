// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Kiwoong Kim <kwmad.kim@samsung.com>
 */

/* OTP for alwasy on */
#define EXYNOS_PHY_BIAS		0x1000C308
#define EXYNOS_PMA_OTP		0x1000E3C0
#define EXYNOS_PMA_OTP_SIZE     64

/* Pad retention for alwasy on */
#define PMU_TOP_OUT		0x3B20
#define PAD_RTO_UFS_EMBD	0x40000

#define PMU_UFS_OUT		0x2120
#define IP_INISO_PHY		0x8000
