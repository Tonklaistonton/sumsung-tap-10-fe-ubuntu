#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list_sort.h>
#include <linux/iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-resv.h>
#include <linux/dma-heap.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/samsung/samsung-dma-mapping.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include "include/npu-memory.h"
#include "include/npu-config.h"
#include "npu-log.h"
#include "npu-system.h"
#include "dsp-dhcp.h"

static bool g_iova_36bit;
static struct npu_memory_manual_info g_npu_memory_manual_info;

DECLARE_HASHTABLE(npu_memory_buffer_hash_table, 8);

inline void default_set_iova_36bit(void)
{
        g_iova_36bit = true;
	probe_info("%s(%d): set iova 36bit\n", __func__, __LINE__);
}

inline void set_iova_36bit(bool iova_36bit)
{
	g_iova_36bit = iova_36bit;
}

inline bool get_iova_36bit(void)
{
	return g_iova_36bit;
}

static void npu_memory_region_init_buffer(struct npu_memory_buffer *buffer, pid_t pid, bool list_init)
{
	buffer->dma.dma_buf = NULL;
	buffer->dma.attachment = NULL;
	buffer->dma.sgt = NULL;
	buffer->dma.daddr = 0;
	buffer->dma.vaddr = NULL;
	buffer->npu_sgt = false;
	buffer->imb_for_ncp = false;
	buffer->imb_for_iofm = false;
	buffer->pid = pid;
	buffer->imb_dup = false;
	buffer->manual_dup = false;
	if (list_init) {
		INIT_LIST_HEAD(&buffer->list);
		INIT_LIST_HEAD(&buffer->imb_map_list);
	}
}

bool npu_memory_region_check_used_buffer(struct npu_memory_region *region, struct npu_memory_buffer *buffer)
{
	bool ret = false;
	struct npu_memory_buffer *buffer_w;

	buffer->dma.dma_buf = dma_buf_get(buffer->usr.fd);

	buffer_w = npu_memory_region_buffer_hash_get(region, buffer->dma.dma_buf);
	if (buffer_w) {
		npu_info("find already used buffer(buffer->pid(%d), fd(%d)), manual_dup(%d)\n",
				buffer_w->pid, buffer_w->usr.fd, buffer_w->manual_dup);
		memcpy((void *)buffer, (void *)buffer_w, sizeof(struct npu_memory_buffer));
		buffer->manual_dup = true;
		ret = true;
	}

	dma_buf_put(buffer->dma.dma_buf);

	return ret;
}

static void npu_memory_manual_info_set(void)
{
	struct npu_memory_manual_info* manual_info = &g_npu_memory_manual_info;
	manual_info->iova_36bit = get_iova_36bit();
	manual_info->region_map.heap = MAP_HEAP;
	manual_info->region_map.dva = MAP_DVA;
	manual_info->region_map.imb = MAP_IMB;

	if (manual_info->iova_36bit) {
		manual_info->region_map.dva_expanded = MAP_DVA_EXPANDED;
		manual_info->num_regions = 4;

		manual_info->region_info[MAP_HEAP][0] = HEAP_EXPANDED_START;
		manual_info->region_info[MAP_HEAP][1] = HEAP_EXPANDED_END;
		manual_info->region_info[MAP_DVA][0] = IOVA_NORMAL_START;
		manual_info->region_info[MAP_DVA][1] = IOVA_NORMAL_END;
		manual_info->region_info[MAP_IMB][0] = IMB_EXPANDED_START;
		manual_info->region_info[MAP_IMB][1] = IMB_EXPANDED_END;
		manual_info->region_info[MAP_DVA_EXPANDED][0] = IOVA_EXPANDED_START;
		manual_info->region_info[MAP_DVA_EXPANDED][1] = IOVA_EXPANDED_END;
	} else {
		manual_info->region_map.dva_expanded = MAP_DVA;
		manual_info->num_regions = 3;

		manual_info->region_info[MAP_HEAP][0] = HEAP_START;
		manual_info->region_info[MAP_HEAP][1] = HEAP_END;
		manual_info->region_info[MAP_IMB][0] = IMB_START;
		manual_info->region_info[MAP_IMB][1] = IMB_END;
		manual_info->region_info[MAP_DVA][0] = IOVA_START;
		manual_info->region_info[MAP_DVA][1] = IOVA_END;
	}
}

inline struct npu_memory_manual_info *npu_memory_manual_info_get(void)
{
	return &g_npu_memory_manual_info;
}

void npu_memory_manual_info_noti(struct npu_memory_manual_info* manual_info,
	struct npu_system* system)
{
	dsp_dhcp_update_imb(system,
		manual_info->region_info[MAP_IMB][0],
		manual_info->region_info[MAP_IMB][1]
		 - manual_info->region_info[MAP_IMB][0]);
}

inline struct npu_memory_region* npu_memory_region_get_heap(struct npu_memory_manual* manual)
{
	struct npu_memory_manual_info* manual_info = npu_memory_manual_info_get();
    return &manual->region[manual_info->region_map.heap];
}
inline struct npu_memory_region* npu_memory_region_get_dva(struct npu_memory_manual* manual)
{
	struct npu_memory_manual_info* manual_info = npu_memory_manual_info_get();
    return &manual->region[manual_info->region_map.dva];
}
inline struct npu_memory_region* npu_memory_region_get_imb(struct npu_memory_manual* manual)
{
	struct npu_memory_manual_info* manual_info = npu_memory_manual_info_get();
    return &manual->region[manual_info->region_map.imb];
}
inline struct npu_memory_region* npu_memory_region_get_dva_expanded(struct npu_memory_manual* manual)
{
	struct npu_memory_manual_info* manual_info = npu_memory_manual_info_get();
    return &manual->region[manual_info->region_map.dva_expanded];
}

int npu_memory_manual_init(struct npu_system *system, struct npu_memory_manual* manual,
	struct npu_memory_manual_info* manual_info,	struct device* dev)
{
	if (!manual_info) {
		npu_err("no npu_memory_manual_info\n");
		return -EFAULT;
	}

	manual->num_regions = manual_info->num_regions;
	for (int i = 0; i < manual_info->num_regions; i++) {
		npu_memory_region_init(system, &manual->region[i],
			dev,
			manual,
			manual_info->region_info[i][0],
			manual_info->region_info[i][1],
			backward_first_fit_search
		);

		npu_info("region[%d] (0x%llx 0x%llx)\n", i,
			manual->region[i].iova_start, manual->region[i].iova_end);
	}

	return 0;
}

int npu_memory_manual_probe(struct npu_system* system, struct device* dev)
{
	struct npu_memory_manual* manual;
	struct npu_memory_manual_info* manual_info = npu_memory_manual_info_get();

	manual = kzalloc(sizeof(*manual), GFP_KERNEL);
	if (!manual) {
		probe_err("failed in npu_memory_manual allocation\n");
		return -ENOMEM;
	}

	default_set_iova_36bit();
	npu_memory_manual_info_set();
	npu_memory_region_buffer_hash_init();

	npu_memory_manual_init(system, manual, manual_info, dev);
	system->manual = manual;

	return 0;
}

void npu_memory_manual_deinit(struct npu_memory_manual* manual)
{
	int num_regions = manual->num_regions;
	for (int i = 0; i < num_regions; i++)
		npu_memory_region_deinit(&manual->region[i]);
	kfree(manual);
}

void npu_memory_manual_release(struct npu_system* system)
{
	npu_memory_manual_deinit(system->manual);
	system->manual = NULL;
}

void npu_memory_manual_dump(struct npu_memory_manual* manual)
{
	int num_regions = manual->num_regions;
	for (int i = 0; i < num_regions; i++)
		npu_memory_region_dump(&manual->region[i]);
}

void npu_memory_manual_health(struct npu_memory_manual* manual)
{
	int num_regions = manual->num_regions;
	for (int i = 0; i < num_regions; i++){
		npu_memory_region_health(&manual->region[i]);
	}
}

u64 first_fit_search(struct npu_memory_region* region,
	size_t size)
{
	u64 curr_start = ALIGNED(region->iova_start);
	struct npu_memory_buffer *buffer;

	if (list_empty(&region->region_list)) {
		if (curr_start + size <= region->iova_end)
			return curr_start;
		return 0;
	}

	list_for_each_entry(buffer, &region->region_list, list) {
		if (curr_start < buffer->dma.daddr) {
			if (curr_start + size <= buffer->dma.daddr)
				return curr_start;

			curr_start = ALIGNED(buffer->dma.daddr + buffer_size(buffer));
		}

		if (curr_start < buffer->dma.daddr + buffer_size(buffer))
			curr_start = ALIGNED(buffer->dma.daddr + buffer_size(buffer));

		if (curr_start >= region->iova_end)
			return 0;
	}

	if (curr_start + size <= region->iova_end)
		return curr_start;

	return 0;
}

u64 best_fit_search(struct npu_memory_region* region, size_t size)
{
	u64 best_start = 0;
	u64 best_gap = ~0UL;
	u64 curr_start = ALIGNED(region->iova_start);
	struct npu_memory_buffer *buffer;

	if (list_empty(&region->region_list)) {
		if (curr_start + size <= region->iova_end)
			return curr_start;
		return 0;
	}

	list_for_each_entry(buffer, &region->region_list, list) {
		if (curr_start < buffer->dma.daddr) {
			u64 gap_size = buffer->dma.daddr - curr_start;

			if (gap_size >= size && gap_size < best_gap) {
				best_gap = gap_size;
				best_start = curr_start;
			}
		}

		if (curr_start < buffer->dma.daddr + buffer_size(buffer))
			curr_start = ALIGNED(buffer->dma.daddr + buffer_size(buffer));

		if (curr_start >= region->iova_end)
			break;
	}

	if (curr_start < region->iova_end) {
		u64 gap_size = buffer->dma.daddr - curr_start;

		if (gap_size >= size && gap_size < best_gap) {
			best_gap = gap_size;
			best_start = curr_start;
		}
	}

	return best_start;
}

u64 backward_first_fit_search(struct npu_memory_region* region, size_t size)
{
	u64 curr_start = ALIGNED(region->iova_end);
	struct npu_memory_buffer *buffer;

	if (list_empty(&region->region_list)) {
		if (curr_start - size >= region->iova_start)
			return curr_start - size;
		return 0;
	}

	list_for_each_entry_reverse(buffer, &region->region_list, list) {
		u64 buffer_end = ALIGNED(buffer->dma.daddr + buffer_size(buffer));

		if (curr_start > buffer_end) {
			if (curr_start - size >= buffer_end)
				return curr_start - size;

			curr_start = buffer_end;
		}

		if (curr_start > buffer->dma.daddr) {
			curr_start = buffer->dma.daddr;
		}

		if (curr_start <= region->iova_start)
			return 0;
	}

	if (curr_start - size >= region->iova_start)
		return curr_start - size;

	return 0;
}

static int cmp(void *priv, const struct list_head *a, const struct list_head *b)
{
	struct npu_memory_buffer* a_buf = container_of(a, struct npu_memory_buffer, list);
	struct npu_memory_buffer* b_buf = container_of(b, struct npu_memory_buffer, list);
	long long diff = a_buf->dma.daddr - b_buf->dma.daddr;

	if (diff < 0)
		return -1;
	else if (diff > 0)
		return 1;
	else
		return 0;
}

u64 npu_memory_region_search_and_insert(struct npu_memory_region* region,
	struct npu_memory_buffer *buffer, size_t size)
{
	u64 daddr;
	unsigned long flags;

	spin_lock_irqsave(&region->region_lock, flags);
	list_sort(NULL, &region->region_list, cmp);
	daddr = region->search(region, size);
	if (!daddr) {
		spin_unlock_irqrestore(&region->region_lock, flags);
		npu_memory_region_remove(region, buffer);
		npu_err("failed region->search\n");
		return 0;
	}

	buffer->dma.daddr = daddr;

	list_add_tail(&buffer->list, &region->region_list);
	region->region_count++;
	spin_unlock_irqrestore(&region->region_lock, flags);

	return daddr;
}

void npu_memory_region_insert(struct npu_memory_region* region,
	struct npu_memory_buffer *buffer, dma_addr_t daddr, size_t size)
{
	unsigned long flags;

	spin_lock_irqsave(&region->region_lock, flags);
	buffer->dma.daddr = daddr;

	list_add_tail(&buffer->list, &region->region_list);
	region->region_count++;
	spin_unlock_irqrestore(&region->region_lock, flags);
}

void npu_memory_region_remove(struct npu_memory_region* region,
	struct npu_memory_buffer *buffer)
{
	unsigned long flags;

	spin_lock_irqsave(&region->region_lock, flags);
	if (likely(!list_empty(&buffer->list))) {
		list_del(&buffer->list);
		INIT_LIST_HEAD(&buffer->list);
		region->region_count--;
	} else
		npu_info("buffer[%pK] is not linked to region_lock. Skipping remove.\n", buffer);

	spin_unlock_irqrestore(&region->region_lock, flags);
}

void npu_memory_buffer_replace(struct npu_memory_region* region,
	struct npu_memory_buffer *src, struct npu_memory_buffer *dst)
{
	unsigned long flags = 0;
	npu_info("npu_memory_buffer_replace : daddr(0x%llx) size(%zu)\n",
			src->dma.daddr, src->dma.dma_buf->size);
	INIT_LIST_HEAD(&dst->list);

	spin_lock_irqsave(&region->region_lock, flags);
	list_replace_init(&src->list, &dst->list);
	spin_unlock_irqrestore(&region->region_lock, flags);
}

void npu_memory_region_replace(struct npu_memory_region* src_region,
	struct npu_memory_region* dst_region)
{
	unsigned long flags;

	spin_lock_irqsave(&src_region->region_lock, flags);
	spin_lock_irqsave(&dst_region->region_lock, flags);

	list_splice_tail_init(&src_region->region_list, &dst_region->region_list);

	spin_unlock_irqrestore(&dst_region->region_lock, flags);
	spin_unlock_irqrestore(&src_region->region_lock, flags);

	spin_lock_irqsave(&src_region->info_lock, flags);
	spin_lock_irqsave(&dst_region->info_lock, flags);

	list_splice_tail_init(&src_region->info_list, &dst_region->info_list);

	spin_unlock_irqrestore(&dst_region->info_lock, flags);
	spin_unlock_irqrestore(&src_region->info_lock, flags);
}

void npu_memory_region_init(struct npu_system *system, struct npu_memory_region *region,
	struct device *dev,	struct npu_memory_manual *manual,
	dma_addr_t iova_start, dma_addr_t iova_end,
	u64 (*search)(struct npu_memory_region*, size_t))
{
	region->system = system;
	region->dev = dev;
	region->manual = manual;
	region->iova_start = iova_start;
	region->iova_end = iova_end;
	region->search = search;

	spin_lock_init(&region->region_lock);
	INIT_LIST_HEAD(&region->region_list);
	region->region_count = 0;

	spin_lock_init(&region->info_lock);
	INIT_LIST_HEAD(&region->info_list);
	region->info_count = 0;

	mutex_init(&region->hash_lock);
}

void npu_memory_region_deinit(struct npu_memory_region *region)
{
	unsigned long flags;
	struct npu_memory_buffer *buffer, *temp_buffer;
	struct npu_memory_debug_info *info, *temp_info;

	if (!region) {
		npu_info("npu_memory_region is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&region->region_lock, flags);
	if (!list_empty(&region->region_list)) {
		list_for_each_entry_safe(buffer, temp_buffer, &region->region_list, list) {
			list_del(&buffer->list);
		}
	}
	spin_unlock_irqrestore(&region->region_lock, flags);

	spin_lock_irqsave(&region->info_lock, flags);
	if (!list_empty(&region->info_list)) {
		list_for_each_entry_safe(info, temp_info, &region->info_list, list) {
			list_del(&info->list);
		}
	}
	spin_unlock_irqrestore(&region->info_lock, flags);
}

struct dma_buf *npu_memory_get_dma_buf(int fd, size_t size, const char *heap_name)
{
	struct dma_buf *dma_buf = NULL;
	struct dma_heap *dma_heap;

	if (fd)
		return dma_buf_get(fd);

	dma_heap = dma_heap_find(heap_name);
	if (dma_heap) {
		dma_buf = dma_heap_buffer_alloc(dma_heap, size, 0, 0);
		dma_heap_put(dma_heap);
	} else {
		npu_info("dma_heap is not exist\n");
	}

	return dma_buf;
}

static int __npu_memory_region_iommu_map(struct npu_memory_region* region,
	struct npu_memory_buffer *buffer, bool is_cacheable, int prot)
{
	int ret = 0;
	size_t map_size = 0;
	int iommu_attributes = 0;
	struct device *dev;
	dma_addr_t daddr;
	struct iommu_domain *domain;

	if (is_cacheable)
		dev = region->dev;
	else
		dev = get_non_share_dev();

	domain = iommu_get_domain_for_dev(dev);
	buffer->domain_dev = dev;

	daddr = npu_memory_region_search_and_insert(region, buffer,
		buffer->dma.dma_buf->size);
	if (!daddr) {
		ret = -ENOMEM;
		probe_err("fail(%d) in npu_memory_region_search_and_insert\n", ret);
		return ret;
	}

	npu_info("npu_memory_region_search_and_insert : daddr(0x%llx) size(%zu) region(0x%p)\n",
		daddr, buffer->dma.dma_buf->size, region);

	if (prot)
		iommu_attributes |= ((prot) << IOMMU_PRIV_SHIFT);

	if (is_cacheable)
		iommu_attributes |= IOMMU_CACHE;

	if (likely(domain)) {
		map_size = iommu_map_sgtable(domain, daddr, buffer->dma.sgt, iommu_attributes);
	} else {
		probe_err("fail(err %pad) in iommu_get_domain_for_dev\n", domain);
		ret = -ENOMEM;
		return ret;
	}

	npu_sgt_flush(buffer->dma.sgt, dev);

	if (map_size != buffer->dma.size) {
		npu_info("iommu_map_sgtable return %lu, we want to map %lu\n",
			map_size, buffer->dma.size);
	}

	return ret;
}

int __npu_memory_region_map(struct npu_memory_region *region, struct npu_memory_buffer *buffer,
	int prot, u32 lazy_unmap, bool is_vmap_req, enum dma_data_direction dir)
{
	int ret = 0;

	void *vaddr;
	bool is_cacheable;
	struct dma_buf *dma_buf = buffer->dma.dma_buf;

	if (buffer->dma.dma_buf->size < buffer->dma.size) {
		npu_err("Allocate buffer size(%zu) is smaller than expectation(%u)\n",
			buffer->dma.dma_buf->size, (unsigned int)buffer->dma.size);
		ret = -EINVAL;
		goto p_err;
	}

	is_cacheable = is_cacheable_dma_buf(dma_buf);

	if (!get_unset_iommu_sgt(buffer, is_cacheable, prot, lazy_unmap, dir)) {
		npu_err("get_unset_iommu_sgt is failed(%pK)\n", dma_buf);
		ret = -ENOMEM;
		goto p_err;
	}

	ret = __npu_memory_region_iommu_map(region, buffer, is_cacheable, prot);
	if (ret) {
		npu_err("__npu_memory_region_iommu_map is failed(%d)\n", ret);
		goto p_err;
	}

	if (is_vmap_req) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR(vaddr)) {
			npu_err("Failed to get vaddr (err %pK)\n", vaddr);
			ret = -EFAULT;
			goto p_err;
		}
		buffer->dma.vaddr = vaddr;
	}

	npu_dbg("buffer[%llx], dma_buf[%llx], paddr[%llx], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		(unsigned long long)buffer, (u64)buffer->dma.dma_buf, buffer->paddr, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt,
		buffer->dma.attachment, buffer->dma.size);

p_err:
	return ret;
}

int npu_memory_region_map(struct npu_memory_region *region, struct npu_session *session,
			struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req)
{
	int ret = 0;
	struct dma_buf *dma_buf;

	if (unlikely(!region)) {
		npu_err("Failed to get npu_region\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	npu_info("Try mapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);
	mutex_lock(&region->hash_lock);

	npu_memory_region_init_buffer(buffer, session->pid, true);

	if (npu_memory_region_check_used_buffer(region, buffer))
		goto exist_buffer;

	dma_buf = npu_memory_get_dma_buf(buffer->usr.fd, 0, NULL);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is fail(%pK)\n", dma_buf);
		mutex_unlock(&region->hash_lock);
		return -EINVAL;
	}
	buffer->dma.dma_buf = dma_buf;

	ret = __npu_memory_region_map(region, buffer, prot, lazy_unmap, is_vmap_req, DMA_BIDIRECTIONAL);
	if (ret) {
		mutex_unlock(&region->hash_lock);
		npu_memory_region_unmap(region, buffer);
		npu_memory_manual_dump(region->manual);
		return ret;
	}

	if (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC)
		npu_memory_region_alloc_dbg_info(region, buffer->dma.dma_buf, buffer->dma.size);

	npu_memory_region_buffer_hash_add(region, buffer);
exist_buffer:
	mutex_unlock(&region->hash_lock);
	return ret;
}

int npu_memory_region_alloc(struct npu_memory_region *region, struct npu_session *session,
				struct npu_memory_buffer *buffer, u32 lazy_unmap, int prot)
{
	int ret = 0;
	struct dma_buf *dma_buf;

	if (unlikely(!region)) {
		npu_err("Failed to get npu_region\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	if (!buffer->dma.size)
		return 0;

	npu_memory_region_init_buffer(buffer, session->pid, true);

	dma_buf = npu_memory_get_dma_buf(false, buffer->dma.size, DMA_HEAP_SYSTEM_UNCACHED);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("npu_memory_ion_alloc_secure is fail(%pK) size(%zu)\n",
				dma_buf, buffer->dma.size);
		return -EINVAL;
	}
	buffer->dma.dma_buf = dma_buf;

	ret = __npu_memory_region_map(region, buffer, prot, lazy_unmap, true, DMA_BIDIRECTIONAL);
	if (ret) {
		npu_memory_region_free(region, buffer);
		npu_memory_manual_dump(region->manual);
	}

	return ret;
}

int npu_memory_region_map_retry(struct npu_memory_region *region, struct npu_session *session,
	struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req)
{
	int ret = 0;
	bool complete_suc = false;
	bool is_cacheable;
	size_t retry_size = 0;
	dma_addr_t daddr = 0;

	struct dma_buf *dma_buf;
	void *vaddr;
	struct device *dev;

	if (unlikely(!region)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	npu_info("Retry mapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);
	mutex_lock(&region->hash_lock);

	npu_memory_region_init_buffer(buffer, session->pid, true);

	if (npu_memory_region_check_used_buffer(region, buffer))
		goto exist_buffer;

	dma_buf = dma_buf_get(buffer->usr.fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is failed(%pK)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma.dma_buf = dma_buf;

	if (buffer->dma.dma_buf->size < buffer->dma.size) {
		npu_err("Allocate buffer size(%zu) is smaller than expectation(%u)\n",
			buffer->dma.dma_buf->size, (unsigned int)buffer->dma.size);
		ret = -EINVAL;
		goto p_err;
	}

	retry_size = ALIGN(buffer->dma.size, NPU_IMB_GRANULARITY);
	npu_info("retry size must align 0x%x. (full size : 0x%lx, aligned size : 0x%lx)\n",
			NPU_IMB_GRANULARITY, buffer->dma.size, retry_size);
	if (rent_imb_space(region->system, &daddr, retry_size, 1)) {
		npu_err("There are no space on IMB for another buffer\n");
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->imb_for_ncp = true;

	is_cacheable = is_cacheable_dma_buf(dma_buf);

	if (!get_unset_iommu_sgt(buffer, is_cacheable, prot, lazy_unmap, DMA_BIDIRECTIONAL)) {
		npu_err("get_unset_iommu_sgt is failed(%pK)\n", dma_buf);
		ret = -ENOMEM;
		goto p_err;
	}

	dev = region->dev;
	{
		unsigned long l_flags;

		ret = dma_buf_get_flags(dma_buf, &l_flags);
		if (ret) {
			npu_err("dma_buf_get_flags is failed\n");
			ret = -EINVAL;
			goto p_err;
		}

		if (npu_memory_dma_heap_flags_uncached(l_flags)) {
			npu_info("This DMABUF is non-cacheable\n");
			dev = get_non_share_dev();
		}
	}
	buffer->mapped_dev = dev;

	{
		size_t map_size = 0;
		int iommu_attributes;
		struct iommu_domain *domain = iommu_get_domain_for_dev(dev);

		if (prot)
			iommu_attributes = ((prot) << IOMMU_PRIV_SHIFT);

		if (likely(domain)) {
			map_size = iommu_map_sgtable(domain, daddr, buffer->dma.sgt, iommu_attributes);
		} else {
			probe_err("fail(err %pad) in iommu_map_sg\n", &daddr);
			ret = -ENOMEM;
			goto p_err;
		}

		npu_sgt_flush(buffer->dma.sgt, dev);

		buffer->dma.daddr = daddr;

		if (map_size != buffer->dma.size) {
			probe_info("iommu_map_sgtable return %lu, we want to map %lu\n",
				map_size, buffer->dma.size);
		}
	}

	if (is_vmap_req) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR(vaddr)) {
			npu_err("Failed to get vaddr (err %pK)\n", vaddr);
			ret = -EFAULT;
			goto p_err;
		}
		buffer->dma.vaddr = vaddr;
	}

	if (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC)
		npu_memory_region_alloc_dbg_info(region, buffer->dma.dma_buf, buffer->dma.size);

	npu_memory_region_buffer_hash_add(region, buffer);
exist_buffer:
	mutex_unlock(&region->hash_lock);
	npu_dbg("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attach[%pK], size[%zu]\n",
		buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment, buffer->dma.size);
	complete_suc = true;
p_err:
	if (complete_suc != true) {
		npu_memory_region_unmap(region, buffer);
		npu_memory_region_dump(region);
	}
	return ret;
}

void __npu_memory_region_unmap(struct npu_memory_region *region,
	struct npu_memory_buffer *buffer, bool mapped)
{
	if (buffer->manual_dup)
		goto clear_buffer;

	if (buffer->imb_for_ncp)
		return_imb_space(region->system, 1);

	if (!IS_ERR_OR_NULL(buffer->dma.vaddr))
		npu_dma_buf_vunmap(buffer->dma.dma_buf, buffer->dma.vaddr);
	if (buffer->dma.daddr && !IS_ERR_VALUE(buffer->dma.daddr))
		npu_memory_dma_buf_dva_unmap(buffer, true);
	if (!IS_ERR_OR_NULL(buffer->dma.sgt))
		dma_buf_unmap_attachment_unlocked(buffer->dma.attachment, buffer->dma.sgt, DMA_BIDIRECTIONAL);
	if (!IS_ERR_OR_NULL(buffer->dma.attachment) && !IS_ERR_OR_NULL(buffer->dma.dma_buf))
		dma_buf_detach(buffer->dma.dma_buf, buffer->dma.attachment);
	if (!IS_ERR_OR_NULL(buffer->dma.dma_buf)) {
		dma_buf_put(buffer->dma.dma_buf);
		if (mapped && (buffer->usr.reserved != VS4L_MEMORY_DMABUF_DYNAMIC))
			npu_memory_region_update_dbg_info(region, buffer->dma.dma_buf);
	}

	npu_memory_region_remove(region, buffer);
clear_buffer:
	npu_memory_region_init_buffer(buffer, 0, false);
}

void npu_memory_region_unmap(struct npu_memory_region *region,
	struct npu_memory_buffer *buffer)
{
	npu_info("Try unmapping DMABUF fd=%d size=%zu\n", buffer->usr.fd, buffer->dma.size);
	npu_trace("buffer[%pK], vaddr[%pK], daddr[%llx], sgt[%pK], attachment[%pK]\n",
			buffer, buffer->dma.vaddr, buffer->dma.daddr, buffer->dma.sgt, buffer->dma.attachment);

	if (!buffer->manual_dup && !IS_ERR_OR_NULL(buffer->dma.dma_buf))
		npu_memory_region_buffer_hash_del(region, buffer->dma.dma_buf);

	__npu_memory_region_unmap(region, buffer, true);
}

int npu_memory_region_free(struct npu_memory_region *region,
	struct npu_memory_buffer *buffer)
{
	if (unlikely(!region)) {
		npu_err("Failed to get npu_memory\n");
		return -EINVAL;
	}

	if (unlikely(!buffer)) {
		npu_err("Failed to get npu_memory_buffer\n");
		return -EINVAL;
	}

	if (!buffer->dma.size)
		return 0;

	__npu_memory_region_unmap(region, buffer, false);
	return 0;
}

int npu_memory_region_alloc_from_heap(struct platform_device *pdev,
		struct npu_session *session, struct npu_memory_buffer *buffer,
		dma_addr_t daddr, struct npu_memory_region *region, const char *heapname, int prot)
{
	int ret = 0;
	bool complete_suc = false;

	struct device *dev;
	phys_addr_t phys_addr;

	size_t size;
	size_t map_size = 0;
	int iommu_attributes = 0;

	if (region->iova_start > daddr || region->iova_end <= daddr) {
		ret = -ERANGE;
		probe_err("daddr [0x%llx] is not in range [0x%llx, 0x%llx]\n",
				daddr, region->iova_start, region->iova_end);
		goto p_err;
	}

	npu_memory_region_init_buffer(buffer, session->pid, true);
	strncpy(buffer->name, heapname, strlen(heapname));

	size = buffer->dma.size;
	dev = get_non_share_dev();
	{
		struct iommu_domain *domain = iommu_get_domain_for_dev(dev);
		buffer->domain_dev = dev;

		ret = npu_memory_create_sgt(buffer, size);
		if (ret) {
			probe_err("fail(err %d) in npu_memory_create_sgt\n", ret);
			ret = -ENOMEM;
			goto p_err;
		}

		mb();

		npu_memory_region_insert(region, buffer, daddr, size);
		npu_memory_trace_for_vh(buffer->dma.size, true);

		if (prot)
			iommu_attributes |= ((prot) << IOMMU_PRIV_SHIFT);

		if (likely(domain)) {
			map_size = iommu_map_sgtable(domain, daddr, buffer->dma.sgt, iommu_attributes);
		} else {
			probe_err("fail(err %pad) in iommu_get_domain_for_dev\n", domain);
			ret = -ENOMEM;
			goto p_err;
		}

		npu_sgt_flush(buffer->dma.sgt, dev);

		if (map_size != size) {
			probe_info("iommu_map_sgtable return %lu, we want to map %lu",
				map_size, size);
		}
		buffer->dma.size = map_size;

		ret = npu_memory_vmap(buffer, size);
		if (ret) {
			probe_err("fail(err %d) in npu_memory_vmap\n", ret);
			goto p_err;
		}
	}

	/* But, first phys. Not all */
	phys_addr = sg_phys(buffer->dma.sgt->sgl);
	buffer->paddr = phys_addr;

	complete_suc = true;
p_err:
	if (complete_suc != true) {
		npu_memory_region_free_from_heap(region, buffer);
		npu_memory_manual_dump(region->manual);
	}
	return ret;
}

void npu_memory_region_free_from_heap(struct npu_memory_region *region, struct npu_memory_buffer *buffer)
{
	struct iommu_domain *domain = iommu_get_domain_for_dev(buffer->domain_dev);

	if (buffer->manual_dup)
		goto clear_buffer;

	if (buffer->dma.vaddr)
		vunmap(buffer->dma.vaddr);
	if (buffer->dma.daddr && !IS_ERR_VALUE(buffer->dma.daddr))
		iommu_unmap(domain, buffer->dma.daddr, buffer->dma.size);
	if (buffer->dma.sgt)
		npu_memory_free_sgt(buffer->dma.sgt);

	npu_memory_trace_for_vh(buffer->dma.size, false);
	npu_memory_region_remove(region, buffer);
clear_buffer:
	npu_memory_region_init_buffer(buffer, 0, false);
}

struct npu_memory_buffer *
npu_memory_region_copy(struct npu_memory_region *region, struct npu_session *session,
			struct npu_memory_buffer *buffer, u32 lazy_unmap, size_t offset, size_t size)
{
	struct npu_memory_buffer *copy_buffer;
	void *vaddr;
	int ret;

	if (!buffer->dma.dma_buf) {
		npu_err("no dmabuf in source buffer\n");
		return ERR_PTR(-EINVAL);
	}

	if (offset >= buffer->dma.dma_buf->size || offset + size > buffer->dma.dma_buf->size) {
		npu_err("invalid size args. size=0x%lx, offset=0x%lx, src size=0x%lx\n",
			size, offset, buffer->dma.size);
		return ERR_PTR(-EINVAL);
	}

	copy_buffer = kzalloc(sizeof(struct npu_memory_buffer), GFP_KERNEL);
	if (!copy_buffer)
		return ERR_PTR(-ENOMEM);

	/* Copy source buffer properties. */
	memcpy(copy_buffer, buffer, sizeof(*buffer));

	copy_buffer->dma.size = size;

	/*
	 * We do memcpy from user NCP header to copy_buffer.
	 * Hence, allocate cached dma-buf heap.
	 */
	ret = npu_memory_region_alloc(region, session, copy_buffer, lazy_unmap, npu_get_configs(NPU_PBHA_HINT_00));
	if (ret) {
		npu_err("failed to allocate copy buffer(%d)\n", ret);
		goto err_free_mem;
	}

	/* Create virtual mapping for CPU access. */
	if (!buffer->dma.vaddr) {
		vaddr = npu_dma_buf_vmap(buffer->dma.dma_buf);
		if (IS_ERR_OR_NULL(vaddr)) {
			npu_err("failed to vmap original dmabuf(%ld)", PTR_ERR(vaddr));
			goto err_free_copy_buffer;
		}

		buffer->dma.vaddr = vaddr;
	}

	vaddr = buffer->dma.vaddr + offset;

	/* Begin CPU access. */
	dma_buf_begin_cpu_access(buffer->dma.dma_buf, DMA_BIDIRECTIONAL);
	dma_buf_begin_cpu_access(copy_buffer->dma.dma_buf, DMA_BIDIRECTIONAL);

	memcpy(copy_buffer->dma.vaddr, vaddr, size);

	/* End CPU access.. */
	dma_buf_end_cpu_access(copy_buffer->dma.dma_buf, DMA_BIDIRECTIONAL);
	dma_buf_end_cpu_access(buffer->dma.dma_buf, DMA_BIDIRECTIONAL);

	return copy_buffer;
err_free_copy_buffer:
	npu_memory_region_free(region, copy_buffer);
err_free_mem:
	kfree(copy_buffer);
	return ERR_PTR(ret);
}

void npu_memory_region_alloc_dbg_info(struct npu_memory_region *region,
	struct dma_buf *dmabuf, size_t size)
{
	bool skip = false;
	unsigned long flags;
	struct npu_memory_debug_info *info, *dbg_info, *temp;

	if (!region) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&region->info_lock, flags);
	list_for_each_entry_safe(info, temp, &region->info_list, list) {
		if (info->dma_buf != (u64)dmabuf)
			continue;

		atomic64_set(&info->ref_count, atomic64_read(&dmabuf->file->f_count));
		skip = true;
		break;
	}

	if (skip)
		goto done;

	dbg_info = kzalloc(sizeof(struct npu_memory_debug_info), GFP_ATOMIC);
	if (!dbg_info) {
		npu_err("there is no memory for npu_memory_debug_info\n");
		goto p_err;
	}
	dbg_info->dma_buf = (u64)dmabuf;
	dbg_info->size = size;
	dbg_info->pid = task_pid_nr(current);
	strncpy(dbg_info->comm, current->comm, TASK_COMM_LEN);
	atomic64_set(&dbg_info->ref_count, atomic64_read(&dmabuf->file->f_count));
	list_add_tail(&dbg_info->list, &region->info_list);
	region->info_count++;
done:
p_err:
	spin_unlock_irqrestore(&region->info_lock, flags);
}

void npu_memory_region_update_dbg_info(struct npu_memory_region *region,
	struct dma_buf *dmabuf)
{
	unsigned long flags;
	struct npu_memory_debug_info *info, *temp;

	if (!region) {
		npu_info("npu_memory is NULL, skip this func.\n");
		return;
	}

	spin_lock_irqsave(&region->info_lock, flags);
	list_for_each_entry_safe(info, temp, &region->info_list, list) {
		if (info->dma_buf != (u64)dmabuf)
			continue;

		if (atomic64_read(&dmabuf->file->f_count) > 2) {
			atomic64_set(&info->ref_count, atomic64_read(&dmabuf->file->f_count));
			continue;
		}

		region->info_count--;
		list_del(&info->list);
		kfree(info);
	}
	spin_unlock_irqrestore(&region->info_lock, flags);
}

void npu_memory_region_health(struct npu_memory_region *region)
{
	unsigned long flags;
	struct npu_memory_buffer *buffer;
	struct npu_memory_debug_info *dbg_info;
	size_t	region_size = 0;

	if (!region)
		return;

	npu_health("region (0x%llx 0x%llx)\n", region->iova_start, region->iova_end);

	spin_lock_irqsave(&region->region_lock, flags);
	npu_health("---------- NPU Memory region list ----------\n");
	list_for_each_entry(buffer, &region->region_list, list) {
		region_size += buffer->dma.size;
	}
	npu_health("npu heap memory(region) total = %lu\n", region_size);
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&region->region_lock, flags);

	spin_lock_irqsave(&region->info_lock, flags);
	npu_health("---------- NPU Memory dbg info list ----------\n");
	npu_health("memory_info_count : %u\n", region->info_count);
	list_for_each_entry(dbg_info, &region->info_list, list) {
		npu_health("PID[%d] CUR[%s] SIZE[%lu] REF_CNT[%lld]\n",
			dbg_info->pid, dbg_info->comm, dbg_info->size,
			atomic64_read(&dbg_info->ref_count));
	}
	npu_health("------------------------------------------------\n");
	spin_unlock_irqrestore(&region->info_lock, flags);
}

void npu_memory_region_dump(struct npu_memory_region *region)
{
	unsigned long flags;
	struct npu_memory_buffer *buffer;

	if (!region)
		return;

	spin_lock_irqsave(&region->region_lock, flags);
	npu_dump("---------- NPU Memory dbg region list ----------\n");
	npu_dump("memory_region_count : %u\n", region->region_count);
	list_for_each_entry(buffer, &region->region_list, list) {
		npu_memory_buffer_dump(buffer);
		if (buffer->ncp)
			npu_dump("Memory name = %s\n", buffer->name);
	}
	npu_dump("------------------------------------------------\n");
	spin_unlock_irqrestore(&region->region_lock, flags);
}

void npu_memory_region_buffer_hash_init(void)
{
	hash_init(npu_memory_buffer_hash_table);
}

struct npu_memory_buffer *npu_memory_region_buffer_hash_get(struct npu_memory_region *region, struct dma_buf *dma_buf)
{
	struct npu_memory_buffer *buffer = NULL;
	u64 hash_key = hash_64((u64)dma_buf, HASH_BITS(npu_memory_buffer_hash_table));

	hlist_for_each_entry(buffer, &npu_memory_buffer_hash_table[hash_key], hash_node) {
		if (buffer->dma.dma_buf == dma_buf) {
			npu_info("buffer->dma.dma_buf(0x%llx), dma_buf(0x%llx)\n", (u64)buffer->dma.dma_buf, (u64)dma_buf);
			return buffer;
		}
	}

	return NULL;
}

int npu_memory_region_buffer_hash_add(struct npu_memory_region *region, struct npu_memory_buffer *buffer)
{
	u64 hash_key = 0;

	if (!buffer) {
		npu_err("buffer is NULL\n");
		return -EINVAL;
	}

	if (!buffer->dma.dma_buf) {
		npu_err("dma_buf is NULL\n");
		return -EINVAL;
	}

	hash_key = hash_64((u64)buffer->dma.dma_buf, HASH_BITS(npu_memory_buffer_hash_table));

	npu_info("hash_key(%llu), buffer->dma.dma_buf(0x%llx)\n", hash_key, (u64)buffer->dma.dma_buf);
	INIT_HLIST_NODE(&buffer->hash_node);
	hlist_add_head(&buffer->hash_node, &npu_memory_buffer_hash_table[hash_key]);

	return 0;
}

int npu_memory_region_buffer_hash_del(struct npu_memory_region *region, struct dma_buf *dma_buf)
{
	struct npu_memory_buffer *buffer = NULL;
	u64 hash_key = hash_64((u64)dma_buf, HASH_BITS(npu_memory_buffer_hash_table));

	mutex_lock(&region->hash_lock);
	hlist_for_each_entry(buffer, &npu_memory_buffer_hash_table[hash_key], hash_node) {
		if (!buffer->dma.dma_buf)
			continue;

		if (buffer->dma.dma_buf == dma_buf) {
			hlist_del(&buffer->hash_node);
			npu_info("succeed to find buffer for 0x%llx\n", (u64)dma_buf);
			mutex_unlock(&region->hash_lock);
			return 0;
		}
	}

	npu_err("Failed to find buffer for dma_buf(0x%llx)\n", (u64)dma_buf);
	mutex_unlock(&region->hash_lock);
	return -ENOENT;
}
