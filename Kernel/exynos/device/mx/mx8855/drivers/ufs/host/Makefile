# drivers/ufs/host/Makefile
obj-$(CONFIG_SEC_UFS_FEATURE) += ufs-sec-driver.o
ufs-sec-driver-$(CONFIG_SEC_UFS_FEATURE) += ufs-sec-misc.o

subdir-ccflags-y += $(UFS_DEFS) \
                    -I${ROOT_DIR}/${KERNEL_DIR}/drivers/ufs/core \
                    -I${ROOT_DIR}/${EXYNOS_SOC_DIR}/drivers/ufs/host \
                    -I${ROOT_DIR}/${EXYNOS_SOC_DIR}/drivers/ufs/host/${EXYNOS_SOC} 
