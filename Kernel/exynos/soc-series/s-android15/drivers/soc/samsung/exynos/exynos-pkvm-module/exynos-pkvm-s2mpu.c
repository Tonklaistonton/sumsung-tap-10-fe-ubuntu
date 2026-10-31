/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS Stage 2 Protection Unit(S2MPU) Module for pKVM
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/kvm_host.h>

#include <soc/samsung/exynos/exynos-pkvm-module.h>

#include <asm/kvm_host.h>
#include <asm/kvm_pkvm.h>

#define ksym_ref_addr_nvhe(x) \
	((typeof(x) *)(pkvm_el2_mod_va(&(x), token)))

extern pkvm_mod_vm_request_t __kvm_nvhe_exynos_pkvm_mod_vm_request;
static unsigned long token;
extern struct kvm_iommu_ops __kvm_nvhe_exynos_kvm_iommu_ops;
int __kvm_nvhe_exynos_pkvm_s2mpu_module_init(const struct pkvm_module_ops *ops);

/*
 * Pre allocated pages that can be used from the EL2 part of the driver from atomic
 * context, ideally used for page table pages for identity domains.
 */
static int atomic_pages;
module_param(atomic_pages, int, 0);

struct s2mpu_drvdata {
	struct device *dev;
	void __iomem *sfrbase;
	bool registered;
};

static int exynos_pkvm_s2mpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s2mpu_drvdata *data;
	struct resource *res;
	int ret = 0;
	struct kvm_hyp_memcache atomic_mc = {};

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to get resource info\n");
		return -ENOENT;
	}

	data->sfrbase = devm_ioremap_resource(dev, res);
	if (IS_ERR(data->sfrbase)) {
		dev_err(dev, "failed to ioremap resource: %ld\n",
				PTR_ERR(data->sfrbase));
		return PTR_ERR(data->sfrbase);
	}

	data->registered = (ret != -ENODEV);
	if (!data->registered) {
		dev_info(dev, "pkvm-S2MPU driver disabled, pKVM not enabled\n");
		goto out_free_mem;
	}

	platform_set_drvdata(pdev, data);

	__kvm_nvhe_exynos_pkvm_mod_vm_request = exynos_pkvm_module_get_s2mpu_ptr();

	ret = kvm_iommu_init_hyp(ksym_ref_addr_nvhe(__kvm_nvhe_exynos_kvm_iommu_ops),
			&atomic_mc,
			0);
	if (ret) {
		pr_notice("%s: Exynos pKVM s2mpu init failed, ret[%d]\n",
				__func__, ret);
	} else {
		pr_notice("%s: Exynos pKVM s2mpu init done\n", __func__);
	}

	return 0;

out_free_mem:
	kfree(data);

	return 0;
}

static const struct of_device_id exynos_pkvm_s2mpu_of_match[] = {
	{ .compatible = "samsung,pkvm-s2mpu" },
	{},
};

static struct platform_driver exynos_pkvm_s2mpu_driver = {
	.driver = {
		.name = "exynos-pkvm-s2mpu",
		.of_match_table = exynos_pkvm_s2mpu_of_match,
	},
};

static int exynos_pkvm_s2mpu_init(void)
{
	int ret = 0;

	ret = platform_driver_probe(&exynos_pkvm_s2mpu_driver, exynos_pkvm_s2mpu_probe);
	if (ret)
		return ret;

	return ret;
}

static void exynos_pkvm_s2mpu_remove(void)
{
	platform_driver_unregister(&exynos_pkvm_s2mpu_driver);
}

pkvm_handle_t exynos_pkvm_s2mpu_get_iommu(struct device *dev)
{
	return 0;
}

struct kvm_iommu_driver exynos_pkvm_s2mpu_ops = {
	.init_driver = exynos_pkvm_s2mpu_init,
	.remove_driver = exynos_pkvm_s2mpu_remove,
	.get_iommu_id = exynos_pkvm_s2mpu_get_iommu,
};

static int exynos_pkvm_s2mpu_register(void)
{
	int ret = 0;

	ret = kvm_iommu_register_driver(&exynos_pkvm_s2mpu_ops);
	if (ret) {
		pr_err("%s: pKVM S2MPU driver register Failed, ret[%d]\n",
				__func__, ret);
		return ret;
	}

	ret = pkvm_load_el2_module(__kvm_nvhe_exynos_pkvm_s2mpu_module_init, &token);
	if (ret && ret != -EOPNOTSUPP) {
		pr_err("%s: Exynos pKVM module init fail ret[%d]\n",
				__func__, ret);
		return ret;
	}

	pr_info("%s: pKVM S2MPU driver register Done\n", __func__);
	return ret;
}

module_init(exynos_pkvm_s2mpu_register);

MODULE_DESCRIPTION("Exynos Hypervisor S2MPU driver for pKVM");
MODULE_LICENSE("GPL");
