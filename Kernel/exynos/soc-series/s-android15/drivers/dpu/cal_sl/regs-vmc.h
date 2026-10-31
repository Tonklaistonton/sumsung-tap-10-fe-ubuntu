/* SPDX-License-Identifier: GPL-2.0-only
 *
 * cal_rt/regs-vmc.h
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register definition file for Samsung Display Pre-Processor driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _REGS_VMC_H
#define _REGS_VMC_H

/* 9955 */
/*
 * [ VMC_TOP EVT0 BASE ADDRESS ] 0x1B07_0000
 */
#define DEFINE_OFFSET(_val, _bitmask, _offset)	\
		(((_val) & (_bitmask)) << (_offset))

#define SHADOW_OFFSET				(0x0800)

#define VMC_SW_RST				(0x0000)
#define SW_RST(_v)				DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_ON					(0x0004)
//#define EWR_ALV_SEL	[9:8] // valid for 9955 EVT1
#define FORCE_UP(_v)				DEFINE_OFFSET((_v), 0x1, 1)
#define VM_EN(_v)				DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_UPDATE_CTRL				(0x0008)
#define SHADOW_UPDATE_TIMING_SHD_UPDATE_DECON(_v)	DEFINE_OFFSET((_v), 0x3, 10)
#define SHADOW_UPDATE_TIMING_CMD_ALLOW(_v)	DEFINE_OFFSET((_v), 0x3, 8)
#define SHADOW_UPDATE_MODE(_v)			DEFINE_OFFSET((_v), 0xf, 4)
#define SHADOW_UPDATE_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_IRQ					(0x000c)
#define INTREQ_VSYNC_FRM_MASK_CNT(_v)		DEFINE_OFFSET((_v), 0xff, 16)
#define INTREQ_VSYNC_FRM_CNT_MODE(_v)		DEFINE_OFFSET((_v), 0x1, 14)
#define INTREQ_VSYNC_FRM_CNT_MASK(_v)		DEFINE_OFFSET((_v), 0x1, 13)
#define INTREQ_VSYNC_FRM_CNT(_v)		DEFINE_OFFSET((_v), 0x1, 12)
#define INTREQ_VSYNC_FRM_SEL(_v)		DEFINE_OFFSET((_v), 0x1, 11)
#define INTREQ_VSYNC_FRM_VSYNC_FRM		INTREQ_VSYNC_FRM_SEL(0)
#define INTREQ_VSYNC_FRM_ESYNC_DECON		INTREQ_VSYNC_FRM_SEL(1) /* VSYNC_O */
#define INTREQ_VSYNC_FRM_MODE(_v)		DEFINE_OFFSET((_v), 0x1, 10)
#define INTREQ_VSYNC_FRM_MASK(_v)		DEFINE_OFFSET((_v), 0x1, 9)
#define INTREQ_VSYNC_FRM(_v)			DEFINE_OFFSET((_v), 0x1, 8)
#define INTREQ_WU_MODE(_v)			DEFINE_OFFSET((_v), 0x1, 6)
#define INTREQ_WU_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 5)
#define INTREQ_WU(_v)				DEFINE_OFFSET((_v), 0x1, 4)
#define IRQ_ENABLE(_v)				DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_IRQ_VSYNC_FRM_MOD			INTREQ_VSYNC_FRM_CNT(1)
#define VMC_IRQ_VSYNC_FRM			INTREQ_VSYNC_FRM(1)
#define VMC_IRQ_WU				INTREQ_WU(1)

#define VMC_TE_CTRL				(0x0010)
#define TE_SEL(_v)				DEFINE_OFFSET((_v), 0x3, 8)
#define CMD_EXIT_TE(_v)				DEFINE_OFFSET((_v), 0x1, 4)
#define TE_ENABLE(_v)				DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_ESYNC_DEBUG				(0x0014)
#define ESYNC_O_ENB(_v)				DEFINE_OFFSET((_v), 0x1, 1)
#define ESYNC_O_ENB_VAL(_v)			DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_STATUS_SEL				(0x0018)
#define STATUS_MON_SEL(_v)			DEFINE_OFFSET((_v), 0x3, 0)

#define VMC_STATUS_MON				(0x001c)
#define STATUS_VMC(_v)				DEFINE_OFFSET((_v), 0xffffffff, 0)

#define VMC_MAIN_CTRL				(0x0020)
/*
 * HSYNC Period Check option
 *  0 = Internal SFR Setting use <= Esync is delayed by the # of underrun lines
 *  1 = ESYNC_I edge check <= Esync is the same even if underrun occurs
 */
#define DDI_HSYNC_OPT(_v)			DEFINE_OFFSET((_v), 0x1, 16)
#define MODULATION_SEL(_v)			DEFINE_OFFSET((_v), 0x7, 12)
#define ESYNC_O_SEL(_v)				DEFINE_OFFSET((_v), 0x1, 9)
#define CLK_SEL(_v)				DEFINE_OFFSET((_v), 0x1, 8)
#define SHD_UPDATE_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 4)
#define CMD_ALLOW_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 3)
#define FRM_UPDATE_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 2)
#define VSYNC_I_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 1)
#define VM_TYPE(_v)				DEFINE_OFFSET((_v), 0x1, 0)
#define VM_TYPE_AP_CENTRIC			VM_TYPE(0)
#define VM_TYPE_DDI_CENTRIC			VM_TYPE(1)

#define VMC_ESYNC_GLITCH			(0x0024)
#define LLMODE(_v)				DEFINE_OFFSET((_v), 0x1, 11)
#define HLMODE(_v)				DEFINE_OFFSET((_v), 0x1, 10)
#define REMODE(_v)				DEFINE_OFFSET((_v), 0x1, 9)
#define FEMODE(_v)				DEFINE_OFFSET((_v), 0x1, 8)
#define FLT_EN(_v)				DEFINE_OFFSET((_v), 0x1, 7)
#define CMP_CNT(_v)				DEFINE_OFFSET((_v), 0x7f, 0)

#define VMC_FRAME_RESOL				(0x0030)
#define VRESOL(_v)				DEFINE_OFFSET((_v), 0x1fff, 16)
#define HRESOL(_v)				DEFINE_OFFSET((_v), 0x1fff, 0)

#define VMC_FRAME_HPORCH			(0x0034)
#define HFP(_v)					DEFINE_OFFSET((_v), 0xffff, 16)
#define HBP(_v)					DEFINE_OFFSET((_v), 0xffff, 0)

#define VMC_FRAME_VPORCH			(0x0038)
#define VFP(_v)					DEFINE_OFFSET((_v), 0xffff, 16)
#define VBP(_v)					DEFINE_OFFSET((_v), 0xffff, 0)

#define VMC_FRAME_SYNC_AREA			(0x003c)
#define VSA(_v)					DEFINE_OFFSET((_v), 0xff, 16)
#define HSA(_v)					DEFINE_OFFSET((_v), 0xffff, 0)

#define VMC_FRAME_ESYNC_NUM			(0x0040)
#define NUM_EMIT(_v)				DEFINE_OFFSET((_v), 0x7f, 0)

#define VMC_ESYNC_PERIOD			(0x0044)
#define NUM_HSYNC(_v)				DEFINE_OFFSET((_v), 0x7ffff, 0)

/*
 * obsolete register
 * => replaced to VMC_HSYNC_DDI_WD
 */
#define VMC_ESYNC_WD				(0x0048)
#define ESYNC_WD0(_v)				DEFINE_OFFSET((_v), 0xfff, 16)
#define ESYNC_WD1(_v)				DEFINE_OFFSET((_v), 0xfff, 0)

#define VMC_VSYNC_WD				(0x004c)
#define VSYNC_WD(_v)				DEFINE_OFFSET((_v), 0x3f, 0)

#define VMC_HSYNC_INT_WD			(0x0050)
#define HSYNC_MAX_EVEN_HALF(_v)			DEFINE_OFFSET((_v), 0xffff, 0)

#define VMC_HSYNC_DDI_WD			(0x0054)
#define HSYNC_WD0(_v)				DEFINE_OFFSET((_v), 0xfff, 16)
#define HSYNC_WD1(_v)				DEFINE_OFFSET((_v), 0xfff, 0)

#define VMC_DECON_IF_DLY			(0x0058)
/*
 * [31]LFR_UPDATE
 * [30]VSYNC
 * [29]ESYNC
 * [28] HSYNC
 */
#define DLY_SYNC_DECON_MASK(_v)			DEFINE_OFFSET((_v), 0xf, 28)
/*
 * delay between ESYNC and VSYNC_O transmitted to DECON
 */
#define DLY_SYNC_DECON(_v)			DEFINE_OFFSET((_v), 0x3ffff, 0)

#define VMC_ESYNC_IF_DLY			(0x005c)
#define DLY_CLK1(_v)				DEFINE_OFFSET((_v), 0xfff, 16)
#define DLY_CLK2(_v)				DEFINE_OFFSET((_v), 0xfff, 0)

#define VMC_DECON_LFR_UPDATE			(0x0060)
#define LFR_UPDATE_HSYNC(_v)			DEFINE_OFFSET((_v), 0xff, 16)
#define LFR_NUM(_v)				DEFINE_OFFSET((_v), 0xff, 8)
#define LFR_MASK(_v)				DEFINE_OFFSET((_v), 0x1, 7)
#define LFR_UPDATE(_v)				DEFINE_OFFSET((_v), 0x7f, 0)

#define VMC_WAKE_UP_H				(0x0064)
#define NUM_HSYNC_WU(_v)			DEFINE_OFFSET((_v), 0x7ffff, 0)

#define VMC_WAKE_UP_V				(0x0068)
#define NUM_VSYNC_WU(_v)			DEFINE_OFFSET((_v), 0xff, 16)
#define NUM_ESYNC_WU(_v)			DEFINE_OFFSET((_v), 0x7f, 0)

#define VMC_TMG_IDLE_CNT			(0x006c)
#define TMG_V_IDLE_CNT_ON(_v)			DEFINE_OFFSET((_v), 0x1, 31)
#define TMG_V_IDLE_CNT(_v)			DEFINE_OFFSET((_v), 0x7fff, 16)
#define TMG_H_IDLE_CNT_ON(_v)			DEFINE_OFFSET((_v), 0x1, 15)
#define TMG_H_IDLE_CNT(_v)			DEFINE_OFFSET((_v), 0x7fff, 0)

#define VMC_TMG_EARLY_TE			(0x0070)
/*
 * TE_WAIT_H_PERIOD
 * : Number of lines waiting to send Esync after detecting rising TE
*/
#define TE_WAIT_H_PERIOD(_v)			DEFINE_OFFSET((_v), 0xff, 16)
#define EARLY_TE_ON(_v)				DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_TE_ESYNC_MASK			(0x0074)
#define MASK_ON(_v)				DEFINE_OFFSET((_v), 0x1, 31)
#define ESYNC_DSIM_UNMASK(_v)			DEFINE_OFFSET((_v), 0x1, 30)
#define HSYNC_DECON_UNMASK(_v)			DEFINE_OFFSET((_v), 0x1, 29)
#define LINE_POINT(_v)				DEFINE_OFFSET((_v), 0xff, 20)
#define PIX_POINT(_v)				DEFINE_OFFSET((_v), 0x7ffff, 0)

#define VMC_MASK_POINT_L			(0x0078) // valid for 9955 EVT1
#define MASK_LINE_POINT(_v)			DEFINE_OFFSET((_v), 0xff, 20)
#define MASK_PIX_POINT(_v)			DEFINE_OFFSET((_v), 0x7ffff, 0)

#define VMC_TZPC				(0x0080)
#define TZPC(_v)				DEFINE_OFFSET((_v), 0x1, 0)

#define PSLVERR_CON_LAYER			(0x0084)
#define VMC_PSLVERR_EN(_v)			DEFINE_OFFSET((_v), 0x1, 0)

#endif /* _REGS_VMC_H */
