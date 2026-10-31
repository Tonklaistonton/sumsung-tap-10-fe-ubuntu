/*
 * s2se910-regulator.h - PMIC regulator for the S2SE910
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

#ifndef __LINUX_S2SE910_REGULATOR_H
#define __LINUX_S2SE910_REGULATOR_H

#include <linux/pmic/s2se910-register.h>

#define REGULATOR_DEV_NAME		"s2se910-regulator"

/* BUCK_SR1M */
#define S2SE910_BUCK_MIN1	700000
#define S2SE910_BUCK_STEP1	6250
/* BUCK_BOOST*/
#define S2SE910_BB_MIN		2600000
#define S2SE910_BB_STEP		12500
/* PLDOs, LVP LDOs 1M~5M/7M/12M~19M/26M */
#define S2SE910_LDO_MIN1	1500000
#define S2SE910_LDO_STEP1	12500
/* PLDOs MVP LDOs 8M~11M/20M~25M */
#define S2SE910_LDO_MIN2	1800000
#define S2SE910_LDO_STEP2	25000
/* PLDOs MVP LDO6M */
#define S2SE910_LDO_MIN3	700000
#define S2SE910_LDO_STEP3	25000

/* soft start time */
#define S2SE910_ENABLE_TIME_LDO		120
#define S2SE910_ENABLE_TIME_BUCK	120
#define S2SE910_ENABLE_TIME_BB		130

/* LDO/BUCK output voltage control */
#define S2SE910_LDO_VSEL_MASK		0x3F	/* LDO_CTRL */
#define S2SE910_BUCK_VSEL_MASK		0xFF	/* BUCK_OUT */
#define S2SE910_BUCK_N_VOLTAGES		(S2SE910_BUCK_VSEL_MASK + 1)
#define S2SE910_BB_VSEL_MASK		0x7F	/* BB_OUT */

/* Buck/LDO Enable control [7:6] */
#define S2SE910_ENABLE_SHIFT		6
#define S2SE910_ENABLE_MASK		(0x03 << S2SE910_ENABLE_SHIFT)
#define S2SE910_SEL_VGPIO_ON		(0x01 << S2SE910_ENABLE_SHIFT)

#define _BUCK(macro)		S2SE910_BUCK##macro
#define _LDO(macro)		S2SE910_LDO##macro
#define _BB(macro)		S2SE910_BB##macro
#define _REG(ctrl)		S2SE910_PM1##ctrl
#define _TIME(macro)		S2SE910_ENABLE_TIME##macro
#define _LDO_MIN(group)		S2SE910_LDO_MIN##group
#define _LDO_STEP(group)	S2SE910_LDO_STEP##group
#define _LDO_MASK(num)		S2SE910_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2SE910_BUCK_MIN##group
#define _BUCK_STEP(group)	S2SE910_BUCK_STEP##group
#define _BB_MIN(group)		S2SE910_BB_MIN##group
#define _BB_STEP(group)		S2SE910_BB_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t) {			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2SE910_BUCK_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2SE910_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2SE910_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#define LDO_DESC(_name, _id, g, v, e, t) {			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2SE910_LDO_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2SE910_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2SE910_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#define BB_DESC(_name, _id, g, v, e, t)	{			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(),				\
	.uV_step	= _BB_STEP(),				\
	.n_voltages	= S2SE910_BB_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2SE910_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2SE910_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
enum s2se910_regulators {
	S2SE910_BUCK_SR1,
	S2SE910_BB1,
	S2SE910_BB2,
	S2SE910_LDO1,
	//S2SE910_LDO2,
	S2SE910_LDO3,
	S2SE910_LDO4,
	S2SE910_LDO5,
	S2SE910_LDO6,
	S2SE910_LDO7,
	S2SE910_LDO8,
	S2SE910_LDO9,
	S2SE910_LDO10,
	S2SE910_LDO11,
	S2SE910_LDO12,
	S2SE910_LDO13,
	S2SE910_LDO14,
	S2SE910_LDO15,
	S2SE910_LDO16,
	S2SE910_LDO17,
	S2SE910_LDO18,
	S2SE910_LDO19,
	S2SE910_LDO20,
	S2SE910_LDO21,
	S2SE910_LDO22,
	S2SE910_LDO23,
	//S2SE910_LDO24,
	//S2SE910_LDO25,
	S2SE910_LDO26,

	S2SE910_REG_MAX,
};

static struct regulator_desc s2se910_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// BUCK_SR1
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 1, _REG(_BUCK_SR1_OUT1), _REG(_BUCK_SR1_CTRL), _TIME(_BUCK)),
	// BUCK BOOST1 ~ 2
	BB_DESC("BUCKB1", _BB(1), 1, _REG(_BB1_OUT1), _REG(_BB1_CTRL), _TIME(_BB)),
	BB_DESC("BUCKB2", _BB(2), 1, _REG(_BB2_OUT1), _REG(_BB2_CTRL), _TIME(_BB)),
	// LDO 1 ~ 26
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_CTRL), _REG(_LDO1_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_CTRL), _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_CTRL), _REG(_LDO3_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4_CTRL), _REG(_LDO4_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5_CTRL), _REG(_LDO5_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), 3, _REG(_LDO6_CTRL), _REG(_LDO6_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), 1, _REG(_LDO7_CTRL), _REG(_LDO7_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), 2, _REG(_LDO8_CTRL), _REG(_LDO8_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), 2, _REG(_LDO9_CTRL), _REG(_LDO9_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), 2, _REG(_LDO10_CTRL), _REG(_LDO10_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), 2, _REG(_LDO11_CTRL), _REG(_LDO11_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), 1, _REG(_LDO12_CTRL), _REG(_LDO12_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), 1, _REG(_LDO13_CTRL), _REG(_LDO13_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), 1, _REG(_LDO14_CTRL), _REG(_LDO14_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), 1, _REG(_LDO15_CTRL), _REG(_LDO15_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), 1, _REG(_LDO16_CTRL), _REG(_LDO16_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), 1, _REG(_LDO17_CTRL), _REG(_LDO17_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), 1, _REG(_LDO18_CTRL), _REG(_LDO18_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), 1, _REG(_LDO19_CTRL), _REG(_LDO19_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO20", _LDO(20), 2, _REG(_LDO20_CTRL), _REG(_LDO20_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO21", _LDO(21), 2, _REG(_LDO21_CTRL), _REG(_LDO21_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO22", _LDO(22), 2, _REG(_LDO22_CTRL), _REG(_LDO22_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO23", _LDO(23), 2, _REG(_LDO23_CTRL), _REG(_LDO23_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO24", _LDO(24), 2, _REG(_LDO24_CTRL), _REG(_LDO24_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO25", _LDO(25), 2, _REG(_LDO25_CTRL), _REG(_LDO25_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO26", _LDO(26), 1, _REG(_LDO26_CTRL), _REG(_LDO26_CTRL), _TIME(_LDO)),
};
#endif /* CONFIG_SOC_S5E9955 */
#endif /* __LINUX_S2SE910_REGULATOR_H */
