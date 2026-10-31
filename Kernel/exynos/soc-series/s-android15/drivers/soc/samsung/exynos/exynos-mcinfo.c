/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Support Memory controller specific information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/bits.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <linux/ems.h>

#define READ_HW_TEMP_RANGE(base, start_bit, width) \
	((__raw_readl(base) & GENMASK(start_bit + width - 1, start_bit)) >> start_bit)

struct mcinfo {
	struct list_head node;
	struct device	 *dev;
	void __iomem	 *base;
	u32		 irq_err;
	u32		 irq_hot;
};
static bool ambient_status;
static u32 mc_bit_array[2];
static struct cpumask mc_cpuhp_mask;
static u32 mc_cpuhp_cond;
static u32 normal_code_upper;
static DEFINE_SPINLOCK(mc_glb_lock);
static LIST_HEAD(mc_list);
static struct delayed_work mc_dwork;

#if IS_ENABLED(CONFIG_MCINFO_SYSFS)
static ssize_t show_exynos_ref_rate(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mcinfo *data = dev_get_drvdata(dev);
	ssize_t count = 0;
	unsigned int tmp;

	tmp = READ_HW_TEMP_RANGE(data->base, mc_bit_array[0], mc_bit_array[1]);
	count += snprintf(buf + count, PAGE_SIZE,
			  "%s: 0x%x\n", dev_name(data->dev), tmp);

	return count;
}

static DEVICE_ATTR(ref_rate, 0640, show_exynos_ref_rate, NULL);
#endif /* MCINFO_SYSFS */

static void exynos_mc_ecs_check(u32 new_cpuhp)
{
	static u32 old_cpuhp;
	struct cpumask mask;

	/* ECS by TEMP_HOT is not enabled */
	if (!mc_cpuhp_cond)
		return;

	if (old_cpuhp == new_cpuhp)
		return;

	if (new_cpuhp) {
		cpumask_copy(&mask, cpu_possible_mask);
		cpumask_andnot(&mask, &mask, &mc_cpuhp_mask);
	} else {
		cpumask_copy(&mask, cpu_possible_mask);
	}

	old_cpuhp = new_cpuhp;

	ecs_request("mcinfo_cpuhp", &mask, ECS_MAX);
	pr_info("%s: 0x%02x\n", __func__, *(u32 *)cpumask_bits(&mask));
}

static irqreturn_t exynos_mc_err_irq_handler(int irq, void *p)
{
	struct mcinfo *data = p;
	u32 code;

	spin_lock(&mc_glb_lock);

	disable_irq_nosync(irq);
	data->irq_err = irq;

	code = READ_HW_TEMP_RANGE(data->base, mc_bit_array[0], mc_bit_array[1]);
	pr_auto(ASL5, "%s %s: TEMP_ERR irq (code: 0x%x)\n", dev_driver_string(data->dev), dev_name(data->dev), code);

	if (ambient_status || code <= normal_code_upper)
		goto out;

	pr_auto(ASL5, "%s %s: [SW Trip] Memory temperature is too high\n", dev_driver_string(data->dev), dev_name(data->dev));
	dbg_snapshot_expire_watchdog();

out:
	spin_unlock(&mc_glb_lock);
	return IRQ_HANDLED;
}

static irqreturn_t exynos_mc_hot_irq_handler(int irq, void *p)
{
	struct mcinfo *data = p;
	u32 code;

	spin_lock(&mc_glb_lock);

	disable_irq_nosync(irq);
	data->irq_hot = irq;

	code = READ_HW_TEMP_RANGE(data->base, mc_bit_array[0], mc_bit_array[1]);
	dev_warn(data->dev, "TEMP_HOT irq (code: 0x%x)\n", code);

	exynos_mc_ecs_check(code >= mc_cpuhp_cond);

	spin_unlock(&mc_glb_lock);

	return IRQ_HANDLED;
}

static void exynos_mc_work_func(struct work_struct *work)
{
	struct mcinfo *data;
	u64 dss_codes = 0;
	u32 new_cpuhp = 0;
	unsigned long flags;

	spin_lock_irqsave(&mc_glb_lock, flags);

	list_for_each_entry(data, &mc_list, node) {
		u32 code;

		if (data->irq_hot) {
			enable_irq(data->irq_hot);
			data->irq_hot = 0;
		}

		if (data->irq_err) {
			enable_irq(data->irq_err);
			data->irq_err = 0;
		}

		code = READ_HW_TEMP_RANGE(data->base,
			mc_bit_array[0], mc_bit_array[1]);
		dss_codes = (dss_codes << 8) | (code);

		if (code >= mc_cpuhp_cond)
			new_cpuhp |= 1;
	}

	exynos_mc_ecs_check(new_cpuhp);
	spin_unlock_irqrestore(&mc_glb_lock, flags);

	dbg_snapshot_thermal(NULL, dss_codes, "MCINFO", 0);
	pr_debug("%s: codes: 0x%llx\n", __func__, dss_codes);

	schedule_delayed_work_on(0, &mc_dwork, msecs_to_jiffies(1000));
}

void exynos_mcinfo_set_ambient_status(bool status)
{
	ambient_status = status;
}
EXPORT_SYMBOL_GPL(exynos_mcinfo_set_ambient_status);

static int exynos_mcinfo_parse_dt_common(struct device_node *np)
{
	int ret = 0;
	const char *buf;

	np = of_find_node_by_name(np, "mcinfo_common");
	if (!np) {
		pr_err("%s: Failed to get mcinfo_common\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32_array(np, "bit_field", (u32 *)&mc_bit_array,
				(size_t)(ARRAY_SIZE(mc_bit_array)));
	if (ret) {
		pr_err("%s: Failed to get bit field information!\n", __func__);
		return ret;
	}

	if (of_property_read_u32(np, "hotplug_cond", &mc_cpuhp_cond))
		pr_warn("%s: do not turn on ECS by TEMP_HOT\n", __func__);

	if (of_property_read_u32(np, "normal_code_upper", &normal_code_upper))
		normal_code_upper = 0xf;
	pr_info("%s: err_cond for TEMP_ERR : %u\n", __func__, normal_code_upper);

	if (!of_property_read_string(np, "hotplug_cpu_list", &buf))
		cpulist_parse(buf, &mc_cpuhp_mask);

	return 0;
}

static int exynos_mcinfo_parse_dt(struct device_node *np, struct mcinfo *data)
{
	int ret = 0;
	struct resource *res;
	u32 irqnum = 0;
	struct platform_device *pdev = to_platform_device(data->dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(data->dev, res);
	if (IS_ERR(data->base)) {
		dev_err(data->dev, "failed to ioremap register\n");
		ret = -ENOMEM;
		return ret;
	}

	/* TEMP ERR */
	irqnum = irq_of_parse_and_map(np, 0);
	if (!irqnum) {
		dev_err(data->dev, "Failed to get IRQ map\n");
		return -EINVAL;
	}
	ret = devm_request_irq(data->dev, irqnum,
		exynos_mc_err_irq_handler,
		IRQF_SHARED, dev_name(data->dev), data);
	if (ret)
		return ret;

	/* TEMP HOT */
	irqnum = irq_of_parse_and_map(np, 1);
	if (!irqnum) {
		dev_info(data->dev, "mcinfo probe without TEMP_HOT IRQ\n");
		return 0;
	}
	ret = devm_request_irq(data->dev, irqnum,
		exynos_mc_hot_irq_handler,
		IRQF_SHARED, dev_name(data->dev), data);
	if (ret)
		return ret;

	return 0;
}

static int exynos_mcinfo_probe(struct platform_device *pdev)
{
	struct mcinfo *data;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	if (list_empty(&mc_list)) {
#if IS_ENABLED(CONFIG_EXYNOS_AMB_CONTROL)
		ambient_status = true;
#else
		/** If CONFIG_EXYNOS_AMB_CONTROL is not enabled than
		 * it should raise panic if hot_flag is 'true'.
		 */
		ambient_status = false;
#endif
		ecs_request_register("mcinfo_cpuhp", cpu_possible_mask, ECS_MAX);
		ret = exynos_mcinfo_parse_dt_common(np->parent);
		if (ret) {
			pr_err("Failed to parse root DT\n");
			return ret;
		}
		INIT_DELAYED_WORK(&mc_dwork, exynos_mc_work_func);
		schedule_delayed_work_on(0, &mc_dwork, msecs_to_jiffies(1000));
	}

	data = devm_kzalloc(&pdev->dev, sizeof(struct mcinfo), GFP_KERNEL);
	if (!data) {
		pr_err("%s: Not enough memory\n", __func__);
		return -ENOMEM;
	}

	data->dev = &pdev->dev;
	dev_set_drvdata(data->dev, data);

	ret = exynos_mcinfo_parse_dt(np, data);
	if (ret) {
		dev_err(data->dev, "Failed to parse device tree\n");
		return ret;
	}

	list_add_tail(&data->node, &mc_list);

#if IS_ENABLED(CONFIG_MCINFO_SYSFS)
	ret = sysfs_create_file(&data->dev->kobj, &dev_attr_ref_rate.attr);
	if (ret)
		dev_warn(data->dev, "Failed to create sysfs for MR4\n");
#endif /* MCINFO_SYSFS */

	dev_info(data->dev, "probe finished!\n");
	return 0;
}

static const struct of_device_id exynos_mcinfo_match[] = {
	{ .compatible	= "samsung,exynos-mcinfo", },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_mcinfo_match);

static struct platform_driver exynos_mcinfo_driver = {
	.probe		= exynos_mcinfo_probe,
	.driver	= {
		.name	= "exynos-mcinfo",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_mcinfo_match),
	},
};

module_platform_driver(exynos_mcinfo_driver);

MODULE_AUTHOR("Eunok Jo <eunok25.jo@samsung.com");
MODULE_DESCRIPTION("Samsung EXYNOS Memory controller specific information");
MODULE_LICENSE("GPL");
