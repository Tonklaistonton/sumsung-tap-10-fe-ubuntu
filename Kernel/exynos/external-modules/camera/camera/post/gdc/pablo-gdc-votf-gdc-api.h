/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos CAMERA-PP GDC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_GDC_VOTF_GDC_API_H_
#define CAMERAPP_GDC_VOTF_GDC_API_H_

#include <linux/types.h>

struct mfc_votf_info {
	int (*gdc_frame_ready)(unsigned long i_ino);
};

void gdc_set_device_for_votf(struct device *dev);
void gdc_out_votf_otf(void *gdc_dev, void *gdc_ctx);
#define gdc_clear_mfc_votf_ops(gdc_dev) do {} while (0)

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define KUNIT_EXPORT_SYMBOL(x) EXPORT_SYMBOL_GPL(x)
struct pkt_gdc_votf_ops {
	int (*device_run)(unsigned long i_ino);
	void (*out_votf_otf)(void *gdc_dev, void *gdc_ctx);
	int (*register_ready_frame_cb)(int (*gdc_ready_frame_cb)(unsigned long i_ino));
	void (*unregister_ready_frame_cb)(void);
};
struct pkt_gdc_votf_ops *pablo_kunit_get_gdc_votf_ops(void);
struct device *gdc_get_votf_device(void);
void gdc_set_votf_device(struct device *data);
#else
#define KUNIT_EXPORT_SYMBOL(x)
#endif

#endif /* CAMERAPP_GDC_VOTF_GDC_API_H_ */
