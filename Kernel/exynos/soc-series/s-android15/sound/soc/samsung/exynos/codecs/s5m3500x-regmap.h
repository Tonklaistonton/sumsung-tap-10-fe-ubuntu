/*
 * /sound/soc/samsung/exynos/codecs/s5m3500x-regmap.h
 *
 *
 * ALSA SoC Audio Layer - Samsung Codec Driver
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S5M3700X_REGMAP_H
#define _S5M3700X_REGMAP_H

/**
 * struct reg_default - Default value for a register.
 *
 * @reg: Register address.
 * @def: Register default value.
 *
 * We use an array of structs rather than a simple array as many modern devices
 * have very sparse register maps.
 */

/* HW Reset Register */
static struct reg_default s5m3500x_reg_defaults[] = {
	/* Interrupt Register */
	{S5M3500X_008_IRQ1M,										0xFF},
	{S5M3500X_009_IRQ2M,										0xFF},
	{S5M3500X_00A_IRQ3M,										0xFF},
	{S5M3500X_00B_IRQ4M,										0xFF},
	{S5M3500X_00C_IRQ5M,										0xFF},
	{S5M3500X_00D_IRQ6M,										0xFF},

	/* Clock & Reset */
	{S5M3500X_010_CLKGATE0,									0x00},
	{S5M3500X_011_CLKGATE1,									0x00},
	{S5M3500X_012_CLKGATE2,									0x00},
	{S5M3500X_013_CLKGATE3,									0x3F},
	{S5M3500X_014_RESETB0,									0x83},
	{S5M3500X_015_RESETB1,									0x00},
	{S5M3500X_016_CLK_MODE_SEL0,							0x04},
	{S5M3500X_017_CLK_MODE_SEL1,							0x00},
	{S5M3500X_018_PWAUTO_AD,								0x00},
	{S5M3500X_019_PWAUTO_DA,								0x00},
	{S5M3500X_01A_COM_OTP_TEST,								0x00},
	{S5M3500X_01B_COM_OTP_ADRL,								0x00},
	{S5M3500X_01C_COM_OTP_ADRH,								0x00},

	/* Digital Audio Interface */
	{S5M3500X_020_IF_FORM1,									0x00},
	{S5M3500X_021_IF_FORM2,									0x18},
	{S5M3500X_022_IF_FORM3,									0x40},
	{S5M3500X_023_IF_FORM4,									0xE4},
	{S5M3500X_024_IF_FORM5,									0x10},
	{S5M3500X_025_IF_LOOPBACK,								0x80},
	{S5M3500X_029_IF_FORM6,									0x00},
	{S5M3500X_02F_GPIO_ST,									0x00},

	/* Recording Path Digital */
	{S5M3500X_030_ADC1,										0x00},
	{S5M3500X_031_ADC2,										0x00},
	{S5M3500X_033_ADC3,										0x00},
	{S5M3500X_034_AD_VOLL,									0x54},
	{S5M3500X_035_AD_VOLR,									0x54},
	{S5M3500X_037_AD_HPF,									0x48},
	{S5M3500X_038_AD_TRIM,									0x05},
	{S5M3500X_039_AD_TRIM,									0x03},
	{S5M3500X_03A_AD_VOL,									0x00},
	{S5M3500X_03B_AD_NS_DET0,								0x00},
	{S5M3500X_03C_AD_NS_DET1,								0x04},
	{S5M3500X_03D_AD_OFFSETL,								0x00},
	{S5M3500X_03E_AD_OFFSETR,								0x00},

	/* Playback Path Digital */
	{S5M3500X_040_PLAY_MODE,								0x04},
	{S5M3500X_041_PLAY_VOLL,								0x54},
	{S5M3500X_042_PLAY_VOLR,								0x54},
	{S5M3500X_044_PLAY_MIX0,								0x00},
	{S5M3500X_045_PLAY_MIX1,								0x04},
	{S5M3500X_046_PLAY_MIX2,								0x00},
	{S5M3500X_047_TRIM_DAC0,								0xF7},
	{S5M3500X_048_TRIM_DAC1,								0x4F},
	{S5M3500X_049_OFFSET_CTR,								0x6C},
	{S5M3500X_04A_HPL_OFFMSK0,								0x00},
	{S5M3500X_04B_HPL_OFFMSK1,								0x00},
	{S5M3500X_04C_HPL_OFFMSK2,								0x00},
	{S5M3500X_04D_HPR_OFFMSK0,								0x00},
	{S5M3500X_04E_HPR_OFFMSK1,								0x00},
	{S5M3500X_04F_HPR_OFFMSK2,								0x00},

	/* Adaptive Volume Control */
	{S5M3500X_050_AVC1,										0x03},
	{S5M3500X_051_AVC2,										0x6E},
	{S5M3500X_052_AVC3,										0x1A},
	{S5M3500X_053_AVC4,										0x80},
	{S5M3500X_054_AVC5,										0x89},
	{S5M3500X_055_AVC6,										0x05},
	{S5M3500X_056_AVC7,										0x05},
	{S5M3500X_058_AVC9,										0x00},
	{S5M3500X_059_AVC10,										0x00},
	{S5M3500X_05A_AVC11,										0x0A},
	{S5M3500X_05B_AVC12,										0x56},
	{S5M3500X_05C_AVC13,										0x00},
	{S5M3500X_05D_AVC14,										0x22},
	{S5M3500X_05E_AVC15,										0x00},
	{S5M3500X_05F_AVC16,										0x00},

	{S5M3500X_060_AVC17,										0x00},
	{S5M3500X_061_AVC18,										0x00},
	{S5M3500X_062_AVC19,										0x00},
	{S5M3500X_063_AVC20,										0x00},
	{S5M3500X_064_AVC21,										0x00},
	{S5M3500X_065_AVC22,										0x1F},
	{S5M3500X_066_AVC23,										0x1F},
	{S5M3500X_067_AVC24,										0x00},
	{S5M3500X_068_AVC25,										0x11},
	{S5M3500X_069_AVC26,										0xD6},
	{S5M3500X_06A_AVC27,										0x06},
	{S5M3500X_06B_AVC28,										0x00},
	{S5M3500X_06C_AVC29,										0x00},
	{S5M3500X_06D_AVC30,										0x00},
	{S5M3500X_06E_AVC31,										0x10},
	{S5M3500X_06F_AVC32,										0x32},

	{S5M3500X_070_AVC33,										0x93},
	{S5M3500X_071_AVC34,										0x18},
	{S5M3500X_072_AVC35,										0xDD},
	{S5M3500X_073_AVC36,										0x17},
	{S5M3500X_074_AVC37,										0xE9},
	{S5M3500X_077_AVC40,										0x4B},
	{S5M3500X_078_AVC41,										0xFF},
	{S5M3500X_079_AVC42,										0xFF},
	{S5M3500X_07A_AVC43,										0xA0},
	{S5M3500X_07B_AVC44,										0x00},
	{S5M3500X_07C_OCPCTRL0,									0x00},
	{S5M3500X_07D_OCPCTRL1,									0xCC},
	{S5M3500X_07E_AVC45,										0x00},
	{S5M3500X_07F_AVC46,										0x72},

	/* IRQ for LV */
	{S5M3500X_089_ANA_INTR_MASK,							0xFF},
	{S5M3500X_08A_NOISE_INTR_MASK,							0xFF},
	{S5M3500X_08B_I2S_INTR_MASK,							0xFF},
	{S5M3500X_08C_VTS_INTR_MASK,							0xFF},
	{S5M3500X_08D_ASEQ_INTR_MASK,							0xFF},
	{S5M3500X_08E_OCP_INTR_MASK,							0xFF},

	/* Digital DSM COntrol */
	{S5M3500X_093_DSM_CON1,									0x52},
	{S5M3500X_094_DSM_CON2,									0x00},
	{S5M3500X_095_DSM_CON3,									0x01},
	{S5M3500X_096_DSM_CON4,									0x00},
	{S5M3500X_097_CP2_HOLD,									0x0A},
	{S5M3500X_098_GPIO1_CON,								0x20},
	{S5M3500X_099_GPIO2_CON,								0x20},
	{S5M3500X_09A_GPIO3_CON,								0x20},
	{S5M3500X_09D_AVC_DWA_OFF_THRES,						0x46},
	{S5M3500X_09E_GPIO123_CON,								0x00},

	/* Auto Sequence Control */
	{S5M3500X_0A0_AMU_CTRL1,								0x55},
	{S5M3500X_0A1_AMU_CTRL2,								0x55},
	{S5M3500X_0A2_AMU_CTRL3,								0x65},
	{S5M3500X_0A3_AMU_CTRL4,								0x55},
	{S5M3500X_0A4_AMU_CTRL5,								0x95},
	{S5M3500X_0A5_AMU_CTRL6,								0x55},
	{S5M3500X_0A6_AMU_CTRL7,								0x59},
	{S5M3500X_0A7_AMU_CTRL8,								0x18},
	{S5M3500X_0A8_AMU_CTRL9,								0xC4},
	{S5M3500X_0A9_AMU_CTRL10,								0xD5},
	{S5M3500X_0AA_AMU_CTRL11,								0x24},
	{S5M3500X_0AB_AMU_CTRL12,								0x67},
	{S5M3500X_0AC_AMU_CTRL13,								0x96},
	{S5M3500X_0AD_AMU_CTRL14,								0x00},
	{S5M3500X_0AE_RESERVED,									0x3F},

	{S5M3500X_0B0_TEST_CTRL1,								0x00},
	{S5M3500X_0B1_TEST_CTRL2,								0x00},
	{S5M3500X_0B2_TEST_CTRL3,								0x00},
	{S5M3500X_0B3_TEST_CTRL4,								0x00},
	{S5M3500X_0B4_TEST_CTRL5,								0x00},
	{S5M3500X_0B5_TEST_CTRL6,								0x00},
	{S5M3500X_0B6_TEST_CTRL7,								0x00},
	{S5M3500X_0B7_TEST_CTRL8,								0x00},
	{S5M3500X_0B8_TEST_CTRL9,								0x00},
	{S5M3500X_0B9_TEST_CTRL10,								0x00},
	{S5M3500X_0BA_TEST_CTRL11,								0x00},

	/* Headphone Management Unit Control */
	{S5M3500X_0C0_ACTR_JD1,									0x00},
	{S5M3500X_0C1_ACTR_JD2,									0x37},
	{S5M3500X_0C2_ACTR_JD3,									0x77},
	{S5M3500X_0C3_ACTR_JD4,									0x31},
	{S5M3500X_0C4_ACTR_JD5,									0x60},
	{S5M3500X_0C5_ACTR_MCB1,								0xD0},
	{S5M3500X_0C6_ACTR_MCB2,								0x01},
	{S5M3500X_0C7_ACTR_MCB3,								0xFF},
	{S5M3500X_0C8_ACTR_MCB4,								0x00},
	{S5M3500X_0C9_ACTR_MCB5,								0x00},
	{S5M3500X_0CA_ACTR_MCB6,								0x00},
	{S5M3500X_0CB_RESERVED,									0x09},
	{S5M3500X_0CC_RESERVED,									0x00},
	{S5M3500X_0CD_RESERVED,									0x00},

	{S5M3500X_0D1_RESERVED,									0x20},
	{S5M3500X_0D2_RESERVED,									0xFF},
	{S5M3500X_0D3_RESERVED,									0xFF},
	{S5M3500X_0D4_DCTR_TEST4,								0x00},
	{S5M3500X_0D5_DCTR_TEST5,								0x00},
	{S5M3500X_0D6_DCTR_TEST6,								0x00},
	{S5M3500X_0D7_RESERVED,									0x55},
	{S5M3500X_0D8_DCTR_DBNC1,								0x90},
	{S5M3500X_0D9_DCTR_DBNC2,								0x90},
	{S5M3500X_0DA_DCTR_DBNC3,								0x90},
	{S5M3500X_0DB_DCTR_DBNC4,								0x90},
	{S5M3500X_0DC_DCTR_DBNC5,								0x90},
	{S5M3500X_0DD_DCTR_DBNC6,								0x00},
	{S5M3500X_0DF_RESERVED,									0x00},

	{S5M3500X_0E0_DCTR_FSM1,								0x05},
	{S5M3500X_0E1_DCTR_FSM2,								0x01},
	{S5M3500X_0E2_RESERVED,									0x89},
	{S5M3500X_0E3_RESERVED,									0x79},
	{S5M3500X_0E4_RESERVED,									0x0E},
	{S5M3500X_0E5_RESERVED,									0x0A},
	{S5M3500X_0E6_RESERVED,									0x79},
	{S5M3500X_0E7_RESERVED,									0x1E},

	{S5M3500X_0FD_ACTR_GP,									0x00},
	{S5M3500X_0FE_DCTR_GP1,									0x02},
	{S5M3500X_0FF_DCTR_GP2,									0x24},

	/* Analog Clock & Reference & CP Control */
	{S5M3500X_100_CTRL_REF1,								0x00},
	{S5M3500X_101_CTRL_REF2,								0x00},
	{S5M3500X_105_AD_CLK0,									0x00},
	{S5M3500X_106_DA_CLK0,									0x00},
	{S5M3500X_107_DA_CLK1,									0x91},
	{S5M3500X_108_DA_CP0,									0xA0},
	{S5M3500X_109_DA_CP1,									0xAD},
	{S5M3500X_10A_DA_CP2,									0x50},
	{S5M3500X_10B_DA_CP3,									0x25},
	{S5M3500X_10C_DA_CP4,									0x23},
	{S5M3500X_10D_DA_CP5,									0x22},
	{S5M3500X_10E_DA_CP6,									0x25},
	{S5M3500X_10F_DA_CP7,									0x24},

	/* Analog Recording Path Control */
	{S5M3500X_110_CTRL_MIC1,								0x00},
	{S5M3500X_111_CTRL_MIC2,								0x0C},
	{S5M3500X_112_CTRL_MIC3,								0x11},
	{S5M3500X_113_CTRL_MIC4,								0x22},
	{S5M3500X_114_CTRL_MIC5,								0x22},
	{S5M3500X_115_CTRL_MIC6,								0x22},
	{S5M3500X_116_CTRL_MIC7,								0x22},
	{S5M3500X_117_CTRL_MIC8,								0x55},
	{S5M3500X_118_CTRL_MIC9,								0x2C},
	{S5M3500X_119_CTRL_MIC10,								0x00},
	{S5M3500X_11A_CTRL_MIC11,								0x03},
	{S5M3500X_11B_CTRL_MIC12,								0xD0},
	{S5M3500X_11C_CTRL_MIC13,								0x04},
	{S5M3500X_11D_CTRL_MIC14,								0x44},
	{S5M3500X_11E_CTRL_MIC15,								0x00},

	/* Analog Playback Path Control */
	{S5M3500X_130_CTRL_RXREF1,								0x00},
	{S5M3500X_131_CTRL_RXREF2,								0x00},
	{S5M3500X_132_CTRL_IDAC1,								0x00},
	{S5M3500X_133_CTRL_IDAC2,								0x00},
	{S5M3500X_134_CTRL_IDAC3,								0x00},
	{S5M3500X_135_CTRL_IDAC4,								0x00},
	{S5M3500X_136_CTRL_IDAC5,								0x06},
	{S5M3500X_137_CTRL_IDAC6,								0x43},
	{S5M3500X_138_CTRL_HP1,									0x00},
	{S5M3500X_139_CTRL_HP2,									0x00},
	{S5M3500X_13A_CTRL_HP3,									0x00},
	{S5M3500X_13B_CTRL_HP4,									0x00},
	{S5M3500X_13C_CTRL_HP5,									0x04},
	{S5M3500X_13D_CTRL_HP6,									0x00},
	{S5M3500X_13E_CTRL_HP7,									0x00},
	{S5M3500X_13F_CTRL_HP8,									0x32},

	{S5M3500X_140_CTRL_HP9,									0x12},
	{S5M3500X_141_CTRL_HP10,								0x2A},
	{S5M3500X_142_CTRL_HP11,								0xA3},
	{S5M3500X_143_CTRL_HP12,								0x07},
	{S5M3500X_144_CTRL_HP13,								0x00},
	{S5M3500X_145_CTRL_HP14,								0x00},
	{S5M3500X_148_CTRL_EP1,									0x03},
	{S5M3500X_149_CTRL_EP2,									0x00},
	{S5M3500X_14A_CTRL_EP3,									0x32},
	{S5M3500X_14B_CTRL_EP4,									0x21},
	{S5M3500X_14C_CTRL_EP5,									0x33},
	{S5M3500X_14E_CTRL_OVP1,								0x00},
	{S5M3500X_14F_CTRL_OVP2,								0x00},

	/* OTP Register for Offset Calibration */
	{S5M3500X_200_HPL_OFFSET0,								0x00},
	{S5M3500X_201_HPL_OFFSET1,								0x00},
	{S5M3500X_202_HPL_OFFSET2,								0x00},
	{S5M3500X_203_HPL_OFFSET3,								0x00},
	{S5M3500X_204_HPL_OFFSET4,								0x00},
	{S5M3500X_205_HPL_OFFSET5,								0x00},
	{S5M3500X_206_HPL_OFFSET6,								0x00},
	{S5M3500X_207_HPL_OFFSET7,								0x00},
	{S5M3500X_208_HPL_OFFSET8,								0x00},
	{S5M3500X_209_HPL_OFFSET9,								0x00},
	{S5M3500X_20A_HPL_OFFSET10,								0x00},
	{S5M3500X_20B_HPL_OFFSET11,								0x00},
	{S5M3500X_20C_HPL_OFFSET12,								0x00},
	{S5M3500X_20D_HPL_OFFSET13,								0x00},
	{S5M3500X_20E_HPL_OFFSET14,								0x00},
	{S5M3500X_20F_HPL_OFFSET15,								0x00},
	{S5M3500X_210_HPL_OFFSET16,								0x00},
	{S5M3500X_211_HPL_OFFSET17,								0x00},
	{S5M3500X_212_HPL_OFFSET18,								0x00},
	{S5M3500X_213_HPL_OFFSET19,								0x00},
	{S5M3500X_214_HPL_OFFSET20,								0x00},
	{S5M3500X_215_HPL_OFFSET_S0,							0x00},
	{S5M3500X_216_HPL_OFFSET_S1,							0x00},
	{S5M3500X_217_HPL_OFFSET_S2,							0x00},

	{S5M3500X_218_HPR_OFFSET0,								0x00},
	{S5M3500X_219_HPR_OFFSET1,								0x00},
	{S5M3500X_21A_HPR_OFFSET2,								0x00},
	{S5M3500X_21B_HPR_OFFSET3,								0x00},
	{S5M3500X_21C_HPR_OFFSET4,								0x00},
	{S5M3500X_21D_HPR_OFFSET5,								0x00},
	{S5M3500X_21E_HPR_OFFSET6,								0x00},
	{S5M3500X_21F_HPR_OFFSET7,								0x00},
	{S5M3500X_220_HPR_OFFSET8,								0x00},
	{S5M3500X_221_HPR_OFFSET9,								0x00},
	{S5M3500X_222_HPR_OFFSET10,								0x00},
	{S5M3500X_223_HPR_OFFSET11,								0x00},
	{S5M3500X_224_HPR_OFFSET12,								0x00},
	{S5M3500X_225_HPR_OFFSET13,								0x00},
	{S5M3500X_226_HPR_OFFSET14,								0x00},
	{S5M3500X_227_HPR_OFFSET15,								0x00},
	{S5M3500X_228_HPR_OFFSET16,								0x00},
	{S5M3500X_229_HPR_OFFSET17,								0x00},
	{S5M3500X_22A_HPR_OFFSET18,								0x00},
	{S5M3500X_22B_HPR_OFFSET19,								0x00},
	{S5M3500X_22C_HPR_OFFSET20,								0x00},
	{S5M3500X_22D_HPR_OFFSET_S0,							0x00},
	{S5M3500X_22E_HPR_OFFSET_S1,							0x00},
	{S5M3500X_22F_HPR_OFFSET_S2,							0x00},

	{S5M3500X_230_DSM_OFFSETL,								0x00},
	{S5M3500X_231_DSM_OFFSETR,								0x00},
	{S5M3500X_232_DSM_OFFSET_RANGE,						0x00},
	{S5M3500X_233_AD_TRIM0,									0x00},
	{S5M3500X_234_AD_TRIM1,									0x00},
	{S5M3500X_235_REF_TRIM,									0x00},
	{S5M3500X_236_HP_TRIM0,									0x00},
	{S5M3500X_237_HP_TRIM1,									0x00},
	{S5M3500X_238_HP_TRIM2,									0x00},
	{S5M3500X_239_GPADC_TRIM,								0x00},
	{S5M3500X_23A_CHIP_ID1,									0x00},
	{S5M3500X_23B_CHIP_ID2,									0x00},
	{S5M3500X_23C_CHIP_ID3,									0x00},
	{S5M3500X_23D_CHIP_ID4,									0x00},
	{S5M3500X_23E_CHIP_ID5,									0x00},
	{S5M3500X_23F_CHIP_ID6,									0x00},
};

/**
 * struct reg_sequence - An individual write from a sequence of writes.
 *
 * @reg: Register address.
 * @def: Register value.
 * @delay_us: Delay to be applied after the register write in microseconds
 *
 * Register/value pairs for sequences of writes with an optional delay in
 * microseconds to be applied after each write.
 */

static const struct reg_sequence s5m3500x_init_patch[] = {
	/* registers for using cache update */
	{S5M3500X_101_CTRL_REF2,				0x20,		0},
	{S5M3500X_130_CTRL_RXREF1,				0x01,		0},
	{S5M3500X_100_CTRL_REF1,				0x81,		0},
	{S5M3500X_010_CLKGATE0,					0x01,		0},
	{S5M3500X_034_AD_VOLL,					0x54,		0},
	{S5M3500X_035_AD_VOLR,					0x54,		0},
	{S5M3500X_037_AD_HPF,					0x4E,		0},
	{S5M3500X_038_AD_TRIM,					0x59,		0},
	{S5M3500X_039_AD_TRIM,					0x55,		0},
	{S5M3500X_041_PLAY_VOLL,				0x54,		0},
	{S5M3500X_042_PLAY_VOLR,				0x54,		0},
	{S5M3500X_08D_ASEQ_INTR_MASK,			0x3F,		0},
	{S5M3500X_08E_OCP_INTR_MASK,			0x0F,		0},
	{S5M3500X_09D_AVC_DWA_OFF_THRES,		0x00,		0},
	{S5M3500X_0C6_ACTR_MCB2,				0xC1,		0},
};

/* Need to be set after MCLK is enabled */
static const struct reg_sequence s5m3500x_jack_patch[] = {
	{S5M3500X_0C0_ACTR_JD1,					0x03,		0},
	{S5M3500X_0C5_ACTR_MCB1,				0x20,		0},
	{S5M3500X_0C8_ACTR_MCB4,				0x04,		0},
	{S5M3500X_0D1_RESERVED,					0x20,		0},
	{S5M3500X_0D2_RESERVED,					0x60,		0},
	{S5M3500X_0D3_RESERVED,					0xA3,		0},
	{S5M3500X_0D6_DCTR_TEST6,				0x01,		0},
	{S5M3500X_0D8_DCTR_DBNC1,				0xC2,		0},
	{S5M3500X_0D9_DCTR_DBNC2,				0xB2,		0},
	{S5M3500X_0DA_DCTR_DBNC3,				0xB2,		0},
	{S5M3500X_0DB_DCTR_DBNC4,				0x92,		0},
	{S5M3500X_0E0_DCTR_FSM1,				0x04,		0},
	{S5M3500X_0E1_DCTR_FSM2,				0x07,		0},
	{S5M3500X_008_IRQ1M,						0x11,		0},
	{S5M3500X_009_IRQ2M,						0x1F,		0},
	{S5M3500X_00A_IRQ3M,						0xFE,		0},
	{S5M3500X_00B_IRQ4M,						0xFF,		0},
	{S5M3500X_010_CLKGATE0,					0x81,		0},
	{S5M3500X_015_RESETB1,					0xC0,		0},
};


#endif /* _S5M3700X_REGMAP_H */