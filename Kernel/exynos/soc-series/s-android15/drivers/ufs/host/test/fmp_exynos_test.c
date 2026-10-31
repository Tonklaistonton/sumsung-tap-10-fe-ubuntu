/*
 * drivers/soc/samsung/exynos-hdcp/test/exynos-hdcp2-test.c
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <kunit/visibility.h>
#include <ufs/ufshcd.h>
#include "fmp_exynos_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct ufs_hba hba;

static int fmp_test_suite_init(struct kunit_suite *suite)
{
	return 0;
}

static int fmp_test_init(struct kunit *test)
{
	return 0;
}

static void exynos_ufs_fmp_enable_test(struct kunit *test)
{
	bool result;

	printk("Expected result of fmp enable test is false\n");

	result = exynos_ufs_fmp_crypto_enable(&hba);

	KUNIT_EXPECT_EQ(test, false, result);
}

static struct kunit_case fmp_test_cases[] = {
	KUNIT_CASE(exynos_ufs_fmp_enable_test),
	{}
};

static struct kunit_suite fmp_test_suite = {
	.name = "fmp_exynos",
	.init = fmp_test_init,
	.suite_init = fmp_test_suite_init,
	.test_cases = fmp_test_cases,
};

kunit_test_suites(&fmp_test_suite);

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);
MODULE_LICENSE("GPL");
