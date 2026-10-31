// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <linux/pm_runtime.h>
#include <linux/usb/otg-fsm.h>
#include "dwc3-exynos.h"

static struct device dev;
static struct dwc3_exynos exynos;
static struct otg_fsm fsm;

static int usb_exynos_test_init(struct kunit *test)
{
	memset(&dev, 0, sizeof(dev));
	memset(&exynos, 0, sizeof(exynos));
	memset(&fsm, 0, sizeof(fsm));

	dev_set_drvdata(&dev, &exynos);
	exynos.rsw.fsm = &fsm;

	return 0;
}

static void usb_exynos_sample_test(struct kunit *test)
{
	int result;

	result = dwc3_exynos_rsw_start(&dev);

	KUNIT_EXPECT_EQ(test, 0, result);
}

static struct kunit_case usb_exynos_test_cases[] = {
	KUNIT_CASE(usb_exynos_sample_test),
	{}
};

static struct kunit_suite usb_exynos_test_suite = {
	.name = "usb_exynos",
	.init = usb_exynos_test_init,
	.test_cases = usb_exynos_test_cases,
};

kunit_test_suites(&usb_exynos_test_suite);

MODULE_LICENSE("GPL");
