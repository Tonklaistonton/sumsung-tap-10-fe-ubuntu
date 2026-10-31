// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts.c
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2021 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <sound/samsung/vts.h>
#include <linux/firmware.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <linux/delay.h>

#include "vts_soc.h"
#include "vts_soc_v5.h"
#include "vts_dbg.h"
#include "vts_util.h"
#include "vts_res.h"

#define LIMIT_IN_JIFFIES (msecs_to_jiffies(1000))

void vts_cpu_power(struct device *dev, bool on)
{
	uint32_t vts_pmu_addr = 0;
	struct vts_data *data = dev_get_drvdata(dev);

	exynos_pmu_read(VTS_IN_PMU_ADDR, &vts_pmu_addr);
	vts_dev_info(dev, "%s: vts_pmu_addr(0x%x) on(%d)\n", __func__, vts_pmu_addr, on);

	/* exynos_pmu_update(VTS_CPU_CONFIGURATION, VTS_CPU_LOCAL_PWR_CFG,
		on ? VTS_CPU_LOCAL_PWR_CFG : 0); */
	if (on) {
		/* Release reset sequence :
		   1) CPU_OUT__GRESETn_CPU 0x1 (Up to 4 CPUs can be supported. If all 4 are released, set to 0xF)
		   2) CPU_OPTION__MASK 0x1 (Up to 4 CPUs can be supported. If all 4 are released, set to 0xF)
		   3) CPU_OUT__CNT_EN_WDT (Up to 4 WDTs can be supported. If all 4 are released, set to 0xF)
		*/

		/* : CPU_OUT__GRESETn_CPU 0x1 : core 1ea */
		writel_phys(0x1, vts_pmu_addr + VTS_PMU_SUB_V4_CPU_OUT);
		/* CPU_OPTION__MASK 0x1 : core 1ea */
		writel_phys(0x1, vts_pmu_addr + VTS_PMU_SUB_V4_CPU_OPTION);
		/* CPU_OUT__CNT_EN_WDT 0x10 : core 1ea */
		writel_phys(0x10 | readl_phys(vts_pmu_addr + VTS_PMU_SUB_V4_CPU_OUT), vts_pmu_addr + VTS_PMU_SUB_V4_CPU_OUT);
	} else {
		/* To reset sequence :
		   1) CPU idle check
		   2) CPU_OUT__CNT_EN_WDT 0x0
		   3) CPU_OUT__GRESETn_CPU 0x0
		*/

		/* CPU_OUT__CNT_EN_WDT & CPU_OUT__GRESETn_CPU 0x0 */
		writel_phys(0x0, vts_pmu_addr + VTS_PMU_SUB_V4_CPU_OUT);
	}

	usleep_range(1000, 2000);

	if(on) {
		/* This is only for Solomon:
		   1) During power down, the CPU and Q-channel handshaking will gets stuck.
		   2) Because the CPU does not send QACCEPTn for QREQn.
		   3) If we set FEEDBACK_SELQCH to 0x3, CPU will be operated as Handshake.
		*/
		writel(0x3, data->sfr_base + FEEDBACK_SEL);
	}
}

int vts_cpu_power_chk(struct device *dev)
{
	uint32_t status = 0;
	uint32_t vts_pmu_addr = 0;

	/* exynos_pmu_read(VTS_CPU_STATUS, &status); */
	exynos_pmu_read(VTS_IN_PMU_ADDR, &vts_pmu_addr);
	status = readl_phys(vts_pmu_addr+VTS_PMU_SUB_V4_STATUS);

	return status;
}

int vts_cpu_enable(struct device *dev, bool enable)
{
/* WFI status register is removed located in VTS_CPU_IN */
	struct vts_data *data = dev_get_drvdata(dev);
	unsigned int status = 0;
	unsigned long after;

	if (data->vts_status == ABNORMAL && data->silent_reset_support) {
		vts_dev_err(dev, "%s: abnormal status, skip checking status\n", __func__);
		return 0;
	}

	after = jiffies + LIMIT_IN_JIFFIES;

	do {
		msleep(1);
		schedule();
		status = readl(data->sfr_base + YAMIN_SLEEP);
		vts_dev_info(dev, "%s: status:0x%x\n", __func__, status);

	} while (((status & VTS_CPU_IN_SLEEPDEEP_MASK)
		!= VTS_CPU_IN_SLEEPDEEP_MASK)
		&& time_is_after_eq_jiffies(after));

	if (time_is_before_jiffies(after)) {
		vts_err("vts cpu enable timeout\n");
		return -ETIME;
	}

	return 0;
}

void vts_reset_cpu(struct device *dev)
{
	vts_cpu_enable(dev, false);
	vts_cpu_power(dev, false);
	vts_cpu_power(dev, true);
}

void vts_disable_fw_ctrl(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

	vts_dev_info(dev, "%s: enter\n", __func__);

	/* Controls by firmware should be disabled before pd_vts off. */
	writel(0x0, data->sfr_base + ENABLE_DMIC_IF);
	writel(0x0, data->sfr_slif_vts + SLIF_CONFIG_DONE);
	writel(0x0, data->sfr_slif_vts + SLIF_CTRL);
	writel(0xffffffff, data->sfr_slif_vts + SLIF_INT_EN_CLR);
	writel(0x0, data->sfr_slif_vts + SLIF_INT_PEND);
	writel(0x0, data->timer0_base);

	/* set to default value */
	writel(0x0, data->dmic_if0_base + VTS_DMIC_CONTROL_DMIC_IF);
	writel(0x0, data->dmic_if1_base + VTS_DMIC_CONTROL_DMIC_IF);

	writel(0x0003C000, data->dmic_if0_base);
	writel(0x0003C000, data->dmic_if1_base);
	mdelay(1);
	writel(0x2003C000, data->dmic_if0_base);
	writel(0x2003C000, data->dmic_if1_base);

	return;
}

void vts_pad_retention(bool retention)
{
	if (!retention) {
		exynos_pmu_update(PAD_RETENTION_VTS_OPTION,
			0x1 << 11, 0x1 << 11);
	}
}
EXPORT_SYMBOL(vts_pad_retention);

u32 vts_set_baaw(void __iomem *sfr_base, u64 base, u32 size) {
	u32 aligned_size = round_up(size, SZ_4M);
	u64 aligned_base = round_down(base, aligned_size);

	/* EVT1 */
	/* set VTS BAAW config */
#if (0)
	/* VTS SRAM */
	writel(0x00000101, sfr_base + 0x1804);
	writel(0x00000000, sfr_base + 0x1808);
	writel(0x00000203, sfr_base + 0x180C);
	writel(0x00014400, sfr_base + 0x1810);

	/* CHUB SRAM */
	writel(0x00000101, sfr_base + 0x1884);
	writel(0x00000300, sfr_base + 0x1888);
	writel(0x0000047F, sfr_base + 0x188C);
	writel(0X00014C00, sfr_base + 0x1890);

	/* ALIVE MAILBOX, CHUB_RTC */
	writel(0x00000101, sfr_base + 0x1904);
	writel(0x00040000, sfr_base + 0x1908);
	writel(0x0004064F, sfr_base + 0x190C);
	writel(0x00013800, sfr_base + 0x1910);

	/* CMGP SFR */
	writel(0x00000101, sfr_base + 0x1984);
	writel(0x00043400, sfr_base + 0x1988);
	writel(0x000435FF, sfr_base + 0x198C);
	writel(0x00015000, sfr_base + 0x1990);

	/* VTS SFR */
	writel(0x00000101, sfr_base + 0x1A04);
	writel(0x00041000, sfr_base + 0x1A08);
	writel(0x000411EF, sfr_base + 0x1A0C);
	writel(0x00014000, sfr_base + 0x1A10);

	/* CHUBVTS SFR */
	writel(0x00000101, sfr_base + 0x1A84);
	writel(0x00042ED0, sfr_base + 0x1A88);
	writel(0x00042EFF, sfr_base + 0x1A8C);
	writel(0x00014ED0, sfr_base + 0x1A90);

	/* PDMA_VTS */
	writel(0x00000101, sfr_base + 0x1B04);
	writel(0x00042FF0, sfr_base + 0x1B08);
	writel(0x00042FFF, sfr_base + 0x1B0C);
	writel(0x00014FF0, sfr_base + 0x1B10);
#endif

	/* DRAM */
	writel(0x00000101, sfr_base + 0x1B84);
	pr_info("[vts] %s: %d 0x%llx\n", __func__, __LINE__, aligned_base / VTS_BAAW_DRAM_DIV);
	/* GUIDE: writel(0x060000, sfr_base + 0x60); */
	writel(VTS_BAAW_BASE / VTS_BAAW_DRAM_DIV, sfr_base + 0x1B88);
	/* GUIDE: writel(0x100000, sfr_base + 0x64); */
	writel((VTS_BAAW_BASE + aligned_size) / VTS_BAAW_DRAM_DIV, sfr_base + 0x1B8C);
	/* GUIDE: writel(0x060000, sfr_base + 0x68); */
	writel(aligned_base / VTS_BAAW_DRAM_DIV, sfr_base + 0x1B90);

	return base - aligned_base + VTS_BAAW_BASE;
}

static void vts_soc_complete_fw_request(
	const struct firmware *fw, void *context)
{
	struct device *dev = context;
	struct vts_data *data = dev_get_drvdata(dev);
	char *ver;

	if (!fw) {
		vts_dev_err(dev, "Failed to request firmware\n");
		return;
	}

	data->firmware = fw;

	vts_dev_info(dev, "firmware loaded at %p (%zu)\n", fw->data, fw->size);

	if (VTSFW_VERSION_OFFSET > 0 && VTSFW_VERSION_OFFSET < fw->size) {
		data->vtsfw_version = *(unsigned int *) (fw->data + VTSFW_VERSION_OFFSET);
		ver = (char *)(&data->vtsfw_version);
		vts_dev_info(dev, "firmware version read from binary file: (%c%c%c%c)\n", ver[3], ver[2], ver[1], ver[0]);
	}
}

int vts_soc_runtime_resume(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	const char *fw_name;
	int ret = 0;

	vts_dev_info(dev, "%s\n", __func__);

	if (IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)) {
		vts_dev_info(dev, "%s: EVT0\n", __func__);
		fw_name = "vts_evt0.bin";
	} else {
		fw_name = "vts.bin";
	}

	ret = vts_clk_set_rate(dev, data->alive_clk_path);

	/* SRAM intmem */
	if (data->intmem_code)
		writel(0x03FF0000, data->intmem_code + 0x4);

	if (data->intmem_data)
		writel(0x03FF0000, data->intmem_data + 0x4);

	if (data->intmem_pcm)
		writel(0x03FF0000, data->intmem_pcm + 0x4);

	if (data->intmem_data1)
		writel(0x03FF0000, data->intmem_data1 + 0x4);

	if (!data->firmware) {
		vts_dev_info(dev, "%s: request_firmware_direct: %s\n", __func__, fw_name);
		ret = request_firmware_direct((const struct firmware **)&data->firmware,
			fw_name, dev);

		if (ret < 0) {
			vts_dev_err(dev, "request_firmware_direct failed\n");
			return ret;
		}
		vts_dev_info(dev, "vts_soc_complete_fw_request : OK\n");
		vts_soc_complete_fw_request(data->firmware, dev);
	}

	return ret;
}

#define YAMIN_MCU_VTS_QCH_CLKIN (0x70e8)
int vts_soc_runtime_suspend(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);

#if 0
	volatile unsigned long yamin_mcu_vts_qch_clkin;
	unsigned int status = 0;

	yamin_mcu_vts_qch_clkin =
		(volatile unsigned long)ioremap_wt(0x15507000, 0x100);

	pr_info("[VTS]YAMIN QCH(0xe8) 0x%08x\n",
			readl((volatile void *)(yamin_mcu_vts_qch_clkin
					+ 0xe8)));
	iounmap((volatile void __iomem *)yamin_mcu_vts_qch_clkin);

	exynos_pmu_read(YAMIN_MCU_VTS_QCH_CLKIN, &status);
	pr_info("[VTS]YAMIN QCH(0xe8) 0x%08x\n", status);
#endif

	vts_dev_info(dev, "%s\n", __func__);

	vts_disable_fw_ctrl(dev);

	vts_clk_disable(dev, data->sys_clk_path);
	vts_clk_disable(dev, data->tri_clk_path);
	vts_clk_disable(dev, data->alive_clk_path);

	return 0;
}

int vts_soc_cmpnt_probe(struct device *dev)
{
	struct vts_data *data = dev_get_drvdata(dev);
	const char* fw_name;
	int ret;

	vts_dev_info(dev, "%s\n", __func__);

	if (IS_ENABLED(CONFIG_SOC_S5E9925_EVT0)) {
		vts_dev_info(dev, "%s: EVT0 \n", __func__);
		fw_name = "vts_evt0.bin";
	} else {
		fw_name = "vts.bin";
	}

	if (!data->firmware) {
		vts_dev_info(dev, "%s : request_firmware_direct: %s\n",
				fw_name,
				__func__);
		ret = request_firmware_direct(
			(const struct firmware **)&data->firmware,
			fw_name, dev);

		if (ret < 0) {
			vts_dev_err(dev, "Failed to request_firmware_nowait\n");
		} else {
			vts_dev_info(dev, "vts_soc_complete_fw_request : OK\n");
			vts_soc_complete_fw_request(data->firmware, dev);
		}
	}

	vts_dev_info(dev, "%s(%d)\n", __func__, __LINE__);

	return 0;
}

int vts_soc_probe(struct device *dev)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	return ret;
}

void vts_soc_remove(struct device *dev)
{
	pr_info("%s\n", __func__);
}

#if defined(CONFIG_VTS_SFR_DUMP)
ssize_t vts_soc_sfr_dump_show(struct device *dev,
               struct device_attribute *attr, char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);
	char *pbuf = buf;
	int i;

	pbuf += sprintf(pbuf, "========================================\n");

	pbuf += sprintf(pbuf, "VTS SFR registers dump:\n");
	for(i=0; i<sizeof(vts_sfr_offset)/sizeof(unsigned int); i++) {
		pbuf += sprintf(pbuf, "offset = 0X%8x : 0X%08x\n", vts_sfr_offset[i], readl(data->sfr_base + vts_sfr_offset[i]));
	}

	pbuf += sprintf(pbuf, "========================================\n");

	return pbuf - buf;
}

ssize_t vts_soc_dmic_if_dump_show(struct device *dev,
               struct device_attribute *attr, char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);
	char *pbuf = buf;
	int i;

	pbuf += sprintf(pbuf, "========================================\n");

	pbuf += sprintf(pbuf, "VTS Dmic_if0 registers dump:\n");
	for(i=0; i<sizeof(vts_dmic_if0_offset)/sizeof(unsigned int); i++) {
		pbuf += sprintf(pbuf, "offset = 0X%8x : 0X%08x\n", vts_dmic_if0_offset[i], readl(data->dmic_if0_base + vts_dmic_if0_offset[i]));
	}

	pbuf += sprintf(pbuf, "VTS Dmic_if1 registers dump:\n");
	for(i=0; i<sizeof(vts_dmic_if1_offset)/sizeof(unsigned int); i++) {
		pbuf += sprintf(pbuf, "offset = 0X%8x : 0X%08x\n", vts_dmic_if1_offset[i], readl(data->dmic_if1_base + vts_dmic_if1_offset[i]));
	}

	pbuf += sprintf(pbuf, "========================================\n");

	return pbuf - buf;
}

ssize_t vts_soc_slif_dump_show(struct device *dev,
               struct device_attribute *attr, char *buf)
{
	struct vts_data *data = dev_get_drvdata(dev);
	char *pbuf = buf;
	int i;

	pbuf += sprintf(pbuf, "========================================\n");

	pbuf += sprintf(pbuf, "VTS Slif registers dump:\n");
	for(i=0; i<sizeof(vts_slif_offset)/sizeof(unsigned int); i++) {
		pbuf += sprintf(pbuf, "offset = 0X%8x : 0X%08x\n", vts_slif_offset[i], readl(data->sfr_slif_vts + vts_slif_offset[i]));
	}

	pbuf += sprintf(pbuf, "========================================\n");

	return pbuf - buf;
}

ssize_t vts_soc_gpio_dump_show(struct device *dev,
               struct device_attribute *attr, char *buf)
{
	//struct vts_data *data = dev_get_drvdata(dev);
	void __iomem *virt = ioremap(VTS_GPIO_BASE, 0x1024);
	char *pbuf = buf;
	int i;

	pbuf += sprintf(pbuf, "========================================\n");

	pbuf += sprintf(pbuf, "VTS GPIO registers dump:\n");
	for(i=0; i<sizeof(vts_gpio_offset)/sizeof(unsigned int); i++) {
		pbuf += sprintf(pbuf, "offset = 0X%8x : 0X%08x\n", vts_gpio_offset[i], readl(virt + vts_gpio_offset[i]));
	}
	iounmap(virt);

	pbuf += sprintf(pbuf, "========================================\n");

	return pbuf - buf;
}
#endif
