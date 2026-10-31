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
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0x1 << 24), (0x1 << 24), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_DELAY, "delay", 0, 0, 0, 0x0, 0, 0, 0, 0, EP_TYPE_ALL, 0, 30, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0x1 << 24), (0x0 << 24), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0x1 << 31), (0x0 << 31), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_SIGNAL_MASK", 0x191b0000, 0x9004, (0xffffffff << 0), (0x0 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_SIGNAL_MASK", 0x191b0000, 0x9004, (0xffffffff << 0), (0xfff0cc7f << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0x1 << 15), (0x1 << 15), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0x1 << 12), (0x1 << 12), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0x1 << 13), (0x1 << 13), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0x1 << 31), (0x1 << 31), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DTB_SEL", 0x191b0000, 0x8014, (0xf << 4), (0x8 << 4), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "TRACE_SIG_CTRL_TRACE_SIG_ESSENTIAL_SEL0", 0x19130000, 0xc408, (0xff << 0), (0x9 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "TRACE_SIG_CTRL_TRACE_SIG_DYNAMIC_SEL0", 0x19130000, 0xc418, (0x1ff << 0), (0x147 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "TRACE_SIG_CTRL_TRACE_SIG_DYNAMIC_SEL0", 0x19130000, 0xc418, (0x1ff << 16), (0x42 << 16), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "TRACE_SIG_CTRL_TRACE_SIG_DYNAMIC_SEL1", 0x19130000, 0xc41c, (0x1ff << 0), (0x2 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "TRACE_SIG_CTRL_TRACE_SIG_DYNAMIC_SEL1", 0x19130000, 0xc41c, (0x1ff << 16), (0x3 << 16), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "TRACE_SIG_CTRL_TRACE_SIG_ESSENTIAL_SEL0", 0x19130000, 0xc408, (0xffffffff << 0), (0x8 << 0), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
	PHYCAL_SEQ_DESC(PHYCAL_EXT_WRITE, "PCIE_DEBUG_MODE", 0x191b0000, 0x9000, (0xf << 8), (0x1 << 8), 0, 0, 0xffffffff, 0, EP_TYPE_ALL, 0, 0, "NULL"),
};

struct phycal_seq pci_history_buff_dis_ch0[] = {
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

