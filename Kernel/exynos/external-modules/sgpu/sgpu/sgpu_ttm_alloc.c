// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <drm/ttm/ttm_tt.h>
#include <drm/ttm/ttm_bo.h>

#include "amdgpu.h"

static atomic_t sgpu_page_alloc_count[NR_PAGE_ORDERS];

static void sgpu_free_pages(struct page **pages, unsigned long nr_pages)
{
	unsigned long freed_pages = 0;

	while (nr_pages != freed_pages) {
		struct page *p = *pages;
		unsigned long pgcount = 1 << p->private;

		atomic_dec(&sgpu_page_alloc_count[p->private]);
		__free_pages(p, p->private);
		pages += pgcount;
		freed_pages += pgcount;
	}

	WARN_ON(nr_pages != freed_pages);
}

int sgpu_ttm_page_alloc(struct ttm_tt *tt)
{
	unsigned int nr_remained = tt->num_pages;
	struct page **pages = tt->pages;
	gfp_t gfp_flags = (GFP_HIGHUSER_MOVABLE & ~__GFP_RECLAIM) | __GFP_NOMEMALLOC |
			  __GFP_NORETRY | __GFP_NOWARN | __GFP_RETRY_MAYFAIL;
	int order = min_t(int, MAX_ORDER, __fls(nr_remained));
	struct page *p;

	if (WARN_ON(!nr_remained || ttm_tt_is_populated(tt)))
		return -EINVAL;

	if (tt->page_flags & TTM_TT_FLAG_ZERO_ALLOC)
		gfp_flags |= __GFP_ZERO;

	for (order = min_t(int, MAX_ORDER, __fls(nr_remained));
	     nr_remained > 0 && order >= 0; order--) {
		unsigned int nr_pages = 1 << order;

		if (0 == order) {
			gfp_flags = GFP_HIGHUSER_MOVABLE;
			if (tt->page_flags & TTM_TT_FLAG_ZERO_ALLOC)
				gfp_flags |= __GFP_ZERO;
		}

		while (nr_remained >= nr_pages) {
			unsigned int i;

			p = alloc_pages(gfp_flags, order);
			if (!p)
				break;

			p->private = order;
			for (i = 0; i < nr_pages; i++)
				pages[i] = &p[i];

			nr_remained -= nr_pages;
			pages += nr_pages;
			atomic_inc(&sgpu_page_alloc_count[order]);
		}
	}

	if (nr_remained != 0) {
		pr_err("%s: failed to alloc %#x pages (%#x left)\n", __func__,
		       tt->num_pages, nr_remained);
		sgpu_free_pages(tt->pages, tt->num_pages - nr_remained);
		return -ENOMEM;
	} else if (tt->dma_address) {
		int i;

		for (i = 0; i < tt->num_pages; i++) {
			tt->dma_address[i] = page_to_phys(tt->pages[i]);
		}
	}

	return 0;
}

void sgpu_ttm_free_pages(struct ttm_tt *tt)
{
	sgpu_free_pages(tt->pages, tt->num_pages);
}

#ifdef CONFIG_DEBUG_FS
static int sgpu_page_pool_stat_show(struct seq_file *m, void *unused)
{
	size_t total = 0;
	unsigned int i;

	seq_puts(m, "ORDER:");
	for (i = 0; i < NR_PAGE_ORDERS; i++)
		seq_printf(m, " %8u", i);
	seq_puts(m, "\nCOUNT:");
	for (i = 0; i < NR_PAGE_ORDERS; i++) {
		unsigned int count = atomic_read(&sgpu_page_alloc_count[i]);
		total += count << i;
		seq_printf(m, " %8u", count);
	}
	seq_printf(m, "\nTOTAL: %zu\n", total);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(sgpu_page_pool_stat);

static struct dentry *debugfs_page_alloc;
void sgpu_debugfs_pagealloc_init(struct amdgpu_device *adev)
{
	debugfs_page_alloc = debugfs_create_file("page_alloc_stat", 0440,
				    adev_to_drm(adev)->render->debugfs_root,
				    adev, &sgpu_page_pool_stat_fops);
	if (debugfs_page_alloc)
		dev_err(adev->dev, "Failed to create 'page_alloc_stat' debugfs file\n");
}

#endif /* CONFIG_DEBUGFS */
