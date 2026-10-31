// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include "../abox.h"

//static struct device dev;

static int audio_exynos_test_init(struct kunit *test)
{
//	memset(&dev, 0, sizeof(dev));
//	memset(&exynos, 0, sizeof(exynos));
//	memset(&fsm, 0, sizeof(fsm));

//	dev_set_drvdata(&dev, &exynos);
//	exynos.rsw.fsm = &fsm;

	return 0;
}

static void audio_exynos_sample_test(struct kunit *test)
{
	unsigned int result;
	bool input = true;

	result = abox_get_waiting_ns(input);

	KUNIT_EXPECT_EQ(test, 0, result);
}

static struct kunit_case audio_exynos_test_cases[] = {
	KUNIT_CASE(audio_exynos_sample_test),
	{}
};

static struct kunit_suite audio_exynos_test_suite = {
	.name = "audio_exynos",
	.init = audio_exynos_test_init,
	.test_cases = audio_exynos_test_cases,
};

kunit_test_suites(&audio_exynos_test_suite);

MODULE_LICENSE("GPL");

