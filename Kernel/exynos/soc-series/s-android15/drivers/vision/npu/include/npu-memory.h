#ifndef _NPU_MEMORY_H_
#define _NPU_MEMORY_H_

#include <linux/platform_device.h>
#include <media/videobuf2-core.h>
#if IS_ENABLED(CONFIG_VIDEOBUF2_CMA_PHYS)
#include <media/videobuf2-cma-phys.h>
#elif IS_ENABLED(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif
#include <linux/hashtable.h>

#include "vs4l.h"
#include "npu-common.h"

#define NAME_LEN	50
struct npu_memory_region;
struct npu_usr_buf {
	__u32 	offset;
	__s32 	fd;
	__u64 	reserved;
};

struct npu_dma_buf {
	void	*vaddr;
	size_t	size;
	dma_addr_t	daddr;
	struct dma_buf	*dma_buf;
	struct sg_table	*sgt;
	struct dma_buf_attachment	*attachment;
};

struct npu_memory_buffer {
	struct npu_usr_buf 	usr;
	struct npu_dma_buf 	dma;
	struct list_head	list;
	struct list_head	imb_map_list;
	struct hlist_node hash_node;
	phys_addr_t	paddr;
	size_t	used_size;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	size_t	ncp_max_size;
#endif
	struct device *mapped_dev;
	char name[NAME_LEN];
	bool	ncp;
	bool	imb_for_ncp;
	bool	imb_for_iofm;
	pid_t	pid;
	bool	imb_dup;
	bool	manual_dup;
	struct device *domain_dev;
	bool npu_sgt;
};

struct npu_memory_v_buf {
	struct list_head		list;
	u8				*v_buf;
	size_t				size;
	char name[NAME_LEN];
};

struct npu_memory_debug_info {
	u64	dma_buf;
	size_t	size;
	pid_t	pid;
	char	comm[TASK_COMM_LEN];
	atomic_long_t	ref_count;
	struct list_head		list;
};

enum {
	NORMAL_SYSMMU = 0,
	VIRTUAL_SYSMMU = 1,
	NUM_OF_SYSMMU,
};

struct npu_memory {
	struct device			*dev;
	struct npu_system		*system;

	struct mutex sysmmu_state_lock;
	u32 last_sysmmu_state;

	spinlock_t			valloc_lock;
	struct list_head		valloc_list;
	u32				valloc_count;

	spinlock_t			map_imb_lock;
	struct list_head		map_imb_list;
	u32				map_imb_count;

	spinlock_t			map_lock;
	struct list_head		map_list;
	u32				map_count;

	spinlock_t			alloc_lock;
	struct list_head		alloc_list;
	u32				alloc_count;

	spinlock_t			info_lock;
	struct list_head		info_list;
	u32				info_count;
};

struct device* get_non_share_dev(void);
struct device* get_manual_dev(void);
struct device* get_nshare_manual_dev(void);
void npu_memory_trace_for_vh(unsigned int size, bool is_add);
int npu_memory_probe(struct npu_memory *memory, struct device *dev);
int npu_memory_v_alloc(struct npu_memory *memory, struct npu_memory_v_buf *buffer);
void npu_memory_v_free(struct npu_memory *memory, struct npu_memory_v_buf *buffer);
void *npu_dma_buf_vmap(struct dma_buf *dmabuf);
void npu_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr);
dma_addr_t npu_memory_dma_buf_dva_map(struct npu_memory_buffer *buffer);
void npu_memory_dma_buf_dva_unmap(__attribute__((unused))struct npu_memory_buffer *buffer, bool mapping);
unsigned long npu_memory_set_prot(int prot, struct dma_buf_attachment *attachment);
void npu_dma_buf_add_attr(struct dma_buf_attachment *attachment);
void npu_sgt_flush(struct sg_table *sgt, struct device *dev);
bool is_cacheable_dma_buf(struct dma_buf *dma_buf);
struct sg_table *get_unset_iommu_sgt(struct npu_memory_buffer *buffer,
	bool is_cacheable, int prot, u32 lazy_unmap, enum dma_data_direction dir);
inline bool npu_memory_dma_heap_flags_uncached(unsigned long flags);
int npu_memory_create_sgt(struct npu_memory_buffer *buffer, u64 size);
void npu_memory_free_sgt(struct sg_table *sgt);
int npu_memory_vmap(struct npu_memory_buffer *buffer, u64 size);
void npu_memory_release(struct npu_memory *memory);
int npu_memory_open(struct npu_memory *memory);
int npu_memory_close(struct npu_memory *memory);
void npu_memory_alloc_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf, size_t size);
void npu_memory_update_dbg_info(struct npu_memory *memory, struct dma_buf *dmabuf);
int npu_memory_map(struct npu_memory *memory, struct npu_session *session,
			struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req);
void npu_memory_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer);
struct npu_memory_buffer *npu_memory_copy(struct npu_memory *memory, struct npu_memory_buffer *buffer,
					u32 lazy_unmap, size_t offset, size_t size);
struct dma_buf *npu_memory_ion_alloc(size_t size, unsigned int flag);
int npu_memory_alloc(struct npu_memory *memory, struct npu_memory_buffer *buffer, u32 lazy_unmap, int prot);
int npu_memory_alloc_cached(struct npu_memory *memory, struct npu_memory_buffer *buffer, u32 lazy_unmap, int prot);
int npu_memory_free(struct npu_memory *memory, struct npu_memory_buffer *buffer);
int npu_memory_alloc_from_heap(struct platform_device *pdev,
		struct npu_memory_buffer *buffer, dma_addr_t daddr,
		struct npu_memory *memory, const char *heapname, int prot);
void npu_memory_free_from_heap(struct npu_memory *memory, struct npu_memory_buffer *buffer, bool dual);
void npu_memory_health(struct npu_memory *memory);

int npu_memory_map_retry(struct npu_memory *memory, struct npu_session *session,
	struct npu_memory_buffer *buffer, int prot, u32 lazy_unmap, bool is_vmap_req);
int npu_memory_clone_buffer(struct npu_memory *memory, struct npu_memory_buffer *sbuf,
				struct npu_memory_buffer *dbuf, phys_addr_t base);
void npu_memory_profile_unmap(struct npu_memory *memory, struct npu_memory_buffer *buffer);
int npu_memory_profile_free(struct npu_memory *memory, struct npu_memory_buffer *buffer);
int npu_memory_update_sysmmu_status(const char *syscall, struct npu_memory *memory, struct npu_session *session);
#ifdef CONFIG_NPU_KUNIT_TEST
#define npu_memory_dump(x) do {} while (0)
#define npu_memory_buffer_dump(x) do {} while (0)
#define npu_memory_buffer_health_dump(x) do {} while (0)
#else
void npu_memory_dump(struct npu_memory *memory);
void npu_memory_buffer_dump(struct npu_memory_buffer *buffer);
void npu_memory_buffer_health_dump(struct npu_memory_buffer *buffer);
#endif

int __init npu_memory_register(void);
void __exit npu_memory_unregister(void);
#endif
