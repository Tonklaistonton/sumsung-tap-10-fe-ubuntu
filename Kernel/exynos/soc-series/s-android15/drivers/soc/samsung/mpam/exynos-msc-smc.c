// SPDX-License-Identifier: GPL-2.0+
/*
 * SMC MSC(Memory-System Component) for exynos
 *
 * Auther : YEONGHWAN SON (yhwan.son@samsung.com)
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "exynos-msc-smc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeonghwan Son <yhwan.son@samsung.com>");
MODULE_DESCRIPTION("Exynos SMC MSC");

static void msc_smc_set_request(struct msc_smc_domain *msc_smc, u32 partid, int request)
{
	struct msc_domain *msc = &msc_smc->msc;
	unsigned long reg;
	int cnt;

	lockdep_assert_held(&msc->lock);

	/* smc supports only 0~31 partid */
	if (partid >= mpam_get_partid_count(msc) || partid >= BITS_PER_TYPE(u32))
		return;

	request = !!request;

	reg = readl_relaxed(msc_smc->base[0] + PARTIDSEL_0);
	assign_bit(partid, &reg, request);

	for (cnt = 0; cnt < msc_smc->base_count; cnt++) {
		writel_relaxed(reg, msc_smc->base[cnt] + PARTIDSEL_0);
		writel_relaxed(reg, msc_smc->base[cnt] + PARTIDSEL_1);
	}

	msc_smc->ko_parts[partid].request = request;
}

static ssize_t msc_smc_request_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	struct smc_part_kobj *spk = container_of(kobj, struct smc_part_kobj, kobj);
	struct msc_smc_domain *msc_smc = spk->msc_smc;
	struct msc_domain *msc = &msc_smc->msc;
	unsigned long reg;
	int val;
	unsigned long flags;

	spin_lock_irqsave(&msc->lock, flags);
	reg = readl_relaxed(msc_smc->base[0] + PARTIDSEL_0);
	val = test_bit(spk->partid, &reg);
	spin_unlock_irqrestore(&msc->lock, flags);

	return sprintf(buf, "%d\n", val);
}

static ssize_t msc_smc_request_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct smc_part_kobj *spk = container_of(kobj, struct smc_part_kobj, kobj);
	struct msc_smc_domain *msc_smc = spk->msc_smc;
	struct msc_domain *msc = &msc_smc->msc;
	unsigned long flags;
	int req;

	if (sscanf(buf, "%d", &req) != 1)
		return -EINVAL;

	spin_lock_irqsave(&msc->lock, flags);
	msc_smc_set_request(msc_smc, spk->partid, req);
	spin_unlock_irqrestore(&msc->lock, flags);

	return size;
}

static struct kobj_attribute msc_smc_request_attr =
	__ATTR(request, 0644, msc_smc_request_show, msc_smc_request_store);

static struct attribute *msc_smc_ctrl_attrs[] = {
	&msc_smc_request_attr.attr,
	NULL,
};
static struct attribute_group msc_smc_ctrl_attr_group = {
	.attrs = msc_smc_ctrl_attrs,
};

static int msc_smc_create_entry_sysfs(struct msc_smc_domain *msc_smc)
{
	int ret = 0;
	int part;

	kobject_init(&msc_smc->ko_entry_dir, get_mpam_kobj_ktype());
	ret = kobject_add(&msc_smc->ko_entry_dir, &msc_smc->ko_root, "entries");
	if (ret)
		goto err_entry_dir;

	for (part = 0; part < NUM_MPAM_ENTRIES; part++) {
		ret = sysfs_create_link(&msc_smc->ko_entry_dir, &msc_smc->ko_parts[part].kobj, mpam_entry_names[part]);
		if (ret) {
			pr_err("Failed to link entry to part");
			return ret;
		}
	}

	return ret;

err_entry_dir:
	kobject_put(&msc_smc->ko_entry_dir);
	return ret;
}

static int msc_smc_create_sysfs(struct platform_device *pdev, struct msc_smc_domain *msc_smc)
{
	int ret;
	u32 part, tmp;
	u32 partid_count = mpam_get_partid_count(&msc_smc->msc);

	kobject_init(&msc_smc->ko_root, get_mpam_kobj_ktype());
	ret = kobject_add(&msc_smc->ko_root, &pdev->dev.kobj, "msc_smc");
	if (ret)
		goto err_root_dir;

	kobject_init(&msc_smc->ko_part_dir, get_mpam_kobj_ktype());
	ret = kobject_add(&msc_smc->ko_part_dir, &msc_smc->ko_root, "partitions");
	if (ret)
		goto err_part_dir;

	msc_smc->ko_parts = devm_kzalloc(&msc_smc->msc.pdev->dev,
				     sizeof(*msc_smc->ko_parts) * partid_count,
				     GFP_KERNEL);
	if (!msc_smc->ko_parts) {
		ret = -ENOMEM;
		goto err_part_dir;
	}

	for (part = 0; part < partid_count; part++) {
		kobject_init(&msc_smc->ko_parts[part].kobj, get_mpam_kobj_ktype());
		msc_smc->ko_parts[part].msc_smc = msc_smc;
		msc_smc->ko_parts[part].partid = part;
		msc_smc->ko_parts[part].request = 0;
		ret = kobject_add(&msc_smc->ko_parts[part].kobj, &msc_smc->ko_part_dir, "%d", part);
		if (ret)
			goto err_parts_add;
	}

	for (part = 0; part < partid_count; part++) {
		ret = sysfs_create_group(&msc_smc->ko_parts[part].kobj, &msc_smc_ctrl_attr_group);
		if (ret)
			goto err_parts_grp;
	}

	ret = msc_smc_create_entry_sysfs(msc_smc);
	if (ret)
		goto err_create_entry;

	msc_smc->msc.ko_root = &msc_smc->ko_root;

	return 0;

err_create_entry:
err_parts_grp:
	for (tmp = 0; tmp < part; tmp++)
		sysfs_remove_group(&msc_smc->ko_parts[part].kobj, &msc_smc_ctrl_attr_group);
	part = partid_count;

err_parts_add:
	for (tmp = 0; tmp < part; tmp++)
		kobject_put(&msc_smc->ko_parts[tmp].kobj);

	devm_kfree(&msc_smc->msc.pdev->dev, msc_smc->ko_parts);
err_part_dir:
	kobject_put(&msc_smc->ko_part_dir);
err_root_dir:
	kobject_put(&msc_smc->ko_root);
	return ret;
}

static void msc_smc_restore_attr(struct msc_domain *msc)
{
	/* DMC register is restored by other IP */
	return;
}

static int init_msc(struct msc_smc_domain *msc_smc, struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *dn = pdev->dev.of_node;
	struct msc_domain *msc = &msc_smc->msc;
	unsigned int partid;
	int cnt;
	unsigned long flags;
	u64 reg;

	if (!dn) {
		pr_err("msc_smc: Failed to get device tree");
		return -EINVAL;
	}

	msc->restore_attr = msc_smc_restore_attr;
	msc->pdev = pdev;

	ret |= of_property_read_s32(dn, "msc-type", &msc->id);
	ret |= of_property_read_u32(dn, "base-count", &msc_smc->base_count);

	if (msc_smc->base_count > NUM_OF_MAX_BLOCK) {
		pr_err("msc_smc: there are too many blocks");
		return -EINVAL;
	}

	ret |= of_property_read_u32_array(dn, "base", msc_smc->base_addr, msc_smc->base_count);
	ret |= of_property_read_u32(dn, "size", &msc->size);

	if (ret) {
		pr_err("msc_smc: Failed to parse device tree");
		return ret;
	}

	for (cnt = 0; cnt < msc_smc->base_count; cnt++) {
		msc_smc->base[cnt] = ioremap(msc_smc->base_addr[cnt], msc->size);
		if (IS_ERR(msc_smc->base[cnt]))
			return PTR_ERR(msc_smc->base[cnt]);
	}

	msc->base = msc_smc->base[0];
	msc->base_addr = msc_smc->base_addr[0];
	spin_lock_init(&msc->lock);

	msc->partid_count = SMC_DEFAULT_PARTID_COUNT;

	ret = msc_smc_create_sysfs(pdev, msc_smc);
	if (ret) {
		pr_err("msc_smc: Failed to create sysfs");
		return ret;
	}

	spin_lock_irqsave(&msc->lock, flags);
	for (partid = 0; partid < mpam_get_partid_count(&msc_smc->msc); partid++)
		msc_smc_set_request(msc_smc, partid, 0);

	reg = readl_relaxed(msc_smc->base[0] + CTRL8_0);
	FIELD_SET(reg, MPAMPRI, 0x3);

	for (cnt = 0; cnt < msc_smc->base_count; cnt++) {
		writel_relaxed(reg, msc_smc->base[cnt] + CTRL8_0);
		writel_relaxed(reg, msc_smc->base[cnt] + CTRL8_1);
	}

	spin_unlock_irqrestore(&msc->lock, flags);
	return 0;
}

static int exynos_msc_smc_probe(struct platform_device *pdev)
{
	int ret;
	struct msc_smc_domain *msc_smc;

	msc_smc = devm_kzalloc(&pdev->dev, sizeof(struct msc_smc_domain), GFP_KERNEL);
	if (!msc_smc) {
		pr_err("msc_smc: Failed to alloc msc_smc");
		return -ENOMEM;
	}

	ret = init_msc(msc_smc, pdev);
	if (ret) {
		pr_err("msc_smc: Failed to init msc_smc");
		goto err_ret;
	}

	pr_info("msc_smc: complete to initialize");

	ret = exynos_mpam_register_domain(&msc_smc->msc);
	if (ret) {
		pr_err("msc_smc: Failed to register domain");
		goto err_ret;
	}

	return 0;
err_ret:
	devm_kfree(&pdev->dev, msc_smc);
	return ret;
}

static const struct of_device_id of_msc_smc_match[] = {
	{
		.compatible = "samsung,msc-smc"
	},
	{ /* end */ },
};

static struct platform_driver exynos_msc_smc_driver = {
	.probe = exynos_msc_smc_probe,
	.driver = {
	       .name = "msc-smc",
	       .of_match_table = of_msc_smc_match,
	},
};

static int __init exynos_msc_smc_driver_init(void)
{
	return platform_driver_register(&exynos_msc_smc_driver);
}
module_init(exynos_msc_smc_driver_init);
