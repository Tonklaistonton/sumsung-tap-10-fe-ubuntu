/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/sched/clock.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include "data_path.h"

#define SHM_START_ADDR	0x1C0000
#define SHM_START_PAGE	(SHM_START_ADDR >> PAGE_SHIFT)
#define SHM_SIZE	0x40000	// 0x1C0000 ~ 0x200000

#define SHM_USE_NO_ONE	0

#define SEQ_SHIFT	4
#define SEQ_MASK	0x07

#define SHM_SETTING_FLAG_TRUE 1
#define SHM_SETTING_FLAG_FALSE 0

#define CONFIG_CUSTOS_POLLING
#define PAUSE_FLAG_TRUE 1
#define PAUSE_FLAG_FALSE 0
#define RESUME_FLAG_TRUE 1
#define RESUME_FLAG_FALSE 0

#define wait_event_interruptible_timeout_locked(q, cond, tmo)		\
({									\
	long __ret = (tmo);						\
	DEFINE_WAIT(__wait);						\
	if (!(cond)) {							\
		for (;;) {						\
			__wait.flags &= ~WQ_FLAG_EXCLUSIVE;		\
			if (list_empty(&__wait.entry))			\
				__add_wait_queue_entry_tail(&(q), &__wait);	\
			set_current_state(TASK_INTERRUPTIBLE);		\
			if ((cond))					\
				break;					\
			if (signal_pending(current)) {			\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
			spin_unlock_irq(&(q).lock);			\
			__ret = schedule_timeout(__ret);		\
			spin_lock_irq(&(q).lock);			\
			if (!__ret) {					\
				if ((cond))				\
					__ret = 1;			\
				break;					\
			}						\
		}							\
		__set_current_state(TASK_RUNNING);			\
		if (!list_empty(&__wait.entry))				\
			list_del_init(&__wait.entry);			\
		else if (__ret == -ERESTARTSYS &&			\
			 /*reimplementation of wait_abort_exclusive() */\
			 waitqueue_active(&(q)))			\
			__wake_up_locked_key(&(q), TASK_INTERRUPTIBLE,	\
			NULL);						\
	} else {							\
		__ret = 1;						\
	}								\
	__ret;								\
})

struct logbuf_hdr {
	u32 len;
	u32 version;
	u32 is_enc;
	u32 reserved[125];
};

struct log_buf {
	struct logbuf_hdr hdr;
	char buf[0];
};

struct custos_log {
	struct work_struct log_work;
	struct log_buf *logbuf;
	u32 eq; // write byte index from custos (0 ~ (LOG_SIZE - sizeof(hdr)))
	u32 dq; // read byte index from ree (0 ~ (LOG_SIZE - sizeof(hdr)))
	u32 is_enc;
};

struct custos_device {
	struct mutex vm_lock;
	struct mutex lock;
	struct mutex logsink_lock;
	struct spinlock iwc_lock;
	unsigned int state;
	struct workqueue_struct *log_wq;
	struct custos_log log;
	atomic_t shm_user;
	wait_queue_head_t shm_wait_queue;
};

struct custos_memory_entry {
	struct list_head node;
	unsigned long addr;
	unsigned long size;
	dma_addr_t handle;
	struct custos_client *client;
};

struct custos_client {
	struct list_head node;
	unsigned int peer_id;
	struct list_head shmem_list;
	struct spinlock shmem_list_lock;
	unsigned int current_dump_offset;
	struct spinlock timeout_set_lock;
	uint64_t timeout_jiffies;
	void *dump_vbase;
};

struct custos_reserved_memory {
	unsigned long base;
	unsigned long size;
};

extern struct custos_device custos_dev;
extern struct custos_reserved_memory revmem;

struct custos_client *custos_create_client(unsigned int peer_id);
void custos_destroy_client(struct custos_client *client);

void custos_iwc_work(uint8_t *receive_buf, uint32_t* rx_total_len, uint32_t* rx_remain_len);
void *custos_request_region(unsigned long addr, unsigned int size);

#endif /* __UTILS_H__ */
