// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/pm_runtime.h>

#include "amdgpu.h"
#include "sgpu_debug.h"
#include "soc15_common.h"
#include "gc/gc_10_4_0_default.h"
#include "gc/gc_10_4_0_offset.h"

#include <soc/samsung/exynos/debug-snapshot.h>

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#ifdef CONFIG_DRM_AMDGPU_DUMP
static void sgpu_show_asic_status(struct amdgpu_device *adev)
{
	if (adev->asic_funcs->get_asic_status)
		adev->asic_funcs->get_asic_status(adev, NULL, 0);
}
#else
#define sgpu_show_asic_status(adev) do { } while (0)
#endif

static const char * const s2mpu_fault_type[] = {"table walk", "permission", "unknown", "context"};
static const char *s2mpu_subsys[NR_SGPU_S2MPU_SUBSYS] = {"SYSTEM", "TZMP2_GPU", "G3D_TMR"};

bool sgpu_jobtimeout_to_panic_check(struct amdgpu_device *adev, int ring_type)
{
	if ((adev->sgpu_debug.gfx_jobtimeout_to_panic && ring_type == AMDGPU_RING_TYPE_GFX) ||
	    (adev->sgpu_debug.comp_jobtimeout_to_panic && ring_type == AMDGPU_RING_TYPE_COMPUTE))
		return true;

	return false;
}

static struct sgpu_debug *to_sgpu_debug(struct s2mpu_notifier_block *nb, const char *subsys)
{
	int i;

	for (i = 0; i < NR_SGPU_S2MPU_SUBSYS; i++)
		if (!strcmp(subsys, s2mpu_subsys[i]))
			return container_of(nb, struct sgpu_debug, s2mpu_nb[i]);

	return NULL;
}

static int sgpu_s2mpu_notifier_call(struct s2mpu_notifier_block *nb,
				    struct s2mpu_notifier_info *info)
{
	const char *faulttype = "unknown";
	struct sgpu_debug *debug;
	struct amdgpu_device *adev;

	debug = to_sgpu_debug(nb, info->subsystem);
	if (!debug)
		return S2MPU_NOTIFY_BAD;

	adev = container_of(debug, struct amdgpu_device, sgpu_debug);

	if ((info->fault_type > 0) && (info->fault_type <= FAULT_TYPE_CONTEXT_FAULT))
		faulttype = s2mpu_fault_type[info->fault_type - 1];

	pr_crit("S2MPU FAULT on %s %#lx (size %#x bytes, type %d(%s))\n",
		info->fault_rw ? "WRITE to" : "READ from", info->fault_addr,
		info->fault_len, info->fault_type, faulttype);

#if !defined(CONFIG_GPU_VERSION_M0) && !defined(CONFIG_GPU_VERSION_M1)
	pr_crit("GPU access address : 0x%x\n", RREG32_SOC15(GC, 0, mmGL2ACEM_STUS_ADDR));
#endif

	sgpu_show_asic_status(container_of(debug, struct amdgpu_device, sgpu_debug));

	return S2MPU_NOTIFY_BAD;
}

static void sgpu_register_s2mpu_notifier(struct amdgpu_device *adev)
{
	int i;

	for (i = 0; i < NR_SGPU_S2MPU_SUBSYS; i++) {
		adev->sgpu_debug.s2mpu_nb[i].subsystem = s2mpu_subsys[i];
		adev->sgpu_debug.s2mpu_nb[i].notifier_call = sgpu_s2mpu_notifier_call;
		if (!exynos_s2mpu_notifier_call_register(&adev->sgpu_debug.s2mpu_nb[i]))
			dev_info(adev->dev, "Registered S2MPU %s Notifier", s2mpu_subsys[i]);
	}
}
#else
#define sgpu_register_s2mpu_notifier(adev) do { } while (0)
#endif


#if IS_ENABLED(CONFIG_GPU_VERSION_M3)
static void __iomem *cmu;

void sgpu_debug_snapshot_expire_watchdog(void)
{
	if (!cmu) {
		pr_err("Failed to ioremap for CMU_G3DCORE, skip GPU clk freeze\n");
		goto skip_clkfreeze;
	}

	writel(4, cmu + 0x14);
	writel(4, cmu + 0x18);
	writel(4, cmu + 0x1c);
	writel(4, cmu + 0x20);
	writel(4, cmu + 0x24);
	writel(4, cmu + 0x34);

skip_clkfreeze:
	dbg_snapshot_expire_watchdog();
}
#else
void sgpu_debug_snapshot_expire_watchdog(void)
{
	dbg_snapshot_expire_watchdog();
}
#endif

/* This sfr is to find the address that gpu approaches in s2mpu situation.
 * The GPU's access to the address area below is not recorded. */
void sgpu_debug_set_gl2acem_address(struct amdgpu_device *adev)
{
	if (adev->runpm)
		pm_runtime_get_sync(adev->ddev.dev);

#if !defined(CONFIG_GPU_VERSION_M0) && !defined(CONFIG_GPU_VERSION_M1)
	WREG32_SOC15(GC, 0, mmGL2ACEM_ADDR_UP, 0xffffff);
	WREG32_SOC15(GC, 0, mmGL2ACEM_ADDR_LOW, 0x220000);
#endif

	if (adev->runpm)
		pm_runtime_put_sync(adev->ddev.dev);
}

void sgpu_debug_init(struct amdgpu_device *adev)
{
	sgpu_register_s2mpu_notifier(adev);
	sgpu_debug_set_gl2acem_address(adev);

	adev->sgpu_debug.gfx_jobtimeout_to_panic = sgpu_crash_on_exception & 1;
	adev->sgpu_debug.comp_jobtimeout_to_panic = (sgpu_crash_on_exception >> 1) & 1;
	adev->sgpu_debug.pagefault_to_panic = (sgpu_crash_on_exception >> 2) & 1;

#if IS_ENABLED(CONFIG_GPU_VERSION_M3)
	cmu = ioremap(0x22033000, PAGE_SIZE);
#endif
}
