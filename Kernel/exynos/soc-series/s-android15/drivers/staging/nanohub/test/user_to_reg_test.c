/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 * Authors:
 *   Hyangmin Bae <hyangmin.bae@samsung.com>
 *
 */
#include <kunit/test.h>

#include "ipc_common.h"
#include "user_to_reg_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

enum cipc_user_id test_id;

static int user_to_reg_test_suite_init(struct kunit_suite *suite)
{
	return 0;
}

static int user_to_reg_test_init(struct kunit *test)
{
	test_id = -1;
	return 0;
}

static void user_to_reg_sample_test(struct kunit *test)
{
	int result = 0;

	test_id = CIPC_USER_CHUB2AP;
	result = (int)user_to_reg(test_id);
	KUNIT_EXPECT_EQ(test, 6, result);
}

static struct kunit_case user_to_reg_test_cases[] = {
	KUNIT_CASE(user_to_reg_sample_test),
	{},
};

static struct kunit_suite user_to_reg_test_suite = {
	.name = "chub_exynos",
	.init = user_to_reg_test_init,
	.suite_init = user_to_reg_test_suite_init,
	.test_cases = user_to_reg_test_cases,
};


kunit_test_suites(&user_to_reg_test_suite);

MODULE_LICENSE("GPL");
