# SPDX-License-Identifier: GPL-2.0

FULL_SGPU_PATH=$(srctree)/$(src)/..

ccflags-y := -I$(FULL_SGPU_PATH)/include/asic_reg \
	-I$(FULL_SGPU_PATH)/include \
	-I$(FULL_SGPU_PATH)/sgpu \
	-I$(FULL_SGPU_PATH)/pm/inc \
	-I$(FULL_SGPU_PATH)/devfreq

sgpu_exynos_test-y := sgpu_exynos_basic_test.o

obj-$(CONFIG_SGPU_EXYNOS_KUNIT_TEST) += sgpu_exynos_test.o
