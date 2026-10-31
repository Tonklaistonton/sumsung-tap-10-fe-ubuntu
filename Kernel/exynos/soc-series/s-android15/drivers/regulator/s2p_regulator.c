/* s2p_regulator.c
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

#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/regulator/of_regulator.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2p_regulator.h>
#include <linux/reset/exynos/exynos-reset.h>

#define BUCK_MODE_SHIFT		2
#define BUCK_MODE_MASK 		(0x03 << BUCK_MODE_SHIFT)
#define BUCK_AUTO_MODE		(0x02 << BUCK_MODE_SHIFT)
#define BUCK_FCCM_MODE		(0x03 << BUCK_MODE_SHIFT)

static struct s2p_src_info *src_info_table[S2P_PMIC_NUM];

int s2p_regulator_read_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, uint8_t *dest)
{
	int ret = 0;

	if (!bus_info->read_reg) {
		pr_info("%s: Not ready yet\n", __func__);
		return 0;
	}

	mutex_lock(&bus_info->bus_lock);
	ret = bus_info->read_reg(bus_info->bus_node, bus_info->sid, base_addr, reg, dest);
	mutex_unlock(&bus_info->bus_lock);

	if (ret)
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_regulator_read_reg);

int s2p_regulator_bulk_read_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf)
{
	int ret = 0;

	if (!bus_info->bulk_read) {
		pr_info("%s: Not ready yet\n", __func__);
		return 0;
	}

	mutex_lock(&bus_info->bus_lock);
	ret = bus_info->bulk_read(bus_info->bus_node, bus_info->sid, base_addr, reg, count, buf);
	mutex_unlock(&bus_info->bus_lock);

	if (ret)
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_regulator_bulk_read_reg);

int s2p_regulator_write_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, uint8_t val)
{
	int ret = 0;

	if (!bus_info->write_reg) {
		pr_info("%s: Not ready yet\n", __func__);
		return 0;
	}

	mutex_lock(&bus_info->bus_lock);
	ret = bus_info->write_reg(bus_info->bus_node, bus_info->sid, base_addr, reg, val);
	mutex_unlock(&bus_info->bus_lock);

	if (ret)
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_regulator_write_reg);

int s2p_regulator_bulk_write_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf)
{
	int ret = 0;

	if (!bus_info->bulk_write) {
		pr_info("%s: Not ready yet\n", __func__);
		return 0;
	}

	mutex_lock(&bus_info->bus_lock);
	ret = bus_info->bulk_write(bus_info->bus_node, bus_info->sid, base_addr, reg, count, buf);
	mutex_unlock(&bus_info->bus_lock);

	if (ret)
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_regulator_bulk_write_reg);

int s2p_regulator_update_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, uint8_t val, uint8_t mask)
{
	int ret = 0;

	if (!bus_info->update_reg) {
		pr_info("%s: Not ready yet\n", __func__);
		return 0;
	}

	mutex_lock(&bus_info->bus_lock);
	ret = bus_info->update_reg(bus_info->bus_node, bus_info->sid, base_addr, reg, val, mask);
	mutex_unlock(&bus_info->bus_lock);

	if (ret)
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_regulator_update_reg);

unsigned int s2p_of_map_mode(unsigned int f_mode)
{
	switch (f_mode) {
		case REGULATOR_MODE_FAST:	/* BUCK FCCM mode */
			return 0x01;
		case REGULATOR_MODE_NORMAL:	/* BUCK Auto */
			return 0x02;
		default:
			return REGULATOR_MODE_INVALID;
	}
}
EXPORT_SYMBOL_GPL(s2p_of_map_mode);

/* BUCKs support mode for Auto/FCCM */
int s2p_set_mode(struct regulator_dev *rdev, struct s2p_regulator_info *pmic_info,
			unsigned int f_mode)
{
	switch (f_mode) {
	case REGULATOR_MODE_FAST:
		f_mode = BUCK_FCCM_MODE;
		break;
	case REGULATOR_MODE_NORMAL:
		f_mode = BUCK_AUTO_MODE;
		break;
	default:
		dev_err(pmic_info->dev, "%s: invalid mode %u specified\n", __func__, f_mode);
		return -EINVAL;
	}

	return s2p_regulator_update_reg(pmic_info->bus_info, pmic_info->pm_addr, rdev->desc->enable_reg,
				f_mode, BUCK_MODE_MASK);
}
EXPORT_SYMBOL_GPL(s2p_set_mode);

/* BUCKs support Auto/FCCM mode */
unsigned int s2p_get_mode(struct regulator_dev *rdev, struct s2p_regulator_info *pmic_info)
{
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_regulator_read_reg(pmic_info->bus_info, pmic_info->pm_addr, rdev->desc->enable_reg, &val);
	if (ret)
		return REGULATOR_MODE_INVALID;

	dev_info(pmic_info->dev, "%s: [%s] reg: 0x%02hhx%02hhx, val: 0x%02hhx\n",
			__func__, rdev->desc->name, pmic_info->pm_addr, rdev->desc->enable_reg, val);

	val &= BUCK_MODE_MASK;

	if (val == BUCK_FCCM_MODE)
		ret = REGULATOR_MODE_FAST;
	else if(val == BUCK_AUTO_MODE)
		ret = REGULATOR_MODE_NORMAL;
	else {
		ret = REGULATOR_MODE_INVALID;
		dev_err(pmic_info->dev, "%s: Not found Auto/FCCM mode (%d)\n", __func__, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_get_mode);

int s2p_enable(struct s2p_regulator_info *pmic_info, struct regulator_dev *rdev)
{
	return s2p_regulator_update_reg(pmic_info->bus_info, pmic_info->pm_addr,
			rdev->desc->enable_reg,
			pmic_info->enmode[rdev_get_id(rdev)],
			rdev->desc->enable_mask);
}
EXPORT_SYMBOL_GPL(s2p_enable);

int s2p_disable_regmap(struct s2p_regulator_info *pmic_info, struct regulator_dev *rdev)
{
	uint8_t val = 0;

	if (rdev->desc->enable_is_inverted)
		val = rdev->desc->enable_mask;

	return s2p_regulator_update_reg(pmic_info->bus_info, pmic_info->pm_addr,
			rdev->desc->enable_reg,	val, rdev->desc->enable_mask);
}
EXPORT_SYMBOL_GPL(s2p_disable_regmap);

int s2p_is_enabled_regmap(struct s2p_regulator_info *pmic_info, struct regulator_dev *rdev)
{
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_regulator_read_reg(pmic_info->bus_info, pmic_info->pm_addr, rdev->desc->enable_reg, &val);
	if (ret < 0)
		return ret;

	if (rdev->desc->enable_is_inverted)
		return (val & rdev->desc->enable_mask) == 0;
	else
		return (val & rdev->desc->enable_mask) != 0;
}
EXPORT_SYMBOL_GPL(s2p_is_enabled_regmap);

int s2p_get_voltage_sel_regmap(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev)
{
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_regulator_read_reg(pmic_info->bus_info, pmic_info->pm_addr, rdev->desc->vsel_reg, &val);
	if (ret < 0)
		return ret;

	val &= rdev->desc->vsel_mask;

	return val;
}
EXPORT_SYMBOL_GPL(s2p_get_voltage_sel_regmap);

int s2p_set_voltage_sel_regmap(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev, unsigned sel)
{
	int ret;

	ret = s2p_regulator_update_reg(pmic_info->bus_info, pmic_info->pm_addr,
			rdev->desc->vsel_reg, sel, rdev->desc->vsel_mask);
	if (ret < 0) {
		pr_warn("%s: failed to set voltage_sel_regmap\n", rdev->desc->name);
		return ret;
	}

	if (rdev->desc->apply_bit)
		ret = s2p_regulator_update_reg(pmic_info->bus_info,
				pmic_info->pm_addr,
				rdev->desc->apply_reg,
				rdev->desc->apply_bit,
				rdev->desc->apply_bit);
	return ret;
}
EXPORT_SYMBOL_GPL(s2p_set_voltage_sel_regmap);

int s2p_set_voltage_time_sel(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev,
		unsigned int old_selector,
		unsigned int new_selector)
{
	unsigned int ramp_delay = 0;
	int old_volt = 0, new_volt = 0;

	if (rdev->constraints->ramp_delay)
		ramp_delay = rdev->constraints->ramp_delay;
	else if (rdev->desc->ramp_delay)
		ramp_delay = rdev->desc->ramp_delay;

	if (ramp_delay == 0) {
		pr_warn("%s: ramp_delay not set\n", rdev->desc->name);
		return -EINVAL;
	}

	/* sanity check */
	if (!rdev->desc->ops->list_voltage)
		return -EINVAL;

	old_volt = rdev->desc->ops->list_voltage(rdev, old_selector);
	new_volt = rdev->desc->ops->list_voltage(rdev, new_selector);

	if (old_selector < new_selector)
		return DIV_ROUND_UP(new_volt - old_volt, ramp_delay);
	else
		return DIV_ROUND_UP(old_volt - new_volt, ramp_delay);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_set_voltage_time_sel);

struct s2p_regulator_bus* s2p_init_bus(struct device *dev)
{
	struct s2p_regulator_bus *bus_info = NULL;

	bus_info = devm_kzalloc(dev, sizeof(struct s2p_regulator_bus), GFP_KERNEL);
	if (!bus_info)
		return ERR_PTR(-ENOMEM);

	bus_info->bus_node = dev->of_node;

	mutex_init(&bus_info->bus_lock);

	return bus_info;
}
EXPORT_SYMBOL_GPL(s2p_init_bus);

int s2p_register_regulators(struct s2p_regulator_info *pmic_info,
		struct s2p_pmic_data *pdata)
{
	struct s2p_pmic_regulators_desc *rdesc = pmic_info->rdesc;
	struct regulator_config config = { };
	int num_rdata = pdata->num_rdata;
	int ret = 0;
	uint32_t i = 0, midx = 0;

	dev_info(pmic_info->dev, "[PMIC] %s: start\n", __func__);

	config.dev = pmic_info->dev;
	config.driver_data = pmic_info;

	for (i = 0; i < num_rdata; i++) {
		midx = pdata->rdata[i].midx;
		config.init_data = pdata->rdata[i].initdata;
		config.of_node = pdata->rdata[i].reg_node;
		pmic_info->rdev[i] = devm_regulator_register(pmic_info->dev, &rdesc->desc[midx], &config);
		if (IS_ERR(pmic_info->rdev[i])) {
			ret = PTR_ERR(pmic_info->rdev[i]);
			dev_err(pmic_info->dev,
				"[PMIC] %s: %s(midx:%d) regulator failed to initialize in loop cnt %d\n",
				__func__, rdesc->desc[midx].name, midx, i);
			return ret;
		}
	}

	dev_info(pmic_info->dev, "[PMIC] %s: end\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_register_regulators);

#if IS_ENABLED(CONFIG_OF)
int of_s2p_regulator_parse_dt(struct device *dev,
		struct s2p_pmic_data *pdata,
		struct s2p_pmic_regulators_desc *regulators,
		struct s2p_regulator_info *pmic_info)
{
	struct device_node *regulators_np = NULL, *reg_np = NULL;
	struct s2p_regulator_data *rdata = NULL;
	int i = 0, ret = 0;
	uint32_t mode = 0;
	uint32_t val = 0;
	uint32_t out_vals[2] = {0};

	dev_info(dev, "%s: start\n", __func__);

	ret = of_property_read_u32(dev->of_node, "sid", &val);
	if (ret)
		return -EINVAL;
	pdata->sid = (uint8_t)val;
	pmic_info->bus_info->sid = pdata->sid;

	ret = of_property_read_u32(dev->of_node, "pmic_src_no", &val);
	if (ret)
		pdata->pmic_src_no = S2P_PMIC_NUM;
	else
		pdata->pmic_src_no = (uint32_t)val;

	ret = of_property_read_u32_array(dev->of_node, "on_src", out_vals, 2);
	if (ret) {
		pdata->on_src_reg = 0;
		pdata->on_src_cnt = 0;
	} else {
		pdata->on_src_reg = (uint16_t)out_vals[0];
		pdata->on_src_cnt = (uint32_t)out_vals[1];
	}

	ret = of_property_read_u32_array(dev->of_node, "off_src", out_vals, 2);
	if (ret) {
		pdata->off_src_reg = 0;
		pdata->off_src_cnt = 0;
	} else {
		pdata->off_src_reg = (uint16_t)out_vals[0];
		pdata->off_src_cnt = (uint32_t)out_vals[1];
	}

	regulators_np = of_find_node_by_name(dev->of_node, "regulators");
	if (!regulators_np) {
		dev_err(dev, "could not find regulators sub-node\n");
		return -ENODEV;
	}
	dev->of_node = regulators_np;
	pdata->num_rdata = 0;

	rdata = devm_kzalloc(dev, sizeof(*rdata) * regulators->count, GFP_KERNEL);
	if (!rdata)
		return -ENOMEM;

	pdata->rdata = rdata;

	pmic_info->rdev = devm_kzalloc(dev,
			sizeof(struct regulator_dev *) * regulators->count, GFP_KERNEL);
	if (!pmic_info->rdev)
		return -ENOMEM;

	pmic_info->enmode = devm_kzalloc(dev,
			sizeof(unsigned int) * pmic_info->regulator_num, GFP_KERNEL);
	if (!pmic_info->enmode)
		return -ENOMEM;

	for_each_child_of_node(regulators_np, reg_np) {
		int id = 0;
		for (i = 0; i < regulators->count; i++)
			if (!of_node_cmp(reg_np->name, regulators->desc[i].name))
				break;

		if (i == regulators->count) {
			dev_warn(dev, "[PMIC] %s: don't know how to configure regulator %s\n",
					__func__, reg_np->name);
			continue;
		}

		id = regulators->desc[i].id;
		rdata->id = id;
		rdata->midx = i;
		rdata->initdata = of_get_regulator_init_data(dev, reg_np,
				&regulators->desc[i]);

		ret = of_property_read_u32(reg_np, "regulator-enable-mode", &mode);
		if (ret) {
			dev_err(dev, "[PMIC] %s: %s falied to get enable-mode\n", __func__, reg_np->name);
			return -ENODEV;
		}

		pmic_info->enmode[id] = mode << pmic_info->enable_shift_bit;
		rdata->reg_node = reg_np;
		rdata++;
		pdata->num_rdata++;
	}

	dev_info(dev, "%s: end\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(of_s2p_regulator_parse_dt);
#endif

int s2p_init_src_info(struct s2p_regulator_info *pmic_info, struct s2p_pmic_data *pdata)
{
	int ret = 0;

	if (!pmic_info || !pdata || pdata->pmic_src_no >= S2P_PMIC_NUM)
		return -EINVAL;

	pmic_info->src_info = devm_kzalloc(pmic_info->dev, sizeof(struct s2p_src_info), GFP_KERNEL);
	if (!pmic_info->src_info)
		return -ENOMEM;

	pmic_info->src_info->bus_info = pmic_info->bus_info;
	pmic_info->src_info->pmic_src_no = pdata->pmic_src_no;
	pmic_info->src_info->on_src_reg = pdata->on_src_reg;
	pmic_info->src_info->on_src_cnt = pdata->on_src_cnt;
	pmic_info->src_info->off_src_reg = pdata->off_src_reg;
	pmic_info->src_info->off_src_cnt = pdata->off_src_cnt;

	src_info_table[pmic_info->src_info->pmic_src_no] = pmic_info->src_info;

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_init_src_info);

int s2p_get_pwronsrc(uint32_t pmic_no, uint8_t *arr, size_t arr_size)
{
	struct s2p_src_info *on_src_info = NULL;
	int ret = 0;
	uint8_t base_addr = 0, reg = 0;
	uint32_t cnt = 0;

	if (pmic_no >= S2P_PMIC_NUM || !src_info_table[pmic_no] || !arr)
		return -ENOMEM;

	on_src_info = src_info_table[pmic_no];
	cnt = on_src_info->on_src_cnt;

	if (!on_src_info->on_src_reg || !cnt || cnt > arr_size)
		return -EINVAL;

	base_addr = (on_src_info->on_src_reg >> 8) & 0xFF;
	reg = on_src_info->on_src_reg & 0xFF;

	ret = s2p_regulator_bulk_read_reg(on_src_info->bus_info, base_addr, reg, cnt, arr);
	ret = (ret) ? -EINVAL : cnt;

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_get_pwronsrc);

int s2p_get_pwroffsrc(uint32_t pmic_no, uint8_t *arr, size_t arr_size)
{
	struct s2p_src_info *off_src_info = NULL;
	int ret = 0;
	uint8_t base_addr = 0, reg = 0;
	uint32_t cnt = 0;

	if (pmic_no >= S2P_PMIC_NUM || !src_info_table[pmic_no] || !arr)
		return -ENOMEM;

	off_src_info = src_info_table[pmic_no];
	cnt = off_src_info->off_src_cnt;

	if (!off_src_info->off_src_reg || !cnt || cnt > arr_size)
		return -EINVAL;

	base_addr = (off_src_info->off_src_reg >> 8) & 0xFF;
	reg = off_src_info->off_src_reg & 0xFF;

	ret = s2p_regulator_bulk_read_reg(off_src_info->bus_info, base_addr, reg, cnt, arr);
	ret = (ret) ? -EINVAL : cnt;

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_get_pwroffsrc);

int s2p_clear_pwroffsrc(uint32_t pmic_no)
{
	struct s2p_src_info *clr_src_info = NULL;
	int ret = 0;
	uint8_t base_addr = 0, reg = 0;
	uint32_t cnt = 0;
	uint8_t *arr = NULL;

	if (pmic_no >= S2P_PMIC_NUM || !src_info_table[pmic_no])
		return -ENOMEM;

	clr_src_info = src_info_table[pmic_no];

	if (!clr_src_info->off_src_reg || !clr_src_info->off_src_cnt)
		return -EINVAL;

	base_addr = (clr_src_info->off_src_reg >> 8) & 0xFF;
	reg = clr_src_info->off_src_reg & 0xFF;
	cnt = clr_src_info->off_src_cnt;

	arr = kzalloc(sizeof(*arr) * cnt, GFP_KERNEL);
	if (!arr)
		return -ENOMEM;

	ret = s2p_regulator_bulk_write_reg(clr_src_info->bus_info, base_addr, reg, cnt, arr);

	kfree(arr);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_clear_pwroffsrc);

int s2p_pmic_remove(struct s2p_regulator_info *pmic_info)
{
	if (!pmic_info || !pmic_info->dev)
		return -ENODEV;

	dev_info(pmic_info->dev, "[PMIC] %s\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_pmic_remove);

int s2p_pmic_suspend(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	dev_info(dev, "[PMIC] %s\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_pmic_suspend);

int s2p_pmic_resume(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	dev_info(dev, "[PMIC] %s\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_pmic_resume);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2p_get_addr_from_buf(const struct s2p_regulator_info *pmic_info, const char *buf,
				uint16_t *base_addr, uint8_t *reg_addr, uint8_t *val, const char c)
{
	int ret = 0;

	if (buf == NULL) {
		pr_info("%s: empty buffer\n", __func__);
		return -EINVAL;
	}

	switch (c) {
	case 'R':
		ret = sscanf(buf, "0x%02hx%02hhx", base_addr, reg_addr);
		if (ret != 2)
			goto err;
		break;
	case 'W':
		ret = sscanf(buf, "0x%02hx%02hhx 0x%02hhx", base_addr, reg_addr, val);
		if (ret != 3)
			goto err;
		break;
	default:
		goto err;
	}

	ret = pmic_info->check_base_address(*base_addr);
	if (ret < 0)
		return ret;

	return ret;

err:
	pr_err("%s: input error\n", __func__);
	return -EINVAL;
}

ssize_t s2p_read_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct s2p_regulator_info *pmic_info = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = pmic_info->pmic_sysfs;
	uint16_t base_addr = 0;
	uint8_t reg_addr = 0, val = 0;
	int ret = 0;

	ret = s2p_get_addr_from_buf(pmic_info, buf, &base_addr, &reg_addr, &val, 'R');
	if (ret < 0)
		return ret;

	ret = s2p_regulator_read_reg(pmic_info->bus_info, base_addr, reg_addr, &val);
	if (ret < 0)
		return ret;

	pmic_sysfs->base_addr = base_addr;
	pmic_sysfs->read_addr = reg_addr;
	pmic_sysfs->read_val = val;

	dev_info(pmic_info->dev, "%s: sid(0x%02hhx) reg(0x%02hx%02hhx) data(0x%02hhx)\n",
			__func__, pmic_info->bus_info->sid, base_addr, reg_addr, val);

	return size;
}

ssize_t s2p_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct s2p_regulator_info *pmic_info = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = pmic_info->pmic_sysfs;

	return sprintf(buf, "0x%02hx%02hhx: 0x%02hhx\n",
			pmic_sysfs->base_addr, pmic_sysfs->read_addr, pmic_sysfs->read_val);
}

ssize_t s2p_write_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct s2p_regulator_info *pmic_info = dev_get_drvdata(dev);
	struct pmic_sysfs_dev *pmic_sysfs = pmic_info->pmic_sysfs;
	uint16_t base_addr = 0;
	uint8_t reg_addr = 0, val = 0;
	int ret = 0;

	ret = s2p_get_addr_from_buf(pmic_info, buf, &base_addr, &reg_addr, &val, 'W');
	if (ret < 0)
		return ret;

	ret = s2p_regulator_write_reg(pmic_info->bus_info, base_addr, reg_addr, val);
	if (ret < 0)
		return ret;

	pmic_sysfs->base_addr = base_addr;
	pmic_sysfs->read_addr = reg_addr;
	pmic_sysfs->read_val = val;

	dev_info(pmic_info->dev, "%s: sid(0x%02hhx) reg(0x%02hx%02hhx) data(0x%02hhx)\n",
			__func__, pmic_info->bus_info->sid, base_addr, reg_addr, val);

	return size;
}

struct pmic_device_attribute regulator_attr[] = {
	PMIC_ATTR(write, S_IRUGO | S_IWUSR, s2p_show, s2p_write_store),
	PMIC_ATTR(read, S_IRUGO | S_IWUSR, s2p_show, s2p_read_store),
};

int s2p_create_sysfs(struct s2p_regulator_info *pmic_info)
{
	struct device *dev = pmic_info->dev;
	struct device *sysfs_dev = NULL;
	char device_name[32] = {0,};
	int err = -ENODEV, i = 0;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s@%s",
			pmic_info->sysfs_name, dev_name(dev));

	pmic_info->pmic_sysfs = devm_kzalloc(dev, sizeof(struct pmic_sysfs_dev), GFP_KERNEL);
	pmic_info->pmic_sysfs->dev = pmic_device_create(pmic_info, device_name);
	sysfs_dev = pmic_info->pmic_sysfs->dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++) {
		err = device_create_file(sysfs_dev, &regulator_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	dev_info(dev, "%s()\n", __func__);

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(sysfs_dev, &regulator_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);

	return -EINVAL;

}
EXPORT_SYMBOL_GPL(s2p_create_sysfs);

void s2p_remove_sysfs_entries(struct device *sysfs_dev)
{
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(regulator_attr); i++)
		device_remove_file(sysfs_dev, &regulator_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);
}
EXPORT_SYMBOL_GPL(s2p_remove_sysfs_entries);
#endif /* CONFIG_DRV_SAMSUNG_PMIC */

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("SAMSUNG S2MPU15 Regulator Driver");
MODULE_LICENSE("GPL");
