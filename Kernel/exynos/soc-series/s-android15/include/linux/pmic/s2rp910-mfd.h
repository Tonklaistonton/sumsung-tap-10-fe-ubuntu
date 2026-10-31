/*
 * s2rp910-mfd.h - PMIC mfd driver for the S2RP910
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

#ifndef __LINUX_MFD_S2RP910_H
#define __LINUX_MFD_S2RP910_H

#include <linux/i2c.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2rp910-register.h>

#if IS_ENABLED(CONFIG_S2RP910_ADC)
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>
#endif

#define S2RP910_REG_INVALID			(0xFF)

/* VGPIO TX (PMIC -> AP) table */
#define S2RP910_VGI2_IRQ_RF1			(1 << 0)
#define S2RP910_VGI2_IRQ_RF2			(1 << 1)

#define S2RP910_BUCK_MAX	1
#define S2RP910_TEMP_MAX	2
#define S2RP910_OVP_MAX		1

enum s2rp910_irq_source {
	S2RP910_PMIC_INT1 = 0,
	S2RP910_PMIC_INT2,
	S2RP910_PMIC_INT3,

	S2RP910_IRQ_GROUP_NR,
};

enum s2rp910_irq {
	S2RP910_PMIC_IRQ_SYS_LDO_OK_F_INT1,
	S2RP910_PMIC_IRQ_SPMI_LDO_OK_F_INT1,
	S2RP910_PMIC_IRQ_INT120C_INT1,
	S2RP910_PMIC_IRQ_INT140C_INT1,
	S2RP910_PMIC_IRQ_TSD_INT1,
	S2RP910_PMIC_IRQ_WTSR_INT1,
	S2RP910_PMIC_IRQ_WRSTB_INT1,
	S2RP910_PMIC_IRQ_TX_TRAN_FAIL_INT1,

	S2RP910_PMIC_IRQ_OI_SR1_INT2,
	S2RP910_PMIC_IRQ_OCP_SR1_INT2,
	S2RP910_PMIC_IRQ_LDO6_SCP_INT2,
	S2RP910_PMIC_IRQ_LDO8_SCP_INT2,
	S2RP910_PMIC_IRQ_OVP_INT2,

	S2RP910_PMIC_IRQ_PWRONF_INT3,
	S2RP910_PMIC_IRQ_PWRONR_INT3,
	S2RP910_PMIC_IRQ_CHECKSUM_INT3,
	S2RP910_PMIC_IRQ_PARITY_ERR_DATA_INT3,
	S2RP910_PMIC_IRQ_PARITY_ERR_ADDR_L_INT3,
	S2RP910_PMIC_IRQ_PARITY_ERR_ADDR_H_INT3,
	S2RP910_PMIC_IRQ_PARITY_ERR_CMD_INT3,

	S2RP910_IRQ_NR,
};

#define MFD_DEV_NAME "s2rp910"

enum s2rp910_types {
	TYPE_S2RP910_1,
	TYPE_S2RP910_2,

	TYPE_S2RP910_NR,
};

struct s2rp910_dev {
	struct device *dev;
	struct s2p_dev *sdev;
	struct mfd_cell *cell;
	int cell_size;

	uint16_t vgpio;
	uint16_t com;
	uint16_t pm1;
	uint16_t pm2;
	uint16_t pm3;
	uint16_t buck;
	uint16_t ldo;
	uint16_t ext;
	uint16_t adc;

	/* IRQ */
	struct mutex irq_lock;
	int irq_masks_cur[S2RP910_IRQ_GROUP_NR];
	int irq_masks_cache[S2RP910_IRQ_GROUP_NR];
	uint8_t irq_reg[S2RP910_IRQ_GROUP_NR];
};

extern int s2rp910_irq_init(struct s2rp910_dev *s2rp910);
#endif /* __LINUX_MFD_S2RP910_H */
