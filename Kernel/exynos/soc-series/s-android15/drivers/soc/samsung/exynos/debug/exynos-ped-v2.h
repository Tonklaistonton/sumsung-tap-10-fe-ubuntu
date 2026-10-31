#if defined(CONFIG_EXYNOS_PD) || defined(CONFIG_EXYNOS_PD_MODULE)
#include <soc/samsung/exynos-pd.h>
#endif

/* LH Group 0 */
#define LH_INTC0_IEN_SET				(0x0288)
#define LH_INTC0_IEN_CLR				(0x028C)
#define LH_INTC0_IPEND					(0x0290)
#define LH_INTC0_IPRIO_IPEND				(0x0294)
#define LH_INTC0_IPRIORIY(x)				(0x0298 + 4*(x))
#define LH_INTC0_IPRIO_SECURE_PEND			(0x02A8)
#define LH_INTC0_IPRIO_NONSECURE_PEND			(0x02AC)
/* LH Group 1 */
#define LH_INTC1_IEN_SET				(0x02B8)
#define LH_INTC1_IEN_CLR				(0x02BC)
#define LH_INTC1_IPEND					(0x02C0)
#define LH_INTC1_IPRIO_IPEND				(0x02C4)
#define LH_INTC1_IPRIORITY(x)				(0x02C8 + 4*(x))
#define LH_INTC1_IPRIO_SECURE_PEND			(0x02D8)
#define LH_INTC1_IPRIO_NONSECURE_PEND			(0x02DC)

#define LH_INT_CLR					(0x0400)
#define LH_FAULT_INJECT					(0x0404)
#define LH_END_OF_REGS					(LH_FAULT_INJECT)

#define MAX_GROUP_ID				(2)
#define MAX_PORT_ID				(32)

#define TREX_PARITY_CTRL				(0xD000)
#define TREX_PARITY_INTERRUPT				(0xD004)
#define TREX_PARITY_VECTOR_0				(0xD008)
#define TREX_PARITY_VECTOR_1				(0xD00C)
#define TREX_END_OF_REGS				(TREX_PARITY_VECTOR_1)

struct ped_platdata {
	int num_nodegroup;
	struct lh_nodegroup *lh_group;
	bool probed;
};

struct ped_dev {
	struct device *dev;
	struct ped_platdata *pdata;
	u32 irq;
	spinlock_t ctrl_lock;
	bool suspend_state;
};

struct lh_nodegroup {
	char name[16];
	u64 phy_regs;
	int num_group0;
	int num_group1;
	u32 group0_mask;
	u32 group1_mask;
	u32 group0_sicd_mask;
	u32 group1_sicd_mask;
	char pd_name[16];
	bool pd_support;
#if defined(CONFIG_EXYNOS_PD) || defined(CONFIG_EXYNOS_PD_MODULE)
	struct exynos_pm_domain *pd_domain;
#else
	void *dummy;
#endif
	void __iomem *regs;
} __packed;

char* lh_aggrigator_list[] = {
	"ALIVE", "AUD", "BYRP", "CHUBVTS", "CHUBVTS",
	"CMGP", "CPUCL0", "CSIS", "CSIS_ALIVE", "DLFE",
	"DNC", "DOF", "DPUB", "DPUF0", "DPUF1",
	"DSP", "G3D", "GNPU0", "GNPU1", "GNSS",
	"HSI0",	"HSI1", "ICPU", "LME", "M2M",
	"MCSC",	"MFC", "MFD", "MIF0", "MIF1",
	"MIF2", "MIF3",	"MLSC", "CPALV_BUS", "CPCPU_BUS",
	"MSNR", "MTNR", "NOCL0", "NOCL1", "NOCL2A",
	"NOCL2B", "NPUMEM_0", "NPUMEM_1", "PERIC0", "PERIC1",
	"PERIC2", "PERIS", "PSP", "RGBP", "SDMA",
	"SNPU0", "SNPU1", "UFD", "UFS", "UNPU",
	"YUVP", "TREX_G3D", "TREX_NOCL0", "TREX_NOCL1", "TREX_NOCL2A",
	"TREX_NOCL2B", "TREX_SDMA",
};

struct lh_nodegroup nodegroup_list[] = {
	{"ALIVE",0x13910000, 32, 3, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"AUD", 0x13090000, 10, 0, 0, 0, 0, 0, "pd_aud", 0, NULL, NULL},
	{"BYRP", 0x23030000, 7, 0, 0, 0, 0, 0, "pd_byrp", 1, NULL, NULL},
	{"CHUBVTS", 0x14E90000, 12, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"CMGP", 0x150B0000, 3, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"CPUCL0", 0x2FEC0000, 32, 7, 0, 0, 0xffffffff, 0x7f,"no_pd", 0, NULL, NULL},
	{"CSIS_ALIVE", 0x23830000, 3, 0, 0, 0, 0, 0, "pd_aoccsis", 1, NULL, NULL},
	{"CSIS", 0x23930000, 14, 0, 0, 0, 0, 0, "pd_csis", 1, NULL, NULL},
	{"DLFE", 0x24050000, 5, 0, 0, 0, 0, 0, "pd_dlfe", 1, NULL, NULL},
	{"DNC", 0x212F0000, 31, 0, 0, 0, 0, 0, "pd_dnc", 1, NULL, NULL},
	{"DOF", 0x1F030000, 3, 0, 0, 0, 0, 0, "pd_dof", 1, NULL, NULL},
	{"DPUB", 0x1B0F0000, 24, 0, 0, 0, 0, 0, "pd_dpub", 1, NULL, NULL},
	{"DPUF0", 0x1B8F0000, 10, 0, 0, 0, 0, 0, "pd_dpuf0", 1, NULL, NULL},
	{"DPUF1", 0x1BAF0000, 5, 0, 0, 0, 0, 0, "pd_dpuf1", 1, NULL, NULL},
	{"DSP", 0x215C0000, 7, 0, 0, 0, 0, 0, "pd_dsp", 1, NULL, NULL},
	{"G3D", 0x22090000, 20, 0, 0, 0, 0, 0, "pd_g3d", 1, NULL, NULL},
	{"GNPU0", 0x216F0000, 21, 0, 0, 0, 0, 0, "pd_gnpu0", 1, NULL, NULL},
	{"GNPU1", 0x217F0000, 21, 0, 0, 0, 0, 0, "pd_gnpu1", 1, NULL, NULL},
	//{"GNSS", 0x171A0000, 2, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"HSI0", 0x18850000, 4, 0, 0, 0, 0, 0, "pd_hsi0", 1, NULL, NULL},
	{"HSI1", 0x190D0000, 7, 0, 0, 0, 0, 0, "pd_hsi1", 1, NULL, NULL},
	{"ICPU", 0x24850000, 9, 0, 0, 0, 0, 0, "pd_icpu", 1, NULL, NULL},
	{"LME", 0x1D0F0000, 5, 0, 0, 0, 0, 0, "pd_lme", 1, NULL, NULL},
	{"M2M", 0x1D920000, 12, 0, 0, 0, 0, 0, "pd_m2m", 1, NULL, NULL},
	{"MCSC", 0x258A0000, 8, 0, 0, 0, 0, 0, "pd_mcsc", 1, NULL, NULL},
	{"MFC", 0x1E100000, 14, 0, 0, 0, 0, 0, "pd_mfc", 1, NULL, NULL},
	{"MFD", 0x1E900000, 9, 0, 0, 0, 0, 0, "pd_mfd", 1, NULL, NULL},
	{"MIF0", 0x2D090000, 4, 0, 0x2, 0, 0xffff, 0, "no_pd", 0, NULL, NULL},
	{"MIF1", 0x2D290000, 4, 0, 0x2, 0, 0xffff, 0, "no_pd", 0, NULL, NULL},
	{"MIF2", 0x2D490000, 4, 0, 0x2, 0, 0xffff, 0, "no_pd", 0, NULL, NULL},
	{"MIF3", 0x2D690000, 4, 0, 0x2, 0, 0xffff, 0, "no_pd", 0, NULL, NULL},
	{"MLSC", 0x278F0000, 8, 0, 0, 0, 0, 0, "pd_mlsc", 1, NULL, NULL},
	{"MSNR", 0x26050000, 9, 0, 0, 0, 0, 0, "pd_msnr", 1, NULL, NULL},
	{"MTNR", 0x26830000, 8, 0, 0, 0, 0, 0, "pd_mtnr", 1, NULL, NULL},
	{"NOCL0", 0x2A3F0000, 30, 0, 0, 0, 0x3fffffff, 0, "no_pd", 0, NULL, NULL},
	{"NOCL1_0", 0x2B0D0000, 32, 32, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"NOCL1_1", 0x2B190000, 26, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"NOCL2A_0", 0x2B880000, 32, 32, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"NOCL2A_1", 0x2B890000, 3, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"NOCL2B", 0x2C050000, 27, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"NPUMEM_0", 0x21A50000, 32, 32, 0, 0, 0, 0, "pd_npumem", 1, NULL, NULL},
	{"NPUMEM_1", 0x21A60000, 25, 0, 0, 0, 0, 0, "pd_npumem", 1, NULL, NULL},
	{"PERIC0", 0x10960000, 2, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"PERIC1", 0x11140000, 2, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"PERIC2", 0x11840000, 3, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"PERIS", 0x100F0000, 5, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"PSP", 0x17880000, 5, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"RGBP", 0x25030000, 8, 0, 0, 0, 0, 0, "pd_rgbp", 1, NULL, NULL},
	{"SDMA", 0x213C0000, 24, 0, 0, 0, 0, 0, "pd_sdma", 1, NULL, NULL},
	{"SNPU0", 0x218F0000, 21, 0, 0, 0, 0, 0, "pd_snpu0", 1, NULL, NULL},
	{"SNPU1", 0x219F0000, 21, 0, 0, 0, 0, 0, "pd_snpu1", 1, NULL, NULL},
	{"UFD", 0x15980000, 5, 0, 0, 0, 0, 0, "pd_ufd", 1, NULL, NULL},
	{"UFS", 0x18090000, 3, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"UNPU", 0x16050000, 7, 0, 0, 0, 0, 0, "pd_unpu", 1, NULL, NULL},
	{"YUVP", 0x288C0000, 7, 0, 0, 0, 0, 0, "pd_yuvp", 1, NULL, NULL},
	//{"CPALV_BUS", 0x2E720000, 5, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	//{"CPCPU_BUS", 0x2E1B0000, 5, 0, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
	{"PARITY_INT_COMB", 0x100B0000, 32, 30, 0, 0, 0, 0, "no_pd", 0, NULL, NULL},
};
