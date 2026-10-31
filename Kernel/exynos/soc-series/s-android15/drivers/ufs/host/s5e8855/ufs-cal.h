// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2016, Linux Foundation. All rights reserved.
 */

#ifndef _UFS_CAL_TABLE_
#define _UFS_CAL_TABLE_

#define UFS_CAL_VER 0
#include "ufs-cal-if.h"

#define UFS_ANY_VENDOR 0xFFFF
#define UFS_ANY_MODEL  "ANY_MODEL"

#define UFS_VENDOR_MICRON      0x12C
#define UFS_VENDOR_SAMSUNG     0x1CE
#define UFS_VENDOR_SKHYNIX     0x1AD
#define UFS_VENDOR_TOSHIBA     0x198
#define UFS_VENDOR_WDC         0x145

struct ufs_cal_phy_cfg {
	u32 mib;
	u32 addr;
	u32 val;
	u32 flg;
	u32 lyr;
	u8 board;
};
struct __ufs_cal_vnd_tbl {
	u16 vendor;
	char model[50];
	struct ufs_cal_phy_cfg tbl[];
};

enum {
	PHY_CFG_NONE = 0,
	PHY_PCS_COMN,
	PHY_PMA_COMN,
	PHY_PMA_TRSV,
	PHY_PLL_WAIT,
	PHY_CDR_WAIT,
	PHY_CDR_AFC_WAIT,
	UNIPRO_STD_MIB,
	UNIPRO_DBG_MIB,
	UNIPRO_DBG_APB,

	PHY_PCS_RX,
	PHY_PCS_TX,
	PHY_PCS_CFG_CLK,
	UNIPRO_DBG_PRD,
	PHY_PMA_TRSV_LANE1_SQ_OFF,
	PHY_PMA_TRSV_SQ,
	COMMON_WAIT,

	UNIPRO_ADAPT_LENGTH,
	PHY_EMB_CDR_WAIT,
	PHY_EMB_CAL_WAIT,

	HCI_AH8_THIBERN,
	HCI_AH8_REFCLKGATINGTING,
	HCI_AH8_ACTIVE_LANE,
	HCI_AH8_CMN,
	HCI_AH8_TRSV,
	HCI_STD,

	PHY_PMA_OC_CODE,
};

enum {
	PMD_PWM_G1 = 0,
	PMD_PWM_G2,
	PMD_PWM_G3,
	PMD_PWM_G4,
	PMD_PWM_G5,
	PMD_PWM,

	PMD_HS_G1,
	PMD_HS_G2,
	PMD_HS_G3,
	PMD_HS_G4,
	PMD_HS_G5,
	PMD_HS,

	PMD_ALL,
};

#undef USE_38_4_MHZ
#define USE_38_4_MHZ	 /* 38.4MHz */

static struct ufs_cal_phy_cfg init_cfg_evt0[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x0044, 0x00, PMD_ALL, UNIPRO_DBG_PRD, BRD_ALL},
	{0x0000, 0x7C04, 0x00, PMD_ALL, UNIPRO_DBG_PRD, BRD_ALL},

	{0x00, 0x013C, 0x02, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	/* caution : val is div_value not write_value if flg is PHY_PCS_CFG_CLK */

	{0x0000, 0x0A80, 0x4C, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0A84, 0x01, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0A8C, 0x4C, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0A90, 0x01, PMD_ALL, PHY_PCS_COMN, BRD_ALL},

	{0x0000, 0x0A94, 0xA6, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0A98, 0x00, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0A9C, 0x53, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0AA0, 0x00, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0AA4, 0x2A, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0AA8, 0x00, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0CD0, 0x4C, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0CD4, 0x01, PMD_ALL, PHY_PCS_COMN, BRD_ALL},


	{0x00, 0x0B1C, 0x08, PMD_ALL,  PHY_PCS_COMN, BRD_ALL},
	{0x00, 0x0B20, 0x08, PMD_ALL,  PHY_PCS_COMN, BRD_ALL},
	{0x00, 0x0C8C, 0x08, PMD_ALL,  PHY_PCS_COMN, BRD_ALL},
	{0x00, 0x0C90, 0x08, PMD_ALL,  PHY_PCS_COMN, BRD_ALL},

	{0x00, 0x0CA8, 0x3D, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x0408, 0x96, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x0000, 0x040C, 0x00, PMD_ALL, PHY_PCS_COMN, BRD_ALL},
	{0x00, 0x013C, 0x00, PMD_ALL, PHY_PCS_COMN, BRD_ALL},

	{0x00, 0x013C, 0x03, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0x00, 0x0008, 0x04, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0x00, 0x0034, 0x41, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0x00, 0x0048, 0x03, PMD_ALL, PHY_PCS_TX, BRD_ALL},
	{0x00, 0x013C, 0x00, PMD_ALL, PHY_PCS_TX, BRD_ALL},

	{0x00, 0x013C, 0x03, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x022C, 0x4C, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0250, 0x4F, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0254, 0x4F, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x026C, 0x4F, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0238, 0x1F, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0220, 0x98, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0208, 0x04, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0240, 0x41, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0D90, 0x00, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x0420, 0xFF, PMD_ALL, PHY_PCS_RX, BRD_ALL},
	{0x00, 0x013C, 0x00, PMD_ALL, PHY_PCS_RX, BRD_ALL},

	{0x155E, 0x3178, 0x00, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x3000, 0x5000, 0x00, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x3001, 0x5004, 0x01, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x4021, 0x6084, 0x01, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x4020, 0x6080, 0x01, PMD_ALL, UNIPRO_STD_MIB, BRD_ALL},
	{0x0000, 0x0A00, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0054, 0x21, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x007C, 0x04, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0084, 0x32, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0214, 0x0B, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0220, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0224, 0x30, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0228, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0230, 0xFF, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0258, 0x35, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x025C, 0x35, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0260, 0x13, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0264, 0x13, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x027C, 0x20, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x02A8, 0x0B, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0354, 0x50, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0834, 0x52, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0838, 0x52, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0840, 0x25, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0844, 0x25, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x084C, 0x12, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0850, 0x12, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0xA38, 0x11, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0xA44, 0x00, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0xA50, 0x11, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0xA5C, 0x11, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x02FC, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0300, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0308, 0x00, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x030C, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x122C, 0x11, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x14E4, 0x11, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1620, 0x11, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1554, 0x24, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1558, 0x24, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1560, 0x53, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1564, 0x53, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x156C, 0x53, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1570, 0x53, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x15E8, 0x24, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x15EC, 0x24, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x15F4, 0x53, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x15F8, 0x53, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1600, 0x24, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1604, 0x24, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x100C, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1010, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1014, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1018, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1050, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x1B04, 0x10, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1AC8, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x12AC, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x12B0, 0x12, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1330, 0x23, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x135C, 0xBA, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1334, 0x22, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1340, 0x12, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1378, 0x23, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x13A4, 0xB9, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x137C, 0x22, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x1388, 0x12, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x13DC, 0x23, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x13F4, 0xA9, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x13E0, 0x34, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x13EC, 0x11, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x13F8, 0x98, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x136C, 0x33, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x150C, 0x03, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x15B8, 0x39, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x19AC, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x1524, 0x0F, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x12BC, 0x01, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x17BC, 0x17, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x17EC, 0x16, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x158C, 0x35, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0x1004, 0x13, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},

	{0x0000, 0xA00, 0x11, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0xA00, 0x00, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x1F5C, 0x01, PMD_ALL, PHY_EMB_CAL_WAIT, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg init_cfg_evt0_overwrite[][5] = {
	{{0x0000, 0x1050, 0x00, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x1018, 0x04, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x136C, 0x33, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0x0000, 0x19AC, 0x06, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}}
};

static struct __ufs_cal_vnd_tbl vendor_samsung = {
	.vendor = UFS_VENDOR_SAMSUNG,
	//.model = UFS_ANY_MODEL,
	.model = "KLUEG4RHHD-B0G1",
	.tbl = {
		/* Tx De-amp */
		{0x0000, 0x1054, 0x02, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
		/* Tx Amplitude */
		{0x0000, 0x1144, 0x02, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
		{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL},
	},
};

static struct __ufs_cal_vnd_tbl *ufs_cal_vnd_tbl[] = {
	&vendor_samsung,
	NULL,
};

static struct ufs_cal_phy_cfg init_cfg_evt1[] = {
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg init_cfg_evt1_overwrite[][5] = {
	{{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}}
};

static struct ufs_cal_phy_cfg post_init_cfg[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x15D2, 0x3348, 0x00, PMD_ALL, UNIPRO_ADAPT_LENGTH, BRD_ALL},
	{0x15D3, 0x334C, 0x00, PMD_ALL, UNIPRO_ADAPT_LENGTH, BRD_ALL},

	{0x0000, 0x153C, 0x10, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_pwm[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x2042, 0x4108, 28224, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B3, 0x32CC, 12000, PMD_PWM, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x788C, 28224, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C4, 12000, PMD_PWM, UNIPRO_DBG_APB, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_pwm[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x15D4, 0x3350, 0x1, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x2042, 0x4108, 28224, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B3, 0x32CC, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C4, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},

	{0x0000, 0x0084, 0x31, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, PMD_HS_G4, PHY_PMA_OC_CODE, BRD_ALL},
	{0, 0, 0, PMD_HS_G5, PHY_PMA_OC_CODE, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x15D4, 0x3350, 0x1, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x2042, 0x4108, 28224, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x2043, 0x410C, 20160, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B0, 0x32C0, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B1, 0x32C4, 32000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B2, 0x32C8, 16000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},
	{0x15B3, 0x32CC, 12000, PMD_HS, UNIPRO_STD_MIB, BRD_ALL},

	{0x0000, 0x788C, 28224, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x7890, 20160, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78B8, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78BC, 32000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C0, 16000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},
	{0x0000, 0x78C4, 12000, PMD_HS, UNIPRO_DBG_APB, BRD_ALL},

	{0x0000, 0x0084, 0x32, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, PMD_HS_G4, PHY_PMA_OC_CODE, BRD_ALL},
	{0, 0, 0, PMD_HS_G5, PHY_PMA_OC_CODE, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg backup_pma_oc_code_for_snr[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	/* 0x00~0x0D : backup index(not used)
	 * these are only lists to save for PHY_PMA_OC_CODE
	 * and should be less or equal than PMA_OC_CODE_SAVECNT value.
	 * refer to the ufs_snr_save_pma_oc_code() for detail.
	 */
	{0, 0x1E78, 0x00, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E7C, 0x01, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E80, 0x02, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E84, 0x03, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E8C, 0x04, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E90, 0x05, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E94, 0x06, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E98, 0x07, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1E9C, 0x08, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1EA0, 0x09, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1EA4, 0x0A, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1EA8, 0x0B, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1EAC, 0x0C, PMD_HS, PHY_PMA_TRSV, BRD_ALL},
	{0, 0x1EB0, 0x0D, PMD_HS, PHY_PMA_TRSV, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg lane1_sq_off[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x1584, 0x21, PMD_ALL, PHY_PMA_TRSV_LANE1_SQ_OFF, BRD_ALL},
	{0x0000, 0x1588, 0x20, PMD_ALL, PHY_PMA_TRSV_LANE1_SQ_OFF, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_h8_enter[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	//H8 Tx glitch issue -  : MX didn't apply this
	{0x0000, 0x0000, 0x01, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0610, 0x21, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x1584, 0x21, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x1588, 0x20, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x0A20, 0x10, PMD_ALL, PHY_PMA_COMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg pre_h8_exit[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	//H8 Tx glitch issue -  : MX didn't apply this
	{0x0000, 0x0A20, 0x00, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0000, 0x1E, PMD_ALL, COMMON_WAIT, BRD_ALL},
	{0x0000, 0x0000, 0x31, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0610, 0x23, PMD_ALL, PHY_PMA_COMN, BRD_ALL},
	{0x0000, 0x0000, 0x03, PMD_ALL, COMMON_WAIT, BRD_ALL},
	{0x0000, 0x1584, 0x01, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},
	{0x0000, 0x1588, 0x00, PMD_ALL, PHY_PMA_TRSV_SQ, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_ah8_cfg[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x1604, 0x00, PMD_ALL, HCI_AH8_THIBERN, BRD_ALL},
	{0x0000, 0x1608, 0x00, PMD_ALL, HCI_AH8_REFCLKGATINGTING, BRD_ALL},
	{0x0000, 0x161C, 0x00, PMD_ALL, HCI_AH8_ACTIVE_LANE, BRD_ALL},
	{0x0000, 0x1624, 0x35B60, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	// Retry Number & Access Number
	//H8 Tx glitch issue -  : MX didn't apply this
	{0x0000, 0x1620, 0x140016, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	// need to lane# check
	{0x0000, 0x1710, 0x00000000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1750, 0x00000001, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1714, 0x00000610, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1754, 0x00000021, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1718, 0x00001584, PMD_ALL, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x1758, 0x00000021, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x171C, 0x00001588, PMD_ALL, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x175C, 0x00000020, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1720, 0x00000A20, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1760, 0x00000010, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1790, 0x00000A20, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17D0, 0x00000000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1794, 0x401E0000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17D4, 0x00000000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x1798, 0x00000000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17D8, 0x00000031, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x179C, 0x00000610, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17DC, 0x00000023, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17A0, 0x40030000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17E0, 0x00000000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17A4, 0x00001584, PMD_ALL, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x17E4, 0x00000001, PMD_ALL, HCI_AH8_CMN, BRD_ALL},
	{0x0000, 0x17A8, 0x00001588, PMD_ALL, HCI_AH8_TRSV, BRD_ALL},
	{0x0000, 0x17E8, 0x00000000, PMD_ALL, HCI_AH8_CMN, BRD_ALL},

	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg eom_prepare[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg eom_enable[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x153C, 0x11, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg eom_disable[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0x0000, 0x153C, 0x10, PMD_ALL, PHY_PMA_TRSV, BRD_ALL},
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg init_cfg_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_init_cfg_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_pwm_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_pwm_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_a_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_a_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg calib_of_hs_rate_b_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_calib_of_hs_rate_b_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg lane1_sq_off_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg post_h8_enter_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg pre_h8_exit_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_init_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_1_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

static struct ufs_cal_phy_cfg loopback_set_2_card[] = {
	/* mib(just to monitor), sfr offset, value, .. */
	{0, 0, 0, 0, PHY_CFG_NONE, BRD_ALL}
};

#endif	/* _UFS_CAL_TABLE_ */
