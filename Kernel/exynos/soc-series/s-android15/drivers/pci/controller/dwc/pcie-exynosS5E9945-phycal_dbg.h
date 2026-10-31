struct phycal_seq pci_sysfs_ch0[] = {
};

struct phycal_seq pci_save_ch0[] = {
};

struct phycal_seq pci_restore_ch0[] = {
};

struct phycal_seq pci_l1ss_en_ch0[] = {
};

struct phycal_seq pci_l1ss_dis_ch0[] = {
};

struct phycal_seq pci_poweron_ch0[] = {
};

struct phycal_seq pci_poweroff_ch0[] = {
};

struct phycal_seq pci_suspend_ch0[] = {
};

struct phycal_seq pci_resume_ch0[] = {
};

struct phycal_seq pci_msi_hndlr_ch0[] = {
};

struct phycal_seq pci_reg_dump_ch0[] = {
};

struct phycal_seq pci_history_buff_en_ch0[] = {
	PHYCAL_SEQ_DESC(PHYCAL_EXT_CLR_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0x1 << 31), (0x0 << 31), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_SIGNAL_MASK", 0x181b0000, 0x9004, (0xffffffff << 0), (0xffffcff7 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_SET_BIT, "PCIE_DTB_SEL", 0x181b0000, 0x8014, (0xffffffff << 0), (0xffffffff << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_CLR_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0xf << 8), (0x0 << 8), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_CLR_BIT, "SII_CONTROL_PCIe0_GEN_CTRL_3", 0x18140000, 0x1058, (0xf << 28), (0x0 << 28), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_CLR_BIT, "SII_CONTROL_PCIe0_GEN_CTRL_3", 0x18140000, 0x1058, (0xf << 24), (0x0 << 24), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "SII_CONTROL_PCIe0_DBG_SIGNAL_1", 0x18140000, 0x1000, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "SII_CONTROL_PCIe0_DBG_SIGNAL_1", 0x18140000, 0x1000, (0xffffffff << 0), (0x79782221 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_READ, "SII_CONTROL_PCIe0_DBG_SIGNAL_1", 0x18140000, 0x1000, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_CLR_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0xf << 4), (0x0 << 4), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_SET_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0x0 << 5), (0x0 << 5), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_SET_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0x1 << 13), (0x1 << 13), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_SET_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0x1 << 15), (0x1 << 15), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_SET_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0x1 << 12), (0x1 << 12), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0x1 << 31), (0x1 << 31), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
};

struct phycal_seq pci_history_buff_dis_ch0[] = {
	PHYCAL_SEQ_DESC(PHYCAL_EXT_CLR_BIT, "PCIE_DEBUG_MODE", 0x181b0000, 0x9000, (0x1 << 31), (0x0 << 31), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
};

struct phycal_seq pci_sysfs_ch1[] = {
};

struct phycal_seq pci_save_ch1[] = {
};

struct phycal_seq pci_restore_ch1[] = {
};

struct phycal_seq pci_l1ss_en_ch1[] = {
};

struct phycal_seq pci_l1ss_dis_ch1[] = {
};

struct phycal_seq pci_poweron_ch1[] = {
};

struct phycal_seq pci_poweroff_ch1[] = {
};

struct phycal_seq pci_suspend_ch1[] = {
};

struct phycal_seq pci_resume_ch1[] = {
};

struct phycal_seq pci_msi_hndlr_ch1[] = {
};

struct phycal_seq pci_reg_dump_ch1[] = {
};

struct phycal_seq pci_history_buff_en_ch1[] = {
};

struct phycal_seq pci_history_buff_dis_ch1[] = {
};
