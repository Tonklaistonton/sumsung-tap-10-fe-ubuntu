/*
 * Copyright 2008-2021 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <linux/kthread.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/poll.h>
#include <drm/drm_debugfs.h>

#include "amdgpu.h"
#include "amdgpu_pm.h"
#include "amdgpu_sws.h"
#include "amdgpu_cwsr.h"
#include "sgpu_bpmd.h"
#include "sgpu_debugfs.h"
#include "amdgpu_ttm.h"

/**
 * amdgpu_debugfs_add_files - Add simple debugfs entries
 *
 * @adev:  Device to attach debugfs entries to
 * @files:  Array of function callbacks that respond to reads
 * @nfiles: Number of callbacks to register
 *
 */
int amdgpu_debugfs_add_files(struct amdgpu_device *adev,
			     const struct drm_info_list *files,
			     unsigned nfiles)
{
	unsigned i;

	for (i = 0; i < adev->debugfs_count; i++) {
		if (adev->debugfs[i].files == files) {
			/* Already registered */
			return 0;
		}
	}

	i = adev->debugfs_count + 1;
	if (i > AMDGPU_DEBUGFS_MAX_COMPONENTS) {
		DRM_ERROR("Reached maximum number of debugfs components.\n");
		DRM_ERROR("Report so we increase "
			  "AMDGPU_DEBUGFS_MAX_COMPONENTS.\n");
		return -EINVAL;
	}
	adev->debugfs[adev->debugfs_count].files = files;
	adev->debugfs[adev->debugfs_count].num_files = nfiles;
	adev->debugfs_count = i;
#if defined(CONFIG_DEBUG_FS)
	drm_debugfs_create_files(files, nfiles,
				 adev_to_drm(adev)->render->debugfs_root,
				 adev_to_drm(adev)->render);
#endif
	return 0;
}

int amdgpu_debugfs_wait_dump(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	unsigned long timeout = 600 * HZ;
	int ret;

	wake_up_interruptible(&adev->autodump.gpu_hang);

	ret = wait_for_completion_interruptible_timeout(&adev->autodump.dumping, timeout);
	if (ret == 0) {
		pr_err("autodump: timeout, move on to gpu recovery\n");
		return -ETIMEDOUT;
	}
#endif
	return 0;
}

#if defined(CONFIG_DEBUG_FS)
/**
 * amdgpu_debugfs_process_reg_op - Handle MMIO register reads/writes
 *
 * @read: True if reading
 * @f: open file handle
 * @buf: User buffer to write/read to
 * @size: Number of bytes to write/read
 * @pos:  Offset to seek to
 *
 * This debugfs entry has special meaning on the offset being sought.
 * Various bits have different meanings:
 *
 * Bit 62:  Indicates a GRBM bank switch is needed
 * Bit 61:  Indicates a SRBM bank switch is needed (implies bit 62 is
 * 			zero)
 * Bits 24..33: The SE or ME selector if needed
 * Bits 34..43: The SH (or SA) or PIPE selector if needed
 * Bits 44..53: The INSTANCE (or CU/WGP) or QUEUE selector if needed
 *
 * Bit 23:  Indicates that the PM power gating lock should be held
 * 			This is necessary to read registers that might be
 * 			unreliable during a power gating transistion.
 *
 * The lower bits are the BYTE offset of the register to read.  This
 * allows reading multiple registers in a single call and having
 * the returned size reflect that.
 */
static int  amdgpu_debugfs_process_reg_op(bool read, struct file *f,
		char __user *buf, size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;
	bool pm_pg_lock, use_bank, use_ring;
	unsigned instance_bank, sh_bank, se_bank, me, pipe, queue, vmid;

	pm_pg_lock = use_bank = use_ring = false;
	instance_bank = sh_bank = se_bank = me = pipe = queue = vmid = 0;

	if (size & 0x3 || *pos & 0x3 ||
			((*pos & (1ULL << 62)) && (*pos & (1ULL << 61))))
		return -EINVAL;

	/* are we reading registers for which a PG lock is necessary? */
	pm_pg_lock = (*pos >> 23) & 1;

	if (*pos & (1ULL << 62)) {
		se_bank = (*pos & GENMASK_ULL(33, 24)) >> 24;
		sh_bank = (*pos & GENMASK_ULL(43, 34)) >> 34;
		instance_bank = (*pos & GENMASK_ULL(53, 44)) >> 44;

		if (se_bank == 0x3FF)
			se_bank = 0xFFFFFFFF;
		if (sh_bank == 0x3FF)
			sh_bank = 0xFFFFFFFF;
		if (instance_bank == 0x3FF)
			instance_bank = 0xFFFFFFFF;
		use_bank = true;
	} else if (*pos & (1ULL << 61)) {

		me = (*pos & GENMASK_ULL(33, 24)) >> 24;
		pipe = (*pos & GENMASK_ULL(43, 34)) >> 34;
		queue = (*pos & GENMASK_ULL(53, 44)) >> 44;
		vmid = (*pos & GENMASK_ULL(58, 54)) >> 54;

		use_ring = true;
	} else {
		use_bank = use_ring = false;
	}

	*pos &= (1UL << 22) - 1;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	if (use_bank) {
		if ((sh_bank != 0xFFFFFFFF && sh_bank >= adev->gfx.config.max_sh_per_se) ||
		    (se_bank != 0xFFFFFFFF && se_bank >= adev->gfx.config.max_shader_engines)) {
			pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
			pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
			return -EINVAL;
		}
		mutex_lock(&adev->grbm_idx_mutex);
		amdgpu_gfx_select_se_sh(adev, se_bank,
					sh_bank, instance_bank);
	} else if (use_ring) {
		mutex_lock(&adev->srbm_mutex);
		amdgpu_gfx_select_me_pipe_q(adev, me, pipe, queue, vmid);
	}

	if (pm_pg_lock)
		mutex_lock(&adev->pm.mutex);

	while (size) {
		uint32_t value;

		if (read) {
			value = RREG32(*pos >> 2);
			r = put_user(value, (uint32_t *)buf);
		} else {
			r = get_user(value, (uint32_t *)buf);
			if (!r)
				amdgpu_mm_wreg_mmio_rlc(adev, *pos >> 2, value);
		}
		if (r) {
			result = r;
			goto end;
		}

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

end:
	if (use_bank) {
		amdgpu_gfx_select_se_sh(adev, 0xffffffff, 0xffffffff, 0xffffffff);
		mutex_unlock(&adev->grbm_idx_mutex);
	} else if (use_ring) {
		amdgpu_gfx_select_me_pipe_q(adev, 0, 0, 0, 0);
		mutex_unlock(&adev->srbm_mutex);
	}

	if (pm_pg_lock)
		mutex_unlock(&adev->pm.mutex);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	return result;
}

/**
 * amdgpu_debugfs_regs_read - Callback for reading MMIO registers
 */
static ssize_t amdgpu_debugfs_regs_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	return amdgpu_debugfs_process_reg_op(true, f, buf, size, pos);
}

/**
 * amdgpu_debugfs_regs_write - Callback for writing MMIO registers
 */
static ssize_t amdgpu_debugfs_regs_write(struct file *f, const char __user *buf,
					 size_t size, loff_t *pos)
{
	return amdgpu_debugfs_process_reg_op(false, f, (char __user *)buf, size, pos);
}

/**
 * amdgpu_debugfs_regs_didt_read - Read from a DIDT register
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to read.  This
 * allows reading multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_didt_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	while (size) {
		uint32_t value;

		value = RREG32_DIDT(*pos >> 2);
		r = put_user(value, (uint32_t *)buf);
		if (r) {
			pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
			pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
			return r;
		}

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	return result;
}

/**
 * amdgpu_debugfs_regs_didt_write - Write to a DIDT register
 *
 * @f: open file handle
 * @buf: User buffer to write data from
 * @size: Number of bytes to write
 * @pos:  Offset to seek to
 *
 * The lower bits are the BYTE offset of the register to write.  This
 * allows writing multiple registers in a single call and having
 * the returned size reflect that.
 */
static ssize_t amdgpu_debugfs_regs_didt_write(struct file *f, const char __user *buf,
					 size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	while (size) {
		uint32_t value;

		r = get_user(value, (uint32_t *)buf);
		if (r) {
			pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
			pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
			return r;
		}

		WREG32_DIDT(*pos >> 2, value);

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	return result;
}

/**
 * amdgpu_debugfs_gca_config_read - Read from gfx config data
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * This file is used to access configuration data in a somewhat
 * stable fashion.  The format is a series of DWORDs with the first
 * indicating which revision it is.  New content is appended to the
 * end so that older software can still read the data.
 */

static ssize_t amdgpu_debugfs_gca_config_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	ssize_t result = 0;
	int r;
	uint32_t *config, no_regs = 0;

	if (size & 0x3 || *pos & 0x3)
		return -EINVAL;

	config = kmalloc_array(256, sizeof(*config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	/* version, increment each time something is added */
	config[no_regs++] = 2;
	config[no_regs++] = adev->gfx.config.max_shader_engines;
	config[no_regs++] = adev->gfx.config.max_tile_pipes;
	config[no_regs++] = adev->gfx.config.max_cu_per_sh;
	config[no_regs++] = adev->gfx.config.max_sh_per_se;
	config[no_regs++] = adev->gfx.config.max_backends_per_se;
	config[no_regs++] = adev->gfx.config.max_texture_channel_caches;
	config[no_regs++] = adev->gfx.config.max_gprs;
	config[no_regs++] = adev->gfx.config.max_gs_threads;
	config[no_regs++] = adev->gfx.config.max_hw_contexts;
	config[no_regs++] = adev->gfx.config.sc_prim_fifo_size_frontend;
	config[no_regs++] = adev->gfx.config.sc_prim_fifo_size_backend;
	config[no_regs++] = adev->gfx.config.sc_hiz_tile_fifo_size;
	config[no_regs++] = adev->gfx.config.sc_earlyz_tile_fifo_size;
	config[no_regs++] = adev->gfx.config.num_tile_pipes;
	config[no_regs++] = adev->gfx.config.backend_enable_mask;
	config[no_regs++] = adev->gfx.config.mem_max_burst_length_bytes;
	config[no_regs++] = adev->gfx.config.mem_row_size_in_kb;
	config[no_regs++] = adev->gfx.config.shader_engine_tile_size;
	config[no_regs++] = adev->gfx.config.num_gpus;
	config[no_regs++] = adev->gfx.config.multi_gpu_tile_size;
	config[no_regs++] = adev->gfx.config.mc_arb_ramcfg;
	config[no_regs++] = adev->gfx.config.gb_addr_config;
	config[no_regs++] = adev->gfx.config.num_rbs;

	/* rev==1 */
	config[no_regs++] = adev->rev_id;
	config[no_regs++] = adev->pg_flags;
	config[no_regs++] = adev->cg_flags;

	/* rev==2 */
	config[no_regs++] = adev->family;
	config[no_regs++] = adev->external_rev_id;

	while (size && (*pos < no_regs * 4)) {
		uint32_t value;

		value = config[*pos >> 2];
		r = put_user(value, (uint32_t *)buf);
		if (r) {
			kfree(config);
			return r;
		}

		result += 4;
		buf += 4;
		*pos += 4;
		size -= 4;
	}

	kfree(config);
	return result;
}

/** amdgpu_debugfs_wave_read - Read WAVE STATUS data
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The offset being sought changes which wave that the status data
 * will be returned for.  The bits are used as follows:
 *
 * Bits 0..6: 	Byte offset into data
 * Bits 7..14:	SE selector
 * Bits 15..22:	SH/SA selector
 * Bits 23..30: CU/{WGP+SIMD} selector
 * Bits 31..36: WAVE ID selector
 * Bits 37..44: SIMD ID selector
 *
 * The returned data begins with one DWORD of version information
 * Followed by WAVE STATUS registers relevant to the GFX IP version
 * being used.  See gfx_v8_0_read_wave_data() for an example output.
 */
static ssize_t amdgpu_debugfs_wave_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = f->f_inode->i_private;
	int r, x;
	ssize_t result=0;
	uint32_t offset, se, sh, cu, wave, simd, data[32];

	if (size & 3 || *pos & 3)
		return -EINVAL;

	/* decode offset */
	offset = (*pos & GENMASK_ULL(6, 0));
	se = (*pos & GENMASK_ULL(14, 7)) >> 7;
	sh = (*pos & GENMASK_ULL(22, 15)) >> 15;
	cu = (*pos & GENMASK_ULL(30, 23)) >> 23;
	wave = (*pos & GENMASK_ULL(36, 31)) >> 31;
	simd = (*pos & GENMASK_ULL(44, 37)) >> 37;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	/* switch to the specific se/sh/cu */
	mutex_lock(&adev->grbm_idx_mutex);
	amdgpu_gfx_select_se_sh(adev, se, sh, cu);

	x = 0;
	if (adev->gfx.funcs->read_wave_data)
		adev->gfx.funcs->read_wave_data(adev, simd, wave, data, &x);

	amdgpu_gfx_select_se_sh(adev, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	mutex_unlock(&adev->grbm_idx_mutex);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	if (!x) {
		return -EINVAL;
	}

	while (size && (offset < x * 4)) {
		uint32_t value;

		value = data[offset >> 2];
		r = put_user(value, (uint32_t *)buf);
		if (r) {
			return r;
		}

		result += 4;
		buf += 4;
		offset += 4;
		size -= 4;
	}

	return result;
}

/** amdgpu_debugfs_gpr_read - Read wave gprs
 *
 * @f: open file handle
 * @buf: User buffer to store read data in
 * @size: Number of bytes to read
 * @pos:  Offset to seek to
 *
 * The offset being sought changes which wave that the status data
 * will be returned for.  The bits are used as follows:
 *
 * Bits 0..11:	Byte offset into data
 * Bits 12..19:	SE selector
 * Bits 20..27:	SH/SA selector
 * Bits 28..35: CU/{WGP+SIMD} selector
 * Bits 36..43: WAVE ID selector
 * Bits 37..44: SIMD ID selector
 * Bits 52..59: Thread selector
 * Bits 60..61: Bank selector (VGPR=0,SGPR=1)
 *
 * The return data comes from the SGPR or VGPR register bank for
 * the selected operational unit.
 */
static ssize_t amdgpu_debugfs_gpr_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_device *adev = f->f_inode->i_private;
	int r;
	ssize_t result = 0;
	uint32_t offset, se, sh, cu, wave, simd, thread, bank, *data;

	if (size > 4096 || size & 3 || *pos & 3)
		return -EINVAL;

	/* decode offset */
	offset = (*pos & GENMASK_ULL(11, 0)) >> 2;
	se = (*pos & GENMASK_ULL(19, 12)) >> 12;
	sh = (*pos & GENMASK_ULL(27, 20)) >> 20;
	cu = (*pos & GENMASK_ULL(35, 28)) >> 28;
	wave = (*pos & GENMASK_ULL(43, 36)) >> 36;
	simd = (*pos & GENMASK_ULL(51, 44)) >> 44;
	thread = (*pos & GENMASK_ULL(59, 52)) >> 52;
	bank = (*pos & GENMASK_ULL(61, 60)) >> 60;

	data = kcalloc(1024, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0)
		goto err;

	/* switch to the specific se/sh/cu */
	mutex_lock(&adev->grbm_idx_mutex);
	amdgpu_gfx_select_se_sh(adev, se, sh, cu);

	if (bank == 0) {
		if (adev->gfx.funcs->read_wave_vgprs)
			adev->gfx.funcs->read_wave_vgprs(adev, simd, wave, thread, offset, size>>2, data);
	} else {
		if (adev->gfx.funcs->read_wave_sgprs)
			adev->gfx.funcs->read_wave_sgprs(adev, simd, wave, offset, size>>2, data);
	}

	amdgpu_gfx_select_se_sh(adev, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	mutex_unlock(&adev->grbm_idx_mutex);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);

	while (size) {
		uint32_t value;

		value = data[result >> 2];
		r = put_user(value, (uint32_t *)buf);
		if (r) {
			goto err;
		}

		result += 4;
		buf += 4;
		size -= 4;
	}

	kfree(data);
	return result;

err:
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
	kfree(data);
	return r;
}

static const struct file_operations amdgpu_debugfs_regs_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_regs_read,
	.write = amdgpu_debugfs_regs_write,
	.llseek = default_llseek
};
static const struct file_operations amdgpu_debugfs_regs_didt_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_regs_didt_read,
	.write = amdgpu_debugfs_regs_didt_write,
	.llseek = default_llseek
};

static const struct file_operations amdgpu_debugfs_gca_config_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_gca_config_read,
	.llseek = default_llseek
};

static const struct file_operations amdgpu_debugfs_wave_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_wave_read,
	.llseek = default_llseek
};
static const struct file_operations amdgpu_debugfs_gpr_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_gpr_read,
	.llseek = default_llseek
};

static const struct file_operations *debugfs_regs[] = {
	&amdgpu_debugfs_regs_fops,
	&amdgpu_debugfs_regs_didt_fops,
	&amdgpu_debugfs_gca_config_fops,
	&amdgpu_debugfs_wave_fops,
	&amdgpu_debugfs_gpr_fops,
};

static const char *debugfs_regs_names[] = {
	"amdgpu_regs",
	"amdgpu_regs_didt",
	"amdgpu_gca_config",
	"amdgpu_wave",
	"amdgpu_gpr",
};

/**
 * amdgpu_debugfs_regs_init -	Initialize debugfs entries that provide
 * 								register access.
 *
 * @adev: The device to attach the debugfs entries to
 */
int amdgpu_debugfs_regs_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev_to_drm(adev)->render;
	struct dentry *ent, *root = minor->debugfs_root;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(debugfs_regs); i++) {
		ent = debugfs_create_file(debugfs_regs_names[i],
					  S_IFREG | S_IRUGO, root,
					  adev, debugfs_regs[i]);
		if (!i && !IS_ERR_OR_NULL(ent))
			i_size_write(ent->d_inode, adev->rmmio_size);
		adev->debugfs_regs[i] = ent;
	}

	return 0;
}

static int amdgpu_debugfs_test_ib(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *) m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = drm_to_adev(dev);
	int r = 0, i;

	r = pm_runtime_get_sync(dev->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	/* Avoid accidently unparking the sched thread during GPU reset */
	r = down_read_killable(&adev->reset_sem);
	if (r)
		return r;

	/* hold on the scheduler */
	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;
		kthread_park(ring->sched.thread);
	}

	seq_printf(m, "run ib test:\n");
	r = amdgpu_ib_ring_tests(adev);
	if (r)
		seq_printf(m, "ib ring tests failed (%d).\n", r);
	else
		seq_printf(m, "ib ring tests passed.\n");

	/* go on the scheduler */
	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;
		kthread_unpark(ring->sched.thread);
	}

	up_read(&adev->reset_sem);

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);

	return 0;
}

static int amdgpu_debugfs_evict_gtt(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *dev = node->minor->dev;
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct ttm_resource_manager *man;
	int r;

	r = pm_runtime_get_sync(dev->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
		return r;
	}

	man = ttm_manager_type(&adev->mman.bdev, TTM_PL_TT);
	r = ttm_resource_manager_evict_all(&adev->mman.bdev, man);
	seq_printf(m, "(%d)\n", r);

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);

	return 0;
}

static const struct drm_info_list amdgpu_debugfs_list[] = {
	{"amdgpu_test_ib", &amdgpu_debugfs_test_ib},
	{"amdgpu_evict_gtt", &amdgpu_debugfs_evict_gtt},
};

static void amdgpu_ib_preempt_fences_swap(struct amdgpu_ring *ring,
					  struct dma_fence **fences)
{
	struct amdgpu_fence_driver *drv = &ring->fence_drv;
	uint32_t sync_seq, last_seq;

	last_seq = atomic_read(&ring->fence_drv.last_seq);
	sync_seq = ring->fence_drv.sync_seq;

	last_seq &= drv->num_fences_mask;
	sync_seq &= drv->num_fences_mask;

	do {
		struct dma_fence *fence, **ptr;

		++last_seq;
		last_seq &= drv->num_fences_mask;
		ptr = &drv->fences[last_seq];

		fence = rcu_dereference_protected(*ptr, 1);
		RCU_INIT_POINTER(*ptr, NULL);

		if (!fence)
			continue;

		fences[last_seq] = fence;

	} while (last_seq != sync_seq);
}

static void amdgpu_ib_preempt_signal_fences(struct dma_fence **fences,
					    int length)
{
	int i;
	struct dma_fence *fence;

	for (i = 0; i < length; i++) {
		fence = fences[i];
		if (!fence)
			continue;
		dma_fence_signal(fence);
		dma_fence_put(fence);
	}
}

static void amdgpu_ib_preempt_job_recovery(struct drm_gpu_scheduler *sched)
{
	struct drm_sched_job *s_job;
	struct dma_fence *fence;

	spin_lock(&sched->job_list_lock);
	list_for_each_entry(s_job, &sched->pending_list, list) {
		fence = sched->ops->run_job(s_job);
		dma_fence_put(fence);
	}
	spin_unlock(&sched->job_list_lock);
}

static void amdgpu_ib_preempt_mark_partial_job(struct amdgpu_ring *ring)
{
	struct amdgpu_job *job;
	struct drm_sched_job *s_job, *tmp;
	uint32_t preempt_seq;
	struct dma_fence *fence, **ptr;
	struct amdgpu_fence_driver *drv = &ring->fence_drv;
	struct drm_gpu_scheduler *sched = &ring->sched;
	bool preempted = true;

	if (ring->funcs->type != AMDGPU_RING_TYPE_GFX)
		return;

	preempt_seq = le32_to_cpu(*(drv->cpu_addr + 2));
	if (preempt_seq <= atomic_read(&drv->last_seq)) {
		preempted = false;
		goto no_preempt;
	}

	preempt_seq &= drv->num_fences_mask;
	ptr = &drv->fences[preempt_seq];
	fence = rcu_dereference_protected(*ptr, 1);

no_preempt:
	spin_lock(&sched->job_list_lock);
	list_for_each_entry_safe(s_job, tmp, &sched->pending_list, list) {
		if (dma_fence_is_signaled(&s_job->s_fence->finished)) {
			/* remove job from ring_mirror_list */
			list_del_init(&s_job->list);
			sched->ops->free_job(s_job);
			continue;
		}
		job = to_amdgpu_job(s_job);
		if (preempted && job->fence == fence)
			/* mark the job as preempted */
			job->preemption_status |= AMDGPU_IB_PREEMPTED;
	}
	spin_unlock(&sched->job_list_lock);
}

static int amdgpu_debugfs_ib_preempt(void *data, u64 val)
{
	int r, length;
	struct amdgpu_ring *ring;
	struct dma_fence **fences = NULL;
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	if (val >= AMDGPU_MAX_RINGS)
		return -EINVAL;

	ring = adev->rings[val];

	if (!ring || !ring->funcs->preempt_ib || !ring->sched.thread)
		return -EINVAL;

	/* the last preemption failed */
	if (ring->trail_seq != le32_to_cpu(*ring->trail_fence_cpu_addr))
		return -EBUSY;

	length = ring->fence_drv.num_fences_mask + 1;
	fences = kcalloc(length, sizeof(void *), GFP_KERNEL);
	if (!fences)
		return -ENOMEM;

	/* Avoid accidently unparking the sched thread during GPU reset */
	r = down_read_killable(&adev->reset_sem);
	if (r)
		goto pro_end;

	/* stop the scheduler */
	kthread_park(ring->sched.thread);

	/* preempt the IB */
	r = amdgpu_ring_preempt_ib(ring);
	if (r) {
		DRM_WARN("failed to preempt ring %d\n", ring->idx);
		goto failure;
	}

	amdgpu_fence_process(ring);

	if (atomic_read(&ring->fence_drv.last_seq) !=
	    ring->fence_drv.sync_seq) {
		DRM_INFO("ring %d was preempted\n", ring->idx);

		amdgpu_ib_preempt_mark_partial_job(ring);

		/* swap out the old fences */
		amdgpu_ib_preempt_fences_swap(ring, fences);

		amdgpu_fence_driver_force_completion(ring);

		/* resubmit unfinished jobs */
		amdgpu_ib_preempt_job_recovery(&ring->sched);

		/* wait for jobs finished */
		amdgpu_fence_wait_empty(ring);

		/* signal the old fences */
		amdgpu_ib_preempt_signal_fences(fences, length);
	}

failure:
	/* restart the scheduler */
	kthread_unpark(ring->sched.thread);

	up_read(&adev->reset_sem);

pro_end:
	kfree(fences);

	return r;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_ib_preempt, NULL,
			amdgpu_debugfs_ib_preempt, "%llu\n");

#if defined(CONFIG_DRM_SGPU_EXYNOS) && defined(CONFIG_DEBUG_FS)
static int sgpu_gfx_jobtimeout_to_panic_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	*val = adev->sgpu_debug.gfx_jobtimeout_to_panic ? 1 : 0;

	return 0;
}

static int sgpu_gfx_jobtimeout_to_panic_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	if (val > 1)
		return -EINVAL;

	adev->sgpu_debug.gfx_jobtimeout_to_panic = val ? true : false;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sgpu_debugfs_gfx_jobtimeout_to_panic_fops,
			sgpu_gfx_jobtimeout_to_panic_get,
			sgpu_gfx_jobtimeout_to_panic_set, "%llu\n");

static int sgpu_debugfs_gfx_jobtimeout_to_panic_init(struct drm_minor *minor)
{
	struct drm_device *drm = minor->dev;
	struct amdgpu_device *adev = drm_to_adev(drm);
	struct dentry *ent, *root = minor->debugfs_root;

	ent = debugfs_create_file("gfx_jobtimeout_to_panic", 0644, root, adev,
				  &sgpu_debugfs_gfx_jobtimeout_to_panic_fops);
	if (!ent) {
		DRM_ERROR("unable to create gfx jobtimeout_to_panic debugfs file\n");
		return -EIO;
	}
	adev->sgpu_debug.debugfs_gfx_jobtimeout_to_panic = ent;

	return 0;
}
static int sgpu_comp_jobtimeout_to_panic_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	*val = adev->sgpu_debug.comp_jobtimeout_to_panic ? 1 : 0;

	return 0;
}

static int sgpu_comp_jobtimeout_to_panic_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	if (val > 1)
		return -EINVAL;

	adev->sgpu_debug.comp_jobtimeout_to_panic = val ? true : false;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sgpu_debugfs_comp_jobtimeout_to_panic_fops,
			sgpu_comp_jobtimeout_to_panic_get,
			sgpu_comp_jobtimeout_to_panic_set, "%llu\n");

static int sgpu_debugfs_comp_jobtimeout_to_panic_init(struct drm_minor *minor)
{
	struct drm_device *drm = minor->dev;
	struct amdgpu_device *adev = drm_to_adev(drm);
	struct dentry *ent, *root = minor->debugfs_root;

	ent = debugfs_create_file("comp_jobtimeout_to_panic", 0644, root, adev,
				  &sgpu_debugfs_comp_jobtimeout_to_panic_fops);
	if (!ent) {
		DRM_ERROR("unable to create comp jobtimeout_to_panic debugfs file\n");
		return -EIO;
	}
	adev->sgpu_debug.debugfs_comp_jobtimeout_to_panic = ent;

	return 0;
}
#else /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */
static int sgpu_debugfs_gfx_jobtimeout_to_panic_init(struct drm_minor *minor)
{
	return 0;
}
static int sgpu_debugfs_comp_jobtimeout_to_panic_init(struct drm_minor *minor)
{
	return 0;
}
#endif  /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */

#if defined(CONFIG_DRM_SGPU_EXYNOS) && defined(CONFIG_DEBUG_FS)
static int sgpu_pagefault_to_panic_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	*val = adev->sgpu_debug.pagefault_to_panic ? 1 : 0;

	return 0;
}

static int sgpu_pagefault_to_panic_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	if (val > 1)
		return -EINVAL;

	adev->sgpu_debug.pagefault_to_panic = val ? true : false;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sgpu_debugfs_pagefault_to_panic_fops,
			sgpu_pagefault_to_panic_get,
			sgpu_pagefault_to_panic_set, "%llu\n");

static int sgpu_debugfs_pagefault_to_panic_init(struct drm_minor *minor)
{
	struct drm_device *drm = minor->dev;
	struct amdgpu_device *adev = drm_to_adev(drm);
	struct dentry *ent, *root = minor->debugfs_root;

	ent = debugfs_create_file("pagefault_to_panic", 0644, root, adev,
				  &sgpu_debugfs_pagefault_to_panic_fops);
	if (!ent) {
		DRM_ERROR("unable to create pagefault_to_panic debugfs file\n");
		return -EIO;
	}
	adev->sgpu_debug.debugfs_pagefault_to_panic = ent;

	return 0;
}
#else /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */
static int sgpu_debugfs_pagefault_to_panic_init(struct drm_minor *minor)
{
	return 0;
}
#endif  /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */

#if defined(CONFIG_DRM_SGPU_EXYNOS) && defined(CONFIG_DEBUG_FS)
static int sgpu_didt_threshold_freq_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)m->private;

	seq_printf(m, "%u\n", adev->didt_edc.didt_threshold_freq);

	return 0;
}

static ssize_t sgpu_didt_threshold_freq_write(struct file *f, const char __user *data,
		size_t len, loff_t *loff)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	char buf[10] = {0, };
	int err;
	u64 value;

	if (len >= 10)
		return -1;

	if (copy_from_user(&buf, data, len))
		return -1;

	err = kstrtou64(buf, 10, &value);
	if (err)
		return err;

	adev->didt_edc.didt_threshold_freq = (unsigned long)value;

	return len;
}

static int sgpu_didt_threshold_freq_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_didt_threshold_freq_show, inode->i_private);
}

static struct file_operations sgpu_debugfs_didt_threshold_freq_fops = {
	.owner	 = THIS_MODULE,
	.open	 = sgpu_didt_threshold_freq_open,
	.read	 = seq_read,
	.write   = sgpu_didt_threshold_freq_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static int sgpu_edc_threshold_freq_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)m->private;

	seq_printf(m, "%u\n", adev->didt_edc.edc_threshold_freq);

	return 0;
}

static ssize_t sgpu_edc_threshold_freq_write(struct file *f, const char __user *data,
		size_t len, loff_t *loff)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	char buf[10] = {0, };
	int err;
	u64 value;

	if (len >= 10)
		return -1;

	if (copy_from_user(&buf, data, len))
		return -1;

	err = kstrtou64(buf, 10, &value);
	if (err)
		return err;

	adev->didt_edc.edc_threshold_freq = (unsigned long)value;

	return len;
}

static int sgpu_edc_threshold_freq_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_edc_threshold_freq_show, inode->i_private);
}

static struct file_operations sgpu_debugfs_edc_threshold_freq_fops = {
	.owner	 = THIS_MODULE,
	.open	 = sgpu_edc_threshold_freq_open,
	.read	 = seq_read,
	.write   = sgpu_edc_threshold_freq_write,
	.llseek  = seq_lseek,
	.release = single_release
};

static int sgpu_didt_edc_value_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)m->private;
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	int i, r = 0;

	if (adev->runpm) {
		r = pm_runtime_get_sync(adev->ddev.dev);
		if (r < 0)
			goto pm_put;
	}
	sgpu_ifpo_lock(adev);

	seq_puts(m, "Table values:\n");
	for (i = 0; i < DIDT_IND_INDEX_MAX_VALUE; ++i) {
		seq_printf(m, "%#010x ", didt->values[i]);
		if ((i + 1) % 8 == 0)
			seq_puts(m, "\n");
	}

	seq_puts(m, "Read registers:\n");
	for (i = 0; i < DIDT_IND_INDEX_MAX_VALUE; ++i) {
		seq_printf(m, "%#010x ", RREG32_DIDT(i));
		if ((i + 1) % 8 == 0)
			seq_puts(m, "\n");
	}

	sgpu_ifpo_unlock(adev);
pm_put:
	if (adev->runpm)
		pm_runtime_put(adev->ddev.dev);

	return 0;
}

static ssize_t sgpu_didt_edc_value_write(struct file *f, const char __user *data,
		size_t len, loff_t *loff)
{
	struct amdgpu_device *adev = file_inode(f)->i_private;
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	char buf[20] = {0, };
	int r;
	u32 idx, val;

	if (len >= 20)
		return -1;

	if (copy_from_user(&buf, data, len))
		return -1;

	r = sscanf(buf, "%x %x", &idx, &val);
	if (r != 2)
		return r;

	if (idx >= DIDT_IND_INDEX_MAX_VALUE)
		return -1;

	didt->values[idx] = (uint32_t)val;

	return len;
}

static int sgpu_didt_edc_value_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_didt_edc_value_show, inode->i_private);
}

static struct file_operations sgpu_debugfs_didt_edc_value_fops = {
	.owner	 = THIS_MODULE,
	.open	 = sgpu_didt_edc_value_open,
	.read	 = seq_read,
	.write   = sgpu_didt_edc_value_write,
	.llseek  = seq_lseek,
	.release = single_release
};

int sgpu_debugfs_didt_edc_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev_to_drm(adev)->render;
	struct dentry *root = minor->debugfs_root;
	struct sgpu_didt_edc *didt = &adev->didt_edc;

	didt->debugfs_didt_threshold_freq = debugfs_create_file("sgpu_didt_threshold_freq",
			0644, root, adev,
			&sgpu_debugfs_didt_threshold_freq_fops);
	if (!didt->debugfs_didt_threshold_freq) {
		DRM_ERROR("unable to create didt_threshold_freq debugfs file\n");
		return -EIO;
	}

	didt->debugfs_edc_threshold_freq = debugfs_create_file("sgpu_edc_threshold_freq",
			0644, root, adev,
			&sgpu_debugfs_edc_threshold_freq_fops);
	if (!didt->debugfs_edc_threshold_freq) {
		DRM_ERROR("unable to create edc_threshold_freq debugfs file\n");
		return -EIO;
	}

	didt->debugfs_didt_edc_values = debugfs_create_file("sgpu_didt_edc",
			0644, root, adev,
			&sgpu_debugfs_didt_edc_value_fops);
	if (!didt->debugfs_didt_edc_values) {
		DRM_ERROR("unable to create sgpu_didt_edc debugfs file\n");
		return -EIO;
	}

	return 0;
}
#else /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */
int sgpu_debugfs_didt_edc_init(struct amdgpu_device *adev)
{
	return 0;
}
#endif  /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */

#if defined(CONFIG_DRM_SGPU_EXYNOS) && defined(CONFIG_DEBUG_FS)
static int sgpu_power_state_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = m->private;
	struct sgpu_pm_stats pm_stats;
	int i;
	static const char *power_states[SGPU_POWER_STATE_NR] = {"Active",
							"IFPO power off",
							"Device Power UP",
							"Device Power DN"};

	sgpu_pm_monitor_get_stats(adev, &pm_stats);

	seq_printf(m, "Accumulated SGPU power time\n");
	seq_printf(m, "STATE            TIME(us)\n");

	for (i = 0; i < SGPU_POWER_STATE_NR; ++i)
		seq_printf(m, "%16s : %10llu\n", power_states[i],
				pm_stats.state_time[i] / 1000);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(sgpu_power_state);

static int sgpu_debugfs_power_state_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev_to_drm(adev)->render;
	struct dentry *ent, *root = minor->debugfs_root;

	ent = debugfs_create_file("sgpu_power_state", 0444, root, adev,
				  &sgpu_power_state_fops);
	if (!ent) {
		DRM_ERROR("unable to create sgpu_power_state debugfs file\n");
		return -EIO;
	}

	adev->pm_monitor.debugfs_power_state = ent;

	return 0;
}
#else /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */
static int sgpu_debugfs_power_state_init(struct amdgpu_device *adev)
{
	return 0;
}
#endif  /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */

#if defined(CONFIG_DRM_SGPU_EXYNOS) && defined(CONFIG_DEBUG_FS)
static void sgpu_entity_show(struct seq_file *m, struct amdgpu_ctx_entity *e)
{
	int i;
	struct dma_fence *fence;

	seq_printf(m, "\t\tpriority=%d rq=%s[%ld] job_queue_count=%d\n",
			e->entity.priority, e->entity.rq->sched->name,
			e->entity.rq - e->entity.rq->sched->sched_rq,
			spsc_queue_count(&e->entity.job_queue));

	seq_printf(m, "\t\tdrm fence info: scheduled(%llu) finished(%llu) seq(%d)\n",
			e->entity.fence_context, e->entity.fence_context + 1,
			atomic_read(&e->entity.fence_seq));

	fence = dma_fence_get(e->entity.last_scheduled);
	if (fence)
		seq_printf(m, "\t\tlast scheduled: %s/%s %llu/%llu t=%llu\n",
				fence->ops->get_driver_name(fence),
				fence->ops->get_timeline_name(fence),
				fence->context, fence->seqno,
				dma_fence_is_signaled(fence) ?
				dma_fence_timestamp(fence) : 0);
	dma_fence_put(fence);

	fence = dma_fence_get(e->entity.dependency);
	if (fence)
		seq_printf(m, "\t\tdependency: %s/%s %llu/%llu t=%llu\n",
				fence->ops->get_driver_name(fence),
				fence->ops->get_timeline_name(fence),
				fence->context, fence->seqno,
				dma_fence_is_signaled(fence) ?
				dma_fence_timestamp(fence) : 0);
	dma_fence_put(fence);

	seq_printf(m, "\t\tfences: last handle(%llu) idx(%llu)\n", e->sequence - 1,
			(e->sequence - 1) & (amdgpu_sched_jobs - 1));

	for (i = 0; i < amdgpu_sched_jobs; ++i) {
		struct drm_sched_fence *s_fence;
		struct dma_fence *scheduled, *hw_fence;
		struct dma_fence *finished = dma_fence_get(e->fences[i]);

		if (!finished)
			continue;

		s_fence = to_drm_sched_fence(finished);
		scheduled = dma_fence_get(&s_fence->scheduled);
		hw_fence = dma_fence_get(s_fence->parent);

		seq_printf(m, "\t\t[%2d]: %s/%s %llu/%llu/%llu HW=%llu/%llu timestamp=%llu/%llu\n",
				i, scheduled->ops->get_driver_name(finished),
				scheduled->ops->get_timeline_name(finished),
				scheduled->context,
				finished->context, finished->seqno,
				hw_fence ? hw_fence->context : 0,
				hw_fence ? hw_fence->seqno : 0,
				dma_fence_is_signaled(scheduled) ?
				dma_fence_timestamp(scheduled) : 0,
				dma_fence_is_signaled(finished) ?
				dma_fence_timestamp(finished) : 0);

		dma_fence_put(hw_fence);
		dma_fence_put(scheduled);
		dma_fence_put(finished);
	}
}

static int sgpu_context_show(int id, void *p, void *data)
{
	struct amdgpu_ctx *ctx = (struct amdgpu_ctx *)p;
	struct seq_file *m = (struct seq_file *)data;
	int i, j, entity_count = 0;

	seq_printf(m, "CONTEXT[%d]\n", id);
	seq_printf(m, "\tpriority(%d) init_prio(%u) override_prio(%d) secure(%u)\n",
				ctx->ctx_priority, ctx->init_priority,
				ctx->override_priority, ctx->secure_mode);
	seq_printf(m, "\tcwsr(%d) cwsr_init(%u) tmz(%d)\n",
				ctx->cwsr, ctx->cwsr_init, ctx->tmz);
	seq_printf(m, "\tperf_counter(gfx/compute): pc=%#x/%#x sqtt=%#x/%#x\n",
				ctx->pc_gfx_rings, ctx->pc_compute_rings,
				ctx->sqtt_gfx_rings, ctx->sqtt_compute_rings);

	for (i = 0; i < AMDGPU_HW_IP_NUM; ++i) {
		for (j = 0; j < AMDGPU_MAX_ENTITY_NUM; ++j) {
			struct amdgpu_ctx_entity *entity = ctx->entities[i][j];

			if (!entity)
				continue;

			seq_printf(m, "\tEntity: HW_IP(%d) IDX(%d)\n", i, j);
			sgpu_entity_show(m, entity);
			++entity_count;
		}
	}

	if (ctx->cwsr || ctx->tmz) {
		if (ctx->priv_entities) {
			seq_puts(m, "\tInternal entity:\n");
			sgpu_entity_show(m, ctx->priv_entities);
		} else
			seq_puts(m, "\tInternal entity is not ready\n");

		++entity_count;
	}

	if (!entity_count)
		seq_puts(m, "\tNo entity exists\n");

	return 0;
}

static int sgpu_context_list_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = m->private;
	struct drm_device *ddev = &adev->ddev;
	struct drm_file *filp;
	const char *bar = "======================================";

	seq_puts(m, "SGPU context/entity lists in drm_files\n");

	mutex_lock(&ddev->filelist_mutex);
	list_for_each_entry_reverse(filp, &ddev->filelist, lhead) {
		struct amdgpu_fpriv *fpriv = filp->driver_priv;

		if (!fpriv || !fpriv->vm.task_info.pid)
			continue;

		seq_printf(m, "\n%s%s\nPROCESS: %s  TASK: %s  PID(%u) TGID(%u)\n%s%s\n",
				bar, bar, fpriv->vm.task_info.process_name,
				fpriv->vm.task_info.task_name,
				fpriv->vm.task_info.pid,
				fpriv->vm.task_info.tgid, bar, bar);

		mutex_lock(&fpriv->ctx_mgr.lock);
		if (idr_is_empty(&fpriv->ctx_mgr.ctx_handles)) {
			seq_puts(m, "No context exists\n");
			mutex_unlock(&fpriv->ctx_mgr.lock);
			continue;
		}
		idr_for_each(&fpriv->ctx_mgr.ctx_handles, sgpu_context_show, m);
		mutex_unlock(&fpriv->ctx_mgr.lock);
	}
	mutex_unlock(&ddev->filelist_mutex);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(sgpu_context_list);

static int sgpu_debugfs_context_list_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev_to_drm(adev)->render;
	struct dentry *ent, *root = minor->debugfs_root;

	ent = debugfs_create_file("sgpu_context_list", 0444, root, adev,
				  &sgpu_context_list_fops);
	if (!ent) {
		DRM_ERROR("unable to create sgpu_context_list debugfs file\n");
		return -EIO;
	}

	adev->debugfs_context_list = ent;

	return 0;
}
#else /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */
static int sgpu_debugfs_context_list_init(struct amdgpu_device *adev)
{
	return 0;
}
#endif  /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */

#if defined(CONFIG_DRM_SGPU_EXYNOS) && defined(CONFIG_DEBUG_FS)
static void sgpu_rq_show(struct seq_file *m, struct drm_sched_rq *rq)
{
	struct drm_sched_entity *entity;
	struct dma_fence *fence;
	int entity_count = 0;

	spin_lock(&rq->lock);
	list_for_each_entry(entity, &rq->entities, list) {
		seq_printf(m, "\t  Entity[%d]: %llu/%llu seq(%d) ",
				entity_count++, entity->fence_context,
				entity->fence_context + 1,
				atomic_read(&entity->fence_seq));

		seq_printf(m, "job_count(%d) oldest_job_submit_t=%llu\n",
				spsc_queue_count(&entity->job_queue),
				entity->oldest_job_waiting);

		fence = dma_fence_get(entity->last_scheduled);
		if (fence)
			seq_printf(m, "\t\tlast scheduled: %s/%s %llu/%llu t=%llu\n",
					fence->ops->get_driver_name(fence),
					fence->ops->get_timeline_name(fence),
					fence->context, fence->seqno,
					dma_fence_is_signaled(fence) ?
					dma_fence_timestamp(fence) : 0);
		else
			seq_puts(m, "\t\tlast scheduled: NULL\n");
		dma_fence_put(fence);

		fence = dma_fence_get(entity->dependency);
		if (fence)
			seq_printf(m, "\t\tdependency: %s/%s %llu/%llu t=%llu\n",
					fence->ops->get_driver_name(fence),
					fence->ops->get_timeline_name(fence),
					fence->context, fence->seqno,
					dma_fence_is_signaled(fence) ?
					dma_fence_timestamp(fence) : 0);
		else
			seq_puts(m, "\t\tdependency: NULL\n");
		dma_fence_put(fence);
	}
	spin_unlock(&rq->lock);

	if (!entity_count)
		seq_puts(m, "\t  No entity exists\n");
}

static void sgpu_sched_show(struct seq_file *m, struct drm_gpu_scheduler *sched)
{
	struct drm_sched_job *s_job;
	int i;

	seq_printf(m, "SCHEDULER - %s\n", sched->name);
	seq_printf(m, "\tready(%u) timeout_ms(%u) submission_limit(%u)\n",
			sched->ready, jiffies_to_msecs(sched->timeout),
			sched->hw_submission_limit);
	seq_printf(m, "\tscore(%d) last_job_id(%llu) running_job_count(%d)\n",
			atomic_read(sched->score),
			atomic64_read(&sched->job_id_count),
			atomic_read(&sched->hw_rq_count));

	for (i = 0; i < DRM_SCHED_PRIORITY_COUNT; ++i) {
		struct drm_sched_rq *rq = &sched->sched_rq[i];

		seq_printf(m, "\trun_queue[%d]:\n", i);
		sgpu_rq_show(m, rq);
	}

	spin_lock(&sched->job_list_lock);

	if (list_empty(&sched->pending_list)) {
		seq_puts(m, "\tNo job in pending list\n\n");
		spin_unlock(&sched->job_list_lock);
		return;
	}

	seq_puts(m, "\tpending_list:\n");
	list_for_each_entry(s_job, &sched->pending_list, list) {
		struct drm_sched_fence *fence = s_job->s_fence;

		seq_printf(m, "\t  job-%llu: drm(%llu/%llu/%llu) hw(%llu/%llu) sched_time=%llu\n",
				s_job->id, fence->scheduled.context,
				fence->finished.context, fence->finished.seqno,
				fence->parent->context, fence->parent->seqno,
				dma_fence_timestamp(&fence->scheduled));
	}
	spin_unlock(&sched->job_list_lock);

	seq_puts(m, "\n");
}

static int sgpu_sched_list_show(struct seq_file *m, void *p)
{
	struct amdgpu_device *adev = m->private;
	const char *bar = "======================================";
	int i, j, k, sched_count;

	seq_puts(m, "SGPU scheduler lists\n");

	for (i = 0; i < AMDGPU_HW_IP_NUM; ++i) {
		for (j = 0, sched_count = 0; j < DRM_SCHED_PRIORITY_COUNT; ++j)
			sched_count += adev->gpu_sched[i][j].num_scheds;

		if (!sched_count)
			continue;

		seq_printf(m, "\n%s%s\nScheduler list for HW_IP(%d)\n%s%s\n",
				bar, bar, i, bar, bar);

		/* large value has high priority */
		for (j = DRM_SCHED_PRIORITY_COUNT - 1; j >= 0; --j) {
			struct amdgpu_sched *sched = &adev->gpu_sched[i][j];

			seq_printf(m, "[ PRIORITY %d ]\n", j);

			if (!sched->num_scheds) {
				seq_puts(m, "\tNo scheduler exists\n\n");
				continue;
			}

			for (k = 0; k < sched->num_scheds; ++k)
				sgpu_sched_show(m, sched->sched[k]);

			seq_puts(m, "\n");
		}
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(sgpu_sched_list);

static int sgpu_debugfs_sched_list_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev_to_drm(adev)->render;
	struct dentry *ent, *root = minor->debugfs_root;

	ent = debugfs_create_file("sgpu_sched_list", 0444, root, adev,
				  &sgpu_sched_list_fops);
	if (!ent) {
		DRM_ERROR("unable to create sgpu_sched_list debugfs file\n");
		return -EIO;
	}

	adev->debugfs_sched_list = ent;

	return 0;
}
#else /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */
static int sgpu_debugfs_sched_list_init(struct amdgpu_device *adev)
{
	return 0;
}
#endif  /* CONFIG_DRM_SGPU_EXYNOS && CONFIG_DEBUG_FS */

void amdgpu_debugfs_init(struct drm_minor *minor)
{
	struct drm_device *drm = minor->dev;
	struct amdgpu_device *adev = drm_to_adev(drm);
	int r, i;

	/*
	 * sgpu driver creates two dri nodes. So amdgpu_debugfs_init() is called twice and the
	 * second call always fails because debugfs files are already created.
	 */
	if (adev->debugfs_preempt)
		return;

	adev->debugfs_preempt =
		debugfs_create_file("amdgpu_preempt_ib", 0600,
				    drm->render->debugfs_root, adev,
				    &fops_ib_preempt);
	if (IS_ERR(adev->debugfs_preempt)) {
		DRM_ERROR("unable to create amdgpu_preempt_ib debugsfs file\n");
		return;
	}

	/* Register debugfs entries for amdgpu_ttm */
	r = amdgpu_ttm_debugfs_init(adev);
	if (r)
		DRM_ERROR("Failed to init TTM debugfs\n");

	if (amdgpu_debugfs_sa_init(adev)) {
		DRM_ERROR("failed to register debugfs file for SA\n");
	}

	if (amdgpu_debugfs_fence_init(adev))
		DRM_ERROR("fence debugfs file creation failed\n");

	r = amdgpu_debugfs_gem_init(adev);
	if (r)
		DRM_ERROR("registering gem debugfs failed (%d).\n", r);

	r = amdgpu_debugfs_regs_init(adev);
	if (r)
		DRM_ERROR("registering register debugfs failed (%d).\n", r);

	r = amdgpu_debugfs_firmware_init(adev);
	if (r)
		DRM_ERROR("registering firmware debugfs failed (%d).\n", r);

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring)
			continue;

		if (amdgpu_debugfs_ring_init(adev, ring)) {
			DRM_ERROR("Failed to register debugfs file for rings !\n");
		}
	}

	if (sgpu_debugfs_gfx_jobtimeout_to_panic_init(minor) != 0)
		return;

	if (sgpu_debugfs_comp_jobtimeout_to_panic_init(minor) != 0)
		return;

	if (sgpu_debugfs_pagefault_to_panic_init(minor) != 0)
		return;

	if (sgpu_ifpo_debugfs_init(minor) != 0)
		return;

	if (sgpu_debugfs_power_state_init(adev) != 0)
		return;

	if (sgpu_debugfs_gvf_init(adev) != 0)
		return;

	if (sgpu_debugfs_bpmd_init(adev) != 0) {
		DRM_ERROR("unable to create sgpu_bpmd debugsfs file\n");
		return;
	}

	if (sgpu_debugfs_didt_edc_init(adev) != 0)
		return;

	if (sgpu_debugfs_dmsg_init(adev) != 0)
		return;

	if (sgpu_instance_data_debugfs_init(adev) != 0)
		return;

	if (cwsr_enable || amdgpu_tmz)
		amdgpu_sws_init_debugfs(adev);

	sgpu_debugfs_pagealloc_init(adev);

	if (sgpu_debugfs_context_list_init(adev) != 0)
		return;

	if (sgpu_debugfs_sched_list_init(adev) != 0)
		return;

	r = amdgpu_debugfs_add_files(adev, amdgpu_debugfs_list,
					ARRAY_SIZE(amdgpu_debugfs_list));
	if (r)
		DRM_ERROR("Failed creating debugfs files\n");

	return;
}

#else
void amdgpu_debugfs_init(struct drm_minor *minor)
{
	return 0;
}
int amdgpu_debugfs_regs_init(struct amdgpu_device *adev)
{
	return 0;
}
#endif

#if defined(CONFIG_DRM_SGPU_BPMD) && defined(CONFIG_DEBUG_FS)
static int sgpu_debugfs_bpmd_show(struct seq_file *m, void *p)
{
	seq_printf(m, "Write 1 to this file to force trigger BPMD\n");
	return 0;
}

static int sgpu_debugfs_bpmd_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_debugfs_bpmd_show, inode->i_private);
}

static ssize_t sgpu_debugfs_bpmd_write(struct file *f, const char __user *data,
				       size_t len, loff_t *loff)
{
	char buf = '0';
	const size_t buf_size = 1;

	if(copy_from_user(&buf, data, buf_size))
		return -1;

	if (buf == '1') {
		struct amdgpu_device *sdev = file_inode(f)->i_private;
		sgpu_bpmd_dump(sdev);
	}

	return len;
}

static struct file_operations sgpu_debugfs_bpmd_fops = {
	.open    = sgpu_debugfs_bpmd_open,
	.read    = seq_read,
	.write   = sgpu_debugfs_bpmd_write,
	.llseek  = seq_lseek,
	.release = single_release
};

int sgpu_debugfs_bpmd_init(struct amdgpu_device *sdev)
{
	sdev->debugfs_bpmd =
		debugfs_create_file("sgpu_bpmd", S_IRUSR | S_IWUSR,
				    adev_to_drm(sdev)->render->debugfs_root,
				    (void *)sdev, &sgpu_debugfs_bpmd_fops);

	if (sdev->debugfs_bpmd == NULL) {
		pr_err("unable to create sgpu_bpmd debugsfs file\n");
		return -EIO;
	}
	return 0;
}

void sgpu_debugfs_bpmd_cleanup(struct amdgpu_device *sdev)
{
	if (sdev->debugfs_bpmd != NULL)
		debugfs_remove(sdev->debugfs_bpmd);
}


#else  /* CONFIG_DRM_SGPU_BPMD && CONFIG_DEBUG_FS */

int sgpu_debugfs_bpmd_init(struct amdgpu_device *sdev)
{
	return 0;
}

void sgpu_debugfs_bpmd_cleanup(struct amdgpu_device *sdev) { }

#endif  /* CONFIG_DRM_SGPU_BPMD && CONFIG_DEBUG_FS */
