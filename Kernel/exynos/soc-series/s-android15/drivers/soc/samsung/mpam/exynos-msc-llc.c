// SPDX-License-Identifier: GPL-2.0+
/*
 * LLC MSC(Memory-System Component) for exynos
 *
 * Auther : YEONGHWAN SON (yhwan.son@samsung.com)
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "exynos-msc-llc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeonghwan Son <yhwan.son@samsung.com>");
MODULE_DESCRIPTION("Exynos LLC MSC");

static ssize_t msc_llc_request_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int count = 0, part;
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);

	count += snprintf(buf + count, PAGE_SIZE, "==partid index==\n");
	for (part = 0; part < NUM_MPAM_ENTRIES; part++) {
		count += snprintf(buf + count, PAGE_SIZE, "%s : %d\n", mpam_entry_names[part], part);
	}
	count += snprintf(buf + count, PAGE_SIZE, "==current setting==\n");
	count += snprintf(buf + count, PAGE_SIZE, "partid = %d, alloc_size = %d, enabled = %d\n", lrk->partid, lrk->alloc_size, lrk->enabled);

	return count;
}

static ssize_t msc_llc_request_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);
	unsigned int alloc_size, part;

	if (sscanf(buf, "%d %d", &part, &alloc_size) != 2)
		return -EINVAL;

	if (part >= NUM_MPAM_ENTRIES)
		return -EINVAL;

	if (size > lrk->msc_llc->cpbm_nbits)
		return -EINVAL;

	lrk->partid = part;
	lrk->alloc_size = alloc_size;
	lrk->init = true;

	return size;
}

static ssize_t msc_llc_submit_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", lrk->enabled);
	return ret;
}

static ssize_t msc_llc_submit_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t size)
{
	int submit;
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);

	if (sscanf(buf, "%d", &submit) != 1)
		return -EINVAL;
	if (!lrk->init)
		return -EINVAL;

	if (submit && lrk->enabled)
		llc_mpam_alloc(lrk->prio + 2, lrk->alloc_size, lrk->partid, 0, 0, 0);

	llc_mpam_alloc(lrk->prio + 2, lrk->alloc_size, lrk->partid, 1, 1, submit);

	lrk->enabled = submit ? 1 : 0;

	return size;
}

static struct kobj_attribute msc_llc_request_attr =
	__ATTR(request, 0644, msc_llc_request_show, msc_llc_request_store);
static struct kobj_attribute msc_llc_submit_attr =
	__ATTR(submit, 0644, msc_llc_submit_show, msc_llc_submit_store);

static struct attribute *msc_llc_ctrl_attrs[] = {
	&msc_llc_request_attr.attr,
	&msc_llc_submit_attr.attr,
	NULL,
};
static struct attribute_group msc_llc_ctrl_attr_group = {
	.attrs = msc_llc_ctrl_attrs,
};

static void msc_llc_restore_attr(struct msc_domain *msc)
{
	/* do nothing */
}

static int msc_llc_create_sysfs(struct platform_device *pdev, struct msc_llc_domain *msc_llc)
{
	int ret;
	int idx;

	kobject_init(&msc_llc->ko_root, get_mpam_kobj_ktype());
	ret = kobject_add(&msc_llc->ko_root, &pdev->dev.kobj, "msc_llc");
	if (ret)
		goto err_root_dir;

	msc_llc->ko_llc_request = devm_kzalloc(&pdev->dev,
				sizeof(*msc_llc->ko_llc_request) * msc_llc->llc_request_count, GFP_KERNEL);
	if (!msc_llc->ko_llc_request) {
		goto err_llc_request_alloc;
	}

	for (idx = 0; idx < msc_llc->llc_request_count; idx++) {
		kobject_init(&msc_llc->ko_llc_request[idx].kobj, get_mpam_kobj_ktype());
		msc_llc->ko_llc_request[idx].msc_llc = msc_llc;
		msc_llc->ko_llc_request[idx].prio = idx;
		ret = kobject_add(&msc_llc->ko_llc_request[idx].kobj, &msc_llc->ko_root, "prio%d", idx);
		if (ret)
			goto err_llc_request_dir;
	}

	for (idx = 0; idx < msc_llc->llc_request_count; idx++) {
		ret = sysfs_create_group(&msc_llc->ko_llc_request[idx].kobj, &msc_llc_ctrl_attr_group);
		if (ret)
			goto err_info_grp;
	}

	return 0;

err_info_grp:
err_llc_request_dir:
	devm_kfree(&pdev->dev, msc_llc->ko_llc_request);
err_llc_request_alloc:
err_root_dir:
	kobject_put(&msc_llc->ko_root);
	return ret;
}

static int init_msc(struct msc_llc_domain *msc_llc, struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *dn = pdev->dev.of_node;
	void __iomem *base;
	struct msc_domain *msc = &msc_llc->msc;

	if (!dn) {
		pr_err("msc_dsu: Failed to get device tree");
		return -EINVAL;
	}

	msc->restore_attr = msc_llc_restore_attr;
	msc->pdev = pdev;

	ret |= of_property_read_s32(dn, "msc-type", &msc->id);
	ret |= of_property_read_u32(dn, "base", &msc->base_addr);
	ret |= of_property_read_u32(dn, "size", &msc->size);
	ret |= of_property_read_u32(dn, "cpbm-nbits", &msc_llc->cpbm_nbits);
	ret |= of_property_read_u32(dn, "llc-request-count", &msc_llc->llc_request_count);

	if (ret) {
		pr_err("msc_llc: Failed to parse device tree");
		return ret;
	}

	base = ioremap(msc->base_addr, msc->size);
	if (IS_ERR(base)) {
		return PTR_ERR(base);
	}

	msc->base = base;
	spin_lock_init(&msc->lock);

	ret = msc_llc_create_sysfs(pdev, msc_llc);
	if (ret) {
		pr_err("msc_llc: Failed to create sysfs");
		return ret;
	}

	msc_llc->msc.ko_root = &msc_llc->ko_root;

	return 0;
}

static int exynos_msc_llc_probe(struct platform_device *pdev)
{
	int ret;
	struct msc_llc_domain *msc_llc;

	msc_llc = devm_kzalloc(&pdev->dev, sizeof(struct msc_llc_domain), GFP_KERNEL);
	if (!msc_llc) {
		pr_err("msc_llc: Failed to alloc msc_llc");
		return -ENOMEM;
	}

	ret = init_msc(msc_llc, pdev);
	if (ret) {
		pr_err("msc_llc: Failed to init msc_llc");
		goto err_ret;
	}

	pr_info("msc_llc: complete to initialize");

	ret = exynos_mpam_register_domain(&msc_llc->msc);
	if (ret) {
		pr_err("msc_llc: Failed to register domain");
		goto err_ret;
	}

	return 0;
err_ret:
	devm_kfree(&pdev->dev, msc_llc);
	return ret;
}

static const struct of_device_id of_msc_llc_match[] = {
	{
		.compatible = "samsung,msc-llc"
	},
	{ /* end */ },
};

static struct platform_driver exynos_msc_llc_driver = {
	.probe = exynos_msc_llc_probe,
	.driver = {
	       .name = "msc-llc",
	       .of_match_table = of_msc_llc_match,
	},
};

static int __init exynos_msc_llc_driver_init(void)
{
	return platform_driver_register(&exynos_msc_llc_driver);
}
module_init(exynos_msc_llc_driver_init);
