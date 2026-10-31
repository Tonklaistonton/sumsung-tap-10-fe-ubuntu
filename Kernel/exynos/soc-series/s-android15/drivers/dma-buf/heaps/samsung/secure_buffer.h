// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF Heap Secure Structure header to interface ldfw
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 * Author: <hyesoo.yu@samsung.com> for Samsung
 */

#ifndef _SECURE_BUFFER_H
#define _SECURE_BUFFER_H

#include <linux/arm-smccc.h>

/*
 * struct buffer_prot_info - buffer protection information
 * @chunk_count: number of physically contiguous memory chunks to protect
 *               each chunk should has the same size.
 * @dma_addr:    device virtual address for protected memory access
 * @flags:       protection flags but actually, protectid
 * @chunk_size:  length in bytes of each chunk.
 * @bus_address: This is physically linear address for device to access.
 */
struct buffer_prot_info {
	unsigned int chunk_count;
	unsigned int dma_addr;
	unsigned int flags;
	unsigned int chunk_size;
	unsigned long bus_address;
};

/*
 * struct drm_sg_table - non-contiguous buffer protection information
 * @lists:	The lists is a chain of pages. Each page contains a u32-sized pfn or
		information entry. Information entry contains the value of the order
		and the number of pfns written after it. If the order changes,
		an information entry is added again. If it exceeds the size of page,
		it may point to another page linked in a chain. Most concepts are the
		same as sg_table, but the use of internal values is different.
 * @nents:	The total number of entry including index, chain, page.
 * @page_nents:	The number of only page entry.
 */
struct drm_sg_table {
	unsigned int *lists;
	unsigned int nents;
	unsigned int page_nents;
};

int drm_sg_alloc_table(struct drm_sg_table *table, unsigned int nents);
void drm_sg_free_table(struct drm_sg_table *table);
void drm_sg_set_table(struct drm_sg_table *table, struct list_head *pages,
		      unsigned int *nr_page_lists);

#define SMC_DRM_MEMORY_PROT		(0x82002110)
#define SMC_DRM_MEMORY_UNPROT		(0x82002111)

#define HVC_DRM_TZMP2_TRAP_MASK		(0x0800)

#if IS_ENABLED(CONFIG_EXYNOS_PKVM_MODULE)
#define HVC_DRM_TZMP2_PROT		(0x82002110 | HVC_DRM_TZMP2_TRAP_MASK)
#define HVC_DRM_TZMP2_UNPROT		(0x82002111 | HVC_DRM_TZMP2_TRAP_MASK)
#else
#define HVC_DRM_TZMP2_PROT		(0x82002110)
#define HVC_DRM_TZMP2_UNPROT		(0x82002111)
#endif

#define E_DRMPLUGIN_BUFFER_LIST_FULL 0x2002

static inline unsigned long ppmp_smc(unsigned long cmd, unsigned long arg0,
				     unsigned long arg1, unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(cmd, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return (unsigned long)res.a0;
}

static inline unsigned long ppmp_hvc(unsigned long cmd, unsigned long arg0,
				     unsigned long arg1, unsigned long arg2,
				     unsigned long arg3)
{
	struct arm_smccc_res res;

	if (cmd & HVC_DRM_TZMP2_TRAP_MASK)
		arm_smccc_smc(cmd, arg0, arg1, arg2, arg3, 0, 0, 0, &res);
	else
		arm_smccc_hvc(cmd, arg0, arg1, arg2, arg3, 0, 0, 0, &res);

	return (unsigned long)res.a0;
}
#endif
