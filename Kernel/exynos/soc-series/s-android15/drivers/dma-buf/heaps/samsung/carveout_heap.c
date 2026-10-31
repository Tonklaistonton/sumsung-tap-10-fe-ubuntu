// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Carveout heap exporter for Samsung
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "heap_private.h"

struct carveout_heap {
	struct gen_pool *pool;
	struct reserved_mem *rmem;
};

static int carveout_heap_buffer_allocate(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct carveout_heap *carveout_heap = heap->priv;
	struct page *pages;
	unsigned long size = buffer->len;
	phys_addr_t paddr;
	int ret;

	paddr = gen_pool_alloc(carveout_heap->pool, size);
	if (!paddr) {
		perrfn("failed to allocate from %s, size %lu", carveout_heap->rmem->name, size);
		return -ENOMEM;
	}

	pages = phys_to_page(paddr);

	if (sg_alloc_table(&buffer->sg_table, 1, GFP_KERNEL)) {
		perrfn("failed to allocate sgtable");

		ret = -ENOMEM;
		goto err_alloc_table;
	}

	sg_set_page(buffer->sg_table.sgl, pages, size, 0);
	heap_page_clean(pages, size);
	heap_cache_flush(buffer);

	if (dma_heap_flags_protected(heap->flags)) {
	       buffer->priv = samsung_dma_buffer_protect(heap, size, paddr);

		if (IS_ERR(buffer->priv)) {
			ret = PTR_ERR(buffer->priv);
			goto err_prot;
		}
	}

	return 0;

err_prot:
	sg_free_table(&buffer->sg_table);
err_alloc_table:
	gen_pool_free(carveout_heap->pool, paddr, size);

	return ret;
}

static void carveout_heap_buffer_release(struct samsung_dma_buffer *buffer, enum df_reason reason)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct carveout_heap *carveout_heap = heap->priv;
	dma_addr_t paddr = sg_phys(buffer->sg_table.sgl);
	int ret = 0;

	if (dma_heap_flags_protected(buffer->flags))
		ret = samsung_dma_buffer_unprotect(heap, buffer->len, paddr, buffer->priv);

	if (!ret)
		gen_pool_free(carveout_heap->pool, paddr, buffer->len);

	sg_free_table(&buffer->sg_table);
}

static void carveout_reserved_free(struct reserved_mem *rmem)
{
	struct page *first = phys_to_page(rmem->base & PAGE_MASK);
	struct page *last = phys_to_page(PAGE_ALIGN(rmem->base + rmem->size));
	struct page *page;

	for (page = first; page != last; page++)
		free_reserved_page(page);
}

static const struct samsung_heap_ops carveout_heap_ops = {
	.allocate = carveout_heap_buffer_allocate,
	.release = carveout_heap_buffer_release,
};

static int carveout_heap_probe(struct platform_device *pdev)
{
	struct samsung_dma_heap *heap;
	struct carveout_heap *carveout_heap;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	int ret = -ENOMEM;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!rmem_np) {
		perrdev(&pdev->dev, "memory-region handle not found");
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		perrdev(&pdev->dev, "memory-region handle not found");
		return -ENODEV;
	}

	carveout_heap = devm_kzalloc(&pdev->dev, sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		goto err_probe;

	carveout_heap->rmem = rmem;
	carveout_heap->pool = devm_gen_pool_create(&pdev->dev, PAGE_SHIFT, -1, 0);
	if (!carveout_heap->pool)
		goto err_probe;

	ret = gen_pool_add(carveout_heap->pool, rmem->base, rmem->size, -1);
	if (ret)
		goto err_probe;

	heap = samsung_heap_create(&pdev->dev, carveout_heap);
	if (IS_ERR(heap)) {
		ret = PTR_ERR(heap);
		goto err_probe;
	}

	heap->ops = &carveout_heap_ops;
	ret = samsung_heap_add(heap);
err_probe:
	if (ret)
		carveout_reserved_free(rmem);

	return ret;
}

static const struct of_device_id carveout_heap_of_match[] = {
	{ .compatible = "samsung,dma-heap-carveout", },
	{ },
};
MODULE_DEVICE_TABLE(of, carveout_heap_of_match);

static struct platform_driver carveout_heap_driver = {
	.driver		= {
		.name	= "samsung,dma-heap-carveout",
		.of_match_table = carveout_heap_of_match,
	},
	.probe		= carveout_heap_probe,
};

int __init carveout_dma_heap_init(void)
{
	return platform_driver_register(&carveout_heap_driver);
}

void carveout_dma_heap_exit(void)
{
	platform_driver_unregister(&carveout_heap_driver);
}
