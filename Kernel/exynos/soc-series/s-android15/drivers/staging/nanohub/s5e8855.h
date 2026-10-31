/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Huiling Wu <huiling.wu@samsung.com>
 *
 */
#ifndef _S5E8855_H_
#define _S5E8855_H_

#undef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX         (780)
#define LOGBUF_NUM              230
#define CIPC_START_OFFSET       21504
#define CHUB_CLK_BASE           393216000

#define CHUB_CPU_CONFIGURATION                  (0x11863580) //PMU
#define REG_CHUB_CPU_STATUS                     (0x0)
#define REG_CHUB_CPU_OPTION                     (0x0)
#define ENABLE_SYSRESETREQ                      BIT(0)
#define CHUB_RESET_RELEASE_VALUE                (0x0)

#define REG_MAILBOX_ISSR0 (0x100)
#define REG_MAILBOX_ISSR1 (0x104)
#define REG_MAILBOX_ISSR2 (0x108)
#define REG_MAILBOX_ISSR3 (0x10c)

#define BAAW_MAX (0)
#define BAAW_MAX_WINDOWS (0)
#define BARAC_MAX (1)
#define BARAC_MAX_WINDOWS (7)

//#define CONFIG_CONTEXTHUB_SENSOR_DEBUG

#endif /* _S5E8855_H_ */
