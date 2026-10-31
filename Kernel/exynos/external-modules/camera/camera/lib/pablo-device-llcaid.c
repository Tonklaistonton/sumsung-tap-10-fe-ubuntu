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

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>

#include "pablo-device-llcaid.h"
#include "pablo-debug.h"

static struct pablo_device_llcaid *llcaid_group[PABLO_MAX_NUM_LLCAID_GROUP];

struct pablo_device_llcaid *pablo_device_llcaid_get(u32 idx)
{
	return llcaid_group[idx];
}
EXPORT_SYMBOL_GPL(pablo_device_llcaid_get);

static int param_debug_llcaid_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] bit value, set LLCAID debug features\n"
				"\tb[0] : dump config\n"
				"\tb[1] : skip config\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static struct pablo_debug_param debug_llcaid = {
	.type = IS_DEBUG_PARAM_TYPE_BIT,
	.max_value = 0x3,
	.ops.usage = param_debug_llcaid_usage,
};
module_param_cb(debug_llcaid, &pablo_debug_param_ops, &debug_llcaid, 0644);

void pablo_device_set_llcaid_group(struct pablo_device_llcaid *llcaid_group_data, int num)
{
	int i;

	for (i = 0; i < num; i++)
		llcaid_group[i] = &llcaid_group_data[i];
}
KUNIT_EXPORT_SYMBOL(pablo_device_set_llcaid_group);

static void pablo_device_llcaid_config_dump(void)
{
	struct pablo_device_llcaid *llcaid;
	struct pablo_llcaid_stream_info *stream_info;
	void *stream_base;
	int i, j;
	unsigned int enable_val, ctrl_val_rd, ctrl_val_wd;

	info("= LLCAID CONFIG DUMP ================================");

	for (i = 0; i < PABLO_MAX_NUM_LLCAID_GROUP; i++) {
		llcaid = llcaid_group[i];
		if (!llcaid)
			break;

		info("%s", llcaid->instance_name);

		for (j = 0; j < PABLO_MAX_NUM_STREAM; j++) {
			stream_info = llcaid->stream[j];
			if (!stream_info)
				break;

			stream_base = llcaid->base[j];
			enable_val = readl(stream_base);
			ctrl_val_rd = readl(stream_base + PABLO_STREAM_RD_CTRL_OFFSET);
			ctrl_val_wd = readl(stream_base + PABLO_STREAM_WD_CTRL_OFFSET);

			info("stream%d enable_val : 0x%x ctrl_val_rd : 0x%x ctrl_val_wd : 0x%x", j,
				enable_val, ctrl_val_rd, ctrl_val_wd);
		}
	}
	info("======================================================");
}

void pablo_device_llcaid_config(void)
{
	struct pablo_device_llcaid *llcaid;
	struct pablo_llcaid_stream_info *stream_info;
	void *stream_base;
	int i, j;

	if (unlikely(test_bit(LLCAID_DBG_SKIP_CONFIG, &debug_llcaid.value))) {
		info("skip LLCAID config. default set.");
		return;
	}

	for (i = 0; i < PABLO_MAX_NUM_LLCAID_GROUP; i++) {
		llcaid = llcaid_group[i];
		if (!llcaid)
			break;

		for (j = 0; j < PABLO_MAX_NUM_STREAM; j++) {
			stream_info = llcaid->stream[j];
			if (!stream_info)
				break;

			stream_base = llcaid->base[j];

			writel(stream_info->set_val[PABLO_SET_VAL_IDX_EN], stream_base);
			writel(stream_info->set_val[PABLO_SET_VAL_IDX_CTRL],
				stream_base + PABLO_STREAM_RD_CTRL_OFFSET);
			writel(stream_info->set_val[PABLO_SET_VAL_IDX_CTRL],
				stream_base + PABLO_STREAM_WD_CTRL_OFFSET);
		}
	}

	if (unlikely(test_bit(LLCAID_DBG_DUMP_CONFIG, &debug_llcaid.value)))
		pablo_device_llcaid_config_dump();
}
EXPORT_SYMBOL_GPL(pablo_device_llcaid_config);

static const struct of_device_id pablo_llcaid_of_table[] = {
	{
		.compatible = "samsung,pablo-llcaid",
	},
	{},
};
MODULE_DEVICE_TABLE(of, pablo_llcaid_of_table);

static int pablo_device_llcaid_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	int ret, id, i, j, n_size_cells;
	struct pablo_device_llcaid *llcaid;
	struct resource *res;
	struct pablo_llcaid_stream_info *stream_info;

	np = dev->of_node;
	id = of_alias_get_id(np, "llcaid");

	llcaid = devm_kzalloc(&pdev->dev, sizeof(struct pablo_device_llcaid), GFP_KERNEL);
	if (!llcaid)
		return -ENOMEM;

	platform_set_drvdata(pdev, llcaid);

	llcaid->id = id;
	llcaid->dev = dev;

	strncpy(llcaid->instance_name, np->name, sizeof(llcaid->instance_name) - 1);

	of_property_read_u32(np, "#val-cells", &n_size_cells);

	for (i = 0; i < pdev->num_resources; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);

		if (!res) {
			dev_err(dev, "can't get memory resource\n");
			ret = -ENODEV;
			goto err_get_res;
		}

		llcaid->base[i] = devm_ioremap(dev, res->start, resource_size(res));
		if (!llcaid->base[i]) {
			dev_err(dev, "failed to get & ioremap for LLCAID control\n");
			ret = PTR_ERR(llcaid->base[i]);
			goto err_get_ioremap_ctl;
		}

		stream_info = devm_kzalloc(&pdev->dev, sizeof(*stream_info), GFP_KERNEL);
		if (!stream_info) {
			ret = -ENOMEM;
			goto err_get_ioremap_ctl;
		}

		for (j = 0; j < n_size_cells; j++) {
			ret = of_property_read_u32_index(
				np, "stream", i * n_size_cells + j, &stream_info->set_val[j]);
			if (ret) {
				dev_err(dev, "stream read fail\n");
				goto err_read_stream;
			}
		}

		llcaid->stream[i] = stream_info;
	}

	llcaid_group[id] = llcaid;

	dev_info(dev, "%s done\n", __func__);

	return 0;

err_read_stream:
	devm_kfree(dev, stream_info);
err_get_ioremap_ctl:
	while (i-- > 0)
		devm_kfree(dev, llcaid->stream[i]);
err_get_res:
	while (i-- > 0)
		devm_iounmap(dev, llcaid->base[i]);
	devm_kfree(dev, llcaid);

	return ret;
}

static int pablo_device_llcaid_remove(struct platform_device *pdev)
{
	struct pablo_device_llcaid *llcaid = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int i;

	for (i = 0; i < pdev->num_resources; i++)
		devm_iounmap(dev, llcaid->base[i]);

	devm_kfree(dev, llcaid);

	return 0;
}

static struct platform_driver pablo_llcaid_driver = {
	.probe = pablo_device_llcaid_probe,
	.remove = pablo_device_llcaid_remove,
	.driver = {
		.name = "pablo-device-llcaid",
		.owner = THIS_MODULE,
		.of_match_table = pablo_llcaid_of_table,
	}
};

struct platform_driver *pablo_llcaid_get_platform_driver(void)
{
	return &pablo_llcaid_driver;
}
EXPORT_SYMBOL_GPL(pablo_llcaid_get_platform_driver);

#ifndef MODULE
static int __init pablo_llcaid_init(void)
{
	int ret;

	ret = platform_driver_probe(&pablo_llcaid_driver, pablo_device_llcaid_probe);
	if (ret)
		pr_err("failed to probe %s driver: %d\n", "pablo-device-llcaid", ret);

	return ret;
}

device_initcall_sync(pablo_llcaid_init);
#endif

MODULE_AUTHOR("SamsungLSI Camera");
MODULE_DESCRIPTION("Exynos Pablo LLCAID driver");
MODULE_LICENSE("GPL v2");
