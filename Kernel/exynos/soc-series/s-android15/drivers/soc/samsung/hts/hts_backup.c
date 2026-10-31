/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_backup.h"
#include "hts_ops.h"
#include "hts_var.h"

static void hts_backup_reset_value_percpu(void *data)
{
	int mask_index, cpu = smp_processor_id();
	struct hts_drvdata *drvdata = (struct hts_drvdata *)data;
	struct hts_devfs *devfs = &drvdata->devfs;
	struct hts_percpu *percpu = &drvdata->percpu;

	mask_index = hts_var_get_mask_index(&drvdata->devfs, cpu);
	if (mask_index < 0)
		return;

	if (devfs->available_cpu[mask_index][KNOB_CTRL1])
		hts_write_ectlr(&percpu->backup[cpu][KNOB_CTRL1]);

	if (devfs->available_cpu[mask_index][KNOB_CTRL2])
		hts_write_ectlr2(&percpu->backup[cpu][KNOB_CTRL2]);
}

static void hts_backup_default_value_percpu(void *data)
{
	int  mask_index, cpu = smp_processor_id();
	struct hts_drvdata *drvdata = (struct hts_drvdata *)data;
	struct hts_devfs *devfs = &drvdata->devfs;
	struct hts_percpu *percpu = &drvdata->percpu;

	mask_index = hts_var_get_mask_index(&drvdata->devfs, cpu);
	if (mask_index < 0)
		return;

	if (devfs->available_cpu[mask_index][KNOB_CTRL1])
		hts_read_ectlr(&percpu->backup[cpu][KNOB_CTRL1]);

	if (devfs->available_cpu[mask_index][KNOB_CTRL2])
		hts_read_ectlr2(&percpu->backup[cpu][KNOB_CTRL2]);
}

void hts_backup_reset_value(struct hts_drvdata *drvdata)
{
	int cpu, ret;
	struct platform_device *pdev;

	if (drvdata == NULL) {
		pr_err("HTS : Invalid data");
		return;
	}

	pdev = drvdata->pdev;

	for_each_online_cpu(cpu) {
		ret = smp_call_function_single(cpu, hts_backup_reset_value_percpu, drvdata, 1);
		if (ret)
			dev_err(&pdev->dev, "Couldn't process reset value CPU%d - %d", cpu, ret);
	}
}

int hts_backup_default_value(struct hts_drvdata *drvdata)
{
	int cpu,  ret;
	struct platform_device *pdev;

	if (drvdata == NULL) {
		pr_err("HTS : Invalid data");
		return -EINVAL;
	}

	pdev = drvdata->pdev;

	for_each_online_cpu(cpu) {
		ret = smp_call_function_single(cpu, hts_backup_default_value_percpu, drvdata, 1);
		if (ret) {
			dev_err(&pdev->dev, "Couldn't backup value CPU%d - %d", cpu, ret);
			return -EINVAL;
		}
	}

	return 0;
}
