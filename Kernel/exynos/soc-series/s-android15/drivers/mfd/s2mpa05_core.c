/*
 * s2mpa05_core.c - mfd core driver for the s2mpa05
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
#include <linux/pmic/s2mpa05-mfd.h>
#include <linux/regulator/machine.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#elif IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif

#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

static struct mfd_cell s2mpa05_devs[] = {
	{ .name = "s2mpa05-gpio",	.id = TYPE_S2MPA05 },
};

#if IS_ENABLED(CONFIG_OF)
static int of_s2mpa05_core_parse_dt(struct device *dev, struct s2mpa05_dev *s2mpa05)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	return 0;
}
#else
static int of_s2mpa05_core_parse_dt(struct device *dev, struct s2mpa05_dev *s2mpa05)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpa05_set_exynos_func(struct s2p_dev *s2mpa05_sdev)
{
	if (!s2mpa05_sdev)
		return -ENODEV;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	s2mpa05_sdev->exynos_read_reg = exynos_acpm_read_reg;
	s2mpa05_sdev->exynos_bulk_read = exynos_acpm_bulk_read;
	s2mpa05_sdev->exynos_write_reg = exynos_acpm_write_reg;
	s2mpa05_sdev->exynos_bulk_write = exynos_acpm_bulk_write;
	s2mpa05_sdev->exynos_update_reg = exynos_acpm_update_reg;
#elif IS_ENABLED(CONFIG_EXYNOS_ESCA)
	s2mpa05_sdev->exynos_read_reg = exynos_esca_read_reg;
	s2mpa05_sdev->exynos_bulk_read = exynos_esca_bulk_read;
	s2mpa05_sdev->exynos_write_reg = exynos_esca_write_reg;
	s2mpa05_sdev->exynos_bulk_write = exynos_esca_bulk_write;
	s2mpa05_sdev->exynos_update_reg = exynos_esca_update_reg;
#endif
	return 0;
}

static void s2mpa05_set_base_addr(struct s2mpa05_dev *s2mpa05)
{
	s2mpa05->vgpio = S2MPA05_VGPIO_ADDR;
	s2mpa05->com = S2MPA05_COM_ADDR;
	s2mpa05->pm1 = S2MPA05_PM1_ADDR;
	s2mpa05->close1 = S2MPA05_CLOSE1_ADDR;
	s2mpa05->gpio = S2MPA05_GPIO_ADDR;
}

static int s2mpa05_get_rev_id(struct s2mpa05_dev *s2mpa05)
{
	struct s2p_dev *sdev = s2mpa05->sdev;
	struct s2p_pmic_rev *rev_id = sdev->rev_id;
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_read_reg(sdev, s2mpa05->com, S2MPA05_COM_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->pmic_rev = val;
	rev_id->pmic_sw_rev = S2MPA05_CHIP_ID_SW(rev_id->pmic_rev);
	rev_id->pmic_hw_rev = S2MPA05_CHIP_ID_HW(rev_id->pmic_rev);

	dev_info(s2mpa05->dev, "%s: rev(0x%02hhx) sw(0x%02hhx) hw(0x%02hhx)\n",
			__func__, rev_id->pmic_rev, rev_id->pmic_sw_rev, rev_id->pmic_hw_rev);

	return 0;
}

static int s2mpa05_i2c_probe(struct i2c_client *i2c)
{
	struct s2mpa05_dev *s2mpa05 = NULL;
	struct s2p_dev *s2mpa05_sdev = NULL;
	const int device_type = (enum s2mpa05_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[EXT_PMIC] %s: start\n", __func__);

	s2mpa05 = devm_kzalloc(&i2c->dev, sizeof(struct s2mpa05_dev), GFP_KERNEL);
	if (!s2mpa05)
		return -ENOMEM;

	ret = of_s2mpa05_core_parse_dt(&i2c->dev, s2mpa05);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}

	s2mpa05_sdev = devm_kzalloc(&i2c->dev, sizeof(struct s2p_dev), GFP_KERNEL);
	if (!s2mpa05_sdev)
		return -ENOMEM;

	s2mpa05_sdev->irq_base_count = S2MPA05_IRQ_NR;
	s2mpa05_sdev->device_type = device_type;

	ret = s2p_init(&i2c->dev, s2mpa05_sdev);
	if (ret < 0)
		return ret;

	s2mpa05->dev = &i2c->dev;
	s2mpa05->sdev = s2mpa05_sdev;

	ret = s2mpa05_set_exynos_func(s2mpa05_sdev);
	if (ret < 0)
		return ret;

	s2mpa05_set_base_addr(s2mpa05);

	i2c_set_clientdata(i2c, s2mpa05);

	ret = s2mpa05_get_rev_id(s2mpa05);
	if (ret < 0)
		return ret;

	ret = s2mpa05_irq_init(s2mpa05);
	if (ret < 0)
		return ret;

	ret = devm_mfd_add_devices(s2mpa05->dev, 1, s2mpa05_devs, ARRAY_SIZE(s2mpa05_devs), NULL, 0, NULL);
	if (ret < 0)
		return ret;

	dev_info(&i2c->dev, "[EXT_PMIC] %s: end\n", __func__);

	return ret;
}

static void s2mpa05_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpa05_dev *s2mpa05 = i2c_get_clientdata(i2c);

	s2p_destroy_workqueue(s2mpa05->sdev->irq_list->irqs, s2mpa05->sdev->irq_list->irq_type_cnt);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_irq_sysfs_entries(s2mpa05->sdev->irq_list->irq_sysfs_dev);
#endif
	s2p_remove(s2mpa05->sdev);
}

static const struct i2c_device_id s2mpa05_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPA05 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpa05_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mpa05_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpa05_mfd", .data = (void *)TYPE_S2MPA05 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mpa05_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mpa05_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mpa05_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mpa05_i2c_probe,
	.remove		= s2mpa05_i2c_remove,
	.id_table	= s2mpa05_i2c_id,
};

static int __init s2mpa05_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mpa05_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mpa05_i2c_init);

static void __exit s2mpa05_i2c_exit(void)
{
	i2c_del_driver(&s2mpa05_i2c_driver);
}
module_exit(s2mpa05_i2c_exit);

MODULE_DESCRIPTION("s2mpa05 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
