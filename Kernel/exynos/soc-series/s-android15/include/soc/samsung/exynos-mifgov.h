/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as publised
 * by the Free Software Foundation.
 *
 * Header for MIFGOV
 *
 */

#ifndef __MIFGOV_H__
#define __MIFGOV_H__

#include <linux/of.h>
#include <linux/platform_device.h>
#if defined(CONFIG_EXYNOS_ACPM) || defined(CONFIG_EXYNOS_ACPM_MODULE)
#include <soc/samsung/acpm_ipc_ctrl.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif

#define NUM_USER	10
#define IP_NAME_LEN	10
#define SKIP_LOG_IP	"LEALT"

#define IPC_GET		0
#define IPC_SET		1
#define MIFGOV_DBG_LOG(x...)	if (mifgov_dbg_log) dev_notice(x)

static bool mifgov_dbg_log = false;

enum mifgov_event_index {
	MIFGOV_INFO = 0,
	MIFGOV_BW_PARAMS,
	MIFGOV_GOV_PARAMS,
	MIFGOV_RUN,
	MIFGOV_SEND_BW,
	MIFGOV_DBGLOG_EN,
};

struct mifgov_ipc_request {
	u8 msg;		/* LSB */
	u8 domain;
	u16 fw_use;	/* MSB */
	u32 resp_rsvd0;
	u32 resp_rsvd1;
	u32 resp_rsvd2;
};

struct mifgov_ipc_bw_params {
	u8 msg;		/* LSB */
	u8 get;
	u16 fw_use;	/* MSB */
	u32 num_channel;
	u32 bus_width;
	u32 util;
};

struct mifgov_ipc_update_info {
	u8 msg;		/* LSB */
	u8 domain;	//ip
	u16 fw_use;	/* MSB */
	u32 resp_rsvd0;
	u32 resp_rsvd1;
	u32 resp_rsvd2;
};

struct mifgov_ipc_send_bw {
	u8 msg;		/* LSB */
	u8 domain;	//ip
	u16 fw_use;	/* MSB */
	u32 peak_bw;
	u32 read_bw;
	u32 write_bw;
};

struct mifgov_ipc_gov_params {
	u8 msg;         /* LSB */
	u8 get;
	u16 fw_use;     /* MSB */
	u32 hold_time;
	u32 bratio;
	u32 period;
};

struct mifgov_ipc_run {
	u8 msg;         /* LSB */
	u8 get;
	u16 fw_use;     /* MSB */
	u32 run;
	u32 resp_rsvd0;
	u32 resp_rsvd1;
};

struct mifgov_ipc_dbg_log {
	u8 msg;         /* LSB */
	u8 get;
	u16 fw_use;     /* MSB */
	u32 ipc_dbg_log_en;
	u32 resp_rsvd0;
	u32 resp_rsvd1;
};

union mifgov_ipc_message {
	unsigned int data[4];
	struct mifgov_ipc_request req;
	struct mifgov_ipc_update_info info;
	struct mifgov_ipc_bw_params bw_params;
	struct mifgov_ipc_send_bw send_bw;
	struct mifgov_ipc_gov_params gov_params;
	struct mifgov_ipc_run run; 
	struct mifgov_ipc_dbg_log dbg_log;
};

struct mifgov_bw_params {
	int	num_channel;
	int	mif_bus_width;
	int	bus_width;
	int	mif_util;
	int	int_util;
};

struct mifgov_gov_params {
	unsigned int hold_time;
	unsigned int bratio;
	unsigned int period;
};

struct mifgov_user_info {
	u32 listed;
	char name[IP_NAME_LEN];
	u32 use_cnt;
};

struct mifgov_data {
	struct device			*dev;
	spinlock_t			lock;
	unsigned int			ipc_ch_num;
	unsigned int			ipc_ch_size;
	unsigned int			ipc_noti_ch_num;
	unsigned int			ipc_noti_ch_size;
	bool				ipc_dbg_log_en;
	unsigned int			run;
	unsigned int			user_cnt;
	struct mifgov_bw_params		bw_params;
	struct mifgov_gov_params	gov_params;
	struct mifgov_user_info		user_info[NUM_USER];
};

#if IS_ENABLED(CONFIG_EXYNOS_MIFGOV)
extern void exynos_mifgov_run(u32 run, const char *name);
#else
static inline void exynos_mifgov_run(u32 run, const char *name)
{
	return;
}
#endif
#endif
