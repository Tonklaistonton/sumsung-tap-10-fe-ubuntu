/*
 * s2se911_regulator.c
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
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
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/pmic/s2se911-mfd.h>
#include <linux/pmic/s2se911-regulator.h>
#include <linux/pmic/s2p_regulator.h>
#include <linux/io.h>
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif

static struct s2p_regulator_info *s2se911_static_info[TYPE_S2SE911_NR];

static int s2se911_get_base_addr(struct regulator_dev *rdev)
{
	int id = rdev_get_id(rdev);

	switch (id) {
	case S2SE911_BUCK1 ... S2SE911_BUCK_SR1:
		return S2SE911_BUCK_ADDR;
	case S2SE911_LDO1 ... S2SE911_LDO6:
		return S2SE911_LDO_ADDR;
	default:
		return -EINVAL;
	}
}

static void s2se911_change_pm_addr(struct s2p_regulator_info *s2se911_pmic_info, int base_addr)
{
	mutex_lock(&s2se911_pmic_info->pm_lock);
	s2se911_pmic_info->pm_addr = base_addr;
	mutex_unlock(&s2se911_pmic_info->pm_lock);
}

static int s2se911_set_pm_addr(struct s2p_regulator_info *s2se911_pmic_info, struct regulator_dev *rdev)
{
	int ret = 0, base_addr = s2se911_get_base_addr(rdev);

	if (base_addr < 0) {
		dev_err(s2se911_pmic_info->dev, "%s: failed to get base addr(%d)\n", __func__, base_addr);
		return -EINVAL;
	}
	s2se911_change_pm_addr(s2se911_pmic_info, base_addr);

	return ret;
}

static unsigned int s2se911_of_map_mode(unsigned int f_mode)
{
	return s2p_of_map_mode(f_mode);
}

static int s2se911_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2SE911_BUCK1 <= id && id <= S2SE911_BUCK_SR1))
		return -EINVAL;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		return ret;

	return s2p_set_mode(rdev, s2se911_pmic_info,  mode);
}

static unsigned int s2se911_get_mode(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2SE911_BUCK1 <= id && id <= S2SE911_BUCK_SR1))
		goto invalid;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		goto invalid;

	return s2p_get_mode(rdev, s2se911_pmic_info);
invalid:
	return REGULATOR_MODE_INVALID;
}

static int s2se911_enable(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		return ret;

	return s2p_enable(s2se911_pmic_info, rdev);
}

static int s2se911_disable_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		return ret;

	return s2p_disable_regmap(s2se911_pmic_info, rdev);
}

static int s2se911_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		return ret;

	return s2p_is_enabled_regmap(s2se911_pmic_info, rdev);
}

static int s2se911_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		return ret;

	return s2p_get_voltage_sel_regmap(s2se911_pmic_info, rdev);
}

static int s2se911_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		return ret;

	return s2p_set_voltage_sel_regmap(s2se911_pmic_info, rdev, sel);
}

static int s2se911_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	struct s2p_regulator_info *s2se911_pmic_info = rdev_get_drvdata(rdev);
	int ret = 0;

	ret = s2se911_set_pm_addr(s2se911_pmic_info, rdev);
	if (ret < 0)
		return ret;

	return s2p_set_voltage_time_sel(s2se911_pmic_info, rdev, old_selector, new_selector);
}

static int s2se911_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	return 0;
}

#define _REGULATOR_OPS(num)	s2se911_regulator_ops##num
static struct regulator_ops s2se911_regulator_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2se911_is_enabled_regmap,
	.enable			= s2se911_enable,
	.disable		= s2se911_disable_regmap,
	.get_voltage_sel	= s2se911_get_voltage_sel_regmap,
	.set_voltage_sel	= s2se911_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2se911_set_voltage_time_sel,
	.set_mode		= s2se911_set_mode,
	.get_mode		= s2se911_get_mode,
	.set_ramp_delay		= s2se911_set_ramp_delay,
};

#define REGULATORS_DEFINE(x)	{ ARRAY_SIZE(x), x }
static struct s2p_pmic_regulators_desc regulators[] = {
	REGULATORS_DEFINE(s2se911_1_regulators),
	REGULATORS_DEFINE(s2se911_2_regulators),
	REGULATORS_DEFINE(s2se911_3_regulators),
	REGULATORS_DEFINE(s2se911_4_regulators),
	REGULATORS_DEFINE(s2se911_5_regulators),
};

#if IS_ENABLED(CONFIG_EXYNOS_AFM) || IS_ENABLED(CONFIG_NPU_AFM)
/*
 * @sub_no : Sub Pmic number. If sub_no is 2, it is sub pmic2.
 */
static int sub_pmic_afm_init_val(struct s2p_regulator_bus **bus_info, const uint32_t sub_no)
{
	int dev_type = sub_no - 1;

	if (sub_no < 1 || sub_no > 5) {
		pr_err("%s: sub_no:(%u) is out of range\n", __func__, sub_no);
		return -ENODEV;
	}

	*bus_info = s2se911_static_info[dev_type]->bus_info;

	return 0;
}

int sub_pmic_afm_read_reg(uint32_t sub_no, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_bus *bus_info = NULL;
	int ret = 0;

	ret = sub_pmic_afm_init_val(&bus_info, sub_no);
	if (ret < 0)
		return ret;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, S2SE911_BUCK_ADDR, reg, val) : -EINVAL;
}
EXPORT_SYMBOL_GPL(sub_pmic_afm_read_reg);

int sub_pmic_afm_update_reg(uint32_t sub_no, uint8_t reg, uint8_t val, uint8_t mask)
{
	struct s2p_regulator_bus *bus_info = NULL;
	int ret = 0;

	ret = sub_pmic_afm_init_val(&bus_info, sub_no);
	if (ret < 0)
		return ret;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, S2SE911_BUCK_ADDR, reg, val, mask) : -EINVAL;
}
EXPORT_SYMBOL_GPL(sub_pmic_afm_update_reg);

bool chk_sub_pmic_info(uint32_t sub_no)
{
	uint32_t dev_type = 0;

	if (sub_no < 1 || sub_no > 5) {
		pr_err("%s: sub_no(%d) is out of range\n", __func__, sub_no);
		return false;
	}

	dev_type = sub_no - 1;

	return (s2se911_static_info[dev_type]) ? true : false;
}
EXPORT_SYMBOL_GPL(chk_sub_pmic_info);
#endif

static int s2se911_wtsr_enable(struct s2p_regulator_info *s2se911_pmic_info)
{
	int ret = 0, dev_type = s2se911_pmic_info->device_type;

	ret = s2p_regulator_update_reg(s2se911_pmic_info->bus_info, S2SE911_PM1_ADDR, S2SE911_PM1_CFG_PM,
			S2SE911_WTSREN_MASK, S2SE911_WTSREN_MASK);
	if (ret < 0) {
		pr_info("[SUB%d_PMIC] %s: fail to update WTSR reg(%d)\n",
						dev_type + 1, __func__, ret);
		return ret;
	}

	s2se911_pmic_info->wtsr_en = true;
	pr_info("[SUB%d_PMIC] %s: enable WTSR\n", dev_type + 1, __func__);

	return ret;
}

static int s2se911_wtsr_disable(struct s2p_regulator_info *s2se911_pmic_info)
{
	int ret = 0, dev_type = s2se911_pmic_info->device_type;

	ret = s2p_regulator_update_reg(s2se911_pmic_info->bus_info, S2SE911_PM1_ADDR, S2SE911_PM1_CFG_PM,
				0x0, S2SE911_WTSREN_MASK);
	if (ret < 0) {
		pr_info("[SUB%d_PMIC] %s: fail to update WTSR reg(%d)\n",
					dev_type + 1, __func__, ret);
		return ret;
	}

	s2se911_pmic_info->wtsr_en = false;
	pr_info("[SUB%d_PMIC] %s: disable WTSR\n", dev_type + 1, __func__);

	return ret;
}

static int s2se911_base_address(uint8_t base_addr)
{
	switch (base_addr) {
	case S2SE911_VGPIO_ADDR:
	case S2SE911_COM_ADDR:
	case S2SE911_PM1_ADDR:
	case S2SE911_PM2_ADDR:
	case S2SE911_PM3_ADDR:
	case S2SE911_BUCK_ADDR:
	case S2SE911_LDO_ADDR:
	case S2SE911_GPIO_ADDR:
	case S2SE911_EXT_ADDR:
	case S2SE911_BUCK_TRIM_ADDR ... S2SE911_BUCK_SR_TRIM_ADDR:
	case S2SE911_LDO_TRIM_ADDR:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return -EINVAL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static int of_s2se911_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata,
					 struct s2p_regulator_info *s2se911_pmic_info)
{
	struct device_node *pmic_np = NULL;
	struct s2p_pmic_regulators_desc *regulators_ptr = NULL;
	int dev_type = s2se911_pmic_info->device_type;
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

	ret = of_s2p_regulator_parse_dt(dev, pdata, regulators_ptr, s2se911_pmic_info);

	dev_info(dev, "[SUB%d_PMIC] %s: end\n", dev_type + 1, __func__);

	return 0;
}
#else
static int of_s2se911_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata,
					 struct s2p_regulator_info *s2se911_pmic_info)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2se911_init_regulators_info(struct s2p_regulator_info *s2se911_pmic_info,
					struct s2p_pmic_data *pdata)
{
	int ret = 0;

	s2se911_pmic_info->wtsr_en = false;
	s2se911_pmic_info->num_regulators = pdata->num_rdata;

	if (pdata->wtsr_en)
		ret = s2se911_wtsr_enable(s2se911_pmic_info);

	return ret;
}

static void s2se911_init_regulators_ops(struct s2p_pmic_regulators_desc *s2se911_regulators)
{
	uint32_t i = 0;

	for (i = 0; i < s2se911_regulators->count; i++) {
		s2se911_regulators->desc[i].ops = &_REGULATOR_OPS();
		s2se911_regulators->desc[i].of_map_mode = s2se911_of_map_mode;
	}
}

static int s2se911_init_bus(struct s2p_regulator_info *s2se911_pmic_info)
{
	struct s2p_regulator_bus *s2se911_bus_info = NULL;

	s2se911_bus_info = s2p_init_bus(s2se911_pmic_info->dev);
	if (!PTR_ERR(s2se911_bus_info))
		return -ENOMEM;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	s2se911_bus_info->read_reg = exynos_esca_read_reg;
	s2se911_bus_info->bulk_read = exynos_esca_bulk_read;
	s2se911_bus_info->write_reg = exynos_esca_write_reg;
	s2se911_bus_info->bulk_write = exynos_esca_bulk_write;
	s2se911_bus_info->update_reg = exynos_esca_update_reg;
#endif
	s2se911_pmic_info->bus_info = s2se911_bus_info;

	return 0;
}

static int s2se911_pmic_probe(struct i2c_client *i2c)
{
	struct s2p_pmic_data *pdata = NULL;
	struct s2p_regulator_info *s2se911_pmic_info = NULL;
	const int dev_type = (enum s2se911_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: start\n", dev_type + 1, __func__);

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2p_pmic_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	s2se911_pmic_info = devm_kzalloc(&i2c->dev, sizeof(struct s2p_regulator_info), GFP_KERNEL);
	if (!s2se911_pmic_info)
		return -ENOMEM;

	s2se911_pmic_info->dev = &i2c->dev;
	s2se911_pmic_info->device_type = dev_type;
	s2se911_pmic_info->rdesc = &regulators[dev_type];
	s2se911_pmic_info->pm_addr = S2SE911_PM1_ADDR;
	s2se911_pmic_info->enable_shift_bit = S2SE911_ENABLE_SHIFT;
	s2se911_pmic_info->regulator_num = S2SE911_REG_MAX;

	ret = s2se911_init_bus(s2se911_pmic_info);
	if (ret < 0)
		return ret;

	s2se911_init_regulators_ops(&regulators[dev_type]);
	mutex_init(&s2se911_pmic_info->pm_lock);

	ret = of_s2se911_regulator_parse_dt(&i2c->dev, pdata, s2se911_pmic_info);
	if (ret < 0)
		goto err_bus_info;

	ret = s2p_init_src_info(s2se911_pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	s2se911_static_info[dev_type] = s2se911_pmic_info;

	i2c_set_clientdata(i2c, s2se911_pmic_info);

	ret = s2p_register_regulators(s2se911_pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	ret = s2se911_init_regulators_info(s2se911_pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	//exynos_reboot_register_pmic_ops(NULL, s2se911_power_off_wa, NULL, NULL);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2se911_pmic_info->check_base_address = s2se911_base_address;
	if (!s2se911_pmic_info->check_base_address) {
		pr_err("%s: Invalid base check function\n", __func__);
		goto err_bus_info;
	}

	snprintf(s2se911_pmic_info->sysfs_name, sizeof(s2se911_pmic_info->sysfs_name) - 1, "%s-%d",
			MFD_DEV_NAME, dev_type + 1);

	ret = s2p_create_sysfs(s2se911_pmic_info);
	if (ret < 0) {
		pr_err("%s: s2p_create_sysfs fail\n", __func__);
		goto err_bus_info;
	}
#endif

	dev_info(&i2c->dev, "[SUB%d_PMIC] %s: end\n", dev_type + 1, __func__);

	return 0;

err_bus_info:
	mutex_destroy(&s2se911_pmic_info->bus_info->bus_lock);
	mutex_destroy(&s2se911_pmic_info->pm_lock);

	return ret;
}

static void s2se911_pmic_remove(struct i2c_client *i2c)
{
	struct s2p_regulator_info *s2se911_pmic_info = i2c_get_clientdata(i2c);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_sysfs_entries(s2se911_pmic_info->pmic_sysfs->dev);
#endif

	s2p_pmic_remove(s2se911_pmic_info);
}

static void s2se911_pmic_shutdown(struct i2c_client *i2c)
{
	struct s2p_regulator_info *s2se911_pmic_info = i2c_get_clientdata(i2c);

	if (s2se911_pmic_info->wtsr_en)
		s2se911_wtsr_disable(s2se911_pmic_info);
}

#if IS_ENABLED(CONFIG_PM)
static int s2se911_pmic_suspend(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}

static int s2se911_pmic_resume(struct device *dev)
{
	pr_info("%s\n", __func__);

	return 0;
}
#else
#define s2se911_pmic_suspend	NULL
#define s2se911_pmic_resume	NULL
#endif /* CONFIG_PM */

static SIMPLE_DEV_PM_OPS(s2se911_pmic_pm, s2se911_pmic_suspend, s2se911_pmic_resume);

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2se911_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2se911_1_regulator", .data = (void*)TYPE_S2SE911_1 },
	{ .compatible = "samsung,s2se911_2_regulator", .data = (void*)TYPE_S2SE911_2 },
	{ .compatible = "samsung,s2se911_3_regulator", .data = (void*)TYPE_S2SE911_3 },
	{ .compatible = "samsung,s2se911_4_regulator", .data = (void*)TYPE_S2SE911_4 },
	{ .compatible = "samsung,s2se911_5_regulator", .data = (void*)TYPE_S2SE911_5 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2se911_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2se911_pmic_driver = {
	.driver = {
		.name = "s2se911-regulator",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm = &s2se911_pmic_pm,
#endif
		.suppress_bind_attrs = true,
		.of_match_table = of_match_ptr(s2se911_i2c_dt_ids),
	},
	.probe = s2se911_pmic_probe,
	.remove = s2se911_pmic_remove,
	.shutdown = s2se911_pmic_shutdown,
};

static int __init s2se911_regulator_i2c_init(void)
{
	pr_info("%s:%s\n", REGULATOR_DEV_NAME, __func__);
	return i2c_add_driver(&s2se911_pmic_driver);
}
subsys_initcall(s2se911_regulator_i2c_init);

static void __exit s2se911_regulator_i2c_exit(void)
{
	i2c_del_driver(&s2se911_pmic_driver);
}
module_exit(s2se911_regulator_i2c_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2SE911 Regulator Driver");
MODULE_LICENSE("GPL");
