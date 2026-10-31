/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/gfp.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>

#include <soc/samsung/exynos/debug-snapshot.h>
#define NUM_OF_MIF_CH (4)
#define CESYNDROME		(0x614)
#define CEINTERRUPTCLEAR (0x604)
#define UCEINTERRUPTCLEAR (0x704)
#define VENDOR_SAMSUNG		(0x1)
#define VENDOR_MICRON		(0xFF)

struct linkecc_irq_desc {
	int irq;
	unsigned int channel;
	unsigned int clr;
};

struct exynos_linkecc_desc {
	struct linkecc_irq_desc *linkecc_irqs;
	raw_spinlock_t lock;
	int irq_num;
	void __iomem			*smc_mif_base[NUM_OF_MIF_CH];
};

static struct exynos_linkecc_desc linkecc_desc;

static struct linkecc_irq_desc *get_linkecc_irq_desc(int irq)
{
	int i;
	struct linkecc_irq_desc *desc = NULL;

	for (i = 0; i < linkecc_desc.irq_num; i++) {
		if (linkecc_desc.linkecc_irqs[i].irq == irq) {
			desc = &linkecc_desc.linkecc_irqs[i];
			break;
		}
	}
	return desc;
}

static irqreturn_t exynos_linkecc_handler(int irq, void *dev_id)
{
	struct linkecc_irq_desc *linkecc_irq_desc;
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned int ch, cesyndrome;

	if (desc && desc->action && desc->action->name)
		printk("[LINKECC] %s\n", desc->action->name);
	else
		printk("[LINKECC] irq=%d\n", irq);

	linkecc_irq_desc = get_linkecc_irq_desc(irq);
	if (!linkecc_irq_desc) {
		printk("Unexpected LINK ECC IRQ(%d)", irq);
		return IRQ_NONE;
	}

	ch = linkecc_irq_desc->channel;
	cesyndrome = __raw_readl(linkecc_desc.smc_mif_base[ch] + CESYNDROME);
	printk("[LINKECC] CESyndrome: 0x%x\n", cesyndrome);

	__raw_writel(1, linkecc_desc.smc_mif_base[ch] + linkecc_irq_desc->clr);

	return IRQ_HANDLED;
}

static int exynos_linkecc_handler_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct property *prop;
	const char *name;
	char *buf = (char *)__get_free_page(GFP_KERNEL);
	int irq_num;
	int err = 0, idx = 0;
	int ch, ch_num;
	unsigned int base;
	unsigned int key;

	if (!buf) {
		err = -ENOMEM;
		goto err_buf_alloc;
	}

	of_property_read_u32(np, "debug_info", &base);
	key = __raw_readl(ioremap(base, SZ_4K));

	if (((key >> 8) & 0xff) == VENDOR_MICRON)
		goto err_no_irq_vendor;

	irq_num = of_property_count_strings(np, "interrupt-names");
	if (irq_num <= 0) {
		dev_err(&pdev->dev, "Invalid Interrupt-names property \
				count(%d)\n", irq_num);
		err = -EINVAL;
		goto err_irq_name_counts;
	}

	linkecc_desc.linkecc_irqs = kzalloc(sizeof(struct linkecc_irq_desc) *
			irq_num, GFP_KERNEL);
	if (!linkecc_desc.linkecc_irqs) {
		err = -ENOMEM;
		goto err_desc_alloc;
	}

	//cpu_hotplug_disable();

	linkecc_desc.irq_num = irq_num;

	ch_num = of_property_count_u32_elems(np, "base");
	for (ch = 0; ch < ch_num; ch++) {
		of_property_read_u32_index(np, "base", ch, &base);
		linkecc_desc.smc_mif_base[ch] = ioremap(base, SZ_4K);
	}

	of_property_for_each_string(np, "interrupt-names", prop, name) {
		unsigned int irq, val;

		if (!name) {
			dev_err(&pdev->dev, "no such name\n");
			err = -EINVAL;
			break;
		}

		if (!of_property_read_u32_index(np, "channel", idx, &val))
			linkecc_desc.linkecc_irqs[idx].channel = val;
		else
			linkecc_desc.linkecc_irqs[idx].channel = 0;

		if (!of_property_read_u32_index(np, "clr_offs", idx, &val))
			linkecc_desc.linkecc_irqs[idx].clr = val;
		else
			linkecc_desc.linkecc_irqs[idx].clr = 0;

		irq = platform_get_irq(pdev, idx);
		err = request_irq(irq, exynos_linkecc_handler,
				IRQF_NOBALANCING, name, NULL);
		if (err) {
			dev_err(&pdev->dev, "unable to request irq%u for \
					linkecc handler[%s]\n",
					irq, name);
			break;
		}
		dev_info(&pdev->dev, "Success to request irq%u for \
				linkecc handler[%s]\n",	irq, name);

		linkecc_desc.linkecc_irqs[idx].irq = irq;

		idx++;
	}

	if (irq_num != idx) {
		int i;

		dev_err(&pdev->dev, "failed, irq_num not matched(%d/%d)\n",
				idx, irq_num);
		for (i = 0; i < idx; i++)
			free_irq(linkecc_desc.linkecc_irqs[i].irq, NULL);

		kfree(linkecc_desc.linkecc_irqs);
		linkecc_desc.linkecc_irqs = NULL;
		linkecc_desc.irq_num = 0;
		goto err_register_irq;
	}

	raw_spin_lock_init(&linkecc_desc.lock);
err_no_irq_vendor:
err_register_irq:
err_desc_alloc:
err_irq_name_counts:
	free_page((unsigned long)buf);
err_buf_alloc:
	return err;
}

static const struct of_device_id exynos_linkecc_handler_matches[] = {
	{ .compatible = "samsung,exynos-linkecc-handler", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_linkecc_handler_matches);

static struct platform_driver exynos_linkecc_handler_driver = {
	.probe	= exynos_linkecc_handler_probe,
	.driver	= {
		.name	= "exynos-linkecc-handler",
		.of_match_table	= of_match_ptr(exynos_linkecc_handler_matches),
	},
};
module_platform_driver(exynos_linkecc_handler_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EXYNOS LINKECC HANDLER");
