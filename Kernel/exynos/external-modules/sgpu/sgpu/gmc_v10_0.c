/*
 * Copyright 2019,2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include "amdgpu.h"
#include "gmc_v10_0.h"

#include "athub/athub_2_0_0_sh_mask.h"
#include "athub/athub_2_0_0_offset.h"
#include "dcn/dcn_2_0_0_offset.h"
#include "dcn/dcn_2_0_0_sh_mask.h"
#include "ivsrcid/vmc/irqsrcs_vmc_1_0.h"
#include "navi10_enum.h"

#include "soc15.h"
#include "soc15_common.h"

#include "gfxhub_v2_1.h"
#include "mmhub_v2_0.h"
#include "athub_v2_0.h"
#include "amdgpu_cwsr.h"

#if IS_ENABLED(CONFIG_DRM_AMDGPU_GMC_DUMP)
#include "sgpu.h"
#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "gc/gc_10_4_0_default.h"
#endif

#if 0
static const struct soc15_reg_golden golden_settings_navi10_hdp[] =
{
	/* TODO add golden setting for hdp */
};
#endif

/* The below ECC functions are routed RAS interface via UMC IP. However UMC IP
 * and RAS features are not supported by SGPU. Set these as empty functions for
 * now.
 *
 * @todo GFXSW-10162  Check if these can be removed completely.
 */
static int gmc_v10_0_ecc_interrupt_state(struct amdgpu_device *adev,
					 struct amdgpu_irq_src *src,
					 unsigned type,
					 enum amdgpu_interrupt_state state)
{
	return 0;
}

static int gmc_v10_0_process_ecc_irq(struct amdgpu_device *adev,
		struct amdgpu_irq_src *source,
		struct amdgpu_iv_entry *entry)
{
	/* This is routed to UMC which is not present in SGPU */
	return 0;
}

static int
gmc_v10_0_vm_fault_interrupt_state(struct amdgpu_device *adev,
				   struct amdgpu_irq_src *src, unsigned type,
				   enum amdgpu_interrupt_state state)
{
	switch (state) {
	case AMDGPU_IRQ_STATE_DISABLE:
		/* GFX HUB */
		amdgpu_gmc_set_vm_fault_masks(adev, AMDGPU_GFXHUB_0, false);
		break;
	case AMDGPU_IRQ_STATE_ENABLE:
		/* GFX HUB */
		amdgpu_gmc_set_vm_fault_masks(adev, AMDGPU_GFXHUB_0, true);
		break;
	default:
		break;
	}

	return 0;
}

static int gmc_v10_0_process_interrupt(struct amdgpu_device *adev,
				       struct amdgpu_irq_src *source,
				       struct amdgpu_iv_entry *entry)
{
	struct amdgpu_vmhub *hub = &adev->vmhub[entry->vmid_src];
	uint32_t status = 0;
	u64 addr;

	addr = (u64)entry->src_data[0] << 12;
	addr |= ((u64)entry->src_data[1] & 0xf) << 44;

	if (!amdgpu_sriov_vf(adev)) {
		/*
		 * Issue a dummy read to wait for the status register to
		 * be updated to avoid reading an incorrect value due to
		 * the new fast GRBM interface.
		 */
		if (entry->vmid_src == AMDGPU_GFXHUB_0)
			RREG32(hub->vm_l2_pro_fault_status);

		status = RREG32(hub->vm_l2_pro_fault_status);
		WREG32_P(hub->vm_l2_pro_fault_cntl, 1, ~1);
	}

	if (printk_ratelimit()) {
		struct amdgpu_task_info task_info;
		uint32_t pasid = entry->pasid;
		struct amdgpu_vm *vm;

		if (pasid == 0 && entry->vmid_src < adev->num_vmhubs &&
		    entry->vmid < AMDGPU_NUM_VMID) {
			struct amdgpu_vmid_mgr *id_mgr =
				&adev->vm_manager.id_mgr[entry->vmid_src];
			pasid = id_mgr->ids[entry->vmid].pasid;

			dev_err(adev->dev, "[%s] pasid parsing with vmid %u\n",
				entry->vmid_src ? "mmhub" : "gfxhub",
				entry->vmid);
		}

		memset(&task_info, 0, sizeof(struct amdgpu_task_info));
		amdgpu_vm_get_task_info(adev, pasid, &task_info);

		dev_err(adev->dev,
			"[%s] page fault (src_id:%u ring:%u vmid:%u pasid:%u, "
			"for process %s pid %d thread %s pid %d)\n",
			entry->vmid_src ? "mmhub" : "gfxhub",
			entry->src_id, entry->ring_id, entry->vmid,
			pasid, task_info.process_name, task_info.tgid,
			task_info.task_name, task_info.pid);
		dev_err(adev->dev, "  in page starting at address 0x%016llx from client %d\n",
			addr, entry->client_id);
		SGPU_LOG(adev, DMSG_INFO, DMSG_ETC,
				"[%s] page fault (src_id:%u ring:%u vmid:%u pasid:%u, for process %s pid %d thread %s pid %d)\n",
				entry->vmid_src ? "mmhub" : "gfxhub",
				entry->src_id, entry->ring_id, entry->vmid,
				pasid, task_info.process_name, task_info.tgid,
				task_info.task_name, task_info.pid);

		if (!amdgpu_sriov_vf(adev))
			hub->vmhub_funcs->print_l2_protection_fault_status(adev, status);
		/* print the status bits from the L2 status register */
		if (hub->vmhub_funcs->parity_errors_show)
			hub->vmhub_funcs->parity_errors_show(adev);

		vm = amdgpu_vm_pasid2vm(adev, pasid);
		if (vm) {
			dev_info(adev->dev, "Find PTE for %#018llx : %#018llx",
				addr, sgpu_vm_get_pte(adev, vm, addr));
			sgpu_vm_log_print_last_info(adev, vm);
		}
		else
			dev_warn(adev->dev, "failed to find vm for pasid(%u)\n", pasid);
	}

	/* page_faulted_vmid setting for skipping resubmit */
	adev->page_faulted_vmid = entry->vmid;
	DRM_INFO("page_faulted_vmid %d set\n", entry->vmid);
	if (adev->runpm)
		pm_runtime_get_noresume(adev->ddev.dev);

	/* Record fault for user query */
	{
		uint32_t pasid = entry->pasid;
		struct amdgpu_vm * vm;

		if (pasid == 0 && entry->vmid_src < adev->num_vmhubs &&
		    entry->vmid < AMDGPU_NUM_VMID) {
			struct amdgpu_vmid_mgr *id_mgr =
				&adev->vm_manager.id_mgr[entry->vmid_src];
			pasid = id_mgr->ids[entry->vmid].pasid;

			/* TODO: Include mmhub/gfxhub as info in fault record? */
			/* TODO: Improve fault address resolution (currently page boundary)? */
		}

		vm = amdgpu_vm_pasid2vm(adev, pasid);
		if (vm) {
			struct amdgpu_fpriv *fpriv = container_of(vm, struct amdgpu_fpriv, vm);
			sgpu_fault_write_record(&fpriv->page_fault_record, addr, pasid);
		}else{
			DRM_ERROR("failed to find vm for pasid");
		}
	}

	queue_delayed_work(system_wq, &adev->page_fault_work,
				msecs_to_jiffies(100));

	return 0;
}

static const struct amdgpu_irq_src_funcs gmc_v10_0_irq_funcs = {
	.set = gmc_v10_0_vm_fault_interrupt_state,
	.process = gmc_v10_0_process_interrupt,
};

static const struct amdgpu_irq_src_funcs gmc_v10_0_ecc_funcs = {
	.set = gmc_v10_0_ecc_interrupt_state,
	.process = gmc_v10_0_process_ecc_irq
};

 static void gmc_v10_0_set_irq_funcs(struct amdgpu_device *adev)
{
	adev->gmc.vm_fault.num_types = 1;
	adev->gmc.vm_fault.funcs = &gmc_v10_0_irq_funcs;

	if (!amdgpu_sriov_vf(adev)) {
		adev->gmc.ecc_irq.num_types = 1;
		adev->gmc.ecc_irq.funcs = &gmc_v10_0_ecc_funcs;
	}
}

/**
 * gmc_v10_0_use_invalidate_semaphore - judge whether to use semaphore
 *
 * @adev: amdgpu_device pointer
 * @vmhub: vmhub type
 *
 */
static bool gmc_v10_0_use_invalidate_semaphore(struct amdgpu_device *adev,
				       uint32_t vmhub)
{
	return ((vmhub == AMDGPU_MMHUB_0 ||
		 vmhub == AMDGPU_MMHUB_1) &&
		(!amdgpu_sriov_vf(adev)));
}

static bool gmc_v10_0_get_atc_vmid_pasid_mapping_info(
					struct amdgpu_device *adev,
					uint8_t vmid, uint16_t *p_pasid)
{
	uint32_t value;

	value = RREG32(SOC15_REG_OFFSET(ATHUB, 0, mmATC_VMID0_PASID_MAPPING)
		     + vmid);
	*p_pasid = value & ATC_VMID0_PASID_MAPPING__PASID_MASK;

	return !!(value & ATC_VMID0_PASID_MAPPING__VALID_MASK);
}

/*
 * GART
 * VMID 0 is the physical GPU addresses as used by the kernel.
 * VMIDs 1-15 are used for userspace clients and are handled
 * by the amdgpu vm/hsa code.
 */

void gmc_v10_0_flush_vm_hub(struct amdgpu_device *adev, uint32_t vmid,
			    unsigned int vmhub, uint32_t flush_type)
{
	bool use_semaphore = gmc_v10_0_use_invalidate_semaphore(adev, vmhub);
	struct amdgpu_vmhub *hub = &adev->vmhub[vmhub];
	u32 inv_req;
	u32 tmp;
	/* Use register 17 for GART */
	const unsigned eng = 17;
	unsigned int i;

	if (sgpu_no_hw_access != 0)
		return;

	if ((vmhub == AMDGPU_MMHUB_0 || vmhub == AMDGPU_MMHUB_1)) {
		DRM_INFO("Skip %s for MMHUB!\n", __func__);
		return;
	}

	inv_req = hub->vmhub_funcs->get_invalidate_req(vmid, flush_type);

	/*
	 * It may lose gpuvm invalidate acknowldege state across power-gating
	 * off cycle, add semaphore acquire before invalidation and semaphore
	 * release after invalidation to avoid entering power gated state
	 * to WA the Issue
	 */

	/* TODO: It needs to continue working on debugging with semaphore for GFXHUB as well. */
	if (use_semaphore) {
		for (i = 0; i < adev->usec_timeout; i++) {
			/* a read return value of 1 means semaphore acuqire */
			tmp = RREG32_NO_KIQ(hub->vm_inv_eng0_sem +
					    hub->eng_distance * eng);
			if (tmp & 0x1)
				break;
			udelay(1);
		}

		if (i >= adev->usec_timeout)
			DRM_ERROR("Timeout waiting for sem acquire in VM flush!\n");
	}

	WREG32_NO_KIQ(hub->vm_inv_eng0_req + hub->eng_distance * eng, inv_req);

	/*
	 * Issue a dummy read to wait for the ACK register to be cleared
	 * to avoid a false ACK due to the new fast GRBM interface.
	 */
	RREG32_NO_KIQ(hub->vm_inv_eng0_req + hub->eng_distance * eng);

	/* Wait for ACK with a delay.*/
	for (i = 0; i < adev->usec_timeout; i++) {
		tmp = RREG32_NO_KIQ(hub->vm_inv_eng0_ack +
				    hub->eng_distance * eng);
		tmp &= 1 << vmid;
		if (tmp)
			break;

		if (!amdgpu_emu_mode)
			udelay(1);
		else
			mdelay(1);
	}

	/* TODO: It needs to continue working on debugging with semaphore for GFXHUB as well. */
	if (use_semaphore)
		/*
		 * add semaphore release after invalidation,
		 * write with 0 means semaphore release
		 */
		WREG32_NO_KIQ(hub->vm_inv_eng0_sem +
			      hub->eng_distance * eng, 0);

	if (i < adev->usec_timeout)
		return;

	DRM_ERROR("Timeout waiting for VM flush ACK!\n");
}

/**
 * gmc_v10_0_flush_gpu_tlb - gart tlb flush callback
 *
 * @adev: amdgpu_device pointer
 * @vmid: vm instance to flush
 *
 * Flush the TLB for the requested page table.
 */
static void gmc_v10_0_flush_gpu_tlb(struct amdgpu_device *adev, uint32_t vmid,
					uint32_t vmhub, uint32_t flush_type)
{
	struct amdgpu_ring *ring = adev->mman.buffer_funcs_ring;
	struct dma_fence *fence;
	struct amdgpu_job *job;

	int r;

	/* flush hdp cache */
	adev->nbio.funcs->hdp_flush(adev, NULL);

	if (vmhub != AMDGPU_GFXHUB_0)
		return;

	mutex_lock(&adev->mman.gtt_window_lock);

	if (vmhub == AMDGPU_MMHUB_0) {
		gmc_v10_0_flush_vm_hub(adev, vmid, AMDGPU_MMHUB_0, 0);
		mutex_unlock(&adev->mman.gtt_window_lock);
		return;
	}

	WARN_ON(vmhub != AMDGPU_GFXHUB_0);

	if (!adev->mman.buffer_funcs_enabled ||
	    !adev->ib_pool_ready ||
	    amdgpu_in_reset(adev) ||
	    ring->sched.ready == false) {
		gmc_v10_0_flush_vm_hub(adev, vmid, AMDGPU_GFXHUB_0, 0);
		mutex_unlock(&adev->mman.gtt_window_lock);
		return;
	}

	/* The SDMA on Navi has a bug which can theoretically result in memory
	 * corruption if an invalidation happens at the same time as an VA
	 * translation. Avoid this by doing the invalidation from the SDMA
	 * itself.
	 */
	r = amdgpu_job_alloc_with_ib(adev, 16 * 4, AMDGPU_IB_POOL_IMMEDIATE,
				     &job);
	if (r)
		goto error_alloc;

	job->vm_pd_addr = adev->csm_gart_paddr;
	job->vm_needs_flush = true;
	job->ibs->ptr[job->ibs->length_dw++] = ring->funcs->nop;
	amdgpu_ring_pad_ib(ring, &job->ibs[0]);
	r = amdgpu_job_submit(job, &adev->mman.entity,
			      AMDGPU_FENCE_OWNER_UNDEFINED, &fence);
	if (r)
		goto error_submit;

	mutex_unlock(&adev->mman.gtt_window_lock);

	dma_fence_wait(fence, false);
	dma_fence_put(fence);

	return;

error_submit:
	amdgpu_job_free(job);

error_alloc:
	mutex_unlock(&adev->mman.gtt_window_lock);
	DRM_ERROR("Error flushing GPU TLB using the SDMA (%d)!\n", r);
}

/**
 * gmc_v10_0_flush_gpu_tlb_pasid - tlb flush via pasid
 *
 * @adev: amdgpu_device pointer
 * @pasid: pasid to be flush
 *
 * Flush the TLB for the requested pasid.
 */
static int gmc_v10_0_flush_gpu_tlb_pasid(struct amdgpu_device *adev,
					uint16_t pasid, uint32_t flush_type,
					bool all_hub)
{
	int vmid, i;
	signed long r;
	uint32_t seq;
	uint16_t queried_pasid;
	bool ret;
	struct amdgpu_ring *ring = &adev->gfx.kiq.ring;
	struct amdgpu_kiq *kiq = &adev->gfx.kiq;

	if (amdgpu_emu_mode == 0 && ring->sched.ready) {
		spin_lock(&adev->gfx.kiq.ring_lock);
		/* 2 dwords flush + 8 dwords fence */
		amdgpu_ring_alloc(ring, kiq->pmf->invalidate_tlbs_size + 8);
		kiq->pmf->kiq_invalidate_tlbs(ring,
					pasid, flush_type, all_hub);
		r = amdgpu_fence_emit_polling(ring, &seq, MAX_KIQ_REG_WAIT);
		if (r) {
			amdgpu_ring_undo(ring);
			spin_unlock(&adev->gfx.kiq.ring_lock);
			return -ETIME;
		}

		amdgpu_ring_commit(ring);
		spin_unlock(&adev->gfx.kiq.ring_lock);
		r = amdgpu_fence_wait_polling(ring, seq, adev->usec_timeout);
		if (r < 1) {
			dev_err(adev->dev, "wait for kiq fence error: %ld.\n", r);
			return -ETIME;
		}

		return 0;
	}

	for (vmid = 1; vmid < 16; vmid++) {

		ret = gmc_v10_0_get_atc_vmid_pasid_mapping_info(adev, vmid,
				&queried_pasid);
		if (ret	&& queried_pasid == pasid) {
			if (all_hub) {
				for (i = 0; i < adev->num_vmhubs; i++)
					gmc_v10_0_flush_gpu_tlb(adev, vmid,
							i, flush_type);
			} else {
				gmc_v10_0_flush_gpu_tlb(adev, vmid,
						AMDGPU_GFXHUB_0, flush_type);
			}
			break;
		}
	}

	return 0;
}

static uint64_t gmc_v10_0_emit_flush_gpu_tlb(struct amdgpu_ring *ring,
					     unsigned vmid, uint64_t pd_addr)
{
	bool use_semaphore = gmc_v10_0_use_invalidate_semaphore(ring->adev, ring->funcs->vmhub);
	struct amdgpu_vmhub *hub = &ring->adev->vmhub[ring->funcs->vmhub];
	uint32_t req;
	unsigned eng = ring->vm_inv_eng;

	if (ring->funcs->vmhub != AMDGPU_GFXHUB_0)
		return pd_addr;

	pd_addr &= ~AMDGPU_PTE_SYSTEM;

	req = hub->vmhub_funcs->get_invalidate_req(vmid, 0);
	/*
	 * It may lose gpuvm invalidate acknowldege state across power-gating
	 * off cycle, add semaphore acquire before invalidation and semaphore
	 * release after invalidation to avoid entering power gated state
	 * to WA the Issue
	 */

	/* TODO: It needs to continue working on debugging with semaphore for GFXHUB as well. */
	if (use_semaphore)
		/* a read return value of 1 means semaphore acuqire */
		amdgpu_ring_emit_reg_wait(ring,
					  hub->vm_inv_eng0_sem +
					  hub->eng_distance * eng, 0x1, 0x1);

	amdgpu_ring_emit_wreg(ring, hub->ctx0_ptb_addr_lo32 +
			      (hub->ctx_addr_distance * vmid),
			      lower_32_bits(pd_addr));

	amdgpu_ring_emit_wreg(ring, hub->ctx0_ptb_addr_hi32 +
			      (hub->ctx_addr_distance * vmid),
			      upper_32_bits(pd_addr));

	amdgpu_ring_emit_reg_write_reg_wait(ring, hub->vm_inv_eng0_req +
					    hub->eng_distance * eng,
					    hub->vm_inv_eng0_ack +
					    hub->eng_distance * eng,
					    req, 1 << vmid);

	/* TODO: It needs to continue working on debugging with semaphore for GFXHUB as well. */
	if (use_semaphore)
		/*
		 * add semaphore release after invalidation,
		 * write with 0 means semaphore release
		 */
		amdgpu_ring_emit_wreg(ring, hub->vm_inv_eng0_sem +
				      hub->eng_distance * eng, 0);

	return pd_addr;
}

static void gmc_v10_0_emit_pasid_mapping(struct amdgpu_ring *ring, unsigned vmid,
					 unsigned pasid)
{
	return;
}

static uint64_t gmc_v10_0_map_mtype(struct amdgpu_device *adev, uint32_t flags)
{
	switch (flags) {
	case AMDGPU_VM_MTYPE_DEFAULT:
		return AMDGPU_PTE_MTYPE_NV10(MTYPE_NC);
	case AMDGPU_VM_MTYPE_NC:
		return AMDGPU_PTE_MTYPE_NV10(MTYPE_NC);
	case AMDGPU_VM_MTYPE_WC:
		return AMDGPU_PTE_MTYPE_NV10(MTYPE_WC);
	case AMDGPU_VM_MTYPE_CC:
		return AMDGPU_PTE_MTYPE_NV10(MTYPE_CC);
	case AMDGPU_VM_MTYPE_UC:
		return AMDGPU_PTE_MTYPE_NV10(MTYPE_UC);
	default:
		return AMDGPU_PTE_MTYPE_NV10(MTYPE_NC);
	}
}

static void gmc_v10_0_get_vm_pde(struct amdgpu_device *adev, int level,
				 uint64_t *addr, uint64_t *flags)
{
	if (!(*flags & AMDGPU_PDE_PTE) && !(*flags & AMDGPU_PTE_SYSTEM))
		*addr = adev->vm_manager.vram_base_offset + *addr -
			adev->gmc.vram_start;

	if (!adev->gmc.translate_further) {
		if (level != AMDGPU_VM_PTB && !(*flags & AMDGPU_PDE_PTE))
			*flags &= ~AMDGPU_PTE_SYSTEM;

		return;
	}

	if (level == AMDGPU_VM_PDB1) {
		/* Set the block fragment size */
		if (!(*flags & AMDGPU_PDE_PTE))
			*flags |= AMDGPU_PDE_BFS(0x9);

	} else if (level == AMDGPU_VM_PDB0) {
		if (*flags & AMDGPU_PDE_PTE)
			*flags &= ~AMDGPU_PDE_PTE;
		else
			*flags |= AMDGPU_PTE_TF;
	}

	if (level != AMDGPU_VM_PTB && !(*flags & AMDGPU_PDE_PTE))
		*flags &= ~AMDGPU_PTE_SYSTEM;
}

static void gmc_v10_0_get_vm_pte(struct amdgpu_device *adev,
				 struct amdgpu_bo_va_mapping *mapping,
				 uint64_t *flags)
{
	*flags &= ~AMDGPU_PTE_EXECUTABLE;
	*flags |= mapping->flags & AMDGPU_PTE_EXECUTABLE;

	*flags &= ~AMDGPU_PTE_MTYPE_NV10_MASK;
	*flags |= (mapping->flags & AMDGPU_PTE_MTYPE_NV10_MASK);

	if (mapping->flags & AMDGPU_PTE_PRT) {
		*flags |= AMDGPU_PTE_PRT;
		*flags |= AMDGPU_PTE_SNOOPED;
		*flags |= AMDGPU_PTE_LOG;
		*flags |= AMDGPU_PTE_SYSTEM;
		*flags &= ~AMDGPU_PTE_VALID;
	}
}

static unsigned gmc_v10_0_get_vbios_fb_size(struct amdgpu_device *adev)
{
	u32 d1vga_control = RREG32_SOC15(DCE, 0, mmD1VGA_CONTROL);
	unsigned size;

	if (REG_GET_FIELD(d1vga_control, D1VGA_CONTROL, D1VGA_MODE_ENABLE)) {
		size = AMDGPU_VBIOS_VGA_ALLOCATION;
	} else {
		u32 viewport;
		u32 pitch;

		viewport = RREG32_SOC15(DCE, 0, mmHUBP0_DCSURF_PRI_VIEWPORT_DIMENSION);
		pitch = RREG32_SOC15(DCE, 0, mmHUBPREQ0_DCSURF_SURFACE_PITCH);
		size = (REG_GET_FIELD(viewport,
					HUBP0_DCSURF_PRI_VIEWPORT_DIMENSION, PRI_VIEWPORT_HEIGHT) *
				REG_GET_FIELD(pitch, HUBPREQ0_DCSURF_SURFACE_PITCH, PITCH) *
				4);
	}

	return size;
}

#if defined(CONFIG_DRM_AMDGPU_GMC_DUMP)
static u64 gmc_v10_0_get_fault_addr(struct amdgpu_device *adev,
				    const struct amdgpu_vmhub *hub)
{
	u64 addr;

	addr = ((u64)(RREG32(hub->vm_l2_pro_fault_addr_hi32) &
		GCVM_L2_PROTECTION_FAULT_ADDR_HI32__LOGICAL_PAGE_ADDR_HI4_MASK))
		 << 32;
	addr |= RREG32(hub->vm_l2_pro_fault_addr_lo32);
	addr <<= PAGE_SHIFT;

	return addr;
}

static size_t gmc_v10_0_get_walker_error(u32 fault_status,
					 char *buf, size_t len)
{
	u32 temp32;

	const char * const walker_error[] = {
		"",
		"Range",
		"PDE0",
		"PDE1",
		"PDE2",
		"Translate Further",
		"NACK",
		"Dummy Page",
	};

	temp32 = REG_GET_FIELD(fault_status, GCVM_L2_PROTECTION_FAULT_STATUS,
				WALKER_ERROR);

	return sgpu_dump_print(buf, len, "    Walker error = %s\n",
			walker_error[temp32]);
}

static size_t gmc_v10_0_get_permission_faults(u32 fault_status,
					      char *buf, size_t len)
{
	u32 temp32, j;
	char dump_string[64] = "";
	const char * const permission_faults[] = {
		"Valid",
		"Read",
		"Write",
		"Execute"
	};
	const unsigned int num_permission_faults =
		sizeof(permission_faults) / sizeof(const char *);

	temp32 = REG_GET_FIELD(fault_status, GCVM_L2_PROTECTION_FAULT_STATUS,
				PERMISSION_FAULTS);
	for (j = 0; j < num_permission_faults; j++) {
		if (temp32 & (1 << j))
			sgpu_dump_print(dump_string,
				 sizeof(dump_string), "%s %s,",
				 dump_string,
				 permission_faults[j]);
	}

	return sgpu_dump_print(buf, len, "    Permission faults =%s\n", dump_string);
}

static size_t gmc_v10_0_get_gmc_status(struct amdgpu_device *adev,
				       char *buf, size_t len)
{
	unsigned int i;
	const struct amdgpu_vmhub *hub;
	u32 fault_status, fault_info;
	u64 fault_addr;
	bool is_fault;
	const struct amd_ip_funcs *ip_funcs;
	bool is_gfxoff_on = false;
	size_t size = 0;

	for (i = 0; i < adev->num_ip_blocks; i++) {
		if (adev->ip_blocks[i].version->type != AMD_IP_BLOCK_TYPE_GFX)
			continue;
		ip_funcs = adev->ip_blocks[i].version->funcs;
		if (ip_funcs->is_power_on) {
			is_gfxoff_on = ip_funcs->is_power_on((void *)adev);
			break;
		}
	}

	for (i = 0; i <= AMDGPU_MMHUB_0; i++) {
		if (i == AMDGPU_GFXHUB_0 && !is_gfxoff_on) {
			is_fault = false;
		} else {
			hub = &adev->vmhub[i];
			fault_status = RREG32(hub->vm_l2_pro_fault_status);
			fault_addr = gmc_v10_0_get_fault_addr(adev, hub);

			is_fault = (fault_status || fault_addr);
		}

		if (i == AMDGPU_GFXHUB_0)
			size += sgpu_dump_print(buf + size, len - size,
					 "VM Protection Fault (GFX HUB): %s\n",
					 (is_fault ? "YES" : "NO"));
		else
			size += sgpu_dump_print(buf + size, len - size,
					 "VM Protection Fault (%s): %s\n",
					 ((i == AMDGPU_MMHUB_0) ? "MM HUB 0" :
					  "MM HUB 1"),
					 (is_fault ? "YES" : "NO"));

		if (is_fault) {
			size += sgpu_dump_print(buf + size, len - size,
					 "  Protection Fault Status = 0x%08x\n",
					 fault_status);
			fault_info = (fault_status &
				GCVM_L2_PROTECTION_FAULT_STATUS__VMID_MASK) >>
				GCVM_L2_PROTECTION_FAULT_STATUS__VMID__SHIFT;
			size += sgpu_dump_print(buf + size, len - size,
					 "    VMID = %d\n", fault_info);

			size += gmc_v10_0_get_walker_error(fault_status,
							   buf + size,
							   len - size);
			size += gmc_v10_0_get_permission_faults(fault_status,
								buf + size,
								len - size);

			fault_info = REG_GET_FIELD(fault_status, GCVM_L2_PROTECTION_FAULT_STATUS,
						   CID);
			size += sgpu_dump_print(buf + size, len - size,
					 "    Memory client id = %d\n",
					 fault_info);

			fault_info = REG_GET_FIELD(fault_status, GCVM_L2_PROTECTION_FAULT_STATUS,
						   RW);
			size += sgpu_dump_print(buf + size, len - size,
					 "    Memory client R/W = %s\n",
					 (fault_info ? "WRITE" : "READ"));

			size += sgpu_dump_print(buf + size, len - size,
					 "  Protection Fault GPU Address = 0x%016llx\n",
					 fault_addr);
			size += sgpu_dump_print(buf + size, len - size,
					 "TODO: Dump VMPT entries of fault address\n");
		}
	}

	return size;
}
#endif /* CONFIG_DRM_AMDGPU_GMC_DuMP */

void gmc_v10_0_cwsr_flush_gpu_tlb(u32 vmid, struct amdgpu_bo *root_bo,
				  struct amdgpu_ring *ring)
{
	int i;
	u32 tmp;
	u64 vm_pd_addr;

	struct amdgpu_device *adev = ring->adev;
	struct amdgpu_vmhub *hub = &adev->vmhub[ring->funcs->vmhub];
	u32 req = hub->vmhub_funcs->get_invalidate_req(vmid, 0);
	u32 eng = ring->vm_inv_eng;

	sgpu_ifpo_lock(adev);

	vm_pd_addr = amdgpu_gmc_pd_addr(root_bo);
	WREG32(hub->ctx0_ptb_addr_lo32 + (hub->ctx_addr_distance * vmid),
	       lower_32_bits(vm_pd_addr));

	WREG32(hub->ctx0_ptb_addr_hi32 + (hub->ctx_addr_distance * vmid),
	       upper_32_bits(vm_pd_addr));

	WREG32(hub->vm_inv_eng0_req + hub->eng_distance * eng, req);
	RREG32(hub->vm_inv_eng0_req + hub->eng_distance * eng);

	/* Wait for ACK with a delay.*/
	for (i = 0; i < adev->usec_timeout; i++) {
		tmp = RREG32(hub->vm_inv_eng0_ack + hub->eng_distance * eng);
		tmp &= 1 << vmid;
		if (tmp)
			break;

		if (!amdgpu_emu_mode)
			udelay(1);
		else
			mdelay(1);
	}

	sgpu_ifpo_unlock(adev);

	if (i == adev->usec_timeout)
		DRM_WARN("Timeout to flush(0x%x) cwsr VM(%d)\n", tmp, vmid);
}

static const struct amdgpu_gmc_funcs gmc_v10_0_gmc_funcs = {
	.flush_gpu_tlb = gmc_v10_0_flush_gpu_tlb,
	.flush_gpu_tlb_pasid = gmc_v10_0_flush_gpu_tlb_pasid,
	.emit_flush_gpu_tlb = gmc_v10_0_emit_flush_gpu_tlb,
	.emit_pasid_mapping = gmc_v10_0_emit_pasid_mapping,
	.map_mtype = gmc_v10_0_map_mtype,
	.get_vm_pde = gmc_v10_0_get_vm_pde,
	.get_vm_pte = gmc_v10_0_get_vm_pte,
	.get_vbios_fb_size = gmc_v10_0_get_vbios_fb_size,
#if defined(CONFIG_DRM_AMDGPU_GMC_DUMP)
	.get_gmc_status = gmc_v10_0_get_gmc_status,
#endif
	.cwsr_flush_gpu_tlb = gmc_v10_0_cwsr_flush_gpu_tlb,
};

static void gmc_v10_0_set_gmc_funcs(struct amdgpu_device *adev)
{
	if (adev->gmc.gmc_funcs == NULL)
		adev->gmc.gmc_funcs = &gmc_v10_0_gmc_funcs;
}

static void gmc_v10_0_set_mmhub_funcs(struct amdgpu_device *adev)
{
	adev->mmhub.funcs = &mmhub_v2_0_funcs;
}

static void gmc_v10_0_set_gfxhub_funcs(struct amdgpu_device *adev)
{
	adev->gfxhub.funcs = &gfxhub_v2_1_funcs;

	return;
}


static int gmc_v10_0_early_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	gmc_v10_0_set_mmhub_funcs(adev);
	gmc_v10_0_set_gfxhub_funcs(adev);
	gmc_v10_0_set_gmc_funcs(adev);
	gmc_v10_0_set_irq_funcs(adev);

	adev->gmc.shared_aperture_start = 0x2000000000000000ULL;
	adev->gmc.shared_aperture_end =
		adev->gmc.shared_aperture_start + (4ULL << 30) - 1;
	adev->gmc.private_aperture_start = 0x1000000000000000ULL;
	adev->gmc.private_aperture_end =
		adev->gmc.private_aperture_start + (4ULL << 30) - 1;

	return 0;
}

static int gmc_v10_0_late_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;
	int r;

	amdgpu_bo_late_init(adev);

	r = amdgpu_gmc_allocate_vm_inv_eng(adev);
	if (r)
		return r;

	r = amdgpu_irq_get(adev, &adev->gmc.vm_fault, 0);

	return r;
}

static void gmc_v10_0_vram_gtt_location(struct amdgpu_device *adev,
					struct amdgpu_gmc *mc)
{
	u64 base = 0;

	base = adev->gfxhub.funcs->get_fb_location(adev);

	amdgpu_gmc_vram_location(adev, &adev->gmc, base);
	amdgpu_gmc_gart_location(adev, mc);

	/* base offset of vram pages */
	adev->vm_manager.vram_base_offset = adev->gfxhub.funcs->get_mc_fb_offset(adev);
}

/**
 * gmc_v10_0_mc_init - initialize the memory controller driver params
 *
 * @adev: amdgpu_device pointer
 *
 * Look up the amount of vram, vram width, and decide how to place
 * vram and gart within the GPU's physical address space.
 * Returns 0 for success.
 */
static int gmc_v10_0_mc_init(struct amdgpu_device *adev)
{
	/* size in MB on si */
	adev->gmc.mc_vram_size =
		adev->nbio.funcs->get_memsize(adev) * 1024ULL * 1024ULL;
	adev->gmc.real_vram_size = adev->gmc.mc_vram_size;

	if (adev->pdev) {
		adev->gmc.aper_base = pci_resource_start(adev->pdev, 0);
		adev->gmc.aper_size = pci_resource_len(adev->pdev, 0);
	} else {
		adev->gmc.aper_base = 0;
		adev->gmc.aper_size = 0;
	}

	/* In case the PCI BAR is larger than the actual amount of vram */
	adev->gmc.visible_vram_size = adev->gmc.aper_size;
	if (adev->gmc.visible_vram_size > adev->gmc.real_vram_size)
		adev->gmc.visible_vram_size = adev->gmc.real_vram_size;

	/* set the gart size */
	if (amdgpu_gart_size == -1) {
		adev->gmc.gart_size = 512ULL << 20;
	} else
		adev->gmc.gart_size = (u64)amdgpu_gart_size << 20;

	gmc_v10_0_vram_gtt_location(adev, &adev->gmc);

	return 0;
}

static int gmc_v10_0_gart_init(struct amdgpu_device *adev)
{
	int r;

	if (adev->gart.bo||adev->csm_gart_vaddr) {
		WARN(1, "GART already initialized\n");
		return 0;
	}

	/* Initialize common gart structure */
	r = amdgpu_gart_init(adev);
	if (r)
		return r;

	adev->gart.table_size = adev->gart.num_gpu_pages * 8;
	adev->gart.gart_pte_flags = AMDGPU_PTE_MTYPE_NV10(MTYPE_UC) |
				 AMDGPU_PTE_EXECUTABLE;

	return amdgpu_gart_table_sysram_alloc(adev);
}

static int gmc_v10_0_sw_init(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	adev->gfxhub.funcs->init(adev);

	adev->mmhub.funcs->init(adev);

	spin_lock_init(&adev->gmc.invalidate_lock);

	if (amdgpu_emu_mode == 1) {
		adev->gmc.vram_type = AMDGPU_VRAM_TYPE_GDDR6;
		adev->gmc.vram_width = 1 * 128; /* numchan * chansize */
	}

	adev->num_vmhubs = 1;
	/*
	 * To fulfill 4-level page support,
	 * vm size is 256TB (48bit), maximum size of Navi10/Navi14/Navi12,
	 * block size 512 (9bit)
	 */
	amdgpu_vm_adjust_size(adev, 256 * 1024, 9, 3, 48);

	r = amdgpu_irq_add_id(adev, SOC15_IH_CLIENTID_UTCL2,
			      UTCL2_1_0__SRCID__FAULT,
			      &adev->gmc.vm_fault);
	if (r)
		return r;

	if (!amdgpu_sriov_vf(adev)) {
		/* interrupt sent to DF. */
		r = amdgpu_irq_add_id(adev, SOC15_IH_CLIENTID_DF, 0,
				      &adev->gmc.ecc_irq);
		if (r)
			return r;
	}

	/*
	 * Set the internal MC address mask This is the max address of the GPU's
	 * internal address space.
	 */
	adev->gmc.mc_mask = 0xffffffffffffULL; /* 48 bit MC */

	r = dma_set_mask_and_coherent(adev->dev, DMA_BIT_MASK(44));
	if (r) {
		printk(KERN_WARNING "amdgpu: No suitable DMA available.\n");
		return r;
	}

	r = gmc_v10_0_mc_init(adev);
	if (r)
		return r;

	/* Memory manager */
	r = amdgpu_bo_init(adev);
	if (r)
		return r;

	r = gmc_v10_0_gart_init(adev);
	if (r)
		return r;

	/*
	 * number of VMs
	 * VMID 0 is reserved for System
	 * amdgpu graphics/compute will use VMIDs 1-7
	 * amdkfd will use VMIDs 8-15
	 */
	adev->vm_manager.first_kfd_vmid = 8;

	amdgpu_vm_manager_init(adev);

	return 0;
}

/**
 * gmc_v8_0_gart_fini - vm fini callback
 *
 * @adev: amdgpu_device pointer
 *
 * Tears down the driver GART/VM setup (CIK).
 */
static void gmc_v10_0_gart_fini(struct amdgpu_device *adev)
{
	amdgpu_gart_table_sysram_free(adev);

	amdgpu_gart_fini(adev);
}

static int gmc_v10_0_sw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	amdgpu_vm_manager_fini(adev);
	gmc_v10_0_gart_fini(adev);
	amdgpu_gem_force_release(adev);
	amdgpu_bo_fini(adev);

	return 0;
}

static void gmc_v10_0_init_golden_registers(struct amdgpu_device *adev)
{
	return;
}

/**
 * gmc_v10_0_gart_enable - gart enable
 *
 * @adev: amdgpu_device pointer
 */
static int gmc_v10_0_gart_enable(struct amdgpu_device *adev)
{
	int r;
	bool value;

	amdgpu_gtt_mgr_recover(&adev->mman.gtt_mgr);
	r = adev->gfxhub.funcs->gart_enable(adev);
	if (r)
		return r;

	r = adev->mmhub.funcs->gart_enable(adev);
	if (r)
		return r;

	value = (amdgpu_vm_fault_stop == AMDGPU_VM_FAULT_STOP_ALWAYS) ?
		false : true;

	adev->gfxhub.funcs->set_fault_enable_default(adev, value);
	adev->mmhub.funcs->set_fault_enable_default(adev, value);
	gmc_v10_0_flush_gpu_tlb(adev, 0, AMDGPU_MMHUB_0, 0);
	gmc_v10_0_flush_gpu_tlb(adev, 0, AMDGPU_GFXHUB_0, 0);

	DRM_DEBUG("PCIE GART of %uM enabled (table at 0x%016llX).\n",
		  (u32)(adev->gmc.gart_size >> 20),
		  adev->csm_gart_paddr);

	adev->gart.ready = true;

	return 0;
}

static int gmc_v10_0_hw_init(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	/* The sequence of these two function calls matters.*/
	gmc_v10_0_init_golden_registers(adev);

	r = gmc_v10_0_gart_enable(adev);
	if (r)
		return r;

	return 0;
}

/**
 * gmc_v10_0_gart_disable - gart disable
 *
 * @adev: amdgpu_device pointer
 *
 * This disables all VM page table.
 */
static void gmc_v10_0_gart_disable(struct amdgpu_device *adev)
{
	adev->gfxhub.funcs->gart_disable(adev);
	adev->mmhub.funcs->gart_disable(adev);
}

static int gmc_v10_0_hw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	if (amdgpu_sriov_vf(adev)) {
		/* full access mode, so don't touch any GMC register */
		DRM_DEBUG("For SRIOV client, shouldn't do anything.\n");
		return 0;
	}

	amdgpu_irq_put(adev, &adev->gmc.vm_fault, 0);
	gmc_v10_0_gart_disable(adev);

	return 0;
}

static int gmc_v10_0_suspend(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	gmc_v10_0_hw_fini(adev);

	return 0;
}

static int gmc_v10_0_resume(void *handle)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = gmc_v10_0_hw_init(adev);
	if (r)
		return r;

	amdgpu_vmid_reset_all(adev);

	return 0;
}

static bool gmc_v10_0_is_idle(void *handle)
{
	/* MC is always ready in GMC v10.*/
	return true;
}

static int gmc_v10_0_wait_for_idle(void *handle)
{
	/* There is no need to wait for MC idle in GMC v10.*/
	return 0;
}

static int gmc_v10_0_soft_reset(void *handle)
{
	return 0;
}

static int gmc_v10_0_set_clockgating_state(void *handle,
					   enum amd_clockgating_state state)
{
	int r;
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	r = adev->mmhub.funcs->set_clockgating(adev, state);
	if (r)
		return r;

	return athub_v2_0_set_clockgating(adev, state);
}

static void gmc_v10_0_get_clockgating_state(void *handle, u64 *flags)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	adev->mmhub.funcs->get_clockgating(adev, flags);

	athub_v2_0_get_clockgating(adev, flags);
}

static int gmc_v10_0_set_powergating_state(void *handle,
					   enum amd_powergating_state state)
{
	return 0;
}

const struct amd_ip_funcs gmc_v10_0_ip_funcs = {
	.name = "gmc_v10_0",
	.early_init = gmc_v10_0_early_init,
	.late_init = gmc_v10_0_late_init,
	.sw_init = gmc_v10_0_sw_init,
	.sw_fini = gmc_v10_0_sw_fini,
	.hw_init = gmc_v10_0_hw_init,
	.hw_fini = gmc_v10_0_hw_fini,
	.suspend = gmc_v10_0_suspend,
	.resume = gmc_v10_0_resume,
	.is_idle = gmc_v10_0_is_idle,
	.wait_for_idle = gmc_v10_0_wait_for_idle,
	.soft_reset = gmc_v10_0_soft_reset,
	.set_clockgating_state = gmc_v10_0_set_clockgating_state,
	.set_powergating_state = gmc_v10_0_set_powergating_state,
	.get_clockgating_state = gmc_v10_0_get_clockgating_state,
};

const struct amdgpu_ip_block_version gmc_v10_0_ip_block =
{
	.type = AMD_IP_BLOCK_TYPE_GMC,
	.major = 10,
	.minor = 0,
	.rev = 0,
	.funcs = &gmc_v10_0_ip_funcs,
};
