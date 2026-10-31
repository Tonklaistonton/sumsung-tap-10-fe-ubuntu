/* soc/samsung/exynos-profiler-extif.h
 *
 * Copyright (C) 2023 Samsung Electronics Co.; Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - Header file for Exynos Profiler support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PROFILER_EXTERNAL_H__
#define __EXYNOS_PROFILER_EXTERNAL_H__

#include <linux/devfreq.h>

#include <soc/samsung/profiler/exynos-profiler-defs.h>

/**
 * exynos_profiler_gpudev_fn - This is the structure used by a profiler.
 * To obtain the DVFS information or other dataof the GPU, register the GPU KMD functions.
 * Through the Profiler's own function
 * the structure is maintained so that the function of the GPU Profiler can be utilized in other Profilers.
 *
 * @get_freq_table: function pointer of the function to get a freq table.
 * @get_max_freq: function pointer of the function to get a max freq.
 * @get_min_freq: function pointer of the function to get a min freq.
 * @get_cur_clock: function pointer of the function to get a current freq.
 * @get_step: function pointer of the function to get a step of dvfs level.
 * @get_time_in_state: function pointer of the function to get a dvfs time in state data.
 * @get_tis_last_update: function pointer of the function to get the last updateed time in state data.
 * @get_governor: function pointer of the function to get a current governor information.
 * @set_governor: function pointer of the function to set the selected governor.
 * @get_polling_interval: function pointer of the function to get the polling interval of governor.
 * @set_polling_interval: function pointer of the function to set the polling interval of governor.
 * @calc_utilization: function pointer of the function to calculate the utilization of domain.
 * @set_autosuspend_delay: function pointer of the function to set the delay time of auto-suspend
 *                         to obtain the stable during the change of governor.
 * @disable_llc_way: function pointer of the function to calculate the utilization of domain.
 * @set_profiler_governor: function pointer of the function to set the governor with the setting of profiler.
 * @get_freq_margin: function pointer of the function to get freq margin value.
 * @set_freq_margin: function pointer of the function to set freq margin value.
 * @set_targetframetime: function pointer of the function to set the target time.
 * @set_vsync: function pointer of the function to set the vsync information.
 * @get_frameinfo: function pointer of the function to get the frame info.
 * @get_gfxinfo: function pointer of the function to get the gfx info.
 * @get_pidinfo: function pointer of the function to get the pid info.
 * @pb_get_params: function pointer of the function to get parameters from PB module.
 * @pb_set_params: function pointer of the function to set paramtetes to PB module.
 * @pb_set_dbglv: function pointer of the function to set the level of debug mode.
 * @pb_set_freqtable: function pointer of the function to set the freq-table.
 * @pb_set_cur_freqlv: function pointer of the function to set the freq by a dvfs level.
 * @pb_set_cur_freq: function pointer of the function to set the freq by a value.
 * @pb_get_mgninfo: function pointer of the function to get the margin info.
 * @pb_get_gpu_target_freq: function pointer of the function to make the target freq val for GPU DVFS.
 * @profiler_sched: function pointer of the function to schedule the profiler works queue
 * @profiler_sleep: function pointer of the function to sleep the profiler works queue
 */
struct exynos_profiler_gpudev_fn {
	/* register function from External GPU to GPU Profiler(Sub) */
	int* (*get_freq_table)(void);
	int (*get_max_freq)(void);
	int (*get_min_freq)(void);
	int (*get_cur_clock)(void);

	int (*get_step)(void);
	ktime_t* (*get_time_in_state)(void);
	ktime_t (*get_tis_last_update)(void);

	char* (*get_governor)(void);
	int (*set_governor)(char *name);
	unsigned int (*get_polling_interval)(void);
	int (*set_polling_interval)(unsigned int interval);
	unsigned long long (*calc_utilization)(struct devfreq *df);
	void (*set_autosuspend_delay)(int delay_ms);
	void (*disable_llc_way)(bool disable);

	/* register function from GPU Profiler(Sub) to GPU/Main Profiler */
	void (*set_profiler_governor)(int mode);
	int (*get_freq_margin)(int id);
	int (*set_freq_margin)(int id, int freq_margin);
	void (*set_targetframetime)(int us);
	void (*set_vsync)(ktime_t ktime_us);

	void (*get_frameinfo)(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
	void (*get_gfxinfo)(u64 *times, u64 *gfxinfo);
	void (*get_pidinfo)(u32 *list, u8 *core_list, u8 *renderer_name);

	/* PB2.0 */
	int (*pb_get_params)(int idx);
	void (*pb_set_params)(int idx, int value);
	void (*pb_set_dbglv)(int value);
	int (*pb_set_freqtable)(int id, int cnt, int *table);
	void (*pb_set_freqtable_en)(int id, int en);
	int (*pb_get_freqtable_en)(int id);
	void (*pb_set_cur_freqlv)(int id, int idx);
	void (*pb_set_cur_freq)(int id, int freq);
	int (*pb_get_mgninfo)(int id, u16 *no_mgnboost, u16 *no_mlckboost);
	long (*pb_get_gpu_target_freq)(unsigned long freq, uint64_t activepct, uint64_t target_norm);

	/* Main Profiler function */
	void (*profiler_sched)(void);
	void (*profiler_sleep)(void);
};

/**
 * exynos_profiler_drmdev_fn - This is the structure used by a profiler.
 * To obtain the drm information or other data of the DPU, register the DPU functions.
 * @get_frame_cnt: function pointer of the function to get a frame count in a period.
 * @get_vsync_cnt: function pointer of the function to get a vsync count in a period.
 * @get_fence_cnt: function pointer of the function to get a fence count in a period.
 */
struct exynos_profiler_drmdev_fn {
	/* register function from DRM to Profiler */
	void (*get_frame_cnt)(u64 *cnt, ktime_t *time);
	void (*get_vsync_cnt)(u64 *cnt, ktime_t *time);
	void (*get_fence_cnt)(u64 *cnt, ktime_t *time);
};

#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GPU)
/**
 * sgpu_profiler_init - function for initilize sgpu profiler by others
 * @exynos_profiler_gpudev_fn:  pointer structure to be registered
*/
extern int sgpu_profiler_init(struct exynos_profiler_gpudev_fn *fn);
/**
 * sgpu_profiler_get_gpudev_fn - function that gets an instance of the structure that the callback function is registered.
*/
extern struct exynos_profiler_gpudev_fn* sgpu_profiler_get_gpudev_fn(void);
/**
 * sgpu_profiler_update_interframe_sw - function that proiles the time to calculate the drawcall execution times from the UMD.
 * @dw_size: chunk data size in dword
 * @chunk_data: chunk data pointer
*/
extern void sgpu_profiler_update_interframe_sw(u32 dw_size, void *chunk_data);
/**
 * sgpu_profiler_update_interframe_hw - function that proiles the time to calculate the execution times of GPU HW.
 * @start: a time which one indicates the start of flushing the gpu job to CP from KMD for each gpu job in a frame.
 * @end: a time which one indicates the end time for each gpu job in a frame.
 * @end_of_frame: a flag which one indicates the job whether it is the end of the frame or not.
*/
extern void sgpu_profiler_update_interframe_hw(ktime_t start, ktime_t end, bool end_of_frame);
/**
 * sgpu_profiler_governor_get_name - function that used when getting sgpu profiler governor meta-data from external drivers.
 * This refers to the governor's name.
*/
extern char *sgpu_profiler_governor_get_name(void);
#endif

#if IS_ENABLED(CONFIG_DRM_SAMSUNG_DPU)
/**
 * exynos_profiler_register_drmdev_fn -function that allows the drm function to be registered in the structure
 * @exynos_profiler_drmdev_fn: a pointer structure to be registered
*/
extern void exynos_profiler_register_drmdev_fn(struct exynos_profiler_drmdev_fn *fn);
#endif

extern int sgpu_profiler_governor_get_target(struct devfreq *df, uint32_t *level);
extern int sgpu_profiler_governor_clear(struct devfreq *df, uint32_t level);

#endif /* __EXYNOS_PROFILER_EXTERNAL_H__ */
