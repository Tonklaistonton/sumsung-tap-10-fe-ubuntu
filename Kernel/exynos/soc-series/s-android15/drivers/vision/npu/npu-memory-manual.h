#ifndef _NPU_MEMORY_MANUAL_H_
#define _NPU_MEMORY_MANUAL_H_

#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#include <linux/types.h>

/* SysMMU DMA Heap Names */
#define DMA_HEAP_SYSTEM "system"
#define DMA_HEAP_SYSTEM_UNCACHED "system-uncached"
#define DMA_HEAP_SECURE_DSP "secure_dsp"

/* For 32 bit dva address */
#define HEAP_START	0x11100000
#define HEAP_END	0x30000000
#define IMB_START	HEAP_END
#define IMB_END		0x70000000
#define IOVA_START	IMB_END
#define IOVA_END	0x100000000

/* For 36 bit expanded dva address */
#define HEAP_EXPANDED_START	0x111000000
#define HEAP_EXPANDED_END	0x123000000
#define IMB_EXPANDED_START	HEAP_EXPANDED_END
#define IMB_EXPANDED_END	0x163000000
#define IOVA_NORMAL_START	IMB_EXPANDED_END
#define IOVA_NORMAL_END		0x200000000
#define IOVA_EXPANDED_START	IMB_EXPANDED_END
#define IOVA_EXPANDED_END	0x200000000

/* For 36 bit expanded region map */
#define MAP_HEAP	0
#define MAP_DVA	1
#define MAP_IMB	2
#define MAP_DVA_EXPANDED	3
#define MAP_DVA_NUM	4

#define ALIGNED(x) ALIGN(x, PAGE_SIZE)
#define buffer_size(buffer) ((buffer->dma.dma_buf) ? \
	(buffer->dma.dma_buf->size) : (buffer->dma.size))

struct npu_system;
struct npu_memory_buffer;
struct npu_memory;

struct npu_memory_region {
	struct device *dev;
	struct npu_system *system;
	struct npu_memory_manual *manual;

	u64 iova_start;
	u64 iova_end;
	u64 (*search)(struct npu_memory_region* region, size_t size);

	spinlock_t			region_lock;
	struct list_head		region_list;
	u32				region_count;

	spinlock_t			info_lock;
	struct list_head		info_list;
	u32				info_count;

	struct mutex			hash_lock;
};

struct npu_memory_manual {
	int num_regions;
	struct npu_memory_region region[MAP_DVA_NUM];
};

struct npu_memory_manual_info {
	bool iova_36bit;
	int num_regions;
	struct {
		int heap;
		int dva;
		int imb;
		int dva_expanded;
	} region_map;
	dma_addr_t region_info[MAP_DVA_NUM][2];
};

void default_set_iova_36bit(void);
void set_iova_36bit(bool iova_36bit);
bool get_iova_36bit(void);
struct npu_memory_manual_info *npu_memory_manual_info_get(void);
bool npu_memory_region_check_used_buffer(struct npu_memory_region *region, struct npu_memory_buffer *buffer);
void npu_memory_manual_info_noti(struct npu_memory_manual_info* manual_info,
	struct npu_system* system);
struct npu_memory_region* npu_memory_region_get_heap(struct npu_memory_manual* manual);
struct npu_memory_region* npu_memory_region_get_dva(struct npu_memory_manual* manual);
struct npu_memory_region* npu_memory_region_get_imb(struct npu_memory_manual* manual);
struct npu_memory_region* npu_memory_region_get_dva_expanded(struct npu_memory_manual* manual);

u64 first_fit_search(struct npu_memory_region* region, size_t size);
u64 best_fit_search(struct npu_memory_region* region, size_t size);
u64 backward_first_fit_search(struct npu_memory_region* region, size_t size);
u64 npu_memory_region_search_and_insert(struct npu_memory_region* region,
	struct npu_memory_buffer *buffer, size_t size);
void npu_memory_region_insert(struct npu_memory_region* region,
	struct npu_memory_buffer *buffer, dma_addr_t daddr, size_t size);
void npu_memory_region_remove(struct npu_memory_region* region,
	struct npu_memory_buffer *buffer);
void npu_memory_buffer_replace(struct npu_memory_region* region,
	struct npu_memory_buffer *src, struct npu_memory_buffer *dest);
void npu_memory_region_replace(struct npu_memory_region* src_region,
	struct npu_memory_region* dst_region);
void npu_memory_region_init(struct npu_system *system, struct npu_memory_region *region,
	struct device *dev,	struct npu_memory_manual *manual,
	dma_addr_t iova_start, dma_addr_t iova_end,
	u64 (*search)(struct npu_memory_region*, size_t));
void npu_memory_region_deinit(struct npu_memory_region *region);
struct dma_buf *npu_memory_get_dma_buf(int fd, size_t size, const char *cached);
int __npu_memory_region_map(struct npu_memory_region *region, struct npu_memory_buffer *buffer,
	int prot, u32 lazy_unmap, bool is_vmap_req, enum dma_data_direction dir);
void __npu_memory_region_unmap(struct npu_memory_region *region,
	struct npu_memory_buffer *buffer, bool mapped);

int npu_memory_manual_init(struct npu_system *system, struct npu_memory_manual* manual,
	struct npu_memory_manual_info* manual_info,	struct device* dev);
void npu_memory_manual_deinit(struct npu_memory_manual* manual);
void npu_memory_manual_dump(struct npu_memory_manual* manual);

int npu_memory_region_map(struct npu_memory_region *region, struct npu_session *session,
			struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req);
int npu_memory_region_map_retry(struct npu_memory_region *region, struct npu_session *session,
	struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req);
int npu_memory_region_alloc(struct npu_memory_region *region, struct npu_session *session,
				struct npu_memory_buffer *buffer, u32 lazy_unmap, int prot);
void npu_memory_region_unmap(struct npu_memory_region *region, struct npu_memory_buffer *buffer);
int npu_memory_region_free(struct npu_memory_region *region, struct npu_memory_buffer *buffer);
int npu_memory_region_alloc_from_heap(struct platform_device *pdev,
		struct npu_session *session, struct npu_memory_buffer *buffer,
		dma_addr_t daddr, struct npu_memory_region *region, const char *heapname, int prot);
void npu_memory_region_free_from_heap(struct npu_memory_region *region,
	struct npu_memory_buffer *buffer);
struct npu_memory_buffer *
npu_memory_region_copy(struct npu_memory_region *region, struct npu_session *session,
			struct npu_memory_buffer *buffer, u32 lazy_unmap,size_t offset, size_t size);
void npu_memory_manual_health(struct npu_memory_manual* manual);
int npu_memory_manual_probe(struct npu_system* system, struct device* dev);
void npu_memory_manual_release(struct npu_system* system);

void npu_memory_region_alloc_dbg_info(struct npu_memory_region *region,
	struct dma_buf *dmabuf, size_t size);
void npu_memory_region_update_dbg_info(struct npu_memory_region *region,
	struct dma_buf *dmabuf);
void npu_memory_region_health(struct npu_memory_region *region);
void npu_memory_region_dump(struct npu_memory_region *region);
void npu_memory_region_buffer_hash_init(void);
struct npu_memory_buffer *npu_memory_region_buffer_hash_get(struct npu_memory_region *region, struct dma_buf *dma_buf);
int npu_memory_region_buffer_hash_add(struct npu_memory_region *region, struct npu_memory_buffer *buffer);
int npu_memory_region_buffer_hash_del(struct npu_memory_region *region, struct dma_buf *dma_buf);

#endif
