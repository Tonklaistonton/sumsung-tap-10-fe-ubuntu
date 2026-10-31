/*
 * Copyright 2019 - 2020 Advanced Micro Devices, Inc.
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

#include "amdgpu.h"
#include "mmhub_v2_0.h"

#include "mmhub/mmhub_2_0_0_offset.h"
#include "mmhub/mmhub_2_0_0_sh_mask.h"
#include "mmhub/mmhub_2_0_0_default.h"

#define mmMM_ATC_L2_MISC_CG_Sienna_Cichlid                      0x064d
#define mmMM_ATC_L2_MISC_CG_Sienna_Cichlid_BASE_IDX             0
#define mmDAGB0_CNTL_MISC2_Sienna_Cichlid                       0x0070
#define mmDAGB0_CNTL_MISC2_Sienna_Cichlid_BASE_IDX              0

static uint32_t mmhub_v2_0_get_invalidate_req(unsigned int vmid,
					      uint32_t flush_type)
{
	u32 req = 0;

	/* invalidate using legacy mode on vmid*/
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ,
			    PER_VMID_INVALIDATE_REQ, 1 << vmid);
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ, FLUSH_TYPE, flush_type);
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ, INVALIDATE_L2_PTES, 1);
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ, INVALIDATE_L2_PDE0, 1);
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ, INVALIDATE_L2_PDE1, 1);
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ, INVALIDATE_L2_PDE2, 1);
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ, INVALIDATE_L1_PTES, 1);
	req = REG_SET_FIELD(req, MMVM_INVALIDATE_ENG0_REQ,
			    CLEAR_PROTECTION_FAULT_STATUS_ADDR,	0);

	return req;
}

static void
mmhub_v2_0_print_l2_protection_fault_status(struct amdgpu_device *adev,
					     uint32_t status)
{
	uint32_t cid, rw;

	cid = REG_GET_FIELD(status,
			    MMVM_L2_PROTECTION_FAULT_STATUS, CID);
	rw = REG_GET_FIELD(status,
			   MMVM_L2_PROTECTION_FAULT_STATUS, RW);

	dev_err(adev->dev,
		"MMVM_L2_PROTECTION_FAULT_STATUS:0x%08X\n",
		status);
	dev_err(adev->dev, "\t Faulty UTCL2 client ID: %s (0x%x)\n",
		"unknown", cid);
	dev_err(adev->dev, "\t MORE_FAULTS: 0x%lx\n",
		REG_GET_FIELD(status,
		MMVM_L2_PROTECTION_FAULT_STATUS, MORE_FAULTS));
	dev_err(adev->dev, "\t WALKER_ERROR: 0x%lx\n",
		REG_GET_FIELD(status,
		MMVM_L2_PROTECTION_FAULT_STATUS, WALKER_ERROR));
	dev_err(adev->dev, "\t PERMISSION_FAULTS: 0x%lx\n",
		REG_GET_FIELD(status,
		MMVM_L2_PROTECTION_FAULT_STATUS, PERMISSION_FAULTS));
	dev_err(adev->dev, "\t MAPPING_ERROR: 0x%lx\n",
		REG_GET_FIELD(status,
		MMVM_L2_PROTECTION_FAULT_STATUS, MAPPING_ERROR));
	dev_err(adev->dev, "\t RW: 0x%x\n", rw);
}

static void mmhub_v2_0_setup_vm_pt_regs(struct amdgpu_device *adev, uint32_t vmid,
				uint64_t page_table_base)
{
	return;
}

static int mmhub_v2_0_gart_enable(struct amdgpu_device *adev)
{
	return 0;
}

static void mmhub_v2_0_gart_disable(struct amdgpu_device *adev)
{
	return;
}

/**
 * mmhub_v2_0_set_fault_enable_default - update GART/VM fault handling
 *
 * @adev: amdgpu_device pointer
 * @value: true redirects VM faults to the default page
 */
static void mmhub_v2_0_set_fault_enable_default(struct amdgpu_device *adev, bool value)
{
	return;
}

static const struct amdgpu_vmhub_funcs mmhub_v2_0_vmhub_funcs = {
	.print_l2_protection_fault_status = mmhub_v2_0_print_l2_protection_fault_status,
	.get_invalidate_req = mmhub_v2_0_get_invalidate_req,
};

static void mmhub_v2_0_init(struct amdgpu_device *adev)
{
	return;
}

static int mmhub_v2_0_set_clockgating(struct amdgpu_device *adev,
			       enum amd_clockgating_state state)
{
	return 0;
}

static void mmhub_v2_0_get_clockgating(struct amdgpu_device *adev, u64 *flags)
{
	return;
}

const struct amdgpu_mmhub_funcs mmhub_v2_0_funcs = {
	.init = mmhub_v2_0_init,
	.gart_enable = mmhub_v2_0_gart_enable,
	.set_fault_enable_default = mmhub_v2_0_set_fault_enable_default,
	.gart_disable = mmhub_v2_0_gart_disable,
	.set_clockgating = mmhub_v2_0_set_clockgating,
	.get_clockgating = mmhub_v2_0_get_clockgating,
	.setup_vm_pt_regs = mmhub_v2_0_setup_vm_pt_regs,
};
