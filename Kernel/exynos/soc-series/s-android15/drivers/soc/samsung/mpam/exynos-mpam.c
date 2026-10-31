// SPDX-License-Identifier: GPL-2.0+
/*
 * MPAM framework for exynos
 *
 * Auther : YEONGHWAN SON (yhwan.son@samsung.com)
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "exynos-mpam.h"
#include "exynos-mpam-internal.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeonghwan Son <yhwan.son@samsung.com>");
MODULE_DESCRIPTION("Exynos MPAM Framework");

static struct kobject mpam_ko_root;
static struct msc_domain *domains[NUM_OF_MSC];
static __read_mostly unsigned int mpam_partid_count = UINT_MAX;

static DEFINE_RWLOCK(notifier_lock);
static RAW_NOTIFIER_HEAD(notifier_chain);

static int is_late_init = 0;

static struct kobj_type mpam_kobj_ktype = {
	.sysfs_ops	= &kobj_sysfs_ops,
};

/******************************************************************************/
/*                               Helper functions                             */
/******************************************************************************/
static inline struct msc_domain *next_msc_domain(int *id)
{
	int idx = *id;

	for (++idx; idx < NUM_OF_MSC; idx++) {
		if (!domains[idx])
			return NULL;

		if (domains[idx]->enabled)
			break;
	}

	*id = idx;

	return idx == NUM_OF_MSC ? NULL : domains[idx];
}

static inline void restore_msc(void)
{
	struct msc_domain *msc;
	int id;

	for_each_msc(msc, id)
		msc->restore_attr(msc);
}

static int check_msc_domain(struct msc_domain *msc_dom)
{
	if (!msc_dom) {
		pr_err("exynos-mpam: msc domain is not initialized\n");
		return -EINVAL;
	}

	if (msc_dom->id >= NUM_OF_MSC) {
		pr_err("exynos-mpam: msc domain id is invalid (id:%d)\n", msc_dom->id);
		return -EINVAL;
	}

	if (!msc_dom->restore_attr) {
		pr_err("exynos-mpam: restore_attr func is not initialized (id:%d)\n", msc_dom->id);
		return -EINVAL;
	}

	return 0;
}

/******************************************************************************/
/*                               API                                          */
/******************************************************************************/
struct kobj_type *get_mpam_kobj_ktype(void)
{
	return &mpam_kobj_ktype;
}
EXPORT_SYMBOL_GPL(get_mpam_kobj_ktype);

unsigned int mpam_get_partid_count(struct msc_domain *msc)
{
	if (msc && msc->partid_count)
		return min(mpam_partid_count, msc->partid_count);

	return mpam_partid_count;
}
EXPORT_SYMBOL_GPL(mpam_get_partid_count);

static void mpam_set_el0_partid(unsigned int inst_id, unsigned int data_id)
{
	u64 reg;

	cant_migrate();

	reg = read_sysreg_s(SYS_MPAM0_EL1);

	FIELD_SET(reg, MPAM0_EL1_PARTID_I, inst_id);
	FIELD_SET(reg, MPAM0_EL1_PARTID_D, data_id);

	write_sysreg_s(reg, SYS_MPAM0_EL1);
	/*
	 * Note: if the scope is limited to userspace, we'll get an EL switch
	 * before getting back to US which will be our context synchronization
	 * event, so this won't be necessary.
	 */
	isb();
}

/*
 * Write the PARTID to use on the local CPU.
 */
void mpam_write_partid(unsigned int partid)
{
	WARN_ON_ONCE(preemptible());
	WARN_ON_ONCE(partid >= mpam_partid_count);

	mpam_set_el0_partid(partid, partid);
}
EXPORT_SYMBOL_GPL(mpam_write_partid);

int exynos_mpam_register_domain(struct msc_domain *msc_dom)
{
	int ret;

	ret = check_msc_domain(msc_dom);
	if (ret)
		return ret;

	if (kobject_move(msc_dom->ko_root, &mpam_ko_root)) {
		pr_err("exynos-mpam: Failed to move msc domain kobject (id:%d)\n", msc_dom->id);
		kobject_put(msc_dom->ko_root);
		return -EINVAL;
	}

	msc_dom->enabled = true;
	domains[msc_dom->id] = msc_dom;

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_mpam_register_domain);

/* late_init attribute START */
int mpam_late_init_notifier_register(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&notifier_lock, flags);
	ret = raw_notifier_chain_register(&notifier_chain, nb);
	write_unlock_irqrestore(&notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(mpam_late_init_notifier_register);

static ssize_t current_partid_show(struct kobject *obj, struct kobj_attribute *attr, char *buf)
{
	u64 reg;
	unsigned int partid;

	if (!is_late_init)
		return sprintf(buf, "mpam doesn't opearte (late_init = 0)\n");

	reg = read_sysreg_s(SYS_MPAM0_EL1);
	partid = FIELD_GET(MPAM0_EL1_PARTID_I, reg);

	if (partid >= NUM_MPAM_ENTRIES)
		return sprintf(buf, "invalid partid: current_partid=%u, max_partid=%u\n", partid, NUM_MPAM_ENTRIES - 1);

	return sprintf(buf, "%s\n", mpam_entry_names[partid]);
}

static struct kobj_attribute current_partid_attr = __ATTR(current_partid, 0444, current_partid_show, NULL);

static ssize_t late_init_show(struct kobject *obj, struct kobj_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d\n", is_late_init);
	return ret;
}

static ssize_t late_init_store(struct kobject *obj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	val = !!val;

	if (val == 1) {
		is_late_init = 1;
		read_lock(&notifier_lock);
		raw_notifier_call_chain(&notifier_chain, is_late_init, NULL);
		read_unlock(&notifier_lock);
	}

	return count;
}

static struct kobj_attribute late_init_attr = __ATTR(late_init, 0664, late_init_show, late_init_store);

static struct attribute *mpam_entry_control_attrs[] = {
	&late_init_attr.attr,
	&current_partid_attr.attr,
	NULL
};

static struct attribute_group mpam_entry_control_group = {
	.attrs = mpam_entry_control_attrs,
};

static struct notifier_block exynos_cpupm_fcd_nb;

static int mpam_cpupm_fcd_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;

	if (is_late_init) {
		restore_msc();
	}
	return notifier_from_errno(ret);
}

static int mpam_cpupm_fcd_notifier_register(void)
{
	exynos_cpupm_fcd_nb.notifier_call = mpam_cpupm_fcd_notifier;

	return exynos_cpupm_fcd_notifier_register(&exynos_cpupm_fcd_nb);
}

static int mpam_resume(struct device *dev)
{
	restore_msc();

	return 0;
}


static int exynos_mpam_probe(struct platform_device *pdev)
{
	int ret;

	kobject_init(&mpam_ko_root, &mpam_kobj_ktype);
	ret = kobject_add(&mpam_ko_root, &pdev->dev.kobj, "mpam");

	ret = sysfs_create_group(&mpam_ko_root, &mpam_entry_control_group);
	if (ret)
		return ret;

	ret = mpam_cpupm_fcd_notifier_register();
	if (ret) {
		pr_err("exynos-mpam: Failed to register fcd notifier\n");
		return ret;
	}

	platform_set_drvdata(pdev, domains);

	return 0;
}

static const struct of_device_id of_mpam_match[] = {
	{
		.compatible = "samsung,mpam"
	},
	{ /* end */ },
};

static const struct dev_pm_ops mpam_pm_ops = {
	.resume = mpam_resume
};

static struct platform_driver exynos_mpam_driver = {
	.probe = exynos_mpam_probe,
	.driver = {
	       .name = "mpam",
	       .of_match_table = of_mpam_match,
	       .pm = &mpam_pm_ops
	},
};

struct mpam_validation_masks {
	cpumask_var_t visited_cpus;
	cpumask_var_t supported_cpus;
	spinlock_t lock;
};

static void mpam_validate_cpu(void *info)
{
	struct mpam_validation_masks *masks = (struct mpam_validation_masks *)info;
	unsigned int partid_count;
	bool valid = true;

	if (!FIELD_GET(ID_AA64PFR0_MPAM, read_sysreg_s(SYS_ID_AA64PFR0_EL1))) {
		valid = false;
		goto out;
	}

	if (!FIELD_GET(MPAM1_EL1_MPAMEN, read_sysreg_s(SYS_MPAM1_EL1))) {
		valid = false;
		goto out;
	}

	partid_count = FIELD_GET(MPAMIDR_EL1_PARTID_MAX, read_sysreg_s(SYS_MPAMIDR_EL1)) + 1;

	spin_lock(&masks->lock);
	mpam_partid_count = min(partid_count, mpam_partid_count);
	spin_unlock(&masks->lock);
out:
	cpumask_set_cpu(smp_processor_id(), masks->visited_cpus);
	if (valid)
		cpumask_set_cpu(smp_processor_id(), masks->supported_cpus);
}

/*
 * Does the system support MPAM, and if so is it actually usable?
 */
static int mpam_validate_sys(void)
{
	struct mpam_validation_masks masks;
	int ret = 0;

	if (!zalloc_cpumask_var(&masks.visited_cpus, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&masks.supported_cpus, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out_free_visited;
	}
	spin_lock_init(&masks.lock);

	on_each_cpu_cond_mask(NULL, mpam_validate_cpu, &masks, true, cpu_online_mask);

	if (!cpumask_equal(masks.visited_cpus, masks.supported_cpus)) {
		pr_warn("MPAM only supported on CPUs [%*pbl]\n",
			cpumask_pr_args(masks.supported_cpus));
		ret = -EOPNOTSUPP;
	}

	free_cpumask_var(masks.supported_cpus);
out_free_visited:
	free_cpumask_var(masks.visited_cpus);

	return ret;
}

static int __init exynos_mpam_driver_init(void)
{
	int ret;

	/* Does the system support MPAM at all? */
	ret = mpam_validate_sys();
	if (ret)
		return -EOPNOTSUPP;

	return platform_driver_register(&exynos_mpam_driver);
}
module_init(exynos_mpam_driver_init);
