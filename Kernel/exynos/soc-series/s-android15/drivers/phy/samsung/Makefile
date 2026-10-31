# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_PHY_EXYNOS_MIPI_DSIM)      += phy-exynos-mipi-dsim.o
obj-$(CONFIG_PHY_EXYNOS_MIPI)      	+= phy-exynos-mipi.o
ifneq ($(CONFIG_PHY_EXYNOS_USBDRD_EUSB),)
obj-$(CONFIG_PHY_EXYNOS_USBDRD_EUSB)	+= phy-exynos-usbdrd-super.o
phy-exynos-usbdrd-super-y		+= phy-exynos-usbdrd-eusb.o
phy-exynos-usbdrd-super-y		+= phy-exynos-eusb.o
phy-exynos-usbdrd-super-y		+= exynos-usb-blkcon.o
phy-exynos-usbdrd-super-y		+= phy-exynos-snps-usbdp.o
phy-exynos-usbdrd-super-y		+= phy-exynos-usbdrd-common.o
ifneq ($(CONFIG_PHY_EXYNOS_EUSB_REPEATER),)
obj-$(CONFIG_PHY_EXYNOS_EUSB_REPEATER)	+= eusb_repeater.o
else
ifneq ($(CONFIG_USB_REPEATER),)
obj-$(CONFIG_PHY_SUPPORT_SEC_REPEATER_CB)		+= sec_repeater_cb.o
endif
endif
else
ifneq ($(CONFIG_PHY_EXYNOS_USBDRD),)
obj-$(CONFIG_PHY_EXYNOS_USBDRD)		+= phy-exynos-usbdrd-super.o
phy-exynos-usbdrd-super-y		+= phy-exynos-usbdrd.o
phy-exynos-usbdrd-super-y      		+= phy-exynos-usb3p1.o
phy-exynos-usbdrd-super-y		+= phy-exynos-usbdrd-common.o
endif
endif

obj-$(CONFIG_EXYNOS_PHYCAL_IF)		+= exynos-phycal-if.o
