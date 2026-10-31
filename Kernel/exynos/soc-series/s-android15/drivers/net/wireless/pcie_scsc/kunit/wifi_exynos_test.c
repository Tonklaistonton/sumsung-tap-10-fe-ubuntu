// SPDX-License-Identifier: GPL-2.0+

#include <kunit/test.h>

#include "kunit-common.h"
//#include "wifi_exynos_test.h"
#include "../if_vif.c"

/* unit test function definition */
static void test_slsi_is_valid_vifnum(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->vifnum = FAPI_VIFRANGE_VIF_INDEX_MIN;
	KUNIT_EXPECT_TRUE(test, slsi_is_valid_vifnum(sdev, dev));
}

/*
 * Test fictures
 */
static int wifi_exynos_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void wifi_exynos_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return;
}

/*
 * KUnit testcase definitions
 */
static struct kunit_case wifi_exynos_test_cases[] = {
	KUNIT_CASE(test_slsi_is_valid_vifnum),
	{}
};

static struct kunit_suite wifi_exynos_test_suite[] = {
	{
		.name = "wifi_exynos",
		.test_cases = wifi_exynos_test_cases,
		.init = wifi_exynos_test_init,
		.exit = wifi_exynos_test_exit,
	}};

kunit_test_suites(wifi_exynos_test_suite);

MODULE_LICENSE("GPL");

