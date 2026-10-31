// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <kunit/static_stub.h>
#include <kunit/visibility.h>

#include "gnss_utils.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

static int gnssif_exynos_test_init(struct kunit *test){ return 0; }

static void gnssif_exynos_test_exit(struct kunit *test){}

static void gnssif_exynos_version_test(struct kunit *test)
{
	KUNIT_EXPECT_NOT_NULL(test, get_gnssif_driver_version());
}

static struct kunit_case gnssif_exynos_test_cases[] = {
        KUNIT_CASE(gnssif_exynos_version_test),
        {}
};

static struct kunit_suite gnssif_exynos_test_suite = {
        .name = "gnssif_exynos",
        .init = gnssif_exynos_test_init,
        .exit = gnssif_exynos_test_exit,
        .test_cases = gnssif_exynos_test_cases,
};

kunit_test_suites(&gnssif_exynos_test_suite);

MODULE_LICENSE("GPL");
