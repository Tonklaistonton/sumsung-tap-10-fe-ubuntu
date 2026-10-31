/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <soc/samsung/exynos/psp/psp_mailbox_common.h>
#include <soc/samsung/exynos/psp/psp_mailbox_ree.h>
#include <soc/samsung/exynos/psp/psp_error.h>
#include <soc/samsung/exynos/psp/psp_mailbox_sfr.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/exynos/debug-snapshot.h>
#endif
#include "logsink.h"
#include "utils.h"

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/exynos/imgloader.h>
#include <linux/firmware.h>
#endif

/*
 * Note: if macro for converting physical address to page is not defined
 * in the kernel itself, it is defined hereby. This is to avoid build errors
 * which are reported during builds for some architectures.
 */
#ifndef phys_to_page
#define phys_to_page(phys) (pfn_to_page(__phys_to_pfn(phys)))
#endif

#include "custos.h"
#include "log.h"
#include "data_path.h"
#include "control_path.h"
#include "mailbox.h"
#include "logsink.h"

MODULE_LICENSE("Dual BSD/GPL");

#define RAMDUMP_OFFSET 0x240000
#define RAMDUMP_SIZE 0x40000
#define MAX_TRY_COUNT 10000000
#define PORT_MAX_TRY 100
#define IMG_LOADER_COUNT 0x1
#define UUID_LEN 36
#define APP_UUID "e0000000-f5c8-5466-bd71-77e927df370c"

struct custos_reserved_memory revmem = { 0, 0 };

static const char device_name[] = "custos_iwc";
struct custos_device custos_dev;

static struct miscdevice iwc_dev;
struct imgloader_desc img_loader[IMG_LOADER_COUNT];

const char* blob[] = {
	"cp_security_app.cpk",
};

extern atomic_t pause_flag;

#if defined(CONFIG_CUSTOS_POLLING)
extern void __iomem *mb_va_base;

void print_debug_info(uint32_t resume_flag)
{
	LOG_ERR("pause flag is 0x%x, resume_flag is 0x%x", atomic_read(&pause_flag), resume_flag);
	psp_print_debug_sfr();
}

void custos_iwc_work(uint8_t *receive_buf, uint32_t* rx_total_len, uint32_t* rx_remain_len)
{
	int count = 0;
	unsigned int status = 0;
	uint32_t ret = 0;
	uint32_t remain_len = 0;
	uint32_t resume_flag = 0;

	while (1) {
		if (count++ > MAX_TRY_COUNT) {
			LOG_ERR("%s over MAX_TRY_COUNT", __func__);
			print_debug_info(resume_flag);
			break;
		}

		status = read_sfr(mb_va_base + PSP_MB_INTSR0_OFFSET);

		if (status & PSP_MB_INTGR0_ON) {
			ret = mailbox_receive_and_callback(receive_buf, rx_total_len, rx_remain_len);
			remain_len = ret & REMAIN_LEN_MASK;
			remain_len = remain_len >> REMAIN_SHFIT;
			ret = ret & RETURN_MASK;

			if (ret) {
		                LOG_ERR("%s [ret: 0x%X]", __func__, ret);
				print_debug_info(resume_flag);
			}

			mb();
	                write_sfr(mb_va_base + PSP_MB_INTCR0_OFFSET, PSP_MB_INTGR0_ON);
			mb();

			if (remain_len == 0) {
				if (atomic_read(&pause_flag) == PAUSE_FLAG_FALSE) {
					if (resume_flag == RESUME_FLAG_TRUE) {
						LOG_INFO("%s Resume message is comming, Next message is working.", __func__);
						resume_flag = RESUME_FLAG_FALSE;
					} else {
						LOG_DBG("%s mailbox receive msg all block done.", __func__);
						break;
					}
				} else {
					LOG_INFO("%s REE driver is paused, Resume message is yet", __func__);
					resume_flag = RESUME_FLAG_TRUE;
					msleep(1);
					continue;
				}
			}
		}
	}
}
#endif

static ssize_t custos_read(struct file *file, char __user *buffer,
							 size_t cnt,
							 loff_t *pos)
{
	struct custos_client *client =
		(struct custos_client *)file->private_data;
	int is_empty;
	int ret;

	LOG_ENTRY;

	if (file->f_flags & O_NONBLOCK) {
		is_empty = custos_is_data_msg_queue_empty(client->peer_id);
		if (is_empty) {
			LOG_ERR("No data for non-blocking read");
			return -EAGAIN;
		}
	}

	ret = custos_read_data_msg_timeout(client->peer_id, buffer, cnt, client->timeout_jiffies);

	if (ret < 0)
		LOG_ERR("%s function Failed ret = %d", __func__, ret);

	return ret;
}

static ssize_t custos_write(struct file *file, const char __user *buffer,
							  size_t cnt, loff_t *pos)
{
	int msg_len = 0;
	int ret = 0;
	size_t sent_msg_len = 0;
	struct custos_client *client =
		(struct custos_client *)file->private_data;
	struct custos_crc_data_msg *msg = NULL;

	LOG_ENTRY;

	if (cnt < sizeof(struct custos_data_msg_header))
		return -EINVAL;

	// crc + flag + (header + payload)
	msg_len = sizeof(uint32_t) + sizeof(struct custos_msg_flag) + cnt;

	msg = kmalloc(msg_len, GFP_KERNEL);
	if (!msg) {
		LOG_ERR("Unable to allocate message buffer");
		return -ENOMEM;
	}

	if (copy_from_user(&msg->data.header, buffer, cnt)) {
		LOG_ERR("Unable to copy request msg buffer");
		ret = -EAGAIN;
		goto free_memory;
	}

	if (msg_len != sizeof(uint32_t) + msg->data.header.length +
		sizeof(struct custos_msg_flag) +
		sizeof(struct custos_data_msg_header)) {
		ret = -EINVAL;
		goto free_memory;
	}

	sent_msg_len = custos_send_data_msg(client->peer_id, msg);
	if (sent_msg_len != msg_len) {
		LOG_ERR("sent msg leng is not matched, sent len = (%zu)", sent_msg_len);
		if (sent_msg_len > 0)
			ret = sent_msg_len - sizeof(struct custos_msg_flag) - sizeof(uint32_t);
		else
			ret = -EAGAIN;
	} else
		ret = cnt;

free_memory:
	kfree(msg);

	return ret;
}

unsigned int custos_poll(struct file *file,
						   struct poll_table_struct *poll_tab)
{
	struct custos_client *client =
		(struct custos_client *)file->private_data;

	LOG_ENTRY;

	return custos_poll_wait_data_msg(client->peer_id, file, poll_tab);
}

void custos_vma_open(struct vm_area_struct *vma)
{
	/* Does Nothing */
}

void custos_vma_close(struct vm_area_struct *vma)
{
	struct custos_memory_entry *mem_entry = NULL;
	unsigned long flags;
	int value;

	mutex_lock(&custos_dev.vm_lock);

	mem_entry = vma->vm_private_data;

	LOG_ENTRY;
	if (mem_entry == NULL) {
		LOG_ERR("Fail to get memory information");
		goto out;
	}

	LOG_DBG("Release memory area %lx - %lx, handle = %llx", vma->vm_start,
			vma->vm_end, mem_entry->handle);
	if (mem_entry->client->peer_id != current->pid) {
		LOG_ERR("Invalid process try to free, owner = (%d), current = (%d)",
				mem_entry->client->peer_id, current->pid);
		goto out;
	}

	LOG_DBG("shm status, owner = (%d)", atomic_read(&custos_dev.shm_user));

	value = mem_entry->client->peer_id;
	if (atomic_try_cmpxchg(&custos_dev.shm_user, &value, SHM_USE_NO_ONE)) {
		LOG_INFO("Release shared memory. owner = (%d)",
				 mem_entry->client->peer_id);
		wake_up_interruptible(&custos_dev.shm_wait_queue);
	}

	spin_lock_irqsave(&mem_entry->client->shmem_list_lock, flags);
	list_del(&mem_entry->node);
	spin_unlock_irqrestore(&mem_entry->client->shmem_list_lock, flags);

	kfree(mem_entry);
	vma->vm_private_data = NULL;

out:
	mutex_unlock(&custos_dev.vm_lock);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
static vm_fault_t custos_vma_fault(struct vm_fault *vmf)
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(4, 10, 0))
static int custos_vma_fault(struct vm_fault *vmf)
#else
static int custos_vma_fault(struct vm_area_struct *vma,
							  struct vm_fault *vmf)
#endif
{
	return VM_FAULT_SIGBUS;
}

static const struct vm_operations_struct custos_vm_ops = {
	.open = custos_vma_open,
	.close = custos_vma_close,
	.fault = custos_vma_fault
};

static bool check_shm_owner_exchange(unsigned int pid)
{
	unsigned int value = SHM_USE_NO_ONE;
	return atomic_try_cmpxchg(&custos_dev.shm_user, &value, pid);
}

static int custos_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct custos_client *client =
		(struct custos_client *)file->private_data;
	struct custos_memory_entry *mem_entry = NULL;
	unsigned long uaddr = vma->vm_start & PAGE_MASK;
	unsigned long usize = ((vma->vm_end - uaddr) + (PAGE_SIZE - 1)) &
						  PAGE_MASK;
	unsigned long pfn = (revmem.base >> PAGE_SHIFT) + vma->vm_pgoff;
	int ret = 0;
	int shm_setting_flag = SHM_SETTING_FLAG_FALSE;

	LOG_DBG("uaddr: 0x%lx, size: 0x%lx, vma->vm_start: 0x%lx, vma->vm_end: 0x%lx, vma->vm_pgoff: 0x%lx",
			uaddr, usize, vma->vm_start, vma->vm_end, vma->vm_pgoff << PAGE_SHIFT);

	/* TODO: separate operations into launch and log */
	if ((vma->vm_pgoff >= SHM_START_PAGE)
		&& (vma->vm_pgoff < (SHM_START_PAGE + (SHM_SIZE >> PAGE_SHIFT)))) {
		unsigned long flag = 0;

		/* map as non-cacheable memory */
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

		if (remap_pfn_range(vma, vma->vm_start, pfn, usize,
					vma->vm_page_prot) != 0) {
			LOG_ERR("Fail to remap pages");
			return -EINVAL;
		}

		LOG_DBG("allocated_memory's PA(mem_info) = 0x%lx", (pfn << PAGE_SHIFT));

		mem_entry = kmalloc(sizeof(struct custos_memory_entry), GFP_KERNEL);
		if (mem_entry == NULL) {
			LOG_ERR("Unable to allocate memory list entry");
			return -ENOMEM;
		}

		LOG_DBG("shm status, owner = (%d)", atomic_read(&custos_dev.shm_user));

		spin_lock_irqsave(&custos_dev.shm_wait_queue.lock, flag);
		ret = wait_event_interruptible_timeout_locked(custos_dev.shm_wait_queue,
											   check_shm_owner_exchange(client->peer_id), msecs_to_jiffies(30000));
		spin_unlock_irqrestore(&custos_dev.shm_wait_queue.lock, flag);
		if (ret == 0) {
			LOG_ERR("Peer %u's waiting for mmap was timeout", client->peer_id);
			kfree(mem_entry);
			return -ETIME;
		} else if (ret == -ERESTARTSYS) {
			LOG_ERR("Peer %u's waiting for mmap was canceled", client->peer_id);
			kfree(mem_entry);
			return -ERESTARTSYS;
		}
		shm_setting_flag = SHM_SETTING_FLAG_TRUE;
		LOG_INFO("shared memory success. id = (%d), owner = (%d)",
				 client->peer_id, atomic_read(&custos_dev.shm_user));
	}

	if (mem_entry == NULL) {
		LOG_ERR("Unable to allocate memory list entry");
		return -ENOMEM;
	}
	mem_entry->size = usize;
	mem_entry->addr = vma->vm_start;
	mem_entry->handle = (pfn << PAGE_SHIFT);

	mem_entry->client = client;
	spin_lock(&client->shmem_list_lock);
	list_add_tail(&mem_entry->node, &client->shmem_list);
	spin_unlock(&client->shmem_list_lock);

	vma->vm_ops = &custos_vm_ops;
	vma->vm_private_data = mem_entry;

	return 0;
}

static long custos_memory_unwrap(struct custos_client *client,
								   struct custos_memory_pair *udata)
{
	struct custos_memory_pair pair;
	struct custos_memory_entry *elem;
	int found = 0;

	if (copy_from_user(&pair, udata, sizeof(struct custos_memory_pair))) {
		LOG_ERR("Fail to get pair data from user.");
		return -EFAULT;
	}

	LOG_DBG("pair.address = %lx", pair.address);

	spin_lock(&client->shmem_list_lock);
	list_for_each_entry (elem, &client->shmem_list, node) {
		if (elem->addr == pair.address) {
			LOG_DBG("target address = %lx, handle = %llx",
					elem->addr, elem->handle);
			pair.handle = elem->handle;
			found = 1;
			break;
		}
	}
	spin_unlock(&client->shmem_list_lock);

	if (found == 0) {
		LOG_INFO("No memory found");
		pair.handle = 0;
	}

	if (copy_to_user(udata, &pair, sizeof(struct custos_memory_pair))) {
		LOG_ERR("Fail to deliver result to user.");
		return -EFAULT;
	}

	return 0;
}

static long custos_ioctl(struct file *file, unsigned int cmd,
						   unsigned long arg)
{
	long res = 0;
	struct custos_client *client =
		(struct custos_client *)file->private_data;

	switch (cmd) {
	case CUSTOS_IOCTL_SET_TIMEOUT: {
		uint64_t time_value = 0;
		unsigned long flags;
		if (copy_from_user(&time_value, (void __user *)arg, sizeof(uint64_t))) {
			LOG_ERR("Fail to get time.");
			return -EFAULT;
		}
		LOG_DBG("timeout value is (%llu)", time_value);
		spin_lock_irqsave(&client->timeout_set_lock, flags);
		if (time_value == 0)
			client->timeout_jiffies = LONG_MAX;

		else
			client->timeout_jiffies = msecs_to_jiffies(10000);
		spin_unlock_irqrestore(&client->timeout_set_lock, flags);
		res = 0;
		break;
	}
	case CUSTOS_IOCTL_GET_HANDLE:
		res = custos_memory_unwrap(client, (void __user *)arg);
		break;
	case CUSTOS_IOCTL_GET_VERSION: {
		struct custos_version version_info;
		version_info.major = MAJOR_VERSION;
		version_info.minor = MINOR_VERSION;
		if (copy_to_user((void __user *)arg, &version_info,
						 sizeof(struct custos_version))) {
			LOG_ERR("Fail to deliver result to user.");
			return -EFAULT;
		}
		break;
	}
	default:
		LOG_ERR("Unknown ioctl command: %d", cmd);
		res = -EINVAL;
	}
	return res;
}

struct custos_client *custos_create_client(unsigned int peer_id)
{
	struct custos_client *client = NULL;

	client = kzalloc(sizeof(struct custos_client), GFP_KERNEL);
	if (!client) {
		LOG_ERR("Can't alloc client context (-ENOMEM)");
		return NULL;
	}

	INIT_LIST_HEAD(&client->shmem_list);

	spin_lock_init(&client->shmem_list_lock);
	spin_lock_init(&client->timeout_set_lock);

	client->peer_id = peer_id;
	client->timeout_jiffies = LONG_MAX;

	return client;
}

void custos_destroy_client(struct custos_client *client)
{
	unsigned long flags;
	struct custos_memory_entry *mem_entry = NULL;
	spin_lock_irqsave(&client->shmem_list_lock, flags);
	while (!list_empty(&client->shmem_list)) {
		mem_entry = list_first_entry(&client->shmem_list,
									 struct custos_memory_entry, node);
		list_del(&mem_entry->node);
		kfree(mem_entry);
	}
	spin_unlock_irqrestore(&client->shmem_list_lock, flags);
	kfree(client);
}

static int custos_open(struct inode *inode, struct file *file)
{
	struct custos_client *client;
	unsigned int peer_id;
	int ret;

	peer_id = task_pid_nr(current);
	ret = custos_data_peer_create(peer_id);
	if (ret) {
		LOG_ERR("Failed to create peer %u", peer_id);
		return -EINVAL;
	}

	client = custos_create_client(peer_id);
	if (client == NULL) {
		custos_data_peer_destroy(peer_id);
		LOG_ERR("Failed to create client");
		return -EINVAL;
	}

	file->private_data = client;

	return 0;
}

static int custos_release(struct inode *inode, struct file *file)
{
	struct custos_client *client =
		(struct custos_client *)file->private_data;
	int value = client->peer_id;

	LOG_ENTRY;

	LOG_DBG("shm status, owner = (%d)", atomic_read(&custos_dev.shm_user));
	if (atomic_try_cmpxchg(&custos_dev.shm_user, &value, SHM_USE_NO_ONE)) {
		LOG_INFO("Release shm at close. owner = (%d)", client->peer_id);
		wake_up_interruptible(&custos_dev.shm_wait_queue);
	}

	custos_data_peer_destroy(client->peer_id);
	custos_destroy_client(client);

	LOG_EXIT;
	return 0;
}

void *custos_request_region(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages = NULL;
	void *v_addr = NULL;

	if (!addr)
		return NULL;

	pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return NULL;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);

	return v_addr;
}

static const struct of_device_id custos_of_match_table[] = {
	{
		.compatible = "exynos,custos",
	},
	{},
};

static int custos_memory_setup(struct device_node **dn)
{
	struct reserved_mem *rmem;
	*dn = of_find_matching_node(NULL, custos_of_match_table);
	if (!(*dn) || !of_device_is_available(*dn)) {
		LOG_ERR("custos node is not available");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(*dn);
	if (!rmem) {
		LOG_ERR("failed to acquire memory region");
		return -EINVAL;
	}

	revmem.base = rmem->base;
	revmem.size = rmem->size;
	LOG_DBG("Found reserved memory: %lx - %lx", revmem.base, revmem.size);

	return 0;
}

static int custos_ram_dump_setup(void)
{
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	unsigned long log_base = revmem.base + LOG_OFFSET;
	unsigned long log_size = LOG_SIZE;
	int result = -1;

	result = dbg_snapshot_add_bl_item_info(LOG_CUSTOS, log_base, log_size);
	if (result)
		return result;

	log_base = revmem.base + PM_DUMP_OFFSET;
	log_size = PM_DUMP_SIZE;
	result = dbg_snapshot_add_bl_item_info(LOG_CUST_PM, log_base, log_size);
	if (result)
		return result;

#if IS_ENABLED(CONFIG_SACPM_DUMP)
	result = dbg_snapshot_add_bl_item_info(LOG_CUST_SACPM, SACPM_DUMP_DRAM_BASE, SACPM_DUMP_SIZE);
	if (result)
		return result;
#endif
#endif
	return 0;
}

static struct file_operations custos_fops = {
	.owner = THIS_MODULE,
	.read = custos_read,
	.write = custos_write,
	.poll = custos_poll,
	.mmap = custos_mmap,
	.unlocked_ioctl = custos_ioctl,
	.open = custos_open,
	.release = custos_release,
};

static ssize_t custos_dbg_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	int index = 0;
	int cnt = 0;

	index += sprintf(buf, "%s: free cache cnt:%d \n",
					 __func__, custos_cache_cnt(1));

	write_sfr(mb_va_base + PSP_MB_INTGR1_OFFSET, IWC_DEBUG);

	do {
		if (read_sfr(mb_va_base + PSP_MB_INTSR1_OFFSET) == 0) {
			break;
		}
		udelay(100);
	} while (cnt++ < 100);
	index += sprintf(buf, "%s: debug requested: cnt:%d\n", __func__, cnt);
	custos_log_work();
	return index;
}

static ssize_t custos_dbg_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t count)
{
	return count;
}

static ssize_t custos_log_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	custos_log_work();

	return 0;
}

static ssize_t custos_download_image_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return 0;
}

static ssize_t custos_download_image_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int idx = 0;
	int ret = 0;

	for (; idx < IMG_LOADER_COUNT ; idx++) {
		ret = imgloader_boot(&img_loader[idx]);
		if (ret)
			goto out;
	}

out:
	if (ret)
		LOG_ERR("fail(%d) in imgloader_boot in custos_download_image idx:(%d),", ret, idx);
	return count;
}

static struct device_attribute attributes[] = {
	__ATTR(dbg, 0664, custos_dbg_show, custos_dbg_store),
	__ATTR(log, 0440, custos_log_show, NULL),
	__ATTR(loader, 0664, custos_download_image_show, custos_download_image_store),
};

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)

unsigned long get_load_phys_addr(void) {
	unsigned long addr = revmem.base + SHM_START_ADDR;
	return addr;
}

int custos_load_cp_app(u64 addr, u64 bin_size)
{
	int ret = 0;
	int msg_len = 0;
	size_t sent_msg_len = 0;
	struct custos_launch_msg *launch_msg = NULL;

	msg_len = sizeof(struct custos_launch_msg);

	LOG_INFO("custos_launch_msg size is %d", msg_len);

	launch_msg = kmalloc(msg_len, GFP_KERNEL);
	if (!launch_msg) {
		LOG_ERR("Unable to allocate message buffer");
		return -ENOMEM;
	}

	if (msg_len != sizeof(struct custos_launch_msg)) {
		ret = -EINVAL;
		goto free_memory;
	}

	launch_msg->launch_header.memory_addr = addr;
	launch_msg->launch_header.addr_length = (u32)bin_size;

	sent_msg_len = custos_send_launch_msg(launch_msg);
	if (sent_msg_len != msg_len) {
		LOG_ERR("launch sent msg leng is not matched, sent len = (%zu)", sent_msg_len);
		if (sent_msg_len > 0)
			ret = sent_msg_len - sizeof(struct custos_msg_flag);
		else
			ret = -EAGAIN;
	}

	LOG_INFO("launch message send & receive done");

free_memory:
	kfree(launch_msg);

	return ret;
}

static int custos_app_memcpy(const char *name, const u8 *data, size_t data_size)
{
	unsigned long phys = get_load_phys_addr();
	int size = round_up(data_size, PAGE_SIZE);
	u8 *dst = custos_request_region(phys, size);

	if (dst == NULL) {
		LOG_ERR("Failed to map copy area in custos_app_memcpy");
		return -ENOMEM;
	}

	LOG_INFO("%s: %s size:(align(%zu) -> %d)\n", __func__, name, data_size, size);

	if (memcpy(dst, data, data_size) == NULL){
		return -ENOMEM;
	}

	dma_sync_single_for_device(iwc_dev.this_device, phys, size, DMA_TO_DEVICE);

	return memcmp(dst, data, data_size);
}

static int cpapp_imgloader(struct imgloader_desc *desc,
				   const u8 *metadata, size_t size,
				   phys_addr_t *fw_phys_base,
				   size_t *fw_bin_size, size_t *fw_mem_size)
{
	int ret = 0;
	unsigned long cp_app_addr = get_load_phys_addr();

	LOG_ENTRY;

	ret = custos_app_memcpy(desc->fw_name, metadata, size);

	if (ret != RV_SUCCESS) {
		LOG_ERR("%s is app_memcpy failed(%d)", desc->fw_name, ret);
		goto out;
	}

	ret = custos_load_cp_app(cp_app_addr, size);

	/* This is meaningless, but imgloader check this value is zero or not.*/
	*fw_phys_base = get_load_phys_addr();
	*fw_bin_size = size;
	*fw_mem_size = 0x10;

out:
	return ret;
}

static struct imgloader_ops custos_imaloader_ops[IMG_LOADER_COUNT] = {
	{
		.mem_setup = cpapp_imgloader,
		.verify_fw = NULL,
	},
};

static int custos_imgloader_desc_init(struct device *dev,
					  struct imgloader_ops *ops,
				      struct imgloader_desc *desc,
				      const char *name, int id)
{
	desc->dev = dev;
	desc->owner = THIS_MODULE;
	desc->ops = ops;
	desc->name = "CUSTOS";
	desc->s2mpu_support = false;
	desc->fw_name = name;
	desc->fw_id = id;

	return imgloader_desc_init(desc);
}

int custos_imgloader_init(struct device *dev)
{
	int idx = 0;
	int ret = 0;

	for (; idx < IMG_LOADER_COUNT ; idx++) {
		ret = custos_imgloader_desc_init(dev, &custos_imaloader_ops[idx], &img_loader[idx], blob[idx], idx);
		if (ret)
			goto out;
	}

out:
	if (ret)
		LOG_ERR("Failed custos imgloader init idx:(%d), ret:(%d)", idx, ret);

	return ret;
}

#endif //   IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)

static int custos_init(void)
{
	int result = 0;
	int i;
	struct device_node *np = NULL;

	result = custos_data_path_init();
	if (result < 0) {
		LOG_ERR("Failed to setup custos data path");
		return result;
	}

	result = custos_memory_setup(&np);
	if (result < 0) {
		LOG_ERR("Failed to setup custos memory");
		return result;
	}

	mutex_init(&custos_dev.vm_lock);
	mutex_init(&custos_dev.lock);
	spin_lock_init(&custos_dev.iwc_lock);
	mutex_init(&custos_dev.logsink_lock);
	atomic_set(&custos_dev.shm_user, SHM_USE_NO_ONE);
	init_waitqueue_head(&custos_dev.shm_wait_queue);

	iwc_dev.minor = MISC_DYNAMIC_MINOR;
	iwc_dev.fops = &custos_fops;
	iwc_dev.name = device_name;

	result = misc_register(&iwc_dev);
	if (result < 0) {
		LOG_ERR("Failed to register custos device: %i", result);
		return result;
	}
	LOG_DBG("Registered custos communication device %d", iwc_dev.minor);

	result = custos_mailbox_init();
	if (result < 0) {
		LOG_ERR("Failed to initialize mailbox: %i", result);
		return result;
	}

	result = custos_ram_dump_setup();
	if (result != 0) {
		LOG_ERR("Failed to add ram dump: %i", result);
		return result;
	}

	result = custos_memlog_register();
	if (result != 0) {
		LOG_ERR("Failed to register memlog: %i", result);
		return result;
	}

	result = custos_log_init(&custos_dev);
	if (result != 0) {
		LOG_ERR("Failed to initialize log: %i", result);
		return result;
	}

	iwc_dev.this_device->of_node = np;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	result = custos_imgloader_init(iwc_dev.this_device);
	if (result != 0) {
		LOG_ERR("Failed to custos_imgloader_init: %i", result);
		return result;
	}
#else
	LOG_ERR("CONFIG_EXYNOS_IMGLOADER is not enabled");
#endif

	/* Add attributes */
	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
		pr_info("Add attribute: %s\n", attributes[i].attr.name);
		result = device_create_file(iwc_dev.this_device, &attributes[i]);
		if (result)
			pr_err("Failed to create file: %s\n", attributes[i].attr.name);
	}
	pr_info("Add attribute Done: %s\n", attributes[0].attr.name);

	return 0;
}

static void custos_exit(void)
{
	int msg_len = 0;
	size_t sent_msg_len = 0;
	struct custos_destroy_msg *destroy_msg = NULL;

	msg_len = sizeof(struct custos_destroy_msg);

	LOG_INFO("custos_destroy_msg size is %d", msg_len);

	destroy_msg = kmalloc(msg_len, GFP_KERNEL);
	if (destroy_msg == NULL)
		LOG_ERR("Unable to allocate message buffer");

	if (msg_len != sizeof(struct custos_destroy_msg))
		goto free_memory;

	destroy_msg->destroy_header.uuid_length = UUID_LEN;
	memcpy(destroy_msg->destroy_header.uuid, APP_UUID, destroy_msg->destroy_header.uuid_length);

	sent_msg_len = custos_send_destroy_msg(destroy_msg);
	if (sent_msg_len != msg_len)
		LOG_ERR("destroy sent msg leng is not matched, sent len = (%zu)", sent_msg_len);

	LOG_INFO("destroy message send & receive done");

free_memory:
	kfree(destroy_msg);

	LOG_DBG("Deregistered custos communication device %d", iwc_dev.minor);

	misc_deregister(&iwc_dev);

	custos_mailbox_deinit();
	custos_data_path_deinit();
}

module_init(custos_init);
module_exit(custos_exit);
