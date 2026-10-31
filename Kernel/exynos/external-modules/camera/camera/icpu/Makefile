pablo-icpu-objs := pablo-icpu-core.o
pablo-icpu-objs += pablo-icpu-itf.o
pablo-icpu-objs += mbox/pablo-icpu-mbox.o
pablo-icpu-objs += pablo-icpu-selftest.o
pablo-icpu-objs += pablo-icpu-debug.o
pablo-icpu-objs += pablo-icpu-msg-queue.o
pablo-icpu-objs += firmware/pablo-icpu-firmware.o
pablo-icpu-objs += firmware/pablo-icpu-imgloader.o
pablo-icpu-objs += pablo-icpu-sysfs.o
pablo-icpu-objs += mem/pablo-icpu-mem.o
pablo-icpu-objs += mem/pablo-icpu-mem-cma-wrap.o
pablo-icpu-objs += mem/pablo-icpu-mem-pmem-wrap.o
pablo-icpu-objs += hardware/pablo-icpu-hw-itf.o

pablo-icpu-$(CONFIG_PABLO_ICPU_HW_V1_0) += hardware/pablo-icpu-v1_0.o
pablo-icpu-$(CONFIG_PABLO_ICPU_HW_V2_0) += hardware/pablo-icpu-v2_0.o
pablo-icpu-$(CONFIG_PABLO_ICPU_HW_V13_0) += hardware/pablo-icpu-v13_0.o

ifeq ($(CONFIG_PABLO_ICPU),y)
obj-m += pablo-icpu.o
endif

FULL_CAM_PATH=$(srctree)/$(src)/..

subdir-ccflags-$(CONFIG_PABLO_ICPU) := -I$(FULL_CAM_PATH)/icpu
subdir-ccflags-$(CONFIG_PABLO_ICPU) += -I$(FULL_CAM_PATH)/icpu/hardware
subdir-ccflags-$(CONFIG_PABLO_ICPU) += -I$(FULL_CAM_PATH)/icpu/mem
