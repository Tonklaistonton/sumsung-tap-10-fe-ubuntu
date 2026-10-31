/*
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <drm/sgpu_drm.h>
#include <drm/drm_aperture.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem.h>
#include <drm/drm_vblank.h>
#include <drm/drm_managed.h>
#include "amdgpu_drv.h"

#include <linux/console.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <drm/drm_probe_helper.h>
#include <linux/mmu_notifier.h>

#include "amdgpu.h"
#include "amdgpu_irq.h"
#include "amdgpu_trace.h"
#include "amdgpu_dma_buf.h"
#include "sgpu_sysfs.h"

/*
 * KMS wrapper.
 * - 3.0.0 - initial driver
 * - 3.1.0 - allow reading more status registers (GRBM, SRBM, SDMA, CP)
 * - 3.2.0 - GFX8: Uses EOP_TC_WB_ACTION_EN, so UMDs don't have to do the same
 *           at the end of IBs.
 * - 3.3.0 - Add VM support for UVD on supported hardware.
 * - 3.4.0 - Add AMDGPU_INFO_NUM_EVICTIONS.
 * - 3.5.0 - Add support for new UVD_NO_OP register.
 * - 3.6.0 - kmd involves use CONTEXT_CONTROL in ring buffer.
 * - 3.7.0 - Add support for VCE clock list packet
 * - 3.8.0 - Add support raster config init in the kernel
 * - 3.9.0 - Add support for memory query info about VRAM and GTT.
 * - 3.10.0 - Add support for new fences ioctl, new gem ioctl flags
 * - 3.11.0 - Add support for sensor query info (clocks, temp, etc).
 * - 3.12.0 - Add query for double offchip LDS buffers
 * - 3.13.0 - Add PRT support
 * - 3.14.0 - Fix race in amdgpu_ctx_get_fence() and note new functionality
 * - 3.15.0 - Export more gpu info for gfx9
 * - 3.16.0 - Add reserved vmid support
 * - 3.17.0 - Add AMDGPU_NUM_VRAM_CPU_PAGE_FAULTS.
 * - 3.18.0 - Export gpu always on cu bitmap
 * - 3.19.0 - Add support for UVD MJPEG decode
 * - 3.20.0 - Add support for local BOs
 * - 3.21.0 - Add DRM_AMDGPU_FENCE_TO_HANDLE ioctl
 * - 3.22.0 - Add DRM_AMDGPU_SCHED ioctl
 * - 3.23.0 - Add query for VRAM lost counter
 * - 3.24.0 - Add high priority compute support for gfx9
 * - 3.25.0 - Add support for sensor query info (stable pstate sclk/mclk).
 * - 3.26.0 - GFX9: Process AMDGPU_IB_FLAG_TC_WB_NOT_INVALIDATE.
 * - 3.27.0 - Add new chunk to to AMDGPU_CS to enable BO_LIST creation.
 * - 3.28.0 - Add AMDGPU_CHUNK_ID_SCHEDULED_DEPENDENCIES
 * - 3.29.0 - Add AMDGPU_IB_FLAG_RESET_GDS_MAX_WAVE_ID
 * - 3.30.0 - Add AMDGPU_SCHED_OP_CONTEXT_PRIORITY_OVERRIDE.
 * - 3.31.0 - Add support for per-flip tiling attribute changes with DC
 * - 3.32.0 - Add syncobj timeline support to AMDGPU_CS.
 * - 3.33.0 - Fixes for GDS ENOMEM failures in AMDGPU_CS.
 * - 3.34.0 - Non-DC can flip correctly between buffers with different pitches
 * - 3.35.0 - Add drm_amdgpu_info_device::tcc_disabled_mask
 * - 3.36.0 - Allow reading more status registers on si/cik
 * - 3.37.0 - L2 is invalidated before SDMA IBs, needed for correctness
 * - 3.38.0 - Add AMDGPU_IB_FLAG_EMIT_MEM_SYNC
 * - 3.39.0 - DMABUF implicit sync does a full pipeline sync
 * - 3.40.0 - Add AMDGPU_IDS_FLAGS_TMZ
 *   3.49.0 - Add gang submit into CS IOCTL
 */
#define KMS_DRIVER_MAJOR	3
#define KMS_DRIVER_MINOR	49
#define KMS_DRIVER_PATCHLEVEL	0

#define GRBM_CHIP_REV_DEFAULT	0x00600100	/* Voyager EVT0 */


int amdgpu_vram_limit = 0;
int amdgpu_vis_vram_limit = 0;
int amdgpu_gart_size = -1; /* auto */
int amdgpu_gtt_size = -1; /* auto */
int amdgpu_moverate = -1; /* auto */
int amdgpu_benchmarking = 0;
int amdgpu_audio = -1;
int amdgpu_disp_priority = 0;
int amdgpu_hw_i2c = 0;
int amdgpu_msi = -1;
char amdgpu_lockup_timeout[AMDGPU_MAX_TIMEOUT_PARAM_LENGTH] = "2500";
int amdgpu_dpm = -1;
int amdgpu_fw_load_type = -1;
int amdgpu_aspm = -1;
uint amdgpu_ip_block_mask = 0xffffffff;
int amdgpu_bapm = -1;
int amdgpu_deep_color = 0;
int amdgpu_vm_size = -1;
int amdgpu_vm_fragment_size = -1;
int amdgpu_vm_block_size = -1;
int amdgpu_vm_fault_stop = 0;
int amdgpu_vm_debug = 0;
int amdgpu_vm_update_mode = -1;
int amdgpu_exp_hw_support = 0;
int amdgpu_sched_jobs = 32;
int amdgpu_sched_hw_submission = 4;
uint amdgpu_cg_mask = 0xffffffff;
uint amdgpu_pg_mask = 0xffffffff;
uint amdgpu_sdma_phase_quantum = 32;
char *amdgpu_disable_cu = NULL;
char *amdgpu_virtual_display = NULL;
/* OverDrive(bit 14) disabled by default*/
uint amdgpu_pp_feature_mask = 0xffffbfff;
uint amdgpu_force_long_training = 0;
int amdgpu_job_hang_limit = 0;
int amdgpu_lbpw = -1;
int amdgpu_compute_multipipe = -1;
int amdgpu_gpu_recovery = -1; /* auto */
#ifdef CONFIG_DRM_SGPU_EMULATION_GPU_ONLY
int amdgpu_emu_mode = 1;
#else
int amdgpu_emu_mode = 0;
#endif
uint amdgpu_smu_memory_pool_size = 0;
int amdgpu_async_gfx_ring = 1;
int amdgpu_mcbp = 2;
int amdgpu_discovery = 0;
int amdgpu_noretry = -1;
uint sgpu_force_chip_rev = 0;
int amdgpu_tmz = 1;
int amdgpu_reset_method = -1; /* auto */
int amdgpu_num_kcq = -1;

int amdgpu_sws_quantum = -1;

#ifdef CONFIG_DRM_SGPU_BPMD
char *sgpu_bpmd_path = CONFIG_DRM_SGPU_BPMD_OUTPUT_DEFAULT;
#endif	/* CONFIG_DRM_SGPU_BPMD */

#if IS_ENABLED(CONFIG_DRM_SGPU_EXYNOS)
int sgpu_exynos_pm = 1;
#else
int sgpu_exynos_pm = 0;
#endif

#if IS_ENABLED(CONFIG_DRM_SGPU_RUNTIME_PM)
int amdgpu_runtime_pm = 1;
#else
int amdgpu_runtime_pm = 0;
#endif /* CONFIG_DRM_SGPU_RUNTIME_PM */

struct amdgpu_mgpu_info mgpu_info = {
	.mutex = __MUTEX_INITIALIZER(mgpu_info.mutex),
};

/**
 * DOC: eval_mode (int)
 * Specify evaluation target.
 * 0: disable evalation mode by default.
 * 1: evaluation target is GC10.4(3RB, 3WGP, 1SA, 1SE)
 * 2: evaluation target is GC40.1(3RB, 4WGP, 1SA, 1SE)
 * 3: evaluation target is GC40.2(4RB, 6WGP, 2SA, 1SE)
 * it can be extended for other projects
 */
int amdgpu_eval_mode = 0;
MODULE_PARM_DESC(eval_mode, "Evaluate gpu performace. 0:disable 1:GC10.4 2:GC40.1 3:GC40.2");
module_param_named(eval_mode, amdgpu_eval_mode, int, 0444);

/**
 * DOC: vramlimit (int)
 * Restrict the total amount of VRAM in MiB for testing.  The default is 0 (Use full VRAM).
 */
MODULE_PARM_DESC(vramlimit, "Restrict VRAM for testing, in megabytes");
module_param_named(vramlimit, amdgpu_vram_limit, int, 0600);

/**
 * DOC: vis_vramlimit (int)
 * Restrict the amount of CPU visible VRAM in MiB for testing.  The default is 0 (Use full CPU visible VRAM).
 */
MODULE_PARM_DESC(vis_vramlimit, "Restrict visible VRAM for testing, in megabytes");
module_param_named(vis_vramlimit, amdgpu_vis_vram_limit, int, 0444);

/**
 * DOC: gartsize (uint)
 * Restrict the size of GART in Mib (32, 64, etc.) for testing. The default is -1 (The size depends on asic).
 */
MODULE_PARM_DESC(gartsize, "Size of GART to setup in megabytes (32, 64, etc., -1=auto)");
module_param_named(gartsize, amdgpu_gart_size, uint, 0600);

/**
 * DOC: gttsize (int)
 * Restrict the size of GTT domain in MiB for testing. The default is -1 (It's VRAM size if 3GB < VRAM < 3/4 RAM,
 * otherwise 3/4 RAM size).
 */
MODULE_PARM_DESC(gttsize, "Size of the GTT domain in megabytes (-1 = auto)");
module_param_named(gttsize, amdgpu_gtt_size, int, 0600);

/**
 * DOC: moverate (int)
 * Set maximum buffer migration rate in MB/s. The default is -1 (8 MB/s).
 */
MODULE_PARM_DESC(moverate, "Maximum buffer migration rate in MB/s. (32, 64, etc., -1=auto, 0=1=disabled)");
module_param_named(moverate, amdgpu_moverate, int, 0600);

/**
 * DOC: audio (int)
 * Set HDMI/DPAudio. Only affects non-DC display handling. The default is -1 (Enabled), set 0 to disabled it.
 */
MODULE_PARM_DESC(audio, "Audio enable (-1 = auto, 0 = disable, 1 = enable)");
module_param_named(audio, amdgpu_audio, int, 0444);

/**
 * DOC: disp_priority (int)
 * Set display Priority (1 = normal, 2 = high). Only affects non-DC display handling. The default is 0 (auto).
 */
MODULE_PARM_DESC(disp_priority, "Display Priority (0 = auto, 1 = normal, 2 = high)");
module_param_named(disp_priority, amdgpu_disp_priority, int, 0444);

/**
 * DOC: hw_i2c (int)
 * To enable hw i2c engine. Only affects non-DC display handling. The default is 0 (Disabled).
 */
MODULE_PARM_DESC(hw_i2c, "hw i2c engine enable (0 = disable)");
module_param_named(hw_i2c, amdgpu_hw_i2c, int, 0444);

/**
 * DOC: msi (int)
 * To disable Message Signaled Interrupts (MSI) functionality (1 = enable, 0 = disable). The default is -1 (auto, enabled).
 */
MODULE_PARM_DESC(msi, "MSI support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(msi, amdgpu_msi, int, 0444);

/**
 * DOC: lockup_timeout (string)
 * Set GPU scheduler timeout value in ms.
 *
 * The format can be [GFX] or [GFX,Compute]. That is there can be one or
 * multiple values specified. 0 and negative values are invalidated. They will be adjusted
 * to the default timeout.
 *
 * - With one value specified, the setting will apply to GFX jobs.
 * - With multiple values specified, the first one will be for GFX and second one is for Compute.
 *
 * By default(with no lockup_timeout settings),
 * - the timeout for all non-compute(GFX) jobs is 2500.
 * - timeout for compute jobs are 20000 by default.
 */
MODULE_PARM_DESC(lockup_timeout, "GPU lockup timeout in ms (default: 2500 for GFX jobs and 20000 timeout for compute jobs;"
		" 0: keep default value. negative: infinity timeout), "
		"format: [GFX] or [GFX,Compute]");
module_param_string(lockup_timeout, amdgpu_lockup_timeout, sizeof(amdgpu_lockup_timeout), 0444);

/**
 * DOC: dpm (int)
 * Override for dynamic power management setting
 * (0 = disable, 1 = enable, 2 = enable sw smu driver for vega20)
 * The default is -1 (auto).
 */
MODULE_PARM_DESC(dpm, "DPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(dpm, amdgpu_dpm, int, 0444);

/**
 * DOC: fw_load_type (int)
 * Set different firmware loading type for debugging (0 = direct, 1 = SMU, 2 = PSP). The default is -1 (auto).
 */
MODULE_PARM_DESC(fw_load_type, "firmware loading type (0 = direct, 1 = SMU, 2 = PSP, -1 = auto)");
module_param_named(fw_load_type, amdgpu_fw_load_type, int, 0444);

/**
 * DOC: aspm (int)
 * To disable ASPM (1 = enable, 0 = disable). The default is -1 (auto, enabled).
 */
MODULE_PARM_DESC(aspm, "ASPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(aspm, amdgpu_aspm, int, 0444);

/**
 * DOC: runpm (int)
 * Override for runtime power management control for dGPUs in PX/HG laptops. The amdgpu driver can dynamically power down
 * the dGPU on PX/HG laptops when it is idle. The default is -1 (auto enable). Setting the value to 0 disables this functionality.
 */
MODULE_PARM_DESC(runpm, "PX runtime pm (1 = force enable, 0 = disable, -1 = PX only default)");
module_param_named(runpm, amdgpu_runtime_pm, int, 0444);

/**
 * DOC: ip_block_mask (uint)
 * Override what IP blocks are enabled on the GPU. Each GPU is a collection of IP blocks (gfx, display, video, etc.).
 * Use this parameter to disable specific blocks. Note that the IP blocks do not have a fixed index. Some asics may not have
 * some IPs or may include multiple instances of an IP so the ordering various from asic to asic. See the driver output in
 * the kernel log for the list of IPs on the asic. The default is 0xffffffff (enable all blocks on a device).
 */
MODULE_PARM_DESC(ip_block_mask, "IP Block Mask (all blocks enabled (default))");
module_param_named(ip_block_mask, amdgpu_ip_block_mask, uint, 0444);

/**
 * DOC: bapm (int)
 * Bidirectional Application Power Management (BAPM) used to dynamically share TDP between CPU and GPU. Set value 0 to disable it.
 * The default -1 (auto, enabled)
 */
MODULE_PARM_DESC(bapm, "BAPM support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(bapm, amdgpu_bapm, int, 0444);

/**
 * DOC: deep_color (int)
 * Set 1 to enable Deep Color support. Only affects non-DC display handling. The default is 0 (disabled).
 */
MODULE_PARM_DESC(deep_color, "Deep Color support (1 = enable, 0 = disable (default))");
module_param_named(deep_color, amdgpu_deep_color, int, 0444);

/**
 * DOC: vm_size (int)
 * Override the size of the GPU's per client virtual address space in GiB.  The default is -1 (automatic for each asic).
 */
MODULE_PARM_DESC(vm_size, "VM address space size in gigabytes (default 64GB)");
module_param_named(vm_size, amdgpu_vm_size, int, 0444);

/**
 * DOC: vm_fragment_size (int)
 * Override VM fragment size in bits (4, 5, etc. 4 = 64K, 9 = 2M). The default is -1 (automatic for each asic).
 */
MODULE_PARM_DESC(vm_fragment_size, "VM fragment size in bits (4, 5, etc. 4 = 64K (default), Max 9 = 2M)");
module_param_named(vm_fragment_size, amdgpu_vm_fragment_size, int, 0444);

/**
 * DOC: vm_block_size (int)
 * Override VM page table size in bits (default depending on vm_size and hw setup). The default is -1 (automatic for each asic).
 */
MODULE_PARM_DESC(vm_block_size, "VM page table size in bits (default depending on vm_size)");
module_param_named(vm_block_size, amdgpu_vm_block_size, int, 0444);

/**
 * DOC: vm_fault_stop (int)
 * Stop on VM fault for debugging (0 = never, 1 = print first, 2 = always). The default is 0 (No stop).
 */
MODULE_PARM_DESC(vm_fault_stop, "Stop on VM fault (0 = never (default), 1 = print first, 2 = always)");
module_param_named(vm_fault_stop, amdgpu_vm_fault_stop, int, 0444);

/**
 * DOC: vm_debug (int)
 * Debug VM handling (0 = disabled, 1 = enabled). The default is 0 (Disabled).
 */
MODULE_PARM_DESC(vm_debug, "Debug VM handling (0 = disabled (default), 1 = enabled)");
module_param_named(vm_debug, amdgpu_vm_debug, int, 0644);

/**
 * DOC: exp_hw_support (int)
 * Enable experimental hw support (1 = enable). The default is 0 (disabled).
 */
MODULE_PARM_DESC(exp_hw_support, "experimental hw support (1 = enable, 0 = disable (default))");
module_param_named(exp_hw_support, amdgpu_exp_hw_support, int, 0444);

/**
 * DOC: sched_jobs (int)
 * Override the max number of jobs supported in the sw queue. The default is 32.
 */
MODULE_PARM_DESC(sched_jobs, "the max number of jobs supported in the sw queue (default 32)");
module_param_named(sched_jobs, amdgpu_sched_jobs, int, 0444);

/**
 * DOC: sched_hw_submission (int)
 * Override the max number of HW submissions. The default is 2.
 */
MODULE_PARM_DESC(sched_hw_submission, "the max number of HW submissions (default 2)");
module_param_named(sched_hw_submission, amdgpu_sched_hw_submission, int, 0444);

/**
 * DOC: ppfeaturemask (hexint)
 * Override power features enabled. See enum PP_FEATURE_MASK in drivers/gpu/drm/amd/include/amd_shared.h.
 * The default is the current set of stable power features.
 */
MODULE_PARM_DESC(ppfeaturemask, "all power features enabled (default))");
module_param_named(ppfeaturemask, amdgpu_pp_feature_mask, hexint, 0444);

/**
 * DOC: forcelongtraining (uint)
 * Force long memory training in resume.
 * The default is zero, indicates short training in resume.
 */
MODULE_PARM_DESC(forcelongtraining, "force memory long training");
module_param_named(forcelongtraining, amdgpu_force_long_training, uint, 0444);

/**
 * DOC: cg_mask (uint)
 * Override Clockgating features enabled on GPU (0 = disable clock gating). See the AMD_CG_SUPPORT flags in
 * drivers/gpu/drm/amd/include/amd_shared.h. The default is 0xffffffff (all enabled).
 */
MODULE_PARM_DESC(cg_mask, "Clockgating flags mask (0 = disable clock gating)");
module_param_named(cg_mask, amdgpu_cg_mask, uint, 0444);

/**
 * DOC: pg_mask (uint)
 * Override Powergating features enabled on GPU (0 = disable power gating). See the AMD_PG_SUPPORT flags in
 * drivers/gpu/drm/amd/include/amd_shared.h. The default is 0xffffffff (all enabled).
 */
MODULE_PARM_DESC(pg_mask, "Powergating flags mask (0 = disable power gating)");
module_param_named(pg_mask, amdgpu_pg_mask, uint, 0444);

/**
 * DOC: sdma_phase_quantum (uint)
 * Override SDMA context switch phase quantum (x 1K GPU clock cycles, 0 = no change). The default is 32.
 */
MODULE_PARM_DESC(sdma_phase_quantum, "SDMA context switch phase quantum (x 1K GPU clock cycles, 0 = no change (default 32))");
module_param_named(sdma_phase_quantum, amdgpu_sdma_phase_quantum, uint, 0444);

/**
 * DOC: disable_cu (charp)
 * Set to disable CUs (It's set like se.sh.cu,...). The default is NULL.
 */
MODULE_PARM_DESC(disable_cu, "Disable CUs (se.sh.cu,...)");
module_param_named(disable_cu, amdgpu_disable_cu, charp, 0444);

/**
 * DOC: virtual_display (charp)
 * Set to enable virtual display feature. This feature provides a virtual display hardware on headless boards
 * or in virtualized environments. It will be set like xxxx:xx:xx.x,x;xxxx:xx:xx.x,x. It's the pci address of
 * the device, plus the number of crtcs to expose. E.g., 0000:26:00.0,4 would enable 4 virtual crtcs on the pci
 * device at 26:00.0. The default is NULL.
 */
MODULE_PARM_DESC(virtual_display,
		 "Enable virtual display feature (the virtual_display will be set like xxxx:xx:xx.x,x;xxxx:xx:xx.x,x)");
module_param_named(virtual_display, amdgpu_virtual_display, charp, 0444);

/**
 * DOC: job_hang_limit (int)
 * Set how much time allow a job hang and not drop it. The default is 0.
 */
MODULE_PARM_DESC(job_hang_limit, "how much time allow a job hang and not drop it (default 0)");
module_param_named(job_hang_limit, amdgpu_job_hang_limit, int ,0444);

/**
 * DOC: lbpw (int)
 * Override Load Balancing Per Watt (LBPW) support (1 = enable, 0 = disable). The default is -1 (auto, enabled).
 */
MODULE_PARM_DESC(lbpw, "Load Balancing Per Watt (LBPW) support (1 = enable, 0 = disable, -1 = auto)");
module_param_named(lbpw, amdgpu_lbpw, int, 0444);

MODULE_PARM_DESC(compute_multipipe, "Force compute queues to be spread across pipes (1 = enable, 0 = disable, -1 = auto)");
module_param_named(compute_multipipe, amdgpu_compute_multipipe, int, 0444);

/**
 * DOC: gpu_recovery (int)
 * Set to enable GPU recovery mechanism (1 = enable, 0 = disable). The default is -1 (auto, disabled except SRIOV).
 */
MODULE_PARM_DESC(gpu_recovery, "Enable GPU recovery mechanism, (1 = enable, 0 = disable, -1 = auto)");
module_param_named(gpu_recovery, amdgpu_gpu_recovery, int, 0444);

/**
 * DOC: emu_mode (int)
 * Set value 1 to enable emulation mode. This is only needed when running on an emulator. The default is 0 (disabled).
 */
MODULE_PARM_DESC(emu_mode, "Emulation mode, (1 = enable, 0 = disable)");
module_param_named(emu_mode, amdgpu_emu_mode, int, 0444);

/**
 * DOC: smu_memory_pool_size (uint)
 * It is used to reserve gtt for smu debug usage, setting value 0 to disable it. The actual size is value * 256MiB.
 * E.g. 0x1 = 256Mbyte, 0x2 = 512Mbyte, 0x4 = 1 Gbyte, 0x8 = 2GByte. The default is 0 (disabled).
 */
MODULE_PARM_DESC(smu_memory_pool_size,
	"reserve gtt for smu debug usage, 0 = disable,"
		"0x1 = 256Mbyte, 0x2 = 512Mbyte, 0x4 = 1 Gbyte, 0x8 = 2GByte");
module_param_named(smu_memory_pool_size, amdgpu_smu_memory_pool_size, uint, 0444);

/**
 * DOC: async_gfx_ring (int)
 * It is used to enable gfx rings that could be configured with different prioritites or equal priorities
 */
MODULE_PARM_DESC(async_gfx_ring,
	"Asynchronous GFX rings that could be configured with either different priorities (HP3D ring and LP3D ring), or equal priorities (0 = disabled, 1 = enabled (default))");
module_param_named(async_gfx_ring, amdgpu_async_gfx_ring, int, 0444);

/**
 * DOC: mcbp (int)
 * It is used to enable mid command buffer preemption.
 * (0 = disabled, 1 = enabled based on priority, 2 = enabled based on quantum timer (default))
 */
MODULE_PARM_DESC(mcbp,
	"Enable Mid-command buffer preemption (0 = disabled, 1 = enabled and based on priority, 2 = enabled and based on quantum timer (default))");
module_param_named(mcbp, amdgpu_mcbp, int, 0444);

/**
 * DOC: discovery (int)
 * Allow driver to discover hardware IP information from IP Discovery table at the top of VRAM.
 * (-1 = auto (default), 0 = disabled, 1 = enabled)
 */
MODULE_PARM_DESC(discovery,
	"Allow driver to discover hardware IPs from IP Discovery table at the top of VRAM");
module_param_named(discovery, amdgpu_discovery, int, 0444);

/**
 * DOC: noretry (int)
 * Disable retry faults in the GPU memory controller.
 * (0 = retry enabled, 1 = retry disabled, -1 auto (default))
 */
MODULE_PARM_DESC(noretry,
	"Disable retry faults (0 = retry enabled, 1 = retry disabled, -1 auto (default))");
module_param_named(noretry, amdgpu_noretry, int, 0644);

/**
 * DOC: force_chip_rev (uint)
 * A non zero value used to specify GRBM chip revision for all supported GPUs.
 */
MODULE_PARM_DESC(force_chip_rev,
	"A non zero value used to specify GRBM chip revision for all supported GPUs");
module_param_named(force_chip_rev, sgpu_force_chip_rev, uint, 0444);

/**
 * DOC: cwsr_enable (int)
 * CWSR(compute wave store and resume) allows the GPU to preempt shader
 * execution in the middle of a compute wave. 1: enable this
 * feature. 0: disables it.
 */
int cwsr_enable = 0;
module_param(cwsr_enable, int, 0444);
MODULE_PARM_DESC(cwsr_enable, "CWSR enable (0 = Off, 1 = On (Default))");

/**
 * DOC: fence_poll_eop (int)
 * Poll gfx/compute eop instead of interrpt driven.
 * For bringup and emulation purpose, only valid for gfx10.
 * Default value, 0, use eop interrupt, not poll.
 * Setting 1 enables poll.
 */
int amdgpu_poll_eop = 0;
module_param_named(poll_eop, amdgpu_poll_eop, int, 0644);
MODULE_PARM_DESC(poll_eop, "Poll gfx/compute EOP fence instead of interrupt (0 = off (default), 1 = on)");

/**
 * DOC: tmz (int)
 * Trusted Memory Zone (TMZ) is a method to protect data being written
 * to or read from memory.
 *
 * The default value: 0 (off).  TODO: change to auto till it is completed.
 */
MODULE_PARM_DESC(tmz, "Enable TMZ feature (-1 = auto, 0 = off (default), 1 = on)");
module_param_named(tmz, amdgpu_tmz, int, 0444);

/**
 * DOC: reset_method (int)
 * GPU reset method (-1 = auto (default), 0 = legacy, 1 = mode0, 2 = mode1, 3 = mode2, 4 = baco)
 */
MODULE_PARM_DESC(reset_method, "GPU reset method (-1 = auto (default), 0 = legacy, 1 = mode0, 2 = mode1, 3 = mode2, 4 = baco)");
module_param_named(reset_method, amdgpu_reset_method, int, 0444);

MODULE_PARM_DESC(num_kcq, "number of kernel compute queue user want to setup (8 if set to greater than 8 or less than 0, only affect gfx 8+)");
module_param_named(num_kcq, amdgpu_num_kcq, int, 0444);

int sgpu_no_timeout = 0; //[Default 0]
 /**
 * DOC: no_timeout (int)
 * Set value to 1 to turn off all timers. The default is 0 (disabled).
 */
MODULE_PARM_DESC(no_timeout, "no timeout mode, (1 = enable, 0 = disable)");
module_param_named(no_timeout, sgpu_no_timeout, int, 0444);

int sgpu_force_ifh = 0;
/**
 * DOC: force_ifh (int)
 * Force to use infinitely fast hardware (IFH) mode
 */
MODULE_PARM_DESC(force_ifh, "Force to use IFH mode for all contexts "
		 "(0 = off (default), 1 = on)");
module_param_named(force_ifh, sgpu_force_ifh, int, 0444);

int sgpu_no_hw_access = 0;
/**
 * DOC: no_hw_access (int)
 * Set value 1 to block all hw access. This is only needed when running on no hardware environment. The default is 0 (disabled).
 */
MODULE_PARM_DESC(no_hw_access, "No HW Access mode, (1 = enable, 0 = disable)");
module_param_named(no_hw_access, sgpu_no_hw_access, int, 0444);

int sgpu_crash_on_exception = 0;
/**
 * DOC: crash_on_exception (int)
 * In system crash circumstance, avoid system from recovering GPU for debugging analysis.
 * Set input as summation of each params e.g)) 5 = 1(gfx jobtimeout) + 4(page fault)
 */
MODULE_PARM_DESC(crash_on_exception, "Forcing system reset on exceptions for analysis "
		 "(1 = gfx jobtimeout, 2 = cmp jobtimeout, 4 = page fault)");
module_param_named(crash_on_exception, sgpu_crash_on_exception, int, 0444);

int sgpu_wf_lifetime_enable = 0;
/**
 * DOC: sgpu_wf_lifetime_enable (int)
 * It is used to enable hang detection using the spi wf lifetime.
 */
MODULE_PARM_DESC(wf_lifetime_enable, "wf_lifetime_enable, (1 = enable, 0 = disable)");
module_param_named(wf_lifetime_enable, sgpu_wf_lifetime_enable, int, 0444);

int sgpu_wf_lifetime_limit = 1953130;
/**
 * DOC: sgpu_wf_lifetime_limit (int)
 * This limit value is for hang detection using the spi wf lifetime.
 * If there's a lifetime count that exceeds this limit value,
 * spi interrupt occurs and handle that.
 * This value is for SPI_WF_LIFETIME_LIMIT_0
 */
MODULE_PARM_DESC(wf_lifetime_limit, "wf_lifetime_limit, (1953130 = 10s at 200MHz (default))");
module_param_named(wf_lifetime_limit, sgpu_wf_lifetime_limit, int, 0644);

int sgpu_uswc_override = 0;
/**
 * DOC: uswc_override(int)
 * Set value 1 to remove CPU_GTT_USWC flag from all user BOs.
 */
MODULE_PARM_DESC(uswc_override, "uswc_override, (0 = disable (default), 1 = enable)");
module_param_named(uswc_override, sgpu_uswc_override, int, 0644);

#ifdef CONFIG_DRM_SGPU_BPMD
/**
 * DOC: bpmdpath (charp)
 * Path to BPMD file
 */
MODULE_PARM_DESC(bpmdpath, "Path to BPMD");

/**
 * Validate the supplied bpmdpath by removing trailing slashes
 * and whitespace
 * */
static int set_bpmd_path(const char *val, const struct kernel_param *kp){

	int new_len, old_len, ret;
	char *path = (char *)kmalloc(strlen(val)+1, GFP_KERNEL);

	if(!path) {
		return -ENOMEM;
	}

	strncpy(path, val, strlen(val)+1);

	// Looping until all trailing slashes and whitespace are removed
	do {

		old_len = strlen(path);

		// Remove the trailing whitespace
		path = strim(path);

		// Remove slash if there's a slash at the end of the new path
		new_len = strlen(path);
		if(path[new_len-1] == '/') {
			path[--new_len] = 0;
		}

	} while(old_len != new_len);

	// Write to variable
	ret = param_set_charp(path, kp);

	kfree(path);

	return ret;
}

static const struct kernel_param_ops bpmd_path_ops = {
	.set = set_bpmd_path,
	.get = param_get_charp,
};


module_param_cb(bpmdpath, &bpmd_path_ops, &sgpu_bpmd_path,
		S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH);

#endif	/* CONFIG_DRM_SGPU_BPMD */

/**
 * DOC: hang_detect (int)
 * GPU hang detection is a method to detect GPU hang by
 * monitoring GPU wave PC register activity at fixed sample interval
 *
 * The default value: 0 (off)
 */
int sgpu_hang_detect = 0;
MODULE_PARM_DESC(hang_detect,
		"Enable GPU Hang Detection feature.\n"
		"0 : disable GPU hang Detection (Default)\n"
		"1 : enable GPU hang Detection\n");
module_param_named(hang_detect, sgpu_hang_detect, int, 0444);

/**
 * DOC: fault_detect (int)
 * GPU fault detection is a method to detect GPU fault by
 * monitoring GPU register activity at fixed sample interval
 *
 * The default value: 0 (off)
 */
int amdgpu_fault_detect = 0;
MODULE_PARM_DESC(fault_detect,
		"Enable GPU Fault Detection feature.\n"
		"0 : disable GPU Fault Detection (Default)\n"
		"1 : enable GPU Fault Detection\n");
module_param_named(fault_detect, amdgpu_fault_detect, int, 0444);

/**
 * DOC: amdgpu_sws_quantum (int)
 * Set quantum granularity for each round SWS scheduling in ms.
 * The default value: -1 (use default setting)
 */
MODULE_PARM_DESC(sws_quantum,
		 "-1 : use default setting\n");
module_param_named(sws_quantum, amdgpu_sws_quantum, int, 0444);

/**
 * DOC: sgpu_gvf_window_ms (int)
 * Set SGPU GVF monitoring window in ms.
 * The default value: 10000 (10sec)
 */
int sgpu_gvf_window_ms = 10000;
MODULE_PARM_DESC(gvf_window_ms,
		 "Set SGPU GVF monitoring window in ms (default 10sec)\n");
module_param_named(gvf_window_ms, sgpu_gvf_window_ms, int, 0444);

/**
 * DOC: sgpu_devfreq_polling_ms (int)
 * Set sgpu devfreq polling_time in ms.
 * The default value: 8
 */
int sgpu_devfreq_polling_ms = 8;
MODULE_PARM_DESC(devfreq_polling_ms,
		 "Set sgpu devfreq polling_time in ms (default 8ms)\n");
module_param_named(devfreq_polling_ms, sgpu_devfreq_polling_ms, int, 0444);
/**
 * DOC: sgpu_gfx_timeout (int)
 * Set SGPU GFX timeout value in ms.
 * The default value:  (2.5sec)
 */
int sgpu_gfx_timeout = 2500;
MODULE_PARM_DESC(gfx_timeout,
		 "Set SGPU GFX timedout value in ms (default 2.5sec)\n");
module_param_named(gfx_timeout, sgpu_gfx_timeout, int, 0444);

static struct drm_driver kms_driver;

static int amdgpu_pmops_suspend(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(drm_dev);
	uint32_t i, ret;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "pmops suspend start");

	device_set_wakeup_path(dev);

	amdgpu_cancel_all_tdr(adev);
	cancel_delayed_work_sync(&adev->page_fault_work);
	cancel_delayed_work_sync(&adev->hang_detect_work);
	if (unlikely(!amdgpu_device_lock_adev(adev, NULL)))
		DRM_ERROR("still recovery state %d\n", amdgpu_in_reset(adev));

	for (i = 0; i < adev->num_rings; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;

		drm_sched_stop(&ring->sched, NULL);
	}

	sgpu_worktime_cleanup(adev);

	if (adev->in_runpm)
		return 0;

	ret = amdgpu_device_suspend(drm_dev, false);
	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "pmops suspend end");

	return ret;
}

static int amdgpu_pmops_resume(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(drm_dev);
	uint32_t i, ret = 0;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "pmops resume start");

	if (!adev->in_runpm) {
		ret = amdgpu_device_resume(drm_dev, false);
		if (ret)
			return ret;
	}

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;

		if (!adev->in_runpm)
			drm_sched_resubmit_jobs(&ring->sched);
		drm_sched_start(&ring->sched, true);
	}

	amdgpu_device_unlock_adev(adev);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "pmops resume end");

	return ret;
}

static int amdgpu_pmops_freeze(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(drm_dev);
	int r;

	adev->in_hibernate = true;
	r = amdgpu_device_suspend(drm_dev, true);
	adev->in_hibernate = false;
	if (r)
		return r;
	return amdgpu_asic_reset(adev);
}

static int amdgpu_pmops_thaw(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return amdgpu_device_resume(drm_dev, true);
}

static int amdgpu_pmops_poweroff(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return amdgpu_device_suspend(drm_dev, true);
}

static int amdgpu_pmops_restore(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return amdgpu_device_resume(drm_dev, true);
}

static int amdgpu_pmops_runtime_suspend(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(drm_dev);
	int ret, i;

	if (!adev->runpm) {
		pm_runtime_forbid(dev);
		return -EBUSY;
	}

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "runtime suspend start");
	/* wait for all rings to drain before suspending */
	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];
		if (ring && ring->sws_ctx.queue_state == AMDGPU_SWS_QUEUE_ENABLED) {
                  	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
                                 "runtime suspend aborted: queue mapped");
			return -EBUSY;
		}
		if (ring && ring->sched.ready) {
			ret = amdgpu_fence_wait_empty(ring);
			if (ret)
				return -EBUSY;
		}
	}

	adev->in_runpm = true;

	if (amdgpu_device_supports_boco(drm_dev))
		drm_dev->switch_power_state = DRM_SWITCH_POWER_CHANGING;

	ret = amdgpu_device_suspend(drm_dev, false);
	if (ret)
		return ret;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "runtime suspend end");
	return 0;
}

static int amdgpu_pmops_runtime_resume(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(drm_dev);
	int ret;

	if (!adev->runpm)
		return -EINVAL;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "runtime resume start");

	ret = amdgpu_device_resume(drm_dev, false);

	if (amdgpu_device_supports_boco(drm_dev))
		drm_dev->switch_power_state = DRM_SWITCH_POWER_ON;
	adev->in_runpm = false;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "runtime resume end");

	return 0;
}

static int amdgpu_pmops_runtime_idle(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(drm_dev);
	/* we don't want the main rpm_idle to call suspend - we want to autosuspend */
	int ret = 1;
	struct drm_connector *list_connector;
	struct drm_connector_list_iter iter;

	if (!adev->runpm) {
		pm_runtime_forbid(dev);
		return -EBUSY;
	}

	mutex_lock(&drm_dev->mode_config.mutex);
	drm_modeset_lock(&drm_dev->mode_config.connection_mutex, NULL);

	drm_connector_list_iter_begin(drm_dev, &iter);
	drm_for_each_connector_iter(list_connector, &iter) {
		if (list_connector->dpms ==  DRM_MODE_DPMS_ON) {
			ret = -EBUSY;
			break;
		}
	}

		drm_connector_list_iter_end(&iter);

		drm_modeset_unlock(&drm_dev->mode_config.connection_mutex);
		mutex_unlock(&drm_dev->mode_config.mutex);

	if (ret == -EBUSY)
		DRM_DEBUG_DRIVER("failing to power off - crtc active\n");

	if (adev->runpm) {
		pm_runtime_mark_last_busy(dev);
		pm_runtime_autosuspend(dev);
	}

	return ret;
}

long amdgpu_drm_ioctl(struct file *filp,
		      unsigned int cmd, unsigned long arg)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev;
	struct amdgpu_device *adev;
	long ret;
	dev = file_priv->minor->dev;
	adev = drm_to_adev(dev);

	if (adev->runpm) {
		trace_pm_runtime_get_sync_start(0);
		ret = pm_runtime_get_sync(dev->dev);
		trace_pm_runtime_get_sync_end(0);

		if (ret < 0)
			goto out;
	}

	if (trace_amdgpu_drm_ioctl_enabled()) {
		int index = DRM_IOCTL_NR(cmd) - DRM_COMMAND_BASE;
		if (index >= 0 && index < DRM_COMMAND_END - DRM_COMMAND_BASE &&
		    index < kms_driver.num_ioctls)
			trace_amdgpu_drm_ioctl(&kms_driver.ioctls[index]);
	}

	ret = drm_ioctl(filp, cmd, arg);

	if (adev->runpm)
		pm_runtime_mark_last_busy(dev->dev);
out:
	if (adev->runpm) {
		trace_pm_runtime_put_autosuspend_start(0);
		pm_runtime_put_autosuspend(dev->dev);
		trace_pm_runtime_put_autosuspend_end(0);
	}
	return ret;
}

static const struct dev_pm_ops amdgpu_pm_ops = {
	.suspend = amdgpu_pmops_suspend,
	.resume = amdgpu_pmops_resume,
	.freeze = amdgpu_pmops_freeze,
	.thaw = amdgpu_pmops_thaw,
	.poweroff = amdgpu_pmops_poweroff,
	.restore = amdgpu_pmops_restore,
	.runtime_suspend = amdgpu_pmops_runtime_suspend,
	.runtime_resume = amdgpu_pmops_runtime_resume,
	.runtime_idle = amdgpu_pmops_runtime_idle,
};

static const struct file_operations amdgpu_driver_kms_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = amdgpu_drm_ioctl,
	.mmap = drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amdgpu_kms_compat_ioctl,
#endif
};

int amdgpu_file_to_fpriv(struct file *filp, struct amdgpu_fpriv **fpriv)
{
        struct drm_file *file;

	if (!filp)
		return -EINVAL;

	if (filp->f_op != &amdgpu_driver_kms_fops) {
		return -EINVAL;
	}

	file = filp->private_data;
	*fpriv = file->driver_priv;
	return 0;
}

static struct drm_driver kms_driver = {
	.driver_features =
	    DRIVER_GEM |
	    DRIVER_RENDER | DRIVER_MODESET | DRIVER_SYNCOBJ |
	    DRIVER_SYNCOBJ_TIMELINE,
	.open = amdgpu_driver_open_kms,
	.postclose = amdgpu_driver_postclose_kms,
	.lastclose = NULL,
	.debugfs_init = amdgpu_debugfs_init,
	.ioctls = amdgpu_ioctls_kms,
	.dumb_create = amdgpu_mode_dumb_create,
	.dumb_map_offset = amdgpu_mode_dumb_mmap,
	.fops = &amdgpu_driver_kms_fops,

	.gem_prime_import = amdgpu_gem_prime_import,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = KMS_DRIVER_MAJOR,
	.minor = KMS_DRIVER_MINOR,
	.patchlevel = KMS_DRIVER_PATCHLEVEL,
};

static const struct of_device_id sgpu_match[] = {
	{ .compatible = "samsung-sgpu,samsung-sgpu", },
	{},
};

static int
sgpu_kms_probe(struct platform_device *pdev)
{
	struct drm_device *ddev;
	struct amdgpu_device *adev;
	int ret, retry = 0;

	adev = devm_drm_dev_alloc(&pdev->dev, &kms_driver, typeof(*adev), ddev);
	if (IS_ERR(adev))
		return PTR_ERR(adev);

	adev->dev  = &pdev->dev;
	adev->pldev = pdev;
	ddev = adev_to_drm(adev);
	dev_set_drvdata(&pdev->dev, ddev);

	ret = sgpu_dmsg_init(adev);
	if (ret)
		return ret;

	if (sgpu_force_chip_rev == 0) {
		ret = of_property_read_u32(pdev->dev.of_node, "chip_revision",
			&adev->grbm_chip_rev);
		if (ret) {
			adev->grbm_chip_rev = GRBM_CHIP_REV_DEFAULT;
			DRM_WARN("Field chip_revision is missing in a device tree, "
				"using default (0x%08x)\n", adev->grbm_chip_rev);
		} else {
			DRM_INFO("Using chip_revision %x from device tree", adev->grbm_chip_rev);
		}
	} else {
		adev->grbm_chip_rev = sgpu_force_chip_rev;
		DRM_INFO("Forcing chip_revision %x from module parameter", sgpu_force_chip_rev);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "gl2acem_instances_count",
		&adev->gl2acem_instances_count);
	if (ret) {
		DRM_ERROR("failed to read number of acem instances\n");
		return ret;
	}

#if defined(CONFIG_DRM_SGPU_EXYNOS) && IS_ENABLED(CONFIG_CAL_IF)
	ret = of_property_read_u32(pdev->dev.of_node, "g3d_cmu_cal_id",
				   &adev->cal_id);
	if (ret) {
		DRM_INFO("sgpu cmu_cal_id is not defind. No GPU DVFS\n");
	}
#endif

	adev->device_id = 0x73A0;

	sgpu_sysfs_init(adev);
	ret = amdgpu_driver_load_kms(adev);
	if (ret)
		goto err_kms_uninit;

	sgpu_debug_init(adev);
retry_init:
	ret = drm_dev_register(ddev, 0);
	if (ret == -EAGAIN && ++retry <= 3) {
		DRM_INFO("retry init %d\n", retry);
		/* Don't request EX mode too frequently which is attacking */
		msleep(5000);
		goto retry_init;
	} else if (ret)
		goto err_kms_uninit;

	return 0;

err_kms_uninit:
	sgpu_sysfs_deinit(adev);
	return ret;
}

static int
sgpu_kms_remove(struct platform_device *pdev)
{
	struct drm_device *drm = dev_get_drvdata(&pdev->dev);

	sgpu_sysfs_deinit(drm_to_adev(drm));
	drm_dev_unregister(drm);
	dev_set_drvdata(&pdev->dev, NULL);
	drm_dev_put(drm);
	return 0;
}
static struct platform_driver sgpu_kms_driver = {
	.probe = sgpu_kms_probe,
	.remove = sgpu_kms_remove,
	.driver = {
		.name = "sgpu",
		.of_match_table = of_match_ptr(sgpu_match),
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
		.pm = &amdgpu_pm_ops,
	},
	.prevent_deferred_probe = true,
};

static int __init amdgpu_init(void)
{
	int r;

	r = amdgpu_sync_init();
	if (r)
		goto error_sync;

	r = amdgpu_fence_slab_init();
	if (r)
		goto error_fence;

	kms_driver.num_ioctls = amdgpu_max_kms_ioctl;

	if (sgpu_force_ifh)
		DRM_INFO("WARNING: forced IFH mode enabled!\n");

	if (sgpu_no_hw_access)
		DRM_INFO("WARNING: forced no hardware access!\n");

	/* let modprobe override vga console setting */
	return platform_driver_register(&sgpu_kms_driver);

error_fence:
	amdgpu_sync_fini();

error_sync:
	return r;
}

static void __exit amdgpu_exit(void)
{
	platform_driver_unregister(&sgpu_kms_driver);
	amdgpu_sync_fini();
	amdgpu_fence_slab_fini();
	mmu_notifier_synchronize();
}

module_init(amdgpu_init);
module_exit(amdgpu_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
