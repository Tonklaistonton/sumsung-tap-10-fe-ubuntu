# SPDX-License-Identifier: GPL-2.0
#
# Makefile for ALSA
#

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_MODULE
subdir-ccflags-y	+= -I${ROOT_DIR}/${KERNEL_DIR}/sound \
			   -I${ROOT_DIR}/${KERNEL_DIR}/sound/usb \
			   -I${ROOT_DIR}/${KERNEL_DIR}/include/linux
#endif

obj-$(CONFIG_SND_EXYNOS_USB_AUDIO_MODULE) += exynos-usb-audio-offloading.o
exynos-usb-audio-offloading-y += exynos_usb_audio_gic.o
