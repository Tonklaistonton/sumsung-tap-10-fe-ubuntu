/*
 * Copyright 2019-2023 Advanced Micro Devices, Inc.
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
#include <linux/bitfield.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "amdgpu.h"
#include "amdgpu_ih.h"
#include "amdgpu_ucode.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "hdp/hdp_5_0_0_offset.h"
#include "hdp/hdp_5_0_0_sh_mask.h"
#include "smuio/smuio_11_0_0_offset.h"
#include "mp/mp_11_0_offset.h"

#include "soc15.h"
#include "soc15_common.h"
#include "gmc_v10_0.h"
#include "nbio_v2_3.h"
#include "nv.h"
#include "gfx_v10_0.h"
#include "vangogh_lite_ih.h"

#include "sgpu.h"

static const struct amd_ip_funcs nv_common_ip_funcs;

static u32 nv_didt_rreg(struct amdgpu_device *adev, u32 reg)
{
	unsigned long flags, address, data;
	u32 r;

	address = SOC15_REG_OFFSET(GC, 0, mmDIDT_IND_INDEX);
	data = SOC15_REG_OFFSET(GC, 0, mmDIDT_IND_DATA);

	spin_lock_irqsave(&adev->didt_idx_lock, flags);
	WREG32(address, (reg));
	r = RREG32(data);
	spin_unlock_irqrestore(&adev->didt_idx_lock, flags);
	return r;
}

static void nv_didt_wreg(struct amdgpu_device *adev, u32 reg, u32 v)
{
	unsigned long flags, address, data;

	address = SOC15_REG_OFFSET(GC, 0, mmDIDT_IND_INDEX);
	data = SOC15_REG_OFFSET(GC, 0, mmDIDT_IND_DATA);

	spin_lock_irqsave(&adev->didt_idx_lock, flags);
	WREG32(address, (reg));
	WREG32(data, (v));
	spin_unlock_irqrestore(&adev->didt_idx_lock, flags);
}

static u32 nv_get_config_memsize(struct amdgpu_device *adev)
{
	return adev->nbio.funcs->get_memsize(adev);
}

static u32 nv_get_xclk(struct amdgpu_device *adev)
{
	/* On Mariner EMU, there is no SMU and VBIOS, so we need to
	 * set hardcocde for xclk.
	 * The value confirmed by HW team is 25.6M
	 */
	return 2560;
}


void nv_grbm_select(struct amdgpu_device *adev,
		     u32 me, u32 pipe, u32 queue, u32 vmid)
{
	u32 grbm_gfx_cntl = 0;
	grbm_gfx_cntl = REG_SET_FIELD(grbm_gfx_cntl, GRBM_GFX_CNTL, PIPEID, pipe);
	grbm_gfx_cntl = REG_SET_FIELD(grbm_gfx_cntl, GRBM_GFX_CNTL, MEID, me);
	grbm_gfx_cntl = REG_SET_FIELD(grbm_gfx_cntl, GRBM_GFX_CNTL, VMID, vmid);
	grbm_gfx_cntl = REG_SET_FIELD(grbm_gfx_cntl, GRBM_GFX_CNTL, QUEUEID, queue);

	WREG32(SOC15_REG_OFFSET(GC, 0, mmGRBM_GFX_CNTL), grbm_gfx_cntl);
}

static void nv_vga_set_state(struct amdgpu_device *adev, bool state)
{
	/* todo */
}

static bool nv_read_disabled_bios(struct amdgpu_device *adev)
{
	/* todo */
	return false;
}

static bool nv_read_bios_from_rom(struct amdgpu_device *adev,
				  u8 *bios, u32 length_bytes)
{
	u32 *dw_ptr;
	u32 i, length_dw;

	if (bios == NULL)
		return false;
	if (length_bytes == 0)
		return false;

	dw_ptr = (u32 *)bios;
	length_dw = ALIGN(length_bytes, 4) / 4;

	/* set rom index to 0 */
	WREG32(SOC15_REG_OFFSET(SMUIO, 0, mmROM_INDEX), 0);
	/* read out the rom data */
	for (i = 0; i < length_dw; i++)
		dw_ptr[i] = RREG32(SOC15_REG_OFFSET(SMUIO, 0, mmROM_DATA));

	return true;
}

static struct soc15_allowed_register_entry nv_allowed_read_registers[] = {
	{ SOC15_REG_ENTRY(GC, 0, mmGRBM_STATUS)},
	{ SOC15_REG_ENTRY(GC, 0, mmGRBM_STATUS2)},
	{ SOC15_REG_ENTRY(GC, 0, mmGRBM_STATUS_SE0)},
#if defined(CONFIG_GPU_VERSION_M3) || defined(CONFIG_DRM_SGPU_UNKNOWN_REGISTERS_ENABLE)
	{ SOC15_REG_ENTRY(GC, 0, mmGRBM_STATUS_SE1)},
#endif
#ifdef CONFIG_DRM_SGPU_UNKNOWN_REGISTERS_ENABLE
	{ SOC15_REG_ENTRY(GC, 0, mmGRBM_STATUS_SE2)},
	{ SOC15_REG_ENTRY(GC, 0, mmGRBM_STATUS_SE3)},
	{ SOC15_REG_ENTRY(SDMA0, 0, mmSDMA0_STATUS_REG)},
	{ SOC15_REG_ENTRY(SDMA1, 0, mmSDMA1_STATUS_REG)},
#endif
	{ SOC15_REG_ENTRY(GC, 0, mmCP_STAT)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_STALLED_STAT1)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_STALLED_STAT2)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_STALLED_STAT3)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_CPF_BUSY_STAT)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_CPF_STALLED_STAT1)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_CPF_STATUS)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_CPC_BUSY_STAT)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_CPC_STALLED_STAT1)},
	{ SOC15_REG_ENTRY(GC, 0, mmCP_CPC_STATUS)},
	{ SOC15_REG_ENTRY(GC, 0, mmGB_ADDR_CONFIG)},
};

static uint32_t nv_read_indexed_register(struct amdgpu_device *adev, u32 se_num,
					 u32 sh_num, u32 reg_offset)
{
	uint32_t val;

	mutex_lock(&adev->grbm_idx_mutex);
	if (se_num != 0xffffffff || sh_num != 0xffffffff)
		amdgpu_gfx_select_se_sh(adev, se_num, sh_num, 0xffffffff);

	val = RREG32(reg_offset);

	if (se_num != 0xffffffff || sh_num != 0xffffffff)
		amdgpu_gfx_select_se_sh(adev, 0xffffffff, 0xffffffff, 0xffffffff);
	mutex_unlock(&adev->grbm_idx_mutex);
	return val;
}

static uint32_t nv_get_register_value(struct amdgpu_device *adev,
				      bool indexed, u32 se_num,
				      u32 sh_num, u32 reg_offset)
{
	if (indexed) {
		return nv_read_indexed_register(adev, se_num, sh_num, reg_offset);
	} else {
		if (reg_offset == SOC15_REG_OFFSET(GC, 0, mmGB_ADDR_CONFIG))
			return adev->gfx.config.gb_addr_config;
		return RREG32(reg_offset);
	}
}

static int nv_read_register(struct amdgpu_device *adev, u32 se_num,
			    u32 sh_num, u32 reg_offset, u32 *value)
{
	uint32_t i;
	struct soc15_allowed_register_entry  *en;

	*value = 0;
	for (i = 0; i < ARRAY_SIZE(nv_allowed_read_registers); i++) {
		en = &nv_allowed_read_registers[i];
		if (en->hwip != GC_HWIP)
			continue;

		if (reg_offset !=
		    (adev->reg_offset[en->hwip][en->inst][en->seg] + en->reg_offset))
			continue;

		*value = nv_get_register_value(adev,
					       nv_allowed_read_registers[i].grbm_indexed,
					       se_num, sh_num, reg_offset);
		return 0;
	}
	return -EINVAL;
}

static int nv_asic_mode1_reset(struct amdgpu_device *adev)
{
	u32 i;
	int ret = 0;

	/* disable BM */
	pci_clear_master(adev->pdev);

	amdgpu_device_cache_pci_state(adev->pdev);

	if (ret)
		dev_err(adev->dev, "GPU mode1 reset failed\n");
	amdgpu_device_load_pci_state(adev->pdev);

	/* wait for asic to come out of reset */
	for (i = 0; i < adev->usec_timeout; i++) {
		u32 memsize = adev->nbio.funcs->get_memsize(adev);

		if (memsize != 0xffffffff)
			break;
		udelay(1);
	}

	return ret;
}

static bool nv_asic_supports_baco(struct amdgpu_device *adev)
{
	return false;
}

static enum amd_reset_method
nv_asic_reset_method(struct amdgpu_device *adev)
{
	return AMD_RESET_METHOD_MARINER_GOPHER;
}

static int nv_asic_reset(struct amdgpu_device *adev)
{
	int ret = 0;
	enum amd_reset_method reset = nv_asic_reset_method(adev);

	if (reset == AMD_RESET_METHOD_MARINER_GOPHER) {
		/* Here is where we initiate the IFPO reset.  Currently
		 * on Gopher, we run a script externally.  The
		 * loop below is to give enough time to run the hard
		 * reset script. */
		return 0;
	} else {
		dev_info(adev->dev, "MODE1 reset\n");
		ret = nv_asic_mode1_reset(adev);
	}

	return ret;
}

/*
 * navi1x_soft_reset_SQG_workaround
 *
 * Description:  The SQG block maintains a copy of
 *    CC_GC_SHADER_ARRAY_CONFIG and
 *    GC_USER_SHADER_ARRAY_CONFIG registers.
 *  These registers inform SQG which workgroup are
 *  active and which are inactive.  After soft reset,
 *  these registers are reset to default values which
 *  is all workgroup active.  This should not happen
 *  according to design rules.  As a result, it can
 *  cause a hang due to Xnack Override exit logic
 *  waiting for response from all active workgroups.
 *  But inactive workgroups do not response and so
 *  the wait will be forever.
 * Workaround: Re-write these registers so the SQG
 *  will pick up the values again.
 */
void navi1x_soft_reset_SQG_workaround(struct amdgpu_device *adev)
{
	return;
}

static void nv_program_aspm(struct amdgpu_device *adev)
{

	if (amdgpu_aspm == 0)
		return;

	/* todo */
}

static void nv_enable_doorbell_aperture(struct amdgpu_device *adev,
					bool enable)
{
	adev->nbio.funcs->enable_doorbell_aperture(adev, enable);
	adev->nbio.funcs->enable_doorbell_selfring_aperture(adev, enable);
}

static const struct amdgpu_ip_block_version nv_common_ip_block =
{
	.type = AMD_IP_BLOCK_TYPE_COMMON,
	.major = 1,
	.minor = 0,
	.rev = 0,
	.funcs = &nv_common_ip_funcs,
};

static int nv_reg_base_init(struct amdgpu_device *adev)
{
	int r;

	if (amdgpu_discovery) {
		r = amdgpu_discovery_reg_base_init(adev);
		if (r) {
			DRM_WARN("failed to init reg base from ip discovery table, "
					"fallback to legacy init method\n");
			goto legacy_init;
		}

		return 0;
	}

legacy_init:
	vangogh_lite_reg_base_init(adev);

	return 0;
}

void nv_set_virt_ops(struct amdgpu_device *adev)
{
}

int nv_set_ip_blocks(struct amdgpu_device *adev)
{
	int r;

	adev->nbio.funcs = &nbio_v2_3_funcs;
	adev->nbio.hdp_flush_reg = &nbio_v2_3_hdp_flush_reg;

	/* Set IP register base before any HW register access */
	r = nv_reg_base_init(adev);
	if (r)
		return r;

	amdgpu_device_ip_block_add(adev, &nv_common_ip_block);
	amdgpu_device_ip_block_add(adev, &gmc_v10_0_ip_block);
	if (!amdgpu_poll_eop)
		amdgpu_device_ip_block_add(adev, &vangogh_lite_ih_ip_block);
	amdgpu_device_ip_block_add(adev, &gfx_v10_0_ip_block);

	return 0;
}

static uint32_t nv_get_rev_id(struct amdgpu_device *adev)
{
	return adev->nbio.funcs->get_chip_revision(adev);
}

static uint32_t nv_get_chip_revision(struct amdgpu_device *adev)
{
	return adev->nbio.funcs->get_chip_revision(adev);
}

static void nv_flush_hdp(struct amdgpu_device *adev, struct amdgpu_ring *ring)
{
	adev->nbio.funcs->hdp_flush(adev, ring);
}

static void nv_invalidate_hdp(struct amdgpu_device *adev,
				struct amdgpu_ring *ring)
{
	return;
}

static bool nv_need_full_reset(struct amdgpu_device *adev)
{
	return false;
}

static bool nv_need_reset_on_init(struct amdgpu_device *adev)
{
	u32 sol_reg;

	/* Check sOS sign of life register to confirm sys driver and sOS
	 * are already been loaded.
	 */
	sol_reg = RREG32_SOC15(MP0, 0, mmMP0_SMN_C2PMSG_81);
	if (sol_reg)
		return true;

	return false;
}

static void nv_init_doorbell_index(struct amdgpu_device *adev)
{
	adev->doorbell_index.kiq = AMDGPU_NAVI10_DOORBELL_KIQ;
	adev->doorbell_index.mec_ring0 = AMDGPU_NAVI10_DOORBELL_MEC_RING0;
	adev->doorbell_index.mec_ring1 = AMDGPU_NAVI10_DOORBELL_MEC_RING1;
	adev->doorbell_index.mec_ring2 = AMDGPU_NAVI10_DOORBELL_MEC_RING2;
	adev->doorbell_index.mec_ring3 = AMDGPU_NAVI10_DOORBELL_MEC_RING3;
	adev->doorbell_index.mec_ring4 = AMDGPU_NAVI10_DOORBELL_MEC_RING4;
	adev->doorbell_index.mec_ring5 = AMDGPU_NAVI10_DOORBELL_MEC_RING5;
	adev->doorbell_index.mec_ring6 = AMDGPU_NAVI10_DOORBELL_MEC_RING6;
	adev->doorbell_index.mec_ring7 = AMDGPU_NAVI10_DOORBELL_MEC_RING7;
	adev->doorbell_index.userqueue_start = AMDGPU_NAVI10_DOORBELL_USERQUEUE_START;
	adev->doorbell_index.userqueue_end = AMDGPU_NAVI10_DOORBELL_USERQUEUE_END;
	adev->doorbell_index.gfx_ring0 = AMDGPU_NAVI10_DOORBELL_GFX_RING0;
	adev->doorbell_index.gfx_ring1 = AMDGPU_NAVI10_DOORBELL_GFX_RING1;
	adev->doorbell_index.gfx_ring2 = AMDGPU_NAVI10_DOORBELL_GFX_RING2;
	adev->doorbell_index.gfx_ring3 = AMDGPU_NAVI10_DOORBELL_GFX_RING3;
	adev->doorbell_index.mes_ring = AMDGPU_NAVI10_DOORBELL_MES_RING;
	adev->doorbell_index.sdma_engine[0] = AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE0;
	adev->doorbell_index.sdma_engine[1] = AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE1;
	adev->doorbell_index.sdma_engine[2] = AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE2;
	adev->doorbell_index.sdma_engine[3] = AMDGPU_NAVI10_DOORBELL_sDMA_ENGINE3;
	adev->doorbell_index.ih = AMDGPU_NAVI10_DOORBELL_IH;
	adev->doorbell_index.vcn.vcn_ring0_1 = AMDGPU_NAVI10_DOORBELL64_VCN0_1;
	adev->doorbell_index.vcn.vcn_ring2_3 = AMDGPU_NAVI10_DOORBELL64_VCN2_3;
	adev->doorbell_index.vcn.vcn_ring4_5 = AMDGPU_NAVI10_DOORBELL64_VCN4_5;
	adev->doorbell_index.vcn.vcn_ring6_7 = AMDGPU_NAVI10_DOORBELL64_VCN6_7;
	adev->doorbell_index.first_non_cp = AMDGPU_NAVI10_DOORBELL64_FIRST_NON_CP;
	adev->doorbell_index.last_non_cp = AMDGPU_NAVI10_DOORBELL64_LAST_NON_CP;

	adev->doorbell_index.first_resv = AMDGPU_NAVI10_DOORBELL64_FIRST_RESV;
	adev->doorbell_index.last_resv = AMDGPU_NAVI10_DOORBELL64_LAST_RESV;

	adev->doorbell_index.max_assignment = AMDGPU_NAVI10_DOORBELL_MAX_ASSIGNMENT << 1;
	adev->doorbell_index.sdma_doorbell_range = 20;
}

static void nv_pre_asic_init(struct amdgpu_device *adev)
{
}

#if defined(CONFIG_DRM_AMDGPU_DUMP)
static size_t nv_get_asic_status(struct amdgpu_device *adev,
				 char *buf, size_t len)
{
	unsigned int i;
	struct amdgpu_ring *ring;
	size_t size;

	size = sgpu_dump_print(buf, len, "******DUMP GPU STATUS START******\n");

	if (adev->gmc.gmc_funcs->get_gmc_status)
		size += adev->gmc.gmc_funcs->get_gmc_status(adev,
							    buf + size,
							    len  - size);

	for (i = 0; i < adev->num_rings; i++) {
		ring = adev->rings[i];
		if (ring->funcs->get_ring_status)
			size += ring->funcs->get_ring_status(ring,
							     buf + size,
							     len - size);
	}

	size += sgpu_dump_print(buf + size, len - size,
			 "******DUMP GPU STATUS_END******\n");

	return size;
}
#endif /* cONFIG_DRM_AMDGPU_DUMP */

static const struct amdgpu_asic_funcs nv_asic_funcs =
{
	.read_disabled_bios = &nv_read_disabled_bios,
	.read_bios_from_rom = &nv_read_bios_from_rom,
	.read_register = &nv_read_register,
	.reset = &nv_asic_reset,
	.reset_method = &nv_asic_reset_method,
	.set_vga_state = &nv_vga_set_state,
	.get_xclk = &nv_get_xclk,
	.get_config_memsize = &nv_get_config_memsize,
	.flush_hdp = &nv_flush_hdp,
	.invalidate_hdp = &nv_invalidate_hdp,
	.init_doorbell_index = &nv_init_doorbell_index,
	.need_full_reset = &nv_need_full_reset,
	.need_reset_on_init = &nv_need_reset_on_init,
	.supports_baco = &nv_asic_supports_baco,
	.pre_asic_init = &nv_pre_asic_init,
#if defined(CONFIG_DRM_AMDGPU_DUMP)
	.get_asic_status = &nv_get_asic_status,
#endif
};

static void amdgpu_get_grbm_chip_rev(struct amdgpu_device *adev)
{
	uint32_t rev;

	/**
	 * After get the GRBM_CHIP_REVISION for Mariner, some fields will be
	 * used to check if this M0, M1, EVT0 and EVT1
	 */
	dev_info(adev->dev, "GRBM_CHIP_REVISION is 0x%08x\n",
		 adev->grbm_chip_rev);

	rev = nv_get_chip_revision(adev);
	if(adev->grbm_chip_rev != rev)
		dev_warn(adev->dev, "GRBM_CHIP_REVISION doesn't match the value "
			"in a device tree.. 0x%x\n", rev);

	if (MGFX_EVT(rev) < 0 || MGFX_EVT(rev) > 2)
		DRM_WARN("EVT should be 0, 1 or 2");

	adev->grbm_chip_rev = rev;

	/**
	 * Warn if there is a mismatch between CONFIG_GPU_VERSION_MX and
	 * current chip revision
	 */
	WARN_ON((AMDGPU_IS_MGFX0(adev->grbm_chip_rev) && !IS_ENABLED(CONFIG_GPU_VERSION_M0))
		|| (AMDGPU_IS_MGFX1(adev->grbm_chip_rev) && !IS_ENABLED(CONFIG_GPU_VERSION_M1))
		|| (AMDGPU_IS_MGFX2(adev->grbm_chip_rev) && !IS_ENABLED(CONFIG_GPU_VERSION_M2))
		|| (AMDGPU_IS_MGFX2_MID(adev->grbm_chip_rev) && !IS_ENABLED(CONFIG_GPU_VERSION_M2_MID))
		|| (AMDGPU_IS_MGFX3(adev->grbm_chip_rev) && !IS_ENABLED(CONFIG_GPU_VERSION_M3))
		|| (MGFX_GEN(adev->grbm_chip_rev) > 3));
}

static int nv_common_early_init(void *handle)
{
#define MMIO_REG_HOLE_OFFSET (0x80000 - PAGE_SIZE)
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	adev->rmmio_remap.reg_offset = MMIO_REG_HOLE_OFFSET;
	adev->rmmio_remap.bus_addr = adev->rmmio_base + MMIO_REG_HOLE_OFFSET;

	adev->didt_rreg = &nv_didt_rreg;
	adev->didt_wreg = &nv_didt_wreg;

	adev->asic_funcs = &nv_asic_funcs;

	return 0;
}

static int nv_common_late_init(void *handle)
{
	return 0;
}

static int nv_common_sw_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	amdgpu_get_grbm_chip_rev(adev);

	adev->gen_id = FIELD_GET(GENMASK(31, 24), adev->grbm_chip_rev);
	adev->model_id = FIELD_GET(GENMASK(23, 16), adev->grbm_chip_rev);
	adev->major_rev = FIELD_GET(GENMASK(15, 8), adev->grbm_chip_rev);
	adev->minor_rev = FIELD_GET(GENMASK(7, 0), adev->grbm_chip_rev);

	DRM_INFO("ASIC: gen_id=%02x, model_id=%02x, major_rev=%02x, minor_rev=%02x\n",
		 adev->gen_id, adev->model_id, adev->major_rev, adev->minor_rev);

	return 0;
}

static int nv_common_sw_fini(void *handle)
{
	return 0;
}

static uint32_t amdgpu_get_family_mgfx(struct amdgpu_device *adev)
{
	if (MGFX_GEN(adev->grbm_chip_rev) > 0 && MGFX_GEN(adev->grbm_chip_rev) <= 3)
		return AMDGPU_FAMILY_MGFX;
	else if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev))
		return AMDGPU_FAMILY_VGH;
	else
		return AMDGPU_FAMILY_UNKNOWN;
}

static int nv_common_hw_init(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	if (!adev->in_suspend) {
		amdgpu_get_grbm_chip_rev(adev);

		adev->rev_id = nv_get_rev_id(adev);
		adev->external_rev_id = 0xff;
		adev->family = amdgpu_get_family_mgfx(adev);
		adev->cg_flags = AMD_CG_SUPPORT_GFX_CGCG |
			AMD_CG_SUPPORT_GFX_3D_CGCG |
			AMD_CG_SUPPORT_GFX_MGCG |
			AMD_CG_SUPPORT_GFX_STATIC_WGP;
		adev->pg_flags = 0;
		/* PAL needs raw value of mmGRBM_CHIP_REVISION for M2/M3 */
		if (AMDGPU_IS_MGFX2(adev->grbm_chip_rev) ||
			AMDGPU_IS_MGFX3(adev->grbm_chip_rev) ||
			AMDGPU_IS_MGFX2_MID(adev->grbm_chip_rev))
			adev->external_rev_id = adev->grbm_chip_rev;
		else if (MGFX_GEN(adev->grbm_chip_rev) == 1)
			adev->external_rev_id = 0x0A; /* MGFX1 */
		else if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev))
			adev->external_rev_id = 0x80; /* M0 */
		else {
			DRM_ERROR("Unknown chip revision");
			return -EINVAL;
		}
	}

	/* enable aspm */
	nv_program_aspm(adev);
	/* setup nbio registers */
	adev->nbio.funcs->init_registers(adev);
	/* remap HDP registers to a hole in mmio space,
	 * for the purpose of expose those registers
	 * to process space
	 */
	if (adev->nbio.funcs->remap_hdp_registers)
		adev->nbio.funcs->remap_hdp_registers(adev);
	/* enable the doorbell aperture */
	nv_enable_doorbell_aperture(adev, true);

	return 0;
}

static int nv_common_hw_fini(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	/* disable the doorbell aperture */
	nv_enable_doorbell_aperture(adev, false);

	return 0;
}

static int nv_common_suspend(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return nv_common_hw_fini(adev);
}

static int nv_common_resume(void *handle)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)handle;

	return nv_common_hw_init(adev);
}

static bool nv_common_is_idle(void *handle)
{
	return true;
}

static int nv_common_wait_for_idle(void *handle)
{
	return 0;
}

static int nv_common_soft_reset(void *handle)
{
	return 0;
}

static int nv_common_set_clockgating_state(void *handle,
					   enum amd_clockgating_state state)
{
	/* TODO: not supported by Vangogh, remove */
	return 0;
}

static int nv_common_set_powergating_state(void *handle,
					   enum amd_powergating_state state)
{
	/* TODO */
	return 0;
}

static void nv_common_get_clockgating_state(void *handle, u64 *flags)
{
	return;
}

static const struct amd_ip_funcs nv_common_ip_funcs = {
	.name = "nv_common",
	.early_init = nv_common_early_init,
	.late_init = nv_common_late_init,
	.sw_init = nv_common_sw_init,
	.sw_fini = nv_common_sw_fini,
	.hw_init = nv_common_hw_init,
	.hw_fini = nv_common_hw_fini,
	.suspend = nv_common_suspend,
	.resume = nv_common_resume,
	.is_idle = nv_common_is_idle,
	.wait_for_idle = nv_common_wait_for_idle,
	.soft_reset = nv_common_soft_reset,
	.set_clockgating_state = nv_common_set_clockgating_state,
	.set_powergating_state = nv_common_set_powergating_state,
	.get_clockgating_state = nv_common_get_clockgating_state,
};
