 // SPDX-License-Identifier: GPL-2.0
 /*
  * Copyright (c) 2024 Samsung Electronics Co., Ltd.
  * http://www.samsung.com
  *
  * Author: NAM-HEE PARK <namh.park@samsung.com>
  *
  * Support for USB PHY and repeater
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

#define pr_fmt(fmt)	"[USB_PHY] " fmt

#include <linux/of.h>
#include <linux/platform_device.h>

#include "sec_repeater_cb.h"

static struct sec_repeater_cb_data *repeater_cb_data;

int cb_repeater_enable(bool en)
{
	if (!repeater_cb_data->repeater_enable) {
		dev_err(repeater_cb_data->dev, "%s: not registered\n", __func__);
		return -EEXIST;
	}

	repeater_cb_data->repeater_enable(en);
	pr_info("%s %d\n", __func__, en);

	return 0;
}
EXPORT_SYMBOL_GPL(cb_repeater_enable);

int register_repeater_enable(int (*repeater_enable) (bool en))
{
	if (repeater_cb_data->repeater_enable) {
		dev_err(repeater_cb_data->dev, "%s: Already registered\n", __func__);
		return -EEXIST;
	}

	repeater_cb_data->repeater_enable = repeater_enable;

	return 0;
}
EXPORT_SYMBOL_GPL(register_repeater_enable);

static int sec_repeater_cb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	repeater_cb_data = devm_kzalloc(dev, sizeof(*repeater_cb_data), GFP_KERNEL);
	if (unlikely(!repeater_cb_data)) {
		pr_err("%s out of memory\n", __func__);
		return -ENOMEM;
	}

	repeater_cb_data->dev = dev;
	return 0;
}

static int sec_repeater_cb_remove(struct platform_device *pdev)
{
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sec_repeater_cb_of_match[] = {
	{ .compatible = "samsung,repeater_cb", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_repeater_cb_of_match);
#endif /* CONFIG_OF */

static struct platform_driver sec_repeater_cb_driver = {
	.driver		= {
		.name	= "sec_repeater_cb",
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_repeater_cb_of_match),
#endif /* CONFIG_OF */
	},

	.probe = sec_repeater_cb_probe,
	.remove = sec_repeater_cb_remove,
};

module_platform_driver(sec_repeater_cb_driver);

MODULE_DESCRIPTION("Samsung Electronics Repeater Callback driver");
MODULE_LICENSE("GPL");
