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
#include <linux/device.h>

#include <media/videobuf2-dma-sg.h>
#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/samsung/samsung-dma-mapping.h>
#include <trace/hooks/mm.h>
#include "npu-log.h"
#include "include/npu-binary.h"
#include "include/npu-memory.h"
#include "npu-session.h"
#include "include/npu-config.h"
#include "npu-memory-manual.h"
#include "npu-system.h"

static atomic_t total_mem_size;

#define NPU_MEMORY_DMA_HEAP_FLAG_UNCACHED  BIT(0)

static struct device *non_share_dev, *manual_dev, *nshare_manual_dev;

int samsung_sysmmu_set_vid(struct iommu_domain *dom, int vid);

struct device* get_non_share_dev(void) {
	if (!non_share_dev) {
		probe_err("non_share_dev is not probed\n");
		return ERR_PTR(-EFAULT);
	}
	return non_share_dev;
}

struct device* get_manual_dev(void) {
	if (!manual_dev) {
		probe_err("manual_dev is not probed\n");
		return ERR_PTR(-EFAULT);
	}
	return manual_dev;
}

struct device* get_nshare_manual_dev(void) {
	if (!nshare_manual_dev) {
		probe_err("non_share_manual_dev is not probed\n");
		return ERR_PTR(-EFAULT);
	}
	return nshare_manual_dev;
}

static void npu_memory_init_buffer(struct npu_memory_buffer *buffer, bool list_init)
{
	buffer->dma.dma_buf = NULL;
	buffer->dma.attachment = NULL;
	buffer->dma.sgt = NULL;
	buffer->dma.daddr = 0;
	buffer->dma.vaddr = NULL;
	buffer->npu_sgt = false;
	buffer->imb_for_ncp = false;
	buffer->imb_for_iofm = false;
	buffer->pid = 0;
	buffer->imb_dup = false;
	buffer->manual_dup = false;
	if (list_init) {
		INIT_LIST_HEAD(&buffer->list);
		INIT_LIST_HEAD(&buffer->imb_map_list);
	}
}

inline bool npu_memory_dma_heap_flags_uncached(unsigned long flags)
{
	return !!(flags & NPU_MEMORY_DMA_HEAP_FLAG_UNCACHED);
}

/* trace npu memory for android vendor hook function */
void npu_memory_trace_for_vh(unsigned int size, bool is_add)
{
	if (is_add)
		atomic_add(size, &total_mem_size);
	else
		atomic_sub(size, &total_mem_size);
}

/* '$ cat /proc/meminfo' */
static void npu_meminfo_show(void *data, struct seq_file *m)
{
	unsigned long kb_size = (unsigned long)(atomic_read(&total_mem_size) >> 10);
	seq_printf(m, "NpuSystem:      %8lu kB\n", kb_size);
}

/* '$ echo m > /proc/sysrq-trigger' */
static void npu_mem_show(void *data, unsigned int filter, nodemask_t *nodemask)
{
	unsigned long kb_size = (unsigned long)(atomic_read(&total_mem_size) >> 10);
	pr_info("npu_sys_size:        %8lu kB\n", kb_size);
}

void npu_sgt_flush(struct sg_table *sgt, struct device *dev)
{
	dma_sync_sg_for_device(dev, sgt->sgl, sgt->orig_nents, DMA_TO_DEVICE);
}

int npu_memory_vmap(struct npu_memory_buffer *buffer, u64 size)
{
	struct sg_table *table = buffer->dma.sgt;
	unsigned int npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;

	if (!pages)
		return -ENOMEM;

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	buffer->dma.vaddr = vmap(pages, npages, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	vfree(pages);

	return 0;
}

void npu_memory_free_sgt(struct sg_table *sgt)
{
	struct scatterlist *sg;

	for (sg = sgt->sgl; sg; sg = sg_next(sg))
		if (sg_page(sg))
			__free_pages(sg_page(sg), get_order(sg->length));
	sg_free_table(sgt);
	kfree(sgt);
}

int npu_memory_create_sgt(struct npu_memory_buffer *buffer, u64 size)
{
	struct scatterlist *sg;
	struct sg_table *sgt;
	struct page **pages;
	int *pages_order;
	int buf_extra;
	int max_order;
	int nr_pages;
	int ret = 0;
	int i, j, k;
	int order;
	gfp_t gfp;

	if (size) {
		nr_pages = DIV_ROUND_UP(size, PAGE_SIZE);
		buf_extra = (PAGE_SIZE - size % PAGE_SIZE) % PAGE_SIZE;
		max_order = min(MAX_ORDER - 1, get_order(size));
	} else {
		nr_pages = 1;
		buf_extra = 0;
		max_order = 0;
	}

	pages = kvmalloc_array(nr_pages, sizeof(*pages) + sizeof(*pages_order), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		goto out;
	}
	pages_order = (void *)pages + sizeof(*pages) * nr_pages;

	i = 0;
	while (nr_pages > 0) {
		order = min(get_order(nr_pages * PAGE_SIZE), max_order);

		while (1) {
			gfp = GFP_HIGHUSER | __GFP_COMP | __GFP_NOWARN | __GFP_ZERO | __GFP_NORETRY;
			if (order)
				gfp = gfp & ~__GFP_RECLAIM;
			else
				gfp = (gfp | __GFP_RETRY_MAYFAIL) & ~__GFP_NORETRY;
			pages[i] = alloc_pages(gfp, order);
			if (pages[i])
				break;
			if (!order--) {
				ret = -ENOMEM;
				goto free_partial_alloc;
			}
		}

		max_order = order;
		pages_order[i] = order;

		nr_pages -= 1 << order;
		if (nr_pages <= 0)
			buf_extra += abs(nr_pages) * PAGE_SIZE;
		i++;
	}

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		ret = -ENOMEM;
		goto free_partial_alloc;
	}

	if (sg_alloc_table(sgt, i, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto free_sgt;
	}

	sg = sgt->sgl;
	for (k = 0; k < i; k++, sg = sg_next(sg)) {
		if (k < i - 1) {
			sg_set_page(sg, pages[k], PAGE_SIZE << pages_order[k], 0);
		} else {
			sg_set_page(sg, pages[k], (PAGE_SIZE << pages_order[k]) - buf_extra, 0);
			sg_mark_end(sg);
		}
	}

	buffer->dma.sgt = sgt;

	kvfree(pages);
	return ret;

free_sgt:
	kfree(sgt);
free_partial_alloc:
	for (j = 0; j < i; j++)
		__free_pages(pages[j], pages_order[j]);
	kvfree(pages);
out:
	return ret;
}

bool is_cacheable_dma_buf(struct dma_buf *dma_buf) {
	unsigned long l_flags;

	if (dma_buf_get_flags(dma_buf, &l_flags)) {
		npu_info("dma_buf_get_flags not found\n");
		return false;
	}

	if (npu_memory_dma_heap_flags_uncached(l_flags)) {
		npu_info("This DMABUF is non-cacheable\n");
		return false;
	}

	return true;
}

struct sg_table *get_unset_iommu_sgt(struct npu_memory_buffer *buffer,
	bool is_cacheable, int prot, u32 lazy_unmap, enum dma_data_direction dir) {
	int ret;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	struct device *dev;

	dma_buf = buffer->dma.dma_buf;

	if (is_cacheable)
		dev = get_manual_dev();
	else
		dev = get_nshare_manual_dev();

	attachment = dma_buf_attach(buffer->dma.dma_buf, dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		npu_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->dma.attachment = attachment;

	buffer->dma.attachment->dma_map_attrs |= (DMA_ATTR_SKIP_CPU_SYNC);

	if (lazy_unmap == NPU_SESSION_LAZY_UNMAP_DISABLE)
		buffer->dma.attachment->dma_map_attrs |= (DMA_ATTR_SKIP_LAZY_UNMAP);

	sgt = dma_buf_map_attachment_unlocked(attachment, dir);
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

static struct sg_table *get_unmapped_iommu_sgt(struct npu_memory_buffer *buffer,
	int prot, u32 lazy_unmap)
{
	int ret;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;

	attachment = dma_buf_attach(buffer->dma.dma_buf, nshare_manual_dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		npu_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->dma.attachment = attachment;

	if (prot)
		npu_dma_buf_add_attr(attachment);

	if (lazy_unmap == NPU_SESSION_LAZY_UNMAP_DISABLE)
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

static bool npu_memory_check_imb_use(struct npu_memory *memory, struct npu_session *session,
		struct npu_memory_buffer *buffer)
{
	bool ret = false;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	unsigned long flags;
	struct npu_memory_buffer *npu_memory;

	if (!memory->map_imb_count)
		return ret;

	spin_lock_irqsave(&memory->map_imb_lock, flags);

	if (!list_empty(&memory->map_imb_list)) {
		list_for_each_entry(npu_memory, &memory->map_imb_list, imb_map_list) {
			if (npu_memory == NULL)
				continue;

			if (npu_memory->imb_for_ncp) {
				if ((npu_memory->pid == session->pid) &&
						(npu_memory->usr.fd == buffer->usr.fd)) {
					memcpy((void *)buffer, (void *)npu_memory, sizeof(struct npu_memory_buffer));
					buffer->imb_dup = true;
					npu_info("find already use imb buffer(session(%u)->pid(%d), fd(%d)), imb_for_ncp(%d), imb_dup(%d)\n",
							session->uid, session->pid, buffer->usr.fd, buffer->imb_for_ncp, buffer->imb_dup);
					ret = true;
					break;
				}
			}
		}
	} else {
		npu_info("there are no imb rent\n");
	}

	spin_unlock_irqrestore(&memory->map_imb_lock, flags);
#endif
	return ret;
}

int npu_memory_map_retry(struct npu_memory *memory, struct npu_session *session,
	struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req)
{
	int ret = 0;
	bool complete_suc = false;
	size_t retry_size = 0;
	dma_addr_t daddr = 0;

	struct dma_buf *dma_buf;
	void *vaddr;
	struct device *dev;
	unsigned long flags = 0;

	if (unlikely(!memory)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	npu_info("Retry mapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);

	npu_memory_init_buffer(buffer, true);

	if (npu_memory_check_imb_use(memory, session, buffer))
		goto exist_buffer;

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

	retry_size = ALIGN(buffer->dma.size, NPU_IMB_GRANULARITY);
	npu_info("retry size must align 0x%x. (full size : 0x%lx, aligned size : 0x%lx)\n",
			NPU_IMB_GRANULARITY, buffer->dma.size, retry_size);
	if (rent_imb_space(memory->system, &daddr, retry_size, 0)) {
		npu_err("There are no space on IMB for another buffer\n");
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->imb_for_ncp = true;

	if (!get_unmapped_iommu_sgt(buffer, prot, lazy_unmap)) {
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
			dev = non_share_dev;
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


	buffer->pid = session->pid;

	spin_lock_irqsave(&memory->map_imb_lock, flags);
	list_add_tail(&buffer->imb_map_list, &memory->map_imb_list);
	memory->map_imb_count++;
	npu_info("add map_imb_list session(U%u), session->pid(%d), session->ppid(%d), cnt(%d), fd(%d)\n",
			session->uid, session->pid, session->p_pid, memory->map_imb_count, buffer->usr.fd);
	spin_unlock_irqrestore(&memory->map_imb_lock, flags);

	if (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC)
		npu_memory_alloc_dbg_info(memory, buffer->dma.dma_buf, buffer->dma.size);

exist_buffer:
	npu_dbg("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment, buffer->dma.size);

	complete_suc = true;
p_err:
	if (complete_suc != true) {
		npu_memory_unmap(memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned int flag)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_heap *dma_heap;

	dma_heap = dma_heap_find("system-uncached");
	if (dma_heap) {
		dma_buf = dma_heap_buffer_alloc(dma_heap, size, 0, flag);
		dma_heap_put(dma_heap);
	} else {
		npu_info("dma_heap is not exist\n");
	}

	return dma_buf;
}

struct dma_buf *npu_memory_ion_alloc_cached(size_t size, unsigned int flag)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_heap *dma_heap;

	dma_heap = dma_heap_find("system");
	if (dma_heap) {
		dma_buf = dma_heap_buffer_alloc(dma_heap, size, 0, flag);
		dma_heap_put(dma_heap);
	}

	return dma_buf;
}

dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer)
{
	return sg_dma_address(buffer->dma.sgt->sgl);
}

static inline bool npu_memory_is_region_address(struct npu_memory_buffer *buffer) {
	return (buffer->dma.daddr & 0x100000000) ? true : false;
}

void npu_memory_dma_buf_dva_unmap(
			__attribute__((unused))struct npu_memory_buffer *buffer, bool mapping)
{
	struct iommu_domain *domain = NULL;
	size_t unmapped = 0;
	struct device *dev = buffer->dma.attachment->dev;

	if (!mapping)
		return;

	if (buffer->imb_for_ncp)
		dev = buffer->mapped_dev;

	if (npu_memory_is_region_address(buffer)) // this means user sysmmu 2
		dev = non_share_dev;

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

unsigned long npu_memory_set_prot(int prot,
		struct dma_buf_attachment *attachment)
{
	probe_info("prot : %d", prot);
	attachment->dma_map_attrs |= DMA_ATTR_SET_PRIV_DATA(prot);
	return ((prot) << IOMMU_PRIV_SHIFT);
}

void npu_dma_buf_add_attr(struct dma_buf_attachment *attachment)
{
	attachment->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
}

int npu_memory_probe(struct npu_memory *memory, struct device *dev)
{
	int ret = 0;

	if (unlikely(!memory)) {
		probe_err("Failed to get memory\n");
		return -EINVAL;
	}

	if (unlikely(!dev)) {
		probe_err("Failed to get dev\n");
		return -EINVAL;
	}

	if (unlikely(!non_share_dev)) {
		probe_err("Failed to get non_share_dev\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	if (unlikely(!manual_dev)) {
		probe_err("Failed to get manual_dev\n");
		return -EINVAL;
	}

	if (unlikely(!nshare_manual_dev)) {
		probe_err("Failed to get nshare_manual_dev\n");
		return -EINVAL;
	}
#endif

	memory->system = container_of(memory, struct npu_system, memory);
	memory->dev = dev;

	ret = dma_set_mask(dev, DMA_BIT_MASK(32));
	if (ret)
		probe_err("dma_set_mask is failed(%d) - device in npu_memory_probe\n", ret);

	ret = dma_set_mask(non_share_dev, DMA_BIT_MASK(32));
	if (ret)
		probe_err("dma_set_mask is failed(%d) - non_share_dev in npu_memory_probe\n", ret);

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	probe_info("try dma_set_mask for manual_dev(%d)\n", (manual_dev == NULL));
	ret = dma_set_mask(manual_dev, DMA_BIT_MASK(36));
	if (ret) {
		probe_err("dma_set_mask is failed(%d) - manual_dev in npu_memory_probe\n", ret);
		return ret;
	}
	ret = dma_set_mask(nshare_manual_dev, DMA_BIT_MASK(36));
	if (ret)
		probe_err("dma_set_mask is failed(%d) - nshare_manual_dev in npu_memory_probe\n", ret);
#endif

	spin_lock_init(&memory->map_lock);
	INIT_LIST_HEAD(&memory->map_list);
	memory->map_count = 0;

	spin_lock_init(&memory->map_imb_lock);
	INIT_LIST_HEAD(&memory->map_imb_list);
	memory->map_imb_count = 0;

	spin_lock_init(&memory->alloc_lock);
	INIT_LIST_HEAD(&memory->alloc_list);
	memory->alloc_count = 0;

	spin_lock_init(&memory->valloc_lock);
	INIT_LIST_HEAD(&memory->valloc_list);
	memory->valloc_count = 0;

	spin_lock_init(&memory->info_lock);
	INIT_LIST_HEAD(&memory->info_list);
	memory->info_count = 0;

	atomic_set(&total_mem_size, 0);

	ret = samsung_sysmmu_set_vid(iommu_get_domain_for_dev(non_share_dev), 0);
	if (ret) {
		probe_err("%s(%d) - samsung_sysmmu_set_vid is failed(%d)\n", __func__, __LINE__, ret);
	}

	register_trace_android_vh_meminfo_proc_show(npu_meminfo_show, dev);
	register_trace_android_vh_show_mem(npu_mem_show, dev);

	mutex_init(&memory->sysmmu_state_lock);
	memory->last_sysmmu_state = NORMAL_SYSMMU;

	return 0;
}

int npu_memory_update_sysmmu_status(const char *syscall, struct npu_memory *memory, struct npu_session *session)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_SOC_S5E9955)

	if (!memory) {
		npu_info("npu_memory is NULL, skip this func.\n");
		ret = -EINVAL;
		return ret;
	}
	if (!session) {
		npu_info("npu_memory is NULL, skip this func.\n");
		ret = -EINVAL;
		return ret;
	}

	mutex_lock(&memory->sysmmu_state_lock);
	if (memory->last_sysmmu_state != (u32)session->need_sysmmu2) {
		npu_session_wfi(session, true);
		ret = samsung_sysmmu_set_vid(iommu_get_domain_for_dev(non_share_dev), session->need_sysmmu2);
		if (ret) {
			npu_err("%s - samsung_sysmmu_set_vid is failed(%d)\n", syscall, ret);
			goto p_err;
		}
		memory->last_sysmmu_state = session->need_sysmmu2;
		npu_session_wfi(session, false);
	}
p_err:
	mutex_unlock(&memory->sysmmu_state_lock);
#endif
	return ret;
}

void npu_memory_release(struct npu_memory *memory)
{
	unsigned long flags;
	struct npu_memory_debug_info *info, *temp;

	if (!memory) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&memory->info_lock, flags);
	list_for_each_entry_safe(info, temp, &memory->info_list, list) {
		list_del(&info->list);
		kfree(info);
	}
	spin_unlock_irqrestore(&memory->info_lock, flags);
}

int npu_memory_open(struct npu_memory *memory)
{
	int ret = 0;

	/* to do init code */

	return ret;
}

int npu_memory_close(struct npu_memory *memory)
{
	int ret = 0;

	/* to do cleanup code */

	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
void npu_memory_buffer_dump(struct npu_memory_buffer *buffer)
{
	if (!buffer)
		return;

	npu_dump("Memory name = %s\n", buffer->name);
	npu_dump("Memory vaddr = 0x%llx\n", (unsigned long long)buffer->dma.vaddr);
	npu_dump("Memory daddr = 0x%llx\n", buffer->dma.daddr);
	npu_dump("Memory size = %lu\n", buffer->dma.size);
}

void npu_memory_buffer_health_dump(struct npu_memory_buffer *buffer)
{
	if (!buffer)
		return;

	npu_health("Memory name = %s\n", buffer->name);
	npu_health("Memory vaddr = 0x%llx\n", (unsigned long long)buffer->dma.vaddr);
	npu_health("Memory daddr = 0x%llx\n", buffer->dma.daddr);
	npu_health("Memory size = %lu\n", buffer->dma.size);
}

void npu_memory_dump(struct npu_memory *memory)
{
	unsigned long flags;
	struct npu_memory_buffer *npu_memory;
	struct npu_memory_v_buf *npu_vmemory;

	if (!memory)
		return;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	npu_dump("---------- NPU Memory alloc list ----------\n");
	list_for_each_entry(npu_memory, &memory->alloc_list, list) {
		npu_memory_buffer_dump(npu_memory);
		npu_dump("------------------------------------------------\n");
	}
	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	spin_lock_irqsave(&memory->valloc_lock, flags);
	npu_dump("---------- NPU Memory valloc list ----------\n");
	list_for_each_entry(npu_vmemory, &memory->valloc_list, list) {
		npu_dump("Memory name = %s\n", npu_vmemory->name);
		npu_dump("Memory size = %lu\n", npu_vmemory->size);
		npu_dump("------------------------------------------------\n");
	}
	spin_unlock_irqrestore(&memory->valloc_lock, flags);

	spin_lock_irqsave(&memory->map_lock, flags);
	npu_dump("---------- NPU Memory map list ----------\n");
	list_for_each_entry(npu_memory, &memory->map_list, list) {
		npu_memory_buffer_dump(npu_memory);
		if (npu_memory->ncp)
			npu_dump("Memory name = %s\n", npu_memory->name);
		npu_dump("------------------------------------------------\n");
	}
	spin_unlock_irqrestore(&memory->map_lock, flags);

	spin_lock_irqsave(&memory->map_imb_lock, flags);
	npu_dump("---------- NPU Memory map imb list ----------\n");
	if (likely(!list_empty(&memory->map_imb_list))) {
		list_for_each_entry(npu_memory, &memory->map_imb_list, imb_map_list) {
			npu_memory_buffer_dump(npu_memory);
			if (npu_memory->ncp)
				npu_dump("Memory name = %s\n", npu_memory->name);
			npu_dump("------------------------------------------------\n");
		}
	}
	spin_unlock_irqrestore(&memory->map_imb_lock, flags);
}

void npu_memory_health(struct npu_memory *memory)
{
	unsigned long flags;
	struct npu_memory_buffer *npu_memory;
	struct npu_memory_debug_info *dbg_info;
	size_t	alloc_size = 0;
	size_t	map_size = 0;

	if (!memory)
		return;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	npu_health("---------- NPU Memory alloc list ----------\n");
	list_for_each_entry(npu_memory, &memory->alloc_list, list) {
		alloc_size += npu_memory->dma.size;
	}
	npu_health("npu heap memory(alloc) total = %lu\n", alloc_size);
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	spin_lock_irqsave(&memory->map_lock, flags);
	npu_health("---------- NPU Memory map list ----------\n");
	list_for_each_entry(npu_memory, &memory->map_list, list) {
		map_size += npu_memory->dma.size;
	}
	npu_health("npu heap memory(map) total = %lu, map_cnt = %u\n", map_size, memory->map_count);
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&memory->map_lock, flags);

	spin_lock_irqsave(&memory->info_lock, flags);
	npu_health("---------- NPU Memory dbg info list ----------\n");
	npu_health("memory_info_count : %u\n", memory->info_count);
	list_for_each_entry(dbg_info, &memory->info_list, list) {
		npu_health("PID[%d] CUR[%s] SIZE[%lu] REF_CNT[%lld]\n",
			dbg_info->pid, dbg_info->comm, dbg_info->size,
			atomic64_read(&dbg_info->ref_count));
	}
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&memory->info_lock, flags);

}
#endif /* CONFIG_NPU_KUNIT_TEST */

int npu_memory_map(struct npu_memory *memory, struct npu_session *session,
			struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req)
{
	int ret = 0;
	bool complete_suc = false;

	struct device *dev;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t daddr;
	void *vaddr;

	unsigned long flags;

	if (unlikely(!memory)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	npu_info("Try mapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);

	npu_memory_init_buffer(buffer, true);

	if (npu_memory_check_imb_use(memory, session, buffer))
		goto exist_buffer;

	dma_buf = dma_buf_get(buffer->usr.fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is fail(%pK)\n", dma_buf);
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
			dev = non_share_dev;
		}
	}

	attachment = dma_buf_attach(buffer->dma.dma_buf, dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		goto p_err;
	}
	buffer->dma.attachment = attachment;

	if (prot)
		npu_dma_buf_add_attr(attachment);

	if (lazy_unmap == NPU_SESSION_LAZY_UNMAP_DISABLE)
		buffer->dma.attachment->dma_map_attrs |= (DMA_ATTR_SKIP_LAZY_UNMAP);

	sgt = dma_buf_map_attachment_unlocked(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		npu_err("dma_buf_map_attach is fail(%d) size(%zu)\n",
			ret, buffer->dma.size);
		goto p_err;
	}
	buffer->dma.sgt = sgt;

	daddr = npu_memory_dma_buf_dva_map(buffer);
	if (IS_ERR_VALUE(daddr)) {
		npu_err("Fail(err %pad) to allocate iova\n", &daddr);
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->dma.daddr = daddr;

	if (is_vmap_req) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR(vaddr)) {
			npu_err("Failed to get vaddr (err %pK)\n", vaddr);
			ret = -EFAULT;
			goto p_err;
		}
		buffer->dma.vaddr = vaddr;
	}

	if (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC) {
		spin_lock_irqsave(&memory->map_lock, flags);
		list_add_tail(&buffer->list, &memory->map_list);
		memory->map_count++;
		spin_unlock_irqrestore(&memory->map_lock, flags);
	}

	if (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC)
		npu_memory_alloc_dbg_info(memory, buffer->dma.dma_buf, buffer->dma.size);

exist_buffer:
	npu_dbg("buffer[%pK], paddr[%llx], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->paddr, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment, buffer->dma.size);

	complete_suc = true;
p_err:
	if (complete_suc != true) {
		npu_memory_unmap(memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

void npu_memory_alloc_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf, size_t size)
{
	bool skip = false;
	unsigned long flags;
	struct npu_memory_debug_info *info, *dbg_info, *temp;

	if (!memory) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&memory->info_lock, flags);
	list_for_each_entry_safe(info, temp, &memory->info_list, list) {
		if (info->dma_buf != (u64)dmabuf)
			continue;

		atomic64_set(&info->ref_count, atomic64_read(&dmabuf->file->f_count));
		skip = true;
		break;
	}

	if (skip)
		goto done;

	dbg_info = kzalloc(sizeof(struct npu_memory_debug_info), GFP_ATOMIC);
	if (!dbg_info) {
		npu_err("there is no memory for npu_memory_debug_info\n");
		goto p_err;
	}
	dbg_info->dma_buf = (u64)dmabuf;
	dbg_info->size = size;
	dbg_info->pid = task_pid_nr(current);
	strncpy(dbg_info->comm, current->comm, TASK_COMM_LEN);
	atomic64_set(&dbg_info->ref_count, atomic64_read(&dmabuf->file->f_count));
	list_add_tail(&dbg_info->list, &memory->info_list);
	memory->info_count++;
done:
p_err:
	spin_unlock_irqrestore(&memory->info_lock, flags);
}

void npu_memory_update_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf)
{
	unsigned long flags;
	struct npu_memory_debug_info *info, *temp;

	if (!memory) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&memory->info_lock, flags);
	list_for_each_entry_safe(info, temp, &memory->info_list, list) {
		if (info->dma_buf != (u64)dmabuf)
			continue;

		if (atomic64_read(&dmabuf->file->f_count) > 2) {
			atomic64_set(&info->ref_count, atomic64_read(&dmabuf->file->f_count));
			continue;
		}

		memory->info_count--;
		list_del(&info->list);
		kfree(info);
	}
	spin_unlock_irqrestore(&memory->info_lock, flags);
}

void npu_memory_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	unsigned long flags;

	npu_info("Try unmapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);
	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
			buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment);

	if (buffer->imb_dup)
		goto clear_buffer;

	lock_imb(memory->system, 0);
	if (buffer->imb_for_ncp) {
		return_imb_space(memory->system, 0);

		spin_lock_irqsave(&memory->map_imb_lock, flags);
		if (likely(!list_empty(&buffer->imb_map_list))) {
			list_del(&buffer->imb_map_list);
			INIT_LIST_HEAD(&buffer->imb_map_list);
			memory->map_imb_count--;
			npu_info("map imb count : %d\n", memory->map_imb_count);
		} else {
			npu_info("buffer[%pK] is not linked to imb_map_lock. Skipping remove.\n", buffer);
		}
		spin_unlock_irqrestore(&memory->map_imb_lock, flags);
	}
	if (buffer->dma.daddr && !IS_ERR_VALUE(buffer->dma.daddr))
		npu_memory_dma_buf_dva_unmap(buffer, buffer->imb_for_ncp);
	unlock_imb(memory->system, 0);
	if (!IS_ERR_OR_NULL(buffer->dma.vaddr))
		npu_dma_buf_vunmap(buffer->dma.dma_buf, buffer->dma.vaddr);
	if (!IS_ERR_OR_NULL(buffer->dma.sgt))
		dma_buf_unmap_attachment_unlocked(buffer->dma.attachment, buffer->dma.sgt, DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(buffer->dma.attachment) && !IS_ERR_OR_NULL(buffer->dma.dma_buf))
		dma_buf_detach(buffer->dma.dma_buf, buffer->dma.attachment);
	if (!IS_ERR_OR_NULL(buffer->dma.dma_buf)) {
		dma_buf_put(buffer->dma.dma_buf);
		if (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC)
			npu_memory_update_dbg_info(memory, buffer->dma.dma_buf);
	}

	if (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC) {
		spin_lock_irqsave(&memory->map_lock, flags);
		if (likely(!list_empty(&buffer->list))) {
			list_del(&buffer->list);
			INIT_LIST_HEAD(&buffer->list);
			memory->map_count--;
		} else
			npu_info("buffer[%pK] is not linked to map_lock. Skipping remove.\n", buffer);

		spin_unlock_irqrestore(&memory->map_lock, flags);
	}

clear_buffer:
	npu_memory_init_buffer(buffer, false);
}

int __npu_memory_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer,
	u32 lazy_unmap, int prot, struct dma_buf *dma_buf)
{
	int ret = 0;
	bool complete_suc = false;

	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t daddr;
	void *vaddr;

	unsigned long flags = 0;

	struct device *dev;

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
			dev = non_share_dev;
		}
	}

	attachment = dma_buf_attach(dma_buf, dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		npu_err("dma_buf_attach is fail(%d)\n", ret);
		goto p_err;
	}
	buffer->dma.attachment = attachment;

	if (prot)
		npu_dma_buf_add_attr(attachment);

	if (lazy_unmap == NPU_SESSION_LAZY_UNMAP_DISABLE)
		buffer->dma.attachment->dma_map_attrs |= (DMA_ATTR_SKIP_LAZY_UNMAP);

	sgt = dma_buf_map_attachment_unlocked(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		npu_err("dma_buf_map_attach is fail(%d) size(%zu)\n",
		ret, buffer->dma.size);
		goto p_err;
	}
	buffer->dma.sgt = sgt;

	daddr = npu_memory_dma_buf_dva_map(buffer);
	if (IS_ERR_VALUE(daddr)) {
		npu_err("fail(err %pad) to allocate iova\n", &daddr);
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->dma.daddr = daddr;

	vaddr = npu_dma_buf_vmap(dma_buf);
	if (IS_ERR(vaddr)) {
		npu_err("fail(err %pK) in dma_buf_vmap\n", vaddr);
		ret = -EFAULT;
		goto p_err;
	}
	buffer->dma.vaddr = vaddr;

	complete_suc = true;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	list_add_tail(&buffer->list, &memory->alloc_list);
	memory->alloc_count++;
	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	npu_dbg("buffer[%pK], paddr[%llx], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
	buffer, buffer->paddr, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment, buffer->dma.size);
p_err:
	if (complete_suc != true) {
		npu_memory_free(memory, buffer);
		npu_memory_dump(memory);
	}
	return ret;
}

int npu_memory_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer, u32 lazy_unmap, int prot)
{
	int ret = 0;
	struct dma_buf *dma_buf;
	unsigned int flag = 0;
	size_t size;

	if (unlikely(!memory)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	if (!buffer->dma.size)
		return 0;

	npu_memory_init_buffer(buffer, true);

	size = buffer->dma.size;

	dma_buf = npu_memory_ion_alloc(size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("npu_memory_ion_alloc is fail(%pK) size(%zu)\n",
				dma_buf, buffer->dma.size);
		ret = -EINVAL;
		npu_memory_dump(memory);
		return ret;
	}
	buffer->dma.dma_buf = dma_buf;

	return __npu_memory_alloc(memory, buffer, lazy_unmap, prot, dma_buf);
}

int npu_memory_alloc_cached(struct npu_memory *memory, struct npu_memory_buffer *buffer, u32 lazy_unmap, int prot)
{
	struct dma_buf *dma_buf;
	unsigned int flag = 0;
	size_t size = buffer->dma.size;;

	if (!buffer->dma.size)
		return 0; /* Nothing to do. */

	INIT_LIST_HEAD(&buffer->list);
	INIT_LIST_HEAD(&buffer->imb_map_list);

	dma_buf = npu_memory_ion_alloc_cached(size, flag);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("cached heap allocation failed(%pK) size(%zu)\n",
			dma_buf, buffer->dma.size);
		npu_memory_dump(memory);
		return -EINVAL;
	}

	buffer->dma.dma_buf = dma_buf;

	return __npu_memory_alloc(memory, buffer, lazy_unmap, prot, dma_buf);
}

int npu_memory_free(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	unsigned long flags;

	if (unlikely(!memory)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	if (!buffer->dma.size)
		return 0;

	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
		buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment);

	if (!IS_ERR_OR_NULL(buffer->dma.vaddr))
		npu_dma_buf_vunmap(buffer->dma.dma_buf, buffer->dma.vaddr);
	if (buffer->dma.daddr && !IS_ERR_VALUE(buffer->dma.daddr))
		npu_memory_dma_buf_dva_unmap(buffer, false);
	if (!IS_ERR_OR_NULL(buffer->dma.sgt))
		dma_buf_unmap_attachment_unlocked(buffer->dma.attachment, buffer->dma.sgt, DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(buffer->dma.attachment) && !IS_ERR_OR_NULL(buffer->dma.dma_buf))
		dma_buf_detach(buffer->dma.dma_buf, buffer->dma.attachment);
	if (!IS_ERR_OR_NULL(buffer->dma.dma_buf))
		dma_buf_put(buffer->dma.dma_buf);

	npu_memory_init_buffer(buffer, false);

	spin_lock_irqsave(&memory->alloc_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		memory->alloc_count--;
	} else
		npu_info("buffer[%pK] is not linked to alloc_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&memory->alloc_lock, flags);

	return 0;
}

void npu_memory_profile_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	if (npu_memory_is_region_address(buffer)) {
		struct npu_memory_region *region = npu_memory_region_get_dva(memory->system->manual);
		npu_memory_region_unmap(region, buffer);
	} else { // !session->need_sysmmu2
		npu_memory_unmap(memory, buffer);
	}
}

int npu_memory_profile_free(struct npu_memory *memory, struct npu_memory_buffer *buffer)
{
	int ret = 0;
	if (npu_memory_is_region_address(buffer)) {
		struct npu_memory_region *region = npu_memory_region_get_dva(memory->system->manual);
		ret = npu_memory_region_free(region, buffer);
	} else { // !session->need_sysmmu2
		ret = npu_memory_free(memory, buffer);
	}

	return ret;
}

/**
 * npu_memory_copy - Create a copy of NPU mem dmabuf.
 * @memory: Session memory manager.
 * @buffer: memory buffer.
 * @offset: Start offset in source buffer.
 * @size: Buffer size to copy.
 */
struct npu_memory_buffer *
npu_memory_copy(struct npu_memory *memory, struct npu_memory_buffer *buffer,
					u32 lazy_unmap, size_t offset, size_t size)
{
	struct npu_memory_buffer *copy_buffer;
	void *vaddr;
	int ret;

	if (!buffer->dma.dma_buf) {
		npu_err("no dmabuf in source buffer\n");
		return ERR_PTR(-EINVAL);
	}

	if (offset >= buffer->dma.dma_buf->size || offset + size > buffer->dma.dma_buf->size) {
		npu_err("invalid size args. size=0x%lx, offset=0x%lx, src size=0x%lx\n",
			size, offset, buffer->dma.size);
		return ERR_PTR(-EINVAL);
	}

	copy_buffer = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (!copy_buffer)
		return ERR_PTR(-ENOMEM);

	/* Copy source buffer properties. */
	memcpy(copy_buffer, buffer, sizeof(*buffer));

	copy_buffer->dma.size = size;

	/*
	 * We do memcpy from user NCP header to copy_buffer.
	 * Hence, allocate cached dma-buf heap.
	 */
	ret = npu_memory_alloc_cached(memory, copy_buffer, lazy_unmap, npu_get_configs(NPU_PBHA_HINT_00));
	if (ret) {
		npu_err("failed to allocate copy buffer(%d)\n", ret);
		goto err_free_mem;
	}

	/* Create virtual mapping for CPU access. */
	if (!buffer->dma.vaddr) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR_OR_NULL(vaddr)) {
			npu_err("failed to vmap original dmabuf(%ld)", PTR_ERR(vaddr));
			goto err_free_copy_buffer;
		}

		buffer->dma.vaddr = vaddr;
	}

	vaddr = buffer->dma.vaddr + offset;

	/* Begin CPU access. */
	dma_buf_begin_cpu_access(buffer->dma.dma_buf, DMA_BIDIRECTIONAL);
	dma_buf_begin_cpu_access(copy_buffer->dma.dma_buf, DMA_BIDIRECTIONAL);

	memcpy(copy_buffer->dma.vaddr, vaddr, size);

	/* End CPU access.. */
	dma_buf_end_cpu_access(copy_buffer->dma.dma_buf, DMA_BIDIRECTIONAL);
	dma_buf_end_cpu_access(buffer->dma.dma_buf, DMA_BIDIRECTIONAL);

	return copy_buffer;
err_free_copy_buffer:
	npu_memory_free(memory, copy_buffer);
err_free_mem:
	kfree(copy_buffer);
	return ERR_PTR(ret);
}

int npu_memory_v_alloc(struct npu_memory *memory, struct npu_memory_v_buf *buffer)
{
	unsigned long flags = 0;

	if (unlikely(!memory)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_v_buf\n");
		return -EINVAL;
	}

	buffer->v_buf = (char *)vmalloc((unsigned long)buffer->size);
	if (!buffer->v_buf) {
		npu_err("failed vmalloc\n");
		npu_memory_dump(memory);
		return -ENOMEM;
	}

	spin_lock_irqsave(&memory->valloc_lock, flags);
	list_add_tail(&buffer->list, &memory->valloc_list);
	memory->valloc_count++;
	spin_unlock_irqrestore(&memory->valloc_lock, flags);

	return 0;
}

void npu_memory_v_free(struct npu_memory *memory, struct npu_memory_v_buf *buffer)
{
	unsigned long flags = 0;

	vfree((void *)buffer->v_buf);

	spin_lock_irqsave(&memory->valloc_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		memory->valloc_count--;
	} else
		npu_info("buffer[%pK] is not linked to alloc_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&memory->valloc_lock, flags);

	return;
}

void *npu_dma_buf_vmap(struct dma_buf *dmabuf)
{
	void *vaddr;
	int ret = 0;
	struct iosys_map map;

	memset(&map, 0, sizeof(map));
	ret = dma_buf_vmap_unlocked(dmabuf, &map);
	if (unlikely(ret)) {
		vaddr = ERR_PTR(ret);
	} else {
		vaddr = map.vaddr;
	}
	return vaddr;
}

void npu_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(vaddr);
	dma_buf_vunmap_unlocked(dmabuf, &map);
}

MODULE_IMPORT_NS(DMA_BUF);

int npu_memory_clone_buffer(struct npu_memory *memory,
			struct npu_memory_buffer *sbuf,	struct npu_memory_buffer *dbuf, phys_addr_t base)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	int iommu_attributes = 0;
	bool complete_suc = false;
	size_t size;
	ssize_t map_size = 0;
	struct iommu_domain *domain = iommu_get_domain_for_dev(non_share_dev);

	memcpy((void *)dbuf, (void *)sbuf, sizeof(struct npu_memory_buffer));

	dbuf->dma.daddr += 0x100000000;
	size = dbuf->dma.size;

	npu_info("iommu map daddr[%llx]\n", dbuf->dma.daddr);

	if (likely(domain)) {
		if (base) {
			ret = iommu_map(domain, dbuf->dma.daddr, base, dbuf->dma.size, 0, GFP_KERNEL);
		} else {
			map_size = iommu_map_sgtable(domain, dbuf->dma.daddr, dbuf->dma.sgt, iommu_attributes);
		}
	} else {
		npu_err("fail(err %pad) in iommu_map_sg\n", &dbuf->dma.daddr);
		ret = -ENOMEM;
		goto p_err;
	}

	if (map_size != size) {
		npu_info("[WARNING] iommu_map_sgtable return %lu, we want to map %lu\n",
				map_size, size);
	}

	if (map_size < 0 || ret < 0) {
		npu_err("fail(err %pad) in iommu_map\n", &dbuf->dma.daddr);
		ret = -ENOMEM;
		goto p_err;
	}

	complete_suc = true;
p_err:
	if (complete_suc != true) {
		npu_memory_free_from_heap(memory, dbuf, true);
		npu_memory_dump(memory);
	}
#endif
	return ret;
}

int npu_memory_alloc_from_heap(struct platform_device *pdev,
	struct npu_memory_buffer *buffer, dma_addr_t daddr,
	struct npu_memory *memory, const char *heapname, int prot)
{
	int ret = 0;
	bool complete_suc = false;
	unsigned long flags;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	phys_addr_t phys_addr;
	void *vaddr;
	int flag;

	size_t size;
	size_t map_size = 0;
	int iommu_attributes = 0;

	npu_memory_init_buffer(buffer, true);
	strncpy(buffer->name, heapname, strlen(heapname));

	size = buffer->dma.size;
	flag = 0;

	if (daddr) {
		struct iommu_domain *domain = iommu_get_domain_for_dev(non_share_dev);

		ret = npu_memory_create_sgt(buffer, size);
		if (ret) {
			probe_err("fail(err %d) in npu_memory_create_sgt\n", ret);
			ret = -ENOMEM;
			goto p_err;
		}

		mb();

		buffer->npu_sgt = true;
		npu_memory_trace_for_vh(buffer->dma.size, true);

		if (prot)
			iommu_attributes = ((prot) << IOMMU_PRIV_SHIFT);

		if (likely(domain)) {
			map_size = iommu_map_sgtable(domain, daddr, buffer->dma.sgt, iommu_attributes);
		} else {
			probe_err("fail(err %pad) in iommu_map_sg\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}

		npu_sgt_flush(buffer->dma.sgt, non_share_dev);

		buffer->dma.daddr = daddr;

		if (map_size != size) {
			probe_info("iommu_map_sgtable return %lu, we want to map %lu",
				map_size, size);
		}

		ret = npu_memory_vmap(buffer, size);
		if (ret) {
			probe_err("fail(err %d) in npu_memory_vmap\n", ret);
			goto p_err;
		}
	} else {
		dma_buf = npu_memory_ion_alloc(size, flag);
		if (IS_ERR_OR_NULL(dma_buf)) {
			probe_err("npu_memory_ion_alloc is fail(%p)\n", dma_buf);
			ret = -EINVAL;
			goto p_err;
		}
		buffer->dma.dma_buf = dma_buf;

		attachment = dma_buf_attach(buffer->dma.dma_buf, &pdev->dev);
		if (IS_ERR(attachment)) {
			ret = PTR_ERR(attachment);
			probe_err("dma_buf_attach is fail(%d)\n", ret);
			goto p_err;
		}
		buffer->dma.attachment = attachment;

		if (prot)
			iommu_attributes = npu_memory_set_prot(prot, attachment);

		sgt = dma_buf_map_attachment_unlocked(buffer->dma.attachment, DMA_BIDIRECTIONAL);
		if (IS_ERR(sgt)) {
			ret = PTR_ERR(sgt);
			probe_err("dma_buf_map_attach is fail(%d)\n", ret);
			goto p_err;
		}
		buffer->dma.sgt = sgt;

		daddr = npu_memory_dma_buf_dva_map(buffer);
		if (IS_ERR_VALUE(daddr)) {
			probe_err("fail(err %pad) to allocate iova\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}
		buffer->dma.daddr = daddr;

		vaddr = npu_dma_buf_vmap(dma_buf);
		if (IS_ERR(vaddr) || !vaddr) {
			if (vaddr)
				probe_err("fail(err %p) in dma_buf_vmap\n", vaddr);
			else /* !vaddr */
				probe_err("fail(err NULL) in dma_buf_vmap\n");

			ret = -EFAULT;
			goto p_err;
		}
		buffer->dma.vaddr = vaddr;
	}

	complete_suc = true;

	/* But, first phys. Not all */
	phys_addr = sg_phys(buffer->dma.sgt->sgl);
	buffer->paddr = phys_addr;

	spin_lock_irqsave(&memory->alloc_lock, flags);
	list_add_tail(&buffer->list, &memory->alloc_list);
	memory->alloc_count++;
	spin_unlock_irqrestore(&memory->alloc_lock, flags);
p_err:
	if (complete_suc != true) {
		npu_memory_free_from_heap(memory, buffer, false);
		npu_memory_dump(memory);
	}
	return ret;
}

void npu_memory_free_from_heap(struct npu_memory *memory, struct npu_memory_buffer *buffer, bool dual)
{
	unsigned long flags;

	if (buffer->npu_sgt) {
		struct iommu_domain *domain = iommu_get_domain_for_dev(non_share_dev);

		if (buffer->dma.vaddr && !dual)
			vunmap(buffer->dma.vaddr);
		if (buffer->dma.daddr && !IS_ERR_VALUE(buffer->dma.daddr)) {
			npu_info("Free daddr[%llx]\n", buffer->dma.daddr);
			iommu_unmap(domain, buffer->dma.daddr, buffer->dma.size);
		}
		if (buffer->dma.sgt && !dual)
			npu_memory_free_sgt(buffer->dma.sgt);
		if (!dual)
			npu_memory_trace_for_vh(buffer->dma.size, false);
	} else {
		if (buffer->dma.vaddr)
			npu_dma_buf_vunmap(buffer->dma.dma_buf, buffer->dma.vaddr);
		if (buffer->dma.daddr && !IS_ERR_VALUE(buffer->dma.daddr))
			npu_memory_dma_buf_dva_unmap(buffer, true);
		if (buffer->dma.sgt)
			dma_buf_unmap_attachment(buffer->dma.attachment, buffer->dma.sgt, DMA_BIDIRECTIONAL);
		if (buffer->dma.attachment)
			dma_buf_detach(buffer->dma.dma_buf, buffer->dma.attachment);
		if (buffer->dma.dma_buf)
			dma_buf_put(buffer->dma.dma_buf);
	}

	npu_memory_init_buffer(buffer,false);

	if (!dual) {
		spin_lock_irqsave(&memory->alloc_lock, flags);
		if (likely(!list_empty(&buffer->list))) {
			list_del(&buffer->list);
			INIT_LIST_HEAD(&buffer->list);
			memory->alloc_count--;
		} else
			npu_info("buffer[%pK] is not linked to alloc_lock. Skipping remove.\n", buffer);

		spin_unlock_irqrestore(&memory->alloc_lock, flags);
	}
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int npu_memory_suspend(struct device *dev)
{
	npu_dbg("called\n");
	return 0;
}

static int npu_memory_resume(struct device *dev)
{
	npu_dbg("called\n");
	return 0;
}
#endif

static int npu_memory_nshare_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	if (unlikely(!pdev)) {
		probe_err("Failed to get platform_device\n");
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	dev = &pdev->dev;
	if (unlikely(!dev)) {
		probe_err("Failed to get device\n");
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	non_share_dev = dev;

	probe_info("success in %s\n", __func__);
err_exit:
	return ret;
}

static int npu_memory_nshare_remove(struct platform_device *pdev)
{
	int ret = 0;

	if (unlikely(!pdev)) {
		probe_err("Failed to get platform_device\n");
		return -EINVAL;
	}

	probe_info("complete in %s\n", __func__);
	return ret;
}

static int npu_memory_manual_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	struct device *dev;

	if (unlikely(!pdev)) {
		probe_err("Failed to get platform_device\n");
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	dev = &pdev->dev;
	if (unlikely(!dev)) {
		probe_err("Failed to get device\n");
		ret = -EPROBE_DEFER;
		goto err_exit;
	}

	manual_dev = dev;

	probe_info("success in %s\n", __func__);
err_exit:
#endif
	return ret;
}

static int  npu_memory_manual_dev_remove(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	if (unlikely(!pdev)) {
		probe_err("Failed to get platform_device\n");
		return  -EINVAL;
	}

	probe_info("complete in %s\n", __func__);
#endif
	return 0;
}

static const struct dev_pm_ops npu_memory_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(npu_memory_suspend, npu_memory_resume)
};

static int npu_memory_nshare_manual_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	if (unlikely(!pdev)) {
		npu_err("Failed to get platform_device\n");
		ret = -EINVAL;
		goto err_exit;
	}

	dev = &pdev->dev;
	if (unlikely(!dev)) {
		npu_err("Failed to get device\n");
		ret = -EINVAL;
		goto err_exit;
	}

	nshare_manual_dev = dev;

	probe_info("success in %s\n", __func__);
err_exit:
	return ret;
}

static int npu_memory_nshare_manual_dev_remove(struct platform_device *pdev)
{
	if (unlikely(!pdev)) {
		probe_err("Failed to get platform_device\n");
		return -EINVAL;
	}

	probe_info("complete in %s\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id exynos_npu_memory_match[] = {
	{
		.compatible = "samsung,exynos-npu-nshare"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_memory_match);

const static struct of_device_id exynos_npu_manual_match[] = {
	{
		.compatible = "samsung,exynos-npu-manual"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_manual_match);

const static struct of_device_id exynos_npu_nshare_manual_match[] = {
	{
		.compatible = "samsung,exynos-npu-nshare-manual"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_nshare_manual_match);
#endif

static struct platform_driver npu_memory_driver = {
	.probe	= npu_memory_nshare_probe,
	.remove = npu_memory_nshare_remove,
	.driver = {
		.name	= "exynos-npu-nshare",
		.owner	= THIS_MODULE,
		.pm	= &npu_memory_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_memory_match),
	},
};

static struct platform_driver npu_manual_driver = {
	.probe	= npu_memory_manual_dev_probe,
	.remove = npu_memory_manual_dev_remove,
	.driver = {
		.name	= "exynos-npu-manual",
		.owner	= THIS_MODULE,
		.pm	= &npu_memory_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_manual_match),
	},
};

static struct platform_driver npu_nshare_manual_driver = {
	.probe	= npu_memory_nshare_manual_dev_probe,
	.remove = npu_memory_nshare_manual_dev_remove,
	.driver = {
		.name	= "exynos-npu-nshare-manual",
		.owner	= THIS_MODULE,
		.pm	= &npu_memory_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_nshare_manual_match),
	},
};

int __init npu_memory_register(void)
{
	int ret = 0;

	ret = platform_driver_register(&npu_memory_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}

	ret = platform_driver_register(&npu_manual_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}

	ret = platform_driver_register(&npu_nshare_manual_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}

	probe_info("success in %s\n", __func__);
	ret = 0;

err_exit:
	return ret;
}

void __exit npu_memory_unregister(void)
{
	platform_driver_unregister(&npu_memory_driver);
	platform_driver_unregister(&npu_manual_driver);
	platform_driver_unregister(&npu_nshare_manual_driver);

	probe_info("success in %s\n", __func__);
}
