/*
 * s2p_core.c - mfd core driver for the S2P
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
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/pmic/s2p.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

int s2p_read_reg(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, uint8_t *dest)
{
	int ret = 0;

	if (!sdev->exynos_read_reg) {
		pr_info("%s: Not used\n", __func__);
		return 0;
	}

	mutex_lock(&sdev->bus_lock);
	ret = sdev->exynos_read_reg(sdev->bus_node, sdev->sid, base_addr, reg, dest);
	mutex_unlock(&sdev->bus_lock);

	if (ret) {
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_read_reg);

int s2p_bulk_read(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf)
{
	int ret = 0;

	if (!sdev->exynos_bulk_read) {
		pr_info("%s: Not used\n", __func__);
		return 0;
	}

	mutex_lock(&sdev->bus_lock);
	ret = sdev->exynos_bulk_read(sdev->bus_node, sdev->sid, base_addr, reg, count, buf);
	mutex_unlock(&sdev->bus_lock);

	if (ret) {
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_bulk_read);

int s2p_write_reg(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, uint8_t value)
{
	int ret = 0;

	if (!sdev->exynos_write_reg) {
		pr_info("%s: Not used\n", __func__);
		return 0;
	}

	mutex_lock(&sdev->bus_lock);
	ret = sdev->exynos_write_reg(sdev->bus_node, sdev->sid, base_addr, reg, value);
	mutex_unlock(&sdev->bus_lock);

	if (ret) {
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_write_reg);

int s2p_bulk_write(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf)
{
	int ret = 0;

	if (!sdev->exynos_bulk_write) {
		pr_info("%s: Not used\n", __func__);
		return 0;
	}

	mutex_lock(&sdev->bus_lock);
	ret = sdev->exynos_bulk_write(sdev->bus_node, sdev->sid, base_addr, reg, count, buf);
	mutex_unlock(&sdev->bus_lock);

	if (ret) {
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_bulk_write);

int s2p_update_reg(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, uint8_t val, uint8_t mask)
{
	int ret = 0;

	if (!sdev->exynos_update_reg) {
		pr_info("%s: Not used\n", __func__);
		return 0;
	}

	mutex_lock(&sdev->bus_lock);
	ret = sdev->exynos_update_reg(sdev->bus_node, sdev->sid, base_addr, reg, val, mask);
	mutex_unlock(&sdev->bus_lock);

	if (ret) {
		pr_err("[%s] esca(acpm) ipc fail!\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2p_core_parse_dt(struct device *dev, struct s2p_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	uint32_t val = 0;

	if (!np)
		return -EINVAL;

	pdata->bus_node = np;

	ret = of_property_read_u32(np, "s2p,wakeup", &val);
	if (ret)
		return ret;
	pdata->wakeup = !!val;

	ret = of_property_read_u32(np, "sid", &val);
	if (ret)
		return ret;
	pdata->sid = val;

	return 0;
}
#else
static int of_s2p_core_parse_dt(struct device *dev, struct s2p_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

int s2p_init(struct device *dev, struct s2p_dev *sdev)
{
	struct s2p_platform_data *pdata = NULL;
	struct s2p_pmic_rev *rev_id = NULL;
	int ret = 0;

	dev_info(dev, "[PMIC] %s: start\n", __func__);

	pdata = devm_kzalloc(dev, sizeof(struct s2p_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	rev_id = devm_kzalloc(dev, sizeof(struct s2p_pmic_rev), GFP_KERNEL);
	if (!rev_id)
		return -ENOMEM;

	ret = of_s2p_core_parse_dt(dev, pdata);
	if (ret < 0) {
		dev_err(dev, "Failed to get device of_node\n");
		return ret;
	}
	dev->platform_data = pdata;

	pdata->irq_base = devm_irq_alloc_descs_from(dev, 0, sdev->irq_base_count, -1);
	if (pdata->irq_base < 0) {
		dev_err(dev, "%s: devm_irq_alloc_descs Fail! (%d)\n", __func__, pdata->irq_base);
		return -EINVAL;
	}

	sdev->dev = dev;
	sdev->pdata = pdata;
	sdev->rev_id = rev_id;
	sdev->wakeup = pdata->wakeup;
	sdev->sid = pdata->sid;
	sdev->irq_base = pdata->irq_base;
	sdev->bus_node = pdata->bus_node;
	mutex_init(&sdev->bus_lock);

	ret = device_init_wakeup(dev, sdev->wakeup);
	if (ret < 0) {
		dev_err(dev, "%s: device_init_wakeup fail(%d)\n", __func__, ret);
		return ret;
	}

	dev_info(dev, "[PMIC] %s: end\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_init);

void s2p_remove(struct s2p_dev *sdev)
{
	if (sdev->wakeup)
		device_init_wakeup(sdev->dev, false);

	mutex_destroy(&sdev->bus_lock);
}
EXPORT_SYMBOL_GPL(s2p_remove);
