# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 The Android Open Source Project
load("//build/kernel/kleaf:kernel.bzl", "kernel_module")
load(":model_variant_modules.bzl", "mx8855_camera_module_outs")

def define_s5e8855_soc_module():
    module_outs = [
        "camera/fimc-is.ko",
        "camera/icpu/pablo-icpu.ko",
        "camera/lib/pablo-libs.ko",
        "camera/lib/votf/votf.ko",
        "camera/hardware/pablo-hws.ko",
        "camera/pablo-smc.ko",
        "camera/post/gdc/gdc.ko",
	"camera/pablo-init.ko",
    ]

    test_module_outs = [
        "camera/testing/self/pablo-self-tests.ko",
    ]

    kernel_module(
        name = "s5e8855_camera",
        srcs = [
            ":camera_src",
            "//exynos/soc-series/common:exynos_soc_header",
        ],
        outs = module_outs + test_module_outs + mx8855_camera_module_outs,
        kernel_build = "//exynos/soc-series/common:exynos_kernel_build",
        visibility = [ "//exynos/device:__subpackages__", ],
        deps = [ "//exynos/soc-series/common:exynos_soc_module" ],
    )

def register_s5e8855_soc_module():
    define_s5e8855_soc_module()
