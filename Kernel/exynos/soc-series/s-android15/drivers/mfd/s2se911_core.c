/*
 * s2se911_core.c - mfd core driver for the s2se911
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
#include <linux/pmic/s2se911-mfd.h>
#include <linux/regulator/machine.h>
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

static struct mfd_cell s2se911_1_devs[] = {
	{ .name = "s2se911-1-gpio",		.id = TYPE_S2SE911_1 },
};

static struct mfd_cell s2se911_2_devs[] = {
	{ .name = "s2se911-2-gpio",		.id = TYPE_S2SE911_2 },
};

static struct mfd_cell s2se911_3_devs[] = {
	{ .name = "s2se911-3-gpio",		.id = TYPE_S2SE911_3 },
};

static struct mfd_cell s2se911_4_devs[] = {
	{ .name = "s2se911-4-gpio",		.id = TYPE_S2SE911_4 },
};

static struct mfd_cell s2se911_5_devs[] = {
	{ .name = "s2se911-5-gpio",		.id = TYPE_S2SE911_5 },
};

#if IS_ENABLED(CONFIG_OF)
static int of_s2se911_core_parse_dt(struct device *dev, struct s2se911_dev *s2se911)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	return 0;
}
#else
static int of_s2se911_core_parse_dt(struct device *dev, struct s2se911_dev *s2se911)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2se911_set_exynos_func(struct s2p_dev *s2se911_sdev)
{
	if (!s2se911_sdev)
		return -ENODEV;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	s2se911_sdev->exynos_read_reg = exynos_esca_read_reg;
	s2se911_sdev->exynos_bulk_read = exynos_esca_bulk_read;
	s2se911_sdev->exynos_write_reg = exynos_esca_write_reg;
	s2se911_sdev->exynos_bulk_write = exynos_esca_bulk_write;
	s2se911_sdev->exynos_update_reg = exynos_esca_update_reg;
#endif
	return 0;
}

static void s2se911_set_base_addr(struct s2se911_dev *s2se911)
{
	s2se911->vgpio = S2SE911_VGPIO_ADDR;
	s2se911->com = S2SE911_COM_ADDR;
	s2se911->pm1 = S2SE911_PM1_ADDR;
	s2se911->pm2 = S2SE911_PM2_ADDR;
	s2se911->pm3 = S2SE911_PM3_ADDR;
	s2se911->buck = S2SE911_BUCK_ADDR;
	s2se911->ldo = S2SE911_LDO_ADDR;
	s2se911->gpio = S2SE911_GPIO_ADDR;
	s2se911->ext = S2SE911_EXT_ADDR;
	s2se911->buck_trim = S2SE911_BUCK_TRIM_ADDR;
	s2se911->ldo_trim = S2SE911_LDO_TRIM_ADDR;
}

static int s2se911_get_rev_id(struct s2se911_dev *s2se911)
{
	struct s2p_dev *sdev = s2se911->sdev;
	struct s2p_pmic_rev *rev_id = sdev->rev_id;
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_read_reg(sdev, s2se911->com, S2SE911_COM_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->pmic_rev = val;
	rev_id->pmic_sw_rev = S2SE911_CHIP_ID_SW(rev_id->pmic_rev);
	rev_id->pmic_hw_rev = S2SE911_CHIP_ID_HW(rev_id->pmic_rev);

	ret = s2p_read_reg(sdev, s2se911->com, S2SE911_COM_PLATFORM_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->platform_rev = val;

	dev_info(s2se911->dev, "%s: SUB%d_PMIC: rev(0x%02hhx) sw(0x%02hhx) hw(0x%02hhx) platform(0x%02hhx)\n",
			__func__, sdev->device_type + 1, rev_id->pmic_rev, rev_id->pmic_sw_rev,
			rev_id->pmic_hw_rev, rev_id->platform_rev);

	return 0;
}

static int s2se911_get_mfd_cell(struct s2se911_dev *s2se911, const int device_type)
{
	switch (device_type) {
	case TYPE_S2SE911_1:
		s2se911->cell = s2se911_1_devs;
		s2se911->cell_size = ARRAY_SIZE(s2se911_1_devs);
		break;
	case TYPE_S2SE911_2:
		s2se911->cell = s2se911_2_devs;
		s2se911->cell_size = ARRAY_SIZE(s2se911_2_devs);
		break;
	case TYPE_S2SE911_3:
		s2se911->cell = s2se911_3_devs;
		s2se911->cell_size = ARRAY_SIZE(s2se911_3_devs);
		break;
	case TYPE_S2SE911_4:
		s2se911->cell = s2se911_4_devs;
		s2se911->cell_size = ARRAY_SIZE(s2se911_4_devs);
		break;
	case TYPE_S2SE911_5:
		s2se911->cell = s2se911_5_devs;
		s2se911->cell_size = ARRAY_SIZE(s2se911_5_devs);
		break;
	default:
		dev_err(s2se911->dev, "%s: device_type(%d) error\n", __func__, device_type);
		return -EINVAL;
	}

	return 0;
}

static int s2se911_i2c_probe(struct i2c_client *i2c)
{
	struct s2se911_dev *s2se911 = NULL;
	struct s2p_dev *s2se911_sdev = NULL;
	const int device_type = (enum s2se911_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: start\n", device_type + 1, __func__);

	s2se911 = devm_kzalloc(&i2c->dev, sizeof(struct s2se911_dev), GFP_KERNEL);
	if (!s2se911)
		return -ENOMEM;

	ret = of_s2se911_core_parse_dt(&i2c->dev, s2se911);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}

	s2se911_sdev = devm_kzalloc(&i2c->dev, sizeof(struct s2p_dev), GFP_KERNEL);
	if (!s2se911_sdev)
		return -ENOMEM;

	s2se911_sdev->irq_base_count = S2SE911_IRQ_NR;
	s2se911_sdev->device_type = device_type;

	ret = s2p_init(&i2c->dev, s2se911_sdev);
	if (ret < 0)
		return ret;

	s2se911->dev = &i2c->dev;
	s2se911->sdev = s2se911_sdev;

	ret = s2se911_set_exynos_func(s2se911_sdev);
	if (ret < 0)
		return ret;

	s2se911_set_base_addr(s2se911);

	i2c_set_clientdata(i2c, s2se911);

	ret = s2se911_get_rev_id(s2se911);
	if (ret < 0)
		return ret;

	ret = s2se911_irq_init(s2se911);
	if (ret < 0)
		return ret;

	ret = s2se911_get_mfd_cell(s2se911, device_type);
	if (ret < 0)
		return ret;

	ret = devm_mfd_add_devices(s2se911->dev, 1, s2se911->cell, s2se911->cell_size, NULL, 0, NULL);
	if (ret < 0)
		return ret;

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: end\n", device_type + 1, __func__);

	return ret;
}

static void s2se911_i2c_remove(struct i2c_client *i2c)
{
	struct s2se911_dev *s2se911 = i2c_get_clientdata(i2c);

	s2p_destroy_workqueue(s2se911->sdev->irq_list->irqs, s2se911->sdev->irq_list->irq_type_cnt);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_irq_sysfs_entries(s2se911->sdev->irq_list->irq_sysfs_dev);
#endif
	s2p_remove(s2se911->sdev);
}

static const struct i2c_device_id s2se911_i2c_id[] = {
	{ "s2se911-1", TYPE_S2SE911_1 },
	{ "s2se911-2", TYPE_S2SE911_2 },
	{ "s2se911-3", TYPE_S2SE911_3 },
	{ "s2se911-4", TYPE_S2SE911_4 },
	{ "s2se911-5", TYPE_S2SE911_5 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2se911_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2se911_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2se911_1_mfd", .data = (void *)TYPE_S2SE911_1 },
	{ .compatible = "samsung,s2se911_2_mfd", .data = (void *)TYPE_S2SE911_2 },
	{ .compatible = "samsung,s2se911_3_mfd", .data = (void *)TYPE_S2SE911_3 },
	{ .compatible = "samsung,s2se911_4_mfd", .data = (void *)TYPE_S2SE911_4 },
	{ .compatible = "samsung,s2se911_5_mfd", .data = (void *)TYPE_S2SE911_5 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2se911_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2se911_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2se911_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2se911_i2c_probe,
	.remove		= s2se911_i2c_remove,
	.id_table	= s2se911_i2c_id,
};

static int __init s2se911_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2se911_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2se911_i2c_init);

static void __exit s2se911_i2c_exit(void)
{
	i2c_del_driver(&s2se911_i2c_driver);
}
module_exit(s2se911_i2c_exit);

MODULE_DESCRIPTION("s2se911 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
