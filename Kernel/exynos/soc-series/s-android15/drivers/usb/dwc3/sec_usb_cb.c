 // SPDX-License-Identifier: GPL-2.0
 /*
  * Copyright (c) 2024 Samsung Electronics Co., Ltd.
  * http://www.samsung.com
  *
  * Author: NAM-HEE PARK <namh.park@samsung.com>
  *
  * Support for USB functions
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  */
#include <linux/of.h>
#include <linux/platform_device.h>

#include <linux/usb/sec_usb_cb.h>

struct sec_usb_cb_data {
	struct device *dev;
	struct sec_usb_functions *ss_usb_funcs;
};

static struct sec_usb_cb_data *usb_cb_data;

void usb_reset_notify_cb(struct usb_gadget *gadget)
{
	if (!usb_cb_data || !usb_cb_data->ss_usb_funcs ||
			!usb_cb_data->ss_usb_funcs->usb_reset_notify) {
		pr_err("usb: %s: not registered\n", __func__);
		return;
	}
	usb_cb_data->ss_usb_funcs->usb_reset_notify(gadget);
}
EXPORT_SYMBOL_GPL(usb_reset_notify_cb);

void vbus_session_notify_cb(struct usb_gadget *gadget, int is_active, int ret)
{
	if (!usb_cb_data || !usb_cb_data->ss_usb_funcs ||
			!usb_cb_data->ss_usb_funcs->vbus_session_notify) {
		pr_err("usb: %s: not registered\n", __func__);
		return;
	}

	usb_cb_data->ss_usb_funcs->vbus_session_notify(gadget, is_active, ret);
}
EXPORT_SYMBOL_GPL(vbus_session_notify_cb);

void make_suspend_current_event_cb(void)
{
	if (!usb_cb_data || !usb_cb_data->ss_usb_funcs ||
			!usb_cb_data->ss_usb_funcs->make_suspend_current_event) {
		pr_err("usb: %s: not registered\n", __func__);
		return;
	}
	usb_cb_data->ss_usb_funcs->make_suspend_current_event();
}
EXPORT_SYMBOL_GPL(make_suspend_current_event_cb);

void store_usblog_notify_cb(int type, void *param1, void *param2)
{
	if (!usb_cb_data || !usb_cb_data->ss_usb_funcs ||
			!usb_cb_data->ss_usb_funcs->store_usblog_notify) {
		pr_err("usb: %s: not registered\n", __func__);
		return;
	}
	usb_cb_data->ss_usb_funcs->store_usblog_notify(type, param1, param2);
}
EXPORT_SYMBOL_GPL(store_usblog_notify_cb);

bool is_blocked_cb(int type)
{
	bool ret = false;

	if (!usb_cb_data || !usb_cb_data->ss_usb_funcs ||
			!usb_cb_data->ss_usb_funcs->is_blocked) {
		pr_err("usb: %s: not registered\n", __func__);
		return false;
	}
		ret = usb_cb_data->ss_usb_funcs->is_blocked(type);
		return ret;
}
EXPORT_SYMBOL_GPL(is_blocked_cb);

int register_ss_mon_funcs(struct sec_usb_functions *funcs)
{

	if (!usb_cb_data)
		return -EEXIST;

	usb_cb_data->ss_usb_funcs = funcs;

	return 0;
}
EXPORT_SYMBOL_GPL(register_ss_mon_funcs);

static int sec_usb_cb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("usb: %s\n", __func__);

	usb_cb_data = devm_kzalloc(&pdev->dev, sizeof(struct sec_usb_cb_data), GFP_KERNEL);

	if (!usb_cb_data) {
		pr_err("usb: %s: alloc failed\n", __func__);
		return -ENOMEM;
	}

	usb_cb_data->dev = dev;
	return 0;
}

static int sec_usb_cb_remove(struct platform_device *pdev)
{
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sec_usb_cb_of_match[] = {
	{ .compatible = "samsung,sec_usb_cb", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_usb_cb_of_match);
#endif /* CONFIG_OF */

static struct platform_driver sec_usb_cb_driver = {
	.driver		= {
		.name	= "sec_usb_cb",
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_usb_cb_of_match),
#endif /* CONFIG_OF */
	},

	.probe = sec_usb_cb_probe,
	.remove = sec_usb_cb_remove,
};

module_platform_driver(sec_usb_cb_driver);

MODULE_DESCRIPTION("Samsung Electronics usb Callback driver");
MODULE_LICENSE("GPL");
