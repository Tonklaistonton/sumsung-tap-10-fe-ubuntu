/*
 * s2mps27-mfd.h - PMIC mfd driver for the S2MPS27
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

#ifndef __LINUX_MFD_S2MPS27_H
#define __LINUX_MFD_S2MPS27_H

#include <linux/i2c.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2mps27-register.h>

#if IS_ENABLED(CONFIG_S2MPS27_ADC)
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>
#endif

#define S2MPS27_REG_INVALID			(0xFF)

/* VGPIO TX (PMIC -> AP) table */
#define S2MPS27_VGI1_WRSTBO			(1 << 0)
#define S2MPS27_VGI1_IRQ_S1			(1 << 1)
#define S2MPS27_VGI1_IRQ_S2			(1 << 2)
#define S2MPS27_VGI1_IRQ_EXTRA			(1 << 3)
#define S2MPS27_VGI2_IRQ_RF			(1 << 0)
#define S2MPS27_VGI2_IRQ_S3			(1 << 2)
#define S2MPS27_VGI4_IRQ_S4			(1 << 0)
#define S2MPS27_VGI4_ONOB			(1 << 1)
#define S2MPS27_VGI4_IRQ_M			(1 << 2)
#define S2MPS27_VGI4_VOL_DN			(1 << 3)
#define S2MPS27_VGI5_IRQ_S5			(1 << 0)

#define S2MPS27_BUCK_MAX		2
#define S2MPS27_TEMP_MAX		2
#define BB1M_IDX			1

enum s2mps27_irq_source {
	S2MPS27_PMIC_INT1 = 0,
	S2MPS27_PMIC_INT2,
	S2MPS27_PMIC_INT3,
	S2MPS27_PMIC_INT4,
	S2MPS27_PMIC_INT5,
	S2MPS27_ADC_INTP,

	S2MPS27_IRQ_GROUP_NR,
};

enum s2mps27_irq {
	S2MPS27_PMIC_IRQ_PWRONR_INT1,
	S2MPS27_PMIC_IRQ_PWRONP_INT1,
	S2MPS27_PMIC_IRQ_JIGONBF_INT1,
	S2MPS27_PMIC_IRQ_JIGONBR_INT1,
	S2MPS27_PMIC_IRQ_ACOKBF_INT1,
	S2MPS27_PMIC_IRQ_ACOKBR_INT1,
	S2MPS27_PMIC_IRQ_PWRON1S_INT1,
	S2MPS27_PMIC_IRQ_MRB_INT1,

	S2MPS27_PMIC_IRQ_RTC60S_INT2,
	S2MPS27_PMIC_IRQ_RTCA1_INT2,
	S2MPS27_PMIC_IRQ_RTCA0_INT2,
	S2MPS27_PMIC_IRQ_SMPL_INT2,
	S2MPS27_PMIC_IRQ_RTC1S_INT2,
	S2MPS27_PMIC_IRQ_WTSR_INT2,
	S2MPS27_PMIC_IRQ_SPMI_LDO_OK_FAIL_INT2,
	S2MPS27_PMIC_IRQ_WRSTB_INT2,

	S2MPS27_PMIC_IRQ_INT120C_INT3,
	S2MPS27_PMIC_IRQ_INT140C_INT3,
	S2MPS27_PMIC_IRQ_TSD_INT3,
	S2MPS27_PMIC_IRQ_OVP_INT3,
	S2MPS27_PMIC_IRQ_TX_TRAN_FAIL_INT3,
	S2MPS27_PMIC_IRQ_OTP_CSUM_ERR_INT3,
	S2MPS27_PMIC_IRQ_VOLDNR_INT3,
	S2MPS27_PMIC_IRQ_VOLDNP_INT3,

	S2MPS27_PMIC_IRQ_OCP_SR1_INT4,
	S2MPS27_PMIC_IRQ_OCP_BB1_INT4,
	S2MPS27_PMIC_IRQ_OI_SR1_INT4,
	S2MPS27_PMIC_IRQ_OI_BB1_INT4,
	S2MPS27_PMIC_IRQ_PARITY_ERR0_INT4,
	S2MPS27_PMIC_IRQ_PARITY_ERR1_INT4,
	S2MPS27_PMIC_IRQ_PARITY_ERR2_INT4,
	S2MPS27_PMIC_IRQ_PARITY_ERR3_INT4,

	S2MPS27_PMIC_IRQ_OI_L1_INT5,
	S2MPS27_PMIC_IRQ_OI_L7_INT5,
	S2MPS27_PMIC_IRQ_OI_L8_INT5,
	S2MPS27_PMIC_IRQ_OI_L9_INT5,
	S2MPS27_PMIC_IRQ_OI_L10_INT5,
	S2MPS27_PMIC_IRQ_OI_L11_INT5,
	S2MPS27_PMIC_IRQ_WDT_INT5,

//	S2MPS27_ADC_IRQ_NTC0_OVER_INTP,
//	S2MPS27_ADC_IRQ_NTC1_OVER_INTP,

	S2MPS27_IRQ_NR,
};

#define MFD_DEV_NAME "s2mps27"

enum s2mps27_types {
	TYPE_S2MPS27,

	TYPE_S2MPS27_NR,
};

struct s2mps27_dev {
	struct device *dev;
	struct s2p_dev *sdev;

	uint16_t vgpio;
	uint16_t com;
	uint16_t rtc;
	uint16_t pm1;
	uint16_t pm2;
	uint16_t pm3;
	uint16_t adc;
	uint16_t gpio;
	uint16_t ext;

	/* IRQ */
	struct mutex irq_lock;
	int irq_masks_cur[S2MPS27_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPS27_IRQ_GROUP_NR];
	uint8_t irq_reg[S2MPS27_IRQ_GROUP_NR];
};

extern int s2mps27_irq_init(struct s2mps27_dev *s2mps27);
#endif /* __LINUX_MFD_S2MPS27_H */
