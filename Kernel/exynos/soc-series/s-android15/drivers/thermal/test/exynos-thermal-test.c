#include <kunit/test.h>
#include <kunit/visibility.h>
#include <kunit/static_stub.h>

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);
int emul_call_get(void *data, unsigned long long *val);
bool exynos_acpm_tmu_is_test_mode(void);

static bool stub_exynos_acpm_tmu_is_test_mode(void)
{
	return 1;
}

static void exynos_thermal_test_0(struct kunit *test)
{
	int ret = 0;
	unsigned long long val;

	ret = emul_call_get(NULL, &val);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1, val);
}

static int exynos_thermal_test_init(struct kunit *test)
{
	kunit_activate_static_stub(test, exynos_acpm_tmu_is_test_mode,
		   	stub_exynos_acpm_tmu_is_test_mode);

	kunit_info(test, "%s\n", __func__);

	return 0;
}

static void exynos_thermal_test_exit(struct kunit *test)
{
	kunit_deactivate_static_stub(test, exynos_acpm_tmu_is_test_mode);

	kunit_info(test, "%s\n", __func__);
}

static struct kunit_case exynos_thermal_test_cases[] = {
	KUNIT_CASE(exynos_thermal_test_0),
	{},
};

static struct kunit_suite exynos_thermal_test_suite = {
	.name = "dtm_exynos_test",
	.init = exynos_thermal_test_init,
	.exit = exynos_thermal_test_exit,
	.test_cases = exynos_thermal_test_cases,
};

kunit_test_suites(&exynos_thermal_test_suite);

MODULE_LICENSE("GPL");
