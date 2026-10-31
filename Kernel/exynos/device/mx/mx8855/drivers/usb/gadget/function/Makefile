# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
# USB Functions
usb_f_conn_gadget-y             := f_conn_gadget.o
obj-$(CONFIG_USB_F_CONN_GADGET) += usb_f_conn_gadget.o
usb_f_ss_mon_gadget-y             := f_ss_mon_gadget.o
obj-$(CONFIG_USB_F_SS_MON_GADGET) += usb_f_ss_mon_gadget.o

subdir-ccflags-y += -I${ROOT_DIR}/${KERNEL_DIR}/drivers/usb/gadget/function
