#include <linux/sizes.h>

#if IS_ENABLED(CONFIG_SOC_S5E9935)
#include "pcie-exynosS5E9935-phycal.h"
#elif IS_ENABLED(CONFIG_SOC_S5E9945)
#include "pcie-exynosS5E9945-phycal.h"
#include "pcie-exynosS5E9945-phycal_dbg.h"
#elif IS_ENABLED(CONFIG_SOC_S5E9955)
#include "pcie-exynosS5E9955-phycal.h"
#include "pcie-exynosS5E9955-phycal_dbg.h"
#else
#error "There is no PHYCAL for specific SOC"
#endif
struct exynos_pcie_phycal pcical_list[] = {
	{
		.pwrdn = pciphy_pwrdn_ch0,
		.pwrdn_clr = pciphy_pwrdn_clr_ch0,
		.config = pciphy_config_ch0,
		.ia0 = pciphy_ch0_ia0,
		.ia1 = pciphy_ch0_ia1,
		.ia2 = pciphy_ch0_ia2,
		.pwrdn_size = ARRAY_SIZE(pciphy_pwrdn_ch0),
		.pwrdn_clr_size = ARRAY_SIZE(pciphy_pwrdn_clr_ch0),
		.config_size = ARRAY_SIZE(pciphy_config_ch0),
		.ia0_size = ARRAY_SIZE(pciphy_ch0_ia0),
		.ia1_size = ARRAY_SIZE(pciphy_ch0_ia1),
		.ia2_size = ARRAY_SIZE(pciphy_ch0_ia2),
	}, {
		.pwrdn = pciphy_pwrdn_ch1,
		.pwrdn_clr = pciphy_pwrdn_clr_ch1,
		.config = pciphy_config_ch1,
		.ia0 = pciphy_ch1_ia0,
		.ia1 = pciphy_ch1_ia1,
		.ia2 = pciphy_ch1_ia2,
		.pwrdn_size = ARRAY_SIZE(pciphy_pwrdn_ch1),
		.pwrdn_clr_size = ARRAY_SIZE(pciphy_pwrdn_clr_ch1),
		.config_size = ARRAY_SIZE(pciphy_config_ch1),
		.ia0_size = ARRAY_SIZE(pciphy_ch1_ia0),
		.ia1_size = ARRAY_SIZE(pciphy_ch1_ia1),
		.ia2_size = ARRAY_SIZE(pciphy_ch1_ia2),
	},
};

char *exynos_pcie_phycal_revinfo = phycal_revinfo;
struct exynos_pcie_phycal_dbg pcical_dbg_list[] = {
	{
		.dbg_seq[SYSFS] = pci_sysfs_ch0,
		.dbg_seq[SAVE] = pci_save_ch0,
		.dbg_seq[RESTORE] = pci_restore_ch0,
		.dbg_seq[L1SS_EN] = pci_l1ss_en_ch0,
		.dbg_seq[L1SS_DIS] = pci_l1ss_dis_ch0,
		.dbg_seq[PWRON] = pci_poweron_ch0,
		.dbg_seq[PWROFF] = pci_poweroff_ch0,
		.dbg_seq[SUSPEND] = pci_suspend_ch0,
		.dbg_seq[RESUME] = pci_resume_ch0,
		.dbg_seq[MSI_HNDLR] = pci_msi_hndlr_ch0,
		.dbg_seq[REG_DUMP] = pci_reg_dump_ch0,
		.dbg_seq[HISTORY_EN] = pci_history_buff_en_ch0,
		.dbg_seq[HISTORY_DIS] = pci_history_buff_dis_ch0,
		.seq_size[SYSFS] = ARRAY_SIZE(pci_sysfs_ch0),
		.seq_size[SAVE] = ARRAY_SIZE(pci_save_ch0),
		.seq_size[RESTORE] = ARRAY_SIZE(pci_restore_ch0),
		.seq_size[L1SS_EN] = ARRAY_SIZE(pci_l1ss_en_ch0),
		.seq_size[L1SS_DIS] = ARRAY_SIZE(pci_l1ss_dis_ch0),
		.seq_size[PWRON] = ARRAY_SIZE(pci_poweron_ch0),
		.seq_size[PWROFF] = ARRAY_SIZE(pci_poweroff_ch0),
		.seq_size[SUSPEND] = ARRAY_SIZE(pci_suspend_ch0),
		.seq_size[RESUME] = ARRAY_SIZE(pci_resume_ch0),
		.seq_size[MSI_HNDLR] = ARRAY_SIZE(pci_msi_hndlr_ch0),
		.seq_size[REG_DUMP] = ARRAY_SIZE(pci_reg_dump_ch0),
		.seq_size[HISTORY_EN] = ARRAY_SIZE(pci_history_buff_en_ch0),
		.seq_size[HISTORY_DIS] = ARRAY_SIZE(pci_history_buff_dis_ch0),
	},
	{
		.dbg_seq[SYSFS] = pci_sysfs_ch1,
		.dbg_seq[SAVE] = pci_save_ch1,
		.dbg_seq[RESTORE] = pci_restore_ch1,
		.dbg_seq[L1SS_EN] = pci_l1ss_en_ch1,
		.dbg_seq[L1SS_DIS] = pci_l1ss_dis_ch1,
		.dbg_seq[PWRON] = pci_poweron_ch1,
		.dbg_seq[PWROFF] = pci_poweroff_ch1,
		.dbg_seq[SUSPEND] = pci_suspend_ch1,
		.dbg_seq[RESUME] = pci_resume_ch1,
		.dbg_seq[MSI_HNDLR] = pci_msi_hndlr_ch1,
		.dbg_seq[REG_DUMP] = pci_reg_dump_ch1,
		.dbg_seq[HISTORY_EN] = pci_history_buff_en_ch1,
		.dbg_seq[HISTORY_DIS] = pci_history_buff_dis_ch1,
		.seq_size[SYSFS] = ARRAY_SIZE(pci_sysfs_ch1),
		.seq_size[SAVE] = ARRAY_SIZE(pci_save_ch1),
		.seq_size[RESTORE] = ARRAY_SIZE(pci_restore_ch1),
		.seq_size[L1SS_EN] = ARRAY_SIZE(pci_l1ss_en_ch1),
		.seq_size[L1SS_DIS] = ARRAY_SIZE(pci_l1ss_dis_ch1),
		.seq_size[PWRON] = ARRAY_SIZE(pci_poweron_ch1),
		.seq_size[PWROFF] = ARRAY_SIZE(pci_poweroff_ch1),
		.seq_size[SUSPEND] = ARRAY_SIZE(pci_suspend_ch1),
		.seq_size[RESUME] = ARRAY_SIZE(pci_resume_ch1),
		.seq_size[MSI_HNDLR] = ARRAY_SIZE(pci_msi_hndlr_ch1),
		.seq_size[REG_DUMP] = ARRAY_SIZE(pci_reg_dump_ch1),
		.seq_size[HISTORY_EN] = ARRAY_SIZE(pci_history_buff_en_ch1),
		.seq_size[HISTORY_DIS] = ARRAY_SIZE(pci_history_buff_dis_ch1),
	},
};
