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

#include "npu-log.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-scheduler.h"

#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION)
int npu_cam_notify_on_off(bool is_on){
	struct npu_scheduler_info *info;
	struct npu_device *device;
	struct npu_scheduler_dvfs_info *d;

	info = npu_scheduler_get_info();
	if (!info) {
		npu_err("no device for scheduler\n");
		return -1;
	}

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		return -1;
	}

	device = info->device;

	if (is_on) {
		npu_info("cam on\n");
		if (atomic_inc_return(&device->cam_on_count) == 1) {
			mutex_lock(&info->exec_lock);
			list_for_each_entry(d, &info->ip_list, ip_list) {
				if (!strcmp("DSP", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max_cam_noti, DSP_SHARED_PLL_CLK);
					npu_dump("DSP Maxlock %d\n", DSP_SHARED_PLL_CLK);
				}
			}
			mutex_unlock(&info->exec_lock);
		}
	} else {
		npu_info("cam off\n");
		if (atomic_dec_return(&device->cam_on_count) == 0) {
			mutex_lock(&info->exec_lock);
			list_for_each_entry(d, &info->ip_list, ip_list) {
				if (!strcmp("DSP", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max_cam_noti, d->max_freq);
					npu_dump("DSP Maxlock %d\n", d->max_freq);
				}
			}
			mutex_unlock(&info->exec_lock);
		}
	}

	return 0;
}
#else
int npu_cam_notify_on_off(bool is_on){
	return 0;
}
#endif
EXPORT_SYMBOL(npu_cam_notify_on_off);
