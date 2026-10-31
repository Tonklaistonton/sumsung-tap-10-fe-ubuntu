// SPDX-License-Identifier: GPL-2.0
/*
 * mfc_kunit_test.c - Samsung Exynos MFC Driver for Kunit
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>
#include <kunit/visibility.h>
#include "../base/mfc_data_struct.h"
#include "mfc_kunit_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

static int mfc_test_init(struct kunit *test)
{
	return 0;
}

static void mfc_dec_find_format_test(struct kunit *test)
{
	struct mfc_fmt *fmt = NULL;
	struct mfc_ctx *ctx = NULL;
	struct mfc_dev *dev;
	dev = kzalloc(sizeof(struct mfc_dev), GFP_KERNEL);
	dev->pdata = kzalloc(sizeof(struct mfc_platdata), GFP_KERNEL);
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	ctx->dev = dev;
	ctx->dev->debugfs.logging_option = 0;
	fmt = __mfc_dec_find_format(ctx, V4L2_PIX_FMT_NV12M);
	KUNIT_EXPECT_EQ(test, V4L2_PIX_FMT_NV12M, fmt->fourcc);

	kfree(ctx);
	kfree(dev->pdata);
	kfree(dev);
}

static struct kunit_case mfc_test_cases[] = {
	KUNIT_CASE(mfc_dec_find_format_test),
	{},
};

static struct kunit_suite mfc_test_suite = {
	.name = "mfc_exynos",
	.init = mfc_test_init,
	.test_cases = mfc_test_cases,
};

kunit_test_suites(&mfc_test_suite);

MODULE_LICENSE("GPL");
