/* SPDX-License-Identifier: GPL-2.0-only
 *
 * cal_9945/regs-dqe.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Dongho Lee <dh205.lee@samsung.com>
 *
 * Register definition file for Samsung Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _REGS_DQE_H
#define _REGS_DQE_H

enum dqe_regs_id {
	REGS_DQE0_ID = 0,
	REGS_DQE1_ID,
	REGS_DQE_ID_MAX
};

enum dqe_regs_type {
	REGS_DQE = 0,
	REGS_EDMA,
	REGS_DQE_TYPE_MAX
};

#define SHADOW_DQE_OFFSET		0x0200

/* DQE_TOP */
#define DQE_TOP_CORE_SECURITY		(0x0000)
#define DQE_CORE_SECURITY		(0x1 << 0)

#define DQE_TOP_IMG_SIZE		(0x0004)
#define DQE_IMG_VSIZE(_v)		((_v) << 16)
#define DQE_IMG_VSIZE_MASK		(0x3FFF << 16)
#define DQE_IMG_HSIZE(_v)		((_v) << 0)
#define DQE_IMG_HSIZE_MASK		(0x3FFF << 0)

#define DQE_TOP_FRM_SIZE		(0x0008)
#define DQE_FULL_IMG_VSIZE(_v)		((_v) << 16)
#define DQE_FULL_IMG_VSIZE_MASK	(0x3FFF << 16)
#define DQE_FULL_IMG_HSIZE(_v)		((_v) << 0)
#define DQE_FULL_IMG_HSIZE_MASK	(0x3FFF << 0)

#define DQE_TOP_FRM_PXL_NUM		(0x000C)
#define DQE_FULL_PXL_NUM(_v)		((_v) << 0)
#define DQE_FULL_PXL_NUM_MASK		(0xFFFFFFF << 0)

#define DQE_TOP_PARTIAL_START		(0x0010)
#define DQE_PARTIAL_START_Y(_v)	((_v) << 16)
#define DQE_PARTIAL_START_Y_MASK	(0x3FFF << 16)
#define DQE_PARTIAL_START_X(_v)	((_v) << 0)
#define DQE_PARTIAL_START_X_MASK	(0x3FFF << 0)

#define DQE_TOP_PARTIAL_CON		(0x0014)
#define DQE_PARTIAL_UPDATE_EN(_v)	((_v) << 0)
#define DQE_PARTIAL_UPDATE_EN_MASK	(0x1 << 0)

/*----------------------[TOP_LPD_MODE]---------------------------------------*/
#define DQE_TOP_LPD_MODE_CONTROL	(0x0018)
#define DQE_LPD_MODE_EXIT(_v)		((_v) << 0)
#define DQE_LPD_MODE_EXIT_MASK		(0x1 << 0)

#define DQE_TOP_LPD_ATC_CON		(0x001C)
#define LPD_ATC_FRAME_CNT(_v)		((_v) << 3)
#define LPD_ATC_FRAME_CNT_MASK		(0x3 << 3)

#define DQE_TOP_LPD(_n)			(DQE_TOP_LPD_ATC_CON + ((_n) * 0x4))
#define DQE_LPD_REG_MAX			(1)

/*----------------------[VERSION]---------------------------------------------*/
#define DQE_TOP_VER			(0x01FC)
#define TOP_VER				(0x08010000)
#define TOP_VER_GET(_v)			(((_v) >> 0) & 0xFFFFFFFF)

/*----------------------[GAMMA_MATRIX]---------------------------------------*/
#define DQE_GAMMA_MATRIX_CON		(0x1400)
#define GAMMA_MATRIX_EN(_v)		(((_v) & 0x1) << 0)
#define GAMMA_MATRIX_EN_MASK		(0x1 << 0)

#define DQE_GAMMA_MATRIX_COEFF(_n)	(0x1404 + ((_n) * 0x4))
#define DQE_GAMMA_MATRIX_COEFF0		(0x1404)
#define DQE_GAMMA_MATRIX_COEFF1		(0x1408)
#define DQE_GAMMA_MATRIX_COEFF2		(0x140C)
#define DQE_GAMMA_MATRIX_COEFF3		(0x1410)
#define DQE_GAMMA_MATRIX_COEFF4		(0x1414)

#define GAMMA_MATRIX_COEFF_H(_v)	(((_v) & 0x3FFF) << 16)
#define GAMMA_MATRIX_COEFF_H_MASK	(0x3FFF << 16)
#define GAMMA_MATRIX_COEFF_L(_v)	(((_v) & 0x3FFF) << 0)
#define GAMMA_MATRIX_COEFF_L_MASK	(0x3FFF << 0)

#define DQE_GAMMA_MATRIX_OFFSET0	(0x1418)
#define GAMMA_MATRIX_OFFSET_1(_v)	(((_v) & 0xFFF) << 16)
#define GAMMA_MATRIX_OFFSET_1_MASK	(0xFFF << 16)
#define GAMMA_MATRIX_OFFSET_0(_v)	(((_v) & 0xFFF) << 0)
#define GAMMA_MATRIX_OFFSET_0_MASK	(0xFFF << 0)

#define DQE_GAMMA_MATRIX_OFFSET1	(0x141C)
#define GAMMA_MATRIX_OFFSET_2(_v)	(((_v) & 0xFFF) << 0)
#define GAMMA_MATRIX_OFFSET_2_MASK	(0xFFF << 0)

#define DQE_GAMMA_MATRIX_LUT(_n)	(0x1400 + ((_n) * 0x4))

#define DQE_GAMMA_MATRIX_REG_MAX	(8)
#define DQE_GAMMA_MATRIX_LUT_MAX	(17)

/*----------------------[DEGAMMA]----------------------------------------*/
#define DQE_DEGAMMA_CON			(0x1800)
#define DEGAMMA_EN(_v)			(((_v) & 0x1) << 0)
#define DEGAMMA_EN_MASK			(0x1 << 0)

#define DQE_DEGAMMA_POSX(_n)		(0x1804 + ((_n) * 0x4))
#define DQE_DEGAMMA_POSY(_n)		(0x1848 + ((_n) * 0x4))
#define DEGAMMA_LUT_H(_v)		(((_v) & 0x1FFF) << 16)
#define DEGAMMA_LUT_H_MASK		(0x1FFF << 16)
#define DEGAMMA_LUT_L(_v)		(((_v) & 0x1FFF) << 0)
#define DEGAMMA_LUT_L_MASK		(0x1FFF << 0)
#define DEGAMMA_LUT(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define DEGAMMA_LUT_MASK(_x)		(0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_DEGAMMA_LUT(_n)		(0x1800 + ((_n) * 0x4))

#define DQE_DEGAMMA_LUT_CNT		(33) // LUT_X + LUT_Y
#define DQE_DEGAMMA_REG_MAX		(1+DIV_ROUND_UP(DQE_DEGAMMA_LUT_CNT, 2)*2) // 35 CON+LUT_XY/2
#define DQE_DEGAMMA_LUT_MAX		(1+DQE_DEGAMMA_LUT_CNT*2)		// 67 CON+LUT

/*----------------------[LINEAR MATRIX]----------------------------------------*/
#define DQE_LINEAR_MATRIX_CON           (0x1C00)
#define LINEAR_MATRIX_EN(_v)		(((_v) & 0x1) << 0)
#define LINEAR_MATRIX_EN_MASK		(0x1 << 0)

#define DQE_LINEAR_MATRIX_COEFF(_n)	(0x1C04 + ((_n) * 0x4))
#define DQE_LINEAR_MATRIX_COEFF0	(0x1C04)
#define DQE_LINEAR_MATRIX_COEFF1	(0x1C08)
#define DQE_LINEAR_MATRIX_COEFF2	(0x1C0C)
#define DQE_LINEAR_MATRIX_COEFF3	(0x1C10)
#define DQE_LINEAR_MATRIX_COEFF4	(0x1C14)

#define LINEAR_MATRIX_COEFF_H(_v)	(((_v) & 0xFFFF) << 16)
#define LINEAR_MATRIX_COEFF_H_MASK	(0xFFFF << 16)
#define LINEAR_MATRIX_COEFF_L(_v)	(((_v) & 0xFFFF) << 0)
#define LINEAR_MATRIX_COEFF_L_MASK	(0xFFFF << 0)

#define DQE_LINEAR_MATRIX_OFFSET0	(0x1C18)
#define LINEAR_MATRIX_OFFSET_1(_v)	(((_v) & 0x3FFF) << 16)
#define LINEAR_MATRIX_OFFSET_1_MASK	(0x3FFF << 16)
#define LINEAR_MATRIX_OFFSET_0(_v)	(((_v) & 0x3FFF) << 0)
#define LINEAR_MATRIX_OFFSET_0_MASK	(0x3FFF << 0)

#define DQE_LINEAR_MATRIX_OFFSET1	(0x1C1C)
#define LINEAR_MATRIX_OFFSET_2(_v)	(((_v) & 0x3FFF) << 0)
#define LINEAR_MATRIX_OFFSET_2_MASK	(0x3FFF << 0)

#define DQE_LINEAR_MATRIX_LUT(_n)	(0x1C00 + ((_n) * 0x4))

#define DQE_LINEAR_MATRIX_REG_MAX	(8)
#define DQE_LINEAR_MATRIX_LUT_MAX	(17)

/*----------------------[CGC]------------------------------------------------*/
#define DQE_CGC_CON			(0x2000)
#define CGC_COEF_DMA_REQ(_v)		((_v) << 4)
#define CGC_COEF_DMA_REQ_MASK		(0x1 << 4)
#define CGC_PIXMAP_EN(_v)		((_v) << 2)
#define CGC_PIXMAP_EN_MASK		(0x1 << 2)
#define CGC_EN(_v)			(((_v) & 0x1) << 0)
#define CGC_EN_MASK			(0x1 << 0)

#define DQE_CGC_MC_R0(_n)		(0x2004 + ((_n) * 0x4))
#define DQE_CGC_MC1_R0			(0x2004)
#define DQE_CGC_MC3_R0			(0x200C)
#define DQE_CGC_MC4_R0			(0x2010)
#define DQE_CGC_MC5_R0			(0x2014)

#define DQE_CGC_MC_R1(_n)		(0x2018 + ((_n) * 0x4))
#define DQE_CGC_MC1_R1			(0x2018)
#define DQE_CGC_MC3_R1			(0x2020)
#define DQE_CGC_MC4_R1			(0x2024)
#define DQE_CGC_MC5_R1			(0x2028)

#define DQE_CGC_MC_R2(_n)		(0x202C + ((_n) * 0x4))
#define DQE_CGC_MC1_R2			(0x202C)
#define CGC_MC_GAIN_R(_v)		(((_v) & 0xFFF) << 16)
#define CGC_MC_BC_VAL_R(_v)		(((_v) & 0x3) << 12)
#define CGC_MC_BC_SAT_R(_v)		(((_v) & 0x3) << 8)
#define CGC_MC_BC_HUE_R(_v)		(((_v) & 0x3) << 4)
#define CGC_MC_INVERSE_R(_v)		(((_v) & 0x1) << 1)
#define CGC_MC_ON_R(_v)			(((_v) & 0x1) << 0)
#define DQE_CGC_MC2_R2			(0x2030)
#define CGC_MC_VAL_GAIN_R(_v)		(((_v) & 0x7FF) << 16)
#define CGC_MC_HUE_GAIN_R(_v)		(((_v) & 0x3FF) << 0)
#define DQE_CGC_MC3_R2			(0x2034)
#define CGC_MC_S2_R(_v)			(((_v) & 0x3FF) << 16)
#define CGC_MC_S1_R(_v)			(((_v) & 0x3FF) << 0)
#define DQE_CGC_MC4_R2			(0x2038)
#define CGC_MC_H2_R(_v)			(((_v) & 0x1FFF) << 16)
#define CGC_MC_H1_R(_v)			(((_v) & 0x1FFF) << 0)
#define DQE_CGC_MC5_R2			(0x203C)
#define CGC_MC_V2_R(_v)			(((_v) & 0x3FF) << 16)
#define CGC_MC_V1_R(_v)			(((_v) & 0x3FF) << 0)

/*----------------------[REGAMMA]----------------------------------------*/
#define DQE_REGAMMA_CON			(0x2400)
#define REGAMMA_EN(_v)			(((_v) & 0x1) << 0)
#define REGAMMA_EN_MASK			(0x1 << 0)

#define DQE_REGAMMA_R_POSX(_n)		(0x2404 + ((_n) * 0x4))
#define DQE_REGAMMA_R_POSY(_n)		(0x2448 + ((_n) * 0x4))
#define DQE_REGAMMA_G_POSX(_n)		(0x248C + ((_n) * 0x4))
#define DQE_REGAMMA_G_POSY(_n)		(0x24D0 + ((_n) * 0x4))
#define DQE_REGAMMA_B_POSX(_n)		(0x2514 + ((_n) * 0x4))
#define DQE_REGAMMA_B_POSY(_n)		(0x2558 + ((_n) * 0x4))

#define REGAMMA_LUT_H(_v)		(((_v) & 0x1FFF) << 16)
#define REGAMMA_LUT_H_MASK		(0x1FFF << 16)
#define REGAMMA_LUT_L(_v)		(((_v) & 0x1FFF) << 0)
#define REGAMMA_LUT_L_MASK		(0x1FFF << 0)
#define REGAMMA_LUT(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define REGAMMA_LUT_MASK(_x)		(0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_REGAMMA_LUT(_n)		(0x2400 + ((_n) * 0x4))

#define DQE_REGAMMA_LUT_CNT		(33) // LUT_RGB_X/Y
#define DQE_REGAMMA_REG_MAX		(1+DIV_ROUND_UP(DQE_REGAMMA_LUT_CNT, 2)*2*3) // 103 CON + LUT_RGB_X/Y
#define DQE_REGAMMA_LUT_MAX		(1+DQE_REGAMMA_LUT_CNT*2*3)	// 199 CON + LUT_RGB_X/Y

/*----------------------[CGC_DITHER]-----------------------------------------*/
#define DQE_CGC_DITHER_CON		(0x2800)
#define CGC_DITHER_FRAME_OFFSET(_v)	(((_v) & 0xF) << 12)
#define CGC_DITHER_BIT(_v)		(((_v) & 0x1) << 8)
#define CGC_DITHER_BIT_MASK		(0x1 << 8)
#define CGC_DITHER_MASK_SEL_B		(0x1 << 7)
#define CGC_DITHER_MASK_SEL_G		(0x1 << 6)
#define CGC_DITHER_MASK_SEL_R		(0x1 << 5)
#define CGC_DITHER_MASK_SEL(_v, _n)	(((_v) & 0x1) << (5 + (_n)))
#define CGC_DITHER_MASK_SPIN(_v)	(((_v) & 0x1) << 3)
#define CGC_DITHER_MODE(_v)		(((_v) & 0x3) << 1)
#define CGC_DITHER_EN(_v)		(((_v) & 0x1) << 0)
#define CGC_DITHER_EN_MASK		(0x1 << 0)
#define DQE_CGC_DITHER_LUT_MAX	(8)

/*----------------------[SCL]-----------------------------------------*/
#define DQE_SCL_SCALED_IMG_SIZE		(0x3400)
#define SCALED_IMG_HEIGHT(_v)		((_v) << 16)
#define SCALED_IMG_HEIGHT_MASK		(0x3FFF << 16)
#define SCALED_IMG_WIDTH(_v)		((_v) << 0)
#define SCALED_IMG_WIDTH_MASK		(0x3FFF << 0)

#define DQE_SCL_MAIN_H_RATIO		(0x3404)
#define H_RATIO(_v)			((_v) << 0)
#define H_RATIO_MASK			(0xFFFFFF << 0)

#define DQE_SCL_MAIN_V_RATIO		(0x3408)
#define V_RATIO(_v)			((_v) << 0)
#define V_RATIO_MASK			(0xFFFFFF << 0)

#define DQE_SCL_Y_VCOEF(_n)		(0x3410 + ((_n) * 0x4))
#define DQE_SCL_Y_HCOEF(_n)		(0x34A0 + ((_n) * 0x4))
#define SCL_COEF(_v)			((_v) << 0)
#define SCL_COEF_LUT(_v)		(((_v) & 0x7FF) << 0)
#define SCL_COEF_MASK			(0x7FF << 0)

#define DQE_SCL_YHPOSITION		(0x35C0)
#define DQE_SCL_YVPOSITION		(0x35C4)
#define POS_I(_v)			((_v) << 20)
#define POS_I_MASK			(0xFFF << 20)
#define POS_I_GET(_v)			(((_v) >> 20) & 0xFFF)
#define POS_F(_v)			((_v) << 0)
#define POS_F_MASK			(0xFFFFF << 0)
#define POS_F_GET(_v)			(((_v) >> 0) & 0xFFFFF)

#define DQE_SCL_COEF_SET		(9) // 0X ~ 8X
#define DQE_SCL_VCOEF_MAX		(4) // nA ~ nD
#define DQE_SCL_HCOEF_MAX		(8) // nA ~ nH
#define DQE_SCL_COEF_MAX		(DQE_SCL_VCOEF_MAX+DQE_SCL_HCOEF_MAX)
#define DQE_SCL_COEF_CNT		(1) // Y coef only
#define DQE_SCL_REG_MAX			(DQE_SCL_COEF_SET*DQE_SCL_COEF_MAX*DQE_SCL_COEF_CNT)
#define DQE_SCL_LUT_MAX			(DQE_SCL_REG_MAX)

/*----------------------[CGC_LUT]-----------------------------------------*/
#define DQE_CGC_LUT_R(_n)		(0x4000 + ((_n) * 0x4))
#define DQE_CGC_LUT_G(_n)		(0x8000 + ((_n) * 0x4))
#define DQE_CGC_LUT_B(_n)		(0xC000 + ((_n) * 0x4))

#define CGC_LUT_H_SHIFT			(16)
#define CGC_LUT_H(_v)			((_v) << CGC_LUT_H_SHIFT)
#define CGC_LUT_H_MASK			(0x1FFF << CGC_LUT_H_SHIFT)
#define CGC_LUT_L_SHIFT			(0)
#define CGC_LUT_L(_v)			((_v) << CGC_LUT_L_SHIFT)
#define CGC_LUT_L_MASK			(0x1FFF << CGC_LUT_L_SHIFT)
#define CGC_LUT_LH(_x, _v)		((_v) << (0 + (16 * ((_x) & 0x1))))
#define CGC_LUT_LH_MASK(_x)		(0x1FFF << (0 + (16 * ((_x) & 0x1))))

#define DQE_CGC_REG_MAX			(2457)
#define DQE_CGC_LUT_MAX			(4914)
#define DQE_CGC_CON_REG_MAX		(16)
#define DQE_CGC_CON_LUT_MAX		(37)

#endif
