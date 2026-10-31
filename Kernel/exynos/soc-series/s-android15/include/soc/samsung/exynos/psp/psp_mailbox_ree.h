/*
 * psp_mailbox_ree.h - Samsung Psp Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PSP_MAILBOX_REE_H__
#define __PSP_MAILBOX_REE_H__

#include <linux/miscdevice.h>
#include "psp_mailbox_sfr.h"

typedef struct {
	void *(*mem_allocator)(ssize_t len);
	int (*msg_receiver)(void *buf, unsigned int len, unsigned int status);
	void (*msg_mlog_write)(void);
	void (*msg_log_work)(void);
	uint8_t mem_allocator_flag;
	uint8_t msg_receiver_flag;
	uint8_t msg_mlog_write_flag;
	uint8_t msg_log_work_flag;
} custos_func_t;

/* debug define */
enum MB_DBG_INDEX {
	MB_DBG_CNT, MB_DBG_R0, MB_DBG_R1, MB_DBG_R2, MB_DBG_R3,
	MB_DBG_IP, MB_DBG_ST, MB_DBG_LR, MB_DBG_PC, MB_DBG_XPSR,
	MB_DBG_MSP, MB_DBG_PSP, MB_DBG_CFSR, MB_DBG_FAR,
	MB_DBG_MSG, MB_DBG_MAX_INDEX = (PSP_MAX_MB_DATA_LEN / 4 - 1)
};
#define MB_DBG_MSG_MAX_SIZE ((MB_DBG_MAX_INDEX - MB_DBG_MSG) * 4)

extern void __iomem *mb_va_base;
/*****************************************************************************/
/* Function prototype							     */
/*****************************************************************************/
uint32_t exynos_psp_mailbox_map_sfr(void);
uint32_t mailbox_receive_and_callback(uint8_t *buf, uint32_t* rx_total_len, uint32_t *rx_remain_len);
uint32_t mailbox_receive_flush_log(void);

/* Export symbols */
void mailbox_register_allocator(void *(*fp)(ssize_t));
void mailbox_register_message_receiver(int (*fp)(void *, unsigned int, unsigned int));
void mailbox_register_message_mlog_writer(void (*fp)(void));
void mailbox_register_message_log_worker(void (*fp)(void));

void mailbox_fault_and_callback(void);

int exynos_ssp_notify_ap_sleep(void);
int exynos_ssp_notify_ap_wakeup(void);

#endif /* __PSP_MAILBOX_REE_H__ */
