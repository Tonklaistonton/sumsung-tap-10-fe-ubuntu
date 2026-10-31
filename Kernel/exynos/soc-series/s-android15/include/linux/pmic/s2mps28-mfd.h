/*
 * s2mps28-mfd.h - PMIC mfd driver for the S2MPS28
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

#ifndef __LINUX_MFD_S2MPS28_H
#define __LINUX_MFD_S2MPS28_H

#include <linux/i2c.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2mps28-register.h>

#define S2MPS28_REG_INVALID		(0xFF)

/* VGPIO TX (PMIC -> AP) table */
#define S2MPS28_VGI1_IRQ_S1			(1 << 1)
#define S2MPS28_VGI1_IRQ_S2			(1 << 2)
#define S2MPS28_VGI2_IRQ_S3			(1 << 2)
#define S2MPS28_VGI4_IRQ_S4			(1 << 0)
#define S2MPS28_VGI5_IRQ_S5			(1 << 0)

#define S2MPS28_BUCK_MAX		6 /* buck 1S ~ 5S, SR1 */
#define S2MPS28_TEMP_MAX		2
#define BUCK_SR1S_IDX			5 /* TODO: check correct value */

enum s2mps28_irq_source {
	S2MPS28_PMIC_INT1 = 0,
	S2MPS28_PMIC_INT2,
	S2MPS28_PMIC_INT3,
	S2MPS28_PMIC_INT4,

	S2MPS28_IRQ_GROUP_NR,
};

enum s2mps28_irq {
	S2MPS28_PMIC_IRQ_PWRONF_INT1,
	S2MPS28_PMIC_IRQ_PWRONR_INT1,
	S2MPS28_PMIC_IRQ_INT120C_INT1,
	S2MPS28_PMIC_IRQ_INT140C_INT1,
	S2MPS28_PMIC_IRQ_TSD_INT1,
	S2MPS28_PMIC_IRQ_WTSR_INT1,
	S2MPS28_PMIC_IRQ_WRSTB_INT1,
	S2MPS28_PMIC_IRQ_TX_TRAN_FAIL_INT1,

	S2MPS28_PMIC_IRQ_OCP_B1_INT2,
	S2MPS28_PMIC_IRQ_OCP_B2_INT2,
	S2MPS28_PMIC_IRQ_OCP_B3_INT2,
	S2MPS28_PMIC_IRQ_OCP_B4_INT2,
	S2MPS28_PMIC_IRQ_OCP_B5_INT2,
	S2MPS28_PMIC_IRQ_OCP_SR1_INT2,
	S2MPS28_PMIC_IRQ_BUCK_AUTO_EXIT_INT2,

	S2MPS28_PMIC_IRQ_OI_B1_INT3,
	S2MPS28_PMIC_IRQ_OI_B2_INT3,
	S2MPS28_PMIC_IRQ_OI_B3_INT3,
	S2MPS28_PMIC_IRQ_OI_B4_INT3,
	S2MPS28_PMIC_IRQ_OI_B5_INT3,
	S2MPS28_PMIC_IRQ_OI_SR1_INT3,
	S2MPS28_PMIC_IRQ_OVP_INT3,

	S2MPS28_PMIC_IRQ_SPMI_LDO_OK_F_INT4,
	S2MPS28_PMIC_IRQ_CHECKSUM_INT4,
	S2MPS28_PMIC_IRQ_PARITY_ERR_DATA_INT4,
	S2MPS28_PMIC_IRQ_PARITY_ERR_ADDR_L_INT4,
	S2MPS28_PMIC_IRQ_PARITY_ERR_ADDR_H_INT4,
	S2MPS28_PMIC_IRQ_PARITY_ERR_CMD_INT4,

	S2MPS28_IRQ_NR,
};

#define MFD_DEV_NAME "s2mps28"

enum s2mps28_types {
	TYPE_S2MPS28_1,
	TYPE_S2MPS28_2,
	TYPE_S2MPS28_3,
	TYPE_S2MPS28_4,
	TYPE_S2MPS28_5,

	TYPE_S2MPS28_NR,
};

struct s2mps28_dev {
	struct device *dev;
	struct s2p_dev *sdev;
	struct mfd_cell *cell;

	int cell_size;

	uint16_t vgpio;
	uint16_t com;
	uint16_t pm1;
	uint16_t pm2;
	uint16_t pm3;
	uint16_t gpio;
	uint16_t ext;

	/* IRQ */
	struct mutex irq_lock;
	int irq_masks_cur[S2MPS28_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPS28_IRQ_GROUP_NR];
	uint8_t irq_reg[S2MPS28_IRQ_GROUP_NR];
};

extern int s2mps28_irq_init(struct s2mps28_dev *s2mps28);
#endif /* __LINUX_MFD_S2MPS28_H */
