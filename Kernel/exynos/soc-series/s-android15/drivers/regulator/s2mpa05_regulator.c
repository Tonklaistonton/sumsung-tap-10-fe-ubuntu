/*
 * s2mpa05_regulator.c
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/pmic/s2mpa05-mfd.h>
#include <linux/pmic/s2mpa05-regulator.h>
#include <linux/pmic/s2p_regulator.h>
#include <linux/pmic/pmic_class.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#elif IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif

static struct s2p_regulator_info *s2mpa05_static_info;

/* TODO: change on mode related ops needed!! */
static unsigned int s2mpa05_of_map_mode(unsigned int f_mode)
{
	return s2p_of_map_mode(f_mode);
}

static int s2mpa05_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2MPA05_BUCK1 <= id && id <= S2MPA05_BUCK4))
		return -EINVAL;

	ret = s2p_set_mode(rdev, s2mpa05_pmic_info, mode);

	return ret;
}

static unsigned int s2mpa05_get_mode(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2MPA05_BUCK1 <= id && id <= S2MPA05_BUCK4))
		return REGULATOR_MODE_INVALID;

	ret = s2p_get_mode(rdev, s2mpa05_pmic_info);

	return ret;
}

static int s2mpa05_enable(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);

	return s2p_enable(s2mpa05_pmic_info, rdev);
}

static int s2mpa05_disable_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);

	return s2p_disable_regmap(s2mpa05_pmic_info, rdev);
}

static int s2mpa05_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);

	return s2p_is_enabled_regmap(s2mpa05_pmic_info, rdev);
}

static int s2mpa05_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);

	return s2p_get_voltage_sel_regmap(s2mpa05_pmic_info, rdev);
}

static int s2mpa05_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);

	return s2p_set_voltage_sel_regmap(s2mpa05_pmic_info, rdev, sel);
}

static int s2mpa05_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = rdev_get_drvdata(rdev);

	return s2p_set_voltage_time_sel(s2mpa05_pmic_info, rdev, old_selector, new_selector);
}

static int s2mpa05_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	return 0;
}

#define _REGULATOR_OPS(num)	s2mpa05_regulator_ops##num
static struct regulator_ops s2mpa05_regulator_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2mpa05_is_enabled_regmap,
	.enable			= s2mpa05_enable,
	.disable		= s2mpa05_disable_regmap,
	.get_voltage_sel	= s2mpa05_get_voltage_sel_regmap,
	.set_voltage_sel	= s2mpa05_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2mpa05_set_voltage_time_sel,
	.set_mode		= s2mpa05_set_mode,
	.get_mode		= s2mpa05_get_mode,
	.set_ramp_delay		= s2mpa05_set_ramp_delay,
};

#define REGULATORS_DEFINE(x)	{ ARRAY_SIZE(x), x }
static struct s2p_pmic_regulators_desc regulators[] = {
	REGULATORS_DEFINE(s2mpa05_regulators),
};

//TODO: change afm api, e.g) call back func
//#if IS_ENABLED(CONFIG_EXYNOS_AFM)
//int ext_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
//{
//	struct s2p_regulator_info *s2mpa05_pmic_info = s2mpa05_static_info;
//	struct s2p_dev *sdev = s2mpa05_pmic_info->sdev;
//
//	return (sdev) ? s2p_read_reg(sdev, s2mpa05_pmic_info->pm_addr, reg, val) : -ENODEV;
//}
//EXPORT_SYMBOL_GPL(ext_pmic_read_reg);
//
//int ext_pmic_update_reg(struct i2c_client *i2c, uint8_t reg, uint8_t val, uint8_t mask)
//{
//	struct s2p_regulator_info *s2mpa05_pmic_info = s2mpa05_static_info;
//	struct s2p_dev *sdev = s2mpa05_pmic_info->sdev;
//
//	return (sdev) ? s2p_update_reg(sdev, s2mpa05_pmic_info->pm_addr, reg, val, mask) : -ENODEV;
//}
//EXPORT_SYMBOL_GPL(ext_pmic_update_reg);
//
//int ext_pmic_get_i2c(struct i2c_client **i2c)
//{
//	return 0;
//}
//EXPORT_SYMBOL_GPL(ext_pmic_get_i2c);
//#endif

static int s2mpa05_wtsr_enable(struct s2p_regulator_info *s2mpa05_pmic_info)
{
	int ret = 0;

	ret = s2p_regulator_update_reg(s2mpa05_pmic_info->bus_info, s2mpa05_pmic_info->pm_addr,
			S2MPA05_PM1_CFG_PM, S2MPA05_WTSREN_MASK, S2MPA05_WTSREN_MASK);
	if (ret < 0) {
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
		return ret;
	}

	s2mpa05_pmic_info->wtsr_en = true;
	pr_info("[EXT PMIC] %s: enable WTSR\n", __func__);

	return ret;
}

static int s2mpa05_wtsr_disable(struct s2p_regulator_info *s2mpa05_pmic_info)
{
	int ret = 0;

	ret = s2p_regulator_update_reg(s2mpa05_pmic_info->bus_info, s2mpa05_pmic_info->pm_addr,
			S2MPA05_PM1_CFG_PM, 0x00, S2MPA05_WTSREN_MASK);
	if (ret < 0) {
		pr_info("%s: fail to update WTSR reg(%d)\n", __func__, ret);
		return ret;
	}

	s2mpa05_pmic_info->wtsr_en = false;
	pr_info("[EXT PMIC] %s: disable WTSR\n", __func__);

	return ret;
}

static int s2mpa05_base_address(uint8_t base_addr)
{
	switch (base_addr) {
	case S2MPA05_COM_ADDR:
	case S2MPA05_PM1_ADDR:
	case S2MPA05_CLOSE1_ADDR:
	case S2MPA05_GPIO_ADDR:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return -EINVAL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static int of_s2mpa05_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata,
					 struct s2p_regulator_info *s2mpa05_pmic_info)
{
	struct device_node *pmic_np;
	struct s2p_pmic_regulators_desc *regulators_ptr;
	int dev_type = s2mpa05_pmic_info->device_type;
	int ret = 0;
	uint32_t val = 0;

	dev_info(dev, "[EXT_PMIC] %s: start\n", __func__);

	pmic_np = dev->of_node;
	if (!pmic_np) {
		dev_err(dev, "could not find pmic ext-node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_en = !!val;

	regulators_ptr = &regulators[dev_type];

	ret = of_s2p_regulator_parse_dt(dev, pdata, regulators_ptr, s2mpa05_pmic_info);

	dev_info(dev, "[EXT_PMIC] %s: end\n", __func__);

	return 0;
}
#else
static int of_s2mpa05_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata,
					 struct s2p_regulator_info *s2mpa05_pmic_info)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpa05_init_regulators_info(struct s2p_regulator_info *s2mpa05_pmic_info,
					struct s2p_pmic_data *pdata)
{
	int ret = 0;

	s2mpa05_pmic_info->wtsr_en = false;
	if (pdata->wtsr_en)
		ret = s2mpa05_wtsr_enable(s2mpa05_pmic_info);

	return ret;
}

static void s2mpa05_init_regulators_ops(struct s2p_pmic_regulators_desc *s2mpa05_regulators)
{
	uint32_t i = 0;

	for (i = 0; i < s2mpa05_regulators->count; i++) {
		s2mpa05_regulators->desc[i].ops = &_REGULATOR_OPS();
		s2mpa05_regulators->desc[i].of_map_mode = s2mpa05_of_map_mode;
	}
}

static int s2mpa05_init_bus(struct s2p_regulator_info *s2mpa05_pmic_info)
{
	struct s2p_regulator_bus *s2mpa05_bus_info = NULL;

	s2mpa05_bus_info = s2p_init_bus(s2mpa05_pmic_info->dev);
	if (!PTR_ERR(s2mpa05_bus_info))
		return -ENOMEM;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	s2mpa05_bus_info->read_reg = exynos_acpm_read_reg;
	s2mpa05_bus_info->bulk_read = exynos_acpm_bulk_read;
	s2mpa05_bus_info->write_reg = exynos_acpm_write_reg;
	s2mpa05_bus_info->bulk_write = exynos_acpm_bulk_write;
	s2mpa05_bus_info->update_reg = exynos_acpm_update_reg;
#elif IS_ENABLED(CONFIG_EXYNOS_ESCA)
	s2mpa05_bus_info->read_reg = exynos_esca_read_reg;
	s2mpa05_bus_info->bulk_read = exynos_esca_bulk_read;
	s2mpa05_bus_info->write_reg = exynos_esca_write_reg;
	s2mpa05_bus_info->bulk_write = exynos_esca_bulk_write;
	s2mpa05_bus_info->update_reg = exynos_esca_update_reg;
#endif
	s2mpa05_pmic_info->bus_info = s2mpa05_bus_info;

	return 0;
}

static int s2mpa05_pmic_probe(struct i2c_client *i2c)
{
	struct s2p_pmic_data *pdata = NULL;
	struct s2p_regulator_info *s2mpa05_pmic_info = NULL;
	const int dev_type = (enum s2mpa05_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[EXT_PMIC] %s: start\n", __func__);

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2p_pmic_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	s2mpa05_pmic_info = devm_kzalloc(&i2c->dev, sizeof(struct s2p_regulator_info), GFP_KERNEL);
	if (!s2mpa05_pmic_info)
		return -ENOMEM;

	s2mpa05_pmic_info->dev = &i2c->dev;
	s2mpa05_pmic_info->rdesc = &regulators[dev_type];
	s2mpa05_pmic_info->pm_addr = S2MPA05_PM1_ADDR;
	s2mpa05_pmic_info->enable_shift_bit = S2MPA05_ENABLE_SHIFT;
	s2mpa05_pmic_info->regulator_num = S2MPA05_REG_MAX;

	ret = s2mpa05_init_bus(s2mpa05_pmic_info);
	if (ret < 0)
		return ret;

	s2mpa05_init_regulators_ops(&regulators[dev_type]);

	ret = of_s2mpa05_regulator_parse_dt(&i2c->dev, pdata, s2mpa05_pmic_info);
	if (ret < 0)
		goto err_bus_info;

	ret = s2p_init_src_info(s2mpa05_pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	s2mpa05_static_info = s2mpa05_pmic_info;

	i2c_set_clientdata(i2c, s2mpa05_pmic_info);

	ret = s2p_register_regulators(s2mpa05_pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	s2mpa05_pmic_info->num_regulators = pdata->num_rdata;

	ret = s2mpa05_init_regulators_info(s2mpa05_pmic_info, pdata);
	if(ret < 0)
		goto err_bus_info;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2mpa05_pmic_info->check_base_address = s2mpa05_base_address;
	if (!s2mpa05_pmic_info->check_base_address) {
		pr_err("%s: Invalid base check function\n", __func__);
		goto err_bus_info;
	}

	snprintf(s2mpa05_pmic_info->sysfs_name, sizeof(s2mpa05_pmic_info->sysfs_name) - 1, "%s",
			MFD_DEV_NAME);

	ret = s2p_create_sysfs(s2mpa05_pmic_info);
	if (ret < 0) {
		pr_err("%s: s2p_create_sysfs fail\n", __func__);
		goto err_bus_info;
	}
#endif

	dev_info(&i2c->dev, "[EXT_PMIC] %s: end\n", __func__);

	return 0;

err_bus_info:
	mutex_destroy(&s2mpa05_pmic_info->bus_info->bus_lock);

	return ret;
}

static void s2mpa05_pmic_remove(struct i2c_client *i2c)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = i2c_get_clientdata(i2c);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_sysfs_entries(s2mpa05_pmic_info->pmic_sysfs->dev);
#endif

	s2p_pmic_remove(s2mpa05_pmic_info);
}

static void s2mpa05_pmic_shutdown(struct i2c_client *i2c)
{
	struct s2p_regulator_info *s2mpa05_pmic_info = i2c_get_clientdata(i2c);

	if (s2mpa05_pmic_info->wtsr_en)
		s2mpa05_wtsr_disable(s2mpa05_pmic_info);
}

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mpa05_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpa05_regulator", .data = (void*)TYPE_S2MPA05 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mpa05_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mpa05_pmic_driver = {
	.driver = {
		.name = "s2mpa05-regulator",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(s2mpa05_i2c_dt_ids),
#endif /* CONFIG_OF */
	},
	.probe = s2mpa05_pmic_probe,
	.remove = s2mpa05_pmic_remove,
	.shutdown = s2mpa05_pmic_shutdown,
};

static int __init s2mpa05_regulator_i2c_init(void)
{
	pr_info("%s:%s\n", REGULATOR_DEV_NAME, __func__);
	return i2c_add_driver(&s2mpa05_pmic_driver);
}
subsys_initcall(s2mpa05_regulator_i2c_init);

static void __exit s2mpa05_regulator_i2c_exit(void)
{
	i2c_del_driver(&s2mpa05_pmic_driver);
}
module_exit(s2mpa05_regulator_i2c_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPA05 Regulator Driver");
MODULE_LICENSE("GPL");
