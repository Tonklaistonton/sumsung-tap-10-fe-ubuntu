#include "power_exynos_test.h"
#include "../cmucal.h"
#include <kunit/test.h>

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

static void cmucal_get_list_size_test(struct kunit *test)
{
	unsigned int size, id;

	id = MUX_TYPE;
	size = cmucal_get_list_size(MUX_TYPE);

	KUNIT_EXPECT_LT(test, 0, size);
}

static void cmucal_get_node_test(struct kunit *test)
{
	struct cmucal_clk *clk;
	unsigned int id;

	id = QCH_TYPE;
	clk = cmucal_get_node(id);

	KUNIT_EXPECT_NOT_NULL(test, clk);
}

static void cmucal_get_id_test(struct kunit *test)
{
	struct cmucal_clk *clk1, *clk2;
	unsigned int id1, id2;

	id1 = DIV_TYPE;

	clk1 = cmucal_get_node(id1);
	KUNIT_ASSERT_NOT_NULL(test, clk1);

	id2 = cmucal_get_id(clk1->name);

	clk2 = cmucal_get_node(id2);

	KUNIT_EXPECT_PTR_EQ(test, clk1, clk2);
}

static int power_exynos_test_init(struct kunit *test)
{
	return 0;
}

static struct kunit_case power_exynos_test_cases[] = {
	KUNIT_CASE(cmucal_get_list_size_test),
	KUNIT_CASE(cmucal_get_node_test),
	KUNIT_CASE(cmucal_get_id_test),
	{}
};

static struct kunit_suite power_exynos_test_suite = {
	.name = "power_exynos_test",
	.init = power_exynos_test_init,
	.test_cases = power_exynos_test_cases,
};

kunit_test_suites(&power_exynos_test_suite);

MODULE_LICENSE("GPL");
