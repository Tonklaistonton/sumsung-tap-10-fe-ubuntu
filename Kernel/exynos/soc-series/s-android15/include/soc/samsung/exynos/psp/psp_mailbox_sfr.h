/*
 * psp_mailbox_sfr.h - Samsung Psp Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PSP_MAILBOX_SFR_H__
#define __PSP_MAILBOX_SFR_H__

/*****************************************************************************/
/* INTERRUPT                                                                 */
/*****************************************************************************/
#define IWC_DATA			(1 << 0)
#define IWC_SUSPEND			(1 << 1)
#define IWC_RESUME			(1 << 2)
#define IWC_DEBUG			(1 << 3)

/*****************************************************************************/
/* IPC									     */
/*****************************************************************************/

#define PSP_MAX_MB_DATA_LEN		(((64 - 4) / 2) * WORD_SIZE)

/*****************************************************************************/
/* Mailbox                                                                   */
/*****************************************************************************/
#if defined(CONFIG_SOC_S5E9955)
#define PSP_MB_BASE			(0x179E0000) /* MAILBOX_AP_NS_SS_PSP */
#elif defined(CONFIG_SOC_S5E8855)
#define PSP_MB_BASE			(0x13DE0000) /* MAILBOX_AP_NS_SS_PSP */
#else
#define PSP_MB_BASE			(0) /* Fault */
#endif

#define PSP_MB_INTGR0_OFFSET		(0x08)
#define PSP_MB_INTCR0_OFFSET		(0x0C)
#define PSP_MB_INTMR0_OFFSET		(0x10)
#define PSP_MB_INTSR0_OFFSET		(0x14)
#define PSP_MB_INTGR1_OFFSET		(0x1C)
#define PSP_MB_INTCR1_OFFSET		(0x20)
#define PSP_MB_INTMR1_OFFSET		(0x24)
#define PSP_MB_INTSR1_OFFSET		(0x28)

#define PSP_MB_INTGR0_ON		(1 << 16)
#define PSP_MB_INTGR0_DBG_ON		(1 << 17)
#define PSP_MB_INTGR0_LOG_ON		(1 << 18)
#define PSP_MB_INTGR1_ON		(1 << 0)

/* REE's CMD */
#define PSP_MB_REE_READ_OFFSET		(0x100)
#define PSP_MB_REE_LEN_OFFSET		(0x104)

/* SEE's CMD */
#define PSP_MB_SEE_READ_OFFSET		(0x108)
#define PSP_MB_SEE_LEN_OFFSET		(0x10C)

#define DATA_LEN_MASK			(0xFF000000)
#define TOTAL_LEN_MASK			(0x00FFFFFF)
#define CMD_MASK			(0xFF000000)
#define RSVD_MASK			(0x00FF0000)
#define RETURN_MASK			(0x0000FFFF)
#define REMAIN_LEN_MASK			(0xFFFF0000)
#define REMAIN_SHFIT			(16)

#define PSP_MB_DATA_00_OFFSET		(0x110)
#define PSP_MB_DATA_00_OFFSET_R2S	(PSP_MB_DATA_00_OFFSET)
#define PSP_MB_DATA_00_OFFSET_S2R	(PSP_MB_DATA_00_OFFSET + PSP_MAX_MB_DATA_LEN)

/* REE debug sfr */
#define PSP_MB_DATA_DEBUG_START_OFFSET  (PSP_MB_DATA_00_OFFSET_R2S + PSP_MAX_MB_DATA_LEN - 4) /* ~ 0x200 */
#define PSP_MB_DATA_DEBUG_END_OFFSET    (PSP_MB_DATA_00_OFFSET_R2S) /* ~ 0x200 */
#endif /* __PSP_MAILBOX_SFR_H__ */
