/*
 * s2mpu16-regulator.h - PMIC regulator for the S2MPU16
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

#ifndef __LINUX_S2MPU16_REGULATOR_H
#define __LINUX_S2MPU16_REGULATOR_H

#include <linux/pmic/s2mpu16-register.h>

#define REGULATOR_DEV_NAME		"s2mpu16-regulator"

/* BUCKs 1S to 10S */
#define S2MPU16_BUCK_MIN1		300000
#define S2MPU16_BUCK_STEP1		6250
/* BUCK SR1 (BB) */
#define S2MPU16_BUCK_MIN2		2600000
#define S2MPU16_BUCK_STEP2		12500
/* BUCK SR2S */
#define S2MPU16_BUCK_MIN3		600000
#define S2MPU16_BUCK_STEP3		12500
/* LDOs LV 1S/2S/4S~7S/14S/15S  */
#define S2MPU16_LDO_MIN1		300000
#define S2MPU16_LDO_STEP1		6250
/* LDO IV 3S/8S~10S */
#define S2MPU16_LDO_MIN2		1500000
#define S2MPU16_LDO_STEP2		12500
/* LDO MV 11S~13S */
#define S2MPU16_LDO_MIN3		1800000
#define S2MPU16_LDO_STEP3		25000

/* Set LDO/BUCK soft time */
#define S2MPU16_ENABLE_TIME_LDO		135
#define S2MPU16_ENABLE_TIME_BUCK	130

/* PM mask */
#define BUCK_RAMP_MASK			(0x03)
#define BUCK_RAMP_UP_SHIFT		6

/* CFG_PM reg WTSR_EN Mask */
#define S2MPU16_WTSREN_MASK		MASK(1, 2)

/* LDO/BUCK output voltage control */
#define S2MPU16_LDO_VSEL_MASK1		0xFF	/* LDO_OUT */
#define S2MPU16_LDO_VSEL_MASK2		0x3F	/* LDO_CTRL */
#define S2MPU16_BUCK_VSEL_MASK		0xFF
#define S2MPU16_BUCK_N_VOLTAGES 	(S2MPU16_BUCK_VSEL_MASK + 1)

#define S2MPU16_ENABLE_SHIFT		6
#define S2MPU16_ENABLE_MASK		(0x03 << S2MPU16_ENABLE_SHIFT)
#define S2MPU16_SEL_VGPIO_ON		(0x01 << S2MPU16_ENABLE_SHIFT)

#define _BUCK(macro)		S2MPU16_BUCK##macro
#define _LDO(macro)		S2MPU16_LDO##macro

#define _REG(ctrl)		S2MPU16_PM1##ctrl
#define _TIME(macro)		S2MPU16_ENABLE_TIME##macro

#define _LDO_MIN(group)		S2MPU16_LDO_MIN##group
#define _LDO_STEP(group)	S2MPU16_LDO_STEP##group
#define _LDO_MASK(num)		S2MPU16_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPU16_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPU16_BUCK_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPU16_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU16_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU16_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#define LDO_DESC(_name, _id, g, v, v_m, e, t)	{	\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= _LDO_MASK(v_m) + 1,			\
	.vsel_reg	= v,					\
	.vsel_mask	= _LDO_MASK(v_m),			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU16_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#if IS_ENABLED(CONFIG_SOC_S5E8855)
enum s2mpu16_regulators {
	S2MPU16_BUCK1,
	S2MPU16_BUCK2,
	S2MPU16_BUCK3,
	S2MPU16_BUCK4,
	S2MPU16_BUCK5,
	S2MPU16_BUCK6,
	S2MPU16_BUCK7,
	S2MPU16_BUCK8,
	S2MPU16_BUCK9,
	S2MPU16_BUCK10,
	S2MPU16_BUCK_SR1,
	S2MPU16_BUCK_SR2,
	S2MPU16_LDO1,
	S2MPU16_LDO2,
	S2MPU16_LDO3,
	S2MPU16_LDO4,
	S2MPU16_LDO5,
	S2MPU16_LDO6,
	S2MPU16_LDO7,
	S2MPU16_LDO8,
	S2MPU16_LDO9,
	S2MPU16_LDO10,
	S2MPU16_LDO11,
	S2MPU16_LDO12,
	S2MPU16_LDO13,
	S2MPU16_LDO14,
	S2MPU16_LDO15,
	S2MPU16_REG_MAX,
};

static struct regulator_desc s2mpu16_regulators[] = {
	/* name, id, ops, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	/* BUCK 1S to 10S, SR1(BB), SR2 */
	//BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1S_OUT1), _REG(_BUCK1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2S_OUT1), _REG(_BUCK2S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3S_OUT1), _REG(_BUCK3S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4S_OUT2), _REG(_BUCK4S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), 1, _REG(_BUCK5S_OUT2), _REG(_BUCK5S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), 1, _REG(_BUCK6S_OUT1), _REG(_BUCK6S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK7", _BUCK(7), 1, _REG(_BUCK7S_OUT1), _REG(_BUCK7S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK8", _BUCK(8), 1, _REG(_BUCK8S_OUT1), _REG(_BUCK8S_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK9", _BUCK(9), 1, _REG(_BUCK9S_OUT1), _REG(_BUCK9S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK10", _BUCK(10), 1, _REG(_BUCK10S_OUT2), _REG(_BUCK10S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_SR1S_OUT1), _REG(_SR1S_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR2", _BUCK(_SR2), 3, _REG(_SR2S_OUT1), _REG(_SR2S_CTRL), _TIME(_BUCK)),

	/* LDO 1S to 15S */
	//LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1S_OUT1), 1, _REG(_LDO1S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2S_OUT), 1, _REG(_LDO2S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), 2, _REG(_LDO3S_CTRL), 2, _REG(_LDO3S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4S_OUT1), 1, _REG(_LDO4S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5S_OUT1), 1, _REG(_LDO5S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6), 1, _REG(_LDO6S_OUT), 1, _REG(_LDO6S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7), 1, _REG(_LDO7S_OUT), 1, _REG(_LDO7S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), 2, _REG(_LDO8S_CTRL), 2, _REG(_LDO8S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO9", _LDO(9), 2, _REG(_LDO9S_CTRL), 2, _REG(_LDO9S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10), 2, _REG(_LDO10S_CTRL), 2, _REG(_LDO10S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO11", _LDO(11), 3, _REG(_LDO11S_CTRL), 2, _REG(_LDO11S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12), 3, _REG(_LDO12S_CTRL), 2, _REG(_LDO12S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), 3, _REG(_LDO13S_CTRL), 2, _REG(_LDO13S_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO14", _LDO(14), 1, _REG(_LDO14S_OUT), 1, _REG(_LDO14S_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), 1, _REG(_LDO15S_OUT1), 1, _REG(_LDO15S_CTRL), _TIME(_LDO)),
};
#endif /* CONFIG_SOC_S5E8855 */
#endif /* __LINUX_S2MPU16_REGULATOR_H */
