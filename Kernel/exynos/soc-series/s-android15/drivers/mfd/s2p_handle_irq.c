/*
 * s2p_handle_irq.c - Interrupt handler support for PMIC
 *
 * Copyright (C) 2024 Samsung Electronics Co.Ltd
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

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/pmic_class.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <linux/notifier.h>

static BLOCKING_NOTIFIER_HEAD(ocp_oi_notifier);
int s2p_register_ocp_oi_notifier(struct notifier_block *nb)
{
	int ret = 0;

	ret = blocking_notifier_chain_register(&ocp_oi_notifier, nb);
	if (ret < 0)
		pr_err("%s: fail blocking notifier chain register (%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_register_ocp_oi_notifier);

static int s2p_call_ocp_oi_notifier(struct s2p_ocp_oi_data *ocp_oi_data)
{
	int ret = 0;

	ret = blocking_notifier_call_chain(&ocp_oi_notifier, 0, ocp_oi_data);
	if (ret < 0)
		pr_err("%s: fail to call s2p ocp oi notifier (%d)\n", __func__, ret);

	return ret;
}

static void s2p_notifier_work(struct work_struct *work)
{
	struct s2p_irq_info *pm_irq_info = container_of(work, struct s2p_irq_info,
		notifier_work.work);

	s2p_call_ocp_oi_notifier(&pm_irq_info->ocp_oi_data);
	pr_info("%s : name = %s, cnt = %d, type = %d\n", __func__,
		pm_irq_info->ocp_oi_data.name, pm_irq_info->ocp_oi_data.cnt, pm_irq_info->ocp_oi_data.irq_type);
}

int s2p_create_irq_notifier_wq(struct device *dev, struct s2p_irq_info *pm_irq_info,
		const uint32_t type_size, const char *mfd_dev_name, const uint32_t dev_id)
{
	uint32_t i = 0, cnt = 0;
	char device_name[S2P_NAME_MAX]= {0, };

	for (i = 0; i < type_size; i++) {
		cnt = snprintf(device_name, S2P_NAME_MAX - 1, "%s", mfd_dev_name);
		if (dev_id)
			cnt += snprintf(device_name + cnt, S2P_NAME_MAX - 1, "-%d", dev_id);
		snprintf(device_name + cnt, S2P_NAME_MAX - 1, "-[%d]", i);

		pm_irq_info[i].notifier_wqueue = create_singlethread_workqueue(device_name);
		if (!pm_irq_info[i].notifier_wqueue) {
			pr_err("%s: failed to create notifier work_queue\n", __func__);
			return -ESRCH;
		}
		INIT_DELAYED_WORK(&pm_irq_info[i].notifier_work, s2p_notifier_work);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_create_irq_notifier_wq);

int s2p_allocate_pmic_irq(struct device *dev,
		struct s2p_irq_info *pm_irq_info, const uint32_t type_size)
{
	uint32_t i = 0;

	if (!dev || !pm_irq_info)
		return -ENODEV;

	for (i = 0; i < type_size; i++) {
		pm_irq_info[i].irq_id = devm_kzalloc(dev,
				pm_irq_info[i].count * sizeof(uint32_t), GFP_KERNEL);
		if (!pm_irq_info[i].irq_id)
			return -ENOMEM;

		pm_irq_info[i].irq_cnt = devm_kzalloc(dev,
				pm_irq_info[i].count * sizeof(uint32_t), GFP_KERNEL);
		if (!pm_irq_info[i].irq_cnt)
			return -ENOMEM;

		pm_irq_info[i].irq_name = devm_kzalloc(dev,
				pm_irq_info[i].count * sizeof(char [S2P_NAME_MAX]), GFP_KERNEL);
		if (!pm_irq_info[i].irq_name)
			return -ENOMEM;

		pm_irq_info[i].irq_noti_name = devm_kzalloc(dev,
				pm_irq_info[i].count * sizeof(char [S2P_NAME_MAX]), GFP_KERNEL);
		if (!pm_irq_info[i].irq_noti_name)
			return -ENOMEM;

		pm_irq_info[i].irq_type = devm_kzalloc(dev,
				pm_irq_info[i].count * sizeof(enum s2p_irq_type), GFP_KERNEL);
		if (!pm_irq_info[i].irq_type)
			return -ENOMEM;
	}

	for (i = 0; i < type_size; i++)
		mutex_init(&pm_irq_info[i].lock);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_allocate_pmic_irq);

irqreturn_t s2p_handle_regulator_irq(int irq, void *data)
{
	struct s2p_irq_info *pmic_irq = data;
	uint32_t i = 0;

	mutex_lock(&pmic_irq->lock);

	for (i = 0; i < pmic_irq->count; i++) {
		if (pmic_irq->irq_id[i] == irq) {
			pmic_irq->irq_cnt[i]++;
			pr_info("%s: %s, cnt : %d\n", __func__,
				pmic_irq->irq_name[i], pmic_irq->irq_cnt[i]);
			dbg_snapshot_pmic(irq, pmic_irq->irq_name[i], i + 1, pmic_irq->irq_cnt[i]);
			pmic_irq->ocp_oi_data.name = pmic_irq->irq_noti_name[i];
			pmic_irq->ocp_oi_data.irq_type = pmic_irq->irq_type[i];
			pmic_irq->ocp_oi_data.cnt = pmic_irq->irq_cnt[i];
			pmic_irq->ocp_oi_data.irq_num = irq;
			queue_delayed_work(pmic_irq->notifier_wqueue, &pmic_irq->notifier_work, 0);
			break;
		}
	}

	mutex_unlock(&pmic_irq->lock);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(s2p_handle_regulator_irq);

int s2p_set_registers(struct s2p_dev *sdev,
		const struct s2p_reg_data *reg_info,
		unsigned int reg_count)
{
	uint32_t i = 0;
	uint8_t val = 0;
	int ret = 0, cnt = 0;
	char buf[1024] = {0, };

	for (i = 0; i < reg_count; i++) {
		ret = s2p_write_reg(sdev, reg_info[i].base,
				reg_info[i].reg, reg_info[i].val);
		if (ret < 0)
			goto err;

	}

	for (i = 0; i < reg_count; i++) {
		ret = s2p_read_reg(sdev, reg_info[i].base,
				reg_info[i].reg, &val);
		if (ret < 0)
			goto err;

		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "0x%02hhx%02hhx[0x%02hhx], ",
				reg_info[i].base, reg_info[i].reg, val);
	}
	dev_info(sdev->dev, "[PMIC] %s: %s\n", __func__, buf);

	return 0;
err:
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(s2p_set_registers);

void s2p_destroy_irq_list_mutex(struct s2p_pmic_irq_list *irq_list)
{
	uint8_t i = 0;

	if (!irq_list)
		return;

	for (i = 0; i < irq_list->irq_type_cnt; i++)
		mutex_destroy(&irq_list->irqs[i].lock);
}
EXPORT_SYMBOL_GPL(s2p_destroy_irq_list_mutex);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2p_irq_read_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s2p_dev *sdev = dev_get_drvdata(dev);
	struct s2p_pmic_irq_list *list = sdev->irq_list;
	uint32_t i = 0, j = 0;
	ssize_t cnt = 0;

	for (i = 0; i < list->irq_type_cnt; i++) {
		for (j = 0; j < list->irqs[i].count; j++) {
			cnt += snprintf(buf + cnt, PAGE_SIZE, "%*s:\t%5d\n",
					35, list->irqs[i].irq_name[j], list->irqs[i].irq_cnt[j]);
		}
	}

	return cnt;
}

static struct pmic_device_attribute s2p_irq_attr[] = {
	PMIC_ATTR(irq_read_all, S_IRUGO, s2p_irq_read_show, NULL),
};

int s2p_create_irq_sysfs(struct s2p_dev *sdev, const char *mfd_dev_name, const uint32_t dev_id)
{
	struct device *dev = sdev->dev;
	struct s2p_pmic_irq_list *irq_list = sdev->irq_list;
	struct device *sysfs_dev = NULL;
	char device_name[S2P_NAME_MAX] = {0, };
	int ret = 0, i = 0;
	uint32_t cnt = 0;

	/* Dynamic allocation for device name */
	cnt += snprintf(irq_list->sysfs_name, S2P_NAME_MAX - 1, "%s", mfd_dev_name);
	if (dev_id)
		snprintf(irq_list->sysfs_name + cnt, S2P_NAME_MAX - 1, "-%d", dev_id);

	snprintf(device_name, S2P_NAME_MAX - 1, "%s-irq@%s",
			irq_list->sysfs_name, dev_name(dev));

	sysfs_dev = pmic_device_create(sdev, device_name);
	irq_list->irq_sysfs_dev = sysfs_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(s2p_irq_attr); i++) {
		ret = device_create_file(sysfs_dev, &s2p_irq_attr[i].dev_attr);
		if (ret)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(sysfs_dev, &s2p_irq_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_create_irq_sysfs);

void s2p_destroy_workqueue(struct s2p_irq_info *pm_irq_info, const uint32_t type_size)
{
	uint32_t i = 0;

	for (i = 0; i < type_size; i++) {
		if (pm_irq_info[i].notifier_wqueue)
			destroy_workqueue(pm_irq_info[i].notifier_wqueue);
	}
}
EXPORT_SYMBOL_GPL(s2p_destroy_workqueue);

void s2p_remove_irq_sysfs_entries(struct device *sysfs_dev)
{
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(s2p_irq_attr); i++)
		device_remove_file(sysfs_dev, &s2p_irq_attr[i].dev_attr);
	pmic_device_destroy(sysfs_dev->devt);
}
EXPORT_SYMBOL_GPL(s2p_remove_irq_sysfs_entries);
#endif /* CONFIG_DRV_SAMSUNG_PMIC */
