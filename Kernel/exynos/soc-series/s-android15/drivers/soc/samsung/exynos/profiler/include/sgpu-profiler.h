/* SPDX-License-Identifier: GPL-2.0 */
/*
 * @file sgpu_profiler.h
 * @copyright 2023 Samsung Electronics
 */
#ifndef _SGPU_PROFILER_H_
#define _SGPU_PROFILER_H_

#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/cpufreq.h>
#include <linux/pm_runtime.h>

#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/profiler/exynos-profiler-fn.h>
#include <exynos-profiler-if.h>

#define sgpu_profiler_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)

#define PROFILER_POLLING_INTERVAL_MS 16
#define PROFILER_AUTOSUSPEND_DELAY_MS 1000
/* TODO: get information from GPU DRV */
#define SGPU_DRV_POLLING_INTERVAL_MS 32
#define SGPU_DRV_AUTOSUSPEND_DELAY_MS 50
#define SGPU_DRV_GOVERNOR_NAME_LEN 16

#define PROFILER_TABLE_MAX 200
#define PROFILER_RENDERTIME_DEFAULT_FRAMETIME 16666ULL
#define PROFILER_RENDERTIME_MAX 999999ULL
#define PROFILER_RENDERTIME_MIN 4000ULL

#define PROFILER_PB_FRAMEINFO_TABLE_SIZE 8
#define PROFILER_PB_FPS_CALC_AVERAGE_US 1000000L

#define PROFILER_GFXINFO_NUM_OF_ELEMENTS 16
#define PROFILER_GFXINFO_LEGACY_SIZE (sizeof(u64) * 3)

#define PROFILER_RENDERER_NAME_LEN 16

enum pb_control_ids {
	PB_CONTROL_USER_TARGET_PID = 0,      /* 0 */
	PB_CONTROL_GPU_FORCED_BOOST_ACTIVE,  /* 1 */
	PB_CONTROL_CPUTIME_BOOST_PM,         /* 2 */
	PB_CONTROL_GPUTIME_BOOST_PM,         /* 3 */
	PB_CONTROL_CPU_MGN_MAX,              /* 4 */
	PB_CONTROL_CPU_MGN_FRAMEDROP_PM,     /* 5 */
	PB_CONTROL_CPU_MGN_FPSDROP_PM,       /* 6 */
	PB_CONTROL_CPU_MGN_ALL_FPSDROP_PM,   /* 7 */
	PB_CONTROL_CPU_MGN_ALL_CL_BITMAP,    /* 8 */
	PB_CONTROL_CPU_MINLOCK_BOOST_PM_MAX, /* 9 */
	PB_CONTROL_CPU_MINLOCK_FRAMEDROP_PM, /* 10 */
	PB_CONTROL_CPU_MINLOCK_FPSDROP_PM,   /* 11 */
	PB_CONTROL_DVFS_INTERVAL,            /* 12 */
	PB_CONTROL_FPS_AVERAGE_US,           /* 13 */
	PB_CONTROL_MSCALER_SETID,            /* 14 */
	PB_CONTROL_MSCALER_GETTYPE = 14,     /* 14 */
	PB_CONTROL_MSCALER_FREQS,            /* 15 */
};

enum mscaler_type {
	MSCALER_TYPE_BYPASS = 0,
	MSCALER_TYPE_LINEAR,                 /* 1 */
	MSCALER_TYPE_HALFLINEAR,             /* 2 */
	MSCALER_TYPE_RHALFLINEAR,            /* 3 */
	MSCALER_TYPE_SQUARE,                 /* 4 */
	MSCALER_TYPE_HALFSQUARE,             /* 5 */
	MSCALER_TYPE_RHALFSQUARE,            /* 6 */
};

enum type {
	GPU_SW,
	GPU_HW,
	NUM_OF_GPU_REGION,
};

enum governor_op {
	CHANGE_GOV_TO_PREV,
	CHANGE_GOV_TO_PROFILER,
};

struct interframe_data {
	unsigned int nrq;

	struct interframe_time_info {
		u64 vsync;
		u64 start;
		u64 end;
		u64 total;
	} interframe_time[NUM_OF_GPU_REGION]; /* [0]: GPU_SW, [1]: GPU_HW */

	ktime_t rtime[NUM_OF_RTIMEINFO];
	u64 gfxinfo[NUM_OF_GFXINFO]; /* Graphics info: ANGLE, draw call, vertex, texture and etc... */

	u64 cputime;
	u64 gputime;
	u64 swaptime;
	u64 vsync_interval;

	u32 coreid;       /* running core# of renderer : get_cpu() */
	u32 pid;          /* renderer Thread ID : current->pid */
	u32 tgid;         /* app. PID : current-tgid */
	u8 renderer_name[PROFILER_RENDERER_NAME_LEN];

	u64 timestamp;    /* timestamp(us) of swap call */
	char name[16];    /* renderer name : current->comm */
};

struct chunk_gfxinfo {
	u64 start;
	u64 end;
	u64 total;
	u32 ids[PROFILER_GFXINFO_NUM_OF_ELEMENTS];
	u64 values[PROFILER_GFXINFO_NUM_OF_ELEMENTS];
};

struct vsync_data {
        ktime_t lastsw;
        ktime_t curhw;
        ktime_t lasthw;
        ktime_t prev;
        ktime_t interval;
        atomic_t swapcall_counter;
        int frame_counter;
	    int counter;
};

struct info_data {
	s32 frame_counter;
	struct vsync_data vsync;
	u64 last_updated_vsync_time;
	int cpufreq_pm_qos_added;
};


struct rtp_data {
	int debug_level;
	unsigned int head;
	unsigned int tail;
	int readout;
	unsigned int nrq;
	unsigned int lastshowidx;
	ktime_t prev_swaptimestamp;
	atomic_t flag;

    struct last {
        ktime_t hw_starttime;
        ktime_t hw_endtime;
        atomic_t hw_totaltime;
        atomic_t hw_read;

        u64 cputime;
        u64 gputime;
        u64 swaptime;
        int cpufreqlv;
        int gpufreqlv;
    } last;

    struct cur {
        ktime_t hw_starttime;
        ktime_t hw_endtime;
        ktime_t hw_totaltime;
    } cur;

    ktime_t rtime[NUM_OF_RTIMEINFO];
    u64 gfxinfo[NUM_OF_GFXINFO]; /* Graphics info: draw call, vertex, texture and etc... */

    struct vsync_data vsync;
};

struct pboost_data {
	/* Performance Booster - input params */
	u32 target_frametime;              /* 0: off, otherwise: do pb */
	int gpu_forced_boost_activepct;    /* Move to upper freq forcely, if GPU active ratio >= this */
	int user_target_pid;               /* Target PID by daemon, if 0 means detected PID */
	int cputime_boost_pm;              /* cputime += cputime * cputime_boost_pm / 1000L */
	int gputime_boost_pm;              /* gputime += gputime * gputime_boost_pm / 1000L */

	int cpu_mgn_margin_max;         /* Upper boosting margin, if drop is detected */
	int framedrop_detection_pm_mgn;    /* if this < pb_frinfo.frame_drop_pm, frame is dropping */
	int fpsdrop_detection_pm_mgn;      /* if this < pb_frinfo.fps_drop_pm, fps is dropping */
	u32 target_clusters_bitmap;        /* 0: detected renderer, 1: include little, 2: include mid(big), 3: include little+mid(big) ... */
	int target_clusters_bitmap_fpsdrop_detection_pm;

	int cpu_minlock_margin_max;     /* Upper boosting margin, if drop is detected */
	int framedrop_detection_pm_minlock;/* if this < pb_frinfo.frame_drop_pm, frame is dropping */
	int fpsdrop_detection_pm_minlock;  /* if this < pb_frinfo.fps_drop_pm, fps is dropping */

	/* Performance Booster - internal params */
	u32 target_fps;
	int freqtable_size[NUM_OF_DOMAIN];
	int *freqtable[NUM_OF_DOMAIN];
	int freqtable_en[NUM_OF_DOMAIN];
	int target_clusterid;              /* Target cluster to control mainly */
	int target_clusterid_prev;
	int pmqos_minlock_prev[NUM_OF_DOMAIN];
	int pmqos_minlock_next[NUM_OF_DOMAIN];
	int cur_freqlv[NUM_OF_DOMAIN];
	int cur_sumfreq[NUM_OF_DOMAIN];
	int cur_ndvfs[NUM_OF_DOMAIN];
	int lastframe_avgfreq[NUM_OF_DOMAIN];
	int next_cpu_minlock_margin;    /* if this < 0, apply daemon's margin. Otherwise, apply this */
	int next_cpu_minlock_boosting_fpsdrop;
	int next_cpu_minlock_boosting_framedrop;

	/* Performance Booster - Profiling Data */
	struct margininfo_data {
		u16 no_mgnboost;
		u16 no_mlckboost;
		u16 no_value;
		s32 sum_margin;
	} mgninfo[NUM_OF_DOMAIN];

	/* Performance Booster - FPS Calculator & Drop Detector*/
	struct frameinfo_data {
		int latest_targetpid;
		int onesecidx;
		u32 pid[PROFILER_PB_FRAMEINFO_TABLE_SIZE];
		u32 hit[PROFILER_PB_FRAMEINFO_TABLE_SIZE];
		u64 latest_ts[PROFILER_PB_FRAMEINFO_TABLE_SIZE];
		u64 earliest_ts[PROFILER_PB_FRAMEINFO_TABLE_SIZE];
		u64 latest_interval[PROFILER_PB_FRAMEINFO_TABLE_SIZE];
		u64 expected_swaptime;
		u32 avg_fps;
		u32 exp_afps;
		u32 fps_drop_pm;
		u32 frame_drop_pm;
	} frinfo;
};

struct pid_data {
	u32 list[NUM_OF_PID];
	u8 core_list[NUM_OF_PID];
	atomic_t list_readout;
	atomic_t list_top;
};

struct mscaler_control {
	u32 id;
	u32 type[NUM_OF_DOMAIN];
	s32 fs[NUM_OF_DOMAIN];
	s32 fz[NUM_OF_DOMAIN];
};

/* ems api for peltboost */
extern void emstune_set_peltboost(int mode, int level, int cpu, int value);
#endif /* _SGPU_PROFILER_H_ */
