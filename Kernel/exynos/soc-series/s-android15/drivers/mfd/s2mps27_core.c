/*
 * s2mps27_core.c - mfd core driver for the s2mps27
 *
 * Copyright (C) 2023 Samsung Electronics
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/pmic/s2mps27-mfd.h>
#include <linux/regulator/machine.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

static struct mfd_cell s2mps27_devs[] = {
	{ .name = "s2p-irq",		.id = TYPE_S2MPS27 },
	{ .name = "s2mps27-rtc",	.id = TYPE_S2MPS27 },
	{ .name = "s2mps27-power-keys",	.id = TYPE_S2MPS27 },
	{ .name = "s2mps27-gpadc", 	.id = TYPE_S2MPS27 },
	{ .name = "s2mps27-gpio", 	.id = TYPE_S2MPS27 },
};

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps27_core_parse_dt(struct device *dev, struct s2mps27_dev *s2mps27)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	return 0;
}
#else
static int of_s2mps27_core_parse_dt(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mps27_set_exynos_func(struct s2p_dev *sdev)
{
	if (!sdev)
		return -ENODEV;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	sdev->exynos_read_reg = exynos_acpm_read_reg;
	sdev->exynos_bulk_read = exynos_acpm_bulk_read;
	sdev->exynos_write_reg = exynos_acpm_write_reg;
	sdev->exynos_bulk_write = exynos_acpm_bulk_write;
	sdev->exynos_update_reg = exynos_acpm_update_reg;
#endif
	return 0;
}

static void s2mps27_set_base_addr(struct s2mps27_dev *s2mps27)
{
	s2mps27->vgpio = S2MPS27_VGPIO_ADDR;
	s2mps27->com = S2MPS27_COM_ADDR;
	s2mps27->rtc = S2MPS27_RTC_ADDR;
	s2mps27->pm1 = S2MPS27_PM1_ADDR;
	s2mps27->pm2 = S2MPS27_PM2_ADDR;
	s2mps27->pm3 = S2MPS27_PM3_ADDR;
	s2mps27->adc = S2MPS27_ADC_ADDR;
	s2mps27->gpio = S2MPS27_GPIO_ADDR;
	s2mps27->ext = S2MPS27_EXT_ADDR;
}

static int s2mps27_get_rev_id(struct s2mps27_dev *s2mps27)
{
	struct s2p_dev *sdev = s2mps27->sdev;
	struct s2p_pmic_rev *rev_id = sdev->rev_id;
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_read_reg(sdev, s2mps27->com, S2MPS27_COM_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->pmic_rev = val;
	rev_id->pmic_sw_rev = S2MPS27_CHIP_ID_SW(rev_id->pmic_rev);
	rev_id->pmic_hw_rev = S2MPS27_CHIP_ID_HW(rev_id->pmic_rev);

	ret = s2p_read_reg(sdev, s2mps27->com, S2MPS27_COM_PLATFORM_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->platform_rev = val;

	dev_info(s2mps27->dev, "%s: rev(0x%02hhx) sw(0x%02hhx) hw(0x%02hhx) platform(0x%02hhx)\n",
			__func__, rev_id->pmic_rev, rev_id->pmic_sw_rev,
			rev_id->pmic_hw_rev, rev_id->platform_rev);

	return 0;
}

static int s2mps27_i2c_probe(struct i2c_client *i2c)
{
	struct s2mps27_dev *s2mps27 = NULL;
	struct s2p_dev *sdev = NULL;
	const int device_type = (enum s2mps27_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[MAIN_PMIC] %s: start\n", __func__);

	s2mps27 = devm_kzalloc(&i2c->dev, sizeof(struct s2mps27_dev), GFP_KERNEL);
	if (!s2mps27)
		return -ENOMEM;

	ret = of_s2mps27_core_parse_dt(&i2c->dev, s2mps27);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}

	sdev = devm_kzalloc(&i2c->dev, sizeof(struct s2p_dev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	sdev->irq_base_count = S2MPS27_IRQ_NR;
	sdev->device_type = device_type;

	ret = s2p_init(&i2c->dev, sdev);
	if (ret < 0)
		return ret;

	s2mps27->dev = &i2c->dev;
	s2mps27->sdev = sdev;

	ret = s2mps27_set_exynos_func(sdev);
	if (ret < 0)
		return ret;

	s2mps27_set_base_addr(s2mps27);

	i2c_set_clientdata(i2c, s2mps27);

	ret = s2mps27_get_rev_id(s2mps27);
	if (ret < 0)
		return ret;

	ret = s2mps27_irq_init(s2mps27);
	if (ret < 0)
		return ret;

	ret = devm_mfd_add_devices(s2mps27->dev, 1, s2mps27_devs, ARRAY_SIZE(s2mps27_devs), NULL, 0, NULL);
	if (ret < 0)
		return ret;

	dev_info(&i2c->dev, "[MAIN_PMIC] %s: end\n", __func__);

	return ret;
}

static void s2mps27_i2c_remove(struct i2c_client *i2c)
{
	struct s2mps27_dev *s2mps27 = i2c_get_clientdata(i2c);

	s2p_destroy_workqueue(s2mps27->sdev->irq_list->irqs, s2mps27->sdev->irq_list->irq_type_cnt);
	s2p_remove(s2mps27->sdev);
}

static const struct i2c_device_id s2mps27_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPS27 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mps27_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps27_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps27_mfd", .data = (void *)TYPE_S2MPS27 },
	{ },
};
#endif /* CONFIG_OF */

static struct i2c_driver s2mps27_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mps27_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mps27_i2c_probe,
	.remove		= s2mps27_i2c_remove,
	.id_table	= s2mps27_i2c_id,
};

static int __init s2mps27_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps27_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mps27_i2c_init);

static void __exit s2mps27_i2c_exit(void)
{
	i2c_del_driver(&s2mps27_i2c_driver);
}
module_exit(s2mps27_i2c_exit);

MODULE_DESCRIPTION("s2mps27 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
