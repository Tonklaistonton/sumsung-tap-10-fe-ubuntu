// SPDX-License-Identifier: GPL-2.0

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/kstrtox.h>
#include <linux/lzo.h>
#include <linux/mm.h>

#include <kunit/test.h>
#include <kunit/static_stub.h>
#include <kunit/visibility.h>

#include <soc/samsung/exynos_hw_decomp.h>
#include <soc/samsung/exynos-cpupm.h>

#include "../exynos_hw_decomp_internal.h"
#include "../exynos_hw_decomp_regs.h"
#include "exynos_hw_decomp_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

struct hwdecomp_decomp_ctx {
	void *origin;
	void *compressed;
	void *ctx;
	struct page *page;
	void *decompressed;
	size_t compressed_size;
	int ret;
};

static vendor_hw_decomp_fn decomp_fn;
static bool stub_enabled;
static struct platform_device hwdecomp_test_pdev;
static struct hwdecomp_decomp_ctx decomp_ctx;

GEN_EXYNOS_DECOMP_RW_REGS(SRC_CONFIG)
GEN_EXYNOS_DECOMP_RW_REGS(IF_CONFIG)
GEN_EXYNOS_DECOMP_RW_REGS(DST_CONFIG)
GEN_EXYNOS_DECOMP_RW_REGS(RESULT)
GEN_EXYNOS_DECOMP_RW_REGS(STATUS)

static void __iomem *stub_exynos_hw_decomp_remap(struct device_node *node, int index)
{
	return (void __iomem *)kzalloc(PAGE_SIZE, GFP_KERNEL);
}

static void stub_exynos_hw_decomp_register_cpupm_callback(struct exynos_hw_decomp_desc *desc)
{
	return;
}

static int hwdecomp_test_suite_init(struct kunit_suite *suite)
{
	dev_set_name(&hwdecomp_test_pdev.dev, "hwdecomp_test_dev");

	return 0;
}

static void hwdecomp_test_suite_exit(struct kunit_suite *suite)
{
	return;
}

static void hwdecomp_test_reset_desc(struct exynos_hw_decomp_desc * desc)
{
	/* clear SFRs except for if_config register */
	write_DECOMP_SRC_CONFIG(desc->regs, 0x0);
	write_DECOMP_DST_CONFIG(desc->regs, 0x0);
	write_DECOMP_RESULT(desc->regs, 0x0);
	write_DECOMP_STATUS(desc->regs, 0x0);

	atomic64_set(&desc->nr_success, 0);
	atomic64_set(&desc->nr_timeout, 0);
	atomic64_set(&desc->nr_failure, 0);
	atomic64_set(&desc->nr_unready, 0);
	atomic64_set(&desc->nr_busy, 0);
}

static void hwdecomp_test_alloc_decomp_ctx(struct kunit *test)
{
	decomp_ctx.origin = kunit_kzalloc(test, PAGE_SIZE, GFP_KERNEL);
	decomp_ctx.compressed = kunit_kzalloc(test, PAGE_SIZE * 2, GFP_KERNEL);
	decomp_ctx.ctx = kunit_kzalloc(test, LZO1X_MEM_COMPRESS, GFP_KERNEL);

	decomp_ctx.page = alloc_pages(GFP_KERNEL, 0);
	decomp_ctx.decompressed = page_to_virt(decomp_ctx.page);

	memset(decomp_ctx.origin, 0xA5, PAGE_SIZE);
	decomp_ctx.ret = lzorle1x_1_compress(decomp_ctx.origin, PAGE_SIZE, decomp_ctx.compressed,
						&decomp_ctx.compressed_size, decomp_ctx.ctx);
}

static void hwdecomp_test_free_decomp_ctx(void)
{
	if (!decomp_ctx.page)
		return;

	__free_pages(decomp_ctx.page, 0);
	decomp_ctx.page = NULL;
}


static int hwdecomp_test_init(struct kunit *test)
{
	if (!!decomp_fn) {
		hwdecomp_test_reset_desc(decomp_desc);
		hwdecomp_test_alloc_decomp_ctx(test);
		return 0;
	}

	kunit_activate_static_stub(test, exynos_hw_decomp_remap, stub_exynos_hw_decomp_remap);
	kunit_activate_static_stub(test, exynos_hw_decomp_register_cpupm_callback,
					stub_exynos_hw_decomp_register_cpupm_callback);
	stub_enabled = true;

	exynos_hw_decomp_probe(&hwdecomp_test_pdev);
	decomp_fn = register_vendor_hw_decomp();
	return 0;
}

static void hwdecomp_test_exit(struct kunit *test)
{
	hwdecomp_test_free_decomp_ctx();

	if (stub_enabled) {
		kunit_deactivate_static_stub(test, exynos_hw_decomp_remap);
		kunit_deactivate_static_stub(test, exynos_hw_decomp_register_cpupm_callback);
		stub_enabled = false;
	}
}

static void hwdecomp_initial_setup_test(struct kunit *test)
{
	KUNIT_EXPECT_PTR_NE(test, decomp_fn, NULL);
	KUNIT_EXPECT_NE(test, read_DECOMP_IF_CONFIG(decomp_desc->regs), 0);
}

static void hwdecomp_read_attributes_test(struct kunit *test)
{
	char *buf;
	int i;
	struct attribute **attrs = (*decomp_desc->kobj.ktype->default_groups)->attrs;

	buf = kunit_kzalloc(test, PAGE_SIZE, GFP_KERNEL);

	for (i = 0; !!attrs[i]; i++) {
		u64 val;
		int ret;
		struct kobj_attribute *kobj_attr =
				container_of(attrs[i], struct kobj_attribute, attr);

		if (!kobj_attr->show)
			continue;

		kobj_attr->show(&decomp_desc->kobj, kobj_attr, buf);
		ret = kstrtou64((const char *)buf, 10, &val);
		KUNIT_EXPECT_EQ_MSG(test, ret, 0, "%s: strtou64 failed", kobj_attr->attr.name);
		KUNIT_EXPECT_EQ_MSG(test, val, 0, "%s: %llu", kobj_attr->attr.name, val);
	}
}

static void hwdecomp_suspend_test(struct kunit *test)
{
	write_DECOMP_RESULT(decomp_desc->regs, 0x0);
	exynos_hw_decomp_suspend(decomp_desc->dev);
	KUNIT_EXPECT_NE(test, read_DECOMP_RESULT(decomp_desc->regs), 0);
}

static void hwdecomp_resume_test(struct kunit *test)
{
	write_DECOMP_IF_CONFIG(decomp_desc->regs, 0x0);
	exynos_hw_decomp_resume(decomp_desc->dev);
	KUNIT_EXPECT_NE(test, read_DECOMP_IF_CONFIG(decomp_desc->regs), 0);
}

static void hwdecomp_cpupm_notifier_test(struct kunit *test)
{
	write_DECOMP_IF_CONFIG(decomp_desc->regs, 0x0);

	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, C2_ENTER, NULL);
	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, C2_EXIT, NULL);
	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, CPD_ENTER, NULL);
	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, CPD_EXIT, NULL);
	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, DSUPD_ENTER, NULL);
	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, SICD_ENTER, NULL);
	KUNIT_EXPECT_EQ(test, read_DECOMP_IF_CONFIG(decomp_desc->regs), 0x0);

	write_DECOMP_IF_CONFIG(decomp_desc->regs, 0x0);
	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, DSUPD_EXIT, NULL);
	KUNIT_EXPECT_NE(test, read_DECOMP_IF_CONFIG(decomp_desc->regs), 0);

	write_DECOMP_IF_CONFIG(decomp_desc->regs, 0x0);
	exynos_hw_decomp_cpupm_notifier(&decomp_desc->cpupm_nb, SICD_EXIT, NULL);
	KUNIT_EXPECT_NE(test, read_DECOMP_IF_CONFIG(decomp_desc->regs), 0);
}

static void hwdecomp_register_fail_test(struct kunit *test)
{
	vendor_hw_decomp_fn fn;
	struct exynos_hw_decomp_desc *saved_desc = decomp_desc;

	decomp_desc = NULL;
	fn = register_vendor_hw_decomp();
	KUNIT_EXPECT_PTR_EQ(test, fn, NULL);

	decomp_desc = saved_desc;
	fn = register_vendor_hw_decomp();
	KUNIT_EXPECT_PTR_EQ(test, fn, NULL);
}

static void hwdecomp_decomp_timeout_test(struct kunit *test)
{
	int ret;

	KUNIT_EXPECT_PTR_NE_MSG(test, decomp_ctx.page, NULL, "alloc pages failed");
	KUNIT_EXPECT_EQ_MSG(test, decomp_ctx.ret, 0, "comress failed");

	ret = decomp_fn(decomp_ctx.compressed, decomp_ctx.compressed_size,
				decomp_ctx.decompressed, decomp_ctx.page);
	KUNIT_EXPECT_EQ_MSG(test, ret, LZO_E_OK, "decomp_fn retrun value err %d", ret);
	KUNIT_EXPECT_EQ_MSG(test, memcmp(decomp_ctx.decompressed, decomp_ctx.origin, PAGE_SIZE),
						0, "decompressed data invalid");
	KUNIT_EXPECT_EQ_MSG(test, atomic64_read(&decomp_desc->nr_timeout),
						1, "nr_timeout invalid");
}

static void hwdecomp_decomp_busy_test(struct kunit *test)
{
	int ret;

	KUNIT_EXPECT_PTR_NE_MSG(test, decomp_ctx.page, NULL, "alloc pages failed");
	KUNIT_EXPECT_EQ_MSG(test, decomp_ctx.ret, 0, "comress failed");

	raw_spin_lock(&decomp_desc->lock);
	ret = decomp_fn(decomp_ctx.compressed, decomp_ctx.compressed_size,
				decomp_ctx.decompressed, decomp_ctx.page);
	raw_spin_unlock(&decomp_desc->lock);

	KUNIT_EXPECT_EQ_MSG(test, ret, LZO_E_OK, "decomp_fn retrun value err %d", ret);
	KUNIT_EXPECT_EQ_MSG(test, memcmp(decomp_ctx.decompressed, decomp_ctx.origin, PAGE_SIZE),
						0, "decompressed data invalid");
	KUNIT_EXPECT_EQ_MSG(test, atomic64_read(&decomp_desc->nr_busy),
						1, "nr_timeout invalid");

}

static void hwdecomp_decomp_success_test(struct kunit *test)
{
	RESULT_t result;
	int ret;

	KUNIT_EXPECT_PTR_NE_MSG(test, decomp_ctx.page, NULL, "alloc pages failed");
	KUNIT_EXPECT_EQ_MSG(test, decomp_ctx.ret, 0, "comress failed");

	result.reg = 0;
	result.field.decomp_done = 1;
	write_DECOMP_RESULT(decomp_desc->regs, result.reg);

	ret = decomp_fn(decomp_ctx.compressed, decomp_ctx.compressed_size,
				decomp_ctx.decompressed, decomp_ctx.page);
	KUNIT_EXPECT_EQ_MSG(test, ret, LZO_E_OK, "decomp_fn retrun value err %d", ret);
	KUNIT_EXPECT_NE_MSG(test, memcmp(decomp_ctx.decompressed, decomp_ctx.origin, PAGE_SIZE),
						0, "decompressed data invalid");
	KUNIT_EXPECT_EQ_MSG(test, atomic64_read(&decomp_desc->nr_success),
						1, "nr_timeout invalid");
}

static void hwdecomp_decomp_err_test(struct kunit *test)
{
	RESULT_t result;
	int ret;

	KUNIT_EXPECT_PTR_NE_MSG(test, decomp_ctx.page, NULL, "alloc pages failed");
	KUNIT_EXPECT_EQ_MSG(test, decomp_ctx.ret, 0, "comress failed");

	result.reg = 0;
	result.field.decomp_err = 1;
	write_DECOMP_RESULT(decomp_desc->regs, result.reg);

	ret = decomp_fn(decomp_ctx.compressed, decomp_ctx.compressed_size,
				decomp_ctx.decompressed, decomp_ctx.page);
	KUNIT_EXPECT_EQ_MSG(test, ret, LZO_E_OK, "decomp_fn retrun value err %d", ret);
	KUNIT_EXPECT_EQ_MSG(test, memcmp(decomp_ctx.decompressed, decomp_ctx.origin, PAGE_SIZE),
						0, "decompressed data invalid");
	KUNIT_EXPECT_EQ_MSG(test, atomic64_read(&decomp_desc->nr_failure),
						1, "nr_timeout invalid");
}

static void hwdecomp_decomp_unready_test(struct kunit *test)
{
	STATUS_t status;
	int ret;

	KUNIT_EXPECT_PTR_NE_MSG(test, decomp_ctx.page, NULL, "alloc pages failed");
	KUNIT_EXPECT_EQ_MSG(test, decomp_ctx.ret, 0, "comress failed");

	status.reg = 0;
	status.field.status = ~0x1;
	write_DECOMP_STATUS(decomp_desc->regs, status.reg);

	ret = decomp_fn(decomp_ctx.compressed, decomp_ctx.compressed_size,
				decomp_ctx.decompressed, decomp_ctx.page);
	KUNIT_EXPECT_EQ_MSG(test, ret, LZO_E_OK, "decomp_fn retrun value err %d", ret);
	KUNIT_EXPECT_EQ_MSG(test, memcmp(decomp_ctx.decompressed, decomp_ctx.origin, PAGE_SIZE),
						0, "decompressed data invalid");
	KUNIT_EXPECT_EQ_MSG(test, atomic64_read(&decomp_desc->nr_unready),
						1, "nr_timeout invalid");
}

static struct kunit_case hwdecomp_test_cases[] = {
	KUNIT_CASE(hwdecomp_initial_setup_test),
	KUNIT_CASE(hwdecomp_read_attributes_test),
	KUNIT_CASE(hwdecomp_suspend_test),
	KUNIT_CASE(hwdecomp_resume_test),
	KUNIT_CASE(hwdecomp_cpupm_notifier_test),
	KUNIT_CASE(hwdecomp_register_fail_test),
	KUNIT_CASE(hwdecomp_decomp_timeout_test),
	KUNIT_CASE(hwdecomp_decomp_busy_test),
	KUNIT_CASE(hwdecomp_decomp_success_test),
	KUNIT_CASE(hwdecomp_decomp_err_test),
	KUNIT_CASE(hwdecomp_decomp_unready_test),
	{}
};

static struct kunit_suite hwdecomp_test_suite = {
	.name = "hwdecomp_exynos",
	.init = hwdecomp_test_init,
	.exit = hwdecomp_test_exit,
	.suite_init = hwdecomp_test_suite_init,
	.suite_exit = hwdecomp_test_suite_exit,
	.test_cases = hwdecomp_test_cases,
};

kunit_test_suites(&hwdecomp_test_suite);

MODULE_LICENSE("GPL");

