/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/time.h>

#include <soc/samsung/exynos/psp/psp_mailbox_common.h>
#include <soc/samsung/exynos/psp/psp_mailbox_ree.h>

#include "utils.h"
#include "control_path.h"
#include "log.h"
#include "cache.h"

typedef void (*control_message_handler)(u8 event);

atomic_t pause_flag;
extern u8 send_cnt;
extern custos_func_t cf;

void custos_handle_security_event(u8 event)
{
	LOG_INFO("Handle security event");
}

void custos_handle_resume_event(u8 event)
{
	LOG_INFO("Handle resume event");

	atomic_dec(&pause_flag);
}

void custos_handle_pause_event(u8 event)
{
	LOG_INFO("Handle pause event");

	atomic_inc(&pause_flag);
}

static control_message_handler control_message_handlers[] = {
	custos_handle_pause_event,
	custos_handle_resume_event,
	custos_handle_security_event,
};

size_t custos_send_ctrl_msg(unsigned int id, struct custos_control_msg *msg)
{
	int ret;
	int msg_len;
	uint32_t len_out = 0;
	uint32_t rx_total_len = 0;
	uint32_t rx_remain_len = 0;
	unsigned long flags = 0;
	uint8_t *receive_buf;
	uint32_t checksum_len = 0;

	msg->flag.value = REE_CONTROL_FLAG;
	msg->checksum = 0;
	msg_len = sizeof(struct custos_control_msg);

	LOG_DUMP_BUF((uint8_t *)msg, msg_len);

	mutex_lock(&custos_dev.lock);

	/* Read tx_total_len */
	receive_buf = cf.mem_allocator(4096);

	if (receive_buf == NULL) {
		LOG_ERR("Fail to mem_allocator\n");
		ret = -1;
		goto out;
	}

	spin_lock_irqsave(&custos_dev.iwc_lock, flags);

	send_cnt++;
	send_cnt &= SEQ_MASK;
	msg->flag.value |= (send_cnt << SEQ_SHIFT);
	LOG_DBG("Send, flag = (%x)\n", msg->flag.value);
	len_out = 0;

	checksum_len = msg_len;
	msg->checksum = custos_get_checksum((uint8_t *)msg, checksum_len);

	ret = mailbox_send((uint8_t *)msg, msg_len, &len_out);

	if (ret == RV_MB_WORLD_WAIT1_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout, total = (%d), len_out = (%d) remain = (%d)\n",
					msg_len, len_out, (msg_len - len_out));
		goto out;
	} else if (ret == RV_MB_WORLD_WAIT2_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout[2]\n");
		goto out;
	}

	LOG_INFO("1 Send Message, perr = (%u:%u), cmd = (%d), sub_cmd = (%d),  msg_len = (%d), ret = (%d) len_out = (%d)",
			id, current->pid, msg->cmd , msg->sub_cmd, msg_len, ret, len_out);

	custos_iwc_work(receive_buf, &rx_total_len, &rx_remain_len);

out:
	spin_unlock_irqrestore(&custos_dev.iwc_lock, flags);

	LOG_INFO("1 callback: %d, %d, %d", rx_total_len, rx_remain_len, ret);

	/* Callback */
	cf.msg_receiver(receive_buf, rx_total_len - rx_remain_len, ret);
	mutex_unlock(&custos_dev.lock);

	return len_out;
}

void custos_dispatch_control_msg(struct custos_control_msg *msg)
{
	LOG_DBG("Control message - command: %d, sub command: %d",
			msg->cmd, msg->sub_cmd);

	if (msg->cmd < ARRAY_SIZE(control_message_handlers))
		control_message_handlers[msg->cmd](msg->sub_cmd);

	else
		LOG_ERR("Unknown command: %hhu", msg->cmd);
	custos_cache_free(msg);
}
