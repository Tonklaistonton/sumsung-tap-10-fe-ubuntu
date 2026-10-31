/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024 Samsung Electronics.
 *
 */

#ifndef __CPIF_CMA_MEM_H__
#define __CPIF_CMA_MEM_H__

#define MAX_CP_CMA_MEM	2
#define MAX_ALLOC_RETRY	5

struct cp_cma_mem {
	u32 idx;
	struct cma *cma_area;
	u32 max_size;
	u32 req_size;
	u32 align;
	struct mutex lock;
	struct page *cma_pages;
	phys_addr_t paddr;
	void *vaddr;
};

#if IS_ENABLED(CONFIG_CPIF_CMA_MEM)
int alloc_cp_cma_region(struct cp_cma_mem *cp_cma, u32 size);
int dealloc_cp_cma_region(struct cp_cma_mem *cp_cma);
struct cp_cma_mem *get_cp_cma_region(u32 idx);
#else
static inline int alloc_cp_cma_region(struct cp_cma_mem *cp_cma, u32 size) { return 0; }
static inline int dealloc_cp_cma_region(struct cp_cma_mem *cp_cma) { return 0; }
static inline struct cp_cma_mem *get_cp_cma_region(u32 idx) { return NULL; }
#endif

#endif /* __CPIF_CMA_MEM_H__ */
