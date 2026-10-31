// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *				http://www.samsung.com
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
#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
#include <linux/usb/f_ss_mon_gadget.h>
#endif
#if IS_ENABLED(CONFIG_USB_NOTIFY_LAYER)
#include <linux/usb_notify.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

struct sec_usb_functions *ss_usb_cb;


#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
extern int dwc3_gadget_get_cmply_link_state(void *drv_data);
static struct typec_manager_gadget_ops typec_manager_dwc3_gadget_ops = {
	.get_cmply_link_state = dwc3_gadget_get_cmply_link_state,
};
#endif

void usblog_notify_wrapper(int type, void *param1, void *param2)
{
	int temp;

	switch (type) {
	case SEC_CB_USBSTATE:
		store_usblog_notify(NOTIFY_USBSTATE, param1, param2);
		break;
	case SEC_CB_EVENT:
		temp = *(int *)param2;

		if (temp == SEC_CB_EVENT_DISABLING)
			*(int *)param2 = NOTIFY_EVENT_DISABLING;
		else if (temp == SEC_CB_EVENT_ENABLING)
			*(int *)param2 = NOTIFY_EVENT_ENABLING;

		store_usblog_notify(NOTIFY_EVENT, param1, param2);
		break;
	case SEC_CB_EXT_EVENT:
		temp = *(int *)param1;

		if (temp == SEC_CB_EXTRA_EVENT_ENABLE_USB_DATA)
			*(int *)param1 = NOTIFY_EXTRA_ENABLE_USB_DATA;
		else if (temp == SEC_CB_EXTRA_EVENT_DISABLE_USB_DATA)
			*(int *)param1 = NOTIFY_EXTRA_DISABLE_USB_DATA;

		store_usblog_notify(NOTIFY_EXTRA, param1, NULL);
		break;
	default:
		break;
	}
}

static bool is_blocked_wrapper(int type)
{
	bool ret = false;
	struct otg_notify *n = get_otg_notify();

	if (!n) {
		pr_err("%s o_notify is null\n", __func__);
		return false;
	}
	ret = is_blocked(n, type);
	return ret;
}

static int sec_usb_hook_probe(struct platform_device *pdev)
{
	int ret = 0;

	ss_usb_cb = devm_kzalloc(&pdev->dev, sizeof(struct sec_usb_functions), GFP_KERNEL);

	if (!ss_usb_cb) {
		pr_err("%s: alloc failed\n", __func__);
		return -ENOMEM;
	}

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
	ss_usb_cb->vbus_session_notify = vbus_session_notify;
	ss_usb_cb->usb_reset_notify = usb_reset_notify;
	ss_usb_cb->make_suspend_current_event = make_suspend_current_event;
#endif
#if IS_ENABLED(CONFIG_USB_NOTIFY_PROC_LOG)
	ss_usb_cb->store_usblog_notify = usblog_notify_wrapper;
#endif
	ss_usb_cb->is_blocked = is_blocked_wrapper;
	ret = register_ss_mon_funcs(ss_usb_cb);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	typec_manager_dwc3_gadget_ops.driver_data = ss_usb_cb;
	probe_typec_manager_gadget_ops(&typec_manager_dwc3_gadget_ops);
#endif

	return ret;
}

static int sec_usb_hook_remove(struct platform_device *pdev)
{
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sec_usb_hook_of_match[] = {
	{ .compatible = "samsung,sec_usb_hook", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_usb_hook_of_match);
#endif /* CONFIG_OF */

static struct platform_driver sec_usb_hook_driver = {
	.driver		= {
		.name	= "sec_usb_hook",
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_usb_hook_of_match),
#endif /* CONFIG_OF */
	},
	.probe = sec_usb_hook_probe,
	.remove = sec_usb_hook_remove,
};

module_platform_driver(sec_usb_hook_driver);

MODULE_DESCRIPTION("Samsung Electronics usb Callback hook driver");
MODULE_LICENSE("GPL");
