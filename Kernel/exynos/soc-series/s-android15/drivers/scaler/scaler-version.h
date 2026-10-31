/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#ifndef SCALER_VERSION_H_
#define SCALER_VERSION_H_

#include "scaler.h"

u32 sc_get_version(u32 hw_verion);
const struct sc_variant *sc_get_variant(u32 sc_version);

#endif /* SCALER_VERSION_H_ */
