// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Secure System heap exporter for Samsung
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
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
#include <linux/sizes.h>
#include <linux/samsung-dma-heap.h>
#include <linux/kthread.h>

#include "heap_private.h"
#include "page_pool.h"

struct secure_heap_prefetch_stat {
	struct kobject kobj;
	unsigned int bulk;
	unsigned int max;
	unsigned int min;
	bool enabled;
};

struct secure_system_heap {
	struct samsung_dma_heap *heap;
	struct secure_heap_prefetch_stat stat;
	struct kthread_work work;
};

static struct kthread_worker secure_system_prefetch_worker;

void secure_system_heap_free_pages(struct secure_page_pool **pools, struct list_head *page_list)
{
	struct page *page, *tmp_page;

	list_for_each_entry_safe(page, tmp_page, page_list, lru) {
		list_del(&page->lru);
		secure_page_pool_free(pools[page_order_index(page)], page);
	}
}

static void secure_system_heap_buffer_release(struct samsung_dma_buffer *buffer,
					      enum df_reason reason)
{
	struct secure_pool_info *info = get_secure_pool_info(buffer->heap->protection_id);
	struct list_head page_list;

	set_sgtable_to_page_list(&buffer->sg_table, &page_list);

	if (reason == DF_NORMAL)
		goto free_secure_pool;

	if (samsung_dma_buffer_hvc_multi(HVC_DRM_TZMP2_UNPROT, &page_list,
					 buffer->heap->protection_id))
		goto free_secure_pool;

	normal_pool_free_pages(&page_list, reason);

	return;

free_secure_pool:
	secure_system_heap_free_pages(info->pools, &page_list);

	dma_heap_event_pool_record(DMA_HEAP_POOL_EVENT_FREE, info->protid, buffer->len,
				   buffer->len, get_secure_pool_size(info->pools));
}

static struct page *fetch_largest_available(struct secure_page_pool **pools, unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = secure_page_pool_alloc(pools[i]);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

static int secure_system_heap_buffer_allocate(struct samsung_dma_buffer *buffer)
{
	struct secure_system_heap *secure_sys_heap = buffer->heap->priv;
	struct secure_pool_info *info = get_secure_pool_info(buffer->heap->protection_id);
	struct scatterlist *sg;
	struct list_head page_list, normal_page_list;
	struct page *page, *tmp_page;
	int ret, nr_pages = 0, nr_normal_pages = 0;
	unsigned int max_order = orders[0];
	unsigned long size_remaining = buffer->len;

	INIT_LIST_HEAD(&page_list);
	INIT_LIST_HEAD(&normal_page_list);

	while (size_remaining > 0) {
		page = fetch_largest_available(info->pools, size_remaining, max_order);
		if (!page)
			break;
		list_add_tail(&page->lru, &page_list);
		size_remaining -= page_size(page);
		nr_pages++;

		max_order = compound_order(page);
	}

	dma_heap_event_pool_record(DMA_HEAP_POOL_EVENT_ALLOC, info->protid, buffer->len,
				   -(buffer->len - size_remaining),
				   get_secure_pool_size(info->pools));

	if (size_remaining) {
		ret = normal_pool_alloc_pages(size_remaining, &normal_page_list, &nr_normal_pages);
		if (ret)
			goto err_alloc;

		heap_pages_flush(dma_heap_get_dev(buffer->heap->dma_heap), &normal_page_list);

		ret = samsung_dma_buffer_hvc_multi(HVC_DRM_TZMP2_PROT, &normal_page_list,
						   buffer->heap->protection_id);
		if (ret) {
			normal_pool_free_pages(&normal_page_list, 0);
			goto err_alloc;
		}

		list_splice(&normal_page_list, &page_list);
		nr_pages += nr_normal_pages;
	}

	if (sg_alloc_table(&buffer->sg_table, nr_pages, GFP_KERNEL)) {
		perrfn("failed to allocate sgtable, nents %d", nr_pages);
		goto err_alloc;
	}

	sg = buffer->sg_table.sgl;
	list_for_each_entry_safe(page, tmp_page, &page_list, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	if (secure_sys_heap->stat.enabled)
		kthread_queue_work(&secure_system_prefetch_worker, &secure_sys_heap->work);

	return 0;
err_alloc:
	secure_system_heap_free_pages(info->pools, &page_list);

	return ret;
}

static struct samsung_heap_ops secure_system_heap_ops = {
	.allocate = secure_system_heap_buffer_allocate,
	.release = secure_system_heap_buffer_release,
};

void secure_system_heap_prefetch(struct secure_system_heap *secure_sys_heap)
{
	struct samsung_dma_heap *heap = secure_sys_heap->heap;
	struct secure_heap_prefetch_stat *stat = &secure_sys_heap->stat;
	struct secure_pool_info *info = get_secure_pool_info(heap->protection_id);
	struct list_head page_list;
	unsigned int total = 0;
	int nr_pages;

	if (!stat->enabled)
		return;

	while (get_secure_pool_size(info->pools) < stat->max) {
		INIT_LIST_HEAD(&page_list);

		if (normal_pool_alloc_pages(stat->bulk, &page_list, &nr_pages))
			return;

		heap_pages_flush(dma_heap_get_dev(heap->dma_heap), &page_list);
		if (samsung_dma_buffer_hvc_multi(HVC_DRM_TZMP2_PROT, &page_list,
						 heap->protection_id)) {
			normal_pool_free_pages(&page_list, 0);
			return;
		}

		secure_system_heap_free_pages(info->pools, &page_list);
		total += stat->bulk;
	}
	dma_heap_event_pool_record(DMA_HEAP_POOL_EVENT_PREFETCH, info->protid, total,
				   total, get_secure_pool_size(info->pools));
}

#define get_prefetch_stat(kobj) container_of(kobj, struct secure_heap_prefetch_stat, kobj)

void secure_system_prefetch_work(struct kthread_work *work)
{
	struct secure_system_heap *secure_sys_heap =
		container_of(work, struct secure_system_heap, work);

	secure_system_heap_prefetch(secure_sys_heap);
}

void secure_pool_prefetch_disabled(const char *name)
{
	struct dma_heap *dma_heap = dma_heap_find(name);
	struct samsung_dma_heap *heap = dma_heap_get_drvdata(dma_heap);
	struct secure_system_heap *secure_sys_heap = heap->priv;
	struct secure_heap_prefetch_stat *stat = &secure_sys_heap->stat;
	struct secure_pool_info *info = get_secure_pool_info(heap->protection_id);

	stat->enabled = false;
	secure_pool_set_prefetch_min(info, 0);

	dma_heap_put(dma_heap);
}

void secure_pool_prefetch_enabled(const char *name)
{
	struct dma_heap *dma_heap = dma_heap_find(name);
	struct samsung_dma_heap *heap = dma_heap_get_drvdata(dma_heap);
	struct secure_system_heap *secure_sys_heap = heap->priv;
	struct secure_heap_prefetch_stat *stat = &secure_sys_heap->stat;
	struct secure_pool_info *info = get_secure_pool_info(heap->protection_id);

	stat->enabled = true;
	secure_pool_set_prefetch_min(info, stat->min);

	kthread_queue_work(&secure_system_prefetch_worker, &secure_sys_heap->work);

	dma_heap_put(dma_heap);
}

static ssize_t prefetch_min_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);
	struct secure_system_heap *secure_sys_heap =
		container_of(stat, struct secure_system_heap, stat);
	struct secure_pool_info *info =
		get_secure_pool_info(secure_sys_heap->heap->protection_id);
	u32 min;

	if (kstrtou32(buf, 10, &min))
		return -EINVAL;

	if (min > SZ_128M)
		min = SZ_128M;

	stat->min = min;

	if (stat->enabled)
		secure_pool_set_prefetch_min(info, stat->min);

	return count;
}

static ssize_t prefetch_min_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);

	return sysfs_emit(buf, "%u\n", stat->min);
}
static struct kobj_attribute prefetch_min_attr = __ATTR_RW(prefetch_min);

static ssize_t prefetch_max_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);
	u32 max;

	if (kstrtou32(buf, 10, &max))
		return -EINVAL;

	if (max > SZ_256M)
		max = SZ_256M;

	stat->max = max;

	return count;
}

static ssize_t prefetch_max_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);

	return sysfs_emit(buf, "%u\n", stat->max);
}
static struct kobj_attribute prefetch_max_attr = __ATTR_RW(prefetch_max);

static ssize_t prefetch_bulk_store(struct kobject *kobj, struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);
	u32 bulk;

	if (kstrtou32(buf, 10, &bulk))
		return -EINVAL;

	if (bulk > SZ_32M)
		bulk = SZ_32M;

	stat->bulk = bulk;

	return count;
}

static ssize_t prefetch_bulk_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);

	return sysfs_emit(buf, "%u\n", stat->bulk);
}
static struct kobj_attribute prefetch_bulk_attr = __ATTR_RW(prefetch_bulk);

static ssize_t prefetch_enabled_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);
	struct secure_system_heap *secure_sys_heap =
		container_of(stat, struct secure_system_heap, stat);
	struct secure_pool_info *info =
		get_secure_pool_info(secure_sys_heap->heap->protection_id);
	bool enabled;

	if (kstrtobool(buf, &enabled))
		return -EINVAL;

	stat->enabled = enabled;

	if (enabled) {
		secure_pool_set_prefetch_min(info, stat->min);
		secure_system_heap_prefetch(secure_sys_heap);
	} else {
		secure_pool_set_prefetch_min(info, 0);
	}

	return count;
}

static ssize_t prefetch_enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct secure_heap_prefetch_stat *stat = get_prefetch_stat(kobj);

	return sysfs_emit(buf, "%u\n", stat->enabled);
}
static struct kobj_attribute prefetch_enabled_attr = __ATTR_RW(prefetch_enabled);

extern struct kobject *samsung_dma_heap_kobj;

static struct attribute *secure_sys_heap_attrs[] = {
	&prefetch_min_attr.attr,
	&prefetch_max_attr.attr,
	&prefetch_bulk_attr.attr,
	&prefetch_enabled_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(secure_sys_heap);

static void secure_sys_heap_kobj_release(struct kobject *obj)
{
	/* Never released the static objects */
}

static struct kobj_type secure_sys_heap_ktype = {
	.release = secure_sys_heap_kobj_release,
	.sysfs_ops = &kobj_sysfs_ops,
	.default_groups = secure_sys_heap_groups,
};

void secure_sys_heap_sysfs_setup(struct samsung_dma_heap *heap)
{
	struct secure_system_heap *secure_sys_heap = heap->priv;
	int ret;

	ret = kobject_init_and_add(&secure_sys_heap->stat.kobj, &secure_sys_heap_ktype,
				   samsung_dma_heap_kobj, heap->name);

	if (ret) {
		pr_err("kobject %s initialized failed\n", heap->name);
		kobject_put(&secure_sys_heap->stat.kobj);
		return;
	}
}

static struct task_struct *secure_system_prefetch_task;

static int secure_system_heap_probe(struct platform_device *pdev)
{
	struct secure_system_heap *secure_sys_heap;
	struct samsung_dma_heap *heap;
	struct secure_pool_info *info;
	unsigned int protid;

	secure_sys_heap = devm_kzalloc(&pdev->dev, sizeof(*secure_sys_heap), GFP_KERNEL);
	if (!secure_sys_heap)
		return -ENOMEM;

	heap = samsung_heap_create(&pdev->dev, secure_sys_heap);
	if (IS_ERR(heap))
		return PTR_ERR(heap);

	kthread_init_work(&secure_sys_heap->work, secure_system_prefetch_work);

	secure_sys_heap->heap = heap;
	secure_sys_heap->stat.bulk = SZ_1M;
	secure_sys_heap->stat.max = SZ_32M;
	secure_sys_heap->stat.min = SZ_8M;
	secure_sys_heap_sysfs_setup(heap);

	protid = heap->protection_id;
	info = get_secure_pool_info(protid);
	if (!info) {
		int ret = add_secure_pool_info(heap->name, protid);

		if (ret) {
			perrfn("Adding secure heap is failed %d", ret);
			return ret;
		}
	}

	heap->ops = &secure_system_heap_ops;
	return samsung_heap_add(heap);
}

static const struct of_device_id secure_system_heap_of_match[] = {
	{ .compatible = "samsung,dma-heap-secure-system", },
	{ },
};
MODULE_DEVICE_TABLE(of, secure_system_heap_of_match);

static struct platform_driver secure_system_heap_driver = {
	.driver		= {
		.name	= "samsung,dma-heap-secure-system",
		.of_match_table = secure_system_heap_of_match,
	},
	.probe		= secure_system_heap_probe,
};

int __init secure_system_dma_heap_init(void)
{
	kthread_init_worker(&secure_system_prefetch_worker);

	secure_system_prefetch_task = kthread_run(kthread_worker_fn,
			&secure_system_prefetch_worker, "secure_system_prefetch_worker");

	return platform_driver_register(&secure_system_heap_driver);
}

void secure_system_dma_heap_exit(void)
{
	platform_driver_unregister(&secure_system_heap_driver);
}
