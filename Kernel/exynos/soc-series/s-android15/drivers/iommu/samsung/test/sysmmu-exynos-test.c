// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 */

#define pr_fmt(fmt) "sysmmu: " fmt

#include <kunit/test.h>
#include <kunit/visibility.h>
#include "samsung-iommu-v9.h"
#include "sysmmu-exynos-test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

static int samsung_iommu_test_suite_init(struct kunit_suite *suite)
{
	return 0;
}

static int samsung_iommu_test_init(struct kunit *test)
{
	return 0;
}

static void samsung_iommu_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, 1);
}

static void samsung_iommu_log_init_test(struct kunit *test)
{
	struct samsung_iommu_log *log = kunit_kzalloc(test, sizeof(*log), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, log);

	KUNIT_EXPECT_EQ(test, 0, samsung_iommu_init_log(log, 2048));

	samsung_iommu_deinit_log(log);

	KUNIT_EXPECT_EQ(test, 0, 0);
}

static struct kunit_case samsung_iommu_test_cases[] = {
	KUNIT_CASE(samsung_iommu_test),
	KUNIT_CASE(samsung_iommu_log_init_test),
	{}
};

static struct kunit_suite samsung_iommu_test_suite = {
	.name = "sysmmu_exynos",
	.init = samsung_iommu_test_init,
	.suite_init = samsung_iommu_test_suite_init,
	.test_cases = samsung_iommu_test_cases,
};

kunit_test_suites(&samsung_iommu_test_suite);

MODULE_LICENSE("GPL");
