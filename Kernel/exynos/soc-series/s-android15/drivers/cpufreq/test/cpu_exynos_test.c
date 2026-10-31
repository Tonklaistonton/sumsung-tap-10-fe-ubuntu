// SPDX-License-Identifier: GPL-2.0
/*
 * cpufreq_test.c - Samsung CPU Freq Driver for Kunit
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>

#include "cpu_exynos_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct cpufreq_policy_data new_policy;
struct device_node dn;

static int cpu_exynos_test_init(struct kunit *test)
{
	memset(&new_policy, 0, sizeof(new_policy));
	memset(&dn, 0, sizeof(dn));
	return 0;
}

static void cpu_exynos_test_cpufreq_verify(struct kunit *test)
{
	int result;

	result = exynos_cpufreq_verify(&new_policy);

	KUNIT_EXPECT_EQ(test, -EINVAL, result);
}

static void cpu_exynos_test_dsufreq_init_stats(struct kunit *test)
{
	int result;

	result = dsufreq_init_stats(&dn);

	KUNIT_EXPECT_EQ(test, -EINVAL, result);
}

static struct kunit_case cpu_exynos_test_cases[] = {
	KUNIT_CASE(cpu_exynos_test_cpufreq_verify),
	KUNIT_CASE(cpu_exynos_test_dsufreq_init_stats),
	{},
};

static struct kunit_suite cpu_exynos_test_suite = {
	.name = "cpu_exynos",
	.init = cpu_exynos_test_init,
	.test_cases = cpu_exynos_test_cases,
};

kunit_test_suites(&cpu_exynos_test_suite);

MODULE_LICENSE("GPL");

