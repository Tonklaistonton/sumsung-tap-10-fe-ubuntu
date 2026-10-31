#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/of_platform.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "dsp-dhcp.h"
#include "npu-log.h"
#include "npu-hw-device.h"
#include "npu-ver.h"
#include "npu-device.h"
#include "npu-system.h"

#undef DHCP_TIGHT_SIZE_CHECK

static void dsp_dhcp_init_mem_info(struct npu_system *system)
{
	struct npu_memory_buffer *pmem;
	struct dhcp_table *dhcp_table = system->dhcp_table;

	pmem = npu_get_mem_area(system, "ivp_pm");
	if (pmem) {
		dhcp_table->IVP_INFO.PM_ADDR = pmem->dma.daddr;
		dhcp_table->IVP_INFO.PM_SIZE = pmem->dma.size;
	}

	pmem = npu_get_mem_area(system, "ivp_dm");
	if (pmem) {
		dhcp_table->IVP_INFO.DM_ADDR = pmem->dma.daddr;
		dhcp_table->IVP_INFO.DM_SIZE = pmem->dma.size;
	}

	pmem = npu_get_mem_area(system, "dl_out");
	if (pmem)
		dhcp_table->DL_TABLE_ADDR = pmem->dma.daddr;
}

int dsp_dhcp_update_pwr_status(struct npu_device *dev, u32 type, bool on)
{
	int ret;
	struct dhcp_table *dhcp_table = dev->system.dhcp_table;
	union dsp_dhcp_pwr_ctl pwr_ctl_b = {
		.value = dhcp_table->DNC_PWR_CTRL,
	};
	union dsp_dhcp_pwr_ctl pwr_ctl_a = pwr_ctl_b;

	switch (type) {
	case NPU_HWDEV_ID_DSP:
		pwr_ctl_a.dsp_pm = on;
		break;
	case NPU_HWDEV_ID_NPU:
		pwr_ctl_a.npu_pm = on;
		break;
	default:
		npu_err("not supported type(%d)\n", type);
		return -EINVAL;
	}

	if (pwr_ctl_a.value != pwr_ctl_b.value) {
		ret = 0;
		dhcp_table->DNC_PWR_CTRL = pwr_ctl_a.value;
	} else
		ret = 1;	/* same as before */

	npu_info("(prev:%#x -> current:%#x) / type(%d)\n", pwr_ctl_b.value, pwr_ctl_a.value, type);

	return ret;
}

int dsp_dhcp_update_imb(struct npu_system *system, dma_addr_t daddr,
	size_t size)
{
	struct dhcp_table *dhcp_table = system->dhcp_table;
	unsigned int *IMB_INIT_INFO = dhcp_table->IMB_INIT_INFO;

	if (daddr >= IOVA_EXPANDED_END)
		return -EFAULT;


	IMB_INIT_INFO[1] = daddr >> 32;
	IMB_INIT_INFO[0] = daddr & 0xFFFFFFFF;

	IMB_INIT_INFO[3] = size >> 32;
	IMB_INIT_INFO[2] = size & 0xFFFFFFFF;

	return 0;
}

int dsp_dhcp_init(struct npu_device *dev)
{
	int i = 0;
	struct dhcp_table *dhcp_table = dev->system.dhcp_table;

	npu_info("%s\n", __func__);

	dhcp_table->DRIVER_SYNC_VERSION = NPU_DD_SYNC_VERSION;
	dhcp_table->FIRMWARE_SYNC_VERSION = 0;
	dhcp_table->PROF_MODE = 0;

	for (i = 0; i < NPU_MAX_PROFILE_SESSION; i++) {
		dhcp_table->PROF_MAIN[i].PROFILE_MAX_ITER = 0;
		dhcp_table->PROF_MAIN[i].PROFILE_SIZE = 0;
		dhcp_table->PROF_MAIN[i].PROFILE_IOVA = 0;

		dhcp_table->PROF_INFO[i].PROFILE_MAX_ITER = 0;
		dhcp_table->PROF_INFO[i].PROFILE_SIZE = 0;
		dhcp_table->PROF_INFO[i].PROFILE_IOVA = 0;
	}

	if (dev->system.s2d_mode == NPU_S2D_MODE_ON) {
		dhcp_table->RECOVERY_DISABLE =  1;
	} else {
		dhcp_table->RECOVERY_DISABLE =  0;
	}

	dhcp_table->SECURE_BOOT = 0xdeadbeef;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	dhcp_table->CIDLE = dev->system.cidle;
	dhcp_table->ENABLE_APG = 0;
	dhcp_table->ENTER_WFI = NORMAL_SYSMMU;
#endif
	dhcp_table->FORCE_SUSPEND_READY = 0;
	dhcp_table->FORCE_SUSPEND_REQUEST = 0;

	/* update mem info */
	dsp_dhcp_init_mem_info(&dev->system);
	return 0;
}

int dsp_dhcp_probe(struct npu_device *dev)
{
	struct npu_system *system = &dev->system;
	struct npu_memory_buffer *buffer;
	struct dhcp_table *dhcp_table;

	buffer = npu_get_mem_area(system, "dhcp");
	if (!buffer) {
		npu_err("fail to prepare dhcp_mem\n");
		goto err_get_dhcp_mem;
	}

	dhcp_table = buffer->dma.vaddr;

	system->dhcp_table = dhcp_table;
	return 0;

err_get_dhcp_mem:
	return -1;
}
