#include <kunit/test.h>

#include "../platform_mif.h"
#include <linux/platform_device.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
#include "../modap/platform_mif_regmap_api.h"
#endif

static void test_all(struct kunit *test)
{
	struct scsc_mif_abs *scscmif;
	struct platform_device pdev;
	struct device dev;
	struct scsc_mifqos_request qos_req;
	struct platform_mif *platform;

	pdev.dev = dev;
	set_test_in_platform_mif(test);

	scscmif = platform_mif_create(&pdev);

	platform = container_of(scscmif, struct platform_mif, interface);

	platform_mif_set_dbus_baaw(platform);
	platform_mif_set_pbus_baaw(platform);
	platform_mif_init_regdump(platform);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(test_all),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_platform_mif_regmap_api",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};


kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("joono.chung@samsung.com>");

