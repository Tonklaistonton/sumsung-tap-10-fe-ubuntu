# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.
#

ifdef CONFIG_SOC_S5E9955
cal-dir := cal_sl
endif

ifdef CONFIG_SOC_S5E9945
cal-dir := cal_rt
endif

ifdef CONFIG_SOC_S5E8845
cal-dir := cal_rs
endif

ifdef CONFIG_SOC_S5E8855
cal-dir := cal_st
endif

subdir-ccflags-y			:= -I$(srctree)/$(src)			\
					   -I$(srctree)/$(src)/include		\
					   -I$(srctree)/$(src)/cal_common	\
					   -I$(srctree)/$(src)/displayport	\
					   -I$(srctree)/$(src)/$(cal-dir)

ifneq ($(filter y m,$(CONFIG_DRM_PANEL_MCD_COMMON)),)
subdir-ccflags-y		+= -I$(srctree)/$(src)/mcd_cal
endif

ifdef CONFIG_SAMSUNG_TUI
subdir-ccflags-y += -I$(srctree)/exynos/device/mx/mx8855/drivers/tee/tui
endif

exynos-drm-y := exynos_drm_drv.o  exynos_drm_fb.o exynos_drm_format.o		\
		exynos_drm_crtc.o exynos_drm_plane.o exynos_drm_encoder.o 	\
		exynos_drm_connector.o exynos_drm_debug.o exynos_drm_gem.o	\
		exynos_drm_tui.o exynos_drm_partial.o				\
		exynos_drm_recovery.o exynos_drm_freq_hop.o exynos_drm_dsc.o	\
		exynos_drm_hibernation.o exynos_drm_sfr_dma.o			\
		exynos_drm_bridge.o exynos_drm_self_refresh.o	\
		exynos_drm_hwapg.o

ifneq ($(filter y m,$(CONFIG_DRM_PANEL_MCD_COMMON)),)
exynos-drm-y += mcd_cal/mcd_drm_dsim.o mcd_cal/mcd_drm_helper.o
endif
exynos-drm-$(CONFIG_DRM_SAMSUNG_DECON)	+= exynos_drm_decon.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DPP)	+= exynos_drm_dpp.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DSI)	+= exynos_drm_dsim.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_WB)	+= exynos_drm_writeback.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DP)	+= displayport/exynos_drm_dp.o		\
					   displayport/exynos_drm_dp_edid.o	\
					   displayport/exynos_drm_dp_hdcp13.o	\
					   displayport/exynos_drm_dp_hdcp22_if.o\
					   $(cal-dir)/dp_reg.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_DQE)	+= exynos_drm_dqe.o			\
					   $(cal-dir)/dqe_reg.o
exynos-drm-$(CONFIG_DRM_SAMSUNG_HDR)	+= exynos_drm_hdr.o			\
					   $(cal-dir)/hdr_reg.o

exynos-drm-$(CONFIG_EXYNOS_DMA_DSIMFC)	+= exynos_drm_dsimfc.o			\
					   $(cal-dir)/dsimfc_reg.o

exynos-drm-$(CONFIG_EXYNOS_DSIM_SYNC_CMD)	+= exynos_drm_dsim_sync_cmd.o

exynos-drm-y				+= $(cal-dir)/decon_reg.o		\
					   $(cal-dir)/dsim_reg.o		\
					   $(cal-dir)/dpp_reg.o			\

exynos-drm-$(CONFIG_EXYNOS_VMC)		+= exynos_drm_vmc.o			\
					   $(cal-dir)/vmc_reg.o

exynos-drm-$(CONFIG_EXYNOS_BTS)		+= exynos_drm_bts.o

exynos-drm-$(CONFIG_EXYNOS_PROFILER_GPU)	+= exynos_drm_profiler.o

obj-$(CONFIG_DRM_SAMSUNG_DPU)		+= exynos-drm.o

obj-$(CONFIG_DRM_PANEL_SAMSUNG_COMMON)	+= panel/

obj-$(CONFIG_DPU_EXYNOS_KUNIT_TEST) 	+= tests/

