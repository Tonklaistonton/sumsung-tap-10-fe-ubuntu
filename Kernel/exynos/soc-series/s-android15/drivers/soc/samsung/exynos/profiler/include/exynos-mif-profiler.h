#ifndef __EXYNOS_MIF_PROFILER_H__
#define __EXYNOS_MIF_PROFILER_H__

#include <exynos-profiler-common.h>

struct mif_freq_state {
	u64		sum;
	u64		avg;
	u64		ratio;
};

struct mif_profile_result {
	struct freq_cstate_result	fc_result;

	s32			cur_temp;
	s32			avg_temp;

	/* private data */
	u64			*tdata_in_state;
	struct mif_freq_state	freq_stats[NR_MASTERS];
	int			llc_status;
};

struct mif_profiler {

	int enabled;

	s32 profiler_id;
	u32 cal_id;
	struct device *devfreq_dev;

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

	struct mif_profile_result result[NUM_OF_USER];
	struct exynos_devfreq_freq_infos freq_infos;
};
static struct profiler {
	struct device_node	*dn;
	struct kobject		*kobj;
	struct mif_profiler mif;
} profiler;

#endif /* # __EXYNOS_MIF_PROFILER_H__ */
