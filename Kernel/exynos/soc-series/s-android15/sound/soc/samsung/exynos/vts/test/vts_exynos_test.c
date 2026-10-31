// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include "vts_ipc.h"

static struct device dev;
static struct vts_data vts_data_t;

static int vts_exynos_test_init(struct kunit *test)
{
	memset(&dev, 0, sizeof(dev));
	memset(&vts_data_t, 0, sizeof(vts_data_t));

	dev_set_drvdata(&dev, &vts_data_t);

	return 0;
}

static void vts_exynos_sample_test(struct kunit *test)
{
	int result;

	result = vts_is_on();

	pr_info("%s: result : [%d]", __func__, result);

	KUNIT_EXPECT_EQ(test, 0, result);
}

static struct kunit_case vts_exynos_test_cases[] = {
	KUNIT_CASE(vts_exynos_sample_test),
	{}
};


static struct kunit_suite vts_exynos_test_suite = {
	.name = "vts_exynos",
	.init = vts_exynos_test_init,
	.test_cases = vts_exynos_test_cases,
};

kunit_test_suites(&vts_exynos_test_suite);

MODULE_LICENSE("GPL");
