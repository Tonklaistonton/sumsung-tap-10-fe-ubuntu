# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
clean-files := built-in.o

subdir-ccflags-y                        += -I$(srctree)/$(src)                  \
                                           -I$(srctree)/$(src)/include		\
					   -I${ROOT_DIR}/${KERNEL_DIR}/include
ifeq ($(CONFIG_SOC_S5E9955),y)
obj-$(CONFIG_SCSC_WLAN) += pcie_scsc/
endif
ifeq ($(CONFIG_SOC_S5E8855),y)
obj-$(CONFIG_SCSC_WLAN) += scsc/
endif

