/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_AFM_H_
#define _NPU_AFM_H_
#ifdef CONFIG_NPU_AFM

#define BUCK_CNT		1
#define GNPU0			(0x0)
#define NPU0			(0x0)
#define NPU1			(0x1)
#define NPU_AFM_ENABLE		(0x1)
#define NPU_AFM_DISABLE		(0x0)
#define	NPU_AFM_LIMIT_SET	(0x1)
#define	NPU_AFM_LIMIT_REL	(0x0)
#define	NPU_AFM_OCP_WARN	(0x1)
#define	NPU_AFM_OCP_NONE	(0x0)
#define	NPU_AFM_STATE_OPEN	(0x0)
#define	NPU_AFM_STATE_CLOSE	(0x1)
#define NPU_AFM_FREQ_SHIFT	(0x1000)
#define NPU_AFM_INC_FREQ	(0x1533)
#define NPU_AFM_DEC_FREQ	(0x2135)

#define NPU_AFM_TDC_THRESHOLD	(0x100)
#define NPU_AFM_TDT		(0x10)	// 32 cnt
#define NPU_AFM_RESTORE_MSEC	(0x1F4)	// 500 msec

#define S2MPS_AFM_WARN_EN		(0x80)
#define S2MPS_AFM_WARN_DEFAULT_LVL	(0x00)
#define S2MPS_AFM_WARN_LVL_MASK		(0x7F)

#define s2se911_4_regulators (4)
#define S2SE911_4_REG s2se911_4_regulators

#define NPU_AFM_STR_LEN (64)

enum {
	NPU_AFM_MODE_NORMAL = 0,
	NPU_AFM_MODE_TDC,
};

struct npu_afm {
	bool afm_local_onoff;
	u32 npu_afm_irp_debug;
	u32 npu_afm_tdc_threshold;  // Throttling Duration Counter
	u32 npu_afm_restore_msec;
	u32 npu_afm_tdt;     // Throttling Duration Threshold
	u32 npu_afm_tdt_debug;

	struct npu_system *system;
	struct task_struct *kthread;
	struct workqueue_struct		*afm_gnpu0_wq;
	struct delayed_work		afm_gnpu0_work;
	struct workqueue_struct		*afm_restore_gnpu0_wq;
	struct delayed_work		afm_restore_gnpu0_work;
	u32 afm_max_lock_level[BUCK_CNT];
	u32 afm_buck_threshold_enable_offset[BUCK_CNT];
	u32 afm_buck_threshold_level_offset[BUCK_CNT];
	u32 afm_buck_threshold_level_value[BUCK_CNT];
	atomic_t ocp_warn_status[BUCK_CNT];
	atomic_t restore_status[BUCK_CNT];
	atomic_t afm_state;
	u32 afm_mode;

	const struct npu_afm_ops *afm_ops;
};

struct npu_afm_ops {
	void (*afm_open_setting_sfr)(struct npu_system *system, bool power_on);
	void (*afm_close_setting_sfr)(struct npu_system *system, bool power_on);
};

struct npu_afm_tdc {
       u32 gnpu0_cnt;
};
void npu_afm_open(struct npu_system *system, int hid);
void npu_afm_close(struct npu_system *system, int hid);
int npu_afm_probe(struct npu_device *device);
int npu_afm_release(struct npu_device *device);
#else	/* CONFIG_NPU_AFM is not defined */
#define npu_afm_open(p, id)	(0)
#define npu_afm_close(p, id)	(0)
#define npu_afm_probe(p)	(0)
#define npu_afm_release(p)	(0)
#endif	/* CONFIG_NPU_AFM */

#endif	/* _NPU_AFM_H */
