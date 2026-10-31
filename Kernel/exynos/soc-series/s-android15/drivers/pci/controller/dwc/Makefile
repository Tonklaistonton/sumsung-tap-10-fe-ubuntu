# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_PCI_EXYNOS_HOST) += pcie-exynos-rc-core.o
ifdef CONFIG_PCIE_EXYNOS_DWPHY
pcie-exynos-rc-core-objs += pcie-exynos-rc-dwphy.o pcie-exynos-dbg-dwphy.o pcie-exynos-phycal.o
else
pcie-exynos-rc-core-objs += pcie-exynos-rc.o pcie-exynos-dbg.o pcie-exynos-phycal.o
endif

obj-$(CONFIG_EXYNOS_PCIE_KUNIT_TEST) += test/
