/*
 * Driver for Samsung ESCA MFD BUS
 *
 * Copyright (C) 2019 Samsung Electronics Ltd.
 * 	Hyeonseong Gil <hs.gil@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

struct exynos_esca_mfd_bus {
	struct i2c_adapter	adap;

	struct device		*dev;
};

static int exynos_esca_mfd_bus_xfer(struct i2c_adapter *adap,
			  struct i2c_msg *msgs, int num)
{
	return 0;
}

static u32 exynos_esca_mfd_bus_func(struct i2c_adapter *adap)
{
        return I2C_FUNC_I2C;
}

static const struct i2c_algorithm exynos_esca_mfd_bus_algorithm = {
        .master_xfer            = exynos_esca_mfd_bus_xfer,
        .functionality          = exynos_esca_mfd_bus_func,
};

static int exynos_esca_mfd_bus_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct exynos_esca_mfd_bus *esca_mfd_bus;
	int ret;

	dev_info(&pdev->dev, "esca mfd bus driver probe started\n");

	if (!np) {
		dev_err(&pdev->dev, "no device node\n");
		return -ENOENT;
	}

	esca_mfd_bus = devm_kzalloc(&pdev->dev, sizeof(struct exynos_esca_mfd_bus), GFP_KERNEL);
	if (!esca_mfd_bus) {
		dev_err(&pdev->dev, "no memory for driver data\n");
		return -ENOMEM;
	}

	strlcpy(esca_mfd_bus->adap.name, "exynos-esca-mfd-bus", sizeof(esca_mfd_bus->adap.name));
	esca_mfd_bus->adap.owner   = THIS_MODULE;
	esca_mfd_bus->adap.algo    = &exynos_esca_mfd_bus_algorithm;
	esca_mfd_bus->adap.retries = 2;
	esca_mfd_bus->adap.class   = I2C_CLASS_HWMON | I2C_CLASS_SPD;

	esca_mfd_bus->dev = &pdev->dev;

	esca_mfd_bus->adap.dev.of_node = np;
	esca_mfd_bus->adap.algo_data = esca_mfd_bus;
	esca_mfd_bus->adap.dev.parent = &pdev->dev;

	platform_set_drvdata(pdev, esca_mfd_bus);

	esca_mfd_bus->adap.nr = -1;
	ret = i2c_add_numbered_adapter(&esca_mfd_bus->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add bus to i2c core\n");
		goto err_probe;
	}

	dev_info(&pdev->dev, "esca mfd bus driver probe was succeeded\n");

	return 0;

 err_probe:
	dev_err(&pdev->dev, "esca mfd bus driver probe failed\n");
	return ret;
}

static const struct of_device_id exynos_esca_mfd_bus_match[] = {
	{ .compatible = "samsung,exynos-esca-mfd-bus" },
	{}
};
MODULE_DEVICE_TABLE(of, exynos_esca_mfd_bus_match);

static struct platform_driver exynos_esca_mfd_bus_driver = {
	.probe		= exynos_esca_mfd_bus_probe,
	.driver		= {
		.name	= "exynos-esca-mfd-bus",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_esca_mfd_bus_match,
	},
};

static int __init exynos_esca_mfd_bus_init(void)
{
	return platform_driver_register(&exynos_esca_mfd_bus_driver);
}
subsys_initcall(exynos_esca_mfd_bus_init);

static void __exit exynos_esca_mfd_bus_exit(void)
{
	platform_driver_unregister(&exynos_esca_mfd_bus_driver);
}
module_exit(exynos_esca_mfd_bus_exit);

MODULE_DESCRIPTION("Exynos ESCA MFD bus driver");
MODULE_AUTHOR("Hyeonseong Gil, <hs.gil@samsung.com>");
MODULE_LICENSE("GPL v2");
