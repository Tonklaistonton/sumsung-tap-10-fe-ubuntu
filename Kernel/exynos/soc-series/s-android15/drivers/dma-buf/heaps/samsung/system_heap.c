// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF System heap exporter for Samsung
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019, 2020 Linaro Ltd.
 *
 * Portions based off of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "heap_private.h"
#include "deferred-free-helper.h"
#include "page_pool.h"

void system_heap_buffer_release(struct samsung_dma_buffer *buffer, enum df_reason reason)
{
	struct list_head page_list;

	if (reason == DF_NORMAL)
		heap_sgtable_pages_clean(&buffer->sg_table);

	set_sgtable_to_page_list(&buffer->sg_table, &page_list);
	normal_pool_free_pages(&page_list, reason);

	if (reason == DF_NORMAL)
		dma_heap_event_pool_record(DMA_HEAP_POOL_EVENT_ALLOC, 0, buffer->len,
					   buffer->len, get_normal_pool_size());

	sg_free_table(&buffer->sg_table);
}

int system_heap_buffer_allocate(struct samsung_dma_buffer *buffer)
{
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int ret, nr_pages;

	INIT_LIST_HEAD(&pages);

	ret = normal_pool_alloc_pages(buffer->len, &pages, &nr_pages);
	if (ret)
		return ret;

	if (sg_alloc_table(&buffer->sg_table, nr_pages, GFP_KERNEL)) {
		perrfn("failed to allocate sgtable (nents : %d)", nr_pages);
		return -ENOMEM;
	}

	sg = buffer->sg_table.sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	heap_cache_flush(buffer);

	return 0;
}

long system_heap_get_pool_size(void)
{
	return get_normal_pool_size() + get_total_secure_pool_size();
}

static const struct samsung_heap_ops system_core_heap_ops = {
	.allocate = system_heap_buffer_allocate,
	.release = system_heap_buffer_release,
	.get_pool_size = system_heap_get_pool_size,
};

static struct samsung_dma_heap system_heap = {
	.name = "system",
	.flags = DMA_HEAP_FLAG_DEFERRED_FREE,
	.ops = &system_core_heap_ops,
	.alignment = PAGE_SIZE,
};

static const struct samsung_heap_ops system_heap_ops = {
	.allocate = system_heap_buffer_allocate,
	.release = system_heap_buffer_release,
};

static struct samsung_dma_heap system_uncached_heap = {
	.name = "system-uncached",
	.flags = DMA_HEAP_FLAG_UNCACHED | DMA_HEAP_FLAG_DEFERRED_FREE,
	.ops = &system_heap_ops,
	.alignment = PAGE_SIZE,
};

static int system_heap_probe(struct platform_device *pdev)
{
	struct samsung_dma_heap *heap;

	heap = samsung_heap_create(&pdev->dev, 0);
	if (IS_ERR(heap))
		return PTR_ERR(heap);

	heap->ops = &system_heap_ops;
	return samsung_heap_add(heap);
}

static const struct of_device_id system_heap_of_match[] = {
	{ .compatible = "samsung,dma-heap-system", },
	{ },
};
MODULE_DEVICE_TABLE(of, system_heap_of_match);

static struct platform_driver system_heap_driver = {
	.driver		= {
		.name	= "samsung,dma-heap-system",
		.of_match_table = system_heap_of_match,
	},
	.probe		= system_heap_probe,
};

int __init system_dma_heap_init(void)
{
	int ret;

	ret = samsung_heap_add(&system_heap);
	if (ret)
		return ret;

	ret = samsung_heap_add(&system_uncached_heap);
	if (ret)
		return ret;

	return platform_driver_register(&system_heap_driver);
}

void system_dma_heap_exit(void)
{
	platform_driver_unregister(&system_heap_driver);
}
