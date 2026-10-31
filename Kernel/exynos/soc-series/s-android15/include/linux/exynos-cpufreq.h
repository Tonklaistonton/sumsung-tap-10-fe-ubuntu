/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * linux/include/linux/exynos-cpufreq.h
 *
 */

#if IS_ENABLED(CONFIG_EXYNOS_CPUFREQ)
extern int exynos_cpufreq_register_transition_notifier(
						struct notifier_block *nb);
extern int exynos_cpufreq_get_minfreq(int domain_id, u32 *min_freq);
#else
static inline int exynos_cpufreq_register_transition_notifier(
						struct notifier_block *nb)
{
	return -ENODEV;
}
static inline int exynos_cpufreq_get_minfreq(int domain_id, u32 *min_freq);
{
	return -ENODEV;
}
#endif

