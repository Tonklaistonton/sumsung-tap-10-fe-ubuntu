/*
 * Samsung Exynos SoC series Camera driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IS_VENDOR_INTERFACE_H
#define _IS_VENDOR_INTERFACE_H

#include "leds/leds-s2mf301.h"

struct is_vendor_interface_ops {
	int (*register_dev_ril_bridge_event_notifier)(struct notifier_block *nb);
	int (*get_cp_adaptive_mipi)(void);
	bool (*sec_debug_get_force_upload)(void);
	void (*sec_abc_send_event)(char *str);
	int (*s2mf301_fled_set_mode_ctrl)(int chan, enum cam_flash_mode cam_mode);
	int (*s2mf301_fled_set_curr)(int chan, enum cam_flash_mode cam_mode);
	void (*pdo_ctrl_by_flash)(bool mode);
	int (*muic_afc_request_voltage)(int cause, int voltage);
	int (*muic_afc_get_voltage)(void);
};

extern int is_vendor_register_ops(struct is_vendor_interface_ops *ops);

#endif /* _IS_VENDOR_INTERFACE_H */
