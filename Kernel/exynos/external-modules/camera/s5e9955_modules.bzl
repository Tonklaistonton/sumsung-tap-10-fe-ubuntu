# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 The Android Open Source Project
load("//build/kernel/kleaf:kernel.bzl", "kernel_module")

def define_s5e9955_soc_module():
    module_outs = [
        "camera/fimc-is.ko",
        "camera/icpu/pablo-icpu.ko",
        "camera/lib/pablo-libs.ko",
        "camera/lib/votf/votf.ko",
        "camera/hardware/pablo-hws.ko",
        "camera/pablo-smc.ko",
        "camera/post/gdc/gdc.ko",
        "camera/sensor/module_framework/cis/is-cis-3lu.ko",
        "camera/sensor/module_framework/cis/is-cis-imx754.ko",
        "camera/sensor/module_framework/cis/is-cis-imx564.ko",
        "camera/sensor/module_framework/cis/is-cis-hp2.ko",
        "camera/sensor/module_framework/actuator/is-actuator-ak737x.ko",
        "camera/sensor/module_framework/flash/is-flash-s2mpb02-i2c.ko",
        "camera/post/lme/lme.ko",
        "camera/post/dof/dof.ko",
	"camera/pablo-init.ko",
    ]

    test_module_outs = [
        "camera/testing/self/pablo-self-tests.ko",
    ]

    kernel_module(
        name = "s5e9955_camera_user",
        srcs = [
            ":camera_src",
            "//exynos/soc-series/common:exynos_soc_header",
        ],
        outs = module_outs,
        kernel_build = "//exynos/soc-series/common:exynos_kernel_build",
        visibility = [ "//exynos/device:__subpackages__", ],
        deps = [ "//exynos/soc-series/common:exynos_soc_module" ],
    )

    kernel_module(
        name = "s5e9955_camera",
        srcs = [
            ":camera_src",
            "//exynos/soc-series/common:exynos_soc_header",
        ],
        outs = module_outs + test_module_outs,
        kernel_build = "//exynos/soc-series/common:exynos_kernel_build",
        visibility = [ "//exynos/device:__subpackages__", ],
        deps = [ "//exynos/soc-series/common:exynos_soc_module" ],
    )

def register_s5e9955_soc_module():
    define_s5e9955_soc_module()