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

#include <linux/platform_device.h>

#include "pablo-device-llcaid.h"
#include "pablo-kunit-test.h"
#include "pablo-debug.h"

#define TEST_STREAM_INFO_NUM 5
#define TEST_LLCAID_GROUP_NUM 2

static void pablo_llcaid_probe_kunit_test(struct kunit *test)
{
	int ret;

	platform_driver_unregister(pablo_llcaid_get_platform_driver());

	ret = platform_driver_register(pablo_llcaid_get_platform_driver());
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_llcaid_driver_get_kunit_test(struct kunit *test)
{
	struct platform_driver *pablo_llcaid_driver;

	pablo_llcaid_driver = pablo_llcaid_get_platform_driver();
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, pablo_llcaid_driver);
}

static struct pablo_llcaid_stream_info stream_data[TEST_STREAM_INFO_NUM] = {
	{ .set_val[PABLO_SET_VAL_IDX_EN] = 0x101, .set_val[PABLO_SET_VAL_IDX_CTRL] = 0x1 },
	{ .set_val[PABLO_SET_VAL_IDX_EN] = 0x101, .set_val[PABLO_SET_VAL_IDX_CTRL] = 0x1 },
	{ .set_val[PABLO_SET_VAL_IDX_EN] = 0x101, .set_val[PABLO_SET_VAL_IDX_CTRL] = 0x2 },
	{ .set_val[PABLO_SET_VAL_IDX_EN] = 0x101, .set_val[PABLO_SET_VAL_IDX_CTRL] = 0x2 },
	{ .set_val[PABLO_SET_VAL_IDX_EN] = 0x101, .set_val[PABLO_SET_VAL_IDX_CTRL] = 0x2 }
};

static struct pablo_device_llcaid llcaid_group_data[TEST_LLCAID_GROUP_NUM] = {
	{
		.stream = { &stream_data[0], &stream_data[1] },
	},
	{
		.stream = { &stream_data[2], &stream_data[3], &stream_data[4] },
	}
};

static void pablo_llcaid_config_kunit_test(struct kunit *test)
{
	struct pablo_device_llcaid *llcaid;
	struct pablo_llcaid_stream_info *stream_info;
	void *stream_base;
	u32 i, j, expected_val, set_val;

	pablo_device_set_llcaid_group(llcaid_group_data, TEST_LLCAID_GROUP_NUM);

	pablo_device_llcaid_config();

	for (i = 0; i < TEST_LLCAID_GROUP_NUM; i++) {
		llcaid = &llcaid_group_data[i];
		if (!llcaid)
			break;

		for (j = 0; j < PABLO_MAX_NUM_STREAM; j++) {
			stream_info = llcaid->stream[j];
			if (!stream_info)
				break;

			stream_base = llcaid->base[j];

			expected_val = stream_info->set_val[PABLO_SET_VAL_IDX_EN];
			set_val = readl(stream_base);
			KUNIT_EXPECT_EQ(test, expected_val, set_val);

			expected_val = stream_info->set_val[PABLO_SET_VAL_IDX_CTRL];
			set_val = readl(stream_base + PABLO_STREAM_RD_CTRL_OFFSET);
			KUNIT_EXPECT_EQ(test, expected_val, set_val);

			expected_val = stream_info->set_val[PABLO_SET_VAL_IDX_CTRL];
			set_val = readl(stream_base + PABLO_STREAM_WD_CTRL_OFFSET);
			KUNIT_EXPECT_EQ(test, expected_val, set_val);
		}
	}
}

static struct kunit_case pablo_device_llcaid_kunit_test_cases[] = {
	KUNIT_CASE(pablo_llcaid_probe_kunit_test),
	KUNIT_CASE(pablo_llcaid_driver_get_kunit_test),
	KUNIT_CASE(pablo_llcaid_config_kunit_test),
	{},
};

static int pablo_device_llcaid_kunit_test_init(struct kunit *test)
{
	int i, j;

	for (i = 0; i < TEST_LLCAID_GROUP_NUM; i++) {
		for (j = 0; j < TEST_STREAM_INFO_NUM; j++) {
			llcaid_group_data[i].base[j] = kunit_kzalloc(test, 0xC, 0);
			KUNIT_ASSERT_NOT_ERR_OR_NULL(test, llcaid_group_data[i].base[j]);
		}
	}

	return 0;
}

static void pablo_device_llcaid_kunit_test_exit(struct kunit *test)
{
	int i, j;

	for (i = 0; i < TEST_LLCAID_GROUP_NUM; i++)
		for (j = 0; j < TEST_STREAM_INFO_NUM; j++)
			kunit_kfree(test, llcaid_group_data[i].base[j]);
}

struct kunit_suite pablo_device_llcaid_kunit_test_suite = {
	.name = "pablo-device-llcaid-kunit-test",
	.init = pablo_device_llcaid_kunit_test_init,
	.exit = pablo_device_llcaid_kunit_test_exit,
	.test_cases = pablo_device_llcaid_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_device_llcaid_kunit_test_suite);

MODULE_LICENSE("GPL");
