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
#include "pablo-hw-api-common-ctrl.h"

/* Define testcases */
static void pablo_hw_common_ctrl_dbg_ops_kunit_test(struct kunit *test)
{
	const struct kernel_param *kp = pablo_common_ctrl_get_kernel_param();
	char *buf = (char *)kunit_kzalloc(test, XATTR_SIZE_MAX, 0);
	int ret;

	ret = kp->ops->set("", kp);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = kp->ops->set("10 0", kp);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = kp->ops->set("0 1", kp);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = kp->ops->set("0 1 1 2 3 4 5", kp);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = kp->ops->get(buf, kp);
	KUNIT_EXPECT_NE(test, ret, 0);

	kunit_kfree(test, buf);
}

static struct kunit_case pablo_hw_common_ctrl_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_common_ctrl_dbg_ops_kunit_test),
	{},
};

static int pablo_hw_common_ctrl_kunit_test_init(struct kunit *test)
{
	/* Do nothing */
	return 0;
}

static void pablo_hw_common_ctrl_kunit_test_exit(struct kunit *test)
{
	/* Do nothing */
}

static struct kunit_suite pablo_hw_common_ctrl_kunit_test_suite = {
	.name = "pablo-hw-common-ctrl-kunit-test",
	.init = pablo_hw_common_ctrl_kunit_test_init,
	.exit = pablo_hw_common_ctrl_kunit_test_exit,
	.test_cases = pablo_hw_common_ctrl_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_common_ctrl_kunit_test_suite);
