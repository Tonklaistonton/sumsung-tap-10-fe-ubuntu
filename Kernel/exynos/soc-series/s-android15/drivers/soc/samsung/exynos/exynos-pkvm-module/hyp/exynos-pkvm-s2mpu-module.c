/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 Hypervisor module
 *
 */

#include <asm/alternative-macros.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/page.h>

#include <nvhe/iommu.h>

#include <linux/kernel.h>
#include <linux/err.h>

#include <soc/samsung/exynos/exynos-pkvm-module.h>

static struct pkvm_module_ops mod_ops;
pkvm_mod_vm_request_t exynos_pkvm_mod_vm_request;

static int exynos_pkvm_s2mpu_init(unsigned long arg)
{
	/*
	 * Notify pKVM S2MPU driver has been initialized.
	 */
	int ret = 0;

	ret = mod_ops.iommu_snapshot_host_stage2(NULL);

	return ret;
}

static struct kvm_hyp_iommu *exynos_pkvm_s2mpu_get_iommu(pkvm_handle_t smmu_id)
{
	return 0;
}

static int exynos_pkvm_s2mpu_alloc_domain(struct kvm_hyp_iommu_domain *domain, u32 type)
{
	return 0;
}

static void exynos_pkvm_s2mpu_free_domain(struct kvm_hyp_iommu_domain *domain)
{
	return;
}
static int exynos_pkvm_s2mpu_attach_dev(struct kvm_hyp_iommu *iommu,
					struct kvm_hyp_iommu_domain *domain,
					u32 endpoint_id,
					u32 pasid,
					u32 pasid_bits)
{
	return 0;
}
static int exynos_pkvm_s2mpu_detach_dev(struct kvm_hyp_iommu *iommu,
					struct kvm_hyp_iommu_domain *domain,
					u32 endpoint_id,
					u32 pasid)
{
	return 0;
}
static bool exynos_pkvm_s2mpu_dabt_handler(struct kvm_cpu_context *host_ctxt,
					   u64 esr, u64 addr)
{
	return false;
}

static int exynos_pkvm_s2mpu_suspend(struct kvm_hyp_iommu *iommu)
{
	/*
	 * Suspend and resume will be managed by HVC
	 */
	return 0;
}

static int exynos_pkvm_s2mpu_resume(struct kvm_hyp_iommu *iommu)
{
	/*
	 * Suspend and resume will be managed by HVC
	 */
	return 0;
}

static void exynos_pkvm_iotlb_sync(struct kvm_hyp_iommu_domain *domain,
				   struct iommu_iotlb_gather *gather)
{
	// TODO:
}

static __nocfi void exynos_pkvm_s2mpu_stage2_idmap(struct kvm_hyp_iommu_domain *domain,
						   phys_addr_t start, phys_addr_t end,
						   int prot)
{
	int ret = 0;
	ret = exynos_pkvm_mod_vm_request(start, end, prot);
}

struct kvm_iommu_ops exynos_kvm_iommu_ops = {
	.init = exynos_pkvm_s2mpu_init,
	.get_iommu_by_id = exynos_pkvm_s2mpu_get_iommu,
	.alloc_domain = exynos_pkvm_s2mpu_alloc_domain,
	.free_domain = exynos_pkvm_s2mpu_free_domain,
	.attach_dev = exynos_pkvm_s2mpu_attach_dev,
	.detach_dev = exynos_pkvm_s2mpu_detach_dev,
	.dabt_handler = exynos_pkvm_s2mpu_dabt_handler,
	.suspend = exynos_pkvm_s2mpu_suspend,
	.resume = exynos_pkvm_s2mpu_resume,
	.iotlb_sync = exynos_pkvm_iotlb_sync,
	.host_stage2_idmap = exynos_pkvm_s2mpu_stage2_idmap,
};

int exynos_pkvm_s2mpu_module_init(const struct pkvm_module_ops *ops)
{
	int ret = 0;

	mod_ops.iommu_snapshot_host_stage2 = ops->iommu_snapshot_host_stage2;

	return ret;
}
