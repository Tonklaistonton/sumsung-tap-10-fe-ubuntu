/* SPDX-License-Identifier: GPL-2.0-only */
/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2019 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*/

#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/kernel.h>
#include <linux/types.h>

struct freq_log;

/*
 * SEC DEBUG MODE
 */
#if IS_ENABLED(CONFIG_SEC_DEBUG_MODE)
extern unsigned int sec_debug_get_debug_level(void);
extern bool sec_debug_get_force_upload(void);
#else
static inline unsigned int sec_debug_get_debug_level(void)
{
	return 0;
}

static inline bool sec_debug_get_force_upload(void)
{
	return 0;
}
#endif

/*
 * SEC DEBUG EXTRA INFO
 */
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
extern void secdbg_exin_set_finish(void);
extern void secdbg_exin_set_panic(const char *str);
extern void secdbg_exin_set_busmon(const char *str);
extern void secdbg_exin_set_sysmmu(const char *str);
extern void secdbg_exin_set_smpl(unsigned long count);
extern void secdbg_exin_set_decon(const char *str);
extern void secdbg_exin_set_batt(int cap, int volt, int temp, int curr);
extern void secdbg_exin_set_mfc_error(const char *str);
extern void secdbg_exin_set_aud(const char *str);
extern void secdbg_exin_set_gpuinfo(const char *str);
extern void secdbg_exin_set_epd(const char *str);
extern void secdbg_exin_set_asv(int bg, int mg, int lg, int g3dg, int mifg);
extern void secdbg_exin_set_ids(int bids, int mids, int lids, int gids);
extern void secdbg_exin_set_unfz(const char *comm, int pid);
extern char *secdbg_exin_get_unfz(void);
extern void secdbg_exin_set_hardlockup_type(const char *fmt, ...);
extern void secdbg_exin_set_hardlockup_data(const char *str);
extern void secdbg_exin_set_hardlockup_freq(const char *domain, struct freq_log *freq);
extern void secdbg_exin_set_hardlockup_ehld(unsigned int hl_info, unsigned int cpu);
extern void secdbg_exin_set_ufs(const char *str);
#else /* !CONFIG_SEC_DEBUG_EXTRA_INFO */
#define secdbg_exin_set_finish(a)	do { } while (0)
#define secdbg_exin_set_panic(a)	do { } while (0)
#define secdbg_exin_set_busmon(a)	do { } while (0)
#define secdbg_exin_set_sysmmu(a)	do { } while (0)
#define secdbg_exin_set_smpl(a)		do { } while (0)
#define secdbg_exin_set_decon(a)	do { } while (0)
#define secdbg_exin_set_batt(a, b, c, d)	do { } while (0)
#define secdbg_exin_set_mfc_error(a)	do { } while (0)
#define secdbg_exin_set_aud(a)		do { } while (0)
#define secdbg_exin_set_gpuinfo(a)		do { } while (0)
#define secdbg_exin_set_epd(a)		do { } while (0)
#define secdbg_exin_set_asv(a, b, c, d, e)	do { } while (0)
#define secdbg_exin_set_ids(a, b, c, d)		do { } while (0)
#define secdbg_exin_set_unfz(a, b)	do { } while (0)
#define secdbg_exin_get_unfz()		("")
#define secdbg_exin_set_hardlockup_type(a, ...)	do { } while (0)
#define secdbg_exin_set_hardlockup_data(a)	do { } while (0)
#define secdbg_exin_set_hardlockup_freq(a, b)	do { } while (0)
#define secdbg_exin_set_hardlockup_ehld(a, b)	do { } while (0)
#define secdbg_exin_set_ufs(a)		do { } while (0)
#endif /* CONFIG_SEC_DEBUG_EXTRA_INFO */

#if IS_ENABLED(CONFIG_SEC_DEBUG_SOFTDOG)
void secdbg_softdog_show_info(void);
#else
#define secdbg_softdog_show_info()		do { } while (0)
#endif

extern void secdbg_exin_set_main_ocp(void *main_ocp_cnt, void *main_oi_cnt, int buck_cnt);

#define OCP_OI_COUNTER_TYPE_OCP	(1)
#define OCP_OI_COUNTER_TYPE_OI	(3)
extern void secdbg_exin_set_main_ocp_count(unsigned int ocp_oi);
extern void secdbg_exin_set_sub_ocp_count(unsigned int ocp_oi);

#endif /* SEC_DEBUG_H */

