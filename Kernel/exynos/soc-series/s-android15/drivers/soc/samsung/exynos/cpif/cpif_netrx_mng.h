/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 Samsung Electronics.
 *
 */

#ifndef __CPIF_NETRX_MNG_H__
#define __CPIF_NETRX_MNG_H__

#include "modem_prj.h"
#include "link_device_memory.h"
#include "cpif_page.h"
#include "cpif_vmapper.h"

#define NETRX_POOL_PAGE_SIZE	SZ_64K

struct cpif_addr_pair {
	u64			cp_addr;	/* cp address */
	void			*ap_addr;	/* ap virtual address */

	struct page		*page;	/* page holding the ap address */
	u32			page_order;
	u32			is_tmp_alloc:1,
				reserved:31;
};

struct cpif_netrx_mng {
	u64 num_packet;
	u64 frag_size;
	u64 total_buf_size;

	struct cpif_va_mapper *desc_map;
	struct cpif_va_mapper *data_map;
	struct cpif_va_mapper *temp_map;

	struct cpif_page_pool	*data_pool;
	struct cpif_addr_pair	*apair_arr;
	u32			map_idx; /* idx mapped recently*/
	u32			unmap_idx; /*  idx unmapped recently */

	/* contains pre-unmapped AP addr which couldn't be delivered to kernel yet */
	void *already_retrieved;

	/* 0: use both data map and temp map, 1: use only data map */
	bool use_one_vmap;
};


static inline u64 calc_vmap_size(u64 frag_size, u64 page_size, u64 num_packet) {
	u64 num_pages;
	u64 packet_per_page = page_size / frag_size;

	num_pages = DIV_ROUND_UP(num_packet, packet_per_page);

	/* Set extra 128k to avoid duplicated sysmmu mapping */
	return (page_size * num_pages) + SZ_128K;
}

#if IS_ENABLED(CONFIG_EXYNOS_CPIF_IOMMU) || IS_ENABLED(CONFIG_EXYNOS_CPIF_IOMMU_V9)
struct cpif_netrx_mng *cpif_create_netrx_mng(struct cpif_addr_pair *desc_addr_pair,
						u64 desc_size, u64 databuf_cp_pbase,
						u64 max_packet_size, u64 num_packet);
void cpif_exit_netrx_mng(struct cpif_netrx_mng *cm);
void cpif_init_netrx_mng(struct cpif_netrx_mng *cm);
struct cpif_addr_pair *cpif_map_rx_buf(struct cpif_netrx_mng *cm, u32 idx);
void *cpif_unmap_rx_buf(struct cpif_netrx_mng *cm, u32 idx);
#else
static inline struct cpif_netrx_mng *cpif_create_netrx_mng(
				struct cpif_addr_pair *desc_addr_pair,
				u64 desc_size, u64 databuf_cp_pbase,
				u64 max_packet_size, u64 num_packet) { return NULL; }
static inline void cpif_exit_netrx_mng(struct cpif_netrx_mng *cm) { return; }
static inline void cpif_init_netrx_mng(struct cpif_netrx_mng *cm) { return; }
static inline struct cpif_addr_pair *cpif_map_rx_buf(struct cpif_netrx_mng *cm, u32 idx)
{ return NULL; }
static inline void *cpif_unmap_rx_buf(struct cpif_netrx_mng *cm, u32 idx)
{ return NULL; }
#endif
#endif /* __CPIF_NETRX_MNG_H__ */
