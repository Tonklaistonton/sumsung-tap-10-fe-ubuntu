#include <exynos-gpu-profiler.h>
#include <sgpu-profiler.h>

static struct exynos_profiler_gpudev_fn *profiler_gpudev_fn;

/************************************************************************
 *				SUPPORT-PROFILER				*
 ************************************************************************/
u32 gpupro_get_table_cnt(s32 id)
{
	return profiler.gpu.table_cnt;
}

u32 gpupro_get_freq_table(s32 id, u32 *table)
{
	int idx;
	for (idx = 0; idx < profiler.gpu.table_cnt; idx++)
		table[idx] = profiler.gpu.table[idx].freq;

	return idx;
}

u32 gpupro_get_max_freq(s32 id)
{
	return profiler.gpu.table[profiler.gpu.max_freq_idx].freq;
}

u32 gpupro_get_min_freq(s32 id)
{
	return profiler.gpu.table[profiler.gpu.min_freq_idx].freq;
}

u32 gpupro_get_freq(s32 id)
{
	return profiler.gpu.result[PROFILER].fc_result.freq[CS_ACTIVE];
}

void gpupro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	*dyn_power = profiler.gpu.result[PROFILER].fc_result.dyn_power;
	*st_power = profiler.gpu.result[PROFILER].fc_result.st_power;
}

void gpupro_get_power_change(s32 id, s32 freq_delta_ratio,
			u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct gpu_profile_result *result = &profiler.gpu.result[PROFILER];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int flag = (STATE_SCALE_WO_SPARE | STATE_SCALE_TIME | STATE_SCALE_WITH_ORG_CAP);

	get_power_change(profiler.gpu.table, profiler.gpu.table_cnt,
		profiler.gpu.cur_freq_idx, profiler.gpu.min_freq_idx, profiler.gpu.max_freq_idx,
		fc_result->time[CS_ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);
}

u32 gpupro_get_active_pct(s32 id)
{
	return profiler.gpu.result[PROFILER].fc_result.ratio[CS_ACTIVE];
}

s32 gpupro_get_temp(s32 id)
{
	return profiler.gpu.result[PROFILER].avg_temp;
}

void gpupro_set_margin(s32 id, s32 margin)
{
	if(profiler_gpudev_fn) {
		profiler_gpudev_fn->set_freq_margin(id, margin);
	}
}

static void gpupro_reset_profiler(int user)
{
	struct freq_cstate *fc = &profiler.gpu.fc;
	struct freq_cstate_snapshot *fc_snap = &profiler.gpu.fc_snap[user];
	if(profiler_gpudev_fn) {
		profiler.gpu.fc.time[CS_ACTIVE] = profiler_gpudev_fn->get_time_in_state();
		sync_fcsnap_with_cur(fc, fc_snap, profiler.gpu.table_cnt);
		fc_snap->last_snap_time = profiler_gpudev_fn->get_tis_last_update();
	}
}

void gpupro_disable_llc_way(bool disable)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->disable_llc_way(disable);
	}
}

void gpupro_set_profiler_governor(int mode)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->set_profiler_governor(mode);
	}
}

void gpupro_set_targetframetime(int us)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->set_targetframetime(us);
	}
}

void gpupro_set_vsync(ktime_t ktime_us)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->set_vsync(ktime_us);
	}
}

void gpupro_get_frameinfo(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->get_frameinfo(nrframe, nrvsync, delta_ms);
	}
}

void gpupro_get_gfxinfo(u64 *times, u64 *gfxinfo)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->get_gfxinfo(times, gfxinfo);
	}
}

void gpupro_get_pidinfo(u32 *list, u8 *core_list, u8 *renderer_name)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->get_pidinfo(list, core_list, renderer_name);
	}
}

s32 gpupro_pb_get_mgninfo(int id, u16 *no_mgnboost, u16 *no_mlckboost)
{
	if (profiler_gpudev_fn) {
		return profiler_gpudev_fn->pb_get_mgninfo(id, no_mgnboost, no_mlckboost);
	}
	return 0;
}

void gpupro_set_pb_params(int idx, int value)
{
	if (profiler_gpudev_fn) {
		profiler_gpudev_fn->pb_set_params(idx, value);
	}
}

int gpupro_get_pb_params(int idx)
{
	if (profiler_gpudev_fn) {
		return profiler_gpudev_fn->pb_get_params(idx);
	}
	return 0;
}

static u32 gpupro_update_profile(int user);
u32 gpupro_update_mode(s32 id, int mode)
{
	if (profiler.gpu.enabled != mode) {
		/* reset profiler struct at start time */
		if (mode)
			gpupro_reset_profiler(PROFILER);

		profiler.gpu.enabled = mode;

		return 0;
	}
	gpupro_update_profile(PROFILER);

	return 0;
}
/************************************************************************
 *			        Register function for GPU Profiler	    			*
 ************************************************************************/
struct profiler_fn_gpu gpu_fn_profiler = {
	.disable_llc_way		= &gpupro_disable_llc_way,
	.get_frameinfo			= &gpupro_get_frameinfo,
	.get_gfxinfo			= &gpupro_get_gfxinfo,
	.get_pidinfo			= &gpupro_get_pidinfo,
	.set_vsync				= &gpupro_set_vsync,
	.set_profiler_governor	= &gpupro_set_profiler_governor,
	.pb_get_mgninfo			= &gpupro_pb_get_mgninfo,
	.set_targetframetime	= &gpupro_set_targetframetime,
	.set_pb_params			= &gpupro_set_pb_params,
	.get_pb_params			= &gpupro_get_pb_params,
};

struct domain_fn gpu_fn_domain = {
	.get_table_cnt			= &gpupro_get_table_cnt,
	.get_freq_table			= &gpupro_get_freq_table,
	.get_max_freq			= &gpupro_get_max_freq,
	.get_min_freq			= &gpupro_get_min_freq,
	.get_freq				= &gpupro_get_freq,
	.get_power				= &gpupro_get_power,
	.get_power_change		= &gpupro_get_power_change,
	.get_active_pct			= &gpupro_get_active_pct,
	.get_temp				= &gpupro_get_temp,
	.set_margin				= &gpupro_set_margin,
	.update_mode			= &gpupro_update_mode,
};

/************************************************************************
 *			Gathering GPU Freq Information			*
 ************************************************************************/
static u32 gpupro_update_profile(int user)
{
	struct gpu_profile_result *result = &profiler.gpu.result[user];
	struct freq_cstate *fc = &profiler.gpu.fc;
	struct freq_cstate_snapshot *fc_snap = &profiler.gpu.fc_snap[user];
	struct freq_cstate_result *fc_result = &result->fc_result;
	ktime_t tis_last_update;
	ktime_t last_snap_time;

	/* Common Data */
	if (profiler.gpu.tz) {
		int temp = exynos_profiler_get_temp(profiler.gpu.tz);
		profiler.gpu.result[user].avg_temp = (temp + profiler.gpu.result[user].cur_temp) >> 1;
		profiler.gpu.result[user].cur_temp = temp;
	}
	if(profiler_gpudev_fn) {
		/* In some cases, freq is set to 0 for power off expression
		 * In this case, mark it as min Freq, not bottom freq */
		if (profiler_gpudev_fn->get_cur_clock()) {
			profiler.gpu.cur_freq_idx = get_idx_from_freq(profiler.gpu.table,
				profiler.gpu.table_cnt, profiler_gpudev_fn->get_cur_clock(), RELATION_LOW);
		} else {
			profiler.gpu.cur_freq_idx = get_idx_from_freq(profiler.gpu.table,
				profiler.gpu.table_cnt, profiler_gpudev_fn->get_min_freq(), RELATION_HIGH);
		}

		profiler.gpu.max_freq_idx = get_idx_from_freq(profiler.gpu.table,
			profiler.gpu.table_cnt, profiler_gpudev_fn->get_max_freq(), RELATION_LOW);
		profiler.gpu.min_freq_idx = get_idx_from_freq(profiler.gpu.table,
			profiler.gpu.table_cnt, profiler_gpudev_fn->get_min_freq(), RELATION_HIGH);

		profiler.gpu.fc.time[CS_ACTIVE] = profiler_gpudev_fn->get_time_in_state();

		tis_last_update = profiler_gpudev_fn->get_tis_last_update();
		last_snap_time = fc_snap->last_snap_time;
		make_snapshot_and_time_delta(fc, fc_snap, fc_result, profiler.gpu.table_cnt);
		fc_result->profile_time = tis_last_update - last_snap_time;
		fc_snap->last_snap_time = tis_last_update;
		compute_freq_cstate_result(profiler.gpu.table, fc_result, profiler.gpu.table_cnt,
			profiler.gpu.cur_freq_idx, profiler.gpu.result[user].avg_temp);
	}
	return 0;
}

/************************************************************************
 *						INITIALIZATON									*
 ************************************************************************/
static int get_freq_table_info(u32 *freq_table, u32 *table_max_freq, u32 *table_min_freq, u32 *cur_freq)
{
	if (profiler_gpudev_fn && freq_table) {
		profiler.gpu.table_cnt = profiler_gpudev_fn->get_step();
		*table_max_freq = freq_table[0];
		*table_min_freq = freq_table[profiler.gpu.table_cnt - 1];
		*cur_freq = profiler_gpudev_fn->get_cur_clock();
		profiler.gpu.fc.time[CS_ACTIVE] = profiler_gpudev_fn->get_time_in_state();
	}
	return 0;
}

static int parse_dt(struct device_node *dn)
{
	int ret;

	/* necessary data */
	ret = of_property_read_u32(dn, "cal-id", &profiler.gpu.cal_id);
	if (ret)
		return -2;

	/* un-necessary data */
	ret = of_property_read_s32(dn, "profiler-id", &profiler.gpu.profiler_id);
	if (ret)
		profiler.gpu.profiler_id = -1;	/* Don't support profiler */

	of_property_read_u32(dn, "power-coefficient", &profiler.gpu.dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &profiler.gpu.st_pwr_coeff);
	of_property_read_string(dn, "tz-name", &profiler.gpu.tz_name);

	return 0;
}

static int init_profile_result(struct gpu_profile_result *result, int size)
{
	if (init_freq_cstate_result(&result->fc_result, size))
		return -ENOMEM;
	return 0;
}

static void show_profiler_info(void)
{
	int idx;

	pr_info("================ gpu domain ================\n");
	if (profiler.gpu.table) {
		pr_info("min= %dKHz, max= %dKHz\n",
				profiler.gpu.table[profiler.gpu.table_cnt - 1].freq, profiler.gpu.table[0].freq);
		for (idx = 0; idx < profiler.gpu.table_cnt; idx++)
			pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%lld st_cost=%lld\n",
				idx, profiler.gpu.table[idx].freq, profiler.gpu.table[idx].volt,
				profiler.gpu.table[idx].dyn_cost,
				profiler.gpu.table[idx].st_cost);
	}
	if (profiler.gpu.tz_name)
		pr_info("support temperature (tz_name=%s)\n", profiler.gpu.tz_name);
	if (profiler.gpu.profiler_id != -1)
		pr_info("support profiler domain(id=%d)\n", profiler.gpu.profiler_id);
}

static int exynos_gpu_profiler_probe(struct platform_device *pdev)
{
	unsigned int dev_max_freq, dev_min_freq, dev_cur_freq;
	int ret, idx;
	int id = 0;
	int *freq_table_gpu = NULL;
	int freq_table_cpu[MAXNUM_OF_DVFS];
	struct domain_data *cpudom = NULL;

	/* get node of device tree */
	profiler_gpudev_fn = sgpu_profiler_get_gpudev_fn();
	if (!pdev->dev.of_node) {
		pr_err("gpupro: failed to get device treee\n");
		return -EINVAL;
	}
	profiler.dn = pdev->dev.of_node;

	/* Parse data from Device Tree to init domain */
	ret = parse_dt(profiler.dn);
	if (ret) {
		pr_err("gpupro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	/* init freq table */
	if(profiler_gpudev_fn) {
		freq_table_gpu = profiler_gpudev_fn->get_freq_table();
	}
	get_freq_table_info(freq_table_gpu, &dev_max_freq, &dev_min_freq, &dev_cur_freq);
	profiler.gpu.table = init_fvtable(
			freq_table_gpu, profiler.gpu.table_cnt,
			profiler.gpu.cal_id, dev_max_freq, dev_min_freq,
			profiler.gpu.dyn_pwr_coeff, profiler.gpu.st_pwr_coeff,
			PWR_COST_CFVV, PWR_COST_CFVV);
	if (!profiler.gpu.table) {
		pr_err("gpupro: failed to init freq_table\n");
		return -EINVAL;
	}

	if(profiler_gpudev_fn) {
		ret = profiler_gpudev_fn->pb_set_freqtable(profiler.gpu.profiler_id, profiler.gpu.table_cnt, freq_table_gpu);
		if (ret) {
			pr_err("gpupro: failed to set power table for gpu profiler(ret: %d)\n", ret);
			return -ENOMEM;
		}
		for(id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
			cpudom = exynos_profiler_get_domain(id);
			if (!cpudom->fn) {
				pr_warn("gpupro: cpu%d failed to get profiler function", id);
				continue;
			}
			cpudom->fn->get_freq_table(id,  (u32*)freq_table_cpu);
			ret = profiler_gpudev_fn->pb_set_freqtable(id, cpudom->fn->get_table_cnt(id), freq_table_cpu);
			if (ret) {
				pr_err("gpupro: failed to set power table for cpu profiler(ret: %d)\n", ret);
				return -ENOMEM;
			}
		}
	}

	profiler.gpu.max_freq_idx = 0;
	profiler.gpu.min_freq_idx = profiler.gpu.table_cnt - 1;
	profiler.gpu.cur_freq_idx = get_idx_from_freq(profiler.gpu.table,
				profiler.gpu.table_cnt, dev_cur_freq, RELATION_HIGH);

	if (init_freq_cstate(&profiler.gpu.fc, profiler.gpu.table_cnt))
			return -ENOMEM;

	/* init snapshot & result table */
	for (idx = 0; idx < NUM_OF_USER; idx++) {
		if (init_freq_cstate_snapshot(&profiler.gpu.fc_snap[idx],
						profiler.gpu.table_cnt))
			return -ENOMEM;

		if (init_profile_result(&profiler.gpu.result[idx], profiler.gpu.table_cnt))
			return -EINVAL;
	}

	/* get thermal-zone to get temperature */
	if (profiler.gpu.tz_name)
		profiler.gpu.tz = exynos_profiler_init_temp(profiler.gpu.tz_name);

	if (profiler.gpu.tz)
		init_static_cost(profiler.gpu.table, profiler.gpu.table_cnt,
				1, profiler.dn, profiler.gpu.tz);

	ret = exynos_profiler_register_domain(PROFILER_GPU, &gpu_fn_domain, &gpu_fn_profiler);
	show_profiler_info();
	return ret;
}

static const struct of_device_id exynos_gpu_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-gpu-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_gpu_profiler_match);

static struct platform_driver exynos_gpu_profiler_driver = {
	.probe		= exynos_gpu_profiler_probe,
	.driver	= {
		.name	= "exynos-gpu-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_gpu_profiler_match,
	},
};

static int exynos_gpu_profiler_init(void)
{
	return platform_driver_register(&exynos_gpu_profiler_driver);
}
late_initcall(exynos_gpu_profiler_init);

MODULE_DESCRIPTION("Exynos GPU Profiler v2");
MODULE_SOFTDEP("pre: exynos_thermal_v2 exynos-cpufreq sgpu exynos_devfreq");
MODULE_LICENSE("GPL");
