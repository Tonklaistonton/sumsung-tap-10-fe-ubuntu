/*
 * s2rp910_core.c - mfd core driver for the s2rp910
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
#include <linux/pmic/s2rp910-mfd.h>
#include <linux/regulator/machine.h>
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

static struct mfd_cell s2rp910_1_devs[] = {
	{ .name = "s2rp910-1-gpadc",		.id = TYPE_S2RP910_1 },
};

static struct mfd_cell s2rp910_2_devs[] = {
	{ .name = "s2rp910-2-gpadc",		.id = TYPE_S2RP910_2 },
};

#if IS_ENABLED(CONFIG_OF)
static int of_s2rp910_core_parse_dt(struct device *dev, struct s2rp910_dev *s2rp910)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	return 0;
}
#else
static int of_s2rp910_core_parse_dt(struct device *dev, struct s2rp910_dev *s2rp910)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2rp910_set_exynos_func(struct s2p_dev *s2rp910_sdev)
{
	if (!s2rp910_sdev)
		return -ENODEV;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	s2rp910_sdev->exynos_read_reg = exynos_esca_read_reg;
	s2rp910_sdev->exynos_bulk_read = exynos_esca_bulk_read;
	s2rp910_sdev->exynos_write_reg = exynos_esca_write_reg;
	s2rp910_sdev->exynos_bulk_write = exynos_esca_bulk_write;
	s2rp910_sdev->exynos_update_reg = exynos_esca_update_reg;
#endif
	return 0;
}

static void s2rp910_set_base_addr(struct s2rp910_dev *s2rp910)
{
	s2rp910->vgpio = S2RP910_VGPIO_ADDR;
	s2rp910->com = S2RP910_COM_ADDR;
	s2rp910->pm1 = S2RP910_PM1_ADDR;
	s2rp910->pm2 = S2RP910_PM2_ADDR;
	s2rp910->pm3 = S2RP910_PM3_ADDR;
	s2rp910->buck = S2RP910_BUCK_ADDR;
	s2rp910->ldo = S2RP910_LDO_ADDR;
	s2rp910->ext = S2RP910_EXT_ADDR;
	s2rp910->adc = S2RP910_ADC_ADDR;
}

static int s2rp910_get_rev_id(struct s2rp910_dev *s2rp910)
{
	struct s2p_dev *sdev = s2rp910->sdev;
	struct s2p_pmic_rev *rev_id = sdev->rev_id;
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_read_reg(sdev, s2rp910->com, S2RP910_COM_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->pmic_rev = val;
	rev_id->pmic_sw_rev = S2RP910_CHIP_ID_SW(rev_id->pmic_rev);
	rev_id->pmic_hw_rev = S2RP910_CHIP_ID_HW(rev_id->pmic_rev);

	ret = s2p_read_reg(sdev, s2rp910->com, S2RP910_COM_PLATFORM_ID, &val);
	if (ret < 0) {
		dev_err(sdev->dev,
			"device not found on this channel (this is not an error)\n");
		return ret;
	}
	rev_id->platform_rev = val;

	dev_info(s2rp910->dev, "%s: RF%d_PMIC: rev(0x%02hhx) sw(0x%02hhx) hw(0x%02hhx) platform(0x%02hhx)\n",
			__func__, sdev->device_type + 1, rev_id->pmic_rev, rev_id->pmic_sw_rev,
			rev_id->pmic_hw_rev, rev_id->platform_rev);

	return 0;
}

static int s2rp910_get_mfd_cell(struct s2rp910_dev *s2rp910, const int device_type)
{
	switch(device_type) {
	case TYPE_S2RP910_1:
		s2rp910->cell = s2rp910_1_devs;
		s2rp910->cell_size = ARRAY_SIZE(s2rp910_1_devs);
		break;
	case TYPE_S2RP910_2:
		s2rp910->cell = s2rp910_2_devs;
		s2rp910->cell_size = ARRAY_SIZE(s2rp910_2_devs);
		break;
	default:
		dev_err(s2rp910->dev, "%s: device_type(%d) error\n", __func__, device_type);
		return -EINVAL;
	}

	return 0;
}

static int s2rp910_chk_rf_sub_board_wa(const struct s2rp910_dev *s2rp910)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	uint8_t val = 0;
	const int device_type = s2rp910->sdev->device_type;

	if (device_type != TYPE_S2RP910_1)
		return 0;

	/* SUB2 PMIC GPIO0 check status */
	exynos_esca_read_reg(s2rp910->sdev->bus_node, 0x04, 0x0B, 0x04, &val);
	val &= 0x01;

	if (val) {
		dev_info(s2rp910->dev, "[RF%d_PMIC] %s: Not connected to rf sub board. SUB2 GPIO0: %u\n",
				device_type + 1, __func__, val);
		return -ENODEV;
	}
#endif
	return ret;
}

static int s2rp910_i2c_probe(struct i2c_client *i2c)
{
	struct s2rp910_dev *s2rp910 = NULL;
	struct s2p_dev *s2rp910_sdev = NULL;
	const int device_type = (enum s2rp910_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[RF%d_PMIC] %s: start\n", device_type + 1, __func__);

	s2rp910 = devm_kzalloc(&i2c->dev, sizeof(struct s2rp910_dev), GFP_KERNEL);
	if (!s2rp910)
		return -ENOMEM;

	ret = of_s2rp910_core_parse_dt(&i2c->dev, s2rp910);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to get device of_node\n");
		return ret;
	}

	s2rp910_sdev = devm_kzalloc(&i2c->dev, sizeof(struct s2p_dev), GFP_KERNEL);
	if (!s2rp910_sdev)
		return -ENOMEM;

	s2rp910_sdev->irq_base_count = S2RP910_IRQ_NR;
	s2rp910_sdev->device_type = device_type;

	ret = s2p_init(&i2c->dev, s2rp910_sdev);
	if (ret < 0)
		return ret;

	s2rp910->dev = &i2c->dev;
	s2rp910->sdev = s2rp910_sdev;

	ret = s2rp910_chk_rf_sub_board_wa(s2rp910);
	if (ret < 0)
		return ret;

	ret = s2rp910_set_exynos_func(s2rp910_sdev);
	if (ret < 0)
		return ret;

	s2rp910_set_base_addr(s2rp910);

	i2c_set_clientdata(i2c, s2rp910);

	ret = s2rp910_get_rev_id(s2rp910);
	if (ret < 0)
		return ret;

	ret = s2rp910_irq_init(s2rp910);
	if (ret < 0)
		return ret;

	ret = s2rp910_get_mfd_cell(s2rp910, device_type);
	if (ret < 0)
		return ret;

	ret = devm_mfd_add_devices(s2rp910->dev, 1, s2rp910->cell, s2rp910->cell_size, NULL, 0, NULL);
	if (ret < 0)
		return ret;

	dev_info(&i2c->dev, "[RF%d_PMIC] %s: end\n", device_type + 1, __func__);

	return ret;
}

static void s2rp910_i2c_remove(struct i2c_client *i2c)
{
	struct s2rp910_dev *s2rp910 = i2c_get_clientdata(i2c);

	s2p_destroy_workqueue(s2rp910->sdev->irq_list->irqs, s2rp910->sdev->irq_list->irq_type_cnt);
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_irq_sysfs_entries(s2rp910->sdev->irq_list->irq_sysfs_dev);
#endif
	s2p_remove(s2rp910->sdev);
}

static const struct i2c_device_id s2rp910_i2c_id[] = {
	{ "s2rp910-1", TYPE_S2RP910_1 },
	{ "s2rp910-2", TYPE_S2RP910_2 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2rp910_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2rp910_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2rp910_1_mfd", .data = (void *)TYPE_S2RP910_1 },
	{ .compatible = "samsung,s2rp910_2_mfd", .data = (void *)TYPE_S2RP910_2 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2rp910_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2rp910_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2rp910_i2c_dt_ids,
#endif /* CONFIG_OF */
		.suppress_bind_attrs = true,
	},
	.probe		= s2rp910_i2c_probe,
	.remove		= s2rp910_i2c_remove,
	.id_table	= s2rp910_i2c_id,
};

static int __init s2rp910_i2c_init(void)
{
	pr_info("[PMIC] %s: %s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2rp910_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2rp910_i2c_init);

static void __exit s2rp910_i2c_exit(void)
{
	i2c_del_driver(&s2rp910_i2c_driver);
}
module_exit(s2rp910_i2c_exit);

MODULE_DESCRIPTION("s2rp910 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
