/*
 * s2p-irq.c - Interrupt controller support for PMIC
 *
 * Copyright (C) 2024 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/pmic/s2p.h>
#include <linux/input.h>

static struct s2p_irq_handler_ops irq_handler_ops[S2P_PMIC_NUM];

static void s2p_clear_sysreg(struct s2p_irq_dev *idev)
{
	uint32_t val = 0;

	/* Clear interrupt pending bit (to AP)*/
	val = readl(idev->sysreg_vgpio2ap);
	writel(val, idev->sysreg_vgpio2ap);

	/* Clear interrupt pending bit (to PMU)*/
	val = readl(idev->sysreg_vgpio2pmu);
	writel(val, idev->sysreg_vgpio2pmu);
}

static void s2p_read_vgpio_rx_monitor(struct s2p_irq_dev *idev)
{
	uint32_t val = 0, i = 0;

	val = readl(idev->mem_base);

	for (i = 0; i < idev->vgi_cnt; i++) {
		if (i == 4)
			val = readl(idev->mem_base2);
		*(idev->vgi_src + i) = val & 0x0F;
		val = (val >> 8);
	}
}

static irqreturn_t s2p_irq_thread(int irq, void *data)
{
	struct s2p_irq_dev *idev = data;
	uint32_t i = 0;

	s2p_clear_sysreg(idev);
	s2p_read_vgpio_rx_monitor(idev);

	while (irq_handler_ops[i].dev) {
		if (irq_handler_ops[i].handler(irq_handler_ops[i].dev, idev->vgi_src))
			return IRQ_NONE;

		i++;
	}

	return IRQ_HANDLED;
}

static void s2p_set_vgpio_monitor(struct s2p_irq_dev *idev)
{
	idev->mem_base = ioremap(idev->spmi_master_pmic + idev->vgpio_monitor, SZ_32);
	if (idev->mem_base == NULL)
		dev_err(idev->dev, "%s: fail to allocate mem_base\n", __func__);

	/* VOL_DN */
	idev->mem_base2 = ioremap(idev->spmi_master_pmic + idev->vgpio_monitor2, SZ_32);
	if (idev->mem_base2 == NULL)
		dev_err(idev->dev, "%s: fail to allocate mem_base2\n", __func__);
}

static void s2p_set_sysreg(struct s2p_irq_dev *idev)
{
	idev->sysreg_vgpio2ap = ioremap(idev->intcomb_vgpio2ap + idev->intc0_ipend, SZ_32);
	if (idev->sysreg_vgpio2ap == NULL)
		dev_err(idev->dev, "%s: fail to allocate sysreg_vgpio2ap\n", __func__);

	idev->sysreg_vgpio2pmu = ioremap(idev->intcomb_vgpio2pmu + idev->intc0_ipend, SZ_32);
	if (idev->sysreg_vgpio2pmu == NULL)
		dev_err(idev->dev, "%s: fail to allocate sysreg_vgpio2pmu\n", __func__);
}

static int s2p_set_interrupt(struct s2p_irq_dev *idev)
{
	static char irq_name[32] = {0, };
	int ret = 0;

	/* Dynamic allocation for device name */
	snprintf(irq_name, sizeof(irq_name) - 1, "%s-irq@%s",
		 dev_driver_string(idev->dev), dev_name(idev->dev));

	ret = devm_request_threaded_irq(idev->dev, idev->irq, NULL, s2p_irq_thread,
					IRQF_ONESHOT, irq_name, idev);
	if (ret) {
		dev_err(idev->dev, "%s: Failed to request IRQ %d: %d\n", __func__, idev->irq, ret);
		return ret;
	}

	return 0;
}

int s2p_register_irq_handler(struct device *dev, void *handler)
{
	static uint32_t size = 0;

	if (!dev || !handler) {
		pr_err("[PMIC] %s: Check %s %s\n", __func__,
				dev ? "" : "dev", handler ? "" : "handler");
		return -ENODEV;
	}

	if (size >= S2P_PMIC_NUM) {
		dev_err(dev, "[PMIC] %s: Check irq_handler_ops size\n", __func__);
		return -ERANGE;
	}

	irq_handler_ops[size].dev = dev;
	irq_handler_ops[size++].handler = handler;

	dev_info(dev, "[PMIC] %s: Add %s irq handler (%d)\n", __func__, dev_name(dev), size);

	return size;
}
EXPORT_SYMBOL(s2p_register_irq_handler);

#if IS_ENABLED(CONFIG_OF)
static int of_s2p_irq_parse_dt(struct device *dev, struct s2p_irq_dev *idev)
{
	struct device_node *par_np = dev->parent->of_node;
	struct device_node *irq_np = NULL;
	uint32_t val = 0;
	int ret = 0;

	if (!par_np)
		return -ENODEV;

	ret = of_property_read_u32(par_np, "s2p,wakeup", &val);
	if (ret)
		return ret;
	idev->wakeup = !!val;

	irq_np = of_find_node_by_name(par_np, "s2p_irq");
	if (!irq_np) {
		dev_err(dev, "%s: could not find s2p_irq sub-node\n", __func__);
		return -EINVAL;
	};
	dev->of_node = irq_np;

	ret = of_property_read_u32(irq_np, "s2p,vgi_cnt", &val);
	if (ret)
		return -EINVAL;
	idev->vgi_cnt = val;

	ret = of_property_read_u32(irq_np, "sysreg,spmi_master_pmic", &val);
	if (ret)
		return -EINVAL;
	idev->spmi_master_pmic = val;

	ret = of_property_read_u32(irq_np, "sysreg,vgpio_monitor", &val);
	if (ret)
		return -EINVAL;
	idev->vgpio_monitor = val;

	ret = of_property_read_u32(irq_np, "sysreg,vgpio_monitor2", &val);
	if (ret)
		return -EINVAL;
	idev->vgpio_monitor2 = val;

	ret = of_property_read_u32(irq_np, "sysreg,intcomb_vgpio2ap", &val);
	if (ret)
		return -EINVAL;
	idev->intcomb_vgpio2ap = val;

	ret = of_property_read_u32(irq_np, "sysreg,intcomb_vgpio2pmu", &val);
	if (ret)
		return -EINVAL;
	idev->intcomb_vgpio2pmu = val;

	ret = of_property_read_u32(irq_np, "sysreg,intc0_ipend", &val);
	if (ret)
		return -EINVAL;
	idev->intc0_ipend= val;

	dev_info(dev, "[PMIC] %s: vgi_cnt(%d) mem_base(%#x + %#x) mem_base2(%#x + %#x) vgpio2ap(%#x + %#x) vgpio2pmu(%#x + %#x)\n",
			__func__, idev->vgi_cnt,
			idev->spmi_master_pmic, idev->vgpio_monitor,
			idev->spmi_master_pmic, idev->vgpio_monitor2,
			idev->intcomb_vgpio2ap, idev->intc0_ipend,
			idev->intcomb_vgpio2pmu, idev->intc0_ipend);

	return 0;
}
#else
static int of_s2p_irq_parse_dt(struct device *dev, struct s2p_irq_dev *idev)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2p_irq_probe(struct platform_device *pdev)
{
	struct s2p_irq_dev *idev = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "[PMIC] %s: start\n", __func__);

	idev = devm_kzalloc(&pdev->dev, sizeof(struct s2p_irq_dev), GFP_KERNEL);
	if (!idev)
		return -ENODEV;

	ret = of_s2p_irq_parse_dt(&pdev->dev, idev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Failed to get device of node\n", __func__);
		return ret;
	}

	idev->vgi_src = devm_kzalloc(&pdev->dev, sizeof(uint8_t) * idev->vgi_cnt, GFP_KERNEL);
	if (!idev->vgi_src)
		return -ENODEV;

	idev->dev = &pdev->dev;
	idev->irq = irq_of_parse_and_map(idev->dev->of_node, 0);

	platform_set_drvdata(pdev, idev);

	s2p_set_vgpio_monitor(idev);
	s2p_set_sysreg(idev);

	ret = s2p_set_interrupt(idev);
	if (ret)
		return ret;

	ret = device_init_wakeup(&pdev->dev, idev->wakeup);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: device_init_wakeup fail(%d)\n", __func__, ret);
	}

	dev_info(&pdev->dev, "[PMIC] %s: end\n", __func__);

	return 0;
}

static int s2p_irq_remove(struct platform_device *pdev)
{
	struct s2p_irq_dev *idev = platform_get_drvdata(pdev);

	if (idev->irq)
		free_irq(idev->irq, idev);

	iounmap(idev->mem_base);
	iounmap(idev->mem_base2);
	iounmap(idev->sysreg_vgpio2ap);
	iounmap(idev->sysreg_vgpio2pmu);

	return 0;
}

static const struct platform_device_id s2p_irq_device_id[] = {
	{ "s2p-irq", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2p_irq_device_id);

#if IS_ENABLED(CONFIG_PM)
static int s2p_irq_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2p_irq_dev *idev = platform_get_drvdata(pdev);

	dev->power.must_resume = true;

	if (device_may_wakeup(dev))
		enable_irq_wake(idev->irq);

	disable_irq(idev->irq);

	return 0;
}

static int s2p_irq_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s2p_irq_dev *idev = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		disable_irq_wake(idev->irq);

	enable_irq(idev->irq);

	return 0;
}
#else
#define s2p_irq_suspend	NULL
#define s2p_irq_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops s2p_irq_pm = {
	.suspend_late = s2p_irq_suspend,
	.resume_early = s2p_irq_resume,
};

static struct platform_driver s2p_irq_driver = {
	.driver = {
		.name = "s2p-irq",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2p_irq_pm,
#endif /* CONFIG_PM */
		.suppress_bind_attrs = true,
	},
	.probe = s2p_irq_probe,
	.remove = s2p_irq_remove,
	.id_table = s2p_irq_device_id,
};
module_platform_driver(s2p_irq_driver);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2P Irq Driver");
MODULE_LICENSE("GPL");
