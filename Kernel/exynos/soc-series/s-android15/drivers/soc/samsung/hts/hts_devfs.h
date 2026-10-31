/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_DEVFS_H_
#define _HTS_DEVFS_H_

#include "hts.h"
#include "hts_pmu.h"

#define DEFAULT_EVAL_WORKQUEUE_TICK_MS		(1000)

struct hts_fops_private_data {
	struct hts_pmu_handle	*pmu_handle;
};

void hts_devfs_control_tick(struct hts_drvdata *drvdata, int enable);
int hts_devfs_initialize(struct hts_drvdata *drvdata);

#endif /* _HTS_DEVFS_H_ */
