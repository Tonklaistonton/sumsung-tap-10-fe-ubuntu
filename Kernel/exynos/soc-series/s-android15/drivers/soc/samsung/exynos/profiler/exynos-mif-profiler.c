#include <exynos-mif-profiler.h>
#include <linux/of.h>
#include <linux/of_platform.h>

/************************************************************************
 *				HELPER					*
 ************************************************************************/

static inline void calc_delta(u64 *result_table, u64 *prev_table, u64 *cur_table, int size)
{
	int i;
	u64 delta, cur;

	for (i = 0; i < size; i++) {
		cur = cur_table[i];
		delta = cur - prev_table[i];
		result_table[i] = delta;
		prev_table[i] = cur;
	}
}

/************************************************************************
 *				SUPPORT-PROFILER				*
 ************************************************************************/
u32 mifpro_get_table_cnt(s32 id)
{
	return profiler.mif.table_cnt;
}

u32 mifpro_get_freq_table(s32 id, u32 *table)
{
	int idx;
	for (idx = 0; idx < profiler.mif.table_cnt; idx++)
		table[idx] = profiler.mif.table[idx].freq;

	return idx;
}

u32 mifpro_get_max_freq(s32 id)
{
	return exynos_pm_qos_request(profiler.mif.freq_infos.pm_qos_class_max);
}

u32 mifpro_get_min_freq(s32 id)
{
	return exynos_pm_qos_request(profiler.mif.freq_infos.pm_qos_class);
}

u32 mifpro_get_freq(s32 id)
{
	return profiler.mif.result[PROFILER].fc_result.freq[CS_ACTIVE];
}

void mifpro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	*dyn_power = profiler.mif.result[PROFILER].fc_result.dyn_power;
	*st_power = profiler.mif.result[PROFILER].fc_result.st_power;
}

void mifpro_get_power_change(s32 id, s32 freq_delta_ratio,
			u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct mif_profile_result *result = &profiler.mif.result[PROFILER];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int flag = (STATE_SCALE_WO_SPARE | STATE_SCALE_CNT);
	u64 dyn_power_backup;

	get_power_change(profiler.mif.table, profiler.mif.table_cnt,
		profiler.mif.cur_freq_idx, profiler.mif.min_freq_idx, profiler.mif.max_freq_idx,
		result->tdata_in_state, fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);

	dyn_power_backup = *dyn_power;

	get_power_change(profiler.mif.table, profiler.mif.table_cnt,
		profiler.mif.cur_freq_idx, profiler.mif.min_freq_idx, profiler.mif.max_freq_idx,
		fc_result->time[CS_ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);

	*dyn_power = dyn_power_backup;
}

u32 mifpro_get_active_pct(s32 id)
{
	return profiler.mif.result[PROFILER].fc_result.ratio[CS_ACTIVE];
}

s32 mifpro_get_temp(s32 id)
{
	return profiler.mif.result[PROFILER].avg_temp;
}
void mifpro_set_margin(s32 id, s32 margin)
{
	return;
}

u32 mifpro_update_profile(int user);
u32 mifpro_update_mode(s32 id, int mode)
{
	int i;

	if (profiler.mif.enabled == 0 && mode == 1) {
		struct freq_cstate *fc = &profiler.mif.fc;
		struct freq_cstate_snapshot *fc_snap = &profiler.mif.fc_snap[PROFILER];

		sync_fcsnap_with_cur(fc, fc_snap, profiler.mif.table_cnt);

		profiler.mif.enabled = mode;
	}
	else if (profiler.mif.enabled == 1 && mode == 0) {
		profiler.mif.enabled = mode;

		// clear
		for (i = 0; i < NUM_OF_CSTATE; i++) {
			memset(profiler.mif.result[PROFILER].fc_result.time[i], 0, sizeof(ktime_t) * profiler.mif.table_cnt);
			profiler.mif.result[PROFILER].fc_result.ratio[i] = 0;
			profiler.mif.result[PROFILER].fc_result.freq[i] = 0;
			memset(profiler.mif.fc.time[i], 0, sizeof(ktime_t) * profiler.mif.table_cnt);
			memset(profiler.mif.fc_snap[PROFILER].time[i], 0, sizeof(ktime_t) * profiler.mif.table_cnt);
		}
		profiler.mif.result[PROFILER].fc_result.dyn_power = 0;
		profiler.mif.result[PROFILER].fc_result.st_power = 0;
		profiler.mif.result[PROFILER].fc_result.profile_time = 0;
		profiler.mif.fc_snap[PROFILER].last_snap_time = 0;
		memset(&profiler.mif.result[PROFILER].freq_stats, 0, sizeof(struct mif_freq_state) * NR_MASTERS);
		memset(&profiler.mif.prev_wow_profile, 0, sizeof(struct exynos_wow_profile));
		memset(profiler.mif.prev_tdata_in_state, 0, sizeof(u64) * profiler.mif.table_cnt);

		return 0;
	}

	mifpro_update_profile(PROFILER);

	return 0;
}

u64 mifpro_get_freq_stats0_sum(void) { return profiler.mif.result[PROFILER].freq_stats[MIF].sum; };
u64 mifpro_get_freq_stats0_avg(void) { return profiler.mif.result[PROFILER].freq_stats[MIF].avg; };
u64 mifpro_get_freq_stats0_ratio(void) { return profiler.mif.result[PROFILER].freq_stats[MIF].ratio; };
u64 mifpro_get_freq_stats1_sum(void) { return profiler.mif.result[PROFILER].freq_stats[CPU].sum; };
u64 mifpro_get_freq_stats1_ratio(void) { return profiler.mif.result[PROFILER].freq_stats[CPU].ratio; };
u64 mifpro_get_freq_stats2_sum(void) { return profiler.mif.result[PROFILER].freq_stats[GPU].sum; };
u64 mifpro_get_freq_stats2_ratio(void) { return profiler.mif.result[PROFILER].freq_stats[GPU].ratio; };
u64 mifpro_get_llc_status(void) { return profiler.mif.result[PROFILER].llc_status; };

struct profiler_fn_mif mif_fn_profiler = {
	.get_stats0_sum		= &mifpro_get_freq_stats0_sum,
	.get_stats0_ratio	= &mifpro_get_freq_stats0_ratio,
	.get_stats0_avg		= &mifpro_get_freq_stats0_avg,
	.get_stats1_sum		= &mifpro_get_freq_stats1_sum,
	.get_stats1_ratio	= &mifpro_get_freq_stats1_ratio,
	.get_stats2_sum		= &mifpro_get_freq_stats2_sum,
	.get_stats2_ratio	= &mifpro_get_freq_stats2_ratio,
	.get_llc_status		= &mifpro_get_llc_status,
};

struct domain_fn mif_fn_domain = {
	.get_table_cnt		= &mifpro_get_table_cnt,
	.get_freq_table		= &mifpro_get_freq_table,
	.get_max_freq		= &mifpro_get_max_freq,
	.get_min_freq		= &mifpro_get_min_freq,
	.get_freq			= &mifpro_get_freq,
	.get_power			= &mifpro_get_power,
	.get_power_change	= &mifpro_get_power_change,
	.get_active_pct		= &mifpro_get_active_pct,
	.get_temp			= &mifpro_get_temp,
	.set_margin			= &mifpro_set_margin,
	.update_mode		= &mifpro_update_mode,
};

/************************************************************************
 *			Gathering MIFFreq Information			*
 ************************************************************************/
//ktime_t * exynos_stats_get_mif_time_in_state(void);
extern u64 exynos_bcm_get_ccnt(unsigned int idx);
u32 mifpro_update_profile(int user)
{
	struct freq_cstate *fc = &profiler.mif.fc;
	struct freq_cstate_snapshot *fc_snap = &profiler.mif.fc_snap[user];
	struct freq_cstate_result *fc_result = &profiler.mif.result[user].fc_result;
	struct mif_profile_result *result = &profiler.mif.result[user];
	int i;
	u64 total_active_time = 0;
//	static u64 *tdata_in_state = NULL;
//	u64 transfer_data[NR_MASTERS] = { 0, };
//	u64 mo_count[NR_MASTERS] = { 0, };
//	u64 nr_requests[NR_MASTERS] = { 0, };
//	u64 diff_ccnt = 0;
	struct exynos_wow_profile wow_profile;
	struct exynos_wow_meta meta;
	u32 cur_freq = (u32)exynos_devfreq_get_domain_freq(DEVFREQ_MIF);

	profiler.mif.cur_freq_idx = get_idx_from_freq(profiler.mif.table, profiler.mif.table_cnt,
			cur_freq, RELATION_LOW);
	profiler.mif.max_freq_idx = get_idx_from_freq(profiler.mif.table, profiler.mif.table_cnt,
			exynos_pm_qos_request(profiler.mif.freq_infos.pm_qos_class_max), RELATION_LOW);
	profiler.mif.min_freq_idx = get_idx_from_freq(profiler.mif.table, profiler.mif.table_cnt,
			exynos_pm_qos_request(profiler.mif.freq_infos.pm_qos_class), RELATION_HIGH);

//	if (!tdata_in_state)
//		tdata_in_state = kzalloc(sizeof(u64) * profiler.mif.table_cnt, GFP_KERNEL);

//	exynos_devfreq_get_profile(profiler.mif.devfreq_type, fc->time, tdata_in_state);
	exynos_wow_get_data(&wow_profile);
	exynos_wow_get_meta(&meta);

	for (i = 0 ; i < profiler.mif.table_cnt; i++) {
		struct exynos_wow_ts_data *ts = &wow_profile.ts[i];

		fc->time[CS_ACTIVE][i] = ts->active_time;
		fc->time[CLK_OFF][i] = ts->time - ts->active_time;
	}

	// calculate delta from previous status
	make_snapshot_and_time_delta(fc, fc_snap, fc_result, profiler.mif.table_cnt);

	// Call to calc power
	compute_freq_cstate_result(profiler.mif.table, fc_result, profiler.mif.table_cnt,
					profiler.mif.cur_freq_idx, result->avg_temp);

	if (!fc_result->profile_time)
		goto out;

	// Calculate freq_stats array
	for (i = 0; i < profiler.mif.table_cnt; i++) {
		struct exynos_wow_meta_data *md = &meta.data[MIF];
		struct exynos_wow_ts_data *ts = &wow_profile.ts[i];
		struct exynos_wow_ts_data *prev_ts =
					&profiler.mif.prev_wow_profile.ts[i];

		// MBytes/s
		result->tdata_in_state[i] =
			((ts->tdata - prev_ts->tdata) * md->bus_width) >> 20;
	}

//	diff_ccnt = (wow_profile.total_ccnt - profiler.mif.prev_wow_profile.total_ccnt);
//	if(wow_profile.data[MIF].transfer_data) {
//		for (i = 0 ; i < NR_MASTERS ; i++) {
//			transfer_data[i] = (wow_profile.data[i].transfer_data - profiler.mif.prev_wow_profile.data[i].transfer_data) >> 20;
//			nr_requests[i] = wow_profile.data[i].nr_requests - profiler.mif.prev_wow_profile.data[i].nr_requests;
//			mo_count[i] = wow_profile.data[i].mo_count - profiler.mif.prev_wow_profile.data[i].mo_count;
//		}
//	} else {
//		for (i = 0 ; i < NR_MASTERS ; i++) {
//			if (i == MIF) {
//				continue;
//			}
//			transfer_data[MIF] += (wow_profile.data[i].transfer_data - profiler.mif.prev_wow_profile.data[i].transfer_data) >> 20;
//			nr_requests[MIF] += wow_profile.data[i].nr_requests - profiler.mif.prev_wow_profile.data[i].nr_requests;
//			mo_count[MIF] += wow_profile.data[i].mo_count - profiler.mif.prev_wow_profile.data[i].mo_count;
//		}
//	}

	/* copy to prev buffer */
//	memcpy(&profiler.mif.prev_wow_profile, &wow_profile, sizeof(struct exynos_wow_profile));
//	memcpy(profiler.mif.prev_tdata_in_state, tdata_in_state, sizeof(u64) * profiler.mif.table_cnt);

//	for (i = 0 ; i < profiler.mif.table_cnt; i++)
//		fc->time[CLK_OFF][i] -= fc->time[CS_ACTIVE][i];

	memset(&result->freq_stats, 0, sizeof(struct mif_freq_state) * NR_MASTERS);
	fc_result->dyn_power = 0;
	for (i = 0; i < profiler.mif.table_cnt; i++) {
		if (profiler.mif.table[i].freq < 1000)
			continue;
		result->freq_stats[MIF].avg += (result->tdata_in_state[i] << 40)
					 / (profiler.mif.table[i].freq / 1000);
		total_active_time += fc_result->time[CS_ACTIVE][i];
		fc_result->dyn_power += ((result->tdata_in_state[i]
					 * profiler.mif.table[i].dyn_cost)
						/ fc_result->profile_time);
	}

	if (!total_active_time)
		goto out;

	result->freq_stats[MIF].avg = result->freq_stats[MIF].avg
							/ total_active_time;

	result->llc_status = llc_get_en();

	// get sum and ratio values for ALL IPs
	for (i = 0 ; i < NR_MASTERS ; i++) {
		struct exynos_wow_meta_data *md = &meta.data[i];
		struct exynos_wow_profile_data *old_pd, *new_pd;
		u64 ccnt_osc_avg, ccnt_avg, tdata_sum, nr_req_sum, mo_cnt_sum;

		old_pd = &profiler.mif.prev_wow_profile.data[i];;
		new_pd = &wow_profile.data[i];
		tdata_sum = (new_pd->transfer_data - old_pd->transfer_data)
				* md->bus_width;
		ccnt_osc_avg = (new_pd->ccnt_osc - old_pd->ccnt_osc)
				/ md->nr_info;
		ccnt_avg = (new_pd->ccnt - old_pd->ccnt)
				/ md->nr_info;

		// WA!! use CCNT_OSC instead of CCNT becaus of CP mif
		if (ccnt_avg > ccnt_osc_avg)
			ccnt_osc_avg = ccnt_avg;

		mo_cnt_sum = new_pd->mo_count - old_pd->mo_count;
		nr_req_sum = new_pd->nr_requests - old_pd->nr_requests;

		if (!ccnt_osc_avg || !nr_req_sum)
			continue;

		result->freq_stats[i].sum = (tdata_sum  * NSEC_PER_SEC)
						/ fc_result->profile_time;
		result->freq_stats[i].ratio = mo_cnt_sum / nr_req_sum
				* (fc_result->profile_time / ccnt_osc_avg);
	}

	// Work Around Start
	if (IS_ENABLED(CONFIG_SOC_S5E9955)) {
		u64 ccnt_osc_avg, ccnt_avg, tdata_sum = 0, nr_req_sum = 0, mo_cnt_sum = 0;

		for (i = 0 ; i < NR_MASTERS ; i++) {
			struct exynos_wow_meta_data *md = &meta.data[i];
			struct exynos_wow_profile_data *old_pd, *new_pd;

			if (i == MIF)
				continue;
			old_pd = &profiler.mif.prev_wow_profile.data[i];;
			new_pd = &wow_profile.data[i];
			tdata_sum += (new_pd->transfer_data - old_pd->transfer_data)
					* md->bus_width;
			ccnt_osc_avg = (new_pd->ccnt_osc - old_pd->ccnt_osc)
					/ md->nr_info;
			ccnt_avg = (new_pd->ccnt - old_pd->ccnt)
					/ md->nr_info;
			// WA!! use CCNT_OSC instead of CCNT becaus of CP mif
			if (ccnt_avg > ccnt_osc_avg)
				ccnt_osc_avg = ccnt_avg;

			mo_cnt_sum += new_pd->mo_count - old_pd->mo_count;
			nr_req_sum += new_pd->nr_requests - old_pd->nr_requests;
		}
		result->freq_stats[MIF].ratio = mo_cnt_sum / nr_req_sum
				* (fc_result->profile_time / ccnt_osc_avg);
	}
	// Work Around END

	if (profiler.mif.tz) {
		int temp = exynos_profiler_get_temp(profiler.mif.tz);
		profiler.mif.result[user].avg_temp = (temp + profiler.mif.result[user].cur_temp) >> 1;
		profiler.mif.result[user].cur_temp = temp;
	}

out:
	profiler.mif.prev_wow_profile = wow_profile;

	return 0;
}

/************************************************************************
 *				INITIALIZATON				*
 ************************************************************************/
/* Initialize profiler data */
static int get_freq_table_info(u32 *max_freq, u32 *min_freq, u32 *cur_freq)
{
	struct exynos_devfreq_freq_infos *freq_infos = &profiler.mif.freq_infos;

	exynos_devfreq_get_freq_infos(profiler.mif.devfreq_dev, freq_infos);

	*max_freq = freq_infos->max_freq;		/* get_dev_max_freq(void) */
	*min_freq = freq_infos->min_freq;		/* get_dev_min_freq(void) */
	*cur_freq = freq_infos->cur_freq;		/* get_cur_freq(void)	  */
	profiler.mif.table_cnt = freq_infos->max_state;	/* get_freq_table_cnt(void)  */

	return 0;
}

static int parse_dt(struct device_node *dn)
{
	struct platform_device *devfreq_pdev;
	struct device_node *devfreq_np;
	int ret;

	/* necessary data */
	ret = of_property_read_u32(dn, "cal-id", &profiler.mif.cal_id);
	if (ret)
		return -2;

	/* un-necessary data */
	ret = of_property_read_s32(dn, "profiler-id", &profiler.mif.profiler_id);
	if (ret)
		profiler.mif.profiler_id = -1;	/* Don't support profiler */

	devfreq_np = of_parse_phandle(dn, "devfreq-dev", 0);
	if (!devfreq_np) {
		pr_err("Unable to find devfreq-dev\n");
		return -ENODEV;
	}

	devfreq_pdev = of_find_device_by_node(devfreq_np);
	profiler.mif.devfreq_dev = &devfreq_pdev->dev;

	of_property_read_u32(dn, "power-coefficient", &profiler.mif.dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &profiler.mif.st_pwr_coeff);
	of_property_read_string(dn, "tz-name", &profiler.mif.tz_name);

	return 0;
}

static int init_profile_result(struct mif_profile_result *result, int size)
{
	if (init_freq_cstate_result(&result->fc_result, size))
		return -ENOMEM;

	/* init private data */
	result->tdata_in_state = alloc_state_in_freq(size);

	return 0;
}

#ifdef CONFIG_EXYNOS_DEBUG_INFO
static void show_profiler_info(void)
{
	int idx;

	pr_info("================ mif domain ================\n");
	pr_info("min= %dKHz, max= %dKHz\n",
			profiler.mif.table[profiler.mif.table_cnt - 1].freq, profiler.mif.table[0].freq);
	for (idx = 0; idx < profiler.mif.table_cnt; idx++)
		pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%lld st_cost=%lld\n",
			idx, profiler.mif.table[idx].freq, profiler.mif.table[idx].volt,
			profiler.mif.table[idx].dyn_cost,
			profiler.mif.table[idx].st_cost);
	if (profiler.mif.tz_name)
		pr_info("support temperature (tz_name=%s)\n", profiler.mif.tz_name);
	if (profiler.mif.profiler_id != -1)
		pr_info("support profiler domain(id=%d)\n", profiler.mif.profiler_id);
}
#endif

static int exynos_mif_profiler_probe(struct platform_device *pdev)
{
	unsigned int dev_max_freq, dev_min_freq, dev_cur_freq;
	int ret, idx;

	/* get node of device tree */
	if (!pdev->dev.of_node) {
		pr_err("mifpro: failed to get device treee\n");
		return -EINVAL;
	}
	profiler.dn = pdev->dev.of_node;

	/* Parse data from Device Tree to init domain */
	ret = parse_dt(profiler.dn);
	if (ret) {
		pr_err("mifpro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	/* init freq table */
	get_freq_table_info(&dev_max_freq, &dev_min_freq, &dev_cur_freq);
	if (profiler.mif.table_cnt < 1) {
		pr_err("mifpro: failed to get table_cnt\n");
		return -EINVAL;
	}
	profiler.mif.table = init_freq_table(NULL, profiler.mif.table_cnt,
			profiler.mif.cal_id, dev_max_freq, dev_min_freq,
			profiler.mif.dyn_pwr_coeff, profiler.mif.st_pwr_coeff,
			PWR_COST_CVV, PWR_COST_CVV);
	if (!profiler.mif.table) {
		pr_err("mifpro: failed to init freq_table\n");
		return -EINVAL;
	}
	profiler.mif.max_freq_idx = 0;
	profiler.mif.min_freq_idx = profiler.mif.table_cnt - 1;
	profiler.mif.cur_freq_idx = get_idx_from_freq(profiler.mif.table,
				profiler.mif.table_cnt, dev_cur_freq, RELATION_HIGH);

	if (init_freq_cstate(&profiler.mif.fc, profiler.mif.table_cnt))
			return -ENOMEM;

	/* init snapshot & result table */
	for (idx = 0; idx < NUM_OF_USER; idx++) {
		if (init_freq_cstate_snapshot(&profiler.mif.fc_snap[idx],
						profiler.mif.table_cnt))
			return -ENOMEM;

		if (init_profile_result(&profiler.mif.result[idx], profiler.mif.table_cnt))
			return -EINVAL;
	}
	profiler.mif.prev_tdata_in_state = kzalloc(sizeof(u64) * profiler.mif.table_cnt, GFP_KERNEL);

	/* get thermal-zone to get temperature */
	if (profiler.mif.tz_name)
		profiler.mif.tz = exynos_profiler_init_temp(profiler.mif.tz_name);

	if (profiler.mif.tz)
		init_static_cost(profiler.mif.table, profiler.mif.table_cnt,
				1, profiler.dn, profiler.mif.tz);

	ret = exynos_profiler_register_domain(PROFILER_MIF, &mif_fn_domain, &mif_fn_profiler);

#ifdef CONFIG_EXYNOS_DEBUG_INFO
	show_profiler_info();
#endif

	return ret;
}

static const struct of_device_id exynos_mif_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-mif-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mif_profiler_match);

static struct platform_driver exynos_mif_profiler_driver = {
	.probe		= exynos_mif_profiler_probe,
	.driver	= {
		.name	= "exynos-mif-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_mif_profiler_match,
	},
};

static int exynos_mif_profiler_init(void)
{
	return platform_driver_register(&exynos_mif_profiler_driver);
}
late_initcall(exynos_mif_profiler_init);

MODULE_DESCRIPTION("Exynos MIF Profiler v2");
MODULE_SOFTDEP("pre: exynos_thermal_v2 exynos-cpufreq exynos_devfreq exynos-wow sgpu");
MODULE_LICENSE("GPL");
