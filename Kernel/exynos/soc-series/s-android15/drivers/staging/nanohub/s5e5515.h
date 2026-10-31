/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Qigan Chen <qigan.chen@samsung.com>
 *
 */
#ifndef _S5E5515_H_
#define _S5E5515_H_

#undef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX     (4096)
#define LOGBUF_NUM          230
#define CIPC_START_OFFSET   21504

#define CHUB_SRAM_SIZE      SZ_1M
#undef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX     (4096)
#define SRAM_CM55_OFFSET    (0x100000)
#define YAMIN_SECURE        (0x1000)
#define YAMIN_NONSECURE     (0x1004)

/* operation state of lpd */
#define LPD_END_WAIT_TIME   250
#define SRAM_OFFSET_OS      SZ_4K
#define ENABLE_NANOHUB_CAL_CHUB_ON
#endif /* _S5E5515_H_ */
