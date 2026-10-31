/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */
#ifndef _CHUB_EXYNOS_H_
#define _CHUB_EXYNOS_H_

#ifdef CONFIG_SENSOR_DRV
#include "main.h"
#endif
#include "comms.h"
#include "chub.h"
#include "ipc_chub.h"
#include "chub_dbg.h"

#ifdef CONFIG_SOC_S5E8855
#include "s5e8855.h"
#endif

//BAARC SFRs
#define REG_BARAC_BA_OFFSET  (0x1000)

#define REG_BARAC_BA_CTRL_OFFSET         (REG_BARAC_BA_OFFSET + 0x000)
#define BA_CTRL_UNMAP_BLOCK_ENABLE (0x1 << 0)

#define REG_BARAC_BA_IRQ_STASTUS_OFFSET  (REG_BARAC_BA_OFFSET + 0x004)
#define BA_IRQ_STASTUS_IRQ_MASK (0x1 << 0)

#define REG_BARAC_BA_IRQ_MASK_OFFSET     (REG_BARAC_BA_OFFSET + 0x008)
#define BA_IRQ_MASK_IRQ_MASK (0x1 << 0)

#define REG_BARAC_BA_IRQ_CLR_OFFSET      (REG_BARAC_BA_OFFSET + 0x00C)
#define BA_IRQ_CLR_INTR_CLR  (0x1 << 0)

#define REG_BARAC_BA_LOG_CTRL_OFFSET     (REG_BARAC_BA_OFFSET + 0x010)
#define BA_LOG_CTRL_AW_CAPT_MODE_LATEST (0x1 << 24)
#define BA_LOG_CTRL_AW_CAPT_MODE_FIRST  (0x0 << 24)
#define BA_LOG_CTRL_AW_CAPT_EN_ENABLE   (0x1 << 16)
#define BA_LOG_CTRL_AW_CAPT_EN_DISABLE  (0x0 << 16)
#define BA_LOG_CTRL_AR_CAPT_MODE_LATEST (0x1 << 8)
#define BA_LOG_CTRL_AR_CAPT_MODE_FIRST  (0x0 << 8)
#define BA_LOG_CTRL_AR_CAPT_EN_ENABLE   (0x1 << 0)
#define BA_LOG_CTRL_AR_CAPT_EN_DISABLE  (0x0 << 0)

#define REG_BARAC_BA_LOG_ST_OFFSET       (REG_BARAC_BA_OFFSET + 0x014)
#define BA_LOG_ST_AW_CONFLIT (0x1 << 9)
#define BA_LOG_ST_AW_CART    (0x1 << 8)
#define BA_LOG_ST_AR_CONFLIT (0x1 << 1)
#define BA_LOG_ST_AR_CART    (0x1 << 0)

#define REG_BARAC_BA_LOG_CLR_OFFSET      (REG_BARAC_BA_OFFSET + 0x018)
#define BA_LOG_CLR_AW_CAPT_CLEAR (0x1 << 8)
#define BA_LOG_CLR_AR_CAPT_CLEAR (0x1 << 0)

#define REG_BARAC_BA_ARLOG0_OFFSET       (REG_BARAC_BA_OFFSET + 0x01C)

#define REG_BARAC_BA_ARLOG1_OFFSET       (REG_BARAC_BA_OFFSET + 0x020)
#define BA_ARLOG1_ARLOG_ARQOS_BIT       (26)
#define BA_ARLOG1_ARLOG_ARQOS           (0xF << BA_ARLOG1_ARLOG_ARQOS_BIT)
#define BA_ARLOG1_ARLOG_ARSIZE_BIT      (23)
#define BA_ARLOG1_ARLOG_ARSIZE          (0x7 << BA_ARLOG1_ARLOG_ARSIZE_BIT)
#define BA_ARLOG1_ARLOG_ARPROT_BIT      (20)
#define BA_ARLOG1_ARLOG_ARPROT          (0x7 << BA_ARLOG1_ARLOG_ARPROT_BIT)
#define BA_ARLOG1_ARLOG_ARLOCK_BIT      (18)
#define BA_ARLOG1_ARLOG_ARLOCK          (0x3 << BA_ARLOG1_ARLOG_ARLOCK_BIT)
#define BA_ARLOG1_ARLOG_ARCACHE_BIT     (14)
#define BA_ARLOG1_ARLOG_ARCACHE         (0xF << BA_ARLOG1_ARLOG_ARCACHE_BIT)
#define BA_ARLOG1_ARLOG_ARBURST_BIT     (12)
#define BA_ARLOG1_ARLOG_ARBURST         (0x3 << BA_ARLOG1_ARLOG_ARBURST_BIT)
#define BA_ARLOG1_ARLOG_ARLEN_BIT       (8)
#define BA_ARLOG1_ARLOG_ARLEN           (0xF << BA_ARLOG1_ARLOG_ARLEN_BIT)
#define BA_ARLOG1_ARLOG_ARADDR39_32_BIT (0)
#define BA_ARLOG1_ARLOG_ARADDR39_32     (0xFF << BA_ARLOG1_ARLOG_ARADDR39_32_BIT)

#define REG_BARAC_BA_ARLOG2_OFFSET       (REG_BARAC_BA_OFFSET + 0x024)

#define REG_BARAC_BA_ARLOG3_OFFSET       (REG_BARAC_BA_OFFSET + 0x028)

#define REG_BARAC_BA_AWLOG0_OFFSET       (REG_BARAC_BA_OFFSET + 0x02C)

#define REG_BARAC_BA_AWLOG1_OFFSET       (REG_BARAC_BA_OFFSET + 0x030)
#define BA_AWLOG1_AWLOG_AWQOS_BIT       (26)
#define BA_AWLOG1_AWLOG_AWQOS           (0xF << BA_AWLOG1_AWLOG_AWQOS_BIT)
#define BA_AWLOG1_AWLOG_AWSIZE_BIT      (23)
#define BA_AWLOG1_AWLOG_AWSIZE          (0x7 << BA_AWLOG1_AWLOG_AWSIZE_BIT)
#define BA_AWLOG1_AWLOG_AWPROT_BIT      (20)
#define BA_AWLOG1_AWLOG_AWPROT          (0x7 << BA_AWLOG1_AWLOG_AWPROT_BIT)
#define BA_AWLOG1_AWLOG_AWLOCK_BIT      (18)
#define BA_AWLOG1_AWLOG_AWLOCK          (0x3 << BA_AWLOG1_AWLOG_AWLOCK_BIT)
#define BA_AWLOG1_AWLOG_AWCACHE_BIT     (14)
#define BA_AWLOG1_AWLOG_AWCACHE         (0xF << BA_AWLOG1_AWLOG_AWCACHE_BIT)
#define BA_AWLOG1_AWLOG_AWBURST_BIT     (12)
#define BA_AWLOG1_AWLOG_AWBURST         (0x3 << BA_AWLOG1_AWLOG_AWBURST_BIT)
#define BA_AWLOG1_AWLOG_AWLEN_BIT       (8)
#define BA_AWLOG1_AWLOG_AWLEN           (0xF << BA_AWLOG1_AWLOG_AWLEN_BIT)
#define BA_AWLOG1_AWLOG_AWADDR39_32_BIT (0)
#define BA_AWLOG1_AWLOG_AWADDR39_32     (0xFF << BA_AWLOG1_AWLOG_AWADDR39_32_BIT)

#define REG_BARAC_BA_AWLOG2_OFFSET       (REG_BARAC_BA_OFFSET + 0x034)

#define REG_BARAC_BA_AWLOG3_OFFSET       (REG_BARAC_BA_OFFSET + 0x038)

int contexthub_blk_poweron(struct contexthub_ipc_info *chub);
int contexthub_soc_poweron(struct contexthub_ipc_info *chub);
void contexthub_disable_pin(struct contexthub_ipc_info *chub);
int contexthub_get_qch_base(struct contexthub_ipc_info *chub);
int contexthub_set_clk(struct contexthub_ipc_info *chub);
int contexthub_get_clock_names(struct contexthub_ipc_info *chub);
int contexthub_core_reset(struct contexthub_ipc_info *chub);
void contexthub_upmu_init(struct contexthub_ipc_info *chub);
#endif /* _CHUB_EXYNOS_H_ */
