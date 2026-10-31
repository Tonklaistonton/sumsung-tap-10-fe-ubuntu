/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts.h"
#include "hts_backup.h"
#include "hts_vh.h"
#include "hts_core.h"
#include "hts_devfs.h"
#include "hts_sysfs.h"
#include "hts_ext_dev.h"
#include "hts_etc.h"

#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/dev_printk.h>

static int hts_initialize(struct hts_drvdata *drvdata)
{
	int ret;

	ret = hts_ext_dev_initialize(drvdata);
	if (ret)
		return ret;
	
	ret = hts_vh_register(drvdata);
	if (ret)
		return ret;

	ret = hts_pmu_initialize(drvdata);
	if (ret)
		return ret;

	ret = hts_core_initialize(drvdata);
	if (ret)
		return ret;

	ret = hts_devfs_initialize(drvdata);
	if (ret)
		return ret;

	ret = hts_sysfs_initialize(drvdata);
	if (ret)
		return ret;

	return 0;
}

static int hts_probe(struct platform_device *pdev)
{
	int ret;
	struct hts_drvdata *drvdata;

	drvdata = kzalloc(sizeof(struct hts_drvdata), GFP_KERNEL);
	if (drvdata == NULL) {
		dev_err(&pdev->dev, "Couldn't allocate device data");
		return -ENOMEM;
	}

	drvdata->pdev = pdev;
	platform_set_drvdata(pdev, drvdata);

	ret = hts_initialize(drvdata);
	if (ret) {
		dev_err(&pdev->dev, "Coulnd't initialized HTS successfully - %d", ret);
		return ret;
	}

	hts_etc_probing_finish(drvdata);

	return 0;
}

static const struct of_device_id of_hts_match[] = {
	{ .compatible = "samsung,hts", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_hts_match);

static struct platform_driver hts_driver = {
	.driver = {
		.name = "hts",
		.owner = THIS_MODULE,
		.of_match_table = of_hts_match,
	},
	.probe	= hts_probe,
};

static int __init hts_init(void)
{
	return platform_driver_register(&hts_driver);
}
arch_initcall(hts_init);

static void __exit hts_exit(void)
{
	platform_driver_unregister(&hts_driver);
}
module_exit(hts_exit);

MODULE_LICENSE("GPL");

