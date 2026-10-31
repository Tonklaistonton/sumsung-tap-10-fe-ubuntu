/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Qigan Chen <qigan.chen@samsung.com>
 *
 */
#ifndef _S5E5535_H_
#define _S5E5535_H_

#undef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX         (4096)
#define LOGBUF_NUM              230
#define CIPC_START_OFFSET       21504
#define CHUB_CLK_BASE           393216000

#define CHUB_CPU_CONFIGURATION  (0x12853540) //PMU

#define NANOHUB_CPU_CM55

#define REG_MAILBOX_ISSR0 (0x100)
#define REG_MAILBOX_ISSR1 (0x104)
#define REG_MAILBOX_ISSR2 (0x108)
#define REG_MAILBOX_ISSR3 (0x10c)

#define BAAW_MAX (4)
#define BAAW_MAX_WINDOWS (0x10)
#define BARAC_MAX (2)
#define BARAC_MAX_WINDOWS (0x10)

#define CONFIG_CONTEXTHUB_SENSOR_DEBUG

/* Indicate I/D cache feature is enabled on the CHUB side */
#define CACHE_SUPPORT

#endif /* _S5E5535_H_ */
