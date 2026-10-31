/*
 * s2mpu15-regulator.h - PMIC regulator for the S2MPU15
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

#ifndef __LINUX_S2MPU15_REGULATOR_H
#define __LINUX_S2MPU15_REGULATOR_H

#include <linux/pmic/s2mpu15-register.h>

#define REGULATOR_DEV_NAME		"s2mpu15-regulator"

/* BUCKs 1M to 8M */
#define S2MPU15_BUCK_MIN1	300000
#define S2MPU15_BUCK_STEP1	6250
/* BUCKs SR1M to SR3M  */
#define S2MPU15_BUCK_MIN2	300000
#define S2MPU15_BUCK_STEP2	6250
/* BUCKs SR4M */
#define S2MPU15_BUCK_MIN3	700000
#define S2MPU15_BUCK_STEP3	6250
/* (LV) N/PLDOs 1M/3M/5M/8M/9M/12M~15M/17M/19M~21M/23M/25M/27M/31M/34M/35M */
#define S2MPU15_LDO_MIN1	300000
#define S2MPU15_LDO_STEP1	6250
/* (IV) PLDOs 4M/7M/16M/22M/24M/26M/28M/30M */
#define S2MPU15_LDO_MIN2	1500000
#define S2MPU15_LDO_STEP2	12500
/* PLDOs 18M */
#define S2MPU15_LDO_MIN3	700000
#define S2MPU15_LDO_STEP3	25000
/* (MV) PLDOs 2M/6M/10M/11M/29M/32M/33M */
#define S2MPU15_LDO_MIN4	1800000
#define S2MPU15_LDO_STEP4	25000

/* soft start time */
#define S2MPU15_ENABLE_TIME_LDO		128
#define S2MPU15_ENABLE_TIME_BUCK	130
#define S2MPU15_ENABLE_TIME_BUCK_SR	130

/* PMIC 1 mask */
#define BUCK_RAMP_MASK			(0x03)
#define BUCK_RAMP_UP_SHIFT		6

/* LDO/BUCK output voltage control */
#define S2MPU15_LDO_VSEL_MASK1		0xFF	/* LDO_OUT  */
#define S2MPU15_LDO_VSEL_MASK2		0x3F	/* LDO_CTRL */
#define S2MPU15_BUCK_VSEL_MASK		0xFF	/* BUCK_OUT */
#define S2MPU15_BUCK_N_VOLTAGES		(S2MPU15_BUCK_VSEL_MASK + 1)

/* Buck/LDO Enable control [7:6] */
#define S2MPU15_ENABLE_SHIFT		0x06
#define S2MPU15_ENABLE_MASK		(0x03 << S2MPU15_ENABLE_SHIFT)
#define S2MPU15_SEL_VGPIO_ON		(0x01 << S2MPU15_ENABLE_SHIFT)

#define _BUCK(macro)		S2MPU15_BUCK##macro
#define _LDO(macro)		S2MPU15_LDO##macro
#define _BB(macro)		S2MPU15_BB##macro
#define _REG(ctrl)		S2MPU15_PM1##ctrl
#define _TIME(macro)		S2MPU15_ENABLE_TIME##macro
#define _LDO_MIN(group)		S2MPU15_LDO_MIN##group
#define _LDO_STEP(group)	S2MPU15_LDO_STEP##group
#define _LDO_MASK(num)		S2MPU15_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPU15_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPU15_BUCK_STEP##group
#define _BB_MIN(group)		S2MPU15_BB_MIN##group
#define _BB_STEP(group)		S2MPU15_BB_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t) {			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPU15_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU15_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU15_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#define LDO_DESC(_name, _id, g, v, v_m, e, t)	{		\
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
	.enable_mask	= S2MPU15_ENABLE_MASK,			\
	.enable_time	= t,					\
}
#if 0
#define BB_DESC(_name, _id, g, v, e, t)	{			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(),				\
	.uV_step	= _BB_STEP(),				\
	.n_voltages	= S2MPU15_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPU15_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPU15_ENABLE_MASK,			\
	.enable_time	= t,					\
}
#endif

#if IS_ENABLED(CONFIG_SOC_S5E8855)
enum s2mpu15_regulators {
	S2MPU15_BUCK1,
	S2MPU15_BUCK2,
	S2MPU15_BUCK3,
	S2MPU15_BUCK4,
	S2MPU15_BUCK5,
	S2MPU15_BUCK6,
	S2MPU15_BUCK7,
	S2MPU15_BUCK8,
	S2MPU15_BUCK_SR1,
	S2MPU15_BUCK_SR2,
	S2MPU15_BUCK_SR3,
	S2MPU15_BUCK_SR4,
	S2MPU15_LDO1,
	S2MPU15_LDO2,
	S2MPU15_LDO3,
	S2MPU15_LDO4,
	S2MPU15_LDO5,
	S2MPU15_LDO6,
	S2MPU15_LDO7,
	S2MPU15_LDO8,
	S2MPU15_LDO9,
	S2MPU15_LDO10,
	S2MPU15_LDO11,
	S2MPU15_LDO12,
	S2MPU15_LDO13,
	S2MPU15_LDO14,
	S2MPU15_LDO15,
	S2MPU15_LDO16,
	S2MPU15_LDO17,
	S2MPU15_LDO18,
	S2MPU15_LDO19,
	S2MPU15_LDO20,
	S2MPU15_LDO21,
	S2MPU15_LDO22,
	S2MPU15_LDO23,
	S2MPU15_LDO24,
	S2MPU15_LDO25,
	S2MPU15_LDO26,
	S2MPU15_LDO27,
	S2MPU15_LDO28,
	S2MPU15_LDO29,
	S2MPU15_LDO30,
	S2MPU15_LDO31,
	S2MPU15_LDO32,
	S2MPU15_LDO33,
	S2MPU15_LDO34,
	S2MPU15_LDO35,
	S2MPU15_REG_MAX,
};

static struct regulator_desc s2mpu15_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// BUCK 1M to 8M, SR1M to SR4M
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1M_OUT2), _REG(_BUCK1M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2M_OUT1), _REG(_BUCK2M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3M_OUT2), _REG(_BUCK3M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4M_OUT2), _REG(_BUCK4M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), 1, _REG(_BUCK5M_OUT1), _REG(_BUCK5M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK6", _BUCK(6), 1, _REG(_BUCK6M_OUT1), _REG(_BUCK6M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK7", _BUCK(7), 1, _REG(_BUCK7M_OUT1), _REG(_BUCK7M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK8", _BUCK(8), 1, _REG(_BUCK8M_OUT1), _REG(_BUCK8M_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_BUCK_SR1M_OUT1), _REG(_BUCK_SR1M_CTRL), _TIME(_BUCK_SR)),
	BUCK_DESC("BUCK_SR2", _BUCK(_SR2), 2, _REG(_BUCK_SR2M_OUT1), _REG(_BUCK_SR2M_CTRL), _TIME(_BUCK_SR)),
	BUCK_DESC("BUCK_SR3", _BUCK(_SR3), 2, _REG(_BUCK_SR3M_OUT1), _REG(_BUCK_SR3M_CTRL), _TIME(_BUCK_SR)),
	BUCK_DESC("BUCK_SR4", _BUCK(_SR4), 3, _REG(_BUCK_SR4M_OUT1), _REG(_BUCK_SR4M_CTRL), _TIME(_BUCK_SR)),

	// LDO 1M to 35M
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1M_OUT), 1, _REG(_LDO1M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 4, _REG(_LDO2M_CTRL), 2, _REG(_LDO2M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3M_OUT), 1, _REG(_LDO3M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), 2, _REG(_LDO4M_CTRL), 2, _REG(_LDO4M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5M_OUT), 1, _REG(_LDO5M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), 4, _REG(_LDO6M_CTRL), 2, _REG(_LDO6M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), 2, _REG(_LDO7M_CTRL), 2, _REG(_LDO7M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), 1, _REG(_LDO8M_OUT1), 1, _REG(_LDO8M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO9", _LDO(9), 1, _REG(_LDO9M_OUT), 1, _REG(_LDO9M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10), 4, _REG(_LDO10M_CTRL), 2, _REG(_LDO10M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO11", _LDO(11), 4, _REG(_LDO11M_CTRL), 2, _REG(_LDO11M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12), 1, _REG(_LDO12M_OUT1), 1, _REG(_LDO12M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), 1, _REG(_LDO13M_OUT), 1, _REG(_LDO13M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO14", _LDO(14), 1, _REG(_LDO14M_OUT), 1, _REG(_LDO14M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO15", _LDO(15), 1, _REG(_LDO15M_OUT), 1, _REG(_LDO15M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), 2, _REG(_LDO16M_CTRL), 2, _REG(_LDO16M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), 1, _REG(_LDO17M_OUT), 1, _REG(_LDO17M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), 3, _REG(_LDO18M_CTRL), 2, _REG(_LDO18M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), 1, _REG(_LDO19M_OUT1), 1, _REG(_LDO19M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), 1, _REG(_LDO20M_OUT1), 1, _REG(_LDO20M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO21", _LDO(21), 1, _REG(_LDO21M_OUT), 1, _REG(_LDO21M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), 2, _REG(_LDO22M_CTRL), 2, _REG(_LDO22M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), 1, _REG(_LDO23M_OUT), 1, _REG(_LDO23M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO24", _LDO(24), 2, _REG(_LDO24M_CTRL), 2, _REG(_LDO24M_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO25", _LDO(25), 1, _REG(_LDO25M_OUT), 1, _REG(_LDO25M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), 2, _REG(_LDO26M_CTRL), 2, _REG(_LDO26M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO27", _LDO(27), 1, _REG(_LDO27M_OUT), 1, _REG(_LDO27M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO28", _LDO(28), 2, _REG(_LDO28M_CTRL), 2, _REG(_LDO28M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO29", _LDO(29), 4, _REG(_LDO29M_CTRL), 2, _REG(_LDO29M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO30", _LDO(30), 2, _REG(_LDO30M_CTRL), 2, _REG(_LDO30M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO31", _LDO(31), 1, _REG(_LDO31M_OUT), 1, _REG(_LDO31M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO32", _LDO(32), 4, _REG(_LDO32M_CTRL), 2, _REG(_LDO32M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO33", _LDO(33), 4, _REG(_LDO33M_CTRL), 2, _REG(_LDO33M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO34", _LDO(34), 1, _REG(_LDO34M_OUT), 1, _REG(_LDO34M_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO35", _LDO(35), 1, _REG(_LDO35M_OUT), 1, _REG(_LDO35M_CTRL), _TIME(_LDO)),
};
#endif /* CONFIG_SOC_S5E8855 */
#endif /* __LINUX_S2MPU15_REGULATOR_H */
