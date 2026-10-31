# drivers/samsung/pm/Makefile
#
# Copyright (c) 2010-2024 Samsung Electronics Co., Ltd.
#		http://www.samsung.com/
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

# Samsung Mobile Power Management Debugging Support
obj-$(CONFIG_SEC_PM_CPUPM)		+= sec_pm_cpupm.o
obj-$(CONFIG_SEC_PM_DEBUG)		+= sec_pm_debug.o
obj-$(CONFIG_SEC_PM_TMU)		+= sec_pm_tmu.o
obj-$(CONFIG_SEC_PM_REGULATOR)		+= sec_pm_regulator.o

ccflags-y := -Wformat
