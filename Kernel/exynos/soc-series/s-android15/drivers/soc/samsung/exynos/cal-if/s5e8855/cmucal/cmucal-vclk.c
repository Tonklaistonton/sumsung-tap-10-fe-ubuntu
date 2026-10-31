#include "../../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_VDDI[] = {
    TOP_PLL_MMC,
    TOP_MUX_CLKCMU_AUD_NOC,
    TOP_MUX_CLKCMU_CPUCL0_SWITCH,
    TOP_MUX_CLKCMU_CPUCL0_NOCP,
    TOP_MUX_CLKCMU_DSU_SWITCH,
    TOP_MUX_CLKCMU_CPUCL1_SWITCH,
    TOP_MUX_CLKCMU_CSIS_NOC,
    TOP_MUX_CLKCMU_CSTAT_NOC,
    TOP_MUX_CLKCMU_CSTAT_BYRP,
    TOP_MUX_CLKCMU_DPU_NOC,
    TOP_MUX_CLKCMU_G3DCORE_SWITCH,
    TOP_MUX_CLKCMU_HSI_NOC,
    TOP_MUX_CLKCMU_M2M_NOC,
    TOP_MUX_CLKCMU_M2M_JPEG,
    TOP_MUX_CLKCMU_M2M_GDC,
    TOP_MUX_CLKCMU_MIF1_NOCP,
    TOP_MUX_CLKCMU_MFC_NOC,
    TOP_MUX_CLKCMU_MIF0_NOCP,
    TOP_MUX_CLKCMU_NOCL0_NOC,
    TOP_MUX_CLKCMU_NOCL1A_NOC,
    TOP_MUX_CLKCMU_PERIC_NOC,
    TOP_MUX_CLKCMU_RGBP_NOC,
    TOP_MUX_CLKCMU_YUVP_NOC,
    TOP_MUX_CLKCMU_CSIS_DCPHY,
    TOP_MUX_CLKCMU_CSIS_OIS,
    TOP_MUX_CLKCMU_ICPU_NOCD_0,
    TOP_MUX_CLKCMU_ICPU_NOCD_1,
    TOP_MUX_CLKCMU_GNPU0_NOC,
    TOP_MUX_CLKCMU_GNPU0_XMAA,
    TOP_MUX_CLKCMU_SDMA_NOC,
    TOP_MUX_CLKCMU_DNC_HTU,
    TOP_MUX_CLKCMU_DNC_NOC,
    TOP_MUX_CLKCMU_PERIS_GIC,
    TOP_MUX_CLKCMU_PERIS_NOC,
    TOP_MUX_CLKCMU_USI_NOC,
    TOP_MUX_CLKCMU_CPUCL0_DBG_NOC,
    TOP_MUX_CLKCMU_NOCL1B_NOC,
    TOP_MUX_CLKCMU_CPUCL2_SWITCH,
    TOP_MUX_CLKCMU_CSTAT_MCSC,
    TOP_DIV_CLKCMU_AUD_NOC,
    TOP_DIV_CLKCMU_CPUCL0_SWITCH,
    TOP_DIV_CLKCMU_CPUCL0_NOCP,
    TOP_DIV_CLKCMU_DSU_SWITCH,
    TOP_DIV_CLKCMU_CPUCL1_SWITCH,
    TOP_DIV_CLKCMU_CIS_CLK0,
    TOP_DIV_CLKCMU_CIS_CLK1,
    TOP_DIV_CLKCMU_CIS_CLK2,
    TOP_DIV_CLKCMU_CIS_CLK3,
    TOP_DIV_CLKCMU_CIS_CLK4,
    TOP_DIV_CLKCMU_CSIS_NOC,
    TOP_DIV_CLKCMU_CMU_BOOST_MIF0,
    TOP_DIV_CLKCMU_CSTAT_NOC,
    TOP_DIV_CLKCMU_CSTAT_BYRP,
    TOP_DIV_CLKCMU_DPU_NOC,
    TOP_DIV_CLKCMU_HSI_NOC,
    TOP_DIV_CLKCMU_M2M_NOC,
    TOP_DIV_CLKCMU_M2M_JPEG,
    TOP_DIV_CLKCMU_M2M_GDC,
    TOP_DIV_CLKCMU_MIF1_NOCP,
    TOP_DIV_CLKCMU_MFC_NOC,
    TOP_DIV_CLKCMU_MIF0_NOCP,
    TOP_DIV_CLKCMU_NOCL0_NOC,
    TOP_DIV_CLKCMU_NOCL1A_NOC,
    TOP_DIV_CLKCMU_PERIC_NOC,
    TOP_DIV_CLKCMU_PERIC_MMC_CARD,
    TOP_DIV_CLKCMU_RGBP_NOC,
    TOP_DIV_CLKCMU_USB_NOC,
    TOP_DIV_CLKCMU_YUVP_NOC,
    TOP_DIV_CLKCMU_CSIS_DCPHY,
    TOP_DIV_CLKCMU_CSIS_OIS,
    TOP_DIV_CLKCMU_ICPU_NOCD_0,
    TOP_DIV_CLKCMU_ICPU_NOCD_1,
    TOP_DIV_CLKCMU_GNPU0_NOC,
    TOP_DIV_CLKCMU_GNPU0_XMAA,
    TOP_DIV_CLKCMU_SDMA_NOC,
    TOP_DIV_CLKCMU_DNC_HTU,
    TOP_DIV_CLKCMU_DNC_NOC,
    TOP_DIV_CLKCMU_PSP_NOC,
    TOP_DIV_CLKCMU_PERIS_GIC,
    TOP_DIV_CLKCMU_PERIS_NOC,
    TOP_DIV_CLKCMU_USI_NOC,
    TOP_DIV_CLKCMU_CPUCL0_DBG_NOC,
    TOP_DIV_CLKCMU_NOCL1B_NOC,
    TOP_DIV_CLKCMU_CPUCL2_SWITCH,
    TOP_DIV_CLKCMU_CSTAT_MCSC,
    TOP_DIV_CLKCMU_G3D_HTU,
    NOCL0_DIV_CLK_NOCL0_NOCP,
    NOCL1A_DIV_CLK_NOCL1A_NOCP,
    DPU_DIV_CLK_DPU_NOCP,
    PSP_DIV_CLK_PSP_NOCP,
    MFC_DIV_CLK_MFC_NOCP,
    M2M_DIV_CLK_M2M_NOCP,
    PERIC_MUX_CLK_PERIC_I2C,
    PERIC_MUX_CLK_PERIC_UART_DBG,
    PERIC_MUX_CLK_PERIC_USI00,
    PERIC_MUX_CLK_PERIC_USI01,
    PERIC_MUX_CLK_PERIC_USI02,
    PERIC_MUX_CLK_PERIC_USI03,
    PERIC_MUX_CLK_PERIC_USI04,
    PERIC_MUX_CLK_PERIC_USI09_USI_OIS,
    PERIC_MUX_CLK_PERIC_USI10_USI_OIS,
    PERIS_DIV_CLK_PERIS_NOCP,
    PERIS_DIV_CLK_PERIS_OTP,
    USI_MUX_CLK_USI_I2C,
    USI_MUX_CLK_USI_USI06,
    USI_MUX_CLK_USI_USI07,
};
enum clk_id cmucal_VDD_ALIVE[] = {
    ALIVE_MUX_CLKCMU_ALIVE_BOOST,
    ALIVE_MUX_CLKCMU_CHUBVTS_NOC,
    ALIVE_MUX_CLKCMU_CMGP_NOC,
    ALIVE_MUX_CLKCMU_CMGP_PERI,
    ALIVE_MUX_CLKCMU_DBGCORE_NOC,
    ALIVE_MUX_CLK_ALIVE_DBGCORE_UART,
    ALIVE_MUX_CLK_ALIVE_I2C,
    ALIVE_MUX_CLK_ALIVE_NOC,
    ALIVE_MUX_CLK_ALIVE_TIMER_USER,
    ALIVE_MUX_CLK_ALIVE_USI0,
    ALIVE_CLKDIV__DIV_CLK_ALIVE_DBGCORE_UART,
    ALIVE_DIV_CLKCMU_ALIVE_BOOST,
    ALIVE_DIV_CLK_ALIVE_CMGP_NOC,
    ALIVE_DIV_CLK_ALIVE_DBGCORE_NOC,
    ALIVE_DIV_CLK_ALIVE_I2C,
    ALIVE_DIV_CLK_ALIVE_NOC,
    ALIVE_DIV_CLK_ALIVE_SPMI,
    CMGP_MUX_CLK_CMGP_I2C,
    CMGP_MUX_CLK_CMGP_I3C,
    CMGP_MUX_CLK_CMGP_NOC,
    CMGP_MUX_CLK_CMGP_USI0,
    CMGP_MUX_CLK_CMGP_USI1,
    CMGP_MUX_CLK_CMGP_USI2,
    CMGP_MUX_CLK_CMGP_USI3,
    CMGP_MUX_CLK_CMGP_USI4,
    CMGP_DIV_CLK_CMGP_USI_I2C,
    CMGP_DIV_CLK_CMGP_USI_I3C,
    USB_DIV_CLK_USB_NOC_DIV3,
};
enum clk_id cmucal_VDD_MIF[] = {
    MIF0_PLL_MIF_MAIN,
    MIF0_PLL_MIF_SUB,
    MIF0_DIV_CLK_MIF_NOCD,
    MIF1_PLL_MIF_MAIN,
    MIF1_PLL_MIF_SUB,
    MIF1_DIV_CLK_MIF_NOCD,
    S2D_DIV_CLK_MIF_NOCD_S2D,
};
enum clk_id cmucal_VDD_CAM[] = {
    CSIS_DIV_CLK_CSIS_NOCP,
    RGBP_DIV_CLK_RGBP_NOCP,
    CSTAT_DIV_CLK_CSTAT_NOCP,
    YUVP_DIV_CLK_YUVP_NOCP,
    NOCL1B_DIV_CLK_NOCL1B_NOCP,
};
enum clk_id cmucal_VDD_CHUBVTS[] = {
    CHUBVTS_MUX_CLK_CHUBVTS_NOC,
    CHUBVTS_DIV_CLK_CHUBVTS_NOC,
};
enum clk_id cmucal_VDD_NPU[] = {
    GNPU0_DIV_CLK_GNPU0_NOCP,
    SDMA_DIV_CLK_SDMA_NOCP,
    DNC_DIV_CLK_DNC_NOCP,
};
enum clk_id cmucal_VDD_G3D[] = {
    G3DCORE_RPLL_CTRL_PLL_G3D,
};
enum clk_id cmucal_VDD_CPUCL0[] = {
    CPUCL0_RPLL_CTRL_PLL_CPUCL0,
    DSU_RPLL_CTRL_PLL_DSU,
};
enum clk_id cmucal_VDD_CPUCL1[] = {
    CPUCL1_PLL_CTRL_PLL_CPUCL1,
    CPUCL1_DIV_CLK_CPUCL1_ATB,
};
enum clk_id cmucal_VDD_AUD[] = {
    AUD_PLL_AUD,
    AUD_MUX_CLK_AUD_AUDIF,
    AUD_MUX_CLK_AUD_CPU,
    AUD_MUX_CLK_AUD_MISC,
    AUD_MUX_CLK_AUD_NOC,
    AUD_MUX_CLK_AUD_PCMC,
    AUD_DIV_CLK_AUD_ACLK,
    AUD_DIV_CLK_AUD_AUDIF,
    AUD_DIV_CLK_AUD_CNT,
    AUD_DIV_CLK_AUD_CPU,
    AUD_DIV_CLK_AUD_CPU_ACP,
    AUD_DIV_CLK_AUD_CPU_PCLKDBG,
    AUD_DIV_CLK_AUD_DSIF,
    AUD_DIV_CLK_AUD_FM,
    AUD_DIV_CLK_AUD_FM_SPDY,
    AUD_DIV_CLK_AUD_MCLK,
    AUD_DIV_CLK_AUD_NOCP,
    AUD_DIV_CLK_AUD_PCMC,
    AUD_DIV_CLK_AUD_SERIAL_LIF,
    AUD_DIV_CLK_AUD_UAIF0,
    AUD_DIV_CLK_AUD_UAIF1,
    AUD_DIV_CLK_AUD_UAIF2,
    AUD_DIV_CLK_AUD_UAIF3,
    AUD_DIV_CLK_AUD_UAIF4,
    AUD_DIV_CLK_AUD_UAIF5,
    AUD_DIV_CLK_AUD_UAIF6,
};
enum clk_id cmucal_VDD_ICPU_CSILINK[] = {
    ICPU_DIV_CLK_ICPU_NOCP,
    ICPU_DIV_CLK_ICPU_PCLKDBG,
};
enum clk_id cmucal_VDD_CPUCL2[] = {
    CPUCL2_PLL_CTRL_PLL_CPUCL2,
    CPUCL2_DIV_CLK_CPUCL2_ATB,
};

/* SPECIAL VCLK -> Clock Node List */

/*CMGP*/
enum clk_id cmgp_usi0_ipclkport_clk[] = {
	CMGP_DIV_CLK_CMGP_USI00_USI,
	CMGP_MUX_CLK_CMGP_USI0,
};

enum clk_id cmgp_usi1_ipclkport_clk[] = {
	CMGP_DIV_CLK_CMGP_USI01_USI,
	CMGP_MUX_CLK_CMGP_USI1,
};

enum clk_id cmgp_usi2_ipclkport_clk[] = {
	CMGP_DIV_CLK_CMGP_USI02_USI,
	CMGP_MUX_CLK_CMGP_USI2,
};

enum clk_id cmgp_usi3_ipclkport_clk[] = {
	CMGP_DIV_CLK_CMGP_USI03_USI,
	CMGP_MUX_CLK_CMGP_USI3,
};

enum clk_id cmgp_usi4_ipclkport_clk[] = {
	CMGP_DIV_CLK_CMGP_USI04_USI,
	CMGP_MUX_CLK_CMGP_USI4,
};

enum clk_id cmgp_i2c_ipclkport_clk[] = {
	CMGP_DIV_CLK_CMGP_USI_I2C,
	CMGP_MUX_CLK_CMGP_I2C,
};

enum clk_id cmgp_i3c_ipclkport_clk[] = {
	CMGP_DIV_CLK_CMGP_USI_I3C,
	CMGP_MUX_CLK_CMGP_I3C,
};
/*PERIC*/

enum clk_id peric_usi00_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI00_USI,
	PERIC_MUX_CLK_PERIC_USI00,
};

enum clk_id peric_usi01_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI01_USI,
	PERIC_MUX_CLK_PERIC_USI01,
};

enum clk_id peric_usi02_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI02_USI,
	PERIC_MUX_CLK_PERIC_USI02,
};

enum clk_id peric_usi03_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI03_USI,
	PERIC_MUX_CLK_PERIC_USI03,
};

enum clk_id peric_usi04_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI04_USI,
	PERIC_MUX_CLK_PERIC_USI04,
};

enum clk_id peric_usi09_ois_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI09_USI_OIS,
	PERIC_MUX_CLK_PERIC_USI09_USI_OIS,
};

enum clk_id peric_usi10_ois_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI10_USI_OIS,
	PERIC_MUX_CLK_PERIC_USI10_USI_OIS,
};

enum clk_id peric_usi_i2c_ipclkport_clk[] = {
	PERIC_DIV_CLK_PERIC_USI_I2C,
	PERIC_MUX_CLK_PERIC_I2C,
};

/*USI*/

enum clk_id usi_usi06_ipclkport_clk[] = {
	USI_DIV_CLK_USI_USI06_USI,
	USI_MUX_CLK_USI_USI06,
};

enum clk_id usi_usi07_ipclkport_clk[] = {
	USI_DIV_CLK_USI_USI07_USI,
	USI_MUX_CLK_USI_USI07,
};

enum clk_id usi_usi_i2c_ipclkport_clk[] = {
	USI_DIV_CLK_USI_USI_I2C,
	USI_MUX_CLK_USI_I2C,
};
/* GATE VCLK -> Clock Node List */

/* SPECIAL VCLK -> LUT List */

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_VDDI_lut[] = {
    { 808000, cmucal_VDDI_SOD_lut_params },
    { 808000, cmucal_VDDI_OD_lut_params },
    { 808000, cmucal_VDDI_NM_lut_params },
    { 808000, cmucal_VDDI_UD_lut_params },
    { 404000, cmucal_VDDI_SUD_lut_params },
    { 101000, cmucal_VDDI_MUD_lut_params },
    { 101000, cmucal_VDDI_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_ALIVE_lut[] = {
    { 393220, cmucal_VDD_ALIVE_SOD_lut_params },
    { 393220, cmucal_VDD_ALIVE_OD_lut_params },
    { 393220, cmucal_VDD_ALIVE_NM_lut_params },
    { 393220, cmucal_VDD_ALIVE_UD_lut_params },
    { 393220, cmucal_VDD_ALIVE_SUD_lut_params },
    { 393220, cmucal_VDD_ALIVE_MUD_lut_params },
    { 393220, cmucal_VDD_ALIVE_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_MIF_lut[] = {
    { 26000, cmucal_VDD_MIF_SOD_lut_params },
    { 26000, cmucal_VDD_MIF_OD_lut_params },
    { 26000, cmucal_VDD_MIF_NM_lut_params },
    { 26000, cmucal_VDD_MIF_UD_lut_params },
    { 26000, cmucal_VDD_MIF_SUD_lut_params },
    { 26000, cmucal_VDD_MIF_MUD_lut_params },
    { 26000, cmucal_VDD_MIF_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CAM_lut[] = {
    { 665780, cmucal_VDD_CAM_SOD_lut_params },
    { 665780, cmucal_VDD_CAM_OD_lut_params },
    { 665780, cmucal_VDD_CAM_NM_lut_params },
    { 533340, cmucal_VDD_CAM_UD_lut_params },
    { 332890, cmucal_VDD_CAM_SUD_lut_params },
    { 10, cmucal_VDD_CAM_MUD_lut_params },
    { 10, cmucal_VDD_CAM_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CHUBVTS_lut[] = {
    { 393220, cmucal_VDD_CHUBVTS_SOD_lut_params },
    { 393220, cmucal_VDD_CHUBVTS_OD_lut_params },
    { 393220, cmucal_VDD_CHUBVTS_NM_lut_params },
    { 393220, cmucal_VDD_CHUBVTS_UD_lut_params },
    { 393220, cmucal_VDD_CHUBVTS_SUD_lut_params },
    { 393220, cmucal_VDD_CHUBVTS_MUD_lut_params },
    { 393220, cmucal_VDD_CHUBVTS_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_NPU_lut[] = {
    { 1066010, cmucal_VDD_NPU_SOD_lut_params },
    { 1066010, cmucal_VDD_NPU_OD_lut_params },
    { 1066010, cmucal_VDD_NPU_NM_lut_params },
    { 665780, cmucal_VDD_NPU_UD_lut_params },
    { 332890, cmucal_VDD_NPU_SUD_lut_params },
    { 10, cmucal_VDD_NPU_MUD_lut_params },
    { 10, cmucal_VDD_NPU_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_G3D_lut[] = {
    { 1300010, cmucal_VDD_G3D_SOD_lut_params },
    { 1300010, cmucal_VDD_G3D_OD_lut_params },
    { 1300010, cmucal_VDD_G3D_NM_lut_params },
    { 1300010, cmucal_VDD_G3D_UD_lut_params },
    { 1300010, cmucal_VDD_G3D_SUD_lut_params },
    { 1300010, cmucal_VDD_G3D_MUD_lut_params },
    { 1300010, cmucal_VDD_G3D_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CPUCL0_lut[] = {
    { 2100850, cmucal_VDD_CPUCL0_SOD_lut_params },
    { 1950080, cmucal_VDD_CPUCL0_OD_lut_params },
    { 1700690, cmucal_VDD_CPUCL0_NM_lut_params },
    { 1200490, cmucal_VDD_CPUCL0_UD_lut_params },
    { 650200, cmucal_VDD_CPUCL0_SUD_lut_params },
    { 350020, cmucal_VDD_CPUCL0_MUD_lut_params },
    { 350020, cmucal_VDD_CPUCL0_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CPUCL1_lut[] = {
    { 2800010, cmucal_VDD_CPUCL1_SOD_lut_params },
    { 2400010, cmucal_VDD_CPUCL1_OD_lut_params },
    { 1950010, cmucal_VDD_CPUCL1_NM_lut_params },
    { 1350010, cmucal_VDD_CPUCL1_UD_lut_params },
    { 700010, cmucal_VDD_CPUCL1_SUD_lut_params },
    { 350010, cmucal_VDD_CPUCL1_MUD_lut_params },
    { 700010, cmucal_VDD_CPUCL1_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_AUD_lut[] = {
    { 1200010, cmucal_VDD_AUD_SOD_lut_params },
    { 1200010, cmucal_VDD_AUD_OD_lut_params },
    { 1200010, cmucal_VDD_AUD_NM_lut_params },
    { 800000, cmucal_VDD_AUD_UD_lut_params },
    { 400000, cmucal_VDD_AUD_SUD_lut_params },
    { 100000, cmucal_VDD_AUD_MUD_lut_params },
    { 100000, cmucal_VDD_AUD_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_ICPU_CSILINK_lut[] = {
    { 800000, cmucal_VDD_ICPU_CSILINK_SOD_lut_params },
    { 800000, cmucal_VDD_ICPU_CSILINK_OD_lut_params },
    { 800000, cmucal_VDD_ICPU_CSILINK_NM_lut_params },
    { 800000, cmucal_VDD_ICPU_CSILINK_UD_lut_params },
    { 800000, cmucal_VDD_ICPU_CSILINK_SUD_lut_params },
    { 800000, cmucal_VDD_ICPU_CSILINK_MUD_lut_params },
    { 800000, cmucal_VDD_ICPU_CSILINK_UUD_lut_params },
};
struct vclk_lut cmucal_VDD_CPUCL2_lut[] = {
    { 2910010, cmucal_VDD_CPUCL2_SOD_lut_params },
    { 2450010, cmucal_VDD_CPUCL2_OD_lut_params },
    { 2000000, cmucal_VDD_CPUCL2_NM_lut_params },
    { 1350010, cmucal_VDD_CPUCL2_UD_lut_params },
    { 720010, cmucal_VDD_CPUCL2_SUD_lut_params },
    { 720010, cmucal_VDD_CPUCL2_MUD_lut_params },
    { 720010, cmucal_VDD_CPUCL2_UUD_lut_params },
};

struct vclk_lut cmucal_vclk_div_clk_peri_usixx_usi_lut[] = {
	{400000000, div_clk_peri_400_lut_params},
	{200000000, div_clk_peri_200_lut_params},
	{133000000, div_clk_peri_133_lut_params},
	{100000000, div_clk_peri_100_lut_params},
	{80000000, div_clk_peri_80_lut_params},
	{66000000, div_clk_peri_66_lut_params},
	{57000000, div_clk_peri_57_lut_params},
	{50000000, div_clk_peri_50_lut_params},
	{44000000, div_clk_peri_44_lut_params},
	{40000000, div_clk_peri_40_lut_params},
	{36000000, div_clk_peri_36_lut_params},
	{33000000, div_clk_peri_33_lut_params},
	{30000000, div_clk_peri_30_lut_params},
	{28000000, div_clk_peri_28_lut_params},
	{26660000, div_clk_peri_26_lut_params},
	{25000000, div_clk_peri_25_lut_params},
	{19200000, div_clk_peri_19_lut_params},
	{15360000, div_clk_peri_15_lut_params},
	{12800000, div_clk_peri_12_lut_params},
	{10970000, div_clk_peri_10_lut_params},
	{9600000, div_clk_peri_9_6_lut_params},
	{8533000, div_clk_peri_8_5_lut_params},
	{7680000, div_clk_peri_7_6_lut_params},
	{6981000, div_clk_peri_6_9_lut_params},
	{6400000, div_clk_peri_6_4_lut_params},
	{5970000, div_clk_peri_5_9_lut_params},
	{5485000, div_clk_peri_5_4_lut_params},
	{5120000, div_clk_peri_5_1_lut_params},
	{4800000, div_clk_peri_4_8_lut_params},
};

/* Switch VCLK -> LUT Parameter List */

/*================================ SWPLL List =================================*/

/*================================ VCLK List =================================*/
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
    CMUCAL_VCLK(VCLK_VDDI, cmucal_VDDI_lut, cmucal_VDDI, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_ALIVE, cmucal_VDD_ALIVE_lut, cmucal_VDD_ALIVE, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_MIF, cmucal_VDD_MIF_lut, cmucal_VDD_MIF, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CAM, cmucal_VDD_CAM_lut, cmucal_VDD_CAM, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CHUBVTS, cmucal_VDD_CHUBVTS_lut, cmucal_VDD_CHUBVTS, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_NPU, cmucal_VDD_NPU_lut, cmucal_VDD_NPU, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_G3D, cmucal_VDD_G3D_lut, cmucal_VDD_G3D, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_VDD_CPUCL0_lut, cmucal_VDD_CPUCL0, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CPUCL1, cmucal_VDD_CPUCL1_lut, cmucal_VDD_CPUCL1, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_AUD, cmucal_VDD_AUD_lut, cmucal_VDD_AUD, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_ICPU_CSILINK, cmucal_VDD_ICPU_CSILINK_lut, cmucal_VDD_ICPU_CSILINK, NULL, NULL),
    CMUCAL_VCLK(VCLK_VDD_CPUCL2, cmucal_VDD_CPUCL2_lut, cmucal_VDD_CPUCL2, NULL, NULL),

/* SPECIAL VCLK*/

/*CMGP*/
	CMUCAL_VCLK(VCLK_DIV_CMGP_USI0, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi0_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CMGP_USI1, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi1_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CMGP_USI2, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi2_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CMGP_USI3, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi3_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CMGP_USI4, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_usi4_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CMGP_USI_I2c, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_i2c_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CMGP_USI_I3c, cmucal_vclk_div_clk_peri_usixx_usi_lut, cmgp_i3c_ipclkport_clk, NULL, NULL),
/*PERIC*/
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI00_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi00_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI01_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi01_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI02_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi02_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI03_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi03_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI04_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi04_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI09_USI_OIS, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi09_ois_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI10_USI_OIS, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi10_ois_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_PERIC_USI_I2C, cmucal_vclk_div_clk_peri_usixx_usi_lut, peric_usi_i2c_ipclkport_clk, NULL, NULL),
/*USI*/
	CMUCAL_VCLK(VCLK_DIV_USI_USI06_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, usi_usi06_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_USI_USI07_USI, cmucal_vclk_div_clk_peri_usixx_usi_lut, usi_usi07_ipclkport_clk, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_USI_USI_I2C, cmucal_vclk_div_clk_peri_usixx_usi_lut, usi_usi_i2c_ipclkport_clk, NULL, NULL),
/* GATE VCLK*/
};
unsigned int cmucal_vclk_size = ARRAY_SIZE(cmucal_vclk_list);
