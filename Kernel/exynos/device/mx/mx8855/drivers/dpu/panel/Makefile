KCFLAGS += -I$(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/drivers/dpu

subdir-ccflags-y := -I$(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/drivers/dpu \
           -I$(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/drivers/dpu/include \
           -I$(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/drivers/dpu/cal_common \
           -I$(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/drivers/dpu/displayport \
           -I$(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/drivers/dpu/cal_st \
           -I$(KERNEL_SRC)/../${EXYNOS_SOC_DIR}/drivers/dpu/mcd_cal \
           -I$(KERNEL_SRC)/../exynos/device/mx/mx8855/drivers/gpu/drm/samsung/panel

obj-$(CONFIG_DRM_PANEL_MCD_COMMON) += mcd-panel-samsung-drv.o mcd-panel-samsung-helper.o
