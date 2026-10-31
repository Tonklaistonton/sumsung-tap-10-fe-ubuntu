# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 The Android Open Source Project
load("//build/kernel/kleaf:kernel.bzl", "kernel_module")
load(":model_variant_modules.bzl", "MODEL_VARIANT_CAMERA_MODULE_LIST")

_COMMON_CAMERA_MODULE_LIST = [
	"camera/fimc-is.ko",
	"camera/icpu/pablo-icpu.ko",
	"camera/lib/pablo-libs.ko",
	"camera/lib/votf/votf.ko",
	"camera/hardware/pablo-hws.ko",
	"camera/pablo-smc.ko",
	"camera/post/gdc/gdc.ko",
]

def get_mx8855_camera_modules_list(variant = None):
    modules_list = []
    modules_list += _COMMON_CAMERA_MODULE_LIST
    modules_list += MODEL_VARIANT_CAMERA_MODULE_LIST
    return modules_list
