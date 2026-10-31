# SPDX-License-Identifier: GPL-2.0
# UFSHCD makefile
ifdef CONFIG_SCSI_UFS_SAMSUNG
subdir-ccflags-y += $(UFS_DEFS) \
		    -I${ROOT_DIR}/${KERNEL_DIR}/drivers/ufs/core \
	            -I${ROOT_DIR}/${KERNEL_DIR}/drivers/ufs/host \
	            -I${ROOT_DIR}/${EXYNOS_SOC_DIR}/drivers/ufs/host/${EXYNOS_SOC}
endif

obj-$(CONFIG_SCSI_UFS_SAMSUNG) += ufs-exynos-core.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_SAMSUNG) += ufs-exynos.o \
			$(EXYNOS_SOC)/ufs-cal-if.o $(EXYNOS_SOC)/ufs-cal-snr-if.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_PERF) += ufs-exynos-perf.o ufs-exynos-perf-v1.o ufs-exynos-gear.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_DBG) += ufs-exynos-dbg.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_EXYNOS_SRPMB) += ufs-exynos-srpmb.o
ufs-exynos-core-$(CONFIG_SCSI_UFS_EXYNOS_FMP) += ufs-exynos-fmp.o
ufs-exynos-core-$(CONFIG_SEC_UFS_FEATURE) += ufs-sec-feature.o ufs-sec-sysfs.o

obj-$(CONFIG_EXYNOS_UFS_KUNIT_TEST) += test/
