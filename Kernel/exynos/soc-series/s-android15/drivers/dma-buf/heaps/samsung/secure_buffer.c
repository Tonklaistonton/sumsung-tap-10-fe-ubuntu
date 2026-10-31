// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Allocator - secure management
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/smc.h>
#include <linux/kmemleak.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/samsung/samsung-secure-iova.h>

#include "secure_buffer.h"
#include "heap_private.h"

static int samsung_dma_buffer_hvc(unsigned int cmd, unsigned int protid, unsigned long size,
				  unsigned long paddr)
{
	unsigned long ret;

	ret = ppmp_hvc(cmd, 1, protid, size, paddr);
	if (ret) {
		perr("%s: %#x (err=%#lx,size=%#lx,protid=%u,phys=%#lx)",
		     __func__, cmd, ret, size, protid, paddr);
		return ret == E_DRMPLUGIN_BUFFER_LIST_FULL ? -ENOSPC : -EACCES;
	}

	return 0;
}

static int buffer_protect_smc(struct samsung_dma_heap *heap, struct buffer_prot_info *protdesc)
{
	struct device *dev = dma_heap_get_dev(heap->dma_heap);
	unsigned long ret, dma_addr = 0;

	dma_addr = secure_iova_alloc(protdesc->chunk_size, heap->alignment);
	if (!dma_addr)
		return -ENOMEM;

	protdesc->dma_addr = (unsigned int)dma_addr;

	dma_map_single(dev, protdesc, sizeof(*protdesc), DMA_TO_DEVICE);
	ret = ppmp_smc(SMC_DRM_MEMORY_PROT, virt_to_phys(protdesc), 0, 0);
	dma_unmap_single(dev, phys_to_dma(dev, virt_to_phys(protdesc)), sizeof(*protdesc),
			 DMA_TO_DEVICE);
	if (ret) {
		secure_iova_free(dma_addr, protdesc->chunk_size);
		perr("CMD %#x (err=%#lx,dva=%#x,size=%#x,protid=%u,phy=%#lx)",
		     SMC_DRM_MEMORY_UNPROT, ret, protdesc->dma_addr,
		     protdesc->chunk_size, protdesc->flags, protdesc->bus_address);
		return ret == E_DRMPLUGIN_BUFFER_LIST_FULL ? -ENOSPC : -EACCES;
	}

	return 0;
}

static int buffer_unprotect_smc(struct buffer_prot_info *protdesc)
{
	unsigned long ret;

	ret = ppmp_smc(SMC_DRM_MEMORY_UNPROT, virt_to_phys(protdesc), 0, 0);
	if (ret) {
		perr("CMD %#x (err=%#lx,dva=%#x,size=%#x,protid=%u,phy=%#lx)",
		     SMC_DRM_MEMORY_UNPROT, ret, protdesc->dma_addr,
		     protdesc->chunk_size, protdesc->flags, protdesc->bus_address);
		return -EACCES;
	}
	/*
	 * retain the secure device address if unprotection to its area fails.
	 * It might be unusable forever since we do not know the state of the
	 * secure world before returning error from ppmp_smc() above.
	 */
	secure_iova_free(protdesc->dma_addr, protdesc->chunk_size);

	return 0;
}

void *samsung_dma_buffer_copy_offset_info(struct samsung_dma_heap *heap,
					  struct buffer_prot_info *org_info, size_t offset)
{
	struct buffer_prot_info *protdesc;

	if (!dma_heap_flags_use_secure_sysmmu(heap->flags))
		return NULL;

	protdesc = kmalloc(sizeof(*protdesc), GFP_KERNEL);
	if (!protdesc)
		return ERR_PTR(-ENOMEM);

	memcpy(protdesc, org_info, sizeof(*protdesc));
	protdesc->dma_addr += offset;

	return protdesc;
}

void samsung_dma_buffer_release_copied_info(struct buffer_prot_info *info)
{
	kfree(info);
}

void *samsung_dma_buffer_protect(struct samsung_dma_heap *heap, unsigned int chunk_size,
				 unsigned long paddr)
{
	struct buffer_prot_info *protdesc;
	int ret;

	dma_heap_event_begin();

	if (!dma_heap_flags_use_secure_sysmmu(heap->flags)) {
		ret = samsung_dma_buffer_hvc(HVC_DRM_TZMP2_PROT, heap->protection_id,
					     chunk_size, paddr);

		return ret ? ERR_PTR(ret) : 0;
	};

	protdesc = kmalloc(sizeof(*protdesc), GFP_KERNEL);
	if (!protdesc)
		return ERR_PTR(-ENOMEM);

	protdesc->chunk_count = 1;
	protdesc->flags = heap->protection_id;
	protdesc->chunk_size = chunk_size;
	protdesc->bus_address = paddr;

	ret = buffer_protect_smc(heap, protdesc);
	if (ret) {
		kfree(protdesc);
		return ERR_PTR(ret);
	}

	__dma_heap_event_record(DMA_HEAP_EVENT_PROT, heap->name, chunk_size, begin);

	return protdesc;
}

int samsung_dma_buffer_unprotect(struct samsung_dma_heap *heap, unsigned int chunk_size,
				 unsigned long paddr, void *priv)
{
	struct buffer_prot_info *protdesc = priv;
	int ret;

	dma_heap_event_begin();

	if (!dma_heap_flags_use_secure_sysmmu(heap->flags))
		return samsung_dma_buffer_hvc(HVC_DRM_TZMP2_UNPROT, heap->protection_id,
					      chunk_size, paddr);

	if (!protdesc)
		return 0;

	ret = buffer_unprotect_smc(protdesc);
	kfree(protdesc);

	__dma_heap_event_record(DMA_HEAP_EVENT_UNPROT, heap->name, chunk_size, begin);

	return ret;
}

int page_order_index(struct page *page)
{
	unsigned int i, order = compound_order(page);

	for (i = 0; i < NUM_ORDERS; i++) {
		if (orders[i] == order)
			return i;
	}

	BUG_ON(1);

	return -1;
}

int samsung_dma_buffer_hvc_multi(unsigned int cmd, struct list_head *pages, unsigned int protid)
{
	struct list_head pages_per_order[NUM_ORDERS];
	struct drm_sg_table *table;
	struct page *page, *tmp_page;
	struct secure_pool_info *info = get_secure_pool_info(protid);
	unsigned int len = 0;
	unsigned int i, nents = 0;
	unsigned int num_page_per_order[NUM_ORDERS] = {0, };
	unsigned long ret;
	enum dma_heap_event_type event_type = (cmd == HVC_DRM_TZMP2_PROT) ?
		DMA_HEAP_EVENT_PROT : DMA_HEAP_EVENT_UNPROT;

	dma_heap_event_begin();

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < NUM_ORDERS; i++)
		INIT_LIST_HEAD(&pages_per_order[i]);

	list_for_each_entry_safe(page, tmp_page, pages, lru) {
		unsigned int idx = page_order_index(page);

		list_move(&page->lru, &pages_per_order[idx]);

		num_page_per_order[idx]++;
		table->page_nents++;
		len += page_size(page);
	}

	nents = table->page_nents;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (num_page_per_order[i])
			nents++;
	}

	ret = drm_sg_alloc_table(table, nents);
	if (ret)
		goto err_sg_alloc;

	drm_sg_set_table(table, pages_per_order, num_page_per_order);
	ppmp_hvc(cmd, table->page_nents, protid, 0, virt_to_phys(table->lists));
	if (ret) {
		perr("%s: %#x (err=%#lx,nents=%u(pnents=%u),protid=%u)",
		     __func__, cmd, ret, table->nents, table->page_nents, protid);
		ret = E_DRMPLUGIN_BUFFER_LIST_FULL ? -ENOSPC : -EACCES;

		goto err_sg_set;
	}

	__dma_heap_event_record(event_type, info->name, len, begin);

	for (i = 0; i < NUM_ORDERS; i++)
		list_splice(&pages_per_order[i], pages);

err_sg_set:
	drm_sg_free_table(table);
err_sg_alloc:
	kfree(table);

	return ret;

}
