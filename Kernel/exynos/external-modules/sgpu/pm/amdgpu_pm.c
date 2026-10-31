/*
 * Copyright 2017 Advanced Micro Devices, Inc.
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
 * Authors: Rafał Miłecki <zajec5@gmail.com>
 *          Alex Deucher <alexdeucher@gmail.com>
 */

#include <drm/drm_debugfs.h>

#include "amdgpu.h"
#include "amdgpu_drv.h"
#include "amdgpu_pm.h"
#include <linux/pci.h>
#include <linux/nospec.h>
#include <linux/pm_runtime.h>

static const struct cg_flag_name clocks[] = {
	{AMD_CG_SUPPORT_GFX_FGCG, "Graphics Fine Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_MGCG, "Graphics Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_MGLS, "Graphics Medium Grain memory Light Sleep"},
	{AMD_CG_SUPPORT_GFX_CGCG, "Graphics Coarse Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_CGLS, "Graphics Coarse Grain memory Light Sleep"},
	{AMD_CG_SUPPORT_GFX_CGTS, "Graphics Coarse Grain Tree Shader Clock Gating"},
	{AMD_CG_SUPPORT_GFX_CGTS_LS, "Graphics Coarse Grain Tree Shader Light Sleep"},
	{AMD_CG_SUPPORT_GFX_CP_LS, "Graphics Command Processor Light Sleep"},
	{AMD_CG_SUPPORT_GFX_RLC_LS, "Graphics Run List Controller Light Sleep"},
	{AMD_CG_SUPPORT_GFX_3D_CGCG, "Graphics 3D Coarse Grain Clock Gating"},
	{AMD_CG_SUPPORT_GFX_3D_CGLS, "Graphics 3D Coarse Grain memory Light Sleep"},
	{AMD_CG_SUPPORT_MC_LS, "Memory Controller Light Sleep"},
	{AMD_CG_SUPPORT_MC_MGCG, "Memory Controller Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_SDMA_LS, "System Direct Memory Access Light Sleep"},
	{AMD_CG_SUPPORT_SDMA_MGCG, "System Direct Memory Access Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_BIF_MGCG, "Bus Interface Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_BIF_LS, "Bus Interface Light Sleep"},
	{AMD_CG_SUPPORT_UVD_MGCG, "Unified Video Decoder Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_VCE_MGCG, "Video Compression Engine Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_HDP_LS, "Host Data Path Light Sleep"},
	{AMD_CG_SUPPORT_HDP_MGCG, "Host Data Path Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_DRM_MGCG, "Digital Right Management Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_DRM_LS, "Digital Right Management Light Sleep"},
	{AMD_CG_SUPPORT_ROM_MGCG, "Rom Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_DF_MGCG, "Data Fabric Medium Grain Clock Gating"},

	{AMD_CG_SUPPORT_ATHUB_MGCG, "Address Translation Hub Medium Grain Clock Gating"},
	{AMD_CG_SUPPORT_ATHUB_LS, "Address Translation Hub Light Sleep"},
	{0, NULL},
};

static ssize_t amdgpu_show_power_state(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	uint32_t power_status = 0;
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;

#ifdef CONFIG_DRM_SGPU_EXYNOS
	if (sgpu_exynos_pm)
		power_status = readl(adev->pm.pmu_mmio);
	else
#endif
		power_status = adev->in_suspend;

	return snprintf(buf, PAGE_SIZE, "%d\n", power_status);
}

static ssize_t amdgpu_set_power_state(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
#ifdef CONFIG_DRM_SGPU_EXYNOS
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = ddev->dev_private;
	int err;
	u32 value;

	err = kstrtou32(buf, 10, &value);
	if (err)
		return err;

	if (sgpu_exynos_pm)
		writel(value, adev->pm.pmu_mmio);
#endif

	return count;
}
static DEVICE_ATTR(amdgpu_power_state, S_IRUGO | S_IWUSR,
		amdgpu_show_power_state, amdgpu_set_power_state);
