// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <linux/device.h>

static int sgpu_test_suite_init(struct kunit_suite *suite)
{
    return 0;
}

static int sgpu_test_init(struct kunit *test)
{
    return 0;
}

static void sgpu_test_fake_device_alloc(struct kunit *test)
{
    KUNIT_EXPECT_EQ(test, 1, 1);
}

static void sgpu_test_sample(struct kunit *test)
{
    KUNIT_EXPECT_EQ(test, 1, 1);
}

static struct kunit_case sgpu_test_cases[] = {
    KUNIT_CASE(sgpu_test_fake_device_alloc),
    KUNIT_CASE(sgpu_test_sample),
    {}
};

static struct kunit_suite sgpu_exynos_test_suite = {
    .name = "sgpu_exynos",
    .init = sgpu_test_init,
    .suite_init = sgpu_test_suite_init,
    .test_cases = sgpu_test_cases,
};

kunit_test_suites(&sgpu_exynos_test_suite);

MODULE_LICENSE("GPL");
