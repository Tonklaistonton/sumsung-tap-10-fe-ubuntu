# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 The Android Open Source Project
load("//build/kernel/kleaf:kernel.bzl", "kernel_module")

_VENDOR_EARLY_MODULE_LIST = [
    "drivers/soc/samsung/exynos/exynos-pkvm-module/exynos-pkvm-module.ko",
    "drivers/soc/samsung/exynos/exynos-pkvm-module/exynos-pkvm-s2mpu-module.ko",
    "drivers/watchdog/s3c2410_wdt_ext.ko",
    "drivers/clocksources/exynos_mct_v3.ko",
    "drivers/soc/samsung/exynos/exynos-chipid_v2.ko",
    "drivers/clk/samsung/clk_exynos.ko",
    "drivers/power/reset/exynos/exynos-reboot.ko",
]

_VENDOR_MODULE_LIST = [
    # No need to sort, but should not be duplicated
    "drivers/pinctrl/samsung/pinctrl-samsung-ext.ko",
    "drivers/tty/serial/exynos_tty.ko",
    "drivers/dma-buf/heaps/samsung/samsung_dma_heap.ko",
    "drivers/iommu/samsung/samsung_iommu_v9.ko",
    "drivers/iommu/samsung/samsung-iommu-group-v9.ko",
    "drivers/iommu/samsung/samsung-secure-iova.ko",
    "drivers/soc/samsung/exynos/exynos-pd/exynos-pd_el3.ko",
    "drivers/soc/samsung/exynos/cal-if/cmupmucal.ko",
    "drivers/soc/samsung/exynos/exynos-pmu-if.ko",
    "drivers/soc/samsung/exynos/exynos-pd/exynos-pd.ko",
    "drivers/soc/samsung/exynos/exynos-pm/exynos-s2i.ko",
    "drivers/i2c/busses/esca-mfd-bus.ko",
    "drivers/soc/samsung/exynos/esca/exynos_esca.ko",
    "drivers/soc/samsung/exynos/esca/plugins/flexpmu_dbg.ko",
    "drivers/phy/samsung/phy-exynos-usbdrd-super.ko",
    "drivers/phy/samsung/sec_repeater_cb.ko",
    "drivers/usb/dwc3/dwc3-exynos-usb.ko",
    "drivers/usb/gadget/function/usb_f_dm.ko",
    "sound/usb/exynos-usb-audio-offloading.ko",
    "drivers/i2c/busses/i2c-exynos5-ext.ko",
    "drivers/spi/spi-s3c64xx-ext.ko",
    "drivers/soc/samsung/exynos/exynos-usi_v2.ko",
    "drivers/mfd/s2p_mfd.ko",
    "drivers/regulator/pmic_class.ko",
    "drivers/regulator/s2p_regulator.ko",
    "drivers/input/keyboard/s2p_key.ko",
    "drivers/pinctrl/s2p_pinctrl.ko",
    "drivers/iio/adc/s2p_adc.ko",
    "drivers/mfd/s2mpu15_mfd.ko",
    "drivers/regulator/s2mpu15_regulator.ko",
    "drivers/input/keyboard/s2mpu15_key.ko",
    "drivers/pinctrl/s2mpu15_pinctrl.ko",
    "drivers/iio/adc/s2mpu15_adc.ko",
    "drivers/mfd/s2mpu16_mfd.ko",
    "drivers/regulator/s2mpu16_regulator.ko",
    "drivers/pinctrl/s2mpu16_pinctrl.ko",
    "drivers/regulator/s2mpb03.ko",
    "drivers/regulator/s2dos05-regulator.ko",
    "drivers/scsi/scsi_srpmb.ko",
    "drivers/ufs/host/ufs-exynos-core.ko",
    "drivers/phy/samsung/phy-exynos-mipi-dsim.ko",
    "drivers/dpu/exynos-drm.ko",
    "drivers/soc/samsung/exynos/exyswd-rng.ko",
    "drivers/soc/samsung/exynos/sysevent.ko",
    "drivers/soc/samsung/exynos/debug/dss.ko",
    "drivers/soc/samsung/exynos/debug/exynos-getcpustate.ko",
    "drivers/soc/samsung/exynos/debug/debug-snapshot-debug-kinfo.ko",
    "drivers/soc/samsung/exynos/debug/exynos-itmon-v2.ko",
    "drivers/soc/samsung/exynos/sysevent_notif.ko",
    "drivers/soc/samsung/exynos/imgloader.ko",
    "drivers/soc/samsung/exynos/debug/exynos-coresight.ko",
    "drivers/soc/samsung/exynos/debug/exynos-ssld.ko",
    "drivers/soc/samsung/exynos/memlogger.ko",
    "drivers/soc/samsung/exynos/debug/hardlockup-watchdog.ko",
    "drivers/soc/samsung/exynos/debug/exynos-ecc-handler.ko",
    "drivers/soc/samsung/exynos/debug/exynos-adv-tracer.ko",
    "drivers/soc/samsung/exynos/debug/exynos-adv-tracer-s2d.ko",
    "drivers/soc/samsung/exynos-cpuhp.ko",
    "drivers/soc/samsung/exynos/exynos-el2/exynos-el2.ko",
    "drivers/soc/samsung/exynos/exynos-el2/exynos-s2mpu.ko",
    "drivers/soc/samsung/exynos/debug/ehld.ko",
    "drivers/soc/samsung/exynos-cpupm.ko",
    "drivers/mmc/host/dw_mmc-samsung.ko",
    "drivers/mmc/host/dw_mmc-exynos-samsung.ko",
    "drivers/mmc/host/dw_mmc-pltfm-samsung.ko",
    "drivers/dma/samsung/samsung-pdma.ko",
    "drivers/soc/samsung/exynos/dm/exynos-esca-dm.ko",
    "drivers/soc/samsung/exynos/ect_parser/ect_parser.ko",
    "drivers/devfreq/lealt-mon.ko",
    "drivers/devfreq/lealt-gov.ko",
    "drivers/devfreq/exynos_devfreq.ko",
    "drivers/soc/samsung/exynos/pm_qos/exynos-pm-qos.ko",
    "drivers/soc/samsung/exynos/exynos-wow.ko",
    "drivers/soc/samsung/exynos-mifgov.ko",
    "drivers/thermal/exynos_thermal_v2.ko",
    "drivers/thermal/gpu_cooling.ko",
    "drivers/soc/samsung/exynos/exynos-mcinfo.ko",
    "drivers/bts/exynos-bts.ko",
    "drivers/bts/exynos-btsops8855.ko",
    "drivers/soc/samsung/exynos-bcm.ko",
    "drivers/cpufreq/exynos-cpufreq.ko",
    "drivers/cpufreq/freq-qos-tracer.ko",
    "drivers/cpufreq/exynos-dsufreq.ko",
    "drivers/soc/samsung/mpam/exynos-mpam.ko",
    "drivers/soc/samsung/mpam/exynos-msc-dsu.ko",
    "kernel/sched/exynos_mpam_policy.ko",
    "kernel/sched/ems/ems.ko",
    "drivers/soc/samsung/exynos/cpif/cpif_memlogger.ko",
    "drivers/soc/samsung/exynos/cpif/mcu_ipc.ko",
    "drivers/soc/samsung/exynos/cpif/shm_ipc.ko",
    "sound/soc/samsung/exynos/abox/snd-soc-samsung-abox-gic.ko",
    "sound/soc/samsung/exynos/abox/snd-soc-samsung-abox.ko",
    "sound/soc/samsung/exynos/sec_audio_exynos.ko",
    "sound/soc/samsung/exynos/snd_debug_proc.ko",
#    "sound/soc/samsung/exynos/codecs/snd-soc-s5m3500.ko",
    "drivers/soc/samsung/exynos/psp/psp.ko",
    "drivers/soc/samsung/exynos/custos/custos.ko",
    "drivers/staging/nanohub/nanohub.ko",
    "drivers/vision/npu.ko",
    "sound/soc/samsung/exynos/vts/snd-soc-samsung-vts-mailbox.ko",
    "sound/soc/samsung/exynos/vts/snd-soc-samsung-vts.ko",
    "drivers/phy/samsung/phy-exynos-mipi.ko",
    #"drivers/soc/samsung/exynos/gcma/gcma.ko",
    "drivers/soc/samsung/exynos/secmem.ko",
    "drivers/soc/samsung/hts/hts.ko",
    "drivers/soc/samsung/exynos-afm.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-profiler-fn.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-main-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-cpu-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-mif-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-gpu-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/submodule/sgpu-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/submodule/sgpu-profiler-governor.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-dsu-profiler.ko",
    "drivers/soc/samsung/xperf/xperf.ko",
    "drivers/soc/samsung/exynos-ufcc.ko",
    "drivers/rtc/s2p_rtc.ko",
    "drivers/rtc/s2mpu15_rtc.ko",
    "drivers/irqchip/exynos/irq-gic-v3-vh.ko",
    "drivers/rtc/sec_pon_alarm.ko",
    "drivers/usb/dwc3/sec_usb_cb.ko",
]

_VENDOR_DEV_MODULE_LIST = [
    # modules only for debug/developments.
]

_VENDOR_DEV_DLKM_MODULE_LIST = [
    # modules only for debug/developments.
    "drivers/soc/samsung/exynos/debug/exynos-debug-test.ko",
    "drivers/pinctrl/samsung/secgpio_dvs.ko",
]

_VENDOR_DLKM_MODULE_LIST = [
    # For vendor_dlkm in order
    "drivers/net/wireless/scsc/scsc_wlan.ko",
    "drivers/soc/samsung/exynos/exynos-pd/exynos-pd-dbg.ko",
    "drivers/soc/samsung/exynos/exynos-sdm.ko",
    "drivers/usb/gadget/function/usb_f_rndis_mp.ko",
    "drivers/soc/samsung/exynos/exynos-pm/exynos-pm.ko",
    "drivers/pwm/pwm-exynos.ko",
    "drivers/smfc/smfc.ko",
    "drivers/scaler/scaler.ko",
    "drivers/mfc/exynos_mfc.ko",
    "drivers/soc/samsung/exynos/exynos-seclog.ko",
    "drivers/soc/samsung/exynos/exynos-seh.ko",
    "drivers/soc/samsung/exynos/exynos-tzasc.ko",
	"drivers/iommu/samsung/exynos-cpif-iommu_v9.ko",
    "drivers/soc/samsung/exynos/cpif/cpif_page.ko",
    "drivers/soc/samsung/exynos/cpif/direct_dm.ko",
    "drivers/soc/samsung/exynos/cpif/cpif.ko",
    "drivers/soc/samsung/exynos/cpif/cpif_cma_mem.ko",
    "drivers/soc/samsung/exynos/cpif/hook.ko",
    "drivers/clo/clo.ko",
    "drivers/soc/samsung/exynos/gnssif/gnssif.ko",
    "drivers/soc/samsung/exynos/gnssif/gnss_mbox.ko",
    "drivers/misc/samsung/scsc/scsc_platform_mif.ko",
    "drivers/misc/samsung/scsc/scsc_mx.ko",
    "drivers/net/wireless/scsc/scsc_wifilogger.ko",
    "drivers/misc/samsung/scsc/scsc_mmap.ko",
    "drivers/misc/samsung/scsc/mx_client_test.ko",
    "drivers/misc/samsung/scsc/scsc_log_collection.ko",
    "drivers/misc/samsung/scsc/scsc_logring.ko",
    "drivers/misc/samsung/scsc_bt/scsc_bt.ko",
    "drivers/block/zram/vendor_zram.ko",
    "mm/sec_mm/sec_mm.ko",
    "sound/soc/samsung/exynos/slif/snd-soc-samsung-slif.ko",
]

_VENDOR_DLKM_BOOT_COMPLETE_MODULE_LIST = [
    # For vendor_dlkm in order
]

_SYSTEM_DLKM_BLOCKLIST = [
    #for system_dlkm blocklist
    "drivers/block/zram/zram.ko",
]

_VENDOR_DLKM_BLOCKLIST = [
    # For vendor_dlkm blocklist
]

_VENDOR_KUNIT_TEST_LIST = [
    "drivers/soc/samsung/test/bus_exynos_test.ko",
    "drivers/ufs/host/test/ufs_exynos_test.ko",
    "drivers/staging/nanohub/test/chub_exynos_test.ko",
    "sound/soc/samsung/exynos/abox/test/audio_exynos_test.ko",
    "sound/soc/samsung/exynos/vts/test/vts_exynos_test.ko",
    "drivers/devfreq/test/dvfs_exynos_test.ko",
    "drivers/thermal/test/dtm_exynos_test.ko",
    "drivers/ufs/host/test/fmp_exynos_test.ko",
    "drivers/soc/samsung/exynos/debug/test/itmon_exynos_test.ko",
    "drivers/soc/samsung/exynos/test/memlogger_exynos_test.ko",
    "drivers/watchdog/test/watchdog_exynos_test.ko",
    "drivers/soc/samsung/exynos/debug/test/debug_snapshot_exynos_test.ko",
    "drivers/soc/samsung/exynos/cal-if/test/power_exynos_test.ko",
    "kernel/sched/ems/test/ems_exynos_test.ko",
    "drivers/cpufreq/test/cpu_exynos_test.ko",
    "drivers/usb/dwc3/test/usb_exynos_test.ko",
    "drivers/scaler/test/mscl_exynos_test.ko",
    "drivers/spi/test/spi_exynos_test.ko",
    "drivers/i2c/busses/test/hsi2c_exynos_test.ko",
    "drivers/iommu/samsung/test/sysmmu_exynos_test.ko",
    "drivers/dpu/tests/dpu_exynos_test.ko",
    "drivers/mfc/test/mfc_exynos_test.ko",
    "drivers/smfc/test/smfc_exynos_test.ko",
    "drivers/soc/samsung/exynos/cpif/test/cpif_exynos_test.ko",
    "drivers/soc/samsung/exynos/gnssif/test/gnssif_exynos_test.ko",
    "drivers/tty/serial/test/uart_exynos_test.ko",
]

def get_soc_vendor_dlkm_blocklist():
    modules_list = []
    modules_list += _VENDOR_DLKM_BLOCKLIST
    return modules_list

def get_soc_system_dlkm_blocklist():
    modules_list = []
    modules_list += _SYSTEM_DLKM_BLOCKLIST
    return modules_list

def get_soc_vendor_early_boot_modules_list():
    return _VENDOR_EARLY_MODULE_LIST

def get_soc_vendor_kunit_test_list(variant = None):
    modules_list = []
    if variant != "user" and variant != "user.fac":
        modules_list += _VENDOR_KUNIT_TEST_LIST
    return modules_list

def get_soc_vendor_boot_modules_list(variant = None):
    modules_list = []
    modules_list += get_soc_vendor_early_boot_modules_list()
    modules_list += _VENDOR_MODULE_LIST
    if variant != "user" and variant != "user.fac":
        modules_list += _VENDOR_DEV_MODULE_LIST
    return modules_list

def get_soc_vendor_dlkm_modules_list(variant = None):
    modules_list = []
    modules_list += _VENDOR_DLKM_MODULE_LIST
    modules_list += _VENDOR_DLKM_BOOT_COMPLETE_MODULE_LIST
    if variant != "user" and variant != "user.fac":
        modules_list += _VENDOR_DEV_DLKM_MODULE_LIST
    modules_list += get_soc_vendor_dlkm_blocklist()
    return modules_list

def get_soc_vendor_modules_list(variant = None):
    modules_list = []
    modules_list += get_soc_vendor_boot_modules_list(variant)
    modules_list += get_soc_vendor_dlkm_modules_list(variant)
    modules_list += get_soc_vendor_kunit_test_list(variant)
    return modules_list

def define_s5e8855_soc_module(variant):
    MODULE_OUTS = get_soc_vendor_modules_list(variant)
    kernel_module(
        name = "s5e8855_soc_module.{}".format(variant),
        srcs = [
            "//exynos/soc-series/s-android15:exynos.soc_sources",
        ],
        outs = MODULE_OUTS,
        kernel_build = "//exynos/soc-series/common:exynos_kernel_build",
        visibility = [
            "//exynos/device:__subpackages__",
            "//exynos/soc-series:__subpackages__",
            "//exynos/external-modules:__subpackages__",
        ],
    )

def register_s5e8855_soc_module():
    define_s5e8855_soc_module("eng")
    define_s5e8855_soc_module("userdebug")
    define_s5e8855_soc_module("user")
