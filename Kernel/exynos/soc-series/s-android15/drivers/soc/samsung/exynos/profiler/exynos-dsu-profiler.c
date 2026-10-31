#include <exynos-dsu-profiler.h>

/************************************************************************
 *				SUPPORT-PROFILER				*
 ************************************************************************/
u32 dsupro_get_table_cnt(s32 id)
{
	return profiler.dsu.table_cnt;
}
u32 dsupro_get_freq_table(s32 id, u32 *table)
{
	int idx;
	for (idx = 0; idx < profiler.dsu.table_cnt; idx++)
		table[idx] = profiler.dsu.table[idx].freq;
	return idx;
}
u32 dsupro_get_max_freq(s32 id)
{
	return dsufreq_get_max_freq();
}
u32 dsupro_get_min_freq(s32 id)
{
	return dsufreq_get_min_freq();
}
u32 dsupro_get_freq(s32 id)
{
	return dsufreq_get_cur_freq();
}
void dsupro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	return;
}
void dsupro_get_power_change(s32 id, s32 freq_delta_ratio,
			u32 *freq, u64 *dyn_power, u64 *st_power)
{
	return;
}
u32 dsupro_get_active_pct(s32 id)
{
	return 0;
}
s32 dsupro_get_temp(s32 id)
{
	return 0;
}
void dsupro_set_margin(s32 id, s32 margin)
{
	return;
}
u32 dsupro_update_profile(void);
u32 dsupro_update_mode(s32 id, int mode)
{
	profiler.dsu.enabled = mode;
	dsupro_update_profile();
	return 0;
}
struct domain_fn dsu_fn = {
	.get_table_cnt		= &dsupro_get_table_cnt,
	.get_freq_table		= &dsupro_get_freq_table,
	.get_max_freq		= &dsupro_get_max_freq,
	.get_min_freq		= &dsupro_get_min_freq,
	.get_freq			= &dsupro_get_freq,
	.get_power			= &dsupro_get_power,
	.get_power_change	= &dsupro_get_power_change,
	.get_active_pct		= &dsupro_get_active_pct,
	.get_temp			= &dsupro_get_temp,
	.set_margin			= &dsupro_set_margin,
	.update_mode		= &dsupro_update_mode,
};
/************************************************************************
 *			Gathering DSU Freq Information			*
 ************************************************************************/
u32 dsupro_update_profile(void)
{
	return 0;
}
/************************************************************************
 *				INITIALIZATON				*
 ************************************************************************/
static int parse_dt(struct device_node *dn)
{
	int ret;
	/* necessary data */
	ret = of_property_read_u32(dn, "cal-id", &profiler.dsu.cal_id);
	if (ret) {
		pr_err("dsupro: failed to get cal-id\n");
		return -1;
	}
	/* un-necessary data */
	ret = of_property_read_s32(dn, "profiler-id", &profiler.dsu.profiler_id);
	if (ret)
		profiler.dsu.profiler_id = -1;	/* Don't support profiler */
	return 0;
}
#ifdef CONFIG_EXYNOS_DEBUG_INFO
static void show_profiler_info(void)
{
	int idx;
	pr_info("================ dsu domain ================\n");
	pr_info("min= %dKHz, max= %dKHz\n",
			profiler.dsu.table[profiler.dsu.table_cnt - 1].freq, profiler.dsu.table[0].freq);
	for (idx = 0; idx < profiler.dsu.table_cnt; idx++)
		pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%lld st_cost=%lld\n",
			idx, profiler.dsu.table[idx].freq, profiler.dsu.table[idx].volt,
			profiler.dsu.table[idx].dyn_cost,
			profiler.dsu.table[idx].st_cost);
	if (profiler.dsu.profiler_id != -1)
		pr_info("support profiler domain(id=%d)\n", profiler.dsu.profiler_id);
}
#endif
static int exynos_dsu_profiler_probe(struct platform_device *pdev)
{
	unsigned long *table;
	int *table_int;
	int ret;
		
	/* get node of device tree */
	if (!pdev->dev.of_node) {
		pr_err("dsupro: failed to get device tree\n");
		return -EINVAL;
	}
	profiler.dsu.root = pdev->dev.of_node;
	/* Parse data from Device Tree to init domain */
	ret = parse_dt(profiler.dsu.root);
	if (ret) {
		pr_err("dsupro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}
	table = dsufreq_get_freq_table(&profiler.dsu.table_cnt);
	if (profiler.dsu.table_cnt < 1 || !table) {
		pr_err("dsupro: failed to get dsu table\n");
		return -EINVAL;
	}
	table_int = kcalloc(profiler.dsu.table_cnt, sizeof(unsigned int), GFP_KERNEL);
	if (!table_int) {
		kfree(table_int);
		return -ENOMEM;
	} else {
		for (int i=0; i < profiler.dsu.table_cnt; i++)
			table_int[i] = (u32)table[i];
	}
	/* init freq table */
    profiler.dsu.table = init_fvtable(
			table_int, profiler.dsu.table_cnt,
			profiler.dsu.cal_id, dsufreq_get_max_freq(), dsufreq_get_min_freq(),
			0, 0, 0, 0);
	if (!profiler.dsu.table) {
		pr_err("dsupro: failed to memory allocation\n");
		kfree(table_int);
		return -ENOMEM;
	} else {
		kfree(table_int);
	}
	profiler.dsu.max_freq_idx = 0;
	profiler.dsu.min_freq_idx = profiler.dsu.table_cnt - 1;
	profiler.dsu.cur_freq_idx = get_idx_from_freq(profiler.dsu.table,
				profiler.dsu.table_cnt, dsufreq_get_cur_freq(), RELATION_HIGH);
	ret = exynos_profiler_register_domain(PROFILER_DSU, &dsu_fn, NULL);
#ifdef CONFIG_EXYNOS_DEBUG_INFO
	show_profiler_info();
#endif
	pr_info("dsupro: finish dsu probe\n");
	return ret;
}
static const struct of_device_id exynos_dsu_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-dsu-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_dsu_profiler_match);
static struct platform_driver exynos_dsu_profiler_driver = {
	.probe		= exynos_dsu_profiler_probe,
	.driver	= {
		.name	= "exynos-dsu-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_dsu_profiler_match,
	},
};
static int exynos_dsu_profiler_init(void)
{
	return platform_driver_register(&exynos_dsu_profiler_driver);
}
late_initcall(exynos_dsu_profiler_init);
MODULE_SOFTDEP("pre: exynos-cpufreq exynos_devfreq");
MODULE_LICENSE("GPL");
