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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <linux/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/bug.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>

#include "npu-log.h"
#include "npu-vertex.h"
#include "npu-syscall.h"
#include "npu-device.h"
#include "npu-session.h"
#include "include/npu-common.h"
#include "npu-scheduler.h"
#include "include/npu-stm-soc.h"
#include "include/npu-system-soc.h"
#include "npu-hw-device.h"
#include "npu-ver.h"
#include "include/vs4l.h"
#include "npu-util-regs.h"
#include "npu-util-memdump.h"
#include "npu-afm.h"
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "npu-governor.h"
#endif
#include "include/npu-memory.h"

#define DEFAULT_KPI_MODE	false	/* can be changed to true while testing */

bool is_kpi_mode_enabled(bool strict)
{
#if IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	return false;
#else
	struct npu_scheduler_info *info;
	info = npu_scheduler_get_info();

	if (info->mode_ref_cnt[NPU_PERF_MODE_NPU_BOOST_BLOCKING] > 0)
		return true;
	else if (info->mode_ref_cnt[NPU_PERF_MODE_NPU_BOOST_PRUNE] > 0)
		return true;
	else if (strict && info->mode_ref_cnt[NPU_PERF_MODE_NPU_BOOST] > 0)
		return true;
	else if (info->dd_direct_path == 0x01)
		return true;
	else
		return (DEFAULT_KPI_MODE);
#endif
}

static int __force_streamoff(struct file *file);

static int __vref_open(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	atomic_set(&vertex->start_cnt.refcount, 0);
	return npu_device_open(device);
}

static int __vref_close(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	if (unlikely(!device)) {
		npu_err("Failed to get npu_device\n");
		return -EINVAL;
	}

	return npu_device_close(device);
}

static int __vref_start(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	if (unlikely(!device)) {
		npu_err("Failed to get npu_device\n");
		return -EINVAL;
	}

	return npu_device_start(device);
}

static int __vref_stop(struct npu_vertex *vertex)
{
	struct npu_device *device;

	device = container_of(vertex, struct npu_device, vertex);
	if (unlikely(!device)) {
		npu_err("Failed to get npu_device\n");
		return -EINVAL;
	}

	return npu_device_stop(device);
}

static inline void __vref_init(struct npu_vertex_refcount *vref,
	struct npu_vertex *vertex, int (*first)(struct npu_vertex *vertex), int (*final)(struct npu_vertex *vertex))
{
	vref->vertex = vertex;
	vref->first = first;
	vref->final = final;
	atomic_set(&vref->refcount, 0);
}

static inline int __vref_get(struct npu_vertex_refcount *vref)
{
	return (atomic_inc_return(&vref->refcount) == 1) ? vref->first(vref->vertex) : 0;
}

static inline int __vref_put(struct npu_vertex_refcount *vref)
{
	return (atomic_dec_return(&vref->refcount) == 0) ? vref->final(vref->vertex) : 0;
}

static inline int check_emergency_vctx(struct npu_vertex_ctx *vctx)
{
	struct npu_vertex *__vertex = vctx->vertex;
	struct npu_device *__device = container_of(__vertex, struct npu_device, vertex);

	return check_emergency(__device);
}

int npu_vertex_s_graph(struct file *file, struct vs4l_graph *sinfo)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_POWER))) {
		npu_ierr("invalid state(%X)\n", vctx, session->hids, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_s_graph(session, sinfo);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_config\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	vctx->state |= BIT(NPU_VERTEX_GRAPH);

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_open(struct file *file)
{
	int ret = 0;
	struct npu_vertex *vertex = dev_get_drvdata(&vision_devdata(file)->dev);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct mutex *lock = &vertex->lock;
	struct npu_vertex_ctx *vctx = NULL;
	struct npu_session *session = NULL;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	if (unlikely(!device)) {
		npu_err("Failed to get npu_device");
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency(device);
	if (ret) {
		npu_warn("Open in emergency recovery - returns -ELIBACC,ret = %d\n", ret);
		return -ELIBACC;
	}

	if (mutex_lock_interruptible(lock)) {
		npu_err("fail in mutex_lock_interruptible\n");
		return -ERESTARTSYS;
	}
	ret = __vref_get(&vertex->open_cnt);
	if (check_emergency(device)) {
		npu_err("start for emergency recovery\n");
		__vref_put(&vertex->open_cnt);
		npu_device_recovery_close(device);
		ret = -EWOULDBLOCK;
		goto ErrorExit;
	}
	if (ret) {
		npu_err("fail(%d) in vref_get", ret);
		__vref_put(&vertex->open_cnt);
		goto ErrorExit;
	}

	ret = npu_session_open(&session, &device->sessionmgr, &device->system.memory, device->system.manual);
	if (ret) {
		npu_err("fail(%d) in npu_graph_create", ret);
		__vref_put(&vertex->open_cnt);
		goto ErrorExit;
	}

	/* set max npu core for the SOC */
	session->sched_param.max_npu_core = device->system.max_npu_core;

	vctx			= &session->vctx;
	vctx->id		= session->uid;
	vctx->vertex		= vertex;
	mutex_init(&vctx->lock);

	ret += npu_queue_open(vctx->queue.inqueue);
	ret += npu_queue_open(vctx->queue.otqueue);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_open", vctx, session->hids, ret);
		npu_session_undo_open(session);
		__vref_put(&vertex->open_cnt);
		goto ErrorExit;
	}

	file->private_data = vctx;
	vctx->state |= BIT(NPU_VERTEX_OPEN);

	npu_iinfo("(%d), open_ref(%d)\n", vctx, session->hids, ret, atomic_read(&vertex->open_cnt.refcount));
ErrorExit:
	mutex_unlock(lock);
	if (ret < -1000) {
		/* Return value is not acceptable as open's result */
		npu_warn("Error [%d/%x] - convert return value to -ELIBACC\n", ret, ret);
		return -ELIBACC;
	}
	return ret;
}

enum npu_vertex_state check_done_state(u32 state)
{
	u32 ret = 0;

	if (state & BIT(NPU_VERTEX_OPEN))
		ret = NPU_VERTEX_OPEN;
	if (state & BIT(NPU_VERTEX_POWER))
		ret = NPU_VERTEX_POWER;
	if (state & BIT(NPU_VERTEX_GRAPH))
		ret = NPU_VERTEX_GRAPH;
	if (state & BIT(NPU_VERTEX_FORMAT))
		ret = NPU_VERTEX_FORMAT;
	if (state & BIT(NPU_VERTEX_STREAMOFF))
		ret = NPU_VERTEX_STREAMOFF;
	return ret;
}

static void is_cpuidle_lock_on_hold(struct npu_vertex *vertex, struct npu_scheduler_info *info) {
	/* cpuidle_cnt can be > 0 if an application closed the session without
	 * changing boost mode to normal mode
	 * if this the last session to be closed, set the cnt to 1 and use the
	 * npu_scheduler_set_cpuidle function to re-enable cpuidle
	 */
	if ((atomic_read(&vertex->open_cnt.refcount) == 0) &&
		(atomic_read(&info->cpuidle_cnt) != 0)) {
		npu_err("cpuidle_count: %d\n", atomic_read(&info->cpuidle_cnt));
		atomic_set(&info->cpuidle_cnt, 1); // to do cpuidle resume, the count should be 1.
		npu_scheduler_set_cpuidle(NPU_SCH_DEFAULT_VALUE); // resume cpuidle and releases lock
	}
}

int npu_vertex_close(struct file *file)
{
	int ret = 0;
	bool is_skip = false;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vertex->lock;
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct npu_scheduler_info *info;
	enum npu_vertex_state done_state;
	int id = vctx->id;
	int hids = session->hids;
	bool is_bootup_error_handling_called = session->is_bootup_error_handling_called;

	if (unlikely(!session)) {
		npu_err("Failed to get npu_session\n");
		return -EINVAL;
	}

	if (unlikely(!device)) {
		npu_ierr("Failed to get npu_device\n", vctx, session->hids);
		return -EINVAL;
	}

	if (unlikely(!queue)) {
		npu_ierr("Failed to get npu_queue\n", vctx, session->hids);
		return -EINVAL;
	}

	is_skip = npu_device_check_and_lock();

	mutex_lock(lock);

	done_state = check_done_state(vctx->state);
	switch (done_state) {
	case NPU_VERTEX_STREAMOFF:
		goto normal_complete;
	case NPU_VERTEX_FORMAT:
		goto force_streamoff;
	case NPU_VERTEX_GRAPH:
	case NPU_VERTEX_POWER:
	case NPU_VERTEX_OPEN:
		goto session_free;
	default:
		ret = -EINVAL;
		npu_ierr("fail(%d) in done_state\n", vctx, session->hids, ret);
		goto p_err;
	}
force_streamoff:
	ret = __force_streamoff(file);
	if (ret) {
		npu_ierr("fail(%d) in __force_streamoff", vctx, session->hids, ret);
		goto p_err;
	}
normal_complete:
	ret = npu_session_NW_CMD_UNLOAD(session);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_NW_CMD_UNLOAD", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}
	vctx->state |= BIT(NPU_VERTEX_CLOSE);
	npu_iinfo("(%d)\n", vctx, session->hids, ret);
session_free:
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
	npu_precision_active_info_hash_delete(session);
#endif
	npu_queue_close(queue, queue->inqueue);
	npu_queue_close(queue, queue->otqueue);

	ret = npu_queue_stop(queue);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_stop(forced)\n", vctx, session->hids, ret);
		goto p_err;
	}

	if (vctx->state & BIT(NPU_VERTEX_POWER)) {
		ret = npu_session_NW_CMD_POWER_NOTIFY(session, false);
		if (ret) {
			npu_ierr("fail(%d) in npu_session_NW_CMD_POWER_NOTIFY\n", vctx, session->hids, ret);
			npu_session_dump(session);
			goto p_err;
		}
	}
	vctx->state &= (~BIT(NPU_VERTEX_POWER));

	ret = npu_session_close(session);
	if (ret) {
		npu_err("[I%d] fail(%d) in npu_session_close", id, ret);
		npu_session_dump(session);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	/* should be closed before hw shutdown */
	if (atomic_read(&vertex->open_cnt.refcount) == 1) {
		npu_cmdq_table_read_close(&device->cmdq_table_info);
	}
#endif

	if (!check_emergency(device) && !is_bootup_error_handling_called) {
		ret = npu_hwdev_shutdown(device, hids);
		if (ret) {
			npu_err("[I%d] fail(%d) in npu_vertex_shutdown\n", id, ret);
			goto p_err;
		}
	}

	ret = __vref_put(&vertex->open_cnt);
	if (ret) {
		npu_err("[I%d] fail(%d) in vref_put", id, ret);
		goto p_err;
	}

	info = npu_scheduler_get_info();
	is_cpuidle_lock_on_hold(vertex, info);
p_err:
	npu_info("[I%d] open_ref(%d), is_bootup_error_handling_called : %d\n",
			id, atomic_read(&vertex->open_cnt.refcount), is_bootup_error_handling_called);
	mutex_unlock(lock);

	npu_device_unlock(is_skip);
	return ret;
}

unsigned int npu_vertex_poll(struct file *file, poll_table *poll)
{
	int mask = 0, ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);

	ret = wait_for_completion_timeout(&device->my_completion, msecs_to_jiffies(MAILBOX_CLEAR_CHECK_TIMEOUT));
	if (!ret) {
		npu_iwarn("timeout executed! [10 secs]\n", vctx, session->hids);
		npu_util_dump_handle_nrespone(&device->system);
	}

	mask |= EPOLLIN;
	reinit_completion(&device->my_completion);

	npu_profile_record_end(PROFILE_DD_TOP, 0, ktime_to_us(ktime_get_boottime()), 8, 0);
	npu_profile_iter_update(0, 0);

	return mask;
}

int npu_vertex_pdcl_plus(struct file *file, struct vs4l_pdcl_plus *pdcl_plus)
{
	int ret = 0;
#if NCP_VERSION >= 28
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct npu_system *system = &device->system;
	struct ncp_header *ncp_hdr = (struct ncp_header *)session->ncp_hdr_buf->dma.vaddr;
	u32 early_response = NCP_GET_(early_response, ncp_hdr);
	const u32 check_value = pdcl_plus->check_value;

	ret = wait_for_completion_timeout(&device->my_completion, msecs_to_jiffies(MAILBOX_CLEAR_CHECK_TIMEOUT));
	if (!ret) {
		npu_iwarn("timeout executed! [10 secs]\n", vctx, session->hids);
		npu_util_dump_handle_nrespone(system);
		ret = -EFAULT;
		goto err;
	} else
		ret = 0;

	reinit_completion(&device->my_completion);

	if (early_response) {
		volatile u32 *busy_trigger = (u32*)
			((volatile char *)npu_get_io_area(system, "sfrmboxmmap")->vaddr + 0x101C);
		mb();
		while (true) {
			/* busy waiting */
			if (*busy_trigger != check_value) {
				continue;
			} else {
				*busy_trigger = 0;
				break;
			}
		}
	}

	npu_profile_record_end(PROFILE_DD_TOP, 0, ktime_to_us(ktime_get_boottime()), 8, 0);
	npu_profile_iter_update(0, 0);
err:
#endif
	return ret;
}

int npu_vertex_map(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	if (unlikely(!vctx)) {
		npu_err("Failed to get npu_vertex_ctx\n");
		return -EINVAL;
	}

	if (!session) {
		npu_err("invalid session\n");
		return -EINVAL;
	}

	if (unlikely(!lock)) {
		npu_ierr("Failed to get mutex lock\n", vctx, session->hids);
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("invalid state\n", vctx, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_map(session);
	if (ret) {
		npu_ierr("npu_session_map is fail(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_unmap(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	if (unlikely(!vctx)) {
		npu_err("Failed to get npu_vertex_ctx\n");
		return -EINVAL;
	}

	if (!session) {
		npu_err("invalid session\n");
		return -EINVAL;
	}

	if (unlikely(!lock)) {
		npu_ierr("Failed to get mutex lock\n", vctx, session->hids);
		return -EINVAL;
	}

	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("invalid state\n", vctx, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_unmap(session);
	if (ret) {
		npu_ierr("npu_session_unmap is fail(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_pg_on(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	if (unlikely(!vctx)) {
		npu_err("Failed to get npu_vertex_ctx\n");
		return -EINVAL;
	}

	if (!session) {
		npu_err("invalid session\n");
		return -EINVAL;
	}

	if (unlikely(!lock)) {
		npu_ierr("Failed to get mutex lock\n", vctx, session->hids);
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_POWER))) {
		npu_ierr("invalid state\n", vctx, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	npu_session_pg_control(session, true);

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_pg_off(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	if (unlikely(!vctx)) {
		npu_err("Failed to get npu_vertex_ctx\n");
		return -EINVAL;
	}

	if (!session) {
		npu_err("invalid session\n");
		return -EINVAL;
	}

	if (unlikely(!lock)) {
		npu_ierr("Failed to get mutex lock\n", vctx, session->hids);
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_POWER))) {
		npu_ierr("invalid state\n", vctx, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	npu_session_pg_control(session, false);

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_flush(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	if (fatal_signal_pending(current) || (current->exit_code != 0)) {
		npu_dump("Flush caused by forced terminated. PID(%d), CUR(%s), EXIT_CODE(%d)\n",
				task_pid_nr(current), current->comm, current->exit_code);

		if (session != NULL) {
			session->is_force_terminated = 0xDEADC0DE;
			npu_dump("set force_terminated on session(uid : %u)\n", session->uid);
		}
	}

	if (session != NULL)
		npu_idbg("(%d)\n", vctx, session->hids, ret);

	return ret;
}

int npu_vertex_mmap(struct file *file, struct vm_area_struct *vm_area)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	size_t size = vm_area->vm_end - vm_area->vm_start;
	phys_addr_t offset = (phys_addr_t)vm_area->vm_pgoff << PAGE_SHIFT;
	struct npu_iomem_area *area = npu_get_io_area(&device->system, "sfrmboxmmap");

	/* Does it even fit in phys_addr_t? */
	if (offset >> PAGE_SHIFT != vm_area->vm_pgoff)
		return -EINVAL;

	/* It's illegal to wrap around the end of the physical address space. */
	if (offset + (phys_addr_t)size - 1 < offset)
		return -EINVAL;

	/* Does it fit in sfrmbox iomem area? */
	if (offset + (phys_addr_t)size > area->size)
		return -EINVAL;

	/* Indicates that the corresponding area is an I/O memory area. */
	vm_flags_set(vm_area, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	vm_area->vm_page_prot = pgprot_device(vm_area->vm_page_prot);

	if (io_remap_pfn_range(vm_area, vm_area->vm_start, area->paddr >> PAGE_SHIFT, size, vm_area->vm_page_prot)) {
		npu_ierr("io_remap_pfn_range error\n", vctx, session->hids);
		return -EAGAIN;
	}

	npu_idbg("(%d)\n", vctx, session->hids, ret);
	return ret;
}

int npu_vertex_probe(struct npu_vertex *vertex, struct device *parent)
{
	int ret = 0;
	struct vision_device *vdev;

	if (unlikely(!vertex)) {
		probe_err("Failed to get npu_vertex\n");
		ret = -EINVAL;
		goto p_err;
	}

	get_device(parent);
	mutex_init(&vertex->lock);
	__vref_init(&vertex->open_cnt, vertex, __vref_open, __vref_close);
	__vref_init(&vertex->start_cnt, vertex, __vref_start, __vref_stop);

	vdev = &vertex->vd;
	snprintf(vdev->name, sizeof(vdev->name), "%s", npu_vertex_name);
	vdev->release		= NULL;
	vdev->lock		= NULL;
	vdev->parent		= parent;

	dev_set_drvdata(&vdev->dev, vertex);

	ret = vision_register_device(vdev, npu_get_configs(NPU_MINOR), NULL);
	if (ret) {
		probe_err("fail(%d) in vision_register_device\n", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int npu_streamon(struct npu_vertex_ctx *vctx)
{
	int ret = 0;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_queue *queue = &vctx->queue;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	ret = __vref_get(&vertex->start_cnt);
	if (ret) {
		npu_ierr("fail(%d) in vref_get\n", vctx, session->hids, ret);
		goto p_err;
	}

	ret = npu_queue_start(queue);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_start\n", vctx, session->hids, ret);
		goto p_err;
	}

p_err:
	npu_iinfo("(%d), start_ref(%d)\n", vctx, session->hids, ret,
						atomic_read(&vertex->start_cnt.refcount));
	return ret;
}

int npu_vertex_s_format(struct file *file, struct vs4l_format_bundle *fbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
	npu_scheduler_boost_on(info);
	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_GRAPH))) {
		npu_ierr("invalid state(%X)\n", vctx, session->hids, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!session)) {
		npu_ierr("Failed to get npu_session\n", vctx, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_memory_update_sysmmu_status("s_format", session->memory, session);
	if (ret) {
		npu_ierr("fail(%d) in npu_memory_update_sysmmu_status\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	ret = npu_session_format(queue, fbundle);
	if (ret) {
		npu_ierr("fail(%d) in npu_queue_s_format\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	ret = npu_session_NW_CMD_LOAD(session);
	ret = chk_nw_result_no_error(session);
	if (ret == NPU_ERR_NO_ERROR)
		vctx->state |= BIT(NPU_VERTEX_FORMAT);
	else
		goto p_err;

	ret = npu_streamon(vctx);
	if (ret) {
		npu_ierr("npu_stream_on is fail(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}

	npu_iinfo("%d\n", vctx, session->hids, ret);
	mutex_unlock(lock);
	npu_scheduler_boost_off_timeout(info, NPU_SCHEDULER_BOOST_TIMEOUT);
	return ret;
p_err:
	vctx->state &= (~BIT(NPU_VERTEX_GRAPH));
	mutex_unlock(lock);
	npu_scheduler_boost_off_timeout(info, NPU_SCHEDULER_BOOST_TIMEOUT);
	return ret;
}

int npu_vertex_s_param(struct file *file, struct vs4l_param_list *plist)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_OPEN))) {
		npu_ierr("invalid state(%X)\n", vctx, session->hids, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_param(session, plist);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_param\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

p_err:
	npu_idbg("(%d)\n", vctx, session->hids, ret);
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_sched_param(struct file *file, struct vs4l_sched_param *param)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vctx->lock;

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_OPEN))) {
		npu_ierr("invalid state(%X)\n", vctx, session->hids, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_nw_sched_param(session, param);
	if (ret) {
		npu_ierr("fail(%d) in npu_vertex_sched_param\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

p_err:
	npu_iinfo("(%d)\n", vctx, session->hids, ret);
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_g_max_freq(struct file *file, struct vs4l_freq_param *param)
{
	int ret;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	ret = npu_dvfs_get_ip_max_freq(param);
	if (ret) {
		npu_ierr("fail in npu_dvfs_get_ip_max_freq\n", vctx, session->hids);
		return ret;
	}

	return ret;
}

int npu_vertex_s_ctrl(struct file *file, struct vs4l_ctrl *ctrl)
{
	int ret = 0;

	return ret;
}

int npu_vertex_qbuf(struct file *file, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	struct npu_vertex *vertex;
	struct npu_device *device;
#endif

	if (unlikely(!vctx)) {
		npu_err("Failed to get npu_vertex_ctx\n");
		return -EINVAL;
	}

	if (unlikely(!cbundle)) {
		npu_ierr("Failed to get vs4l_container_bundle\n", vctx, session->hids);
		return -EINVAL;
	}

	if (unlikely(!queue)) {
		npu_ierr("Failed to get npu_queue\n", vctx, session->hids);
		return -EINVAL;
	}

	if (unlikely(!lock)) {
		npu_ierr("Failed to get mutex lock\n", vctx, session->hids);
		return -EINVAL;
	}

	npu_profile_record_start(PROFILE_DD_TOP, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);
	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("invalid state\n", vctx, session->hids);
		ret = -EINVAL;
		goto p_err;
	}
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	vertex = vctx->vertex;
	device = container_of(vertex, struct npu_device, vertex);
	ret = start_cmdq_table_read(&device->cmdq_table_info);
	if (ret) {
		npu_ierr("start_cmdq_table_read is fail(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}
#endif
	ret = npu_queue_qbuf(queue, cbundle);
	if (ret) {
		npu_ierr("npu_queue_qbuf is fail(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_dqbuf(struct file *file, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;
	bool nonblocking = file->f_flags & O_NONBLOCK;

	if (unlikely(!queue)) {
		npu_ierr("Failed to get queue\n", vctx, session->hids);
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}
	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("invalid state(%X)\n", vctx, session->hids, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_queue_dqbuf(session, queue, cbundle, nonblocking);
	if (ret) {
		if (ret != -EWOULDBLOCK)
			npu_ierr("fail(%d) npu_queue_dqbuf\n", vctx, session->hids, ret);
		goto p_err;
	}

p_err:
	mutex_unlock(lock);
	npu_profile_record_end(PROFILE_DD_TOP, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);
	npu_profile_iter_update(cbundle->m[0].clist->index, session->uid);
	return ret;
}

int npu_vertex_prepare(struct file *file, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;

	if (unlikely(!vctx)) {
		npu_err("Failed to get npu_vertex_ctx\n");
		return -EINVAL;
	}

	if (unlikely(!cbundle)) {
		npu_ierr("Failed to get vs4l_container_bundle\n", vctx, session->hids);
		return -EINVAL;
	}

	if (unlikely(!queue)) {
		npu_ierr("Failed to get npu_queue\n", vctx, session->hids);
		return -EINVAL;
	}

	if (unlikely(!lock)) {
		npu_ierr("Failed to get mutex lock\n", vctx, session->hids);
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("invalid state(%X)\n", vctx, session->hids, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_memory_update_sysmmu_status("prepare", session->memory, session);
	if (ret) {
		npu_ierr("fail(%d) in npu_memory_update_sysmmu_status\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	ret = npu_queue_prepare(queue, cbundle);
	if (ret) {
		npu_ierr("npu_queue_prepare is fail(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}
p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_unprepare(struct file *file, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vctx->lock;

	if (unlikely(!vctx)) {
		npu_err("Failed to get npu_vertex_ctx\n");
		return -EINVAL;
	}

	if (unlikely(!cbundle)) {
		npu_ierr("Failed to get vs4l_container_bundle\n", vctx, session->hids);
		return -EINVAL;
	}

	if (unlikely(!queue)) {
		npu_ierr("Failed to get npu_queue\n", vctx, session->hids);
		return -EINVAL;
	}

	if (unlikely(!lock)) {
		npu_ierr("Failed to get mutex lock\n", vctx, session->hids);
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	if (!(vctx->state & BIT(NPU_VERTEX_FORMAT))) {
		npu_ierr("invalid state(%X)\n", vctx, session->hids, vctx->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_queue_unprepare(queue, cbundle);
	if (ret) {
		npu_ierr("npu_queue_unprepare is failed(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}
p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_streamon(struct file *file)
{
	/* No OP */
	return 0;
}

int npu_vertex_streamoff(struct file *file)
{
	/* No OP */
	return 0;
}

static int __npu_vertex_bootup(struct file *file, struct vs4l_ctrl *ctrl)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vertex->lock;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
#if !IS_ENABLED(CONFIG_DSP_USE_VS4L)
	if (ctrl->value == NPU_HWDEV_ID_DSP) {
		npu_ierr("recv err cmd\n", vctx, ctrl->value);
		return -EINVAL;
	}
#endif
	if (ctrl->value != NPU_HWDEV_ID_NPU && ctrl->value != NPU_HWDEV_ID_DSP) {
		npu_ierr("invalid hw device id\n", vctx, ctrl->value);
		return -EINVAL;
	}

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	session->hids = ctrl->value;

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	mutex_lock(&device->sessionmgr.freq_set_lock);
#endif
	ret = npu_hwdev_bootup(device, session, ctrl->value);
	if (ret) {
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
		mutex_unlock(&device->sessionmgr.freq_set_lock);
#endif
		npu_ierr("fail(%d): hids %x\n", vctx, session->hids, ret, ctrl->value);
		goto p_err;
	}
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	mutex_unlock(&device->sessionmgr.freq_set_lock);
#endif

	ret = npu_session_NW_CMD_POWER_NOTIFY(session, true);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_NW_CMD_POWER_NOTIFY\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	vctx->state |= BIT(NPU_VERTEX_POWER);

	npu_scheduler_send_wait_info_to_hw(session,	info);

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_bootup(struct file *file, struct vs4l_ctrl *ctrl)
{
	if (!file->private_data) {
		return -EINVAL;
	}
	return __npu_vertex_bootup(file, ctrl);

}

int npu_vertex_profileon(struct file *file, struct vs4l_profiler *phead)
{
	int ret = 0;

	return ret;
}

int npu_vertex_profileoff(struct file *file, struct vs4l_profiler *phead)
{
	int ret = 0;

	return ret;
}

int npu_vertex_profile_ready(struct file *file, struct vs4l_profile_ready *pread)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct mutex *lock = &vertex->lock;

	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	ret = npu_session_profile_ready(session, pread);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_profile_ready\n", vctx, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_version(struct file *file, struct vs4l_version *version)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct mutex *lock = &vertex->lock;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);

	/* check npu_device emergency error */
	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	npu_ver_info(device, version);

	if ((vctx->state & BIT(NPU_VERTEX_GRAPH)))
		npu_ver_fw_info(device, version);

	mutex_unlock(lock);
	return 0;
}

int npu_vertex_qbuf_cancel(struct file *file, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_queue *queue = &vctx->queue;
	struct mutex *lock = &vertex->lock;

	ret = check_emergency_vctx(vctx);
	if (ret)
		return ret;

	if (mutex_lock_interruptible(lock)) {
		npu_ierr("fail in mutex_lock_interruptible\n", vctx, session->hids);
		return -ERESTARTSYS;
	}

	ret = npu_queue_qbuf_cancel(queue, cbundle);
	if (ret) {
		npu_ierr("npu_queue_qbuf_cancel is fail(%d)\n", vctx, session->hids, ret);
		goto p_err;
	}
	npu_device_unlock(session->skip_qbuf_lock);

p_err:
	mutex_unlock(lock);
	return ret;
}

int npu_vertex_sync(struct file *file, struct vs4l_ctrl *ctrl)
{
	return 0;
}

int npu_vertex_buf_votf(struct file *file, struct vs4l_buf_votf *buf_votf)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	ret = npu_memory_update_sysmmu_status("votf", session->memory, session);
	if (ret) {
		npu_ierr("fail(%d) in npu_memory_update_sysmmu_status\n", vctx, session->hids, ret);
		npu_session_dump(session);
		ret = -EFAULT;
	}

	ret = npu_session_buf_votf(session, buf_votf);
	if (ret < 0) {
		npu_uerr("npu_session_buf_votf is fail(%d).\n", session, session->hids, ret);
		ret = -EFAULT;
	}

	return ret;
}

static int __force_streamoff(struct file *file)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = file->private_data;
	struct npu_vertex *vertex = vctx->vertex;
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	/* check npu_device emergency error */
	struct npu_device *device = container_of(vertex, struct npu_device, vertex);

	ret = npu_session_NW_CMD_STREAMOFF(session);
	if (ret) {
		npu_ierr("fail(%d) in npu_session_NW_CMD_STREAMOFF\n", vctx, session->hids, ret);
		goto p_err;
	}

	if (!npu_device_is_emergency_err(device)) {
		ret = chk_nw_result_no_error(session);
		if (ret == NPU_ERR_NO_ERROR) {
			vctx->state |= BIT(NPU_VERTEX_STREAMOFF);
			vctx->state &= (~BIT(NPU_VERTEX_FORMAT));
		} else
			npu_iwarn("NPU DEVICE IS EMERGENCY ERROR\n", vctx, session->hids);
	} else {
		npu_iinfo("EMERGENCY_RECOVERY - %ums delay insearted instead of STREAMOFF.\n", vctx, session->hids,
			npu_get_configs(STREAMOFF_DELAY_ON_EMERGENCY));
		msleep(npu_get_configs(STREAMOFF_DELAY_ON_EMERGENCY));
	}

	ret = __vref_put(&vertex->start_cnt);
	if (ret) {
		npu_ierr("fail(%d) in vref_put\n", vctx, session->hids, ret);
		goto p_err;
	}
p_err:
	npu_iinfo("(%d)\n", vctx, session->hids, ret);
	return ret;
}
