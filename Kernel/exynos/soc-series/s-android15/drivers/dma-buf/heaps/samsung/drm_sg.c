// SPDX-License-Identifier: GPL-2.0-only
/*
 * Scatterlist handling for DRM
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <asm/memory.h>

#include "heap_private.h"

#define DRM_SG_MAX_SINGLE_ALLOC		(PAGE_SIZE / sizeof(u32))

#define DRM_FIELD_SHIFT	4

#define DRM_SG_CHAIN	0x01UL
#define DRM_SG_END	0x02UL
#define DRM_SG_MASK (DRM_SG_CHAIN | DRM_SG_END)

#define DRM_SG_INDEX	0x04UL

#define DRM_SG_INDEX_ORDER_BITS (8)
#define DRM_SG_INDEX_ORDER_MASK ((1 << DRM_SG_INDEX_ORDER_BITS) - 1)
#define DRM_SG_INDEX_ORDER_SHIFT (DRM_FIELD_SHIFT)

#define DRM_SG_INDEX_NRPAGES_SHIFT (DRM_FIELD_SHIFT + DRM_SG_INDEX_ORDER_BITS)
#define DRM_SG_INDEX_NRPAGES_MASK ((1 << (32 - DRM_SG_INDEX_NRPAGES_SHIFT)) - 1)

static inline unsigned int __drm_sg_flags(unsigned int *entry)
{
	return *entry & DRM_SG_MASK;
}

static inline bool drm_sg_is_chain(unsigned int *entry)
{
	return __drm_sg_flags(entry) & DRM_SG_CHAIN;
}

static inline bool drm_sg_is_last(unsigned int *entry)
{
	return __drm_sg_flags(entry) & DRM_SG_END;
}

static inline unsigned int *drm_sg_chain_ptr(unsigned int *entry)
{
	unsigned int *addr;
	addr = pfn_to_kaddr((unsigned long)(*entry >> DRM_FIELD_SHIFT));

	return addr;
}

static inline unsigned int *drm_sg_next(unsigned int *entry)
{
	if (drm_sg_is_last(entry))
		return NULL;

	entry++;
	if (unlikely(drm_sg_is_chain(entry)))
		entry = drm_sg_chain_ptr(entry);

	return entry;
}

static inline void drm_sg_chain(unsigned int *prev, unsigned int *next)
{
	unsigned int entry = virt_to_pfn(next) << DRM_FIELD_SHIFT;

	*prev = (entry | DRM_SG_CHAIN) & ~DRM_SG_END;
}

static inline void drm_sg_mark_end(unsigned int *entry)
{
	*entry |= DRM_SG_END;
	*entry &= ~DRM_SG_CHAIN;
}

static void drm_sg_set_index(unsigned int *entry, unsigned int order, unsigned int nr_pages)
{
	*entry |= (order & DRM_SG_INDEX_ORDER_MASK) << DRM_SG_INDEX_ORDER_SHIFT;
	*entry |= (nr_pages & DRM_SG_INDEX_NRPAGES_MASK) << DRM_SG_INDEX_NRPAGES_SHIFT;
	*entry |= DRM_SG_INDEX;
}

static void drm_sg_set_pfn(unsigned int *entry, struct page *page)
{
	*entry |= page_to_pfn(page) << DRM_FIELD_SHIFT;
}

void drm_sg_free_table(struct drm_sg_table *table)
{
	unsigned int *lists, *next;
	unsigned int num_ents = table->nents;

	if (unlikely(!table->lists))
		return;

	lists = table->lists;
	while (num_ents) {
		unsigned int drm_sg_size;

		if (num_ents > DRM_SG_MAX_SINGLE_ALLOC) {
			drm_sg_size = DRM_SG_MAX_SINGLE_ALLOC - 1;
			next = drm_sg_chain_ptr(&lists[DRM_SG_MAX_SINGLE_ALLOC - 1]);
		} else {
			drm_sg_size = num_ents;
			next = NULL;
		}

		num_ents -= drm_sg_size;
		free_page((unsigned long)lists);

		lists = next;
	}

	table->lists = NULL;
}

int __drm_sg_alloc_table(struct drm_sg_table *table, unsigned int nents)
{
	unsigned int *lists, *prv;
	unsigned int left;

	if (nents == 0)
		return -EINVAL;

	left = nents;
	prv = NULL;
	do {
		unsigned int drm_sg_size;

		if (left > DRM_SG_MAX_SINGLE_ALLOC)
			drm_sg_size = DRM_SG_MAX_SINGLE_ALLOC - 1;
		else
			drm_sg_size = left;

		left -= drm_sg_size;

		lists = (void *) __get_free_page(GFP_KERNEL | __GFP_ZERO);
		if (unlikely(!lists)) {
			/*
			 * Adjust entry count to reflect that the last
			 * entry of the previous table won't be used for
			 * linkage.  Without this, drm_sg_free_table() may get
			 * confused.
			 */
			if (prv)
				table->nents++;

			return -ENOMEM;
		}

		table->nents += drm_sg_size;

		/*
		 * If this is the first mapping, assign the drm_sg_table header.
		 * If this is not the first mapping, chain previous part.
		 */
		if (prv)
			drm_sg_chain(&prv[DRM_SG_MAX_SINGLE_ALLOC - 1], lists);
		else
			table->lists = lists;

		/*
		 * If no more entries after this one, mark the end
		 */
		if (!left)
			drm_sg_mark_end(&lists[drm_sg_size - 1]);

		prv = lists;
	} while (left);

	return 0;
}

int drm_sg_alloc_table(struct drm_sg_table *table, unsigned int nents)
{
	int ret;

	ret = __drm_sg_alloc_table(table, nents);
	if (unlikely(ret))
		drm_sg_free_table(table);
	return ret;
}

void drm_sg_set_table(struct drm_sg_table *table, struct list_head *pages,
		      unsigned int *nr_page_lists)
{
	unsigned int *entry = table->lists;
	unsigned int i;
	struct page *page, *tmp_page;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (!nr_page_lists[i])
			continue;

		drm_sg_set_index(entry, orders[i], nr_page_lists[i]);
		entry = drm_sg_next(entry);

		list_for_each_entry_safe(page, tmp_page, &pages[i], lru) {
			drm_sg_set_pfn(entry, page);
			entry = drm_sg_next(entry);
		}
	}
}
