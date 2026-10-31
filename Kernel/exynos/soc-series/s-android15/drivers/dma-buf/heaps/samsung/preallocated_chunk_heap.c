// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Preallocated Chunk heap exporter for Samsung
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 * Author: <yong.t.kim@samsung.com> for Samsung
 */

#include <linux/cma.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-direct.h>
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
#include "secure_buffer.h"

struct preallocated_heap {
	struct cma *cma;
	struct gen_pool *pool;
	struct reserved_mem *rmem;
	struct mutex prealloc_lock;
	struct page *pages;
	void *priv;
	int alloc_count;
	unsigned long alloc_size;
};

static void preallocated_chunk_heap_region_flush(struct device *dev, struct page *page, size_t size)
{
	dma_map_single(dev, page_to_virt(page), size, DMA_TO_DEVICE);
	dma_unmap_single(dev, phys_to_dma(dev, page_to_phys(page)), size, DMA_FROM_DEVICE);
}

static int preallocated_chunk_heap_init(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct preallocated_heap *preallocated_heap = heap->priv;
	unsigned long heap_size = preallocated_heap->alloc_size;
	unsigned long nr_pages = heap_size >> PAGE_SHIFT;
	int ret = -ENOMEM;

	preallocated_heap->pages = cma_alloc(preallocated_heap->cma, nr_pages,
					     get_order(heap->alignment), 0);
	if (!preallocated_heap->pages) {
		perrfn("failed to allocate preallocated chunk heap from %s, size %lu",
		       dma_heap_get_name(heap->dma_heap), heap_size);
		goto err_preallocated_alloc;
	}

	heap_page_clean(preallocated_heap->pages, heap_size);
	preallocated_chunk_heap_region_flush(dma_heap_get_dev(heap->dma_heap),
					     preallocated_heap->pages, heap_size);

	preallocated_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!preallocated_heap->pool) {
		perrfn("failed to create pool");
		goto err_create_genpool;
	}

	ret = gen_pool_add(preallocated_heap->pool, page_to_phys(preallocated_heap->pages),
			   heap_size, -1);
	if (ret) {
		perrfn("failed to add pool from %s, base %lx size %lu",
		       dma_heap_get_name(heap->dma_heap),
		       (unsigned long)page_to_phys(preallocated_heap->pages), heap_size);
		goto err_add_genpool;
	}

	if (dma_heap_flags_protected(heap->flags)) {
		void *priv = samsung_dma_buffer_protect(heap, heap_size,
							page_to_phys(preallocated_heap->pages));
		if (IS_ERR(priv)) {
			ret = PTR_ERR(priv);
			goto err_add_genpool;
		}
		preallocated_heap->priv = priv;
	}

	return 0;

err_add_genpool:
	gen_pool_destroy(preallocated_heap->pool);
err_create_genpool:
	cma_release(preallocated_heap->cma, preallocated_heap->pages, nr_pages);
err_preallocated_alloc:
	return ret;
}

static void preallocated_chunk_heap_deinit(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct preallocated_heap *preallocated_heap = heap->priv;
	unsigned long heap_size = preallocated_heap->alloc_size;
	int protret = 0;

	if (dma_heap_flags_protected(buffer->flags))
		protret = samsung_dma_buffer_unprotect(heap, heap_size,
						       page_to_phys(preallocated_heap->pages),
						       preallocated_heap->priv);
	gen_pool_destroy(preallocated_heap->pool);

	if (!protret)
		cma_release(preallocated_heap->cma, preallocated_heap->pages,
			    heap_size >> PAGE_SHIFT);
}

static struct page *allocate_from_genpool(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct preallocated_heap *preallocated_heap = heap->priv;
	struct page *alloc_pages;
	phys_addr_t paddr;
	unsigned long size = buffer->len;

	paddr = gen_pool_alloc(preallocated_heap->pool, size);
	if (!paddr) {
		perrfn("failed to allocate pool from %s, size %lu",
		       dma_heap_get_name(heap->dma_heap), size);
		return NULL;
	}
	alloc_pages = phys_to_page(paddr);

	if (sg_alloc_table(&buffer->sg_table, 1, GFP_KERNEL)) {
		perrfn("failed to allocate sgtable");

		gen_pool_free(preallocated_heap->pool, paddr, size);
		return NULL;
	}

	sg_set_page(buffer->sg_table.sgl, alloc_pages, size, 0);

	return alloc_pages;
}

static int preallocated_chunk_heap_buffer_allocate(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct preallocated_heap *preallocated_heap = heap->priv;
	struct page *alloc_pages;
	int ret;

	mutex_lock(&preallocated_heap->prealloc_lock);
	if (++preallocated_heap->alloc_count == 1) {
		ret = preallocated_chunk_heap_init(buffer);
		if (ret)
			goto err_preallocated_init;
	}

	alloc_pages = allocate_from_genpool(buffer);
	if (!alloc_pages) {
		ret = -ENOMEM;
		goto err_alloc_pages;
	}

	if (dma_heap_flags_protected(heap->flags)) {
		size_t offset = page_to_phys(alloc_pages) - page_to_phys(preallocated_heap->pages);

		buffer->priv = samsung_dma_buffer_copy_offset_info(heap, preallocated_heap->priv, offset);
		if (IS_ERR(buffer->priv)) {
			ret = PTR_ERR(buffer->priv);
			goto err_copy_protect;
		}
	}

	mutex_unlock(&preallocated_heap->prealloc_lock);
	return 0;

err_copy_protect:
	gen_pool_free(preallocated_heap->pool, sg_phys(buffer->sg_table.sgl), buffer->len);
err_alloc_pages:
	if (preallocated_heap->alloc_count == 1)
		preallocated_chunk_heap_deinit(buffer);
err_preallocated_init:
	preallocated_heap->alloc_count--;
	mutex_unlock(&preallocated_heap->prealloc_lock);
	return ret;
}

static void preallocated_chunk_heap_buffer_release(struct samsung_dma_buffer *buffer,
						   enum df_reason reason)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct preallocated_heap *preallocated_heap = heap->priv;

	mutex_lock(&preallocated_heap->prealloc_lock);

	if (dma_heap_flags_protected(heap->flags))
		samsung_dma_buffer_release_copied_info(buffer->priv);

	gen_pool_free(preallocated_heap->pool, sg_phys(buffer->sg_table.sgl), buffer->len);

	if (--preallocated_heap->alloc_count == 0)
		preallocated_chunk_heap_deinit(buffer);

	sg_free_table(&buffer->sg_table);

	mutex_unlock(&preallocated_heap->prealloc_lock);
}

static void rmem_remove_callback(void *p)
{
	of_reserved_mem_device_release((struct device *)p);
}

static const struct samsung_heap_ops preallocated_chunk_heap_ops = {
	.allocate = preallocated_chunk_heap_buffer_allocate,
	.release = preallocated_chunk_heap_buffer_release,
};

static int preallocated_chunk_heap_probe(struct platform_device *pdev)
{
	struct samsung_dma_heap *heap;
	struct preallocated_heap *preallocated_heap;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	int ret;
	unsigned int alloc_size;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!rmem_np) {
		perrdev(&pdev->dev, "no memory-region handle");
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		perrdev(&pdev->dev, "memory-region handle not found");
		return -ENODEV;
	}

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret || !pdev->dev.cma_area) {
		dev_err(&pdev->dev, "The CMA reserved area is not assigned (ret %d)\n", ret);
		return -EINVAL;
	}

	ret = devm_add_action(&pdev->dev, rmem_remove_callback, &pdev->dev);
	if (ret) {
		of_reserved_mem_device_release(&pdev->dev);
		return ret;
	}

	preallocated_heap = devm_kzalloc(&pdev->dev, sizeof(*preallocated_heap), GFP_KERNEL);
	if (!preallocated_heap)
		return -ENOMEM;

	ret = of_property_read_u32_index(pdev->dev.of_node, "dma-heap,alloc-size", 0, &alloc_size);
	if (ret)
		alloc_size = rmem->size;

	preallocated_heap->alloc_size = alloc_size;
	preallocated_heap->cma = pdev->dev.cma_area;
	preallocated_heap->rmem = rmem;
	mutex_init(&preallocated_heap->prealloc_lock);

	heap = samsung_heap_create(&pdev->dev, preallocated_heap);
	if (IS_ERR(heap))
		return PTR_ERR(heap);

	heap->ops = &preallocated_chunk_heap_ops;
	return samsung_heap_add(heap);
}

static const struct of_device_id preallocated_chunk_heap_of_match[] = {
	{ .compatible = "samsung,dma-heap-preallocated-chunk", },
	{ },
};
MODULE_DEVICE_TABLE(of, preallocated_chunk_heap_of_match);

static struct platform_driver preallocated_chunk_heap_driver = {
	.driver		= {
		.name	= "samsung,dma-heap-preallocated-chunk",
		.of_match_table = preallocated_chunk_heap_of_match,
	},
	.probe		= preallocated_chunk_heap_probe,
};

int __init preallocated_chunk_dma_heap_init(void)
{
	return platform_driver_register(&preallocated_chunk_heap_driver);
}

void preallocated_chunk_dma_heap_exit(void)
{
	platform_driver_unregister(&preallocated_chunk_heap_driver);
}
