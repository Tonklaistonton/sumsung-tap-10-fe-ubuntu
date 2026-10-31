/*
 * exynos-wow.h - Exynos Workload Watcher Driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *  Hanjun Shin <hanjun.shin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __EXYNOS_WOW_H_
#define __EXYNOS_WOW_H_

#include <linux/types.h>

enum wow_master_id {
	CPU,
	GPU,
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	IRP,
#endif
	MIF,
	NR_MASTERS,
};

struct exynos_wow_meta_data {
	bool enable;
	u32 bus_width;
	u32 nr_info;
};

struct exynos_wow_meta {
	struct exynos_wow_meta_data data[NR_MASTERS];
	int ts_nr_levels;
};

struct exynos_wow_profile_data {
	u64 ccnt_osc;
	u64 ccnt;
	u64 active;
	u64 transfer_data;
	u64 nr_requests;
	u64 mo_count;
};

struct exynos_wow_ts_data {
	u64 time;
	u64 active_time;
	u64 tdata;
};

#define MAX_STATE (15)
struct exynos_wow_profile {
	u64 ktime;
	struct exynos_wow_profile_data data[NR_MASTERS];
	struct exynos_wow_ts_data ts[MAX_STATE];
};

#if IS_ENABLED(CONFIG_EXYNOS_WOW)
extern int exynos_wow_get_meta(struct exynos_wow_meta *result);
extern int exynos_wow_get_data(struct exynos_wow_profile *result);
#else
static inline int exynos_wow_get_meta(struct exynos_wow_meta *result)
{
	return 0;
}
static inline int exynos_wow_get_data(struct exynos_wow_profile *result)
{
	return 0;
}
#endif
#endif	/* __EXYNOS_WOW_H_ */
