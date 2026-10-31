/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_KUNIT_TEST_H
#define PABLO_KUNIT_TEST_H

#include <kunit/test.h>

#include "pablo-utc.h"

#define __pablo_kunit_test_suites(unique_array, unique_suites, ...)		\
	static struct kunit_suite *unique_array[] = { __VA_ARGS__, NULL };	\
	static struct kunit_suite **unique_suites				\
	__used __section("pablo_kunit_test_suites") = unique_array

#define define_pablo_kunit_test_suites(__suites...)			\
	__pablo_kunit_test_suites(__UNIQUE_ID(array),			\
			__UNIQUE_ID(suites),				\
			##__suites)

#define __pablo_kunit_test_suites_end(unique_array, unique_suites, ...)	\
	static struct kunit_suite *unique_array[] = { __VA_ARGS__, NULL };	\
	static struct kunit_suite **unique_suites				\
	__used __section("__end_pablo_kunit_test_suite") = unique_array

#define define_pablo_kunit_test_suites_end(__suites...)			\
	__pablo_kunit_test_suites_end(__UNIQUE_ID(array),			\
			__UNIQUE_ID(suites),					\
			##__suites)

#ifndef KUNIT_EXPECT_NULL
#define KUNIT_EXPECT_NULL(test, ptr)				               \
	KUNIT_EXPECT_PTR_EQ(test, ptr, NULL)
#endif

#define UTC_ID_FMT_RAW8 1220
#define UTC_ID_FMT_RAW10 1221
#define UTC_ID_FMT_RAW12 1222

#define UTC_ID_MCSC_FMT_P010 570
#define UTC_ID_MCSC_EN_DIS 1944
#define UTC_ID_MCSC_OTF_YUVIN 1945
#define UTC_ID_MCSC_M2M_YUVOUT 1946
#define UTC_ID_MCSC_CHECK_POST_SC 1954
#define UTC_ID_MCSC_POLY_COEF 1955
#define UTC_ID_MCSC_CONV420 568
#define UTC_ID_MCSC_YUV_RANGE 1958
#define UTC_ID_MCSC_BCHS_CLAMP 1967
#define UTC_ID_MCSC_FMT_P210 1968
#define UTC_ID_MCSC_STRIDE_ALIGN 1971
#define UTC_ID_MCSC_FMT_MONO 1972
#define UTC_ID_MCSC_MAX_WIDTH 1939
#define UTC_ID_MCSC_PER_FRM_CTRL 1987
#define UTC_ID_MCSC_DIS_DMA 1973
#define UTC_ID_MCSC_ERR_SCALE 1974
#define UTC_ID_MCSC_FMT_ERR 1976
#define UTC_ID_MCSC_RST_TO 1959
#define UTC_ID_YUVP_BYPASS 1941
#define UTC_ID_YUVP_PER_FRM_CTRL 850
#define UTC_ID_YUVP_EN_DIS 1960
#define UTC_ID_YUVP_SET_YUVIN 1961
#define UTC_ID_YUVP_META_INF 849
#define UTC_ID_YUVP_DIS_DMA 848
#define UTC_ID_BYRP_STAT_WDMA 1561
#define UTC_ID_BYRP_BINNING_RATIO 1563
#define UTC_ID_BYRP_PER_FRM_CTRL 557
#define UTC_ID_BYRP_EN_DIS 559
#define UTC_ID_BYRP_META_INF 1613
#define UTC_ID_BYRP_DIS_DMA 1619
#define UTC_ID_RGBP_BYPASS 1914
#define UTC_ID_MCFP_BYPASS 1985
#define UTC_ID_MCFP_RDMA_SBWC 1982
#endif /* PABLO_KUNIT_TEST_H */
