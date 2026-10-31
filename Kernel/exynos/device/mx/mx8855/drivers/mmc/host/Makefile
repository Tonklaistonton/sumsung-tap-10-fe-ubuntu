# drivers/mmc/host/Makefile
obj-$(CONFIG_SEC_MMC_FEATURE) += mmc-sec-driver.o
mmc-sec-driver-$(CONFIG_SEC_MMC_FEATURE) += mmc-sec-misc.o

subdir-ccflags-y += $(MMC_DEFS) \
                    -I${ROOT_DIR}/${KERNEL_DIR}/drivers/mmc/core \
                    -I${ROOT_DIR}/${EXYNOS_SOC_DIR}/drivers/mmc/host \
