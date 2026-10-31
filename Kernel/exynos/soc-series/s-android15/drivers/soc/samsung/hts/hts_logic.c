/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define CREATE_TRACE_POINTS
#include <trace/events/hts.h>
#undef CREATE_TRACE_POINTS

#include "hts_logic.h"
#include "hts_pmu.h"
#include "hts_var.h"
#include "hts_ops.h"
#include "hts_common.h"

#include <linux/cgroup.h>

void hts_logic_read_task_data(struct hts_drvdata *drvdata, struct task_struct *task)
{
	struct hts_devfs *devfs = &drvdata->devfs;
	struct cgroup_subsys_state *css;
	int cgroup = 0, cpu = task_cpu(task);

	if (!task->pid ||
		!cpumask_test_cpu(cpu, &devfs->log_mask))
		return;

	rcu_read_lock();
	css = task_css(task, cpuset_cgrp_id);
	if (!IS_ERR_OR_NULL(css))
		cgroup = css->id;
	rcu_read_unlock();

	if (0 <= hts_var_get_cgroup_index(devfs, cgroup))
		hts_pmu_read_event(cpu, task);
}

void hts_logic_reflect_task_data(struct hts_drvdata *drvdata, int cpu, struct task_struct *task)
{
	struct hts_devfs *devfs = &drvdata->devfs;
	struct hts_percpu *percpu = &drvdata->percpu;
	struct hts_config *task_config = NULL;
	int mask_index, predefined_mode;
	unsigned long ectlr = 0, ectlr2 = 0;

	if (!task->pid ||
		!cpumask_test_cpu(cpu, &devfs->ref_mask))
		return;

	mask_index = hts_var_get_mask_index(devfs, cpu);
	if (mask_index < 0)
		return;

	predefined_mode = atomic_read(&devfs->predefined_mode);
	if (predefined_mode < 0) {
		if (devfs->ems_mode) {
			task_config = hts_task_get_config(task);
			if (task_config != NULL) {
				ectlr = task_config->value[mask_index][KNOB_CTRL1];
				ectlr2 = task_config->value[mask_index][KNOB_CTRL2];
			}
		}
	} else {
		ectlr = percpu->predefined[predefined_mode][mask_index][KNOB_CTRL1];
		ectlr2 = percpu->predefined[predefined_mode][mask_index][KNOB_CTRL2];
	}

	if (!ectlr)
		ectlr = percpu->backup[cpu][KNOB_CTRL1];
	if (!ectlr2)
		ectlr2 = percpu->backup[cpu][KNOB_CTRL2];

	percpu->transition_count[cpu]++;

	if (devfs->available_cpu[mask_index][KNOB_CTRL1]) {
		hts_write_ectlr(&ectlr);
		percpu->written_count[cpu][KNOB_CTRL1]++;
	}

	if (devfs->available_cpu[mask_index][KNOB_CTRL2]) {
		hts_write_ectlr2(&ectlr2);
		percpu->written_count[cpu][KNOB_CTRL2]++;
	}

	isb();
}
