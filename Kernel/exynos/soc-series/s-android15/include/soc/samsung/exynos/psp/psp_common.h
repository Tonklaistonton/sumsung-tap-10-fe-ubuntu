/*
 * psp_common.h - Samsung Psp Mailbox common definition
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PSP_COMMON_H__
#define __PSP_COMMON_H__

#include <linux/io.h>

/*****************************************************************************/
/* Common flag								     */
/*****************************************************************************/
/* define logical true/false */
#define TRUE				(1)
#define FALSE				(0)
#define true				(TRUE)
#define false				(FALSE)
/* define logical on/off */
#define ON				(1)
#define OFF				(0)

/*****************************************************************************/
/* Common value								     */
/*****************************************************************************/
#define WORD_SIZE			(4)

/*****************************************************************************/
/* Define macros for SFR						     */
/*****************************************************************************/
#define read_sfr(_addr_) \
		(*(volatile unsigned int *)((unsigned long)(_addr_)))

#define write_sfr(_addr_, _val_) \
		__raw_writel(_val_, (void *)_addr_)

#define psp_err(dev, fmt, arg...)       printk("[EXYNOS][PSP][ERROR] " fmt, ##arg)
#define psp_info(dev, fmt, arg...)      printk("[EXYNOS][PSP][ INFO] " fmt, ##arg)

#endif /* __PSP_COMMON_H__ */
