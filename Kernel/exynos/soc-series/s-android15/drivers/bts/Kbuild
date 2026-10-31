# SPDX-License-Identifier: GPL-2.0-only
#
# BTS device driver
#

obj-$(CONFIG_EXYNOS_BTS)	+= exynos-bts.o

ifdef CONFIG_SOC_S5E8855
obj-$(CONFIG_EXYNOS_BTS)        += exynos-btsops8855.o
endif

ifdef CONFIG_SOC_S5E9945
obj-$(CONFIG_EXYNOS_BTS)        += exynos-btsops9945.o
endif

ifdef CONFIG_SOC_S5E9955
obj-$(CONFIG_EXYNOS_BTS)        += exynos-btsops9955.o
endif
