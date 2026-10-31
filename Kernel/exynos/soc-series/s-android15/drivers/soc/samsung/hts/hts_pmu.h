/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_PMU_H_
#define _HTS_PMU_H_

#include "hts.h"

#include <linux/sched.h>
#include <linux/perf_event.h>
#include <linux/mm_types.h>
#include <linux/atomic/atomic-instrumented.h>
#include <asm-generic/int-ll64.h>

#define NON_EVENT_DATA_COUNT	(2)
#define MAXIMUM_BUFFER_SIZE	(25)
#define MAXIMUM_EVENT_COUNT	(20)
#define MAXIMUM_THRESHOLD_WARN	(1000)

#define BUFFER_PAGE_EXP		(1)
#define BUFFER_PAGE_SIZE	(PAGE_SIZE * 2)

struct hts_pmu_event {
	struct perf_event_attr  perf_attr;
	struct perf_event	*perf_handle;

	unsigned long		prev_value;
	int			zero_count;
};

struct hts_pmu_handle {
	int			cpu;

	struct hts_pmu_event	events[MAXIMUM_EVENT_COUNT];
	int			event_count;

	atomic_t		preemption;

	struct hts_mmap		mmap;
};

struct hts_pmu_handle *hts_pmu_get_handle(int cpu);
void hts_pmu_read_event(int cpu, struct task_struct *prev);
void hts_pmu_unregister_event(int cpu);
int hts_pmu_register_event(int cpu);
void hts_pmu_unregister_event_all(void);
int hts_pmu_register_event_all(void);
int hts_pmu_configure_events(int cpu, int *events, int event_count);
void hts_pmu_release_event(int cpu);
void hts_pmu_clear_event(int cpu);
void hts_pmu_reset_cpu(int cpu);
int hts_pmu_initialize(struct hts_drvdata *drvdata);

#endif /* _HTS_PMU_H_ */
