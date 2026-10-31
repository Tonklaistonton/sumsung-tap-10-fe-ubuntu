// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung page pool system
 *
 * Based on the ION page pool code
 * Copyright (C) 2020 Linaro Ltd.
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/list.h>
#include <linux/shrinker.h>
#include <linux/spinlock.h>
#include <linux/swap.h>
#include <linux/sched/signal.h>

#include "page_pool.h"
#include "heap_private.h"

struct dmabuf_page_pool *normal_pools[NUM_ORDERS];

long get_normal_pool_size(void)
{
	long size = 0;
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		size += dmabuf_page_pool_get_size(normal_pools[i]);

	return size;
}

static void free_page_to_pool(struct page *page)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (compound_order(page) == orders[i])
			break;
	}
	dmabuf_page_pool_free(normal_pools[i], page);
}

void normal_pool_free_pages(struct list_head *page_list, enum df_reason reason)
{
	struct page *page, *tmp_page;
	unsigned int size = 0;

	list_for_each_entry_safe(page, tmp_page, page_list, lru) {
		if (reason == DF_UNDER_PRESSURE) {
			__free_pages(page, compound_order(page));
		} else {
			size += page_size(page);
			free_page_to_pool(page);
		}
	}

	if (reason != DF_UNDER_PRESSURE)
		dma_heap_event_pool_record(DMA_HEAP_POOL_EVENT_FREE, 0, size,
					   size, get_normal_pool_size());
}

static struct page *alloc_largest_available(unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = dmabuf_page_pool_alloc(normal_pools[i]);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

#define DMA_HEAP_ALLOC_MAX	(totalram_pages() >> 1)
int normal_pool_alloc_pages(unsigned long size, struct list_head *page_list, unsigned int *nr_pages)
{
	struct list_head exception_pages;
	struct page *page, *tmp_page;
	long pool_before, pool_after;
	unsigned long size_remaining = size;
	unsigned int max_order = orders[0];
	int ret, i = 0;

	dma_heap_event_begin();

	if (size / PAGE_SIZE > DMA_HEAP_ALLOC_MAX) {
		perrfn("Requested size %zu is too large, it should be under %ld",
		       size, DMA_HEAP_ALLOC_MAX << PAGE_SHIFT);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&exception_pages);

	pool_before = get_normal_pool_size();

	while (size_remaining > 0) {
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current)) {
			perrfn("Fatal signal pending pid #%d", current->pid);
			ret = -EINTR;
			goto free_buffer;
		}

		page = alloc_largest_available(size_remaining, max_order);
		if (!page) {
			ret = fatal_signal_pending(current) ? -EINTR : -ENOMEM;
			perrfn("Failed to allocate page (ret %d)", ret);
			goto free_buffer;
		}

		if (is_dma_heap_exception_page(page)) {
			list_add_tail(&page->lru, &exception_pages);
		} else {
			list_add_tail(&page->lru, page_list);
			size_remaining -= page_size(page);
			i++;
		}
		max_order = compound_order(page);
	}
	list_for_each_entry_safe(page, tmp_page, &exception_pages, lru)
		__free_pages(page, compound_order(page));

	__dma_heap_event_record(DMA_HEAP_EVENT_PAGE_POOL_ALLOC, "normal_page_pool", size, begin);

	*nr_pages = i;

	pool_after = get_normal_pool_size();

	dma_heap_event_pool_record(DMA_HEAP_POOL_EVENT_ALLOC, 0, size,
				   pool_after - pool_before, pool_after);

	return 0;

free_buffer:
	list_for_each_entry_safe(page, tmp_page, &exception_pages, lru)
		__free_pages(page, compound_order(page));

	list_for_each_entry_safe(page, tmp_page, page_list, lru)
		__free_pages(page, compound_order(page));

	return ret;
}

static inline void secure_page_pool_free_pages(struct secure_page_pool *pool,
					       struct page *page)
{
	__free_pages(page, pool->order);
}

static void secure_page_pool_add(struct secure_page_pool *pool, struct page *page)
{
	spin_lock(&pool->lock);
	list_add_tail(&page->lru, &pool->items);
	pool->count++;
	spin_unlock(&pool->lock);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    1 << pool->order);
}

struct page *secure_page_pool_remove(struct secure_page_pool *pool)
{
	struct page *page;

	spin_lock(&pool->lock);
	page = list_first_entry_or_null(&pool->items, struct page, lru);
	if (page) {
		pool->count--;
		list_del(&page->lru);
		spin_unlock(&pool->lock);
		mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
				    -(1 << pool->order));
		goto out;
	}
	spin_unlock(&pool->lock);
out:
	return page;
}

struct page *secure_page_pool_alloc(struct secure_page_pool *pool)
{
	if (WARN_ON(!pool))
		return NULL;

	return secure_page_pool_remove(pool);
}

void secure_page_pool_free(struct secure_page_pool *pool, struct page *page)
{
	if (WARN_ON(pool->order != compound_order(page)))
		return;

	secure_page_pool_add(pool, page);
}

static int secure_page_pool_total(struct secure_page_pool *pool)
{
	return pool->count << pool->order;
}

static struct secure_page_pool *secure_page_pool_create(void)
{
	struct secure_page_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool)
		return NULL;

	pool->count = 0;
	INIT_LIST_HEAD(&pool->items);
	spin_lock_init(&pool->lock);

	return pool;
}

static void secure_page_pool_destroy(struct secure_page_pool *pool)
{
	struct page *page;

	/* Free any remaining pages in the pool */
	while ((page = secure_page_pool_remove(pool)))
		secure_page_pool_free_pages(pool, page);

	kfree(pool);
}

static unsigned long get_secure_page_pool_size(struct secure_page_pool *pool)
{
       unsigned long num_pages = 0;

       spin_lock(&pool->lock);
       num_pages = secure_page_pool_total(pool);
       spin_unlock(&pool->lock);

       return num_pages * PAGE_SIZE;
}

#define MAX_SECURE_HEAP 16

struct secure_pool_info secure_pool_infos[MAX_SECURE_HEAP];
unsigned char num_secure_heap;
rwlock_t secure_pool_info_lock;

void secure_pool_set_prefetch_min(struct secure_pool_info *info, unsigned int min)
{
	write_lock(&secure_pool_info_lock);
	info->min = min;
	write_unlock(&secure_pool_info_lock);
}

long get_secure_pool_size(struct secure_page_pool **pools)
{
	long size = 0;
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		size += get_secure_page_pool_size(pools[i]);

	return size;
}

struct secure_pool_info *get_secure_pool_info(unsigned int protid)
{
	int i;

	read_lock(&secure_pool_info_lock);
	for (i = 0; i < num_secure_heap; i++) {
		if (protid == secure_pool_infos[i].protid) {
			read_unlock(&secure_pool_info_lock);
			return &secure_pool_infos[i];
		}
	}
	read_unlock(&secure_pool_info_lock);
	return NULL;
}

int add_secure_pool_info(const char *name, unsigned int protid)
{
	int i, ret = 0;
	struct secure_pool_info *info;

	write_lock(&secure_pool_info_lock);
	if (num_secure_heap == MAX_SECURE_HEAP) {
		perrfn("The secure heap protid exceeds %d", MAX_SECURE_HEAP);
		ret = -EINVAL;
		goto err_add_heap;
	}

	info = &secure_pool_infos[num_secure_heap];

	info->name = name;
	info->protid = protid;

	for (i = 0; i < NUM_ORDERS; i++) {
		info->pools[i] = secure_page_pool_create();
		if (!info->pools[i]) {
			int j;

			for (j = 0; j < i; j++)
				secure_page_pool_destroy(info->pools[i]);

			ret = -ENOMEM;
			goto err_add_heap;
		}
		info->pools[i]->order = orders[i];
		info->pools[i]->protid = protid;
	}
	num_secure_heap++;

err_add_heap:
	write_unlock(&secure_pool_info_lock);

	return ret;
}

long get_total_secure_pool_size(void)
{
	int i;
	long size = 0;

	read_lock(&secure_pool_info_lock);
	for (i = 0; i < num_secure_heap; i++) {
		size += get_secure_pool_size(secure_pool_infos[i].pools);
	}
	read_unlock(&secure_pool_info_lock);

	return size;
}

long get_samsung_pool_size(struct dma_heap *dma_heap)
{
	struct samsung_dma_heap *heap = dma_heap_get_drvdata(dma_heap);

	if (heap->ops->get_pool_size)
		return heap->ops->get_pool_size();

	return 0;
}

static int secure_page_pool_do_shrink(struct secure_page_pool *pool, int nr_to_scan)
{
	struct list_head page_list;
	int freed = 0;

	INIT_LIST_HEAD(&page_list);
	while (freed < nr_to_scan) {
		struct page *page;

		page = secure_page_pool_remove(pool);
		if (!page)
			break;

		list_add_tail(&page->lru, &page_list);
		freed += (1 << pool->order);
	}

	if (!freed)
		return 0;

	if (samsung_dma_buffer_hvc_multi(HVC_DRM_TZMP2_UNPROT, &page_list, pool->protid))
		return 0;

	normal_pool_free_pages(&page_list, DF_UNDER_PRESSURE);

	return freed;
}

static int secure_page_pools_shrink(int nr_to_scan)
{
	long nr_total = 0;
	long nr_freed;
	int i, j;

	read_lock(&secure_pool_info_lock);
	for (i = 0; i < num_secure_heap; i++) {
		struct secure_pool_info *info = &secure_pool_infos[i];

		if (info->min > get_secure_pool_size(info->pools))
			continue;

		for (j = 0; j < NUM_ORDERS; j++) {
			nr_freed = secure_page_pool_do_shrink(info->pools[j], nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
		dma_heap_event_pool_record(DMA_HEAP_POOL_EVENT_SHRINK, info->protid, 0,
					   -(nr_freed << PAGE_SHIFT),
					   get_secure_pool_size(info->pools));

		if (nr_to_scan <= 0)
			break;
	}
	read_unlock(&secure_pool_info_lock);

	return nr_total;
}

static unsigned long secure_page_pools_shrink_count(struct shrinker *shrinker,
						   struct shrink_control *sc)
{
	long nr_total = 0;
	int i;

	read_lock(&secure_pool_info_lock);

	for (i = 0; i < num_secure_heap; i++)
		nr_total += get_secure_pool_size(secure_pool_infos[i].pools);

	read_unlock(&secure_pool_info_lock);

	return (nr_total) ? nr_total >> PAGE_SHIFT : SHRINK_EMPTY;
}

static unsigned long secure_page_pools_shrink_scan(struct shrinker *shrinker,
						  struct shrink_control *sc)
{
	long nr_total;

	if (sc->nr_to_scan == 0)
		return 0;

	nr_total = secure_page_pools_shrink(sc->nr_to_scan);

	return (nr_total) ? nr_total : SHRINK_STOP;
}

struct shrinker secure_pool_shrinker = {
	.count_objects = secure_page_pools_shrink_count,
	.scan_objects = secure_page_pools_shrink_scan,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};

static int secure_page_pool_init_shrinker(void)
{
	return register_shrinker(&secure_pool_shrinker, "secure-page-pool-shrinker");
}

#define HIGH_ORDER_GFP  (((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN \
				| __GFP_NORETRY) & ~__GFP_RECLAIM) \
				| __GFP_COMP)
#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP)
static gfp_t order_flags[] = {HIGH_ORDER_GFP, HIGH_ORDER_GFP, HIGH_ORDER_GFP, LOW_ORDER_GFP};

int samsung_page_pool_init(void)
{
	int i;

	rwlock_init(&secure_pool_info_lock);

	for (i = 0; i < NUM_ORDERS; i++) {
		normal_pools[i] = dmabuf_page_pool_create(order_flags[i], orders[i]);
		if (!normal_pools[i]) {
			pr_err("%s: page pool creation failed!\n", __func__);
			samsung_page_pool_destroy();
			return -ENOMEM;
		}
	}

	return secure_page_pool_init_shrinker();
}

void samsung_page_pool_destroy(void)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (normal_pools[i])
			dmabuf_page_pool_destroy(normal_pools[i]);
	}
}
