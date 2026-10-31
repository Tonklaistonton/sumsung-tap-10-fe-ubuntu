/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/pm_runtime.h>

#include "include/npu-common.h"
#include "npu-device.h"
#include "npu-session.h"
#include "npu-vertex.h"
#include "npu-hw-device.h"
#include "npu-log.h"
#include "npu-util-regs.h"
#include "npu-util-memdump.h"
#include "npu-pm.h"
#include "npu-afm.h"
#include "dsp-dhcp.h"
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
#include "npu-governor.h"
#endif
#if IS_ENABLED(CONFIG_NPU_USE_ESCA_DTM)
#include "npu-dtm.h"
#endif

static unsigned int suspend_resume;

static struct npu_device *npu_pm_dev;

extern u32 g_hwdev_num;
extern struct npu_proto_drv *protodr;
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
extern struct npu_precision *npu_precision;
#endif

void npu_pm_wake_lock(struct npu_session *session)
{
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_system *system;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);
	system = &device->system;

#if IS_ENABLED(CONFIG_PM_SLEEP)
	/* prevent the system to suspend */
	if (!npu_wake_lock_active(system->ws)) {
		npu_wake_lock(system->ws);
		npu_udbg("wake_lock, now(%d)\n", session, session->hids,
				npu_wake_lock_active(system->ws));
	}
#endif
}

void npu_pm_wake_unlock(struct npu_session *session)
{
	struct npu_vertex *vertex;
	struct npu_device *device;
	struct npu_system *system;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);
	system = &device->system;

#if IS_ENABLED(CONFIG_PM_SLEEP)
	if (npu_wake_lock_active(system->ws)) {
		npu_wake_unlock(system->ws);
		npu_udbg("wake_unlock, now(%d)\n", session, session->hids,
				npu_wake_lock_active(system->ws));
	}
#endif
}

static bool npu_pm_check_power_on(struct npu_system *system, const char *domain)
{
	int hi;
	struct npu_hw_device *hdev;

	for (hi = 0; hi < g_hwdev_num; hi++) {
		hdev = system->hwdev_list[hi];
		if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
			if (!strcmp(hdev->name, domain)) {
				return true;
			}
		}
	}

	return false;
}

static void npu_pm_suspend_setting_sfr(struct npu_system *system)
{
	bool power_on;
	power_on = npu_pm_check_power_on(system, "NPU");
#if IS_ENABLED(CONFIG_NPU_AFM)
	if (system->afm->afm_ops->afm_close_setting_sfr)
		system->afm->afm_ops->afm_close_setting_sfr(system, power_on);
#endif
	return;
}

static int npu_pm_resume_setting_sfr(struct npu_system *system)
{
	int ret = 0;
	bool power_on;

	ret = npu_cmd_map(system, "llcaid");
	if (ret) {
		npu_err("fail(%d) in npu_llcaid_init\n", ret);
		goto p_err;
	}

	power_on = npu_pm_check_power_on(system, "NPU");
#if IS_ENABLED(CONFIG_NPU_AFM)
	if (system->afm->afm_ops->afm_open_setting_sfr)
		system->afm->afm_ops->afm_open_setting_sfr(system, power_on);
#endif

p_err:
	return ret;
}

static const char *npu_check_fw_arch(struct npu_system *system,
				struct npu_memory_buffer *fwmem)
{
	if (!strncmp(FW_64_SYMBOL, fwmem->dma.vaddr + FW_SYMBOL_OFFSET, FW_64_SYMBOL_S)) {
		npu_info("FW is 64 bit, cmd map %s\n", FW_64_BIT);
		return FW_64_BIT;
	}

	npu_info("FW is 32 bit, cmd map : %s\n", FW_32_BIT);
	return FW_32_BIT;
}

static int npu_pm_check_power_and_on(struct npu_system *system)
{
	int hi;
	int ret = 0;
	struct npu_hw_device *hdev;

	for (hi = 0; hi < g_hwdev_num; hi++) {
		hdev = system->hwdev_list[hi];
		if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
			npu_info("find %s, and try power on\n", hdev->name);
			ret = pm_runtime_get_sync(hdev->dev);
			if (ret && (ret != 1)) {
				npu_err("fail in runtime resume(%d)\n", ret);
				goto err_exit;
			}
		}
	}
	return 0;
err_exit:
	return ret;
}

static int npu_pm_check_power_and_off(struct npu_system *system)
{
	int hi;
	int ret = 0;
	struct npu_hw_device *hdev;

	for (hi = g_hwdev_num - 1; hi >= 0; hi--) {
		hdev = system->hwdev_list[hi];
		if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
			npu_info("find %s, and try power off\n", hdev->name);
			ret = pm_runtime_put_sync(hdev->dev);
			if (ret) {
				npu_err("fail in runtime suspend(%d)\n", ret);
				goto err_exit;
			}
		}
	}
err_exit:
	return ret;
}

static void npu_pm_update_dhcp_power_status(struct npu_system *system, bool on)
{
	int hi;
	struct npu_hw_device *hdev;
	struct npu_session *session;

	session = npu_pm_dev->first_session;

	if (on) {
		for (hi = 0; hi < g_hwdev_num; hi++) {
			hdev = system->hwdev_list[hi];
			if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
				npu_info("find %s, and update dhcp for power on\n", hdev->name);
				if (!strcmp(hdev->name, "NPU") || !strcmp(hdev->name, "DSP")) {
					dsp_dhcp_update_pwr_status(npu_pm_dev, hdev->id, true);
					if (!strcmp(hdev->name, "NPU")) {
						session->hids = NPU_HWDEV_ID_NPU;
					} else { // DSP
						session->hids = NPU_HWDEV_ID_DSP;
					}
					session->nw_result.result_code = NPU_NW_JUST_STARTED;
					npu_session_put_nw_req(session, NPU_NW_CMD_POWER_CTL);
					wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
				}
			}
		}
	} else {
		for (hi = g_hwdev_num - 1; hi >= 0; hi--) {
			hdev = system->hwdev_list[hi];
			if (hdev && (atomic_read(&hdev->boot_cnt.refcount) >= 1)) {
				npu_info("find %s, and update dhcp for power off\n", hdev->name);
				if (!strcmp(hdev->name, "NPU") || !strcmp(hdev->name, "DSP")) {
					dsp_dhcp_update_pwr_status(npu_pm_dev, hdev->id, false);
					if (!strcmp(hdev->name, "NPU")) {
						session->hids = NPU_HWDEV_ID_NPU;
					} else { // DSP
						session->hids = NPU_HWDEV_ID_DSP;
					}
					session->nw_result.result_code = NPU_NW_JUST_STARTED;
					npu_session_put_nw_req(session, NPU_NW_CMD_SUSPEND);
					wait_event(session->wq, session->nw_result.result_code != NPU_NW_JUST_STARTED);
				}
			}
		}
	}

	if (chk_nw_result_no_error(session) != NPU_ERR_NO_ERROR)
		npu_util_dump_handle_nrespone(system);
}

int npu_pm_suspend(struct device *dev)
{
	int ret = 0;
	struct npu_system *system = &npu_pm_dev->system;
	struct dhcp_table *dhcp_table = system->dhcp_table;
	struct npu_device *device;

	device = container_of(system, struct npu_device, system);

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	struct npu_scheduler_info *info = device->sched;
#endif
#if IS_ENABLED(CONFIG_NPU_AFM)
	struct npu_afm *afm = system->afm;
#endif

	npu_dbg("start(0x%x)\n", dhcp_table->DNC_PWR_CTRL);
	if (dhcp_table->DNC_PWR_CTRL) {
#if IS_ENABLED(CONFIG_NPU_USE_ESCA_DTM)
		ret = npu_dtm_close(device);
		if (ret)
			npu_err("fail(%d) in npu_dtm_close\n", ret);
#endif
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
		cancel_delayed_work_sync(&info->sched_work);
#endif
#if IS_ENABLED(CONFIG_NPU_AFM)
		cancel_delayed_work_sync(&afm->afm_gnpu0_work);
		cancel_delayed_work_sync(&afm->afm_restore_gnpu0_work);
#endif
		cancel_delayed_work_sync(&device->npu_log_work);
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
		cancel_delayed_work_sync(&npu_precision->precision_work);
#endif

		npu_pm_update_dhcp_power_status(system, false);

		/* if do net working well with only this func.
		we need to change using proto_drv_open / proto_drv_close.
		*/
		auto_sleep_thread_terminate(&protodr->ast);

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
		npu_cmdq_table_read_close(&device->cmdq_table_info);
#endif

		system->interface_ops->interface_suspend(system);

		npu_pm_suspend_setting_sfr(system);
		ret = npu_cmd_map(system, "cpuoff");
		if (ret) {
			npu_err("fail(%d) in npu_cmd_map for cpu_off\n", ret);
			goto err_exit;
		}

		if (suspend_resume) {
			ret = npu_pm_check_power_and_off(system);
			if (ret) {
				npu_err("fail(%d) in try power on\n", ret);
				goto err_exit;
			}
		}

		system->enter_suspend = 0xCAFE;
	}

err_exit:
	npu_dbg("end(%d)\n", ret);
	return ret;
}

int npu_pm_resume(struct device *dev)
{
	int ret = 0;
	struct npu_system *system = &npu_pm_dev->system;
	struct npu_memory_buffer *fwmem;
	struct npu_device *device;

	device = container_of(system, struct npu_device, system);

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	struct npu_scheduler_info *info = device->sched;
#endif

	npu_dbg("start(0x%x)\n", system->enter_suspend);
	if (system->enter_suspend == 0xCAFE) {
		fwmem = npu_get_mem_area(system, "fwmem");

		print_ufw_signature(fwmem);

		if (suspend_resume) {
			ret = npu_pm_check_power_and_on(system);
			if (ret) {
				npu_err("fail(%d) in try power on\n", ret);
				goto err_exit;
			}
		}

		ret = npu_cmd_map(system, npu_check_fw_arch(system, fwmem));
		if (ret) {
			npu_err("fail(%d) in npu_cmd_map for cpu on\n", ret);
			goto err_exit;
		}
#if IS_ENABLED(CONFIG_NPU_USE_ESCA_DTM)
		ret = npu_dtm_open(device);
		if (ret)
			npu_err("npu_dtm_open is fail(%d)\n", ret);
#endif
		ret = system->interface_ops->interface_resume(system);
		if (ret) {
			npu_err("error(%d) in interface_resume\n", ret);
			goto err_exit;
		}

		ret = npu_pm_resume_setting_sfr(system);
		if (ret) {
			npu_err("error(%d) in npu_pm_resume_setting_sfr\n", ret);
			goto err_exit;
		}

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
		queue_delayed_work(info->sched_wq, &info->sched_work,
				msecs_to_jiffies(0));
#endif
		queue_delayed_work(device->npu_log_wq,
				&device->npu_log_work,
				msecs_to_jiffies(1000));

		/* if do net working well with only this func.
			we need to change using proto_drv_open / proto_drv_close.
		*/
		ret = auto_sleep_thread_start(&protodr->ast, protodr->ast_param);
		if (unlikely(ret)) {
			npu_err("fail(%d) in AST start\n", ret);
			goto err_exit;
		}

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
		ret = start_cmdq_table_read(&device->cmdq_table_info);
		if (ret) {
			npu_err("start_cmdq_table_read is fail(%d)\n", ret);
			goto err_exit;
		}
#endif

		npu_pm_update_dhcp_power_status(system, true);

		system->enter_suspend = 0;

		return ret;
	}
err_exit:
	npu_dbg("end(%d)\n", ret);
	return ret;
}

static ssize_t suspend_resume_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "suspend_resume_test : 0x%x\n", suspend_resume);

	return ret;
}

static ssize_t suspend_resume_test_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;

	if (sscanf(buf, "%u", &x) > 0) {
		if (x > 4) {
			npu_err("Invalid npu_pm_suspend_resume setting : 0x%x, please input 0 ~ 2\n", x);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	suspend_resume = x;

	if (suspend_resume == 1) {
		ret = npu_pm_suspend(npu_pm_dev->dev);
	} else if (suspend_resume == 2) {
		ret = npu_pm_resume(npu_pm_dev->dev);
	} else if (suspend_resume == 3) {
		ret = npu_pm_suspend(npu_pm_dev->dev);
		ret = npu_pm_resume(npu_pm_dev->dev);
	}

	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(suspend_resume_test);

int npu_pm_probe(struct npu_device *device)
{
	int ret = 0;

	suspend_resume = 0;

	npu_pm_dev = device;

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_suspend_resume_test.attr);
	if (ret) {
		probe_err("sysfs_create_file error : ret = %d\n", ret);
		goto err_exit;
	}

err_exit:
	return ret;
}
