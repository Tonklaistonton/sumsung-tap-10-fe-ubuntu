/*
 * s2mpa05-mfd.h - PMIC mfd driver for the S2MPA05
 *
 *  Copyright (C) 2023 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MFD_S2MPA05_H
#define __LINUX_MFD_S2MPA05_H

#include <linux/i2c.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2mpa05-register.h>

#define S2MPA05_REG_INVALID			(0xFF)

/* VGPIO TX (PMIC -> AP) table */
#define S2MPA05_VGI1_IRQ_EXTRA			(1 << 3)

#define S2MPA05_BUCK_MAX		4
#define S2MPA05_TEMP_MAX		2
#define S2MPA05_OVP_MAX			1

enum s2mpa05_irq_source {
	S2MPA05_PMIC_INT1 = 0,
	S2MPA05_PMIC_INT2,
	S2MPA05_PMIC_INT3,
	S2MPA05_PMIC_INT4,

	S2MPA05_IRQ_GROUP_NR,
};

enum s2mpa05_irq {
	S2MPA05_PMIC_IRQ_PWRONF_INT1,
	S2MPA05_PMIC_IRQ_PWRONR_INT1,
	S2MPA05_PMIC_IRQ_INT120C_INT1,
	S2MPA05_PMIC_IRQ_INT140C_INT1,
	S2MPA05_PMIC_IRQ_TSD_INT1,
	S2MPA05_PMIC_IRQ_WTSR_INT1,
	S2MPA05_PMIC_IRQ_WRSTB_INT1,
	S2MPA05_PMIC_IRQ_TX_TRAN_FAIL_INT1,

	S2MPA05_PMIC_IRQ_OCP_B1E_INT2,
	S2MPA05_PMIC_IRQ_OCP_B2E_INT2,
	S2MPA05_PMIC_IRQ_OCP_B3E_INT2,
	S2MPA05_PMIC_IRQ_OCP_B4E_INT2,
	S2MPA05_PMIC_IRQ_OI_B1E_INT2,
	S2MPA05_PMIC_IRQ_OI_B2E_INT2,
	S2MPA05_PMIC_IRQ_OI_B3E_INT2,
	S2MPA05_PMIC_IRQ_OI_B4E_INT2,

	S2MPA05_PMIC_IRQ_OVP_INT3,
	S2MPA05_PMIC_IRQ_BUCK_AUTO_EXIT_INT3,
	S2MPA05_PMIC_IRQ_RSVD1_INT3,
	S2MPA05_PMIC_IRQ_RSVD2_INT3,
	S2MPA05_PMIC_IRQ_LDO1_SCP_INT3,
	S2MPA05_PMIC_IRQ_LDO2_SCP_INT3,
	S2MPA05_PMIC_IRQ_LDO3_SCP_INT3,
	S2MPA05_PMIC_IRQ_LDO4_SCP_INT3,

	S2MPA05_PMIC_IRQ_RSVD0_INT4,
	S2MPA05_PMIC_IRQ_RSVD1_INT4,
	S2MPA05_PMIC_IRQ_SPMI_LDO_OK_F_INT4,
	S2MPA05_PMIC_IRQ_CHECKSUM_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_DATA_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_ADDR_L_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_ADDR_H_INT4,
	S2MPA05_PMIC_IRQ_PARITY_ERR_CMD_INT4,

	S2MPA05_IRQ_NR,
};

#define MFD_DEV_NAME "s2mpa05"

enum s2mpa05_types {
	TYPE_S2MPA05,

	TYPE_S2MPA05_NR,
};

struct s2mpa05_dev {
	struct device *dev;
	struct s2p_dev *sdev;

	uint16_t vgpio;
	uint16_t com;
	uint16_t pm1;
	uint16_t close1;
	uint16_t gpio;

	/* IRQ */
	struct mutex irq_lock;
	int irq_masks_cur[S2MPA05_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPA05_IRQ_GROUP_NR];
	uint8_t irq_reg[S2MPA05_IRQ_GROUP_NR];
};

extern int s2mpa05_irq_init(struct s2mpa05_dev *s2mpa05);
#endif /* __LINUX_MFD_S2MPA05_H */
