#ifndef __EXYNOS_GPU_PROFILER_H__
#define __EXYNOS_GPU_PROFILER_H__

#include <exynos-profiler-common.h>

struct gpu_profile_result {
	struct freq_cstate_result	fc_result;

	s32 cur_temp;
	s32 avg_temp;
};

struct gpu_profiler {
	int enabled;

	s32 profiler_id;
	u32 cal_id;

	struct freq_table *table;
	u32 table_cnt;
	u32 dyn_pwr_coeff;
	u32 st_pwr_coeff;

	const char *tz_name;
	struct thermal_zone_device *tz;

	struct freq_cstate fc;
	struct freq_cstate_snapshot fc_snap[NUM_OF_USER];

	u32 cur_freq_idx;	/* current freq_idx */
	u32 max_freq_idx;	/* current max_freq_idx */
	u32 min_freq_idx;	/* current min_freq_idx */

	/* Profile Result */
	struct gpu_profile_result result[NUM_OF_USER];
};

static struct profiler {
	struct device_node *dn;
	struct kobject *kobj;
	struct gpu_profiler gpu;
} profiler;

#endif /* __EXYNOS_GPU_PROFILER_H__ */
