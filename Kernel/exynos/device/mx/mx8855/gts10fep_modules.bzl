# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 The Android Open Source Project

model_variant_common_kernel_module_list = [
    # Please add to model variant common kernel drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
]

model_variant_vendor_boot_module_list = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
]

model_variant_vendor_dlkm_module_list = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
    "sound/soc/samsung/exynos/exynos8855_audio.ko",
    "sound/soc/samsung/exynos/abox/snd-soc-samsung-abox-sync-ti.ko",
    "sound/soc/codecs/tas25xx/snd-soc-tas25xx.ko",
    "sound/soc/samsung/exynos/sec_audio_debug.ko",
    "sound/soc/samsung/exynos/sec_audio_sysfs.ko",
]

model_variant_external_module_list = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
]

model_variant_external_dlkm_module_list = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
    "is-cis-ov13a10.ko",
    "is-cis-hi1337.ko",
    "is-actuator-dw9808.ko",
]

