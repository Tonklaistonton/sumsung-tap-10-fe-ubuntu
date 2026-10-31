/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SCHEDULER_H_
#define _NPU_SCHEDULER_H_

#include <linux/version.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/thermal.h>

#include "include/npu-preset.h"
#include "include/npu-common.h"
#include "npu-session.h"
#include "include/npu-wakeup.h"
#include "npu-util-common.h"
#include "npu-util-autosleepthr.h"

#define NPU_MIN(a, b)		((a) < (b) ? (a) : (b))

static char *npu_scheduler_ip_name[] = {
	"CL0",
	"CL1",
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	"CL2",
#endif
	"MIF",
	"INT",
	"NPU",
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	"DSP",
#endif
	"DNC",
};

static char *npu_scheduler_core_name[] = {
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	"DSP",
#endif
	"DNC",
	"NPU",
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	"NPU1",
#endif
};

typedef enum {
	DIT_CORE = 0,
	DIT_DNC,
	DIT_MIF,
	DIT_INT,
	DIT_MAX
}dvfs_ip_type;

static char *npu_scheduler_debugfs_name[] = {
	"period",

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	"pid_target_thermal",
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	"pid_p_gain",
	"pid_i_gain",
	"pid_inv_gain",
	"pid_period",
#endif
	"debug_log",
#endif

#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
	"cpu_utilization",
	"dsp_utilization",
	"npu_utilization",
#endif

};

enum {
	NPU_SCHEDULER_PERIOD,

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	NPU_SCHEDULER_PID_TARGET_THERMAL,
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	NPU_SCHEDULER_PID_P_GAIN,
	NPU_SCHEDULER_PID_I_GAIN,
	NPU_SCHEDULER_PID_D_GAIN,
	NPU_SCHEDULER_PID_PERIOD,
#endif
	NPU_SCHEDULER_DEBUG_LOG,
#endif

#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
	NPU_SCHEDULER_CPU_UTILIZATION,
	NPU_SCHEDULER_DSP_UTILIZATION,
	NPU_SCHEDULER_NPU_UTILIZATION,
#endif
};

static int npu_scheduler_ip_pmqos_min[] = {
	PM_QOS_CLUSTER0_FREQ_MIN,
	PM_QOS_CLUSTER1_FREQ_MIN,
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	PM_QOS_CLUSTER2_FREQ_MIN,
#endif
	PM_QOS_BUS_THROUGHPUT,
	PM_QOS_DEVICE_THROUGHPUT,
	PM_QOS_NPU_THROUGHPUT,
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	PM_QOS_DSP_THROUGHPUT,
#endif
	PM_QOS_DNC_THROUGHPUT,
};

static int npu_scheduler_ip_pmqos_max[] = {
	PM_QOS_CLUSTER0_FREQ_MAX,
	PM_QOS_CLUSTER1_FREQ_MAX,
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	PM_QOS_CLUSTER2_FREQ_MAX,
#endif
	PM_QOS_BUS_THROUGHPUT_MAX,
	PM_QOS_DEVICE_THROUGHPUT_MAX,
	PM_QOS_NPU_THROUGHPUT_MAX,
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	PM_QOS_DSP_THROUGHPUT_MAX,
#endif
	PM_QOS_DNC_THROUGHPUT_MAX,
};

static inline int get_pm_qos_max(char *name)
{
	int i;
	int list_num = ARRAY_SIZE(npu_scheduler_ip_name);

	if (list_num != ARRAY_SIZE(npu_scheduler_ip_pmqos_max))
		return -1;

	for (i = 0; i < list_num; i++) {
		if (!strcmp(name, npu_scheduler_ip_name[i]))
			return npu_scheduler_ip_pmqos_max[i];
	}
	return -1;
}

static inline int get_pm_qos_min(char *name)
{
	int i;
	int list_num = ARRAY_SIZE(npu_scheduler_ip_name);

	if (list_num != ARRAY_SIZE(npu_scheduler_ip_pmqos_min))
		return -1;

	for (i = 0; i < list_num; i++) {
		if (!strcmp(name, npu_scheduler_ip_name[i]))
			return npu_scheduler_ip_pmqos_min[i];
	}
	return -1;
}

/*
 * time domain : us
 */

//#define CONFIG_NPU_SCHEDULER_OPEN_CLOSE
#define CONFIG_NPU_SCHEDULER_START_STOP
//#define CONFIG_NPU_USE_PI_DTM_DEBUG

#define NPU_SCHEDULER_NAME	"npu-scheduler"
#define NPU_SCHEDULER_DEFAULT_PERIOD	50	/* msec */
#define NPU_SCHEDULER_DEFAULT_AFMLIMIT	800000	/* 800MHz */
#define NPU_SCHEDULER_DEFAULT_TPF	500000	/* maximum 500ms */
#define NPU_SCHEDULER_DEFAULT_REQUESTED_TPF	16667
#define NPU_SCHEDULER_FPS_LOAD_RESET_FRAME_NUM	3
#define NPU_SCHEDULER_BOOST_TIMEOUT	20 /* msec */
#define NPU_SCHEDULER_DEFAULT_IDLE_DELAY	10 /* msec */
#define NPU_SCHEDULER_MAX_IDLE_DELAY	65 /* msec */
#define NPU_SCHEDULER_MAX_DVFS_INFO	50

static char *npu_perf_mode_name[] = {
	"normal",
	"boostonexe",
	"boost",
	"cpu",
	"DN",
	"boostonexemo",
	"boostdlv3",
	"boostblocking",
	"prune"
};

enum {
	NPU_PERF_MODE_NORMAL = 0,           /* no boost */
	NPU_PERF_MODE_NPU_BOOST_ONEXE,      /* Not used : legacy mode */
	NPU_PERF_MODE_NPU_BOOST,            /* KPI mode Boost */
	NPU_PERF_MODE_CPU_BOOST,            /* Not used : legacy mode */
	NPU_PERF_MODE_NPU_DN,               /* Not used : legacy mode */
	NPU_PERF_MODE_NPU_BOOST_ONEXE_MO,   /* Not used : legacy mode */
	NPU_PERF_MODE_NPU_BOOST_DLV3,       /* Not used : legacy mode */
	NPU_PERF_MODE_NPU_BOOST_BLOCKING,   /* KPI mode Sync */
	NPU_PERF_MODE_NPU_BOOST_PRUNE,      /* KPI mode PDCL */
	NPU_PERF_MODE_NUM,
};

#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION)
#define DSP_SHARED_PLL_CLK	1066000
#endif

#define NPU_SCHEDULER_DVFS_ARG_NUM	10
/* total arg num should be less than arg limit 16 */
#define NPU_SCHEDULER_DVFS_TOTAL_ARG_NUM	\
	(NPU_SCHEDULER_DVFS_ARG_NUM + NPU_PERF_MODE_NUM - 3)
#define HWACG_ALWAYS_DISABLE	(0x01)
#define	HWACG_STATUS_ENABLE	(0x3A)
#define	HWACG_STATUS_DISABLE	(0x7F)
#define	HWACG_NPU				(0x01)
#define	HWACG_DSP				(0x02)
#define	HWACG_DNC				(0x04)
#define	MO_SCEN_NORMAL			(0x01)
#define	MO_SCEN_PERF			(0x02)
#define	MO_SCEN_G3D_HEAVY		(0x03)
#define	MO_SCEN_G3D_PERF		(0x04)
#define	LLC_MAX_WAYS			(12)

#define NPU_SCH_DEFAULT_VALUE   (INT_MAX)

struct npu_scheduler_control {
	wait_queue_head_t		wq;
	int				result_code;
	atomic_t			result_available;
};

#define NPU_SCHEDULER_CMD_POST_RETRY_INTERVAL	100
#define NPU_SCHEDULER_CMD_POST_RETRY_CNT	10
#define NPU_SCHEDULER_HW_RESP_TIMEOUT		1000

#define NPU_SCHEDULER_PRIORITY_MIN	0
#define NPU_SCHEDULER_PRIORITY_MAX	255

struct npu_scheduler_fps_load {
	const struct npu_session *session;
	npu_uid_t	uid;
	u32		priority;
	u32		mode;
	struct list_head	list;
};

struct npu_scheduler_dvfs_info {
	char			*name;
	struct platform_device	*dvfs_dev;
	struct exynos_pm_qos_request	qos_req_min;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	struct exynos_pm_qos_request	qos_req_max;
#endif
	struct exynos_pm_qos_request	qos_req_min_dvfs_cmd;
	struct exynos_pm_qos_request	qos_req_max_dvfs_cmd;
	struct exynos_pm_qos_request	qos_req_min_nw_boost;
#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION)
	struct exynos_pm_qos_request	qos_req_max_cam_noti;
#endif
#if IS_ENABLED(CONFIG_NPU_AFM)
	struct exynos_pm_qos_request	qos_req_max_afm;
#endif
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
	struct exynos_pm_qos_request	qos_req_max_precision;
#endif
	u32			cur_freq;
	u32			min_freq;
	u32			max_freq;
	u32			limit_min;
	u32			limit_max;

	struct list_head ip_list;	/* device list */
};

#define NPU_DVFS_CMD_LEN	2
enum npu_dvfs_cmd {
	NPU_DVFS_CMD_MIN = 0,
	NPU_DVFS_CMD_MAX
};

struct dvfs_cmd_contents {
	u32 	cmd;
	u32	freq;
};

struct dvfs_cmd_map {
	char				*name;
	struct dvfs_cmd_contents	contents;
};

struct dvfs_cmd_list {
	char			*name;
	struct dvfs_cmd_map	*list;
	int			count;
};

#define PID_I_BUF_SIZE (5)
#define PID_MAX_FREQ_MARGIN (50000)
#ifdef CONFIG_NPU_USE_PI_DTM_DEBUG
#define PID_DEBUG_CNT (3000)
#endif

struct npu_scheduler_info {
	struct device	*dev;
	struct npu_device *device;

	u32		enable;
	u32		activated;
	u32		prev_mode;
	u32		mode;
	u32		mode_ref_cnt[NPU_PERF_MODE_NUM];
	u32		llc_mode;
	int		bts_scenindex;

	u32		period;
	atomic_t	cpuidle_cnt;

/* FPS-based load calculation */
	struct mutex	fps_lock;
	struct list_head fps_load_list;

/* IP dvfs information */
	struct list_head ip_list;	/* device list */

/* DVFS command list */
	struct dvfs_cmd_list *dvfs_list;	/* DVFS cmd list */

	struct mutex	exec_lock;
	bool		is_dvfs_cmd;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	struct wakeup_source		*sws;
#endif
	struct workqueue_struct		*sched_wq;
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	struct delayed_work		sched_work;
	struct thermal_zone_device 	*npu_tzd;
#endif
	struct delayed_work		boost_off_work;
	npu_errno_t			result_code;
	u32		llc_status;
	u32		llc_ways;
	u32		hwacg_status;

/* Frequency boost information only for open() and ioctl(S_FORMAT) */
	int		boost_count;

	struct npu_scheduler_control	sched_ctl;
	u32	dd_direct_path;
	u32	wait_hw_boot_flag;

/* NPU DTM */
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	int	idx_cnt;
	int	th_err_db[PID_I_BUF_SIZE];
	int	curr_thermal;
	int	dtm_curr_freq;
	int	dtm_prev_freq;
	int	debug_log_en;
	u32	pid_en;
	u32	pid_target_thermal;
	int	pid_max_clk;
	int	pid_p_gain;
	int	pid_i_gain;
	int	pid_inv_gain;
	int	pid_period;

#ifdef CONFIG_NPU_USE_PI_DTM_DEBUG
	short	debug_log[PID_DEBUG_CNT][3];
	int	debug_cnt;
	int	debug_dump_cnt;
#endif
#endif

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	u32	dvfs_table_num[2];
	u32	*dvfs_table;
	u32	dtm_nm_lut[2];
#endif
	struct mutex	param_handle_lock;
};

static inline struct dvfs_cmd_list *get_npu_dvfs_cmd_map(struct npu_scheduler_info *info, const char *cmd_name)
{
	int i;

	for (i = 0; ((info->dvfs_list) + i)->name != NULL; i++) {
		if (!strcmp(((info->dvfs_list) + i)->name, cmd_name))
			return (info->dvfs_list + i);
	}
	return (struct dvfs_cmd_list *)NULL;
}

static inline struct npu_scheduler_dvfs_info *get_npu_dvfs_info(struct npu_scheduler_info *info, const char *ip_name)
{
	struct npu_scheduler_dvfs_info *d;

	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp(ip_name, d->name))
			return d;
	}
	return (struct npu_scheduler_dvfs_info *)NULL;
}

struct npu_device;

int npu_scheduler_probe(struct npu_device *device);
int npu_scheduler_release(struct npu_device *device);
int npu_scheduler_open(struct npu_device *device);
int npu_scheduler_close(struct npu_device *device);
int npu_scheduler_resume(struct npu_device *device);
int npu_scheduler_suspend(struct npu_device *device);
int npu_scheduler_start(struct npu_device *device);
int npu_scheduler_stop(struct npu_device *device);
int npu_dvfs_set_freq(struct npu_scheduler_dvfs_info *d, void *req, u32 freq);
void npu_scheduler_send_wait_info_to_hw(struct npu_session *session,	struct npu_scheduler_info *info);
npu_s_param_ret npu_scheduler_param_handler(struct npu_session *sess, struct vs4l_param *param);
npu_s_param_ret npu_preference_param_handler(struct npu_session *sess, struct vs4l_param *param);
int npu_dvfs_get_ip_max_freq(struct vs4l_freq_param *param);
struct npu_scheduler_info *npu_scheduler_get_info(void);
int npu_scheduler_boost_on(struct npu_scheduler_info *info);
int npu_scheduler_boost_off(struct npu_scheduler_info *info);
int npu_scheduler_boost_off_timeout(struct npu_scheduler_info *info, s64 timeout);
int npu_scheduler_enable(struct npu_scheduler_info *info);
int npu_scheduler_disable(struct npu_scheduler_info *info);
void npu_scheduler_system_param_unset(void);
int npu_scheduler_register_session(const struct npu_session *session);
void npu_scheduler_unregister_session(const struct npu_session *session);
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
int npu_scheduler_get_clk(struct npu_scheduler_info *info, int lut_idx, dvfs_ip_type ip_type);
int npu_scheduler_get_lut_idx(struct npu_scheduler_info *info, int clk, dvfs_ip_type ip_type);
#endif
u32 npu_get_perf_mode(void);
void npu_scheduler_set_cpuidle(u32 val);

#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
struct npu_utilization_ops {
	int (*get_s_npu_utilization)(int n);
	int (*get_s_cpu_utilization)(void);
	int (*get_s_dsp_utilization)(void);
};

struct npu_scheduler_utilization_ops {
	struct device *dev;
	const struct npu_utilization_ops *utilization_ops;
};
extern const struct npu_utilization_ops n_utilization_ops;
#endif

#endif
