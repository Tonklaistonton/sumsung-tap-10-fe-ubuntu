/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_LOG_H_
#define _NPU_LOG_H_

#define DEBUG

#include <linux/version.h>
#include <linux/types.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/printk.h>
#include <linux/smp.h>
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/exynos/memlogger.h>
#else
#include "npu-device.h"
#endif
#include "include/npu-common.h"

#define NPU_RING_LOG_BUFFER_MAGIC	0x3920FFAC
#define LOG_UNIT_NUM		(1024)
#define	NPU_LOG_ACTIVE		(0xCAFECAFE)
#define	NPU_LOG_DEACTIVE	(0x0)

struct npu_device;

/* Log level definition */
typedef enum {
	NPU_LOG_TRACE	= 0,
	NPU_LOG_DBG,	/* Default. Set in npu_log.c */
	NPU_LOG_INFO,
	NPU_LOG_WARN,
	NPU_LOG_ERR,
	NPU_LOG_NONE,
	NPU_LOG_INVALID,
} npu_log_level_e;

#define LOG_TAG(x) (x == NPU_HWDEV_ID_NPU) ? "NPU" : (x == NPU_HWDEV_ID_NONE) ? "AIC" : "DSP"
#define NPU_KPTR_LOG    "%pK"

struct npu_log_ops {
	void (*fw_rprt_manager)(void);
	void (*fw_rprt_gather)(void);
	int (*npu_check_unposted_mbox)(int nCtrl);
};

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
struct npu_log_ioctl {
	u64	timestamp;
	u32	cmd;	/* IOCTL cmd */
	u32	dir;	/* IOCTL dir */
};

union npu_log_tag {
	struct npu_log_ioctl i;
};
#endif

#if IS_ENABLED(CONFIG_SOC_S5E9955)
enum exynos_dm_index {
    DM_MIF = 0,
    DM_INT,
    DM_NPU = 6,
    DM_DSP = 19,
    DM_DNC = 20,
    DM_UNPU = 27,
    DM_END,
};

struct apm_trace {
	u64 time:32;
	u64 direction:1;
	u64 type:1;
	u64 domain:8;
	u64 freq:22;
};
#endif

struct npu_log {
	char			*st_buf;
	size_t			wr_pos;
	size_t			rp_pos;
	size_t			st_size;
	size_t			line_cnt;
	char			npu_err_in_dmesg;

	size_t			last_dump_line_cnt;
	atomic_t	npu_log_active;

	wait_queue_head_t	wq;

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	/* memlog for Unified Logging System*/
	struct memlog *memlog_desc_log;
	struct memlog *memlog_desc_array;
	struct memlog *memlog_desc_dump;
	struct memlog *memlog_desc_health;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	struct memlog *memlog_desc_fw_governor;
#endif
	struct memlog_obj *npu_memlog_obj;
	struct memlog_obj *npu_dumplog_obj;
	struct memlog_obj *npu_memfile_obj;
	struct memlog_obj *npu_memhealth_obj;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	struct memlog_obj *npu_memfw_governor_obj;
#endif
	struct memlog_obj *npu_ioctl_array_obj;
	struct memlog_obj *npu_array_file_obj;
#endif
	const struct npu_log_ops	*log_ops;
};

/* Log flush wait interval and count when the store log is de-initialized */
#define NPU_STORE_LOG_FLUSH_INTERVAL_MS		500

void npu_fw_report_init(char *buf_addr, const size_t size);
int npu_fw_report_store(char *strRep, size_t nSize);
void dbg_print_fw_report_st(struct npu_log stLog);
void npu_fw_report_deinit(void);

void npu_memlog_store(npu_log_level_e loglevel, const char *fmt, ...);
void npu_dumplog_store(npu_log_level_e loglevel, const char *fmt, ...);
void npu_healthlog_store(npu_log_level_e loglevel, const char *fmt, ...);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
void npu_fw_governor_log_store(npu_log_level_e loglevel, const char *fmt, ...);
#endif
void npu_log_set_loglevel(int slient, int loglevel);

int npu_debug_memdump32_by_memcpy(u32 *start, u32 *end);

/* for debug and performance */
inline void npu_log_ioctl_set_date(int cmd, int dir);

#define ISPRINTABLE(strValue)	((isascii(strValue) && isprint(strValue)) ? \
	((strValue == '%') ? '.' : strValue) : '.')

#define probe_info(fmt, ...)            pr_info("NPU:" fmt, ##__VA_ARGS__)
#define probe_warn(fmt, args...)        pr_warn("NPU:[WRN]" fmt, ##args)
#define probe_err(fmt, args...)         pr_err("NPU:[ERR]%s:%d:" fmt, __func__, __LINE__, ##args)
#define probe_trace(fmt, ...)           pr_info(fmt, ##__VA_ARGS__)

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define npu_log_on_lv_target(LV, fmt, ...)	\
				npu_memlog_store(LV, "[%d: %15s:%5d]" fmt, __smp_processor_id(), current->comm, current->pid, ##__VA_ARGS__)

#define npu_dump_on_lv_target(LV, fmt, ...)	\
				npu_dumplog_store(LV, "[%d: %15s:%5d]" fmt, __smp_processor_id(), current->comm, current->pid, ##__VA_ARGS__)

#define npu_health_on_lv_target(LV, fmt, ...)	\
				npu_healthlog_store(LV, "[%d: %15s:%5d]" fmt, __smp_processor_id(), current->comm, current->pid, ##__VA_ARGS__)

#if IS_ENABLED(CONFIG_SOC_S5E9955)
#define npu_fw_governor_on_lv_target(LV, fmt, ...)	\
				npu_fw_governor_log_store(LV, "[%d: %15s:%5d]" fmt, __smp_processor_id(), current->comm, current->pid, ##__VA_ARGS__)
#endif

#define npu_err_target(fmt, ...)		npu_log_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#define npu_warn_target(fmt, ...)		npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define npu_info_target(fmt, ...)		npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define npu_notice_target(fmt, ...)		npu_log_on_lv_target(MEMLOG_LEVEL_CAUTION, fmt, ##__VA_ARGS__)
#define npu_dbg_target(fmt, ...)		npu_log_on_lv_target(MEMLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define npu_trace_target(fmt, ...)		npu_log_on_lv_target(MEMLOG_LEVEL_NOTICE, fmt, ##__VA_ARGS__)
#define npu_dump_target(fmt, ...)		npu_dump_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#define npu_health_target(fmt, ...)		npu_health_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#if IS_ENABLED(CONFIG_SOC_S5E9955)
#define npu_fw_governor_target(fmt, ...)	npu_fw_governor_on_lv_target(MEMLOG_LEVEL_ERR, fmt, ##__VA_ARGS__)
#endif
#else
#define npu_err_target(fmt, ...)		printk(KERN_ERR fmt, ##__VA_ARGS__)
#define npu_warn_target(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define npu_info_target(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define npu_notice_target(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define npu_dbg_target(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define npu_trace_target(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define npu_dump_target(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define npu_health_target(fmt, ...)		printk(KERN_WARNING fmt, ##__VA_ARGS__)
#if IS_ENABLED(CONFIG_SOC_S5E9955)
#define npu_fw_governor_target(fmt, ...)	printk(KERN_WARNING fmt, ##__VA_ARGS__)
#endif
#endif

#define npu_err(fmt, args...) \
	npu_err_target("AIC:[ERR]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_warn(fmt, args...) \
	npu_warn_target("AIC:[WRN]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_info(fmt, args...) \
	npu_info_target("AIC:[INFO]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_notice(fmt, args...)	\
	npu_notice_target("AIC:[NOTICE]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_dbg(fmt, args...) \
	npu_dbg_target("AIC:[DBG]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_trace(fmt, args...) \
	npu_trace_target("AIC:[T]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_dump(fmt, args...) \
	npu_dump_target("AIC:[DUMP]%s(%d):" fmt, __func__, __LINE__, ##args)

#define npu_health(fmt, args...) \
	npu_health_target("AIC:[HEAL]%s(%d):" fmt, __func__, __LINE__, ##args)

#if IS_ENABLED(CONFIG_SOC_S5E9955)
#define npu_fw_governor_log(fmt, args...) \
	npu_fw_governor_target("[GVRN]%s(%d):" fmt, __func__, __LINE__, ##args)
#endif

/* Printout context ID */
#define npu_ierr(fmt, vctx, hid, args...) \
	npu_err_target("%s:[ERR][I%d]%s(%d):" fmt, LOG_TAG(hid), (vctx)->id, __func__, __LINE__, ##args)

#define npu_iwarn(fmt, vctx, hid, args...) \
	npu_warn_target("%s:[WRN][I%d]%s(%d):" fmt, LOG_TAG(hid), (vctx)->id, __func__, __LINE__, ##args)

#define npu_iinfo(fmt, vctx, hid, args...) \
	npu_info_target("%s:[INFO][I%d]%s(%d):" fmt, LOG_TAG(hid), (vctx)->id, __func__, __LINE__, ##args)

#define npu_inotice(fmt, vctx, hid, args...) \
	npu_notice_target("%s:[NOTICE][I%d]%s(%d):" fmt, LOG_TAG(hid), (vctx)->id, __func__, __LINE__, ##args)

#define npu_idbg(fmt, vctx, hid, args...) \
	npu_dbg_target("%s:[DBG][I%d]%s(%d):" fmt, LOG_TAG(hid), (vctx)->id, __func__, __LINE__, ##args)

#define npu_itrace(fmt, vctx, hid, args...) \
	npu_trace_target("%s:[T][I%d]%s(%d):" fmt, LOG_TAG(hid), (vctx)->id, __func__, __LINE__, ##args)

/* Printout unique ID */
#define npu_uerr(fmt, uid_obj, hid, args...) \
	npu_err_target("%s:[ERR][U%d]%s(%d):" fmt, LOG_TAG(hid), (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_uwarn(fmt, uid_obj, hid, args...) \
	npu_warn_target("%s:[WRN][U%d]%s(%d):" fmt, LOG_TAG(hid), (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_uinfo(fmt, uid_obj, hid, args...) \
	npu_info_target("%s:[INFO][U%d]%s(%d):" fmt, LOG_TAG(hid), (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_unotice(fmt, uid_obj, hid, args...)	\
	npu_notice_target("%s:[NOTICE][U%d]%s(%d):" fmt, LOG_TAG(hid), (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_udbg(fmt, uid_obj, hid, args...) \
	npu_dbg_target("%s:[DBG][U%d]%s(%d):" fmt, LOG_TAG(hid), (uid_obj)->uid, __func__, __LINE__, ##args)

#define npu_utrace(fmt, uid_obj, hid, args...) \
	npu_trace_target("%s:[T][U%d]%s(%d):" fmt, LOG_TAG(hid), (uid_obj)->uid, __func__, __LINE__, ##args)

/* Printout unique ID and frame ID */
#define npu_uferr(fmt, obj_ptr, hid, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_err_target("%s:[ERR][U%u][F%u]%s(%d):" fmt, LOG_TAG(hid), (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufwarn(fmt, obj_ptr, hid, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_warn_target("%s:[WRN][U%u][F%u]%s(%d):" fmt, LOG_TAG(hid), (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufinfo(fmt, obj_ptr, hid, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_info_target("%s:[INFO][U%u][F%u]%s(%d):" fmt, LOG_TAG(hid), (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufnotice(fmt, obj_ptr, hid, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_notice_target("%s:[NOTICE][U%u][F%u]%s(%d):" fmt, LOG_TAG(hid), \
							(__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_ufdbg(fmt, obj_ptr, hid, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_dbg_target("%s:[DBG][U%u][F%u]%s(%d):" fmt, LOG_TAG(hid), (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

#define npu_uftrace(fmt, obj_ptr, hid, args...) \
({	typeof(*obj_ptr) *__pt = (obj_ptr); npu_trace_target("%s:[T][U%u][F%u]%s(%d):" fmt, LOG_TAG(hid), (__pt)->uid, (__pt)->frame_id, __func__, __LINE__, ##args); })

/* Exported functions */
int npu_log_probe(struct npu_device *npu_device);
int npu_log_release(struct npu_device *npu_device);
int npu_log_open(struct npu_device *npu_device);
int npu_log_close(struct npu_device *npu_device);
int fw_will_note(size_t len);
int fw_will_note_to_kernel(size_t len);

#if IS_ENABLED(CONFIG_SOC_S5E9955)
int npu_log_fw_governor_probe(struct npu_device *device);
int npu_log_fw_governor(struct npu_device *device);
#endif

extern const struct npu_log_ops npu_log_ops;

#endif /* _NPU_LOG_H_ */
