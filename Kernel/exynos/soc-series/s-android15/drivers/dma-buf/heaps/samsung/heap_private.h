// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Allocator - Internal header
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#ifndef _HEAP_PRIVATE_H
#define _HEAP_PRIVATE_H

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/sec_mm.h>

#include "secure_buffer.h"
#include "deferred-free-helper.h"

struct dma_iovm_map {
	struct list_head list;
	struct device *dev;
	struct sg_table table;
	unsigned long attrs;
	unsigned int mapcnt;
};

struct dmabuf_trace_buffer;

struct samsung_dma_buffer {
	struct samsung_dma_heap *heap;
	struct dmabuf_trace_buffer *trace_buffer;
	struct list_head attachments;
	/* Manage buffer resource of attachments and vaddr, vmap_cnt */
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	void *vaddr;
	void *priv;
	unsigned long flags;
	unsigned long i_node;
	int vmap_cnt;
	struct deferred_freelist_item deferred_free;
};

struct sg_table *samsung_heap_map_dma_buf(struct dma_buf_attachment *a,
					  enum dma_data_direction direction);
void samsung_heap_unmap_dma_buf(struct dma_buf_attachment *a,
				struct sg_table *table,
				enum dma_data_direction direction);
int samsung_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction);
int samsung_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction);
int samsung_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma);
int samsung_heap_vmap(struct dma_buf *dmabuf, struct iosys_map *map);
void samsung_heap_vunmap(struct dma_buf *dmabuf, struct iosys_map *map);
int samsung_heap_dma_buf_get_flags(struct dma_buf *dmabuf, unsigned long *flags);
void samsung_heap_dma_buf_release(struct dma_buf *dmabuf);

/*
 * struct samsung_heap_ops
 * @allocate : Allocate the page as much as buffer->len, which is aligned with
 * heap->alignment. On success, buffer->sg_table is allocated and pages are set up.
 * @release : Release the pages registerted in buffer->sg_table. Remove the sg_table.
 */
struct samsung_heap_ops {
	int (*allocate)(struct samsung_dma_buffer *buffer);
	void (*release)(struct samsung_dma_buffer *buffer, enum df_reason reason);
	long (*get_pool_size)(void);
};

struct samsung_dma_heap {
	struct dma_heap *dma_heap;
	const struct samsung_heap_ops *ops;
	void *priv;
	const char *name;
	unsigned long flags;
	unsigned int alignment;
	unsigned int protection_id;
	atomic_long_t total_bytes;
};

extern const struct dma_buf_ops samsung_dma_buf_ops;

#define DEFINE_SAMSUNG_DMA_BUF_EXPORT_INFO(name, heap_name)	\
	struct dma_buf_export_info name = { .exp_name = heap_name, \
					 .owner = THIS_MODULE }

enum dma_heap_pool_event_type {
	DMA_HEAP_POOL_EVENT_ALLOC = 0,
	DMA_HEAP_POOL_EVENT_FREE,
	DMA_HEAP_POOL_EVENT_SHRINK,
	DMA_HEAP_POOL_EVENT_PREFETCH,
};

enum dma_heap_event_type {
	DMA_HEAP_EVENT_ALLOC = 0,
	DMA_HEAP_EVENT_FREE,
	DMA_HEAP_EVENT_PROT,
	DMA_HEAP_EVENT_UNPROT,
	DMA_HEAP_EVENT_FLUSH,
	DMA_HEAP_EVENT_PAGE_POOL_ALLOC,
};

#define dma_heap_event_begin() ktime_t begin  = ktime_get()
void dma_heap_event_record(enum dma_heap_event_type type, struct dma_buf *dmabuf, ktime_t begin);
void __dma_heap_event_record(enum dma_heap_event_type type, const char *name,
			     size_t size, ktime_t begin);
void dma_heap_event_pool_record(enum dma_heap_pool_event_type type, unsigned int protid,
				size_t size, long diff, long pool_size);

bool is_dma_heap_exception_page(struct page *page);
void heap_sgtable_pages_clean(struct sg_table *sgt);
void heap_cache_flush(struct samsung_dma_buffer *buffer);
void heap_pages_flush(struct device *dev, struct list_head *page_list);
void heap_page_clean(struct page *pages, unsigned long size);
struct samsung_dma_buffer *samsung_dma_buffer_alloc(struct samsung_dma_heap *samsung_dma_heap,
						    unsigned long size);
void samsung_dma_buffer_free(struct samsung_dma_buffer *buffer);
struct samsung_dma_heap *samsung_heap_create(struct device *dev, void *priv);
int samsung_heap_add(struct samsung_dma_heap *heap);
struct dma_buf *samsung_export_dmabuf(struct samsung_dma_buffer *buffer, unsigned long fd_flags);
void show_dmabuf_trace_info(void);
void show_dmabuf_dva(struct device *dev);

#define DMA_HEAP_FLAG_UNCACHED  BIT(0)
#define DMA_HEAP_FLAG_PROTECTED BIT(1)
#define DMA_HEAP_FLAG_SECURE_SYSMMU BIT(2)
#define DMA_HEAP_FLAG_DEFERRED_FREE BIT(3)
#define DMA_HEAP_FLAG_PREFETCH BIT(4)
#define DMA_HEAP_FLAG_DIRECT_IO BIT(5)

static inline bool dma_heap_flags_direct_io(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_DIRECT_IO);
}

static inline bool dma_heap_flags_prefetch(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_PREFETCH);
}

static inline bool dma_heap_flags_deferred_free(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_DEFERRED_FREE);
}

static inline bool dma_heap_flags_uncached(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_UNCACHED);
}

static inline bool dma_heap_flags_protected(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_PROTECTED);
}

static inline bool dma_heap_skip_cache_ops(unsigned long flags)
{
	return dma_heap_flags_protected(flags) || dma_heap_flags_uncached(flags);
}

static inline bool dma_heap_flags_use_secure_sysmmu(unsigned long flags)
{
	return !!(flags & DMA_HEAP_FLAG_SECURE_SYSMMU);
}

/*
 * Use pre-mapped protected device virtual address instead of dma-mapping.
 */
static inline bool dma_heap_secure_sysmmu_buffer(struct device *dev, unsigned long flags)
{
	return dma_heap_flags_protected(flags) &&
		dma_heap_flags_use_secure_sysmmu(flags) && !!dev_iommu_fwspec_get(dev);
}

int page_order_index(struct page *page);
int samsung_dma_buffer_hvc_multi(unsigned int cmd, struct list_head *pages, unsigned int protid);
void *samsung_dma_buffer_protect(struct samsung_dma_heap *heap, unsigned int chunk_size,
				 unsigned long paddr);
int samsung_dma_buffer_unprotect(struct samsung_dma_heap *heap, unsigned int chunk_size,
				 unsigned long paddr, void *priv);
void *samsung_dma_buffer_copy_offset_info(struct samsung_dma_heap *heap,
					  struct buffer_prot_info *org_info, size_t offset);
void samsung_dma_buffer_release_copied_info(struct buffer_prot_info *info);

int __init dmabuf_trace_create(void);
void dmabuf_trace_remove(void);

int dmabuf_trace_alloc(struct dma_buf *dmabuf);
void dmabuf_trace_free(struct dma_buf *dmabuf);
int dmabuf_trace_track_buffer(struct dma_buf *dmabuf);
int dmabuf_trace_untrack_buffer(struct dma_buf *dmabuf);
void dmabuf_trace_map(struct dma_buf_attachment *a);
void dmabuf_trace_unmap(struct dma_buf_attachment *a);

static inline u64 samsung_heap_total_kbsize(struct samsung_dma_heap *heap)
{
	return div_u64(atomic_long_read(&heap->total_bytes), 1024);
}

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_PREALLOCATED_CHUNK)
int __init preallocated_chunk_dma_heap_init(void);
void preallocated_chunk_dma_heap_exit(void);
#else
static inline int __init preallocated_chunk_dma_heap_init(void)
{
	return 0;
}

#define preallocated_chunk_dma_heap_exit() do { } while (0)
#endif

/*
 * The selection of the orders used for allocation (2MB, 1MB, 64K, 4K) is designed
 * to match with the sizes often found in IOMMUs. Using high order pages instead
 * of order 0 pages can significantly improve the performance of many IOMMUs
 * by reducing TLB pressure and time spent updating page tables.
 */
static const unsigned int orders[] = {9, 8, 4, 0};
#define NUM_ORDERS ARRAY_SIZE(orders)

/**
 * struct secure_page_pool - secure page pool struct
 * @items:		array of list of pages
 * @count:		array of number of pages in the pool
 * @lock:		lock protecting this struct and especially the count item list
 * @order:		order of pages in the pool
 *
 * Allows you to keep a pool of pre allocated pages to use
 */
struct secure_page_pool {
	struct list_head items;
	int count;
	spinlock_t lock;
	unsigned int order;
	unsigned int protid;
};

struct secure_pool_info {
	struct secure_page_pool *pools[NUM_ORDERS];
	unsigned int min;
	unsigned int protid;
	const char *name;
};

int samsung_page_pool_init(void);
void samsung_page_pool_destroy(void);

struct secure_pool_info *get_secure_pool_info(unsigned int protid);
int add_secure_pool_info(const char *name, unsigned int protid);

long get_normal_pool_size(void);
long get_secure_pool_size(struct secure_page_pool **pools);
long get_total_secure_pool_size(void);

long get_samsung_pool_size(struct dma_heap *dma_heap);

struct page *secure_page_pool_alloc(struct secure_page_pool *pool);
void secure_page_pool_free(struct secure_page_pool *pool, struct page *page);
struct page *secure_page_pool_remove(struct secure_page_pool *pool);

void secure_pool_set_prefetch_min(struct secure_pool_info *info, unsigned int min);

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_SECURE_SYSTEM)
int __init secure_system_dma_heap_init(void);
void secure_system_dma_heap_exit(void);
#else
static inline int __init secure_system_dma_heap_init(void)
{
	return 0;
}
#define secure_system_dma_heap_exit() do { } while (0)
#endif

#if IS_ENABLED(CONFIG_DMABUF_CONTAINER)
int __init dmabuf_container_init(void);
#else
static inline int __init dmabuf_container_init(void)
{
	return 0;
}
#endif

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_SYSTEM)
int __init system_dma_heap_init(void);
void system_dma_heap_exit(void);
#else
static inline int __init system_dma_heap_init(void)
{
	return 0;
}
#define system_dma_heap_exit() do { } while (0)
#endif
void normal_pool_free_pages(struct list_head *page_list, enum df_reason reason);
int normal_pool_alloc_pages(unsigned long size, struct list_head *page_list, unsigned int *nr_pages);

void set_sgtable_to_page_list(struct sg_table *sg_table, struct list_head *list);

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_CMA)
int __init cma_dma_heap_init(void);
void cma_dma_heap_exit(void);
#else
static inline int __init cma_dma_heap_init(void)
{
	return 0;
}

#define cma_dma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_DMABUF_HEAPS_SAMSUNG_CARVEOUT)
int __init carveout_dma_heap_init(void);
void carveout_dma_heap_exit(void);
#else
static inline int __init carveout_dma_heap_init(void)
{
	return 0;
}

#define carveout_dma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_DMABUF_HEAPS_GOOGLE_GCMA)
int __init gcma_dma_heap_init(void);
void gcma_dma_heap_exit(void);
#else
static inline int __init gcma_dma_heap_init(void)
{
	return 0;
}

#define gcma_dma_heap_exit() do { } while (0)
#endif

#if defined(CONFIG_RBIN)
int __init rbin_dma_heap_init(void);
void rbin_dma_heap_exit(void);
#else
static inline int __init rbin_dma_heap_init(void)
{
	return 0;
}

#define rbin_dma_heap_exit() do { } while (0)
#endif

#ifdef CONFIG_DMABUF_HEAPS_CAMERAPOOL
int __init camerapool_dma_heap_init(void);
void camerapool_dma_heap_exit(void);
unsigned long camerapool_static_pool_size(void);
#endif

#define DMAHEAP_PREFIX "[Exynos][DMA-HEAP] "
#define perr(format, arg...) \
	pr_err(DMAHEAP_PREFIX format "\n", ##arg)

#define perrfn(format, arg...) \
	pr_err(DMAHEAP_PREFIX "%s: " format "\n", __func__, ##arg)

#define perrdev(dev, format, arg...) \
	dev_err(dev, DMAHEAP_PREFIX format "\n", ##arg)

#define perrfndev(dev, format, arg...) \
	dev_err(dev, DMAHEAP_PREFIX "%s: " format "\n", __func__, ##arg)

static inline void samsung_allocate_error_report(struct samsung_dma_heap *heap, unsigned long len,
						 unsigned long fd_flags, unsigned long heap_flags)
{
	perrfn("failed to alloc (len %zu, %#lx %#lx) from %s heap (total allocated %llu kb)",
	       len, fd_flags, heap_flags, heap->name, samsung_heap_total_kbsize(heap));
}

#endif /* _HEAP_PRIVATE_H */
