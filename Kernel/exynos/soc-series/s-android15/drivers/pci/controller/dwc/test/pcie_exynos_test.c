// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <linux/errno.h>
#include "pcie-exynos-rc.h"

static int pcie_exynos_test_init(struct kunit *test){ return 0; }

static void pcie_exynos_rcpower_test(struct kunit *test)
{
	int result;

	result = exynos_pcie_rc_poweron(1);

	KUNIT_EXPECT_EQ(test, -EPROBE_DEFER, result);
}

static struct kunit_case pcie_exynos_test_cases[] = {
	KUNIT_CASE(pcie_exynos_rcpower_test),
	{}
};

static struct kunit_suite pcie_exynos_test_suite = {
	.name = "pcie_exynos",
	.init = pcie_exynos_test_init,
	.test_cases = pcie_exynos_test_cases,
};

kunit_test_suites(&pcie_exynos_test_suite);

MODULE_LICENSE("GPL");
