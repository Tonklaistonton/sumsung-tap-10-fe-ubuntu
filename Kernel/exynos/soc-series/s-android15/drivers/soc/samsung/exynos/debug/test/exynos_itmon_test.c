// SPDX-License-Identifier: GPL-2.0

#include <linux/platform_device.h>
#include <kunit/test.h>
#include "exynos_itmon_test.h"

static struct platform_device itmon_test_pdev;

static int itmon_test_suite_init(struct kunit_suite *suite)
{
	dev_set_name(&itmon_test_pdev.dev, "itmon_test_dev");
	return 0;
}

static void itmon_test_suite_exit(struct kunit_suite *suite)
{
	return;
}

static int itmon_test_init(struct kunit *test)
{
	return 0;
}

static void itmon_test_exit(struct kunit *test)
{
	return;
}

static void itmon_sample_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, 1);
}

static struct kunit_case itmon_test_cases[] = {
	KUNIT_CASE(itmon_sample_test),
	{}
};

static struct kunit_suite itmon_test_suite = {
	.name = "itmon_exynos",
	.init = itmon_test_init,
	.exit = itmon_test_exit,
	.suite_init = itmon_test_suite_init,
	.suite_exit = itmon_test_suite_exit,
	.test_cases = itmon_test_cases,
};

kunit_test_suites(&itmon_test_suite);

MODULE_LICENSE("GPL");
