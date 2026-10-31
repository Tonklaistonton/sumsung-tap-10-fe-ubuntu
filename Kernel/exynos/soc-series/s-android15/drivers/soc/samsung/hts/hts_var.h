/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_VAR_H_
#define _HTS_VAR_H_

#include "hts.h"

#include <linux/cpumask.h>

#define CGROUP_BUFFER_LENGTH            (128)

int hts_var_get_mask_count(struct hts_devfs *devfs);
int hts_var_get_mask_index(struct hts_devfs *devfs, int cpu);
struct cpumask *hts_var_get_mask(struct hts_devfs *devfs, int index);
void hts_var_add_cpumask(struct hts_devfs *devfs, struct cpumask *mask);
void hts_var_add_mask(struct hts_devfs *devfs, unsigned long mask);
void hts_var_clear_mask(struct hts_devfs *devfs);
void hts_var_disable_mask(struct hts_devfs *devfs, int group_index, int knob_index);
void hts_var_enable_mask(struct hts_devfs *devfs, int group_index, int knob_index);

int hts_var_get_cgroup_count(struct hts_devfs *devfs);
int hts_var_get_cgroup(struct hts_devfs *devfs, int index);
int hts_var_get_cgroup_index(struct hts_devfs *devfs, int cgroup);
void hts_var_add_cgroup(struct hts_drvdata *drvdata, char *cgroup_name);
void hts_var_clear_cgroup(struct hts_devfs *devfs);

#endif /* _HTS_VAR_H_ */
