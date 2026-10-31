# SPDX-License-Identifier: GPL-2.0-only

INCLUDE_PATH = $(srctree)/$(src)/include
ccflags-y += -I$(INCLUDE_PATH)
ccflags-y += -I$(srctree)


obj-$(CONFIG_EXYNOS_PROFILER_MAIN)	+= exynos-main-profiler.o
obj-$(CONFIG_EXYNOS_PROFILER_MAIN) 	+= exynos-profiler-fn.o

obj-$(CONFIG_EXYNOS_PROFILER_CPU)	+= exynos-cpu-profiler.o
obj-$(CONFIG_EXYNOS_PROFILER_MIF)	+= exynos-mif-profiler.o

obj-$(CONFIG_EXYNOS_PROFILER_GPU)	+= exynos-gpu-profiler.o
obj-$(CONFIG_EXYNOS_PROFILER_GPU)	+= submodule/sgpu-profiler.o
obj-$(CONFIG_EXYNOS_PROFILER_GPU)	+= submodule/sgpu-profiler-governor.o

obj-$(CONFIG_EXYNOS_PROFILER_DSU)	+= exynos-dsu-profiler.o