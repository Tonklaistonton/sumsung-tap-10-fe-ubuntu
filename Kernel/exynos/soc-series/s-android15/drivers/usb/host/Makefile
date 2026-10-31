# SPDX-License-Identifier: GPL-2.0
#
# Makefile for USB Host Controller Drivers
#

# tell define_trace.h where to find the xhci trace header

subdir-ccflags-y	+= -I${ROOT_DIR}/${KERNEL_DIR}/drivers/usb/host \
			   -I${ROOT_DIR}/${KERNEL_DIR}/include/linux

obj-$(CONFIG_USB_XHCI_EXYNOS)	+= xhci-exynos.o xhci-exynos-audio.o
