// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 *
 * Authors:
 * Storage Driver <storage.sec@samsung.com>
 */

#include "ufs-sec-misc.h"

#include <trace/hooks/ufshcd.h>

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

static int sec_ufs_create_sysfs_dev(struct ufs_hba *hba)
{
	/* sec specific vendor sysfs nodes */
	if (!sec_ufs_node_dev) {
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
		sec_ufs_node_dev = sec_device_create(hba, "ufs");
#else
		/*
		 * If sec_ufs_node_dev is NULL, IS_ERR(NULL) is false
		 * and sysfs create goes to kernel panic. So return -ENODEV to avoid kernel panic.
		 */
		pr_err("Fail to create dev node\n");
		return -ENODEV;
#endif
	}

	if (IS_ERR(sec_ufs_node_dev)) {
		pr_err("Fail to create sysfs dev\n");
		return -ENODEV;
	}

	return 0;
}

static int sec_ufs_mode_enter_upload(void)
{
#if IS_ENABLED(CONFIG_SEC_DEBUG)
	return sec_debug_get_force_upload();
#else
	return 0;
#endif
}

static void sec_ufs_exin_ufs(char *str)
{
	secdbg_exin_set_ufs(str);
}

static void sec_ufs_abc_event(char *str)
{
#if IS_ENABLED(CONFIG_SEC_ABC)
	sec_abc_send_event(str);
#endif
}

static struct ufs_sec_variant_ops sec_ufs_ops = {
	.create_sysfs_dev = sec_ufs_create_sysfs_dev,
	.mode_enter_upload = sec_ufs_mode_enter_upload,
	.exin_ufs = sec_ufs_exin_ufs,
	.abc_event = sec_ufs_abc_event
};

static int sec_ufs_driver_probe(struct platform_device *pdev)
{
	ufs_sec_register_callback(&sec_ufs_ops);

	return 0;
}

static int sec_ufs_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id sec_ufs_driver_match[] = {
	{.compatible = "samsung,ufs-sec-driver", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_ufs_driver_match);

static struct platform_driver sec_ufs_platform_driver = {
	.driver = {
		.name = "ufs-sec-driver",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_ufs_driver_match),
	},
	.probe = sec_ufs_driver_probe,
	.remove = sec_ufs_driver_remove,
};

static int __init sec_ufs_driver_init(void)
{
	return platform_driver_register(&sec_ufs_platform_driver);
}
late_initcall(sec_ufs_driver_init);

static void __exit sec_ufs_driver_exit(void)
{
	platform_driver_unregister(&sec_ufs_platform_driver);
}
module_exit(sec_ufs_driver_exit);

MODULE_AUTHOR("SeungHwan Baek <sh8267.baek@samsung.com>");
MODULE_DESCRIPTION("SEC UFS vendor feature");
MODULE_LICENSE("GPL");
