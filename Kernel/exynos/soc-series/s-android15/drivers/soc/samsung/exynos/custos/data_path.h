/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CUSTOS_DATA_PATH_H__
#define __CUSTOS_DATA_PATH_H__

#include <linux/poll.h>

#include "mailbox.h"

#define FLAG_DATA_BIT		0x00

#define REE_DATA_FLAG		(FLAG_DATA_BIT | 0x1)

struct custos_data_msg_header {
	u32 source;
	u32 destination;
	u32 length;
	u8 type;
} __attribute__((packed));

struct custos_launch_msg_header {
	u32 launch_length;
	u8 launch_payload[6];
	u32 payload_length;
	u64 memory_addr;
	u32 addr_length;
} __attribute__((packed));

struct custos_destroy_msg_header {
	u32 destroy_length;
	u8 destroy_payload[7];
	u32 payload_length;
	u32 uuid_length;
	u8 uuid[36];
} __attribute__((packed));

struct custos_data_msg {
	struct custos_msg_flag flag;
	struct custos_data_msg_header header;
	u8 payload[];
} __attribute__((packed));

struct custos_launch_msg {
	u32 checksum;
	struct custos_msg_flag flag;
	struct custos_data_msg_header msg_header;
	struct custos_launch_msg_header launch_header;
} __attribute__((packed));

struct custos_destroy_msg {
	u32 checksum;
	struct custos_msg_flag flag;
	struct custos_data_msg_header msg_header;
	struct custos_destroy_msg_header destroy_header;
} __attribute__((packed));

struct custos_crc_data_msg {
	u32 checksum;
	struct custos_data_msg data;
} __attribute__((packed));

void custos_dispatch_data_msg(struct custos_data_msg *msg);
size_t custos_send_data_msg(unsigned int id, struct custos_crc_data_msg *msg);
size_t custos_send_launch_msg(struct custos_launch_msg *msg);
size_t custos_send_destroy_msg(struct custos_destroy_msg *msg);
int custos_read_data_msg_timeout(unsigned int id, char __user *buffer,
								   size_t cnt, uint64_t timeout);
unsigned int custos_poll_wait_data_msg(unsigned int id,
		struct file *file,
		struct poll_table_struct *poll_tab);
int custos_is_data_msg_queue_empty(unsigned int id);
int custos_data_peer_create(unsigned int id);
void custos_data_peer_destroy(unsigned int id);

int custos_data_path_init(void);
void custos_data_path_deinit(void);

#endif
