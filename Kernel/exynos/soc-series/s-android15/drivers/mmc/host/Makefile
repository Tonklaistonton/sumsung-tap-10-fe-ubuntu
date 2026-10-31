# SPDX-License-Identifier: GPL-2.0
#
# Makefile for MMC/SD host controller drivers
#

ifdef CONFIG_MMC_DW_SAMSUNG
subdir-ccflags-y += $(MMC_DEFS) \
	-I${ROOT_DIR}/${KERNEL_DIR}/drivers/mmc/core \
	-I${ROOT_DIR}/${KERNEL_DIR}/drivers/mmc/host
endif

obj-$(CONFIG_MMC_DW_SAMSUNG)		+= dw_mmc-samsung.o
dw_mmc-samsung-y			:= dw_mmc.o
obj-$(CONFIG_MMC_DW_PLTFM_SAMSUNG)	+= dw_mmc-pltfm-samsung.o
dw_mmc-pltfm-samsung-y			:= dw_mmc-pltfm.o
ifeq ($(CONFIG_SEC_MMC_FEATURE),m)
obj-$(CONFIG_MMC_DW_EXYNOS_SAMSUNG)	+= dw_mmc-exynos-samsung.o
dw_mmc-exynos-samsung-y			:= dw_mmc-exynos.o mmc-sec-feature.o mmc-sec-sysfs.o
else
obj-$(CONFIG_MMC_DW_EXYNOS_SAMSUNG)	+= dw_mmc-exynos-samsung.o
dw_mmc-exynos-samsung-y			:= dw_mmc-exynos.o
endif
