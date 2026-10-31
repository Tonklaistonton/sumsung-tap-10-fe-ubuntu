#ifndef ACPM_FRAMEWORK
/* common sequence descriptor for lpm init. - exposed to common logic */
struct pmucal_seq pmucal_lpm_init[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PS_HOLD_CONTROL", 0x11860000, 0x3f90, (0x1 << 31), (0x1 << 31), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PS_HOLD_CONTROL", 0x11860000, 0x3f90, (0x1 << 8), (0x1 << 8), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "APM_RCO_DIVIDER_CTRL", 0x11860000, 0x3f34, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_RCO_DIVIDER_CTRL", 0x11860000, 0x3f30, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SHORTSTOP_DSU_SHORTSTOP", 0x18870000, 0x0820, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SHORTSTOP_CPUCL0_GLB_SHORTSTOP", 0x18830000, 0x0820, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SHORTSTOP_CPUCL0_SHORTSTOP", 0x18840000, 0x0820, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SHORTSTOP_CPUCL1_SHORTSTOP", 0x18850000, 0x0824, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SHORTSTOP_CMU_CUSTOM_EMASHORTSTOP_MIF", 0x10400000, 0x0000, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SHORTSTOP_CMU_CUSTOM_EMASHORTSTOP_MIF", 0x10800000, 0x0000, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x11820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x16420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x11620000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x12020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x13820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x18800000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x11e20000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10c00000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x17020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x15420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x13c20000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x15020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BURNIN_CTRL", 0x11860000, 0x3d48, (0x3 << 0), (0x2 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BURNIN_CTRL", 0x11860000, 0x3d48, (0xff << 16), (0x0 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLK_CON_DIV_DIV_OSC_PMU", 0x11860000, 0x0b54, (0x3ff << 17), (0x3 << 17), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_RCO_DIVIDER_CTRL2", 0x11860000, 0x3f44, (0xffffffff << 0), (0xc350 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "PMU_RCO_DIVIDER_CTRL", 0x11860000, 0x3f30, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLK_CON_GAT_BLK_G3DCORE_UID_RSTNSYNC_SR_CLK_G3DCORE_NOCP_IPCLKPORT_CLK", 0x10c30000, 0x2010, (0x3 << 20), (0x3 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLK_CON_GAT_BLK_CHUBVTS_UID_BLK_CHUBVTS_FRC_OTP_DESERIAL_IPCLKPORT_I_CLK", 0x11600000, 0x205c, (0x3 << 20), (0x3 << 20), 0, 0, 0xffffffff, 0),
};
unsigned int pmucal_lpm_init_size = ARRAY_SIZE(pmucal_lpm_init);
/* individual sequence descriptor for each power mode - enter, exit, early_wakeup */
struct pmucal_seq enter_sicd[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLUSTER0_NONCPU_CTRL", 0x11860000, 0x1210, (0xffff << 0), (0x2 << 0), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq save_sicd[] = {
};
struct pmucal_seq exit_sicd[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP27_INTR_BID_CLEAR", 0x11860000, 0x31bc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31b8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP31_INTR_BID_CLEAR", 0x11860000, 0x31fc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31f8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_EN", 0x11860000, 0x3b44, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_EN", 0x11860000, 0x3b64, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq early_sicd[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP27_INTR_BID_CLEAR", 0x11860000, 0x31bc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31b8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP31_INTR_BID_CLEAR", 0x11860000, 0x31fc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31f8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_EN", 0x11860000, 0x3b44, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_EN", 0x11860000, 0x3b64, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SYSTEM_CTRL", 0x11860000, 0x3c10, (0x1 << 14), (0x0 << 14), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP4_INTR_BID_CLEAR", 0x11860000, 0x304c, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3048, (0x1 << 0), (0x1 << 0) | (0x1 << 0)),
};
struct pmucal_seq enter_sleep[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLUSTER0_NONCPU_CTRL", 0x11860000, 0x1210, (0xffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq save_sleep[] = {
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_CPUCL0_SHORTSTOP", 0x18840000, 0x0820, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "DMYQCH_CON_DFTMUX_TOP_QCH_CIS_CLK0", 0x13900000, 0x3004, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "DMYQCH_CON_DFTMUX_TOP_QCH_CIS_CLK1", 0x13900000, 0x3008, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "DMYQCH_CON_DFTMUX_TOP_QCH_CIS_CLK2", 0x13900000, 0x300c, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "DMYQCH_CON_DFTMUX_TOP_QCH_CIS_CLK3", 0x13900000, 0x3010, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "DMYQCH_CON_DFTMUX_TOP_QCH_CIS_CLK4", 0x13900000, 0x3014, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_DIV_CLK_NOCL1A_NOCP", 0x16400000, 0x1800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_DIV_CLK_G3D_HTU", 0x10c20000, 0x1800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_DIV_CLK_G3D_NOCP", 0x10c20000, 0x1804, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_COND_SAVE_RESTORE, "CLK_CON_DIV_CLK_PSP_NOCP", 0x13c00000, 0x1800, 0xffffffff, 0, 0x11860000, 0x20c4, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_NOCL1A_RCO_BOOST", 0x16400000, 0x1000, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_NOCL1A_CMUREF", 0x16400000, 0x1004, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_I2C", 0x15400000, 0x1000, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_UART_DBG", 0x15400000, 0x1004, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_USI00", 0x15400000, 0x1008, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_USI01", 0x15400000, 0x100c, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_USI02", 0x15400000, 0x1010, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_USI03", 0x15400000, 0x1014, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_USI04", 0x15400000, 0x1018, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_USI09_USI_OIS", 0x15400000, 0x101c, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIC_USI10_USI_OIS", 0x15400000, 0x1020, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_PERIS_GIC", 0x10030000, 0x1000, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_USI_I2C", 0x15000000, 0x1000, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_USI_USI06", 0x15000000, 0x1004, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_USI_USI07", 0x15000000, 0x1008, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_NOCL1A_NOC_USER", 0x16400000, 0x0600, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_G3D_HTU", 0x10c20000, 0x0600, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_G3D_NOCP", 0x10c20000, 0x0610, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_HSI_NOC_USER", 0x17000000, 0x0600, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_HSI_UFS_EMBD_USER", 0x17000000, 0x0610, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_PERIC_IP_USER", 0x15400000, 0x0600, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_PERIC_MMC_CARD_USER", 0x15400000, 0x0610, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_PERIC_NOC_USER", 0x15400000, 0x0620, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_COND_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_PSP_NOC_USER", 0x13c00000, 0x0600, 0xffffffff, 0, 0x11860000, 0x20c4, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_PERIS_NOC_USER", 0x10030000, 0x0600, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLK_PERIS_GIC_USER", 0x10030000, 0x0610, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_USI_IP_USER", 0x15000000, 0x0600, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_USI_NOC_USER", 0x15000000, 0x0610, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_NOCL1A_CLKOUT0", 0x16400000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_TOP_CLKOUT0", 0x13900000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_NOCL0_CLKOUT0", 0x13800000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_CPUCL0_CLKOUT0", 0x18840000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_CPUCL1_CLKOUT0", 0x18850000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_CPUCL2_CLKOUT0", 0x18860000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_DSU_CLKOUT0", 0x18870000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_CPUCL0_GLB_CLKOUT0", 0x18830000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_G3D_CLKOUT0", 0x10c20000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_HSI_CLKOUT0", 0x17000000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_MIF_CLKOUT0", 0x10400000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_MIF_CLKOUT0", 0x10800000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_PERIC_CLKOUT0", 0x15400000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_COND_SAVE_RESTORE, "CLKOUT_CON_CMU_PSP_CLKOUT0", 0x13c00000, 0x0810, 0xffffffff, 0, 0x11860000, 0x20c4, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_PERIS_CLKOUT0", 0x10030000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_USI_CLKOUT0", 0x15000000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "NOCL1A_CONTROLLER_OPTION0_CMU_CTRL", 0x16400000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "TOP_CONTROLLER_OPTION0_CMU_CTRL", 0x13900000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "NOCL0_CONTROLLER_OPTION0_CMU_CTRL", 0x13800000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CPUCL0_CONTROLLER_OPTION0_CMU_CTRL", 0x18840000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CPUCL1_CONTROLLER_OPTION0_CMU_CTRL", 0x18850000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CPUCL2_CONTROLLER_OPTION0_CMU_CTRL", 0x18860000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "DSU_CONTROLLER_OPTION0_CMU_CTRL", 0x18870000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CPUCL0_GLB_CONTROLLER_OPTION0_CMU_CTRL", 0x18830000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "G3D_CONTROLLER_OPTION0_CMU_CTRL", 0x10c20000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "HSI_CONTROLLER_OPTION0_CMU_CTRL", 0x17000000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PERIC_CONTROLLER_OPTION0_CMU_CTRL", 0x15400000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_COND_SAVE_RESTORE, "PSP_CONTROLLER_OPTION0_CMU_CTRL", 0x13c00000, 0x0800, 0xffffffff, 0, 0x11860000, 0x20c4, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PERIS_CONTROLLER_OPTION0_CMU_CTRL", 0x10030000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "USI_CONTROLLER_OPTION0_CMU_CTRL", 0x15000000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_G3DCORE_SHORTSTOP", 0x10c30000, 0x0820, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_CPUCL0_GLB_SHORTSTOP", 0x18830000, 0x0820, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLKOUT_CON_CMU_CPUCL0_CLKOUT0", 0x18840000, 0x0810, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_CPUCL1_SHORTSTOP", 0x18850000, 0x0824, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_DSU_SHORTSTOP", 0x18870000, 0x0820, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_CMU_CUSTOM_EMASHORTSTOP_MIF", 0x10400000, 0x0000, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_CMU_CUSTOM_EMASHORTSTOP_MIF", 0x10800000, 0x0000, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_LOCKTIME_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x000c, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON2_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x0108, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON4_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x0110, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON5_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x0114, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON6_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x0118, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON7_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x011c, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "CLK_CON_MUX_CLK_G3DCORE_PLL", 0x10c30000, 0x1000, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_MUX_CLKCMU_G3DCORE_SWITCH_USER", 0x10c30000, 0x0600, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON3_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x010c, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON0_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x0100, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "PLL_CON1_RPLL_CTRL_PLL_G3D", 0x10c30000, 0x0104, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "SHORTSTOP_G3DCORE_SHORTSTOP", 0x10c30000, 0x0820, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "G3DCORE_CONTROLLER_OPTION0_CMU_CTRL", 0x10c30000, 0x0800, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SAVE_RESTORE, "G3DCORE_CONTROLLER_OPTION1_CMU_CTRL", 0x10c30000, 0x0804, 0xffffffff, 0, 0, 0, 0xffffffff, 0),
};
struct pmucal_seq exit_sleep[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP27_INTR_BID_CLEAR", 0x11860000, 0x31bc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31b8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP31_INTR_BID_CLEAR", 0x11860000, 0x31fc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31f8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_EN", 0x11860000, 0x3b44, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_EN", 0x11860000, 0x3b64, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x11820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x16420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x12020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x13820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x18800000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x17020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x15420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x15020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_COND_WRITE, "SLEEP_IO_CTRL", 0x11860000, 0x3d30, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3b20, (0x1 << 11), (0x0 << 11)),
	PMUCAL_SEQ_DESC(PMUCAL_SET_BIT_ATOMIC, "TOP_OUT", 0x11860000, 0x3b20, (0xffffffff << 0), (0xb << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SET_BIT_ATOMIC, "TOP_OUT", 0x11860000, 0x3b20, (0xffffffff << 0), (0xf << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_SET_BIT_ATOMIC, "AUD_OUT", 0x11860000, 0x1960, (0xffffffff << 0), (0xb << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SLEEP_IO_CTRL", 0x11860000, 0x3d30, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLK_CON_GAT_BLK_G3DCORE_UID_RSTNSYNC_SR_CLK_G3DCORE_NOCP_IPCLKPORT_CLK", 0x10c30000, 0x2010, (0x3 << 20), (0x3 << 20), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq early_sleep[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP27_INTR_BID_CLEAR", 0x11860000, 0x31bc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31b8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP31_INTR_BID_CLEAR", 0x11860000, 0x31fc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31f8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_EN", 0x11860000, 0x3b44, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_EN", 0x11860000, 0x3b64, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SYSTEM_CTRL", 0x11860000, 0x3c10, (0x1 << 14), (0x0 << 14), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP4_INTR_BID_CLEAR", 0x11860000, 0x304c, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3048, (0x1 << 0), (0x1 << 0) | (0x1 << 0)),
};
struct pmucal_seq enter_stop[] = {
};
struct pmucal_seq save_stop[] = {
};
struct pmucal_seq exit_stop[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP27_INTR_BID_CLEAR", 0x11860000, 0x31bc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31b8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP31_INTR_BID_CLEAR", 0x11860000, 0x31fc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31f8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_EN", 0x11860000, 0x3b44, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_EN", 0x11860000, 0x3b64, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x11820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x16420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x12020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x13820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x18800000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x17020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10820000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x10020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x15420000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "BUS_COMPONENT_DRCG_EN", 0x15020000, 0x0104, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq early_stop[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_TYPE", 0x11860000, 0x3b68, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP27_INTR_BID_CLEAR", 0x11860000, 0x31bc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31b8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP31_INTR_BID_CLEAR", 0x11860000, 0x31fc, (0xffffffff << 0), (0xffffffff << 0), 0x11860000, 0x31f8, (0xffffffff << 0), (0x1 << 0) | (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_EN", 0x11860000, 0x3b44, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP2_INT_EN", 0x11860000, 0x3b64, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "WAKEUP_INT_TYPE", 0x11860000, 0x3b48, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "SYSTEM_CTRL", 0x11860000, 0x3c10, (0x1 << 14), (0x0 << 14), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_CLEAR_PEND, "GRP4_INTR_BID_CLEAR", 0x11860000, 0x304c, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3048, (0x1 << 0), (0x1 << 0) | (0x1 << 0)),
};
/* global array for supported low power modes - exposed to common logic */
 struct pmucal_lpm pmucal_lpm_list[NUM_SYS_POWERDOWN] = {
	[SYS_SICD] = {
		.id = SYS_SICD,
		.enter = enter_sicd,
		.save = save_sicd,
		.exit = exit_sicd,
		.early_wakeup = early_sicd,
		.num_enter = ARRAY_SIZE(enter_sicd),
		.num_save = ARRAY_SIZE(save_sicd),
		.num_exit = ARRAY_SIZE(exit_sicd),
		.num_early_wakeup = ARRAY_SIZE(early_sicd),
	},
	[SYS_SLEEP] = {
		.id = SYS_SLEEP,
		.enter = enter_sleep,
		.save = save_sleep,
		.exit = exit_sleep,
		.early_wakeup = early_sleep,
		.num_enter = ARRAY_SIZE(enter_sleep),
		.num_save = ARRAY_SIZE(save_sleep),
		.num_exit = ARRAY_SIZE(exit_sleep),
		.num_early_wakeup = ARRAY_SIZE(early_sleep),
	},
	[SYS_STOP] = {
		.id = SYS_STOP,
		.enter = enter_stop,
		.save = save_stop,
		.exit = exit_stop,
		.early_wakeup = early_stop,
		.num_enter = ARRAY_SIZE(enter_stop),
		.num_save = ARRAY_SIZE(save_stop),
		.num_exit = ARRAY_SIZE(exit_stop),
		.num_early_wakeup = ARRAY_SIZE(early_stop),
	},
};
unsigned int pmucal_lpm_list_size = ARRAY_SIZE(pmucal_lpm_list);
#else
/* common sequence descriptor for apm pmu init. - exposed to common logic */
struct pmucal_seq pmucal_apm_pmu_init[] = {
};
unsigned int pmucal_apm_pmu_init_size = ARRAY_SIZE(pmucal_apm_pmu_init);
struct pmucal_seq soc_sequencer_sicd_down[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "soc_sicd_down", 0, 0, 0x0, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq soc_sequencer_sicd_up[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "soc_sicd_up", 0, 0, 0x1, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq soc_sequencer_sleep_down[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "soc_sleep_down", 0, 0, 0x2, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq soc_sequencer_sleep_up[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "soc_sleep_up", 0, 0, 0x3, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq soc_sequencer_stop_down[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "soc_stop_down", 0, 0, 0x6, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq soc_sequencer_stop_up[] = {
};

struct pmucal_seq mif_sequencer_sicd_down[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "mif_sicd_down", 0, 0, 0x10, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq mif_sequencer_sicd_up[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "mif_sicd_up", 0, 0, 0x11, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq mif_sequencer_sleep_down[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "mif_sleep_down", 0, 0, 0x12, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq mif_sequencer_sleep_up[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "mif_sleep_up", 0, 0, 0x13, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq mif_sequencer_stop_down[] = {
	PMUCAL_SEQ_DESC(PMUCAL_EXT_FUNC, "mif_stop_down", 0, 0, 0x16, 0x0, 0, 0, 0, 0),
};

struct pmucal_seq mif_sequencer_stop_up[] = {
};

enum sys_powermode {
	SYS_SICD,
	SYS_SLEEP,
	SYS_STOP,
	NUM_SYS_POWERMODE,
};

struct pmucal_system_sequencer pmucal_system_apsoc_list[NUM_SYS_POWERMODE] = {
	[SYS_SICD] = {
		.id = SYS_SICD,
		.up = soc_sequencer_sicd_up,
		.down = soc_sequencer_sicd_down,
		.num_up = ARRAY_SIZE(soc_sequencer_sicd_up),
		.num_down = ARRAY_SIZE(soc_sequencer_sicd_down),
	},
	[SYS_SLEEP] = {
		.id = SYS_SLEEP,
		.up = soc_sequencer_sleep_up,
		.down = soc_sequencer_sleep_down,
		.num_up = ARRAY_SIZE(soc_sequencer_sleep_up),
		.num_down = ARRAY_SIZE(soc_sequencer_sleep_down),
	},
	[SYS_STOP] = {
		.id = SYS_STOP,
		.up = soc_sequencer_stop_up,
		.down = soc_sequencer_stop_down,
		.num_up = ARRAY_SIZE(soc_sequencer_stop_up),
		.num_down = ARRAY_SIZE(soc_sequencer_stop_down),
	},
};

struct pmucal_system_sequencer pmucal_system_mif_list[NUM_SYS_POWERMODE] = {
	[SYS_SICD] = {
		.id = SYS_SICD,
		.up = mif_sequencer_sicd_up,
		.down = mif_sequencer_sicd_down,
		.num_up = ARRAY_SIZE(mif_sequencer_sicd_up),
		.num_down = ARRAY_SIZE(mif_sequencer_sicd_down),
	},
	[SYS_SLEEP] = {
		.id = SYS_SLEEP,
		.up = mif_sequencer_sleep_up,
		.down = mif_sequencer_sleep_down,
		.num_up = ARRAY_SIZE(mif_sequencer_sleep_up),
		.num_down = ARRAY_SIZE(mif_sequencer_sleep_down),
	},
	[SYS_STOP] = {
		.id = SYS_STOP,
		.up = mif_sequencer_stop_up,
		.down = mif_sequencer_stop_down,
		.num_up = ARRAY_SIZE(mif_sequencer_stop_up),
		.num_down = ARRAY_SIZE(mif_sequencer_stop_down),
	},
};

unsigned int pmucal_system_sequencer_list_size = NUM_SYS_POWERMODE;
#endif