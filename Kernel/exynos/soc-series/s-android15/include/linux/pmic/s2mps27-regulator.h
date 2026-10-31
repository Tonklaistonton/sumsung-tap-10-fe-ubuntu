/*
 * s2mps27-regulator.h - PMIC regulator for the S2MPS27
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

#ifndef __LINUX_S2MPS27_REGULATOR_H
#define __LINUX_S2MPS27_REGULATOR_H

#include <linux/pmic/s2mps27-register.h>

#define REGULATOR_DEV_NAME		"s2mps27-regulator"

/* BUCK_SR1M */
#define S2MPS27_BUCK_MIN1	700000
#define S2MPS27_BUCK_STEP1	6250
/* BUCK_BOOST*/
#define S2MPS27_BB_MIN		2600000
#define S2MPS27_BB_STEP		12500
/* (IV_PMOS) LDOs 1M/2M/3M/4M/5M/7M/12M/13M/14M/15M/16M/17M/18M */
#define S2MPS27_LDO_MIN1	1500000
#define S2MPS27_LDO_STEP1	12500
/* (MV_PMOS) LDO 6M - skip on PMIC EVT0.0 */
#define S2MPS27_LDO_MIN2	700000
#define S2MPS27_LDO_STEP2	25000
/* (MV_PMOS) LDOs 8M/9M/10M/11M */
#define S2MPS27_LDO_MIN3	1800000
#define S2MPS27_LDO_STEP3	25000

/* soft start time */
#define S2MPS27_ENABLE_TIME_LDO		120
#define S2MPS27_ENABLE_TIME_BUCK	120
#define S2MPS27_ENABLE_TIME_BB		120

#define _BUCK(macro)		S2MPS27_BUCK##macro
#define _LDO(macro)		S2MPS27_LDO##macro
#define _BB(macro)		S2MPS27_BB##macro
#define _REG(ctrl)		S2MPS27_PM1##ctrl
#define _TIME(macro)		S2MPS27_ENABLE_TIME##macro
#define _LDO_MIN(group)		S2MPS27_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS27_LDO_STEP##group
#define _LDO_MASK(num)		S2MPS27_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPS27_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS27_BUCK_STEP##group
#define _BB_MIN(group)		S2MPS27_BB_MIN##group
#define _BB_STEP(group)		S2MPS27_BB_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t) {			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS27_BUCK_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS27_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS27_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#define LDO_DESC(_name, _id, g, v, e, t) {			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _LDO_MIN(g),				\
	.uV_step	= _LDO_STEP(g),				\
	.n_voltages	= S2MPS27_LDO_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS27_LDO_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS27_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#define BB_DESC(_name, _id, g, v, e, t)	{			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(),				\
	.uV_step	= _BB_STEP(),				\
	.n_voltages	= S2MPS27_BB_VSEL_MASK + 1,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS27_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS27_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
enum s2mps27_regulators_9945{
	S2MPS27_BUCK_SR1,
	S2MPS27_BB,
	S2MPS27_LDO1,
	S2MPS27_LDO2,
	S2MPS27_LDO3,
	S2MPS27_LDO4,
	S2MPS27_LDO5,
	S2MPS27_LDO6,
	S2MPS27_LDO7,
	S2MPS27_LDO8,
	S2MPS27_LDO9,
	S2MPS27_LDO10,
	S2MPS27_LDO11,
	S2MPS27_LDO12,
	S2MPS27_LDO13,
	S2MPS27_LDO14,
	S2MPS27_LDO15,
	S2MPS27_LDO16,
	S2MPS27_LDO17,
	S2MPS27_LDO18,
	S2MPS27_REG_MAX,
};

static struct regulator_desc s2mps27_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// BUCK_SR1
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 1, _REG(_BUCK_SR1_OUT1), _REG(_BUCK_SR1_CTRL), _TIME(_BUCK)),
	// BUCK BOOST
	BB_DESC("BUCKB", _BB(), 1, _REG(_BB_OUT1), _REG(_BB_CTRL), _TIME(_BB)),
	// LDO 1 ~ 18
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_CTRL), _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_CTRL), _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_CTRL), _REG(_LDO3_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4_CTRL), _REG(_LDO4_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5_CTRL), _REG(_LDO5_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO6", _LDO(6), 2, _REG(_LDO6_CTRL), _REG(_LDO6_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO7", _LDO(7), 1, _REG(_LDO7_CTRL), _REG(_LDO7_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO8", _LDO(8), 3, _REG(_LDO8_CTRL), _REG(_LDO8_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO9", _LDO(9), 3, _REG(_LDO9_CTRL), _REG(_LDO9_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO10", _LDO(10), 3, _REG(_LDO10_CTRL), _REG(_LDO10_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO11", _LDO(11), 3, _REG(_LDO11_CTRL), _REG(_LDO11_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO12", _LDO(12), 1, _REG(_LDO12_CTRL), _REG(_LDO12_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO13", _LDO(13), 1, _REG(_LDO13_CTRL), _REG(_LDO13_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO14", _LDO(14), 1, _REG(_LDO14_CTRL), _REG(_LDO14_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO15", _LDO(15), 1, _REG(_LDO15_CTRL), _REG(_LDO15_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO16", _LDO(16), 1, _REG(_LDO16_CTRL), _REG(_LDO16_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO17", _LDO(17), 1, _REG(_LDO17_CTRL), _REG(_LDO17_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), 1, _REG(_LDO18_CTRL), _REG(_LDO18_CTRL), _TIME(_LDO)),
};
#endif /* CONFIG_SOC_S5E9945 */

#endif /* __LINUX_S2MPS27_REGULATOR_H */
