/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_vh.h"
#include "hts_logic.h"
#include "hts_common.h"

#include <trace/hooks/fpsimd.h>
#include <trace/hooks/sched.h>

#include <linux/dev_printk.h>

static void hts_vh_is_fpsimd_save(void *data,
		struct task_struct *prev, struct task_struct *next)
{
	struct hts_drvdata *drvdata = (struct hts_drvdata *)data;
	int cpu = task_cpu(prev);

	hts_logic_read_task_data(drvdata, prev);
	hts_logic_reflect_task_data(drvdata, cpu, next);
}

int hts_vh_register_tick(struct hts_drvdata *drvdata)
{
	return register_trace_android_vh_is_fpsimd_save(hts_vh_is_fpsimd_save, drvdata);
}

int hts_vh_unregister_tick(struct hts_drvdata *drvdata)
{
	if (drvdata->etc.enabled_count <= 0)
		return 0;

	return unregister_trace_android_vh_is_fpsimd_save(hts_vh_is_fpsimd_save, drvdata);
}

static void hts_vh_sched_fork_init(void *data, struct task_struct *p)
{
	hts_task_get_or_alloc_config(p);
}

static void hts_vh_free_task(void *data, struct task_struct *p)
{
	hts_task_free_config(p);
}

int hts_vh_register(struct hts_drvdata *drvdata)
{
	int ret = 0;
	struct device *dev = &drvdata->pdev->dev;

	ret = register_trace_android_vh_free_task(hts_vh_free_task, NULL);
	if (ret) {
		dev_err(dev, "Couldn't register hook for free task");
		return ret;
	}

	ret = register_trace_android_rvh_sched_fork_init(hts_vh_sched_fork_init, NULL);
	if (ret) {
		dev_err(dev, "Couldn't register hook for fork init");
		return ret;
	}

	return 0;
}
