/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_sysfs.h"
#include "hts_ops.h"
#include "hts_var.h"
#include "hts_pmu.h"
#include "hts_devfs.h"
#include "hts_common.h"

#include <linux/sysfs.h>

// base path

static ssize_t enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.enable);

	return scnprintf(buf, PAGE_SIZE, "%d\n", drvdata->devfs.enabled);
}

static ssize_t enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.enable);
	int value;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	hts_devfs_control_tick(drvdata, value);

	return count;
}

// conf path

static ssize_t log_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.log_mask);

	return scnprintf(buf, PAGE_SIZE, "%*pbl\n", cpumask_pr_args(&drvdata->devfs.log_mask));
}

static ssize_t log_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.log_mask);
	cpumask_t value;

	if (cpulist_parse(buf, &value) != 0)
		return -EINVAL;

	drvdata->devfs.log_mask = value;

	return count;
}

static ssize_t ref_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.ref_mask);

	return scnprintf(buf, PAGE_SIZE, "%*pbl\n", cpumask_pr_args(&drvdata->devfs.ref_mask));
}

static ssize_t ref_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.ref_mask);
	cpumask_t value;

	if (cpulist_parse(buf, &value) != 0)
		return -EINVAL;

	drvdata->devfs.ref_mask = value;

	return count;
}

static ssize_t def_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.def_mask);

	return scnprintf(buf, PAGE_SIZE, "%*pbl\n", cpumask_pr_args(&drvdata->devfs.def_mask));
}

static ssize_t def_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.def_mask);
	cpumask_t value;

	if (cpulist_parse(buf, &value) != 0)
		return -EINVAL;

	drvdata->devfs.def_mask = value;

	return count;
}

static ssize_t backup_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.backup);
	struct hts_percpu *percpu = &drvdata->percpu;
	int cpu, written = 0;

	for_each_online_cpu(cpu) {
		written += scnprintf(buf + written,
				PAGE_SIZE - written,
				"CPU%d - KNOB_CTRL1 %16lx, KNOB_CTRL2 %16lx\n",
				cpu,
				percpu->backup[cpu][KNOB_CTRL1],
				percpu->backup[cpu][KNOB_CTRL2]);
	}

	return written;
}

static ssize_t backup_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.backup);
	struct hts_percpu *percpu = &drvdata->percpu;
	int cpu, knob_index;
	unsigned long value;

	if (sscanf(buf, "%d %d %lx", &cpu, &knob_index, &value) != 3)
		return -EINVAL;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu)
		return -EINVAL;

	if (knob_index < 0 ||
		KNOB_COUNT <= knob_index)
		return -EINVAL;

	percpu->backup[cpu][knob_index] = value;

	return count;
}

static ssize_t core_thre_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.core_thre);
	struct hts_devfs *devfs = &drvdata->devfs;

	return  scnprintf(buf,
			PAGE_SIZE,
			"%lu\n",
			devfs->core_active_thre);
}

static ssize_t core_thre_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.core_thre);
	struct hts_devfs *devfs = &drvdata->devfs;
	unsigned long value;

	if (sscanf(buf, "%lu", &value) != 1)
		return -EINVAL;

	devfs->core_active_thre = value;

	return count;
}

static ssize_t total_thre_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.total_thre);
	struct hts_devfs *devfs = &drvdata->devfs;

	return scnprintf(buf,
			PAGE_SIZE,
			"%lu\n",
			devfs->total_active_thre);
}

static ssize_t total_thre_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.total_thre);
	struct hts_devfs *devfs = &drvdata->devfs;
	unsigned long value;

	if (sscanf(buf, "%lu", &value) != 1)
		return -EINVAL;

	devfs->total_active_thre = value;

	return count;
}

static ssize_t group_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.group_mask);
	struct hts_devfs *devfs = &drvdata->devfs;
	int i, written = 0, mask_count = hts_var_get_mask_count(devfs);

	for (i = 0; i < mask_count; ++i) {
		written += scnprintf(buf + written,
				PAGE_SIZE - written,
				"GROUP%d - %*pbl\n",
				i,
				cpumask_pr_args(hts_var_get_mask(devfs, i)));
	}

	return written;
}

static ssize_t group_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.group_mask);
	struct hts_devfs *devfs = &drvdata->devfs;
	struct cpumask value;

	if (cpulist_parse(buf, &value) != 0)
		return -EINVAL;

	hts_var_add_cpumask(devfs, &value);

	return count;
}

static ssize_t enable_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.enable_mask);
	struct hts_devfs *devfs = &drvdata->devfs;
	int i, j, written = 0;

	for (i = 0; i < devfs->mask_count; ++i) {
		written += scnprintf(buf + written,
				PAGE_SIZE - written,
				"GROUP%d -",
				i);

		for (j = 0; j < KNOB_COUNT; ++j) {
			written += scnprintf(buf + written,
					PAGE_SIZE - written,
					" %d",
					devfs->available_cpu[i][j]);
		}

		written += scnprintf(buf + written,
				PAGE_SIZE - written,
				"\n");
	}

	return written;
}

static ssize_t enable_mask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.enable_mask);
	struct hts_devfs *devfs = &drvdata->devfs;
	int group, knob, enable;

	if (sscanf(buf, "%d %d %d", &group, &knob, &enable) != 3)
		return -EINVAL;

	if (enable)
		hts_var_enable_mask(devfs, group, knob);
	else
		hts_var_disable_mask(devfs, group, knob);

	return count;
}

static ssize_t cgroup_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.cgroup);
	struct hts_devfs *devfs = &drvdata->devfs;
	int i, written = 0, cgroup_count = hts_var_get_cgroup_count(devfs);

	for (i = 0; i < cgroup_count; ++i) {
		written += scnprintf(buf + written,
				PAGE_SIZE - written,
				"CGROUP%d - %d\n",
				i,
				hts_var_get_cgroup(devfs, i));
	}

	return written;
}

static ssize_t cgroup_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.cgroup);
	char cgroup_name[32];

	if (sscanf(buf, "%31s", cgroup_name) != 1)
		return -EINVAL;

	hts_var_add_cgroup(drvdata, cgroup_name);

	return count;
}


// knob path

static ssize_t extended_control_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.extended_control);
	struct hts_devfs *devfs = &drvdata->devfs;
	struct platform_device *pdev = drvdata->pdev;
	unsigned long value = 0;
	int cpu, ret, mask_index, written = 0;

	for_each_online_cpu(cpu) {
		mask_index = hts_var_get_mask_index(devfs, cpu);

		if (0 <= mask_index &&
			devfs->available_cpu[mask_index][KNOB_CTRL1]) {

			ret = smp_call_function_single(cpu, hts_read_ectlr, &value, 1);
			if (ret) {
				dev_err(&pdev->dev, "IPI function couldn't be running CPU%d - %d", cpu, ret);
				return ret;
			}
		}
		else
			value = 0;

		written += scnprintf(buf + written, PAGE_SIZE - written, "CPU%d : %lx\n", cpu, value);
	}

	return written;
}

static ssize_t extended_control_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.extended_control);
	struct task_struct *target_task;
	struct hts_config *config;
	unsigned long value;
	int pid, group_idx, mask_count;

	if (sscanf(buf, "%d %d %lx", &pid, &group_idx, &value) != 3)
		return -EINVAL;

	mask_count = hts_var_get_mask_count(&drvdata->devfs);
	if (group_idx < 0 ||
		mask_count <= group_idx)
		return -EINVAL;

	target_task = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	if (target_task == NULL)
		return -ENOENT;

	config = hts_task_get_or_alloc_config(target_task);
	config->value[group_idx][KNOB_CTRL1] = value;

	put_task_struct(target_task);

	return count;
}

static ssize_t extended_control2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.extended_control2);
	struct hts_devfs *devfs = &drvdata->devfs;
	struct platform_device *pdev = drvdata->pdev;
	unsigned long value = 0;
	int cpu, ret, mask_index, written = 0;

	for_each_online_cpu(cpu) {
		mask_index = hts_var_get_mask_index(devfs, cpu);

		if (0 <= mask_index &&
			devfs->available_cpu[mask_index][KNOB_CTRL2]) {

			ret = smp_call_function_single(cpu, hts_read_ectlr2, &value, 1);
			if (ret) {
				dev_err(&pdev->dev, "IPI function couldn't be running CPU%d - %d", cpu, ret);
				return ret;
			}
		}
		else
			value = 0;

		written += scnprintf(buf + written, PAGE_SIZE - written, "CPU%d : %lx\n", cpu, value);
	}

	return written;
}

static ssize_t extended_control2_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.extended_control2);
	struct task_struct *target_task;
	struct hts_config *config;
	unsigned long value;
	int pid, group_idx, mask_count;

	if (sscanf(buf, "%d %d %lx", &pid, &group_idx, &value) != 3)
		return -EINVAL;

	mask_count = hts_var_get_mask_count(&drvdata->devfs);
	if (group_idx < 0 ||
		mask_count <= group_idx)
		return -EINVAL;

	target_task = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	if (target_task == NULL)
		return -ENOENT;

	config = hts_task_get_or_alloc_config(target_task);
	config->value[group_idx][KNOB_CTRL2] = value;

	put_task_struct(target_task);

	return count;
}

// stat path

static ssize_t transition_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.transition);
	struct hts_percpu *percpu = &drvdata->percpu;
	int cpu, written = 0;

	for_each_online_cpu(cpu) {
		written += scnprintf(buf + written,
				PAGE_SIZE - written,
				"CPU%d - %lu\n",
				cpu,
				percpu->transition_count[cpu]);
	}

	return written;
}

static ssize_t written_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hts_drvdata *drvdata = container_of(attr, struct hts_drvdata, sysfs.written);
	struct hts_percpu *percpu = &drvdata->percpu;
	int cpu, written = 0;

	for_each_online_cpu(cpu) {
		written += scnprintf(buf + written,
				PAGE_SIZE - written,
				"CPU%d : %lu, %lu\n",
				cpu,
				percpu->written_count[cpu][KNOB_CTRL1],
			       	percpu->written_count[cpu][KNOB_CTRL2]);
	}

	return written;
}

int hts_sysfs_initialize(struct hts_drvdata *drvdata)
{
	struct hts_sysfs *sysfs = &drvdata->sysfs;
	struct platform_device *pdev = drvdata->pdev;
	struct attribute_group hts_conf_group, hts_knob_group, hts_stat_group;
	struct attribute *hts_conf_attrs[10], *hts_knob_attrs[3], *hts_stat_attrs[3];
	int ret = 0;

	sysfs->enable				= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(enable,
												VERIFY_OCTAL_PERMISSIONS(0644),
												enable_show,
												enable_store);

	if (device_create_file(&pdev->dev, &sysfs->enable)) {
		dev_err(&pdev->dev, "Couldn't create enable sysfs");
		ret = -EINVAL;
		goto err_create_base;
	}

	sysfs->log_mask				= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(log_mask,
												VERIFY_OCTAL_PERMISSIONS(0644),
												log_mask_show,
												log_mask_store);

	sysfs->ref_mask				= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(ref_mask,
												VERIFY_OCTAL_PERMISSIONS(0644),
												ref_mask_show,
												ref_mask_store);

	sysfs->def_mask				= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(def_mask,
												VERIFY_OCTAL_PERMISSIONS(0644),
												def_mask_show,
												def_mask_store);

	sysfs->backup				= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(backup,
												VERIFY_OCTAL_PERMISSIONS(0644),
												backup_show,
												backup_store);

	sysfs->core_thre			= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(core_thre,
												VERIFY_OCTAL_PERMISSIONS(0644),
												core_thre_show,
												core_thre_store);

	sysfs->total_thre			= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(total_thre,
												VERIFY_OCTAL_PERMISSIONS(0644),
												total_thre_show,
												total_thre_store);

	sysfs->group_mask			= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(group_mask,
												VERIFY_OCTAL_PERMISSIONS(0644),
												group_mask_show,
												group_mask_store);

	sysfs->enable_mask			= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(enable_mask,
												VERIFY_OCTAL_PERMISSIONS(0644),
												enable_mask_show,
												enable_mask_store);

	sysfs->cgroup				= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(cgroup,
												VERIFY_OCTAL_PERMISSIONS(0644),
												cgroup_show,
												cgroup_store);

	hts_conf_attrs[0]			= &sysfs->log_mask.attr;
	hts_conf_attrs[1]			= &sysfs->ref_mask.attr;
	hts_conf_attrs[2]			= &sysfs->def_mask.attr;
	hts_conf_attrs[3]			= &sysfs->backup.attr;
	hts_conf_attrs[4]			= &sysfs->core_thre.attr;
	hts_conf_attrs[5]			= &sysfs->total_thre.attr;
	hts_conf_attrs[6]			= &sysfs->group_mask.attr;
	hts_conf_attrs[7]			= &sysfs->enable_mask.attr;
	hts_conf_attrs[8]			= &sysfs->cgroup.attr;
	hts_conf_attrs[9]			= NULL;

	hts_conf_group.name			= "conf";
	hts_conf_group.attrs			= hts_conf_attrs;

	if (sysfs_create_group(&drvdata->pdev->dev.kobj, &hts_conf_group)) {
		dev_err(&drvdata->pdev->dev, "Couldn't create conf sysfs");
		ret = -EINVAL;
		goto err_create_conf;
	}

	sysfs->extended_control			= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(extended_control,
												VERIFY_OCTAL_PERMISSIONS(0644),
												extended_control_show,
												extended_control_store);

	sysfs->extended_control2		= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(extended_control2,
												VERIFY_OCTAL_PERMISSIONS(0644),
												extended_control2_show,
												extended_control2_store);

	hts_knob_attrs[0]			= &sysfs->extended_control.attr;
	hts_knob_attrs[1]			= &sysfs->extended_control2.attr;
	hts_knob_attrs[2]			= NULL;

	hts_knob_group.name			= "knob";
	hts_knob_group.attrs			= hts_knob_attrs;

	if (sysfs_create_group(&drvdata->pdev->dev.kobj, &hts_knob_group)) {
		dev_err(&drvdata->pdev->dev, "Couldn't create knob sysfs");
		ret = -EINVAL;
		goto err_create_knob;
	}

	sysfs->transition			= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(transition,
												VERIFY_OCTAL_PERMISSIONS(0644),
												transition_show,
												NULL);

	sysfs->written				= (struct device_attribute)__ATTR_IGNORE_LOCKDEP(written,
												VERIFY_OCTAL_PERMISSIONS(0644),
												written_show,
												NULL);

	hts_stat_attrs[0]			= &sysfs->transition.attr;
	hts_stat_attrs[1]			= &sysfs->written.attr;
	hts_stat_attrs[2]			= NULL;

	hts_stat_group.name			= "stat";
	hts_stat_group.attrs			= hts_stat_attrs;

	if (sysfs_create_group(&drvdata->pdev->dev.kobj, &hts_stat_group)) {
		dev_err(&drvdata->pdev->dev, "Couldn't create stat sysfs");
		ret = -EINVAL;
		goto err_create_stat;
	}

	return 0;

err_create_stat:
	sysfs_remove_group(&drvdata->pdev->dev.kobj, &hts_knob_group);
err_create_knob:
	sysfs_remove_group(&drvdata->pdev->dev.kobj, &hts_conf_group);
err_create_conf:
	device_remove_file(&pdev->dev, &sysfs->enable);
err_create_base:

	return ret;
}
