/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_UTC_H
#define PABLO_UTC_H

#include <linux/bitmap.h>

/* CAMDEV id list */
enum kunit_utc_result {
	KUTC_CSI_PROBE, /* 919 */
	KUTC_VOTF_CFG, /* 962 */
	KUTC_S_FMT_RAW8, /* 1220 */
	KUTC_S_FMT_RAW10, /* 1221 */
	KUTC_S_FMT_RAW12, /* 1222 */
	KUTC_MCSC_CONV420, /* 568 */
	KUTC_MCSC_FMT_P010, /* 570 */
	KUTC_MCSC_MAX_WIDTH, /* 1939 */
	KUTC_MCSC_EN_DIS, /* 1944 */
	KUTC_MCSC_OTF_YUVIN, /* 1945 */
	KUTC_MCSC_M2M_YUVOUT, /* 1946 */
	KUTC_MCSC_CHECK_POST_SC, /* 1954 */
	KUTC_MCSC_POLY_COEF, /* 1955 */
	KUTC_MCSC_YUV_RANGE, /* 1958 */
	KUTC_MCSC_RST_TO, /* 1959 */
	KUTC_MCSC_BCHS_CLAMP, /* 1967 */
	KUTC_MCSC_FMT_P210, /* 1968 */
	KUTC_MCSC_STRIDE_ALIGN, /* 1971 */
	KUTC_MCSC_FMT_MONO, /* 1972 */
	KUTC_MCSC_DIS_DMA, /* 1973 */
	KUTC_MCSC_ERR_SCALE, /* 1974 */
	KUTC_MCSC_FMT_ERR, /* 1976 */
	KUTC_MCSC_PER_FRM_CTRL, /* 1987 */
	KUTC_YUVP_BYPASS, /* 1941 */
	KUTC_YUVP_PER_FRM_CTRL, /* 850 */
	KUTC_YUVP_EN_DIS, /* 1960 */
	KUTC_YUVP_SET_YUVIN, /* 1961 */
	KUTC_YUVP_META_INF, /* 849 */
	KUTC_YUVP_DIS_DMA, /* 848 */
	KUTC_BYRP_STAT_WDMA, /* 1561 */
	KUTC_BYRP_BINNING_RATIO, /* 1563 */
	KUTC_BYRP_PER_FRM_CTRL, /* 557 */
	KUTC_BYRP_EN_DIS, /* 559 */
	KUTC_BYRP_META_INF, /* 1613 */
	KUTC_BYRP_DIS_DMA, /* 1619 */
	KUTC_RGBP_BYPASS, /* 1914 */
	KUTC_MCFP_RDMA_SBWC, /* 1982 */
	KUTC_MCFP_BYPASS, /* 1985 */
	KUTC_MAX,
};

enum putc_cmd {
	P_B_RDMA,
	PUTC_MAX,
};

#define UTC_UPDATE_BIT(bit, res, cond)                                                             \
	do {                                                                                       \
		(cond) ? set_bit(bit, res) : 0;                                                    \
	} while (0)

#define P_NO_RUN 0
#define P_PASS 1
#define P_FAIL -1

struct putc_info {
	u32 camdev_id;
	int result;
};

void set_utc_result(u32 idx, u32 id, bool pass);
int putc_check_result(const enum putc_cmd utc_cmd, const void *expect, const void *acture);
int putc_get_dma_in(void *hardware_ip, void *dma, u32 dma_offset);
int putc_get_result(char *buffer, size_t buf_size, struct putc_info *utc_list, int utc_num);
#endif /* PABLO_UTC_H */
