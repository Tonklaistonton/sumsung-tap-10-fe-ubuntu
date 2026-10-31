/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/printk.h>
#include <linux/cpuidle.h>
#include <linux/sysfs.h>
#include <soc/samsung/bts.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos-devfreq.h>
#endif
#include <soc/samsung/exynos/exynos-soc.h>

#include "npu-dvfs.h"
#include "dsp-dhcp.h"
#include "npu-governor.h"

static ssize_t npu_show_attrs_dvfs_sysfs(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_dvfs_sysfs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static int npu_dvfs_sysfs_create(struct npu_system *system);

static struct device_attribute npu_dvfs_sysfs_attr =
	__ATTR(npu_freq_info, 0664,
		npu_show_attrs_dvfs_sysfs,
		npu_store_attrs_dvfs_sysfs);

int npu_dvfs_get_ip_max_freq(struct vs4l_freq_param *param)
{
	int ret = 0;
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
	param->ip_count = 0;
	if (!info->activated || info->is_dvfs_cmd)
		return -ENODATA;
	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		strcpy(param->ip[param->ip_count].name, d->name);
		param->ip[param->ip_count].max_freq = d->max_freq;
		param->ip_count++;
	}
	mutex_unlock(&info->exec_lock);
#endif
	return ret;
}

int npu_dvfs_set_freq_boost(struct npu_scheduler_dvfs_info *d, void *req, u32 freq)
{
	u32 *cur_freq;
	struct exynos_pm_qos_request *target_req;

	WARN_ON(!d);

	if (req)
		target_req = (struct exynos_pm_qos_request *)req;
	else
		target_req = &d->qos_req_min;

	if (target_req->exynos_pm_qos_class == get_pm_qos_max(d->name))
		cur_freq = &d->limit_max;
	else if (target_req->exynos_pm_qos_class == get_pm_qos_min(d->name))
		cur_freq = &d->limit_min;
	else {
		npu_err("Invalid pm_qos class : %d\n", target_req->exynos_pm_qos_class);
		return -EINVAL;
	}

	//if (freq && *cur_freq == freq) {
		//npu_dbg("stick to current freq : %d\n", cur_freq);
	//	return 0;
	//}

	if (d->max_freq < freq)
		freq = d->max_freq;
	else if (d->min_freq > freq)
		freq = d->min_freq;

	npu_dvfs_pm_qos_update_request_boost(target_req, freq);

	*cur_freq = freq;

	d->cur_freq = npu_dvfs_devfreq_get_domain_freq(d);

	return freq;
}

int npu_dvfs_set_freq(struct npu_scheduler_dvfs_info *d, void *req, u32 freq)
{
	u32 *cur_freq;
	struct exynos_pm_qos_request *target_req;

	WARN_ON(!d);

	if (req)
		target_req = (struct exynos_pm_qos_request *)req;
	else
		target_req = &d->qos_req_min;

	if (target_req->exynos_pm_qos_class == get_pm_qos_max(d->name))
		cur_freq = &d->limit_max;
	else if (target_req->exynos_pm_qos_class == get_pm_qos_min(d->name))
		cur_freq = &d->limit_min;
	else {
		npu_err("Invalid pm_qos class : %d\n", target_req->exynos_pm_qos_class);
		return -EINVAL;
	}

	//if (freq && *cur_freq == freq) {
		//npu_dbg("stick to current freq : %d\n", cur_freq);
	//	return 0;
	//}

	if (d->max_freq < freq)
		freq = d->max_freq;
	else if (d->min_freq > freq)
		freq = d->min_freq;

	npu_dvfs_pm_qos_update_request(target_req, freq);

	*cur_freq = freq;

	d->cur_freq = npu_dvfs_devfreq_get_domain_freq(d);

	return freq;
}

u32 npu_dvfs_devfreq_get_domain_freq(struct npu_scheduler_dvfs_info *d)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	struct platform_device *pdev;
	struct exynos_devfreq_data *data;
	u32 devfreq_type;

	pdev = d->dvfs_dev;
	if (!pdev) {
		npu_err("platform_device is NULL!\n");
		return d->min_freq;
	}

	data = platform_get_drvdata(pdev);
	if (!data) {
		npu_err("exynos_devfreq_data is NULL!\n");
		return d->min_freq;
	}

	devfreq_type = data->devfreq_type;

	return exynos_devfreq_get_domain_freq(devfreq_type);
#else
	return 0;
#endif
}

void npu_dvfs_pm_qos_add_request(struct exynos_pm_qos_request *req,
					int exynos_pm_qos_class, s32 value)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_add_request(req, exynos_pm_qos_class, value);
#endif

}

void npu_dvfs_pm_qos_update_request_boost(struct exynos_pm_qos_request *req,
							s32 new_value)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(req, new_value);
#endif
}

void npu_dvfs_pm_qos_update_request(struct exynos_pm_qos_request *req,
							s32 new_value)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_update_request(req, new_value);
#endif
}

int npu_dvfs_pm_qos_get_class(struct exynos_pm_qos_request *req)
{
	return req->exynos_pm_qos_class;
}

/* dt_name has prefix "samsung,npudvfs-" */
#define NPU_DVFS_CMD_PREFIX	"samsung,npudvfs-"
static int npu_dvfs_get_cmd_map(struct npu_system *system, struct dvfs_cmd_list *cmd_list)
{
	int i, ret = 0;
	struct dvfs_cmd_contents *cmd_clock = NULL;
	char dvfs_name[64], clock_name[64];
	char **cmd_dvfs;
	struct device *dev;

	dvfs_name[0] = '\0';
	clock_name[0] = '\0';
	strcpy(dvfs_name, NPU_DVFS_CMD_PREFIX);
	strcpy(clock_name, NPU_DVFS_CMD_PREFIX);
	strcat(strcat(dvfs_name, cmd_list->name), "-dvfs");
	strcat(strcat(clock_name, cmd_list->name), "-clock");

	dev = &(system->pdev->dev);
	cmd_list->count = of_property_count_strings(
			dev->of_node, dvfs_name);
	if (cmd_list->count <= 0) {
		probe_warn("invalid dvfs_cmd list by %s\n", dvfs_name);
		cmd_list->list = NULL;
		cmd_list->count = 0;
		ret = -ENODEV;
		goto err_exit;
	}
	probe_info("%s dvfs %d commands\n", dvfs_name, cmd_list->count);

	cmd_list->list = (struct dvfs_cmd_map *)devm_kmalloc(dev,
				(cmd_list->count + 1) * sizeof(struct dvfs_cmd_map),
				GFP_KERNEL);
	if (!cmd_list->list) {
		probe_err("failed to alloc for dvfs cmd map\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	(cmd_list->list)[cmd_list->count].name = NULL;

	cmd_dvfs = (char **)devm_kmalloc(dev,
			cmd_list->count * sizeof(char *), GFP_KERNEL);
	if (!cmd_dvfs) {
		probe_err("failed to alloc for dvfs_cmd devfreq for %s\n", dvfs_name);
		ret = -ENOMEM;
		goto err_exit;
	}
	ret = of_property_read_string_array(dev->of_node, dvfs_name, (const char **)cmd_dvfs, cmd_list->count);
	if (ret < 0) {
		probe_err("failed to get dvfs_cmd for %s (%d)\n", dvfs_name, ret);
		ret = -EINVAL;
		goto err_dvfs;
	}

	cmd_clock = (struct dvfs_cmd_contents *)devm_kmalloc(dev,
			cmd_list->count * sizeof(struct dvfs_cmd_contents), GFP_KERNEL);
	if (!cmd_clock) {
		probe_err("failed to alloc for dvfs_cmd clock for %s\n", clock_name);
		ret = -ENOMEM;
		goto err_dvfs;
	}
	ret = of_property_read_u32_array(dev->of_node, clock_name, (u32 *)cmd_clock,
			cmd_list->count * NPU_DVFS_CMD_LEN);
	if (ret) {
		probe_err("failed to get reg_cmd for %s (%d)\n", clock_name, ret);
		ret = -EINVAL;
		goto err_clock;
	}

	for (i = 0; i < cmd_list->count; i++) {
		(*(cmd_list->list + i)).name = *(cmd_dvfs + i);
		memcpy((void *)(&((cmd_list->list + i)->contents)),
				(void *)(&cmd_clock[i]), sizeof(struct dvfs_cmd_contents));

		probe_info("copy %s cmd (%lu)\n", *(cmd_dvfs + i), sizeof(u32) * NPU_DVFS_CMD_LEN);
	}
err_clock:
	devm_kfree(dev, cmd_clock);
err_dvfs:
	devm_kfree(dev, cmd_dvfs);
err_exit:
	return ret;
}

static struct dvfs_cmd_list npu_dvfs_cmd_list[] = {
	{	.name = "open",	.list = NULL,	.count = 0	},
	{	.name = "close",	.list = NULL,	.count = 0	},
	{	.name = NULL,		.list = NULL,	.count = 0	}
};

int npu_dvfs_init_cmd_list(struct npu_system *system, struct npu_scheduler_info *info)
{
	int ret = 0;
	int i;

	info->dvfs_list = (struct dvfs_cmd_list *)npu_dvfs_cmd_list;

	for (i = 0; ((info->dvfs_list) + i)->name; i++) {
		if (npu_dvfs_get_cmd_map(system, (info->dvfs_list) + i) == -ENODEV)
			probe_info("No cmd for %s\n", ((info->dvfs_list) + i)->name);
		else
			probe_info("register cmd for %s\n", ((info->dvfs_list) + i)->name);
	}

	if (npu_dvfs_sysfs_create(system)) {
		probe_info("npu_qos_sysfs create failed\n");
		return EBUSY;
	}

	return ret;
}

int npu_dvfs_cmd(struct npu_scheduler_info *info, const struct dvfs_cmd_list *cmd_list)
{
	int ret = 0;
	size_t i;
	char *name;
	struct dvfs_cmd_contents *t;
	struct npu_scheduler_dvfs_info *d;

	if (!cmd_list) {
		npu_info("No cmd for dvfs\n");
		/* no error, just skip */
		return 0;
	}

	info->is_dvfs_cmd = true;
	mutex_lock(&info->exec_lock);
	for (i = 0; i < cmd_list->count; ++i) {
		name = (cmd_list->list + i)->name;
		t = &((cmd_list->list + i)->contents);
		if (name) {
			npu_info("set %s %s as %d\n", name, t->cmd ? "max" : "min", t->freq);
			d = get_npu_dvfs_info(info, name);
			switch (t->cmd) {
			case NPU_DVFS_CMD_MIN:
				npu_dvfs_set_freq(d, &d->qos_req_min_dvfs_cmd, t->freq);
				break;
			case NPU_DVFS_CMD_MAX:
				{
					unsigned long f;
					struct dev_pm_opp *opp;

					f = ULONG_MAX;

					opp = dev_pm_opp_find_freq_floor(&d->dvfs_dev->dev, &f);
					if (IS_ERR(opp)) {
						npu_err("invalid max freq for %s\n", d->name);
						f = d->max_freq;
					} else {
						dev_pm_opp_put(opp);
					}

					d->max_freq = NPU_MIN(f, t->freq);
				}
				npu_dvfs_set_freq(d, &d->qos_req_max_dvfs_cmd, d->max_freq);
				break;
			default:
				break;
			}
		} else
			break;
	}
	mutex_unlock(&info->exec_lock);
	info->is_dvfs_cmd = false;
	return ret;
}

int npu_dvfs_cmd_map(struct npu_scheduler_info *info, const char *cmd_name)
{
	if(unlikely(!cmd_name)) {
		npu_err("Failed to get cmd_name\n");
		return -EINVAL;
	}

	return npu_dvfs_cmd(info, (const struct dvfs_cmd_list *)get_npu_dvfs_cmd_map(info, cmd_name));
}

static ssize_t npu_show_attrs_dvfs_sysfs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t node_count = 0, count = 0;
	u32 freq;
	u32 node_num;
	const char *node_name;
	int i, j, ret = 0;

	ret = of_property_read_u32(dev->of_node, "samsung,npunode-num",
				&node_num);
	if (ret) {
		count += scnprintf(&buf[count], PAGE_SIZE - count, "failed parsing npunode dt(npunode-num)\n");
		return count;
	}

	node_count = of_property_count_strings(dev->of_node, "samsung,npunode-names");
	if (IS_ERR_VALUE((unsigned long)node_count)) {
		count += scnprintf(&buf[count], PAGE_SIZE - count, "failed to count node name in %s node\n", dev->of_node->name);
		ret = -EINVAL;
		return count;
	}

	for(i = 0; i < node_count; i++) {
		ret = of_property_read_string_index(dev->of_node, "samsung,npunode-names",
					i, (const char **)&node_name);
		if (ret) {
			count += scnprintf(&buf[count], PAGE_SIZE - count, "failed to read node name %d from %s node : %d\n",
					i, dev->of_node->name, ret);
			return count;
		}
		count += scnprintf(&buf[count], PAGE_SIZE - count, "%s, ", node_name);
	}
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	for(i = 0; i < node_num; i++) {
		for(j = 0; j < node_count; j++) {
			ret = of_property_read_u32_index(dev->of_node, "samsung,npunode-freq",
						j + i * node_count, &freq);
			if (ret) {
				count += scnprintf(&buf[count], PAGE_SIZE - count, "failed to read npunode freq %d from %s node : %d\n",
						i + 1, dev->of_node->name, ret);
				return count;
			}
			count += scnprintf(&buf[count], PAGE_SIZE - count, "%u, ", freq);
		}
		count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");
	}
	return count;
}

static ssize_t npu_store_attrs_dvfs_sysfs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
    return 0;
}

static int npu_dvfs_sysfs_create(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device;

	device = container_of(system, struct npu_device, system);

	probe_info("npu dvfs-sysfs create\n");
	probe_info("creating sysfs file %s\n", npu_dvfs_sysfs_attr.attr.name);

	ret = sysfs_create_file(&device->dev->kobj, &npu_dvfs_sysfs_attr.attr);
	if (ret) {
		probe_err("failed(%d) to create sysfs for %s\n",
						ret, npu_dvfs_sysfs_attr.attr.name);
	}

	return ret;
}
