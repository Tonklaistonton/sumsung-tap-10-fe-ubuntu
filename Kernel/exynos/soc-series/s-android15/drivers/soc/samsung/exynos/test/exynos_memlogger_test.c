// SPDX-License-Identifier: GPL-2.0

#include <linux/platform_device.h>
#include <kunit/test.h>
#include "exynos_memlogger_test.h"

static struct platform_device memlogger_test_pdev;

static int memlogger_test_suite_init(struct kunit_suite *suite)
{
	dev_set_name(&memlogger_test_pdev.dev, "memlogger_test_dev");
	return 0;
}

static void memlogger_test_suite_exit(struct kunit_suite *suite)
{
	return;
}

static int memlogger_test_init(struct kunit *test)
{
	return 0;
}

static void memlogger_test_exit(struct kunit *test)
{
	return;
}

static void memlogger_sample_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, 1);
}

static struct kunit_case memlogger_test_cases[] = {
	KUNIT_CASE(memlogger_sample_test),
	{}
};

static struct kunit_suite memlogger_test_suite = {
	.name = "memlogger_exynos",
	.init = memlogger_test_init,
	.exit = memlogger_test_exit,
	.suite_init = memlogger_test_suite_init,
	.suite_exit = memlogger_test_suite_exit,
	.test_cases = memlogger_test_cases,
};

kunit_test_suites(&memlogger_test_suite);

MODULE_LICENSE("GPL");

