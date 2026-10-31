// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <kunit/static_stub.h>
#include <kunit/visibility.h>

#include "ufs-exynos-dbg.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct ufs_vs_handle handle;

static int ufs_exynos_test_init(struct kunit *test)
{
	memset(&handle, 0, sizeof(handle));
	return 0;
}

static void ufs_exynos_test_exit(struct kunit *test){}

static void ufs_exynos_dbg_test(struct kunit *test)
{
	int ret = 0;
	printk("starting ufs dbg test\n");
	ret = exynos_ufs_init_dbg(&handle, NULL);
	printk("end of ufs dbg test\n");
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static struct kunit_case ufs_exynos_test_cases[] = {
        KUNIT_CASE(ufs_exynos_dbg_test),
        {}
};

static struct kunit_suite ufs_exynos_test_suite = {
        .name = "ufs_exynos",
        .init = ufs_exynos_test_init,
        .exit = ufs_exynos_test_exit,
        .test_cases = ufs_exynos_test_cases,
};

kunit_test_suites(&ufs_exynos_test_suite);

MODULE_LICENSE("GPL");
