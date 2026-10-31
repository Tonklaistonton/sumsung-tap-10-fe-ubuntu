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

# common_kernel's module. In vendor_boot.
_COMMON_KERNEL_MODULE_LIST = [
    "drivers/gpu/drm/drm_ttm_helper.ko",
    "drivers/gpu/drm/scheduler/gpu-sched.ko",
    "drivers/gpu/drm/display/drm_display_helper.ko",
    "drivers/firmware/cirrus/cs_dsp.ko",
    "drivers/spi/spidev.ko",
    "drivers/usb/host/xhci-sideband.ko",
]

_COMMON_KERNEL_VENDOR_DLKM_MODULE_LIST = [
    "net/wireless/cfg80211.ko",
]

# vendor_boot modules. In vendor_boot
_VENDOR_MODULE_LIST = [
]

# Only for debug/developement. In vendor_boot
_VENDOR_DEV_MODULE_LIST = [
]

# Only for debug/developement. In vendor_dlkm
_VENDOR_DEV_DLKM_MODULE_LIST = [
    "pablo-self-tests.ko",
]

# In vendor_dlkm
_VENDOR_DLKM_MODULE_LIST = [
    "sec_ts.ko",
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

_VENDOR_KUNIT_TEST_LIST = [
    "sgpu_exynos_test.ko",
]

# external-modules. In vendor_dlkm
_EXTERNAL_MODULE_IN_DLKM_LIST = [
    "fimc-is.ko",
    "pablo-libs.ko",
    "pablo-smc.ko",
    "votf.ko",
    "pablo-hws.ko",
    "pablo-icpu.ko",
    "is-cis-hm6.ko",
    "is-cis-4ha.ko",
    "is-cis-2la.ko",
    "is-cis-3lu.ko",
    "is-actuator-ak737x.ko",
    "is-flash-s2mf301.ko",
    "gdc.ko",
    "pablo-init.ko",
]

# external-modules. In vendor_dlkm
_EXTERNAL_MODULE_BOOT_COMPLETE_IN_DLKM_LIST = [
]

def get_erd8855_kunit_test_list(variant = None):
    modules_list = []
    if variant != "user":
        modules_list += _VENDOR_KUNIT_TEST_LIST
    return modules_list

def get_erd8855_system_dlkm_blocklist():
    modules_list = []
    modules_list += _SYSTEM_DLKM_BLOCKLIST
    return modules_list

def get_erd8855_vendor_dlkm_blocklist():
    modules_list = []
    modules_list += _VENDOR_DLKM_BLOCKLIST
    return modules_list

def get_erd8855_vendor_dlkm_modules_list(variant = None):
    modules_list = []
    modules_list += _VENDOR_DLKM_MODULE_LIST
    modules_list += _VENDOR_DLKM_BOOT_COMPLETE_MODULE_LIST
    if variant != "user":
        modules_list += _VENDOR_DEV_DLKM_MODULE_LIST
    modules_list += get_erd8855_vendor_dlkm_blocklist()
    return modules_list

# All modules built from common_kernel
def get_common_kernel_modules_list():
    modules_list = []
    modules_list += _COMMON_KERNEL_MODULE_LIST
    modules_list += _COMMON_KERNEL_VENDOR_DLKM_MODULE_LIST
    return modules_list

def get_erd8855_vendor_boot_modules_list(variant = None):
    modules_list = []
    modules_list += _VENDOR_MODULE_LIST
    if variant != "user":
        modules_list += _VENDOR_DEV_MODULE_LIST
    return modules_list

# All modules included in vendor boot
def get_all_vendor_boot_modules_list(variant = None):
    modules_list = []
    modules_list += get_erd8855_vendor_boot_modules_list(variant)
    modules_list += get_soc_vendor_boot_modules_list(variant)
    modules_list += _COMMON_KERNEL_MODULE_LIST
    modules_list += _EXTERNAL_MODULE_LIST
    return modules_list

def get_all_system_dlkm_blocklist():
    modules_list = []
    modules_list += get_soc_system_dlkm_blocklist()
    modules_list += get_erd8855_system_dlkm_blocklist()
    return modules_list

# All vendor_dlkm blocklist included in vendor_boot
def get_all_vendor_dlkm_blocklist():
    modules_list = []
    modules_list += get_soc_vendor_dlkm_blocklist()
    modules_list += get_erd8855_vendor_dlkm_blocklist()
    return modules_list

def get_all_kunit_test_list(variant = None):
    modules_list = []
    modules_list += get_soc_vendor_kunit_test_list(variant)
    modules_list += get_erd8855_kunit_test_list(variant)
    return modules_list

# All modules included in soc_vendor_dlkm
def get_all_vendor_dlkm_modules_list(variant = None):
    modules_list = []
    modules_list += _COMMON_KERNEL_VENDOR_DLKM_MODULE_LIST
    modules_list += get_soc_vendor_dlkm_modules_list(variant)
    modules_list += get_erd8855_vendor_dlkm_modules_list(variant)
    modules_list += _EXTERNAL_MODULE_IN_DLKM_LIST
    return modules_list

# All modules built from this directory
def get_erd8855_vendor_modules_list(variant = None):
    modules_list = []
    modules_list += get_erd8855_vendor_boot_modules_list(variant)
    modules_list += get_erd8855_vendor_dlkm_modules_list(variant)
    modules_list += get_erd8855_kunit_test_list(variant)
    return modules_list

def define_erd8855_module(variant = None):
    MODULE_OUTS = get_erd8855_vendor_modules_list(variant)
    if MODULE_OUTS:
        kernel_module(
            name = "erd8855_module.{}".format(variant),
             srcs = [
                ":exynos.erd8855_sources",
            ],
            outs = MODULE_OUTS,
            kernel_build = "//exynos/soc-series/common:exynos_kernel_build",
            visibility = [
                "//exynos/device:__subpackages__",
            ],
        )

def register_erd8855_module():
    define_erd8855_module("eng")
    define_erd8855_module("userdebug")
    define_erd8855_module("user")
