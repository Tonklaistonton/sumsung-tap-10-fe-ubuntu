// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF GCMA heap
 *
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
#include <linux/pfn.h>
#include <soc/samsung/exynos/gcma.h>

#include "heap_private.h"

#include "gcma_heap.h"
#include "gcma_heap_sysfs.h"

static int gcma_heap_allocate(struct samsung_dma_buffer *buffer)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct gcma_heap *gcma_heap = heap->priv;
	unsigned long size = buffer->len;
	struct page *pages;
	unsigned int alignment = heap->alignment;
	phys_addr_t paddr;
	unsigned long pfn;
	int ret;

	size = ALIGN(size, alignment);

	paddr = gen_pool_alloc(gcma_heap->pool, size);
	if (!paddr) {
		perrfn("failed to allocate from GCMA, size %lu", size);
		return -ENOMEM;
	}

	pfn = PFN_DOWN(paddr);
	gcma_alloc_range(pfn, pfn + (size >> PAGE_SHIFT) - 1);

	if (sg_alloc_table(&buffer->sg_table, 1, GFP_KERNEL)) {
		perrfn("failed to allocate sgtable");
		ret = -ENOMEM;
		goto err_alloc_table;
	}

	pages = phys_to_page(paddr);
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
	gcma_free_range(pfn, pfn + (size >> PAGE_SHIFT) - 1);
	gen_pool_free(gcma_heap->pool, paddr, size);

	pr_err("failed to allocate from %s heap, size %lu ret %d",
	       heap->name, size, ret);

	return ret;
}

static void gcma_heap_release(struct samsung_dma_buffer *buffer, enum df_reason reason)
{
	struct samsung_dma_heap *heap = buffer->heap;
	struct gcma_heap *gcma_heap = heap->priv;
	struct page *page = sg_page(buffer->sg_table.sgl);
	int ret = 0;
	unsigned long pfn;

	if (dma_heap_flags_protected(heap->flags))
		ret = samsung_dma_buffer_unprotect(heap, buffer->len, page_to_phys(page),
						   buffer->priv);

	pfn = PFN_DOWN(sg_phys(buffer->sg_table.sgl));
	gcma_free_range(pfn, pfn + (buffer->len >> PAGE_SHIFT) - 1);

	if (!ret)
		gen_pool_free(gcma_heap->pool, sg_phys(buffer->sg_table.sgl), buffer->len);

	sg_free_table(&buffer->sg_table);
}

static const struct samsung_heap_ops gcma_heap_ops = {
	.allocate = gcma_heap_allocate,
	.release = gcma_heap_release,
};

static int gcma_heap_probe(struct platform_device *pdev)
{
	struct samsung_dma_heap *heap;
	struct gcma_heap *gcma_heap;
	struct reserved_mem *rmem;
	struct device_node *rmem_np;
	int ret;

	rmem_np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!rmem_np)
		return -ENODEV;

	rmem = of_reserved_mem_lookup(rmem_np);
	if (!rmem) {
		perrdev(&pdev->dev, "memory-region handle not found");
		return -ENODEV;
	}

	gcma_heap = devm_kzalloc(&pdev->dev, sizeof(*gcma_heap), GFP_KERNEL);
	if (!gcma_heap)
		return -ENOMEM;

	ret = register_gcma_area(rmem->name, rmem->base, rmem->size);
	if (ret)
		return ret;

	gcma_heap->pool = devm_gen_pool_create(&pdev->dev, PAGE_SHIFT, -1, 0);
	if (!gcma_heap->pool)
		return -ENOMEM;

	ret = gen_pool_add(gcma_heap->pool, rmem->base, rmem->size, -1);
	if (ret)
		return ret;

	heap = samsung_heap_create(&pdev->dev, gcma_heap);
	if (IS_ERR(heap))
		return PTR_ERR(heap);

	heap->ops = &gcma_heap_ops;
	register_heap_sysfs(gcma_heap, pdev->name);

	return samsung_heap_add(heap);
}

static const struct of_device_id gcma_heap_of_match[] = {
	{ .compatible = "google,dma-heap-gcma", },
	{ },
};
MODULE_DEVICE_TABLE(of, gcma_heap_of_match);

static struct platform_driver gcma_heap_driver = {
	.driver		= {
		.name	= "google,dma-heap-gcma",
		.of_match_table = gcma_heap_of_match,
	},
	.probe		= gcma_heap_probe,
};

int __init gcma_dma_heap_init(void)
{
	gcma_heap_sysfs_init();
	return platform_driver_register(&gcma_heap_driver);
}

void gcma_dma_heap_exit(void)
{
	platform_driver_unregister(&gcma_heap_driver);
}
