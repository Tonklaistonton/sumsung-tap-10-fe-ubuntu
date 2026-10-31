#
# Makefile for the stui Module
#

$(info *********************************************)
$(info srctree=$(srctree))
$(info srctree=$(srctree)/../exynos/device/mx/mx8855/drivers/tee/)
$(info *********************************************)

ccflags-y += -I$(srctree)/../exynos/device/mx/mx8855/drivers/tee/
ccflags-y += -I$(srctree)/../exynos/device/mx/mx8855/drivers/tee/tui/
ccflags-y += -I$(srctree)/../exynos/device/mx/mx8855/drivers/tee/tzdev/
ccflags-y += -I$(srctree)/../exynos/device/mx/mx8855/drivers/tee/tzdev/include/
ccflags-y += -I$(srctree)/../exynos/device/mx/mx8855/drivers/tee/tzdev/include/tzdev/

ccflags-y += -DUSE_TEE_CLIENT_API

tuihw-inf-objs := stui_inf.o stui_log.o

tuihw-objs := main.o
tuihw-objs += stui_core.o
tuihw-objs += stui_log.o

ifeq ($(CONFIG_SOC_S5E8855),y)

ccflags-y += -DCONFIG_SOC_S5E8855

ccflags-y += -I$(srctree)/../device/mx/mx8855/drivers/tee/tui/platforms/exynos8855
ccflags-y += -I$(srctree)/../device/mx/mx8855/drivers/gpu/drm/samsung/dpu
tuihw-objs += platforms/exynos8855/stui_hal_display.o
tuihw-objs += platforms/exynos8855/stui_hal_touch.o

endif

ifdef CONFIG_SAMSUNG_TUI_LOWLEVEL
tuihw-objs += iwd_agent.o

ccflags-y += -DCONFIG_SAMSUNG_TUI_LOWLEVEL
endif

ifneq ($(TARGET_BUILD_VARIANT),user)
ccflags-y += -DDEBUG_LOG_ALLOWED
endif

obj-$(CONFIG_SAMSUNG_TUI) += tuihw.o tuihw-inf.o
