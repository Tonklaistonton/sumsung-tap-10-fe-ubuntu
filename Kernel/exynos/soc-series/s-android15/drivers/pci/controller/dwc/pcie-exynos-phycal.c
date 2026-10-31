/*
 * PCIe phy driver for Samsung
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/exynos-pci-noti.h>
#include <linux/regmap.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#include <crypto/hash.h>
#include <linux/platform_device.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "pcie-exynos-phycal_common.h"
#include "pcie-exynos-phycal_data.h"

#include "pcie-designware.h"
#if IS_ENABLED(CONFIG_PCIE_EXYNOS_DWPHY)
#include "pcie-exynos-rc-dwphy.h"
#else
#include "pcie-exynos-rc.h"
#endif

/* PHY all power down clear */
void exynos_pcie_rc_phy_all_pwrdn_clear(struct exynos_pcie *exynos_pcie, int ch_num)
{

	pcie_dbg("[%s] PWRDN CLR, CH#%d\n",
			__func__, exynos_pcie->ch_num);
	exynos_phycal_seq(exynos_pcie->phy->pcical_lst->pwrdn_clr,
			exynos_pcie->phy->pcical_lst->pwrdn_clr_size,
			exynos_pcie->ep_device_type);
}

/* PHY all power down */
void exynos_pcie_rc_phy_all_pwrdn(struct exynos_pcie *exynos_pcie, int ch_num)
{
	pcie_dbg("[%s] PWRDN, CH#%d\n",
			__func__, exynos_pcie->ch_num);
	exynos_phycal_seq(exynos_pcie->phy->pcical_lst->pwrdn,
			exynos_pcie->phy->pcical_lst->pwrdn_size,
			exynos_pcie->ep_device_type);
}

void exynos_pcie_rc_pcie_phy_config(struct exynos_pcie *exynos_pcie, int ch_num)
{
	pcie_err("[%s] CONFIG: PCIe CH#%d PHYCAL %s\n",
			__func__, exynos_pcie->ch_num, exynos_pcie->phy->revinfo);

	exynos_phycal_seq(exynos_pcie->phy->pcical_lst->config,
			exynos_pcie->phy->pcical_lst->config_size,
			exynos_pcie->ep_device_type);
}

void exynos_pcie_rc_phydbg(struct exynos_pcie *exynos_pcie, int id)
{
	if (exynos_pcie->phy->pcical_dbg_lst->seq_size[id] > 0) {
		exynos_phycal_seq(exynos_pcie->phy->pcical_dbg_lst->dbg_seq[id],
				exynos_pcie->phy->pcical_dbg_lst->seq_size[id],
				exynos_pcie->ep_device_type);
	}
}

int exynos_pcie_rc_calc_eom(struct exynos_pcie *exynos_pcie,
				struct exynos_pcie_eom_info *eom_info,
				int rate, int lane)
{
	struct pcie_eom_result **eom_result = exynos_pcie->eom_result;
	void *phy_base_regs = exynos_pcie->phy_base;
	unsigned int time = 0, timeout = 1000000;
	unsigned int val;
	int test_cnt = eom_info->test_cnt;
	int i;

	u32 phase = 0, vref = 0, error = 0;
	u32 real_vref_even = 0, real_vref_odd = 0;

	pcie_info("Configure index values at least...\n");
	pcie_info("[%s] rate(phy) : GEN%d (cur-lane: %d)\n", __func__, rate, lane);

	for (i = 0; i < lane; i++) {
		test_cnt = eom_info->test_cnt;
		/* initial setting */
		writel(0x27, phy_base_regs + 0x11A0 + (0x1000 * i));
		writel(0x52, phy_base_regs + 0x12A8 + (0x1000 * i));
		writel(0x53, phy_base_regs + 0x12AC + (0x1000 * i));
		writel(0x14, phy_base_regs + 0x1548 + (0x1000 * i));

		val = readl(phy_base_regs + 0x1050 + (0x1000 * i));
		val &= ~(0x3 << 4);
		val |= (0x2 << 4);
		writel(val, phy_base_regs + 0x1050 + (0x1000 * i));

		writel(eom_info->num_of_sample, phy_base_regs + 0x154C + (0x1000 * i));

		/* even & odd */
		val = readl(phy_base_regs + 0x52E8 + (0x1000 * i));
		val |= (0x1 << 1);
		writel (val, phy_base_regs + 0x52E8 + (0x1000 * i));

		val = readl(phy_base_regs + 0x5514 + (0x1000 * i));
		val |= (0x1 << 1);
		writel (val, phy_base_regs + 0x5514 + (0x1000 * i));

		val = readl(phy_base_regs + 0x127C + (0x1000 * i));
		val |= (0x1 << 4);
		writel (val, phy_base_regs + 0x127C + (0x1000 * i));

		/* start measurement */
		val = readl(phy_base_regs + 0x1540 + (0x1000 * i));
		val |= 0x1;
		writel(val, phy_base_regs + 0x1540 + (0x1000 * i));

		for (phase = eom_info->phase_start; phase < eom_info->phase_stop; phase += eom_info->phase_step)
		{
			/* ph_sel */
			writel(phase, phy_base_regs + 0x1558 + (0x1000 * i));

			for (vref = eom_info->vref_start; vref < eom_info->vref_stop; vref += eom_info->vref_step)
			{
				real_vref_even = vref * 2;
				real_vref_odd = vref * 2;

				/* set vref */
				if (rate == 0x4) {
					writel((real_vref_even % 256), phy_base_regs + 0x52E4 + (0x1000 * i));

					val = readl(phy_base_regs + 0x52E8 + (0x1000 * i));
					val |= ((int)(real_vref_even / 256) & 0x1);
					writel (val, phy_base_regs + 0x52E8 + (0x1000 * i));

					writel((real_vref_odd % 256), phy_base_regs + 0x5510 + (0x1000 * i));

					val = readl(phy_base_regs + 0x5514 + (0x1000 * i));
					val |= ((int)(real_vref_odd / 256) & 0x1);
					writel(val, phy_base_regs + 0x5514 + (0x1000 * i));

				} else if (rate == 0x3) {
					writel((real_vref_even % 256), phy_base_regs + 0x5284 + (0x1000 * i));
					writel((int)(real_vref_even / 256), phy_base_regs + 0x5288 + (0x1000 * i));
					writel((real_vref_odd % 256), phy_base_regs + 0x5508 + (0x1000 * i));
					writel((int)(real_vref_odd / 256), phy_base_regs + 0x550C + (0x1000 * i));

				} else if (rate == 0x2) {
					writel((real_vref_even % 256), phy_base_regs + 0x527C + (0x1000 * i));
					writel ((int)(real_vref_even / 256), phy_base_regs + 0x5280 + (0x1000 * i));
					writel((real_vref_odd % 256), phy_base_regs + 0x5500 + (0x1000 * i));
					writel((int)(real_vref_odd / 256), phy_base_regs + 0x5504 + (0x1000 * i));

				} else if (rate == 0x1) {
					writel((real_vref_even % 256), phy_base_regs + 0x5274 + (0x1000 * i));
					writel((int)(real_vref_even / 256), phy_base_regs + 0x5278 + (0x1000 * i));
					writel((real_vref_odd % 256), phy_base_regs + 0x5308 + (0x1000 * i));
					writel((int)(real_vref_odd / 256), phy_base_regs + 0x530C + (0x1000 * i));
				} else {
					pcie_err("[%s] Gen%d isn't supported\n", __func__, rate);
					return -1;
				}

				/* Start measurment */
				writel(0x80, phy_base_regs + 0x12A8 + (0x1000 * i));
				val = readl(phy_base_regs + 0x12A8 + (0x1000 * i));
				val |= (0x1 << 1);
				writel(val, phy_base_regs + 0x12A8 + (0x1000 * i));

				/* Wait Done */
				do {
					udelay(1);
					time++;
					if (time == timeout) {
						/* end measurement */
						val = readl(phy_base_regs + 0x1540 + (0x1000 * i));
						val &= ~(0x1);
						writel(val, phy_base_regs + 0x1540 + (0x1000 * i));

						pcie_err("[%s] Timeout !!! (%d)\n", __func__, timeout);
						return -1;
					}
				} while (!(readl(phy_base_regs + 0x12B0 + (0x1000 * i)) & 0x1));

				time = 0;

				/* Read Error Count */
				error = readl(phy_base_regs + 0x1560 + (0x1000 * i)) |
					(readl(phy_base_regs + 0x1564 + (0x1000 * i)) << 8) |
					(readl(phy_base_regs + 0x1568 + (0x1000 * i)) << 16)|
					(readl(phy_base_regs + 0x156C + (0x1000 * i)) << 24);

				pcie_dbg("[%d] (%d, %d, %d) error : 0x%x , 0x%x, 0x%x, 0x%x (%d)\n", i,
						test_cnt, phase, vref,
						readl(phy_base_regs + 0x1560 + (0x1000 * i)),
						(readl(phy_base_regs + 0x1564 + (0x1000 * i)) << 8),
						(readl(phy_base_regs + 0x1568 + (0x1000 * i)) << 16),
						(readl(phy_base_regs + 0x156C + (0x1000 * i)) << 24),
						error);

				//save result
				eom_result[i][test_cnt].phase = phase;
				eom_result[i][test_cnt].vref = vref;
				eom_result[i][test_cnt].err_cnt = error;
				test_cnt++;

				//writel(0x0, phy_base_regs + 0x1540 + (0x1000 * i));
				val = readl(phy_base_regs + 0x12A8 + (0x1000 * i));
				val &= ~(0x1 << 1);
				writel(val, phy_base_regs + 0x12A8 + (0x1000 * i));
			}
		}
		/* end measurement */
		val = readl(phy_base_regs + 0x1540 + (0x1000 * i));
		val &= ~(0x1);
		writel(val, phy_base_regs + 0x1540 + (0x1000 * i));
	}
	eom_info->test_cnt = test_cnt;

	return 0;
}

int exynos_pcie_rc_eom(struct device *dev, void *phy_base_regs)
{
	struct exynos_pcie *exynos_pcie = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	struct dw_pcie *pci = exynos_pcie->pci;
	struct dw_pcie_rp *pp = &pci->pp;
	unsigned int val;
	unsigned int lane= 1;
	unsigned int rate = 1;
	int ret = 0, i;
	struct pcie_eom_result **eom_result;
	struct exynos_pcie_eom_info *eom_info;

	dev_info(dev, "[%s] START!\n", __func__);

	ret = of_property_read_u32(np, "num-lanes", &lane);
	if (ret)
		dev_err(dev, "[%s] failed to get num of lane\n", __func__);
	else
		dev_info(dev, "[%s] MAX lanes: %d\n", __func__, lane);

	if (exynos_pcie->eom_result) {
		for (i = 0; i < lane; i++) {
			if (exynos_pcie->eom_result[i])
				kfree(exynos_pcie->eom_result[i]);
		}
		kfree(exynos_pcie->eom_result);
		exynos_pcie->eom_result = NULL;
	}

	exynos_pcie_rc_rd_own_conf(pp, exynos_pcie->pci_dev->pcie_cap +
			PCI_EXP_LNKCTL, 4, &val);
	lane = (val >> 20) & 0x3f;

	rate = readl(phy_base_regs + 0x1400) & ((0x3) << 6);
	rate = (rate >> 6) + 1;
	pcie_info("[%s] cur: GEN%d-%d\n", __func__, rate, lane);
	exynos_pcie->eom_rate = rate;

	/* eom_result[lane_num][test_cnt] */
	eom_result = kzalloc(sizeof(struct pcie_eom_result*) * lane, GFP_KERNEL);
	if (eom_result == NULL) {
		pcie_err("eom result allocation is failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < lane; i++) {
		eom_result[i] = kzalloc(sizeof(struct pcie_eom_result) *
				PCIE_EOM_PH_SEL_MAX * PCIE_EOM_DEF_VREF_MAX, GFP_KERNEL);
		if (eom_result[i] == NULL) {
			pcie_err("eom result[%d] allocation is failed\n", i);
			while (--i >= 0)
				kfree(eom_result[i]);
			kfree(eom_result);
			return -ENOMEM;
		}
	}

	exynos_pcie->eom_result = eom_result;

	/* eom information */
	eom_info = kzalloc(sizeof(struct exynos_pcie_eom_info), GFP_KERNEL);
	if (!eom_info) {
		for (i = 0; i < lane; i++)
			kfree(exynos_pcie->eom_result[i]);
		kfree(eom_result);
		return -ENOMEM;

	}

	eom_info->num_of_sample = 0x18;
	eom_info->phase_step = 2;
	eom_info->vref_step = 2;

	if (rate > 1) {
		eom_info->phase_start = 0;
		eom_info->phase_stop = PCIE_EOM_PH_SEL_MAX;
		eom_info->vref_start = 0;
		eom_info->vref_stop = PCIE_EOM_DEF_VREF_MAX;
		eom_info->test_cnt = 0;

		ret = exynos_pcie_rc_calc_eom(exynos_pcie, eom_info, rate, lane);
	} else {
		eom_info->phase_start = 0;
		eom_info->phase_stop = 64;
		eom_info->vref_start = 0;
		eom_info->vref_stop = PCIE_EOM_DEF_VREF_MAX;
		eom_info->test_cnt = 0;

		ret = exynos_pcie_rc_calc_eom(exynos_pcie, eom_info, rate, lane);
		if (ret) {
			kfree(eom_info);
			return ret;
		}

		eom_info->phase_start = 192;
		eom_info->phase_stop = 256;

		ret = exynos_pcie_rc_calc_eom(exynos_pcie, eom_info, rate, lane);
	}

	kfree(eom_info);

	return ret;
}

void exynos_pcie_rc_phy_getva(struct exynos_pcie *exynos_pcie)
{
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	int i;

	pcie_err("[%s]\n", __func__);

	if (!phy->pcical_p2vmap->size) {
		pcie_err("PCIE CH#%d hasn't p2vmap, ERROR!!!!\n", exynos_pcie->ch_num);
		return;
	}
	pcie_err("PCIE CH#%d p2vmap size: %d\n", exynos_pcie->ch_num,
						phy->pcical_p2vmap->size);

	for (i = 0; i < phy->pcical_p2vmap->size; i++) {
		pcie_dbg("[%d] pa: %#llx\n", i, phy->pcical_p2vmap->p2vmap[i].pa);
		phy->pcical_p2vmap->p2vmap[i].va = ioremap(phy->pcical_p2vmap->p2vmap[i].pa, SZ_256K);
		if (phy->pcical_p2vmap->p2vmap[i].va == NULL) {
			pcie_err("PCIE CH#%d p2vmap[%d] ioremap failed!!, ERROR!!!!\n",
								exynos_pcie->ch_num, i);
			return;
		}
	}
}

void exynos_pcie_rc_ia(struct exynos_pcie *exynos_pcie)
{
	struct exynos_pcie_phy *phy = exynos_pcie->phy;

	pcie_info("[%s] start I/A configurations.\n", __func__);

	if (phy->pcical_lst->ia0_size > 0) {
		pcie_info("[%s] IA0, CH#%d - sysreg offset : 0x%x\n",
			  __func__, exynos_pcie->ch_num, exynos_pcie->sysreg_ia0_sel);
		regmap_set_bits(exynos_pcie->sysreg, exynos_pcie->sysreg_ia0_sel,
				exynos_pcie->ch_num);
		exynos_phycal_seq(exynos_pcie->phy->pcical_lst->ia0,
				exynos_pcie->phy->pcical_lst->ia0_size,
				exynos_pcie->ep_device_type);
	}
	if (phy->pcical_lst->ia1_size > 0) {
		pcie_info("[%s] IA1, CH#%d - sysreg offset : 0x%x\n",
			  __func__, exynos_pcie->ch_num, exynos_pcie->sysreg_ia1_sel);
		regmap_set_bits(exynos_pcie->sysreg, exynos_pcie->sysreg_ia1_sel,
				exynos_pcie->ch_num);
		exynos_phycal_seq(exynos_pcie->phy->pcical_lst->ia1,
				exynos_pcie->phy->pcical_lst->ia1_size,
				exynos_pcie->ep_device_type);
	}
	if (phy->pcical_lst->ia2_size > 0) {
		pcie_info("[%s] IA2, CH#%d - sysreg offset : 0x%x\n",
			  __func__, exynos_pcie->ch_num, exynos_pcie->sysreg_ia2_sel);
		regmap_set_bits(exynos_pcie->sysreg, exynos_pcie->sysreg_ia2_sel,
				exynos_pcie->ch_num);
		exynos_phycal_seq(exynos_pcie->phy->pcical_lst->ia2,
				exynos_pcie->phy->pcical_lst->ia2_size,
				exynos_pcie->ep_device_type);
	}

}
EXPORT_SYMBOL(exynos_pcie_rc_ia);

static int exynos_pcie_rc_phy_phy2virt(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	int ret = 0;

	ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
			phy->pcical_lst->pwrdn, phy->pcical_lst->pwrdn_size);
	if (ret) {
		dev_err(pci->dev, "ERROR on PA2VA conversion, seq: pwrdn (i: %d)\n", ret);
		return ret;
	}
	ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
			phy->pcical_lst->pwrdn_clr, phy->pcical_lst->pwrdn_clr_size);
	if (ret) {
		dev_err(pci->dev, "ERROR on PA2VA conversion, seq: pwrdn_clr (i: %d)\n", ret);
		return ret;
	}
	ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
			phy->pcical_lst->config, phy->pcical_lst->config_size);
	if (ret) {
		dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
		return ret;
	}
	if (phy->pcical_lst->ia0_size > 0) {
		ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
				phy->pcical_lst->ia0, phy->pcical_lst->ia0_size);
		if (ret) {
			dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
			return ret;
		}
	} else {
		dev_err(pci->dev, "CH#%d IA0 is NULL!!\n", exynos_pcie->ch_num);
	}

	if (phy->pcical_lst->ia1_size > 0) {
		ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
				phy->pcical_lst->ia1, phy->pcical_lst->ia1_size);
		if (ret) {
			dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
			return ret;
		}
	} else {
		dev_err(pci->dev, "CH#%d IA1 is NULL!!\n", exynos_pcie->ch_num);
	}
	if (phy->pcical_lst->ia2_size > 0) {
		ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
				phy->pcical_lst->ia2, phy->pcical_lst->ia2_size);
		if (ret) {
			dev_err(pci->dev, "ERROR on PA2VA conversion, seq: config (i: %d)\n", ret);
			return ret;
		}
	} else {
		dev_err(pci->dev, "CH#%d IA2 is NULL!!\n", exynos_pcie->ch_num);
	}

	return ret;
}

static int exynos_pcie_rc_dbg_phy2virt(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	struct exynos_pcie_phycal_dbg *dbg_lst = phy->pcical_dbg_lst;
	int ret = 0, i;

	for (i = 1; i <= DBG_SEQ_NUM; i++) {
		ret = exynos_phycal_phy2virt(phy->pcical_p2vmap->p2vmap, phy->pcical_p2vmap->size,
				dbg_lst->dbg_seq[i], dbg_lst->seq_size[i]);

		if (ret) {
			dev_err(pci->dev, "ERROR on PA2VA conversion, seq id: %d (%d)\n", i, ret);
			return ret;
		}
	}
	return ret;
}

#if IS_ENABLED(CONFIG_PCI_EXYNOS_PHYCAL_DEBUG)
static void exynos_pcie_rc_phy_cal_override(struct exynos_pcie *exynos_pcie,
				int id, int size, struct phycal_seq *memblock)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	int idx;

	if (id > ID_DBG(0)) {
		idx = id - ID_DBG(0);
		if (idx > DBG_SEQ_NUM) {
			dev_err(pci->dev, "%s: Invalid ID (0x%x) !!!! \n", __func__, id);
			return;
		}
		phy->pcical_dbg_lst->dbg_seq[idx] = memblock;
		phy->pcical_dbg_lst->seq_size[idx] = size;
		return;
	}

	switch (id) {
	case ID_PWRDN:
		phy->pcical_lst->pwrdn = memblock;
		phy->pcical_lst->pwrdn_size = size;
		break;
	case ID_PWRDN_CLR:
		phy->pcical_lst->pwrdn_clr = memblock;
		phy->pcical_lst->pwrdn_clr_size = size;
		break;
	case ID_CONFIG:
		if (exynos_soc_info.main_rev != 0) {
			dev_err(pci->dev, "%s: override Config for EVT1\n", __func__);
			phy->pcical_lst->config = memblock;
			phy->pcical_lst->config_size = size;
		}
		break;
	case ID_CONFIG_EVT0:
		if (exynos_soc_info.main_rev == 0) {
			dev_err(pci->dev, "%s: override Config for EVT0\n", __func__);
			phy->pcical_lst->config = memblock;
			phy->pcical_lst->config_size = size;
		}
		break;
	case ID_IA0:
		phy->pcical_lst->ia0 = memblock;
		phy->pcical_lst->ia0_size = size;
		break;
	case ID_IA1:
		phy->pcical_lst->ia1 = memblock;
		phy->pcical_lst->ia1_size = size;
		break;
	case ID_IA2:
		phy->pcical_lst->ia2 = memblock;
		phy->pcical_lst->ia2_size = size;
		break;
	default:
		dev_err(pci->dev, "%s: Invalid ID (0x%x) !!!!\n", __func__, id);
	}

	return;
}

static int exynos_pcie_rc_phy_chksum_cmpr(struct exynos_pcie *exynos_pcie,
		unsigned int *memblock, char *chksum, int down_len)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	int y, success = 0;
	struct crypto_shash *shash = NULL;
	struct shash_desc *sdesc = NULL;
	char *result = NULL, *org_result;

	dev_dbg(pci->dev, "++++++ BINARY CHKSUM ++++++\n");

	shash = crypto_alloc_shash("md5", 0, 0);
	if (IS_ERR(shash)) {
		dev_err(pci->dev, "md5 ERROR : shash alloc fail\n");
		return -EINVAL;
	}

	sdesc = kzalloc(sizeof(struct shash_desc) + crypto_shash_descsize(shash),
			GFP_KERNEL);
	if (sdesc == NULL) {
		dev_err(pci->dev, "md5 ERROR : 'sdesc' alloc fail\n");
		return -ENOMEM;
	}

	result = kzalloc(sizeof(struct shash_desc) + crypto_shash_descsize(shash),
			GFP_KERNEL);
	if (result == NULL) {
		dev_err(pci->dev, "md5 ERROR : 'result' alloc fail\n");
		kfree(sdesc);
		return -ENOMEM;
	}
	org_result = result;

	sdesc->tfm = shash;
	success = crypto_shash_init(sdesc);
	if (success < 0) {
		dev_err(pci->dev, "md5 ERROR : cryto_shash_init fail(%d)\n", success);
	}

	success = crypto_shash_update(sdesc, (char*)memblock, (down_len - 24));
	if (success < 0) {
		dev_err(pci->dev, "md5 ERROR : cryto_shash_update fail(%d)\n", success);
	}

	success = crypto_shash_final(sdesc, result);
	if (success < 0) {
		dev_err(pci->dev, "md5 ERROR : cryto_shash_final fail(%d)\n", success);
	}

	y = crypto_shash_digestsize(sdesc->tfm);
	while(y--){
		dev_dbg(pci->dev, "(%d): %02x / %02x", y, *result, *chksum);
		if (*result != *chksum) {
			dev_err(pci->dev, "(%d): %02x / %02x", y, *result, *chksum);
			return -1;
		}
		result++;
		chksum++;
	}
	dev_info(pci->dev, "++ BINARY CHKSUM Compare DONE ++\n");

	kfree(org_result);
	kfree(sdesc);

	return 0;
}

static int exynos_pcie_rc_phy_load_bin(struct exynos_pcie *exynos_pcie, char *filepath)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	struct file *fp;
	int file_size;
	int ret = 0;

	unsigned int *memblock = NULL;
	int down_len = 0;
	int copy_len = 0;
	int struct_size = 0;

	char *checksum;
	unsigned int magic_number, binsize;

	u32 header, id, ch;

	/* 1. file open */
	dev_info(pci->dev, "%s: PHYCAL Binary Load START(%s)\n", __func__, filepath);
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		dev_err(pci->dev, " fp ERROR!!\n");
		return -1;
	}
	if (!S_ISREG(file_inode(fp)->i_mode)) {
		dev_err(pci->dev, "%s: %s is not regular file\n", __func__, filepath);
		ret = -1;
		goto err;
	}
	file_size = i_size_read(file_inode(fp));
	if (file_size <= 0) {
		dev_err(pci->dev, "%s: %s file size invalid (%d)\n", __func__, filepath, file_size);
		ret = -1;
		goto err;
	}

	dev_err(pci->dev, "%s: opened file size is %d\n", __func__, file_size);

	/* 2. alloc buffer */
	memblock = devm_kzalloc(pci->dev, file_size, GFP_KERNEL);
	if (!memblock) {
		dev_err(pci->dev, "%s: file buffer allocation is failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}

	/* 3. Donwload image */
	down_len = kernel_read(fp, memblock, (size_t)file_size, &fp->f_pos);
	if (down_len != file_size) {
		dev_err(pci->dev, "%s: Image Download is failed, down_size: %d, file_size: %d\n",
					__func__, down_len, file_size);
		ret = -EIO;
		goto err;
	}

	/* 3-1. check Magic number */
	magic_number = *memblock;
	memblock++;
	copy_len += BIN_MAGIC_NUM_SIZE;
	if (BIN_MAGIC_NUMBER != magic_number){
		dev_err(pci->dev, "%s: Magic# : 0x%x / BinMagic# : 0x%x\n",
				__func__, BIN_MAGIC_NUMBER, magic_number);
		dev_err(pci->dev, "%s: It isn't PCIe PHY Binary!\n", __func__);
		ret = -1;
		goto err;
	}

	/* 3-2. check Binary Size */
	binsize = *memblock;
	memblock++;
	copy_len += BIN_TOTAL_SIZE;

	dev_err(pci->dev, "%s: DowloadSize : %d / BinSize# : %d\n",
			__func__, down_len, binsize);
	if (down_len != binsize){
		dev_err(pci->dev, "%s: PCIe PHY Binary Size is not matched!\n", __func__);
		ret = -1;
		goto err;
	}

	/*3-3. get checksum value in header */
	checksum = devm_kzalloc(pci->dev, BIN_CHKSUM_SIZE, GFP_KERNEL);
	memcpy(checksum, memblock, BIN_CHKSUM_SIZE);

	memblock += BIN_CHKSUM_SIZE / sizeof(*memblock);
	copy_len += BIN_CHKSUM_SIZE;

	/*3-4. compare checksum */
	ret = exynos_pcie_rc_phy_chksum_cmpr(exynos_pcie, memblock, checksum, down_len);
	if (ret) {
		dev_err(pci->dev, "CHECKSUM isn't Match!!!!\n");
		goto err;
	}

	/* 4. parse binary */
	phy->revinfo = (char*)memblock;
	memblock += BIN_REV_HEADER_SIZE / (sizeof(*memblock));
	copy_len += BIN_REV_HEADER_SIZE;
	dev_err(pci->dev, "%s: New Loaded Binary revision : ver.%s\n", __func__,
		phy->revinfo);

	while (copy_len < down_len) {
		struct_size = ((*memblock) & BIN_SIZE_MASK);
		header = ((*memblock) & BIN_HEADER_MASK) >> 16;
		ch = (header & BIN_HEADER_CH_MASK) >> 8;
		id = (header & BIN_HEADER_ID_MASK);
		memblock++;
		copy_len += 4;

		dev_err(pci->dev, "%s: CH#%d / seq_id: 0x%x / size: 0x%x\n",
				__func__, ch, id, struct_size);
		if (exynos_pcie->ch_num == ch && struct_size > 0)
			exynos_pcie_rc_phy_cal_override(exynos_pcie, id, struct_size,
							(struct phycal_seq *)memblock);
		memblock += ((sizeof(struct phycal_seq) * struct_size) / sizeof(*memblock));
		copy_len += (sizeof(struct phycal_seq) * struct_size);
		dev_err(pci->dev, "%s: copy_len: %d/%d\n", __func__, copy_len, down_len);
	}
	dev_info(pci->dev, "%s: PHYCAL Binary Load END\n", __func__);

err:
	filp_close(fp, NULL);
	return ret;
}
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

int exynos_pcie_rc_phy_load(struct exynos_pcie *exynos_pcie, int is_dbg)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	char *filepath;
	int ret;

	if (!is_dbg) {
		filepath = "/vendor/etc/pcie_phycal.bin";
	} else {
		filepath = "/vendor/etc/pcie_dbg.bin";
	}

	ret = exynos_pcie_rc_phy_load_bin(exynos_pcie, filepath);
	if (ret) {
		dev_err(pci->dev, "%s: Load binary is failed, use default PHYCAL\n", __func__);
		return -1;
	}

	if (!is_dbg) {
		ret = exynos_pcie_rc_phy_phy2virt(exynos_pcie);
		if (ret) {
			dev_err(pci->dev, "%s: PHY2VIRT is failed, use default PHYCAL\n", __func__);
			return ret;
		}
	} else {
		ret = exynos_pcie_rc_dbg_phy2virt(exynos_pcie);
		if (ret) {
			dev_err(pci->dev, "%s: PHY2VIRT for DBG is failed\n", __func__);
			return ret;
		}
	}

	return 0;

}
EXPORT_SYMBOL(exynos_pcie_rc_phy_load);
#else
int exynos_pcie_rc_phy_load(struct exynos_pcie *exynos_pcie, int is_dbg)
{
	pcie_err("CONFIG_PCI_EXYNOS_PHYCAL_DEBUG is not enabled\n");
	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_phy_load);
#endif

static int exynos_pcie_rc_phy_get_p2vmap(struct exynos_pcie *exynos_pcie)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct exynos_pcie_phy *phy = exynos_pcie->phy;
	struct phycal_p2v_map *p2vmap_list;
	int i, r_cnt = 0;

	p2vmap_list = devm_kzalloc(pci->dev,
			sizeof(struct phycal_p2v_map) * pdev->num_resources,
			GFP_KERNEL);
	if (!p2vmap_list) {
		dev_err(pci->dev, "%s: p2vmap_list alloc is failed\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < pdev->num_resources; i++) {
		struct resource *r = &pdev->resource[i];

		if (unlikely(!r->name))
			continue;
		if (resource_type(r) == IORESOURCE_MEM) {
			r_cnt++;
			p2vmap_list[i].pa = r->start;
			dev_err(pci->dev, "%s\t: %#llx\n", r->name, r->start);
		}
	}
	dev_err(pci->dev, "mem resource: %d (total: %d)\n", r_cnt, pdev->num_resources);
	phy->pcical_p2vmap->size = r_cnt;
	phy->pcical_p2vmap->p2vmap = p2vmap_list;

	return 0;
}

int exynos_pcie_rc_phy_init(struct exynos_pcie *exynos_pcie, int rom_change)
{
	struct dw_pcie *pci = exynos_pcie->pci;
	struct exynos_pcie_phy *phy;
	int ret;

	dev_info(pci->dev, "Initialize PHY functions.\n");

	phy = devm_kzalloc(pci->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(pci->dev, "%s: exynos_pcie_phy alloc is failed\n", __func__);
		return -ENOMEM;
	}

	phy->phy_ops.phy_all_pwrdn = exynos_pcie_rc_phy_all_pwrdn;
	phy->phy_ops.phy_all_pwrdn_clear = exynos_pcie_rc_phy_all_pwrdn_clear;
	phy->phy_ops.phy_config = exynos_pcie_rc_pcie_phy_config;
	phy->phy_ops.phy_eom = exynos_pcie_rc_eom;

	phy->pcical_lst = &pcical_list[exynos_pcie->ch_num];
	phy->revinfo = exynos_pcie_phycal_revinfo;

	if (exynos_soc_info.main_rev == 0) {
		phy->pcical_lst->config = pciphy_config_ch0_evt0;
		phy->pcical_lst->config_size = ARRAY_SIZE(pciphy_config_ch0_evt0);
	}

# ifdef CONFIG_PCIE_EXYNOS_DWPHY
	if (rom_change) {
		phy->pcical_lst->config = pciphy_config_ch0_rom_change;
		phy->pcical_lst->config_size =
			ARRAY_SIZE(pciphy_config_ch0_rom_change);
	}
#endif

	phy->dbg_ops = exynos_pcie_rc_phydbg;
	phy->pcical_dbg_lst = &pcical_dbg_list[exynos_pcie->ch_num];

	exynos_pcie->phy = phy;

	phy->pcical_p2vmap = devm_kzalloc(pci->dev,
			sizeof(struct exynos_pcie_phycal_p2vmap), GFP_KERNEL);

	ret = exynos_pcie_rc_phy_get_p2vmap(exynos_pcie);
	if (ret) {
		dev_err(pci->dev, "%s: Getting P2VMap is failed\n", __func__);
		return ret;
	}

	exynos_pcie_rc_phy_getva(exynos_pcie);

	ret = exynos_pcie_rc_phy_phy2virt(exynos_pcie);
	if (ret) {
		dev_err(pci->dev, "%s: PHY2VIRT is failed\n", __func__);
		return ret;
	}

	ret = exynos_pcie_rc_dbg_phy2virt(exynos_pcie);
	if (ret) {
		dev_err(pci->dev, "%s: PHY2VIRT for DBG is failed\n", __func__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(exynos_pcie_rc_phy_init);

MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_DESCRIPTION("Samsung PCIe Host PHY controller driver");
MODULE_LICENSE("GPL v2");
