/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include "include/npu-common.h"
#include "npu-debug.h"
#include "npu-util-memdump.h"
#include "npu-util-regs.h"
#include "include/npu-memory.h"
#include "npu-scheduler.h"
#include "npu-ver.h"
#include "dsp-dhcp.h"

#if IS_ENABLED(CONFIG_SOC_S5E9955)
#define MAX_IOMAP_COUNT (256)
#define PRINT_NUM (8)
static void npu_util_dump_npu_dtcm_for_imb(struct npu_system *system)
{
	int i = 0;
	u32 val[MAX_IOMAP_COUNT] = {0, };

	npu_info("IOmap for GNPU0\n");
	for (i = 0; i < MAX_IOMAP_COUNT; i++) {
		val[i] = npu_read_hw_reg(npu_get_io_area(system, "sfrgnpu0"), (0x8208 + (i * 4)), 0xFFFFFFFF, 2);
	}

	for (i = 0; i < (MAX_IOMAP_COUNT / PRINT_NUM); i++) {
		npu_info("0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
			val[PRINT_NUM * i + 0], val[PRINT_NUM * i + 1],
			val[PRINT_NUM * i + 2], val[PRINT_NUM * i + 3],
			val[PRINT_NUM * i + 4], val[PRINT_NUM * i + 5],
			val[PRINT_NUM * i + 6], val[PRINT_NUM * i + 7]);
	}

	npu_info("IOmap for GNPU1\n");
	for (i = 0; i < MAX_IOMAP_COUNT; i++) {
		val[i] = npu_read_hw_reg(npu_get_io_area(system, "sfrgnpu1"), (0x8208 + (i * 4)), 0xFFFFFFFF, 2);
	}

	for (i = 0; i < (MAX_IOMAP_COUNT / PRINT_NUM); i++) {
		npu_info("0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
			val[PRINT_NUM * i + 0], val[PRINT_NUM * i + 1],
			val[PRINT_NUM * i + 2], val[PRINT_NUM * i + 3],
			val[PRINT_NUM * i + 4], val[PRINT_NUM * i + 5],
			val[PRINT_NUM * i + 6], val[PRINT_NUM * i + 7]);
	}
}

#define MAX_BARAC_COUNT (16)
#define VALID_BARAC_COUNT (5)
#define VALID_INFO_COUNT (80)
static void npu_util_dump_npu_barac_for_imb(struct npu_system *system)
{
	int i, j, k = 0;
	u32 val[VALID_INFO_COUNT] = {0, };

	npu_info("Barac for IMB\n");
	for (i = 0; i < MAX_BARAC_COUNT; i++) {
		for (j = 0; j < VALID_BARAC_COUNT; j++) {
			val[k] = npu_read_hw_reg(npu_get_io_area(system, "sfrsdma"),
				(0x31800 + (i * 0x80) + (j * 4)), 0xFFFFFFFF, 2);
			k++;
		}
	}

	for (i = 0; i < MAX_BARAC_COUNT; i++) {
		j = i * VALID_BARAC_COUNT;
		npu_info("0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
			val[j + 0], val[j + 1], val[j + 2], val[j + 3], val[j + 4]);
	}
}
#endif

void npu_util_dump_handle_nrespone(struct npu_system *system)
{
	int i = 0;
	struct npu_device *device = NULL;

	device = container_of(system, struct npu_device, system);

	for (i = 0; i < 5; i++)
		npu_soc_status_report(&device->system);

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	{
		union dsp_dhcp_pwr_ctl pwr_ctl;

		pwr_ctl.value = device->system.dhcp_table->DNC_PWR_CTRL;

		if (pwr_ctl.npu_pm)
			npu_cmd_map_dump(&device->system, "gnpucmdqpc");
		if (pwr_ctl.dsp_pm)
			npu_cmd_map_dump(&device->system, "dspcmdqpc");

		npu_read_hw_reg(npu_get_io_area(system, "sfrmbox1"), 0x2000, 0xFFFFFFFF, 0);
	}
#endif

	npu_ver_dump(device);
	fw_will_note(FW_LOGSIZE);
	npu_memory_dump(&device->system.memory);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_log_fw_governor(device);
	npu_info("sysmmu table current vid : %u\n", device->system.memory.last_sysmmu_state);
	npu_util_dump_npu_dtcm_for_imb(system);
	npu_util_dump_npu_barac_for_imb(system);
#endif
	session_fault_listener();

	/* trigger a wdreset to analyse s2d dumps in this case */
	dbg_snapshot_expire_watchdog();
}

int npu_util_dump_handle_error_k(struct npu_device *device)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < 5; i++)
		npu_soc_status_report(&device->system);

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	union dsp_dhcp_pwr_ctl pwr_ctl;

	pwr_ctl.value = device->system.dhcp_table->DNC_PWR_CTRL;

	if (pwr_ctl.npu_pm) {
		npu_dump("GNPU 0/1 PC Value\n");
		npu_cmd_map_dump(&device->system, "gnpucmdqpc");
	}
	if (pwr_ctl.dsp_pm) {
		npu_dump("DSP PC Value\n");
		npu_cmd_map_dump(&device->system, "dspcmdqpc");
	}

	npu_read_hw_reg(npu_get_io_area(&device->system, "sfrmbox1"), 0x2000, 0xFFFFFFFF, 0);
#endif

	npu_ver_dump(device);
	fw_will_note(FW_LOGSIZE);
	npu_memory_dump(&device->system.memory);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_log_fw_governor(device);
	npu_info("sysmmu table current vid : %u\n", device->system.memory.last_sysmmu_state);
	npu_util_dump_npu_dtcm_for_imb(&device->system);
	npu_util_dump_npu_barac_for_imb(&device->system);
#endif
	session_fault_listener();

	/* trigger a wdreset to analyse s2d dumps in this case */
	dbg_snapshot_expire_watchdog();

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////
// Lifecycle functions

int npu_util_memdump_probe(struct npu_system *system)
{
	return 0;
}

int npu_util_memdump_open(struct npu_system *system)
{
	/* NOP */
	return 0;
}

int npu_util_memdump_close(struct npu_system *system)
{
	/* NOP */
	return 0;
}
