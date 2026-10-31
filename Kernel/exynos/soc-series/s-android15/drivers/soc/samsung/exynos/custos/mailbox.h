/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CUSTOS_MAILBOX_H__
#define __CUSTOS_MAILBOX_H__

#define DISCOVERY		0
#define DISCOVERY_RETURN	1
#define REQUEST			2
#define RESPONSE		3
#define ERROR			4

#define REE_SRC			(0xF0)

#define SS_TOKEN		(100)

#define LAUNCH_PAYLOAD_LEN	(12)
#define DESTROY_PAYLOAD_LEN	(40)

struct custos_msg_flag {
	u8 value;
} __attribute__((packed));

uint32_t custos_get_checksum(uint8_t *data, uint32_t data_len);
int custos_mailbox_init(void);
void custos_mailbox_deinit(void);
int custos_cache_cnt(bool loop_search);
void *custos_cache_get_buf(void *);
#endif
