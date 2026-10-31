// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024, Samsung Electronics.
 *
 */
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>

#include "cpif_cma_mem.h"
#include "modem_prj.h"
#include "modem_utils.h"

/*
 *
 */
static struct cp_cma_mem _cp_cma[MAX_CP_CMA_MEM];
static struct device sync_dev;

int alloc_cp_cma_region(struct cp_cma_mem *cp_cma, u32 size)
{
	int retry_count = 0;
	int ret = 0;

	if (!cp_cma) {
		mif_err("cp_cma is null\n");
		return -ENOMEM;
	}

	mutex_lock(&cp_cma->lock);

	if (cp_cma->paddr) {
		mif_err("already allocated\n");
		ret = 0;
		goto exit;
	}

	if (!size || (size > cp_cma->max_size)) {
		mif_err("size error:0x%08x\n", size);
		ret = -EINVAL;
		goto exit;
	}
	cp_cma->req_size = size;

	while (retry_count < MAX_ALLOC_RETRY) {
		cp_cma->cma_pages = cma_alloc(cp_cma->cma_area,
			cp_cma->req_size >> PAGE_SHIFT, cp_cma->align, false);
		if (cp_cma->cma_pages)
			break;

		mif_err("retry:%d\n", retry_count);
		retry_count++;
		udelay(100);
	}
	if (!cp_cma->cma_pages) {
		mif_err("cma_pages is null\n");
		ret = -ENOMEM;
		goto exit;
	}

	cp_cma->paddr = page_to_phys(cp_cma->cma_pages);
	cp_cma->vaddr = phys_to_virt(cp_cma->paddr);

	dma_sync_single_for_device(&sync_dev, cp_cma->paddr, cp_cma->req_size, DMA_FROM_DEVICE);

	mif_info("paddr:0x%llx\n", cp_cma->paddr);

exit:
	mutex_unlock(&cp_cma->lock);

	return ret;
}
EXPORT_SYMBOL(alloc_cp_cma_region);

int dealloc_cp_cma_region(struct cp_cma_mem *cp_cma)
{
	int ret = 0;

	if (!cp_cma) {
		mif_err("cp_cma is null\n");
		return -ENOMEM;
	}

	mutex_lock(&cp_cma->lock);

	if (!cp_cma->paddr) {
		mif_err("already deallocated\n");
		ret = 0;
		goto exit;
	}

	mif_info("paddr:0x%llx\n", cp_cma->paddr);

	dma_sync_single_for_device(&sync_dev, cp_cma->paddr, cp_cma->req_size, DMA_FROM_DEVICE);

	cma_release(cp_cma->cma_area, cp_cma->cma_pages,
		cp_cma->req_size >> PAGE_SHIFT);

	cp_cma->paddr = 0;
	cp_cma->vaddr = 0;
	cp_cma->req_size = 0;

exit:
	mutex_unlock(&cp_cma->lock);

	return ret;
}
EXPORT_SYMBOL(dealloc_cp_cma_region);

struct cp_cma_mem *get_cp_cma_region(u32 idx)
{
	if(idx >= MAX_CP_CMA_MEM) {
		mif_err("idx is over max:%d\n", idx);
		return NULL;
	}

	return &_cp_cma[idx];
}
EXPORT_SYMBOL(get_cp_cma_region);

/*
 * sysfs
 */
static ssize_t alloc_cma_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cp_cma_mem *cp_cma;
	int idx;
	int size;
	int ret;

	ret = sscanf(buf, "%i %i", &idx, &size);
	if (ret < 1) {
		mif_err("sscanf() error:%d\n", ret);
		return -EINVAL;
	}

	mif_info("alloc idx:%d size:0x%08x\n", idx, size);
	cp_cma = get_cp_cma_region(idx);
	if (!cp_cma) {
		mif_err("get_cp_cma_region() error:%d\n", idx);
		return count;
	}

	ret = alloc_cp_cma_region(cp_cma, size);
	if (ret < 0)
		mif_err("alloc_cp_cma_region() err:%d\n", ret);
	else
		mif_info("success. paddr:0x%llx\n", cp_cma->paddr);

	return count;
}

static ssize_t dealloc_cma_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cp_cma_mem *cp_cma;
	int idx;
	int ret;

	ret = kstrtoint(buf, 0, &idx);
	if (ret) {
		mif_err("kstrtoint() error:%d\n", ret);
		return ret;
	}

	mif_info("dealloc idx:%d\n", idx);
	cp_cma = get_cp_cma_region(idx);
	if (!cp_cma) {
		mif_err("get_cp_cma_region() error:%d\n", idx);
		return count;
	}

	dealloc_cp_cma_region(cp_cma);

	return count;
}

static ssize_t status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	int i;

	for (i = 0; i < MAX_CP_CMA_MEM; i++) {
		count += scnprintf(&buf[count], PAGE_SIZE - count,
			"idx:%d align:0x%08x max_size:0x%08x req_size:0x%08x paddr:0x%llx vaddr:%p\n",
			_cp_cma[i].idx, _cp_cma[i].align, _cp_cma[i].max_size,
			_cp_cma[i].req_size, _cp_cma[i].paddr, _cp_cma[i].vaddr);
	}

	return count;
}

static ssize_t write_cma_region_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct cp_cma_mem *cp_cma;
	int idx, offset, val;
	int ret;

	ret = sscanf(buf, "%i %i %i", &idx, &offset, &val);
	if (ret < 1) {
		mif_err("sscanf() error:%d\n", ret);
		return -EINVAL;
	}
	mif_info("idx:%d offset:%d val:0x%08x\n", idx, offset, val);

	cp_cma = get_cp_cma_region(idx);
	if (!cp_cma) {
		mif_err("cp_cma is null\n");
		return -EINVAL;
	}

	if (offset > cp_cma->req_size) {
		mif_err("offset is too big:%d\n", offset);
		return -EINVAL;
	}

	mutex_lock(&cp_cma->lock);

	if (!cp_cma->vaddr) {
		mif_err("vaddr is null\n");
		mutex_unlock(&cp_cma->lock);
		return -ENOMEM;
	}

	__raw_writel(val, cp_cma->vaddr + offset);
	mif_info("0x%08x\n", __raw_readl(cp_cma->vaddr + offset));

	mutex_unlock(&cp_cma->lock);

	return count;
}

static DEVICE_ATTR_WO(alloc_cma);
static DEVICE_ATTR_WO(dealloc_cma);
static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_WO(write_cma_region);

static struct attribute *cpif_mem_cma_attrs[] = {
	&dev_attr_alloc_cma.attr,
	&dev_attr_dealloc_cma.attr,
	&dev_attr_status.attr,
	&dev_attr_write_cma_region.attr,
	NULL,
};
ATTRIBUTE_GROUPS(cpif_mem_cma);

/*
 * Platform driver
 */
static int cp_cma_mem_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct device_node *desc_np;
	u32 index;
	int ret = 0;

	np = of_get_child_by_name(dev->of_node, "regions");
	if (!np) {
		mif_err("of_get_child_by_name() error:np\n");
		return -EINVAL;
	}

	for_each_child_of_node(np, desc_np) {
		ret = of_reserved_mem_device_init(dev);
		if (ret) {
			mif_err("Failed to get reserved memory region:%d\n", ret);
			goto err;
		}

		mif_dt_read_u32(desc_np, "index", index);
		_cp_cma[index].idx = index;
		_cp_cma[index].cma_area = dev->cma_area;
		mif_dt_read_u32(desc_np, "size", _cp_cma[index].max_size);
		mif_dt_read_u32(desc_np, "align", _cp_cma[index].align);

		mutex_init(&_cp_cma[index].lock);

		mif_info("idx:%d max_size:0x%08x align:0x%08x\n",
			_cp_cma[index].idx, _cp_cma[index].max_size, _cp_cma[index].align);
	}

	if (sysfs_create_groups(&dev->kobj, cpif_mem_cma_groups))
		mif_err("failed to create cpif_mem_cma_groups node\n");

	device_initialize(&sync_dev);

err:
	return ret;
}

static int cp_cma_mem_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id cp_cma_mem_dt_match[] = {
	{ .compatible = "samsung,exynos-cp-cma-mem", },
	{},
};
MODULE_DEVICE_TABLE(of, cp_cma_mem_dt_match);

static struct platform_driver cp_cma_mem_driver = {
	.probe = cp_cma_mem_probe,
	.remove = cp_cma_mem_remove,
	.driver = {
		.name = "cp_cma_mem",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cp_cma_mem_dt_match),
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(cp_cma_mem_driver);

MODULE_DESCRIPTION("Exynos CMA memory driver for CP");
MODULE_LICENSE("GPL");
