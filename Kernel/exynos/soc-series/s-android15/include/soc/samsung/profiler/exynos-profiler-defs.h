#ifndef __EXYNOS_PROFILER_DEFS_H__
#define __EXYNOS_PROFILER_DEFS_H__

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <soc/samsung/cal-if.h>

#if IS_ENABLED(CONFIG_SOC_S5E8855)
#include <dt-bindings/soc/samsung/s5e8855-profiler-dt.h>
#elif IS_ENABLED(CONFIG_SOC_S5E9945)
#include <dt-bindings/soc/samsung/s5e9945-profiler-dt.h>
#else
#include <dt-bindings/soc/samsung/s5e9955-profiler-dt.h>
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_MAIN)
#define PROFILER_VERSION 2
#endif

#ifndef PROFILER_UNUSED
#define PROFILER_UNUSED(x)  ((void)(x))
#endif

/* Common Defs */

#define EPOLL_MAX_PID 4

#define PROFILER_STATE_SLEEP 0
#define PROFILER_STATE_GFX 1
#define PROFILER_STATE_JOB_SUBMIT 2
#define PROFILER_STATE_MONITOR 4

#define PROFILER_FORCED_RUNNING 1
#define PROFILER_FORCED_RUNNING_DBG 2

#define REQ_PROFILE 0

#define CONTROL_CMD_PERF_DN_RATIO 1
#define CONTROL_CMD_PERF_UP_RATIO 2
#define CONTROL_CMD_PERF_MAINBW_THR 3
#define CONTROL_CMD_DISABLE_LLC_WAY 4
#define CONTROL_CMD_ENABLE_PROFILER_GOVERNOR 5
#define CONTROL_CMD_MAX_LLC_WAY 6
#define CONTROL_CMD_LIGHT_LLC_WAY 7

#define CONTROL_CMD_TERMINATE_GFX 8
#define CONTROL_CMD_GFX_THR1 9
#define CONTROL_CMD_GFX_THR2 10
#define CONTROL_CMD_GFX_THR3 11
#define CONTROL_CMD_SET_CATID 12

#define DEF_GFX_COUNTER 10
#define DEF_GFX_THR1 50
#define DEF_GFX_THR2 3000
#define DEF_GFX_THR3 50

#define DEF_PERF_MAINBW_THR 2000
#define DEF_PERF_GPUBW_UP_RATIO 30
#define DEF_PERF_GPUBW_DN_RATIO 25

/* Orgin from exynos-profiler.h */
#define RATIO_UNIT		1000

#define nsec_to_usec(time)	((time) / 1000)
#define khz_to_mhz(freq)	((freq) / 1000)
#define ST_COST_BASE_TEMP_WEIGHT	(70 * 70)

/*
 * Input table should be DECENSING-ORDER
 * RELATION_LOW : return idx of freq lower than or same with input freq
 * RELATOIN_HIGH: return idx of freq higher thant or same with input freq
 */
#define RELATION_LOW	0
#define RELATION_HIGH	1

#define STATE_SCALE_CNT			(1 << 0)	/* ramainnig cnt even if freq changed */
#define STATE_SCALE_TIME		(1 << 1)	/* scaling up/down time with changed freq */
#define	STATE_SCALE_WITH_SPARE		(1 << 2)	/* freq boost with spare cap */
#define	STATE_SCALE_WO_SPARE		(1 << 3)	/* freq boost without spare cap */
#define	STATE_SCALE_WITH_ORG_CAP	(1 << 4)	/* freq boost with original capacity */

#define PWR_COST_CFVV	0
#define PWR_COST_CVV	1

/* Shared variables between profiler & external devices */
#define NUM_OF_PID 32

/* Array size of Graphics API information */
#define NUM_OF_GFXINFO 16

enum rtimeinfo {
	PRESUM,
	PREAVG,
	PREMAX,

	CPUSUM,
	CPUAVG,
	CPUMAX,

	V2SSUM,
	V2SAVG,
	V2SMAX,

	GPUSUM,
	GPUAVG,
	GPUMAX,

	V2FSUM,
	V2FAVG,
	V2FMAX,

	NUM_OF_RTIMEINFO,
};

enum user_type {
	SYSFS,
	PROFILER,
	NUM_OF_USER,
};

enum service_type {
	SVC_GFX,
#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GFLOW)
	SVC_JOB_SUBMIT,
#endif
	SVC_RESERVED,
	NUM_OF_PROFILER_SVC,
};

enum cstate {
	CS_ACTIVE,
	CLK_OFF,
	PWR_OFF,
	NUM_OF_CSTATE,
};

/* Structure for FREQ */
struct freq_table {
	u32	freq;		/* KHz */
	u32	volt;		/* uV */

	/*
	 * Equation for cost
	 * CPU/GPU : Dyn_Coeff/St_Coeff * F(MHz) * V(mV)^2
	 * MIF	   : Dyn_Coeff/St_Coeff * V(mV)^2
	 */
	u64	dyn_cost;
	u64	st_cost;
};

/*
 * It is free-run count
 * NOTICE: MUST guarantee no overflow
 */
struct freq_cstate {
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_snapshot {
	ktime_t	last_snap_time;
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_result {
	ktime_t	profile_time;

	ktime_t	*time[NUM_OF_CSTATE];
	u32	ratio[NUM_OF_CSTATE];
	u32	freq[NUM_OF_CSTATE];

	u64	dyn_power;
	u64	st_power;
};

#define for_each_cstate(state)		for (state = 0; state < NUM_OF_CSTATE; state++)

#endif /*__EXYNOS_PROFILER_DEFS_H__*/
