/*
 * exynos-wow.h - Exynos Workload Watcher Driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *  Hanjun Shin <hanjun.shin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __EXYNOS_WOW_PRIVATE_H_
#define __EXYNOS_WOW_PRIVATE_H_

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/threads.h>
#include <linux/slab.h>
#include <uapi/linux/sched/types.h>
#include <linux/workqueue.h>
#include <soc/samsung/exynos-wow.h>

#define PPC_NAME_LENGTH	16

/* PPMU & WoW commen registers */
#define WOW_PPC_PMNC	0x4
#define WOW_PPC_CNTENS	0x8
#define WOW_PPC_CNTENC	0xC
#define WOW_PPC_INTENS	0x10
#define WOW_PPC_INENC	0x14
#define WOW_PPC_PMNC_GLB_CNT_EN 0x1
#define WOW_PPC_PMNC_RESET_CCNT_PMCNT	(0x2 | 0x4)
#define WOW_PPC_PMNC_Q_CH_MODE	(0x1 << 24)

/* WoW driver native WoW IP */
#define WOW_PPC_CCNT		0x48
#define WOW_PPC_PMCNT0		0x34
#define WOW_PPC_PMCNT1		0x38
#define WOW_PPC_PMCNT2		0x3C
#define WOW_PPC_PMCNT3		0x40
#define WOW_PPC_PMCNT3_HIGH	0x44

#define WOW_PPC_CNTENS_CCNT_OFFSET	31
#define WOW_PPC_CNTENS_PMCNT0_OFFSET	0
#define WOW_PPC_CNTENS_PMCNT1_OFFSET	1
#define WOW_PPC_CNTENS_PMCNT2_OFFSET	2
#define WOW_PPC_CNTENS_PMCNT3_OFFSET	3

#define WOW_PPC_CNTENC_CCNT_OFFSET	31
#define WOW_PPC_CNTENC_PMCNT0_OFFSET	0
#define WOW_PPC_CNTENC_PMCNT1_OFFSET	1
#define WOW_PPC_CNTENC_PMCNT2_OFFSET	2
#define WOW_PPC_CNTENC_PMCNT3_OFFSET	3

#define NUM_WOW_EVENT	5
#define MAX_PPC		4

#define WOW_POLLING_MS_MAX 1024
#define WOW_POLLING_MS_MIN 4

enum exynos_wow_event_status {
	WOW_PPC_INIT,
	WOW_PPC_START,
	WOW_PPC_STOP,
	WOW_PPC_RESET,
};

enum exynos_wow_mode {
	WOW_DISABLED,
	WOW_ENABLED,
};

struct exynos_wow_ip_data {
	void __iomem **wow_base;
	char ppc_name[PPC_NAME_LENGTH];
	u32 nr_base;
	u32 nr_ppc;
	int ip_type;

	u32 enable;
	u32 bus_width;
	u32 nr_info;
	u32 trace_on;
};

struct exynos_wow_data {
	struct device	*dev;
	struct exynos_wow_ip_data ip_data[NR_MASTERS];
	struct delayed_work dwork;
	unsigned int polling_delay;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
	struct mutex lock;
#else
	struct exynos_wow_profile profile;
	spinlock_t lock;
#endif

	bool mode;

	unsigned int ipc_ch;
	void __iomem *dump_base;

	void __iomem *ts_base;
	u32 ts_offset;
	u32 ts_nr_levels;
};

struct exynos_wow_buf {
	u64 ccnt_osc;
	u64 ccnt;
	u64 active;
	u64 transfer_data;
	u64 nr_requests;
	u64 mo_count;
};

enum exynos_wow_ipc_msg {
	WOW_SET_START,
	WOW_SET_STOP,
	WOW_GET_ADDR_META,
	WOW_GET_ADDR_TS,
	WOW_GET_DATA,
	WOW_SET_FREQCHG,
};

struct exynos_wow_ipc_request {
	u8 msg;		/* LSB */
	u8 req_0;
	u16 fw_use;	/* MSB */
	u32 req_1;
	u32 req_2;
	u32 req_3;
};

struct exynos_wow_ipc_response {
	u8 msg;		/* LSB */
	u8 resp_0;
	u16 fw_use;	/* MSB */
	u32 resp_1;
	u32 resp_2;
	u32 resp_3;
};

union exynos_wow_ipc_message {
	u32 data[4];
	struct exynos_wow_ipc_request req;
	struct exynos_wow_ipc_response resp;
};
#endif	/* __EXYNOS_WOW_H_ */
