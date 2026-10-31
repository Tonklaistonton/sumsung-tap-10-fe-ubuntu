/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/delay.h>

#include <soc/samsung/exynos/psp/psp_mailbox_common.h>
#include <soc/samsung/exynos/psp/psp_mailbox_ree.h>

#include "log.h"
#include "utils.h"
#include "data_path.h"
#include "cache.h"
#include "custos_error.h"
#include "mailbox.h"

extern struct custos_device custos_dev;
extern custos_func_t cf;

static LIST_HEAD(peer_list);
static DEFINE_RWLOCK(peer_list_lock);
static struct kmem_cache *peer_cache;
u8 send_cnt;

struct custos_data_peer {
	u32 id;
	ssize_t pos;
	struct list_head msg_queue;
	wait_queue_head_t msg_wait_queue;
	struct spinlock msg_queue_lock;
	struct kref refcount;
	struct list_head node;
};

struct custos_data_msg_entry {
	struct list_head node;
	u8 msg[];
};

void custos_data_peer_release(struct kref *ref)
{
	struct custos_data_peer *peer = container_of(ref,
									  struct custos_data_peer,
									  refcount);

	LOG_ENTRY;

	kmem_cache_free(peer_cache, peer);
}

static struct custos_data_peer *custos_get_data_peer(unsigned int id)
{
	struct custos_data_peer *peer = NULL;
	list_for_each_entry (peer, &peer_list, node) {
		if (peer->id == id)
			return peer;
	}

	return NULL;
}

void custos_dispatch_data_msg(struct custos_data_msg *msg)
{
	struct custos_data_msg_entry *msg_entry = NULL;
	struct custos_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;

	LOG_ENTRY;

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = custos_get_data_peer(msg->header.destination);
	if (!peer) {
		LOG_ERR("Received SEE response to inactive session, dest = %d",
				msg->header.destination);
		read_unlock_irqrestore(&peer_list_lock, flags_peer);
		custos_cache_free(msg);
		return;
	}
	kref_get(&peer->refcount);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	msg_entry = custos_cache_get_buf(msg);
	if (!msg_entry) {
		LOG_ERR("Unable to allocate message list entry");
		return;
	}

	spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
	list_add_tail(&msg_entry->node, &peer->msg_queue);
	spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);

	wake_up_interruptible(&peer->msg_wait_queue);

	kref_put(&peer->refcount, custos_data_peer_release);
}

unsigned int custos_poll_wait_data_msg(unsigned int id,
		struct file *file,
		struct poll_table_struct *poll_tab)
{
	struct custos_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;
	unsigned int mask = 0;
	int is_empty;

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = custos_get_data_peer(id);
	if (!peer) {
		LOG_ERR("Peer %u dose not exist", id);
		read_unlock_irqrestore(&peer_list_lock, flags_peer);
		return RV_ERR_CUSTOS_WAIT_DATA_MSG_NO_PEER;
	}
	kref_get(&peer->refcount);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	poll_wait(file, &peer->msg_wait_queue, poll_tab);

	spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
	is_empty = list_empty(&peer->msg_queue);
	spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);

	kref_put(&peer->refcount, custos_data_peer_release);

	if (!is_empty)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

size_t custos_send_launch_msg(struct custos_launch_msg *msg)
{
	int ret;
	int total_len;
	int launch_len;
	uint32_t len_out = 0;
	uint32_t rx_total_len = 0;
	uint32_t rx_remain_len = 0;
	unsigned long flags = 0;
	uint8_t *receive_buf;
	uint32_t checksum_len = 0;

	total_len = sizeof(struct custos_launch_msg);
	launch_len = sizeof(struct custos_launch_msg_header);

	msg->flag.value = REE_DATA_FLAG;
	msg->msg_header.source = REE_SRC;
	msg->msg_header.destination = SS_TOKEN;
	msg->msg_header.length = launch_len;
	msg->msg_header.type = REQUEST;
	msg->checksum = 0;

	msg->launch_header.launch_length = sizeof("launch") - 1;
	memcpy(msg->launch_header.launch_payload, "launch", msg->launch_header.launch_length);
	msg->launch_header.payload_length = LAUNCH_PAYLOAD_LEN;

	LOG_DUMP_BUF((uint8_t *)msg, total_len);

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

	if (total_len > PSP_MAX_MB_DATA_LEN)
		checksum_len = PSP_MAX_MB_DATA_LEN;
	else
		checksum_len = total_len;

	msg->checksum = custos_get_checksum((uint8_t*)msg, checksum_len);

	ret = mailbox_send((uint8_t *)msg, total_len, &len_out);

	if (ret == RV_MB_WORLD_WAIT1_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout, total = (%d), len_out = (%d) remain = (%d)\n",
				total_len, len_out, (total_len - len_out));
		goto out;
	} else if (ret == RV_MB_WORLD_WAIT2_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout[2]\n");
		goto out;
	}

	LOG_INFO("2 Send Message, perr = (%u:%u), source = (%d), total_len = (%d), ret = (%d) len_out = (%d)",
			0, 0, msg->msg_header.source, total_len, ret, len_out);

	custos_iwc_work(receive_buf, &rx_total_len, &rx_remain_len);

out:
	spin_unlock_irqrestore(&custos_dev.iwc_lock, flags);

	LOG_INFO("2 callback: %d, %d, %d", rx_total_len, rx_remain_len, ret);

	/* Callback */
	cf.msg_receiver(receive_buf, rx_total_len - rx_remain_len, ret);
	mutex_unlock(&custos_dev.lock);

	return len_out;
}

size_t custos_send_destroy_msg(struct custos_destroy_msg *msg)
{
	int ret;
	int total_len;
	int destroy_len;
	uint32_t len_out = 0;
	uint32_t rx_total_len = 0;
	uint32_t rx_remain_len = 0;
	unsigned long flags = 0;
	uint8_t *receive_buf;
	uint32_t checksum_len = 0;

	total_len = sizeof(struct custos_destroy_msg);
	destroy_len = sizeof(struct custos_destroy_msg_header);

	msg->flag.value = REE_DATA_FLAG;
	msg->msg_header.source = REE_SRC;
	msg->msg_header.destination = SS_TOKEN;
	msg->msg_header.length = destroy_len;
	msg->msg_header.type = REQUEST;
	msg->checksum = 0;

	msg->destroy_header.destroy_length = sizeof("destroy") - 1;
	memcpy(msg->destroy_header.destroy_payload, "destroy", msg->destroy_header.destroy_length);
	msg->destroy_header.payload_length = DESTROY_PAYLOAD_LEN;

	LOG_DUMP_BUF((uint8_t *)msg, total_len);

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

	if (total_len > PSP_MAX_MB_DATA_LEN)
		checksum_len = PSP_MAX_MB_DATA_LEN;
	else
		checksum_len = total_len;

	msg->checksum = custos_get_checksum((uint8_t*)msg, checksum_len);

	ret = mailbox_send((uint8_t *)msg, total_len, &len_out);

	if (ret == RV_MB_WORLD_WAIT1_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout, total = (%d), len_out = (%d) remain = (%d)\n",
				total_len, len_out, (total_len - len_out));
		goto out;
	} else if (ret == RV_MB_WORLD_WAIT2_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout[2]\n");
		goto out;
	}

	LOG_INFO("3 Send Message, perr = (%u:%u), source = (%d), total_len = (%d), ret = (%d) len_out = (%d)",
			0, 0, msg->msg_header.source, total_len, ret, len_out);

	custos_iwc_work(receive_buf, &rx_total_len, &rx_remain_len);

out:
	spin_unlock_irqrestore(&custos_dev.iwc_lock, flags);

	LOG_INFO("3 callback: %d, %d, %d", rx_total_len, rx_remain_len, ret);

	/* Callback */
	cf.msg_receiver(receive_buf, rx_total_len - rx_remain_len, ret);
	mutex_unlock(&custos_dev.lock);

	return len_out;
}

size_t custos_send_data_msg(unsigned int id, struct custos_crc_data_msg *msg)
{
	int ret;
	int total_len;
	uint32_t len_out = 0;
	uint32_t rx_total_len = 0;
	uint32_t rx_remain_len = 0;
	unsigned long flags = 0;
	uint8_t *receive_buf;
	uint32_t checksum_len = 0;

	msg->data.flag.value = REE_DATA_FLAG;
	msg->data.header.source = id;
	msg->checksum = 0;
	total_len = sizeof(struct custos_crc_data_msg) + msg->data.header.length;

	LOG_DUMP_BUF((uint8_t *)msg, total_len);

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
	msg->data.flag.value |= (send_cnt << SEQ_SHIFT);
	LOG_DBG("Send, flag = (%x)\n", msg->data.flag.value);
	len_out = 0;

	if (total_len > PSP_MAX_MB_DATA_LEN)
		checksum_len = PSP_MAX_MB_DATA_LEN;
	else
		checksum_len = total_len;

	msg->checksum = custos_get_checksum((uint8_t*)msg, checksum_len);

	ret = mailbox_send((uint8_t *)msg, total_len, &len_out);

	if (ret == RV_MB_WORLD_WAIT1_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout, total = (%d), len_out = (%d) remain = (%d)\n",
					total_len, len_out, (total_len - len_out));
		goto out;
	} else if (ret == RV_MB_WORLD_WAIT2_TIMEOUT + RV_REE_OFFSET) {
		LOG_ERR("Send Timeout[2]\n");
		goto out;
	}

	LOG_INFO("4 Send Message, perr = (%u:%u), source = (%d), total_len = (%d), ret = (%d) len_out = (%d)",
			id, current->pid, msg->data.header.source, total_len, ret, len_out);


	custos_iwc_work(receive_buf, &rx_total_len, &rx_remain_len);

out:
	spin_unlock_irqrestore(&custos_dev.iwc_lock, flags);

	LOG_INFO("4 callback: %d, %d, %d", rx_total_len, rx_remain_len, ret);

	/* Callback */
	cf.msg_receiver(receive_buf, rx_total_len - rx_remain_len, ret);
	mutex_unlock(&custos_dev.lock);

	return len_out;
}

int custos_is_data_msg_queue_empty(unsigned int id)
{
	struct custos_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;
	int is_empty = 1;

	LOG_ENTRY;

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = custos_get_data_peer(id);
	if (peer) {
		spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
		is_empty = list_empty(&peer->msg_queue);
		spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
	} else
		LOG_ERR("Peer %u dose not exist", id);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	return is_empty;
}

int custos_read_data_msg_timeout(unsigned int id, char __user *buffer,
								   size_t len, uint64_t timeout_jiffies)
{
	struct custos_data_msg_entry *msg_entry = NULL;
	struct custos_data_peer *peer = NULL;
	unsigned long flags_peer;
	size_t msg_len, read;
	int handled = 0;
	int ret;
	struct custos_data_msg *msg;

	LOG_INFO("Peer %u, Read Start, len = %zu", id, len);

	read_lock_irqsave(&peer_list_lock, flags_peer);
	peer = custos_get_data_peer(id);
	if (!peer) {
		LOG_ERR("Peer %u dose not exist", id);
		read_unlock_irqrestore(&peer_list_lock, flags_peer);
		return -ENOENT;
	}
	kref_get(&peer->refcount);
	read_unlock_irqrestore(&peer_list_lock, flags_peer);

	flags_peer = 0;
	spin_lock_irqsave(&peer->msg_wait_queue.lock, flags_peer);
	ret = wait_event_interruptible_timeout_locked(
			  peer->msg_wait_queue, !list_empty(&peer->msg_queue),
			  timeout_jiffies);
	spin_unlock_irqrestore(&peer->msg_wait_queue.lock, flags_peer);

	if (ret == 0) {
		kref_put(&peer->refcount, custos_data_peer_release);
		LOG_ERR("Peer %u's waiting for read was timeout", id);
		return -ETIME;
	} else if (ret == -ERESTARTSYS) {
		kref_put(&peer->refcount, custos_data_peer_release);
		LOG_ERR("Peer %u's waiting was canceled", id);
		return -ERESTARTSYS;
	}

	while (handled < len) {
		unsigned long flags_msg = 0;
		LOG_INFO("Current Read handled = %d , peer->pos = %zd", handled, peer->pos);
		spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
		msg_entry = list_first_entry(&peer->msg_queue,
									 struct custos_data_msg_entry, node);

		if (!msg_entry) {
			spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
			break;
		}

		msg = (struct custos_data_msg *)&msg_entry->msg[0];
		msg_len = msg->header.length + sizeof(struct custos_data_msg_header);

		LOG_DUMP_BUF(msg_entry->msg, sizeof(struct custos_msg_flag) + msg_len);

		read = min(len - handled, msg_len - peer->pos);
		ret = copy_to_user(buffer + handled,
						 ((char *)msg_entry->msg) + peer->pos +
						 sizeof(struct custos_msg_flag),
						 read);
		if (ret != 0) {
			LOG_ERR("msg->flag = %d ", msg->flag.value);
			LOG_ERR("msg->src = %d ", msg->header.source);
			LOG_ERR("msg->dst = %d ", msg->header.destination);
			LOG_ERR("msg->len = %d ", msg->header.length);
			LOG_ERR("msg->type = %d ", msg->header.type);
			LOG_ERR("Unable to copy message to userspace handled = %d len = %zu read = %zu ret = %d", handled, len, read, ret);
			spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
			return -EFAULT;
		}

		handled += read;
		peer->pos += read;

		if (peer->pos == msg_len) {
			list_del(&msg_entry->node);
			custos_cache_free(msg_entry->msg);
			peer->pos = 0;

			if (list_empty(&peer->msg_queue)) {
				LOG_INFO("Message Queue is Empty handled = %d, len = %zu", handled, len);
				len = handled;
				LOG_INFO("Set len = %zu, handled = %d", len, handled);
			}
		}
		spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);
	}
	kref_put(&peer->refcount, custos_data_peer_release);

	LOG_INFO("Peer %u, Read finished, final cnt = %u", id, handled);
	return handled;
}

int custos_data_peer_create(unsigned int id)
{
	struct custos_data_peer *peer = NULL;
	unsigned long flags;
	int ret = 0;

	write_lock_irqsave(&peer_list_lock, flags);

	peer = custos_get_data_peer(id);
	if (peer)
		goto out;

	LOG_DBG("Create new peer %u", id);

	peer = kmem_cache_alloc(peer_cache, GFP_KERNEL);
	if (!peer) {
		LOG_ERR("Failed to allocate peer %u", id);
		ret = -ENOMEM;
		goto out;
	}

	peer->id = id;
	peer->pos = 0;

	INIT_LIST_HEAD(&peer->msg_queue);
	spin_lock_init(&peer->msg_queue_lock);
	init_waitqueue_head(&peer->msg_wait_queue);

	kref_init(&peer->refcount);

	list_add_tail(&peer->node, &peer_list);

out:
	write_unlock_irqrestore(&peer_list_lock, flags);
	return ret;
}

void custos_data_peer_destroy(unsigned int id)
{
	struct custos_data_msg_entry *msg_entry = NULL;
	struct custos_data_peer *peer = NULL;
	unsigned long flags_peer, flags_msg;

	write_lock_irqsave(&peer_list_lock, flags_peer);

	peer = custos_get_data_peer(id);
	if (!peer) {
		LOG_ERR("Peer %u dose not exist", id);
		write_unlock_irqrestore(&peer_list_lock, flags_peer);
		return;
	}

	LOG_DBG("Delete peer %u", id);
	list_del(&peer->node);

	spin_lock_irqsave(&peer->msg_queue_lock, flags_msg);
	while (!list_empty(&peer->msg_queue)) {
		msg_entry = list_first_entry(&peer->msg_queue,
									 struct custos_data_msg_entry, node);
		list_del(&msg_entry->node);
		custos_cache_free(msg_entry->msg);
	}
	spin_unlock_irqrestore(&peer->msg_queue_lock, flags_msg);

	kref_put(&peer->refcount, custos_data_peer_release);

	write_unlock_irqrestore(&peer_list_lock, flags_peer);
}

int custos_data_path_init(void)
{
	peer_cache = kmem_cache_create("custos-data-peer",
								   (unsigned int)sizeof(struct custos_data_peer),
								   0,
								   SLAB_HWCACHE_ALIGN,
								   NULL
								  );
	if (!peer_cache) {
		LOG_ERR("Unable to initialize cache for data peers");
		return -1;
	}

	return 0;
}

void custos_data_path_deinit(void)
{
	kmem_cache_destroy(peer_cache);
}

