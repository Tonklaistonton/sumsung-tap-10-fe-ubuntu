#ifndef __PLATFORM_MIF_MODAP_MEMORY_API_H__
#define __PLATFORM_MIF_MODAP_MEMORY_API_H__

#include "platform_mif.h"

#define SCSC_STATIC_MIFRAM_PAGE_TABLE
#define HOST_TO_FW_MASK 0xffff0000
#define FW_TO_HOST_MASK 0x0000ffff
#define ZERO_PAD_32BIT	0x00000000

enum mbox_base_target {
	MBOX_BASE_TARGET_WLAN = 0,
	MBOX_BASE_TARGET_WPAN,
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
	MBOX_BASE_TARGET_PMU,
#endif
	MBOX_BASE_TARGET_SIZE,
	/* Must be the last */
};

const char *platform_mif_get_name_from_abs_target(enum scsc_mif_abs_target target);
void *__iomem platform_mif_get_base_from_abs_target(enum scsc_mif_abs_target target);
void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated);
void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem);
void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target);
u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target);
int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface);
int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val);
int platform_mif_get_mbox_pmu_error(struct scsc_mif_abs *interface);
int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref);
void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref);
uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface);
int __init platform_mif_wifibt_if_reserved_mem_setup(struct reserved_mem *remem);

void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target);
void platform_mif_memory_api_init(struct scsc_mif_abs *interface);
int platform_mif_init_ioresource(struct platform_device *pdev, struct platform_mif *platform);
#endif
