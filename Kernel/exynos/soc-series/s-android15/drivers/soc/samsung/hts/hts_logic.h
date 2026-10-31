/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_LOGIC_H_
#define _HTS_LOGIC_H_

#include "hts.h"

void hts_logic_read_task_data(struct hts_drvdata *drvdata, struct task_struct *task);
void hts_logic_reflect_task_data(struct hts_drvdata *drvdata, int cpu, struct task_struct *task);

#endif /* _HTS_LOGIC_H_ */
