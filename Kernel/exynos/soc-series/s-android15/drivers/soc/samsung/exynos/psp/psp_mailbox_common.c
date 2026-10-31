/*
 * psp_mailbox_common.c - Samsung Psp Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <asm/barrier.h>
#include <linux/delay.h>
#include <soc/samsung/exynos/psp/psp_mailbox_common.h>
#include <soc/samsung/exynos/psp/psp_mailbox_sfr.h>

#define psp_err(dev, fmt, arg...)       printk("[EXYNOS][PSP][ERROR] " fmt, ##arg)
#define psp_info(dev, fmt, arg...)      printk("[EXYNOS][PSP][ INFO] " fmt, ##arg)
#define MB_TIMEOUT (500000)

extern void __iomem *mb_va_base;

void psp_print_debug_sfr(void)
{
	psp_err(0, "=============== Non-Secure Mailbox Status ===============\n");

	psp_err(0, "PSP_MB_REE_READ_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_REE_READ_OFFSET));
	psp_err(0, "PSP_MB_REE_LEN_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_REE_LEN_OFFSET));
	psp_err(0, "PSP_MB_SEE_READ_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_SEE_READ_OFFSET));
	psp_err(0, "PSP_MB_SEE_LEN_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_SEE_LEN_OFFSET));

	psp_err(0, "INTGR1_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTGR1_OFFSET));
	psp_err(0, "INTCR1_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTCR1_OFFSET));
	psp_err(0, "INTMR1_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTMR1_OFFSET));
	psp_err(0, "INTSR1_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTSR1_OFFSET));

	psp_err(0, "INTGR0_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTGR0_OFFSET));
	psp_err(0, "INTCR0_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTCR0_OFFSET));
	psp_err(0, "INTMR0_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTMR0_OFFSET));
	psp_err(0, "INTSR0_OFFSET is 0x%x \n", read_sfr(mb_va_base + PSP_MB_INTSR0_OFFSET));

	psp_err(0, "=============== Non-Secure Mailbox Status ===============\n");
}
EXPORT_SYMBOL(psp_print_debug_sfr);

static uint32_t check_pending(bool delay)
{
	int cnt = 0;

	do {
		if (!(read_sfr(mb_va_base + PSP_MB_INTSR1_OFFSET) & PSP_MB_INTGR1_ON))
			return 0;
	} while (cnt++ < MB_TIMEOUT);

	psp_print_debug_sfr();

	return RV_MB_TIMEOUT_INTERNAL;
}

static uint32_t busy_waiting(void)
{
	uint32_t reg = RV_FAIL;

	mb();
	write_sfr(mb_va_base + PSP_MB_INTGR1_OFFSET, PSP_MB_INTGR1_ON);
	mb();

	if (check_pending(1))
		return RV_MB_WORLD_WAIT2_TIMEOUT + RV_REE_OFFSET;

	reg = read_sfr(mb_va_base + PSP_MB_REE_READ_OFFSET);

	return reg & RETURN_MASK;
}

static uint32_t move_data_to_mailbox(uint64_t mbox_addr, uint8_t *data, uint32_t data_len)
{
	uint32_t ret = RV_FAIL;
	uint32_t i;
	uint8_t last_data[4];
	uint8_t *src;

	for (i = 0; i < data_len / WORD_SIZE; i++)
		write_sfr(mbox_addr + (WORD_SIZE * i), ((uint32_t *)data)[i]);

	if (data_len & 0x3) {
		src = (uint8_t *)((uint64_t)data + data_len - (data_len % 4));
		memcpy(last_data, src, data_len % 4);
		write_sfr(mbox_addr + (WORD_SIZE * i), ((uint32_t *)last_data)[0]);
	}

	ret = RV_SUCCESS;
	return ret;
}

uint32_t move_mailbox_to_buf(uint8_t *data, uint64_t mbox_addr, uint32_t data_len)
{
	uint32_t ret = RV_FAIL;
	uint32_t i;
	uint8_t last_data[4];
	uint8_t *dst;

	for (i = 0; i < data_len / WORD_SIZE; i++)
		((uint32_t *)data)[i] = read_sfr(mbox_addr + (WORD_SIZE * i));

	if (data_len & 0x3) {
		((uint32_t *)last_data)[0] = read_sfr(mbox_addr + (WORD_SIZE * i));
		dst = (uint8_t *)((uint64_t)data + data_len - (data_len % 4));
		memcpy(dst, last_data, data_len % 4);
	}

        ret = RV_SUCCESS;
        return ret;
}

uint32_t mailbox_send(uint8_t *tx_data, uint32_t tx_total_len, uint32_t *tx_len_out)
{
	uint32_t ret = RV_FAIL;
	uint32_t i = 0;
	uint32_t tx_data_len = 0;
	uint32_t tx_remain_len = 0;
	uint32_t tx_transmit_len = 0;

	if (tx_total_len == 0) {
		ret = RV_MB_WORLD_TX_EMPTY_LEN + RV_REE_OFFSET;
		goto out;
	}

	if (tx_total_len & DATA_LEN_MASK) {
		ret = RV_MB_WORLD_TX_TOTAL_LEN_OVERFLOW + RV_REE_OFFSET;
		goto out;
	}

	tx_remain_len = tx_total_len;

	/* tx mailbox data */
	i = 0;
	tx_transmit_len = 0;
	while (tx_remain_len > 0) {
		if(check_pending(0)) {
			ret = RV_MB_WORLD_WAIT1_TIMEOUT + RV_REE_OFFSET;
			break;
		}

		if (tx_remain_len >= PSP_MAX_MB_DATA_LEN)
			tx_data_len = PSP_MAX_MB_DATA_LEN;
		else
			tx_data_len = tx_remain_len;

		ret = move_data_to_mailbox((uint64_t)mb_va_base + PSP_MB_DATA_00_OFFSET_R2S,
				tx_data + (i * PSP_MAX_MB_DATA_LEN),
				tx_data_len);
		if (ret != RV_SUCCESS) {
			psp_print_debug_sfr();
			ret = RV_MB_WORLD_TX_COPY_TO_MAILBOX + RV_REE_OFFSET;
			goto out;
		}

		/*
		 * PSP_MB_REE_LEN SFR use separately
		 * 0xFF00_0000 = Data Length (1 Byte)
		 * 0x00FF_FFFF = Total Length (3 Bytes)
		 */

		tx_transmit_len = tx_data_len << STMB_LEN_BIT_OFFSET;
		tx_transmit_len = tx_transmit_len | tx_total_len;

		write_sfr(mb_va_base + PSP_MB_REE_LEN_OFFSET, tx_transmit_len);

		ret = busy_waiting();
		if (ret != RV_SUCCESS) {
			psp_err(0, "%s: not send: %d\n", __func__, ret);
			goto out;
		}

		tx_remain_len -= tx_data_len;
		i++;
	}

out:
	if (ret == RV_SUCCESS)
		*tx_len_out = (i - 1) * PSP_MAX_MB_DATA_LEN + tx_data_len;
	else
		*tx_len_out = i * PSP_MAX_MB_DATA_LEN;

	return ret;
}
EXPORT_SYMBOL(mailbox_send);

/**
 @brief Function PSP to get a (portion of long) message from mailbox
 @param message : Address for message buffer to receive
 @param message_len : Address to get the received message length of the message buffer (in byte)
 @param total_len : Address to get total message length to be received ultimately (in byte)
 @param remain_len : Address to get remaining message length be be recevied subsequently (in byte)
 @return 0: success, otherwise: error code
 @note : If sender (ex. REE) sends a long "total_len" message, this function can be called multiple times in the interrupt handler.
	 when this function gets:
	 - the first portion of the long message, the total_len is message_len plus remain_len.
	 - the final portion of the long message, the remain_len is 0.
	 - a short message, the total_len can be equal to message_len and the remain_len can be 0.
 */
uint32_t mailbox_receive(uint8_t *rx_buf, uint32_t *rx_data_len,
		uint32_t *rx_total_len, uint32_t *rx_remain_len)
{
	uint32_t ret = RV_FAIL;
	uint32_t reg;
	int32_t transmitted_len;
	static uint32_t remain_len;

	reg = read_sfr(mb_va_base + PSP_MB_SEE_LEN_OFFSET);
	*rx_data_len = reg & DATA_LEN_MASK;
	*rx_data_len = *rx_data_len >> STMB_LEN_BIT_OFFSET;
	*rx_total_len = reg & TOTAL_LEN_MASK;

	if (*rx_total_len & DATA_LEN_MASK) {
		ret = RV_MB_WORLD_RX_TOTAL_LEN_OVERFLOW + RV_REE_OFFSET;
		goto out;
        }

	if (remain_len == 0)
		remain_len = *rx_total_len;

	*rx_remain_len = remain_len;

	if (*rx_data_len > PSP_MAX_MB_DATA_LEN) {
		ret = RV_MB_WORLD_RX_DATA_LEN_OVERFLOW + RV_REE_OFFSET;
		goto out;
	}

	if (*rx_data_len != PSP_MAX_MB_DATA_LEN && *rx_remain_len > *rx_data_len) {
		ret = RV_MB_WORLD_RX_INVALID_REMAIN_LEN + RV_REE_OFFSET;
		goto out;
	}

	if (*rx_remain_len > *rx_total_len) {
		ret = RV_MB_WORLD_RX_INVALID_REMAIN_LEN2 + RV_REE_OFFSET;
		goto out;
	}

	transmitted_len = *rx_total_len - *rx_remain_len;
	ret = move_mailbox_to_buf(rx_buf + transmitted_len,
			(uint64_t)mb_va_base + PSP_MB_DATA_00_OFFSET_S2R, *rx_data_len);
	if (ret != RV_SUCCESS) {
		psp_print_debug_sfr();
		ret = RV_MB_WORLD_RX_COPY_TO_BUF + RV_REE_OFFSET;
		goto out;
	}

	remain_len -= *rx_data_len;
	*rx_remain_len = remain_len;

out:
	write_sfr(mb_va_base + PSP_MB_SEE_READ_OFFSET, ret);

	return ret;
}
EXPORT_SYMBOL(mailbox_receive);

MODULE_LICENSE("GPL");
