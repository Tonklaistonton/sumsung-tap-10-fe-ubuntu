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
#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/samsung/samsung-dma-mapping.h>

#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/samsung/samsung-dma-mapping.h>
#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-memdump.h"
#include "npu-queue.h"
#include "npu-vertex.h"
#include "include/npu-config.h"
#include "npu-session.h"
#include "npu-log.h"
#include "npu-hw-device.h"
#include "npu-pm.h"
#include "npu-memory-manual.h"
#include "include/npu-memory.h"

/*
 * queue: npu_queue instance strucure
 * memory: Reference to memory management helper
 * lock: Session-wide mutex to protect access to vb_queue
 */

int npu_queue_region_unmap(struct npu_memory_region *region, struct npu_memory_buffer *buffer);

int npu_queue_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer);

int npu_queue_flush(struct npu_queue *queue, struct npu_queue_list *queue_list);

int npu_queue_memory_map(struct npu_queue_list *queue_list,
			struct npu_memory *memory, struct npu_session *session,
			struct npu_memory_buffer *buffer, size_t size, u32 lazy_unmap, bool is_vmap_req);

#if IS_ENABLED(CONFIG_SOC_S5E9955)
int npu_queue_memory_map_retry(	struct npu_memory *memory, struct npu_session *session,
			struct npu_memory_buffer *buffer, bool is_vmap_req);
#endif

int npu_queue_memory_unmap(struct npu_memory *memory,
				struct npu_session *session, struct npu_memory_buffer *buffer);

int npu_queue_open(struct npu_queue_list *queue)
{
	int ret = 0, i = 0;

	for (i = 0; i < NPU_MAX_QUEUE; i++) {
		init_waitqueue_head(&queue[i].done_wq);
		queue[i].state = 0;
		set_bit(NPU_QUEUE_STATE_DEQUEUED, &queue[i].state);
	}

	return ret;
}

void npu_queue_close(struct npu_queue *queue, struct npu_queue_list *queue_list)
{
	int i = 0, j = 0, k = 0;
	struct nq_container *temp_container;
	struct npu_memory_buffer *temp_buffer;
	struct npu_memory_buffer *temp_buffer_pool;
	struct npu_vertex_ctx *vctx = container_of(queue, struct npu_vertex_ctx, queue);
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	if (!test_bit(NPU_QUEUE_STATE_UNPREPARE, &queue_list->state))
		npu_queue_flush(queue, queue_list);

	for (i = 0; i < NPU_MAX_QUEUE; i++)
		wake_up_all(&queue_list[i].done_wq);

	for (i = 0; i < NPU_MAX_QUEUE; i++) {
		if (!test_bit(NPU_QUEUE_STATE_ALLOC, &queue_list[i].state))
			continue;

		for (j = 0; j < queue_list[i].count; j++) {
			temp_buffer = queue_list[i].containers[j].buffers;
			if (temp_buffer)
				kvfree(temp_buffer);
		}
		temp_container = queue_list[i].containers;
		if (temp_container)
			kfree(temp_container);

		for (k = 0; k < queue_list[i].b_count; k++) {
			temp_buffer_pool = &queue_list[i].buffer_pool[k];
			npu_queue_memory_unmap(session->memory, session, temp_buffer_pool);
		}
		queue_list[i].b_count = 0;
		temp_buffer_pool = queue_list[i].buffer_pool;
		if (temp_buffer_pool)
			kvfree(temp_buffer_pool);
	}

	clear_bit(NPU_QUEUE_STATE_ALLOC, &queue_list->state);
}

int npu_queue_start(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	session->ss_state |= BIT(NPU_SESSION_STATE_START);

	return ret;
}

int npu_queue_stop(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	session->ss_state |= BIT(NPU_SESSION_STATE_STOP);

	return ret;
}

static int fill_vs4l_buffer(struct npu_queue_list *queue_list, struct vs4l_container_list *c, bool fill)
{
	struct nq_container *container;
	u32 i, j, k;
	int ret = 0;

	c->flags &= ~(1 << VS4L_CL_FLAG_TIMESTAMP);
	c->flags &= ~(1 << VS4L_CL_FLAG_PREPARE);
	c->flags &= ~(1 << VS4L_CL_FLAG_INVALID);
	c->flags &= ~(1 << VS4L_CL_FLAG_DONE);
	c->flags &= ~(1 << VS4L_CL_FLAG_FW_TIMEOUT);
	c->flags &= ~(1 << VS4L_CL_FLAG_HW_TIMEOUT_RECOVERED);
	c->flags &= ~(1 << VS4L_CL_FLAG_HW_TIMEOUT_NOTRECOVERABLE);
	c->flags &= ~(1 << VS4L_CL_FLAG_CANCEL_DD);

	if (test_bit(NPU_QUEUE_STATE_PREPARE, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_PREPARE);

	if (test_bit(VS4L_CL_FLAG_INVALID, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_INVALID);

	if (test_bit(VS4L_CL_FLAG_DONE, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_DONE);

	if (test_bit(VS4L_CL_FLAG_FW_TIMEOUT, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_FW_TIMEOUT);

	if (test_bit(VS4L_CL_FLAG_HW_TIMEOUT_RECOVERED, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_HW_TIMEOUT_RECOVERED);

	if (test_bit(VS4L_CL_FLAG_HW_TIMEOUT_NOTRECOVERABLE, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_HW_TIMEOUT_NOTRECOVERABLE);

	if (test_bit(VS4L_CL_FLAG_CANCEL_DD, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_CANCEL_DD);

	if (!fill)
		return ret;

	if (queue_list->count != c->count) {
		ret = -EINVAL;
		npu_err("container_list count should not vary than queue_list count\n");
		return ret;
	}

	/* sync buffers */
	for (i = 0; i < queue_list->count; ++i) {
		container = &queue_list->containers[i];
		k = container->count;
		if (k != c->containers[i].count) {
			ret = -EINVAL;
			npu_err("container_list count should not vary than queue_list count\n");
			return ret;
		}

		for (j = 0; j < k; ++j)
			c->containers[i].buffers[j].reserved = container->buffers[j].usr.reserved;
	}

	return ret;
}

static void dma_buf_sync(struct npu_memory_buffer *buffers, u32 direction, u32 action)
{
	unsigned long flags;

	if (is_kpi_mode_enabled(true))
		local_irq_save(flags);

	if (action == NPU_FRAME_QBUF)
		dma_buf_end_cpu_access(buffers->dma.dma_buf, direction);
	else // action == VISION_DQBUF
		dma_buf_begin_cpu_access(buffers->dma.dma_buf, direction);

	if (is_kpi_mode_enabled(true))
		local_irq_restore(flags);
}

void npu_queue_buf_sync(struct npu_queue_list *queue_list, u32 action)
{
	int i = 0, j = 0, k = 0, direction = 0;
	struct nq_container	*container;

	if (queue_list->direction == VS4L_DIRECTION_OT)
		direction = DMA_FROM_DEVICE;
	else
		direction = DMA_TO_DEVICE;

	/* sync buffers */
	for (i = 0; i < queue_list->count; ++i) {
		container = &queue_list->containers[i];
		k = container->count;
		for (j = 0; j < k; ++j) {
			dma_buf_sync(&(container->buffers[j]), direction, action);
		}
	}
}

int npu_queue_region_unmap(struct npu_memory_region *region, struct npu_memory_buffer *buffer)
{
	int ret = 0;

	if (!buffer) {
		npu_err("vb_buffer(buffer) is NULL\n");
		ret = -EFAULT;
		return ret;
	}

	if (!buffer->manual_dup && !IS_ERR_OR_NULL(buffer->dma.dma_buf))
		npu_memory_region_buffer_hash_del(region, buffer->dma.dma_buf);

	__npu_memory_region_unmap(region, buffer, true);

	buffer->usr.fd = 0;
	buffer->dma.size = 0;
	return ret;
}

int npu_queue_region_unmapping(struct npu_memory_region *region, size_t	*size, struct npu_queue_list *queue_list)
{
	int ret = 0, i = 0, j = 0;
	struct nq_container	*q_container;
	struct npu_memory_buffer		*q_buffer_pool;
	struct npu_memory_buffer		*q_buffer;

	if (!test_bit(NPU_QUEUE_STATE_PREPARE, &queue_list->state))
		return ret;

	for (j = 0; j < queue_list->b_count; ++j) {
		q_buffer_pool = &queue_list->buffer_pool[j];
		ret = npu_queue_region_unmap(region, q_buffer_pool);
		if (ret) {
			npu_err("__vb_qbuf_dmabuf is fail(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < queue_list->count; i++) {
		q_container = &queue_list->containers[i];
		for (j = 0; j < q_container->count; ++j) {
			q_buffer = &q_container->buffers[j];
			q_buffer->usr.fd = 0;
		}
	}
	queue_list->b_count = 0;

	return ret;
}

static dma_addr_t npu_dma_buf_dva_map(struct npu_memory_buffer *buffer)
{
	return sg_dma_address(buffer->dma.sgt->sgl);
}

static void npu_dma_buf_dva_unmap(struct npu_memory_buffer *buffer, bool mapping)
{
	struct iommu_domain *domain = NULL;
	size_t unmapped = 0;
	struct device *dev = buffer->dma.attachment->dev;

	if (!mapping)
		return;

	if (buffer->imb_for_iofm)
		dev = buffer->mapped_dev;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		probe_warn("%s(%d) iommu_unmap failed\n", __func__, __LINE__);
		return;
	}

	unmapped = iommu_unmap(domain, buffer->dma.daddr, buffer->dma.dma_buf->size);
	if (!unmapped)
		probe_warn("%s(%d) iommu_unmap failed\n", __func__, __LINE__);
	if (unmapped != buffer->dma.dma_buf->size)
		probe_warn("iomuu_unmap unmapped size(%lu) is not buffer size(%lu)\n",
				unmapped, buffer->dma.dma_buf->size);
}

int npu_queue_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	int ret = 0;

	if (buffer == NULL) {
		npu_err("vb_buffer(buffer) is NULL\n");
		ret = -EFAULT;
		goto p_err;
	}

	npu_info("Trying queue unmap fd=%d\n", buffer->usr.fd);
	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
			buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment);

	lock_imb(memory->system, 0);
	if (buffer->imb_for_iofm)
		return_imb_space(memory->system, 0);
	if (buffer->dma.daddr)
		npu_dma_buf_dva_unmap(buffer, buffer->imb_for_iofm);
	unlock_imb(memory->system, 0);
	if (buffer->dma.vaddr)
		npu_dma_buf_vunmap(buffer->dma.dma_buf, buffer->dma.vaddr);
	if (buffer->dma.sgt)
		dma_buf_unmap_attachment_unlocked(
			buffer->dma.attachment, buffer->dma.sgt, DMA_BIDIRECTIONAL);
	if (buffer->dma.attachment)
		dma_buf_detach(buffer->dma.dma_buf, buffer->dma.attachment);
	if (buffer->dma.dma_buf) {
		dma_buf_put(buffer->dma.dma_buf);
		npu_memory_update_dbg_info(memory, buffer->dma.dma_buf);
	}

	buffer->dma.attachment = NULL;
	buffer->dma.dma_buf = NULL;
	buffer->dma.sgt = NULL;
	buffer->dma.daddr = 0;
	buffer->dma.vaddr = NULL;
	buffer->dma.size = 0;
	buffer->npu_sgt = false;
	buffer->imb_for_ncp = false;
	buffer->imb_for_iofm = false;

	buffer->usr.fd = 0;

p_err:
	return ret;
}

int npu_queue_unmapping(struct npu_memory *memory, struct npu_session *session,
					size_t *size, struct npu_queue_list *queue_list)
{
	int ret = 0, i = 0, j = 0;
	struct nq_container	*q_container;
	struct npu_memory_buffer		*q_buffer_pool;
	struct npu_memory_buffer		*q_buffer;

	if (!test_bit(NPU_QUEUE_STATE_PREPARE, &queue_list->state))
		return ret;

	for (j = 0; j < queue_list->b_count; ++j) {
		q_buffer_pool = &queue_list->buffer_pool[j];
		ret = npu_queue_memory_unmap(memory, session, q_buffer_pool);
		if (ret) {
			npu_err("__vb_qbuf_dmabuf is fail(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < queue_list->count; i++) {
		q_container = &queue_list->containers[i];
		for (j = 0; j < q_container->count; ++j) {
			q_buffer = &q_container->buffers[j];
			q_buffer->usr.fd = 0;
		}
	}
	queue_list->b_count = 0;

	return ret;
}

int npu_queue_region_map(struct npu_queue_list *queue_list, struct npu_session *session,
	struct npu_memory_region *region, struct npu_memory_buffer *buffer, size_t size, u32 lazy_unmap, bool is_vmap_req)
{
	int ret = 0;

	struct dma_buf *dma_buf;

	buffer->dma.dma_buf = NULL;
	buffer->dma.attachment = NULL;
	buffer->dma.sgt = NULL;
	buffer->dma.daddr = 0;
	buffer->dma.vaddr = NULL;
	buffer->npu_sgt = false;
	buffer->imb_for_ncp = false;
	buffer->imb_for_iofm = false;
	buffer->pid = session->pid;
	buffer->imb_dup = false;
	buffer->manual_dup = false;
	INIT_LIST_HEAD(&buffer->list);
	INIT_LIST_HEAD(&buffer->imb_map_list);

	if (npu_memory_region_check_used_buffer(region, buffer))
		goto exist_buffer;

	dma_buf = npu_memory_get_dma_buf(buffer->usr.fd, 0, NULL);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is fail(%p)\n", dma_buf);
		ret = -EINVAL;
		return ret;
	}
	buffer->dma.dma_buf = dma_buf;

	ret = __npu_memory_region_map(region, buffer, lazy_unmap, false, is_vmap_req,
		queue_list->direction == VS4L_DIRECTION_OT ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
	if (ret) {
		npu_queue_region_unmap(region, buffer);
		return ret;
	}

	npu_memory_region_buffer_hash_add(region, buffer);
exist_buffer:
	npu_memory_region_alloc_dbg_info(region, buffer->dma.dma_buf, buffer->dma.size);
	return ret;
}


int npu_queue_map(struct npu_queue_list *queue_list, struct npu_memory *memory, struct npu_memory_buffer *buffer, size_t size, u32 lazy_unmap, bool is_vmap_req)
{
	int ret = 0;
	bool complete_suc = false;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t base_daddr;
	void *vaddr;
	u32 direction;

	buffer->dma.dma_buf = NULL;
	buffer->dma.attachment = NULL;
	buffer->dma.sgt = NULL;
	buffer->dma.daddr = 0;
	buffer->dma.vaddr = NULL;
	buffer->npu_sgt = false;
	buffer->imb_for_ncp = false;
	buffer->imb_for_iofm = false;

	npu_info("Try mapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);

	dma_buf = dma_buf_get(buffer->usr.fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is fail(%p)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma.dma_buf = dma_buf;

	if (buffer->dma.dma_buf->size < size) {
		npu_err("Allocate buffer size(%zu) is smaller than expectation(%zu)\n",
			buffer->dma.dma_buf->size, size);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma.size = size;

	attachment = dma_buf_attach(buffer->dma.dma_buf, memory->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		goto p_err;
	}
	buffer->dma.attachment = attachment;

	npu_dma_buf_add_attr(attachment);

	if (queue_list->direction == VS4L_DIRECTION_OT)
		direction = DMA_FROM_DEVICE;
	else
		direction = DMA_TO_DEVICE;

	if (lazy_unmap == NPU_SESSION_LAZY_UNMAP_DISABLE)
		buffer->dma.attachment->dma_map_attrs |= (DMA_ATTR_SKIP_LAZY_UNMAP);

	sgt = dma_buf_map_attachment_unlocked(attachment, direction);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto p_err;
	}
	buffer->dma.sgt = sgt;

	base_daddr = npu_dma_buf_dva_map(buffer);
	if (IS_ERR_VALUE(base_daddr)) {
		npu_err("Failed to allocate iova (err 0x%pK)\n", &base_daddr);
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->dma.daddr = base_daddr;

	/* Only DSP requires vaddr, hence condition check is added */
	if (is_vmap_req) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR(vaddr)) {
			npu_err("Failed to get vaddr (err 0x%pK)\n", &vaddr);
			ret = -EFAULT;
			goto p_err;
		}
		buffer->dma.vaddr = vaddr;
	}

	npu_dbg("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment, dma_buf->size);

	complete_suc = true;
	npu_memory_alloc_dbg_info(memory, buffer->dma.dma_buf, buffer->dma.size);

p_err:
	if (complete_suc != true)
		npu_queue_unmap(memory, buffer);
	return ret;
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
static struct sg_table *get_unmapped_iommu_sgt(struct npu_memory_buffer *buffer, int prot)
{
	int ret;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;

	attachment = dma_buf_attach(buffer->dma.dma_buf, get_nshare_manual_dev());
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		npu_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->dma.attachment = attachment;

	if (prot)
		npu_dma_buf_add_attr(attachment);

	buffer->dma.attachment->dma_map_attrs |= (DMA_ATTR_SKIP_LAZY_UNMAP);

	sgt = dma_buf_map_attachment_unlocked(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		npu_err("dma_buf_map_attachment is fail(%d) size(%zu)\n",
			ret, buffer->dma.size);
		goto p_err;
	}
	buffer->dma.sgt = sgt;
	return sgt;
p_err:
	return NULL;
}

int npu_queue_region_map_retry(struct npu_memory_region *region,
	struct npu_memory_buffer *buffer, int prot, bool is_vmap_req)
{
	int ret = 0;
	bool complete_suc = false;
	size_t retry_size = 0;
	size_t temp_size = 0;
	dma_addr_t daddr = 0;

	struct dma_buf *dma_buf;
	void *vaddr;
	struct device *dev;

	if (unlikely(!region)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	npu_info("Retry mapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);

	buffer->dma.dma_buf = NULL;
	buffer->dma.attachment = NULL;
	buffer->dma.sgt = NULL;
	buffer->dma.daddr = 0;
	buffer->dma.vaddr = NULL;
	buffer->npu_sgt = false;
	buffer->imb_for_ncp = false;
	buffer->imb_for_iofm = false;

	dma_buf = dma_buf_get(buffer->usr.fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is failed(%pK)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma.dma_buf = dma_buf;

	if (buffer->dma.dma_buf->size < buffer->dma.size) {
		npu_err("Allocate buffer size(%zu) is smaller than expectation(%u)\n",
			buffer->dma.dma_buf->size, (unsigned int)buffer->dma.size);
		ret = -EINVAL;
		goto p_err;
	}

	temp_size = buffer->dma.dma_buf->size;
	buffer->dma.size = temp_size;

	retry_size = ALIGN(buffer->dma.size, NPU_IMB_GRANULARITY);
	npu_info("retry size must align 0x%x. (full size : 0x%lx, aligned size : 0x%lx)\n",
			NPU_IMB_GRANULARITY, buffer->dma.size, retry_size);
	if (rent_imb_space(region->system, &daddr, retry_size, 0)) {
		npu_err("There are no space on IMB for another buffer\n");
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->imb_for_iofm = true;

	if (!get_unmapped_iommu_sgt(buffer, prot)) {
		npu_err("get_unmapped_iommu_sgt is failed(%pK)\n", dma_buf);
		ret = -ENOMEM;
		goto p_err;
	}

	dev = region->dev;
	{
		unsigned long l_flags;

		ret = dma_buf_get_flags(dma_buf, &l_flags);
		if (ret) {
			npu_err("dma_buf_get_flags is failed\n");
			ret = -EINVAL;
			goto p_err;
		}

		if (npu_memory_dma_heap_flags_uncached(l_flags)) {
			npu_info("This DMABUF is non-cacheable\n");
			dev = get_non_share_dev();
		}
	}
	buffer->mapped_dev = dev;

	{
		size_t map_size = 0;
		int iommu_attributes;
		struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

		if (prot)
			iommu_attributes = ((prot) << IOMMU_PRIV_SHIFT);

		if (likely(domain)) {
			map_size = iommu_map_sgtable(domain, daddr, buffer->dma.sgt, iommu_attributes);
		} else {
			probe_err("fail(err %pad) in iommu_map_sg\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}

		npu_sgt_flush(buffer->dma.sgt, dev);

		buffer->dma.daddr = daddr;

		if (map_size != buffer->dma.size) {
			probe_info("iommu_map_sgtable return %lu, we want to map %lu",
				map_size, buffer->dma.size);
		}
	}

	if (is_vmap_req) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR(vaddr)) {
			npu_err("Failed to get vaddr (err %pK)\n", vaddr);
			ret = -EFAULT;
			goto p_err;
		}
		buffer->dma.vaddr = vaddr;
	}

	npu_dbg("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment, buffer->dma.size);

	complete_suc = true;
p_err:
	if (complete_suc != true) {
		npu_queue_region_unmap(region, buffer);
	}
	return ret;
}
#endif

static void npu_queue_region_replacement(struct npu_memory_buffer *src, struct npu_memory_buffer *dest,
	struct npu_memory_region* region, struct vs4l_buffer *buffer)
{
	memcpy((void *)dest, (void *)src, sizeof(*src));
	dest->usr.offset = src->usr.offset = buffer->roi.offset;
	npu_memory_buffer_replace(region, src, dest);
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
int npu_queue_map_retry(struct npu_memory *memory,
		struct npu_memory_buffer *buffer, int prot, bool is_vmap_req)
{
	int ret = 0;
	bool complete_suc = false;
	size_t retry_size = 0;
	size_t temp_size = 0;
	dma_addr_t daddr = 0;

	struct dma_buf *dma_buf;
	void *vaddr;
	struct device *dev;

	if (unlikely(!memory)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	npu_info("Retry mapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);

	buffer->dma.dma_buf = NULL;
	buffer->dma.attachment = NULL;
	buffer->dma.sgt = NULL;
	buffer->dma.daddr = 0;
	buffer->dma.vaddr = NULL;
	buffer->npu_sgt = false;
	buffer->imb_for_ncp = false;
	buffer->imb_for_iofm = false;

	dma_buf = dma_buf_get(buffer->usr.fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is failed(%pK)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma.dma_buf = dma_buf;

	if (buffer->dma.dma_buf->size < buffer->dma.size) {
		npu_err("Allocate buffer size(%zu) is smaller than expectation(%u)\n",
			buffer->dma.dma_buf->size, (unsigned int)buffer->dma.size);
		ret = -EINVAL;
		goto p_err;
	}

	temp_size = buffer->dma.dma_buf->size;
	buffer->dma.size = temp_size;

	retry_size = ALIGN(buffer->dma.size, NPU_IMB_GRANULARITY);
	npu_info("retry size must align 0x%x. (full size : 0x%lx, aligned size : 0x%lx)\n",
			NPU_IMB_GRANULARITY, buffer->dma.size, retry_size);
	if (rent_imb_space(memory->system, &daddr, retry_size, 0)) {
		npu_err("There are no space on IMB for another buffer\n");
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->imb_for_iofm = true;

	if (!get_unmapped_iommu_sgt(buffer, prot)) {
		npu_err("get_unmapped_iommu_sgt is failed(%pK)\n", dma_buf);
		ret = -ENOMEM;
		goto p_err;
	}

	dev = memory->dev;
	{
		unsigned long l_flags;

		ret = dma_buf_get_flags(dma_buf, &l_flags);
		if (ret) {
			npu_err("dma_buf_get_flags is failed\n");
			ret = -EINVAL;
			goto p_err;
		}

		if (npu_memory_dma_heap_flags_uncached(l_flags)) {
			npu_info("This DMABUF is non-cacheable\n");
			dev = get_non_share_dev();
		}
	}
	buffer->mapped_dev = dev;

	{
		size_t map_size = 0;
		int iommu_attributes;
		struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

		if (prot)
			iommu_attributes = ((prot) << IOMMU_PRIV_SHIFT);

		if (likely(domain)) {
			map_size = iommu_map_sgtable(domain, daddr, buffer->dma.sgt, iommu_attributes);
		} else {
			probe_err("fail(err %pad) in iommu_map_sg\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}

		npu_sgt_flush(buffer->dma.sgt, dev);

		buffer->dma.daddr = daddr;

		if (map_size != buffer->dma.size) {
			probe_info("iommu_map_sgtable return %lu, we want to map %lu",
				map_size, buffer->dma.size);
		}
	}

	if (is_vmap_req) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR(vaddr)) {
			npu_err("Failed to get vaddr (err %pK)\n", vaddr);
			ret = -EFAULT;
			goto p_err;
		}
		buffer->dma.vaddr = vaddr;
	}

	npu_dbg("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment, buffer->dma.size);

	complete_suc = true;
p_err:
	if (complete_suc != true) {
		npu_queue_unmap(memory, buffer);
	}
	return ret;
}
#endif

static void npu_queue_replacement(struct npu_memory_buffer *src, struct npu_memory_buffer *dest, struct vs4l_buffer *buffer)
{
	dest->usr.fd = src->usr.fd;
	dest->dma.dma_buf = src->dma.dma_buf;
	dest->dma.attachment = src->dma.attachment;
	dest->dma.sgt = src->dma.sgt;
	dest->dma.vaddr = src->dma.vaddr;
	dest->dma.daddr = src->dma.daddr;
	dest->dma.size = src->dma.size;
	dest->usr.offset = src->usr.offset = buffer->roi.offset;
	dest->npu_sgt = src->npu_sgt;
	dest->imb_for_iofm = src->imb_for_iofm;
	dest->imb_for_ncp = src->imb_for_ncp;
	dest->mapped_dev = src->mapped_dev;
}

static void npu_queue_memory_replacement(struct npu_session *session, struct npu_memory_buffer *src, struct npu_memory_buffer *dest, struct vs4l_buffer *buffer) {
	if (session->need_sysmmu2) {
		struct npu_memory_region *region = npu_memory_region_get_dva(session->manual);
		npu_queue_region_replacement(src, dest, region, buffer);
	} else { // !session->need_sysmmu2
		npu_queue_replacement(src, dest, buffer);
	}
}

int npu_queue_mapping(struct npu_session *session, size_t *size, struct npu_queue_list *queue_list, struct vs4l_container_list *c, bool is_vmap_req)
{
	int ret = 0, i = 0, j = 0, k = 0, flag = 0;
	struct vs4l_container *container;
	struct vs4l_buffer *buffer;
	struct nq_container *q_container;
	struct npu_memory_buffer *q_buffer;
	struct npu_memory *memory;

	if (unlikely(!session)) {
		npu_err("Failed to get session\n");
		return -EINVAL;
	}

	memory = session->memory;

	if (unlikely(!memory)) {
		npu_uerr("Failed to get memory\n", session, session->hids);
		return -EINVAL;
	}

	queue_list->id = c->id;
	queue_list->index = c->index;
	queue_list->count = c->count;
	queue_list->flags = c->flags;
	queue_list->direction = c->direction;

	for (i = 0; i < c->count; ++i) {
		container = &c->containers[i];
		q_container = &queue_list->containers[i];
		q_container->count = container->count;
		switch (container->memory) {
		case VS4L_MEMORY_DMABUF:
			for (j = 0; j < container->count; ++j) {
				buffer = &container->buffers[j];
				q_buffer = &q_container->buffers[j];

				if (q_buffer->usr.fd != buffer->m.fd) {
					for (k = 0; k < queue_list->b_count; k++) {
						if (buffer->m.fd == queue_list->buffer_pool[k].usr.fd) {
							flag = 1;
							break;
						}
					}

					if (flag) {
						npu_queue_memory_replacement(session, &queue_list->buffer_pool[k], q_buffer, buffer);
					} else {
						if (queue_list->b_count > NPU_FRAME_MAX_CONTAINERLIST)
							return -ENOMEM;

						q_buffer->usr.fd = buffer->m.fd;
						q_buffer->dma.size = size[i];
						q_buffer->usr.offset = buffer->roi.offset;

						ret = npu_queue_memory_map(queue_list, memory, session, q_buffer,
									size[i], session->lazy_unmap_disable, is_vmap_req);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
						if (ret == -ENOMEM) {
							q_buffer->usr.fd = buffer->m.fd;
							q_buffer->dma.size = size[i];
							q_buffer->usr.offset = buffer->roi.offset;

							npu_uinfo("Retry ifm/ofm\n", session, session->hids);
							ret = npu_queue_memory_map_retry(memory, session, q_buffer, is_vmap_req);
#endif
							if (unlikely(ret)) {
								npu_err("__vb_qbuf_dmabuf is fail(%d), size(%zu)\n", ret, size[i]);
								return ret;
							}
#if IS_ENABLED(CONFIG_SOC_S5E9955)
						}
#endif
						npu_queue_memory_replacement(session, q_buffer, &queue_list->buffer_pool[k], buffer);
						queue_list->b_count++;
					}
				}
			}
			break;
		default:
			buffer = &container->buffers[j];
			npu_err("unsupported container memory type i %d, memory %d, fd %d\n",
					i, container->memory, buffer->m.fd);
			ret = -EINVAL;
			return ret;
		}
	}

	return ret;
}

int npu_queue_memory_map(struct npu_queue_list *queue_list,
			struct npu_memory *memory, struct npu_session *session,
			struct npu_memory_buffer *buffer, size_t size, u32 lazy_unmap, bool is_vmap_req)
{
	int ret = 0;

	if (session->need_sysmmu2) {
		struct npu_memory_region *region = npu_memory_region_get_dva(session->manual);
		ret = npu_queue_region_map(queue_list, session, region, buffer, size, lazy_unmap, is_vmap_req);
	} else { // !session->need_sysmmu2
		ret = npu_queue_map(queue_list, memory, buffer, size, lazy_unmap, is_vmap_req);
	}

	return ret;
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
int npu_queue_memory_map_retry(	struct npu_memory *memory, struct npu_session *session,
			struct npu_memory_buffer *buffer, bool is_vmap_req)
{
	int ret = 0;

	if (session->need_sysmmu2) {
		npu_err("not support retry for sysmmu2\n");
		ret = -ENOMEM;
	} else {
		ret = npu_queue_map_retry(memory, buffer, false, is_vmap_req);
	}

	return ret;
}
#endif

int npu_queue_memory_unmap(struct npu_memory *memory,
				struct npu_session *session, struct npu_memory_buffer *buffer)
{
	int ret = 0;
	if (session->need_sysmmu2) {
		struct npu_memory_region *region = npu_memory_region_get_dva(session->manual);
		ret = npu_queue_region_unmap(region, buffer);
	} else { // !session->need_sysmmu2
		ret = npu_queue_unmap(memory, buffer);
	}

	return ret;
}
int npu_queue_check(unsigned long state, struct vs4l_container_list *c)
{
	int ret = 0;
	int i;

	if (c->index >= NPU_FRAME_MAX_CONTAINERLIST) {
		npu_err("invalid container list index\n");
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < c->count; i++) {
		if (c->containers[i].count > NPU_FRAME_MAX_BUFFER) {
			npu_err("Max buffers are %d; passed %d buffers\n",
				NPU_FRAME_MAX_BUFFER, c->containers[i].count);
			ret = -EINVAL;
			goto p_err;
		}
	}

	if (!test_bit(NPU_QUEUE_STATE_DEQUEUED, &state)) {
		npu_err("buffer already in use\n");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static int npu_queue_update(struct npu_queue_list *queue_list, struct vs4l_container_list *c)
{
	int ret = 0, i = 0, j = 0;
	struct vs4l_container *container;
	struct vs4l_buffer *buffer;
	struct nq_container *q_container;
	struct npu_memory_buffer *q_buffer;

	queue_list->id = c->id;
	queue_list->flags = c->flags;

	if (queue_list->count != c->count) {
		ret = -EINVAL;
		npu_err("container_list count should not vary than queue_list count\n");
		return ret;
	}

	for (i = 0; i < c->count; ++i) {
		container = &c->containers[i];
		q_container = &queue_list->containers[i];
		q_container->count = container->count;
		switch (container->memory) {
		case VS4L_MEMORY_DMABUF:
			for (j = 0; j < container->count; ++j) {
				buffer = &container->buffers[j];
				q_buffer = &q_container->buffers[j];

				if (q_buffer->usr.offset != buffer->roi.offset)
					q_buffer->usr.offset = buffer->roi.offset;
			}
			break;
		default:
			buffer = &container->buffers[j];
			npu_err("unsupported container memory type i %d, memory %d, fd %d\n",
					i, container->memory, buffer->m.fd);
			ret = -EINVAL;
			return ret;
		}
	}

	return ret;
}

int npu_queue_qbuf_cancel(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;
#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	struct npu_vertex_ctx *vctx = container_of(queue, struct npu_vertex_ctx, queue);
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	npu_pm_wake_lock(session);
#endif
	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];

	if (test_bit(VS4L_CL_FLAG_CANCEL_DD, &inqueue->state) || test_bit(VS4L_CL_FLAG_CANCEL_DD, &otqueue->state)
				|| !test_bit(NPU_QUEUE_STATE_QUEUED, &inqueue->state))  {
		set_bit(VS4L_CL_FLAG_CANCEL_DD, &inqueue->flags);
		set_bit(VS4L_CL_FLAG_CANCEL_DD, &otqueue->flags);
		goto p_err;
	}

	if ((inqueue->id != cbundle->m[0].clist->id) || (otqueue->id != cbundle->m[1].clist->id)) {
		npu_err("inqueue or outqueue id is not matched\n");
		goto p_err;
	}

	ret = npu_session_queue_cancel(queue, inqueue, otqueue);
	if (ret) {
		npu_err("npu_session_queue_cancel is fail(%d)\n", ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &otqueue->state);

	set_bit(VS4L_CL_FLAG_DONE, &inqueue->flags);
	set_bit(VS4L_CL_FLAG_DONE, &otqueue->flags);

p_err:
	ret = fill_vs4l_buffer(inqueue, cbundle->m[0].clist, true);
	if (ret)
		npu_err("fails to fill vs4l buffer for output");

	ret = fill_vs4l_buffer(otqueue, cbundle->m[1].clist, true);
	if (ret)
		npu_err("fails to fill vs4l buffer for output");
#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	npu_pm_wake_unlock(session);
#endif
	return ret;
}

int npu_queue_qbuf(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = container_of(queue, struct npu_vertex_ctx, queue);
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;

#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	npu_pm_wake_lock(session);
#endif

	npu_profile_record_start(PROFILE_DD_QUEUE, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];
	if (unlikely(!inqueue)) {
		npu_uerr("Failed to get inqueue\n", session, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!otqueue)) {
		npu_uerr("Failed to get otqueue\n", session, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state)) {
		if (test_bit(NPU_QUEUE_STATE_DEQUEUED, &inqueue->state)) {
			clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state);
			clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &otqueue->state);
		} else {
			return -EAGAIN;
		}
	}

	if (!test_bit(NPU_QUEUE_STATE_PREPARE, &inqueue->state)) {
		ret = npu_queue_prepare(queue, cbundle);
		if (ret) {
			npu_uerr("vpu_queue_qbuf is fail(%d)\n", session, session->hids, ret);
			goto p_err;
		}
	}

	ret = npu_queue_update(inqueue, cbundle->m[0].clist);
	if (ret) {
		npu_uerr("npu_queue_update(in) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	ret = npu_queue_update(otqueue, cbundle->m[1].clist);
	if (ret) {
		npu_uerr("npu_queue_update(out) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	npu_queue_buf_sync(inqueue, NPU_FRAME_QBUF);
	npu_queue_buf_sync(otqueue, NPU_FRAME_QBUF);

	ret = npu_session_update_iofm(queue, inqueue);
	if (ret) {
		npu_uerr("npu_session_update_iofm(in) is fail(%d)\n", session, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	ret = npu_session_update_iofm(queue, otqueue);
	if (ret) {
		npu_uerr("npu_session_update_iofm(out) is fail(%d)\n", session, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}
	npu_profile_record_end(PROFILE_DD_QUEUE, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);

	npu_profile_record_start(PROFILE_DD_SESSION, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);
	ret = npu_session_queue(queue, inqueue, otqueue);
	if (ret) {
		npu_uerr("fail(%d) in npu_session_queue\n", session, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_QUEUED, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_QUEUED, &otqueue->state);

p_err:
#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	npu_pm_wake_unlock(session);
#endif
	return ret;
}

int npu_queue_alloc(struct npu_queue_list *queue_list, struct vs4l_container_list *c)
{
	int ret = 0, i = 0;
	struct nq_container *temp_container;
	struct npu_memory_buffer *temp_buffer;
	struct npu_memory_buffer *temp_buffer_pool;

	if (test_bit(NPU_QUEUE_STATE_ALLOC, &queue_list->state)) {
		npu_info("inqueue/otqueue is already in alloc state\n");
		return ret;
	}

	temp_container = kzalloc(c->count * sizeof(struct nq_container), GFP_KERNEL);
	if (temp_container == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	queue_list->containers = temp_container;
	for (i = 0; i < c->count; i++) {
		temp_buffer = kvmalloc(c->containers[i].count * sizeof(struct npu_memory_buffer), GFP_KERNEL | __GFP_ZERO);
		if (temp_buffer == NULL) {
			ret = -ENOMEM;
			return ret;
		}
		queue_list->containers[i].buffers = temp_buffer;
	}

	temp_buffer_pool = kvmalloc(NPU_FRAME_MAX_CONTAINERLIST * sizeof(struct npu_memory_buffer), GFP_KERNEL | __GFP_ZERO);
	if (temp_buffer_pool == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	queue_list->buffer_pool = temp_buffer_pool;
	queue_list->b_count = 0;

	return ret;
}

int npu_queue_prepare(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;
	bool is_vmap_req = true;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	if (unlikely(!session)) {
		npu_err("Failed to get session\n");
		ret = -EINVAL;
		goto p_err;
	}

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];
	if (unlikely(!inqueue)) {
		npu_uerr("Failed to get inqueue\n", session, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!otqueue)) {
		npu_uerr("Failed to get otqueue\n", session, session->hids);
		ret = -EINVAL;
		goto p_err;
	}

	inqueue->flags = cbundle->m[0].clist->flags;
	otqueue->flags = cbundle->m[1].clist->flags;

	if (!test_bit(NPU_QUEUE_STATE_DEQUEUED, &inqueue->state)) {
		npu_uerr("invalid in invb state(%lu)\n", session, session->hids, inqueue->state);
		return -EAGAIN;
	}

	if (!test_bit(NPU_QUEUE_STATE_DEQUEUED, &otqueue->state)) {
		npu_uerr("invalid in otvb state(%lu)\n", session, session->hids, otqueue->state);
		return -EAGAIN;
	}

	/* vaddr is not used for NPU, but DSP uses vaddr */
	is_vmap_req = ((session->hids & NPU_HWDEV_ID_DSP) ? true : false);

	ret = npu_queue_check(inqueue->state, cbundle->m[0].clist);
	if (ret) {
		npu_uerr("npu_queue_check(in) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	ret = npu_queue_check(otqueue->state, cbundle->m[1].clist);
	if (ret) {
		npu_uerr("npu_queue_check(out) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	ret = npu_queue_alloc(inqueue, cbundle->m[0].clist);
	if (ret) {
		npu_uerr("npu_queue_alloc(in) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	ret = npu_queue_alloc(otqueue, cbundle->m[1].clist);
	if (ret) {
		npu_uerr("npu_queue_alloc(out) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_ALLOC, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_ALLOC, &otqueue->state);

	ret = npu_queue_mapping(session, queue->insize, inqueue, cbundle->m[0].clist, is_vmap_req);
	if (ret) {
		npu_uerr("npu_queue_mapping(in) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	ret = npu_queue_mapping(session, queue->otsize, otqueue, cbundle->m[1].clist, is_vmap_req);
	if (ret) {
		npu_uerr("npu_queue_mapping(out) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	ret = npu_session_prepare(queue, inqueue);
	if (ret) {
		npu_uerr("npu_session_prepare(in) is fail(%d)\n", session, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}
	ret = npu_session_prepare(queue, otqueue);
	if (ret) {
		npu_uerr("npu_session_prepare(out) is fail(%d)\n", session, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_PREPARE, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_PREPARE, &otqueue->state);

p_err:
	return ret;
}

int npu_queue_flush(struct npu_queue *queue, struct npu_queue_list *queue_list)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	ret = npu_queue_unmapping(session->memory, session, queue->insize, queue_list);
	if (ret) {
		npu_uerr("npu_queue_unmapping(in) is fail(%d)\n", session, session->hids, ret);
		goto p_err;
	}

	ret = npu_session_unprepare(queue, queue_list);
	if (ret) {
		npu_uerr("npu_session_unprepare(out) is fail(%d)\n", session, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

p_err:
	return ret;
}

int npu_queue_unprepare(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];
	if (unlikely(!inqueue)) {
		npu_err("Failed to get inqueue\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!otqueue)) {
		npu_err("Failed to get otqueue\n");
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_queue_flush(queue, inqueue);
	if (ret) {
		npu_err("npu_queue_flush(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_queue_flush(queue, otqueue);
	if (ret) {
		npu_err("npu_queue_flush(out) is fail(%d)\n", ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_UNPREPARE, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_UNPREPARE, &otqueue->state);
	clear_bit(NPU_QUEUE_STATE_PREPARE, &inqueue->state);
	clear_bit(NPU_QUEUE_STATE_PREPARE, &otqueue->state);

p_err:
	return ret;
}

bool check_done_for_me(struct npu_queue_list *queue)
{
	if (!test_bit(NPU_QUEUE_STATE_DONE, &queue->state))
		return false;

	return true;
}

int npu_queue_wait_for_done(struct npu_session *session, struct npu_queue_list *queue_list, int nonblocking)
{
	int ret = 0;
	u32 timeout = 10000;
	struct npu_vertex *vertex;
	struct npu_device *device;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);

	if (nonblocking) {
		npu_uinfo("Nonblocking and no buffers to dequeue, will not wait\n", session, session->hids);
		ret = -EWOULDBLOCK;
		goto p_err;
	}

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	timeout = 60000;
#endif

	if (wait_event_timeout(queue_list->done_wq, check_done_for_me(queue_list), msecs_to_jiffies(timeout)) == 0) {
		npu_uerr("timeout executed! [%u secs], index = %d, fid = %d\n", session, session->hids,
								timeout, queue_list->index, queue_list->id);
		npu_util_dump_handle_nrespone(&device->system);
	}

p_err:
	return ret;
}

int npu_queue_dqbuf(struct npu_session *session, struct npu_queue *queue, struct vs4l_container_bundle *cbundle, bool nonblocking)
{
	int ret = 0;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;

#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	npu_pm_wake_lock(session);
#endif

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];

	if (!(test_bit(NPU_QUEUE_STATE_QUEUED, &inqueue->state)) ||
		!(test_bit(NPU_QUEUE_STATE_QUEUED, &otqueue->state))) {
		ret = -EINVAL;
		npu_err("invalid in dqbuf state(%lu)\n", inqueue->state);
		goto p_err;
	}

	ret = npu_queue_wait_for_done(session, otqueue, nonblocking);
	if (ret) {
		if (ret != -EWOULDBLOCK)
			npu_uerr("npu_queue_wait_for_done is fail(%d)\n", session, session->hids, ret);
		return ret;
	}

	if (!test_bit(NPU_QUEUE_STATE_DONE, &inqueue->state)) {
		npu_uerr("invalid in invb state(%lu)\n", session, session->hids, inqueue->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(NPU_QUEUE_STATE_DONE, &otqueue->state)) {
		npu_uerr("invalid in otvb state(%lu)\n", session, session->hids, otqueue->state);
		ret = -EINVAL;
		goto p_err;
	}

	clear_bit(NPU_QUEUE_STATE_DONE, &inqueue->state);
	clear_bit(NPU_QUEUE_STATE_DONE, &otqueue->state);

	ret = fill_vs4l_buffer(inqueue, cbundle->m[0].clist, false);
	if (ret) {
		npu_err("fails to fill vs4l buffer for output");
		goto p_err;
	}

	ret = fill_vs4l_buffer(otqueue, cbundle->m[1].clist, false);
	if (ret) {
		npu_err("fails to fill vs4l buffer for output");
		goto p_err;
	}

	npu_queue_buf_sync(inqueue, NPU_FRAME_DQBUF);
	npu_queue_buf_sync(otqueue, NPU_FRAME_DQBUF);

	ret = npu_session_deque(queue);
	if (ret) {
		npu_uerr("fail(%d) in npu_session_deque\n", session, session->hids, ret);
		npu_session_dump(session);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_DEQUEUED, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_DEQUEUED, &otqueue->state);
	clear_bit(NPU_QUEUE_STATE_QUEUED, &inqueue->state);
	clear_bit(NPU_QUEUE_STATE_QUEUED, &otqueue->state);

p_err:
#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	npu_pm_wake_unlock(session);
#endif
	return ret;
}

void npu_queue_done(struct npu_queue *queue,
	struct npu_queue_list *inqueue,
	struct npu_queue_list *otqueue,
	unsigned long flags)
{
	inqueue->flags |= flags;
	otqueue->flags |= flags;

	set_bit(NPU_QUEUE_STATE_DONE, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_DONE, &otqueue->state);
	clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state);
	clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &otqueue->state);
	wake_up(&otqueue->done_wq);
}
