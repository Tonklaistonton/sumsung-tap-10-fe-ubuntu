/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIFPMUMAN_H
#define __MIFPMUMAN_H

#include <linux/mutex.h>

struct scsc_mif_abs;
struct mifpmuman;

/** PMU Interrupt Bit Handler prototype. */
#define MIFPMU_ERROR_WLAN		0
#define MIFPMU_ERROR_WPAN		1
#define MIFPMU_ERROR_WLAN_WPAN		2

#if defined(CONFIG_WLBT_SPLIT_RECOVERY)
#define MIFPMU_RECOVER_WLAN		3
#define MIFPMU_RECOVER_WPAN		4
int mifpmuman_recovery_mode_subsystem(struct mifpmuman *pmu, bool enable);
#endif

#ifdef CONFIG_SCSC_XO_CDAC_CON
int mifpmuman_send_rfic_dcxo_config(struct mifpmuman *pmu);
#endif

typedef void (*mifpmuisr_handler)(void *data, u32 cmd);
int mifpmuman_start_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
int mifpmuman_stop_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
int mifpmuman_force_monitor_mode_subsystem(struct mifpmuman *pmu, enum scsc_subsystem sub);
int mifpmuman_trigger_scan2mem(struct mifpmuman *pmu, uint32_t enable_scan2mem_dump, bool dump);

#if IS_ENABLED(CONFIG_SCSC_PMU_BOOTFLAGS)
int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch,
		      size_t ka_patch_len, u32 flags);
#else
int mifpmuman_load_fw(struct mifpmuman *pmu, int *ka_patch,
		      size_t ka_patch_len);
#endif

/* Inclusion in core.c treat it as opaque */
struct mifpmuman {
	void (*pmu_irq_handler)(void *data, u32 cmd);
	u32 last_msg;
	void *irq_data;
	bool in_use;
	struct scsc_mif_abs *mif;
	struct mutex lock;
	struct completion msg_ack;
};

int mifpmuman_init(struct mifpmuman *pmu, struct scsc_mif_abs *mif,
		   mifpmuisr_handler handler, void *irq_data);
int mifpmuman_deinit(struct mifpmuman *pmu);
#endif
