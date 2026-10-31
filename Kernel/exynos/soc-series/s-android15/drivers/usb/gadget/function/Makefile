# SPDX-License-Identifier: GPL-2.0

ccflags-y	:="-I${ROOT_DIR}/${KERNEL_DIR}/drivers/usb/gadget"
ccflags-y	+="-I${ROOT_DIR}/${KERNEL_DIR}/drivers/usb/gadget/udc"
ccflags-y	+="-I${ROOT_DIR}/${KERNEL_DIR}/drivers/usb/gadget/function"

usb_f_rndis_mp-y		:= f_rndis_mp.o u_ether_mp.o rndis_mp.o
obj-$(CONFIG_USB_F_RNDIS_MP)	+= usb_f_rndis_mp.o
usb_f_dm-y			:= f_dm.o
obj-$(CONFIG_USB_CONFIGFS_F_DM) += usb_f_dm.o
