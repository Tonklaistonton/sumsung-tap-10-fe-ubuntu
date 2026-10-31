/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_VH_H_
#define _HTS_VH_H_

#include "hts.h"

int hts_vh_register_tick(struct hts_drvdata *drvdata);
int hts_vh_unregister_tick(struct hts_drvdata *drvdata);

int hts_vh_register(struct hts_drvdata *drvdata);

#endif /* _HTS_VH_H_ */
