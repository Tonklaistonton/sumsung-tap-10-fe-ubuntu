/*
 * psp_error.h - Samsung Psp Mailbox error return
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PSP_ERROR_H__
#define __PSP_ERROR_H__

/*****************************************************************************/
/* Define error return							     */
/*****************************************************************************/
/* Common */
#define RV_SUCCESS					0x0000
#define RV_PASS						0x1234
#define RV_FAIL						0x7432

/* World OFFSET*/
#define RV_REE_OFFSET					0x100
#define RV_TEE_OFFSET					0x200
#define RV_CP_OFFSET					0x300

/* WORLD tx err */
#define RV_MB_WORLD_TX_TOTAL_LEN_OVERFLOW		0xC001
#define RV_MB_WORLD_TX_EMPTY_LEN			0xC002
#define RV_MB_WORLD_TX_COPY_TO_MAILBOX			0xC003
#define RV_MB_WORLD_TX_PSP_POWER_OFF			0xC004
/* WORLD rx err */
#define RV_MB_WORLD_RX_DATA_LEN_OVERFLOW		0xC010
#define RV_MB_WORLD_RX_TOTAL_LEN_OVERFLOW		0xC011
#define RV_MB_WORLD_RX_INVALID_DATA_LEN			0xC012
#define RV_MB_WORLD_RX_COPY_TO_BUF			0xC013
#define RV_MB_WORLD_RX_INVALID_REMAIN_LEN		0xC014
#define RV_MB_WORLD_RX_INVALID_REMAIN_LEN2		0xC015

/* CUSTOS tx err */
#define RV_MB_CUSTOS_TX_REMAIN_LEN_OVERFLOW		0xC020
#define RV_MB_CUSTOS_TX_TOTAL_LEN_OVERFLOW		0xC021
#define RV_MB_CUSTOS_TX_EMPTY_LEN			0xC022
#define RV_MB_CUSTOS_TX_COPY_TO_MAILBOX			0xC023
#define RV_MB_CUSTOS_TX_CNT_TIMEOUT			0xC024
#define RV_MB_CUSTOS_TX_NOT_IDLE			0xC025
/* CUSTOS rx err */
#define RV_MB_CUSTOS_RX_DATA_LEN_OVERFLOW		0xC030
#define RV_MB_CUSTOS_RX_TOTAL_LEN_OVERFLOW		0xC031
#define RV_MB_CUSTOS_RX_INVALID_REMAIN_LEN		0xC032
#define RV_MB_CUSTOS_RX_COPY_TO_BUF			0xC033
#define RV_MB_CUSTOS_RX_NOT_REQ				0xC034
#define RV_MB_CUSTOS_RX_INVALID_CMD			0xC035
#define RV_MB_CUSTOS_RX_REPEATED_BLOCK			0xC036

/* pending wait err */
#define RV_MB_WORLD_WAIT1_TIMEOUT			0xC040
#define RV_MB_WORLD_WAIT2_TIMEOUT			0xC041
#define RV_MB_TIMEOUT_INTERNAL				0xC042

/* TEE err */
#define RV_MESSAGE_ERROR				0xC050
#define RV_MESSAGE_INVALID				0xC051
#define RV_PENDING_WAIT1_TIMEOUT			0xC052
#define RV_PENDING_WAIT2_TIMEOUT			0xC053

#define RV_CHECKSUM_FAIL				0xC060

#endif /* __PSP_ERROR_H__ */
