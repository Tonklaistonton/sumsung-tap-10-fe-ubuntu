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


#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/dma-direct.h>
#include <linux/dma-buf.h>
#include <linux/iommu.h>
//#include <linux/dma-iommu.h>
#include <linux/smc.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-heap.h>
#include <linux/notifier.h>
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>
#endif

#include "npu-log.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "include/npu-stm-soc.h"
#include "npu-llc.h"
#include "include/npu-memory.h"
#include "interface/hardware/mailbox_ipc.h"
#include "dsp-dhcp.h"
#include "npu-ver.h"
#include "npu-util-memdump.h"
#include "npu-clock.h"
#include "npu-memory-manual.h"

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#if IS_ENABLED(CONFIG_NPU_USE_S2MPU_NAME_IS_NPUS)
#define NPU_S2MPU_NAME	"NPUS"
#else
#define NPU_S2MPU_NAME	"DNC"
#endif
#endif

static int of_get_irq_count(struct device_node *dev)
{
	struct of_phandle_args irq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &irq) == 0)
		nr++;

	return nr;
}

static int npu_platform_get_irq(struct npu_system *system)
{
	int i, irq;

	system->irq_num = of_get_irq_count(system->pdev->dev.of_node);

	if (system->irq_num > NPU_SYSTEM_IRQ_MAX) {
		probe_err("irq number for NPU from dts(%d) and required irq's(%d)"
			"didnt match\n", system->irq_num, NPU_SYSTEM_IRQ_MAX);
		irq = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < system->irq_num; i++) {
		irq = platform_get_irq(system->pdev, i);

		if (irq < 0) {
			probe_err("fail(%d) in platform_get_irq(%d)\n", irq, i);
			irq = -EINVAL;
			goto p_err;
		}
		system->irq[i] = irq;
	}
	system->afm_irq_num = NPU_AFM_IRQ_CNT;
	probe_info("NPU AFM irq registered, mailbox_irq : %u, afm_irq : %u\n",
		system->irq_num, system->afm_irq_num);

p_err:
	return irq;
}

static struct system_pwr sysPwr;

#define OFFSET_END	0xFFFFFFFF

/* Initialzation steps for system_resume */
enum npu_system_resume_steps {
#if !IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	NPU_SYS_RESUME_SETUP_WAKELOCK,
#endif
	NPU_SYS_RESUME_INIT_FWBUF,
	NPU_SYS_RESUME_FW_LOAD,
	NPU_SYS_RESUME_CLK_PREPARE,
	NPU_SYS_RESUME_FW_VERIFY,
	NPU_SYS_RESUME_SOC,
	NPU_SYS_RESUME_OPEN_INTERFACE,
	NPU_SYS_RESUME_COMPLETED
};

/* Initialzation steps for system_soc_resume */
enum npu_system_resume_soc_steps {
	NPU_SYS_RESUME_SOC_CPU_ON,
	NPU_SYS_RESUME_SOC_COMPLETED
};

static int npu_firmware_load(struct npu_system *system);

void npu_soc_status_report(struct npu_system *system)
{
#if !IS_ENABLED(CONFIG_SOC_S5E8835)
	int ret = 0;

	npu_info("Print CPU PC value\n");
	ret = npu_cmd_map(system, "cpupc");
	if (ret)
		npu_err("fail(%d) in npu_cmd_map for cpupc\n", ret);
#endif
}

#define DRAM_FW_REPORT_BUF_SIZE		(1024*1024)

static struct npu_memory_v_buf fw_report_buf = {
	.size = DRAM_FW_REPORT_BUF_SIZE,
};

int npu_system_alloc_fw_dram_log_buf(struct npu_system *system)
{
	int ret = 0;

	npu_info("start: initialization.\n");

	if (!fw_report_buf.v_buf) {
		strcpy(fw_report_buf.name, "FW_REPORT");
		ret = npu_memory_v_alloc(&system->memory, &fw_report_buf);
		if (ret) {
			npu_err("fail(%d) in FW report buffer memory allocation\n", ret);
			return ret;
		}

		if (fw_report_buf.size < PAGE_SIZE) {
			npu_err("fw_report_buf.size %zu is not right\n", fw_report_buf.size);
			ret = -EINVAL;
			goto err;
		}

		npu_fw_report_init(fw_report_buf.v_buf, fw_report_buf.size);
	} else {//Case of fw_report is already allocated by ion memory
		npu_dbg("fw_report is already initialized - %pK.\n", fw_report_buf.v_buf);
	}

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* Initialize firmware utc handler with dram log buf */
	ret = npu_fw_test_initialize(system);
	if (ret) {
		npu_err("npu_fw_test_probe() failed : ret = %d\n", ret);
		goto err;
	}
#endif

	npu_info("complete : initialization.\n");
	return ret;
err:
	npu_memory_v_free(&system->memory, &fw_report_buf);
	return ret;
}

static int npu_system_free_fw_dram_log_buf(void)
{
	/* TODO */
	return 0;
}

static struct reg_cmd_list npu_cmd_list[] = {
	{	.name = "cpupc",	.list = NULL,	.count = 0	},
	{	.name = "cpuon",	.list = NULL,	.count = 0	},
	{	.name = "cpuon64",	.list = NULL,	.count = 0	},
	{	.name = "cpuoff",	.list = NULL,	.count = 0	},
	{	.name = "gnpucmdqpc",	.list = NULL,	.count = 0	},
	{	.name = "dspcmdqpc",	.list = NULL,	.count = 0	},
	{	.name = "npupgon",	.list = NULL,	.count = 0	},
	{	.name = "dsppgon",	.list = NULL,	.count = 0	},
	{	.name = "npupgoff",	.list = NULL,	.count = 0	},
	{	.name = "dsppgoff",	.list = NULL,	.count = 0	},
#if IS_ENABLED(CONFIG_NPU_AFM)
	{	.name = "afmgnpu0en",	.list = NULL,	.count = 0	},
	{	.name = "afmgnpu0dis",	.list = NULL,	.count = 0	},
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	{	.name = "afmgnpu1en",	.list = NULL,	.count = 0	},
	{	.name = "afmgnpu1dis",	.list = NULL,	.count = 0	},
	{	.name = "afmsnpu0en",	.list = NULL,	.count = 0	},
	{	.name = "afmsnpu0dis",	.list = NULL,	.count = 0	},
	{	.name = "afmsnpu1en",	.list = NULL,	.count = 0	},
	{	.name = "afmsnpu1dis",	.list = NULL,	.count = 0	},
#endif
	{	.name = "clrdnctdc",	.list = NULL,	.count = 0	},
	{	.name = "chkdnctdc",	.list = NULL,	.count = 0	},
	{	.name = "chkgnpu0itr",	.list = NULL,	.count = 0	},
	{	.name = "clrgnpu0itr",	.list = NULL,	.count = 0	},
	{	.name = "engnpu0itr",	.list = NULL,	.count = 0	},
	{	.name = "disgnpu0itr",	.list = NULL,	.count = 0	},
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	{	.name = "clrgnpu1tdc",	.list = NULL,	.count = 0	},
	{	.name = "chkgnpu1tdc",	.list = NULL,	.count = 0	},
#endif
	{	.name = "clrdncdiv",	.list = NULL,	.count = 0	},
	{	.name = "clrgnpu0div",	.list = NULL,	.count = 0	},
#if !IS_ENABLED(CONFIG_SOC_S5E8855)
	{	.name = "clrgnpu1div",	.list = NULL,	.count = 0	},
#endif
	{	.name = "clrdspdiv",	.list = NULL,	.count = 0	},
#endif
#if IS_ENABLED(CONFIG_NPU_STM)
	{	.name = "enablestm",	.list = NULL,	.count = 0	},
	{	.name = "disablestm",	.list = NULL,	.count = 0	},
	{	.name = "enstmdnc",	.list = NULL,	.count = 0	},
	{	.name = "disstmdnc",	.list = NULL,	.count = 0	},
	{	.name = "enstmnpu",	.list = NULL,	.count = 0	},
	{	.name = "disstmnpu",	.list = NULL,	.count = 0	},
	{	.name = "enstmdsp",	.list = NULL,	.count = 0	},
	{	.name = "disstmdsp",	.list = NULL,	.count = 0	},
	{	.name = "allow64stm",	.list = NULL,	.count = 0	},
	{	.name = "allow64stm1",	.list = NULL,	.count = 0	},
	{	.name = "allow64ns",	.list = NULL,	.count = 0	},
	{	.name = "allow64ns1",	.list = NULL,	.count = 0	},
	{	.name = "sdma_vc0",	.list = NULL,	.count = 0	},
	{	.name = "sdma_vc1",	.list = NULL,	.count = 0	},
#endif
	{	.name = "llcaid",	.list = NULL,	.count = 0	},
	{	.name = NULL,	.list = NULL,	.count = 0	}
};

static int npu_init_cmd_list(struct npu_system *system)
{
	int ret = 0;
	int i;

	system->cmd_list = (struct reg_cmd_list *)npu_cmd_list;

	for (i = 0; ((system->cmd_list) + i)->name; i++) {
		if (npu_get_reg_cmd_map(system, (system->cmd_list) + i) == -ENODEV)
			probe_info("No cmd for %s\n", ((system->cmd_list) + i)->name);
		else
			probe_info("register cmd for %s\n", ((system->cmd_list) + i)->name);
	}

	return ret;
}

static inline int set_max_npu_core(struct npu_system *system, s32 num)
{

	if (unlikely(num < 0)) {
		probe_err("set wrong npu core num\n");
		return -EINVAL;
	}

	probe_info("Max number of npu core : %d\n", num);
	system->max_npu_core = num;

	return 0;
}

static int npu_rsvd_map(struct npu_system *system, struct npu_rmem_data *rmt)
{
	int ret = 0;
	unsigned int num_pages;
	struct page **pages, **page;
	phys_addr_t phys;

	/* try to map kvmap */
	num_pages = (unsigned int)DIV_ROUND_UP(rmt->area_info->dma.size, PAGE_SIZE);
	pages = kcalloc(num_pages, sizeof(pages[0]), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		probe_err("fail to alloc pages for rmem(%s)\n", rmt->name);
		iommu_unmap(system->domain, rmt->area_info->dma.daddr,
				(size_t) rmt->rmem->size);
		goto p_err;
	}

	phys = rmt->rmem->base;
	for (page = pages; (page - pages < num_pages); page++) {
		*page = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	rmt->area_info->paddr = rmt->rmem->base;

	rmt->area_info->dma.vaddr = vmap(pages, num_pages,
			VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	kfree(pages);
	if (!rmt->area_info->dma.vaddr) {
		ret = -ENOMEM;
		probe_err("fail to vmap %u pages for rmem(%s)\n",
				num_pages, rmt->name);
		iommu_unmap(system->domain, rmt->area_info->dma.daddr,
				(size_t) rmt->area_info->dma.size);
		goto p_err;
	}

p_err:
	return ret;
}

static int npu_init_iomem_area(struct npu_system *system)
{
	int ret = 0;
	int i, k, si, mi;
	void __iomem *iomem;
	struct device *dev;
	int iomem_count, init_count, phdl_cnt, rmem_cnt;
	struct iomem_reg_t *iomem_data;
	struct iomem_reg_t *iomem_init_data = NULL;
	struct iommu_domain	*domain;
	const char *iomem_name;
	const char *heap_name;
	struct npu_iomem_area *id;
	struct npu_memory_buffer *bd;
	struct npu_io_data *it;
	struct npu_mem_data *mt;
	struct npu_rmem_data *rmt;
	struct device_node *mems_node, *mem_node, *phdl_node;
	struct reserved_mem *rsvd_mem;
	char tmpname[32];
	u32 core_num;
	u32 size;

	if (unlikely(!system->pdev)) {
		probe_err("failed to get system->pdev\n");
		ret = -EINVAL;
		goto err_exit;
	}

	dev = &(system->pdev->dev);
	iomem_count = of_property_count_strings(
			dev->of_node, "samsung,npumem-names") / 2;
	if (IS_ERR_VALUE((unsigned long)iomem_count)) {
		probe_err("invalid iomem list in %s node", dev->of_node->name);
		ret = -EINVAL;
		goto err_exit;
	}
	probe_info("%d iomem items\n", iomem_count);

	iomem_data = (struct iomem_reg_t *)devm_kzalloc(dev,
			iomem_count * sizeof(struct iomem_reg_t), GFP_KERNEL);
	if (!iomem_data) {
		probe_err("failed to alloc for iomem data");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = of_property_read_u32_array(dev->of_node, "samsung,npumem-address", (u32 *)iomem_data,
			iomem_count * sizeof(struct iomem_reg_t) / sizeof(u32));
	if (ret) {
		probe_err("failed to get iomem data (%d)\n", ret);
		ret = -EINVAL;
		goto err_data;
	}

	si = 0; mi = 0;
	for (i = 0; i < iomem_count; i++) {
		ret = of_property_read_string_index(dev->of_node,
				"samsung,npumem-names", i * 2, &iomem_name);
		if (ret) {
			probe_err("failed to read iomem name %d from %s node(%d)\n",
					i, dev->of_node->name, ret);
			ret = -EINVAL;
			goto err_data;
		}
		ret = of_property_read_string_index(dev->of_node,
				"samsung,npumem-names", i * 2 + 1, &heap_name);
		if (ret) {
			probe_err("failed to read iomem type for %s (%d)\n",
					iomem_name, ret);
			ret = -EINVAL;
			goto err_data;
		}
		if (strcmp(heap_name, "SFR") // !SFR
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
		    && strcmp(heap_name, "IMB") // !IMB
#endif
		    ){
			mt = &((system->mem_area)[mi]);
			mt->heapname = heap_name;
			mt->name = iomem_name;
			probe_info("MEM %s(%d) uses %s\n", mt->name, mi,
				strcmp("", mt->heapname) ? mt->heapname : "System Heap");

			mt->area_info = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);
			if (mt->area_info == NULL) {
				probe_err("error allocating npu buffer mt->area_info\n");
				ret = -ENOMEM;
				goto err_data;
			}

			mt->area_info2 = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);
			if (mt->area_info2 == NULL) {
				probe_err("error allocating npu buffer mt->area_info2\n");
				ret = -ENOMEM;
				goto err_clone1;
			}

			mt->area_info->dma.size = (iomem_data + i)->size;
			probe_info("Flags[%08x], DVA[%08x], SIZE[%08x]",
				(iomem_data + i)->pbha_hint, (iomem_data + i)->vaddr, (iomem_data + i)->size);
#if !IS_ENABLED(CONFIG_DEBUG_FS)
			if (!strcmp(heap_name, "fwunittest"))
				goto common1;
#endif
			ret = npu_memory_alloc_from_heap(system->pdev, mt->area_info,
				(iomem_data + i)->vaddr, &system->memory, mt->heapname, (iomem_data + i)->pbha_hint);
			if (ret) {
				for (k = 0; k < mi; k++) {
					bd = (system->mem_area)[k].area_info;
					if (bd) {
						if (bd->dma.vaddr)
							npu_memory_free_from_heap(&system->memory, bd, false);
						devm_kfree(dev, bd);
					}
					bd = (system->mem_area)[k].area_info2;
					if (bd) {
						if (bd->dma.vaddr)
							npu_memory_free_from_heap(&system->memory, bd, true);
						devm_kfree(dev, bd);
					}
				}
				probe_err("buffer allocation for %s heap failed w/ err: %d\n",
						mt->name, ret);
				ret = -EFAULT;
				goto err_data;
			}
			ret = npu_memory_clone_buffer(&system->memory, mt->area_info, mt->area_info2, 0);
			if (ret) {
				for (k = 0; k < mi; k++) {
					bd = (system->mem_area)[k].area_info;
					if (bd) {
						if (bd->dma.vaddr)
							npu_memory_free_from_heap(&system->memory, bd, false);
						devm_kfree(dev, bd);
					}
					bd = (system->mem_area)[k].area_info2;
					if (bd) {
						if (bd->dma.vaddr)
							npu_memory_free_from_heap(&system->memory, bd, true);
						devm_kfree(dev, bd);
					}
				}
				probe_err("buffer allocation for %s heap failed w/ err: %d\n",
						mt->name, ret);
				ret = -EFAULT;
				goto err_data;
			}
#if !IS_ENABLED(CONFIG_DEBUG_FS)
common1:
#endif
			probe_info("%s : daddr[%08x], paddr[%08x], [%08x] alloc memory\n",
					iomem_name, (iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size);

			mi++;
		} else { // heap_name is SFR or IMB
			it = &((system->io_area)[si]);
			it->heapname = heap_name;
			it->name = iomem_name;

			probe_info("SFR %s(%d)\n", it->name, si);

			it->area_info = (struct npu_iomem_area *)devm_kzalloc(dev, sizeof(struct npu_iomem_area), GFP_KERNEL);
			if (it->area_info == NULL) {
				probe_err("error allocating npu iomem\n");
				goto err_data;
			}
			id = (struct npu_iomem_area *)it->area_info;
			id->daddr = (iomem_data + i)->vaddr;
			id->paddr = (iomem_data + i)->paddr;
			id->size = (iomem_data + i)->size;
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
			if (!strcmp(heap_name, "IMB"))
				goto common;
#endif

			if (!strcmp(it->name, "mboxapm1dnc")) {
				domain = iommu_get_domain_for_dev(dev);
				if (!domain) {
					probe_err("get domain failed\n");
					ret = -EPROBE_DEFER;
					goto err_data;
				}
				ret = iommu_map(domain, (iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}

				ret = iommu_map(domain, (iomem_data + i)->vaddr + 0x100000000,
						(iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}
			}

			if (!strcmp(it->name, "pmu_sub")) {
				domain = iommu_get_domain_for_dev(dev);
				if (!domain) {
					probe_err("get domain failed\n");
					ret = -EPROBE_DEFER;
					goto err_data;
				}
				ret = iommu_map(domain, (iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}

				ret = iommu_map(domain, (iomem_data + i)->vaddr + 0x100000000,
						(iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}
			}

			if (!strcmp(it->name, "gpuc2a")) {
				domain = iommu_get_domain_for_dev(dev);
				if (!domain) {
					probe_err("get domain failed\n");
					ret = -EPROBE_DEFER;
					goto err_data;
				}
				ret = iommu_map(domain, (iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}

				ret = iommu_map(domain, (iomem_data + i)->vaddr + 0x100000000,
						(iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}
			}

			if (!strcmp(it->name, "sfrblkall")) {
				domain = iommu_get_domain_for_dev(dev);
				if (!domain) {
					probe_err("get domain failed\n");
					ret = -EPROBE_DEFER;
					goto err_data;
				}
				ret = iommu_map(domain, (iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}

				ret = iommu_map(domain, (iomem_data + i)->vaddr + 0x100000000,
						(iomem_data + i)->paddr, (iomem_data + i)->size, 0, GFP_KERNEL);
				if (ret) {
					probe_err("iommu_map is failed about  %s mapping\n", it->name);
					goto err_data;
				}
			}

			iomem = devm_ioremap(&(system->pdev->dev),
				(iomem_data + i)->paddr, (iomem_data + i)->size);
			if (IS_ERR_OR_NULL(iomem)) {
				probe_err("fail(%pK) in devm_ioremap(0x%08x, %u)\n",
					iomem, id->paddr, (u32)id->size);
				ret = -EFAULT;
				goto err_data;
			}
			id->vaddr = iomem;

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
common:
#endif
			probe_info("%s : Daddr [%08x], Paddr[%08x], [%08x] => Mapped @[%pK], Length = %llu\n",
					iomem_name, (iomem_data + i)->vaddr, (iomem_data + i)->paddr, (iomem_data + i)->size,
					id->vaddr, id->size);

			/* initial settings for this area
			 * use dt "samsung,npumem-xxx"
			 */
			tmpname[0] = '\0';
			strcpy(tmpname, "samsung,npumem-");
			strcat(tmpname, iomem_name);
			init_count = of_property_read_variable_u32_array(dev->of_node,
					tmpname, (u32 *)iomem_init_data,
					sizeof(struct reg_set_map) / sizeof(u32), 0);
			if (init_count > 0) {
				probe_trace("start in init settings for %s\n", iomem_name);
				ret = npu_set_hw_reg(id,
						(struct reg_set_map *)iomem_init_data,
						init_count / sizeof(struct reg_set_map), 0);
				if (ret) {
					probe_err("fail(%d) in npu_set_hw_reg\n", ret);
					goto err_data;
				}
				probe_info("complete in %d init settings for %s\n",
						(int)(init_count / sizeof(struct reg_set_map)),
						iomem_name);
			}
			si++;
		}
	}
	ret = of_property_read_u32(dev->of_node,
			"samsung,npusys-corenum", &core_num);
	if (ret)
		core_num = NPU_SYSTEM_DEFAULT_CORENUM;

	ret = set_max_npu_core(system, core_num);
	if (unlikely(ret < 0))
		goto err_data;


	/* reserved memory */
	rmem_cnt = 0;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		probe_err("fail to get domain for dnc\n");
		ret = -EPROBE_DEFER;
		goto err_data;
	}
	system->domain = domain;

	mems_node = of_get_child_by_name(dev->of_node, "samsung,npurmem-address");
	if (!mems_node) {
		ret = 0;	/* not an error */
		probe_err("null npurmem-address node\n");
		goto err_data;
	}

	for_each_child_of_node(mems_node, mem_node) {
		rmt = &((system->rmem_area)[rmem_cnt]);
		rmt->area_info = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);
		if (rmt->area_info == NULL) {
			probe_err("error allocating rmt area_info\n");
			ret = -ENOMEM;
			goto err_data;
		}

		rmt->area_info2 = (struct npu_memory_buffer *)devm_kzalloc(dev, sizeof(struct npu_memory_buffer), GFP_KERNEL);
		if (rmt->area_info2 == NULL) {
			probe_err("error allocating rmt area_info2\n");
			ret = -ENOMEM;
			goto err_clone2;
		}

		rmt->name = kbasename(mem_node->full_name);
		ret = of_property_read_u32(mem_node,
				"iova", (u32 *) &rmt->area_info->dma.daddr);
		if (ret) {
			probe_err("'iova' is mandatory but not defined (%d)\n", ret);
			goto err_data;
		}

		phdl_cnt = of_count_phandle_with_args(mem_node,
					"memory-region", NULL);
		if (phdl_cnt > 1) {
			probe_err("only one phandle required. "
					"phdl_cnt(%d)\n", phdl_cnt);
			ret = -EINVAL;
			goto err_data;
		}

		if (phdl_cnt == 1) {	/* reserved mem case */
			phdl_node = of_parse_phandle(mem_node,
						"memory-region", 0);
			if (!phdl_node) {
				ret = -EINVAL;
				probe_err("fail to get memory-region in name(%s)\n", rmt->name);
				goto err_data;
			}

			rsvd_mem = of_reserved_mem_lookup(phdl_node);
			if (!rsvd_mem) {
				ret = -EINVAL;
				probe_err("fail to look-up rsvd mem(%s)\n", rmt->name);
				goto err_data;
			}
			rmt->rmem = rsvd_mem;
		}

		ret = of_property_read_u32(mem_node,
				"size", &size);
		if (ret) {
			probe_err("'size' is mandatory but not defined (%d)\n", ret);
			goto err_data;
		} else {
			if (size > rmt->rmem->size) {
				ret = -EINVAL;
				probe_err("rmt->size(%x) > rsvd_size(%llx)\n", size, rmt->rmem->size);
				goto err_data;
			}
		}
		rmt->area_info->dma.size = size;

		/* iommu map */
		ret = iommu_map(system->domain, rmt->area_info->dma.daddr, rmt->rmem->base,
										rmt->area_info->dma.size, 0, GFP_KERNEL);
		if (ret) {
			probe_err("fail to map iova for rmem(%s) ret(%d)\n",
				rmt->name, ret);
			goto err_data;
		}

		ret = npu_rsvd_map(system, rmt);
		if (ret) {
			probe_err("fail to map kvmap, rmem(%s), ret(%d)\n", rmt->name, ret);
			goto err_data;
		}

		ret = npu_memory_clone_buffer(&system->memory, rmt->area_info, rmt->area_info2, rmt->rmem->base);
		if (ret) {
			probe_err("fail to clone iova for rmem(%s) ret(%d)\n",
				rmt->name, ret);
			goto err_data;
		}

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
		system->binary.rmem_base = rmt->rmem->base;
		system->binary.rmem_size = rmt->rmem->size;
		probe_info("rmem[%d] base : %lu, size : %lu\n",
			rmem_cnt, system->binary.rmem_base, system->binary.rmem_size);
#endif
		rmem_cnt++;
	}

	/* set NULL for last */
	(system->mem_area)[mi].heapname = NULL;
	(system->mem_area)[mi].name = NULL;
	(system->rmem_area)[rmem_cnt].heapname = NULL;
	(system->rmem_area)[rmem_cnt].name = NULL;
	(system->io_area)[si].heapname = NULL;
	(system->io_area)[si].name = NULL;

	probe_info("complete in init_iomem_area\n");
	devm_kfree(dev, iomem_data);

	return ret;
err_clone2:
	if (rmt->area_info != NULL) {
		devm_kfree(dev, rmt->area_info);
		rmt->area_info = NULL;
	}
err_clone1:
	if (mt->area_info != NULL) {
		devm_kfree(dev, mt->area_info);
		mt->area_info = NULL;
	}
err_data:
	devm_kfree(dev, iomem_data);
err_exit:
	return ret;
}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
int dsp_system_load_binary(struct npu_system *system)
{
	int ret = 0, i;
	struct device *dev;
	struct npu_mem_data *md;

	dev = &system->pdev->dev;

	/* load binary only for heap memory area */
	for (md = (system->mem_area), i = 0; md[i].name; i++) {
		/* if heapname is filename,
		 * this means we need load the file on this memory area
		 */
		if ((md[i].heapname)[strlen(md[i].heapname)-4] == '.') {
			const struct firmware	*fw_blob;

			ret = request_firmware_direct(&fw_blob, md[i].heapname, dev);
			if (ret < 0) {
				npu_err("%s : error in loading %s on %s\n",
						md[i].name, md[i].heapname, md[i].name);
				goto err_data;
			}
			if (fw_blob->size > (md[i].area_info)->dma.size) {
				npu_err("%s : not enough memory for %s (%d/%d)\n",
						md[i].name, md[i].heapname,
						(int)fw_blob->size, (int)(md[i].area_info)->dma.size);
				release_firmware(fw_blob);
				ret = -ENOMEM;
				goto err_data;
			}
			memcpy((void *)(u64)((md[i].area_info)->dma.vaddr), fw_blob->data, fw_blob->size);
			release_firmware(fw_blob);
			npu_info("%s : %s loaded on %s\n",
					md[i].name, md[i].heapname, md[i].name);
		}
	}
err_data:
	return ret;
}
#endif

static int npu_clk_init(struct npu_system *system)
{
	/* TODO : need core driver */
	return 0;
}

static const char *npu_check_fw_arch(struct npu_system *system,
				struct npu_memory_buffer *fwmem)
{
	if (!strncmp(FW_64_SYMBOL, fwmem->dma.vaddr + FW_SYMBOL_OFFSET, FW_64_SYMBOL_S)) {
		npu_info("FW is 64 bit, cmd map %s\n", FW_64_BIT);
		return FW_64_BIT;
	}

	npu_info("FW is 32 bit, cmd map : %s\n", FW_32_BIT);
	return FW_32_BIT;
}

static int npu_cpu_on(struct npu_system *system)
{
	int ret = 0;
	struct npu_memory_buffer *fwmem;

	npu_dbg("start\n");

	fwmem = npu_get_mem_area(system, "fwmem");
	ret = npu_cmd_map(system, npu_check_fw_arch(system, fwmem));
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for cpu on\n", ret);
		goto err_exit;
	}

	npu_info("release CPU complete\n");

	return 0;
err_exit:
	npu_info("error(%d) in npu_cpu_on\n", ret);
	return ret;
}

static int npu_cpu_off(struct npu_system *system)
{
	int ret = 0;

	npu_dbg("start\n");
	ret = npu_cmd_map(system, "cpuoff");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for cpu_off\n", ret);
		goto err_exit;
	}

	npu_dbg("complete\n");
	return 0;
err_exit:
	npu_info("error(%d)\n", ret);
	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
int npu_pwr_on(struct npu_system *system)
{
	int ret = 0;

	if (unlikely(!system)) {
		npu_err("Failed to get npu_system\n");
		ret = -EINVAL;
		goto err_exit;
	}

	npu_dbg("start\n");

	ret = npu_cmd_map(system, "pwron");
	if (ret) {
		npu_err("fail(%d) in npu_cmd_map for pwr_on\n", ret);
		goto err_exit;
	}

	npu_dbg("complete\n");
	return 0;
err_exit:
	npu_info("error(%d)\n", ret);
	return ret;

}
#endif

static int npu_system_soc_probe(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;

	probe_info("system soc probe: ioremap areas\n");

	ret = npu_init_iomem_area(system);
	if (ret) {
		probe_err("fail(%d) in init iomem area\n", ret);
		goto p_err;
	}

	ret = npu_init_cmd_list(system);
	if (ret) {
		probe_err("fail(%d) in cmd list register\n", ret);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_NPU_STM)
	ret = npu_stm_probe(system);
	if (ret)
		probe_err("npu_stm_probe error : %d\n", ret);
#endif

p_err:
	return ret;
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
#define NPU_SYSTEM_DT_NAME_LEN	(64)
static void npu_system_dt_parsing(struct npu_system *system)
{
	int ret = 0;
	char name[NPU_SYSTEM_DT_NAME_LEN];

	struct device *dev = &system->pdev->dev;

	name[0] = '\0';
	strncpy(name, "samsung,npucidle", NPU_SYSTEM_DT_NAME_LEN);
	strncat(name, "-value", NPU_SYSTEM_DT_NAME_LEN);

	ret = of_property_read_u32(dev->of_node, name, &(system->cidle));
	if (ret)
		system->cidle = 0x0;

	probe_info("cidle value : 0x%x)\n", system->cidle);
}
#endif

inline struct npu_iomem_area* get_chunk_imb_io_area(struct npu_system *system)
{
	if (get_iova_36bit())
		return npu_get_io_area(system, "CHUNK_IMB_36BIT");
	return npu_get_io_area(system, "CHUNK_IMB");
}


int npu_system_probe(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!system)) {
		probe_err("Failed to get npu_system\n");
		ret = -EINVAL;
		goto p_err;
	}

	dev = &pdev->dev;
	device = container_of(system, struct npu_device, system);
	system->pdev = pdev;	/* TODO: Reference counting ? */
	system->interface_ops = &n_npu_interface_ops;

	ret = npu_platform_get_irq(system);
	if (ret < 0)
		goto p_err;

	ret = npu_memory_probe(&system->memory, dev);
	if (ret) {
		probe_err("fail(%d) in npu_memory_probe\n", ret);
		goto p_err;
	}

	ret = npu_memory_manual_probe(system, dev);
	if (ret) {
		probe_err("fail(%d) in npu_memory_manual_probe\n", ret);
		goto p_err;
	}

	/* Invoke platform specific probe routine */
	ret = npu_system_soc_probe(system, pdev);
	if (ret) {
		probe_err("fail(%d) in npu_system_soc_probe\n", ret);
		goto p_err;
	}

	ret = system->interface_ops->interface_probe(system);
	if (ret) {
		probe_err("fail(%d) in npu_interface_probe\n", ret);
		goto p_err;
	}

	ret = npu_binary_init(&system->binary,
		dev,
		NPU_FW_PATH1,
		NPU_FW_PATH2,
		NPU_FW_NAME);
	if (ret) {
		probe_err("fail(%d) in npu_binary_init\n", ret);
		goto p_err;
	}

	ret = npu_util_memdump_probe(system);
	if (ret) {
		probe_err("fail(%d) in npu_util_memdump_probe\n", ret);
		goto p_err;
	}

	ret = npu_scheduler_probe(device);
	if (ret) {
		probe_err("npu_scheduler_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

	ret = npu_qos_probe(system);
	if (ret) {
		probe_err("npu_qos_probe is fail(%d)\n", ret);
		goto p_qos_err;
	}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_system_dt_parsing(system);
#endif
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
	memset(&system->imb_size_ctl, 0, sizeof(struct imb_size_control));
	init_waitqueue_head(&system->imb_size_ctl.waitq);

	memset(&system->imb_alloc_data, 0, sizeof(struct imb_alloc_info));
	mutex_init(&system->imb_alloc_data.imb_alloc_lock);
	system->imb_alloc_data.chunk_imb = npu_get_io_area(system, "CHUNK_IMB");
	system->imb_alloc_data.imb_chunk_max_num = NPU_IMB_CHUNK_MAX_NUM;
	system->imb_alloc_data.rent_cnt = 0;
	system->imb_alloc_data.parts = 1;

	memset(&system->imb_alloc_data2, 0, sizeof(struct imb_alloc_info));
	mutex_init(&system->imb_alloc_data2.imb_alloc_lock);
	system->imb_alloc_data2.chunk_imb = npu_get_io_area(system, "CHUNK_IMB");
	system->imb_alloc_data2.imb_chunk_max_num = NPU_IMB_CHUNK_MAX_NUM;
	system->imb_alloc_data2.rent_cnt = 0;
	system->imb_alloc_data2.parts = 1;
#endif

#if IS_ENABLED(CONFIG_PM_SLEEP)
	/* initialize the npu wake lock */
	npu_wake_lock_init(dev, &system->ws,
				NPU_WAKE_LOCK_SUSPEND, "npu_run_wlock");
#endif
	init_waitqueue_head(&sysPwr.wq);

	sysPwr.system_result.result_code = NPU_SYSTEM_JUST_STARTED;

	system->layer_start = NPU_SET_DEFAULT_LAYER;
	system->layer_end = NPU_SET_DEFAULT_LAYER;

	system->fw_cold_boot = true;
	system->s2d_mode = NPU_S2D_MODE_OFF;

	system->enter_suspend = 0;

#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION)
	atomic_set(&device->cam_on_count, 0);
#endif
	goto p_exit;
p_qos_err:
p_err:
p_exit:
	return ret;
}

static int npu_system_soc_release(struct npu_system *system, struct platform_device *pdev)
{
	int ret;

	ret = npu_stm_release(system);
	if (ret)
		npu_err("npu_stm_release error : %d\n", ret);
	return 0;
}

/* TODO: Implement throughly */
int npu_system_release(struct npu_system *system, struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!system)) {
		npu_err("Failed to get npu system\n");
		return -EINVAL;
	}

	if (unlikely(!pdev)) {
		npu_err("Failed to get system->pdev\n");
		return -EINVAL;
	}

	dev = &pdev->dev;
	device = container_of(system, struct npu_device, system);

#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_destroy(system->ws);
#endif

	npu_memory_release(&system->memory);
	npu_memory_manual_release(system);

	ret = npu_scheduler_release(device);
	if (ret)
		probe_err("fail(%d) in npu_scheduler_release\n", ret);

	/* Invoke platform specific release routine */
	ret = npu_system_soc_release(system, pdev);
	if (ret)
		probe_err("fail(%d) in npu_system_soc_release\n", ret);

	return ret;
}

int npu_system_open(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!system)) {
		npu_err("Failed to get npu system\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!system->pdev)) {
		npu_err("Failed to get system->pdev\n");
		ret = -EINVAL;
		goto p_err;
	}

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);
	system->binary.dev = dev;

	ret = npu_memory_open(&system->memory);
	if (ret) {
		npu_err("fail(%d) in npu_memory_open\n", ret);
		goto p_err;
	}

	ret = npu_util_memdump_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_util_memdump_open\n", ret);
		goto p_err;
	}
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	ret = npu_scheduler_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_open\n", ret);
		goto p_err;
	}
	// to check available max freq for current NPU dvfs governor, need to finish scheduler open
	// then we can set boost as available
	ret = npu_qos_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_qos_open\n", ret);
		goto p_err;
	}
#endif

	/* Clear resume steps */
	system->resume_steps = 0;
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	system->fr_timeout = 10000;
#endif
#if IS_ENABLED(CONFIG_NPU_STM)
	system->fw_load_success = 0;
#endif
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
	ret = npu_precision_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_precision_open\n", ret);
		goto p_err;
	}
#endif
	atomic_set(&system->pg_control, 0);

p_err:
	return ret;
}

int npu_system_close(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device;

	device = container_of(system, struct npu_device, system);

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	system->fr_timeout = 10000;
#endif
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
	ret = npu_precision_close(system);
	if (ret)
		npu_err("fail(%d) in npu_precision_close\n", ret);
#endif

#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	ret = npu_qos_close(system);
	if (ret)
		npu_err("fail(%d) in npu_qos_close\n", ret);

	ret = npu_scheduler_close(device);
	if (ret)
		npu_err("fail(%d) in npu_scheduler_close\n", ret);

#endif
	ret = npu_util_memdump_close(system);
	if (ret)
		npu_err("fail(%d) in npu_util_memdump_close\n", ret);

	ret = npu_memory_close(&system->memory);
	if (ret)
		npu_err("fail(%d) in npu_memory_close\n", ret);
	
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
	npu_llc_close(device->sched);
#endif

	return ret;
}

static inline void print_iomem_area(const char *pr_name, const struct npu_iomem_area *mem_area)
{
	if (mem_area->vaddr)
		npu_dbg("(%13s) Phy(0x%08x)-(0x%08llx) Virt(%pK) Size(%llu)\n",
			 pr_name, mem_area->paddr, mem_area->paddr + mem_area->size,
			 mem_area->vaddr, mem_area->size);
	else
		npu_dbg("(%13s) Reserved Phy(0x%08x)-(0x%08llx) Size(%llu)\n",
			 pr_name, mem_area->paddr, mem_area->paddr + mem_area->size,
			 mem_area->size);
}

static void print_all_iomem_area(const struct npu_system *system)
{
	int i;

	npu_dbg("start in IOMEM mapping\n");
	for (i = 0; i < NPU_MAX_IO_DATA && system->io_area[i].name != NULL; i++) {
		if (system->io_area[i].area_info)
			print_iomem_area(system->io_area[i].name, system->io_area[i].area_info);
	}
	npu_dbg("end in IOMEM mapping\n");
}

static int npu_llcaid_init(struct npu_system *system)
{
	return npu_cmd_map(system, "llcaid");
}

static int npu_system_soc_resume(struct npu_system *system)
{
	int ret = 0;

	/* Clear resume steps */
	system->resume_soc_steps = 0;

	print_all_iomem_area(system);

	npu_clk_init(system);

	ret = npu_llcaid_init(system);
	if (ret) {
		npu_err("fail(%d) in npu_llcaid_init\n", ret);
		goto p_err;
	}

	ret = npu_cpu_on(system);
	if (ret) {
		npu_err("fail(%d) in npu_cpu_on\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps);
	set_bit(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps);

	return ret;
p_err:
	npu_err("Failure detected[%d].\n", ret);
	return ret;
}

int npu_system_soc_suspend(struct npu_system *system)
{
	int ret = 0;

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_COMPLETED, &system->resume_soc_steps, "SOC completed", ;);
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC_CPU_ON, &system->resume_soc_steps, "Turn NPU cpu off", {
		ret = npu_cpu_off(system);
		if (ret)
			npu_err("fail(%d) in npu_cpu_off\n", ret);
	});

	if (system->resume_soc_steps != 0)
		npu_warn("Missing clean-up steps [%lu] found.\n", system->resume_soc_steps);

	/* Function itself never be failed, even thought there was some error */
	return ret;
}

int npu_system_resume(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;
	struct npu_memory_buffer *fwmbox;
	struct npu_memory_buffer *fwmem;
	struct npu_binary		vector_binary;
	struct npu_memory_manual_info *manual_info;

	if (unlikely(!system)) {
		npu_err("Failed to get npu system\n");
		return -EINVAL;
	}

	if (unlikely(!system->pdev)) {
		npu_err("Failed to get system->pdev\n");
		return -EINVAL;
	}

	manual_info = npu_memory_manual_info_get();
	npu_memory_manual_info_noti(manual_info, system);
	system->dhcp_table->CHIP_REV = exynos_soc_info.main_rev << 20 | exynos_soc_info.sub_rev << 16;

	npu_info("fw_cold_boot(%d)\n", system->fw_cold_boot);
	dev = &system->pdev->dev;
	vector_binary.dev = dev;

	device = container_of(system, struct npu_device, system);
	fwmbox = npu_get_mem_area(system, "fwmbox");
	system->mbox_hdr = (volatile struct mailbox_hdr *)(fwmbox->dma.vaddr);

	npu_info("mbox_hdr physical : %llx\n", fwmbox->paddr);

	/* Clear resume steps */
	system->resume_steps = 0;

#if IS_ENABLED(CONFIG_PM_SLEEP)
#if !IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	/* prevent the system to suspend */
	if (!npu_wake_lock_active(system->ws)) {
		npu_wake_lock(system->ws);
		npu_info("wake_lock, now(%d)\n", npu_wake_lock_active(system->ws));
	}
	set_bit(NPU_SYS_RESUME_SETUP_WAKELOCK, &system->resume_steps);
#endif
#endif

	if (system->fw_cold_boot) {
		ret = npu_system_alloc_fw_dram_log_buf(system);
		if (ret) {
			npu_err("fail(%d) in npu_system_alloc_fw_dram_log_buf\n", ret);
			goto p_err;
		}
	}
	set_bit(NPU_SYS_RESUME_INIT_FWBUF, &system->resume_steps);

	if (system->fw_cold_boot) {
		npu_dbg("reset FW mailbox memory : vaddr 0x%p, daddr 0x%08llx, size %lu\n",
				fwmbox->dma.vaddr, fwmbox->dma.daddr, fwmbox->dma.size);

		memset(fwmbox->dma.vaddr, 0, fwmbox->dma.size);

		ret = npu_firmware_load(system);
		if (ret) {
			npu_err("fail(%d) in npu_firmware_load\n", ret);
			goto p_err;
		}
	}

	set_bit(NPU_SYS_RESUME_FW_LOAD, &system->resume_steps);
	set_bit(NPU_SYS_RESUME_FW_VERIFY, &system->resume_steps);
	set_bit(NPU_SYS_RESUME_CLK_PREPARE, &system->resume_steps);

	if (system->fw_cold_boot) {
		/* Clear mailbox area and setup some initialization variables */
		memset((void *)system->mbox_hdr, 0, sizeof(*system->mbox_hdr));
	}

	/* Invoke platform specific resume routine */
	if (system->fw_cold_boot) {
		fwmem = npu_get_mem_area(system, "fwmem");

		if (likely(fwmem && fwmem->dma.vaddr)) {
			npu_fw_slave_load(&vector_binary, fwmem->dma.vaddr + 0xf000);
			print_ufw_signature(fwmem);
			npu_ver_dump(device);
		}
	}

	ret = npu_system_soc_resume(system);
	if (ret) {
		npu_err("fail(%d) in npu_system_soc_resume\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_SOC, &system->resume_steps);

	ret = system->interface_ops->interface_open(system);
	if (ret) {
		npu_err("fail(%d) in npu_interface_open\n", ret);
		goto p_err;
	}
	set_bit(NPU_SYS_RESUME_OPEN_INTERFACE, &system->resume_steps);

	set_bit(NPU_SYS_RESUME_COMPLETED, &system->resume_steps);
#if IS_ENABLED(CONFIG_NPU_STM)
	system->fw_load_success = NPU_FW_LOAD_SUCCESS;
#endif
	return ret;
p_err:
	npu_err("Failure detected[%d]. Set emergency recovery flag.\n", ret);
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
	ret = 0;//emergency case will be cared by suspend func
	return ret;
}

int npu_system_suspend(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!system)) {
		npu_err("Failed to get npu system\n");
		return -EINVAL;
	}

	if (unlikely(!system->pdev)) {
		npu_err("Failed to get system->pdev\n");
		return -EINVAL;
	}

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

	npu_info("fw_cold_boot(%d)\n", system->fw_cold_boot);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_COMPLETED, &system->resume_steps, NULL, ;);

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_OPEN_INTERFACE, &system->resume_steps, "Close interface", {
		ret = system->interface_ops->interface_close(system);
		if (ret)
			npu_err("fail(%d) in npu_interface_close\n", ret);
	});

	/* Invoke platform specific suspend routine */
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SOC, &system->resume_steps, "SoC suspend", {
		ret = npu_system_soc_suspend(system);
		if (ret)
			npu_err("fail(%d) in npu_system_soc_suspend\n", ret);
	});

	if (system->fw_cold_boot)
		BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_FW_LOAD, &system->resume_steps, "FW load", {
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
			{
				unsigned long t_ret = 0;

				npu_info("Release s2mpu verification for %s locally\n", NPU_S2MPU_NAME);
				t_ret = exynos_s2mpu_release_fw_stage2_ap(NPU_S2MPU_NAME, 0);
				if (t_ret)
					npu_err("exynos_s2mpu_release_fw_stage2_ap is failed(%lu)\n", t_ret);
			}
#endif
		});

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_CLK_PREPARE, &system->resume_steps, "Unprepare clk", ;);
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_FW_VERIFY, &system->resume_steps, "FW VERIFY suspend", {
		/* Do not need anything */
	});

	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_INIT_FWBUF, &system->resume_steps, "Free DRAM fw log buf", {
		ret = npu_system_free_fw_dram_log_buf();
		if (ret)
			npu_err("fail(%d) in npu_cpu_off\n", ret);
	});

#if IS_ENABLED(CONFIG_PM_SLEEP)
#if !IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	BIT_CHECK_AND_EXECUTE(NPU_SYS_RESUME_SETUP_WAKELOCK, &system->resume_steps, "Unlock wake lock", {
		if (npu_wake_lock_active(system->ws)) {
			npu_wake_unlock(system->ws);
			npu_info("wake_unlock, now(%d)\n", npu_wake_lock_active(system->ws));
		}
	});
#endif
#endif

	if (system->resume_steps != 0)
		npu_warn("Missing clean-up steps [%lu] found.\n", system->resume_steps);

	/* Function itself never be failed, even thought there was some error */
	return 0;
}

int npu_system_start(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!system)) {
		npu_err("Failed to get npu system\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!system->pdev)) {
		npu_err("Failed to get system->pdev\n");
		ret = -EINVAL;
		goto p_err;
	}

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

	ret = npu_scheduler_start(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_start\n", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int npu_system_stop(struct npu_system *system)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!system)) {
		npu_err("Failed to get npu system\n");
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!system->pdev)) {
		npu_err("Failed to get system->pdev\n");
		ret = -EINVAL;
		goto p_err;
	}

	dev = &system->pdev->dev;
	device = container_of(system, struct npu_device, system);

	ret = npu_scheduler_stop(device);
	if (ret) {
		npu_err("fail(%d) in npu_scheduler_stop\n", ret);
		goto p_err;
	}

p_err:
	return ret;
}

#ifndef CONFIG_NPU_KUNIT_TEST
int npu_system_save_result(struct npu_session *session, struct nw_result nw_result)
{
	int ret = 0;

	sysPwr.system_result.result_code = nw_result.result_code;
	wake_up(&sysPwr.wq);
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR)
int __alloc_imb_chunk(struct npu_memory_buffer *IMB_mem_buf, struct npu_system *system,
		struct npu_session *session, struct imb_alloc_info *imb_range, u32 req_chunk_cnt)
{
	int ret = 0;
	dma_addr_t dAddr;
	u32 i, prev_chunk_cnt;

	WARN_ON(!IMB_mem_buf);

	prev_chunk_cnt = imb_range->alloc_chunk_cnt;
	for (i = imb_range->alloc_chunk_cnt; i < req_chunk_cnt; i++) {
		dAddr = imb_range->chunk_imb->paddr + (i * (dma_addr_t)NPU_IMB_CHUNK_SIZE);
		imb_range->chunk[i].dma.size = NPU_IMB_CHUNK_SIZE;

		if (session->need_sysmmu2) {
			dAddr += 0x100000000;
			ret = npu_memory_region_alloc_from_heap(system->pdev, session, &(imb_range->chunk[i]),
				dAddr, npu_memory_region_get_imb(session->manual), "IMB", 0);
		} else {
			ret = npu_memory_alloc_from_heap(
				system->pdev, &(imb_range->chunk[i]), dAddr, &system->memory, "IMB", 0);
		}

		if (unlikely(ret)) {
			npu_err("IMB: npu_memory_%s_from_heap failed, err: %d\n",
				(session->need_sysmmu2) ? "region_alloc" : "alloc", ret);
			ret = -EFAULT;
			goto err_exit;
		}

		imb_range->alloc_chunk_cnt++;
		npu_dbg("IMB(PAGE_TABLE[%llu]): successfully allocated chunk = %u with size = 0x%X\n",
			session->need_sysmmu2, i, NPU_IMB_CHUNK_SIZE);
	}

	if (prev_chunk_cnt != imb_range->alloc_chunk_cnt) {
		npu_dbg("IMB(PAGE_TABLE[%llu]): system total size 0x%X -> 0x%X\n",
			session->need_sysmmu2,
			prev_chunk_cnt * NPU_IMB_CHUNK_SIZE,
			 imb_range->alloc_chunk_cnt * NPU_IMB_CHUNK_SIZE);
	}

	IMB_mem_buf->dma.daddr = imb_range->chunk_imb->paddr;
	if (session->need_sysmmu2)
		IMB_mem_buf->dma.daddr += 0x100000000;

err_exit:
	return ret;
}

void __free_imb_chunk(u32 new_chunk_cnt, struct npu_system *system,
		struct npu_session *session, struct imb_alloc_info *imb_range)
{
	u32 i, cur_chunk_cnt;

	WARN_ON(!system);
	cur_chunk_cnt = imb_range->alloc_chunk_cnt;
	for (i = cur_chunk_cnt; i > new_chunk_cnt; i--) {
		if (session->need_sysmmu2) {
			npu_memory_region_free_from_heap(npu_memory_region_get_imb(session->manual),
						&(imb_range->chunk[i - 1]));
		} else {
			npu_memory_free_from_heap(&system->memory, &(imb_range->chunk[i - 1]), false);
		}
		imb_range->alloc_chunk_cnt--;
	}
	if (imb_range->alloc_chunk_cnt != cur_chunk_cnt) {
		npu_dbg("IMB(PAGE_TABLE[%llu]): system total size 0x%X -> 0x%X\n",
			session->need_sysmmu2,
			cur_chunk_cnt * NPU_IMB_CHUNK_SIZE,
			imb_range->alloc_chunk_cnt * NPU_IMB_CHUNK_SIZE);
	}
}
#endif

static int npu_firmware_load(struct npu_system *system)
{
	int ret = 0;
#if defined(CLEAR_SRAM_ON_FIRMWARE_LOADING)
	u32 v;
#endif /* CLEAR_SRAM_ON_FIRMWARE_LOADING */
	struct npu_memory_buffer *fwmem;

	fwmem = npu_get_mem_area(system, "fwmem");

#ifdef CLEAR_SRAM_ON_FIRMWARE_LOADING
#ifdef CLEAR_ON_SECOND_LOAD_ONLY

	if (unlikely(!system->fwmemory)) {
		npu_err("Failed to get system->fwmemory\n");
		ret = -EINVAL;
		goto err_exit;
	}

	npu_dbg("start\n");
	if (fwmem->vaddr) {
		v = readl(fwmem->vaddr + fwmem->size - sizeof(u32));
		npu_dbg("firmware load: Check current signature value : 0x%08x (%s)\n",
			v, (v == 0)?"First load":"Second load");
	}
#else
	v = 1;
#endif /* CLEAR_ON_SECOND_LOAD_ONLY */
	if (v != 0) {
		if (fwmem->dma.vaddr) {
			npu_dbg("firmware load : clear working memory at %p(0x%llx), Len(%llu)\n",
				fwmem->dma.vaddr, fwmem->dma.daddr, (long long unsigned int)fwmem->dma.size);
			/* Using memset here causes unaligned access fault.
			Refer: https://patchwork.kernel.org/patch/6362401/ */
			memset(fwmem->dma.vaddr, 0, fwmem->dma.size);
		}
	}
#else
	if (fwmem->vaddr) {
		npu_dbg("firmware load: clear firmware signature at %pK(u64)\n",
			fwmem->vaddr + fwmem->size - sizeof(u64));
		writel(0, fwmem->vaddr + fwmem->size - sizeof(u64));
	}
#endif /* CLEAR_SRAM_ON_FIRMWARE_LOADING */

	if (fwmem->dma.vaddr) {
		npu_dbg("firmware load: read and locate firmware to %pK\n", fwmem->dma.vaddr);
		ret = npu_fw_load(&system->binary,
					fwmem->dma.vaddr, fwmem->dma.size);
		if (ret) {
			npu_err("error(%d) in npu_fw_load\n", ret);
			goto err_exit;
		}
		npu_dbg("checking firmware head MAGIC(0x%08x)\n", *(u32 *)fwmem->dma.vaddr);
	}

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	{
		unsigned long t_ret = 0;

		npu_info("Start s2mpu verification for %s locally\n", NPU_S2MPU_NAME);
		t_ret = exynos_s2mpu_verify_subsystem_fw(NPU_S2MPU_NAME,
					0,
					system->binary.rmem_base,
					system->binary.image_size,
					system->binary.rmem_size);
		if (t_ret) {
			npu_err("exynos_s2mpu_verty_subsystem_fw is failed(%lu)\n", t_ret);
			goto err_exit;
		}

		t_ret = exynos_s2mpu_request_fw_stage2_ap(NPU_S2MPU_NAME);
		if (t_ret) {
			npu_err("exynos_s2mpu_verty_subsystem_fw is failed(%lu)\n", t_ret);
			goto err_exit;
		}
	}
#endif /* CONFIG_EXYNOS_S2MPU */

	npu_dbg("complete\n");
	return ret;
err_exit:

	npu_err("error(%d)\n", ret);
	return ret;
}
