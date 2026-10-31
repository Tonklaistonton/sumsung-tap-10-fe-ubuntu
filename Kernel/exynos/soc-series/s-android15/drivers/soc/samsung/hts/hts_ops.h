/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_OPS_H_
#define _HTS_OPS_H_

#define SYS_ECTLR				(sys_reg(3, 0, 15, 1, 4))
#define SYS_ECTLR2				(sys_reg(3, 0, 15, 1, 5))

void hts_read_ectlr(void *data);
void hts_read_ectlr2(void *data);
void hts_write_ectlr(void *data);
void hts_write_ectlr2(void *data);

#endif /* _HTS_OPS_H_ */
