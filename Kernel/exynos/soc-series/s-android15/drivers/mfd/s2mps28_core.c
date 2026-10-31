/*
 * s2mps28_core.c - mfd core driver for the s2mps28
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
#include <linux/pmic/s2mps28-mfd.h>
#include <linux/regulator/machine.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

static struct mfd_cell s2mps28_1_devs[] = {
	{ .name = "s2mps28-1-regulator",	.id = TYPE_S2MPS28_1, },
	{ .name = "s2mps28-1-gpio", 		.id = TYPE_S2MPS28_1, },
};

static struct mfd_cell s2mps28_2_devs[] = {
	{ .name = "s2mps28-2-regulator",	.id = TYPE_S2MPS28_2, },
	{ .name = "s2mps28-2-gpio", 		.id = TYPE_S2MPS28_2, },
};

static struct mfd_cell s2mps28_3_devs[] = {
	{ .name = "s2mps28-3-regulator",	.id = TYPE_S2MPS28_3, },
	{ .name = "s2mps28-3-gpio", 		.id = TYPE_S2MPS28_3, },
};

static struct mfd_cell s2mps28_4_devs[] = {
	{ .name = "s2mps28-4-regulator",	.id = TYPE_S2MPS28_4, },
	{ .name = "s2mps28-4-gpio", 		.id = TYPE_S2MPS28_4, },
};

static struct mfd_cell s2mps28_5_devs[] = {
	{ .name = "s2mps28-5-regulator",	.id = TYPE_S2MPS28_5, },
	{ .name = "s2mps28-5-gpio", 		.id = TYPE_S2MPS28_5, },
};

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps28_core_parse_dt(struct device *dev, struct s2mps28_dev *s2mps28)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	return 0;
}
#else
static int of_s2mps28_core_parse_dt(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mps28_set_exynos_func(struct s2p_dev *sdev)
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

static void s2mps28_set_base_addr(struct s2mps28_dev *s2mps28)
{
	s2mps28->vgpio = S2MPS28_VGPIO_ADDR;
	s2mps28->com = S2MPS28_COM_ADDR;
	s2mps28->pm1 = S2MPS28_PM1_ADDR;
	s2mps28->pm2 = S2MPS28_PM2_ADDR;
	s2mps28->pm3 = S2MPS28_PM3_ADDR;
	s2mps28->gpio = S2MPS28_GPIO_ADDR;
	s2mps28->ext = S2MPS28_EXT_ADDR;
}

static int s2mps28_get_rev_id(struct s2mps28_dev *s2mps28)
{
	struct s2p_dev *sdev = s2mps28->sdev;
	struct s2p_pmic_rev *rev_id = sdev->rev_id;
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_read_reg(sdev, s2mps28->com, S2MPS28_COM_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->pmic_rev = val;
	rev_id->pmic_sw_rev = S2MPS28_CHIP_ID_SW(rev_id->pmic_rev);
	rev_id->pmic_hw_rev = S2MPS28_CHIP_ID_HW(rev_id->pmic_rev);

	ret = s2p_read_reg(sdev, s2mps28->com, S2MPS28_COM_PLATFORM_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->platform_rev = val;

	dev_info(s2mps28->dev, "%s: SUB%d_PMIC: rev(0x%02hhx) sw(0x%02hhx) hw(0x%02hhx) platform(0x%02hhx)\n",
			__func__, sdev->device_type + 1, rev_id->pmic_rev, rev_id->pmic_sw_rev,
			rev_id->pmic_hw_rev, rev_id->platform_rev);

	return 0;
}

static int s2mps28_get_mfd_cell(struct s2mps28_dev *s2mps28, const int device_type)
{
	switch (device_type) {
	case TYPE_S2MPS28_1:
		s2mps28->cell = s2mps28_1_devs;
		s2mps28->cell_size = ARRAY_SIZE(s2mps28_1_devs);
		break;
	case TYPE_S2MPS28_2:
		s2mps28->cell = s2mps28_2_devs;
		s2mps28->cell_size = ARRAY_SIZE(s2mps28_2_devs);
		break;
	case TYPE_S2MPS28_3:
		s2mps28->cell = s2mps28_3_devs;
		s2mps28->cell_size = ARRAY_SIZE(s2mps28_3_devs);
		break;
	case TYPE_S2MPS28_4:
		s2mps28->cell = s2mps28_4_devs;
		s2mps28->cell_size = ARRAY_SIZE(s2mps28_4_devs);
		break;
	case TYPE_S2MPS28_5:
		s2mps28->cell = s2mps28_5_devs;
		s2mps28->cell_size = ARRAY_SIZE(s2mps28_5_devs);
		break;
	default:
		dev_err(s2mps28->dev, "%s: device_type(%d) error\n", __func__, device_type);
		return -EINVAL;
	}

	return 0;
}

static int s2mps28_i2c_probe(struct i2c_client *i2c)
{
	struct s2mps28_dev *s2mps28 = NULL;
	struct s2p_dev *sdev = NULL;
	const int device_type = (enum s2mps28_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: start\n", device_type + 1, __func__);

	s2mps28 = devm_kzalloc(&i2c->dev, sizeof(struct s2mps28_dev), GFP_KERNEL);
	if (!s2mps28)
		return -ENOMEM;

	ret = of_s2mps28_core_parse_dt(&i2c->dev, s2mps28);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}

	sdev = devm_kzalloc(&i2c->dev, sizeof(struct s2p_dev), GFP_KERNEL);
	if (!sdev)
		return -ENOMEM;

	sdev->irq_base_count = S2MPS28_IRQ_NR;
	sdev->device_type = device_type;

	ret = s2p_init(&i2c->dev, sdev);
	if (ret < 0)
		return ret;

	s2mps28->dev = &i2c->dev;
	s2mps28->sdev = sdev;

	ret = s2mps28_set_exynos_func(sdev);
	if (ret < 0)
		return ret;

	s2mps28_set_base_addr(s2mps28);

	i2c_set_clientdata(i2c, s2mps28);

	ret = s2mps28_get_rev_id(s2mps28);
	if (ret < 0)
		return ret;

	ret = s2mps28_irq_init(s2mps28);
	if (ret < 0)
		return ret;

	ret = s2mps28_get_mfd_cell(s2mps28, device_type);
	if (ret < 0)
		return ret;

	ret = devm_mfd_add_devices(s2mps28->dev, 1, s2mps28->cell, s2mps28->cell_size, NULL, 0, NULL);
	if (ret < 0)
		return ret;

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: end\n", device_type + 1, __func__);

	return ret;
}

static void s2mps28_i2c_remove(struct i2c_client *i2c)
{
	struct s2mps28_dev *s2mps28 = i2c_get_clientdata(i2c);

	s2p_destroy_workqueue(s2mps28->sdev->irq_list->irqs, s2mps28->sdev->irq_list->irq_type_cnt);
	s2p_remove(s2mps28->sdev);
}

static const struct i2c_device_id s2mps28_i2c_id[] = {
	{ "s2mps28-1", TYPE_S2MPS28_1 },
	{ "s2mps28-2", TYPE_S2MPS28_2 },
	{ "s2mps28-3", TYPE_S2MPS28_3 },
	{ "s2mps28-4", TYPE_S2MPS28_4 },
	{ "s2mps28-5", TYPE_S2MPS28_5 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mps28_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps28_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps28_1_mfd", .data = (void *)TYPE_S2MPS28_1 },
	{ .compatible = "samsung,s2mps28_2_mfd", .data = (void *)TYPE_S2MPS28_2 },
	{ .compatible = "samsung,s2mps28_3_mfd", .data = (void *)TYPE_S2MPS28_3 },
	{ .compatible = "samsung,s2mps28_4_mfd", .data = (void *)TYPE_S2MPS28_4 },
	{ .compatible = "samsung,s2mps28_5_mfd", .data = (void *)TYPE_S2MPS28_5 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mps28_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mps28_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mps28_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2mps28_i2c_probe,
	.remove		= s2mps28_i2c_remove,
	.id_table	= s2mps28_i2c_id,
};

static int __init s2mps28_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps28_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mps28_i2c_init);

static void __exit s2mps28_i2c_exit(void)
{
	i2c_del_driver(&s2mps28_i2c_driver);
}
module_exit(s2mps28_i2c_exit);

MODULE_DESCRIPTION("s2mps28 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
