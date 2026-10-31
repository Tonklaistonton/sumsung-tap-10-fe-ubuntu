/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CUSTOS_LOGSINK_H__
#define __CUSTOS_LOGSINK_H__

#include "utils.h"

#define PM_DUMP_OFFSET 0x1FF000
#define PM_DUMP_SIZE 0x1000
#define LOG_OFFSET 0x200000
#define LOG_SIZE 0x40000
#if IS_ENABLED(CONFIG_SACPM_DUMP)
#define SACPM_DUMP_DRAM_BASE	(0x900F4000)
#define SACPM_DUMP_SIZE		(0x200)
#endif

int custos_memlog_register(void);
int custos_log_init(struct custos_device *cust_st);
void custos_log_work(void);
void custos_log_flush(struct work_struct *work);

#endif
