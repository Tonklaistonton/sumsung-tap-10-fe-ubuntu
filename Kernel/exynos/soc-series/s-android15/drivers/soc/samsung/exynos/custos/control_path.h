/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CUSTOS_CONTROL_PATH_H__
#define __CUSTOS_CONTROL_PATH_H__

#include "mailbox.h"

#define FLAG_CONTROL_BIT	0x80
#define CONTROL_PAYLOAD		0x10

#define REE_CONTROL_FLAG	(FLAG_CONTROL_BIT | 0x1)

struct custos_control_msg {
	u32 checksum;
	struct custos_msg_flag flag;
	u8 cmd;
	u8 sub_cmd;
	u8 payload_len;
	u8 payload[CONTROL_PAYLOAD];
} __attribute__((packed));

void custos_dispatch_control_msg(struct custos_control_msg *msg);
size_t custos_send_ctrl_msg(unsigned int id, struct custos_control_msg *msg);

int custos_control_path_init(void);
void custos_control_path_deinit(void);

/* IOCTL commands */
#define CUSTOS_CONTROL_IOCTL_MAGIC 'd'
#define CUSTOS_CONTROL_IOCTL_SET_DAEMON _IOW(CUSTOS_CONTROL_IOCTL_MAGIC, 40, pid_t)

#endif
