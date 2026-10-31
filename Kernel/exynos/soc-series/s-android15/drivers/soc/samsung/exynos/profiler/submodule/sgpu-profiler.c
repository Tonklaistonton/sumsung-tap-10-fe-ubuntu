// SPDX-License-Identifier: GPL-2.0
/*
 * @file profiler-sgpu.c
 * @copyright 2024 Samsung Electronics
 */
#include <sgpu-profiler.h>
#include <sgpu-profiler-governor.h>

static struct exynos_pm_qos_request sgpu_profiler_gpu_min_qos;
static struct freq_qos_request sgpu_profiler_cpu_min_qos[NUM_OF_CPU_DOMAIN];
static struct interframe_data interframe[PROFILER_TABLE_MAX];
static struct rtp_data rtp;
static struct pboost_data pb;
static struct pid_data pid;
static struct info_data pinfo;
static unsigned int sgpu_profiler_polling_interval;
static u32 sgpu_profiler_pb_fps_calc_average_us;
static int sgpu_profiler_margin[NUM_OF_DOMAIN];
static int sgpu_profiler_margin_applied[NUM_OF_DOMAIN];
static struct mutex sgpu_profiler_pmqos_lock;
static struct mscaler_control mscaler;

static struct exynos_profiler_gpudev_fn *profiler_gpudev_fn;

/* prototype of internal functions */
static void sgpu_profiler_pid_init(void);
void sgpu_profiler_mscaler_init(void);
void sgpu_profiler_mscaler_setid(u32 cmd);
void sgpu_profiler_mscaler_set_fs_fz(u32 value);
u32 sgpu_profiler_mscaler_getid(void);
u32 sgpu_profiler_mscaler_get_fs_fz(void);
int sgpu_profiler_mscaler(int id, int freq, int margin);

static int sgpu_profiler_get_current_cpuid(void)
{
	unsigned int core;
	int id;

	core = get_cpu();
	id = PROFILER_GET_CL_ID(core);
	put_cpu();

	return id;
}

static void sgpu_profiler_init_pmqos(void)
{
	int id;
	struct cpufreq_policy *policy;

	if (pinfo.cpufreq_pm_qos_added == 0) {
		pinfo.cpufreq_pm_qos_added = 1;

		for (id = 0; id < NUM_OF_CPU_DOMAIN ; id++) {
			policy = cpufreq_cpu_get(PROFILER_GET_CPU_BASE_ID(id));
			if (policy != NULL)
				freq_qos_tracer_add_request(&policy->constraints,
						&sgpu_profiler_cpu_min_qos[id],
						FREQ_QOS_MIN, FREQ_QOS_MIN_DEFAULT_VALUE);
		}
	}

	memset(&pb.pmqos_minlock_prev[0], 0, sizeof(pb.pmqos_minlock_prev[0]) * NUM_OF_DOMAIN);
	memset(&pb.pmqos_minlock_next[0], 0, sizeof(pb.pmqos_minlock_next[0]) * NUM_OF_DOMAIN);
}

static void sgpu_profiler_set_pmqos_next(void)
{
	int id;

	if (pinfo.cpufreq_pm_qos_added > 0) {
		for (id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
			if (pb.pmqos_minlock_prev[id] != pb.pmqos_minlock_next[id]) {
				if (freq_qos_request_active(&sgpu_profiler_cpu_min_qos[id])) {
					freq_qos_update_request(&sgpu_profiler_cpu_min_qos[id], pb.pmqos_minlock_next[id]);
					pb.mgninfo[id].no_mlckboost++;
				}
				pb.pmqos_minlock_prev[id] = pb.pmqos_minlock_next[id];
			}
		}
	}

	id = PROFILER_GPU;
	if (pb.pmqos_minlock_prev[id] != pb.pmqos_minlock_next[id]) {
		/* If pbooster runs in GPU DVFS Governor, pmqos request will produce deadlock
		exynos_pm_qos_update_request(&sgpu_profiler_gpu_min_qos, pb.pmqos_minlock_next[id]);*/
		pb.pmqos_minlock_prev[id] = pb.pmqos_minlock_next[id];
	}
}

static void sgpu_profiler_reset_next_minlock(void)
{
	memset(&pb.pmqos_minlock_next[0], 0, sizeof(pb.pmqos_minlock_next[0]) * NUM_OF_DOMAIN);
}


void sgpu_profiler_set_profiler_governor(int mode)
{

	char *sgpu_profiler_governor = "profiler";
	char *sgpu_cur_governor;

	static char sgpu_prev_governor[SGPU_DRV_GOVERNOR_NAME_LEN + 1] = {0};
	static unsigned sgpu_prev_polling_interval;

	int ret = 0;

	if ((mode == CHANGE_GOV_TO_PROFILER) && (sgpu_prev_polling_interval == 0)) {
		sgpu_cur_governor = profiler_gpudev_fn->get_governor();

		/* change gpu governor to profiler if not set */
		if (strncmp(sgpu_cur_governor, sgpu_profiler_governor, strlen(sgpu_profiler_governor))) {
			strncpy(sgpu_prev_governor, sgpu_cur_governor, SGPU_DRV_GOVERNOR_NAME_LEN);
			ret = profiler_gpudev_fn->set_governor(sgpu_profiler_governor);
			if (!ret) {
				sgpu_prev_polling_interval = profiler_gpudev_fn->get_polling_interval();
				profiler_gpudev_fn->set_polling_interval(sgpu_profiler_polling_interval);
				profiler_gpudev_fn->set_autosuspend_delay(PROFILER_AUTOSUSPEND_DELAY_MS);
				sgpu_profiler_init_pmqos();
			} else {
				pr_err(	"cannot change GPU governor for PROFILER, err:%d", ret);
			}
		}
	} else if ((mode == CHANGE_GOV_TO_PREV) && (sgpu_prev_polling_interval > 0) && sgpu_prev_governor[0] != 0) {
		sgpu_cur_governor = profiler_gpudev_fn->get_governor();
		/* change gpu governor to previous value if profiler */
		if (!strncmp(sgpu_cur_governor, sgpu_profiler_governor, strlen(sgpu_profiler_governor))) {
			ret = profiler_gpudev_fn->set_governor(sgpu_prev_governor);
			if (!ret) {
				profiler_gpudev_fn->set_polling_interval(sgpu_prev_polling_interval);
				sgpu_prev_polling_interval = 0;
				profiler_gpudev_fn->set_autosuspend_delay(SGPU_DRV_AUTOSUSPEND_DELAY_MS);
				mutex_lock(&sgpu_profiler_pmqos_lock);
				sgpu_profiler_reset_next_minlock();
				sgpu_profiler_set_pmqos_next();
				mutex_unlock(&sgpu_profiler_pmqos_lock);
				memset(sgpu_prev_governor, 0, SGPU_DRV_GOVERNOR_NAME_LEN + 1);
			} else {
				pr_err("cannot change GPU governor back to %s, err:%d", sgpu_prev_governor, ret);
			}
		}
	} else {
		pr_err("cannot change GPU governor from:%s, mode=%d, sgpu_prev_polling_interval=%d",
				profiler_gpudev_fn->get_governor(), mode, sgpu_prev_polling_interval);
	}
}

int sgpu_profiler_set_freq_margin(int id, int freq_margin)
{
	if (freq_margin > 1000 || freq_margin < -1000) {
		pr_err("freq_margin range : (-1000 ~ 1000)\n");
		return -EINVAL;
	}
	if (id < 0 || id >= NUM_OF_DOMAIN) {
		pr_err("Out of Range for id");
		return -EINVAL;
	}

	if (sgpu_profiler_margin[id] != freq_margin) {
		sgpu_profiler_margin[id] = freq_margin;

		if (id < NUM_OF_CPU_DOMAIN) {
			sgpu_profiler_margin_applied[id] = freq_margin;
			emstune_set_peltboost(0, 0, PROFILER_GET_CPU_BASE_ID(id), freq_margin / 10);
			emstune_set_peltboost(0, 1, PROFILER_GET_CPU_BASE_ID(id), freq_margin / 10);
		}
	}

	return 0;
}

static int sgpu_profiler_get_target_frametime(void)
{
	return pb.target_frametime;
}

void sgpu_profiler_set_targetframetime(int us)
{
	if ((us < PROFILER_RENDERTIME_MAX && us > PROFILER_RENDERTIME_MIN) || us == 0) {
		pb.target_frametime = (u32)us;
		if (us > 0)
			pb.target_fps = 10000000 / (u32)us;
		else
			pb.target_fps = 0;
	}
}

void sgpu_profiler_set_vsync(ktime_t time_us)
{
	rtp.vsync.interval = time_us - rtp.vsync.prev;
	rtp.vsync.prev = time_us;
	if (rtp.vsync.lastsw == 0)
		rtp.vsync.lastsw = time_us;
	if (rtp.vsync.curhw == 0)
		rtp.vsync.curhw = time_us;
	if (atomic_read(&rtp.vsync.swapcall_counter) > 0)
		rtp.vsync.frame_counter++;
	atomic_set(&rtp.vsync.swapcall_counter, 0);
	rtp.vsync.counter++;
	pinfo.vsync.counter++;
}

static struct interframe_data *sgpu_profiler_get_next_frameinfo(void)
{
	if (rtp.head == rtp.tail)
		return NULL;
	struct interframe_data *dst = &interframe[rtp.head];
	rtp.head = (rtp.head + 1) % PROFILER_TABLE_MAX;
	return dst;
}

void sgpu_profiler_get_frameinfo(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	*nrframe = pinfo.frame_counter;
	pinfo.frame_counter = 0;
	*nrvsync = pinfo.vsync.counter;
	pinfo.vsync.counter = 0;
	*delta_ms = (u64)(rtp.vsync.prev - (ktime_t)(pinfo.last_updated_vsync_time)) / 1000;
	pinfo.last_updated_vsync_time = (u64)rtp.vsync.prev;
}

void sgpu_profiler_get_gfxinfo(u64 *times, u64 *gfxinfo)
{
	struct interframe_data last_frame;
	u64 last_frame_nrq = 0;

	/* Transaction to transfer Rendering Time to profiler */
	if (rtp.readout != 0) {
		memset(&last_frame, 0, sizeof(struct interframe_data));
	} else {
		int tail = rtp.tail;
		int index = (tail == 0) ? PROFILER_TABLE_MAX - 1 : tail - 1;
		index = umin(index, PROFILER_TABLE_MAX - 1);

		memcpy(&last_frame, &interframe[index], sizeof(struct interframe_data));
	}
	rtp.readout = 1;

	last_frame_nrq = last_frame.nrq;
	if (last_frame_nrq > 0) {
		int i;
		u64 tmp;

		times[PRESUM] = last_frame.rtime[PRESUM];
		tmp = times[PRESUM] / last_frame_nrq;
		times[PREAVG] = umin(tmp, PROFILER_RENDERTIME_MAX);
		times[CPUSUM] = last_frame.rtime[CPUSUM];
		tmp = times[CPUSUM] / last_frame_nrq;
		times[CPUAVG] = umin(tmp, PROFILER_RENDERTIME_MAX);
		times[V2SSUM] = last_frame.rtime[V2SSUM];
		tmp = times[V2SSUM] / last_frame_nrq;
		times[V2SAVG] = umin(tmp, PROFILER_RENDERTIME_MAX);
		times[GPUSUM] = last_frame.rtime[GPUSUM];
		tmp = times[GPUSUM] / last_frame_nrq;
		times[GPUAVG] = umin(tmp, PROFILER_RENDERTIME_MAX);
		times[V2FSUM] = last_frame.rtime[V2FSUM];
		tmp = times[V2FSUM] / last_frame_nrq;
		times[V2FAVG] = umin(tmp, PROFILER_RENDERTIME_MAX);

		times[PREMAX] = umin(last_frame.rtime[PREMAX], PROFILER_RENDERTIME_MAX);
		times[CPUMAX] = umin(last_frame.rtime[CPUMAX], PROFILER_RENDERTIME_MAX);
		times[V2SMAX] = umin(last_frame.rtime[V2SMAX], PROFILER_RENDERTIME_MAX);
		times[GPUMAX] = umin(last_frame.rtime[GPUMAX], PROFILER_RENDERTIME_MAX);
		times[V2FMAX] = umin(last_frame.rtime[V2FMAX], PROFILER_RENDERTIME_MAX);

		rtp.vsync.frame_counter = 0;
		rtp.vsync.counter = 0;

		for (i = 0; i < NUM_OF_GFXINFO; i++)
			gfxinfo[i] = last_frame.gfxinfo[i] / last_frame_nrq;
	} else {
		memset(times, 0, sizeof(times[0]) * NUM_OF_RTIMEINFO);
		memset(gfxinfo, 0, sizeof(gfxinfo[0]) * NUM_OF_GFXINFO);
	}
}

void sgpu_profiler_get_pidinfo(u32 *list, u8 *core_list, u8 *renderer_name)
{
	int prev_top;

	if (list == NULL) {
		pr_err("Null Pointer for list");
		return;
	}
	if (core_list == NULL) {
		pr_err("Null Pointer for core_list");
		return;
	}

	if (atomic_read(&pid.list_readout) != 0) {
		memset(&list[0], 0, sizeof(list[0]) * NUM_OF_PID);
		memset(&core_list[0], 0, sizeof(core_list[0]) * NUM_OF_PID);
		renderer_name[0] = 0;
	} else {
		/* Transaction to transfer PID list to profiler */
		int limit_count = 2;
		do {
			struct interframe_data *dst = &interframe[rtp.tail];
			prev_top = atomic_read(&pid.list_top);
			memcpy(list, &pid.list[0], sizeof(pid.list));
			memcpy(core_list, &pid.core_list[0], sizeof(pid.core_list));
			memcpy(renderer_name, &dst->renderer_name, sizeof(dst->renderer_name));
		} while (prev_top != atomic_read(&pid.list_top) && limit_count-- > 0);
	}
	atomic_set(&pid.list_readout, 1);
}

/* Performance Booster */
int sgpu_profiler_pb_get_params(int idx)
{
	int ret = -1;
	switch(idx) {
		case PB_CONTROL_USER_TARGET_PID:
			ret = pb.user_target_pid;
			break;
		case PB_CONTROL_GPU_FORCED_BOOST_ACTIVE:
			ret = pb.gpu_forced_boost_activepct;
			break;
		case PB_CONTROL_CPUTIME_BOOST_PM:
			ret = pb.cputime_boost_pm;
			break;
		case PB_CONTROL_GPUTIME_BOOST_PM:
			ret = pb.gputime_boost_pm;
			break;
		case PB_CONTROL_CPU_MGN_MAX:
			ret = pb.cpu_mgn_margin_max;
			break;
		case PB_CONTROL_CPU_MGN_FRAMEDROP_PM:
			ret = pb.framedrop_detection_pm_mgn;
			break;
		case PB_CONTROL_CPU_MGN_FPSDROP_PM:
			ret = pb.fpsdrop_detection_pm_mgn;
			break;
		case PB_CONTROL_CPU_MGN_ALL_FPSDROP_PM:
			ret = pb.target_clusters_bitmap_fpsdrop_detection_pm;
			break;
		case PB_CONTROL_CPU_MGN_ALL_CL_BITMAP:
			ret = (int)pb.target_clusters_bitmap;
			break;
		case PB_CONTROL_CPU_MINLOCK_BOOST_PM_MAX:
			ret = pb.cpu_minlock_margin_max;
			break;
		case PB_CONTROL_CPU_MINLOCK_FRAMEDROP_PM:
			ret = pb.framedrop_detection_pm_minlock;
			break;
		case PB_CONTROL_CPU_MINLOCK_FPSDROP_PM:
			ret = pb.fpsdrop_detection_pm_minlock;
			break;
		case PB_CONTROL_DVFS_INTERVAL:
			ret = sgpu_profiler_polling_interval;
			break;
		case PB_CONTROL_FPS_AVERAGE_US:
			ret = sgpu_profiler_pb_fps_calc_average_us;
			break;
		case PB_CONTROL_MSCALER_GETTYPE:
			ret = sgpu_profiler_mscaler_getid();
			break;
		case PB_CONTROL_MSCALER_FREQS:
			ret = sgpu_profiler_mscaler_get_fs_fz();
			break;
		default:
			break;
	}
	return ret;
}

void sgpu_profiler_pb_set_params(int idx, int value)
{
	switch(idx) {
		case PB_CONTROL_USER_TARGET_PID:
			pb.user_target_pid = value;
			break;
		case PB_CONTROL_GPU_FORCED_BOOST_ACTIVE:
			pb.gpu_forced_boost_activepct = value;
			break;
		case PB_CONTROL_CPUTIME_BOOST_PM:
			pb.cputime_boost_pm = value;
			break;
		case PB_CONTROL_GPUTIME_BOOST_PM:
			pb.gputime_boost_pm = value;
			break;
		case PB_CONTROL_CPU_MGN_MAX:
			pb.cpu_mgn_margin_max = value;
			break;
		case PB_CONTROL_CPU_MGN_FRAMEDROP_PM:
			pb.framedrop_detection_pm_mgn = value;
			break;
		case PB_CONTROL_CPU_MGN_FPSDROP_PM:
			pb.fpsdrop_detection_pm_mgn = value;
			break;
		case PB_CONTROL_CPU_MGN_ALL_FPSDROP_PM:
			pb.target_clusters_bitmap_fpsdrop_detection_pm = value;
			break;
		case PB_CONTROL_CPU_MGN_ALL_CL_BITMAP:
			pb.target_clusters_bitmap = (u32)value;
			break;
		case PB_CONTROL_CPU_MINLOCK_BOOST_PM_MAX:
			pb.cpu_minlock_margin_max = value;
			break;
		case PB_CONTROL_CPU_MINLOCK_FRAMEDROP_PM:
			pb.framedrop_detection_pm_minlock = value;
			break;
		case PB_CONTROL_CPU_MINLOCK_FPSDROP_PM:
			pb.fpsdrop_detection_pm_minlock = value;
			break;
		case PB_CONTROL_DVFS_INTERVAL:
			if (value >= 4 && value <= 32)
					sgpu_profiler_polling_interval = value;
			break;
		case PB_CONTROL_FPS_AVERAGE_US:
			if (value >= 32000 && value <= 1000000)
					sgpu_profiler_pb_fps_calc_average_us = value;
			break;
		case PB_CONTROL_MSCALER_SETID:
			sgpu_profiler_mscaler_setid(value);
			break;
		case PB_CONTROL_MSCALER_FREQS:
			sgpu_profiler_mscaler_set_fs_fz(value);
			break;
		default:
			break;
    }
}

void sgpu_profiler_pb_set_dbglv(int value)
{
	rtp.debug_level = value;
}

void sgpu_profiler_pb_set_freqtable_en(int id, int en)
{
	pb.freqtable_en[id] = en;
	pr_info("%s: id=%d has set freqtable successfully (en=%d)",__func__, id, en);
}

int sgpu_profiler_pb_get_freqtable_en(int id)
{
	return pb.freqtable_en[id];
}

int sgpu_profiler_pb_set_freqtable(int id, int cnt, int *table)
{
	int i;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		pr_err("Out of Range for id");
		return -EINVAL;
	}

	if (table == NULL) {
		pr_err("%s: id=%d table is Null Pointer for table", __func__, id);
		return -EINVAL;
	}

	if (cnt == 0) {
		pr_err("invalid freq_table count");
		return -EINVAL;
	}

	if (pb.freqtable[id] != NULL) {
		pr_err("Already allocated by first device for id(%d)", id);
		return -EINVAL;
	}

	pb.freqtable[id] = kzalloc(sizeof(int) * cnt, GFP_KERNEL);
	if (!pb.freqtable[id]) {
		pr_err("failed to allocate for estpower talbe id(%d)", id);
		return -ENOMEM;
	}

	pb.freqtable_size[id] = cnt;

	for (i = 0; i < cnt; i++)
		pb.freqtable[id][i] = table[i];

	if (!pb.freqtable[id]) {
		pr_err("%s: pb.freqtable[%d] is NULL", __func__, id);
		return -EINVAL;
	}

	sgpu_profiler_pb_set_freqtable_en(id, 1);
	return 0;
}

void sgpu_profiler_pb_set_cur_freqlv(int id, int idx)
{
	if (id < 0 || id >= NUM_OF_DOMAIN) {
		pr_err("Out of Range for id");
		return;
	}
	pb.cur_freqlv[id] = idx;

	if (pb.cur_ndvfs[id] > 128) {
		pb.cur_sumfreq[id] = pb.freqtable[id][idx];
		pb.cur_ndvfs[id] = 1;
	} else {
		pb.cur_sumfreq[id] += pb.freqtable[id][idx];
		pb.cur_ndvfs[id]++;
	}
}

void sgpu_profiler_pb_set_cur_freq(int id, int freq)
{
	int i;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		pr_err("Out of Range for id");
		return;
	}

	if (!pb.freqtable[id] || pb.freqtable_size[id] == 0) {
		pr_err("FreqPowerTable was not set, id(%d)", id);
		return;
	}

	if (pb.cur_ndvfs[id] > 128) {
		pb.cur_sumfreq[id] = freq;
		pb.cur_ndvfs[id] = 1;
	} else {
		pb.cur_sumfreq[id] += freq;
		pb.cur_ndvfs[id]++;
	}

	for (i = pb.cur_freqlv[id]; i > 0; i--)
		if (i < pb.freqtable_size[id] && freq <= pb.freqtable[id][i])
			break;

	while (i < pb.freqtable_size[id]) {
		if (freq >= pb.freqtable[id][i]) {
			pb.cur_freqlv[id] = i;
			return;
		}
		i++;
	}

	if (pb.freqtable_size[id] > 0)
		pb.cur_freqlv[id] = pb.freqtable_size[id] - 1;
}

int sgpu_profiler_pb_get_mgninfo(int id, u16 *no_mgnboost, u16 *no_mlckboost)
{
	int avg_margin = 0;

	if (pb.mgninfo[id].no_value > 0)
		avg_margin = pb.mgninfo[id].sum_margin / (s32)pb.mgninfo[id].no_value;

	*no_mgnboost = pb.mgninfo[id].no_mgnboost;
	*no_mlckboost = pb.mgninfo[id].no_mlckboost;

	pb.mgninfo[id].sum_margin = 0;
	pb.mgninfo[id].no_value = 0;
	pb.mgninfo[id].no_mgnboost = 0;
	pb.mgninfo[id].no_mlckboost = 0;
	return avg_margin;
}

static int sgpu_profiler_pb_get_dvfsfreqh(int id, int targetfreq)
{
	int s = 0;
	int e = pb.freqtable_size[id] - 1;
	int m = (s + e) >> 1;
	if (!pb.freqtable[id]) {
		pr_err("%s: pb.freqtable[%d] is NULL",__func__, id);
	}
	if (targetfreq >= pb.freqtable[id][s])
		return pb.freqtable[id][s];
	if (targetfreq <= pb.freqtable[id][e])
		return pb.freqtable[id][e];

	while (s < m) {
		if (targetfreq <= pb.freqtable[id][m])
			s = m;
		else
		e = m;
		m = (s + e) >> 1;
	}
	return pb.freqtable[id][s];
}

static int sgpu_profile_getlastframefreq(int id) {
	return pb.lastframe_avgfreq[id];
}

int profiler_getavgfreq(int id)
{
	if (!pb.freqtable[id]) {
		pr_err("%s: pb.freqtable[%d] is NULL",__func__, id);
	}
	if (pb.cur_ndvfs[id] == 0)
		return pb.freqtable[id][pb.cur_freqlv[id]];
	if (pb.cur_ndvfs[id] == 1)
		return pb.cur_sumfreq[id];
	return pb.cur_sumfreq[id] / pb.cur_ndvfs[id];
}

static void sgpu_profiler_pb_update_minlock(u64 nowtimeus, int rtp_tail)
{
	struct interframe_data *dst;
	int cur_clid = pb.target_clusterid;
	int id, fnext, fnextdvfs;
	int tft = (int)pb.target_frametime;
	int tcur = pb.frinfo.expected_swaptime;
	int fcur, flast;
	int minlock_mgn;

	if (rtp_tail >= PROFILER_TABLE_MAX) {
		pr_err("Index for interframe is over MAX");
		return;
	}

	dst = &interframe[rtp_tail];

	if ((!pb.freqtable[cur_clid]) || (pb.freqtable_size[cur_clid] <= 0)) {
		pr_err("FreqPowerTable was not set, cpuid(%d)", cur_clid);
		return;
	}


	/* compute fmin_cpu */
	for (id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
		if (id == cur_clid) {
			int cputime = rtp.last.cputime + rtp.last.cputime * pb.cputime_boost_pm / RATIO_UNIT;

			flast = sgpu_profile_getlastframefreq(pb.target_clusterid_prev) / RATIO_UNIT;
			fcur = profiler_getavgfreq(id) / RATIO_UNIT;

			minlock_mgn = pb.next_cpu_minlock_boosting_framedrop;
			if (pb.next_cpu_minlock_boosting_fpsdrop > minlock_mgn)
				minlock_mgn = pb.next_cpu_minlock_boosting_fpsdrop;

			if (minlock_mgn > 0) {
				if (cputime > tft)
					minlock_mgn += (cputime - tft) * RATIO_UNIT / tft;

				fnext = flast * (RATIO_UNIT + minlock_mgn);
			} else
				fnext = 0;

			fnextdvfs = sgpu_profiler_pb_get_dvfsfreqh(id, fnext);

			if ((rtp.debug_level & 8) == 8)
				sgpu_profiler_info("EGP:PB:fmin:%d: mgn(d,m)=%4d,%4d <- %4d,%4d, time(last,exp)=%8llu,%8d, f(pre,cur,nxt,dvfs)=%5d,%5d,%5d,%5d\n"
						, id, sgpu_profiler_margin[id], minlock_mgn
						, pb.next_cpu_minlock_boosting_framedrop
						, pb.next_cpu_minlock_boosting_fpsdrop
						, rtp.last.cputime, tcur
						, flast, fcur, fnext/RATIO_UNIT, fnextdvfs/RATIO_UNIT);
		} else
			fnextdvfs = 0;

		pb.pmqos_minlock_next[id] = fnextdvfs;
	}

	/* compute fmin_gpu */
	fnextdvfs = 0;
	if (pb.next_cpu_minlock_boosting_fpsdrop > 0) {
		int gputime = rtp.last.gputime + rtp.last.gputime * pb.gputime_boost_pm / RATIO_UNIT;

		if (gputime > tft) {
			flast = sgpu_profile_getlastframefreq(PROFILER_GPU) / RATIO_UNIT;
			fcur = profiler_getavgfreq(PROFILER_GPU) / RATIO_UNIT;
			if (gputime > pb.target_frametime) {
				minlock_mgn = (int)((gputime - pb.target_frametime) * RATIO_UNIT / pb.target_frametime);
				fnext = flast * (RATIO_UNIT + minlock_mgn);

				fnextdvfs = sgpu_profiler_pb_get_dvfsfreqh(PROFILER_GPU, fnext);
				if ((rtp.debug_level & 8) == 8)
					sgpu_profiler_info("EGP:PB:fmin:%d: mgn(d,m)=%4d,%4d, time(last,exp)=%8llu,%8d, f(pre,cur,nxt,dvfs)=%5d,%5d,%5d,%5d\n"
							, PROFILER_GPU, sgpu_profiler_margin[PROFILER_GPU], minlock_mgn, rtp.last.gputime, tcur
							, flast, fcur, fnext/RATIO_UNIT, fnextdvfs/RATIO_UNIT);
			}
		}
	}
	pb.pmqos_minlock_next[PROFILER_GPU] = fnextdvfs;
}

static void sgpu_profiler_pb_frameinfo_data_reset(void)
{
	int i;

	for (i = 0; i < PROFILER_PB_FRAMEINFO_TABLE_SIZE; i++)
		pb.frinfo.pid[i] = 0;
}

static int sgpu_profiler_pb_frameinfo_data_pididx(u32 pid)
{
	int i;

	for (i = 0; i < PROFILER_PB_FRAMEINFO_TABLE_SIZE; i++)
		if (pb.frinfo.pid[i] != 0 && pb.frinfo.pid[i] == pid)
			return i;
	return -1;
}

static int sgpu_profiler_pb_frameinfo_data_getnopids(void)
{
	int i;
	int no = 0;

	for (i = 0; i < PROFILER_PB_FRAMEINFO_TABLE_SIZE; i++)
		if (pb.frinfo.pid[i] != 0)
			no++;

	return no;
}

static int sgpu_profiler_pb_frameinfo_data_getempty(void)
{
	int i;
	u64 lru_ts = 0;
	int lruidx = -1;

	for (i = 0; i < PROFILER_PB_FRAMEINFO_TABLE_SIZE; i++) {
		if (pb.frinfo.pid[i] == 0)
			return i;

		/* if there is not an empty slot, find lru policy */
		if (lru_ts == 0 || pb.frinfo.latest_ts[i] < lru_ts) {
			lru_ts = pb.frinfo.latest_ts[i];
			lruidx = i;
		}
	}

	return lruidx;
}

static int sgpu_profiler_pb_frameinfo_data_getmaxhitidx(void)
{
	int i;
	int maxhit = 0;
	int maxidx = -1;

	for (i = 0; i < PROFILER_PB_FRAMEINFO_TABLE_SIZE; i++) {
		if (pb.frinfo.pid[i] > 0 && maxhit < pb.frinfo.hit[i]) {
			maxidx = i;
			maxhit = pb.frinfo.hit[i];
		}
	}

	return maxidx;
}

static void sgpu_profiler_pb_frameinfo_profile(int qidx)
{
	ktime_t nowtimeus = ktime_get_real() / 1000L;
	u64 latest_ts;
	int previdx = (qidx + PROFILER_TABLE_MAX - 1) % PROFILER_TABLE_MAX;
	int hitpfi = -1;
	int i;

	latest_ts = interframe[qidx].timestamp;
	if ((nowtimeus - interframe[previdx].timestamp) > 1000000L) {
		/* If timestamp of prev is older than 1sec, reset */
		sgpu_profiler_pb_frameinfo_data_reset();
	} else {
		/* Move 1sec index */
		for (i = 0; i < PROFILER_TABLE_MAX; i++) {
			int idx = (pb.frinfo.onesecidx + i) % PROFILER_TABLE_MAX;
			u64 ts = interframe[idx].timestamp;
			u64 interval = latest_ts - ts;
			int pfi;

			/* If current timestamp is in 1sec, finish this loop */
			if (interval <= sgpu_profiler_pb_fps_calc_average_us) {
				pb.frinfo.onesecidx = idx;
				break;
			}

			/* Find index in pb_frameinfo_data which matched to pid */
			pfi = sgpu_profiler_pb_frameinfo_data_pididx(interframe[idx].tgid);
			if (pfi >= 0) {
				pb.frinfo.hit[pfi]--;
				/* if not hit any more, delete it */
				if (pb.frinfo.hit[pfi] == 0)
					pb.frinfo.pid[pfi] = 0;
				else {
					/* update earliest timestamp */
					pb.frinfo.earliest_ts[pfi] = ts;
				}
			}
		}

		/* find idx for new pid */
		hitpfi = sgpu_profiler_pb_frameinfo_data_pididx(interframe[qidx].tgid);
	}

	if (hitpfi >= 0) {
		/* Update if hit */
		pb.frinfo.hit[hitpfi]++;
		pb.frinfo.latest_interval[hitpfi] = latest_ts - pb.frinfo.latest_ts[hitpfi];
		pb.frinfo.latest_ts[hitpfi] = latest_ts;
	} else {
		/* Miss, then add it */
		int pfi = sgpu_profiler_pb_frameinfo_data_getempty();

		if (pfi >= 0) {
			pb.frinfo.pid[pfi] = interframe[qidx].tgid;
			pb.frinfo.hit[pfi] = 1;
			pb.frinfo.latest_ts[pfi] = latest_ts;
			pb.frinfo.earliest_ts[pfi] = latest_ts;
			pb.frinfo.latest_interval[pfi] = 0;
		}
	}

	if ((rtp.debug_level & 4) == 4)
		for(i = 0; i < PROFILER_PB_FRAMEINFO_TABLE_SIZE; i++) {
			sgpu_profiler_info("EGP: %d: %6u, %3u, %8llu, %8llu, %8llu, 1sidx=%d /%d"
				, i, pb.frinfo.pid[i], pb.frinfo.hit[i]
				, pb.frinfo.earliest_ts[i], pb.frinfo.latest_ts[i], pb.frinfo.latest_interval[i]
				, pb.frinfo.onesecidx, qidx);
		}
}

static void sgpu_profiler_pb_frameinfo_calc_fps(u64 nowtimeus, int rtp_tail)
{
	int dstidx = -1;

	const u64 max_interval = 99999999L;
	const u32 max_fps = 99999;
	u64 sum_interval;
	u32 avg_fps = 0;
	u64 latest_interval;
	u32 latest_fps = 0;
	u64 nowsum_interval;
	u32 expected_afps = 0;
	u64 now_interval;
	u32 expected_fps = 0;

	u32 tft = pb.target_frametime;
	u32 tfps = pb.target_fps;
	u32 fps_drop_pm = 0;
	u32 frame_drop_pm = 0;

	if (pb.user_target_pid > 0)
		dstidx = sgpu_profiler_pb_frameinfo_data_pididx(pb.user_target_pid);

	if (dstidx < 0)
		dstidx = sgpu_profiler_pb_frameinfo_data_getmaxhitidx();

	if (dstidx < 0) {
		if ((rtp.debug_level & 1) == 1)
			sgpu_profiler_info("EGP: FPS Calc: dstidx < 0\n");
		return;
	}

	pb.frinfo.latest_targetpid = pb.frinfo.pid[dstidx];

	sum_interval = pb.frinfo.latest_ts[dstidx] - pb.frinfo.earliest_ts[dstidx];
	latest_interval = pb.frinfo.latest_interval[dstidx];
	nowsum_interval = nowtimeus - pb.frinfo.earliest_ts[dstidx];
	now_interval = nowtimeus - pb.frinfo.latest_ts[dstidx];

	if (nowsum_interval > max_interval)
		nowsum_interval = max_interval;
	if (now_interval > max_interval)
		now_interval = max_interval;

	if (sum_interval > 0)
		avg_fps = 10000000L * (u64)pb.frinfo.hit[dstidx] / sum_interval;
	if (latest_interval > 0)
		latest_fps = (u32)(10000000L / latest_interval);
	if (nowsum_interval > 0)
		expected_afps = (u32)(10000000L * (u64)(pb.frinfo.hit[dstidx] + 1) / nowsum_interval);
	if (now_interval > 0)
		expected_fps = (u32)(10000000L / now_interval);
	if (expected_fps > max_fps)
		expected_fps = max_fps;

	if (tft > 0) {
		if (expected_afps < tfps)
			fps_drop_pm = (tfps - expected_afps) * 1000 / tfps;

		if ((now_interval >> 3) > tft)
			frame_drop_pm = 9999;
		else if (now_interval > tft)
			frame_drop_pm = (now_interval - tft) * 1000 / tft;
	}

	pb.frinfo.expected_swaptime = now_interval;
	pb.frinfo.avg_fps = avg_fps;
	pb.frinfo.exp_afps = expected_fps;
	pb.frinfo.fps_drop_pm = fps_drop_pm;
	pb.frinfo.frame_drop_pm = frame_drop_pm;

	if ((rtp.debug_level & 1) == 1)
		sgpu_profiler_info(
			"EGP: #PID=%2d, Interval(t,l,et,e)=%8llu,%8llu,%8llu,%8llu, fps(t/a/l/ea/e)=%5u,%5u,%5u,%5u,%5u, drop(fps,frame)=%5u,%5u\n"
			, sgpu_profiler_pb_frameinfo_data_getnopids()
			, sum_interval, latest_interval, nowsum_interval, now_interval
			, tfps, avg_fps, latest_fps, expected_afps, expected_fps
			, fps_drop_pm, frame_drop_pm);
}

static void sgpu_profiler_pb_update_boost_margin(void)
{
	int id, cpu_margin_prev, cpu_margin_next;
	u32 target_cl_msk;
	int gputime, gpumgn;

	pb.next_cpu_minlock_boosting_fpsdrop = -1;
	pb.next_cpu_minlock_boosting_framedrop = -1;

	if (pb.frinfo.avg_fps < 50)
		return;
	if (pb.target_clusterid < 0 || pb.target_clusterid >= NUM_OF_CPU_DOMAIN)
		return;

	id = pb.target_clusterid;
	cpu_margin_prev = cpu_margin_next = sgpu_profiler_margin[id];
	target_cl_msk = 1 << id;

	/* CPU minlock by frame */
	if (pb.framedrop_detection_pm_minlock > 0 && pb.frinfo.frame_drop_pm >= pb.framedrop_detection_pm_minlock) {
		if (pb.frinfo.frame_drop_pm > pb.cpu_minlock_margin_max)
			pb.next_cpu_minlock_boosting_framedrop = pb.cpu_minlock_margin_max;
		else
			pb.next_cpu_minlock_boosting_framedrop = pb.frinfo.frame_drop_pm;
	}

	/* CPU minlock by fps */
	if (pb.fpsdrop_detection_pm_minlock > 0 && pb.frinfo.fps_drop_pm >= pb.fpsdrop_detection_pm_minlock) {
		if (pb.frinfo.fps_drop_pm > pb.cpu_minlock_margin_max)
			pb.next_cpu_minlock_boosting_fpsdrop = pb.cpu_minlock_margin_max;
		else
			pb.next_cpu_minlock_boosting_fpsdrop = pb.frinfo.fps_drop_pm;
	}

	/* CPU margin by frame */
	if (pb.framedrop_detection_pm_mgn > 0 && pb.frinfo.frame_drop_pm >= pb.framedrop_detection_pm_mgn) {
		int new_margin = pb.frinfo.frame_drop_pm - pb.framedrop_detection_pm_mgn;

		if (new_margin > cpu_margin_next)
			cpu_margin_next = new_margin;
	}

	/* CPU margin by fps */
	if (pb.fpsdrop_detection_pm_mgn > 0 && pb.frinfo.fps_drop_pm >= pb.fpsdrop_detection_pm_mgn) {
		int new_margin = pb.frinfo.fps_drop_pm;

		if (new_margin > cpu_margin_next)
			cpu_margin_next = new_margin;
	}

	/* CPU Bitmap */
	if (pb.target_clusters_bitmap_fpsdrop_detection_pm > 0 &&
			pb.frinfo.fps_drop_pm >= pb.target_clusters_bitmap_fpsdrop_detection_pm)
		target_cl_msk = pb.target_clusters_bitmap;

	for (id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
		u32 bitmsk = 1 << id;
		int new_margin = -1;

		if ((target_cl_msk & bitmsk) != 0)
			new_margin = cpu_margin_next + pb.cputime_boost_pm;
		else
			new_margin = sgpu_profiler_margin[id];

		if (new_margin > sgpu_profiler_margin[id])
			pb.mgninfo[id].no_mgnboost++;

		new_margin = sgpu_profiler_mscaler(id, profiler_getavgfreq(id), new_margin);

		if (new_margin > pb.cpu_mgn_margin_max)
			new_margin = pb.cpu_mgn_margin_max;

		sgpu_profiler_margin_applied[id] = new_margin;

		if ((rtp.debug_level & 2) == 2)
			sgpu_profiler_info("EGP: CPU margin(id, in, applied) = %d, %5d,%5d\n"
				, id, sgpu_profiler_margin[id], sgpu_profiler_margin_applied[id]);
	}

	gputime = (u32)rtp.last.gputime * (u32)(RATIO_UNIT + pb.gputime_boost_pm) / RATIO_UNIT;
	gpumgn = sgpu_profiler_margin[PROFILER_GPU];
	if (gputime > pb.target_frametime) {
		gpumgn += (gputime - pb.target_frametime) * RATIO_UNIT / pb.target_frametime;

		if (gpumgn > sgpu_profiler_margin[PROFILER_GPU])
			pb.mgninfo[PROFILER_GPU].no_mgnboost++;
	}
	gpumgn = sgpu_profiler_mscaler(PROFILER_GPU, profiler_getavgfreq(PROFILER_GPU), gpumgn);
	sgpu_profiler_margin_applied[PROFILER_GPU] = gpumgn;

	if ((rtp.debug_level & 2) == 2) {
		sgpu_profiler_info("EGP: GPU margin(in, applied) = %5d,%5d\n"
			, sgpu_profiler_margin[PROFILER_GPU], sgpu_profiler_margin_applied[PROFILER_GPU]);
	}
}

long sgpu_profiler_pb_get_gpu_target_freq(unsigned long freq, uint64_t activepct, uint64_t target_norm)
{
	ktime_t nowtimeus = ktime_get_real() / 1000L;
	//int gpu_mgn = sgpu_profiler_margin[PROFILER_GPU];
	int rtp_tail = rtp.tail;
	long fnext = 0;

	mutex_lock(&sgpu_profiler_pmqos_lock);
	sgpu_profiler_reset_next_minlock();

	if (pb.target_frametime > 0) {
		sgpu_profiler_pb_frameinfo_calc_fps(nowtimeus, rtp_tail);
		sgpu_profiler_pb_update_boost_margin();
		sgpu_profiler_pb_update_minlock(nowtimeus, rtp_tail);
	} else {
		for (int i = 0; i <= PROFILER_GPU; i++)
			sgpu_profiler_margin_applied[i] = sgpu_profiler_margin[i];
	}

	sgpu_profiler_set_pmqos_next();
	mutex_unlock(&sgpu_profiler_pmqos_lock);

	for (int i = 0; i <= PROFILER_GPU; i++) {
		int new_margin = sgpu_profiler_margin_applied[i];

		new_margin = sgpu_profiler_mscaler(i, profiler_getavgfreq(i), new_margin);
		pb.mgninfo[i].no_value++;
		pb.mgninfo[i].sum_margin += new_margin;
		if (i < PROFILER_GPU && new_margin != sgpu_profiler_margin_applied[i]) {
			emstune_set_peltboost(0, 0, PROFILER_GET_CPU_BASE_ID(i), new_margin / 10);
			emstune_set_peltboost(0, 1, PROFILER_GET_CPU_BASE_ID(i), new_margin / 10);
		}
	}

	fnext = (long)target_norm + ((long)target_norm * (long)sgpu_profiler_margin_applied[PROFILER_GPU] / 1000);

	if (activepct >= pb.gpu_forced_boost_activepct && fnext < freq) {
		fnext = freq + 10000;
	}
	if (fnext < pb.pmqos_minlock_next[PROFILER_GPU])
		fnext = pb.pmqos_minlock_next[PROFILER_GPU];

	if (fnext < 0)
		fnext = 0;

	for (int i = 0; i <= PROFILER_GPU; i++) {
		if (pb.cur_ndvfs > 0)
			pb.lastframe_avgfreq[i] = pb.cur_sumfreq[i] / pb.cur_ndvfs[i];
	}
	memset(pb.cur_sumfreq, 0, sizeof(pb.cur_sumfreq));
	memset(pb.cur_ndvfs, 0, sizeof(pb.cur_ndvfs));

	return fnext;
}
/* End of Performance Booster */

/* Start of Margin Scaler */
void sgpu_profiler_mscaler_init(void)
{
	memset(&mscaler, 0, sizeof(mscaler));
}

void sgpu_profiler_mscaler_setid(u32 cmd)
{
	u32 settype = (cmd >> 24) & 0xf;
	u32 id = cmd & 0xffff;

	if (id <= PROFILER_GPU) {
		mscaler.id = id;
		if(settype == 1)
			mscaler.type[id] = (cmd >> 16) & 0xff;
	}
}

u32 sgpu_profiler_mscaler_getid(void)
{
	return mscaler.id;
}

void sgpu_profiler_mscaler_set_fs_fz(u32 value)
{
	mscaler.fs[mscaler.id] = ((value >> 16) & 0xffff) * 1000;
	mscaler.fz[mscaler.id] = (value & 0xffff) * 1000;
}

u32 sgpu_profiler_mscaler_get_fs_fz(void)
{
	int id = mscaler.id;

	return ((mscaler.fs[id]/1000) << 16) | ((mscaler.fz[id]/1000) & 0xffff);
}

int sgpu_profiler_mscaler_internal(int id, int freq, int margin)
{
	int type = mscaler.type[id];
	int diff, range, sf;

	if (margin == 0 || mscaler.fs[id] <= mscaler.fz[id])
		return margin;

	if (margin > 0 && (type == MSCALER_TYPE_HALFLINEAR || type == MSCALER_TYPE_HALFSQUARE))
		return margin;

	if (margin > 0 && (type == MSCALER_TYPE_RHALFLINEAR || type == MSCALER_TYPE_RHALFSQUARE)) {
		diff = (mscaler.fs[id] - freq);
	} else {
		diff = (freq - mscaler.fz[id]);
	}
	range = (mscaler.fs[id] - mscaler.fz[id]) / RATIO_UNIT;
	sf = diff / range;

	if (sf <= 0)
		return 0;
	else if (sf >= RATIO_UNIT)
		return margin;

	margin = margin * sf / RATIO_UNIT;

	if (type == MSCALER_TYPE_SQUARE || type == MSCALER_TYPE_HALFSQUARE || type == MSCALER_TYPE_RHALFSQUARE)
		margin += margin;

	return margin;
}

int sgpu_profiler_mscaler(int id, int freq, int margin)
{
	int ret = margin;

	if (id > PROFILER_GPU || mscaler.type[id] == MSCALER_TYPE_BYPASS)
		return margin;
	if (margin == 0 || mscaler.fz[id] == mscaler.fs[id])
		return margin;

	ret = sgpu_profiler_mscaler_internal(id, freq, margin);
	if ((rtp.debug_level & 0x10) == 0x10)
		sgpu_profiler_info("EGP:PB:mscaler:%d:%d f(c,s,z)=%6u,%6u,%6u, mgn(i,o)=%4d,%4d\n"
				, id, mscaler.type[id], freq/1000, mscaler.fs[id]/1000, mscaler.fz[id]/1000, margin, ret);
	return ret;
}

/* End of Margin Scaler */

/* export function */

void sgpu_profiler_update_interframe_sw(u32 dw_size, void *chunk_data)
{
	struct interframe_data *dst;
	struct chunk_gfxinfo *gfxinfo = chunk_data;
	u32 chunk_size = dw_size * sizeof(u32);
	int list_tail_pos = 0;
	int curidx = rtp.tail;
	ktime_t start = 0;
	ktime_t end = 0;
	ktime_t total = 0;
	ktime_t swaptime = 0;
	ktime_t v2ftime = 0;
	ktime_t pretime = 0;
	ktime_t cputime = 0;
	ktime_t gputime = 0;

	/* Triggering */
	if(!profiler_gpudev_fn->profiler_sched)
		return;

	if (chunk_size >= PROFILER_GFXINFO_LEGACY_SIZE) {
		start = gfxinfo->start;
		end = gfxinfo->end;
		total = gfxinfo->total;
		swaptime = end - rtp.prev_swaptimestamp;
		v2ftime = 0;
		pretime = (rtp.prev_swaptimestamp < start) ? start - rtp.prev_swaptimestamp : 0;
		cputime = end - start;
		gputime = (ktime_t)atomic_read(&rtp.last.hw_totaltime);
	}

	atomic_set(&rtp.flag, 1);
	profiler_gpudev_fn->profiler_sched();
	atomic_set(&rtp.last.hw_read, 1);

	rtp.prev_swaptimestamp = end;
	atomic_inc(&rtp.vsync.swapcall_counter);
	pinfo.frame_counter++;

	dst = &interframe[curidx];
	rtp.tail = (rtp.tail + 1) % PROFILER_TABLE_MAX;
	if (rtp.tail == rtp.head)
		rtp.head = (rtp.head + 1) % PROFILER_TABLE_MAX;
	if (rtp.tail == rtp.lastshowidx)
		rtp.lastshowidx = (rtp.lastshowidx + 1) % PROFILER_TABLE_MAX;

	dst->interframe_time[GPU_SW].vsync = (rtp.vsync.lastsw == 0) ? rtp.vsync.prev : rtp.vsync.lastsw;
	rtp.vsync.lastsw = 0;
	dst->interframe_time[GPU_SW].start = start;
	dst->interframe_time[GPU_SW].end = end;
	dst->interframe_time[GPU_SW].total = total;

	dst->interframe_time[GPU_HW].vsync = (rtp.vsync.lasthw == 0) ? rtp.vsync.prev : rtp.vsync.lasthw;
	dst->interframe_time[GPU_HW].start = rtp.last.hw_starttime;
	dst->interframe_time[GPU_HW].end = rtp.last.hw_endtime;
	dst->interframe_time[GPU_HW].total = gputime;
	v2ftime = (dst->interframe_time[GPU_HW].vsync < dst->interframe_time[GPU_HW].end) ?
		dst->interframe_time[GPU_HW].end - dst->interframe_time[GPU_HW].vsync : dst->interframe_time[GPU_HW].end - rtp.vsync.prev;

	if (rtp.nrq > 128 || rtp.readout != 0) {
		rtp.nrq = 0;
		rtp.rtime[PRESUM] = 0;
		rtp.rtime[CPUSUM] = 0;
		rtp.rtime[V2SSUM] = 0;
		rtp.rtime[GPUSUM] = 0;
		rtp.rtime[V2FSUM] = 0;

		rtp.rtime[PREMAX] = 0;
		rtp.rtime[CPUMAX] = 0;
		rtp.rtime[V2SMAX] = 0;
		rtp.rtime[GPUMAX] = 0;
		rtp.rtime[V2FMAX] = 0;

		memset(rtp.gfxinfo, 0, sizeof(rtp.gfxinfo));
	}
	rtp.readout = 0;
	rtp.nrq++;

	rtp.rtime[PRESUM] += pretime;
	rtp.rtime[CPUSUM] += cputime;
	rtp.rtime[V2SSUM] += swaptime;
	rtp.rtime[GPUSUM] += gputime;
	rtp.rtime[V2FSUM] += v2ftime;

	if (rtp.rtime[PREMAX] < pretime)
		rtp.rtime[PREMAX] = pretime;
	if (rtp.rtime[CPUMAX] < cputime)
		rtp.rtime[CPUMAX] = cputime;
	if (rtp.rtime[V2SMAX] < swaptime)
		rtp.rtime[V2SMAX] = swaptime;
	if (rtp.rtime[GPUMAX] < gputime)
		rtp.rtime[GPUMAX] = gputime;
	if (rtp.rtime[V2FMAX] < v2ftime)
		rtp.rtime[V2FMAX] = v2ftime;

	dst->nrq = rtp.nrq;
	dst->rtime[PRESUM] = rtp.rtime[PRESUM];
	dst->rtime[CPUSUM] = rtp.rtime[CPUSUM];
	dst->rtime[V2SSUM] = rtp.rtime[V2SSUM];
	dst->rtime[GPUSUM] = rtp.rtime[GPUSUM];
	dst->rtime[V2FSUM] = rtp.rtime[V2FSUM];

	dst->rtime[PREMAX] = rtp.rtime[PREMAX];
	dst->rtime[CPUMAX] = rtp.rtime[CPUMAX];
	dst->rtime[V2SMAX] = rtp.rtime[V2SMAX];
	dst->rtime[GPUMAX] = rtp.rtime[GPUMAX];
	dst->rtime[V2FMAX] = rtp.rtime[V2FMAX];
	dst->cputime = cputime;
	dst->gputime = gputime;

	dst->vsync_interval = rtp.vsync.interval;

	dst->timestamp = end;
	dst->pid = (u32)current->pid;
	dst->tgid = (u32)current->tgid;
	dst->coreid = (u32)get_cpu();
	put_cpu(); /* for enabling preemption */
	memcpy(dst->name, current->comm, sizeof(dst->name));
	memcpy(dst->renderer_name, current->comm, sizeof(dst->renderer_name));

	if (chunk_size >= sizeof(struct chunk_gfxinfo)) {
		int i;

		for (i = 0; i < NUM_OF_GFXINFO; i++) {
			int gfxinfo_idx = gfxinfo->ids[i];

			if (gfxinfo_idx < 0)
				continue;

			if (gfxinfo_idx < NUM_OF_GFXINFO) {
				rtp.gfxinfo[gfxinfo_idx] += gfxinfo->values[i];
				dst->gfxinfo[gfxinfo_idx] = rtp.gfxinfo[gfxinfo_idx];
			}
		}
	}

	pb.target_clusterid_prev = pb.target_clusterid;
	if (pb.frinfo.latest_targetpid > 0) {
		if (dst->tgid == pb.frinfo.latest_targetpid) {
			pb.target_clusterid = PROFILER_GET_CL_ID(dst->coreid);
			rtp.last.cputime = cputime;
			rtp.last.swaptime = swaptime;
			rtp.last.gputime = gputime;
			rtp.last.cpufreqlv = pb.cur_freqlv[pb.target_clusterid];
			rtp.last.gpufreqlv = pb.cur_freqlv[PROFILER_GPU];
		}
	} else {
		pb.target_clusterid = PROFILER_GET_CL_ID(dst->coreid);
		rtp.last.cputime = cputime;
		rtp.last.swaptime = swaptime;
		rtp.last.gputime = gputime;
		rtp.last.cpufreqlv = pb.cur_freqlv[pb.target_clusterid];
		rtp.last.gpufreqlv = pb.cur_freqlv[PROFILER_GPU];
	}

	if (atomic_read(&pid.list_readout) != 0)
		sgpu_profiler_pid_init();

	list_tail_pos = atomic_read(&pid.list_top);
	if (list_tail_pos < NUM_OF_PID) {
		/* coreid '0' means not recorded */
		pid.core_list[list_tail_pos] = dst->coreid + 1;
		/* tgid '0' => filtering of System UI */
		pid.list[list_tail_pos] = (dst->tgid == 0) ? 1 : dst->tgid;
		atomic_inc(&pid.list_top);
	}

	sgpu_profiler_pb_frameinfo_profile(curidx);
}
EXPORT_SYMBOL(sgpu_profiler_update_interframe_sw);

void sgpu_profiler_update_interframe_hw(ktime_t start, ktime_t end, bool end_of_frame)
{
	int flag = atomic_read(&rtp.flag);
	if (flag == 0)
		return;

	if (end_of_frame) {
		ktime_t diff = ktime_get_real() - ktime_get();
		ktime_t term = 0;
		int time = atomic_read(&rtp.last.hw_totaltime);
		int read = atomic_read(&rtp.last.hw_read);

		atomic_set(&rtp.last.hw_read, 0);

		rtp.last.hw_starttime = (diff + rtp.cur.hw_starttime)/1000;
		rtp.last.hw_endtime = (diff + end)/1000;
		if (start < rtp.cur.hw_endtime)
			term = end - rtp.cur.hw_endtime;
		else
			term = end - start;
		rtp.cur.hw_totaltime += term;
		if (read == 1 || time < (int)(rtp.cur.hw_totaltime / 1000)) {
			time = (int)(rtp.cur.hw_totaltime / 1000);
			atomic_set(&rtp.last.hw_totaltime, time);
		}

		rtp.cur.hw_starttime = 0;
		rtp.cur.hw_endtime = end;
		rtp.cur.hw_totaltime = 0;

		rtp.vsync.lasthw = (rtp.vsync.curhw == 0) ? rtp.vsync.prev : rtp.vsync.curhw;
		rtp.vsync.curhw = 0;
	} else {
		ktime_t term = 0;

		if (rtp.cur.hw_starttime == 0)
			rtp.cur.hw_starttime = start;
		if (start < rtp.cur.hw_endtime)
			term = end - rtp.cur.hw_endtime;
		else
			term = end - start;
		rtp.cur.hw_totaltime += term;
		rtp.cur.hw_endtime = end;
	}
}
EXPORT_SYMBOL(sgpu_profiler_update_interframe_hw);

void sgpu_profiler_register_sgpupro_fn(struct exynos_profiler_gpudev_fn *fn)
{
	profiler_gpudev_fn = fn;
	profiler_gpudev_fn->set_profiler_governor = sgpu_profiler_set_profiler_governor;
	profiler_gpudev_fn->set_freq_margin = sgpu_profiler_set_freq_margin;
	profiler_gpudev_fn->set_targetframetime = sgpu_profiler_set_targetframetime;
	profiler_gpudev_fn->set_vsync = sgpu_profiler_set_vsync;
	profiler_gpudev_fn->get_frameinfo = sgpu_profiler_get_frameinfo;
	profiler_gpudev_fn->get_gfxinfo = sgpu_profiler_get_gfxinfo;
	profiler_gpudev_fn->get_pidinfo = sgpu_profiler_get_pidinfo;
	profiler_gpudev_fn->pb_get_params = sgpu_profiler_pb_get_params;
	profiler_gpudev_fn->pb_set_params = sgpu_profiler_pb_set_params;
	profiler_gpudev_fn->pb_set_dbglv = sgpu_profiler_pb_set_dbglv;
	profiler_gpudev_fn->pb_set_freqtable = sgpu_profiler_pb_set_freqtable;
	profiler_gpudev_fn->pb_set_freqtable_en = sgpu_profiler_pb_set_freqtable_en;
	profiler_gpudev_fn->pb_get_freqtable_en = sgpu_profiler_pb_get_freqtable_en;
	profiler_gpudev_fn->pb_set_cur_freqlv = sgpu_profiler_pb_set_cur_freqlv;
	profiler_gpudev_fn->pb_set_cur_freq = sgpu_profiler_pb_set_cur_freq;
	profiler_gpudev_fn->pb_get_mgninfo = sgpu_profiler_pb_get_mgninfo;
	profiler_gpudev_fn->pb_get_gpu_target_freq = sgpu_profiler_pb_get_gpu_target_freq;
}
EXPORT_SYMBOL(sgpu_profiler_register_sgpupro_fn);

struct exynos_profiler_gpudev_fn* sgpu_profiler_get_gpudev_fn(void)
{
	if (profiler_gpudev_fn) {
		return profiler_gpudev_fn;
	}
	return NULL;
}
EXPORT_SYMBOL(sgpu_profiler_get_gpudev_fn);

ssize_t sgpu_profiler_show_status(char *buf)
{
	struct interframe_data *dst = NULL;
	ssize_t count = 0;
	int id = 0;
	int target_frametime = sgpu_profiler_get_target_frametime();

	while ((dst = sgpu_profiler_get_next_frameinfo()) != NULL){
		if (dst->nrq > 0) {
			ktime_t avg_pre = dst->rtime[PRESUM] / dst->nrq;
			ktime_t avg_cpu = dst->rtime[CPUSUM] / dst->nrq;
			ktime_t avg_swap = dst->rtime[V2SSUM] / dst->nrq;
			ktime_t avg_gpu = dst->rtime[GPUSUM] / dst->nrq;
			ktime_t avg_v2f = dst->rtime[V2FSUM] / dst->nrq;

			count += scnprintf(buf + count, PAGE_SIZE - count,"%4d, %6llu, %3u, %6llu, %6llu, %6llu, %6llu, %6llu"
				, id++
				, dst->vsync_interval
				, dst->nrq
				, avg_pre
				, avg_cpu
				, avg_swap
				, avg_gpu
				, avg_v2f);
			count += scnprintf(buf + count, PAGE_SIZE - count,",|, %6d, %6llu, %6llu"
				, target_frametime
				, dst->cputime
				, dst->gputime);
		}
	}

	return count;
}

static void sgpu_profiler_pid_init(void)
{
	atomic_set(&pid.list_readout, 0);
	atomic_set(&pid.list_top, 0);
	memset(&(pid.list[0]), 0, sizeof(pid.list[0]) * NUM_OF_PID);
	memset(&(pid.core_list[0]), 0, sizeof(pid.core_list[0]) * NUM_OF_PID);
}

static void sgpu_profiler_rtp_init(void)
{
	memset(&rtp, 0, sizeof(struct rtp_data));

	/* GPU Profiler Governor */
	mutex_init(&sgpu_profiler_pmqos_lock);
	sgpu_profiler_polling_interval = PROFILER_POLLING_INTERVAL_MS;

	/* default PB input params */
	sgpu_profiler_pb_fps_calc_average_us = PROFILER_PB_FPS_CALC_AVERAGE_US;
	rtp.debug_level = 0;
	pb.target_clusterid = sgpu_profiler_get_current_cpuid();
	pb.target_clusterid_prev = pb.target_clusterid;
	pb.target_frametime = PROFILER_RENDERTIME_DEFAULT_FRAMETIME;
	pb.gpu_forced_boost_activepct = 98;
	pb.user_target_pid = 0;

	pb.cpu_mgn_margin_max = 400;
	pb.framedrop_detection_pm_mgn = 100;
	pb.fpsdrop_detection_pm_mgn = 50; /* 57fps/60fps, 114fps/120fps */
	pb.target_clusters_bitmap = 0xff - 1;
	pb.target_clusters_bitmap_fpsdrop_detection_pm = 100;

	pb.cpu_minlock_margin_max = 4000;
	pb.framedrop_detection_pm_minlock = 200; /* expected rendertime / latest rendertime */
	pb.fpsdrop_detection_pm_minlock = 100; /* 54fps/60fps, 108fps/120fps */
}
/* need to be initialized by external modules*/
int sgpu_profiler_init(struct exynos_profiler_gpudev_fn *fn)
{
	int ret = 0;
	pinfo.cpufreq_pm_qos_added = 0;
	exynos_pm_qos_add_request(&sgpu_profiler_gpu_min_qos,
				  PM_QOS_GPU_THROUGHPUT_MIN, 0);
	sgpu_profiler_rtp_init();
	sgpu_profiler_pid_init();
	sgpu_profiler_register_sgpupro_fn(fn);
	sgpu_profiler_governor_init(fn);
	return ret;
}
EXPORT_SYMBOL(sgpu_profiler_init);

void sgpu_profiler_deinit(void)
{
	exynos_pm_qos_remove_request(&sgpu_profiler_gpu_min_qos);
}
MODULE_LICENSE("GPL");
