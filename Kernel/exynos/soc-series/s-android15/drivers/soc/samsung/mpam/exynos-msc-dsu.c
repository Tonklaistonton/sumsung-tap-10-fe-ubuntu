// SPDX-License-Identifier: GPL-2.0+
/*
 * DSU MSC(Memory-System Component) for exynos
 *
 * Auther : YEONGHWAN SON (yhwan.son@samsung.com)
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "exynos-msc-dsu.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeonghwan Son <yhwan.son@samsung.com>");
MODULE_DESCRIPTION("Exynos DSU MSC");

static unsigned int msc_dsu_get_partid_max(struct msc_domain *msc)
{
	lockdep_assert_held(&msc->lock);

	return FIELD_GET(MPAMF_IDR_PARTID_MAX, readq_relaxed(msc->base + MPAMF_IDR));
}

static void msc_dsu_sel_partid(struct msc_dsu_domain *msc_dsu, unsigned int id)
{
	u32 reg;

	lockdep_assert_held(&msc_dsu->msc.lock);

	reg = readl_relaxed(msc_dsu->msc.base + MPAMCFG_PART_SEL);

	FIELD_SET(reg, MPAMCFG_PART_SEL_PARTID_SEL, id);
	if (msc_dsu->has_ris)
		FIELD_SET(reg, MPAMCFG_PART_SEL_RIS, 0);

	writel_relaxed(reg, msc_dsu->msc.base + MPAMCFG_PART_SEL);
}

static void msc_dsu_set_cpbm(struct msc_dsu_domain *msc_dsu,
			      unsigned int id,
			      const unsigned long *bitmap)
{
	void __iomem *addr = msc_dsu->msc.base + MPAMCFG_CPBM_n;
	unsigned int bit = 0, n = 0;
	u32 acc = 0;

	lockdep_assert_held(&msc_dsu->msc.lock);

	if (bitmap != msc_dsu->ko_parts[id].cpbm)
		bitmap_copy(msc_dsu->ko_parts[id].cpbm, bitmap, msc_dsu->cpbm_nbits);

	msc_dsu_sel_partid(msc_dsu, id);

	/* Single write every reg boundary */
	while (n++ < BITS_TO_U32(msc_dsu->cpbm_nbits)) {
		for_each_set_bit(bit, bitmap, min_t(unsigned int,
						    (n * BITS_PER_TYPE(u32)),
						     msc_dsu->cpbm_nbits))
			acc |= 1 << bit % BITS_PER_TYPE(u32);

		writel_relaxed(acc, addr);
		addr += sizeof(acc);
		bit = n*BITS_PER_TYPE(u32);
		acc = 0;
	}
}

static void msc_dsu_get_cpbm(struct msc_dsu_domain *msc_dsu,
			      unsigned int id,
			      unsigned long *bitmap)
{
	void __iomem *addr = msc_dsu->msc.base + MPAMCFG_CPBM_n;
	size_t regsize = BITS_PER_TYPE(u32);
	unsigned int bit;
	int n;

	lockdep_assert_held(&msc_dsu->msc.lock);

	msc_dsu_sel_partid(msc_dsu, id);

	for (n = 0; (n * regsize) < msc_dsu->cpbm_nbits; n++) {
		unsigned long tmp = readl_relaxed(addr);

		for_each_set_bit(bit, &tmp, min(regsize, msc_dsu->cpbm_nbits - (n * regsize)))
			bitmap_set(bitmap, bit + (n * regsize), 1);

		addr += regsize;
	}
}

static void msc_dsu_set_cmax(struct msc_dsu_domain *msc_dsu, unsigned int id, u16 val)
{
	lockdep_assert_held(&msc_dsu->msc.lock);

	msc_dsu_sel_partid(msc_dsu, id);
	writel_relaxed(FIELD_PREP(MPAMCFG_CMAX_CMAX, val >> msc_dsu->cmax_shift),
		       msc_dsu->msc.base + MPAMCFG_CMAX);
}

static u16 msc_dsu_get_cmax(struct msc_dsu_domain *msc_dsu, unsigned int id)
{
	u32 reg;
	u16 res;

	lockdep_assert_held(&msc_dsu->msc.lock);

	msc_dsu_sel_partid(msc_dsu, id);

	reg = readl_relaxed(msc_dsu->msc.base + MPAMCFG_CMAX);
	res = FIELD_GET(MPAMCFG_CMAX_CMAX, reg);
	return res << msc_dsu->cmax_shift;
}

static void msc_dsu_cmax_shift_set(struct msc_dsu_domain *msc_dsu)
{
	u16 val;
	/*
	 * Note: The TRM says the implemented bits are the most significant ones,
	 * but the model doesn't seem to agree with it...
	 * Handle that in the background, dropping a warning case needed
	 */
	lockdep_assert_held(&msc_dsu->msc.lock);

	if (!(msc_dsu->cmax_nbits < 16))
		return;
	/*
	 * Unimplemented bits within the field are RAZ/WI
	 * At this point the MPAM_CMAX.CMAX will not be adjusted with the shift
	 * so this operates on an unmodified reg content.
	 * Also, the default value for CMAX will be set further down the init
	 * so there is no need for reset here.
	 */
	msc_dsu_set_cmax(msc_dsu, MPAM_PARTID_DEFAULT, GENMASK(15, 0));
	val = msc_dsu_get_cmax(msc_dsu, MPAM_PARTID_DEFAULT);

	if (val & GENMASK(15 - msc_dsu->cmax_nbits, 0)) {
		msc_dsu->cmax_shift = 16 - msc_dsu->cmax_nbits;
		pr_warn("msc_dsu: MPAM_CMAX: implemented bits are the least-significant ones!");
	}
}

static ssize_t msc_dsu_cpbm_nbits_show(struct kobject *kobj, struct kobj_attribute *attr,
				       char *buf)
{
	struct msc_dsu_domain *msc_dsu = container_of(kobj, struct msc_dsu_domain, ko_root);

	return sprintf(buf, "%u\n", msc_dsu->cpbm_nbits);
}

static ssize_t msc_dsu_cmax_nbits_show(struct kobject *kobj, struct kobj_attribute *attr,
				       char *buf)
{
	struct msc_dsu_domain *msc_dsu = container_of(kobj, struct msc_dsu_domain, ko_root);

	return sprintf(buf, "%u\n", msc_dsu->cmax_nbits);
}

static ssize_t msc_dsu_restore_partid_count_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct msc_dsu_domain *msc_dsu = container_of(kobj, struct msc_dsu_domain, ko_root);
	int ret;

	ret = sprintf(buf, "%u\n", msc_dsu->restore_partid_count);
	return ret;
}

static ssize_t msc_dsu_restore_partid_count_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	struct msc_dsu_domain *msc_dsu = container_of(kobj, struct msc_dsu_domain, ko_root);
	int val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	if (val < 0 || val > msc_dsu->msc.partid_count)
		return -EINVAL;

	msc_dsu->restore_partid_count = val;

	return count;
}

static struct kobj_attribute msc_dsu_cpbm_nbits_attr =
	__ATTR(cpbm_nbits, 0444, msc_dsu_cpbm_nbits_show, NULL);
static struct kobj_attribute msc_dsu_cmax_nbits_attr =
	__ATTR(cmax_nbits, 0444, msc_dsu_cmax_nbits_show, NULL);
static struct kobj_attribute msc_dsu_restore_partid_count_attr =
	__ATTR(restore_partid_count, 0644, msc_dsu_restore_partid_count_show, msc_dsu_restore_partid_count_store);

static struct attribute *msc_dsu_info_attrs[] = {
	&msc_dsu_cpbm_nbits_attr.attr,
	&msc_dsu_cmax_nbits_attr.attr,
	&msc_dsu_restore_partid_count_attr.attr,
	NULL,
};

static umode_t msc_dsu_info_attr_visible(struct kobject *kobj,
					  struct attribute *attr,
					  int n)
{
	struct msc_dsu_domain *msc_dsu = container_of(kobj, struct msc_dsu_domain, ko_root);

	if (attr == &msc_dsu_cpbm_nbits_attr.attr &&
	    msc_dsu->has_cpor)
		goto visible;

	if (attr == &msc_dsu_cmax_nbits_attr.attr &&
	    msc_dsu->has_ccap)
		goto visible;

	if (attr == &msc_dsu_restore_partid_count_attr.attr &&
		msc_dsu->restore_partid_count)
		goto visible;

	return 0;

visible:
	return attr->mode;
}

static struct attribute_group msc_dsu_info_attr_group = {
	.attrs = msc_dsu_info_attrs,
	.is_visible = msc_dsu_info_attr_visible,
};

static ssize_t msc_dsu_cpbm_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long *bitmap;
	unsigned long flags;
	size_t size;

	bitmap = bitmap_zalloc(mpk->msc_dsu->cpbm_nbits, GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;

	spin_lock_irqsave(&mpk->msc_dsu->msc.lock, flags);
	msc_dsu_get_cpbm(mpk->msc_dsu, mpk->partid, bitmap);
	spin_unlock_irqrestore(&mpk->msc_dsu->msc.lock, flags);

	size = bitmap_print_to_pagebuf(true, buf, bitmap, mpk->msc_dsu->cpbm_nbits);

	bitmap_free(bitmap);
	return size;
}

static ssize_t msc_dsu_cpbm_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t size)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long *bitmap;
	unsigned long flags;
	int ret;

	bitmap = bitmap_zalloc(mpk->msc_dsu->cpbm_nbits, GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;
	ret = bitmap_parselist(buf, bitmap, mpk->msc_dsu->cpbm_nbits);
	if (ret)
		goto out_free;

	spin_lock_irqsave(&mpk->msc_dsu->msc.lock, flags);
	msc_dsu_set_cpbm(mpk->msc_dsu, mpk->partid, bitmap);
	spin_unlock_irqrestore(&mpk->msc_dsu->msc.lock, flags);
out_free:
	bitmap_free(bitmap);
	return ret ?: size;
}

static ssize_t msc_dsu_cmax_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long flags;
	u16 val;

	spin_lock_irqsave(&mpk->msc_dsu->msc.lock, flags);
	val = msc_dsu_get_cmax(mpk->msc_dsu, mpk->partid);
	spin_unlock_irqrestore(&mpk->msc_dsu->msc.lock, flags);

	return sprintf(buf, "0x%04x\n", val);
}

static ssize_t msc_dsu_cmax_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t size)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long flags;
	u16 val;
	int ret;

	ret = kstrtou16(buf, 0, &val);
	if (ret)
		return ret;

	spin_lock_irqsave(&mpk->msc_dsu->msc.lock, flags);
	msc_dsu_set_cmax(mpk->msc_dsu, mpk->partid, val);
	spin_unlock_irqrestore(&mpk->msc_dsu->msc.lock, flags);

	return size;
}

static struct kobj_attribute msc_dsu_cpbm_attr =
	__ATTR(cpbm, 0644, msc_dsu_cpbm_show, msc_dsu_cpbm_store);

static struct kobj_attribute msc_dsu_cmax_attr =
	__ATTR(cmax, 0644, msc_dsu_cmax_show, msc_dsu_cmax_store);

static struct attribute *msc_dsu_ctrl_attrs[] = {
	&msc_dsu_cpbm_attr.attr,
	&msc_dsu_cmax_attr.attr,
	NULL,
};

static umode_t msc_dsu_ctrl_attr_visible(struct kobject *kobj,
				     struct attribute *attr,
				     int n)
{
	struct msc_part_kobj *mpk;

	mpk = container_of(kobj, struct msc_part_kobj, kobj);

	if (attr == &msc_dsu_cpbm_attr.attr &&
	    mpk->msc_dsu->has_cpor)
		goto visible;

	if (attr == &msc_dsu_cmax_attr.attr &&
	    mpk->msc_dsu->has_ccap)
		goto visible;

	return 0;

visible:
	return attr->mode;
}

static struct attribute_group msc_dsu_ctrl_attr_group = {
	.attrs = msc_dsu_ctrl_attrs,
	.is_visible = msc_dsu_ctrl_attr_visible,
};

static int msc_dsu_create_entry_sysfs(struct msc_dsu_domain *msc_dsu)
{
	int ret = 0;
	int part;

	kobject_init(&msc_dsu->ko_entry_dir, get_mpam_kobj_ktype());
	ret = kobject_add(&msc_dsu->ko_entry_dir, &msc_dsu->ko_root, "entries");
	if (ret)
		goto err_entry_dir;

	for (part = 0; part < NUM_MPAM_ENTRIES; part++) {
		ret = sysfs_create_link(&msc_dsu->ko_entry_dir, &msc_dsu->ko_parts[part].kobj, mpam_entry_names[part]);
		if (ret) {
			pr_err("Failed to link entry to part");
			return ret;
		}
	}

	return ret;

err_entry_dir:
	kobject_put(&msc_dsu->ko_entry_dir);
	return ret;
}

static int msc_dsu_create_sysfs(struct platform_device *pdev, struct msc_dsu_domain *msc_dsu)
{
	u32 partid_count = mpam_get_partid_count(&msc_dsu->msc);
	u32 part, tmp;
	int ret;

	kobject_init(&msc_dsu->ko_root, get_mpam_kobj_ktype());
	ret = kobject_add(&msc_dsu->ko_root, &pdev->dev.kobj, "msc_dsu");
	if (ret)
		goto err_root_dir;

	kobject_init(&msc_dsu->ko_part_dir, get_mpam_kobj_ktype());
	ret = kobject_add(&msc_dsu->ko_part_dir, &msc_dsu->ko_root, "partitions");
	if (ret)
		goto err_part_dir;

	msc_dsu->ko_parts = devm_kzalloc(&msc_dsu->msc.pdev->dev,
				     sizeof(*msc_dsu->ko_parts) * partid_count,
				     GFP_KERNEL);
	if (!msc_dsu->ko_parts) {
		ret = -ENOMEM;
		goto err_part_dir;
	}

	ret = sysfs_create_group(&msc_dsu->ko_root, &msc_dsu_info_attr_group);
	if (ret)
		goto err_info_grp;

	for (part = 0; part < partid_count; part++) {
		kobject_init(&msc_dsu->ko_parts[part].kobj, get_mpam_kobj_ktype());
		msc_dsu->ko_parts[part].msc_dsu = msc_dsu;
		msc_dsu->ko_parts[part].partid = part;
		msc_dsu->ko_parts[part].cpbm = bitmap_zalloc(msc_dsu->cpbm_nbits, GFP_KERNEL);
		if (!msc_dsu->ko_parts[part].cpbm) {
			ret = -ENOMEM;
			goto err_parts_add;
		}

		ret = kobject_add(&msc_dsu->ko_parts[part].kobj, &msc_dsu->ko_part_dir, "%d", part);
		if (ret)
			goto err_parts_add;
	}

	for (part = 0; part < partid_count; part++) {
		ret = sysfs_create_group(&msc_dsu->ko_parts[part].kobj, &msc_dsu_ctrl_attr_group);
		if (ret)
			goto err_parts_grp;
	}

	ret = msc_dsu_create_entry_sysfs(msc_dsu);
	if (ret)
		goto err_create_entry;

	msc_dsu->msc.ko_root = &msc_dsu->ko_root;

	return 0;

err_create_entry:
err_parts_grp:
	for (tmp = 0; tmp < part; tmp++)
		sysfs_remove_group(&msc_dsu->ko_parts[part].kobj, &msc_dsu_ctrl_attr_group);
	part = partid_count;

err_parts_add:
	for (tmp = 0; tmp < part; tmp++)
		kobject_put(&msc_dsu->ko_parts[tmp].kobj);

	sysfs_remove_group(&msc_dsu->ko_root, &msc_dsu_info_attr_group);

err_info_grp:
	devm_kfree(&msc_dsu->msc.pdev->dev, msc_dsu->ko_parts);
err_part_dir:
	kobject_put(&msc_dsu->ko_part_dir);
err_root_dir:
	kobject_put(&msc_dsu->ko_root);
	return ret;
}
static void msc_dsu_restore_attr(struct msc_domain *msc)
{
	int partid;
	unsigned long flags;
	struct msc_dsu_domain *msc_dsu = container_of(msc, struct msc_dsu_domain, msc);

	spin_lock_irqsave(&msc->lock, flags);
	for (partid = 0; partid < msc_dsu->restore_partid_count; partid++)
		msc_dsu_set_cpbm(msc_dsu, partid, msc_dsu->ko_parts[partid].cpbm);

	spin_unlock_irqrestore(&msc->lock, flags);
}

static int init_msc(struct msc_dsu_domain *msc_dsu, struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *dn = pdev->dev.of_node;
	void __iomem *base;
	static unsigned long *bitmap;
	int partid;
	u64 reg;
	unsigned long flags;

	if (!dn) {
		pr_err("msc_dsu: Failed to get device tree");
		return -EINVAL;
	}

	msc_dsu->msc.restore_attr = msc_dsu_restore_attr;

	msc_dsu->msc.pdev = pdev;

	ret |= of_property_read_s32(dn, "msc-type", &msc_dsu->msc.id);
	ret |= of_property_read_u32(dn, "base", &msc_dsu->msc.base_addr);
	ret |= of_property_read_u32(dn, "size", &msc_dsu->msc.size);

	if (ret) {
		pr_err("msc_dsu: Failed to parse device tree");
		return ret;
	}

	base = ioremap(msc_dsu->msc.base_addr, msc_dsu->msc.size);
	if (IS_ERR(base)) {
		return PTR_ERR(base);
	}

	msc_dsu->msc.base = base;
	msc_dsu->restore_partid_count = sizeof(mpam_entry_names) / sizeof(char*);
	spin_lock_init(&msc_dsu->msc.lock);

	/*
	 * We're using helpers that expect the lock to be held, but we're
	 * setting things up and there is no interface yet, so nothing can
	 * race with us. Make lockdep happy, and save ourselves from a couple
	 * of lock/unlock.
	 */
	spin_acquire(&msc_dsu->msc.lock.dep_map, 0, 0, _THIS_IP_);

	reg = readq_relaxed(base + MPAMF_IDR);
	msc_dsu->has_cpor = FIELD_GET(MPAMF_IDR_HAS_CPOR_PART, reg);
	msc_dsu->has_ccap = FIELD_GET(MPAMF_IDR_HAS_CCAP_PART, reg);
	/* Detect more features here */

	if (!msc_dsu->part_feats) {
		pr_err("MSC does not support any recognized partitionning feature\n");
		return -EOPNOTSUPP;
	}

	/* Check for features that aren't supported, disable those we can */
	if (FIELD_GET(MPAMF_IDR_HAS_PRI_PART, reg)) {
		pr_err("Priority partitionning present but not supported\n");
		return -EOPNOTSUPP;
	}

	msc_dsu->has_ris = FIELD_GET(MPAMF_IDR_HAS_RIS, reg);
	if (msc_dsu->has_ris)
		pr_warn("msc_dsu: RIS present but not supported, only instance 0 will be used\n");

	/* Error interrupts aren't handled */
	reg = readl_relaxed(base + MPAMF_ECR);
	FIELD_SET(reg, MPAMF_ECR_INTEN, 0);
	writel_relaxed(reg, base + MPAMF_ECR);

	msc_dsu->msc.partid_count = msc_dsu_get_partid_max(&msc_dsu->msc) + 1;
	pr_info("%d partitions supported\n", msc_dsu->msc.partid_count);
	if (msc_dsu->msc.partid_count > mpam_get_partid_count(NULL))
		pr_info("System limited to %d partitions\n", mpam_get_partid_count(NULL));

	reg = readl_relaxed(base + MPAMF_CPOR_IDR);
	msc_dsu->cpbm_nbits = FIELD_GET(MPAMF_CPOR_IDR_CPBM_WD, reg);
	pr_info("%d portions supported\n", msc_dsu->cpbm_nbits);

	reg = readl_relaxed(base + MPAMF_CCAP_IDR);
	msc_dsu->cmax_nbits = FIELD_GET(MPAMF_CCAP_IDR_CMAX_WD, reg);
	msc_dsu_cmax_shift_set(msc_dsu);

	bitmap = bitmap_alloc(mpam_get_partid_count(NULL), GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;

	spin_release(&msc_dsu->msc.lock.dep_map, _THIS_IP_);

	ret = msc_dsu_create_sysfs(pdev, msc_dsu);
	if (ret)
		return ret;

	/*
	 * Make all partitions have a sane default setting. The reference manual
	 * "suggests" sane defaults, be paranoid.
	 */
	bitmap_fill(bitmap, mpam_get_partid_count(NULL));

	spin_lock_irqsave(&msc_dsu->msc.lock, flags);
	for (partid = 0; partid < mpam_get_partid_count(NULL); partid++) {
		msc_dsu_set_cpbm(msc_dsu, partid, bitmap);
		msc_dsu_set_cmax(msc_dsu, partid,
				  GENMASK(15, 15 - (msc_dsu->cmax_nbits -1)));
	}
	spin_unlock_irqrestore(&msc_dsu->msc.lock, flags);
	bitmap_free(bitmap);

	return 0;
}

static int exynos_msc_dsu_probe(struct platform_device *pdev)
{
	int ret;
	struct msc_dsu_domain *msc_dsu;

	msc_dsu = devm_kzalloc(&pdev->dev, sizeof(struct msc_dsu_domain), GFP_KERNEL);
	if (!msc_dsu) {
		pr_err("msc_dsu: Failed to alloc msc_dsu");
		return -ENOMEM;
	}

	ret = init_msc(msc_dsu, pdev);
	if (ret) {
		pr_err("msc_dsu: Failed to init msc_dsu");
		goto err_ret;
	}

	pr_info("msc_dsu: complete to initialize");

	ret = exynos_mpam_register_domain(&msc_dsu->msc);
	if (ret) {
		pr_err("msc_dsu: Failed to register domain");
		goto err_ret;
	}

	return 0;
err_ret:
	devm_kfree(&pdev->dev, msc_dsu);
	return ret;
}

static const struct of_device_id of_msc_dsu_match[] = {
	{
		.compatible = "samsung,msc-dsu"
	},
	{ /* end */ },
};

static struct platform_driver exynos_msc_dsu_driver = {
	.probe = exynos_msc_dsu_probe,
	.driver = {
	       .name = "msc-dsu",
	       .of_match_table = of_msc_dsu_match,
	},
};

static int __init exynos_msc_dsu_driver_init(void)
{
	return platform_driver_register(&exynos_msc_dsu_driver);
}
module_init(exynos_msc_dsu_driver_init);
