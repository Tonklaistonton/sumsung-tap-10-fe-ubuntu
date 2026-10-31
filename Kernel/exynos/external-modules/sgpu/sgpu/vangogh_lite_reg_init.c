/*
 *Copyright (C) 2019-2021 Advanced Micro Devices, Inc.
 */
#include "amdgpu.h"
#include "nv.h"

#include "soc15_common.h"
#include "soc15_hw_ip.h"

#include "vangogh_lite_ip_offset.h"
#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "vangogh_lite_reg.h"

#include "sgpu_bpmd.h"

#include <linux/delay.h>
#include <linux/devfreq.h>
#ifdef CONFIG_DRM_SGPU_EXYNOS
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/exynos-soc.h>
#endif

/* TODO, remove some IP's definitions */
int vangogh_lite_reg_base_init(struct amdgpu_device *adev)
{
	u32 i;

	for (i = 0 ; i < MAX_INSTANCE ; ++i) {
		adev->reg_offset[GC_HWIP][i] = (u32 *)(&GC_BASE.instance[i]);
		adev->reg_offset[NBIO_HWIP][i] =
				(u32 *)(&NBIO_BASE.instance[i]);
	}

	return 0;
}

/* TODO: GFXSW-3317 This code will be removed at silicon */
void vangogh_lite_gpu_hard_reset(struct amdgpu_device *adev)
{
#ifdef CONFIG_DRM_SGPU_EXYNOS
	uint32_t ret = 0;
	uint32_t val = 0x0;
	uint32_t addr = (uint32_t)adev->rmmio_base +
			(SOC15_REG_OFFSET(GC, 0, mmRSMU_HARD_RESETB_GC) * BYTES_PER_DWORD);

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(addr), val, 0);
	if (ret)
		pr_err("writing 0x%x to 0x%x is fail\n", val, addr);

	udelay(SMC_SAFE_WAIT_US);
	val = 0x1;

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(addr), val, 0);
	if (ret)
		pr_err("writing 0x%x to 0x%x is fail\n", val, addr);
#else
	uint32_t value = 0;

	WREG32_SOC15(GC, 0, mmRSMU_HARD_RESETB_GC, 0x0);
	msleep(1);
	value = REG_SET_FIELD(value, RSMU_HARD_RESETB_GC, RSMU_HARD_RESETB, 1);
	WREG32_SOC15(GC, 0, mmRSMU_HARD_RESETB_GC, value);
#endif /* CONFIG_DRM_SGPU_EXYNOS */
}

void vangogh_lite_gpu_soft_reset(struct amdgpu_device *adev)
{
	uint32_t value = 0;

	value = REG_SET_FIELD(value, RLC_RLCS_GRBM_SOFT_RESET, RESET, 1);
	WREG32_SOC15(GC, 0, mmRLC_RLCS_GRBM_SOFT_RESET, value);
}

static void request_gpu_acem_quiesce(struct amdgpu_device *adev)
{
	u32 val = readl(((void __iomem *)adev->rmmio) + GL2ACEM_RST_OFFSET);

	amdgpu_gfx_select_se_sh(adev, 0xffffffff, 0xffffffff, 0xffffffff);

	val = REG_SET_FIELD(val, GL2ACEM_RST, SRT_ST, 1);
	writel(val, ((void __iomem *)adev->rmmio) + GL2ACEM_RST_OFFSET);
}

static int poll_gpu_acem_quiesce(struct amdgpu_device *adev)
{
	u32 val, quiescent = GL2ACEM_RST__SRT_CP_MASK;
	int i;

	for (i = 0; i < adev->gl2acem_instances_count; i++) {
		WREG32_SOC15(GC, 0, mmGRBM_GFX_INDEX, i);

		val = readl(((void __iomem *)adev->rmmio) + GL2ACEM_RST_OFFSET);
		quiescent &= (val & GL2ACEM_RST__SRT_CP_MASK);
	}

	if (!quiescent)
		return -EBUSY;

	return 0;
}

void vangogh_lite_gpu_quiesce(struct amdgpu_device *adev)
{
	u32 polling_us = QUIESCE_TIMEOUT_US;
	int r;

	mutex_lock(&adev->grbm_idx_mutex);
	request_gpu_acem_quiesce(adev);

	while (polling_us) {
		r = poll_gpu_acem_quiesce(adev);
		if (!r) {
			mutex_unlock(&adev->grbm_idx_mutex);
			return;
		}

		udelay(QUIESCE_WAIT_US);
		polling_us -= QUIESCE_WAIT_US;
	}
	mutex_unlock(&adev->grbm_idx_mutex);

#ifdef CONFIG_DRM_SGPU_BPMD
	if (adev->bpmd.funcs != NULL)
		sgpu_bpmd_dump(adev);
#endif  /* CONFIG_DRM_SGPU_BPMD */

	dev_err(adev->dev, "HW defect detected : ACEM quiesce fail!\n");
	sgpu_debug_snapshot_expire_watchdog();
}

#if IS_ENABLED(CONFIG_DRM_SGPU_DVFS)
void vangogh_lite_didt_edc_init(struct amdgpu_device *adev)
{
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	struct device *dev = adev->dev;
	int i, ret, elems = 0;
	uint32_t *init_values;
	uint32_t index, value;

	if (exynos_soc_info.product_id == S5E9955_SOC_ID &&
				(exynos_soc_info.revision & 0xF0) == 0) {
		didt->enable = false;
		DRM_ERROR("Do not support DIDT/EDC in m3 evt0\n");
		return;
	}

	elems = of_property_count_u32_elems(dev->of_node, "didt");
	if (elems <= 0) {
		didt->enable = false;
		return;
	}

	didt->enable = true;
	didt->didt_enable = false;
	didt->edc_enable = false;
	init_values = kcalloc(elems, sizeof(uint32_t), GFP_KERNEL);

	ret = of_property_read_u32(adev->pldev->dev.of_node,
				"didt_threshold", &didt->didt_threshold_freq);
	ret |= of_property_read_u32(adev->pldev->dev.of_node,
				"edc_threshold", &didt->edc_threshold_freq);
	ret |= of_property_read_u32_array(dev->of_node, "didt",
				init_values, elems);
	if (!ret) {
		for (i = 0; i < elems; i += 2) {
			index = init_values[i];
			value = init_values[i+1];
			didt->values[index] = value;
		}
	} else
		DRM_ERROR("Cannot read the didt nodes in DT %d\n", ret);

	kfree(init_values);
}

static void vangogh_lite_didt_enable(struct amdgpu_device *adev)
{
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	uint32_t value = 0;

	if (didt->didt_enable)
		return;

	didt->didt_enable = true;
	value = didt->values[ixDIDT_SQ_CTRL0];
	value = REG_SET_FIELD(value, DIDT_SQ_CTRL0, DIDT_CTRL_EN, 1);
	WREG32_DIDT(ixDIDT_SQ_CTRL0, value);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "didt enable 0x%08x\n",
			RREG32_DIDT(ixDIDT_SQ_CTRL0));
}

static void vangogh_lite_didt_disable(struct amdgpu_device *adev)
{
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	uint32_t value = 0;

	if (!didt->didt_enable)
		return;

	didt->didt_enable = false;
	value = didt->values[ixDIDT_SQ_CTRL0];
	value = REG_SET_FIELD(value, DIDT_SQ_CTRL0, DIDT_CTRL_EN, 0);
	WREG32_DIDT(ixDIDT_SQ_CTRL0, value);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "didt disable 0x%08x\n",
			RREG32_DIDT(ixDIDT_SQ_CTRL0));
}

static void vangogh_lite_edc_enable(struct amdgpu_device *adev)
{
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	uint32_t value = 0;

	if (didt->edc_enable)
		return;

	didt->edc_enable = true;
	value = didt->values[ixDIDT_SQ_EDC_CTRL];
	value = REG_SET_FIELD(value, DIDT_SQ_EDC_CTRL, EDC_EN, 1);
	WREG32_DIDT(ixDIDT_SQ_EDC_CTRL, value);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "edc enable 0x%08x\n",
			RREG32_DIDT(ixDIDT_SQ_EDC_CTRL));
}

static void vangogh_lite_edc_disable(struct amdgpu_device *adev)
{
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	uint32_t value = 0;

	if (!didt->edc_enable)
		return;

	didt->edc_enable = false;
	value = didt->values[ixDIDT_SQ_EDC_CTRL];
	value = REG_SET_FIELD(value, DIDT_SQ_EDC_CTRL, EDC_EN, 0);
	WREG32_DIDT(ixDIDT_SQ_EDC_CTRL, value);

	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "edc disable 0x%08x\n",
			RREG32_DIDT(ixDIDT_SQ_EDC_CTRL));
}

void vangogh_lite_didt_edc_resume(struct amdgpu_device *adev)
{
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	uint32_t value = 0;

	if (!didt->enable)
		return;

	didt->didt_enable = false;
	didt->edc_enable = false;

	value = REG_SET_FIELD(0, GC_CAC_CTRL_2, SE_LCAC_ENABLE, 1);
	WREG32_SOC15(GC, 0, mmGC_CAC_CTRL_2, value);
	WREG32_SOC15(GC, 0, mmDIDT_INDEX_AUTO_INCR_EN, 0x00000001);

	if (AMDGPU_IS_MGFX3(adev->grbm_chip_rev)) {
		/* WEIGHT0 : 0x08, WEIGHT1 : 0x41, WEIGHT2 : 0x2a, WEIGHT3 : 0x96 */
		WREG32_DIDT(ixDIDT_SQ_WEIGHT0_3, didt->values[ixDIDT_SQ_WEIGHT0_3]);
		/* WEIGHT4 : 0xff */
		WREG32_DIDT(ixDIDT_SQ_WEIGHT4_7, didt->values[ixDIDT_SQ_WEIGHT4_7]);
		/* DIDT only */
		/* DIDT_STALL_DELAY_HI : 0x01, DIDT_STALL_DELAY_LO : 0x01,
	 	 * DIDT_MAX_STALLS_ALLOWED_HI : 0x03, DIDT_MAX_STALLS_ALLOWED_LO : 0x03
	 	 */
		WREG32_DIDT(ixDIDT_SQ_STALL_CTRL, didt->values[ixDIDT_SQ_STALL_CTRL]);
		/* MIN_POWER : 0x0000, MAX_POWER : 0x8200 */
		WREG32_DIDT(ixDIDT_SQ_CTRL1, didt->values[ixDIDT_SQ_CTRL1]);
		/* MAX_POWER_DELTA : 0x0a6, SHORT_TERM_INTERVAL_SIZE : 0x20,
	 	 * LONG_TERM_INTERVAL_RATIO : 0x04
	 	 */
		WREG32_DIDT(ixDIDT_SQ_CTRL2, didt->values[ixDIDT_SQ_CTRL2]);
		/* DIDT_STALL_RELEASE_CNTL_EN : 0x1, DIDT_STALL_CNTL_SEL : 0x1,
		 * DIDT_RELEASE_DELAY_HI : 0x8, DIDT_RELEASE_DELAY_LO : 0x8
		 */
		WREG32_DIDT(ixDIDT_SQ_STALL_RELEASE_CNTL0, didt->values[ixDIDT_SQ_STALL_RELEASE_CNTL0]);
		/* DIDT_BASE_RELEASE_ALLOWED_HI : 0x1, DIDT_BASE_RELEASE_ALLOWED_LO : 0x1 */
		WREG32_DIDT(ixDIDT_SQ_STALL_RELEASE_CNTL1, didt->values[ixDIDT_SQ_STALL_RELEASE_CNTL1]);
		/* DIDT_THROTTLE_MODE : 0x1, DIDT_STALL_EVENT_EN : 0x1 */
		WREG32_DIDT(ixDIDT_SQ_CTRL0, didt->values[ixDIDT_SQ_CTRL0]);

		vangogh_lite_didt_enable(adev);

		/* EDC only */
		/* EDC_THRESHOLD : 0x4b */
		WREG32_DIDT(ixDIDT_SQ_EDC_THRESHOLD, didt->values[ixDIDT_SQ_EDC_THRESHOLD]);
		/* EDC_STALL_PATTERN_1 : 0x0001, EDC_STALL_PATTERN_2 : 0x0101 */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_1_2, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_1_2]);
		/* EDC_STALL_PATTERN_3 : 0x0421, EDC_STALL_PATTERN_4 : 0x1111 */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_3_4, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_3_4]);
		/* EDC_STALL_PATTERN_5 : 0x1249, EDC_STALL_PATTERN_6 : 0x2529 */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_5_6, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_5_6]);
		/* EDC_STALL_PATTERN_7 : 0x2aaa */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_7, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_7]);
		/* EDC_TRIGGER_THROTTLE_LOWBIT : 0x4,  SE_EDC_LEVEL_COMB_EN : 0x1 */
		WREG32_DIDT(ixDIDT_SQ_EDC_CTRL, didt->values[ixDIDT_SQ_EDC_CTRL]);

	} else if(AMDGPU_IS_MGFX2_MID(adev->grbm_chip_rev)) {

		/* WEIGHT0 : 0x08, WEIGHT1 : 0x41, WEIGHT2 : 0x2a, WEIGHT3 : 0x96 */
		WREG32_DIDT(ixDIDT_SQ_WEIGHT0_3, didt->values[ixDIDT_SQ_WEIGHT0_3]);
		/* WEIGHT4 : 0xff */
		WREG32_DIDT(ixDIDT_SQ_WEIGHT4_7, didt->values[ixDIDT_SQ_WEIGHT4_7]);
       		/* DIDT only */
		/* DIDT_STALL_DELAY_HI : 0x01, DIDT_STALL_DELAY_LO : 0x01,
		 * DIDT_MAX_STALLS_ALLOWED_HI : 0x03, DIDT_MAX_STALLS_ALLOWED_LO : 0x03
		 */
		WREG32_DIDT(ixDIDT_SQ_STALL_CTRL, didt->values[ixDIDT_SQ_STALL_CTRL]);
		WREG32_DIDT(ixDIDT_SQ_TUNING_CTRL, didt->values[ixDIDT_SQ_TUNING_CTRL]);
		/* MIN_POWER : 0x0000, MAX_POWER : 0x8200 */
		WREG32_DIDT(ixDIDT_SQ_CTRL1, didt->values[ixDIDT_SQ_CTRL1]);
		/* MAX_POWER_DELTA : 0x0a6, SHORT_TERM_INTERVAL_SIZE : 0x20,
		 * LONG_TERM_INTERVAL_RATIO : 0x04
		 */
		WREG32_DIDT(ixDIDT_SQ_CTRL2, didt->values[ixDIDT_SQ_CTRL2]);
		/* DIDT_THROTTLE_MODE : 0x1, DIDT_STALL_EVENT_EN : 0x1 */
		WREG32_DIDT(ixDIDT_SQ_CTRL0, didt->values[ixDIDT_SQ_CTRL0]);

		vangogh_lite_didt_enable(adev);

		/* EDC only */
		/* EDC_THRESHOLD : 0x4b */
		WREG32_DIDT(ixDIDT_SQ_EDC_THRESHOLD, didt->values[ixDIDT_SQ_EDC_THRESHOLD]);
		/* EDC_STALL_PATTERN_1 : 0x0001, EDC_STALL_PATTERN_2 : 0x0101 */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_1_2, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_1_2]);
		/* EDC_STALL_PATTERN_3 : 0x0421, EDC_STALL_PATTERN_4 : 0x1111 */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_3_4, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_3_4]);
		/* EDC_STALL_PATTERN_5 : 0x1249, EDC_STALL_PATTERN_6 : 0x2529 */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_5_6, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_5_6]);
		/* EDC_STALL_PATTERN_7 : 0x2aaa */
		WREG32_DIDT(ixDIDT_SQ_EDC_STALL_PATTERN_7, didt->values[ixDIDT_SQ_EDC_STALL_PATTERN_7]);
		/* EDC_TRIGGER_THROTTLE_LOWBIT : 0x4,  SE_EDC_LEVEL_COMB_EN : 0x1 */
		WREG32_DIDT(ixDIDT_SQ_EDC_CTRL, didt->values[ixDIDT_SQ_EDC_CTRL]);
	}
}

void vangogh_lite_set_didt_edc(struct amdgpu_device *adev, unsigned long freq)
{
	struct devfreq *df = adev->devfreq;
	struct sgpu_didt_edc *didt = &adev->didt_edc;
	bool didt_enable = didt->didt_enable;
	bool edc_enable = didt->edc_enable;

	if (!didt->enable)
		return;

	/* in resume or ifpo power on case */
	if (freq == 0) {
		if (df->profile->get_cur_freq)
			df->profile->get_cur_freq(df->dev.parent, &freq);
		else
			freq = df->previous_freq;
	}

	/* DIDT */
	if (!didt_enable && freq >= didt->didt_threshold_freq)
		vangogh_lite_didt_enable(adev);
	else if (didt_enable && freq < didt->didt_threshold_freq)
		vangogh_lite_didt_disable(adev);

	/* EDC */
	if (!edc_enable && freq >= didt->edc_threshold_freq)
		vangogh_lite_edc_enable(adev);
	else if (edc_enable && freq < didt->edc_threshold_freq)
		vangogh_lite_edc_disable(adev);

	if (didt_enable != didt->didt_enable || edc_enable != didt->edc_enable)
		SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS,
				"freq %lu didt_enable %d edc_enable %d",
				freq, didt->didt_enable ? 1:0, didt->edc_enable ? 1:0);
}
#endif
