# SPDX-License-Identifier: GPL-2.0

##
# Platform-specific glue layers go here
#
# NOTICE: Make sure your glue layer doesn't depend on anything
# which is arch-specific and that it compiles on all situations.
#
# We want to keep this requirement in order to be able to compile
# the entire driver (with all its glue layers) on several architectures
# and make sure it compiles fine. This will also help with allmodconfig
# and allyesconfig builds.
##

USB_HDRS="-I${ROOT_DIR}/${KERNEL_DIR}/drivers/usb/dwc3"
USB_LDRS="-I${ROOT_DIR}/${KERNEL_DIR}/include/linux"

ifdef CONFIG_USB_DWC3_EXYNOS_MODULE
subdir-ccflags-y += $(USB_HDRS)
subdir-ccflags-y += $(USB_LDRS)
endif

obj-$(CONFIG_USB_DWC3_EXYNOS_MODULE)		+= dwc3-exynos-usb.o
dwc3-exynos-usb-y			+= dwc3-exynos.o
dwc3-exynos-usb-y			+= dwc3-exynos-otg.o
dwc3-exynos-usb-y			+= dwc3_kret_ops.o
ifneq ($(CONFIG_USB_EXYNOS_TPMON_MODULE),)
	dwc3-exynos-usb-y			+= exynos_usb_tpmon.o
endif
obj-$(CONFIG_SEC_USB_CB)		+= sec_usb_cb.o
obj-$(CONFIG_USB_DEBUG_MODULE)		+= exynos_usb_debug.o
obj-$(CONFIG_EXYNOS_USB_KUNIT_TEST) += test/
