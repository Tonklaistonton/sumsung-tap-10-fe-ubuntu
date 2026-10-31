// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#ifndef __MMC_SEC_FEATURE_H__
#define __MMC_SEC_FEATURE_H__

#include <linux/platform_device.h>
#include <linux/mmc/core.h>

#include "mmc-sec-sysfs.h"

extern struct device *sec_sdcard_cmd_dev;

struct mmc_sd_sec_device_info {
	struct mmc_host *mmc;
	struct dw_mci *host;
	unsigned int card_detect_cnt;
	unsigned long tstamp_last_cmd;
	struct work_struct noti_work;
	struct sd_sec_err_info err_info[MAX_LOG_INDEX];
	struct sd_sec_status_err_info status_err;
	struct sd_sec_err_info saved_err_info[MAX_LOG_INDEX];
	struct sd_sec_status_err_info saved_status_err;
	int sd_slot_type;
	bool support;

	const struct mmc_sec_variant_ops *vops;
};

struct mmc_sec_variant_ops {
	int (*mmc_create_sysfs_dev)(struct dw_mci *host, const char *group_name);
};

void mmc_sec_register_callback(const struct mmc_sec_variant_ops *vops);
void mmc_sd_sec_register_vendor_hooks(void);
void sd_sec_set_features(struct platform_device *pdev);
void sd_sec_detect_interrupt(struct dw_mci *host);
void sd_sec_check_req_err(struct dw_mci *host, struct mmc_request *mrq);

static inline int sec_sd_vops_create_sysfs_dev(struct mmc_sd_sec_device_info *sdi,
	const char *group_name)
{
	if (sdi && sdi->vops && sdi->vops->mmc_create_sysfs_dev)
		return sdi->vops->mmc_create_sysfs_dev(sdi->host, group_name);

	return -ENODEV;
}

#endif
