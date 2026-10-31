// SPDX-License-Identifier: GPL-2.0
/*
 * smfc_kunit_test.c - Samsung Exynos SMFC Driver for Kunit
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>
#include <kunit/visibility.h>
#include "smfc_kunit_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

static int smfc_test_init(struct kunit *test)
{
	return 0;
}

static void smfc_jpeg_format_test(struct kunit *test)
{
	u32 result = 0;
	unsigned int hfactor, vfactor;
	hfactor = 2;
	vfactor = 1;
	result = smfc_get_jpeg_format(hfactor, vfactor);
	KUNIT_EXPECT_EQ(test, 2 << 24, result);
}

static struct kunit_case smfc_test_cases[] = {
	KUNIT_CASE(smfc_jpeg_format_test),
	{},
};

static struct kunit_suite smfc_test_suite = {
	.name = "smfc_exynos",
	.init = smfc_test_init,
	.test_cases = smfc_test_cases,
};

kunit_test_suites(&smfc_test_suite);

MODULE_LICENSE("GPL");
