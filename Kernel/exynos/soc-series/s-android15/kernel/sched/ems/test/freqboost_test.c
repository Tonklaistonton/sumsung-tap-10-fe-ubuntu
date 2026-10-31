// SPDX-License-Identifier: GPL-2.0
/*
 * Freqboost_test.c - Samsung EMS(Eynos Mobile Schedular) Driver for Kunit
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>

#include "ems.h"

#include "freqboost_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct rq cur_rq;
struct task_struct taskStruct;

static int ems_exynos_test_init(struct kunit *test)
{
	memset(&cur_rq, 0, sizeof(cur_rq));
	memset(&taskStruct, 0, sizeof(taskStruct));

	return 0;
}

static void ems_exynos_test_freqboost_boost_timeout_true(struct kunit *test)
{
	int result = 0;

	result = freqboost_boost_timeout(10, 5, 4);

	KUNIT_EXPECT_EQ(test, 1, result);
}

static void ems_exynos_test_freqboost_boost_timeout_false(struct kunit *test)
{
	int result = 0;

	result = freqboost_boost_timeout(10, 5, 6);

	KUNIT_EXPECT_EQ(test, 0, result);
}

static struct kunit_case ems_exynos_test_cases[] = {
	KUNIT_CASE(ems_exynos_test_freqboost_boost_timeout_true),
	KUNIT_CASE(ems_exynos_test_freqboost_boost_timeout_false),
	{},
};

static struct kunit_suite ems_exynos_test_suite = {
	.name = "ems_exynos",
	.init = ems_exynos_test_init,
	.test_cases = ems_exynos_test_cases,
};

kunit_test_suites(&ems_exynos_test_suite);

MODULE_LICENSE("GPL");
