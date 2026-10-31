/*
 * s2mps27_regulator.c
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
#include <linux/pmic/s2mps27-mfd.h>
#include <linux/pmic/s2mps27-regulator.h>
#include <linux/pmic/s2p_regulator.h>
#include <linux/pmic/pmic_class.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/reset/exynos/exynos-reset.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include <soc/samsung/acpm_mfd.h>
#endif

static struct s2p_regulator_info *s2mps27_static_info;

static unsigned int s2mps27_of_map_mode(unsigned int f_mode)
{
	return s2p_of_map_mode(f_mode);
}

static int s2mps27_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2MPS27_BUCK_SR1 <= id && id <= S2MPS27_BB))
		return -EINVAL;

	ret = s2p_set_mode(rdev, pmic_info, mode);

	return ret;
}

static unsigned int s2mps27_get_mode(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int ret = 0;

	if (!(S2MPS27_BUCK_SR1 <= id && id <= S2MPS27_BB))
		return REGULATOR_MODE_INVALID;

	ret = s2p_get_mode(rdev, pmic_info);

	return ret;
}

static int s2mps27_enable(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_enable(pmic_info, rdev);
}

static int s2mps27_disable_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_disable_regmap(pmic_info, rdev);
}

static int s2mps27_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_is_enabled_regmap(pmic_info, rdev);
}

static int s2mps27_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_get_voltage_sel_regmap(pmic_info, rdev);
}

static int s2mps27_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_set_voltage_sel_regmap(pmic_info, rdev, sel);
}

static int s2mps27_set_voltage_time_sel(struct regulator_dev *rdev,
				   unsigned int old_selector,
				   unsigned int new_selector)
{
	struct s2p_regulator_info *pmic_info = rdev_get_drvdata(rdev);

	return s2p_set_voltage_time_sel(pmic_info, rdev, old_selector, new_selector);
}

static int s2mps27_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	return 0;
}

#define _REGULATOR_OPS(num)	s2mps27_regulator_ops##num
static struct regulator_ops s2mps27_regulator_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.is_enabled		= s2mps27_is_enabled_regmap,
	.enable			= s2mps27_enable,
	.disable		= s2mps27_disable_regmap,
	.get_voltage_sel	= s2mps27_get_voltage_sel_regmap,
	.set_voltage_sel	= s2mps27_set_voltage_sel_regmap,
	.set_voltage_time_sel	= s2mps27_set_voltage_time_sel,
	.set_mode		= s2mps27_set_mode,
	.get_mode		= s2mps27_get_mode,
	.set_ramp_delay		= s2mps27_set_ramp_delay,
};

#define REGULATORS_DEFINE(x)	{ ARRAY_SIZE(x), x }
static struct s2p_pmic_regulators_desc regulators[] = {
	REGULATORS_DEFINE(s2mps27_regulators),
};

#if IS_ENABLED(CONFIG_EXYNOS_AFM) || IS_ENABLED(CONFIG_NPU_AFM)
/*
 * @main_no : Main Pmic number. If main_no is 1, it is main pmic1.
 */
int main_pmic_afm_read_reg(uint32_t main_no, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_info *pmic_info = NULL;
	struct s2p_regulator_bus *bus_info = NULL;

	if (main_no != 1) {
		pr_err("%s: main_no:(%d) is out of range\n", __func__, main_no);
		return -ENODEV;
	}

	pmic_info = s2mps27_static_info;
	bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, S2MPS27_PM1_ADDR, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(main_pmic_afm_read_reg);

int main_pmic_afm_update_reg(uint32_t main_no, uint8_t reg, uint8_t val, uint8_t mask)
{
	struct s2p_regulator_info *pmic_info = NULL;
	struct s2p_regulator_bus *bus_info = NULL;

	if (main_no != 1) {
		pr_err("%s: main_no:(%d) is out of range\n", __func__, main_no);
		return -ENODEV;
	}

	pmic_info = s2mps27_static_info;
	bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, S2MPS27_PM1_ADDR, reg, val, mask) : -ENODEV;
}
EXPORT_SYMBOL_GPL(main_pmic_afm_update_reg);

bool chk_main_pmic_info(uint32_t main_no)
{
	struct s2p_regulator_info *pmic_info = NULL;

	if (main_no != 1) {
		pr_err("%s: main_no(%d) is out of range\n", __func__, main_no);
		return false;
	}

	pmic_info = s2mps27_static_info;

	return (pmic_info) ? true : false;
}
EXPORT_SYMBOL_GPL(chk_main_pmic_info);

int main_pmic_read_reg(struct i2c_client *i2c, uint8_t reg, uint8_t *val)
{
	struct s2p_regulator_info *pmic_info = s2mps27_static_info;
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_read_reg(bus_info, pmic_info->pm_addr, reg, val) : -ENODEV;
}
EXPORT_SYMBOL_GPL(main_pmic_read_reg);

int main_pmic_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2p_regulator_info *pmic_info = s2mps27_static_info;
	struct s2p_regulator_bus *bus_info = pmic_info->bus_info;

	return (bus_info) ? s2p_regulator_update_reg(bus_info, pmic_info->pm_addr, reg, val, mask) : -ENODEV;

}
EXPORT_SYMBOL_GPL(main_pmic_update_reg);

int main_pmic_get_i2c(struct i2c_client **i2c)
{
	return 0;
}
EXPORT_SYMBOL_GPL(main_pmic_get_i2c);
#endif

static int s2mps27_read_pwron_status(void)
{
	uint8_t val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2p_regulator_info *pmic_info = s2mps27_static_info;

	exynos_acpm_read_reg(pmic_info->bus_info->bus_node, pmic_info->bus_info->sid,
				S2MPS27_PM1_ADDR, S2MPS27_PM1_STATUS1, &val);

	pr_info("%s: 0x%02hhx\n", __func__, val);
#endif
	return (val & S2MPS27_STATUS1_PWRON);
}

static int s2mps27_read_mrb_status(void)
{
	uint8_t val = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	struct s2p_regulator_info *pmic_info = s2mps27_static_info;

	exynos_acpm_read_reg(pmic_info->bus_info->bus_node, pmic_info->bus_info->sid,
				S2MPS27_PM1_ADDR, S2MPS27_PM1_STATUS1, &val);

	pr_info("%s: 0x%02hhx\n", __func__, val);
#endif
	return (val & S2MPS27_STATUS1_MR1B);
}

int pmic_read_pwrkey_status(void)
{
	return s2mps27_read_pwron_status();
}
EXPORT_SYMBOL_GPL(pmic_read_pwrkey_status);

int pmic_read_vol_dn_key_status(void)
{
	return s2mps27_read_mrb_status();
}
EXPORT_SYMBOL_GPL(pmic_read_vol_dn_key_status);

#define DECLARE_JIG_ACOK(n, r, m, s, e) { .name = (n), .reg = (r), .mask = (m), .shift = (s), .en = (e) }
static struct s2p_reg_update s2mps27_jig_acok[] = {
	DECLARE_JIG_ACOK("CFG_PM", S2MPS27_PM1_CFG_PM, S2MPS27_PM1_INST_ACOK_EN_MASK,
			S2MPS27_PM1_INST_ACOK_EN_SHIFT, false),
	DECLARE_JIG_ACOK("TIME_CTRL", S2MPS27_PM1_TIME_CTRL, S2MPS27_PM1_JIG_REBOOT_EN_MASK,
			S2MPS27_PM1_JIG_REBOOT_EN_SHIFT, false),
};

static int s2mps27_jig_acok_update(struct s2p_regulator_info *pmic_info)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
	uint32_t i = 0, cnt = 0;
	const uint32_t size = ARRAY_SIZE(s2mps27_jig_acok);
	struct s2p_reg_update *jig_acok = s2mps27_jig_acok;
	char buf[1024] = {0, };
	uint8_t val = 0;
	const bool jig_acok_en[] = {pmic_info->inst_acok_en, pmic_info->jig_reboot_en};

	if (size != ARRAY_SIZE(jig_acok_en))
		return -EINVAL;

	for (i = 0; i < size; i++) {
		jig_acok[i].en = jig_acok_en[i];
		ret = exynos_acpm_update_reg(pmic_info->bus_info->bus_node, pmic_info->bus_info->sid,
						pmic_info->pm_addr, jig_acok[i].reg,
						jig_acok[i].en << jig_acok[i].shift,
						jig_acok[i].mask);
		if (ret < 0) {
			pr_err("%s: update fail\n", __func__);
			return ret;
		}

		ret = exynos_acpm_read_reg(pmic_info->bus_info->bus_node, pmic_info->bus_info->sid,
				pmic_info->pm_addr, jig_acok[i].reg, &val);
		if (ret < 0) {
			pr_err("%s: read fail\n", __func__);
			return ret;
		}

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "%s(0x%02hhx%02hhx): 0x%02hhx, ",
				jig_acok[i].name, pmic_info->pm_addr, jig_acok[i].reg, val);
	}

	dev_info(pmic_info->dev, "%s: %s\n", __func__, buf);
#endif
	return ret;
}

static int s2mps27_power_off(void)
{
	struct s2p_regulator_info *pmic_info = s2mps27_static_info;

	if (!pmic_info)
		return -ENODEV;

	return s2mps27_jig_acok_update(pmic_info);
}

static int __maybe_unused s2mps27_power_off_seq_wa(void)
{
	int ret = 0;

	return ret;
}

int s2mps27_base_address(uint8_t base_addr)
{
	switch (base_addr) {
	case S2MPS27_VGPIO_ADDR:
	case S2MPS27_COM_ADDR:
	case S2MPS27_RTC_ADDR:
	case S2MPS27_PM1_ADDR:
	case S2MPS27_PM2_ADDR:
	case S2MPS27_PM3_ADDR:
	case S2MPS27_ADC_ADDR:
	case S2MPS27_GPIO_ADDR:
	case S2MPS27_EXT_ADDR:
		break;
	default:
		pr_err("%s: base address error(0x%02hhx)\n", __func__, base_addr);
		return -EINVAL;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static int of_s2mps27_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata,
					 struct s2p_regulator_info *pmic_info)
{
	struct device_node *pmic_np = NULL;
	int dev_type = pmic_info->device_type;
	int ret = 0;
	uint32_t val = 0;

	dev_info(dev, "[MAIN PMIC] %s: start\n", __func__);

	pmic_np = dev->of_node;
	if (!pmic_np) {
		dev_err(dev, "could not find pmic sub-node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(pmic_np, "inst_acok_en", &val);
	if (ret)
		return -EINVAL;
	pdata->inst_acok_en = !!val;

	ret = of_property_read_u32(pmic_np, "jig_reboot_en", &val);
	if (ret)
		return -EINVAL;
	pdata->jig_reboot_en = !!val;

	ret = of_property_read_u32(pmic_np, "smpl_warn_vth", &val);
	if (!ret && !(val & ~0x7))
		pdata->smpl_warn_vth = val;
	else
		pdata->smpl_warn_vth = -1;

	ret = of_s2p_regulator_parse_dt(dev, pdata, &regulators[dev_type], pmic_info);
	if (ret < 0)
		return ret;

	dev_info(dev, "[MAIN PMIC] %s: end\n", __func__);

	return ret;
}
#else
static int of_s2mps27_regulator_parse_dt(struct device *dev,
					 struct s2p_pmic_data *pdata
					 struct s2p_regulator_info *pmic_info)
{
	return 0;
}
#endif /* CONFIG_OF */

static void s2mps27_init_jig_acok_info(struct s2p_regulator_info *pmic_info,
					struct s2p_pmic_data *pdata)
{
	pmic_info->inst_acok_en = pdata->inst_acok_en;
	pmic_info->jig_reboot_en = pdata->jig_reboot_en;
}

static int s2mps27_set_smpl_warn_vth(struct s2p_regulator_info *pmic_info,
					int smpl_warn_vth)
{
	if (smpl_warn_vth < 0)
		return 0;

	return s2p_regulator_update_reg(pmic_info->bus_info, S2MPS27_PM1_ADDR,
			S2MPS27_PM1_CTRL2, smpl_warn_vth << S2MPS27_PM1_SMPLWARN_LEVEL_SHIFT,
			S2MPS27_PM1_SMPLWARN_LEVEL_MASK);
}

static int s2mps27_init_regulators_info(struct s2p_regulator_info *pmic_info,
					struct s2p_pmic_data *pdata)
{
	int ret = 0;
	pmic_info->wtsr_en = false;

	ret = s2mps27_set_smpl_warn_vth(pmic_info, pdata->smpl_warn_vth);
	if (ret < 0)
		return ret;

	s2mps27_init_jig_acok_info(pmic_info, pdata);

	return 0;
}

static void s2mps27_init_regulators_ops(struct s2p_pmic_regulators_desc *_regulators)
{
	uint32_t i;

	for (i = 0; i < _regulators->count; i++) {
		_regulators->desc[i].ops = &_REGULATOR_OPS();
		_regulators->desc[i].of_map_mode = s2mps27_of_map_mode;
	}
}

static int s2mps27_init_bus(struct s2p_regulator_info *pmic_info)
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

static int s2mps27_pmic_probe(struct i2c_client *i2c)
{
	struct s2p_pmic_data *pdata = NULL;
	struct s2p_regulator_info *pmic_info = NULL;
	const int dev_type = (enum s2mps27_types)of_device_get_match_data(&i2c->dev);
	int ret = 0;

	dev_info(&i2c->dev, "[MAIN PMIC] %s: start\n", __func__);

	pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2p_pmic_data), GFP_KERNEL);
	if (!pdata)
		return -ENODEV;

	pmic_info = devm_kzalloc(&i2c->dev, sizeof(struct s2p_regulator_info), GFP_KERNEL);
	if (!pmic_info)
		return -ENOMEM;

	pmic_info->dev = &i2c->dev;
	pmic_info->device_type = dev_type;
	pmic_info->rdesc = &regulators[dev_type];
	pmic_info->pm_addr = S2MPS27_PM1_ADDR;
	pmic_info->enable_shift_bit = S2MPS27_ENABLE_SHIFT;
	pmic_info->regulator_num = S2MPS27_REG_MAX;

	ret = s2mps27_init_bus(pmic_info);
	if (ret < 0)
		return ret;

	s2mps27_init_regulators_ops(&regulators[dev_type]);

	ret = of_s2mps27_regulator_parse_dt(&i2c->dev, pdata, pmic_info);
	if (ret < 0)
		goto err_bus_info;


	s2mps27_static_info = pmic_info;

	i2c_set_clientdata(i2c, pmic_info);

	ret = s2p_register_regulators(pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

	ret = s2mps27_init_regulators_info(pmic_info, pdata);
	if (ret < 0)
		goto err_bus_info;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	pmic_info->check_base_address = s2mps27_base_address;
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

	exynos_reboot_register_pmic_ops(s2mps27_power_off, NULL,
			s2mps27_power_off_seq_wa, s2mps27_read_pwron_status);

	dev_info(&i2c->dev, "[MAIN PMIC] %s: end\n", __func__);

	return 0;

err_bus_info:
	mutex_destroy(&pmic_info->bus_info->bus_lock);

	return ret;
}

static void s2mps27_pmic_remove(struct i2c_client *i2c)
{
	struct s2p_regulator_info *pmic_info = i2c_get_clientdata(i2c);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_sysfs_entries(pmic_info->pmic_sysfs->dev);
#endif

	s2p_pmic_remove(pmic_info);
}

#if IS_ENABLED(CONFIG_OF)
static struct of_device_id s2mps27_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mps27_regulator", .data = (void*)TYPE_S2MPS27 },
	{ },
};
MODULE_DEVICE_TABLE(of, s2mps27_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver s2mps27_pmic_driver = {
	.driver = {
		.name = "s2mps27-regulator",
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(s2mps27_i2c_dt_ids),
#endif /* CONFIG_OF */
	},
	.probe = s2mps27_pmic_probe,
	.remove = s2mps27_pmic_remove,
};

static int __init s2mps27_regulator_i2c_init(void)
{
	pr_info("%s:%s\n", REGULATOR_DEV_NAME, __func__);
	return i2c_add_driver(&s2mps27_pmic_driver);
}
subsys_initcall(s2mps27_regulator_i2c_init);

static void __exit s2mps27_regulator_i2c_exit(void)
{
	i2c_del_driver(&s2mps27_pmic_driver);
}
module_exit(s2mps27_regulator_i2c_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPS27 Regulator Driver");
MODULE_LICENSE("GPL");
