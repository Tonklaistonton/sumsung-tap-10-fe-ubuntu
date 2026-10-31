/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/platform_device.h>
#include <ufs/ufshcd.h>

#include "ufs-vs-mmio.h"

int exynos_ufs_init_dbg(struct ufs_vs_handle *, struct device_node *np);
int exynos_ufs_dbg_set_lanes(struct ufs_vs_handle *,
				struct device *dev, u32);
void exynos_ufs_dump_info(struct ufs_hba *, struct ufs_vs_handle *,
			  struct device *dev);
void exynos_ufs_cmd_log_start(struct ufs_vs_handle *,
				struct ufs_hba *, struct scsi_cmnd *, u32);
void exynos_ufs_cmd_log_end(struct ufs_vs_handle *,
				struct ufs_hba *hba, int tag);
int exynos_ufs_init_mem_log(struct platform_device *pdev);
bool exynos_ufs_check_error(struct ufs_vs_handle *handle);
