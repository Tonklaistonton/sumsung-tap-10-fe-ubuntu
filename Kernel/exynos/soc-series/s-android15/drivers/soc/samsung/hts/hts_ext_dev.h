/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_EXT_DEV_H_
#define _HTS_EXT_DEV_H_

#include "hts.h"

#if defined(CONFIG_HTS_EXT)

int hts_ext_dev_initialize(struct hts_drvdata *drvdata);
void hts_ext_dev_uninitialize(struct hts_drvdata *drvdata);

#else

static inline int hts_ext_dev_initialize(struct hts_drvdata *drvdata)
{
	return 0;
}
void hts_ext_dev_uninitialize(struct hts_drvdata *drvdata)
{
}

#endif

#endif /* _HTS_EXT_DEV_H_ */
