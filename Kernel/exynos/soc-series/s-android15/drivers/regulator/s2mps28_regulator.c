/*
 * s2mps28_regulator.c
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
#include <linux/pmic/s2mps28-mfd.h>
#include <linux/pmic/s2mps28-regulator.h>
#include <linux/pmic/s2p_regulator.h>
#include <linux/pmic/pmic_class.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#endif

static struct s2p_regulator_info *s2mps28_static_info[TYPE_S2MPS28_NR];

static unsigned int s2mps28_of_map_mode(unsigned int f_mode)
{
	return s2p_of_map_mode(f_mode);
}

static int s2mps28_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2MPS28_BUCK1 <= id && id <= S2MPS28_BUCK_SR1))
		return -EINVAL;

	ret = s2p_set_mode(rdev, pmic_info,  mode);

	return ret;
}

static unsigned int s2mps28_get_mode(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2MPS28_BUCK1 <= id && id <= S2MPS28_BUCK_SR1))
		return REGULATOR_MODE_INVALID;

	ret = s2p_get_mode(rdev, pmic_info);

	return ret;
}

static int s2mps28_enable(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_enable(pmic_info, rdev);
}

static int s2mps28_disable_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_disable_regmap(pmic_info, rdev);
}

static int s2mps28_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_is_enabled_regmap(pmic_info, rdev);
}

static int s2mps28_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_get_voltage_sel_regmap(pmic_info, rdev);
}

static int s2mps28_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_set_voltage_sel_regmap(pmic_info, rdev, sel);
}

static int s2mps28_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_set_voltage_time_sel(pmic_info, rdev, old_selector, new_selector);
}

static int s2mps28_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	return 0;
}

#define _REGULATOR_OPS(num)	s2mps28_regulator_ops##num
static struct regulator_ops s2mps28_regulator_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2mps28_is_enabled_regmap,
	.enable			= s2mps28_enable,
	.disable		= s2mps28_disable_regmap,
	.get_voltage_sel	= s2mps28_get_voltage_sel_regmap,
	.set_voltage_sel	= s2mps28_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2mps28_set_voltage_time_sel,
	.set_mode		= s2mps28_set_mode,
	.get_mode		= s2mps28_get_mode,
	.set_ramp_delay		= s2mps28_set_ramp_delay,
};

#define REGULATORS_DEFINE(x)	{ ARRAY_SIZE(x), x }
static struct s2p_pmic_regulators_desc regulators[] = {
	REGULATORS_DEFINE(s2mps28_1_regulators),
	REGULATORS_DEFINE(s2mps28_2_regulators),
	REGULATORS_DEFINE(s2mps28_3_regulators),
	REGULATORS_DEFINE(s2mps28_4_regulators),
	REGULATORS_DEFINE(s2mps28_5_regulators),
};

#if IS_ENABLED(CONFIG_EXYNOS_AFM) || IS_ENABLED(CONFIG_NPU_AFM)
/*
 * @sub_no : Sub Pmic number. If sub_no is 2, it is sub pmic2.
 */
int sub_pmic_afm_read_reg(uint32_t sub_no, uint8_t reg, uint8_t *val)
{
	uint32_t dev_type = 0;
	struct s2p_regulator_info *pmic_info = NULL;
	struct s2p_regulator_bus *bus_info = NULL;

	if (sub_no < 1 || sub_no > 5) {
		pr_err("%s: sub_no:(%d) is out of range\n", __func__, sub_no);
		return -ENODEV;
	}

	dev_type = sub_no - 1;
	pmic_info = s2mps28_static_info[dev_type];
	bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, S2MPS28_PM1_ADDR, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub_pmic_afm_read_reg);

int sub_pmic_afm_update_reg(uint32_t sub_no, uint8_t reg, uint8_t val, uint8_t mask)
{
	uint32_t dev_type = 0;
	struct s2p_regulator_info *pmic_info = NULL;
	struct s2p_regulator_bus *bus_info = NULL;

	if (sub_no < 1 || sub_no > 5) {
		pr_err("%s: sub_no:(%d) is out of range\n", __func__, sub_no);
		return -ENODEV;
	}

	dev_type = sub_no - 1;
	pmic_info = s2mps28_static_info[dev_type];
	bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, S2MPS28_PM1_ADDR, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub_pmic_afm_update_reg);

bool chk_sub_pmic_info(uint32_t sub_no)
{
	struct s2p_regulator_info *pmic_info = NULL;
	uint32_t dev_type = 0;

	if (sub_no < 1 || sub_no > 5) {
		pr_err("%s: sub_no(%d) is out of range\n", __func__, sub_no);
		return false;
	}

	dev_type = sub_no - 1;
	pmic_info = s2mps28_static_info[dev_type];

	return (pmic_info) ? true : false;
}
EXPORT_SYMBOL_GPL(chk_sub_pmic_info);

int sub_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_1];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, pmic_info->pm_addr, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub_pmic_read_reg);

int sub_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_1];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, pmic_info->pm_addr, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub_pmic_update_reg);

int sub_pmic_get_i2c(struct i2c_client **i2c)
{
	return 0;
}
EXPORT_SYMBOL_GPL(sub_pmic_get_i2c);

int sub2_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_2];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, pmic_info->pm_addr, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub2_pmic_read_reg);

int sub2_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_2];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, pmic_info->pm_addr, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub2_pmic_update_reg);

int sub2_pmic_get_i2c(struct i2c_client **i2c)
{
	return 0;
}
EXPORT_SYMBOL_GPL(sub2_pmic_get_i2c);

int sub3_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_3];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, pmic_info->pm_addr, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub3_pmic_read_reg);

int sub3_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_3];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, pmic_info->pm_addr, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub3_pmic_update_reg);

int sub3_pmic_get_i2c(struct i2c_client **i2c)
{
	return 0;
}
EXPORT_SYMBOL_GPL(sub3_pmic_get_i2c);

int sub4_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_4];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, pmic_info->pm_addr, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub4_pmic_read_reg);

int sub4_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_4];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, pmic_info->pm_addr, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub4_pmic_update_reg);

int sub4_pmic_get_i2c(struct i2c_client **i2c)
{
	return 0;
}
EXPORT_SYMBOL_GPL(sub4_pmic_get_i2c);

int sub5_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_5];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, pmic_info->pm_addr, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub5_pmic_read_reg);

int sub5_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2p_regulator_info *pmic_info = s2mps28_static_info[TYPE_S2MPS28_5];
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, pmic_info->pm_addr, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(sub5_pmic_update_reg);

int sub5_pmic_get_i2c(struct i2c_client **i2c)
{
	return 0;
}
EXPORT_SYMBOL_GPL(sub5_pmic_get_i2c);
#endif

static void s2mps28_wtsr_enable(struct s2p_regulator_info *pmic_info)
{
	int ret, dev_type = pmic_info->device_type;

	pr_info("[SUB%d_PMIC] %s: enable WTSR\n", dev_type + 1, __func__);

	ret = s2p_regulator_update_reg(pmic_info->bus_info, S2MPS28_PM1_ADDR,
			S2MPS28_PM1_CFG_PM, S2MPS28_WTSREN_MASK, S2MPS28_WTSREN_MASK);
	if (ret < 0)
		pr_info("[SUB%d_PMIC] %s: fail to update WTSR reg(%d)\n",
						dev_type + 1, __func__, ret);
	pmic_info->wtsr_en = true;
}

static void s2mps28_wtsr_disable(struct s2p_regulator_info *pmic_info)
{
	int ret, dev_type = pmic_info->device_type;

	pr_info("[SUB%d_PMIC] %s: disable WTSR\n", dev_type + 1, __func__);

	ret = s2p_regulator_update_reg(pmic_info->bus_info, S2MPS28_PM1_ADDR,
			S2MPS28_PM1_CFG_PM, 0x0, S2MPS28_WTSREN_MASK);
	if (ret < 0)
		pr_info("[SUB%d_PMIC] %s: fail to update WTSR reg(%d)\n",
					dev_type + 1, __func__, ret);
}

static int s2mps28_base_address(uint8_t base_addr)
{
	switch (base_addr) {
	case S2MPS28_COM_ADDR:
	case S2MPS28_PM1_ADDR:
	case S2MPS28_PM2_ADDR:
	case S2MPS28_PM3_ADDR:
	case S2MPS28_GPIO_ADDR:
	case S2MPS28_EXT_ADDR:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return -EINVAL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps28_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata,
					 struct s2p_regulator_info *pmic_info)
{
	struct device_node *pmic_np = NULL;
	struct s2p_pmic_regulators_desc *regulators_ptr = NULL;
	int dev_type = pmic_info->device_type;
	int ret = 0;
	uint32_t val = 0;

	dev_info(dev, "[SUB%d_PMIC] %s: start\n", dev_type + 1, __func__);

	pmic_np = dev->of_node;
	if (!pmic_np) {
		dev_err(dev, "could not find pmic sub%d-node\n", dev_type + 1);
		return -ENODEV;
	}

	ret = of_property_read_u32(pmic_np, "wtsr_en", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_en = !!val;

	regulators_ptr = &regulators[dev_type];

	ret = of_s2p_regulator_parse_dt(dev, pdata, regulators_ptr, pmic_info);

	dev_info(dev, "[SUB%d_PMIC] %s: end\n", dev_type + 1, __func__);

	return 0;
}
#else
static int of_s2mps28_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata,
					 struct s2p_regulator_info *pmic_info)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mps28_init_regulators_info(struct s2p_regulator_info *pmic_info,
					struct s2p_pmic_data *pdata)
{
	pmic_info->num_regulators = pdata->num_rdata;

	if (pdata->wtsr_en)
		s2mps28_wtsr_enable(pmic_info);

	return 0;
}

static void s2mps28_init_regulators_ops(struct s2p_pmic_regulators_desc *_regulators)
{
	uint32_t i;

	for (i = 0; i < _regulators->count; i++) {
		_regulators->desc[i].ops = &_REGULATOR_OPS();
		_regulators->desc[i].of_map_mode = s2mps28_of_map_mode;
	}
}

static int s2mps28_init_bus(struct s2p_regulator_info *pmic_info)
{
	struct s2p_regulator_bus *bus_info = NULL;

	bus_info = s2p_init_bus(pmic_info->dev);
	if (!PTR_ERR(bus_info))
		return -ENOMEM;

#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	bus_info->read_reg = exynos_acpm_read_reg;
	bus_info->bulk_read = exynos_acpm_bulk_read;
	bus_info->write_reg = exynos_acpm_write_reg;
	bus_info->bulk_write = exynos_acpm_bulk_write;
	bus_info->update_reg = exynos_acpm_update_reg;
#endif
	pmic_info->bus_info = bus_info;

	return 0;
}

static int s2mps28_pmic_probe(struct i2c_client *i2c)
{
	struct s2p_pmic_data *pdata = NULL;
	struct s2p_regulator_info *pmic_info = NULL;
	const int dev_type = (enum s2mps28_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: start\n", dev_type + 1, __func__);

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2p_pmic_data), GFP_KERNEL);
	if (!pdata)
		return -ENODEV;

	pmic_info = devm_kzalloc(&i2c->dev, sizeof(struct s2p_regulator_info), GFP_KERNEL);
	if (!pmic_info)
		return -ENOMEM;

	pmic_info->dev = &i2c->dev;
	pmic_info->device_type = dev_type;
	pmic_info->rdesc = &regulators[dev_type];
	pmic_info->pm_addr = S2MPS28_PM1_ADDR;
	pmic_info->enable_shift_bit = S2MPS28_ENABLE_SHIFT;
	pmic_info->regulator_num = S2MPS28_REG_MAX;

	ret = s2mps28_init_bus(pmic_info);
	if (ret < 0)
		return ret;

	s2mps28_init_regulators_ops(&regulators[dev_type]);

	ret = of_s2mps28_regulator_parse_dt(&i2c->dev, pdata, pmic_info);
	if (ret < 0)
		goto err_bus_info;

	s2mps28_static_info[dev_type] = pmic_info;

	i2c_set_clientdata(i2c, pmic_info);

	ret = s2p_register_regulators(pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	ret = s2mps28_init_regulators_info(pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	//exynos_reboot_register_pmic_ops(NULL, s2mps28_power_off_wa, NULL, NULL);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	pmic_info->check_base_address = s2mps28_base_address;
	if (!pmic_info->check_base_address) {
		pr_err("%s: Invalid base check function\n", __func__);
		goto err_bus_info;
	}

	snprintf(pmic_info->sysfs_name, sizeof(pmic_info->sysfs_name) - 1, "%s",
			MFD_DEV_NAME);

	ret = s2p_create_sysfs(pmic_info);
	if (ret < 0) {
		pr_err("%s: s2p_create_sysfs fail\n", __func__);
		goto err_bus_info;
	}
#endif

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: end\n", dev_type + 1, __func__);

	return 0;

err_bus_info:
	mutex_destroy(&pmic_info->bus_info->bus_lock);

	return ret;
}

static void s2mps28_pmic_remove(struct i2c_client *i2c)
{
	struct s2p_regulator_info *pmic_info = i2c_get_clientdata(i2c);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_sysfs_entries(pmic_info->pmic_sysfs->dev);
#endif

	s2p_pmic_remove(pmic_info);
}

static void s2mps28_pmic_shutdown(struct i2c_client *i2c)
{
	struct s2p_regulator_info *pmic_info = i2c_get_clientdata(i2c);

	/* disable WTSR */
	if (pmic_info->wtsr_en)
		s2mps28_wtsr_disable(pmic_info);
}

#if IS_ENABLED(CONFIG_PM)
static int s2mps28_pmic_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int s2mps28_pmic_resume(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}
#else
#define s2mps28_pmic_suspend	NULL
#define s2mps28_pmic_resume	NULL
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(s2mps28_pmic_pm, s2mps28_pmic_suspend, s2mps28_pmic_resume);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps28_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps28_1_regulator", .data = (void*)TYPE_S2MPS28_1 },
	{ .compatible = "samsung,s2mps28_2_regulator", .data = (void*)TYPE_S2MPS28_2 },
	{ .compatible = "samsung,s2mps28_3_regulator", .data = (void*)TYPE_S2MPS28_3 },
	{ .compatible = "samsung,s2mps28_4_regulator", .data = (void*)TYPE_S2MPS28_4 },
	{ .compatible = "samsung,s2mps28_5_regulator", .data = (void*)TYPE_S2MPS28_5 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mps28_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mps28_pmic_driver = {
	.driver = {
		.name = "s2mps28-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2mps28_pmic_pm,
#endif
		.suppress_bind_attrs = true,
		.of_match_table = of_match_ptr(s2mps28_i2c_dt_ids),
	},
	.probe = s2mps28_pmic_probe,
	.remove = s2mps28_pmic_remove,
	.shutdown = s2mps28_pmic_shutdown,
};

static int __init s2mps28_regulator_i2c_init(void)
{
	pr_info("%s:%s\n", REGULATOR_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps28_pmic_driver);
}
subsys_initcall(s2mps28_regulator_i2c_init);

static void __exit s2mps28_regulator_i2c_exit(void)
{
	i2c_del_driver(&s2mps28_pmic_driver);
}
module_exit(s2mps28_regulator_i2c_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPS28 Regulator Driver");
MODULE_LICENSE("GPL");
