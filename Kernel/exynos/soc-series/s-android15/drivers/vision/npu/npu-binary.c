/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>
#endif
#include "include/npu-binary.h"
#include "npu-log.h"
#include "npu-device.h"
#include "npu-scheduler.h"
#include "include/npu-common.h"
#include "npu-ver.h"

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#if IS_ENABLED(CONFIG_NPU_USE_S2MPU_NAME_IS_NPUS)
#define NPU_S2MPU_NAME	"NPUS"
#else
#define NPU_S2MPU_NAME	"DNC"
#endif // IS_ENABLED(CONFIG_NPU_USE_S2MPU_NAME_IS_NPUS)
#endif // IS_ENABLED(CONFIG_EXYNOS_S2MPU)

#define FW_SIGNATURE_LEN        528
int npu_fw_load(struct npu_binary *binary,
				void *target, size_t target_size)
{
	int ret = 0;
	const struct firmware *fw_blob;

	ret = request_firmware(&fw_blob, NPU_FW_NAME, binary->dev);
	if (ret) {
		npu_err("fail(%d) in request_firmware", ret);
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob) {
		npu_err("null in fw_blob\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob->data) {
		npu_err("null in fw_blob->data\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (fw_blob->size > target_size) {
		npu_err("image size over(%ld > %ld)\n", fw_blob->size, target_size);
		ret = -EIO;
		goto request_err;
	}

	binary->image_size = fw_blob->size;
	memcpy(target, fw_blob->data, fw_blob->size);
	npu_info("success of binary(%ld) apply.\n", fw_blob->size);

request_err:
	release_firmware(fw_blob);
	return ret;
}

int npu_fw_vector_load(struct npu_binary *binary,
			void *target, size_t target_size)
{
	int ret = 0;
	const struct firmware *fw_blob;

	ret = request_firmware(&fw_blob, NPU_FW_VECTOR_NAME, binary->dev);
	if (ret) {
		npu_err("fail(%d) in request_firmware(%s)", ret, NPU_FW_VECTOR_NAME);
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob) {
		npu_err("null in fw_blob\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob->data) {
		npu_err("null in fw_blob->data\n");
		ret = -EINVAL;
		goto request_err;
	}

	binary->image_size = fw_blob->size;
	memcpy(target, fw_blob->data, fw_blob->size);
	npu_info("success of binary(%s, %ld) apply.\n", NPU_FW_VECTOR_NAME, fw_blob->size);

request_err:
	release_firmware(fw_blob);
	return ret;
}

int npu_fw_slave_load(struct npu_binary *binary, void *target)
{
	int ret = 0;
	const struct firmware *fw_blob;

	ret = firmware_request_nowarn(&fw_blob, NPU_FW_SLAVE_NAME, binary->dev);
	if (ret) {
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob) {
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob->data) {
		ret = -EINVAL;
		goto request_err;
	}

	binary->image_size = fw_blob->size;
	memcpy(target, fw_blob->data, fw_blob->size);
	npu_info("success of binary(%s, %ld) apply.\n", NPU_FW_SLAVE_NAME, fw_blob->size);

request_err:
	release_firmware(fw_blob);
	return ret;
}

int npu_binary_init(struct npu_binary *binary,
	struct device *dev,
	char *fpath1,
	char *fpath2,
	char *fname)
{

	if (unlikely(!binary)) {
		probe_err("Failed to get npu_binary\n");
		return -EINVAL;
	}

	binary->dev = dev;
	binary->image_size = 0;
	snprintf(binary->fpath1, sizeof(binary->fpath1), "%s%s", fpath1, fname);
	snprintf(binary->fpath2, sizeof(binary->fpath2), "%s%s", fpath2, fname);

	return 0;
}

#define MAX_SIGNATURE_LEN       128
#define SIGNATURE_HEADING       "NPU Firmware signature : "
void print_ufw_signature(struct npu_memory_buffer *fwmem)
{
	int i = 0;
	char version_buf[MAX_FW_VERSION_LEN];
	char *fw_version_addr;
	char *p_fw_version;

	fw_version_addr = (char *)(fwmem->dma.vaddr + 0xF000);

	version_buf[MAX_FW_VERSION_LEN - 1] = '\0';

	p_fw_version = version_buf;

	for (i = 0; i < MAX_FW_VERSION_LEN - 1; i++, p_fw_version++, fw_version_addr++)
		*p_fw_version = *fw_version_addr;

	npu_info("NPU Firmware version : %s\n", version_buf);
}
