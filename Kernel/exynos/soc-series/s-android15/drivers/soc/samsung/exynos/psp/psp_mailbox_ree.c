/*
 * psp_mailbox_ree.c - Samsung Psp Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/samsung/exynos/psp/psp_mailbox_common.h>
#include <soc/samsung/exynos/psp/psp_mailbox_ree.h>
#include <soc/samsung/exynos/psp/psp_mailbox_sfr.h>

void __iomem *mb_va_base;
EXPORT_SYMBOL(mb_va_base);

custos_func_t cf = {
	.mem_allocator_flag = OFF,
	.msg_receiver_flag = OFF,
	.msg_mlog_write_flag = OFF,
	.msg_log_work_flag = OFF,
};
EXPORT_SYMBOL(cf);

uint32_t exynos_psp_mailbox_map_sfr(void)
{
	uint32_t ret = 0;

	mb_va_base = ioremap_np(PSP_MB_BASE, SZ_4K);
	if (!mb_va_base) {
		psp_err(0, "%s: fail to ioremap\n", __func__);
		ret = -1;
	}

	return ret;
}

uint32_t mailbox_receive_and_callback(uint8_t *receive_buf, uint32_t *rx_total_len, uint32_t *rx_remain_len)
{
	uint32_t ret = 0;
	uint32_t rx_data_len;

	/* Receive data */
	ret = mailbox_receive(receive_buf, &rx_data_len, rx_total_len, rx_remain_len);
	if (ret) {
		psp_err(0, "mailbox_receive [ret: 0x%X]\n", ret);
		psp_err(0, "rx_data_len: 0x%X\n", rx_data_len);
		psp_err(0, "rx_total_len: 0x%X\n", (uint32_t)*rx_total_len);
		psp_err(0, "rx_remain_len: 0x%X\n", (uint32_t)*rx_remain_len);
		goto exit;
	}

exit:
	ret = mailbox_receive_flush_log();
	if (ret)
		psp_err(0, "%s [ret: 0x%X]\n", __func__, ret);

#if !defined(CONFIG_CUSTOS_INTERRUPT)
	ret = ret | (*rx_remain_len << REMAIN_SHFIT);
#endif

	return ret;
}
EXPORT_SYMBOL(mailbox_receive_and_callback);

uint32_t mailbox_receive_flush_log(void)
{
	if (cf.msg_log_work_flag == ON)
		cf.msg_log_work();

	return 0;
}

void mailbox_register_allocator(void *(*fp)(ssize_t))
{
	cf.mem_allocator = fp;
	cf.mem_allocator_flag = ON;
	psp_info(0, "%s is called\n", __func__);
}
EXPORT_SYMBOL(mailbox_register_allocator);

void mailbox_register_message_receiver(int (*fp)(void *, unsigned int, unsigned int))
{
	cf.msg_receiver = fp;
	cf.msg_receiver_flag = ON;
	psp_info(0, "%s is called\n", __func__);
}
EXPORT_SYMBOL(mailbox_register_message_receiver);

void mailbox_register_message_mlog_writer(void (*fp)(void))
{
	cf.msg_mlog_write = fp;
	cf.msg_mlog_write_flag = ON;
	psp_info(0, "%s is called\n", __func__);
}
EXPORT_SYMBOL(mailbox_register_message_mlog_writer);

void mailbox_register_message_log_worker(void (*fp)(void))
{
	cf.msg_log_work = fp;
	cf.msg_log_work_flag = ON;
	psp_info(0, "%s is called\n", __func__);
}
EXPORT_SYMBOL(mailbox_register_message_log_worker);

void mailbox_fault_and_callback(void)
{
        psp_info(0, "%s occurs.\n", __func__);

	// print log & save it to storage
	mailbox_receive_flush_log();
}
