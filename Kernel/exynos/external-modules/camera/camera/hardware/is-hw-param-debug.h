/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_PARAM_DEBUG_H
#define IS_PARAM_DEBUG_H

#include "is-param.h"

#define WRITTEN(total, remain) (((total) > (remain)) ? ((total) - (remain)) : (total))

char *dump_param_control(char *buf, const char *name, struct param_control *param, size_t *rem);
char *dump_param_sensor_config(
	char *buf, const char *name, struct param_sensor_config *param, size_t *rem);
char *dump_param_otf_input(char *buf, const char *name, struct param_otf_input *param, size_t *rem);
char *dump_param_otf_output(
	char *buf, const char *name, struct param_otf_output *param, size_t *rem);
char *dump_param_dma_input(char *buf, const char *name, struct param_dma_input *param, size_t *rem);
char *dump_param_dma_output(
	char *buf, const char *name, struct param_dma_output *param, size_t *rem);
char *dump_param_stripe_input(
	char *buf, const char *name, struct param_stripe_input *param, size_t *rem);
char *dump_param_mcs_input(char *buf, const char *name, struct param_mcs_input *param, size_t *rem);
char *dump_param_mcs_output(
	char *buf, const char *name, struct param_mcs_output *param, size_t *rem);
char *dump_param_hw_ip(char *buf, const char *name, const u32 id, size_t *rem);

#endif /* IS_PARAM_DEBUG_H */
