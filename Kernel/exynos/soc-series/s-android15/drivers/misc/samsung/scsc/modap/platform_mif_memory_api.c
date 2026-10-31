/****************************************************************************
 *
 * Copyright (c) 2014 - 2024 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
#include "../modap/platform_mif_memory_api.h"
#include "../modap/platform_mif_regmap_api.h"
#include "../modap/platform_mif_irq_api.h"
#include "../mif_reg.h"
#else
#include "modap/platform_mif_memory_api.h"
#include "modap/platform_mif_regmap_api.h"
#include "modap/platform_mif_irq_api.h"
#include "mif_reg.h"
#endif

#ifdef CONFIG_WLBT_KUNIT
#include "../kunit/kunit_platform_mif_irq_api.c"
#endif

void __iomem *base_wlan;
void __iomem *base_wpan;
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
void __iomem *base_pmu;
#endif

const char *platform_mif_get_target_name(enum mbox_base_target target)
{
	switch (target) {
		case MBOX_BASE_TARGET_WLAN:	return "WLAN";
		case MBOX_BASE_TARGET_WPAN:	return "WPAN";
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
		case MBOX_BASE_TARGET_PMU:	return "PMU";
#endif
		default:			return NULL;
	}
}

static void __iomem *platform_mif_get_base_from_mbox_target(enum mbox_base_target target)
{
	switch (target) {
		case MBOX_BASE_TARGET_WLAN:	return base_wlan;
		case MBOX_BASE_TARGET_WPAN:	return base_wpan;
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
		case MBOX_BASE_TARGET_PMU:	return base_pmu;
#endif
		default:			return NULL;
	}
}

const char *platform_mif_get_name_from_abs_target(enum scsc_mif_abs_target target)
{
	switch (target) {
		case SCSC_MIF_ABS_TARGET_WLAN:	return "WLAN";
		case SCSC_MIF_ABS_TARGET_WPAN:	return "WPAN";
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
		case SCSC_MIF_ABS_TARGET_PMU:	return "PMU";
#endif
		default:			return "ETC, assume as WLAN";
	}
}

void __iomem *platform_mif_get_base_from_abs_target(enum scsc_mif_abs_target target)
{
	switch (target) {
		case SCSC_MIF_ABS_TARGET_WLAN:	return base_wlan;
		case SCSC_MIF_ABS_TARGET_WPAN:	return base_wpan;
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
		case SCSC_MIF_ABS_TARGET_PMU:	return base_pmu;
#endif
		default:			return base_wlan;
	}
}

int platform_mif_init_ioresource(
	struct platform_device *pdev,
	struct platform_mif *platform)
{
	enum mbox_base_target target;

	for (target = MBOX_BASE_TARGET_WLAN; target < MBOX_BASE_TARGET_SIZE; target++) {
		struct resource *reg_res = platform_get_resource(pdev, IORESOURCE_MEM, target);
		void __iomem *base;
		switch (target) {
			case MBOX_BASE_TARGET_WLAN:
				base_wlan = devm_ioremap_resource(&pdev->dev, reg_res);
				base = base_wlan;
				break;
			case MBOX_BASE_TARGET_WPAN:
				base_wpan = devm_ioremap_resource(&pdev->dev, reg_res);
				base = base_wpan;
				break;
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
			case MBOX_BASE_TARGET_PMU:
				base_pmu = devm_ioremap_resource(&pdev->dev, reg_res);
				base = base_pmu;
				break;
#endif
			default:
				base = NULL;
		}
		if (IS_ERR(base)) {
			const char *target_name = platform_mif_get_target_name(target);
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"Error getting mem resource for %s\n", target_name);
			return PTR_ERR(base);
		}
	}
	return 0;
}

void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size)
{
	size_t      i;
	struct page **pages;
	void        *vmem;

	size = PAGE_ALIGN(size);
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: kmalloc of %zd byte pages table failed\n", (size >> PAGE_SHIFT) * sizeof(*pages));
		return NULL;
	}
#else
	/* Reserve the table statically, but make sure .dts doesn't exceed it */
	{
		static struct page *mif_map_pages[(MIFRAMMAN_MAXMEM >> PAGE_SHIFT)];
		static struct page *mif_map_pagesT[(MIFRAMMAN_MAXMEM >> PAGE_SHIFT) * sizeof(*pages)];

		pages = mif_map_pages;

		SCSC_TAG_INFO(PLAT_MIF, "count %d, PAGE_SHFIT = %d\n", MIFRAMMAN_MAXMEM >> PAGE_SHIFT,PAGE_SHIFT);
		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));
		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pagesT size %zd\n", sizeof(mif_map_pagesT));

		if (size > MIFRAMMAN_MAXMEM) { /* Size passed in from .dts exceeds array */
			SCSC_TAG_ERR(PLAT_MIF, "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
					size, MIFRAMMAN_MAXMEM);
			return NULL;
		}
	}
#endif

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	kfree(pages);
#endif
	if (!vmem)
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: vmap of %zd pages failed\n", (size >> PAGE_SHIFT));
	return (void __iomem *)vmem;
}

void platform_mif_unmap_region(void *vmem)
{
	vunmap(vmem);
}

static void platform_mif_subsystem_mailbox_init(void)
{
	enum mbox_base_target target, i;
	for (target = 0; target < MBOX_BASE_TARGET_SIZE; target++) {
		/* MBOXes */
		void __iomem *base = platform_mif_get_base_from_mbox_target(target);

		for (i = 0; i < NUM_MBOX_PLAT; i++){
			writel(ZERO_PAD_32BIT, base + MAILBOX_WLBT_REG(ISSR(i)));
		}
		/* MRs */ /*1's - set all as Masked */
		writel(HOST_TO_FW_MASK, base + MAILBOX_WLBT_REG(INTMR0));
		writel(FW_TO_HOST_MASK, base + MAILBOX_WLBT_REG(INTMR1));
	}
}

static void platform_mif_subsystem_mailbox_clear(void)
{
	enum mbox_base_target target;
	for (target = 0; target < MBOX_BASE_TARGET_SIZE; target++) {
		void __iomem *base = platform_mif_get_base_from_mbox_target(target);
		/* CRs */ /* 1's - clear all the interrupts */
                writel(HOST_TO_FW_MASK, base + MAILBOX_WLBT_REG(INTCR0));
                writel(FW_TO_HOST_MASK, base + MAILBOX_WLBT_REG(INTCR1));
	}
}

void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (allocated)
		*allocated = 0;

	platform->mem = platform_mif_map_region(platform->mem_start, platform->mem_size);

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error remaping shared memory\n");
		return NULL;
	}

#ifdef CONFIG_SCSC_WLBT_PTR_PRINT
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Map: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
#else
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Mapping the shared memory\n");
#endif

	platform_mif_subsystem_mailbox_init();
	platform_mif_subsystem_mailbox_clear();

#ifdef CONFIG_SCSC_CHV_SUPPORT
	if (chv_disable_irq == true) {
		if (allocated)
			*allocated = platform->mem_size;
		return platform->mem;
	}
#endif
	/* register interrupts */
	if (platform_mif_register_irq(platform)) {
#ifdef CONFIG_SCSC_WLBT_PTR_PRINT
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
#else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmapping the shared memory\n");
#endif
		platform_mif_unmap_region(platform->mem);
		return NULL;
	}

	if (allocated)
		*allocated = platform->mem_size;
	/* Set the CR4 base address in Mailbox??*/
	return platform->mem;
}

/* HERE: Not sure why mem is passed in - its stored in platform - as it should be */
void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Avoid unused parameter error */
	(void)mem;

	platform_mif_subsystem_mailbox_init();

#ifdef CONFIG_SCSC_CHV_SUPPORT
	/* Restore PIO changed by Maxwell subsystem */
	if (chv_disable_irq == false)
		/* Unregister IRQs */
		platform_mif_unregister_irq(platform);
#else
	platform_mif_unregister_irq(platform);
#endif
	platform_mif_subsystem_mailbox_clear();

#ifdef CONFIG_SCSC_WLBT_PTR_PRINT
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
#else
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmapping the shared memory\n");
#endif
	platform_mif_unmap_region(platform->mem);
	platform->mem = NULL;
}

u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	void __iomem *base = platform_mif_get_base_from_abs_target(target);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "mbox index 0x%x target %s\n", mbox_index,
	   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");

	return base + MAILBOX_WLBT_REG(ISSR(mbox_index));
}

#if defined(CONFIG_WLBT_PMU2AP_MBOX)
int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val;
	u32 irq_val;

	irq_val = platform_mif_reg_read(SCSC_MIF_ABS_TARGET_PMU, MAILBOX_WLBT_REG(INTMSR0)) >> 16;
	if (!irq_val){
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Wrong PMU MAILBOX Interrupt!!\n");
		return 0;
	}

	val = platform_mif_reg_read(SCSC_MIF_ABS_TARGET_PMU, MAILBOX_WLBT_REG(ISSR(0)));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read PMU MAILBOX: %u\n", val);
	platform_mif_reg_write(SCSC_MIF_ABS_TARGET_PMU, MAILBOX_WLBT_REG(INTCR0), (1 << 16));
	return val;
}

int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform_mif_reg_write(SCSC_MIF_ABS_TARGET_PMU, MAILBOX_WLBT_REG(ISSR(0)), val);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write PMU MAILBOX: %u\n", val);

	platform_mif_reg_write(SCSC_MIF_ABS_TARGET_PMU, MAILBOX_WLBT_REG(INTGR1), 1);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR1: bit 1 on target PMU\n");
	return 0;
}
#else
int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);
	u32 val;

	regmap_read(regmap, WB2AP_MAILBOX, &val);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

	regmap_write(regmap, AP2WB_MAILBOX, val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write AP2WB_MAILBOX: %u\n", val);
	return 0;
}

#endif

void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_start) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	return (void *)((uintptr_t)platform->mem_start + (uintptr_t)ref);
}

uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return vmalloc_to_pfn(platform->mem);
}

int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem + platform->mem_size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Unable to get pointer reference\n");
		return -ENOMEM;
	}

	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)platform->mem);

	return 0;
}

void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size)
		return (void *)((uintptr_t)platform->mem + (uintptr_t)ref);
	else
		return NULL;
}

void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
		"Setting remapper address %u target %s\n",
		remap_addr,
		(target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform->remap_addr_wlan = remap_addr;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		platform->remap_addr_wpan = remap_addr;
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Incorrect target %d\n", target);
}

void platform_mif_memory_api_init(struct scsc_mif_abs *interface)
{
	interface->map = platform_mif_map;
	interface->unmap = platform_mif_unmap;
	interface->remap_set = platform_mif_remap_set;
	interface->get_mifram_ptr = platform_mif_get_mifram_ptr;
	interface->get_mifram_ref = platform_mif_get_mifram_ref;
	interface->get_mifram_pfn = platform_mif_get_mif_pfn;
	interface->get_mbox_pmu = platform_mif_get_mbox_pmu;
	interface->get_mbox_ptr = platform_mif_get_mbox_ptr;
	interface->get_mifram_phy_ptr = platform_mif_get_mifram_phy_ptr;
	interface->set_mbox_pmu = platform_mif_set_mbox_pmu;
}
