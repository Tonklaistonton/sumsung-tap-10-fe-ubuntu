// SPDX-License-Identifier: GPL-2.0

#include <linux/platform_device.h>
#include <kunit/test.h>
#include "exynos_watchdog_test.h"

static struct platform_device watchdog_test_pdev;

static int watchdog_test_suite_init(struct kunit_suite *suite)
{
	dev_set_name(&watchdog_test_pdev.dev, "watchdog_test_dev");
	return 0;
}

static void watchdog_test_suite_exit(struct kunit_suite *suite)
{
	return;
}

static int watchdog_test_init(struct kunit *test)
{
	return 0;
}

static void watchdog_test_exit(struct kunit *test)
{
	return;
}

static void watchdog_sample_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, 1);
}

static struct kunit_case watchdog_test_cases[] = {
	KUNIT_CASE(watchdog_sample_test),
	{}
};

static struct kunit_suite watchdog_test_suite = {
	.name = "watchdog_exynos",
	.init = watchdog_test_init,
	.exit = watchdog_test_exit,
	.suite_init = watchdog_test_suite_init,
	.suite_exit = watchdog_test_suite_exit,
	.test_cases = watchdog_test_cases,
};

kunit_test_suites(&watchdog_test_suite);

MODULE_LICENSE("GPL");

