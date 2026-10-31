// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[@][PCC] " fmt

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/moduleparam.h>

#include "api/pablo-hw-api-common-ctrl.h"
#include "pablo-hw-common-ctrl.h"
#include "pablo-debug.h"

static struct pablo_common_ctrl_hw *pcc_hw;

struct pablo_common_ctrl_dbg_param_info dbg_param_info;

typedef int (*dbg_parser)(char **argv, int argc);
struct dbg_parser_info {
	char name[NAME_MAX];
	char man[NAME_MAX];
	dbg_parser parser;
};

/* Internal functions */
static int pablo_common_ctrl_hw_register_api(struct pablo_common_ctrl_hw *hw, struct device *hw_dev)
{
	struct device_node *node = hw_dev->of_node;
	struct of_phandle_iterator iter;
	struct platform_device *pdev;
	struct device *dev;
	struct resource *res;
	struct pablo_common_ctrl *pcc;
	struct pmio_config *pcfg;
	int ret;
	u32 pcc_num, i = 0;

	pcc_num = of_count_phandle_with_args(node, "pccs", NULL);
	if (!pcc_num) {
		dev_err(hw_dev, "There are no pccs\n");
		return -ENODEV;
	}

	hw->pcc_num = pcc_num;
	hw->pccs = devm_kcalloc(hw_dev, pcc_num, sizeof(*(hw->pccs)), GFP_KERNEL);
	if (!hw->pccs) {
		dev_err(hw_dev, "failed to allocate pccs\n");
		return -ENODEV;
	}

	of_for_each_phandle(&iter, ret, node, "pccs", NULL, 0) {
		pdev = of_find_device_by_node(iter.node);
		dev = &pdev->dev;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res) {
			dev_err(dev, "failed to get base_addr\n");
			ret = -EINVAL;
			goto err_get_resource;
		}

		pcc = &hw->pccs[i];
		pcfg = &pcc->pmio_config;
		pcfg->phys_base = res->start;

		platform_set_drvdata(pdev, pcc);

		dev_info(dev, "[I%d] registered\n", i);
		i++;
	}

#if IS_ENABLED(CONFIG_PABLO_CMN_CTRL_API_V1_4)
	ret = platform_driver_register(pablo_common_ctrl_api_get_platform_driver_v1_4());
	if (ret) {
		dev_err(hw_dev, "pablo_common_ctrl_api_v1_4 register failed(%d)", ret);
		goto err_register;
	}
#endif

#if IS_ENABLED(CONFIG_PABLO_CMN_CTRL_API_V2_0)
	ret = platform_driver_register(pablo_common_ctrl_api_get_platform_driver_v2_0());
	if (ret) {
		dev_err(hw_dev, "pablo_common_ctrl_api_v2_0 register failed(%d)", ret);
		goto err_register;
	}
#endif

	return ret;

err_register:
err_get_resource:
	devm_kfree(hw_dev, hw->pccs);

	return ret;
}

/* Public functions */
struct pablo_common_ctrl *pablo_common_ctrl_hw_get_pcc(struct pmio_config *cfg)
{
	struct pablo_common_ctrl *pcc;
	u32 i;

	if (!cfg) {
		pr_err("pcc_cfg is NULL!\n");
		return NULL;
	} else if (!cfg->phys_base) {
		pr_err("Invalid phys_base\n");
		return NULL;
	}

	if (!pcc_hw->pcc_num) {
		pr_err("There is no pcc!\n");
		return NULL;
	}

	for (i = 0; i < pcc_hw->pcc_num; i++) {
		pcc = &pcc_hw->pccs[i];
		if (pcc->pmio_config.phys_base == cfg->phys_base)
			return pcc;
	}

	pr_warn("Failed to find PCC for 0x%08x\n", (u32)cfg->phys_base);

	return NULL;
}
EXPORT_SYMBOL_GPL(pablo_common_ctrl_hw_get_pcc);

/* PCC module_param_cb functions */
static int _parse_dbg_dump(char **argv, int argc)
{
	int ret;
	u32 argv_i = 0, i;
	u32 mode_msk, int_msk[PCC_INT_ID_NUM];

	if (argc != (PCC_INT_ID_NUM + 1)) {
		pr_err("[DBG] Invalid arguments! %d\n", argc);
		return -EINVAL;
	}

	ret = kstrtouint(argv[argv_i++], 0, &mode_msk);
	if (ret) {
		pr_err("[DBG] Invalid mode_msk 0x%x ret %d\n", mode_msk, ret);
		return -EINVAL;
	}

	for (i = 0; i < PCC_INT_ID_NUM; i++) {
		ret = kstrtouint(argv[argv_i++], 0, &int_msk[i]);
		if (ret) {
			pr_err("[DBG] Invalid int_msk[%d] 0x%08x ret %d\n", i, int_msk[i], ret);
			return -EINVAL;
		}
	}

	dbg_param_info.mode_msk = mode_msk;
	memcpy(dbg_param_info.int_msk, int_msk, sizeof(int_msk));

	return 0;
}

static struct dbg_parser_info dbg_parsers[PCC_DBG_TYPE_NUM] = {
	[PCC_DBG_DUMP] = {
		"DUMP",
		"<pcc_mode_msk> <int0_msk> <int1_msk> <cmdq_int_msk> <corex_int_msk>",
		_parse_dbg_dump,
	},
};

static int pablo_common_ctrl_dbg_set(const char *val)
{
	int ret = 0, argc = 0;
	char **argv;
	u32 dbg_type, en, arg_i = 0;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("[DBG] No arguments!\n");
		return -EINVAL;
	} else if (argc < 2) {
		pr_err("[DBG] Too short argument!\n");
		goto exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &dbg_type);
	if (ret || dbg_type >= PCC_DBG_TYPE_NUM) {
		pr_err("[DBG] Invalid dbg_type %u ret %d\n", dbg_type, ret);
		goto exit;
	}

	ret = kstrtouint(argv[arg_i++], 0, &en);
	if (ret) {
		pr_err("[DBG] Invalid en %u ret %d\n", en, ret);
		goto exit;
	}

	dbg_param_info.en[dbg_type] = en;
	pr_info("[DBG] %s[%s]\n", dbg_parsers[dbg_type].name,
		GET_EN_STR(dbg_param_info.en[dbg_type]));

	argc = (argc > arg_i) ? (argc - arg_i) : 0;
	if (argc && dbg_parsers[dbg_type].parser &&
		dbg_parsers[dbg_type].parser(&argv[arg_i], argc)) {
		pr_err("[DBG] Failed to %s\n", dbg_parsers[dbg_type].name);
		goto exit;
	}

exit:
	argv_free(argv);
	return ret;
}

static int pablo_common_ctrl_dbg_get(char *buffer, const size_t buf_size)
{
	int ret;
	u32 dbg_type;
	const char *get_msg =
		"= PCC DEBUG Configuration =====================\n"
		"  DBG Infos:\n"
		"    - MODE_MASK: 0x%x\n"
		"    - INT_MASK: INT0(0x%08x) INT1(0x%08x) CMDQ(0x%08x) COREX(0x%08x)\n"
		"===============================================\n";

	ret = scnprintf(buffer, buf_size, get_msg, dbg_param_info.mode_msk,
		dbg_param_info.int_msk[PCC_INT_0], dbg_param_info.int_msk[PCC_INT_1],
		dbg_param_info.int_msk[PCC_CMDQ_INT], dbg_param_info.int_msk[PCC_COREX_INT]);

	for (dbg_type = 0; dbg_type < PCC_DBG_TYPE_NUM; dbg_type++)
		ret += scnprintf(buffer + ret, buf_size - ret, "%10s[%3s]\n",
			dbg_parsers[dbg_type].name, GET_EN_STR(dbg_param_info.en[dbg_type]));

	return ret;
}

static int pablo_common_ctrl_dbg_usage(char *buffer, const size_t buf_size)
{
	int ret = 0;
	u32 dbg_type;

	for (dbg_type = 0; dbg_type < PCC_DBG_TYPE_NUM; dbg_type++)
		ret += scnprintf(buffer + ret, buf_size - ret,
			"    %10s : echo %d <en> %s > debug_pcc\n", dbg_parsers[dbg_type].name,
			dbg_type, dbg_parsers[dbg_type].man);

	return ret;
}

static struct pablo_debug_param debug_pcc = {
	.type = IS_DEBUG_PARAM_TYPE_STR,
	.ops.set = pablo_common_ctrl_dbg_set,
	.ops.get = pablo_common_ctrl_dbg_get,
	.ops.usage = pablo_common_ctrl_dbg_usage,
};
module_param_cb(debug_pcc, &pablo_debug_param_ops, &debug_pcc, 0644);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
const struct kernel_param *pablo_common_ctrl_get_kernel_param(void)
{
	return G_KERNEL_PARAM(debug_pcc);
}
KUNIT_EXPORT_SYMBOL(pablo_common_ctrl_get_kernel_param);
#endif

/* Driver functions */
static int pablo_common_ctrl_hw_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pablo_common_ctrl_hw *hw;
	int ret;

	hw = devm_kzalloc(dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	hw->dev = dev;
	pcc_hw = hw;
	platform_set_drvdata(pdev, hw);

	ret = pablo_common_ctrl_hw_register_api(hw, dev);
	if (ret)
		goto err_register_api;

	dev_info(dev, "%s done\n", __func__);

	return 0;

err_register_api:
	devm_kfree(dev, hw);

	return ret;
}

static int pablo_common_ctrl_hw_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id pablo_common_ctrl_hw_of_table[] = {
	{
		.name = "pablo_common_ctrl",
		.compatible = "samsung,pablo-common-ctrl",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pablo_common_ctrl_hw_of_table);

static struct platform_driver pablo_common_ctrl_hw_driver = {
	.probe = pablo_common_ctrl_hw_probe,
	.remove = pablo_common_ctrl_hw_remove,
	.driver = {
		.name = "pablo_common_ctrl",
		.owner = THIS_MODULE,
		.of_match_table = pablo_common_ctrl_hw_of_table,
	},
};

struct platform_driver *pablo_common_ctrl_hw_get_driver(void)
{
	return &pablo_common_ctrl_hw_driver;
}

#ifndef MODULE
static int __init pablo_common_ctrl_hw_init(void)
{
	int ret;

	ret = platform_driver_probe(&pablo_common_ctrl_hw_driver, pablo_common_ctrl_hw_probe);
	if (ret)
		pr_err("%s: platform_driver_probe failed(%d)", __func__, ret);

	return ret;
}
device_initcall_sync(pablo_common_ctrl_hw_init);
#endif
