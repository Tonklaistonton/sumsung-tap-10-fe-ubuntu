# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 The Android Open Source Project

mx8855_camera_module_outs = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
    "camera/sensor/module_framework/cis/is-cis-ov13a10.ko",
    "camera/sensor/module_framework/cis/is-cis-hi1337.ko",
    "camera/sensor/module_framework/actuator/is-actuator-dw9808.ko",
]
