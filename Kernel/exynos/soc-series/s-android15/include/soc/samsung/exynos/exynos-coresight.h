/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 */

#ifndef EXYNOS_CORESIGHT_H
#define EXYNOS_CORESIGHT_H

#if IS_ENABLED(CONFIG_EXYNOS_CORESIGHT)
int exynos_cs_stop_cpus(void);
int exynos_cs_stop_cpu(unsigned int cpu);
#else
#define exynos_cs_stop_cpus()	do { } while (0)
#define exynos_cs_stop_cpu(a)   do { } while (0)
#endif

#if IS_ENABLED(CONFIG_EXYNOS_CORESIGHT_ETM)
extern int gs_coresight_etm_external_etr_on(u64 buf_addr, u32 buf_size);
extern int gs_coresight_etm_external_etr_off(void);
extern int coresight_etm_funnel_on(unsigned int funnel_num, unsigned int port_num, bool on);
#else
#define gs_coresight_etm_external_etr_on(a, b) do { } while (0)
#define gs_coresight_etm_external_etr_off() do { } while (0)
#define coresight_etm_funnel_on(a, b, c) do { } while (0)
#endif

/*
 * For GKI boot.img code space. It can only be used
 * when it is set for the halt of the exynos-coresight module driver.
 */
static inline void raw_exynos_stop_cpus(void) {
	asm volatile("hlt #0xff");
}

#endif
