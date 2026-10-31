# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 The Android Open Source Project
load("//build/kernel/kleaf:kernel.bzl", "kernel_module")

_VENDOR_EARLY_MODULE_LIST = [
    "drivers/soc/samsung/exynos/exynos-pkvm-module/exynos-pkvm-module.ko",
    "drivers/soc/samsung/exynos/exynos-pkvm-module/exynos-pkvm-s2mpu-module.ko",
    "drivers/soc/samsung/exynos/exynos-chipid_v2.ko",
    "drivers/power/reset/exynos/exynos-reboot.ko",
    "drivers/clk/samsung/clk_exynos.ko",
    "drivers/watchdog/s3c2410_wdt_ext.ko",
    "drivers/clocksources/exynos_mct_v3.ko"
]

_VENDOR_MODULE_LIST = [
    # No need to sort, but should not be duplicated
    "drivers/pinctrl/samsung/pinctrl-samsung-ext.ko",
    "drivers/tty/serial/exynos_tty.ko",
    "drivers/soc/samsung/exynos/exynos-pmu-if.ko",
    "drivers/soc/samsung/exynos/esca/exynos_esca.ko",
    "drivers/soc/samsung/exynos/esca/plugins/flexpmu_dbg.ko",
    "drivers/soc/samsung/exynos/ect_parser/ect_parser.ko",
    "drivers/soc/samsung/exynos/pm_qos/exynos-pm-qos.ko",
    "drivers/soc/samsung/exynos/dm/exynos-esca-dm.ko",
    "drivers/devfreq/lealt-mon.ko",
    "drivers/devfreq/lealt-gov.ko",
    "drivers/devfreq/exynos_devfreq.ko",
    "drivers/thermal/exynos_thermal_v2.ko",
    "drivers/thermal/exynos_amb_control.ko",
    "drivers/thermal/gpu_cooling.ko",
    "drivers/soc/samsung/exynos/exynos-mcinfo.ko",
    "drivers/soc/samsung/exynos/exynos-wow.ko",
    "drivers/soc/samsung/exynos-mifgov.ko",
    "drivers/i2c/busses/esca-mfd-bus.ko",
    "drivers/soc/samsung/exynos/exynos-pd/exynos-pd.ko",
    "drivers/soc/samsung/exynos/exynos-pd/exynos-pd_el3.ko",
    "drivers/soc/samsung/exynos/exyswd-rng.ko",
    "drivers/iommu/samsung/samsung_iommu_v9.ko",
    "drivers/iommu/samsung/samsung-iommu-group-v9.ko",
    "drivers/iommu/samsung/samsung-secure-iova.ko",
    "drivers/dma-buf/heaps/samsung/samsung_dma_heap.ko",
    "drivers/soc/samsung/exynos/cal-if/cmupmucal.ko",
    "drivers/ufs/host/ufs-exynos-core.ko",
    "drivers/phy/samsung/phy-exynos-mipi-dsim.ko",
    "drivers/dpu/exynos-drm.ko",
    "drivers/dpu/panel/panel-samsung-drv.ko",
    "drivers/dpu/panel/panel-samsung-command-ctrl.ko",
    "drivers/mfd/s2p_mfd.ko",
    "drivers/regulator/pmic_class.ko",
    "drivers/regulator/s2p_regulator.ko",
    "drivers/input/keyboard/s2p_key.ko",
    "drivers/pinctrl/s2p_pinctrl.ko",
    "drivers/iio/adc/s2p_adc.ko",
    "drivers/mfd/s2se910_mfd.ko",
    "drivers/regulator/s2se910_regulator.ko",
    "drivers/input/keyboard/s2se910_key.ko",
    "drivers/iio/adc/s2se910_adc.ko",
    "drivers/mfd/s2se911_mfd.ko",
    "drivers/regulator/s2se911_regulator.ko",
    "drivers/pinctrl/s2se911_pinctrl.ko",
    "drivers/mfd/s2rp910_mfd.ko",
    "drivers/regulator/s2rp910_regulator.ko",
    "drivers/iio/adc/s2rp910_adc.ko",
    "drivers/mfd/s2mpa05_mfd.ko",
    "drivers/regulator/s2mpa05_regulator.ko",
    "drivers/pinctrl/s2mpa05_pinctrl.ko",
    "drivers/mfd/s2mpb02_mfd.ko",
    "drivers/regulator/s2mpb02-regulator.ko",
    "drivers/regulator/s2mpb03.ko",
    "drivers/regulator/s2dos05-regulator.ko",
    "drivers/power/supply/samsung/ifpmic_class.ko",
    "drivers/power/supply/samsung/s2mf301_fuelgauge.ko",
    "drivers/misc/samsung/muic/s2m_muic_module.ko",
    "drivers/misc/samsung/ifconn/ifconn_manager.ko",
    "drivers/usb/typec/samsung/s2m_pdic_module.ko",
    "drivers/misc/samsung/ifconn/ifconn_notifier.ko",
    "drivers/power/supply/samsung/s2mf301_pmeter.ko",
    "drivers/power/supply/samsung/s2mf301_top.ko",
    "drivers/mfd/samsung/s2mf301_mfd.ko",
    "drivers/power/supply/samsung/s2mf301_charger.ko",
    "drivers/soc/samsung/exynos-cpuhp.ko",
    "drivers/soc/samsung/exynos-cpupm.ko",
    "drivers/phy/samsung/phy-exynos-usbdrd-super.ko",
    "drivers/phy/samsung/eusb_repeater.ko",
    "drivers/usb/dwc3/dwc3-exynos-usb.ko",
    "drivers/usb/dwc3/exynos_usb_debug.ko",
    "drivers/usb/notify_lsi/usb_notify_layer.ko",
    "drivers/usb/notify_lsi/usb_notifier.ko",
    "drivers/soc/samsung/exynos/sysevent.ko",
    "drivers/soc/samsung/exynos/debug/dss.ko",
    "drivers/soc/samsung/exynos/debug/exynos-getcpustate.ko",
    "drivers/soc/samsung/exynos/debug/debug-snapshot-debug-kinfo.ko",
    "drivers/soc/samsung/exynos/sysevent_notif.ko",
    "drivers/soc/samsung/exynos/debug/exynos-coresight.ko",
    "drivers/soc/samsung/exynos/debug/exynos-coresight-etm.ko",
    "drivers/soc/samsung/exynos/debug/exynos-ssld.ko",
    "drivers/soc/samsung/exynos/memlogger.ko",
    "drivers/soc/samsung/exynos/debug/hardlockup-watchdog.ko",
    "drivers/soc/samsung/exynos/debug/exynos-ecc-handler.ko",
    "drivers/soc/samsung/exynos/debug/exynos-linkecc-handler.ko",
    "drivers/soc/samsung/exynos/debug/exynos-ddrphylockdelta-handler.ko",
    "drivers/soc/samsung/exynos/debug/exynos-itmon-v2.ko",
    "drivers/soc/samsung/exynos/debug/exynos-adv-tracer.ko",
    "drivers/soc/samsung/exynos/debug/exynos-adv-tracer-s2d.ko",
    "drivers/soc/samsung/exynos/debug/ehld.ko",
    "drivers/i2c/busses/i2c-exynos5-ext.ko",
    "drivers/spi/spi-s3c64xx-ext.ko",
    "drivers/bts/exynos-bts.ko",
    "drivers/bts/exynos-btsops9955.ko",
    "drivers/soc/samsung/exynos-bcm.ko",
    "drivers/soc/samsung/exynos_sci.ko",
    "drivers/dma/samsung/samsung-pdma.ko",
    "drivers/soc/samsung/exynos/exynos-el2/exynos-el2.ko",
    "drivers/soc/samsung/exynos/exynos-el2/exynos-s2mpu.ko",
    "drivers/cpufreq/exynos-cpufreq.ko",
    "drivers/cpufreq/freq-qos-tracer.ko",
    "drivers/soc/samsung/exynos/psp/psp.ko",
    "drivers/soc/samsung/exynos/custos/custos.ko",
    "drivers/gud/gud-s5e9955/MobiCoreDriver/mcDrvModule.ko",
    "drivers/soc/samsung/exynos/imgloader.ko",
    "drivers/cpufreq/exynos-dsufreq.ko",
    "drivers/usb/gadget/function/usb_f_dm.ko",
    "sound/usb/exynos-usb-audio-offloading.ko",
    "drivers/staging/nanohub/nanohub.ko",
    "drivers/soc/samsung/mpam/exynos-mpam.ko",
    "drivers/soc/samsung/mpam/exynos-msc-dsu.ko",
    "drivers/soc/samsung/mpam/exynos-msc-llc.ko",
    "drivers/soc/samsung/mpam/exynos-msc-smc.ko",
    "kernel/sched/exynos_mpam_policy.ko",
    "kernel/sched/ems/ems.ko",
    "drivers/soc/samsung/exynos/cpif/cpif_memlogger.ko",
    "drivers/soc/samsung/exynos/cpif/shm_ipc.ko",
    "drivers/tsmux/tsmux.ko",
    "drivers/repeater/repeater.ko",
    "drivers/scsi/scsi_srpmb.ko",
    "drivers/phy/samsung/phy-exynos-mipi.ko",
    "drivers/pci/controller/dwc/pcie-exynos-rc-core.ko",
    "drivers/phy/samsung/exynos-phycal-if.ko",
    "drivers/gud/gud-s5e9955/TlcTui/t-base-tui.ko",
    "drivers/irqchip/exynos/irq-gic-v3-vh.ko",
    "drivers/soc/samsung/exynos/exynos-pm/exynos-s2i.ko",
    "drivers/soc/samsung/exynos/exynos-usi_v2.ko",
    "drivers/i3c/i3c-hci-exynos.ko",
    "drivers/soc/samsung/exynos/debug/exynos-ped-v2.ko",
    "drivers/iommu/samsung/exynos-pcie-iommu.ko",
    "sound/soc/samsung/exynos/abox/snd-soc-samsung-abox-gic.ko",
    "sound/soc/samsung/exynos/abox/snd-soc-samsung-abox.ko",
    "sound/soc/samsung/exynos/vts/snd-soc-samsung-vts-mailbox.ko",
    "sound/soc/samsung/exynos/vts/snd-soc-samsung-vts.ko",
    "drivers/soc/samsung/exynos-afm.ko",
    "drivers/soc/samsung/exynos/exynos-hdcp/hdcp2.ko",
    "drivers/nfc/samsung/sec_nfc.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-profiler-fn.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-main-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-cpu-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-mif-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-gpu-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/submodule/sgpu-profiler.ko",
    "drivers/soc/samsung/exynos/profiler/submodule/sgpu-profiler-governor.ko",
    "drivers/soc/samsung/exynos/profiler/exynos-dsu-profiler.ko",
    "drivers/soc/samsung/exynos-ufcc.ko",
    "drivers/soc/samsung/xperf/xperf.ko",
    "drivers/soc/samsung/exynos/gcma/gcma.ko",
]

_VENDOR_DEV_MODULE_LIST = [
    # modules only for debug/developments.
]

_VENDOR_DEV_DLKM_MODULE_LIST = [
    # modules only for debug/developments.
    # Do NOT add kunit test modules here. Please use _VENDOR_KUNIT_TEST_LIST.
    "drivers/soc/samsung/exynos/debug/exynos-debug-test.ko",
]

_VENDOR_DLKM_MODULE_LIST = [
    # For vendor_dlkm in order
    "drivers/soc/samsung/exynos/exynos-pd/exynos-pd-dbg.ko",
    "drivers/soc/samsung/exynos/exynos-seh.ko",
    "drivers/soc/samsung/exynos/exynos-tzasc.ko",
    "drivers/soc/samsung/exynos/exynos-seclog.ko",
    "drivers/power/supply/samsung/s2m_chg_manager.ko",
    "sound/soc/codecs/snd-soc-cs35l43-i2c.ko",
    "drivers/soc/samsung/exynos/exynos-sdm.ko",
    "drivers/soc/samsung/exynos-sci_dbg.ko",
    "drivers/soc/samsung/exynos/exynos-pm/exynos-pm.ko",
    "drivers/smfc/smfc.ko",
    "drivers/mfc/exynos_mfc.ko",
    "drivers/pwm/pwm-exynos.ko",
    "sound/soc/samsung/exynos/snd-soc-dp_dma.ko",
    "drivers/soc/samsung/exynos/exynos-ssp.ko",
    "sound/soc/samsung/exynos/exynos9955_sound.ko",
    "sound/soc/codecs/snd-soc-wm-adsp.ko",
    "sound/soc/samsung/exynos/slif/snd-soc-samsung-slif.ko",
    "drivers/soc/samsung/exynos/cpif/cpif.ko",
    "drivers/iommu/samsung/exynos-cpif-iommu_v9.ko",
    "drivers/soc/samsung/exynos/cpif/mcu_ipc.ko",
    "drivers/scaler/scaler.ko",
    "drivers/soc/samsung/exynos/cpif/cpif_cma_mem.ko",
    "drivers/soc/samsung/exynos/cpif/hook.ko",
    "drivers/soc/samsung/exynos/cpif/cpif_page.ko",
    "drivers/soc/samsung/exynos/cpif/direct_dm.ko",
    "drivers/soc/samsung/exynos/secmem.ko",
    "drivers/rtc/s2se910_rtc.ko",
    "drivers/rtc/s2p_rtc.ko",
    "drivers/usb/gadget/function/usb_f_rndis_mp.ko",
    "drivers/leds/samsung/leds-s2mf301.ko",
    "drivers/leds/samsung/leds-s2mpb02.ko",
    "drivers/soc/samsung/exynos/gnssif/gnss_mbox.ko",
    "drivers/soc/samsung/exynos/gnssif/gnssif.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_platform_mif.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_boot_service.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_mx.ko",
    "drivers/net/wireless/pcie_scsc/scsc_wifilogger.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_mmap.ko",
    "drivers/misc/samsung/pcie_scsc/mx_client_test.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_log_collection.ko",
    "drivers/misc/samsung/pcie_scsc/scsc_logring.ko",
    "drivers/misc/samsung/slsi_bt/scsc_bt.ko",
    "drivers/net/wireless/pcie_scsc/scsc_wlan.ko",
    "drivers/soc/samsung/hts/hts.ko",
    "drivers/soc/samsung/hwdecomp/hwdecomp.ko",
    "drivers/block/zram/vendor_zram.ko",
    "drivers/vision/npu.ko",
]

_VENDOR_DLKM_BOOT_COMPLETE_MODULE_LIST = [
    # For vendor_dlkm in order
]

_SYSTEM_DLKM_BLOCKLIST = [
    "kernel/drivers/ptp/ptp_kvm.ko",
    "drivers/block/zram/zram.ko",
]

_VENDOR_DLKM_BLOCKLIST = [
    # For vendor_dlkm blocklist
]

_VENDOR_KUNIT_TEST_LIST = [
    "drivers/ufs/host/test/fmp_exynos_test.ko",
    "drivers/ufs/host/test/ufs_exynos_test.ko",
    "drivers/usb/dwc3/test/usb_exynos_test.ko",
    "drivers/pci/controller/dwc/test/pcie_exynos_test.ko",
    "drivers/soc/samsung/exynos/cpif/test/cpif_exynos_test.ko",
    "drivers/soc/samsung/exynos/gnssif/test/gnssif_exynos_test.ko",
    "drivers/soc/samsung/exynos/exynos-hdcp/test/hdcp2_exynos_test.ko",
    "drivers/dpu/tests/dpu_exynos_test.ko",
    "drivers/staging/nanohub/test/chub_exynos_test.ko",
    "sound/soc/samsung/exynos/vts/test/vts_exynos_test.ko",
    "sound/soc/samsung/exynos/abox/test/audio_exynos_test.ko",
    "drivers/smfc/test/smfc_exynos_test.ko",
    "drivers/mfc/test/mfc_exynos_test.ko",
    "drivers/soc/samsung/hwdecomp/test/hwdecomp_exynos_test.ko",
    "drivers/tsmux/test/tsmux_exynos_test.ko",
    "drivers/cpufreq/test/cpu_exynos_test.ko",
    "kernel/sched/ems/test/ems_exynos_test.ko",
    "drivers/scaler/test/mscl_exynos_test.ko",
    "drivers/devfreq/test/dvfs_exynos_test.ko",
    "drivers/thermal/test/dtm_exynos_test.ko",
    "drivers/soc/samsung/exynos/cal-if/test/power_exynos_test.ko",
    "drivers/net/wireless/pcie_scsc/kunit/wifi_exynos_test.ko",
    "drivers/iommu/samsung/test/sysmmu_exynos_test.ko",
    "drivers/soc/samsung/test/bus_exynos_test.ko",
    "drivers/i2c/busses/test/hsi2c_exynos_test.ko",
    "drivers/spi/test/spi_exynos_test.ko",
    "drivers/tty/serial/test/uart_exynos_test.ko",
    "drivers/soc/samsung/exynos/debug/test/itmon_exynos_test.ko",
    "drivers/soc/samsung/exynos/test/memlogger_exynos_test.ko",
    "drivers/watchdog/test/watchdog_exynos_test.ko",
    "drivers/soc/samsung/exynos/debug/test/debug_snapshot_exynos_test.ko",
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
    if variant != "user":
        modules_list += _VENDOR_KUNIT_TEST_LIST
    return modules_list

def get_soc_vendor_boot_modules_list(variant = None):
    modules_list = []
    modules_list += get_soc_vendor_early_boot_modules_list()
    modules_list += _VENDOR_MODULE_LIST
    if variant != "user":
        modules_list += _VENDOR_DEV_MODULE_LIST
    return modules_list

def get_soc_vendor_dlkm_modules_list(variant = None):
    modules_list = []
    modules_list += _VENDOR_DLKM_MODULE_LIST
    modules_list += _VENDOR_DLKM_BOOT_COMPLETE_MODULE_LIST
    if variant != "user":
        modules_list += _VENDOR_DEV_DLKM_MODULE_LIST
    modules_list += get_soc_vendor_dlkm_blocklist()
    return modules_list

def get_soc_vendor_modules_list(variant = None):
    modules_list = []
    modules_list += get_soc_vendor_boot_modules_list(variant)
    modules_list += get_soc_vendor_dlkm_modules_list(variant)
    modules_list += get_soc_vendor_kunit_test_list(variant)
    return modules_list

def define_s5e9955_soc_module(variant):
    MODULE_OUTS = get_soc_vendor_modules_list(variant)
    kernel_module(
        name = "s5e9955_soc_module.{}".format(variant),
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

def register_s5e9955_soc_module():
    define_s5e9955_soc_module("eng")
    define_s5e9955_soc_module("userdebug")
    define_s5e9955_soc_module("user")


