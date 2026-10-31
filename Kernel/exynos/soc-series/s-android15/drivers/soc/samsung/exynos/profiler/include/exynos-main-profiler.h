#ifndef __EXYNOS_MAIN_PROFILER_H__
#define __EXYNOS_MAIN_PROFILER_H__

#include <exynos-profiler-common.h>
#include <exynos-profiler-shared.h>

struct main_profiler {
	struct domain_data domains[NUM_OF_DOMAIN];
	const char *domain_name[NUM_OF_DOMAIN];
	wait_queue_head_t wq;

	int forced_running;
	int disable_gfx;
	int profiler_gov;
	int gpu_profiler_gov;

	int bts_idx;
	int bts_added;
	int len_mo_id;
	int *mo_id;

	int llc_config;

	bool dm_dynamic;
	bool disable_llc_way;

	ktime_t start_time;
	ktime_t end_time;

	u64 start_frame_cnt;
	u64 end_frame_cnt;
	u64 start_frame_vsync_cnt;
	u64 end_frame_vsync_cnt;
	u64 start_fence_cnt;
	u64 end_fence_cnt;

	s32 max_fps;

	struct work_struct fps_work;
	struct device_file_operation gov_fops;

	int profiler_state;
	atomic_t profiler_running;
	struct work_struct profiler_wq;
	int profiler_stay_counter;

	u64 gfx_wakeup_gpubw_thr;
	int gfx_wakeup_counter;
	u64 gfx_wakeup_thr1;
	u64 gfx_wakeup_thr2;
	u64 gfx_wakeup_thr3;

	u64 perf_mainbw_thr;
	u64 perf_gpubw_up_ratio;
	u64 perf_gpubw_dn_ratio;
	int perf_max_llc_way;
	int perf_light_llc_way;
};

static struct profiler {
	struct device_node *dn;
	struct kobject *kobj;
	struct main_profiler main;
} profiler;

struct fps_profile {
	/* store last profile average fps */
	int start;
	ktime_t profile_time;	/* sec */
	u64 fps;
};

struct fps_profile fps_profile;

/*  shared data with Platform */
struct profile_sharing_data psd;
struct delta_sharing_data dsd;
struct tunable_sharing_data tsd;

static void profiler_sched(void);
static void profiler_sleep(void);
#endif /* __EXYNOS_MAIN_PROFILER_H__ */
