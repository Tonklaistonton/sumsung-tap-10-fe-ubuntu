/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_ops.h"

#include <asm/sysreg.h>

void hts_read_ectlr(void *data)
{
	*(unsigned long *)data = read_sysreg_s(SYS_ECTLR);
}

void hts_read_ectlr2(void *data)
{
	*(unsigned long *)data = read_sysreg_s(SYS_ECTLR2);
}

void hts_write_ectlr(void *data)
{
	write_sysreg_s(*(unsigned long *)data, SYS_ECTLR);
}

void hts_write_ectlr2(void *data)
{
	write_sysreg_s(*(unsigned long *)data, SYS_ECTLR2);
}
