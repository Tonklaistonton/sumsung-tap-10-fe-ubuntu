// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sec_debug.h>
#if IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
#include <linux/dev_ril_bridge.h>
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
#include <linux/leds-s2mf301.h>
#include <linux/muic/common/muic.h>
#include <linux/usb/typec/slsi/common/usbpd_ext.h>
#endif

#include "camera/is-vendor-interface.h"

#if IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
static int get_cp_adaptive_mipi(void)
{
	return IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO;
}
#endif

static struct is_vendor_interface_ops ops = {
	.sec_debug_get_force_upload = sec_debug_get_force_upload,
#if IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
	.register_dev_ril_bridge_event_notifier = register_dev_ril_bridge_event_notifier,
	.get_cp_adaptive_mipi = get_cp_adaptive_mipi,
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
	.sec_abc_send_event = sec_abc_send_event,
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MF301_FLASH)
	.s2mf301_fled_set_mode_ctrl = s2mf301_fled_set_mode_ctrl,
	.s2mf301_fled_set_curr = s2mf301_fled_set_curr,
	.pdo_ctrl_by_flash = pdo_ctrl_by_flash,
	.muic_afc_request_voltage = muic_afc_request_voltage,
	.muic_afc_get_voltage = muic_afc_get_voltage,
#endif
};

static int __init is_vendor_interface_init(void)
{
	int ret = 0;

	is_vendor_register_ops(&ops);
	pr_info("%s\n", __func__);

	return ret;
}

static void is_vendor_interface_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(is_vendor_interface_init);
module_exit(is_vendor_interface_exit);

MODULE_DESCRIPTION("Vendor image subsystem interface");
MODULE_LICENSE("GPL");
