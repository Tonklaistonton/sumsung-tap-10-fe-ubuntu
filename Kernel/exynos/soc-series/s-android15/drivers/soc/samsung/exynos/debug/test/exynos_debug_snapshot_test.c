// SPDX-License-Identifier: GPL-2.0

#include <linux/platform_device.h>
#include <kunit/test.h>
#include "exynos_debug_snapshot_test.h"

static struct platform_device debug_snapshot_test_pdev;

static int debug_snapshot_test_suite_init(struct kunit_suite *suite)
{
	dev_set_name(&debug_snapshot_test_pdev.dev, "debug_snapshot_test_dev");
	return 0;
}

static void debug_snapshot_test_suite_exit(struct kunit_suite *suite)
{
	return;
}

static int debug_snapshot_test_init(struct kunit *test)
{
	return 0;
}

static void debug_snapshot_test_exit(struct kunit *test)
{
	return;
}

static void debug_snapshot_sample_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, 1);
}

static struct kunit_case debug_snapshot_test_cases[] = {
	KUNIT_CASE(debug_snapshot_sample_test),
	{}
};

static struct kunit_suite debug_snapshot_test_suite = {
	.name = "debug_snapshot_exynos",
	.init = debug_snapshot_test_init,
	.exit = debug_snapshot_test_exit,
	.suite_init = debug_snapshot_test_suite_init,
	.suite_exit = debug_snapshot_test_suite_exit,
	.test_cases = debug_snapshot_test_cases,
};

kunit_test_suites(&debug_snapshot_test_suite);

MODULE_LICENSE("GPL");
