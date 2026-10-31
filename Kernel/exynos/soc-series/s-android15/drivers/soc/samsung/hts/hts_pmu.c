/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_pmu.h"
#include "hts_common.h"

#include <linux/cpumask.h>
#include <linux/fortify-string.h>
#include <asm-generic/int-ll64.h>

static struct hts_pmu_handle __percpu *hts_pmu_core_handle;

struct hts_pmu_handle *hts_pmu_get_handle(int cpu)
{
	return per_cpu_ptr(hts_pmu_core_handle, cpu);
}

static inline int hts_pmu_cmpxhg_preemption(struct hts_pmu_handle *handle)
{
	return hts_atomic_cmpxhg(&handle->preemption);
}

static inline void hts_pmu_set_preemption(struct hts_pmu_handle *handle)
{
	hts_atomic_set(&handle->preemption);
}

static inline void hts_pmu_clear_preemption(struct hts_pmu_handle *handle)
{
	hts_atomic_clear(&handle->preemption);
}

static void hts_pmu_enable_event(struct hts_pmu_handle *pmu_handle)
{
	int i;
	struct hts_pmu_event *pmu_event_node;

	for (i = 0; i < pmu_handle->event_count; ++i) {
		pmu_event_node = &pmu_handle->events[i];

		if (IS_ERR_OR_NULL(pmu_event_node->perf_handle))
			continue;

		perf_event_enable(pmu_event_node->perf_handle);
	}
}

void hts_pmu_read_event(int cpu, struct task_struct *prev)
{
	int i, isFirstPhase = 0, value_buffer_idx = NON_EVENT_DATA_COUNT;
	u64 pmu_value;
	u64 value_buffer[MAXIMUM_BUFFER_SIZE] = { prev->pid, ktime_get() };
	struct hts_pmu_event *pmu_event;
	struct hts_pmu_handle *pmu_handle;
	struct hts_mmap *pmu_mmap;

	pmu_handle = hts_pmu_get_handle(cpu);
	if (pmu_handle == NULL)
		return;

	pmu_mmap = &pmu_handle->mmap;

	if (hts_pmu_cmpxhg_preemption(pmu_handle))
		return;

	for (i = 0; i < pmu_handle->event_count; ++i) {
		pmu_event = &pmu_handle->events[i];

		if (IS_ERR_OR_NULL(pmu_event->perf_handle))
			goto skip_serialize;

		perf_event_read_local(pmu_event->perf_handle, &pmu_value, NULL, NULL);

		if (pmu_value == 0) {
			pmu_event->zero_count++;
			isFirstPhase = 1;
		}
		else
			pmu_event->zero_count = 0;

		if (pmu_event->zero_count >= MAXIMUM_THRESHOLD_WARN) {
			pr_warn("HTS : Event maybe invalid value -> CPU%d, %llu",
					cpu, pmu_event->perf_attr.config);
			pmu_event->zero_count = 0;
		}

		value_buffer[value_buffer_idx] = pmu_value - pmu_event->prev_value;
		value_buffer_idx++;
		pmu_event->prev_value = pmu_value;
	}

	if (value_buffer_idx <= NON_EVENT_DATA_COUNT ||
		isFirstPhase)
		goto skip_serialize;

	hts_write_to_buffer(pmu_mmap->buffer_event,
			pmu_mmap->buffer_size / BUFFER_UNIT_SIZE - BUFFER_OFFSET_SIZE,
			value_buffer,
			value_buffer_idx);

skip_serialize:
	hts_pmu_clear_preemption(pmu_handle);
}

static void __hts_pmu_unregister_event(int cpu, struct hts_pmu_handle *pmu_handle)
{
	int i;
	struct hts_pmu_event *pmu_event_node;

	for (i = 0; i < pmu_handle->event_count; ++i) {
		pmu_event_node = &pmu_handle->events[i];

		if (IS_ERR_OR_NULL(pmu_event_node->perf_handle)) {
			pr_err("HTS : Couldn't release %dth counter for CPU%d",
				i, cpu);
			continue;
		}

		perf_event_release_kernel(pmu_event_node->perf_handle);
		pmu_event_node->perf_handle = NULL;
	}
}

static int __hts_pmu_register_event(int cpu, struct hts_pmu_handle *pmu_handle)
{
	int i, ret = 0;
	struct hts_pmu_event *pmu_event_node;

	for (i = 0; i < pmu_handle->event_count; ++i) {
		pmu_event_node = &pmu_handle->events[i];

		pmu_event_node->perf_handle = perf_event_create_kernel_counter(&pmu_event_node->perf_attr, cpu, NULL, NULL, NULL);
		if (IS_ERR_OR_NULL(pmu_event_node->perf_handle)) {
			pr_err("HTS : Couldn't create counter with event %llu for CPU%d",
				pmu_event_node->perf_attr.config, cpu);
			ret = -EINVAL;
			goto err_create_counter;
		}
	}

	hts_pmu_enable_event(pmu_handle);

	return 0;

err_create_counter:
	__hts_pmu_unregister_event(cpu, pmu_handle);

	return ret;
}

void hts_pmu_unregister_event(int cpu)
{
	struct hts_pmu_handle *pmu_handle = hts_pmu_get_handle(cpu);

	hts_pmu_set_preemption(pmu_handle);
	__hts_pmu_unregister_event(cpu, pmu_handle);
	hts_pmu_clear_preemption(pmu_handle);
}

int hts_pmu_register_event(int cpu)
{
	int ret;
	struct hts_pmu_handle *pmu_handle = hts_pmu_get_handle(cpu);

	hts_pmu_set_preemption(pmu_handle);
	ret = __hts_pmu_register_event(cpu, pmu_handle);
	hts_pmu_clear_preemption(pmu_handle);

	return ret;
}

void hts_pmu_unregister_event_all(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		hts_pmu_unregister_event(cpu);
	}
}

int hts_pmu_register_event_all(void)
{
	int cpu, ret;

	for_each_online_cpu(cpu) {
		ret = hts_pmu_register_event(cpu);
		if (ret)
			return ret;
	}

	return 0;
}

static int hts_pmu_configure_event(int cpu, int event)
{
	struct hts_pmu_handle *pmu_handle = NULL;
	struct hts_pmu_event *pmu_event_node;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu) {
		pr_err("HTS: : Core Index is invalid to configure event CPU%d", cpu);
		return -EINVAL;
	}

	pmu_handle = hts_pmu_get_handle(cpu);

	if (MAXIMUM_EVENT_COUNT <= pmu_handle->event_count) {
		pr_err("HTS : No space to configure event %d for CPU%d",
			event, cpu);
		return -ENOENT;
	}

	pmu_event_node = &pmu_handle->events[pmu_handle->event_count];

	pmu_event_node->perf_attr.size = sizeof(struct perf_event_attr);
	pmu_event_node->perf_attr.pinned = 1;
	pmu_event_node->perf_attr.type = PERF_TYPE_RAW;
	pmu_event_node->perf_attr.config = event;
	pmu_event_node->perf_attr.config1 = 1;

	pmu_handle->event_count++;

	return 0;
}

int hts_pmu_configure_events(int cpu, int *events, int event_count)
{
	int i, event, ret = 0;
	struct hts_pmu_handle *pmu_handle = NULL;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu)
		return -EINVAL;

	pmu_handle = hts_pmu_get_handle(cpu);

	if (pmu_handle == NULL ||
		events == NULL)
		return -EINVAL;

	hts_pmu_set_preemption(pmu_handle);

	for (i = 0; i < event_count; ++i) {
		event = events[i];

		ret = hts_pmu_configure_event(cpu, event);
		if (ret) {
			pr_err("HTS : Couldn't configure %dth event - %d for Core %d",
				i, event, cpu);
			goto err_configure_event;
		}
	}

err_configure_event:
	hts_pmu_clear_preemption(pmu_handle);

	return ret;
}

void __hts_pmu_release_event(struct hts_pmu_handle *pmu_handle)
{
	int i;
	struct hts_pmu_event *pmu_event_node;

	for (i = 0; i < pmu_handle->event_count; ++i) {
		pmu_event_node = &pmu_handle->events[i];

		if (IS_ERR_OR_NULL(pmu_event_node->perf_handle))
			continue;

		perf_event_release_kernel(pmu_event_node->perf_handle);
		pmu_event_node->perf_handle = NULL;
	}
}

void __hts_pmu_clear_event(struct hts_pmu_handle *pmu_handle)
{
	memset(pmu_handle->events, 0, sizeof(struct hts_pmu_event) * MAXIMUM_EVENT_COUNT);
	pmu_handle->event_count = 0;
}

void hts_pmu_release_event(int cpu)
{
	struct hts_pmu_handle *pmu_handle;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu) {
		pr_err("HTS : Core is not available (%d) for releasing", cpu);
		return;
	}

	pmu_handle = hts_pmu_get_handle(cpu);

	hts_pmu_set_preemption(pmu_handle);
	__hts_pmu_release_event(pmu_handle);
	hts_pmu_clear_preemption(pmu_handle);
}

void hts_pmu_clear_event(int cpu)
{
	struct hts_pmu_handle *pmu_handle;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu) {
		pr_err("HTS : Core is not available (%d) for clearing", cpu);
		return;
	}

	pmu_handle = hts_pmu_get_handle(cpu);

	hts_pmu_set_preemption(pmu_handle);
	__hts_pmu_clear_event(pmu_handle);
	hts_pmu_clear_preemption(pmu_handle);
}

void hts_pmu_reset_cpu(int cpu)
{
	struct hts_pmu_handle *pmu_handle;

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu) {
		pr_err("HTS : Core is not available (%d) for reset", cpu);
		return;
	}

	pmu_handle = hts_pmu_get_handle(cpu);

	hts_pmu_set_preemption(pmu_handle);
	__hts_pmu_release_event(pmu_handle);
	__hts_pmu_clear_event(pmu_handle);
	hts_pmu_clear_preemption(pmu_handle);
}

int hts_pmu_initialize(struct hts_drvdata *drvdata)
{
	int cpu;
	struct device *dev = &drvdata->pdev->dev;
	struct hts_pmu_handle *pmu_handle;

	hts_pmu_core_handle = alloc_percpu(struct hts_pmu_handle);
	if (hts_pmu_core_handle == NULL) {
		dev_err(dev, "Couldn't alloc per-core PMU handle");
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		pmu_handle = hts_pmu_get_handle(cpu);

		pmu_handle->cpu = cpu;
		memset(&pmu_handle->events, 0, sizeof(struct hts_pmu_event) * MAXIMUM_EVENT_COUNT);
		pmu_handle->event_count = 0;

		hts_pmu_clear_preemption(pmu_handle);
	}

	return 0;
}
