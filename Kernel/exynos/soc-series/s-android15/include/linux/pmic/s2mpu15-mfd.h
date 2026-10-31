/*
 * s2mpu15-mfd.h - PMIC mfd driver for the S2MPU15
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

#ifndef __LINUX_MFD_S2MPU15_H
#define __LINUX_MFD_S2MPU15_H

#include <linux/i2c.h>
#include <linux/pmic/s2p.h>
#include <linux/pmic/s2mpu15-register.h>

#if IS_ENABLED(CONFIG_S2MPU15_ADC)
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/iio/consumer.h>
#endif

/* VGPIO TX (PMIC -> AP) table */
#define S2MPU15_VGI0_WRSTBO		(1 << 0)
#define S2MPU15_VGI0_ONOB		(1 << 1)
#define S2MPU15_VGI0_IRQ_M		(1 << 2)
#define S2MPU15_VGI0_IRQ_S1		(1 << 3)
#define S2MPU15_VGI4_VOL_DN		(1 << 3)

#define S2MPU15_BUCK_MAX	12
#define S2MPU15_TEMP_MAX	2
#define BUCK_SR1M_IDX		(8)

enum s2mpu15_irq_source {
	S2MPU15_PMIC_INT1 = 0,
	S2MPU15_PMIC_INT2,
	S2MPU15_PMIC_INT3,
	S2MPU15_PMIC_INT4,
	S2MPU15_PMIC_INT5,
	S2MPU15_PMIC_INT6,
	S2MPU15_PMIC_INT7,
	S2MPU15_PMIC_INT8,
	S2MPU15_IRQ_GROUP_NR,
};

enum s2mpu15_irq {
	S2MPU15_PMIC_IRQ_PWRONR_INT1,
	S2MPU15_PMIC_IRQ_PWRONP_INT1,
	S2MPU15_PMIC_IRQ_JIGONBF_INT1,
	S2MPU15_PMIC_IRQ_JIGONBR_INT1,
	S2MPU15_PMIC_IRQ_ACOKBF_INT1,
	S2MPU15_PMIC_IRQ_ACOKBR_INT1,
	S2MPU15_PMIC_IRQ_PWRON1S_INT1,
	S2MPU15_PMIC_IRQ_MRB_INT1,

	S2MPU15_PMIC_IRQ_RTC60S_INT2,
	S2MPU15_PMIC_IRQ_RTCA1_INT2,
	S2MPU15_PMIC_IRQ_RTCA0_INT2,
	S2MPU15_PMIC_IRQ_SMPL_INT2,
	S2MPU15_PMIC_IRQ_RTC1S_INT2,
	S2MPU15_PMIC_IRQ_WTSR_INT2,
	S2MPU15_PMIC_IRQ_BUCK_AUTO_EXIT_INT2,
	S2MPU15_PMIC_IRQ_WRSTB_INT2,

	S2MPU15_PMIC_IRQ_120C_INT3,
	S2MPU15_PMIC_IRQ_140C_INT3,
	S2MPU15_PMIC_IRQ_TSD_INT3,
	S2MPU15_PMIC_IRQ_OVP_INT3,
	S2MPU15_PMIC_IRQ_TX_TRAN_FAIL_INT3,
	S2MPU15_PMIC_IRQ_OTP_CSUM_ERR_INT3,
	S2MPU15_PMIC_IRQ_VOLDNR_INT3,
	S2MPU15_PMIC_IRQ_VOLDNP_INT3,

	S2MPU15_PMIC_IRQ_OCP_B1M_INT4,
	S2MPU15_PMIC_IRQ_OCP_B2M_INT4,
	S2MPU15_PMIC_IRQ_OCP_B3M_INT4,
	S2MPU15_PMIC_IRQ_OCP_B4M_INT4,
	S2MPU15_PMIC_IRQ_OCP_B5M_INT4,
	S2MPU15_PMIC_IRQ_OCP_B6M_INT4,
	S2MPU15_PMIC_IRQ_OCP_B7M_INT4,
	S2MPU15_PMIC_IRQ_OCP_B8M_INT4,

	S2MPU15_PMIC_IRQ_OCP_SR1M_INT5,
	S2MPU15_PMIC_IRQ_OCP_SR2M_INT5,
	S2MPU15_PMIC_IRQ_OCP_SR3M_INT5,
	S2MPU15_PMIC_IRQ_OCP_SR4M_INT5,
	S2MPU15_PMIC_IRQ_PARITY_ERR0_INT5,
	S2MPU15_PMIC_IRQ_PARITY_ERR1_INT5,
	S2MPU15_PMIC_IRQ_PARITY_ERR2_INT5,

	S2MPU15_PMIC_IRQ_OI_B1M_INT6,
	S2MPU15_PMIC_IRQ_OI_B2M_INT6,
	S2MPU15_PMIC_IRQ_OI_B3M_INT6,
	S2MPU15_PMIC_IRQ_OI_B4M_INT6,
	S2MPU15_PMIC_IRQ_OI_B5M_INT6,
	S2MPU15_PMIC_IRQ_OI_B6M_INT6,
	S2MPU15_PMIC_IRQ_OI_B7M_INT6,
	S2MPU15_PMIC_IRQ_OI_B8M_INT6,

	S2MPU15_PMIC_IRQ_OI_SR1M_INT7,
	S2MPU15_PMIC_IRQ_OI_SR2M_INT7,
	S2MPU15_PMIC_IRQ_OI_SR3M_INT7,
	S2MPU15_PMIC_IRQ_OI_SR4M_INT7,
	S2MPU15_PMIC_IRQ_WDT_INT7,
	S2MPU15_PMIC_IRQ_PARITY_ERR3_INT7,

	S2MPU15_PMIC_IRQ_OI_L10M_INT8,
	S2MPU15_PMIC_IRQ_OI_L11M_INT8,
	S2MPU15_PMIC_IRQ_OI_L29M_INT8,
	S2MPU15_PMIC_IRQ_OI_L30M_INT8,
	S2MPU15_PMIC_IRQ_OI_L31M_INT8,

	S2MPU15_IRQ_NR,
};

#define MFD_DEV_NAME "s2mpu15"

enum s2mpu15_types {
	TYPE_S2MPU15,

	TYPE_S2MPU15_NR,
};

struct s2mpu15_dev {
	struct device *dev;
	struct s2p_dev *sdev;

	uint16_t vgpio;
	uint16_t com;
	uint16_t rtc;
	uint16_t pmic1;
	uint16_t pmic2;
	uint16_t close1;
	uint16_t close2;
	uint16_t adc;

	/* IRQ */
	struct mutex irq_lock;
	int irq_masks_cur[S2MPU15_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPU15_IRQ_GROUP_NR];
	uint8_t irq_reg[S2MPU15_IRQ_GROUP_NR];
};

extern int s2mpu15_irq_init(struct s2mpu15_dev *s2mpu15);
#endif /* __LINUX_MFD_S2MPU15_H */
