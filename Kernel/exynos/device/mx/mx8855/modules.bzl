# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 The Android Open Source Project
load("//build/kernel/kleaf:kernel.bzl", "kernel_module")
load("//exynos/soc-series/s-android15:s5e8855_modules.bzl",
    "get_soc_vendor_boot_modules_list",
    "get_soc_vendor_dlkm_modules_list",
    "get_soc_vendor_dlkm_blocklist",
    "get_soc_system_dlkm_blocklist",
    "get_soc_vendor_kunit_test_list",
)
load(":model_variant_modules.bzl", 
    "model_variant_common_kernel_module_list",
    "model_variant_vendor_boot_module_list",
    "model_variant_vendor_dlkm_module_list",
    "model_variant_external_module_list",
    "model_variant_external_dlkm_module_list",
)
load(":lego.bzl", "lego_module_list", "lego_module_list_first", "lego_module_list_second")

_FACTORY_BUILD_MODULE_LIST = [
]

# common_kernel's module. In vendor_boot.
_COMMON_KERNEL_MODULE_LIST = [
    "crypto/twofish_common.ko",
    "crypto/twofish_generic.ko",
    "drivers/gpu/drm/drm_ttm_helper.ko",
    "drivers/gpu/drm/scheduler/gpu-sched.ko",
    "drivers/gpu/drm/display/drm_display_helper.ko",
#    "drivers/firmware/cirrus/cs_dsp.ko",
    "drivers/spi/spidev.ko",
    "drivers/usb/host/xhci-sideband.ko",
]

_COMMON_KERNEL_VENDOR_DLKM_MODULE_LIST = [
    "net/wireless/cfg80211.ko",
    "drivers/net/phy/smsc.ko",
    "drivers/net/usb/smsc75xx.ko",
    "drivers/net/usb/smsc95xx.ko",
]

# vendor_boot modules. In vendor_boot
_VENDOR_MODULE_LIST = [
    "drivers/samsung/pm/sec_pm_cpupm.ko",
    "drivers/samsung/pm/sec_pm_debug.ko",
    "drivers/samsung/pm/sec_pm_tmu.ko",
    "drivers/ufs/host/ufs-sec-driver.ko",
    "drivers/mmc/host/mmc-sec-driver.ko",
    "drivers/usb/gadget/function/usb_f_conn_gadget.ko",
    "drivers/usb/gadget/function/usb_f_ss_mon_gadget.ko",
    "drivers/tee/tzdev/tzdev.ko",
    "drivers/samsung/pm/sec_pm_regulator.ko",
    "block/blk-sec-common.ko",
    "block/blk-sec-stats.ko",
    "block/blk-sec-wb.ko",
    "block/ssg.ko",
    "drivers/tee/tui/tuihw.ko",
    "drivers/tee/tui/tuihw-inf.ko",
    "drivers/usb/dwc3/sec_usb_hook.ko",
    "drivers/usb/misc/sec_lvstest.ko",
]

_VENDOR_KUNIT_TEST_LIST = [

]

# Only for debug/developement. In vendor_boot
_VENDOR_DEV_MODULE_LIST = [
#    "pablo-self-tests.ko",
]

# Only for debug/developement. In vendor_dlkm
_VENDOR_DEV_DLKM_MODULE_LIST = [
]

# In vendor_dlkm
_VENDOR_DLKM_MODULE_LIST = [
    "drivers/camera/is-vendor-interface.ko",
]

# In vendor_dlkm
_VENDOR_DLKM_BOOT_COMPLETE_MODULE_LIST = [
]

# In system_dlkm blocklist
_SYSTEM_DLKM_BLOCKLIST = [
    # For system_dlkm blocklist
    "kernel/drivers/ptp/ptp_kvm.ko",
]

# In vendor_dlkm blocklist
_VENDOR_DLKM_BLOCKLIST = [
    # For vendor_dlkm blocklist
]

# external-modules. In vendor_boot
_EXTERNAL_MODULE_LIST = [
    "sgpu.ko",
]

# external-modules. In vendor_dlkm
_EXTERNAL_MODULE_IN_DLKM_LIST = [
    "fimc-is.ko",
    "pablo-libs.ko",
    "pablo-smc.ko",
    "votf.ko",
    "pablo-hws.ko",
    "pablo-icpu.ko",
    "gdc.ko",

]

_EXTERNAL_KUNIT_TEST_LIST = [
    "pablo-self-tests.ko",
]

# external-modules. In vendor_dlkm
_EXTERNAL_MODULE_BOOT_COMPLETE_IN_DLKM_LIST = [
]

def get_mx8855_kunit_test_list(variant = None):
    modules_list = []
    if variant != "user" and variant != "user.fac":
        modules_list += _VENDOR_KUNIT_TEST_LIST
    return modules_list

def get_mx8855_system_dlkm_blocklist():
    modules_list = []
    modules_list += _SYSTEM_DLKM_BLOCKLIST
    return modules_list

def get_mx8855_vendor_dlkm_blocklist():
    modules_list = []
    modules_list += _VENDOR_DLKM_BLOCKLIST
    return modules_list

def get_mx8855_vendor_dlkm_modules_list(variant = None):
    modules_list = []
    modules_list += _VENDOR_DLKM_MODULE_LIST
    modules_list += _VENDOR_DLKM_BOOT_COMPLETE_MODULE_LIST
    if variant != "user" and variant != "user.fac":
        modules_list += _VENDOR_DEV_DLKM_MODULE_LIST
    modules_list += get_mx8855_vendor_dlkm_blocklist()
    modules_list += lego_module_list_second
    modules_list += model_variant_vendor_dlkm_module_list
    return modules_list

# All modules built from common_kernel
def get_common_kernel_modules_list():
    modules_list = []
    modules_list += _COMMON_KERNEL_MODULE_LIST
    modules_list += _COMMON_KERNEL_VENDOR_DLKM_MODULE_LIST
    modules_list += model_variant_common_kernel_module_list
    return modules_list

def get_mx8855_vendor_boot_modules_list(variant = None):
    modules_list = []
    modules_list += _VENDOR_MODULE_LIST
    modules_list += model_variant_vendor_boot_module_list
    modules_list += lego_module_list
    modules_list += lego_module_list_first
    if variant != "user" and variant != "user.fac":
        modules_list += _VENDOR_DEV_MODULE_LIST
    if "fac" in variant:
        modules_list += _FACTORY_BUILD_MODULE_LIST
    return modules_list

# All modules included in vendor boot
def get_all_vendor_boot_modules_list(variant = None):
    modules_list = []
    modules_list += get_mx8855_vendor_boot_modules_list(variant)
    modules_list += get_soc_vendor_boot_modules_list(variant)
    modules_list += _COMMON_KERNEL_MODULE_LIST
    modules_list += _EXTERNAL_MODULE_LIST
    modules_list += model_variant_common_kernel_module_list
    modules_list += model_variant_external_module_list
    return modules_list

def get_all_system_dlkm_blocklist():
    modules_list = []
    modules_list += get_soc_system_dlkm_blocklist()
    modules_list += get_mx8855_system_dlkm_blocklist()
    return modules_list

# All vendor_dlkm blocklist included in vendor_boot
def get_all_vendor_dlkm_blocklist():
    modules_list = []
    modules_list += get_soc_vendor_dlkm_blocklist()
    modules_list += get_mx8855_vendor_dlkm_blocklist()
    return modules_list

def get_all_kunit_test_list(variant = None):
    modules_list = []
    modules_list += get_soc_vendor_kunit_test_list(variant)
    modules_list += get_mx8855_kunit_test_list(variant)
    if variant != "user" and variant != "user.fac":
        modules_list += _EXTERNAL_KUNIT_TEST_LIST
    return modules_list

# All modules included in soc_vendor_dlkm
def get_all_vendor_dlkm_modules_list(variant = None):
    modules_list = []
    modules_list += _COMMON_KERNEL_VENDOR_DLKM_MODULE_LIST
    modules_list += get_soc_vendor_dlkm_modules_list(variant)
    modules_list += get_mx8855_vendor_dlkm_modules_list(variant)
    modules_list += _EXTERNAL_MODULE_IN_DLKM_LIST
    modules_list += model_variant_external_dlkm_module_list
    return modules_list

# All modules built from this directory
def get_mx8855_vendor_modules_list(variant = None):
    modules_list = []
    modules_list += get_mx8855_vendor_boot_modules_list(variant)
    modules_list += get_mx8855_vendor_dlkm_modules_list(variant)
    modules_list += get_mx8855_kunit_test_list(variant)
    return modules_list

def define_mx8855_module(variant = None):
    MODULE_OUTS = get_mx8855_vendor_modules_list(variant)
    if MODULE_OUTS:
        kernel_module(
            name = "mx8855_module.{}".format(variant),
             srcs = [
                ":exynos.mx8855_sources",
                "//exynos/soc-series/common:exynos_soc_header",
                "//exynos/external-modules/camera:camera_header",
            ],
            outs = MODULE_OUTS,
            deps = [
                "//exynos/soc-series/common:exynos_soc_module",
                "//exynos/external-modules/camera:s5e8855_camera",
            ],
            kernel_build = "//exynos/soc-series/common:exynos_kernel_build",
            visibility = [
                "//exynos/device:__subpackages__",
                "//exynos/external-modules:__subpackages__",
            ],
        )

def register_mx8855_module():
    for variant in ["eng", "userdebug", "user"]:
        define_mx8855_module(variant)
        define_mx8855_module(variant + ".fac")
