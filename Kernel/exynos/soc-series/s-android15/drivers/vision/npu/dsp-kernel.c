// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "include/npu-preset.h"
#include "npu-device.h"
#include "npu-system.h"
#include "npu-session.h"
#include "npu-hw-device.h"
#include "npu-log.h"

#include "dsp-util.h"
#include "dsp-binary.h"
#include "dsp-kernel.h"

#include "dl/dsp-dl-engine.h"
#include "dl/dsp-gpt-manager.h"
#include "dl/dsp-hash.h"
#include "dl/dsp-tlsf-allocator.h"
#include "dl/dsp-xml-parser.h"
#include "dl/dsp-string-tree.h"
#include "dl/dsp-rule-reader.h"
#include "dl/dsp-lib-manager.h"

#define DSP_ELF_TAG_INFO_SIZE		(103)

int __dsp_kernel_alloc_load_user(struct device *dev, const char *name, void *source,
		size_t source_size, void **target, size_t *loaded_size)
{
	int ret;

	if (!target) {
		ret = -EINVAL;
		npu_err("dest address must be not NULL[%s]\n", name);
		goto p_err_target;
	}

	*target = vmalloc(source_size);
	if (!(*target)) {
		ret = -ENOMEM;
		npu_err("Failed to allocate target for binary[%s](%zu)\n",
				name, source_size);
		goto p_err_alloc;
	}

	memcpy(*target, source, source_size);
	if (loaded_size)
		*loaded_size = source_size;
	npu_info("binary[%s/%zu] is loaded\n", name, source_size);
	return 0;
p_err_alloc:
p_err_target:
	return ret;
}

static struct dsp_kernel *dsp_kernel_alloc(struct device *dev,
		struct npu_session *session, struct dsp_kernel_manager *kmgr,
		unsigned int name_length, struct dsp_dl_lib_info *dl_lib, int idx)
{
	int ret = 0;
	struct dsp_kernel *new, *list, *temp;
	bool checked = false;
	char *tag_info = NULL;

	new = kzalloc(sizeof(*new), GFP_KERNEL);
	if (!new) {
		ret = -ENOMEM;
		npu_uerr("Failed to allocate kernel[%s]\n", session, session->hids, dl_lib->name);
		goto p_err;
	}
	new->owner = kmgr;

	new->name_length = name_length + 4;
	new->name = kzalloc(new->name_length, GFP_KERNEL);
	if (!new->name) {
		ret = -ENOMEM;
		npu_uerr("Failed to allocate kernel_name[%s]\n", session, session->hids, dl_lib->name);
		goto p_err_name;
	}
	snprintf(new->name, new->name_length, "%s.elf", dl_lib->name);

	mutex_lock(&kmgr->lock);
	new->id = dsp_util_bitmap_set_region(&kmgr->kernel_map, 1);
	if (new->id < 0) {
		mutex_unlock(&kmgr->lock);
		ret = new->id;
		npu_uerr("Failed to allocate kernel bitmap(%d)\n", session, session->hids, ret);
		goto p_err_bitmap;
	}

	list_for_each_entry_safe(list, temp, &kmgr->kernel_list, list) {
		if (list->name_length == new->name_length &&
				!strncmp(list->name, new->name,
					new->name_length)) {
			list->ref_count++;
			if (!checked) {
				new->ref_count = list->ref_count;
				new->elf = list->elf;
				new->elf_size = list->elf_size;
				checked = true;
			}
		}
	}

	if (!checked) {
		if (session->kernel_count) {
			if (session->kernel_count - 1 < idx) {
				ret = -EINVAL;
				mutex_unlock(&kmgr->lock);
				goto p_err_load;
			}
			ret = __dsp_kernel_alloc_load_user(dev, new->name, session->kernel_elf[idx],
				session->kernel_elf_size[idx], &new->elf, &new->elf_size);
			if (ret) {
				mutex_unlock(&kmgr->lock);
				goto p_err_load;
			}
			new->ref_count = 1;
		} else {
			new->ref_count = 1;
			ret = dsp_binary_alloc_load(dev, new->name, NULL, NULL,
						    &new->elf, &new->elf_size);
			if (ret) {
				mutex_unlock(&kmgr->lock);
				goto p_err_load;
			}
		}
		tag_info = (char *)new->elf + new->elf_size - DSP_ELF_TAG_INFO_SIZE;
	}

	list_add_tail(&new->list, &kmgr->kernel_list);
	kmgr->kernel_count++;
	mutex_unlock(&kmgr->lock);

	dl_lib->file.mem = new->elf;
	dl_lib->file.size = new->elf_size;

	npu_uinfo("loaded kernel name : [%s](%zu)\n", session, session->hids, new->name, new->elf_size);
	if (tag_info)
		npu_uinfo("loaded kernel tag info : %s\n", session, session->hids, tag_info);

	return new;
p_err_load:
p_err_bitmap:
	kfree(new->name);
p_err_name:
	kfree(new);
p_err:
	return ERR_PTR(ret);
}

static void __dsp_kernel_free(struct dsp_kernel *kernel)
{
	struct dsp_kernel_manager *kmgr;
	struct dsp_kernel *list, *temp;

	kmgr = kernel->owner;

	mutex_lock(&kmgr->lock);
	if (kernel->ref_count == 1) {
		vfree(kernel->elf);
		goto free;
	}

	list_for_each_entry_safe(list, temp, &kmgr->kernel_list, list) {
		if (list->name_length == kernel->name_length &&
				!strncmp(list->name, kernel->name,
					list->name_length))
			list->ref_count--;
	}

free:
	kmgr->kernel_count--;
	list_del(&kernel->list);
	dsp_util_bitmap_clear_region(&kmgr->kernel_map, kernel->id, 1);
	mutex_unlock(&kmgr->lock);

	kfree(kernel->name);
	kfree(kernel);
}

#ifndef CONFIG_NPU_KUNIT_TEST
void dsp_kernel_dump(struct dsp_kernel_manager *kmgr)
{
	if (kmgr->dl_init)
		dsp_dl_print_status();
}
#endif

int dsp_kernel_load(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count)
{
	int ret;
	struct dsp_dl_load_status dl_ret;

	if (kmgr->dl_init) {
		mutex_lock(&kmgr->lock);
		dl_ret = dsp_dl_load_libraries(dl_libs, kernel_count);
		mutex_unlock(&kmgr->lock);
		if (dl_ret.status) {
			ret = dl_ret.status;
			npu_err("Failed to load kernel(%u/%d)\n", kernel_count, ret);
			dsp_kernel_dump(kmgr);
			goto p_err;
		}
	} else {
		ret = -EINVAL;
		npu_err("Failed to load kernel as DL is not initilized\n");
		goto p_err;
	}

	return 0;
p_err:
	return ret;
}

static int __dsp_kernel_unload(struct dsp_kernel_manager *kmgr,
		struct dsp_dl_lib_info *dl_libs, unsigned int kernel_count, bool is_user_kernel)
{
	int ret;

	if (kmgr->dl_init) {
		mutex_lock(&kmgr->lock);
		ret = dsp_dl_unload_libraries(dl_libs, kernel_count, is_user_kernel);
		mutex_unlock(&kmgr->lock);
		if (ret) {
			npu_err("Failed to unload kernel(%u/%d)\n",
					kernel_count, ret);
			goto p_err;
		}
	} else {
		ret = -EINVAL;
		npu_err("Failed to unload kernel as DL is not initilized\n");
		goto p_err;
	}
	return 0;
p_err:
	return ret;
}

void dsp_graph_remove_kernel(struct npu_device *device, struct npu_session *session)
{
	struct dsp_kernel_manager *kmgr;
	struct dsp_kernel *kernel, *t;
	bool is_user_kernel = (session->str_manager.count > 0);

	kmgr = &device->kmgr;

	if (session->kernel_loaded) {
		__dsp_kernel_unload(kmgr, session->dl_libs, session->kernel_count, is_user_kernel);
		dsp_lib_manager_delete_remain_xml(&session->str_manager);
		kfree(session->dl_libs);
		session->dl_libs = NULL;
	} else {
		kfree(session->dl_libs);
		session->dl_libs = NULL;
	}

	list_for_each_entry_safe(kernel, t, &session->kernel_list, graph_list) {
		list_del(&kernel->graph_list);
		__dsp_kernel_free(kernel);
	}
}

char *append_string_number_suffix(const char *str, unsigned int length, unsigned int suffix)
{
	char temp[256] = { 0, };
	char *result;

	strncpy(temp, str, length);
	result = kzalloc(length + 12, GFP_KERNEL);
	if (!result)
		goto p_err;

	sprintf(result, "%s_%u", temp, suffix);
p_err:
	return result;
}

int dsp_graph_add_kernel(struct npu_device *device,
		struct npu_session *session, const char *kernel_name, unsigned int unique_id, int i)
{
	int ret = 0;
	struct dsp_kernel *kernel;

	if (unique_id != 0) {
		session->dl_libs[i].name =
			append_string_number_suffix(kernel_name,
					strlen(kernel_name) + 1, unique_id);
	} else {
		session->dl_libs[i].name = kernel_name;
	}
	npu_uinfo("Convert lib name[%s]\n", session, session->hids, session->dl_libs[i].name);
	kernel = dsp_kernel_alloc(device->dev, session, &device->kmgr, strlen(kernel_name) + 12,
			&session->dl_libs[i], i);
	if (IS_ERR(kernel)) {
		ret = PTR_ERR(kernel);
		npu_uerr("Failed to alloc kernel(%u/%u)\n", session, session->hids, i, session->kernel_count);
		goto p_err_alloc;
	}

	list_add_tail(&kernel->graph_list, &session->kernel_list);

	return 0;
p_err_alloc:
	dsp_graph_remove_kernel(device, session);
	return ret;
}

int dsp_kernel_manager_open(struct npu_system *system, struct dsp_kernel_manager *kmgr)
{
	int ret;

	mutex_lock(&kmgr->lock);
	if (kmgr->dl_init) {
		if (kmgr->dl_init + 1 < kmgr->dl_init) {
			ret = -EINVAL;
			npu_err("dl init count is overflowed\n");
			goto p_err;
		}

		kmgr->dl_init++;
	} else {
		ret = dsp_kernel_manager_dl_init(&system->pdev->dev, &kmgr->dl_param,
							npu_get_mem_area(system, "ivp_pm"), npu_get_mem_area(system, "dl_out"));
		if (ret)
			goto p_err;

		kmgr->dl_init = 1;
	}
	mutex_unlock(&kmgr->lock);
	return 0;
p_err:
	mutex_unlock(&kmgr->lock);
	return ret;
}

void dsp_kernel_manager_close(struct dsp_kernel_manager *kmgr,
		unsigned int count)
{
	mutex_lock(&kmgr->lock);
	if (!kmgr->dl_init) {
		npu_warn("dl was not initilized");
		mutex_unlock(&kmgr->lock);
		return;
	}

	if (kmgr->dl_init > count) {
		kmgr->dl_init -= count;
		mutex_unlock(&kmgr->lock);
		return;
	}

	if (kmgr->dl_init < count)
		npu_warn("dl_init is unstable(%u/%u)", kmgr->dl_init, count);

	dsp_kernel_manager_dl_deinit(&kmgr->dl_param);
	kmgr->dl_init = 0;
	mutex_unlock(&kmgr->lock);
}

int dsp_kernel_manager_probe(struct npu_device *device)
{
	int ret;
	struct dsp_kernel_manager *kmgr;

	kmgr = &device->kmgr;
	kmgr->device = device;

	INIT_LIST_HEAD(&kmgr->kernel_list);

	mutex_init(&kmgr->lock);
	ret = dsp_util_bitmap_init(&kmgr->kernel_map, "kernel_bitmap",
			DSP_KERNEL_MAX_COUNT);
	if (ret)
		goto p_err;

	return 0;
p_err:
	return ret;
}

void dsp_kernel_manager_remove(struct dsp_kernel_manager *kmgr)
{
	struct dsp_kernel *kernel, *temp;

	list_for_each_entry_safe(kernel, temp, &kmgr->kernel_list, list) {
		npu_warn("kernel[%u] is destroyed(count:%u)\n",
				kernel->id, kmgr->kernel_count);
		__dsp_kernel_free(kernel);
	}
	dsp_util_bitmap_deinit(&kmgr->kernel_map);
}
