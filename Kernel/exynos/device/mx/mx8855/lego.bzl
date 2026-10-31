
# SPDX-License-Identifier: GPL-2.0
# COPYRIGHT(C) 2023 Samsung Electronics Co., Ltd. All Right Reserved.

lego_module_list_first = [

]

lego_module_list_second = [
"drivers/input/touchscreen/himax/hx83xxx_spi/himax_ts_83xxx_spi.ko",
"drivers/input/touchscreen/novatek/nt36523_tablet_spi/novatek_ts_nt36523.ko"
]

lego_module_list = [
"drivers/battery/battery_auth/ds28e30/sec-auth-ds28e30.ko",
"drivers/battery/battery_auth/sle956681/sec-auth-sle956681.ko",
"drivers/battery/charger/sm5440_charger/sm5440-charger.ko",
"drivers/battery/charger/sm5714_charger/sm5714-charger.ko",
"drivers/battery/common/sb_wireless.ko",
"drivers/battery/common/sec-battery.ko",
"drivers/battery/common/sec-direct-charger.ko",
"drivers/battery/common/sec_pd.ko",
"drivers/battery/core/sb-core.ko",
"drivers/battery/fuelgauge/sm5714_fuelgauge/sm5714_fuelgauge.ko",
"drivers/dpu/panel/mcd-panel-samsung-drv.ko",
"drivers/dpu/panel/mcd-panel-samsung-helper.ko",
"drivers/fingerprint/fingerprint.ko",
"drivers/fingerprint/fingerprint_sysfs.ko",
"drivers/gpu/drm/samsung/panel/mcd-panel.ko",
"drivers/gpu/drm/samsung/panel/tft_common/mcd-panel-hx83102j_gts10fe_boe.ko",
"drivers/gpu/drm/samsung/panel/tft_common/mcd-panel-nt36523n_gts10fe_csot.ko",
"drivers/gpu/drm/samsung/panel/tft_common/usdm-panel-tft-common.ko",
"drivers/input/input_boost/input_booster_lkm.ko",
"drivers/input/misc/hall/hall_ic.ko",
"drivers/input/misc/hall/hall_ic_notifier.ko",
"drivers/input/sec_input/sec_common_fn.ko",
"drivers/input/sec_input/sec_input_notifier.ko",
"drivers/input/sec_input/stm32/stm32_pogo_v3.ko",
"drivers/input/wacom/wez01.ko",
"drivers/knox/hdm/hdm.ko",
"drivers/knox/kzt/kzt.ko",
"drivers/knox/ngksm/ngksm.ko",
"drivers/lego/lego.ko",
"drivers/mfd/sm/sm5714/mfd_sm5714.ko",
"drivers/misc/drb/dev_ril_bridge.ko",
"drivers/muic/common/common_muic.ko",
"drivers/muic/sm/sm5714/muic_sm5714.ko",
"drivers/nfc/snvm/snvm.ko",
"drivers/phy/common/usb_repeater_module.ko",
"drivers/phy/nxp/ptn3222/repeater_ptn3222_module.ko",
"drivers/phy/ti/tusb2e11/repeater_tusb2e11_module.ko",
"drivers/samsung/debug/sec_debug.ko",
"drivers/samsung/debug/sec_debug_base_early.ko",
"drivers/samsung/debug/sec_debug_dprm.ko",
"drivers/samsung/debug/sec_debug_dprt.ko",
"drivers/samsung/debug/sec_debug_dtask.ko",
"drivers/samsung/debug/sec_debug_extra_info.ko",
"drivers/samsung/debug/sec_debug_hardlockup_info.ko",
"drivers/samsung/debug/sec_debug_hw_param.ko",
"drivers/samsung/debug/sec_debug_kerror_report.ko",
"drivers/samsung/debug/sec_debug_mode.ko",
"drivers/samsung/debug/sec_debug_reset_reason.ko",
"drivers/samsung/debug/sec_debug_sched_info.ko",
"drivers/samsung/debug/sec_debug_sched_report.ko",
"drivers/samsung/debug/sec_debug_slab_info.ko",
"drivers/samsung/debug/sec_debug_softdog.ko",
"drivers/samsung/debug/sec_debug_stacktrace.ko",
"drivers/samsung/debug/sec_debug_test.ko",
"drivers/samsung/debug/sec_debug_wdd_info.ko",
"drivers/samsung/debug/softdog.ko",
"drivers/samsung/factory/sec_reloc_gpio.ko",
"drivers/samsung/pm/sec_thermistor/sec_thermistor.ko",
"drivers/samsung/sec_bootstat.ko",
"drivers/samsung/sec_class.ko",
"drivers/samsung/sec_crash_key_user.ko",
"drivers/samsung/sec_hard_reset_hook.ko",
"drivers/samsung/sec_key_notifier.ko",
"drivers/samsung/sec_reboot.ko",
"drivers/sec_panel_notifier_v2/sec_panel_notifier_v2.ko",
"drivers/sensorhub/shub.ko",
"drivers/sensorhub/utility/sensor_core.ko",
"drivers/sensors_lego/isg6320.ko",
"drivers/staging/android/switch/sec_switch_class.ko",
"drivers/sti/abc/abc.ko",
"drivers/sti/abc/abc_hub.ko",
"drivers/usb/common/vbus_notifier/vbus_notifier.ko",
"drivers/usb/notify/usb_notifier.ko",
"drivers/usb/notify/usb_notify_layer.ko",
"drivers/usb/typec/common/pdic_notifier_module.ko",
"drivers/usb/typec/manager/if_cb_manager.ko",
"drivers/usb/typec/manager/usb_typec_manager.ko",
"drivers/usb/typec/sm/sm5714/pdic_sm5714.ko"
]

lego_dtbo_list = [
"gts10fewifi_eur_open_w00_r05.dtbo",
"gts10fewifi_eur_open_w00_r04.dtbo",
"gts10fewifi_eur_open_w00_r03.dtbo",
"gts10fewifi_eur_open_w00_r01.dtbo",
"gts10fewifi_eur_open_w00_r00.dtbo"
]
lego_model = 'gts10fewifi'