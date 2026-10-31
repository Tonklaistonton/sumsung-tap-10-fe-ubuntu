// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series uNPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/irqreturn.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "npu-clock.h"
#include "npu-config.h"
#include "npu-util-regs.h"
#include "interface/hardware/npu-interface.h"
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-pmu-if.h>

#define DEBUG_FS_UNPU_ROOT_NAME "unpu"

#define DEBUG_FS_UNPU_PWR_CTRL "power_ctrl_and_boot"
#define DEBUG_FS_UNPU_LOG_NAME "fw-report"
#define DEBUG_FS_UNPU_SREG "access_sreg"

#define UNPU_REMAP_DEST_RESET 0x20200000
#define UNPU_REMAP_SRC_RESET 0x00080000
#define UNPU_FW_UT_OFFSET 0x40000
#define UNPU_REMAP_DEST_START 0x8
#define UNPU_REMAP_SRC_END 0x4
#define UNPU_FW_OFFSET 0x0
#define UNPU_CPU_PC 0x4c

#define UNPU_BAAW_WINDOW_OFFSET 0x10
#define UNPU_BAAW_ENABLE 0x80000003
#define UNPU_BAAW_ENABLE_ADDR 0xc
#define UNPU_BAAW_REMAP_ADDR 0x8
#define UNPU_BAAW_START_ADDR 0x0
#define UNPU_MAX_BAAW_WINDOW 16
#define UNPU_BAAW_END_ADDR 0x4

#define UNPU_NCP_IN_OUT_SIZE 0x1C20
#define UNPU_FW_UT_SIZE 0x40000
#define UNPU_OFFSET_10KB 0x2800
#define UNPU_MAX_PATH_SIZE 128
#define UNPU_NCP_SIZE 0x12C0
#define UNPU_FW_SIZE 0x40000
#define ACCESS_4BYTE 0x4

#define NPUMEM_ENABLE_ACCESS 0xFFFFFFFF
#define NPUMEM_REGION0_OFF 0x4
#define NPUMEM_REGION1_OFF 0x8
#define NPUMEM_REGION2_OFF 0xC

#define UNPU_HOST_MBOX_OFFSET UNPU_FW_OFFSET + 0x2B100
#define UNPU_CLIENT_SIGNATURE 0xC1E7C0DE
#define MS_TIMEOUT 3000

#define UNPU_FW_UT_REGION_SZ 0x40000
#define UNPU_FW_UT_DVA_START 0x90000000
#define UNPU_FW_UT_BAAW_OFFSET (4 * UNPU_OFFSET_10KB)
#define UNPU_FW_UT_DVA_END (UNPU_FW_UT_DVA_START + UNPU_FW_UT_REGION_SZ)

#define UNPU_NCP_DVA_START 0x80000000
#define UNPU_NCP_DVA_END (UNPU_NCP_DVA_START + UNPU_NCP_SIZE)

#define UNPU_NCP_IN_DVA_START UNPU_NCP_DVA_START + UNPU_OFFSET_10KB
#define UNPU_NCP_IN_DVA_END (UNPU_NCP_IN_DVA_START + UNPU_NCP_IN_OUT_SIZE)

#define UNPU_NCP_OUT_DVA_START UNPU_NCP_IN_DVA_START + UNPU_OFFSET_10KB
#define UNPU_NCP_OUT_DVA_END (UNPU_NCP_OUT_DVA_START + UNPU_NCP_IN_OUT_SIZE)

#define UNPU_FW_LOG_SIZE 0x80000
#define UNPU_FW_LOG_OFFSET 0x0
const char * const reg_names[UNPU_MAX_IOMEM] = {"unpu_ctrl", "yamin_ctrl", "unpu_sram", "npumem_sram", "npumem_ctrl_s", "unpu_baaw"};

#define UNPU_CMD_INPUT_AV 0x5
#define UNPU_CMD_OUTPUT_AV 0x7
#define UNPU_CMD_TIMEOUT 100

/* Singleton reference to debug object */
static struct unpu_debug *unpu_debug_ref;

#define UNPU_FW_UT_NAME "uNPU_UT.bin"
#define UNPU_FW_NAME "uNPU.bin"
#define UNPU_NCP_OUT_NAME "output.bin"
#define UNPU_NCP_IN_NAME "input.bin"
#define UNPU_NCP_NAME "ncp.bin"

typedef enum {
	FS_READY = 0,
} unpu_debug_state_bits_e;

static struct unpu_client_interface unpu_client_interface;
static struct unpu_mbox_interface unpu_mbox_h2f_interface;
static struct unpu_mbox_interface unpu_mbox_f2h_interface;
static bool check_done_for_cmd(struct unpu_client_interface interface, u32 *res);

extern struct npu_interface *gnpu_if;

static int unpu_config_baaw(struct unpu_iomem_data *baaw, u32 dst_start,
			    u32 dst_end, phys_addr_t src) {
	void __iomem *reg_addr;
	int iter, check = 0;

	for (iter = 0; (iter < UNPU_MAX_BAAW_WINDOW) && (check != 0x1); iter++) {
		reg_addr = (baaw->vaddr + (UNPU_BAAW_WINDOW_OFFSET * iter));
		if (readl(reg_addr + UNPU_BAAW_ENABLE_ADDR) != UNPU_BAAW_ENABLE) {
			writel(dst_start >> ACCESS_4BYTE, (reg_addr + UNPU_BAAW_START_ADDR));
			writel(dst_end >> ACCESS_4BYTE, (reg_addr + UNPU_BAAW_END_ADDR));
			writel(src >> ACCESS_4BYTE, (reg_addr + UNPU_BAAW_REMAP_ADDR));
			writel(UNPU_BAAW_ENABLE, (reg_addr + UNPU_BAAW_ENABLE_ADDR));
			check = 0x1;
		}
	}

	return 0;
}

int __unpu_send_interrupt(struct unpu_mbox_interface host_mbox, u32 value) {
	int ret;
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)host_mbox.intr),
			value, 0x0);
	if (ret)
		npu_err("sending interrupt via smc call failed(%d)\n", ret);

	return ret;
}

static irqreturn_t unpu_mbox_yamin_response(int irq, void *data) {
	struct unpu_device *unpu;
	unsigned long val;
	u32 ret;

	unpu = container_of(unpu_debug_ref, struct unpu_device, debug);
	ret = exynos_smc_readsfr(unpu_mbox_f2h_interface.intr, &val);
	if (val) {
		ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)unpu_mbox_f2h_interface.intr), 0x0, 0x0);
	}

	wake_up(&unpu->done_wq);

	return 0;
}

static irqreturn_t (*unpu_mbox_isr_list[])(int, void *) = {
	unpu_mbox_yamin_response,
};

/* wait until client interface state is not ready */
int wait_till_npu_state_not_ready(struct unpu_client_interface interface) {
#if !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	void __iomem *vaddr = NULL;
#endif
	int ret, iter = 0;
	unsigned long v;

	do {
		msleep(1);
		if (!(iter % 100 )) {
#if IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
			ret = exynos_smc_readsfr(interface.npu_state, &v);
#else
			vaddr = ioremap(interface.npu_state, 0x4);
			v = readl(vaddr);
			if (vaddr)
				iounmap(vaddr);
#endif
		}
		iter++;
		if (iter == MS_TIMEOUT) {
			npu_err("mailbox initialization timed out\n");
			ret = -EINVAL;
			goto p_err;
		}
	} while (v != UNPU_STATE_READY);

	ret = v;
p_err:
	return ret;
}

int wait_till_npu_run_not_zero(struct unpu_client_interface interface) {
#if !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	void __iomem *vaddr = NULL;
#endif
	int ret, iter = 0;
	unsigned long v;
	do {
		msleep(1);
		if (!(iter % 100 )) {
#if IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
			ret = exynos_smc_readsfr(interface.flag_npu_run, &v);
#else
			vaddr = ioremap(interface.flag_npu_run, 0x4);
			v = readl(vaddr);
			if (vaddr)
				iounmap(vaddr);
#endif
		}
		iter++;
		if (iter == MS_TIMEOUT) {
			npu_err("flag_npu_run enable timed out\n");
			ret = -EINVAL;
			goto p_err;
		}
	} while (v != 0);

	ret = 0;
p_err:
	return ret;
}

int wait_till_npu_done_not_zero(struct unpu_client_interface interface) {
#ifndef CONFIG_SOC_S5E9945_EVT0
	void __iomem *vaddr;
#endif
	int ret, iter = 0;
	unsigned long v;

	do {
		msleep(1);
		if (!(iter % 100 )) {
#ifdef CONFIG_SOC_S5E9945_EVT0
			ret = exynos_smc_readsfr(interface.flag_npu_done, &v);
#else
			vaddr = ioremap(interface.flag_npu_done, 0x4);
			v = readl(vaddr);
			if (vaddr)
			        iounmap(vaddr);
#endif
		}
		iter++;
		if (iter == MS_TIMEOUT) {
			npu_err("flag_npu_run enable timed out\n");
			ret = -EINVAL;
			goto p_err;
		}
	} while (v == 0);

	ret = 0;
p_err:
	return ret;
}

void memcpy_via_smc(volatile unsigned int *dst, volatile unsigned int *src,
		      size_t size) {
#if !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	void __iomem *vaddr = NULL;
#endif
	volatile u32 v;
#if IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	int ret;
#endif
	int i;

	isb();
	msleep(2);
	for (i =0; i < size; i++) {
		v = readl((volatile void *)(src + i));
#if IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
		ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)*(dst + i)),
						 v, 0x0);
#else
		vaddr = ioremap((phys_addr_t)*(dst + i), 0x4);
		writel(v, vaddr);
		if (vaddr)
			iounmap(vaddr);
#endif

	}
	mb();
}

unsigned long check_unpu_state(struct unpu_client_interface interface) {
#if !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	void __iomem *vaddr = NULL;
#endif
	unsigned long v;

#if IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	exynos_smc_readsfr(interface.npu_state, &v);
#else
	vaddr = ioremap(interface.npu_state, 0x4);
	v = readl(vaddr);
	if (vaddr)
		iounmap(vaddr);
#endif

	return v;
}

bool check_done_for_cmd(struct unpu_client_interface interface, u32 *res) {
#if !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	void __iomem *vaddr = NULL;
#endif
	unsigned long v;

#if IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	exynos_smc_readsfr(interface.npu_response, &v);
#else
	vaddr = ioremap(interface.npu_response, 0x4);
	v = readl(vaddr);
	if (vaddr)
		iounmap(vaddr);
#endif

	*res = (u32)v;
	if ((v == UNPU_RES_LOAD) || (v == UNPU_RES_PROCESS) || (v == UNPU_RES_DEINIT))
		return true;

	return false;
}

void unpu_init_h2f_mbox(struct unpu_mbox_interface mbox) {
	int ret = 0;
	ret += exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)mbox.client_interface_ptr),
			 UNPU_HOST_MBOX_OFFSET, 0x0);
	ret += exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)mbox.client_signature),
			 UNPU_CLIENT_SIGNATURE, 0x0);
	if (ret)
		npu_err("set values in mbox via smc call failed(%d)\n", ret);

	return;
}

void unpu_set_cmd(struct unpu_client_interface interface) {
	memcpy_via_smc(&unpu_client_interface.npu_state, &interface.npu_state,
		       sizeof(interface)/ACCESS_4BYTE);
};

void unpu_nw_req_manager(enum unpu_request request) {
	struct unpu_client_interface c_interface = {};
	int ret;

	switch (request) {
	case UNPU_REQ_INIT:
		unpu_init_h2f_mbox(unpu_mbox_h2f_interface);
		break;
	case UNPU_REQ_LOAD:
		c_interface.flag_npu_run = 0x1;
		c_interface.client_request = UNPU_REQ_LOAD;
		for (int i = 0; i < UNPU_CLIENT_INTERFACE_MAX_REGS; i++)
			c_interface.data[i] = 0x0;

		c_interface.data[0] = 0;						/* flags */
		c_interface.data[1] = UNPU_CMD_TIMEOUT;			/* timeout in us*/
		c_interface.data[2] = 0;						/* ncp idx */
		c_interface.data[3] = UNPU_NCP_DVA_START;		/* NCP address */
		c_interface.data[4] = UNPU_NCP_SIZE;			/* NCP size */
		unpu_set_cmd(c_interface);
		break;
	case UNPU_REQ_PROCESS:
		c_interface.flag_npu_run = 0x1;
		c_interface.client_request = UNPU_REQ_PROCESS;
		for (int i = 0; i < UNPU_CLIENT_INTERFACE_MAX_REGS; i++)
			c_interface.data[i] = 0x0;
		/* send process command */
		c_interface.data[0] = 0;						/* oid */
		c_interface.data[1] = 0;						/* fid */
		c_interface.data[2] = UNPU_CMD_TIMEOUT;			/* timeout in us */
		c_interface.data[3] = 1;						/* input type */
		c_interface.data[4] = 1;						/* output type */
		c_interface.data[5] = 1;						/* num of input */
		c_interface.data[6] = 1;						/* num of output */
		c_interface.data[7] = UNPU_CMD_INPUT_AV;		/* input_buf_id */
		c_interface.data[8] = UNPU_NCP_IN_DVA_START;	/* IFM address */
		c_interface.data[9] = UNPU_NCP_IN_OUT_SIZE;		/* IFM size */
		c_interface.data[10] = UNPU_CMD_OUTPUT_AV;		/* output_buf_id */
		c_interface.data[11] = UNPU_NCP_OUT_DVA_START;	/* OFM address */
		c_interface.data[12] = UNPU_NCP_IN_OUT_SIZE;	/* OFM address */
		unpu_set_cmd(c_interface);
		break;
	case UNPU_REQ_CLEAR_DONE:
		c_interface.flag_npu_done = 0x0;
		unpu_set_cmd(c_interface);
		break;
	case UNPU_REQ_DEINIT:
		c_interface.flag_npu_run = 0x1;
		c_interface.client_request = UNPU_REQ_DEINIT;
		unpu_set_cmd(c_interface);
		break;
	default:
		npu_err("Invalid command\n");
		break;
	}

	/* send interrupt to fw*/
	ret = __unpu_send_interrupt(unpu_mbox_h2f_interface, 0x100);
	if (ret)
		npu_err("send interrupt failed from host to fw(%d)\n", ret);
}

static int __validate_user_file(const struct firmware *blob, uint32_t size)
{
	int ret = 0;

	if (unlikely(!blob))
	{
		npu_err("NULL in blob\n");
		return -EINVAL;
	}

	if (unlikely(!blob->data))
	{
		npu_err("NULL in blob->data\n");
		return -EINVAL;
	}

	if (blob->size > size)
	{
		npu_err("blob is too big (%ld > %u)\n", blob->size, size);
		return -EIO;
	}

	return ret;
}

static int __load_user_file(const struct firmware **blob, const char *path, struct device *dev, uint32_t size)
{
	int ret = 0;

	// get binary from userspace
	ret = request_firmware(blob, path, dev);
	if (ret)
	{
		npu_err("fail(%d) in request_firmware_direct: %s\n", ret, path);
		return ret;
	}

	ret = __validate_user_file(*blob, size);
	if (ret)
	{
		npu_err("fail(%d) to validate user file: %s\n", ret, path);
		release_firmware(*blob);
		*blob = NULL;
		return ret;
	}

	return ret;
}

static int __load_image_to_io(struct device *dev, struct unpu_iomem_data *mem,
						      char *name, unsigned int offset, size_t size)
{
	const struct firmware *blob = NULL;
	int ret = 0;
	char path[100] = "";
	volatile u32 v;
	int iter = 0, i;
#ifdef CONFIG_SOC_S5E9945_EVT0
	phys_addr_t sram_offset;
	sram_offset = mem->paddr + offset;
#else
	void __iomem *sram_offset;
	sram_offset = mem->vaddr + offset;
#endif
	strcat(path, name);

	ret = __load_user_file(&blob, path, dev, size);
	if (ret) {
		npu_err("fail(%d) to load user file: %s\n", ret, path);
		goto exit;
	}

	iter = (blob->size) / ACCESS_4BYTE;
	for (i = 0; i < iter; i++) {
		v = readl((volatile void *)(blob->data + i * ACCESS_4BYTE));
#ifdef CONFIG_SOC_S5E9945_EVT0
		ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)(sram_offset + i * ACCESS_4BYTE)), v, 0x0);
#else
		writel(v, (sram_offset + i * ACCESS_4BYTE));
#endif
	}

	mb();
	if (blob)
		release_firmware(blob);

exit:
	return ret;
}

static int __load_bin_to_dram(struct device *dev, struct unpu_rmem_data *mem,
			      char *name, unsigned int offset, size_t size)
{
	const struct firmware *blob = NULL;
	void __iomem *dram_offset;
	char path[100] = "";
	int ret = 0;

	strcat(path, name);
	dram_offset = mem->vaddr + offset;

	ret = __load_user_file(&blob, path, dev, size);
	if (ret)
	{
		npu_err("fail(%d) to load user file: %s\n", ret, path);
		goto exit;
	}

	memcpy((void *)dram_offset, (void *)blob->data, blob->size);

	mb();
	if (blob)
		release_firmware(blob);

exit:
	return ret;
}

static inline void set_state_bit(unpu_debug_state_bits_e state_bit)
{
	int old = test_and_set_bit(state_bit, &(unpu_debug_ref->state));

	if (old)
		npu_warn("state(%d): state set requested but already set.", state_bit);
}

static inline void clear_state_bit(unpu_debug_state_bits_e state_bit)
{
	int old = test_and_clear_bit(state_bit, &(unpu_debug_ref->state));

	if (!old)
		npu_warn("state(%d): state clear requested but already cleared.", state_bit);
}

static inline int check_state_bit(unpu_debug_state_bits_e state_bit)
{
	return test_bit(state_bit, &(unpu_debug_ref->state));
}

static int unpu_load_fw(void)
{
	struct unpu_device *unpu_device;
	struct unpu_iomem_data *mem;
	int ret = 0;

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	mem = &unpu_device->iomem_data[UNPU_SRAM];

	isb();
	msleep(2);

	npu_dbg("firmware load: vaddr: 0x%p paddr: 0x%x size: 0x%x\n",
			(mem)->vaddr, (mem)->paddr, (mem)->size);
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	ret = unpu_firmware_file_read_signature(unpu_device);
	if (ret) {
		npu_err("error(%d) in npu_firmware_file_read_signature\n", ret);
		goto p_err;
	}
#else
	ret = __load_image_to_io(unpu_device->dev, mem, UNPU_FW_NAME,
				 UNPU_FW_OFFSET, UNPU_FW_SIZE);
	if (ret) {
		npu_err("error(%d) in loading firmware.\n", ret);
		goto p_err;
	}
#endif

	mb();
p_err:
	return ret;
}

static int __attribute__((unused)) unpu_load_fw_ut(void)
{
	struct unpu_device *unpu_device;
	struct unpu_iomem_data *mem, *baaw_ctrl;
	struct unpu_rmem_data *rmem;
	int ret = 0;

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	baaw_ctrl = &unpu_device->iomem_data[UNPU_BAAW];
	mem = &unpu_device->iomem_data[UNPU_SRAM];
	rmem = &unpu_device->rmem_data;

	isb();
	msleep(2);

	ret = __load_image_to_io(unpu_device->dev, mem, UNPU_FW_UT_NAME,
				 UNPU_FW_UT_OFFSET, UNPU_FW_UT_SIZE);
	if (ret) {
		npu_err("loading firmware ut failed(%d)\n", ret);
		goto p_err;
	}

	mb();

	ret = unpu_config_baaw(baaw_ctrl, UNPU_FW_UT_DVA_START,
				(UNPU_FW_UT_DVA_END), (rmem->paddr + UNPU_FW_UT_BAAW_OFFSET));
	memset((rmem->vaddr + UNPU_FW_UT_BAAW_OFFSET), 0, UNPU_FW_UT_REGION_SZ);

	npu_dbg("load firmware ut: vaddr: 0x%p paddr: 0x%#llx size: 0x%x\n", (mem)->vaddr,
		((mem)->paddr + UNPU_FW_UT_OFFSET), UNPU_FW_UT_SIZE);
p_err:
	return ret;
}

static int unpu_load_and_run_ncp(void)
{
	struct unpu_iomem_data *baaw_ctrl;
	struct unpu_device *unpu_device;
	struct unpu_rmem_data *rmem;
	unsigned long state;
	int ret = 0, i = 0;
	u32 res;

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	baaw_ctrl = &unpu_device->iomem_data[UNPU_BAAW];
	rmem = &unpu_device->rmem_data;

	isb();
	msleep(2);

	/* load ncp binary */
	ret = __load_bin_to_dram(unpu_device->dev, rmem, UNPU_NCP_NAME,
						 0, UNPU_NCP_SIZE);
	if (ret) {
		npu_err("loading ncp failed(%d)\n", ret);
		goto p_err;
	}

	ret = unpu_config_baaw(baaw_ctrl, UNPU_NCP_DVA_START,
				UNPU_NCP_DVA_END, rmem->paddr);

	/* load input binary */
	ret = __load_bin_to_dram(unpu_device->dev, rmem, UNPU_NCP_IN_NAME,
				 UNPU_OFFSET_10KB, UNPU_NCP_IN_OUT_SIZE);
	if (ret) {
		npu_err("loading input failed(%d)\n", ret);
		goto p_err;
	}

	ret = unpu_config_baaw(baaw_ctrl, UNPU_NCP_IN_DVA_START,
				UNPU_NCP_IN_DVA_END, (rmem->paddr + UNPU_OFFSET_10KB));

	/* config baaw ofm */
	ret = unpu_config_baaw(baaw_ctrl, UNPU_NCP_OUT_DVA_START,
				UNPU_NCP_OUT_DVA_END, (rmem->paddr + (2 * UNPU_OFFSET_10KB)));

	/* load golden binary */
	ret = __load_bin_to_dram(unpu_device->dev, rmem, UNPU_NCP_OUT_NAME,
				 (3 * UNPU_OFFSET_10KB), UNPU_NCP_IN_OUT_SIZE);
	if (ret) {
		npu_err("loading input failed(%d)\n", ret);
		goto p_err;
	}

	mb();

	npu_dbg("load ncp: paddr: 0x%x vaddr: 0x%p size: 0x%x\n", rmem->paddr,
		rmem->vaddr, UNPU_NCP_SIZE);

	state = check_unpu_state(unpu_client_interface);
	if (state == UNPU_STATE_READY) {
		/* handle load command */
		unpu_nw_req_manager(UNPU_REQ_LOAD);
		wait_event(unpu_device->done_wq, check_done_for_cmd(unpu_client_interface, &res));
		unpu_nw_req_manager(UNPU_REQ_CLEAR_DONE);
		ret = wait_till_npu_run_not_zero(unpu_client_interface);
		if (ret) {
			npu_err("error during flag_npu_run not zero(%d)\n", ret);
			goto p_err;
		}

		state = check_unpu_state(unpu_client_interface);
		/* handle process command */
		if (res == UNPU_RES_LOAD && state == UNPU_STATE_READY) {
			unpu_nw_req_manager(UNPU_REQ_PROCESS);
			wait_event(unpu_device->done_wq, check_done_for_cmd(unpu_client_interface, &res));
			unpu_nw_req_manager(UNPU_REQ_CLEAR_DONE);
			ret = wait_till_npu_run_not_zero(unpu_client_interface);
			if (ret) {
				npu_err("error during flag_npu_run not zero(%d)\n", ret);
				goto p_err;
			}
		}

		state = check_unpu_state(unpu_client_interface);
		/* handle de-init command */
		if (res == UNPU_RES_PROCESS && state == UNPU_STATE_READY) {
			npu_dbg("received response after process command.\n");
		}

		if (res == UNPU_RES_ERR)
			npu_dbg("Error command received from fw.\n");
	}

	/* compare output and golden */
	for (i=0; i < (UNPU_NCP_IN_OUT_SIZE/ACCESS_4BYTE); i++) {
		if (readl(rmem->vaddr + (2 * UNPU_OFFSET_10KB) + (i * ACCESS_4BYTE)) !=
			readl(rmem->vaddr + (3 * UNPU_OFFSET_10KB) + (i * ACCESS_4BYTE))) {
			npu_err("Golden Bit Mismatch at 0x%x! output:(0x%x) golden(0x%x)\n", i,
				readl(rmem->vaddr + (2 * UNPU_OFFSET_10KB) + (i * ACCESS_4BYTE)),
				readl(rmem->vaddr + (3 * UNPU_OFFSET_10KB) + (i * ACCESS_4BYTE)));
			goto p_err;
		}
	}

	npu_dbg("Bit Matching and NCP test is completed\n");
p_err:
	return ret;
}

static int unpu_boot_fw(void)
{
	struct unpu_iomem_data *unpu_ctrl, *yamin_ctrl, *baaw_ctrl, *npumem_ctrl;
	struct unpu_rmem_data *rmem;
	struct unpu_device *unpu;
	int ret = 0, i = 0;
	unsigned long v_tmp;

	unpu = container_of(unpu_debug_ref, struct unpu_device, debug);
	baaw_ctrl = &unpu->iomem_data[UNPU_BAAW];
	unpu_ctrl = &unpu->iomem_data[UNPU_CTRL];
	yamin_ctrl = &unpu->iomem_data[YAMIN_CTRL];
	npumem_ctrl = &unpu->iomem_data[NPUMEM_CTRL_S];
	rmem = &unpu->rmem_data;

	for (i = 0; i < unpu->irq_num; i++) {
		ret = devm_request_irq(unpu->dev, unpu->irq[i], unpu_mbox_isr_list[i],
					IRQF_TRIGGER_HIGH, "exynos-unpu", NULL);
		if (ret) {
			npu_err("fail(%d) in devm_request_irq(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)unpu_ctrl->paddr + UNPU_REMAP_DEST_START),
		   UNPU_REMAP_DEST_RESET, 0x0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)unpu_ctrl->paddr + UNPU_REMAP_SRC_END),
		   UNPU_REMAP_SRC_RESET, 0x0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)unpu_ctrl->paddr), 0x1, 0x0);

	/* making npumem sram region non-secure by enabling flags */
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)npumem_ctrl->paddr + NPUMEM_REGION0_OFF),
		   NPUMEM_ENABLE_ACCESS, 0x0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)npumem_ctrl->paddr + NPUMEM_REGION1_OFF),
		   NPUMEM_ENABLE_ACCESS, 0x0);
	exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W((int)npumem_ctrl->paddr + NPUMEM_REGION2_OFF),
		   NPUMEM_ENABLE_ACCESS, 0x0);

	exynos_smc_readsfr(yamin_ctrl->paddr + UNPU_CPU_PC, &v_tmp);
	npu_dbg("cpupc val is 0x%#x\n", (unsigned int)v_tmp);

	ret = exynos_pmu_write(unpu->pmu_offset, 0x1);
	if (ret) {
		npu_err("pmu write is not succesful.\n");
		goto p_err;
	}

	for (i = 0; i < 300; i++) {
		mdelay(1);
		if (!(i % 100)) {
			exynos_smc_readsfr(yamin_ctrl->paddr + UNPU_CPU_PC, &v_tmp);
			npu_dbg("cpupc val is %#x\n", (unsigned int)v_tmp);
		}
	}

	/* init mailbox communication */
	unpu_nw_req_manager(UNPU_REQ_INIT);
	ret = wait_till_npu_state_not_ready(unpu_client_interface);
	if (!ret) {
		npu_err("error in waiting for client npu state(%d)\n", ret);
		return ret;
	}

	return ret;
err_probe_irq:
	for (i = 0; i < unpu->irq_num; i++) {
		devm_free_irq(unpu->dev, unpu->irq[i], NULL);
	}
p_err:
	return ret;
}

int unpu_nw_request_test(u32 param) {
	int ret = 0;
	struct unpu_device *unpu_device;
	struct unpu_client_interface c_interface = {};

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);

	c_interface.flag_npu_run = 0x1;
	c_interface.client_request = UNPU_REQ_TEST;
	for (int i = 0; i < UNPU_CLIENT_INTERFACE_MAX_REGS; i++)
		c_interface.data[i] = 0x0;
	/* send process command */
	c_interface.data[0] = param;
	unpu_set_cmd(c_interface);

	/* send interrupt to fw*/
	ret = __unpu_send_interrupt(unpu_mbox_h2f_interface, 0x100);
	if (ret) {
		npu_err("%s() send interrupt failed from host to fw(%d)\n", __func__,  ret);
		return ret;
	}

	ret = wait_till_npu_done_not_zero(unpu_client_interface);
	if (ret) {
		npu_err("%s() error due to flag_npu_done zero currently(%d)\n", __func__, ret);
		return ret;
	}

	gnpu_if->rslt_notifier(NULL);

	return ret;
}

int unpu_ncp_test(void) {
	int ret = 0;

	unpu_load_and_run_ncp();
	gnpu_if->rslt_notifier(NULL);

	return ret;
}

int unpu_power_ctl(struct unpu_device *device, bool is_on) {
	int ret = 0;
	bool active;
	u32 res;

	if (is_on) {
		active = pm_runtime_active(device->dev);
		npu_dbg("uNPU is already turned %s\n",active ? "on":"off");
		if (!active) {
			/* power control */
			ret = pm_runtime_get_sync(device->dev);
			if (ret)
				npu_err("fail in runtime resume(%d)\n", ret);
		}
		unpu_load_fw();
		unpu_load_fw_ut();
		unpu_boot_fw();
	} else {
		/* power control */
		unpu_nw_req_manager(UNPU_REQ_DEINIT);
		wait_event(device->done_wq, check_done_for_cmd(unpu_client_interface, &res));

		unpu_nw_req_manager(UNPU_REQ_CLEAR_DONE);
		ret = wait_till_npu_run_not_zero(unpu_client_interface);
		if (ret) {
			npu_err("%s() error during flag_npu_run not zero(%d)\n", __func__, ret);
			return ret;
		}

		msleep(100);
		ret = pm_runtime_put_sync(device->dev);
		if (ret)
			npu_err("fail in runtime suspend(%d)\n", ret);
	}

	npu_info("%s (%d)\n", is_on ? "on":"off", ret);
	return ret;
}

static int unpu_pwr_ctl_and_boot_show(struct seq_file *file, void *unused)
{
	seq_printf(file, "echo 1 > /d/unpu/power_ctrl_and_boot to power on and boot FW\n");

	return 0;
}

static int unpu_pwr_ctl_and_boot_open(struct inode *inode, struct file *file)
{
	return single_open(file, unpu_pwr_ctl_and_boot_show,
			inode->i_private);
}

static ssize_t unpu_pwr_ctl_and_boot_store(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct unpu_device *unpu;
	int ret = 0, pwr_ctrl;
	char buf[30];
	ssize_t size;

	unpu = container_of(unpu_debug_ref, struct unpu_device, debug);

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = kstrtoint(buf, 10, &pwr_ctrl);
	if (ret) {
		npu_err("Failed to get fw_load parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}

	ret = unpu_power_ctl(unpu, pwr_ctrl);
	if (ret)
		npu_err("failed(%d) in unpu_power_control\n", ret);
p_err:
	return ret;
}

static const struct file_operations unpu_pwr_ctl_and_boot_fops = {
	.open		= unpu_pwr_ctl_and_boot_open,
	.read		= seq_read,
	.write		= unpu_pwr_ctl_and_boot_store,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int unpu_fw_report_show(struct seq_file *file, void *unused)
{

	seq_printf(file, "unpu_fw_report.\n");

	return 0;
}

static int unpu_fw_report_open(struct inode *inode, struct file *file)
{
	return single_open(file, unpu_fw_report_show,
			inode->i_private);
}

static ssize_t unpu_fw_report_read(struct file *file, char __user *outbuf,
			       size_t outlen, loff_t *loff)
{
	struct unpu_device *unpu_device;
	struct unpu_iomem_data *npumem_sram;
	u32 log_size, ret = 0;
	char *tmp_buf;

	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	log_size = UNPU_FW_LOG_SIZE;
	npumem_sram = &unpu_device->iomem_data[NPUMEM_SRAM];

	tmp_buf = kzalloc(log_size, GFP_KERNEL);
	if (!tmp_buf) {
		ret = -ENOMEM;
		goto err_exit;
	}

	isb();

	memcpy(tmp_buf, (npumem_sram->vaddr + UNPU_FW_LOG_OFFSET), log_size);

	mb();

	ret = copy_to_user(outbuf, tmp_buf, outlen);
	if (ret) {
		npu_err("copy_to_user failed : 0x%x\n", ret);
		ret = -EFAULT;
		goto err_exit;
	}

	ret = outlen;
err_exit:
	if (tmp_buf)
		kfree(tmp_buf);
	return ret;
}

static const struct file_operations unpu_fw_report_fops = {
	.open		= unpu_fw_report_open,
	.read		= unpu_fw_report_read,
	.release	= single_release
};

static u32 tmp_reg_addr = 0x1511004C;  /* yamin cpupc value */
static int sreg_show(struct seq_file *file, void *unused)
{
#if !IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	void __iomem *vaddr = NULL;
#endif
	unsigned long value;

	seq_printf(file, "echo 0x<address> > /d/unpu/access_sreg\n");
#if IS_ENABLED(CONFIG_SOC_S5E9945_EVT0)
	exynos_smc_readsfr((unsigned long)tmp_reg_addr, &value);
#else
	vaddr = ioremap(tmp_reg_addr, 0x4);
	value = readl(vaddr);
	if (vaddr)
		iounmap(vaddr);
#endif
	seq_printf(file, "address = %08x, value: %#lx\n", tmp_reg_addr, value);

	return 0;
}

static int unpu_sreg_read_open(struct inode *inode, struct file *file)
{
	return single_open(file, sreg_show,
			inode->i_private);
}
static ssize_t unpu_sreg_store(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[30];
	ssize_t size;
	u32 address;
	int ret;

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "0x%xd", &address);
	if (ret != 1)
	{
		npu_err("fail(%d) in parsing sreg address & value\n", ret);
		return -EINVAL;
	}
	tmp_reg_addr = address;

	npu_dbg("address = %08x\n", address);

p_err:
	return ret;
}

static const struct file_operations unpu_sreg_fops = {
	.open		= unpu_sreg_read_open,
	.read		= seq_read,
	.write		= unpu_sreg_store,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int of_get_irq_count(struct device_node *dev)
{
	struct of_phandle_args irq;
	int nr = 0;

	while (of_irq_parse_one(dev, nr, &irq) == 0)
		nr++;

	return nr;
}

static int unpu_platform_get_irq(struct unpu_device *unpu) {
	int i, irq;

	unpu->irq_num = of_get_irq_count(unpu->pdev->dev.of_node);

	if (unpu->irq_num != UNPU_MAX_IRQ) {
		probe_err("irq number for uNPU from dts(%d) and required irq's(%d)"
			"didnt match\n", unpu->irq_num, UNPU_MAX_IRQ);
		irq = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < unpu->irq_num; i++) {
		irq = platform_get_irq(unpu->pdev, i);
		if (irq < 0) {
			probe_err("fail(%d) in platform_get_irq(%d)\n", irq, i);
			irq = -EINVAL;
			goto p_err;
		}
		unpu->irq[i] = irq;
	}

p_err:
	return irq;
}

static int unpu_interface_probe(phys_addr_t regs1, phys_addr_t regs2)
{
	int ret = 0;

	if (!regs1 || !regs2) {
		probe_err("fail\n");
		ret = -EINVAL;
		return ret;
	}

	unpu_mbox_h2f_interface.intr = (volatile unsigned int)regs1;
	unpu_mbox_h2f_interface.verstat = (volatile unsigned int)regs1 + 0x4;
	unpu_mbox_h2f_interface.npu_signature = (volatile unsigned int)regs1 + 0x8;
	unpu_mbox_h2f_interface.client_interface_ptr = (volatile unsigned int)regs1 + 0xC;
	unpu_mbox_h2f_interface.client_signature = (volatile unsigned int)regs1 + 0x10;

	unpu_mbox_f2h_interface.intr = (volatile unsigned int)regs2;
	unpu_mbox_f2h_interface.verstat = (volatile unsigned int)regs2 + 0x4;
	unpu_mbox_f2h_interface.npu_signature = (volatile unsigned int)regs2 + 0x8;
	unpu_mbox_f2h_interface.client_interface_ptr = (volatile unsigned int)regs2 + 0xC;
	unpu_mbox_f2h_interface.client_signature = (volatile unsigned int)regs2 + 0x10;

	return ret;
}

static int unpu_mbox_probe(phys_addr_t regs1)
{
	int ret = 0, i;

	if (!regs1) {
		probe_err("fail\n");
		ret = -EINVAL;
		return ret;
	}

	unpu_client_interface.npu_state = (volatile unsigned int)regs1;
	unpu_client_interface.flag_npu_run = (volatile unsigned int)regs1 + 0x4;
	unpu_client_interface.flag_npu_done = (volatile unsigned int)regs1 + 0x8;
	unpu_client_interface.client_request = (volatile unsigned int)regs1 + 0xC;
	unpu_client_interface.npu_response = (volatile unsigned int)regs1 + 0x10;
	for (i = 0; i < UNPU_CLIENT_INTERFACE_MAX_REGS; i++)
		unpu_client_interface.data[i] = (volatile unsigned int)(regs1 + 0x14 + (i * ACCESS_4BYTE));
	unpu_client_interface.magic = (volatile unsigned int)(regs1 + 0xB4);

	return ret;
}

int unpu_debug_register_arg(const char *name, void *private_arg, const struct file_operations *ops)
{
	struct unpu_device	*unpu_device;
	struct dentry *dbgfs_entry;
	struct device *dev;
	mode_t mode = 0;
	int ret	= 0;

	/* Check whether the debugfs is properly initialized */
	if (!check_state_bit(FS_READY)) {
		probe_warn("DebugFS not initialized or disabled. Skip creation [%s]\n", name);
		ret = 0;
		goto err_exit;
	}

	/* Default parameter is npu_debug object */
	if (private_arg == NULL)
		private_arg = (void *)unpu_debug_ref;

	/* Retrieve struct device to use devm_XX api */
	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	BUG_ON(!unpu_device);
	dev = unpu_device->dev;
	BUG_ON(!dev);

	if (name == NULL) {
		npu_err("name is null\n");
		ret = -EFAULT;
		goto err_exit;
	}
	if (ops == NULL) {
		npu_err("ops is null\n");
		ret = -EFAULT;
		goto err_exit;
	}

	/* Setting file permission based on file_operation member */
	if (ops->read || ops->compat_ioctl || ops->unlocked_ioctl)
		mode |= 0400;	/* Read permission to owner */

	if (ops->write)
		mode |= 0200;	/* Write permission to owner */

	/* Register */
	dbgfs_entry = debugfs_create_file(name, mode, unpu_debug_ref->dfile_root, private_arg, ops);
	if (IS_ERR(dbgfs_entry)) {
		probe_err("fail in unpu DebugFS registration (%s)\n", name);
		ret = PTR_ERR(dbgfs_entry);
		goto err_exit;
	}
	probe_info("success in unpu DebugFS registration (%s) : Mode %04o\n", name, mode);
	return 0;

err_exit:
	return ret;
}

int unpu_debug_register(const char *name, const struct file_operations *ops)
{
	int ret = 0, debugfs_enable = 0;

#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_enable = 1;
	ret = unpu_debug_register_arg(name, NULL, ops);
	if (ret)
		probe_err("failed (%d)", ret);
#endif

	probe_info("register %s debugfs_state in unpu %s", name,
			   debugfs_enable ? "enabled" : "disabled");
	return ret;
}

int unpu_debug_probe(struct unpu_device *unpu)
{
	int ret = 0;

	BUG_ON(!unpu);

	/* Save reference */
	unpu_debug_ref = &unpu->debug;
	memset(unpu_debug_ref, 0, sizeof(*unpu_debug_ref));

	unpu_debug_ref->dfile_root = debugfs_create_dir(DEBUG_FS_UNPU_ROOT_NAME, NULL);
	if (!unpu_debug_ref->dfile_root) {
		dev_err(unpu->dev, "Loading npu_debug : debugfs root [%s] can not be created\n",
				DEBUG_FS_UNPU_ROOT_NAME);

		unpu_debug_ref->dfile_root = NULL;
		ret = -ENOENT;
		goto err_exit;
	}

	dev_dbg(unpu->dev, "Loading unpu_debug : debugfs root [%s] created\n" ,
			DEBUG_FS_UNPU_ROOT_NAME);
	set_state_bit(FS_READY);

	ret = unpu_debug_register(DEBUG_FS_UNPU_PWR_CTRL, &unpu_pwr_ctl_and_boot_fops);
	if (ret) {
		probe_err("loading unpu_debug : debugfs to turn on unpu and boot fw can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = unpu_debug_register(DEBUG_FS_UNPU_LOG_NAME, &unpu_fw_report_fops);
	if (ret) {
		probe_err("loading unpu_debug : debugfs for getting unpu fw-report can not be created(%d)\n", ret);
		goto err_exit;
	}

	ret = unpu_debug_register(DEBUG_FS_UNPU_SREG, &unpu_sreg_fops);
	if (ret) {
		probe_err("loading npu_debug : debugfs for sreg access can not be created(%d)\n", ret);
		goto err_exit;
	}

	return 0;

err_exit:
	return ret;
}

static int unpu_get_iomem_from_dt(struct platform_device *pdev, const char *name,
								  struct unpu_iomem_data *data)
{
	struct resource *res;

	if (!pdev || !data | !name)
		return -EINVAL;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		dev_err(&pdev->dev, "REG name %s not defined for UNPU\n", name);
		return 0;
	}

	data->vaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->vaddr)) {
		dev_err(&pdev->dev, "IO remap failed for region %s\n", name);
		return -EINVAL;
	}
	data->paddr = res->start;
	data->size = resource_size(res);

	dev_info(&pdev->dev, "UNPU IOMEM: name: %s vaddr: 0x%p paddr: 0x%#llx size: 0x%x\n",
			 name, data->vaddr, data->paddr, (unsigned int)data->size);

	return 0;
}

static int unpu_get_rmem_from_dt(struct platform_device *pdev, struct unpu_rmem_data *data)
{
	struct device_node *mems_node, *mem_node, *phdl_node;
	struct device *dev = &pdev->dev;
	struct resource res;
	unsigned int rc;
	int  ret, size;

	/* getting reserved mem from dt */
	mems_node = of_get_child_by_name(dev->of_node, "samsung,unpurmem-address");
	if (!mems_node) {
		ret = 0;    /* not an error */
		probe_err("null npurmem-address node\n");
		goto err_data;
	}

	for_each_child_of_node(mems_node, mem_node) {
		data->name = kbasename(mem_node->full_name);

		/* reserved mem case */
		phdl_node = of_parse_phandle(mem_node,
				"memory-region", 0);
		if (!phdl_node) {
			ret = -EINVAL;
			probe_err("fail to get memory-region in name(%s)\n", data->name);
			goto err_data;
		}

		rc = of_address_to_resource(phdl_node, 0, &res);
		if (rc) {
			  dev_err(dev, "No memory address assigned to the region\n");
			    goto err_data;
		}

		ret = of_property_read_u32(mem_node,
					"size", &size);
		if (ret) {
			probe_err("'size' is mandatory but not defined (%d)\n", ret);
			goto err_data;
		}
		data->size = size;
		data->paddr = res.start;
		data->vaddr = memremap(res.start, resource_size(&res), MEMREMAP_WB);
	}
	probe_info("reserved mem paddr: 0x%llx, vaddr: 0x%p size: 0x%llx\n",
		   data->paddr, data->vaddr,data->size);
	ret = 0;
err_data:
	return ret;
}

static int unpu_mem_probe(struct platform_device *pdev, struct unpu_device *unpu)
{
	struct device *dev = &pdev->dev;
	u32 pmu_offset;
	int i, ret;

	/* getting offset from  dt */
	ret = of_property_read_u32(dev->of_node, "samsung,unpu-pmu-offset", &pmu_offset);
	if (ret) {
		npu_err("parsing pmu-offset from dt failed.\n");
		return ret;
	}
	unpu->pmu_offset = pmu_offset;

	for (i = 0 ; i < UNPU_MAX_IOMEM; i++) {
		ret = unpu_get_iomem_from_dt(pdev, reg_names[i], &unpu->iomem_data[i]);
		if (ret)
			return ret;
	}

	ret = unpu_get_rmem_from_dt(pdev, &unpu->rmem_data);
	if (ret)
		return ret;

	return 0;
}

int unpu_debug_release(void)
{
	struct unpu_device *unpu_device;
	struct device *dev;
	int ret = 0;

	if (!check_state_bit(FS_READY)) {
		/* No need to clean-up */
		probe_err("not ready in npu_debug\n");
		return -1;
	}

	/* Retrieve struct device to use devm_XX api */
	unpu_device = container_of(unpu_debug_ref, struct unpu_device, debug);
	dev = unpu_device->dev;

	/* Remove debug fs entries */
	debugfs_remove_recursive(unpu_debug_ref->dfile_root);
	probe_info("unloading unpu_debug: root node is removed.\n");

	clear_state_bit(FS_READY);
	return ret;
}

#if IS_ENABLED(CONFIG_UNPU_POWER_NOTIFIER)
int unpu_on_off(struct notifier_block *noti_data, unsigned long on, void *data)
{
	struct unpu_device *device;
	bool is_on = (bool) on;
	int ret = 0;

	device = container_of(noti_data, struct unpu_device, noti_data_for_unpu);
	npu_info("abox's request of power %s arrived\n", is_on ? "on":"off");
	ret = unpu_power_ctl(device, is_on);
	if (ret)
		npu_err("failed(%d) in unpu_power_control\n", ret);

	return ret;
}

extern int abox_register_pwr_notifier(struct notifier_block *nb);
extern int abox_unregister_pwr_notifier(struct notifier_block *nb);

int __abox_register_pwr_notifier(struct notifier_block *nb)
{
	return abox_register_pwr_notifier(nb);
}

int __abox_unregister_pwr_notifier(struct notifier_block *nb)
{
	return abox_unregister_pwr_notifier(nb);
}
#endif

static int unpu_device_probe(struct platform_device *pdev)
{
	struct unpu_iomem_data *unpu_ctrl, *unpu_sram;
	struct device *dev = &pdev->dev;
	struct unpu_device *unpu;
	phys_addr_t addr1, addr2;
	int ret;

	unpu = devm_kzalloc(dev, sizeof(*unpu), GFP_KERNEL);
	if (!unpu) {
		dev_err(dev, "fail in devm_kzalloc\n");
		return -ENOMEM;
	}
	unpu->dev = dev;
	unpu->pdev = pdev;

	ret = unpu_platform_get_irq(unpu);
	if (ret < 0) {
		dev_err(dev, "unpu_platform_get_irq failed. ret = %d\n", ret);
	}

	ret = unpu_mem_probe(pdev, unpu);
	if (ret) {
		dev_err(dev, "unpu_mem_probe failed. ret = %d\n", ret);
		return ret;
	}

	unpu_ctrl = &unpu->iomem_data[UNPU_CTRL];
	addr1 = (unpu_ctrl->paddr + 0x02E0);
	addr2 = (unpu_ctrl->paddr + 0x0250);
	ret = unpu_interface_probe(addr1, addr2);
	if (ret) {
		dev_err(dev, "unpu_interface_probe is fail(%d)\n", ret);
		return ret;
	}

	unpu_sram = &unpu->iomem_data[UNPU_SRAM];
	addr1 = (unpu_sram->paddr + UNPU_HOST_MBOX_OFFSET);

	ret = unpu_mbox_probe(addr1);
	if (ret) {
		dev_err(dev, "unpu_mbox_probe is fail(%d)\n", ret);
		return ret;
	}

	ret = unpu_debug_probe(unpu);
	if (ret) {
		dev_err(dev, "fail(%d) in unpu_debug_probe\n", ret);
		return ret;
	}

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	ret = unpu_imgloader_probe(unpu, dev);
	if (ret) {
		dev_err(dev, "unpu_imgloader_probe is fail(%d)\n", ret);
		goto clean_debug_probe;
	}
#endif
	pm_runtime_enable(dev);
	dev_set_drvdata(dev, unpu);

#if IS_ENABLED(CONFIG_UNPU_POWER_NOTIFIER)
	unpu->noti_data_for_unpu.notifier_call = unpu_on_off;
	dev_info(dev, "abox_register_pwr_notifier called\n");
	__abox_register_pwr_notifier(&unpu->noti_data_for_unpu);
#endif

	init_waitqueue_head(&unpu->done_wq);
	ret = 0;
	dev_info(dev, "unpu probe is successful \n");

	return ret;

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
clean_debug_probe:
	unpu_debug_release();
#endif

	return ret;
}

static int unpu_device_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
#if IS_ENABLED(CONFIG_UNPU_POWER_NOTIFIER)
	struct unpu_device *unpu;
#endif
	int ret = 0;

	ret = unpu_debug_release();
	if (ret)
		probe_err("fail(%d) in unpu_debug_release\n", ret);

#if IS_ENABLED(CONFIG_UNPU_POWER_NOTIFIER)
	unpu = dev_get_drvdata(dev);
	probe_info("abox_unregister_pwr_notifier called\n");
	__abox_unregister_pwr_notifier(&unpu->noti_data_for_unpu);
#endif

	pm_runtime_disable(dev);
	probe_info("completed in %s\n", __func__);
	return ret;
}
#if IS_ENABLED(CONFIG_PM_SLEEP)
static int unpu_suspend(struct device *dev)
{
	npu_dbg("called\n");
	return 0;
}

static int unpu_resume(struct device *dev)
{
	npu_dbg("called\n");
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_PM)
static int unpu_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct unpu_device *unpu;

	unpu = container_of(unpu_debug_ref, struct unpu_device, debug);
	dev_set_drvdata(dev, unpu);

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	imgloader_shutdown(&unpu->imgloader);
#endif

	npu_dbg("called\n");

	return ret;
}

static int unpu_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct unpu_device *unpu;

	unpu = container_of(unpu_debug_ref, struct unpu_device, debug);
	dev_set_drvdata(dev, unpu);

	npu_dbg("called\n");
	return ret;
}
#endif

static const struct dev_pm_ops unpu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(unpu_suspend, unpu_resume)
	SET_RUNTIME_PM_OPS(unpu_runtime_suspend, unpu_runtime_resume, NULL)
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id exynos_unpu_match[] = {
	{
		.compatible = "samsung,exynos-unpu"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_unpu_match);
#endif

static struct platform_driver unpu_driver = {
	.probe	= unpu_device_probe,
	.remove = unpu_device_remove,
	.driver = {
		.name	= "exynos-unpu",
		.owner	= THIS_MODULE,
		.pm	= &unpu_pm_ops,
		.of_match_table = of_match_ptr(exynos_unpu_match),
	},
}
;

int __init unpu_register(void)
{
	int ret = 0;

	ret = platform_driver_register(&unpu_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		return ret;
	}

	return ret;
}

