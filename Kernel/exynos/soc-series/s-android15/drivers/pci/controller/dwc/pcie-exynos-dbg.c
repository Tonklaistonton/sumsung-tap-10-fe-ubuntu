/*
 * PCIe host controller driver for Samsung EXYNOS SoCs
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/random.h>
#include <linux/gpio.h>
#include <linux/pm_runtime.h>
#include <linux/exynos-pci-noti.h>
#include <linux/exynos-pci-ctrl.h>
#include "pcie-designware.h"
#include "pcie-exynos-rc.h"

#include "pcie-exynos-phycal_common.h"

int exynos_pcie_rc_set_outbound_atu(int ch_num, u32 target_addr, u32 offset, u32 size);
void exynos_pcie_rc_register_dump(int ch_num);
void exynos_pcie_set_perst_gpio(int ch_num, bool on);
void remove_pcie_sys_file(struct device *dev);
int exynos_pcie_dbg_link_test(struct device *dev, struct exynos_pcie *exynos_pcie, int enable);

int exynos_pcie_dbg_memlog_dump_print(struct exynos_pcie *exynos_pcie,
		int level, const char *fmt, ...)
{
	int ret;
	va_list args;

	if (!exynos_pcie->dump_obj) {
		pcie_err("%s: exynos_pcie log object is null! \n", __func__);
		return -EINVAL;
	}

	va_start(args, fmt);
	ret = memlog_write_vsprintf(exynos_pcie->dump_obj, level, fmt, args);
	va_end(args);

	return ret;
}

int exynos_pcie_dbg_memlog_print(struct exynos_pcie *exynos_pcie,
		int level, const char *fmt, ...)
{
	int ret;
	va_list args;

	if (!exynos_pcie->log_obj) {
		pcie_err("%s: exynos_pcie log object is null! \n", __func__);
		return -EINVAL;
	}

	va_start(args, fmt);
	ret = memlog_write_vsprintf(exynos_pcie->log_obj, level, fmt, args);
	va_end(args);

	return ret;
}

void exynos_pcie_dbg_memlog_init(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	int ret;

	ret = memlog_register("PCIe", pci->dev, &exynos_pcie->log_desc);
	if (ret) {
		pcie_err("%s: Failed to register pcie memlog\n", __func__);
		return;
	}

	exynos_pcie->log_obj = memlog_alloc_printf(exynos_pcie->log_desc, SZ_128K,
				NULL, "Log", 0);
	if (!exynos_pcie->log_obj)
		pcie_err("%s: Failed to allocate memory for driver\n", __func__);

	ret = memlog_register("PCIe_dump", pci->dev, &exynos_pcie->dump_desc);
	if (ret) {
		pcie_err("%s: Failed to register pcie dump memlog\n", __func__);
		return;
	}

	exynos_pcie->dump_obj = memlog_alloc_printf(exynos_pcie->log_desc, SZ_128K,
				NULL, "Reg", 0);
	if (!exynos_pcie->log_obj)
		pcie_err("%s: Failed to allocate memory for driver\n", __func__);
}

void exynos_pcie_dbg_print_oatu_register(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	u32 val;

	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_CR1_OUTBOUND2, 4, &val);
	pcie_info("%s:  PCIE_ATU_CR1_OUTBOUND2(0x400) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LOWER_BASE_OUTBOUND2, 4, &val);
	pcie_info("%s:  PCIE_ATU_LOWER_BASE_OUTBOUND2(0x408) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_UPPER_BASE_OUTBOUND2, 4, &val);
	pcie_info("%s:  PCIE_ATU_UPPER_BASE_OUTBOUND2(0x40C) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LIMIT_OUTBOUND2, 4, &val);
	pcie_info("%s:  PCIE_ATU_LIMIT_OUTBOUND2(0x410) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_LOWER_TARGET_OUTBOUND2, 4, &val);
	pcie_info("%s:  PCIE_ATU_LOWER_TARGET_OUTBOUND2(0x414) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_UPPER_TARGET_OUTBOUND2, 4, &val);
	pcie_info("%s:  PCIE_ATU_UPPER_TARGET_OUTBOUND2(0x418) = 0x%x\n", __func__, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_ATU_CR2_OUTBOUND2, 4, &val);
	pcie_info("%s:  PCIE_ATU_CR2_OUTBOUND2(0x404) = 0x%x\n", __func__, val);
}

void exynos_pcie_dbg_print_msi_register(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	u32 val;

	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_MASK, 4, &val);
	pcie_info("PCIE_MSI_INTR0_MASK(0x%x):0x%x\n", PCIE_MSI_INTR0_MASK, val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE, 4, &val);
	pcie_info("PCIE_MSI_INTR0_ENABLE(0x%x):0x%x\n", PCIE_MSI_INTR0_ENABLE, val);
	val = exynos_elbi_read(exynos_pcie, PCIE_IRQ2_EN);
	pcie_info("PCIE_IRQ2_EN: 0x%x\n", val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_ADDR_LO, 4, &val);
	pcie_info("PCIE_MSI_ADDR_LO: 0x%x\n", val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_ADDR_HI, 4, &val);
	pcie_info("PCIE_MSI_ADDR_HI: 0x%x\n", val);
	exynos_pcie_rc_rd_own_conf(pp, PCIE_MSI_INTR0_STATUS, 4, &val);
	pcie_info("PCIE_MSI_INTR0_STATUS: 0x%x\n", val);
}

void exynos_pcie_dbg_dump_link_down_status(struct exynos_pcie *exynos_pcie)
{
	pcie_info("LTSSM: 0x%08x\n",
			exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP));
}

void exynos_pcie_dbg_print_link_history(struct exynos_pcie *exynos_pcie)
{
	u32 history_buffer[32];
	int i;

	for (i = 31; i >= 0; i--)
		history_buffer[i] = exynos_soc_read(exynos_pcie,
						     PCIE_CTRL_DEBUG_REG(i));
	for (i = 31; i >= 0; i--)
		pcie_info("(%#x) LTSSM: %#04x, L1sub: %#x, [15:14]: %#x, [13:4]: %#x, LOCK: %#x\n",
			  history_buffer[i],
			  CTRL_DEBUG_LTSSM_STATE(history_buffer[i]),
			  CTRL_DEBUG_L1SUB_STATE(history_buffer[i]),
			  (history_buffer[i]>> 14 & 0x3),
			  (history_buffer[i]>> 4 & 0x3FF),
			  CTRL_DEBUG_PHY_DYN_STATE(history_buffer[i]));
}

void exynos_pcie_dbg_register_dump(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	struct exynos_pcie_dbg_range *elbi = &exynos_pcie->dbg_range_elbi;
	struct exynos_pcie_dbg_range *phy = &exynos_pcie->dbg_range_phy;
	struct exynos_pcie_dbg_range *pcs = &exynos_pcie->dbg_range_pcs;
	struct exynos_pcie_dbg_range *udbg = &exynos_pcie->dbg_range_udbg;
	struct exynos_pcie_dbg_range *dbi = &exynos_pcie->dbg_range_dbi;
	u32 i, val_0, val_4, val_8, val_c;

	pcie_dump("%s: +++\n", __func__);

	pcie_dump("[Print SUB_CTRL region]\n");
	pcie_dump("offset:             0x0               0x4               0x8               0xC\n");
	for (i = elbi->start; i < elbi->end; i += 0x10) {
		pcie_dump("ELBI 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_elbi_read(exynos_pcie, i + 0x0),
				exynos_elbi_read(exynos_pcie, i + 0x4),
				exynos_elbi_read(exynos_pcie, i + 0x8),
				exynos_elbi_read(exynos_pcie, i + 0xC));
	}
	pcie_err("\n");

	pcie_dump("[Print PHY region]\n");
	pcie_dump("offset:             0x0               0x4               0x8               0xC\n");
	for (i = phy->start; i < phy->end; i += 0x10) {
		pcie_dump("PHY 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_read(exynos_pcie, i + 0x0),
				exynos_phy_read(exynos_pcie, i + 0x4),
				exynos_phy_read(exynos_pcie, i + 0x8),
				exynos_phy_read(exynos_pcie, i + 0xC));
	}

	pcie_dump("[Print PHY_PCS region]\n");
	pcie_dump("offset:             0x0               0x4               0x8               0xC\n");
	for (i = pcs->start; i < pcs->end; i += 0x10) {
		pcie_dump("PCS 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phy_pcs_read(exynos_pcie, i + 0x0),
				exynos_phy_pcs_read(exynos_pcie, i + 0x4),
				exynos_phy_pcs_read(exynos_pcie, i + 0x8),
				exynos_phy_pcs_read(exynos_pcie, i + 0xC));
	}
	pcie_dump("\n");

	pcie_dump("[Print PHY_UDBG region]\n");
	pcie_dump("offset:             0x0               0x4               0x8               0xC\n");
	for (i = udbg->start; i < udbg->end; i += 0x10) {
		pcie_dump("PHYUDBG 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i,
				exynos_phyudbg_read(exynos_pcie, i + 0x0),
				exynos_phyudbg_read(exynos_pcie, i + 0x4),
				exynos_phyudbg_read(exynos_pcie, i + 0x8),
				exynos_phyudbg_read(exynos_pcie, i + 0xC));
	}
	pcie_dump("\n");

	pcie_dump("[Print DBI region]\n");
	pcie_dump("offset:             0x0               0x4               0x8               0xC\n");
	for (i = dbi->start; i < dbi->end; i += 0x10) {
		exynos_pcie_rc_rd_own_conf(pp, i + 0x0, 4, &val_0);
		exynos_pcie_rc_rd_own_conf(pp, i + 0x4, 4, &val_4);
		exynos_pcie_rc_rd_own_conf(pp, i + 0x8, 4, &val_8);
		exynos_pcie_rc_rd_own_conf(pp, i + 0xC, 4, &val_c);
		pcie_dump("DBI 0x%04x:    0x%08x    0x%08x    0x%08x    0x%08x\n",
				i, val_0, val_4, val_8, val_c);
	}
	pcie_dump("\n");

	if(exynos_pcie->phy->dbg_ops != NULL)
		exynos_pcie->phy->dbg_ops(exynos_pcie, REG_DUMP);

	pcie_dump("%s: ---\n", __func__);
}

static int chk_pcie_dislink(struct exynos_pcie *exynos_pcie)
{
	int test_result = 0;
	u32 linkup_offset = PCIE_ELBI_RDLH_LINKUP;
	u32 val;

	exynos_pcie_rc_poweroff(exynos_pcie->ch_num);

	pm_runtime_get_sync(exynos_pcie->pci->dev);

	val = exynos_elbi_read(exynos_pcie,
				linkup_offset) & 0x1f;
	if (val == 0x15 || val == 0x0) {
		pcie_info("PCIe link Down test Success.\n");
	} else {
		pcie_info("PCIe Link Down test Fail...\n");
		test_result = -1;
	}

	pm_runtime_put_sync(exynos_pcie->pci->dev);

	return test_result;
}

static int chk_link_recovery(struct exynos_pcie *exynos_pcie)
{
	int test_result = 0;
	u32 linkup_offset = PCIE_ELBI_RDLH_LINKUP;
	u32 val;

	exynos_elbi_write(exynos_pcie, 0x1, PCIE_APP_REQ_EXIT_L1);
	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val &= ~APP_REQ_EXIT_L1_MODE;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
	pcie_info("%s: Before set perst, gpio val = %d\n",
		__func__, gpio_get_value(exynos_pcie->perst_gpio));
	gpio_set_value(exynos_pcie->perst_gpio, 0);
	msleep(5000);
	pcie_info("%s: After set perst, gpio val = %d\n",
		__func__, gpio_get_value(exynos_pcie->perst_gpio));
	val = exynos_elbi_read(exynos_pcie, PCIE_APP_REQ_EXIT_L1_MODE);
	val |= APP_REQ_EXIT_L1_MODE;
	exynos_elbi_write(exynos_pcie, val, PCIE_APP_REQ_EXIT_L1_MODE);
	exynos_elbi_write(exynos_pcie, 0x0, PCIE_APP_REQ_EXIT_L1);
	msleep(5000);

	val = exynos_elbi_read(exynos_pcie,
				linkup_offset) & 0x1f;
	if (val >= 0x0d && val <= 0x14) {
		pcie_info("PCIe link Recovery test Success.\n");
	} else {
		/* If recovery callback is defined, pcie poweron
		 * function will not be called.
		 */
		exynos_pcie_rc_poweroff(exynos_pcie->ch_num);
		exynos_pcie_rc_poweron(exynos_pcie->ch_num);
		if (test_result != 0) {
			pcie_info("PCIe Link Recovery test Fail...\n");
			return test_result;
		}
		val = exynos_elbi_read(exynos_pcie,
				linkup_offset) & 0x1f;
		if (val >= 0x0d && val <= 0x14) {
			pcie_info("PCIe link Recovery test Success.\n");
		} else {
			pcie_info("PCIe Link Recovery test Fail...\n");
			test_result = -1;
		}
	}

	return test_result;
}

static int chk_epmem_access(struct exynos_pcie *exynos_pcie)
{
	u32 val;
	int test_result = 0;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;

	struct pci_bus *ep_pci_bus;
	void __iomem *reg_addr;
	struct resource_entry *tmp = NULL, *entry = NULL;

	/* Get last memory resource entry */
	resource_list_for_each_entry(tmp, &pp->bridge->windows)
		if (resource_type(tmp->res) == IORESOURCE_MEM)
			entry = tmp;

	if (entry == NULL) {
		pcie_err("Can't find mem resource!\n");
		return -1;
	}

	ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
	if (ep_pci_bus == NULL) {
		pcie_err("Can't find PCIe ep_pci_bus structure\n");
		return -1;
	}

	exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_0,
				4, &val);
	if (val & 0x4) {
		pcie_info("EP supports 64-bit Bar\n");
		exynos_pcie_rc_wr_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_0,
				4, lower_32_bits(entry->res->start));
		exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_0,
				4, &val);
		pcie_info("Set BAR0 : 0x%x\n", val);
		exynos_pcie_rc_wr_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_1,
				4, upper_32_bits(entry->res->start));
		exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_1,
				4, &val);
		pcie_info("Set BAR1 : 0x%x\n", val);
	} else {
		exynos_pcie_rc_wr_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_0,
				4, lower_32_bits(entry->res->start));
		exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, PCI_BASE_ADDRESS_0,
				4, &val);
		pcie_info("Set BAR0 : 0x%x\n", val);
	}

	reg_addr = ioremap(entry->res->start, SZ_4K);

	val = readl(reg_addr);
	iounmap(reg_addr);
	if (val != 0xffffffff) {
		pcie_info("PCIe EP Outbound mem access Success.\n");
	} else {
		pcie_info("PCIe EP Outbound mem access Fail...\n");
		test_result = -1;
	}

	return test_result;
}

static int chk_epconf_access(struct exynos_pcie *exynos_pcie)
{
	u32 val;
	int test_result = 0;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	struct pci_bus *ep_pci_bus;

	ep_pci_bus = pci_find_bus(exynos_pcie->pci_dev->bus->domain_nr, 1);
	if (ep_pci_bus == NULL) {
		pcie_err("Can't find PCIe ep_pci_bus structure\n");
		return -1;
	}

	exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus, 0, 0x0, 4, &val);
	pcie_info("PCIe EP Vendor ID/Device ID = 0x%x\n", val);

	exynos_pcie_rc_wr_other_conf(pp, ep_pci_bus,
					0, PCI_COMMAND, 4, 0x146);
	exynos_pcie_rc_rd_other_conf(pp, ep_pci_bus,
					0, PCI_COMMAND, 4, &val);
	if ((val & 0xfff) == 0x146) {
		pcie_info("PCIe EP conf access Success.\n");
	} else {
		pcie_info("PCIe EP conf access Fail...\n");
		test_result = -1;
	}

	return test_result;
}

static int chk_dbi_access(struct exynos_pcie *exynos_pcie)
{
	u32 val;
	int test_result = 0;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;

	exynos_pcie_rc_wr_own_conf(pp, PCI_COMMAND, 4, 0x140);
	exynos_pcie_rc_rd_own_conf(pp, PCI_COMMAND, 4, &val);
	if ((val & 0xfff) == 0x140) {
		pcie_info("PCIe DBI access Success.\n");
	} else {
		pcie_info("PCIe DBI access Fail...\n");
		test_result = -1;
	}

	return test_result;
}

static int chk_pcie_link(struct exynos_pcie *exynos_pcie)
{
	int test_result = 0;
	u32 linkup_offset = PCIE_ELBI_RDLH_LINKUP;
	u32 val;

	pm_runtime_get_sync(exynos_pcie->pci->dev);
	val = exynos_elbi_read(exynos_pcie,
					linkup_offset) & 0x1f;
	if (val >= 0x0d && val <= 0x14) {
		pcie_info("CHECK PCIe LINK(%x)\n", val);
	} else {
		test_result = -1;
	}
	pm_runtime_put_sync(exynos_pcie->pci->dev);

	return test_result;
}

int exynos_pcie_dbg_unit_test(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci;
	struct device *dev;
	int ret = 0;

	if (!exynos_pcie)
		return -1;

	pci = exynos_pcie->pci;
	dev = pci->dev;

	/* Test PCIe Link */
	pcie_info("1. Test PCIe LINK...\n");
	if (exynos_pcie_dbg_link_test(dev, exynos_pcie, 1)) {
		pcie_info("PCIe UNIT test FAIL[1/6]!!!\n");
		ret = -1;
		goto done;
	}

	pcie_info("2. Test DBI access...\n");
	/* Test PCIe DBI access */
	if (chk_dbi_access(exynos_pcie)) {
		pcie_info("PCIe UNIT test FAIL[2/5]!!!\n");
		ret = -2;
		goto done;
	}

	pcie_info("3. Test EP configuration access...\n");
	/* Test EP configuration access */
	if (chk_epconf_access(exynos_pcie)) {
		pcie_info("PCIe UNIT test FAIL[3/5]!!!\n");
		ret = -3;
		goto done;
	}

	pcie_info("4. Test EP Outbound memory region...\n");
	/* Test EP Outbound memory region */
	if (chk_epmem_access(exynos_pcie)) {
		pcie_info("PCIe UNIT test FAIL[4/5]!!!\n");
		ret = -4;
		goto done;
	}

	pcie_info("5. Test PCIe Dislink...\n");
	/* PCIe DisLink Test */
	if (chk_pcie_dislink(exynos_pcie)) {
		pcie_info("PCIe UNIT test FAIL[5/5]!!!\n");
		ret = -5;
		goto done;
	}

done:
	return ret;
}

int exynos_pcie_dbg_link_test(struct device *dev,
			struct exynos_pcie *exynos_pcie, int enable)
{
	int ret;

	pcie_info("TEST PCIe %sLink Test\n", enable ? "" : "Dis");

	if (enable) {
		if (!chk_pcie_link(exynos_pcie)) {
			ret = 0;
			goto done;
		}

		if (exynos_pcie->ep_power_gpio < 0) {
			pcie_warn("can't find wlan pin info. Need to check EP device power pin\n");
		} else {
			/* POWER PIN CONTROL */
			pcie_err("## make gpio direction to output\n");
			gpio_direction_output(exynos_pcie->ep_power_gpio, 0);

			pcie_err("## make gpio set high\n");
			gpio_set_value(exynos_pcie->ep_power_gpio, 1);
			mdelay(10);

			pcie_err("## EP_POWER_GPIO = %d\n",
					gpio_get_value(exynos_pcie->ep_power_gpio));

			mdelay(60);

			/* RESET PIN CONTROL */
			pcie_err("## make wlan reset gpio direction to output\n");
			gpio_direction_output(exynos_pcie->ep_reset_gpio, 0);

			pcie_err("## make wlan reset gpio set high\n");
			gpio_set_value(exynos_pcie->ep_reset_gpio, 1);
			mdelay(10);

			pcie_err("## EP_RESET_GPIO = %d\n",
					gpio_get_value(exynos_pcie->ep_reset_gpio));
		}

		mdelay(100);

		ret = exynos_pcie_rc_poweron(exynos_pcie->ch_num);

		if (!ret) {
			if (!chk_pcie_link(exynos_pcie))
				ret = 0;
			else
				ret = -1;
		}
	} else {
		exynos_pcie_rc_poweroff(exynos_pcie->ch_num);

		if (exynos_pcie->ep_power_gpio < 0) {
			pcie_warn("can't find wlan pin info. Need to check EP device power pin\n");
		} else {
			gpio_set_value(exynos_pcie->ep_power_gpio, 0);
		}
		ret = 0;
	}

done:
	return ret;

}

static ssize_t exynos_pcie_eom1_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 0, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!!\n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret)
		lane_width = 0;

	if (exynos_pcie->eom_rate > 1) {
		while (current_cnt != (PCIE_EOM_PH_SEL_MAX / 2) * (PCIE_EOM_DEF_VREF_MAX / 2)) {
			len += snprintf(buf + len, PAGE_SIZE,
					"%u %u %lu\n",
					eom_result[i][current_cnt].phase,
					eom_result[i][current_cnt].vref,
					eom_result[i][current_cnt].err_cnt);
			current_cnt++;
			test_cnt++;
			if (test_cnt == 100)
				break;
		}
	} else {
		while (current_cnt != (PCIE_EOM_PH_SEL_MAX / 2) * (PCIE_EOM_DEF_VREF_MAX / 2)) {
			len += snprintf(buf + len, PAGE_SIZE,
					"%u %u %lu\n",
					(eom_result[i][current_cnt].phase) / 2,
					eom_result[i][current_cnt].vref,
					eom_result[i][current_cnt].err_cnt);
			current_cnt++;
			test_cnt++;
			if (test_cnt == 100)
				break;
		}
	}

	if (current_cnt == (PCIE_EOM_PH_SEL_MAX / 2) * (PCIE_EOM_DEF_VREF_MAX / 2))
		current_cnt = 0;

exit:
	return len;
}

static ssize_t exynos_pcie_eom1_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (sscanf(buf, "%10d", &op_num) == 0)
		return -EINVAL;
	switch (op_num) {
	case 0:
		if (exynos_pcie->phy->phy_ops.phy_eom != NULL)
			exynos_pcie->phy->phy_ops.phy_eom(dev,
					exynos_pcie->phy_base);
		break;
	}

	return count;
}

static ssize_t exynos_pcie_eom2_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	/* prevent to print kerenl warning message
	   eom1_store function do all operation to get eom data */

	return count;
}

static ssize_t exynos_pcie_eom2_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	struct device_node *np = dev->of_node;
	int len = 0;
	u32 test_cnt = 0;
	static int current_cnt = 0;
	unsigned int lane_width = 1;
	int i = 1, ret;

	if (eom_result  == NULL) {
		len += snprintf(buf + len, PAGE_SIZE,
				"eom_result structure is NULL !!!\n");
		goto exit;
	}

	ret = of_property_read_u32(np, "num-lanes", &lane_width);
	if (ret) {
		lane_width = 0;
		len += snprintf(buf + len, PAGE_SIZE,
				"can't get num of lanes !!\n");
		goto exit;
	}

	if (lane_width == 1) {
		len += snprintf(buf + len, PAGE_SIZE,
				"EOM2NULL\n");
		goto exit;
	}

	if (exynos_pcie->eom_rate > 1) {
		while (current_cnt != (PCIE_EOM_PH_SEL_MAX / 2) * (PCIE_EOM_DEF_VREF_MAX / 2)) {
			len += snprintf(buf + len, PAGE_SIZE,
					"%u %u %lu\n",
					eom_result[i][current_cnt].phase,
					eom_result[i][current_cnt].vref,
					eom_result[i][current_cnt].err_cnt);
			current_cnt++;
			test_cnt++;
			if (test_cnt == 100)
				break;
		}
	} else {
		while (current_cnt != (PCIE_EOM_PH_SEL_MAX / 2) * (PCIE_EOM_DEF_VREF_MAX / 2)) {
			len += snprintf(buf + len, PAGE_SIZE,
					"%u %u %lu\n",
					(eom_result[i][current_cnt].phase) / 2,
					eom_result[i][current_cnt].vref,
					eom_result[i][current_cnt].err_cnt);
			current_cnt++;
			test_cnt++;
			if (test_cnt == 100)
				break;
		}
	}

	if (current_cnt == (PCIE_EOM_PH_SEL_MAX / 2) * (PCIE_EOM_DEF_VREF_MAX / 2))
		current_cnt = 0;

exit:
	return len;
}

static DEVICE_ATTR(eom1, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom1_show, exynos_pcie_eom1_store);

static DEVICE_ATTR(eom2, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_eom2_show, exynos_pcie_eom2_store);

int create_pcie_eom_file(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	int num_lane;

	ret = of_property_read_u32(np, "num-lanes", &num_lane);
	if (ret)
		num_lane = 0;

	ret = device_create_file(dev, &dev_attr_eom1);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
				__func__, ret);
		return ret;
	}

	if (num_lane > 0) {
		ret = device_create_file(dev, &dev_attr_eom2);
		if (ret) {
			dev_err(dev, "%s: couldn't create device file for eom(%d)\n",
					__func__, ret);
			return ret;
		}

	}

	return 0;
}

void remove_pcie_eom_file(struct device *dev)
{
	if (dev == NULL) {
		pr_err("Can't remove EOM files.\n");
		return;
	}
	device_remove_file(dev, &dev_attr_eom1);
}

int exynos_pcie_rc_lane_check(int ch_num);
int exynos_pcie_rc_speed_check(int ch_num);

static ssize_t exynos_pcie_rc_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	int i;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, ">>>> PCIe Test <<<<\n");

	for (i = TEST_START; i <= TEST_END; i++)
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d: %s\n",
				i, EXYNOS_PCIE_TEST_CASE_NAME(i));

	return ret;
}

int exynos_pcie_rc_lane_change(int ch_num, int req_lane);
int exynos_pcie_rc_speed_change(int ch_num, int req_speed);
int exynos_pcie_rc_poweron_speed(int ch_num, int spd);

static ssize_t exynos_pcie_rc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int op_num, param1;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	int ret = 0, val = 0;
	dma_addr_t phys_addr;
	void *virt_addr, *alloc;

	if (sscanf(buf, "%10d %10d", &op_num, &param1) == 0)
		return -EINVAL;

	switch (op_num) {
	case TEST_ALL:
		pcie_info("## PCIe UNIT test START ##\n");
		ret = exynos_pcie_dbg_unit_test(exynos_pcie);
		if (ret) {
			pcie_err("PCIe UNIT test failed (%d)\n", ret);
			break;
		}
		pcie_err("## PCIe UNIT test SUCCESS!!##\n");
		break;
	case TEST_LINK:
		pcie_info("## PCIe establish link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 1);
		if (ret) {
			pcie_err("PCIe establish link test failed (%d)\n", ret);
			break;
		}
		pcie_err("PCIe establish link test success\n");
		break;
	case TEST_DIS_LINK:
		pcie_info("## PCIe dis-link test ##\n");
		ret = exynos_pcie_dbg_link_test(dev, exynos_pcie, 0);
		if (ret) {
			pcie_err("PCIe dis-link test failed (%d)\n", ret);
			break;
		}
		pcie_err("PCIe dis-link test success\n");
		break;
	case TEST_CHK_LTSSM:
		pm_runtime_get_sync(dev);
		pcie_info("## LTSSM ##\n");
		ret = exynos_elbi_read(exynos_pcie,
					PCIE_ELBI_RDLH_LINKUP) & 0x3f;
		pcie_info("PCIE_ELBI_RDLH_LINKUP :0x%x, PM_STATE:0x%x\n", ret,
				exynos_phy_pcs_read(exynos_pcie, 0x188) & 0x7);
		pm_runtime_put_sync(dev);
		break;
	case TEST_DBG_OPS:
		if (exynos_pcie->phy->dbg_ops != NULL) {
			pm_runtime_get_sync(dev);
			exynos_pcie->phy->dbg_ops(exynos_pcie, SYSFS);
			pm_runtime_put_sync(dev);
		}
		break;
	case TEST_LOAD_BIN:
		pcie_info("Load DBG bin ...\n");
		exynos_pcie_rc_phy_load(exynos_pcie, DBG_BIN);
		break;
	case TEST_L1SS_DIS:
		pcie_info("L1.2 Disable....\n");
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		break;

	case TEST_L1SS_EN:
		pcie_info("L1.2 Enable....\n");
		exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		break;

	case TEST_L1SS_CTRL_STAT:
		pcie_info("l1ss_ctrl_id_state = 0x%08x\n",
				exynos_pcie->l1ss_ctrl_id_state);
		pcie_info("LTSSM: 0x%08x, PM_STATE = 0x%08x\n",
				exynos_elbi_read(exynos_pcie, PCIE_ELBI_RDLH_LINKUP),
				exynos_phy_pcs_read(exynos_pcie, 0x188));
		break;

	case TEST_FORCE_PERST:
		pcie_info("%s: force perst setting\n", __func__);
		exynos_pcie_set_perst_gpio(exynos_pcie->ch_num, 0);
		break;

	case TEST_FORCE_ALLPHYDWN:
		pcie_info("%s: force all pwndn", __func__);
		exynos_pcie->phy->phy_ops.phy_all_pwrdn(exynos_pcie, exynos_pcie->ch_num);
		break;

	case TEST_ALL_DUMP:
		exynos_pcie_rc_register_dump(exynos_pcie->ch_num);
		break;

	case TEST_SPEED_CHNG:
		pcie_info("Change speed to %d.\n", param1);
		exynos_pcie_rc_speed_change(exynos_pcie->ch_num, param1);
		break;

	case TEST_LANE_CHNG:
		pcie_info("Change Lane to %d.\n", param1);
		exynos_pcie_rc_lane_change(exynos_pcie->ch_num, param1);
		break;

	case TEST_LINKUP_W_SPD:
		pcie_err("## make gpio direction to output\n");
		gpio_direction_output(exynos_pcie->ep_power_gpio, 0);
		pcie_err("## make gpio set high\n");
		gpio_set_value(exynos_pcie->ep_power_gpio, 1);

		pcie_err("## make wlan reset gpio direction to output\n");
		gpio_direction_output(exynos_pcie->ep_reset_gpio, 0);
		pcie_err("## make wlan reset gpio set high\n");
		gpio_set_value(exynos_pcie->ep_reset_gpio, 1);

		mdelay(100);

		pcie_info("Link up with speed %d.\n", param1);
		exynos_pcie_rc_poweron_speed(exynos_pcie->ch_num, param1);
		break;

	case TEST_HISTORY_BUF:
		pm_runtime_get_sync(dev);
		exynos_pcie_dbg_print_link_history(exynos_pcie);
		pm_runtime_put_sync(dev);
		break;

	case TEST_RECOVERY:
		if (chk_link_recovery(exynos_pcie)) {
			pcie_info("PCIe Recovery Test FAIL !!\n");
		} else {
			pcie_info("PCIe Recovery Test Success !!\n");
		}
		break;

	case TEST_SPEED_LANE_CHECK:
		pcie_info("## Checking PCIe Speed & Lane  ##\n");
		pm_runtime_get_sync(dev);
		val = exynos_elbi_read(exynos_pcie,
				PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		if (val < 0x0d || val > 0x14) {
			pcie_err("is NOT link-up state(0x%x)\n", ret);
		} else {
			ret = exynos_pcie_rc_lane_check(exynos_pcie->ch_num);
			if (ret < 0) {
				pcie_err("PCIe lane check failed(%d)\n", ret);
			} else {
				pcie_info("Current PCIe Lane is %d\n", ret);
			}

			ret = exynos_pcie_rc_speed_check(exynos_pcie->ch_num);
			if (ret < 0) {
				pcie_err("PCIe Speed check failed(%d)\n", ret);
			} else {
				pcie_info("Current PCIe Speed is GEN %d\n", ret);
			}
		}
		pm_runtime_put_sync(dev);
		break;

	case TEST_TEM: /* For the code coverage check */
		pcie_info("Start TEM test.\n");

		pcie_info("1. Start Unit Test\n");
		exynos_pcie_l1ss_ctrl(1, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		exynos_pcie_l1ss_ctrl(0, PCIE_L1SS_CTRL_TEST, exynos_pcie->ch_num);
		exynos_pcie_dbg_unit_test(exynos_pcie);

		pm_runtime_get_sync(dev);
		pcie_info("2. PCIe Power on\n");
		exynos_pcie_dbg_link_test(dev, exynos_pcie, 1);

		pcie_info("3. SysMMU mapping\n");
		if (exynos_pcie->use_sysmmu) {
			virt_addr = dma_alloc_coherent(&exynos_pcie->ep_pci_dev->dev,
					SZ_4K, &phys_addr, GFP_KERNEL);
			alloc = kmalloc(SZ_4K, GFP_KERNEL);
			dma_map_single(&exynos_pcie->ep_pci_dev->dev,
					alloc,
					SZ_4K,
					DMA_FROM_DEVICE);
			dma_unmap_single(&exynos_pcie->ep_pci_dev->dev, phys_addr,
					SZ_4K, DMA_FROM_DEVICE);
			dma_free_coherent(&exynos_pcie->ep_pci_dev->dev, SZ_4K, virt_addr,
					phys_addr);
		}

		pcie_info("4. Check EP related function\n");
		exynos_pcie_rc_check_function_validity(exynos_pcie);
		exynos_pcie_dbg_print_oatu_register(exynos_pcie);

		pm_runtime_put_sync(dev);

		remove_pcie_sys_file(NULL);
		remove_pcie_eom_file(NULL);

		if (exynos_pcie->use_sysmmu)
			kfree(alloc);
		break;
	}

	return count;
}

static DEVICE_ATTR(pcie_rc_test, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
			exynos_pcie_rc_show, exynos_pcie_rc_store);

static ssize_t exynos_pcie_rc_ltssm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct exynos_pcie_ltssm_test *data = exynos_pcie->ltssm_test_data;
	int test_time = exynos_pcie->ltssm_test_time_sec;
	int total_cnt = 0;
	int ret = 0;

	total_cnt += data->l0_tick_cnt + data->l1_tick_cnt + data->l11_tick_cnt;
	total_cnt += data->l12_tick_cnt + data->l2_tick_cnt + data->rcvr_tick_cnt;

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, ">>>> PCIe LTSSM Test for %d sec<<<<\n",
							test_time);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "L0\t: %dmsec(%d%%)\n",
				data->l0_tick_cnt, (data->l0_tick_cnt * 100) / total_cnt);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "L1\t: %dmsec(%d%%)\n",
				data->l1_tick_cnt, (data->l1_tick_cnt * 100) / total_cnt);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "L11\t: %dmsec(%d%%)\n",
				data->l11_tick_cnt, (data->l11_tick_cnt * 100) / total_cnt);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "L12\t: %dmsec(%d%%)\n",
				data->l12_tick_cnt, (data->l12_tick_cnt * 100) / total_cnt);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "RCVR\t: %dmsec(%d%%)\n",
				data->rcvr_tick_cnt, (data->rcvr_tick_cnt*100) / total_cnt);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "L2\t: %dmsec(%d%%)\n",
				data->l2_tick_cnt, (data->l2_tick_cnt * 100) / total_cnt);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Total count: %d\n", total_cnt);

	return ret;
}

void exynos_pcie_rc_ltssm_test_work(struct work_struct *work)
{
	struct exynos_pcie *exynos_pcie =
			container_of(work, struct exynos_pcie, ltssm_test_work);
	struct exynos_pcie_ltssm_test *data = exynos_pcie->ltssm_test_data;
	u32 ltssm, l1sub;
	unsigned long timeout;

	pcie_info("Start LTSSM Check!!\n");
	timeout = jiffies + msecs_to_jiffies(exynos_pcie->ltssm_test_time_sec * 1000);

	while (time_before(jiffies, timeout)) {
		if (exynos_pcie->state != STATE_LINK_UP) {
			data->l2_tick_cnt++;
			udelay(3);
			goto check_delay;
		}

		ltssm = exynos_elbi_read(exynos_pcie,
				PCIE_ELBI_RDLH_LINKUP) & 0x1f;
		l1sub = exynos_phy_pcs_read(exynos_pcie, 0x188) & 0x7;

		switch (ltssm) {
		case 0x11:
			data->l0_tick_cnt++;
			break;
		case 0x14:
			if (l1sub == 0x2)
				data->l1_tick_cnt++;
			else if (l1sub == 0x5)
				data->l11_tick_cnt++;
			else if (l1sub == 0x6)
				data->l12_tick_cnt++;
			break;
		default:
			data->rcvr_tick_cnt++;
		}
check_delay:
		usleep_range(500, 500); /* 50usec margin for 500usec tick */
	}
	pcie_info("End LTSSM check!!\n");

	return;
}

static ssize_t exynos_pcie_rc_ltssm_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int time_sec;
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);

	if (sscanf(buf, "%10d", &time_sec) == 0)
		return -EINVAL;

	exynos_pcie->ltssm_test_time_sec = time_sec;
	if (!exynos_pcie->ltssm_test_data) {
		exynos_pcie->ltssm_test_data = devm_kzalloc(dev,
				sizeof(struct exynos_pcie_ltssm_test), GFP_KERNEL);
		if (!exynos_pcie->ltssm_test_data)
			return -ENOMEM;
	} else {
		exynos_pcie->ltssm_test_data->l0_tick_cnt = 0;
		exynos_pcie->ltssm_test_data->l1_tick_cnt = 0;
		exynos_pcie->ltssm_test_data->l11_tick_cnt = 0;
		exynos_pcie->ltssm_test_data->l12_tick_cnt = 0;
		exynos_pcie->ltssm_test_data->l2_tick_cnt = 0;
		exynos_pcie->ltssm_test_data->rcvr_tick_cnt = 0;
	}

	queue_work(exynos_pcie->ltssm_test_wq, &exynos_pcie->ltssm_test_work);

	return count;
}

static DEVICE_ATTR(ltssm_test, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP,
		exynos_pcie_rc_ltssm_show, exynos_pcie_rc_ltssm_store);

int create_pcie_sys_file(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_pcie_rc_test);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for test(%d)\n",
				__func__, ret);
		return ret;
	}

	ret = device_create_file(dev, &dev_attr_ltssm_test);
	if (ret) {
		dev_err(dev, "%s: couldn't create device file for ltssm_test(%d)\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

void remove_pcie_sys_file(struct device *dev)
{
	if (dev == NULL) {
		pr_err("Can't remove pcie_rc_test file.\n");
		return;
	}
	device_remove_file(dev, &dev_attr_pcie_rc_test);
}

