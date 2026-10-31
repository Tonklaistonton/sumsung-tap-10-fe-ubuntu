#
# Copyright (c) 2012 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

subdir-ccflags-y := -I$(srctree)/$(src)
scaler-objs := scaler-core.o scaler-regs.o scaler-debug.o scaler-ext.o
scaler-objs += scaler-version.o scaler-format.o
obj-$(CONFIG_VIDEO_EXYNOS_SCALER)	+= scaler.o

# test
obj-$(CONFIG_MSCL_EXYNOS_KUNIT_TEST) += test/
