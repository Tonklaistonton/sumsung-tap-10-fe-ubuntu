/*
 * s2mpu16-mfd.h - PMIC mfd driver for the S2MPU16
 *
 *  Copyright (C) 2024 Samsung Electrnoics
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

#ifndef __LINUX_MFD_S2MPU16_H
#define __LINUX_MFD_S2MPU16_H

#include <linux/i2c.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2mpu16-register.h>

/* VGPIO TX (PMIC -> AP) table */
#define S2MPU16_VGI0_IRQ_S1			(1 << 3)

#define S2MPU16_BUCK_MAX		12
#define S2MPU16_TEMP_MAX		2
#define BUCK_SR1S_IDX			(10)

enum s2mpu16_irq_source {
	S2MPU16_PMIC_INT1 = 0,
	S2MPU16_PMIC_INT2,
	S2MPU16_PMIC_INT3,
	S2MPU16_PMIC_INT4,
	S2MPU16_PMIC_INT5,
	S2MPU16_PMIC_INT6,
	S2MPU16_PMIC_INT7,
	S2MPU16_IRQ_GROUP_NR,
};

enum s2mpu16_irq {
	/* PMIC */
	S2MPU16_PMIC_IRQ_PWRONF_INT1,
	S2MPU16_PMIC_IRQ_PWRONR_INT1,
	S2MPU16_PMIC_IRQ_INT120C_INT1,
	S2MPU16_PMIC_IRQ_INT140C_INT1,
	S2MPU16_PMIC_IRQ_TSD_INT1,
	S2MPU16_PMIC_IRQ_WTSR_INT1,
	S2MPU16_PMIC_IRQ_WRSTB_INT1,
	S2MPU16_PMIC_IRQ_TX_TRAN_FAIL_INT1,

	S2MPU16_PMIC_IRQ_OCP_B1S_INT2,
	S2MPU16_PMIC_IRQ_OCP_B2S_INT2,
	S2MPU16_PMIC_IRQ_OCP_B3S_INT2,
	S2MPU16_PMIC_IRQ_OCP_B4S_INT2,
	S2MPU16_PMIC_IRQ_OCP_B5S_INT2,
	S2MPU16_PMIC_IRQ_OCP_B6S_INT2,
	S2MPU16_PMIC_IRQ_OCP_B7S_INT2,
	S2MPU16_PMIC_IRQ_OCP_B8S_INT2,

	S2MPU16_PMIC_IRQ_OCP_B9S_INT3,
	S2MPU16_PMIC_IRQ_OCP_B10S_INT3,
	S2MPU16_PMIC_IRQ_OCP_SR1S_INT3,
	S2MPU16_PMIC_IRQ_UV_BB_INT3,
	S2MPU16_PMIC_IRQ_BB_NTR_DET_INT3,

	S2MPU16_PMIC_IRQ_OI_B1S_INT4,
	S2MPU16_PMIC_IRQ_OI_B2S_INT4,
	S2MPU16_PMIC_IRQ_OI_B3S_INT4,
	S2MPU16_PMIC_IRQ_OI_B4S_INT4,
	S2MPU16_PMIC_IRQ_OI_B5S_INT4,
	S2MPU16_PMIC_IRQ_OI_B6S_INT4,
	S2MPU16_PMIC_IRQ_OI_B7S_INT4,
	S2MPU16_PMIC_IRQ_OI_B8S_INT4,

	S2MPU16_PMIC_IRQ_OI_B9S_INT5,
	S2MPU16_PMIC_IRQ_OI_B10S_INT5,
	S2MPU16_PMIC_IRQ_OI_SR1S_INT5,
	S2MPU16_PMIC_IRQ_OI_SR2S_INT5,
	S2MPU16_PMIC_IRQ_OVP_INT5,
	S2MPU16_PMIC_IRQ_AUTO_EXIT_INT5,

	S2MPU16_PMIC_IRQ_LDO13_SCP_INT6,
	S2MPU16_PMIC_IRQ_CHECKSUM_INT6,
	S2MPU16_PMIC_IRQ_PARITY_ERR_DATA_INT6,
	S2MPU16_PMIC_IRQ_PARITY_ERR_ADDR_L_INT6,
	S2MPU16_PMIC_IRQ_PARITY_ERR_ADDR_H_INT6,
	S2MPU16_PMIC_IRQ_PARITY_ERR_CMD_INT6,

	S2MPU16_PMIC_IRQ_SPMI_LDO_OK_F_INT7,

	S2MPU16_IRQ_NR,
};

#define MFD_DEV_NAME "s2mpu16"

enum s2mpu16_types {
	TYPE_S2MPU16,

	TYPE_S2MPU16_NR,
};

struct s2mpu16_dev {
	struct device *dev;
	struct s2p_dev *sdev;
	struct mfd_cell *cell;

	int cell_size;

	uint16_t vgpio;
	uint16_t com;
	uint16_t pmic1;
	uint16_t pmic2;
	uint16_t close1;
	uint16_t close2;
	uint16_t gpio;

	/* IRQ */
	struct mutex irq_lock;
	int irq_masks_cur[S2MPU16_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPU16_IRQ_GROUP_NR];
	uint8_t irq_reg[S2MPU16_IRQ_GROUP_NR];
};

extern int s2mpu16_irq_init(struct s2mpu16_dev *s2mpu16);
#endif /* __LINUX_MFD_S2MPU16_H */
