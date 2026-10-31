// SPDX-License-Identifier: GPL-2.0
/*
 * Exynos - HW Decompression engine Driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 *
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/lzo.h>
#include <linux/of_address.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/cpu_pm.h>
#include <linux/sysfs.h>

#include <kunit/static_stub.h>
#include <kunit/visibility.h>

#include <soc/samsung/exynos_hw_decomp.h>
#include <soc/samsung/exynos-cpupm.h>

#include "exynos_hw_decomp_internal.h"
#include "exynos_hw_decomp_regs.h"

static DEFINE_SPINLOCK(decomp_register_lock);
VISIBLE_IF_KUNIT struct exynos_hw_decomp_desc *decomp_desc;
EXPORT_SYMBOL_IF_KUNIT(decomp_desc);

GEN_EXYNOS_DECOMP_RW_REGS(SRC_CONFIG)
GEN_EXYNOS_DECOMP_RW_REGS(IF_CONFIG)
GEN_EXYNOS_DECOMP_RW_REGS(DST_CONFIG)
GEN_EXYNOS_DECOMP_RW_REGS(RESULT)
GEN_EXYNOS_DECOMP_RW_REGS(STATUS)

static void clear_decompressor(struct exynos_hw_decomp_desc *desc)
{
	RESULT_t result;

	result.reg = 0;
	result.field.decomp_done = 0x1;
	result.field.decomp_err = 0x1;

	write_DECOMP_RESULT(desc->regs, result.reg);
}

static int decomp_setup_cmd(struct exynos_hw_decomp_desc *desc,
			void *src, size_t src_len, struct page *page)
{
	void *src_vaddr;
	unsigned long src_paddr;
	unsigned long dst_paddr;
	STATUS_t status;
	SRC_CONFIG_t src_config;
	DST_CONFIG_t dst_config;

	status.reg = read_DECOMP_STATUS(desc->regs);
	if (status.field.status != 0 && status.field.status != 0x1) {
		clear_decompressor(desc);
		return -1;
	}

	if ((unsigned long)src & (SZ_64 - 1)) {
		src_vaddr = (void *)(*per_cpu_ptr(desc->bounce_buffer, raw_smp_processor_id()));
		memcpy(src_vaddr, src, src_len);
		src_paddr = virt_to_phys(src_vaddr);
		mb();
	} else {
		src_paddr = virt_to_phys(src);
	}
	dst_paddr = page_to_phys(page);

	src_config.reg = 0;
	src_config.field.src_addr = src_paddr;
	src_config.field.src_size = src_len;
	write_DECOMP_SRC_CONFIG(desc->regs, src_config.reg);

	dst_config.reg = 0;
	dst_config.field.dst_addr = dst_paddr;
	dst_config.field.debug_en = 0x1;
	write_DECOMP_DST_CONFIG(desc->regs, dst_config.reg);

	dst_config.field.enable = 0x1;
	write_DECOMP_DST_CONFIG(desc->regs, dst_config.reg);

	return 0;
}

static int decomp_wait_for_completion(struct exynos_hw_decomp_desc *desc)
{
	unsigned long timeout;
	RESULT_t result;

	timeout = jiffies + msecs_to_jiffies(8);

	do {
		result.reg = read_DECOMP_RESULT(desc->regs);

		if (time_after(jiffies, timeout)) {
			atomic64_inc(&desc->nr_timeout);
			return -ETIME;
		}

	} while (!result.field.decomp_done && !result.field.decomp_err);

	if (result.field.decomp_done)
		mb();

	if (result.field.decomp_err) {
		STATUS_t status;

		status.reg = read_DECOMP_STATUS(desc->regs);
		dev_err(desc->dev, "%s: err status = [%#llx]\n", __func__, status.reg);
		atomic64_inc(&desc->nr_failure);
		return -EADV;
	}

	atomic64_inc(&desc->nr_success);

	return 0;
}

static int exynos_hw_decompress(const unsigned char *src, size_t src_len,
				unsigned char *dst, struct page *page)
{
	struct exynos_hw_decomp_desc *desc = decomp_desc;
	size_t dst_len = PAGE_SIZE;
	int ret = LZO_E_OK;

	/*
	 * It may use per-cpu bound buffer for src data. A src buffer
	 * contamination problem may occur when it is called in interrupt context.
	 */
	WARN_ON(in_interrupt());

	if (!raw_spin_trylock(&desc->lock)) {
		atomic64_inc(&desc->nr_busy);
		return lzo1x_decompress_safe(src, src_len, dst, &dst_len);
	}

	if (decomp_setup_cmd(desc, (void *)src, src_len, page)) {
		raw_spin_unlock(&desc->lock);
		atomic64_inc(&desc->nr_unready);
		return lzo1x_decompress_safe(src, src_len, dst, &dst_len);
	}

	if (decomp_wait_for_completion(desc))
		ret = lzo1x_decompress_safe(src, src_len, dst, &dst_len);

	clear_decompressor(desc);

	raw_spin_unlock(&desc->lock);

	return ret;
}

vendor_hw_decomp_fn register_vendor_hw_decomp(void)
{
	spin_lock(&decomp_register_lock);
	if (!decomp_desc) {
		spin_unlock(&decomp_register_lock);
		pr_info("%s: hw decomp is not used\n", __func__);
		return NULL;
	}

	if (!!decomp_desc->owner) {
		spin_unlock(&decomp_register_lock);
		pr_info("%s: hw decomp is already used\n", __func__);
		return NULL;
	}

	decomp_desc->owner = (unsigned long)return_address(0);
	spin_unlock(&decomp_register_lock);
	pr_info("%s: hw decomp is registerd\n", __func__);

	return exynos_hw_decompress;
}
EXPORT_SYMBOL(register_vendor_hw_decomp);

static void prepare_exynos_hw_decomp(struct exynos_hw_decomp_desc *desc)
{
	IF_CONFIG_t if_config;

	if_config.reg = 0;
	if_config.field.wu_policy = 0x1;
	if_config.field.arcache = 0xf;
	if_config.field.awcache = 0xf;
	if_config.field.ardomain = 0x2;
	if_config.field.awdomain = 0x2;
	if_config.field.page_perf_up = 0x1;

	write_DECOMP_IF_CONFIG(desc->regs, if_config.reg);

	return;
}

static void clear_err_exynos_hw_decomp(struct exynos_hw_decomp_desc *desc)
{
	clear_decompressor(desc);
	return;
}

VISIBLE_IF_KUNIT int exynos_hw_decomp_cpupm_notifier(struct notifier_block *self,
						unsigned long action, void *v)
{
	struct exynos_hw_decomp_desc *desc = container_of(self, struct exynos_hw_decomp_desc,
								cpupm_nb);

	switch (action) {
	case SICD_ENTER:
	case DSUPD_ENTER:
		clear_err_exynos_hw_decomp(desc);
		break;
	case SICD_EXIT:
	case DSUPD_EXIT:
		prepare_exynos_hw_decomp(desc);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}
EXPORT_SYMBOL_IF_KUNIT(exynos_hw_decomp_cpupm_notifier);

VISIBLE_IF_KUNIT int exynos_hw_decomp_suspend(struct device *dev)
{
	struct exynos_hw_decomp_desc *desc;

	desc = dev_get_drvdata(dev);

	if (!desc)
		return 0;

	clear_err_exynos_hw_decomp(desc);

	return 0;
}
EXPORT_SYMBOL_IF_KUNIT(exynos_hw_decomp_suspend);

VISIBLE_IF_KUNIT int exynos_hw_decomp_resume(struct device *dev)
{
	struct exynos_hw_decomp_desc *desc;

	desc = dev_get_drvdata(dev);

	if (!desc)
		return 0;

	prepare_exynos_hw_decomp(desc);

	return 0;
}
EXPORT_SYMBOL_IF_KUNIT(exynos_hw_decomp_resume);

static const struct dev_pm_ops decomp_pm_ops = {
	.suspend = exynos_hw_decomp_suspend,
	.resume = exynos_hw_decomp_resume,
};

static ssize_t nr_success_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct exynos_hw_decomp_desc *desc = container_of(kobj,
						struct exynos_hw_decomp_desc, kobj);

	return sysfs_emit(buf, "%llu\n", atomic64_read(&desc->nr_success));
}
static struct kobj_attribute nr_success_attr = __ATTR_RO(nr_success);

static ssize_t nr_timeout_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct exynos_hw_decomp_desc *desc = container_of(kobj,
						struct exynos_hw_decomp_desc, kobj);

	return sysfs_emit(buf, "%llu\n", atomic64_read(&desc->nr_timeout));
}
static struct kobj_attribute nr_timeout_attr = __ATTR_RO(nr_timeout);

static ssize_t nr_failure_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct exynos_hw_decomp_desc *desc = container_of(kobj,
						struct exynos_hw_decomp_desc, kobj);

	return sysfs_emit(buf, "%llu\n", atomic64_read(&desc->nr_failure));
}
static struct kobj_attribute nr_failure_attr = __ATTR_RO(nr_failure);

static ssize_t nr_unready_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct exynos_hw_decomp_desc *desc = container_of(kobj,
						struct exynos_hw_decomp_desc, kobj);

	return sysfs_emit(buf, "%llu\n", atomic64_read(&desc->nr_unready));
}
static struct kobj_attribute nr_unready_attr = __ATTR_RO(nr_unready);

static ssize_t nr_busy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct exynos_hw_decomp_desc *desc = container_of(kobj,
						struct exynos_hw_decomp_desc, kobj);

	return sysfs_emit(buf, "%llu\n", atomic64_read(&desc->nr_busy));
}
static struct kobj_attribute nr_busy_attr = __ATTR_RO(nr_busy);

static struct attribute *exynos_decomp_attrs[] = {
	&nr_success_attr.attr,
	&nr_timeout_attr.attr,
	&nr_failure_attr.attr,
	&nr_unready_attr.attr,
	&nr_busy_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(exynos_decomp);

static struct kobj_type exynos_decomp_ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
	.default_groups = exynos_decomp_groups,
};

VISIBLE_IF_KUNIT void __iomem *exynos_hw_decomp_remap(struct device_node *node, int index)
{
	KUNIT_STATIC_STUB_REDIRECT(exynos_hw_decomp_remap, node, index);

	return of_iomap(node, index);
}
EXPORT_SYMBOL_IF_KUNIT(exynos_hw_decomp_remap);

VISIBLE_IF_KUNIT void exynos_hw_decomp_register_cpupm_callback(struct exynos_hw_decomp_desc *desc)
{
	desc->cpupm_nb.notifier_call = exynos_hw_decomp_cpupm_notifier;

	KUNIT_STATIC_STUB_REDIRECT(exynos_hw_decomp_register_cpupm_callback, desc);

	exynos_cpupm_notifier_register(&desc->cpupm_nb);
}
EXPORT_SYMBOL_IF_KUNIT(exynos_hw_decomp_register_cpupm_callback);

VISIBLE_IF_KUNIT int exynos_hw_decomp_probe(struct platform_device *pdev)
{
	struct exynos_hw_decomp_desc *desc;
	int cpu;
	int ret = 0;

	if (PAGE_SIZE != SZ_4K) {
		dev_info(&pdev->dev, "dummy probed(Unsupported page size[0x%#lx])\n", PAGE_SIZE);
		dev_set_drvdata(&pdev->dev, NULL);
		goto out;
	}

	desc = kzalloc(sizeof(struct exynos_hw_decomp_desc), GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto out_fail;
	}

	desc->bounce_buffer = alloc_percpu(unsigned long);
	if (!desc->bounce_buffer) {
		ret = -ENOMEM;
		goto out_free_desc;
	}

	for_each_possible_cpu(cpu) {
		unsigned long buf = __get_free_pages(GFP_KERNEL, 0);

		if (!buf) {
			int _cpu;

			for (_cpu = 0; _cpu < cpu; _cpu++)
				free_pages(*per_cpu_ptr(desc->bounce_buffer, _cpu), 0);
			ret = -ENOMEM;
			goto out_free_percpu_ptr;
		}
		*per_cpu_ptr(desc->bounce_buffer, cpu) = buf;
	}

	desc->regs = exynos_hw_decomp_remap(pdev->dev.of_node, 0);
	if (!desc->regs) {
		ret = -ENOMEM;
		goto out_free_bounce_buffers;
	}

	atomic64_set(&desc->nr_success, 0);
	atomic64_set(&desc->nr_timeout, 0);
	atomic64_set(&desc->nr_failure, 0);
	atomic64_set(&desc->nr_unready, 0);
	atomic64_set(&desc->nr_busy, 0);

	ret = kobject_init_and_add(&desc->kobj, &exynos_decomp_ktype,
					kernel_kobj, "%s", "hwdecomp");
	if (ret)
		goto out_iounmap_and_put_kobj;

	raw_spin_lock_init(&desc->lock);
	desc->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, desc);

	exynos_hw_decomp_register_cpupm_callback(desc);

	prepare_exynos_hw_decomp(desc);
	decomp_desc = desc;

	dev_info(&pdev->dev, "probed\n");

	goto out;

out_iounmap_and_put_kobj:
	iounmap(desc->regs);
	kobject_put(&desc->kobj);
out_free_bounce_buffers:
	for_each_possible_cpu(cpu)
		free_pages(*per_cpu_ptr(desc->bounce_buffer, cpu), 0);
out_free_percpu_ptr:
	free_percpu(desc->bounce_buffer);
out_free_desc:
	kfree(desc);
out_fail:
	dev_err(&pdev->dev, "Fail to probe %d\n", ret);
out:
	return ret;
}
EXPORT_SYMBOL_IF_KUNIT(exynos_hw_decomp_probe);

static const struct of_device_id exynos_hw_decomp_match[] = {
	{ .compatible = "samsung,exynos-hw-decomp" },
	{},
};

static struct platform_driver exynos_hw_decomp_driver = {
	.probe	= exynos_hw_decomp_probe,
	.driver	= {
		.name = "exynos-hw-decomp",
		.owner	= THIS_MODULE,
		.of_match_table	= exynos_hw_decomp_match,
		.pm = &decomp_pm_ops,
	},
};
module_platform_driver(exynos_hw_decomp_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Donghyeok Choe <d7271.choe@samsung.com>");
MODULE_DESCRIPTION("Exynos HW decompression engine driver");
