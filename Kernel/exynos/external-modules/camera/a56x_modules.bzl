# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 The Android Open Source Project

mx8855_camera_module_outs = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
    "camera/sensor/module_framework/cis/is-cis-imx906.ko",
    "camera/sensor/module_framework/cis/is-cis-3lc.ko",
    "camera/sensor/module_framework/cis/is-cis-imx258.ko",
    "camera/sensor/module_framework/cis/is-cis-gc05a3.ko",
    "camera/sensor/module_framework/cis/is-cis-imx823.ko",
    "camera/sensor/module_framework/actuator/is-actuator-ak737x.ko",
    "camera/sensor/module_framework/flash/is-flash-s2mf301.ko",
]
