#ifndef __EXYNOS_PROFILER_DSU_H__
#define __EXYNOS_PROFILER_DSU_H__

#include <exynos-profiler-common.h>

/* Result during profile time */
struct dsu_freq_state {
	u64		sum;
	u64		avg;
	u64		ratio;
};

struct dsu_profile_result {
	struct freq_cstate_result fc_result;

	s32 cur_temp;
	s32 avg_temp;

	/* private data */
	u64 *tdata_in_state;
	struct dsu_freq_state freq_stats[NR_MASTERS];
	int llc_status;
};

struct dsu_profiler {
	struct device_node *root;
	int enabled;

	s32 profiler_id;
	u32 cal_id;
	u32 devfreq_type;

	struct freq_table *table;
	u32 table_cnt;
	u32 dyn_pwr_coeff;
	u32 st_pwr_coeff;

	const char *tz_name;		/* currently do not use in MIF */
	struct thermal_zone_device *tz;			/* currently do not use in MIF */

	struct freq_cstate fc;			/* latest time_in_state info */
	struct freq_cstate_snapshot fc_snap[NUM_OF_USER];	/* previous time_in_state info */

	struct exynos_wow_profile prev_wow_profile;
	u64 *prev_tdata_in_state;

	u32 cur_freq_idx;	/* current freq_idx */
	u32 max_freq_idx;	/* current max_freq_idx */
	u32 min_freq_idx;	/* current min_freq_idx */

	struct dsu_profile_result result[NUM_OF_USER];
	struct exynos_devfreq_freq_infos freq_infos;
};

static struct profiler {
	struct device_node *dn;
	struct kobject *kobj;
	struct dsu_profiler dsu;
} profiler;

#endif /* __EXYNOS_PROFILER_DSU_H__ */
