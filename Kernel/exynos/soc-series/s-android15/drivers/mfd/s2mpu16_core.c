/*
 * s2mpu16_core.c - mfd core driver for the s2mpu16
 *
 * Copyright (C) 2024 Samsung Electronics
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
#include <linux/pmic/s2mpu16-mfd.h>
#include <linux/regulator/machine.h>
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

static struct mfd_cell s2mpu16_devs[] = {
	{ .name = "s2mpu16-gpio",	.id = TYPE_S2MPU16 },
};

#if IS_ENABLED(CONFIG_OF)
static int of_s2mpu16_core_parse_dt(struct device *dev, struct s2mpu16_dev *s2mpu16)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	return 0;
}
#else
static int of_s2mpu16_core_parse_dt(struct device *dev, struct s2mpu16_dev *s2mpu16)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpu16_set_exynos_func(struct s2p_dev *s2mpu16_sdev)
{
	if (!s2mpu16_sdev)
		return -ENODEV;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	s2mpu16_sdev->exynos_read_reg = exynos_esca_read_reg;
	s2mpu16_sdev->exynos_bulk_read = exynos_esca_bulk_read;
	s2mpu16_sdev->exynos_write_reg = exynos_esca_write_reg;
	s2mpu16_sdev->exynos_bulk_write = exynos_esca_bulk_write;
	s2mpu16_sdev->exynos_update_reg = exynos_esca_update_reg;
#endif
	return 0;
}

static void s2mpu16_set_base_addr(struct s2mpu16_dev *s2mpu16)
{
	s2mpu16->vgpio = S2MPU16_VGPIO_ADDR;
	s2mpu16->com = S2MPU16_COMMON_ADDR;
	s2mpu16->pmic1 = S2MPU16_PMIC1_ADDR;
	s2mpu16->pmic2 = S2MPU16_PMIC2_ADDR;
	s2mpu16->close1 = S2MPU16_CLOSE1_ADDR;
	s2mpu16->close2 = S2MPU16_CLOSE2_ADDR;
	s2mpu16->gpio = S2MPU16_GPIO_ADDR;
}

static int s2mpu16_get_rev_id(struct s2mpu16_dev *s2mpu16)
{
	struct s2p_dev *sdev = s2mpu16->sdev;
	struct s2p_pmic_rev *rev_id = sdev->rev_id;
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_read_reg(sdev, s2mpu16->com, S2MPU16_COMMON_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->pmic_rev = val;
	rev_id->pmic_sw_rev = S2MPU16_CHIP_ID_SW(rev_id->pmic_rev);
	rev_id->pmic_hw_rev = S2MPU16_CHIP_ID_HW(rev_id->pmic_rev);

	ret = s2p_read_reg(sdev, s2mpu16->pmic1, S2MPU16_PM1_PLATFORM_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->platform_rev = val;

	dev_info(s2mpu16->dev, "%s: SUB_PMIC: rev(0x%02hhx) sw(0x%02hhx) hw(0x%02hhx) platform(0x%02hhx)\n",
			__func__, rev_id->pmic_rev, rev_id->pmic_sw_rev,
			rev_id->pmic_hw_rev, rev_id->platform_rev);

	return 0;
}

static int s2mpu16_i2c_probe(struct i2c_client *i2c)
{
	struct s2mpu16_dev *s2mpu16 = NULL;
	struct s2p_dev *s2mpu16_sdev = NULL;
	int ret = 0;

	dev_info(&i2c->dev, "[SUB_PMIC] %s: start\n", __func__);

	s2mpu16 = devm_kzalloc(&i2c->dev, sizeof(struct s2mpu16_dev), GFP_KERNEL);
	if (!s2mpu16)
		return -ENOMEM;

	ret = of_s2mpu16_core_parse_dt(&i2c->dev, s2mpu16);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}

	s2mpu16_sdev = devm_kzalloc(&i2c->dev, sizeof(struct s2p_dev), GFP_KERNEL);
	if (!s2mpu16_sdev)
		return -ENOMEM;

	s2mpu16_sdev->irq_base_count = S2MPU16_IRQ_NR;

	ret = s2p_init(&i2c->dev, s2mpu16_sdev);
	if (ret < 0)
		return ret;

	s2mpu16->dev = &i2c->dev;
	s2mpu16->sdev = s2mpu16_sdev;

	ret = s2mpu16_set_exynos_func(s2mpu16_sdev);
	if (ret < 0)
		return ret;

	s2mpu16_set_base_addr(s2mpu16);

	i2c_set_clientdata(i2c, s2mpu16);

	ret = s2mpu16_get_rev_id(s2mpu16);
	if (ret < 0)
		return ret;

	ret = s2mpu16_irq_init(s2mpu16);
	if (ret < 0)
		return ret;

	ret = devm_mfd_add_devices(s2mpu16->dev, 1, s2mpu16_devs, ARRAY_SIZE(s2mpu16_devs), NULL, 0, NULL);
	if (ret < 0)
		return ret;

	dev_info(&i2c->dev, "[SUB_PMIC] %s: end\n", __func__);

	return ret;
}

static void s2mpu16_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpu16_dev *s2mpu16 = i2c_get_clientdata(i2c);

	s2p_destroy_workqueue(s2mpu16->sdev->irq_list->irqs, s2mpu16->sdev->irq_list->irq_type_cnt);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_irq_sysfs_entries(s2mpu16->sdev->irq_list->irq_sysfs_dev);
#endif
	s2p_remove(s2mpu16->sdev);
}

static const struct i2c_device_id s2mpu16_i2c_id[] = {
	{ "s2mpu16", TYPE_S2MPU16 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpu16_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mpu16_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpu16_mfd", .data = (void *)TYPE_S2MPU16 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mpu16_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mpu16_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mpu16_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mpu16_i2c_probe,
	.remove		= s2mpu16_i2c_remove,
	.id_table	= s2mpu16_i2c_id,
};

static int __init s2mpu16_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mpu16_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mpu16_i2c_init);

static void __exit s2mpu16_i2c_exit(void)
{
	i2c_del_driver(&s2mpu16_i2c_driver);
}
module_exit(s2mpu16_i2c_exit);

MODULE_DESCRIPTION("s2mpu16 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
