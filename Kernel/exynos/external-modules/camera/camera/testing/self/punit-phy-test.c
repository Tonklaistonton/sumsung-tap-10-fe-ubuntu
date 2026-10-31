// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/regulator/consumer.h>
#include "is-core.h"
#include "pablo-utc.h"

static int pst_set_phy(const char *val, const struct kernel_param *kp);
static int pst_get_phy(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_phy = {
	.set = pst_set_phy,
	.get = pst_get_phy,
};
module_param_cb(test_phy, &pablo_param_ops_phy, NULL, 0644);

enum pst_power_state {
	PST_PHY_POWER,
	PST_PHY_SFR,
	PST_PHY_SETTING,
	PST_PHY_TEST,
};

static unsigned long result_phy;
static struct putc_info putc_res_phy[PST_PHY_TEST];
static u32 putc_phy_camdev_id[PST_PHY_TEST] = {
	899, /* MIPI-DPHY on/off */
	1548, /* set value for MIPI-DPHY */
	1549, /* set MIPI-DPHY with data rate */
};

static struct is_sensor_cfg cfg[] = {
	/* for cphy */
	[0].mipi_speed = 2000,
	[0].lanes = 2,
};

static void pst_phy_power_ctl(bool enable)
{
	int ret;
	struct is_core *core = is_get_is_core();

	if (enable)
		ret = is_resource_get(&core->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	else
		ret = is_resource_put(&core->resourcemgr, RESOURCE_TYPE_ISCHAIN);

	if (ret)
		pr_err("%s: is_resource_%s is fail (%d)\n", __func__, enable ? "get":"put", ret);
}

static inline int phy_lane_check(void __iomem *base, struct phy_setfile *sf, bool pw_gating)
{
	struct phy_sfr_info *phy_sfr;
	int lane_ofs = 0x100;
	int i, lane;
	u32 val;

	for (i = 0; i < sf->phy_sfr_cnt; i++) {
		phy_sfr = &sf->phy_sfr_info[i];
		for (lane = 0; lane < phy_sfr->max_lane; lane++) {
			if (phy_sfr->index != IDX_SKW_DLY && phy_sfr->index != IDX_STL_CLK) {
				update_bits(base + (lane * lane_ofs) + phy_sfr->addr,
					phy_sfr->start, phy_sfr->width, phy_sfr->val);

				val = readl(base + (lane * lane_ofs) + phy_sfr->addr);
				if (pw_gating) {
					if (val != 0x0) {
						pr_err("phy power gating addr: %04X, val: %08X != %08X\n",
							phy_sfr->addr, phy_sfr->val, val);
						return -EPERM;
					}
				} else {
					if (phy_sfr->val != val) {
						pr_err("phy sfr addr: %04X, val: %08X != %08X\n",
							phy_sfr->addr, phy_sfr->val, val);
						return -EPERM;
					}
				}
			}
		}
	}

	return 0;
}

static int phy_sfr_check(struct v4l2_subdev *sd_csi)
{
	struct is_device_csi *csi;
	struct phy_setfile_table *sf_tbl;
	struct mipi_phy_desc *phy_desc;
	struct phy_setfile *sf;
	struct phy_sfr_info *phy_sfr;
	void __iomem *base;
	int i, ret = 0, ret_pwr = 0;
	u32 val;

	csi = (struct is_device_csi *)v4l2_get_subdevdata(sd_csi);
	sf_tbl = &csi->phy_sf_tbl[0];

	phy_desc = (struct mipi_phy_desc *)phy_get_drvdata(csi->phy);
	if (!phy_desc) {
		pr_err("failed to get phy description\n");
		return -ENXIO;
	}

	/* for power gating */
	sf = &sf_tbl->sf[PPS_LANE];
	ret = phy_lane_check(phy_desc->regs_lane, sf, true);
	if (ret)
		return ret;

	ret_pwr = phy_power_on(csi->phy);
	if (ret_pwr) {
		pr_err("failed to phy power on\n");
		return ret_pwr;
	}

	/* for bias and clock */
	sf = &sf_tbl->sf[PPS_COMM];
	for (i = 0; i < sf->phy_sfr_cnt; i++) {
		phy_sfr = &sf->phy_sfr_info[i];

		base = (phy_sfr->index == IDX_BIA_VAL) ? csi->phy_reg : phy_desc->regs;
		update_bits(base + phy_sfr->addr, phy_sfr->start, phy_sfr->width, phy_sfr->val);

		val = readl(base + phy_sfr->addr);
		if (phy_sfr->val != val) {
			pr_err("phy sfr addr: %04X, val: %08X != %08X\n", phy_sfr->addr,
				phy_sfr->val, val);
			ret = -EPERM;
			goto out;
		}
	}

	/* for data lane */
	sf = &sf_tbl->sf[PPS_LANE];
	ret = phy_lane_check(phy_desc->regs_lane, sf, false);

out:
	ret_pwr = phy_power_off(csi->phy);
	if (ret_pwr)
		pr_err("failed to phy power off\n");

	return ret_pwr ? ret_pwr : ret;
}

static inline void pst_phy_set_utc_result(u32 idx, bool flag)
{
	putc_res_phy[idx].camdev_id = putc_phy_camdev_id[idx];
	putc_res_phy[idx].result = flag ? P_PASS : P_FAIL;
}

static void pst_phy_power(void)
{
	struct is_core *core = is_get_is_core();
	struct is_resourcemgr *resmgr;
	int i, ret = 0, flag = true;

	pst_phy_power_ctl(true);

	resmgr = &core->resourcemgr;

	for (i = 0; i < resmgr->num_phy_ldos; i++) {
		ret = regulator_enable(resmgr->phy_ldos[i]);
		if (ret)
			pr_err("failed to enable phy ldo[%d](%d)", i, ret);

		if (!regulator_is_enabled(resmgr->phy_ldos[i])) {
			pr_info("phy ldo[%d] is not enabled", i);
			flag = false;
		}
	}

	pst_phy_power_ctl(false);

	for (i = 0; i < resmgr->num_phy_ldos; i++) {
		ret = regulator_disable(resmgr->phy_ldos[i]);
		if (ret)
			pr_err("failed to disable phy ldo[%d](%d)", i, ret);

		if (regulator_is_enabled(resmgr->phy_ldos[i])) {
			pr_info("phy ldo[%d] is not disabled", i);
			flag = false;
		}
	}

	if (flag)
		set_bit(PST_PHY_POWER, &result_phy);

	pst_phy_set_utc_result(PST_PHY_POWER, flag);
}

static void pst_phy_sfr(void)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *device;
	struct is_device_csi *csi;
	int i, ret, flag = true;

	pst_phy_power_ctl(true);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		if (!device->subdev_csi)
			continue;

		csi = (struct is_device_csi *)v4l2_get_subdevdata(device->subdev_csi);

		ret = phy_sfr_check(device->subdev_csi);
		if (ret) {
			pr_err("CSIS%d phy sfr check fail(%d)\n", csi->otf_info.csi_ch, ret);
			flag = false;
		}

		break;
	}

	pst_phy_power_ctl(false);

	if (flag)
		set_bit(PST_PHY_SFR, &result_phy);

	pst_phy_set_utc_result(PST_PHY_SFR, flag);
}

static void pst_phy_setting(void)
{
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *device;
	struct is_device_csi *csi;
	int i, ret, flag = true;
	u32 settle = 0;

	pst_phy_power_ctl(true);

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		device = &core->sensor[i];
		if (!device->subdev_csi)
			continue;

		csi = (struct is_device_csi *)v4l2_get_subdevdata(device->subdev_csi);

		ret = phy_power_on(csi->phy);
		if (ret)
			pr_err("failed to phy power on\n");

		ret = csi_hw_s_phy_set(csi->phy, cfg[0].lanes, cfg[0].mipi_speed, settle,
						csi->otf_info.csi_ch, csi->use_cphy, &csi->phy_sf_tbl[0],
						csi->phy_reg, csi->base_reg);
		if (ret) {
			pr_err("failed to phy set\n");
			flag = false;
		}

		ret = phy_power_off(csi->phy);
		if (ret)
			pr_err("failed to phy power off\n");

		break;
	}

	pst_phy_power_ctl(false);

	if (flag)
		set_bit(PST_PHY_SETTING, &result_phy);

	pst_phy_set_utc_result(PST_PHY_SETTING, flag);
}

static int pst_set_phy(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	u32 act;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("No argument!\n");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &act);
	if (ret) {
		pr_err("Invalid act %d ret %d\n", act, ret);
		goto func_exit;
	}

	if (!is_get_is_core()) {
		pr_err("core is NULL\n");
		ret = -EPERM;
		goto func_exit;
	}

	switch (act) {
	case PST_PHY_POWER:
		pr_info("phy power check\n");
		pst_phy_power();
		break;
	case PST_PHY_SFR:
		pr_info("phy sfr check\n");
		pst_phy_sfr();
		break;
	case PST_PHY_SETTING:
		pr_info("phy setting check\n");
		pst_phy_setting();
		break;
	default:
		pr_err("NOT supported act %u\n", act);
		ret = -EPERM;
	}

func_exit:
	argv_free(argv);

	return ret;
}

static int pst_get_phy(char *buffer, const struct kernel_param *kp)
{
	int ret;
	int i;

	pr_info("phy test result(0x%lx)\n", result_phy);

	ret = sprintf(buffer, "%d ", PST_PHY_TEST);
	ret += sprintf(buffer + ret, "%lx\n", result_phy);
	ret += sprintf(buffer + ret, "[UTC]\n");
	for (i = 0; i < PST_PHY_TEST; i++)
		ret += sprintf(
			buffer + ret, "%d:%d\n", putc_res_phy[i].camdev_id, putc_res_phy[i].result);

	result_phy = 0;

	return ret;
}
