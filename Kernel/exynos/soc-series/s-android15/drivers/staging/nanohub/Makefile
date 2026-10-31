#
# Makefile for nanohub
#
ifneq ($(TARGET_SOC),"")
EXTRA_CFLAGS := -DTARGET_SOC=$(TARGET_SOC)
endif

ifneq ($(EXYNOS_SOC),"")
EXTRA_CFLAGS := -DTARGET_SOC=$(EXYNOS_SOC)
endif

obj-$(CONFIG_NANOHUB) += nanohub.o
nanohub-y := chub.o chub_bootup.o chub_dbg.o chub_log.o chub_exynos.o
nanohub-y += chub_ipc_if.o ipc_chub.o ipc_chub_ap.o ipc_common.o ipc_hw.o
nanohub-$(CONFIG_SENSOR_DRV) += main.o comms.o
ifneq (,$(filter m ,$(CONFIG_SHUB)))
nanohub-y += chub_shub.o
endif

obj-$(CONFIG_CHUB_EXYNOS_KUNIT_TEST) += test/
