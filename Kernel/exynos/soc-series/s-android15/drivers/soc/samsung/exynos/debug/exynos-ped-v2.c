/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Parity Error detector(PED) Driver for Samsung Exynos SOC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/suspend.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif
#include "exynos-ped-v2.h"

static struct ped_dev *g_ped;

static bool ped_pd_is_power_up(struct lh_nodegroup *group)
{
	if (!group)
		return false;

	if (!strcmp(group->pd_name, "no_pd"))
		return true;

#if defined(CONFIG_EXYNOS_PD) || defined(CONFIG_EXYNOS_PD_MODULE)
	return (exynos_pd_status(group->pd_domain) > 0)? true:false;
#else
	return false;
#endif
}

struct lh_nodegroup *find_group_by_name(struct ped_dev *ped, const char* name)
{
	struct ped_platdata *pdata;
	struct lh_nodegroup *group = NULL;
	int i;

	if (!ped)
		return NULL;
	pdata = ped->pdata;
	dev_dbg(ped->dev, "%s: Looking for %s..\n", __func__, name);

	for (i = 0; i < pdata->num_nodegroup; i++) {
		if(!strncmp(name, pdata->lh_group[i].name,
				sizeof(pdata->lh_group[i].name))) {
			dev_dbg(ped->dev, "%s: %s(0x%08llx) found.\n", __func__,
				name, pdata->lh_group[i].phy_regs);
			group = &pdata->lh_group[i];
			break;
		}
	}

	return group;
}

static int ped_init_by_group(struct lh_nodegroup *group, bool was_sicd)
{
	u32 val;

	if (!group) {
		dev_err(g_ped->dev, "Group Empty!\n");
		return -EINVAL;
	}
	if (group->num_group0 == 0)
		goto group1;
	if (group->num_group0 == MAX_PORT_ID)
		val = UINT_MAX;
	else
		val = (1 << group->num_group0) - 1;
	val ^= group->group0_mask;
	dev_dbg(g_ped->dev, "%s: sicd status:%d block %s grp0 val=%x\n", __func__, was_sicd, &group->name[0], val);
	if (ped_pd_is_power_up(group) > 0)
		writel(val, group->regs + LH_INTC0_IEN_SET);

group1:
	if (group->num_group1 == 0)
		return 0;
	if (group->num_group1 == MAX_PORT_ID)
		val = UINT_MAX;
	else
		val = (1 << group->num_group1) - 1;
	val ^= group->group1_mask;
	dev_dbg(g_ped->dev, "%s: sicd status:%d block %s grp1 val=%x\n", __func__, was_sicd, &group->name[0], val);
	if (ped_pd_is_power_up(group) > 0)
		writel(val, group->regs + LH_INTC1_IEN_SET);
	return 0;
}

static int ped_clear_by_group(struct lh_nodegroup *group, bool is_sicd)
{
	u32 val;

	if (!group) {
		dev_err(g_ped->dev, "Group Empty!\n");
		return -EINVAL;
	}
	if (group->num_group0 == 0)
		goto group1;
	if (group->num_group0 == MAX_PORT_ID)
		val = UINT_MAX;
	else
		val = (1 << group->num_group0) - 1;
	val ^= (is_sicd)?group->group0_sicd_mask:group->group0_mask;
	dev_dbg(g_ped->dev, "%s: sicd status:%d block %s grp0 val=%x\n", __func__, is_sicd, &group->name[0], val);
	if (ped_pd_is_power_up(group))
		writel(val, group->regs + LH_INTC0_IEN_CLR);

group1:
	if (group->num_group1 == 0)
		return 0;
	if (group->num_group1 == MAX_PORT_ID)
		val = UINT_MAX;
	else
		val = (1 << group->num_group1) - 1;
	val ^= (is_sicd)?group->group1_sicd_mask:group->group1_mask;
	dev_dbg(g_ped->dev, "%s: sicd status:%d block %s grp1 val=%x\n", __func__, is_sicd, &group->name[0], val);
	if (ped_pd_is_power_up(group))
		writel(val, group->regs + LH_INTC1_IEN_CLR);
	return 0;
}

static int ped_int_clear(struct lh_nodegroup *group, bool en)
{
	void __iomem *reg;

	if (!group)
		return -EINVAL;
	reg = group->regs;
	barrier();
	if (ped_pd_is_power_up(group))
		writel(en?1:0, reg + LH_INT_CLR);

	return 0;
}

static int ped_init_all(struct ped_dev *ped, bool was_sicd)
{
	struct ped_platdata *pdata;
	struct lh_nodegroup *group;
	int i;

	if (!ped)
		return -ENODEV;
	pdata = ped->pdata;
	for (i = 0; i < pdata->num_nodegroup; i++) {
		group = &pdata->lh_group[i];
		if (group->pd_support)
			continue;
		if (was_sicd && !(group->group0_sicd_mask | group->group1_sicd_mask))
			continue;

		ped_int_clear(group, true);
		ped_init_by_group(group, was_sicd);
		ped_int_clear(group, false);
	}
	return 0;
}

static int ped_clear_all(struct ped_dev *ped, bool is_sicd)
{
	struct ped_platdata *pdata;
	struct lh_nodegroup *group;
	int i;

	if (!ped)
		return -ENODEV;
	pdata = ped->pdata;
	for (i = 0; i < pdata->num_nodegroup; i++) {
		group = &pdata->lh_group[i];
		if (!is_sicd && group->pd_support && !ped_pd_is_power_up(group)) {
			dev_dbg(g_ped->dev, "%s: Group %s power down\n",
						__func__, &group->name[0]);
			continue;
		}
		if (is_sicd && !(group->group0_sicd_mask | group->group1_sicd_mask))
			continue;

		dev_dbg(g_ped->dev, "%s: Group %s INT clear\n",
					__func__, &group->name[0]);
		if (!is_sicd)
			ped_int_clear(group, true);
		ped_clear_by_group(group, is_sicd);
		if (!is_sicd)
			ped_int_clear(group, false);
	}
	return 0;
}

static void ped_dump_parity_fault(struct lh_nodegroup *group)
{
	if (group->num_group0 <= 0)
		goto dump_group1;
	if (ped_pd_is_power_up(group))
		dev_info(g_ped->dev, "Block:%s Group#0:0x%08x\n",
				&group->name[0],
				readl(group->regs + LH_INTC0_IPEND));
	else {
		dev_info(g_ped->dev, "Block:%s Group#0:None(power down)\n", &group->name[0]);
		return;
	}
dump_group1:
	if (group->num_group1 <= 0)
		return;
	if (ped_pd_is_power_up(group))
		dev_info(g_ped->dev, "Block:%s Group#1:0x%08x\n",
				&group->name[0],
				readl(group->regs + LH_INTC1_IPEND));
	else
		dev_info(g_ped->dev, "Block:%s Group#1:None(power down)\n", &group->name[0]);
}

static void ped_dump_parity_fault_summary(void)
{
	struct lh_nodegroup *group;
	u64 result;
	int i;

	group = find_group_by_name(g_ped, "PARITY_INT_COMB");
	if (!group) {
		dev_err(g_ped->dev, "PARITY_INT_COMB not found\n");
		return;
	}
	result = readl(group->regs + LH_INTC1_IPEND);
	result = result << 32;
	result |= readl(group->regs + LH_INTC0_IPEND);

	dev_info(g_ped->dev, "==================================\n");
	dev_info(g_ped->dev, "PARITY_INT_COMB : %016llx\n", result);

	for (i = 0; i < ARRAY_SIZE(lh_aggrigator_list); i++) {
		if (result & ((u64)1 << i)) {
			dev_info(g_ped->dev, "Block %s: parity error found!!\n", lh_aggrigator_list[i]);
			group = find_group_by_name(g_ped, lh_aggrigator_list[i]);
			if (group)
				ped_dump_parity_fault(group);
		}
	}
	dev_info(g_ped->dev, "==================================\n");
}

static irqreturn_t ped_irq_handler(int irq, void *data)
{
	unsigned long flags;

	dev_err(g_ped->dev, "Parity Error Detected!\n");

	spin_lock_irqsave(&g_ped->ctrl_lock, flags);

	ped_dump_parity_fault_summary();

	spin_unlock_irqrestore(&g_ped->ctrl_lock, flags);

	dbg_snapshot_do_dpm_policy(GO_S2D_ID);

	return IRQ_HANDLED;
}

static void ped_fault_injection(struct lh_nodegroup *group)
{
	void __iomem *reg;

	if (!group) {
		dev_err(g_ped->dev, "Group Empty!\n");
		return;
	}
	reg  = group->regs;
	writel(1, reg + LH_FAULT_INJECT);
	barrier();
	writel(0, reg + LH_FAULT_INJECT);
}

static ssize_t fault_injection_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ped_dev *ped;
	struct ped_platdata *pdata;
	struct lh_nodegroup *group;
	char name[16];
	int ret;

	ped = g_ped;
	pdata = ped->pdata;

	ret = sscanf(buf, "%15s", name);
	if (ret != 1) {
		dev_err(dev, "%s: Please type a group name.\n", __func__);
		return count;
	}
	group = find_group_by_name(g_ped, name);
	if (!group) {
		dev_err(dev, "%s: group  %s not found\n", __func__, name);
		return count;
	}

	if (ped_pd_is_power_up(group))
		ped_fault_injection(group);
	else
		dev_err(dev, "%s: group %s power down! Skip it\n", __func__, name);

	return count;
}

static DEVICE_ATTR_WO(fault_injection);

static const struct of_device_id ped_dt_match[] = {
	{
		.compatible = "samsung,exynos-ped-v2",
		.data = NULL,
	},
	{},
};
MODULE_DEVICE_TABLE(of, ped_dt_match);

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
static int ped_cpupm_notifier(struct notifier_block *self,
				unsigned long cmd, void *v)
{
	if (g_ped->suspend_state)
		goto out;

	switch (cmd) {
	case DSUPD_ENTER:
	case SICD_ENTER:
		ped_clear_all(g_ped, true);
		break;

	case DSUPD_EXIT:
	case SICD_EXIT:
		ped_init_all(g_ped, true);
		break;

	default:
		break;
	}
out:
	return NOTIFY_DONE;
}

static struct notifier_block ped_cpupm_notifier_block = {
	.notifier_call = ped_cpupm_notifier,
};
#endif

static int ped_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *v)
{
	struct irq_desc *desc = irq_to_desc(g_ped->irq);

	switch (pm_event) {
	case PM_POST_SUSPEND:
		dev_dbg(g_ped->dev, "resume...\n");
		if (desc && desc->depth > 0)
			enable_irq(g_ped->irq);
		g_ped->suspend_state = false;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int ped_probe(struct platform_device *pdev)
{
	struct ped_dev *ped;
	struct device *dev = &pdev->dev;
	struct ped_platdata *pdata;
	struct lh_nodegroup *group;
	char *node_name;
	unsigned int irq;
	int ret, i;

	ped = devm_kzalloc(&pdev->dev,
				sizeof(struct ped_dev), GFP_KERNEL);
	if (!ped)
		return -ENOMEM;
	ped->dev = dev;

	spin_lock_init(&ped->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct ped_platdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ped->pdata = pdata;

	// TODO : Read LH list from DT
	ped->pdata->lh_group = &nodegroup_list[0];
	ped->pdata->num_nodegroup = ARRAY_SIZE(nodegroup_list);

	for (i = 0; i < (int)pdata->num_nodegroup; i++) {
		group = &pdata->lh_group[i];
		if (!group) {
			dev_err(ped->dev, "Group %d Empty!\n", i);
			return -ENOMEM;
		}

		node_name = group->name;

		if (group->phy_regs) {
			group->regs =
				devm_ioremap(&pdev->dev,
						group->phy_regs, LH_END_OF_REGS);
			if (group->regs == NULL) {
				dev_err(&pdev->dev,
					"failed to claim register region - %s\n",
					node_name);
				return -ENOENT;
			}
		}

#if defined(CONFIG_EXYNOS_PD) || defined(CONFIG_EXYNOS_PD_MODULE)
		if (group->pd_support)
			group->pd_domain = exynos_pd_lookup_name(&group->pd_name[0]);
#endif
	}

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	ped->irq = irq;
	irq_set_status_flags(irq, IRQ_NOAUTOEN);
	disable_irq(irq);
	ret = devm_request_irq(&pdev->dev, irq, ped_irq_handler, IRQF_NOBALANCING,
			dev_name(&pdev->dev), ped);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to request irq - %s\n", dev_name(&pdev->dev));
		return -ENOENT;
	}

	platform_set_drvdata(pdev, ped);
	ped->suspend_state = false;
	g_ped = ped;

	pm_notifier(ped_pm_notifier, INT_MAX);

#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
	exynos_cpupm_notifier_register(&ped_cpupm_notifier_block);
#endif

	pdata->probed = true;
	dev_info(&pdev->dev, "success to probe Exynos PED v2 driver\n");

	ret = device_create_file(&pdev->dev, &dev_attr_fault_injection);
	if (ret < 0)
		dev_err(&pdev->dev, "Failed to create fault_injection sysfs\n");

	enable_irq(ped->irq);

	return 0;
}

static int ped_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int ped_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	g_ped->suspend_state = true;
	disable_irq(g_ped->irq);

	return ped_clear_all(g_ped, false);
}

static int ped_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(ped_pm_ops, ped_suspend, ped_resume);

static struct platform_driver exynos_ped_v2_driver = {
	.probe = ped_probe,
	.remove = ped_remove,
	.driver = {
		.name = "exynos-ped-v2",
		.of_match_table = ped_dt_match,
		.pm = &ped_pm_ops,
	},
};
module_platform_driver(exynos_ped_v2_driver);

MODULE_DESCRIPTION("Samsung Exynos PED V2 DRIVER");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-ped");
