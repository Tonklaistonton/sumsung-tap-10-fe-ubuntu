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

/* 9945 EVT1 */
/*
 * [ VMC_TOP BASE ADDRESS ] 0x1907_0000
 */

#define DEFINE_OFFSET(_val, _bitmask, _offset)	\
	(((_val) & (_bitmask)) << (_offset))

#define SHADOW_OFFSET			(0x0800)

#define VMC_ON_IRQ			(0x0000)
#define MODULATION_SEL(_v)		DEFINE_OFFSET((_v), 0x7, 28)
#define MODULATION_SEL_MASK		MODULATION_SEL(0x7)
#define TE_SEL(_v)			DEFINE_OFFSET((_v), 0x3, 25)
#define TE_SEL_TE0			TE_SEL(0)
#define TE_SEL_TE1			TE_SEL(1)
#define TE_SEL_TE2			TE_SEL(2)
#define CMD_EXIT(_v)			DEFINE_OFFSET((_v), 0x1, 24)
#define IRQ_VSYNC_FRM(_v)		DEFINE_OFFSET((_v), 0x1, 21)
#define IRQ_VSYNC_FRM_MASK(_v)		DEFINE_OFFSET((_v), 0x1, 20)
#define UPDATE_SEL(_v)			DEFINE_OFFSET((_v), 0x1, 16)
#define UPDATE_SEL_VSYNC_FRM		UPDATE_SEL(0)
#define UPDATE_SEL_VSYNC_FRM_MOD	UPDATE_SEL(1)
// STATUS_SEL ???
#define STATUS_SEL(_v)			DEFINE_OFFSET((_v), 0x1, 15)
#define STATUS_SEL_INTERNAL_FSM		STATUS_SEL(0)
#define STATUS_SEL_VSYNC_FRM_COUNT	STATUS_SEL(1)
// control for what ???
#define TE_ENABLE(_v)			DEFINE_OFFSET((_v), 0x1, 14)
#define TE_INITIAL_OFF			TE_ENABLE(0)
#define TE_INITIAL_ON			TE_ENABLE(1)
#define UPDATE_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 13)
#define IRQ_ENABLE(_v)			DEFINE_OFFSET((_v), 0x1, 12)
#define IRQ_VSYNC(_v)			DEFINE_OFFSET((_v), 0x1, 11)
#define IRQ_WU(_v)			DEFINE_OFFSET((_v), 0x1, 10)
#define IRQ_VSYNC_MASK(_v)		DEFINE_OFFSET((_v), 0x1, 9)
#define IRQ_WU_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 8)
// SW_RST is auto cleared
#define SW_RST				DEFINE_OFFSET(0x1, 0x1, 4)
// First Shadow update for reset value. auto cleared
#define FORCE_UP			DEFINE_OFFSET(0x1, 0x1, 1)
#define VM_EN(_v)			DEFINE_OFFSET((_v), 0x1, 0)

#define VMC_IRQ_VSYNC_FRM_MOD		IRQ_VSYNC_FRM(1)
#define VMC_IRQ_VSYNC_FRM		IRQ_VSYNC(1)
#define VMC_IRQ_WU			IRQ_WU(1)

#define VMC_CTRL			(0x0004)
#define VSYNC_FRM_IRQ_MASK_CNT(_v)	DEFINE_OFFSET((_v), 0xff, 16)
#define ESYNC_O_SEL(_v)			DEFINE_OFFSET((_v), 0x1, 4)
#define ESYNC_O_SEL_HSYNC		ESYNC_O_SEL(0)
#define ESYNC_O_SEL_ESYNC		ESYNC_O_SEL(1)
#define ESYNC_O_ENB(_v)			DEFINE_OFFSET((_v), 0x1, 3)
#define ESYNC_O_ENB_VAL(_v)		DEFINE_OFFSET((_v), 0x1, 2)
#define CLK_SEL(_v)			DEFINE_OFFSET((_v), 0x1, 1)
#define CLK_SEL_CLK1			CLK_SEL(0)	// main OSC 76.8MHz
#define CLK_SEL_CLK2			CLK_SEL(1)	// RCO 49.152MHz
#define VM_TYPE(_v)			DEFINE_OFFSET((_v), 0x1, 0)
#define VM_TYPE_AP_CENTRIC		VM_TYPE(0)
#define VM_TYPE_DDI_CENTRIC		VM_TYPE(1)

#define GLITCH_FREE			(0x0008)
#define LLMODE(_v)			DEFINE_OFFSET((_v), 0x1, 11)
#define HLMODE(_v)			DEFINE_OFFSET((_v), 0x1, 10)
#define REMODE(_v)			DEFINE_OFFSET((_v), 0x1, 9)
#define FEMODE(_v)			DEFINE_OFFSET((_v), 0x1, 8)
#define FLT_EN(_v)			DEFINE_OFFSET((_v), 0x1, 7)
#define CMP_CNT(_v)			DEFINE_OFFSET((_v), 0x7f, 0)

#define DISP_RESOL			(0x000c)
#define VRESOL(_v)			DEFINE_OFFSET((_v), 0x1fff, 16)
#define HRESOL(_v)			DEFINE_OFFSET((_v), 0x1fff, 0)

#define DISP_HPORCH			(0x0010)
#define HFP(_v)				DEFINE_OFFSET((_v), 0xffff, 16)
#define HBP(_v)				DEFINE_OFFSET((_v), 0xffff, 0)

#define DISP_VPORCH			(0x0014)
#define VFP(_v)				DEFINE_OFFSET((_v), 0xffff, 16)
#define VBP(_v)				DEFINE_OFFSET((_v), 0xffff, 0)

#define DISP_SYNC_AREA			(0x0018)
#define VSA(_v)				DEFINE_OFFSET((_v), 0xff, 16)
#define HSA(_v)				DEFINE_OFFSET((_v), 0xffff, 0)

#define VSYNC_PERIOD			(0x001c)
#define NUM_HSYNC(_v)			DEFINE_OFFSET((_v), 0x3ffff, 0)

#define LFR_UPDATE			(0x0020)
#define DDI_HSYNC_OPT(_v)		DEFINE_OFFSET((_v), 0x1, 16)
#define LFR_NUM(_v)			DEFINE_OFFSET((_v), 0xff, 8)
#define LFR_MASK(_v)			DEFINE_OFFSET((_v), 0x1, 7)
#define LFR_UPDATE_ESYNC_NUM(_v)	DEFINE_OFFSET((_v), 0x7f, 0)

#define HSYNC_WIDTH1			(0x0024)
#define HSYNC_MAX_EVEN_HALF(_v)		DEFINE_OFFSET((_v), 0xffff, 0)

#define HSYNC_WIDTH2			(0x0028)
#define HSYNC_WD0(_v)			DEFINE_OFFSET((_v), 0xfff, 16)
#define HSYNC_WD1(_v)			DEFINE_OFFSET((_v), 0xfff, 0)

#define VSYNC_WIDTH			(0x002c)
#define VSYNC_WD(_v)			DEFINE_OFFSET((_v), 0x3f, 0)

#define DLY_CTRL			(0x0030)
#define DLY_CLK1(_v)			DEFINE_OFFSET((_v), 0xfff, 12)
#define DLY_CLK2(_v)			DEFINE_OFFSET((_v), 0xfff, 0)

#define DISP_WAKEUP			(0x0034)
#define NUM_VSYNC_WU(_v)		DEFINE_OFFSET((_v), 0xffff, 14)
#define NUM_HSYNC_WU(_v)		DEFINE_OFFSET((_v), 0x3fff, 0)

#define EMISSION_NUM			(0x0038)
#define NUM_EMIT(_v)			DEFINE_OFFSET((_v), 0x7f, 0)

#define STATUS				(0x003c)
#define STATUS_VMC(_v)			DEFINE_OFFSET((_v), 0xffffffff, 0)

#define VMC_TZPC			(0x0040)
#define TZPC(_v)			DEFINE_OFFSET((_v), 0x1, 0)

#define PSLVERR_CON_LAYER		(0x0050)
#define PSLVERR_EN			(1 << 0)

#endif /* _REGS_VMC_H */
