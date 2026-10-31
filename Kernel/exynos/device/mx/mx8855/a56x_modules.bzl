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
    "sound/soc/samsung/exynos/abox/snd-soc-samsung-abox-sync-aw.ko",
    "sound/soc/codecs/aw882xx/snd-soc-smartpa-aw882xx.ko",
    "sound/soc/samsung/exynos/sec_audio_debug.ko",
    "sound/soc/samsung/exynos/sec_audio_sysfs.ko",
    "drivers/leds/leds-s2mf301.ko",
]

model_variant_external_module_list = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
]

model_variant_external_dlkm_module_list = [
    # Please add to model variant drivers
    # Chipset common driver have to add to {PROJECT_NAME}_modules.bzl
    "is-cis-imx906.ko",
    "is-cis-3lc.ko",
    "is-cis-imx823.ko",
    "is-cis-imx258.ko",
    "is-cis-gc05a3.ko",
    "is-actuator-ak737x.ko",
    "is-flash-s2mf301.ko",
]

