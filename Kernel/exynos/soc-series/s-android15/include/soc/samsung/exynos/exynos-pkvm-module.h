/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS pKVM module
 *
 */

#ifndef __EXYNOS_PKVM_MODULE_H
#define __EXYNOS_PKVM_MODULE_H

#include <asm/kvm_pkvm_module.h>

#define cpu_reg(ctxt, r)	(ctxt)->regs.regs[r]
#define DECLARE_REG(type, name, ctxt, reg)	\
				type name = (type)cpu_reg(ctxt, (reg))

#define user_cpu_reg(ctxt, r)	ctxt->regs[r]
#define DECLARE_USER_REG(type, name, ctxt, reg)	\
				type name = (type)user_cpu_reg(ctxt, (reg))

#define EXYNOS_PKVM_HEADER_MAGIC_SIZE		(0x10)
#define EXYNOS_PKVM_HEADER_INST			(2)

#ifndef __ASSEMBLY__

struct exynos_module_ops {
	int (*create_private_mapping)(phys_addr_t phys, size_t size,
				      enum kvm_pgtable_prot prot,
				      unsigned long *haddr);
	void *(*fixmap_map)(phys_addr_t phys);
	void (*fixmap_unmap)(void);
	void (*flush_dcache_to_poc)(void *addr, size_t size);
	int (*host_stage2_mod_prot)(u64 pfn, enum kvm_pgtable_prot prot, u64 nr_pages, bool update_iommu);
	int (*host_stage2_get_leaf)(phys_addr_t phys, kvm_pte_t *ptep, u32 *level);
	int (*host_donate_hyp)(u64 pfn, u64 nr_pages, bool accept_mmio);
	int (*hyp_donate_host)(u64 pfn, u64 nr_pages);
	phys_addr_t (*hyp_pa)(void *x);
	void* (*hyp_va)(phys_addr_t phys);
	int (*host_share_hyp)(u64 pfn);
	int (*host_unshare_hyp)(u64 pfn);
	int (*pin_shared_mem)(void *from, void *to);
	void (*unpin_shared_mem)(void *from, void *to);
	void (*update_hcr_el2)(unsigned long set_mask, unsigned long clear_mask);
	void (*update_hfgwtr_el2)(unsigned long set_mask, unsigned long clear_mask);
};

typedef unsigned long (*pkvm_mod_entry_t)(struct exynos_module_ops *ops,
					  unsigned long va_offset,
					  unsigned long kimg_offset,
					  unsigned long page_size);

typedef unsigned long (*pkvm_mod_hvc_handler_t)(unsigned int hvc_fid,
						unsigned long x1,
						unsigned long x2,
						unsigned long x3,
						unsigned long x4,
						void *cookie,
						void *cpu_context);

typedef unsigned long (*pkvm_mod_s2mpu_handler_t)(unsigned long x0,
						  unsigned long x1,
						  unsigned long x2,
						  unsigned long x3,
						  unsigned long x4);
typedef int (*pkvm_mod_sync_handler_t)(unsigned long esr_el2,
				       unsigned long elr_el2,
				       unsigned long sp_el1,
				       void *gpregs,
				       void *cookie);

typedef void (*pkvm_mod_psci_handler_t)(unsigned int psci_type,
					unsigned long power_state);

typedef int (*plugin_runtime_init_t)(uintptr_t plugin_runtime_entry,
				     unsigned long va_offset,
				     unsigned long plugin_base,
				     size_t plugin_size);
typedef unsigned long (*pkvm_mod_vm_request_t)(u64 base,
					       u64 size,
					       int prot);

/*
 * struct exynos_pkvm_mod_sec - Exynos pKVM module section
 *
 */
struct exynos_pkvm_mod_sec {
	unsigned long long text_start;
	unsigned long long text_end;
	unsigned long long rodata_start;
	unsigned long long rodata_end;
	unsigned long long rwdata_start;
	unsigned long long rwdata_end;
	unsigned long long got_start;
	unsigned long long got_end;
	unsigned long long rela_start;
	unsigned long long rela_end;
};

/*
 * struct exynos_pkvm_header - Exynos pKVM module binary header
 *
 */
struct exynos_pkvm_header {
	unsigned long long head[EXYNOS_PKVM_HEADER_INST];
	char bin_str[EXYNOS_PKVM_HEADER_MAGIC_SIZE];
	struct exynos_pkvm_mod_sec sec_info;
	unsigned long exynos_pkvm_mod_entrypoint;
};

/*
 * struct plugin_memory_info - Plug-in memory/address information
 *
 */
struct plugin_memory_info {
	unsigned long base_va;
	unsigned long base;
	size_t size;
	unsigned long offset;
	struct exynos_pkvm_mod_sec section;
};

/*
 * struct exynos_module_info
 * struct exynos_plugin_info
 *
 * - The information of Plug-in modules
 */
struct plugin_module_info {
	struct plugin_memory_info mem;
	uintptr_t plugin_runtime_entry;
};

struct exynos_plugin_info {
	struct plugin_module_info *modules;
	unsigned int plugin_num;
};

struct sysreg_mask {
	unsigned long set_mask;
	unsigned long clear_mask;
};
/*
 * struct exynos_pkvm_interface - Exynos pKVM module binary interface
 *
 */
struct exynos_pkvm_interface {
	pkvm_mod_hvc_handler_t hvc_handler;
	pkvm_mod_s2mpu_handler_t s2mpu_prepare;
	pkvm_mod_sync_handler_t sync_handler;
	pkvm_mod_psci_handler_t psci_handler;
	struct exynos_plugin_info *plugin_module;
	plugin_runtime_init_t plugin_runtime_init;
	struct sysreg_mask hcr_el2_mask;
	pkvm_mod_vm_request_t vm_request;
};

int exynos_pkvm_update_fg_write_trap(unsigned long sysreg_bit_pos,
				     bool trap);
pkvm_mod_vm_request_t exynos_pkvm_module_get_s2mpu_ptr(void);

#endif	/* __ASSEMBLY__ */

#endif	/* __EXYNOS_PKVM_MODULE_H */
