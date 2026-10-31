#ifndef __EXYNOS_PROFILER_COMMON_H__
#define __EXYNOS_PROFILER_COMMON_H__

#include <exynos-profiler-if.h>

/******************************************************************************/
/*                                      fops                                  */
/******************************************************************************/
#define IOCTL_MAGIC	'M'
#define IOCTL_READ_TSD	_IOR(IOCTL_MAGIC, 0x50, struct tunable_sharing_data)
#define IOCTL_READ_PSD	_IOR(IOCTL_MAGIC, 0x51, struct profile_sharing_data)
#define IOCTL_WR_DSD	_IOWR(IOCTL_MAGIC, 0x52, struct delta_sharing_data)

static struct mutex profiler_dsd_lock;	/* SHOULD: should check to merge profiler_lock and profiler_dsd_lock */
struct profiler_fops_priv {
	int pid;
	int id;
	int poll_mask;
};

struct device_file_operation {
	struct file_operations          fops;
	struct miscdevice               miscdev;
};


/******************************************************************************/
/*                            Macro for profiler                             */
/******************************************************************************/
#define for_each_mdomain(dom, id)	\
	for (id = -1; (dom) = next_domain(&id), id < NUM_OF_DOMAIN;)

/******************************************************************************/
/*                            Structure for profiler                             */
/******************************************************************************/
struct domain_data {
	bool enabled;
	s32 id;

	struct domain_fn *fn;
	void *private;
	struct attribute_group	attr_group;
};
/* Structure for IP's Private Data */
struct profiler_data_cpu {
	struct profiler_fn_cpu *fn;

	struct freq_qos_request pm_qos_min_req;
	s32	pm_qos_cpu;
	s32	pm_qos_min_freq;

	s32	local_min_freq[EPOLL_MAX_PID];

	s32	pid_util_max;
	s32	pid_util_min;

	s32	num_of_cpu;
};

struct profiler_data_gpu {
	struct profiler_fn_gpu *fn;

	struct exynos_pm_qos_request pm_qos_min_req;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	s32	local_min_freq[EPOLL_MAX_PID];
};

struct profiler_data_mif {
	struct profiler_fn_mif *fn;

	struct exynos_pm_qos_request pm_qos_min_req;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	s32	local_min_freq[EPOLL_MAX_PID];

	s32	pid_util_max;
	s32	pid_util_min;
};

struct profiler_data_dsu {
	struct profiler_fn_dsu *fn;

	struct dev_pm_qos_request pm_qos_min_req;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	s32	local_min_freq[EPOLL_MAX_PID];
};

struct domain_fn {
	u32 (*get_table_cnt)(s32 id);
	u32 (*get_freq_table)(s32 id, u32 *table);
	u32 (*get_max_freq)(s32 id);
	u32 (*get_min_freq)(s32 id);
	u32 (*get_freq)(s32 id);
	void (*get_power)(s32 id, u64 *dyn_power, u64 *st_power);
	void (*get_power_change)(s32 id, s32 freq_delta_ratio,
		u32 *freq, u64 *dyn_power, u64 *st_power);
	u32 (*get_active_pct)(s32 id);
	s32 (*get_temp)(s32 id);
	void (*set_margin)(s32, s32 margin);
	u32 (*update_mode)(s32 id, int mode);
};

struct profiler_fn_cpu {
	s32 (*cpu_active_pct)(s32 id, s32 *cpu_active_pct);
	s32 (*cpu_asv_ids)(s32 id);
};

struct profiler_fn_gpu {
	void (*disable_llc_way)(bool disable);

	void (*set_profiler_governor)(int mode);
	void (*set_targetframetime)(int us);
	void (*set_vsync)(ktime_t ktime_us);
	void (*get_frameinfo)(s32 *nrframe, u64 *nrvsync, u64 *delta_ms);
	void (*get_gfxinfo)(u64 *times, u64 *gfxinfo);
	void (*get_pidinfo)(u32 *list, u8 *core_list, u8 *renderer_name);

	s32 (*pb_get_mgninfo)(int id, u16 *no_mgnboost, u16 *no_mlckboost);
	void (*set_pb_params)(int idx, int value);
	int (*get_pb_params)(int idx);
};

struct profiler_fn_mif {
	u64 (*get_stats0_sum)(void);
	u64 (*get_stats0_ratio)(void);
	u64 (*get_stats0_avg)(void);
	u64 (*get_stats1_sum)(void);
	u64 (*get_stats1_ratio)(void);
	u64 (*get_stats2_sum)(void);
	u64 (*get_stats2_ratio)(void);
	u64 (*get_llc_status)(void);
};

struct profiler_fn_dsu {
	/* Empty */
};

/* Shared Function */
extern s32 exynos_profiler_register_domain(s32 id, struct domain_fn *fn, void *private_fn);
extern struct domain_data* exynos_profiler_get_domain(s32 id);

#endif
