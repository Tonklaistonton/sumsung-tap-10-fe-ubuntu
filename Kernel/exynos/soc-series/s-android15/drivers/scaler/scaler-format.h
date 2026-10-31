/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#ifndef SCALER_FORMAT_H_
#define SCALER_FORMAT_H_

#include "scaler.h"

/* Find the matches format */
const struct sc_fmt *sc_find_format(unsigned int pixfmt);
const struct sc_fmt *sc_get_default_format(void);

void sc_get_bytesperline(u32 pixelformat, u32 spansize, u32 *bytesperline);

#endif /* SCALER_FORMAT_H_ */
