/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_SYSTEM_H_
#define _NPU_SYSTEM_H_

#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/hashtable.h>
#include "npu-scheduler.h"
#include "npu-qos.h"
#include "include/npu-wakeup.h"
#include "interface/hardware/npu-interface.h"
#include "interface/hardware/mailbox.h"
#include "include/npu-memory.h"
#include "npu-memory-manual.h"
#include "include/npu-binary.h"
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include "npu-fw-test-handler.h"
#endif
#if IS_ENABLED(CONFIG_NPU_AFM)
#include "npu-afm.h"
#endif
#include "npu-clock.h"

#define NPU_SYSTEM_DEFAULT_CORENUM	1
#define NPU_SYSTEM_IRQ_MAX		5
#define NPU_AFM_IRQ_CNT			1

#define NPU_SET_DEFAULT_LAYER   (0xffffffff)

#define NPU_FW_LOAD_SUCCESS	(0x5055E557)

#define NPU_S2D_MODE_ON		(0x20120102)
#define NPU_S2D_MODE_OFF	(0x20130201)

#define FW_32_BIT		"cpuon"
#define FW_64_BIT		"cpuon64"
#define FW_64_SYMBOL		"AIE64"
#define FW_64_SYMBOL_S		(0x5)
#define FW_SYMBOL_OFFSET	(0xF009)

struct iomem_reg_t {
	u32 pbha_hint;
	u32 vaddr;
	u32 paddr;
	u32 size;
};

struct npu_iomem_init_data {
	const char	*heapname;
	const char	*name;
	void	*area_info;       /* Save iomem result */
};

#define NPU_MAX_IO_DATA		50
struct npu_io_data {
	const char	*heapname;
	const char	*name;
	struct npu_iomem_area	*area_info;
};

#define NPU_MAX_MEM_DATA	32
struct npu_mem_data {
	const char	*heapname;
	const char	*name;
	struct npu_memory_buffer	*area_info;
	struct npu_memory_buffer	*area_info2;
};

struct npu_rmem_data {
	const char	*heapname;
	const char	*name;
	struct npu_memory_buffer	*area_info;
	struct npu_memory_buffer	*area_info2;
	struct reserved_mem	*rmem;
};

struct reg_cmd_list {
	char			*name;
	struct reg_cmd_map	*list;
	int			count;
};

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
struct imb_parts_info {
	u32 addr;
	u32 size;
};

#define	NPU_PAGE_TABLE_NUM	(0x2)

struct imb_alloc_info {
	struct npu_memory_buffer	chunk[NPU_IMB_CHUNK_MAX_NUM];
	u32				alloc_chunk_cnt;
	struct npu_iomem_area		*chunk_imb;
	struct mutex			imb_alloc_lock;
	u32				imb_chunk_max_num;
	u32 				rent_cnt;
	u32				total_chunk_num;
	u32 				need_over_chunk_num; // need more allocation on DVA window 
	u32 				parts; // number imb area of pieces
	struct imb_parts_info		ipi[2];
	struct npu_memory_buffer	over_imb_buf;
};

struct imb_size_control {
	int				result_code;
	u32				fw_size;
	wait_queue_head_t		waitq;
};
#endif

struct dsp_dhcp;
struct npu_interface_ops {
	int (*interface_probe)(struct npu_system *system);
	int (*interface_open)(struct npu_system *system);
	int (*interface_close)(struct npu_system *system);
	void (*interface_suspend)(struct npu_system *system);
	int (*interface_resume)(struct npu_system *system);
};
extern const struct npu_interface_ops n_npu_interface_ops;

struct npu_system {
	struct platform_device	*pdev;
	struct npu_hw_device **hwdev_list;
	u32 hwdev_num;
	struct dsp_dhcp *dhcp;
	struct dhcp_table *dhcp_table;

	struct npu_io_data	io_area[NPU_MAX_IO_DATA];
	struct npu_mem_data	mem_area[NPU_MAX_MEM_DATA];
	struct npu_rmem_data	rmem_area[NPU_MAX_MEM_DATA];
	struct iommu_domain *domain;

	struct reg_cmd_list	*cmd_list;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct npu_fw_test_handler	npu_fw_test_handler;
#endif

	int			irq_num;
	int			afm_irq_num;
	int			irq[NPU_SYSTEM_IRQ_MAX];

	struct npu_qos_setting	qos_setting;

	u32			max_npu_core;
	struct npu_clocks	clks;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	/* maintain to be awake */
	struct wakeup_source	*ws;
#endif

	struct npu_memory memory;
	struct npu_memory_manual *manual;
	struct npu_binary binary;

	volatile struct mailbox_hdr	*mbox_hdr;
	volatile struct npu_interface	*interface;

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	struct imb_alloc_info		imb_alloc_data;
	struct imb_alloc_info		imb_alloc_data2;
	struct imb_size_control		imb_size_ctl;
#endif

#if IS_ENABLED(CONFIG_NPU_STM)
	unsigned int fw_load_success;
#endif

#if IS_ENABLED(CONFIG_NPU_AFM)
	struct npu_afm *afm;
#endif
	/* Open status (Bitfield of npu_system_resume_steps) */
	unsigned long			resume_steps;
	unsigned long			resume_soc_steps;
	unsigned long			saved_warm_boot_flag;

	unsigned int layer_start;
	unsigned int layer_end;

	bool fw_cold_boot;
	u32 s2d_mode;
	unsigned long default_affinity;
	const struct npu_interface_ops *interface_ops;

	struct workqueue_struct *wq;
	struct work_struct work_report;

	u32 enter_suspend;
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	/* msec */
	u32 fr_timeout;
#endif
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	u32 cidle;
#endif
	atomic_t pg_control;
};

static inline struct npu_io_data *npu_get_io_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_IO_DATA && system->io_area[i].name != NULL; i++) {
		if (!strcmp(name, system->io_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_io_data *)NULL;
	else
		return (system->io_area + t);
}
static inline struct npu_iomem_area *npu_get_io_area(struct npu_system *system, const char *name)
{
	struct npu_io_data *t;

	t = npu_get_io_data(system, name);
	return t ? t->area_info : (struct npu_iomem_area *)NULL;
}

static inline struct npu_mem_data *npu_get_mem_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_MEM_DATA && system->mem_area[i].name != NULL; i++) {
		if (!strcmp(name, system->mem_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_mem_data *)NULL;
	else
		return (system->mem_area + t);
}

static inline struct npu_rmem_data *npu_get_rmem_data(struct npu_system *system, const char *name)
{
	int i, t = -1;

	for (i = 0; i < NPU_MAX_MEM_DATA && system->rmem_area[i].name != NULL; i++) {
		if (!strcmp(name, system->rmem_area[i].name)) {
			t = i;
			break;
		}
	}
	if (t < 0)
		return (struct npu_rmem_data *)NULL;
	else
		return (system->rmem_area + t);
}

static inline struct npu_memory_buffer *npu_get_mem_area(struct npu_system *system, const char *name)
{
	struct npu_mem_data *t;
	struct npu_rmem_data *tr;

	t = npu_get_mem_data(system, name);
	if (t)
		return t->area_info;
	tr = npu_get_rmem_data(system, name);
	if (tr)
		return tr->area_info;
	return (struct npu_memory_buffer *)NULL;
}

static inline int get_iomem_data_index(const struct npu_iomem_init_data data[], const char *name)
{
	int i;

	for (i = 0; data[i].name != NULL; i++) {
		if (!strcmp(data[i].name, name))
			return i;
	}
	return -1;
}

static inline struct reg_cmd_list *get_npu_cmd_map(struct npu_system *system, const char *cmd_name)

{
	int i;

	for (i = 0; ((system->cmd_list) + i)->name != NULL; i++) {
		if (!strcmp(((system->cmd_list) + i)->name, cmd_name))
			return (system->cmd_list + i);
	}
	return (struct reg_cmd_list *)NULL;
}

int npu_system_probe(struct npu_system *system, struct platform_device *pdev);
int npu_system_release(struct npu_system *system, struct platform_device *pdev);
int npu_system_open(struct npu_system *system);
int npu_system_close(struct npu_system *system);
int npu_system_resume(struct npu_system *system);
int npu_system_suspend(struct npu_system *system);
int npu_system_start(struct npu_system *system);
int npu_system_stop(struct npu_system *system);

void npu_memory_sync_for_cpu(void);
void npu_memory_sync_for_device(void);
void npu_soc_status_report(struct npu_system *system);
void fw_print_log2dram(struct npu_system *system, u32 len);
int __alloc_imb_chunk(struct npu_memory_buffer *IMB_mem_buf, struct npu_system *system,
		struct npu_session *session, struct imb_alloc_info *imb_range, u32 req_chunk_cnt);
void __free_imb_chunk(u32 new_chunk_cnt, struct npu_system *system,
		struct npu_session *session, struct imb_alloc_info *imb_range);
struct npu_iomem_area* get_chunk_imb_io_area(struct npu_system *system);
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
int dsp_system_load_binary(struct npu_system *system);
#endif
#endif
