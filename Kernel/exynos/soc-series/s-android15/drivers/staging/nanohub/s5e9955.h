/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Hyangmin Bae <hyangmin.bae@samsung.com>
 *
 */
#ifndef _S5E9955_H_
#define _S5E9955_H_

#undef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX         (4096)
#define LOGBUF_NUM              230
#define CIPC_START_OFFSET       21504
#define CHUB_CLK_BASE           393216000

#define SIZE_OF_BUFFER (SZ_1M - SZ_128)

#define CHUB_CPU_CONFIGURATION  (0x13863780) //PMU_ALIVE

#define NANOHUB_CPU_CM55

#define REG_MAILBOX_ISSR0 (0x100)
#define REG_MAILBOX_ISSR1 (0x104)
#define REG_MAILBOX_ISSR2 (0x108)
#define REG_MAILBOX_ISSR3 (0x10c)

#define BAAW_MAX (0)
#define BAAW_MAX_WINDOWS (0)
#define BARAC_MAX (2)
#define BARAC_MAX_WINDOWS (0x10)

#define CONFIG_CONTEXTHUB_SENSOR_DEBUG

#endif /* _S5E9955_H_ */
