// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Allocator - Common implementation
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/rwlock.h>

#include <trace/hooks/mm.h>

#include "page_pool.h"
#include "heap_private.h"

#define MAX_EVENT_LOG	2048
#define EVENT_CLAMP_ID(id) ((id) & (MAX_EVENT_LOG - 1))

static atomic_t dma_heap_pool_eventid;

static char * const dma_heap_pool_event_name[] = {
	"alloc",
	"free",
	"shrink",
	"prefetch",
};

static struct dma_heap_pool_event {
	ktime_t timestamp;
	unsigned int protid;
	size_t requested;
	long changed;
	long remained;
	enum dma_heap_pool_event_type type;
} dma_heap_pool_events[MAX_EVENT_LOG];

void dma_heap_event_pool_record(enum dma_heap_pool_event_type type, unsigned int protid,
				size_t size, long diff, long pool_size)
{
	int idx = EVENT_CLAMP_ID(atomic_inc_return(&dma_heap_pool_eventid));
	struct dma_heap_pool_event *event = &dma_heap_pool_events[idx];

	event->timestamp = ktime_get();
	event->protid = protid;
	event->type = type;
	event->requested = size;
	event->changed = diff;
	event->remained = pool_size;
}

static atomic_t dma_heap_eventid;

static char * const dma_heap_event_name[] = {
	"alloc",
	"free",
	"prot",
	"unprot",
	"flush",
	"page_pool_alloc",
};

static struct dma_heap_event {
	ktime_t begin;
	ktime_t done;
	unsigned char heapname[16];
	size_t size;
	enum dma_heap_event_type type;
} dma_heap_events[MAX_EVENT_LOG];

void __dma_heap_event_record(enum dma_heap_event_type type, const char *name,
			     size_t size, ktime_t begin)
{
	int idx = EVENT_CLAMP_ID(atomic_inc_return(&dma_heap_eventid));
	struct dma_heap_event *event = &dma_heap_events[idx];

	event->begin = begin;
	event->done = ktime_get();
	strlcpy(event->heapname, name, sizeof(event->heapname));
	event->size = size;
	event->type = type;
}

void dma_heap_event_record(enum dma_heap_event_type type, struct dma_buf *dmabuf, ktime_t begin)
{
	__dma_heap_event_record(type, dmabuf->exp_name, dmabuf->size, begin);
}

#ifdef CONFIG_DEBUG_FS
static int dma_heap_event_pool_show(struct seq_file *s, void *unused)
{
	int index = EVENT_CLAMP_ID(atomic_read(&dma_heap_pool_eventid) + 1);
	int i = index;

	seq_printf(s, "%14s %16s %16s %10s %10s %10s\n",
		   "timestamp", "type", "protid", "request(kb)", "changed(kb)", "remained(kb");

	do {
		struct dma_heap_pool_event *event = &dma_heap_pool_events[EVENT_CLAMP_ID(i)];
		struct timespec64 ts = ktime_to_timespec64(event->timestamp);

		if (ts.tv_sec == 0 && ts.tv_nsec == 0)
			continue;

		seq_printf(s, "[%06lld.%06ld]", ts.tv_sec, ts.tv_nsec / NSEC_PER_USEC);
		seq_printf(s, "%16s %16u %10zd %10ld %10ld", dma_heap_pool_event_name[event->type],
			   event->protid, event->requested >> 10,
			   event->changed >> 10, event->remained >> 10);
		seq_puts(s, "\n");
	} while (EVENT_CLAMP_ID(++i) != index);

	return 0;
}

static int dma_heap_pool_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_heap_event_pool_show, inode->i_private);
}

static const struct file_operations dma_heap_pool_event_fops = {
	.open = dma_heap_pool_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dma_heap_event_show(struct seq_file *s, void *unused)
{
	int index = EVENT_CLAMP_ID(atomic_read(&dma_heap_eventid) + 1);
	int i = index;

	seq_printf(s, "%14s %18s %16s %16s %10s\n",
		   "timestamp", "event", "name", "size(kb)", "elapsed(us)");

	do {
		struct dma_heap_event *event = &dma_heap_events[EVENT_CLAMP_ID(i)];
		long elapsed = ktime_us_delta(event->done, event->begin);
		struct timespec64 ts = ktime_to_timespec64(event->begin);

		if (elapsed == 0)
			continue;

		seq_printf(s, "[%06lld.%06ld]", ts.tv_sec, ts.tv_nsec / NSEC_PER_USEC);
		seq_printf(s, "%18s %16s %16zd %10ld", dma_heap_event_name[event->type],
			   event->heapname, event->size >> 10, elapsed);

		if (elapsed > 100 * USEC_PER_MSEC)
			seq_puts(s, " *");

		seq_puts(s, "\n");
	} while (EVENT_CLAMP_ID(++i) != index);

	return 0;
}

static int dma_heap_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_heap_event_show, inode->i_private);
}

static const struct file_operations dma_heap_event_fops = {
	.open = dma_heap_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void dma_heap_debug_init(void)
{
	struct dentry *root = debugfs_create_dir("dma_heap", NULL);

	if (IS_ERR(root)) {
		pr_err("Failed to create debug directory for dma_heap");
		return;
	}

	if (IS_ERR(debugfs_create_file("event", 0444, root, NULL, &dma_heap_event_fops)))
		pr_err("Failed to create event file for dma_heap\n");

	if (IS_ERR(debugfs_create_file("pool_event", 0444, root, NULL, &dma_heap_pool_event_fops)))
		pr_err("Failed to create debug file for page pool\n");
}
#else
void dma_heap_debug_init(void)
{
}
#endif

#define MAX_EXCEPTION_AREAS 4
static phys_addr_t dma_heap_exception_areas[MAX_EXCEPTION_AREAS][2];
static int nr_dma_heap_exception;

bool is_dma_heap_exception_page(struct page *page)
{
	phys_addr_t phys = page_to_phys(page);
	int i;

	for (i = 0; i < nr_dma_heap_exception; i++)
		if ((dma_heap_exception_areas[i][0] <= phys) &&
		    (phys <= dma_heap_exception_areas[i][1]))
			return true;

	return false;
}

void heap_pages_flush(struct device *dev, struct list_head *page_list)
{
	struct page *page, *tmp_page;
	unsigned int total = 0;

	dma_heap_event_begin();

	list_for_each_entry_safe(page, tmp_page, page_list, lru) {
		void *addr = page_address(page);
		size_t size = page_size(page);
		dma_addr_t dma_handle;

		total += size;

		dma_handle = dma_map_single(dev, addr, size, DMA_TO_DEVICE);
		dma_unmap_single(dev, dma_handle, size, DMA_FROM_DEVICE);
	}
	__dma_heap_event_record(DMA_HEAP_EVENT_FLUSH, dev_name(dev), total, begin);
}

void heap_cache_flush(struct samsung_dma_buffer *buffer)
{
	struct device *dev = dma_heap_get_dev(buffer->heap->dma_heap);

	if (!dma_heap_skip_cache_ops(buffer->flags))
		return;

	dma_heap_event_begin();
	/*
	 * Flushing caches on buffer allocation is intended for preventing
	 * corruption from writing back to DRAM from the dirty cache lines
	 * while updating the buffer from DMA. However, cache flush should be
	 * performed on the entire allocated area if the buffer is to be
	 * protected from non-secure access to prevent the dirty write-back
	 * to the protected area.
	 */
	dma_map_sgtable(dev, &buffer->sg_table, DMA_TO_DEVICE, 0);
	dma_unmap_sgtable(dev, &buffer->sg_table, DMA_FROM_DEVICE, 0);

	__dma_heap_event_record(DMA_HEAP_EVENT_FLUSH, buffer->heap->name, buffer->len, begin);
}

void heap_sgtable_pages_clean(struct sg_table *sgt)
{
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_atomic(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}
}

void set_sgtable_to_page_list(struct sg_table *sg_table, struct list_head *list)
{
	struct page *page;
	struct scatterlist *sg;
	int i;

	INIT_LIST_HEAD(list);

	for_each_sgtable_sg(sg_table, sg, i) {
		page = sg_page(sg);
		list_add_tail(&page->lru, list);
	}
}

/*
 * It should be called by physically contiguous buffer.
 */
void heap_page_clean(struct page *pages, unsigned long size)
{
	unsigned long nr_pages, i;

	if (!PageHighMem(pages)) {
		memset(page_address(pages), 0, size);
		return;
	}

	nr_pages = PAGE_ALIGN(size) >> PAGE_SHIFT;

	for (i = 0; i < nr_pages; i++) {
		void *vaddr = kmap_atomic(&pages[i]);

		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}
}

struct samsung_dma_buffer *samsung_dma_buffer_alloc(struct samsung_dma_heap *heap,
						    unsigned long size)
{
	struct samsung_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = size;
	buffer->flags = heap->flags;

	return buffer;
}
EXPORT_SYMBOL_GPL(samsung_dma_buffer_alloc);

void samsung_dma_buffer_free(struct samsung_dma_buffer *buffer)
{
	kfree(buffer);
}
EXPORT_SYMBOL_GPL(samsung_dma_buffer_free);

struct dma_buf *samsung_heap_allocate(struct dma_heap *dma_heap, unsigned long len,
				      u32 fd_flags, u64 heap_flags)
{
	struct samsung_dma_heap *heap = dma_heap_get_drvdata(dma_heap);
	struct samsung_dma_buffer *buffer;
	struct dma_buf *dmabuf;
	int ret;

	dma_heap_event_begin();

	len = ALIGN(len, heap->alignment);
	buffer = samsung_dma_buffer_alloc(heap, len);
	if (IS_ERR(buffer))
		return ERR_PTR(-ENOMEM);

	ret = heap->ops->allocate(buffer);
	if (ret)
		goto err_alloc;

	dmabuf = samsung_export_dmabuf(buffer, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_export;
	}

	dma_heap_event_record(DMA_HEAP_EVENT_ALLOC, dmabuf, begin);

	return dmabuf;

free_export:
	heap->ops->release(buffer, 0);
err_alloc:
	samsung_dma_buffer_free(buffer);
	samsung_allocate_error_report(heap, len, fd_flags, heap_flags);

	return ERR_PTR(ret);
}

static void show_dmaheap_total_handler(void *data, unsigned int filter, nodemask_t *nodemask)
{
	struct samsung_dma_heap *heap = data;
	u64 total_size_kb = samsung_heap_total_kbsize(heap);

	/* skip if the size is zero in order not to show meaningless log. */
	if (total_size_kb)
		pr_info("%s: %llukb ", heap->name, total_size_kb);
}

static void show_dmaheap_meminfo(void *data, struct seq_file *m)
{
	struct samsung_dma_heap *heap = data;
	u64 total_size_kb = samsung_heap_total_kbsize(heap);

	if (total_size_kb == 0)
		return;

	show_val_meminfo(m, heap->name, total_size_kb);
}

static int samsung_heap_read_dt(struct samsung_dma_heap *heap, struct device *dev)
{
	unsigned int alignment = PAGE_SIZE, order, protid = 0;
	const char *name;
	unsigned long flags = 0;

	if (of_property_read_bool(dev->of_node, "dma-heap,direct_io"))
		flags |= DMA_HEAP_FLAG_DIRECT_IO;

	if (of_property_read_bool(dev->of_node, "dma-heap,deferred_free"))
		flags |= DMA_HEAP_FLAG_DEFERRED_FREE;

	if (of_property_read_bool(dev->of_node, "dma-heap,uncached"))
		flags |= DMA_HEAP_FLAG_UNCACHED;

	if (of_property_read_bool(dev->of_node, "dma-heap,secure")) {
		flags |= DMA_HEAP_FLAG_PROTECTED;

		of_property_read_u32(dev->of_node, "dma-heap,protection_id", &protid);
		if (!protid) {
			perrfn("Secure heap should be set with protection id");
			return -EINVAL;
		}
		if (of_property_read_bool(dev->of_node, "dma-heap,secure_sysmmu"))
			flags |= DMA_HEAP_FLAG_SECURE_SYSMMU;
	}

	if (of_property_read_string(dev->of_node, "dma-heap,name", &name)) {
		perrfn("The heap should define name on device node");
		return -EINVAL;
	}

	of_property_read_u32(dev->of_node, "dma-heap,alignment", &alignment);
	order = min_t(unsigned int, get_order(alignment), MAX_ORDER);

	heap->flags = flags;
	heap->protection_id = protid;
	heap->alignment = 1 << (order + PAGE_SHIFT);
	heap->name = name;

	return 0;
}

struct samsung_dma_heap *samsung_heap_create(struct device *dev, void *priv)
{
	struct samsung_dma_heap *heap;
	int ret;

	if (!dev)
		return ERR_PTR(-ENODEV);

	heap = devm_kzalloc(dev, sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);

	ret = samsung_heap_read_dt(heap, dev);
	if (ret)
		return ERR_PTR(ret);

	heap->priv = priv;

	return heap;
}

static const struct dma_heap_ops samsung_heap_ops = {
	.allocate = samsung_heap_allocate,
	.get_pool_size = get_samsung_pool_size,
};

int samsung_heap_add(struct samsung_dma_heap *heap)
{
	struct dma_heap_export_info exp_info;

	exp_info.name = heap->name;
	exp_info.ops = &samsung_heap_ops;
	exp_info.priv = heap;

	heap->dma_heap = dma_heap_add(&exp_info);
	if (IS_ERR(heap->dma_heap))
		return PTR_ERR(heap->dma_heap);

	dma_coerce_mask_and_coherent(dma_heap_get_dev(heap->dma_heap), DMA_BIT_MASK(36));
	register_trace_android_vh_show_mem(show_dmaheap_total_handler, heap);
	if (!strncmp(heap->name, "system", strlen("system"))
#ifdef CONFIG_DMABUF_HEAPS_CAMERAPOOL
		|| !strncmp(heap->name, "camerapool", strlen("camerapool"))
#endif
	)
		register_trace_android_vh_meminfo_proc_show(show_dmaheap_meminfo, heap);

	pr_info("Registered %s dma-heap successfully\n", heap->name);

	return 0;
}

struct dma_buf *samsung_export_dmabuf(struct samsung_dma_buffer *buffer, unsigned long fd_flags)
{
	struct dma_buf *dmabuf;

	DEFINE_SAMSUNG_DMA_BUF_EXPORT_INFO(exp_info, buffer->heap->name);

	exp_info.ops = &samsung_dma_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		perrfn("failed to export buffer (ret: %ld)", PTR_ERR(dmabuf));
		return dmabuf;
	}
	buffer->i_node = file_inode(dmabuf->file)->i_ino;
	atomic_long_add(dmabuf->size, &buffer->heap->total_bytes);
	dmabuf_trace_alloc(dmabuf);

	return dmabuf;
}
EXPORT_SYMBOL_GPL(samsung_export_dmabuf);

static void dma_heap_add_exception_area(void)
{
	struct device_node *np;

	for_each_node_by_name(np, "dma_heap_exception_area") {
		int naddr = of_n_addr_cells(np);
		int nsize = of_n_size_cells(np);
		phys_addr_t base, size;
		const __be32 *prop;
		int len;
		int i;

		prop = of_get_property(np, "#address-cells", NULL);
		if (prop)
			naddr = be32_to_cpup(prop);

		prop = of_get_property(np, "#size-cells", NULL);
		if (prop)
			nsize = be32_to_cpup(prop);

		prop = of_get_property(np, "exception-range", &len);
		if (prop && len > 0) {
			int n_area = len / (sizeof(*prop) * (nsize + naddr));

			n_area = min_t(int, n_area, MAX_EXCEPTION_AREAS);

			for (i = 0; i < n_area ; i++) {
				base = (phys_addr_t)of_read_number(prop, naddr);
				prop += naddr;
				size = (phys_addr_t)of_read_number(prop, nsize);
				prop += nsize;

				dma_heap_exception_areas[i][0] = base;
				dma_heap_exception_areas[i][1] = base + size - 1;
			}

			nr_dma_heap_exception = n_area;
		}
	}
}

struct kobject *samsung_dma_heap_kobj;

static int __init samsung_dma_heap_init(void)
{
	int ret;

	ret = samsung_page_pool_init();
	if (ret)
		return ret;

	samsung_dma_heap_kobj = kobject_create_and_add("samsung_dma_heap", kernel_kobj);
	if (!samsung_dma_heap_kobj)
		goto err_kobj;

	ret = preallocated_chunk_dma_heap_init();
	if (ret)
		goto err_chunk;

	ret = cma_dma_heap_init();
	if (ret)
		goto err_cma;

	ret = carveout_dma_heap_init();
	if (ret)
		goto err_carveout;

	ret = gcma_dma_heap_init();
	if (ret)
		goto err_gcma;

	ret = system_dma_heap_init();
	if (ret)
		goto err_system;

	ret = secure_system_dma_heap_init();
	if (ret)
		goto err_secure_system;

	ret = dmabuf_trace_create();
	if (ret)
		goto err_trace;

	ret = rbin_dma_heap_init();
	if (ret)
		goto err_rbin;

#ifdef CONFIG_DMABUF_HEAPS_CAMERAPOOL
	ret = camerapool_dma_heap_init();
	if (ret)
		goto err_camerapool;
#endif

	dma_heap_add_exception_area();
	dma_heap_debug_init();
	dmabuf_container_init();

	return 0;
#ifdef CONFIG_DMABUF_HEAPS_CAMERAPOOL
err_camerapool:
	camerapool_dma_heap_exit();
#endif
err_rbin:
	dmabuf_trace_remove();
err_trace:
	secure_system_dma_heap_exit();
err_secure_system:
	system_dma_heap_exit();
err_system:
	gcma_dma_heap_exit();
err_gcma:
	carveout_dma_heap_exit();
err_carveout:
	cma_dma_heap_exit();
err_cma:
	preallocated_chunk_dma_heap_exit();
err_chunk:
	kobject_put(samsung_dma_heap_kobj);
err_kobj:
	samsung_page_pool_destroy();

	return ret;
}

static void __exit samsung_dma_heap_exit(void)
{
	rbin_dma_heap_exit();
	dmabuf_trace_remove();
	secure_system_dma_heap_exit();
	system_dma_heap_exit();
	carveout_dma_heap_exit();
	cma_dma_heap_exit();
	preallocated_chunk_dma_heap_exit();
#ifdef CONFIG_DMABUF_HEAPS_CAMERAPOOL
	camerapool_dma_heap_exit();
#endif
	kobject_put(samsung_dma_heap_kobj);
	samsung_page_pool_destroy();
}

module_init(samsung_dma_heap_init);
module_exit(samsung_dma_heap_exit);
MODULE_DESCRIPTION("DMA-BUF Samsung Heap");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
