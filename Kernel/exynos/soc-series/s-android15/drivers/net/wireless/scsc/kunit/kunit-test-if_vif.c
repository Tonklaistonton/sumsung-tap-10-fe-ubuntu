// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "../if_vif.c"

static void test_slsi_get_ifnum_by_vifid(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u16 vif_id = 0;

	KUNIT_EXPECT_EQ(test, 0, slsi_get_ifnum_by_vifid(sdev, vif_id));

	sdev->vif_netdev_id_map[vif_id] = 1;
	sdev->nan_max_ndp_instances = 1;
	vif_id = 1;
	KUNIT_EXPECT_EQ(test, sdev->vif_netdev_id_map[vif_id], slsi_get_ifnum_by_vifid(sdev, vif_id));

	vif_id = FAPI_VIFRANGE_VIF_INDEX_MAX;
	KUNIT_EXPECT_EQ(test, SLSI_NAN_DATA_IFINDEX_START, slsi_get_ifnum_by_vifid(sdev, vif_id));

	vif_id = FAPI_VIFRANGE_VIF_INDEX_MAX + 1;
	KUNIT_EXPECT_EQ(test, SLSI_INVALID_IFNUM, slsi_get_ifnum_by_vifid(sdev, vif_id));
}

static void test_slsi_mlme_assign_vif(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	u16 vif_id = 0;

	slsi_mlme_assign_vif(sdev, dev, vif_id);
}

static void test_slsi_mlme_clear_vif(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	u16 vif_id = 0;

	slsi_mlme_clear_vif(sdev, dev, vif_id);
}

static void test_slsi_is_valid_vifnum(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->vifnum = FAPI_VIFRANGE_VIF_INDEX_MAX + 1;
	KUNIT_EXPECT_EQ(test, false, slsi_is_valid_vifnum(sdev, dev));

	ndev_vif->vifnum = FAPI_VIFRANGE_VIF_INDEX_MIN;
	KUNIT_EXPECT_EQ(test, true, slsi_is_valid_vifnum(sdev, dev));
}

static int if_vif_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void if_vif_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case if_vif_test_cases[] = {
	KUNIT_CASE(test_slsi_get_ifnum_by_vifid),
	KUNIT_CASE(test_slsi_mlme_assign_vif),
	KUNIT_CASE(test_slsi_mlme_clear_vif),
	KUNIT_CASE(test_slsi_is_valid_vifnum),
	{}
};

static struct kunit_suite if_vif_test_suite[] = {
	{
		.name = "kunit-if_vif-test",
		.test_cases = if_vif_test_cases,
		.init = if_vif_test_init,
		.exit = if_vif_test_exit,
	}
};

kunit_test_suites(if_vif_test_suite);
