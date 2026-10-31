// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <kunit/static_stub.h>
#include <kunit/visibility.h>

#include "cpif_page.h"

static struct cpif_page_pool *pool;

static int cpif_exynos_test_init(struct kunit *test){ return 0; }

static void cpif_exynos_test_exit(struct kunit *test){}

static void cpif_exynos_page_pool_test(struct kunit *test)
{
	pool = cpif_page_pool_create(100, SZ_64K);
	if (!pool)
		KUNIT_FAIL(test, "failed to create page pool");

	KUNIT_EXPECT_EQ(test, get_order(SZ_64K), pool->page_order);
	KUNIT_EXPECT_EQ(test, SZ_64K, pool->page_size);
	KUNIT_EXPECT_EQ(test, 100 * MIN_PAGE_POOL_MULT + 1, pool->rpage_arr_len);
	KUNIT_EXPECT_EQ(test, 0, pool->rpage_arr_idx);

	pool = cpif_page_pool_delete(pool);
	if (pool)
		KUNIT_FAIL(test, "page pool deletion imcomplete");
}

static struct kunit_case cpif_exynos_test_cases[] = {
        KUNIT_CASE(cpif_exynos_page_pool_test),
        {}
};

static struct kunit_suite cpif_exynos_test_suite = {
        .name = "cpif_exynos",
        .init = cpif_exynos_test_init,
        .exit = cpif_exynos_test_exit,
        .test_cases = cpif_exynos_test_cases,
};

kunit_test_suites(&cpif_exynos_test_suite);

MODULE_LICENSE("GPL");
