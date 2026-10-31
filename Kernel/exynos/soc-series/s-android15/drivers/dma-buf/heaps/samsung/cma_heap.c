// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF CMA heap exporter for Samsung
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
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

struct cma_heap {
	struct cma *cma;
};

static int cma_heap_buffer_allocate(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct cma_heap *cma_heap = heap->priv;
	unsigned long size = buffer->len;
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned int alignment = heap->alignment;
	struct page *pages;
	int ret;

	pages = cma_alloc(cma_heap->cma, nr_pages, get_order(alignment), 0);
	if (!pages) {
		perrfn("failed to allocate from %s, size %lu", dma_heap_get_name(heap->dma_heap), size);
		return -ENOMEM;
	}

	if (sg_alloc_table(&buffer->sg_table, 1, GFP_KERNEL)) {
		perrfn("failed to allocate sgtable");

		ret = -ENOMEM;
		goto err_alloc_table;
	}

	sg_set_page(buffer->sg_table.sgl, pages, size, 0);
	heap_page_clean(pages, size);
	heap_cache_flush(buffer);

	if (dma_heap_flags_protected(heap->flags)) {
		buffer->priv = samsung_dma_buffer_protect(heap, size, page_to_phys(pages));
		if (IS_ERR(buffer->priv)) {
			ret = PTR_ERR(buffer->priv);
			goto err_prot;
		}
	}

	return 0;

err_prot:
	sg_free_table(&buffer->sg_table);
err_alloc_table:
	cma_release(cma_heap->cma, pages, nr_pages);

	return ret;
}

static void cma_heap_buffer_release(struct samsung_dma_buffer *buffer, enum df_reason reason)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct cma_heap *cma_heap = heap->priv;
	unsigned long nr_pages = buffer->len >> PAGE_SHIFT;
	struct page *page = sg_page(buffer->sg_table.sgl);
	int protret = 0;

	if (dma_heap_flags_protected(heap->flags))
		protret = samsung_dma_buffer_unprotect(heap, buffer->len,
						       page_to_phys(page), buffer->priv);

	if (!protret)
		cma_release(cma_heap->cma, page, nr_pages);

	sg_free_table(&buffer->sg_table);
}

static void rmem_remove_callback(void *p)
{
	of_reserved_mem_device_release((struct device *)p);
}

static const struct samsung_heap_ops cma_heap_ops = {
	.allocate = cma_heap_buffer_allocate,
	.release = cma_heap_buffer_release,
};

static int cma_heap_probe(struct platform_device *pdev)
{
	struct samsung_dma_heap *heap;
	struct cma_heap *cma_heap;
	int ret;

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

	cma_heap = devm_kzalloc(&pdev->dev, sizeof(*cma_heap), GFP_KERNEL);
	if (!cma_heap)
		return -ENOMEM;

	cma_heap->cma = pdev->dev.cma_area;

	heap = samsung_heap_create(&pdev->dev, cma_heap);
	if (IS_ERR(heap))
		return PTR_ERR(heap);

	heap->ops = &cma_heap_ops;
	return samsung_heap_add(heap);
}

static const struct of_device_id cma_heap_of_match[] = {
	{ .compatible = "samsung,dma-heap-cma", },
	{ },
};
MODULE_DEVICE_TABLE(of, cma_heap_of_match);

static struct platform_driver cma_heap_driver = {
	.driver		= {
		.name	= "samsung,dma-heap-cma",
		.of_match_table = cma_heap_of_match,
	},
	.probe		= cma_heap_probe,
};

int __init cma_dma_heap_init(void)
{
	return platform_driver_register(&cma_heap_driver);
}

void cma_dma_heap_exit(void)
{
	platform_driver_unregister(&cma_heap_driver);
}
