/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * IO Performance mode with UFS
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#ifndef _UFS_PERF_V1_H_
#define _UFS_PERF_V1_H_

enum {
	__POLICY_HEAVY = 0,
	__POLICY_MAX,
};
#define POLICY_HEAVY	BIT(__POLICY_HEAVY)

typedef enum {
	CHUNK_SEQ = 0,
	CHUNK_RAN,

	CHUNK_NUM,
} __chuck_type;

typedef enum {
	FREQ_RARE = 0,
	FREQ_REACH,				/* transient */
	FREQ_DWELL,
	FREQ_DROP,				/* transient */
} ufs_freq_sts;

typedef struct {
	ufs_freq_sts freq_state;
	s64 s_time_start;
	s64 s_time_prev;
	u32 s_count;

	u32 th_reach_count;
	u64 th_reach_interval_in_us;
	u32 th_reach_density;	/* N * 100000 / us */
	u32 th_drop_count;
	u64 th_drop_interval_in_us;
	u32 th_drop_density;	/* N * 100000 / us */
} ufs_perf_stat_type;

struct ufs_perf_v1 {
	/* enable bits */
	u32 policy_bits;
	u32 req_bits[__POLICY_MAX];

	/* reset timer */
	struct timer_list reset_timer;	/* stat reset timer */

	/* sysfs */
	struct kobject sysfs_kobj;

	/* threshold for reset, acccessible through sysfs */
	u32 th_reset_in_ms;
	u32 th_mixed_interval_in_us;

	/* sync */
	spinlock_t lock;

	/* stats */
	ufs_perf_stat_type stats[CHUNK_NUM];
	__chuck_type chunk_prev;

	/* related to outside */
	char ecs_range[2][10];
	int bts_scen_idx;
};

#endif /* _UFS_PERF_V1_H_ */
