// SPDX-License-Identifier: GPL-2.0
/*
 * Example KUnit test to show how to use KUnit.
 *
 * Copyright (C) 2019, Google LLC.
 * Author: Brendan Higgins <brendanhiggins@google.com>
 */

#include <kunit/test.h>
#include <soc/samsung/exynos-devfreq.h>
#include <kunit/visibility.h>

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

int exynos_devfreq_reboot(struct exynos_devfreq_data *data);

static struct exynos_devfreq_data data;

/*
 * This is run once before each test case, see the comment on
 * example_test_suite for more information.
 */
static int __exynos_devfreq_test_init(struct kunit *test)
{
	memset(&data, 0, sizeof(data));

	kunit_info(test, "%s\n", __func__);

	return 0;
}

static void exynos_devfreq_test_0(struct kunit *test)
{
    int result = 0;

    result = exynos_devfreq_reboot(&data);

    KUNIT_EXPECT_EQ(test, 0, result);
}

static void __exynos_devfreq_test_exit(struct kunit *test)
{
	kunit_info(test, "%s\n", __func__);
}

/*
 * Here we make a list of all the test cases we want to add to the test suite
 * below.
 */
static struct kunit_case exynos_devfreq_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test suite.
	 */
	KUNIT_CASE(exynos_devfreq_test_0),
	{}
};

/*
 * This defines a suite or grouping of tests.
 *
 * Test cases are defined as belonging to the suite by adding them to
 * `kunit_cases`.
 *
 * Often it is desirable to run some function which will set up things which
 * will be used by every test; this is accomplished with an `init` function
 * which runs before each test case is invoked. Similarly, an `exit` function
 * may be specified which runs after every test case and can be used to for
 * cleanup. For clarity, running tests in a test suite would behave as follows:
 *
 * suite.init(test);
 * suite.test_case[0](test);
 * suite.exit(test);
 * suite.init(test);
 * suite.test_case[1](test);
 * suite.exit(test);
 * ...;
 */
static struct kunit_suite exynos_devfreq_test_suite = {
	.name = "dvfs_exynos_test",
	.init = __exynos_devfreq_test_init,
	.exit = __exynos_devfreq_test_exit,
	.test_cases = exynos_devfreq_test_cases,
};


/*
static struct kunit_suite* exynos_devfreq_test_suites[] =
	 { &exynos_devfreq_test_suite, NULL };

static int suites_num = sizeof(exynos_devfreq_test_suites)
	/ sizeof(exynos_devfreq_test_suites[0]);
int exynos_devfreq_test_init(void)
{
	printk("[KT] %s:%d\n", __func__, __LINE__);
	return __kunit_test_suites_init(exynos_devfreq_test_suites,
				suites_num);
}

void exynos_devfreq_test_exit(void)
{
	printk("[KT] %s:%d\n", __func__, __LINE__);
	return __kunit_test_suites_exit(exynos_devfreq_test_suites,
				suites_num);
}
*/
kunit_test_suites(&exynos_devfreq_test_suite);
MODULE_LICENSE("GPL");
