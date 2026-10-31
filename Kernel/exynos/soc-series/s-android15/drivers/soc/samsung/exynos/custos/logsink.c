/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER) && IS_ENABLED(CONFIG_CUSTOS_MEMLOG_MEMORY)
#include <soc/samsung/exynos/memlogger.h>
#endif
#include <soc/samsung/exynos/psp/psp_mailbox_common.h>
#include <soc/samsung/exynos/psp/psp_mailbox_ree.h>
#include <soc/samsung/exynos/psp/psp_error.h>

#include "data_path.h"
#include "log.h"
#include "logsink.h"
#include "mailbox.h"
#include "utils.h"

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER) && IS_ENABLED(CONFIG_CUSTOS_MEMLOG_MEMORY)
struct device custos_memlog_dev;
struct memlog *custos_memlog_desc;
struct memlog_obj *custos_memlog_log;
#endif

extern atomic_t psp_sleep;

#define LOG_NONE_EVENT         0
#define LOG_BUFFER_RESET_EVENT 1

#ifdef CONFIG_PRINTK_CALLER
#define CUSTOS_LOG_LINE_MAX    (1024 - 48 - 12)  // CONSOLE_LOG_MAX - PREFIX_MAX - CUSTOS_TAG
#else
#define CUSTOS_LOG_LINE_MAX	(1024 - 32 - 12)
#endif

#define CUSTOS_LOG_NOT_ENCRYPTED	0x8B09DE55

#if defined(CONFIG_CUSTOS_MEMLOG_MEMORY) || defined(CONFIG_CUSTOS_LOG_DMESG)
static int is_logbuf_updated(struct custos_log *log_st)
{
	unsigned int max_len = (LOG_SIZE - sizeof(struct logbuf_hdr));

	if (log_st == NULL) {
		LOG_ERR("log_st is NULL");
		return 0;
	}

	log_st->eq = log_st->logbuf->hdr.len % max_len;

	return log_st->eq != log_st->dq;
}
#endif

void custos_log_work(void)
{
#if defined(CONFIG_CUSTOS_MEMLOG_MEMORY) || defined(CONFIG_CUSTOS_LOG_DMESG)
	if (custos_dev.log.is_enc == 1)
		return;

	if (is_logbuf_updated(&custos_dev.log) && !atomic_read(&psp_sleep))
		schedule_work(&custos_dev.log.log_work);
#endif
}

#ifdef CONFIG_CUSTOS_LOG_DMESG
static void custos_print_buf(struct log_buf *logbuf, char *start, unsigned int msg_len)
{
	char *const delimiter = "\x0a"; // This means newline
	char *print_msg = NULL;
	char *token;
	char *cur;
	unsigned int max_size, line_max;

	if (logbuf == NULL) {
		LOG_ERR("logbuf is NULL");
		return;
	}

	max_size = LOG_SIZE - sizeof(struct logbuf_hdr);

	if ((start == NULL) ||
		(((unsigned long)start + msg_len) >
		 ((unsigned long)logbuf->buf + max_size))) {
		LOG_ERR("Failed to print out (start: 0x%lx, msg_len: 0x%x, \
			base: 0x%lx, max_size: 0x%x", (unsigned long)start,
				msg_len, (unsigned long)logbuf->buf, max_size);
		return;
	}

	// print out the message
	print_msg = cur = kstrndup(start, msg_len, GFP_KERNEL);
	if (print_msg == NULL) {
		LOG_ERR("failed to allocate memory, start: 0x%lx, msg_len: 0x%x",
				(unsigned long)start, msg_len);
		return;
	}

	line_max = CUSTOS_LOG_LINE_MAX;
	while ((token = strsep(&cur, delimiter))) {
		CUSTOS_PRINT(" %s", token);

		// The size that can be printed at once is defined
		// (LOG_LINE_MAX, kernel/printk/printk.c). if custos requests
		// a larger size to print, split it and print it.
		while (strlen(token) > line_max) {
			token += line_max;
			CUSTOS_PRINT(" %s", token);
		}
	}

	if (print_msg)
		kfree(print_msg);
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
static void custos_memlog_write(void *src, unsigned int size)
{
#ifdef CONFIG_CUSTOS_MEMLOG_MEMORY
	if (custos_memlog_log)
		memlog_write(custos_memlog_log, MEMLOG_LEVEL_ERR, src, size);
#endif
}
#endif

#define CUSTOS_LOG_LOOP_CNT	(100)
void custos_log_flush(struct work_struct *work)
{
	struct custos_log *log_st =
		container_of(work, struct custos_log, log_work);
	char *start = NULL;
	unsigned int msg_len = 0;
	unsigned int max_len = LOG_SIZE - sizeof(struct logbuf_hdr);
	unsigned int cnt = CUSTOS_LOG_LOOP_CNT;

	if (log_st == NULL) {
		LOG_ERR("log_st is NULL");
		return;
	}

	if (log_st->logbuf == NULL) {
		LOG_ERR("logbuf is NULL");
		return;
	}

	log_st->eq = log_st->logbuf->hdr.len % max_len;
	start = log_st->logbuf->buf;

	while ((log_st->eq != log_st->dq) && (cnt-- > 0)) {
		if (atomic_read(&psp_sleep))
			return;

		// calculate the message length to print out
		if (log_st->eq > log_st->dq) {
			msg_len = log_st->eq - log_st->dq;
		} else {
			if (log_st->dq > max_len) {
				LOG_ERR("log_st->dq is bigger than max_len");
				return;
			}
			msg_len = max_len - log_st->dq;
		}

#ifdef CONFIG_CUSTOS_LOG_DMESG
		custos_print_buf(log_st->logbuf, start + log_st->dq, msg_len);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
		custos_memlog_write(start + log_st->dq, msg_len);
#endif

		log_st->dq += msg_len;
		if (log_st->dq >= max_len)
			log_st->dq = log_st->dq % max_len;

		msleep(20);

		log_st->eq = log_st->logbuf->hdr.len % max_len;
	};
}

int custos_memlog_register(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER) && IS_ENABLED(CONFIG_CUSTOS_MEMLOG_MEMORY)
	int ret;

	device_initialize(&custos_memlog_dev);
	ret = memlog_register("cust", &custos_memlog_dev, &custos_memlog_desc);
	if (ret) {
		LOG_ERR("Failed to register memlog, ret:%d", ret);
		return ret;
	}

	custos_memlog_log =
		memlog_alloc(custos_memlog_desc, SZ_1M, 0, "log-mem", 0);
	if (custos_memlog_log == NULL) {
		LOG_ERR("Failed to alloc memlog");
		return -EFAULT;
	}

	return ret;
#else
	return 0;
#endif
}

bool cust_log_check_enc(struct log_buf *buf)
{
	return (buf->hdr.is_enc != CUSTOS_LOG_NOT_ENCRYPTED) ? true : false;
}

int custos_log_init(struct custos_device *cust_st)
{
	unsigned long dump_base = revmem.base + LOG_OFFSET;

	if (cust_st == NULL) {
		LOG_ERR("cust_st is NULL");
		return -EFAULT;
	}

	/* translate PA to VA of log buffer */
	cust_st->log.logbuf = custos_request_region(dump_base, LOG_SIZE);
	if (cust_st->log.logbuf == NULL) {
		LOG_ERR("Failed to map dump area");
		return -EFAULT;
	}

	if (cust_log_check_enc(cust_st->log.logbuf)) {
		LOG_INFO("custos log is encrypted and will not be printed.");
		cust_st->log.is_enc = 1;
		return 0;
	}

	cust_st->log.eq = 0;
	cust_st->log.dq = 0;
	cust_st->log.is_enc = 0;

	INIT_WORK(&cust_st->log.log_work, custos_log_flush);
	mailbox_register_message_log_worker(custos_log_work);

	return 0;
}
