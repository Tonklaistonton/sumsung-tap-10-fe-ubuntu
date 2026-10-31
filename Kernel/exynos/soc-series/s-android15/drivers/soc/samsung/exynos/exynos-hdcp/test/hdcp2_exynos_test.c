/*
 * drivers/soc/samsung/exynos-hdcp/test/exynos-hdcp2-test.c
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include "../exynos-hdcp2-crypto.h"

static int hdcp2_test_suite_init(struct kunit_suite *suite)
{
	return 0;
}

static int hdcp2_test_init(struct kunit *test)
{
	return 0;
}

static void hdcp2_crypto_calc_sha1_test(struct kunit *test)
{
	const char *message = "Test Message";
	uint8_t result[20];
	const uint8_t expected[20] = {
		0x3d, 0x23, 0x4c, 0x03, 0x17, 0x62, 0x8a, 0xb6,
		0xd8, 0x8e, 0xa6, 0xdf, 0xf9, 0xd6, 0x85, 0xbf,
		0x48, 0x42, 0x3b, 0x2d
	};

	printk("Expected size of SHA-1 result is %lu.\n", sizeof(result));

	hdcp_calc_sha1(result, (const uint8_t *)message, strlen(message));

	KUNIT_EXPECT_EQ(test, 0, memcmp(result, expected, sizeof(result)));
}

static struct kunit_case hdcp2_test_cases[] = {
	KUNIT_CASE(hdcp2_crypto_calc_sha1_test),
	{}
};

static struct kunit_suite hdcp2_test_suite = {
	.name = "hdcp2_exynos",
	.init = hdcp2_test_init,
	.suite_init = hdcp2_test_suite_init,
	.test_cases = hdcp2_test_cases,
};

kunit_test_suites(&hdcp2_test_suite);

MODULE_LICENSE("GPL");
