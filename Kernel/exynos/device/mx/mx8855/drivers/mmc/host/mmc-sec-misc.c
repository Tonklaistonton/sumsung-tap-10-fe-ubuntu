// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Authors:
 * Storage Driver <storage.sec@samsung.com>
 */

#include "mmc-sec-misc.h"

#include <trace/hooks/mmc.h>

static int sec_sd_create_sysfs_dev(struct dw_mci *host, const char *group_name)
{
	struct device *dev;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
	dev = sec_device_create(host, group_name);
#else
	pr_err("%s: Not support to create sec dev node for %s\n",
			__func__, group_name);
	return -ENODEV;
#endif

	if (IS_ERR(dev)) {
		pr_err("%s: Failed to create device for %s\n",
				__func__, group_name);
		return -ENODEV;
	}

	/* sec specific vendor sysfs nodes */
	if (!strncmp(group_name, "sdcard", 6) && (!sec_sdcard_cmd_dev))
		sec_sdcard_cmd_dev = dev;
	else if (!strncmp(group_name, "sdinfo", 6) && (!sec_sdinfo_cmd_dev))
		sec_sdinfo_cmd_dev = dev;
	else if (!strncmp(group_name, "sddata", 6) && (!sec_sddata_cmd_dev))
		sec_sddata_cmd_dev = dev;
	else {
		pr_err("%s: Do not create dev node for %s\n",
				__func__, group_name);
		return -ENODEV;
	}

	return 0;
}

static struct mmc_sec_variant_ops sec_mmc_ops = {
	.mmc_create_sysfs_dev = sec_sd_create_sysfs_dev,
};

static int sec_mmc_driver_probe(struct platform_device *pdev)
{
	mmc_sec_register_callback(&sec_mmc_ops);

	return 0;
}

static int sec_mmc_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id sec_mmc_driver_match[] = {
	{.compatible = "samsung,mmc-sec-driver", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_mmc_driver_match);

static struct platform_driver sec_mmc_platform_driver = {
	.driver = {
		.name = "mmc-sec-driver",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_mmc_driver_match),
	},
	.probe = sec_mmc_driver_probe,
	.remove = sec_mmc_driver_remove,
};

static int __init sec_mmc_driver_init(void)
{
	return platform_driver_register(&sec_mmc_platform_driver);
}
late_initcall(sec_mmc_driver_init);

static void __exit sec_mmc_driver_exit(void)
{
	platform_driver_unregister(&sec_mmc_platform_driver);
}
module_exit(sec_mmc_driver_exit);

MODULE_AUTHOR("SeungHwan Baek <sh8267.baek@samsung.com>");
MODULE_DESCRIPTION("SEC MMC vendor feature");
MODULE_LICENSE("GPL");
