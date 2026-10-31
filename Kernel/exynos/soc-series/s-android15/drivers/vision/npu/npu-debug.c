/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/bitops.h>

#include "npu-debug.h"
#include "npu-device.h"
#include "npu-ver.h"
#include "npu-system.h"
#include "dsp-dhcp.h"
#include "npu-util-regs.h"
#include "npu-log.h"
#include "npu-memory-manual.h"

#define DEBUG_FS_ROOT_NAME "npu"
#define DEBUG_FS_LAYER_INFO_NAME "layer_range"
#define DEBUG_FS_COLD_BOOT_NAME "cold_boot"
#define DEBUG_FS_S2D_MODE_NAME "s2d_mode"
#define DEBUG_FS_DUMP_MODE_NAME "dump_mode"
#define DEBUG_FS_APG_STATE_NAME "apg_state"
#define DEBUG_FS_APG_DUMP_ON_NAME "apg_dump_on"

#define DEBUG_FS_CLOCK_DNC_MIN_NAME "dnc_min"
#define DEBUG_FS_CLOCK_DNC_MAX_NAME "dnc_max"
#define DEBUG_FS_CLOCK_NPU_MIN_NAME "npu_min"
#define DEBUG_FS_CLOCK_NPU_MAX_NAME "npu_max"
#define DEBUG_FS_CIDLE_NAME "cidle"
#define DEBUG_FS_EXPAND_DVA "expand_dva"

#ifdef CONFIG_NPU_DEBUG_MEMDUMP
#define DEBUG_FS_MEMDUMP_NAME "memdump"
#endif

#define NPU_DEBUG_FILENAME_LEN 32 /* Maximum file name length under npu */

/* Singleton reference to debug object */
static struct npu_debug *npu_debug_ref;

typedef enum {
	FS_READY = 0,
} npu_debug_state_bits_e;

static inline void set_state_bit(npu_debug_state_bits_e state_bit)
{
	int old = test_and_set_bit(state_bit, &(npu_debug_ref->state));

	if (old)
		npu_warn("state(%d): state set requested but already set.", state_bit);
}

static inline void clear_state_bit(npu_debug_state_bits_e state_bit)
{
	int old = test_and_clear_bit(state_bit, &(npu_debug_ref->state));

	if (!old)
		npu_warn("state(%d): state clear requested but already cleared.", state_bit);
}

static inline int check_state_bit(npu_debug_state_bits_e state_bit)
{
	return test_bit(state_bit, &(npu_debug_ref->state));
}

#if IS_ENABLED(CONFIG_NPU_UNITTEST)
static const struct file_operations npu_debug_unittest_fops = {
		.owner = THIS_MODULE, .open = IDIOT_open, .read = IDIOT_read, .write = IDIOT_write,
};
#endif

static int npu_hw_debug_layer_range_show(struct seq_file *file, void *unused)
{
	struct npu_device	*npu_device;
	struct device		*dev;

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (unlikely(!npu_device)) {
		npu_err("Failed to get npu_device\n");
		return -EINVAL;
	}

	dev = npu_device->dev;
	if (unlikely(!dev)) {
		npu_err("Failed to get dev\n");
		return -EINVAL;
	}

	seq_printf(file, "layer_range start:%#x / end:%#x\n",
			npu_device->system.layer_start, npu_device->system.layer_end);
	seq_puts(file, "Command to set layer_range to run layer by layer\n");
	seq_printf(file, "%#x is default value and it is ignored\n",
			NPU_SET_DEFAULT_LAYER);
	seq_puts(file, " echo ${start} ${end} > /d/npu/hardware/layer_range\n");
	seq_puts(file, "Please use a decimal number\n");

	return 0;
}

static int npu_hw_debug_layer_range_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, npu_hw_debug_layer_range_show,
			inode->i_private);
}

static ssize_t npu_hw_debug_layer_range_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret;

	struct npu_device	*npu_device;
	struct device		*dev;

	char buf[30];
	ssize_t size;
	unsigned int start_layer;
	unsigned int end_layer;

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (unlikely(!npu_device)) {
		npu_err("Failed to get npu_device\n");
		ret = -EINVAL;
		goto p_err;
	}

	dev = npu_device->dev;
	if (unlikely(!dev)) {
		npu_err("Failed to get dev\n");
		ret = -EINVAL;
		goto p_err;
	}


	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%u %u", &start_layer, &end_layer);
	if (ret != 2) {
		npu_err("Failed to get layer_range parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&npu_device->sessionmgr.mlock);
	if (atomic_read(&npu_device->sessionmgr.session_cnt)) {
		npu_err("device already opened (%u)\n",
				atomic_read(&npu_device->sessionmgr.session_cnt));
	} else {
		npu_device->system.layer_start = start_layer;
		npu_device->system.layer_end = end_layer;
		npu_info("layer_range is set (%d ~ %d)\n",
				start_layer, end_layer);
	}
	mutex_unlock(&npu_device->sessionmgr.mlock);

	return count;
p_err:
	return ret;
}

static const struct file_operations npu_hw_debug_layer_range_fops = {
	.open		= npu_hw_debug_layer_range_open,
	.read		= seq_read,
	.write		= npu_hw_debug_layer_range_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int npu_hw_debug_cold_boot_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	seq_printf(file, "Current npu's fw_cold_boot flag is %s(%d).\n",
			npu_system->fw_cold_boot ? "true" : "false", npu_system->fw_cold_boot);

	seq_puts(file, "Please use this command for enabling cold booot : \"echo 1 > /d/npu/cold_boot\"\n");

	return 0;
p_err:
	return ret;
}

static int npu_hw_debug_cold_boot_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_hw_debug_cold_boot_show,
			inode->i_private);
}

static ssize_t npu_hw_debug_cold_boot_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	int fw_cold_boot;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &fw_cold_boot);
	if (ret) {
		npu_err("Failed to get fw_cold_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&npu_device->sessionmgr.mlock);
	npu_system->fw_cold_boot = fw_cold_boot;
	mutex_unlock(&npu_device->sessionmgr.mlock);

	return 0;
p_err:
	return ret;
}

static const struct file_operations npu_debug_cold_boot_fops = {
	.open		= npu_hw_debug_cold_boot_open,
	.read		= seq_read,
	.write		= npu_hw_debug_cold_boot_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int npu_hw_debug_s2d_mode_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	seq_printf(file, "Current s2d mode is %s(0x%x(%u)).\n",
			(npu_system->s2d_mode == NPU_S2D_MODE_ON) ? "ON" : "OFF",
			npu_system->s2d_mode, npu_system->s2d_mode);

	seq_puts(file, "Please use this command for enabling s2d_mode : \"echo 538050818 > /d/npu/s2d_mode\"\n");

	return 0;
p_err:
	return ret;
}

static int npu_hw_debug_s2d_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_hw_debug_s2d_mode_show,
			inode->i_private);
}

static ssize_t npu_hw_debug_s2d_mode_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	int s2d_mode;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &s2d_mode);
	if (ret) {
		npu_err("Failed to get s2d_mode parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&npu_device->sessionmgr.mlock);
	if (s2d_mode == NPU_S2D_MODE_ON) {
		npu_system->s2d_mode = NPU_S2D_MODE_ON;
	} else  {
		npu_system->s2d_mode = NPU_S2D_MODE_OFF;
	}
	mutex_unlock(&npu_device->sessionmgr.mlock);

	return 0;
p_err:
	return ret;
}

static const struct file_operations npu_debug_s2d_mode_fops = {
	.open		= npu_hw_debug_s2d_mode_open,
	.read		= seq_read,
	.write		= npu_hw_debug_s2d_mode_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int npu_hw_debug_dump_mode_show(struct seq_file *file, void *unused)
{
	int ret = 0;
	u32 dump_mode, start, end;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	dump_mode = npu_system->dhcp_table->DUMP_MODE;
	start = npu_system->dhcp_table->DUMP_START_ADDR;
	end = npu_system->dhcp_table->DUMP_END_ADDR;

	seq_printf(file, "DUMP mode(0x%8x) Start ADDR(0x%8x), End ADDR(0x%8x)\n",
			dump_mode, start, end);

	return 0;
p_err:
	return ret;
}

static int npu_hw_debug_dump_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_hw_debug_dump_mode_show,
			inode->i_private);
}

static ssize_t npu_hw_debug_dump_mode_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	u32 dump_mode = 0, start = 0, end = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%x %x %x", &dump_mode, &start, &end);
	if (ret != 3) {
		npu_err("Failed to get dump mode parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&npu_device->sessionmgr.mlock);
	npu_system->dhcp_table->DUMP_MODE = dump_mode;
	npu_system->dhcp_table->DUMP_START_ADDR = start;
	npu_system->dhcp_table->DUMP_END_ADDR = end;
	mutex_unlock(&npu_device->sessionmgr.mlock);

	return 0;
p_err:
	return ret;
}

static const struct file_operations npu_debug_dump_mode_fops = {
	.open		= npu_hw_debug_dump_mode_open,
	.read		= seq_read,
	.write		= npu_hw_debug_dump_mode_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

#ifdef CONFIG_NPU_DEBUG_MEMDUMP

#define NPU_MEMDUMP_MAGIC 0xBEBEDABE

struct memdump_data {
	u32  magic;
	u32  is_iomem;
	union {
		struct npu_iomem_area *io;
		struct npu_memory_buffer *npu;
	} mem;
	char name[32];
	char buff[PAGE_SIZE];
};

static int npu_hw_debug_memdump_open(struct inode *inode, struct file *filep)
{
    int ret = 0;
	struct memdump_data *mdata;

	mdata = (struct memdump_data *)kzalloc(sizeof(struct memdump_data), GFP_KERNEL);

	if (!mdata)
		return -ENOMEM;

	mdata->magic = NPU_MEMDUMP_MAGIC;

	filep->private_data = mdata;

    return ret;
}


static int npu_hw_debug_memdump_close(struct inode *inode, struct file *filep)
{
	if (filep->private_data)
		kfree(filep->private_data);

    return 0;
}

static ssize_t npu_hw_debug_memdump_write(struct file *filep, const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	ssize_t size;

	struct npu_device *npu_device;
	struct npu_system *npu_system;
	struct memdump_data *mdata;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	mdata = (struct memdump_data *)filep->private_data;

	if (!mdata) {
		ret = -EINVAL;
		npu_err("Failed to get private data\n");
		goto p_err;
	}

	if (mdata->magic != NPU_MEMDUMP_MAGIC) {
		ret = -EINVAL;
		npu_err("Invalid magic\n");
		goto p_err;
	}

	size = simple_write_to_buffer(mdata->name, sizeof(mdata->name) - 1, ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}

	*ppos = 0; // reset offset for reading since the same read / write descriptor is used

	mdata->name[size] = '\0';

	mdata->mem.npu = npu_get_mem_area(npu_system, mdata->name);

	if (!mdata->mem.npu) {
		mdata->mem.io = npu_get_io_area(npu_system, mdata->name);

		if (!mdata->mem.io) {
			ret = -EINVAL;
			npu_err("Memory not found: %s\n", mdata->name);
			goto p_err;
		}

		mdata->is_iomem = 1;

		npu_info("dump: %s paddr: 0x%px size: 0x%x\n",
		mdata->name, mdata->mem.io->paddr, mdata->mem.io->size);
	} else {
		mdata->is_iomem = 0;

		npu_info("dump: %s paddr: 0x%px daddr: 0x%px size: 0x%x\n",
		mdata->name, mdata->mem.npu->paddr, mdata->mem.npu->daddr, mdata->mem.npu->size);
	}

	return size;
p_err:
	return ret;
}

static ssize_t npu_hw_debug_memdump_read(struct file *filep, char __user *buff, size_t count, loff_t *offset)
{
    int ret = 0;

	size_t copy_count, done_count, failed_count;

	ssize_t left_count;

	struct npu_device *npu_device;
	struct npu_system *npu_system;
	struct memdump_data *mdata;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	mdata = (struct memdump_data *)filep->private_data;

	if (!mdata)	{
		ret = -EINVAL;
		npu_err("Failed to get private data\n");
		goto p_err;
	}

	if (mdata->magic != NPU_MEMDUMP_MAGIC) {
		ret = -EINVAL;
		npu_err("Invalid magic\n");
		goto p_err;
	}

	if (!mdata->mem.io) {
		ret = -EINVAL;
		npu_err("Invalid memory info\n");
		goto p_err;
	}

	if (mdata->is_iomem) {
#if 0
		// TODO: enable this function to prevent crashes
		if (!npu_system_is_powered_on(npu_system)) {
			ret = -EINVAL;
			npu_err("System is not powered on\n");
			goto p_err;
		}
#endif

		left_count = min((size_t)mdata->mem.io->size - (size_t)*offset, count);
	} else {
		left_count = min((size_t)mdata->mem.npu->size - (size_t)*offset, count);
	}

    if (left_count <= 0)
		return 0;

    done_count = 0;

    while (done_count < left_count) {
		copy_count = min(left_count - done_count, sizeof(mdata->buff));

		if (mdata->is_iomem) {
			memcpy_fromio(mdata->buff, mdata->mem.io->vaddr + *offset, copy_count);
		} else {
			memcpy(mdata->buff, mdata->mem.npu->vaddr + *offset, copy_count);
		}

		failed_count = copy_to_user(buff + done_count, mdata->buff, copy_count);
		if (failed_count) {
			ret = -EFAULT;
			npu_err("Failed to copy 0x%lX bytes", failed_count);
			goto p_err;
		}

		*offset += copy_count;
		done_count += copy_count;
    }

    return done_count;
p_err:
	return ret;
}

static const struct file_operations npu_debug_memdump_fops = {
	.open		= npu_hw_debug_memdump_open,
	.release	= npu_hw_debug_memdump_close,
	.write		= npu_hw_debug_memdump_write,
	.read		= npu_hw_debug_memdump_read,
	.llseek		= default_llseek,
};
#endif

int npu_debug_register_arg(
	const char *name, void *private_arg,
	const struct file_operations *ops)
{
	int			ret	    = 0;
	struct npu_device	*npu_device;
	struct device		*dev;
	mode_t			mode = 0;
	struct dentry		*dbgfs_entry;

	/* Check whether the debugfs is properly initialized */
	if (!check_state_bit(FS_READY)) {
		probe_warn("DebugFS not initialized or disabled. Skip creation [%s]\n", name);
		ret = 0;
		goto err_exit;
	}

	/* Default parameter is npu_debug object */
	if (private_arg == NULL)
		private_arg = (void *)npu_debug_ref;

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (unlikely(!npu_device)) {
		npu_err("Failed to get npu_device\n");
		ret = -EINVAL;
		goto err_exit;
	}

	dev = npu_device->dev;
	if (unlikely(!dev)) {
		npu_err("Failed to get dev\n");
		ret = -EINVAL;
		goto err_exit;
	}

	if (name == NULL) {
		npu_err("name is null\n");
		ret = -EFAULT;
		goto err_exit;
	}
	if (ops == NULL) {
		npu_err("ops is null\n");
		ret = -EFAULT;
		goto err_exit;
	}

	/* Setting file permission based on file_operation member */
	if (ops->read || ops->compat_ioctl || ops->unlocked_ioctl)
		mode |= 0400;	/* Read permission to owner */

	if (ops->write)
		mode |= 0200;	/* Write permission to owner */

	/* Register */
	dbgfs_entry = debugfs_create_file(name, mode,
			npu_debug_ref->dfile_root, private_arg, ops);
	if (IS_ERR(dbgfs_entry)) {
		npu_err("fail in DebugFS registration (%s)\n", name);
		ret = PTR_ERR(dbgfs_entry);
		goto err_exit;
	}
	probe_info("success in DebugFS registration (%s) : Mode %04o\n"
		 , name, mode);
	return 0;

err_exit:
	return ret;
}

int npu_debug_register(const char *name, const struct file_operations *ops)
{
	int ret = 0, debugfs_enable = 0;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_enable = 1;
	ret = npu_debug_register_arg(name, NULL, ops);
	if (ret)
		probe_err("failed (%d)", ret);
#endif
	probe_info("register %s debugfs_state %s", name,
		   debugfs_enable ? "enabled" : "disabled");
	return ret;
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
static int npu_clock_dnc_min_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT);

	seq_printf(file, "DNC MIN : %d\n", value);
	seq_printf(file, "echo [clock] > /d/npu/clock/dnc_min \n");

	return 0;
p_err:
	return ret;
}

static int npu_clock_dnc_max_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	value = exynos_pm_qos_request(PM_QOS_DNC_THROUGHPUT_MAX);

	seq_printf(file, "DNC MAX : %d\n", value);
	seq_printf(file, "echo [clock] > /d/npu/clock/dnc_max \n");

	return 0;
p_err:
	return ret;
}

static int npu_clock_npu_min_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	value = exynos_pm_qos_request(PM_QOS_NPU_THROUGHPUT);

	seq_printf(file, "NPU MIN : %d\n", value);
	seq_printf(file, "echo [clock] > /d/npu/clock/npu_min \n");

	return 0;
p_err:
	return ret;
}

static int npu_clock_npu_max_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	value = exynos_pm_qos_request(PM_QOS_NPU_THROUGHPUT_MAX);

	seq_printf(file, "NPU MAX : %d\n", value);
	seq_printf(file, "echo [clock] > /d/npu/clock/npu_max \n");

	return 0;
p_err:
	return ret;
}

static int npu_cidle_show(struct seq_file *file, void *unused)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	u32 cidle;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	cidle = npu_system->dhcp_table->CIDLE;

	seq_printf(file, "CIDLE (0x%8x)\n", cidle);

	return 0;
p_err:
	return ret;
}

static int npu_clock_dnc_min_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_clock_dnc_min_show, inode->i_private);
}

static int npu_clock_dnc_max_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_clock_dnc_max_show, inode->i_private);
}

static int npu_clock_npu_min_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_clock_npu_min_show, inode->i_private);
}

static int npu_clock_npu_max_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_clock_npu_max_show, inode->i_private);
}

static int npu_cidle_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_cidle_show, inode->i_private);
}

static ssize_t npu_clock_dnc_min_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	char buf[30];
	ssize_t size;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		npu_err("Failed to get fw_cold_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_dnc, value);

	return 0;
p_err:
	return ret;
}

static ssize_t npu_clock_dnc_max_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	char buf[30];
	ssize_t size;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		npu_err("Failed to get fw_cold_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_dnc_max, value);

	return 0;
p_err:
	return ret;
}

static ssize_t npu_clock_npu_min_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	char buf[30];
	ssize_t size;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	struct dhcp_table *dhcp_table = npu_system->dhcp_table;
	if (!dhcp_table) {
		ret = -EINVAL;
		npu_err("Failed to get dhcp_table\n");
		goto p_err;
	}
#endif

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		npu_err("Failed to get fw_cold_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_npu, value);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	dhcp_table->PRESET_NPU_FREQ = value;
#endif

	return 0;
p_err:
	return ret;
}

static ssize_t npu_clock_npu_max_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device = NULL;
	struct npu_system *npu_system = NULL;
	struct npu_scheduler_info *info = NULL;
	// struct npu_scheduler_dvfs_info *d = NULL;

	struct npu_qos_setting *qos_setting = NULL;

	char buf[30];
	ssize_t size;

	int value = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}

	info = npu_device->sched;

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &value);
	if (ret) {
		npu_err("Failed to get fw_cold_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	qos_setting = &npu_system->qos_setting;
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_npu_max, value);

	return 0;
p_err:
	return ret;
}

static ssize_t npu_cidle_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	u32 cidle = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtouint(buf, 10, &cidle);
	if (ret) {
		npu_err("Failed to get fw_cold_boot parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	mutex_lock(&npu_device->sessionmgr.mlock);
	npu_system->dhcp_table->CIDLE = cidle;
	mutex_unlock(&npu_device->sessionmgr.mlock);

	return 0;
p_err:
	return ret;


}
static const struct file_operations npu_debug_clock_dnc_min_fops = {
	.open		= npu_clock_dnc_min_open,
	.read		= seq_read,
	.write		= npu_clock_dnc_min_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static const struct file_operations npu_debug_clock_dnc_max_fops = {
	.open		= npu_clock_dnc_max_open,
	.read		= seq_read,
	.write		= npu_clock_dnc_max_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static const struct file_operations npu_debug_clock_npu_min_fops = {
	.open		= npu_clock_npu_min_open,
	.read		= seq_read,
	.write		= npu_clock_npu_min_write,
	.llseek		= seq_lseek,
	.release	= single_release
};


static const struct file_operations npu_debug_clock_npu_max_fops = {
	.open		= npu_clock_npu_max_open,
	.read		= seq_read,
	.write		= npu_clock_npu_max_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static const struct file_operations npu_debug_cidle_fops = {
	.open		= npu_cidle_open,
	.read		= seq_read,
	.write		= npu_cidle_write,
	.llseek		= seq_lseek,
	.release	= single_release
};
#endif

static int npu_debug_expand_dva_show(struct seq_file *file, void *unused)
{
	bool iova_36bit = get_iova_36bit();
	seq_printf(file, "IOVA %s\n", iova_36bit ? "36BIT" : "32BIT");
	return 0;
}

static int npu_debug_expand_dva_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_debug_expand_dva_show, inode->i_private);
}

static ssize_t npu_debug_expand_dva_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	int iova_36bit;

	if (unlikely(!npu_debug_ref)) {
		npu_err("Failed to get npu_debug_ref\n");
		return -EINVAL;
	}

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);

	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtouint(buf, 10, &iova_36bit);
	if (ret) {
		npu_err("Failed to get expand_dva parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	set_iova_36bit(iova_36bit);

	return 0;
p_err:
	return ret;
}

static const struct file_operations npu_debug_expand_dva_fops = {
	.open		= npu_debug_expand_dva_open,
	.read		= seq_read,
	.write		= npu_debug_expand_dva_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

#if IS_ENABLED(CONFIG_SOC_S5E9955)
static int npu_debug_apg_state_show(struct seq_file *file, void *unused)
{
	int ret = 0;
	volatile u32 apg_state, apg_count, apg_total_time;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	apg_state = npu_system->dhcp_table->APG_STATE;
	apg_count = npu_system->dhcp_table->APG_COUNT;
	apg_total_time = npu_system->dhcp_table->APG_TIME;

	seq_printf(file, "APG state (0x%8x), APG count (0x%8x), APG total time (%8u ms)\n", apg_state, apg_count, apg_total_time);

	return 0;
p_err:
	return ret;
}

static int npu_debug_apg_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_debug_apg_state_show,
			inode->i_private);
}

static ssize_t npu_debug_apg_state_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	u32 apg_state_reset = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtouint(buf, 10, &apg_state_reset);
	if (ret) {
		npu_err("Failed to set apg_state reset(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(NPU_DEVICE_STATE_OPEN, &npu_device->state)) {
		npu_system->dhcp_table->APG_COUNT = 0;
		npu_system->dhcp_table->APG_TIME = 0;
	}

	return 0;
p_err:
	return ret;
}

static const struct file_operations npu_debug_apg_state_fops = {
	.open		= npu_debug_apg_state_open,
	.read		= seq_read,
	.write		= npu_debug_apg_state_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int npu_debug_apg_dump_on_show(struct seq_file *file, void *unused)
{
	int ret = 0;
	volatile u32 apg_dump_on;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	apg_dump_on = npu_system->dhcp_table->ENABLE_DUMP_APG;

	seq_printf(file, "APG_dump_on : %s\n", apg_dump_on ? "on" : "off");

	return 0;
p_err:
	return ret;
}

static int npu_debug_apg_dump_on_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_debug_apg_dump_on_show,
			inode->i_private);
}

static ssize_t npu_debug_apg_dump_on_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	struct npu_device *npu_device;
	struct npu_system *npu_system;

	char buf[30];
	ssize_t size;
	u32 apg_dump_on = 0;

	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	if (!npu_device) {
		ret = -EINVAL;
		npu_err("Failed to get npu_device\n");
		goto p_err;
	}
	npu_system = &npu_device->system;
	if (!npu_system) {
		ret = -EINVAL;
		npu_err("Failed to get npu_system\n");
		goto p_err;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtouint(buf, 10, &apg_dump_on);
	if (ret) {
		npu_err("Failed to set apg_state reset(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(NPU_DEVICE_STATE_OPEN, &npu_device->state)) {
		npu_system->dhcp_table->ENABLE_DUMP_APG = apg_dump_on;
	}

	return 0;
p_err:
	return ret;
}

static const struct file_operations npu_debug_apg_dump_on_fops = {
	.open		= npu_debug_apg_dump_on_open,
	.read		= seq_read,
	.write		= npu_debug_apg_dump_on_write,
	.llseek		= seq_lseek,
	.release	= single_release
};
#endif

int npu_debug_probe(struct npu_device *npu_device)
{
	int ret = 0;

	/* Save reference */
	npu_debug_ref = &npu_device->debug;
	memset(npu_debug_ref, 0, sizeof(*npu_debug_ref));

	probe_info("Loading npu_debug : starting\n");
	npu_debug_ref->dfile_root = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);

	if (!npu_debug_ref->dfile_root) {
		probe_err("Loading npu_debug : debugfs root [%s] can not be created\n",
			  DEBUG_FS_ROOT_NAME);

		npu_debug_ref->dfile_root = NULL;
		ret = -ENOENT;
		goto err_exit;
	}
	probe_info("Loading npu_debug : debugfs root [%s] created\n"
		   , DEBUG_FS_ROOT_NAME);
	set_state_bit(FS_READY);

#if IS_ENABLED(CONFIG_NPU_UNITTEST)
	ret = npu_debug_register(DEBUG_FS_UNITTEST_NAME
				 , &npu_debug_unittest_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for unittest can not be created(%d)\n", ret);
		goto err_exit;
	}
#endif

	ret = npu_debug_register(DEBUG_FS_LAYER_INFO_NAME
				 , &npu_hw_debug_layer_range_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for layer range can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_COLD_BOOT_NAME
				 , &npu_debug_cold_boot_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for cold_boot can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_S2D_MODE_NAME
				 , &npu_debug_s2d_mode_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for s2d_mode can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_DUMP_MODE_NAME
				 , &npu_debug_dump_mode_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for dump_mode can not be created(%d)\n", ret);
		goto err_exit;
	}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	ret = npu_debug_register(DEBUG_FS_CLOCK_DNC_MIN_NAME
				 , &npu_debug_clock_dnc_min_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for clock_dnc_min can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_CLOCK_DNC_MAX_NAME
				 , &npu_debug_clock_dnc_max_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for clock_dnc_max can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_CLOCK_NPU_MIN_NAME
				 , &npu_debug_clock_npu_min_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for clock_npu_min can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_CLOCK_NPU_MAX_NAME
				 , &npu_debug_clock_npu_max_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for clock_npu_max can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_CIDLE_NAME, &npu_debug_cidle_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for cilde can not be created(%d)\n", ret);
		goto err_exit;
	}
#endif
	ret = npu_debug_register(DEBUG_FS_EXPAND_DVA, &npu_debug_expand_dva_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for expand_dva can not be created(%d)\n", ret);
		goto err_exit;
	}
#ifdef CONFIG_NPU_DEBUG_MEMDUMP
	ret = npu_debug_register(DEBUG_FS_MEMDUMP_NAME
				 , &npu_debug_memdump_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for " DEBUG_FS_MEMDUMP_NAME " can not be created\n");
		goto err_exit;
	}
#endif
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	ret = npu_debug_register(DEBUG_FS_APG_STATE_NAME, &npu_debug_apg_state_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for apg state can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = npu_debug_register(DEBUG_FS_APG_DUMP_ON_NAME, &npu_debug_apg_dump_on_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for apg dump on can not be created(%d)\n", ret);
		goto err_exit;
	}
#endif
	ret = npu_ver_probe(npu_device);
	if (ret) {
		probe_err("loading npu_debug : npu_ver_probe failed(%d).\n", ret);
		goto err_exit;
	}

	return 0;

err_exit:
	return ret;
}

int npu_debug_release(void)
{
	int ret = 0;
	struct npu_device	*npu_device;
	struct device		*dev;

	if (!check_state_bit(FS_READY)) {
		/* No need to clean-up */
		probe_err("not ready in npu_debug\n");
		return -1;
	}

	/* Retrieve struct device to use devm_XX api */
	npu_device = container_of(npu_debug_ref, struct npu_device, debug);
	dev = npu_device->dev;

	/* remove version info */
	ret = npu_ver_release(npu_device);
	if (ret) {
		probe_err("npu_ver_release error : ret = %d\n", ret);
		return ret;
	}

	/* Remove debug fs entries */
	debugfs_remove_recursive(npu_debug_ref->dfile_root);
	probe_info("unloading npu_debug: root node is removed.\n");

	clear_state_bit(FS_READY);
	return ret;
}

int npu_debug_open(struct npu_device *npu_device)
{
	return 0;
}

int npu_debug_close(struct npu_device *npu_device)
{
	return 0;
}
