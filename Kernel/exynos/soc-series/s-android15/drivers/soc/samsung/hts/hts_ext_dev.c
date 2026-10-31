/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_ext_dev.h"
#include "hts_devfs.h"
#include "hts_var.h"
#include "hts_common.h"

#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <soc/samsung/exynos-devfreq.h>
#include <linux/cpufreq.h>
#include <trace/hooks/cpufreq.h>
#include <linux/exynos-dsufreq.h>

#define	MAXIMUM_EVENT_BUFFER_SIZE			(4)

static struct hts_drvdata *drvdata_instance;

#if IS_ENABLED(CONFIG_SCHED_EMS)
#include <linux/ems.h>

static int hts_ext_dev_emstune_notifier(struct notifier_block *nb,
		unsigned long val, void *v)
{
	struct hts_drvdata *drvdata = container_of(nb,
						struct hts_drvdata, notifier.ems.noti_block);
	struct hts_devfs *devfs = &drvdata->devfs;
	struct hts_notifier_ems *ems_notifier = &drvdata->notifier.ems;
	int curset_mode = *(int *)v;

	if (ems_notifier->prev_mode == curset_mode)
		goto end;

	ems_notifier->prev_mode = curset_mode;
	devfs->ems_mode = (curset_mode == EMSTUNE_GAME_MODE);

end:

	return NOTIFY_OK;
}

static int hts_ext_dev_emstune_notifier_register(struct hts_drvdata *drvdata)
{
	struct hts_notifier_ems *ems_notifier = &drvdata->notifier.ems;
	struct hts_devfs *devfs = &drvdata->devfs;

	devfs->ems_mode = 0;
	ems_notifier->prev_mode = -1;
	ems_notifier->noti_block.notifier_call = hts_ext_dev_emstune_notifier;

	return emstune_register_notifier(&ems_notifier->noti_block);
}

static int hts_ext_dev_emstune_notifier_unregister(struct hts_drvdata *drvdata)
{
	struct hts_notifier_ems *ems_notifier = &drvdata->notifier.ems;

	return emstune_unregister_notifier(&ems_notifier->noti_block);
}
#else
static int hts_ext_dev_emstune_notifier_register(struct hts_drvdata *drvdata)
{
	return 0;
}

static int hts_ext_dev_emstune_notifier_unregister(struct hts_drvdata *drvdata)
{
	return 0;
}
#endif

static void hts_ext_dev_cpufreq_transition_notifier(void *data, struct cpufreq_policy *policy)
{
	struct hts_notifier *notifier = &drvdata_instance->notifier;
	struct hts_notifier_cpu *cpu_notifier = &notifier->cpu;
	struct hts_mmap *mmap_buffer = &notifier->mmap;
	unsigned int freq = policy->cur;
	ktime_t now_time = ktime_get();
	int mask_index = hts_var_get_mask_index(&drvdata_instance->devfs, policy->cpu);
	u64 value_buffer[MAXIMUM_EVENT_BUFFER_SIZE] = { FREQ_CPU, 0, 0, mask_index };
	unsigned long flag;

	if (!drvdata_instance->devfs.enabled ||
		!drvdata_instance->devfs.ems_mode ||
		mask_index < 0 ||
		cpu_notifier->freq[mask_index] == freq)
		return;

	if (cpu_notifier->freq[mask_index] != 0) {
		value_buffer[1] = now_time;
		value_buffer[2] = freq;

		spin_lock_irqsave(&notifier->lock, flag);
		hts_write_to_buffer(mmap_buffer->buffer_event,
					mmap_buffer->buffer_size / BUFFER_UNIT_SIZE - BUFFER_OFFSET_SIZE,
					value_buffer,
					MAXIMUM_EVENT_BUFFER_SIZE);
		spin_unlock_irqrestore(&notifier->lock, flag);
	}

	cpu_notifier->prev_time[mask_index]	= now_time;
	cpu_notifier->freq[mask_index]		= freq;
}

static int hts_ext_dev_register_cpufreq_transition_notifier(struct hts_drvdata *drvdata)
{
	return register_trace_android_rvh_cpufreq_transition(hts_ext_dev_cpufreq_transition_notifier, NULL);
}

static int hts_ext_dev_dsufreq_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct hts_notifier *notifier = &drvdata_instance->notifier;
	struct hts_notifier_dsu *dsu_notifier = &notifier->dsu;
	struct hts_mmap *mmap_buffer = &notifier->mmap;
	unsigned int freq = *(unsigned int *)data;
	ktime_t now_time = ktime_get();
	u64 value_buffer[MAXIMUM_EVENT_BUFFER_SIZE] = { FREQ_DSU, 0, 0, 0 };
	unsigned long flag;

	if (!drvdata_instance->devfs.enabled ||
		!drvdata_instance->devfs.ems_mode ||
		dsu_notifier->freq == freq)
		return NOTIFY_OK;

	if (dsu_notifier->freq != 0) {
		value_buffer[1] = now_time;
		value_buffer[2] = freq;

		spin_lock_irqsave(&notifier->lock, flag);
		hts_write_to_buffer(mmap_buffer->buffer_event,
					mmap_buffer->buffer_size / BUFFER_UNIT_SIZE - BUFFER_OFFSET_SIZE,
					value_buffer,
					MAXIMUM_EVENT_BUFFER_SIZE);
		spin_unlock_irqrestore(&notifier->lock, flag);
	}

	dsu_notifier->prev_time = now_time;
	dsu_notifier->freq = freq;

	return NOTIFY_OK;
}

static int hts_ext_dev_register_dsufreq_transition_notifier(struct hts_drvdata *drvdata)
{
	struct hts_notifier_dsu *dsu_notifier = &drvdata->notifier.dsu;

	dsu_notifier->freq = 0;
	dsu_notifier->noti_block.notifier_call = hts_ext_dev_dsufreq_notifier;

	return dsufreq_register_notifier(&dsu_notifier->noti_block);
}

static int hts_ext_dev_unregister_dsufreq_transition_notifier(struct hts_drvdata *drvdata)
{
	struct hts_notifier_dsu *dsu_notifier = &drvdata->notifier.dsu;

	return dsufreq_unregister_notifier(&dsu_notifier->noti_block);
}

static int hts_ext_dev_devfreq_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct hts_notifier *notifier = &drvdata_instance->notifier;
	struct hts_notifier_devfreq *mif_notifier = &notifier->mif;
	struct hts_mmap *mmap_buffer = &notifier->mmap;
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)data;
	unsigned int freq = freqs->new;
	ktime_t now_time = ktime_get();
	u64 value_buffer[MAXIMUM_EVENT_BUFFER_SIZE] = { FREQ_MIF, 0, 0, 0 };
	unsigned long flag;

	if (!drvdata_instance->devfs.enabled ||
		!drvdata_instance->devfs.ems_mode ||
		event != DEVFREQ_POSTCHANGE ||
		mif_notifier->freq == freq)
		return NOTIFY_OK;

	if (mif_notifier->freq != 0) {
		value_buffer[1] = now_time;
		value_buffer[2] = freq;

		spin_lock_irqsave(&notifier->lock, flag);
		hts_write_to_buffer(mmap_buffer->buffer_event,
					mmap_buffer->buffer_size / BUFFER_UNIT_SIZE - BUFFER_OFFSET_SIZE,
					value_buffer,
					MAXIMUM_EVENT_BUFFER_SIZE);
		spin_unlock_irqrestore(&notifier->lock, flag);
	}

	mif_notifier->prev_time = now_time;
	mif_notifier->freq = freq;

	return NOTIFY_OK;
}

static int hts_ext_dev_register_devfreq_transition_notifier(struct hts_drvdata *drvdata)
{
	struct devfreq *devfreq;
	struct platform_device *pdev = drvdata->pdev;
	struct hts_notifier_devfreq *mif_notifier = &drvdata->notifier.mif;

	devfreq = devfreq_get_devfreq_by_phandle(&pdev->dev, "handle", 0);

	mif_notifier->freq = 0;
	mif_notifier->noti_block.notifier_call = hts_ext_dev_devfreq_notifier;

	devm_devfreq_register_notifier(devfreq->dev.parent, devfreq,
			&mif_notifier->noti_block, DEVFREQ_TRANSITION_NOTIFIER);

	return 0;
}

static void hts_ext_dev_unregister_devfreq_transition_notifier(struct hts_drvdata *drvdata)
{
}

int hts_ext_dev_initialize(struct hts_drvdata *drvdata)
{
	int ret = 0;

	drvdata_instance = drvdata;
	spin_lock_init(&drvdata->notifier.lock);

	ret = hts_ext_dev_emstune_notifier_register(drvdata);
	if (ret)
		goto err_emstune_register;

	ret = hts_ext_dev_register_dsufreq_transition_notifier(drvdata);
	if (ret)
		goto err_dsufreq_register;

	ret = hts_ext_dev_register_devfreq_transition_notifier(drvdata);
	if (ret)
		goto err_devfreq_register;

	ret = hts_ext_dev_register_cpufreq_transition_notifier(drvdata);
	if (ret)
		goto err_cpufreq_register;

	return 0;

err_cpufreq_register:
	hts_ext_dev_unregister_devfreq_transition_notifier(drvdata);

err_devfreq_register:
	hts_ext_dev_unregister_dsufreq_transition_notifier(drvdata);

err_dsufreq_register:
	hts_ext_dev_emstune_notifier_unregister(drvdata);

err_emstune_register:
	return ret;
}

void hts_ext_dev_uninitialize(struct hts_drvdata *drvdata)
{
	hts_ext_dev_unregister_devfreq_transition_notifier(drvdata);
	hts_ext_dev_unregister_dsufreq_transition_notifier(drvdata);
	hts_ext_dev_emstune_notifier_unregister(drvdata);
}
