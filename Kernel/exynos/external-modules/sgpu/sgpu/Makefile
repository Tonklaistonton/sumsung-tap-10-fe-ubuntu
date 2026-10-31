#
# Copyright 2017-2021 Advanced Micro Devices, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

FULL_SGPU_PATH=$(srctree)/$(src)/..

ccflags-y := -I$(FULL_SGPU_PATH)/include/asic_reg \
	-I$(FULL_SGPU_PATH)/include \
	-I$(FULL_SGPU_PATH)/sgpu \
	-I$(FULL_SGPU_PATH)/pm/inc \
	-I$(KERNEL_SRC)/drivers/devfreq

sgpu-y := amdgpu_drv.o

# add KMS driver
sgpu-y += amdgpu_device.o amdgpu_kms.o \
	amdgpu_fence.o amdgpu_ttm.o amdgpu_object.o amdgpu_gart.o \
	sgpu_ttm_alloc.o amdgpu_gem.o amdgpu_ring.o \
	amdgpu_cs.o \
	amdgpu_afmt.o amdgpu_trace_points.o \
	amdgpu_sa.o \
	amdgpu_dma_buf.o amdgpu_vm.o amdgpu_ib.o \
	amdgpu_ucode.o amdgpu_bo_list.o amdgpu_ctx.o amdgpu_sync.o \
	amdgpu_gtt_mgr.o \
	amdgpu_sched.o amdgpu_debugfs.o amdgpu_ids.o \
	amdgpu_gmc.o amdgpu_csa.o amdgpu_vm_cpu.o \
	amdgpu_discovery.o

sgpu-$(CONFIG_PERF_EVENTS) += amdgpu_pmu.o
sgpu-$(CONFIG_DEBUG_FS) += sgpu_debugfs.o

sgpu-y += \
	soc15.o emu_soc.o \
	nbio_v2_3.o nv.o \
	vangogh_lite_reg_init.o \
	vangogh_lite_gc.o

# add GMC block
sgpu-y += \
	mmhub_v2_0.o gmc_v10_0.o gfxhub_v2_1.o

# add IH block
sgpu-y += \
	amdgpu_irq.o \
	amdgpu_ih.o \
	vangogh_lite_ih.o

# add GFX block
sgpu-y += \
	amdgpu_gfx.o \
	amdgpu_rlc.o \
	gfx_v10_0.o \
	amdgpu_cwsr.o \
	amdgpu_tmz.o \
	amdgpu_sws.o

# add ATHUB block
sgpu-y += \
	athub_v2_0.o

# GPU scheduler
sgpu-y += amdgpu_job.o

sgpu-$(CONFIG_COMPAT) += amdgpu_ioc32.o

sgpu-y += $(AMD_POWERPLAY_FILES)

sgpu-$(CONFIG_DRM_SGPU_BPMD) += \
	sgpu_bpmd.o \
	sgpu_bpmd_layout_common.o \
	sgpu_bpmd_layout.o

sgpu-$(CONFIG_DRM_SGPU_BPMD_MEMLOGGER_TEXT) += \
	sgpu_bpmd_log.o

# add pm dvfs block
sgpu-$(CONFIG_DRM_SGPU_DVFS) += \
	sgpu_utilization.o sgpu_governor.o sgpu_user_interface.o sgpu_devfreq.o

# add Vangogh lite hw counter
sgpu-y += \
	vangogh_lite_hw_counter.o

# sgpu debug information
sgpu-$(CONFIG_DRM_SGPU_EXYNOS) += sgpu_debug.o sgpu_sysfs.o

# add afm driver
sgpu-$(CONFIG_DRM_SGPU_AFM) += sgpu_afm.o

# add Exynos interface api
sgpu-$(CONFIG_DRM_SGPU_DVFS) += exynos_gpu_interface.o
sgpu-$(CONFIG_DRM_SGPU_GRAPHIC_MEMORY_RECLAIM) += sgpu_swap.o

# add GPU Profiler api
ifeq ($(AMIGO_VER), )
ifdef CONFIG_SOC_S5E8535
	ccflags-y += -DAMIGO_BUILD_VER=3
	AMIGO_VER=3
else
	ccflags-y += -DAMIGO_BUILD_VER=4
	AMIGO_VER=4
endif
else
	ccflags-y += -DAMIGO_BUILD_VER=$(AMIGO_VER)
endif

# add IFPO functions
sgpu-$(CONFIG_DRM_SGPU_EXYNOS) += sgpu_ifpo.o

# add pm monitor functions
sgpu-$(CONFIG_DRM_SGPU_EXYNOS) += sgpu_pm_monitor.o

# add SGPU Debug messages
sgpu-y += sgpu_dmsg.o

# add gpu_work_period tracepoint
sgpu-$(CONFIG_DRM_SGPU_WORKTIME) += sgpu_worktime.o

# add sgpu_power_tracepoint
sgpu-y += sgpu_power_trace_points.o

sgpu-$(CONFIG_DRM_SGPU_GVF) += sgpu_gvf.o sgpu_gvf_governor.o

obj-$(CONFIG_DRM_SGPU)+= sgpu.o
