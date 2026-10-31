/* SPDX-License-Identifier: GPL-2.0 */
/*
 * @file sgpu-profiler-governor.h
 * @copyright 2023 Samsung Electronics
 */
#ifndef _SGPU_PROFILER_GOVERNOR_H_
#define _SGPU_PROFILER_GOVERNOR_H_

#define WINDOW_MAX_SIZE			12
#define NORMALIZE_SHIFT (10)
#define NORMALIZE_FACT  (1<<(NORMALIZE_SHIFT))
#define NORMALIZE_FACT3 (1<<((NORMALIZE_SHIFT)*3))
#define ITERATION_MAX	(10)

char *profiler_governor_name = "profiler";

static const uint32_t weight_table[WINDOW_MAX_SIZE + 1] = {
	48,  44,  40,  36,  32,  28,  24,  20,  16,  12,   8,   4,  312
};

struct sgpu_profiler_governor_data {
	unsigned int item[WINDOW_MAX_SIZE];
	unsigned int cur_idx;
};

extern void sgpu_profiler_governor_init(struct exynos_profiler_gpudev_fn *fn);
#endif /* _SGPU_PROFILER_GOVERNOR_H_ */
