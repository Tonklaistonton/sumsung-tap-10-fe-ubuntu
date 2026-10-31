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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/iommu.h>
#include <linux/pm_runtime.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include <soc/samsung/exynos/debug-snapshot.h>

#include "npu-profile-v2.h"
#include "include/npu-config.h"
#include "include/vs4l.h"
#include "npu-clock.h"
#include "npu-device.h"
#include "npu-debug.h"
#include "npu-protodrv.h"
#include "npu-util-memdump.h"
#include "npu-util-regs.h"
#include "npu-ver.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"
#include "npu-afm.h"
#include "npu-dtm.h"
#include "npu-dvfs.h"
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
#include "npu-precision.h"
#endif
#include "npu-ver.h"
#include "npu-pm.h"

extern struct class vision_class;
extern int npu_system_save_result(struct npu_session *session, struct nw_result nw_result);

struct npu_device *npu_local_device;

bool npu_device_check_and_lock(void)
{
	if (atomic_read(&(npu_local_device->gnet_ref[1])) == 0)
		return true;

	mutex_lock(&npu_local_device->gnet_glock);
	return false;
}

void npu_device_unlock(bool skip)
{
	if (!skip)
		mutex_unlock(&npu_local_device->gnet_glock);
}

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>

static struct s2mpu_notifier_block *s2mpu_nb;

static int npu_s2mpu_fault_handler(struct s2mpu_notifier_block *nb,
						struct s2mpu_notifier_info *ni)
{
	fw_will_note_to_kernel(0x10000);
	return S2MPU_NOTIFY_BAD;
}

#if IS_ENABLED(CONFIG_NPU_USE_S2MPU_NAME_IS_NPUS)
#define NPU_S2MPU_NAME	"NPUS"
#else
#define NPU_S2MPU_NAME	"DNC"
#endif

static void npu_s2mpu_set_fault_handler(struct device *dev)
{
	s2mpu_nb = NULL;

	s2mpu_nb = devm_kzalloc(dev, sizeof(struct s2mpu_notifier_block),
				GFP_KERNEL);
	if (!s2mpu_nb) {
		npu_err("s2mpu notifier block alloc failed\n");
		return;
	}

	s2mpu_nb->subsystem = NPU_S2MPU_NAME;
	s2mpu_nb->notifier_call = npu_s2mpu_fault_handler;

	exynos_s2mpu_notifier_call_register(s2mpu_nb);
}

static void npu_s2mpu_free_fault_handler(struct device *dev)
{
	if (s2mpu_nb)
		devm_kfree(dev, s2mpu_nb);
}
#else // !IS_ENABLED(CONFIG_EXYNOS_S2MPU)
static void npu_s2mpu_set_fault_handler(
			__attribute__((unused))struct device *dev)
{
}

static void npu_s2mpu_free_fault_handler(
			__attribute__((unused))struct device *dev)
{
}
#endif

static int npu_iommu_fault_handler(struct iommu_fault *fault, void *token)
{
	struct npu_device *device = token;

	if (test_bit(NPU_DEVICE_STATE_OPEN, &device->state)) {
		npu_err("called with state(%pK))\n", &device->state);
		npu_util_dump_handle_error_k(device);
	}

	return 0;
}

static int npu_iommu_set_fault_handler(struct device *dev,
					struct npu_device *device)
{	int ret = 0;
	ret = iommu_register_device_fault_handler(dev,
				npu_iommu_fault_handler, device);

	return ret;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
static void npu_itmon_noti_reg(__attribute__((unused))struct npu_device *device)
{
	itmon_notifier_chain_register(&device->itmon_nb);
	return;
}
#endif

int check_emergency(struct npu_device *dev)
{
	if (unlikely(npu_device_is_emergency_err((dev)))) {
		npu_warn("EMERGENCY ERROR STATE!\n");
		return NPU_CRITICAL_DRIVER(NPU_ERR_IN_EMERGENCY);
	}
	return 0;
}

static int __npu_device_start(struct npu_device *device)
{
	int ret = 0;

	if (test_bit(NPU_DEVICE_STATE_START, &device->state)) {
		npu_err("already started\n");
		ret = -EINVAL;
		goto ErrorExit;
	}

	ret = npu_system_start(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_system_start\n", ret);
		goto ErrorExit;
	}

	set_bit(NPU_DEVICE_STATE_START, &device->state);
ErrorExit:
	return ret;
}

static int __npu_device_stop(struct npu_device *device)
{
	int ret = 0;

	if (!test_bit(NPU_DEVICE_STATE_START, &device->state))
		goto ErrorExit;

	ret = npu_system_stop(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_stop\n", ret);

	clear_bit(NPU_DEVICE_STATE_START, &device->state);

ErrorExit:
	return ret;
}

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
static int npu_itmon_notifier(struct notifier_block *nb, unsigned long action, void *nb_data)
{
	struct npu_device *device;
	struct itmon_notifier *itmon_info = nb_data;
	int i = 0;

	device = container_of(nb, struct npu_device, itmon_nb);

	if (IS_ERR_OR_NULL(itmon_info))
		return NOTIFY_DONE;

	/*
	 * 1. When BUS Error occurs, IPs Traffic Monitor (ITMON) Error occurs.
	 * 2. Delivery of Master/Slave/Target information, etc. from ITMON Notifiercall to each driver.
	 * 3. The driver determines and prints the register dump, etc.
	 * 4. After that, ITMON counted three times and judged it as Panic or Not Panic to proceed with the Post Action.
	 */
	// Master or Slave
	if ((strstr(itmon_info->port,   "DNC" ))  ||
	    (strstr(itmon_info->dest,   "DNC" ))  ||
	    (strstr(itmon_info->master, "DNC" ))  ||
	    (strstr(itmon_info->port,   "SDMA"))  ||
	    (strstr(itmon_info->dest,   "SDMA"))  ||
	    (strstr(itmon_info->master, "SDMA"))) {
		npu_err("NPU/DSP ITMON(Master) : power domain(%s), power on/off(%s)\n",
			itmon_info->pd_name, itmon_info->onoff ? "on" : "off");

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
	#endif

		for (i = 0; i < 5; i++)
			npu_soc_status_report(&device->system);

		npu_ver_dump(device);
		fw_will_note(FW_LOGSIZE);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
		npu_log_fw_governor(device);
#endif
		npu_memory_dump(&device->system.memory);
		return NOTIFY_BAD; // Error
	}

	return NOTIFY_OK;
}
#endif

extern int npu_pwr_on(struct npu_system *system);
extern int npu_clk_init(struct npu_system *system);

static int npu_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!pdev)) {
		probe_err("Failed to get platform device\n");
		return -EINVAL;
	}

	dev = &pdev->dev;
	if (unlikely(!dev)) {
		probe_err("Failed to get dev\n");
		return -EINVAL;
	}

	device = vzalloc(sizeof(*device));
	if (unlikely(!device)) {
		probe_err("fail in vzalloc\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	device->dev = dev;

	mutex_init(&device->start_stop_lock);
	probe_info("NPU Device - init start_stop lock\n");

	mutex_init(&device->gnet_glock);
	npu_local_device = device;

	ret = npu_create_config(dev);
	if (ret) {
		probe_err("failed(%d) npu_create_config\n", ret);
		goto err_exit;
	}

	/* Probe sub modules */
	ret = npu_debug_probe(device);
	if (ret) {
		probe_err("fail(%d) in npu_debug_probe\n", ret);
		goto err_exit;
	}

	ret = npu_log_probe(device);
	if (ret) {
		probe_err("fail(%d) in npu_log_probe\n", ret);
		goto err_exit;
	}

	ret = npu_system_probe(&device->system, pdev);
	if (ret) {
		probe_err("fail(%d) in npu_system_probe\n", ret);
		ret = -EINVAL;
		goto err_exit;
	}

	ret = npu_vertex_probe(&device->vertex, dev);
	if (ret) {
		probe_err("fail(%d) in npu_vertex_probe\n", ret);
		goto err_exit;
	}
	ret = proto_drv_probe(device);
	if (ret) {
		probe_err("fail(%d) in proto_drv_probe\n", ret);
		goto err_exit;
	}

	ret = npu_sessionmgr_probe(&device->sessionmgr);

	ret = npu_iommu_set_fault_handler(dev, device);
	if (ret) {
		probe_err("fail(%d) in iommu_register_device_fault_handler\n", ret);
		goto err_exit;
	}

	npu_s2mpu_set_fault_handler(dev);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
	device->itmon_nb.notifier_call = npu_itmon_notifier;
	npu_itmon_noti_reg(device);
#endif

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	ret = dsp_kernel_manager_probe(device);
	if (ret) {
		probe_err("fail(%d) in dsp_kernel_manager_probe\n", ret);
		goto err_exit;
	}
#endif

	ret = dsp_dhcp_probe(device);
	if (ret) {
		probe_err("fail(%d) in dsp_dhcp_probe\n", ret);
		goto err_exit;
	}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	ret = npu_log_fw_governor_probe(device);
	if (ret) {
		probe_err("fail(%d) to initialze fw governor log\n", ret);
		goto err_exit;
	}
#endif

	ret = npu_afm_probe(device);
	if (ret) {
		probe_err("npu_afm_probe is fail(%d)\n", ret);
		goto err_exit;
	}

#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
	ret = npu_precision_probe(device);
	if (ret) {
		probe_err("npu_precision_probe is fail(%d)\n", ret);
		goto err_exit;
	}
#endif

	dev_set_drvdata(dev, device);

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	ret = npu_cmdq_table_read_probe(&device->cmdq_table_info);
	if (ret)
		goto err_exit;
#endif
	npu_pm_probe(device);
	if (ret) {
		probe_err("npu_pm_probe is fail(%d)\n", ret);
		goto err_exit;
	}

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	npu_dtm_emode_probe(device);
	if (ret) {
		probe_err("npu_dtm_emode_probe is fail(%d)\n", ret);
		goto err_exit;
	}
#endif

	ret = 0;
	probe_info("complete in %s\n", __func__);

	goto ok_exit;

err_exit:
	npu_s2mpu_free_fault_handler(dev);
	probe_err("error: ret(%d)\n", ret);
ok_exit:
	return ret;

}

int npu_device_open(struct npu_device *device)
{
	int ret = 0, ret2 = 0;

	npu_hwdev_register_hwdevs(device, &device->system);
	init_completion(&device->my_completion);

	ret = npu_debug_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_debug_open\n", ret);
		goto ErrorExit;
	}

	ret = npu_log_open(device);
	if (ret) {
		npu_err("fail(%d) in npu_log_open\n", ret);
		goto err_log_open;
	}

	ret = npu_system_open(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_system_open\n", ret);
		goto err_sys_open;
	}

	ret = npu_sessionmgr_open(&device->sessionmgr);
	if (ret) {
		npu_err("fail(%d) in npu_sessionmgr_open\n", ret);
		goto err_sys_open;
	}

	/* clear npu_devcice_err_state to no_error for emergency recover */
	clear_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);

	set_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	device->is_first = 0xC0DE;

	if (device->is_first == 0xC0DE) {
		ret = npu_session_open(&device->first_session, &device->sessionmgr, &device->system.memory, device->system.manual);
		if (ret) {
			npu_err("fail(%d) in npu first session create", ret);
			goto err_sys_open;
		}
	}

	atomic_set(&(device->num_open_gnet), 0);
	{
		u32 index;

		for (index = 0; index < NPU_MAX_GNET; index++) {
			atomic_set(&(device->gnet_ref[index]), 0);
			device->gnet[index] = 0;
		}
	}

	npu_info("%d\n", ret);
	return ret;
err_sys_open:
	ret2 = npu_log_close(device);
	if (ret2)
		npu_err("fail(%d) in npu_log_close)\n", ret2);
err_log_open:
	ret2 = npu_debug_close(device);
	if (ret2)
		npu_err("fail(%d) in npu_debug_close\n", ret2);
ErrorExit:
	npu_err("ret(%d) ret2(%d)\n", ret, ret2);
	return ret;
}

int npu_device_recovery_close(struct npu_device *device)
{
	int ret = 0;

	ret += npu_system_suspend(&device->system);
	ret += npu_hwdev_recovery_shutdown(device);
	ret += npu_log_close(device);
	ret += npu_debug_close(device);
	if (ret)
		goto err_coma;

	clear_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	npu_info("%d\n", ret);
	return ret;

err_coma:
	npu_ver_dump(device);
	dbg_snapshot_expire_watchdog();
	return ret;
}

int npu_device_bootup(struct npu_device *device)
{
	int ret = 0, ret2 = 0;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	npu_scheduler_boost_on(device->sched);

	if (unlikely(!device)) {
		npu_err("Failed to get npu_device\n");
		return -EINVAL;
	}

	ret = dsp_dhcp_init(device);
	if (ret) {
		npu_err("fail(%d) in dsp_dhcp_init\n", ret);
		goto err_system_resume;
	}

	ret = npu_system_resume(&device->system);
	if (ret) {
		npu_err("fail(%d) in npu_system_resume\n", ret);
		goto err_system_resume;
	}

	if (test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		ret = -ELIBACC;
		goto err_system_resume;
	}

	ret = proto_drv_open(device);
	if (ret) {
		npu_err("fail(%d) in proto_drv_open\n", ret);
		goto err_proto_open;
	}

	npu_scheduler_boost_off_timeout(info, NPU_SCHEDULER_BOOST_TIMEOUT);

	npu_info("%d\n", ret);
	return ret;

err_proto_open:
	ret2 = npu_interface_force_suspend(&device->system);
	if (ret2)
		npu_err("fail(%d) in npu_interface_force_suspend\n", ret2);

	ret2 = npu_system_suspend(&device->system);
	if (ret2)
		npu_err("fail(%d) in npu_system_suspend\n", ret2);

err_system_resume:
	ret2 = npu_system_close(&device->system);
	if (ret2)
		npu_err("fail(%d) in npu_system_close\n", ret2);

	npu_scheduler_boost_off_timeout(info, NPU_SCHEDULER_BOOST_TIMEOUT);
	npu_err("ret(%d) ret2(%d)\n", ret, ret2);
	return ret;
}

int npu_device_shutdown(struct npu_device *device)
{
	int ret = 0;

	if (!test_bit(NPU_DEVICE_STATE_OPEN, &device->state)) {
		npu_err("already closed of device\n");
		ret = -EINVAL;
		goto ErrorExit;
	}

	if (npu_device_is_emergency_err(device))
		npu_warn("NPU DEVICE IS EMERGENCY ERROR!\n");


	ret = proto_drv_close(device);
	if (ret)
		npu_err("fail(%d) in proto_drv_close\n", ret);

	ret = npu_system_suspend(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_suspend\n", ret);
ErrorExit:
	npu_info("%d\n", ret);
	return ret;
}

int npu_device_close(struct npu_device *device)
{
	int ret = 0;
	struct npu_system *system = &device->system;

	if (unlikely(!system)) {
		npu_err("Failed to get npu_system\n");
		return -EINVAL;
	}

	device->is_first = 0x0;

	if (!device->is_first) {
		ret = npu_session_close(device->first_session);
		if (ret)
			npu_err("fail(%d) in npu first session close", ret);
	}

	npu_profile_deinit(&system->memory);

	ret = npu_system_close(&device->system);
	if (ret)
		npu_err("fail(%d) in npu_system_close\n", ret);

	ret = npu_log_close(device);
	if (ret)
		npu_err("fail(%d) in npu_log_close)\n", ret);

	ret = npu_debug_close(device);
	if (ret)
		npu_err("fail(%d) in npu_debug_close\n", ret);

	clear_bit(NPU_DEVICE_STATE_OPEN, &device->state);

	if (test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state)) {
		npu_dbg("NPU_DEVICE_ERR_STATE_EMERGENCY bit was set before\n");
		clear_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
		npu_dbg("NPU_DEVICE_ERR_STATE_EMERGENCY bit is clear now");
		npu_dbg(" (%d) \n", test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state));
	}

	npu_info("%d\n", ret);
	return ret;
}

int npu_device_start(struct npu_device *device)
{
	int ret = 0;

	mutex_lock(&device->start_stop_lock);
	ret = __npu_device_start(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_start\n", ret);

	mutex_unlock(&device->start_stop_lock);
	npu_info("%d\n", ret);
	return ret;
}

int npu_device_stop(struct npu_device *device)
{
	int ret = 0;

	mutex_lock(&device->start_stop_lock);
	ret = __npu_device_stop(device);
	if (ret)
		npu_err("fail(%d) in __npu_device_stop\n", ret);

	mutex_unlock(&device->start_stop_lock);
	npu_info("%d\n", ret);

	return ret;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int npu_device_suspend(struct device *dev)
{
	int ret = 0;
	npu_dbg("called\n");
#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	ret = npu_pm_suspend(dev);
#endif
	return ret;
}

static int npu_device_resume(struct device *dev)
{
	int ret = 0;
	npu_dbg("called\n");
#if IS_ENABLED(CONFIG_NPU_PM_SLEEP_WAKEUP)
	ret = npu_pm_resume(dev);
#endif
	return ret;
}
#endif

static int npu_device_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;
	struct npu_device *device;

	if (unlikely(!pdev)) {
		probe_info("failed to get platform device\n");
		return -EINVAL;
	}

	dev = &pdev->dev;
	if (unlikely(!dev)) {
		npu_err("failed to get dev\n");
		return -EINVAL;
	}

	device = dev_get_drvdata(dev);
	if (unlikely(!device)) {
		npu_err("failed to get npu_device\n");
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	dsp_kernel_manager_remove(&device->kmgr);
#endif

	ret = npu_afm_release(device);
	if (ret)
		probe_err("fail(%d) in npu_afm_release\n", ret);

#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
	ret = npu_precision_release(device);
	if (ret)
		probe_err("fail(%d) in npu_precision_release\n", ret);
#endif

#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
	ret = npu_sessionmgr_release(&device->sessionmgr);
	if (ret)
		probe_err("fail(%d) in npu_sessionmgr_release\n", ret);
#endif

	ret = proto_drv_release();
	if (ret)
		probe_err("fail(%d) in proto_drv_release\n", ret);

	ret = npu_debug_release();
	if (ret)
		probe_err("fail(%d) in npu_debug_release\n", ret);

	ret = npu_log_release(device);
	if (ret)
		probe_err("fail(%d) in npu_log_release\n", ret);

	ret = npu_system_release(&device->system, pdev);
	if (ret)
		probe_err("fail(%d) in npu_system_release\n", ret);

	vfree(device);

	probe_info("completed in %s\n", __func__);
	return 0;
}

void npu_device_set_emergency_err(struct npu_device *device)
{
	npu_warn("Emergency flag set.\n");
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
	npu_soc_status_report(&device->system);
}

int npu_device_is_emergency_err(struct npu_device *device)
{

	return test_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
}

static const struct dev_pm_ops npu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(npu_device_suspend, npu_device_resume)
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id exynos_npu_match[] = {
	{
		.compatible = "samsung,exynos-npu"
	},
	{}
};
MODULE_DEVICE_TABLE(of, exynos_npu_match);
#endif

static struct platform_driver npu_driver = {
	.probe	= npu_device_probe,
	.remove = npu_device_remove,
	.driver = {
		.name	= "exynos-npu",
		.owner	= THIS_MODULE,
		.pm	= &npu_pm_ops,
		.of_match_table = of_match_ptr(exynos_npu_match),
	},
};

static int __init npu_device_init(void)
{
	int ret = 0;
	dev_t dev = MKDEV(VISION_MAJOR, 0);

	probe_info("Linux vision interface: v1.00\n");
	ret = register_chrdev_region(dev, VISION_NUM_DEVICES, VISION_NAME);
	if (ret < 0) {
		probe_err("videodev: unable to get major %d (%d)\n", VISION_MAJOR, ret);
		return ret;
	}

	ret = class_register(&vision_class);
	if (ret < 0) {
		unregister_chrdev_region(dev, VISION_NUM_DEVICES);
		probe_err("video_dev: class_register failed(%d)\n", ret);
		return -EIO;
	}

	ret = npu_hwdev_register();
	if (ret) {
		probe_err("error(%d) in npu_hwdev_register\n", ret);
		goto err_exit;
	}

	ret = npu_memory_register();
	if (ret) {
		probe_err("error(%d) in npu_memory_register\n", ret);
		goto err_exit;
	}

	ret = platform_driver_register(&npu_driver);
	if (ret) {
		probe_err("error(%d) in platform_driver_register\n", ret);
		goto err_exit;
	}

	probe_info("success in %s\n", __func__);
	ret = 0;
	goto ok_exit;

err_exit:
	// necessary clean-up

ok_exit:
	return ret;
}

static void __exit npu_device_exit(void)
{
	dev_t dev = MKDEV(VISION_MAJOR, 0);

	platform_driver_unregister(&npu_driver);

	npu_hwdev_unregister();
	npu_memory_unregister();
	class_unregister(&vision_class);
	unregister_chrdev_region(dev, VISION_NUM_DEVICES);

	probe_info("success in %s\n", __func__);
}

module_init(npu_device_init);
module_exit(npu_device_exit);

MODULE_AUTHOR("Eungjoo Kim<ej7.kim@samsung.com>");
MODULE_VERSION("14.2");
MODULE_DESCRIPTION("Exynos NPU driver");
MODULE_LICENSE("GPL");
