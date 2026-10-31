// SPDX-License-Identifier: GPL-2.0
/*
 * hsi2c_exynos_test.c - Samsung Exynos5 I2C Controller Driver for Kunit
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <linux/errno.h>
#include <drm/drm_rect.h>
#include <drm/drm_modes.h>
#include <kunit/test.h>

#include "exynos_drm_partial_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

static void test_exynos_partial_is_full(struct kunit *test)
{
	struct drm_rect partial_region = DRM_RECT_INIT(0, 0, 0x100, 0x100);
	struct drm_display_mode mode = {
		.hdisplay = 0x100,
		.vdisplay = 0x100,
	};

	KUNIT_EXPECT_TRUE(test, exynos_partial_is_full(&mode, &partial_region));
}

static struct kunit_case exynos_drm_partial_test_cases[] = {
	KUNIT_CASE(test_exynos_partial_is_full),
	{}
};
static struct kunit_suite exynos_drm_partial_test_suite = {
	.name		= "disp_exynos_partial",
	.test_cases	= exynos_drm_partial_test_cases,
};
kunit_test_suite(exynos_drm_partial_test_suite);

MODULE_LICENSE("GPL");
