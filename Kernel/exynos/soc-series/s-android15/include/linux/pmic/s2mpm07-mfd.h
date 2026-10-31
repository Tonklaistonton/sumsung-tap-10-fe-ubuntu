/*
 * s2mpm07-mfd.h - PMIC mfd driver for the S2MPM07
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

#ifndef __LINUX_MFD_S2MPM07_H
#define __LINUX_MFD_S2MPM07_H

#include <linux/i2c.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2mpm07-register.h>

#if IS_ENABLED(CONFIG_S2MPM07_ADC)
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>
#endif

#define S2MPM07_REG_INVALID			(0xFF)

/* VGPIO TX (PMIC -> AP) table */
#define S2MPM07_VGI2_IRQ_RF			(1 << 0)

enum s2mpm07_irq_source {
	S2MPM07_PMIC_INT1 = 0,
	S2MPM07_PMIC_INT2,
	S2MPM07_PMIC_INT3,

	S2MPM07_IRQ_GROUP_NR,
};

enum s2mpm07_irq {
	S2MPM07_PMIC_IRQ_PWRONF_INT1,
	S2MPM07_PMIC_IRQ_PWRONR_INT1,
	S2MPM07_PMIC_IRQ_120C_INT1,
	S2MPM07_PMIC_IRQ_140C_INT1,
	S2MPM07_PMIC_IRQ_TSD_INT1,
	S2MPM07_PMIC_IRQ_WTSR_INT1,
	S2MPM07_PMIC_IRQ_WRSTB_INT1,

	S2MPM07_PMIC_IRQ_OCP_B1R_INT2,
	S2MPM07_PMIC_IRQ_OCP_SR1R_INT2,
	S2MPM07_PMIC_IRQ_OI_B1R_INT2,
	S2MPM07_PMIC_IRQ_OI_SR1R_INT2,
	S2MPM07_PMIC_IRQ_OVP_INT2,
	S2MPM07_PMIC_IRQ_BUCK_AUTO_EXIT_INT2,

	S2MPM07_PMIC_IRQ_OI_LDO20R_INT3,
	S2MPM07_PMIC_IRQ_OI_LDO21R_INT3,
	S2MPM07_PMIC_IRQ_TX_TRAN_FAIL_INT3,
	S2MPM07_PMIC_IRQ_CHECKSUM_INT3,
	S2MPM07_PMIC_IRQ_PARITY_ERR0_INT3,
	S2MPM07_PMIC_IRQ_PARITY_ERR1_INT3,
	S2MPM07_PMIC_IRQ_PARITY_ERR2_INT3,
	S2MPM07_PMIC_IRQ_PARITY_ERR3_INT3,

	S2MPM07_IRQ_NR,
};

#define MFD_DEV_NAME "s2mpm07"

enum s2mpm07_types {
	TYPE_S2MPM07,

	TYPE_S2MPM07_NR,
};

struct s2mpm07_dev {
	struct device *dev;
	struct s2p_dev *sdev;

	uint16_t vgpio;
	uint16_t com;
	uint16_t pm1;
	uint16_t close1;
	uint16_t adc;
	uint16_t gpio;

	/* IRQ */
	struct mutex irq_lock;
	int irq_masks_cur[S2MPM07_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPM07_IRQ_GROUP_NR];
	uint8_t irq_reg[S2MPM07_IRQ_GROUP_NR];
};

extern int s2mpm07_irq_init(struct s2mpm07_dev *s2mpm07);
#endif /* __LINUX_MFD_S2MPM07_H */
