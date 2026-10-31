/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_var.h"

#include <linux/cgroup.h>

// mask group

int hts_var_get_mask_count(struct hts_devfs *devfs)
{
	if (devfs == NULL)
		return -1;

	return devfs->mask_count;
}

int hts_var_get_mask_index(struct hts_devfs *devfs, int cpu)
{
	int i;

	if (devfs == NULL)
		return -1;

	for (i = 0; i < devfs->mask_count; ++i) {
		if (cpumask_test_cpu(cpu, &devfs->mask_cpu[i]))
			return i;
	}

	return -1;
}

struct cpumask *hts_var_get_mask(struct hts_devfs *devfs, int index)
{
	if (devfs == NULL ||
		index < 0 ||
		devfs->mask_count <= index)
		return NULL;

	return &devfs->mask_cpu[index];
}

void hts_var_add_cpumask(struct hts_devfs *devfs, struct cpumask *mask)
{
	if (devfs == NULL ||
		devfs->mask_count == MAX_GROUP_ENTRIES_COUNT)
		return;

	cpumask_copy(&devfs->mask_cpu[devfs->mask_count], mask);
	devfs->mask_count++;
}

void hts_var_add_mask(struct hts_devfs *devfs, unsigned long mask)
{
	int bit;
	struct cpumask *cpu_mask;

	if (devfs == NULL ||
		devfs->mask_count == MAX_GROUP_ENTRIES_COUNT)
		return;

	cpu_mask = &devfs->mask_cpu[devfs->mask_count];
	for_each_set_bit(bit, &mask, CONFIG_VENDOR_NR_CPUS)
		cpumask_set_cpu(bit, cpu_mask);

	devfs->mask_count++;
}

void hts_var_clear_mask(struct hts_devfs *devfs)
{
	if (devfs == NULL)
		return;

	devfs->mask_count = 0;
}

static void __hts_var_enable_mask(struct hts_devfs *devfs, int group_index, int knob_index, int value)
{
	if (devfs == NULL)
		return;

	if (group_index < 0 ||
		MAX_GROUP_ENTRIES_COUNT <= group_index ||
		knob_index < 0 ||
		KNOB_COUNT <= knob_index)
		return;

	devfs->available_cpu[group_index][knob_index] = value;
}

void hts_var_disable_mask(struct hts_devfs *devfs, int group_index, int knob_index)
{
	__hts_var_enable_mask(devfs, group_index, knob_index, 0);
}

void hts_var_enable_mask(struct hts_devfs *devfs, int group_index, int knob_index)
{
	__hts_var_enable_mask(devfs, group_index, knob_index, 1);
}

// cgroup

static void hts_var_add_cgroup_id(struct hts_devfs *devfs, int cgroup)
{
	if (devfs == NULL ||
		devfs->cgroup_count == MAX_CGROUP_ENTRIES_COUNT)
		return;

	devfs->target_cgroup[devfs->cgroup_count] = cgroup;
	devfs->cgroup_count++;
}

static void hts_var_map_css_partid(struct hts_devfs *devfs,
				struct cgroup_subsys_state *css,
				char *tmp,
				int pathlen,
				char *cgroup_name)
{
	cgroup_path(css->cgroup, tmp, pathlen);

	if (strcmp(tmp, cgroup_name))
		return;

	hts_var_add_cgroup_id(devfs, css->id);
}

static void hts_var_map_css_children(struct hts_devfs *devfs,
				struct cgroup_subsys_state *css,
				char *tmp,
				int pathlen,
				char *cgroup_name)
{
	struct cgroup_subsys_state *child;

	list_for_each_entry_rcu(child, &css->children, sibling) {
		if (!child ||
			!child->cgroup)
			continue;

		hts_var_map_css_partid(devfs, child, tmp, pathlen, cgroup_name);
		hts_var_map_css_children(devfs, child, tmp, pathlen, cgroup_name);
	}
}

int hts_var_get_cgroup_count(struct hts_devfs *devfs)
{
	if (devfs == NULL)
		return -1;

	return devfs->cgroup_count;
}

int hts_var_get_cgroup(struct hts_devfs *devfs, int index)
{
	if (devfs == NULL ||
		index < 0 ||
		devfs->cgroup_count <= index)
		return -1;

	return devfs->target_cgroup[index];
}

int hts_var_get_cgroup_index(struct hts_devfs *devfs, int cgroup)
{
	int i;

	if (devfs == NULL)
		return -1;

	for (i = 0; i < devfs->cgroup_count; ++i) {
		if (devfs->target_cgroup[i] == cgroup)
			return i;
	}

	return -1;
}

void hts_var_add_cgroup(struct hts_drvdata *drvdata, char *cgroup_name)
{
	struct cgroup *cg;
	struct cgroup_subsys_state *css;
	struct platform_device *pdev = drvdata->pdev;
	struct hts_devfs *devfs = &drvdata->devfs;
	char buf[CGROUP_BUFFER_LENGTH];

	if (devfs == NULL ||
		devfs->cgroup_count == MAX_CGROUP_ENTRIES_COUNT) {
		dev_err(&pdev->dev, "Couldn't add targeted cgroup anymore");
		return;
	}

	cg = task_cgroup(current, cpuset_cgrp_id);
	if (IS_ERR_OR_NULL(cg)) {
		dev_err(&pdev->dev, "Couldn't get cgroup system");
		return;
	}

	rcu_read_lock();
	css = rcu_dereference(cg->subsys[cpuset_cgrp_id]);
	if (!IS_ERR_OR_NULL(css)) {
		hts_var_map_css_partid(devfs,
					css,
					buf,
					CGROUP_BUFFER_LENGTH - 1,
					cgroup_name);
		hts_var_map_css_children(devfs,
					css,
					buf,
					CGROUP_BUFFER_LENGTH - 1,
					cgroup_name);
	}
	rcu_read_unlock();
}

void hts_var_clear_cgroup(struct hts_devfs *devfs)
{
	if (devfs == NULL)
		return;

	devfs->cgroup_count = 0;
}

// predefined

//void hts_var_predefined_value(
