/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/sched/clock.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "npu-device.h"
#include "npu-debug.h"
#include "npu-log.h"
#include "interface/hardware/npu-interface.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"
#include "npu-memory-manual.h"

static struct npu_log npu_log = {
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	.npu_ioctl_array_obj = NULL,
#endif
};

static struct npu_log fw_report = {
	.st_buf = NULL,
	.st_size = 0,
	.wr_pos = 0,
	.rp_pos = 0,
	.line_cnt = 0,
	.last_dump_line_cnt = 0,
};

static DEFINE_SPINLOCK(fw_report_lock);

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
size_t npu_ioctl_array_to_string(void *src, size_t src_size,
			void *buf, size_t count, loff_t *pos)
{
	struct npu_log_ioctl *ptr;
	size_t n = 0;
	unsigned long nsec = 0;

	if (src_size < sizeof(struct npu_log_ioctl))
		return 0;

	ptr = src;
	nsec = do_div(ptr->timestamp, 1000000000);

	n += scnprintf(buf + n, count - n,
			"[%5lu.%-6lu] [NPU][ioctl] ioctl_cmd : 0x%x, ioctl_dir : %d\n",
			(unsigned long)ptr->timestamp, nsec / 1000, ptr->cmd, ptr->dir);

	*pos += n;

	return sizeof(*ptr);
}

static size_t npu_log_memlog_data_to_string(void *src, size_t src_size,
			void *buf, size_t count, loff_t *pos)
{
	size_t	cp_sz = src_size >= count - 1 ? count - 1 : src_size;
	char	*dst_str = buf;

	dst_str[cp_sz] = 0;
	memcpy(buf, src, cp_sz);

	*pos += cp_sz;

	return cp_sz;
}

static void npu_log_rmemlog(struct npu_device *npu_dev)
{
	int ret = 0;

	/* Register NPU Device Driver for Unified Logging */
	ret = memlog_register("NPU_DRV1", npu_dev->dev, &npu_log.memlog_desc_log);
	if (ret)
		probe_err("memlog_register NPU_DRV1() for log failed: ret = %d\n", ret);

	ret = memlog_register("NPU_DRV2", npu_dev->dev, &npu_log.memlog_desc_dump);
	if (ret)
		probe_err("memlog_register NPU_DRV2() for log failed: ret = %d\n", ret);

	ret = memlog_register("NPU_DRV3", npu_dev->dev, &npu_log.memlog_desc_array);
	if (ret)
		probe_err("memlog_register NPU_DRV3() for array failed: ret = %d\n", ret);

	ret = memlog_register("NPU_DRV4", npu_dev->dev, &npu_log.memlog_desc_health);
	if (ret)
		probe_err("memlog_register NPU_DRV4() for array failed: ret = %d\n", ret);

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	ret = memlog_register("NPU_DRV5", npu_dev->dev, &npu_log.memlog_desc_fw_governor);
	if (ret)
		probe_err("memlog_register NPU_DRV5() for array failed: ret = %d\n", ret);
#endif

	/* Receive allocation of memory for saving data to vendor storage */
	npu_log.npu_memfile_obj = memlog_alloc_file(npu_log.memlog_desc_log, "npu-fil",
						SZ_2M*2, SZ_2M*2, 500, 1);
	if (npu_log.npu_memfile_obj) {
		memlog_register_data_to_string(npu_log.npu_memfile_obj, npu_log_memlog_data_to_string);
		npu_log.npu_memlog_obj = memlog_alloc_printf(npu_log.memlog_desc_log, SZ_1M,
						npu_log.npu_memfile_obj, "npu-mem", 0);
		if (!npu_log.npu_memlog_obj)
			probe_err("memlog_alloc_printf() failed\n");
	} else {
		probe_err("memlog_alloc_file() failed\n");
	}

	npu_log.npu_dumplog_obj = memlog_alloc_printf(npu_log.memlog_desc_dump, SZ_1M,
						NULL, "npu-dum", 0);
	if (!npu_log.npu_dumplog_obj)
		probe_err("memlog_alloc_printf() failed\n");

	npu_log.npu_memhealth_obj = memlog_alloc_printf(npu_log.memlog_desc_health, SZ_128K,
						NULL, "npu-dum", 0);
	if (!npu_log.npu_memhealth_obj)
		probe_err("memlog_alloc_printf() failed\n");
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_log.npu_memfw_governor_obj = memlog_alloc_printf(npu_log.memlog_desc_fw_governor, 2 * SZ_128K,
						NULL, "npu-dum", 0);
	if (!npu_log.npu_memfw_governor_obj)
		probe_err("memlog_alloc_printf() failed\n");
#endif

	npu_log.npu_array_file_obj = memlog_alloc_file(npu_log.memlog_desc_array, "hw-fil",
						sizeof(union npu_log_tag)*LOG_UNIT_NUM,
						sizeof(union npu_log_tag)*LOG_UNIT_NUM,
						500, 1);

	if (npu_log.npu_array_file_obj) {
		npu_log.npu_ioctl_array_obj = memlog_alloc_array(npu_log.memlog_desc_array, LOG_UNIT_NUM,
			sizeof(struct npu_log_ioctl), npu_log.npu_array_file_obj, "io-arr",
			"npu_log_ioctl", 0);
		if (npu_log.npu_ioctl_array_obj) {
			memlog_register_data_to_string(npu_log.npu_ioctl_array_obj, npu_ioctl_array_to_string);
		} else {
			probe_err("memlog_alloc_array() failed\n");
		}
	} else {
		probe_err("memlog_alloc_file() for array failed\n");
	}
}

void npu_log_set_loglevel(int slient, int loglevel)
{
	if (slient)
		npu_log.npu_memlog_obj->log_level = loglevel;
	else
		npu_log.npu_memlog_obj->log_level = npu_log.npu_dumplog_obj->log_level;
}

inline void npu_log_ioctl_set_date(int cmd, int dir)
{
	struct npu_log_ioctl npu_log_ioctl;

	npu_log_ioctl.cmd = cmd;
	npu_log_ioctl.dir = dir;

	if (npu_log.npu_ioctl_array_obj)
		memlog_write_array(npu_log.npu_ioctl_array_obj, MEMLOG_LEVEL_CAUTION, &npu_log_ioctl);
}

void npu_memlog_store(npu_log_level_e loglevel, const char *fmt, ...)
{
	char npu_string[1024];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(npu_string, fmt, ap);
	va_end(ap);

	if (npu_log.npu_memlog_obj) {
		if (npu_log.npu_memlog_obj->log_level < loglevel)
			return;

		memlog_write_printf(npu_log.npu_memlog_obj, loglevel, npu_string);
	}

	if (npu_log.npu_err_in_dmesg == 1)
		pr_err("%s\n", npu_string);
}

void npu_dumplog_store(npu_log_level_e loglevel, const char *fmt, ...)
{
	char npu_string[1024];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(npu_string, fmt, ap);
	va_end(ap);

	if (npu_log.npu_dumplog_obj)
		memlog_write_printf(npu_log.npu_dumplog_obj, loglevel, npu_string);
}

void npu_healthlog_store(npu_log_level_e loglevel, const char *fmt, ...)
{
	char npu_string[1024];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(npu_string, fmt, ap);
	va_end(ap);

	if (npu_log.npu_memhealth_obj)
		memlog_write_printf(npu_log.npu_memhealth_obj, loglevel, npu_string);
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
void npu_fw_governor_log_store(npu_log_level_e loglevel, const char *fmt, ...)
{
	char npu_string[1024];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(npu_string, fmt, ap);
	va_end(ap);

	if (npu_log.npu_memfw_governor_obj)
		memlog_write_printf(npu_log.npu_memfw_governor_obj, loglevel, npu_string);
}
#endif
#else
/* for debug and performance */
inline void npu_log_ioctl_set_date(int cmd, int dir) {}
static void npu_log_rmemlog(__attribute__((unused))struct npu_device *npu_dev) {}
#endif

void npu_fw_report_init(char *buf_addr, const size_t size)
{
	unsigned long	intr_flags;

	spin_lock_irqsave(&fw_report_lock, intr_flags);
	fw_report.st_buf = buf_addr;
	buf_addr[0] = '\0';
	buf_addr[size - 1] = '\0';
	fw_report.st_size = size;
	fw_report.wr_pos = 0;
	fw_report.rp_pos = 0;
	fw_report.line_cnt = 0;

	spin_unlock_irqrestore(&fw_report_lock, intr_flags);
}

void npu_fw_report_deinit(void)
{
	unsigned long	intr_flags;

	/* Wake-up readers and preserve some time to flush */
	wake_up_all(&fw_report.wq);
	msleep(NPU_STORE_LOG_FLUSH_INTERVAL_MS);

	npu_info("fw_report memory deinitializing : %pK -> NULL\n", fw_report.st_buf);
	spin_lock_irqsave(&fw_report_lock, intr_flags);
	fw_report.st_buf = NULL;
	fw_report.st_size = 0;
	fw_report.wr_pos = 0;
	fw_report.line_cnt = 0;

	spin_unlock_irqrestore(&fw_report_lock, intr_flags);
}

struct npu_store_log_read_obj {
	u32			magic;
	size_t			read_pos;
};

static ssize_t npuerr_in_dmesg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	count = sprintf(buf, "\nvalue = %d\n\
			Valid values:\n\
			0 -> npu errors go into memlogger only\n\
			1 -> npu errors go into dmesg AND memloger)\n",
				npu_log.npu_err_in_dmesg);
	return count;
}

static ssize_t npuerr_in_dmesg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int idx;
	int ret;

	ret = sscanf(buf, "%1d", &idx);
	if (ret != 1) {
		npu_err("parsing error %d\n", ret);
		return -EINVAL;
	}

	if ((idx < 0) || (idx > 1)) {
		npu_err("invalid value: %d\n", idx);
		return -EINVAL;
	}
	npu_log.npu_err_in_dmesg = idx;
	return count;
}
static DEVICE_ATTR(npu_err_in_dmesg, 0644, npuerr_in_dmesg_show, npuerr_in_dmesg_store);

static int npu_fw_report_fops_open(struct inode *inode, struct file *file)
{
	int				ret = 0;
	struct npu_store_log_read_obj	*robj;

	robj = kzalloc(sizeof(*robj), GFP_ATOMIC);
	if (!robj) {
		ret = -ENOMEM;
		goto err_exit;
	}
	robj->read_pos = 0;
	file->private_data = robj;

	npu_info("fd open robj @ %pK\n", robj);
	return 0;

err_exit:
	return ret;
}

static int npu_fw_report_fops_close(struct inode *inode, struct file *file)
{
	struct npu_store_log_read_obj	*robj;

	if (unlikely(!file)) {
		npu_err("Failed to get file\n");
		return -EINVAL;
	}
	robj = file->private_data;
	if (unlikely(!robj)) {
		npu_err("Failed to get robj\n");
		return -EINVAL;
	}

	npu_info("fd close robj @ %pK\n", robj);
	kfree(robj);

	return 0;

}

int npu_fw_report_store(char *strRep, size_t nSize)
{
	size_t	wr_len = 0;
	size_t	remain;
	char	*buf = NULL;
	unsigned long intr_flags;

	spin_lock_irqsave(&fw_report_lock, intr_flags);
	remain = fw_report.st_size - fw_report.wr_pos;

	if (fw_report.st_buf == NULL)
		return -ENOMEM;

	buf = fw_report.st_buf + fw_report.wr_pos;

	//check overflow
	if (nSize >= remain) {
		fw_report.st_buf[fw_report.wr_pos] = '\0';
		fw_report.line_cnt = fw_report.wr_pos;
		fw_report.wr_pos = 0;
		remain = fw_report.st_size;
		buf = fw_report.st_buf;
		fw_report.last_dump_line_cnt++;
	}

	memcpy(buf, strRep, nSize);
	remain -= nSize;
	wr_len += nSize;
	npu_dbg("fw_report nSize : %zu,\t remain = %zu\n", nSize, remain);

	/* Update write position */
	fw_report.wr_pos = (fw_report.wr_pos + wr_len) % fw_report.st_size;
	fw_report.st_buf[fw_report.wr_pos] = '\0';

	spin_unlock_irqrestore(&fw_report_lock, intr_flags);

	return 0;
}

static ssize_t __npu_fw_report_fops_read(struct npu_store_log_read_obj *robj, char *outbuf, const size_t outlen)
{
	size_t	copy_len, buf_end;

	/* Copy data to temporary buffer*/
	if (fw_report.st_buf) {
		buf_end = (robj->read_pos > fw_report.wr_pos) ? fw_report.line_cnt : fw_report.wr_pos;
		copy_len = min(outlen, buf_end - robj->read_pos);
		memcpy(outbuf, fw_report.st_buf + robj->read_pos, copy_len);
	} else
		copy_len = 0;

	if (copy_len > 0) {
		robj->read_pos = (robj->read_pos + copy_len) % fw_report.st_size;
		if (fw_report.line_cnt == robj->read_pos) {
			fw_report.line_cnt = 0;
			robj->read_pos = 0;
			memset(&fw_report.st_buf[fw_report.wr_pos], '\0', (fw_report.st_size - fw_report.wr_pos));
		}
	}

	return copy_len;
}

static ssize_t npu_fw_report_fops_read(struct file *file, char __user *outbuf, size_t outlen, loff_t *loff)
{
	struct npu_store_log_read_obj	*robj;
	ssize_t				ret, copy_len;
	size_t				tmp_buf_len;
	char				*tmp_buf = NULL;
	unsigned long			intr_flags;

	if (unlikely(!file)) {
		npu_err("Failed to get file\n");
		ret = -EINVAL;
		goto err_exit;
	}

	robj = file->private_data;
	if (unlikely(!robj)) {
		npu_err("Failed to get robj\n");
		ret = -EINVAL;
		goto err_exit;
	}

	/* Temporal kernel buffer, to read inside of spinlock */
	tmp_buf_len = min(outlen, PAGE_SIZE);
	tmp_buf = kzalloc(tmp_buf_len, GFP_KERNEL);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto err_exit;
	}

	/* Check data available */
	ret = 0;
	while (ret == 0) {	/* ret = 0 on timeout */
		/* TODO: Accessing npu_log.wr_pos outside of spinlock is potentially dangerous */
		ret = wait_event_interruptible_timeout(fw_report.wq, robj->read_pos != fw_report.wr_pos, 1 * HZ);
		if (ret == -ERESTARTSYS) {
			ret = 0;
			goto err_exit;
		}
	}

	spin_lock_irqsave(&fw_report_lock, intr_flags);
	copy_len = __npu_fw_report_fops_read(robj, tmp_buf, tmp_buf_len);
	spin_unlock_irqrestore(&fw_report_lock, intr_flags);

	if (copy_len > 0) {
		ret = copy_to_user(outbuf, tmp_buf, copy_len);
		if (ret) {
			npu_err("copy_to_user failed : %zd\n", ret);
			ret = -EFAULT;
			goto err_exit;
		}
	}

	ret = copy_len;
err_exit:
	if (tmp_buf)
		kfree(tmp_buf);
	return ret;
}

const struct file_operations npu_fw_report_fops = {
	.owner = THIS_MODULE,
	.open = npu_fw_report_fops_open,
	.release = npu_fw_report_fops_close,
	.read = npu_fw_report_fops_read,
};

int npu_debug_memdump32_by_memcpy(u32 *start, u32 *end)
{
	int j, k, l;
	int ret = 0;
	u32 items;
	u32 *cur;
	char term[4], strHexa[128], strString[128], sentence[256];

	cur = start;
	items = 0;

	memset(sentence, 0, sizeof(sentence));
	memset(strString, 0, sizeof(strString));
	memset(strHexa, 0, sizeof(strHexa));
	j = sprintf(sentence, "[V] Memory Dump32(%pK ~ %pK)", start, end);
	while (cur < end) {
		if ((items % 4) == 0) {
			j += sprintf(sentence + j, "%s   %s", strHexa, strString);
#ifdef DEBUG_LOG_MEMORY
			pr_debug("%s\n", sentence);
#else
			npu_dump("%s\n", sentence);
#endif
			j = 0; items = 0; k = 0; l = 0;
			j = sprintf(sentence, "[V] %pK:      ", cur);
			items = 0;
		}
		memcpy_fromio(term, cur, sizeof(term));
		k += sprintf(strHexa + k, "%02X%02X%02X%02X ",
			term[0], term[1], term[2], term[3]);
		l += sprintf(strString + l, "%c%c%c%c", ISPRINTABLE(term[0]),
			ISPRINTABLE(term[1]), ISPRINTABLE(term[2]), ISPRINTABLE(term[3]));
		cur++;
		items++;
	}
	if (items) {
		j += sprintf(sentence + j, "%s   %s", strHexa, strString);
#ifdef DEBUG_LOG_MEMORY
		pr_debug("%s\n", sentence);
#else
		npu_dump("%s\n", sentence);
#endif
	}
	ret = cur - end;
	return ret;
}

int fw_will_note_to_kernel(size_t len)
{
	unsigned long intr_flags;
	size_t i, pos;
	bool bReqLegacy = FALSE;

	//Gather one more time before make will note.
	npu_log.log_ops->fw_rprt_gather();
	spin_lock_irqsave(&fw_report_lock, intr_flags);

	if (fw_report.st_buf == NULL)
		return -ENOMEM;

	//Consideration for early phase
	if (fw_report.wr_pos < len) {
		len = fw_report.wr_pos;
		bReqLegacy = TRUE;
	}

	pos = 0;
	probe_err("----------- Start will_note for npu_fw (/sys/kernel/debug/npu/fw-report )-------------\n");
	if ((fw_report.last_dump_line_cnt != 0) && (bReqLegacy == TRUE)) {
		pos = fw_report.st_size - (len - fw_report.wr_pos);
		for (i = pos; i < fw_report.st_size; i++) {
			if (fw_report.st_buf[i] == '\n') {
				fw_report.st_buf[i] = '\0';
				probe_err("%s\n", &fw_report.st_buf[pos]);
				fw_report.st_buf[i] = '\n';
				pos = i+1;
			}
		}
		//Change length to current position.
		len = fw_report.wr_pos;
	}
	pos = fw_report.wr_pos - len;
	for (i = pos ; i < fw_report.wr_pos; i++) {
		if (fw_report.st_buf[i] == '\n') {
			fw_report.st_buf[i] = '\0';
			probe_err("%s\n", &fw_report.st_buf[pos]);
			fw_report.st_buf[i] = '\n';
			pos = i+1;
		}
	}

	fw_report.rp_pos = fw_report.wr_pos;

	probe_err("----------- End of will_note for fw -------------\n");
	spin_unlock_irqrestore(&fw_report_lock, intr_flags);

	return 0;
}

int fw_will_note(size_t len)
{
	unsigned long intr_flags;
	size_t i, pos;
	bool bReqLegacy = FALSE;

	//Gather one more time before make will note.
	npu_log.log_ops->fw_rprt_gather();
	spin_lock_irqsave(&fw_report_lock, intr_flags);

	if (fw_report.st_buf == NULL)
		return -ENOMEM;

	//Consideration for early phase
	if (fw_report.wr_pos < len) {
		len = fw_report.wr_pos;
		bReqLegacy = TRUE;
	}

	pos = 0;
	npu_dump("----------- Start will_note for npu_fw (/sys/kernel/debug/npu/fw-report )-------------\n");
	if ((fw_report.last_dump_line_cnt != 0) && (bReqLegacy == TRUE)) {
		pos = fw_report.st_size - (len - fw_report.wr_pos);
		for (i = pos; i < fw_report.st_size; i++) {
			if (fw_report.st_buf[i] == '\n') {
				fw_report.st_buf[i] = '\0';
				npu_dump("%s\n", &fw_report.st_buf[pos]);
				fw_report.st_buf[i] = '\n';
				pos = i+1;
			}
		}
		//Change length to current position.
		len = fw_report.wr_pos;
	}
	pos = fw_report.wr_pos - len;
	for (i = pos ; i < fw_report.wr_pos; i++) {
		if (fw_report.st_buf[i] == '\n') {
			fw_report.st_buf[i] = '\0';
			npu_dump("%s\n", &fw_report.st_buf[pos]);
			fw_report.st_buf[i] = '\n';
			pos = i+1;
		}
	}

	fw_report.rp_pos = fw_report.wr_pos;

	npu_dump("----------- End of will_note for npu_fw -------------\n");
	spin_unlock_irqrestore(&fw_report_lock, intr_flags);
	npu_dump("----------- Check unposted_mbox ---------------------\n");
	npu_log.log_ops->npu_check_unposted_mbox(ECTRL_LOW);
	npu_log.log_ops->npu_check_unposted_mbox(ECTRL_MEDIUM);
	npu_log.log_ops->npu_check_unposted_mbox(ECTRL_HIGH);
	npu_log.log_ops->npu_check_unposted_mbox(ECTRL_ACK);
	npu_log.log_ops->npu_check_unposted_mbox(ECTRL_NACK);
	npu_log.log_ops->npu_check_unposted_mbox(ECTRL_REPORT);
	npu_dump("----------- Done unposted_mbox ----------------------\n");

	return 0;
}

#define DEACTIVE_INTERVAL	(10000000) // 10 second
static void npu_log_deactive_session_memory_info(struct npu_device *device)
{
	s64 now = 0, deactive_time = 0;
	struct npu_session *cur = NULL;
	struct npu_sessionmgr *sess_mgr;
	struct npu_memory_manual *manual;
	struct npu_vertex_ctx *vctx = NULL;

	manual = device->system.manual;
	sess_mgr = &device->sessionmgr;

	npu_memory_manual_health(manual);

	mutex_lock(&sess_mgr->active_list_lock);
	list_for_each_entry(cur, &sess_mgr->active_session_list, active_session) {
		now = npu_get_time_us();
		deactive_time = now - cur->last_qbuf_arrival;
		if (deactive_time < DEACTIVE_INTERVAL)
			continue;

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
		if (cur->hids & NPU_HWDEV_ID_DSP)
			continue;
#endif
		vctx = &cur->vctx;
		if (!(vctx->state & BIT(NPU_VERTEX_FORMAT)))
			continue;

		/* This session is not triggered by frame request over 10 second */
		npu_health("--------- session(%u) NCP/IOFM/IMB memory ----------\n", cur->uid);
		npu_health("model name : %s\n", cur->model_name);
		npu_memory_buffer_health_dump(cur->ncp_hdr_buf);
		npu_memory_buffer_health_dump(cur->IOFM_mem_buf);
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		npu_memory_buffer_health_dump(cur->IMB_mem_buf);
#endif
	}
	mutex_unlock(&sess_mgr->active_list_lock);

	queue_delayed_work(device->npu_log_wq,
			&device->npu_log_work,
			msecs_to_jiffies(10000));
}

static void npu_log_health_monitor(struct work_struct *work)
{
	struct npu_device *npu_device;
	struct dsp_dhcp *dhcp;

	if (atomic_read(&npu_log.npu_log_active) != NPU_LOG_ACTIVE)
		return;

	npu_device = container_of(work, struct npu_device, npu_log_work.work);

	if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev > 1)) {
		npu_log_deactive_session_memory_info(npu_device);
		return;
	}

	dhcp = npu_device->system.dhcp;

	queue_delayed_work(npu_device->npu_log_wq,
			&npu_device->npu_log_work,
			msecs_to_jiffies(60000));
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
static const char *npu_fw_governor_get_domain(u8 domain) {
	const char *domain_name;
	switch(domain) {
		case DM_MIF:
			domain_name = "DM_MIF";
			break;
		case DM_INT:
			domain_name = "DM_INT";
			break;
		case DM_NPU:
			domain_name = "DM_NPU";
			break;
		case DM_DSP:
			domain_name = "DM_DSP";
			break;
		case DM_DNC:
			domain_name = "DM_DNC";
			break;
		case DM_UNPU:
			domain_name = "DM_UNPU";
			break;
		default:
			domain_name = "DM_WRONG";
			break;
	}

	return domain_name;
}

int npu_log_fw_governor(struct npu_device *device) {
	int ret = 0;
	struct npu_memory_buffer *dvfs_log_buf;
	struct apm_trace *apm_trace;
	u32 time;
	u8 direction;
	u8 type;
	u8 domain;
	u32 freq;
	int i = 0;

	dvfs_log_buf = npu_get_mem_area(&device->system, "CMDQ_TABLE");
	if (!dvfs_log_buf) {
		probe_err("\n CMDQ Table buffer not present\n");
		ret = -ENOMEM;
		return ret;
	}

	apm_trace = (struct apm_trace *)dvfs_log_buf->dma.vaddr;

	npu_fw_governor_log("--------- fw governor apm request trace ----------\n");
	for(i = 0; i * 8 < 0x3000; i++) {
		time = apm_trace[i].time;
		direction = apm_trace[i].direction;
		type = apm_trace[i].type;
		domain = apm_trace[i].domain;
		freq = apm_trace[i].freq << 1;
		if (time == 0) continue;

		npu_fw_governor_log("[%8d.%03d] %s %s %s %d\n", time / 1000, time % 1000,
				direction ? "[Respond]" : "[Request]", npu_fw_governor_get_domain(domain),
				type ? "Minlock": "Maxlock", freq);
	}

	return ret;
}

int npu_log_fw_governor_probe(struct npu_device *device){
	int ret = 0;
	struct npu_memory_buffer *dvfs_log_buf;
	struct dhcp_table *dhcp_table;

	dhcp_table = device->system.dhcp_table;

	dvfs_log_buf = npu_get_mem_area(&device->system, "CMDQ_TABLE");
	if (!dvfs_log_buf) {
		probe_err("\n CMDQ Table buffer not present\n");
		ret = -ENOMEM;
		return ret;
	}

	dhcp_table->DVFS_TABLE_ADDR = dvfs_log_buf->dma.daddr;

	return ret;
}
#endif

/* Exported functions */
int npu_log_probe(struct npu_device *npu_device)
{
	int ret = 0;

	/* Basic initialization of log store */
	npu_log.st_buf = NULL;
	npu_log.st_size = 0;
	npu_log.wr_pos = 0;
	npu_log.log_ops = &npu_log_ops;
	init_waitqueue_head(&npu_log.wq);
	init_waitqueue_head(&fw_report.wq);

	npu_log_rmemlog(npu_device);

	atomic_set(&npu_log.npu_log_active, NPU_LOG_DEACTIVE);

	/* Register FW log keeper */
	ret = npu_debug_register("fw-report", &npu_fw_report_fops);
	if (ret) {
		npu_err("npu_debug_register error : ret = %d\n", ret);
		return ret;
	}

	npu_log.npu_err_in_dmesg = 0;
	ret = device_create_file(npu_device->dev, &dev_attr_npu_err_in_dmesg);
	if (ret) {
		npu_err("npu_debug_register error : ret = %d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&npu_device->npu_log_work, npu_log_health_monitor);

	npu_device->npu_log_wq = create_singlethread_workqueue(dev_name(npu_device->dev));
	if (!npu_device->npu_log_wq) {
		probe_err("fail to create workqueue -> npu_device->npu_log_wq\n");
		ret = -EFAULT;
	}

	return ret;
}

int npu_log_release(struct npu_device *npu_device)
{
	return 0;
}

int npu_log_open(struct npu_device *npu_device)
{
	if (exynos_soc_info.main_rev != 1)
		goto done;

	atomic_set(&npu_log.npu_log_active, NPU_LOG_ACTIVE);

	queue_delayed_work(npu_device->npu_log_wq,
			&npu_device->npu_log_work,
			msecs_to_jiffies(1000));

done:
	return 0;
}

int npu_log_close(struct npu_device *npu_device)
{
	atomic_set(&npu_log.npu_log_active, NPU_LOG_DEACTIVE);
	cancel_delayed_work_sync(&npu_device->npu_log_work);

	if ((exynos_soc_info.main_rev == 1) && (exynos_soc_info.sub_rev > 1))
		goto done;

done:
	return 0;
}
