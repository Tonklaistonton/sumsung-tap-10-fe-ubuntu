// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit tests
 *
 * Copyright (C) 2021,
 *
 */
#include <kunit/test.h>
#include <soc/samsung/exynos-sci.h>

static int exynos_sci_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);

	return 0;
}

static void exynos_sci_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static void exynos_sci_get_llc_way_max_test(struct kunit *test)
{
	int ret;

	ret = get_llc_way_max();
#if IS_ENABLED(CONFIG_EXYNOS_SCI)
	KUNIT_EXPECT_EQ(test, 16, ret);
#else
	KUNIT_EXPECT_EQ(test, 0, ret);
#endif
}

static struct kunit_case exynos_sci_test_cases[] = {
	KUNIT_CASE(exynos_sci_get_llc_way_max_test),
	{}
};

static struct kunit_suite exynos_sci_test_suite = {
	.name = "bus_exynos",
	.init = exynos_sci_test_init,
	.exit = exynos_sci_test_exit,
	.test_cases = exynos_sci_test_cases,
};

kunit_test_suites(&exynos_sci_test_suite);

MODULE_LICENSE("GPL");
