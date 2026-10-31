// SPDX-License-Identifier: GPL-2.0-only
/**
 * i2c-exynos5_test.c - Samsung Exynos5 I2C Controller Test Driver for KUNIT
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>
#include <linux/i2c.h>
#include <kunit/visibility.h>

u32 exynos5_i2c_func(struct i2c_adapter *adap);

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct i2c_adapter adap;

static int i2c_exynos5_ext_test_init(struct kunit *test)
{
	memset(&adap, 0, sizeof(adap));

	return 0;
}

static void i2c_exynos5_ext_sample_test(struct kunit *test)
{
	int result = 0;

	result = exynos5_i2c_func(&adap);

	KUNIT_EXPECT_EQ(test, 0xEFE0009, result);
}

static struct kunit_case i2c_exynos5_ext_test_cases[] = {
	KUNIT_CASE(i2c_exynos5_ext_sample_test),
	{},
};

static struct kunit_suite i2c_exynos5_ext_test_suite = {
	.name = "hsi2c_exynos",
	.init = i2c_exynos5_ext_test_init,
	.test_cases = i2c_exynos5_ext_test_cases,
};

kunit_test_suites(&i2c_exynos5_ext_test_suite);

MODULE_LICENSE("GPL v2");
