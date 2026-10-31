// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "is-hw-param-debug.h"

static void pable_hw_dump_param_control(struct kunit *test)
{
	char *p;
	char *buf = NULL;
	const char *name = "TEST_HW";
	char exp[] = "\"TEST_HW\":{\"cmd\":1,\"bypass\":0,\"strgen\":0,\"err\":0},";
	struct param_control test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.cmd = 1;
	rem = size;
	p = dump_param_control(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, WRITTEN(size, rem), (sizeof(exp) - 1));
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pable_hw_dump_param_sensor_config(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "TEST_SENSOR";
	char exp_prefix[] = "\"TEST_SENSOR\":{\"frametime\":10000000,";
	struct param_sensor_config test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.frametime = 10000000;
	rem = size;
	p = dump_param_sensor_config(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static void pable_hw_dump_param_otf_input(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "otf_input";
	char exp_prefix[] = "\"otf_input\":{\"cmd\":1,";
	struct param_otf_input test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.cmd = 1;
	rem = size;
	p = dump_param_otf_input(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static void pable_hw_dump_param_otf_output(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "otf_output";
	char exp_prefix[] = "\"otf_output\":{\"cmd\":1,";
	struct param_otf_output test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.cmd = 1;
	rem = size;
	p = dump_param_otf_output(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static void pable_hw_dump_param_dma_input(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "dma_input";
	char exp_prefix[] = "\"dma_input\":{\"cmd\":1,";
	struct param_dma_input test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.cmd = 1;
	rem = size;
	p = dump_param_dma_input(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static void pable_hw_dump_param_dma_output(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "dma_output";
	char exp_prefix[] = "\"dma_output\":{\"cmd\":1,";
	struct param_dma_output test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.cmd = 1;
	rem = size;
	p = dump_param_dma_output(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static void pable_hw_dump_param_stripe_input(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "stripe_input";
	char exp_prefix[] = "\"stripe_input\":{\"index\":1,";
	struct param_stripe_input test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.index = 1;
	rem = size;
	p = dump_param_stripe_input(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static void pablo_hw_dump_param_mcs_input(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "mcs_input";
	char exp_prefix[] = "\"mcs_input\":{\"otf_cmd\":1,";
	struct param_mcs_input test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.otf_cmd = 1;
	rem = size;
	p = dump_param_mcs_input(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static void pablo_hw_dump_param_mcs_output(struct kunit *test)
{
	char *p;
	char *buf;
	char *name = "mcs_output";
	char exp_prefix[] = "\"mcs_output\":{\"otf_cmd\":1,";
	struct param_mcs_output test_param;
	size_t rem, size = PAGE_SIZE;

	buf = (char *)kunit_kzalloc(test, size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	test_param.otf_cmd = 1;
	rem = size;
	p = dump_param_mcs_output(buf, name, &test_param, &rem);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, p);
	KUNIT_EXPECT_EQ(test, 0, strncmp(buf, exp_prefix, strlen(exp_prefix)));
	KUNIT_EXPECT_LT(test, rem, size);

	kunit_kfree(test, buf);
}

static struct kunit_case pablo_hw_param_debug_kunit_test_cases[] = {
	KUNIT_CASE(pable_hw_dump_param_control),
	KUNIT_CASE(pable_hw_dump_param_sensor_config),
	KUNIT_CASE(pable_hw_dump_param_otf_input),
	KUNIT_CASE(pable_hw_dump_param_otf_output),
	KUNIT_CASE(pable_hw_dump_param_dma_input),
	KUNIT_CASE(pable_hw_dump_param_dma_output),
	KUNIT_CASE(pable_hw_dump_param_stripe_input),
	KUNIT_CASE(pablo_hw_dump_param_mcs_input),
	KUNIT_CASE(pablo_hw_dump_param_mcs_output),
	{},
};

struct kunit_suite pablo_hw_param_debug_kunit_test_suite = {
	.name = "pablo-hw-param-debug-kunit-test",
	.test_cases = pablo_hw_param_debug_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_param_debug_kunit_test_suite);

MODULE_LICENSE("GPL");
