/*
 * s2mpm07-regulator.h - PMIC regulator for the S2MPM07
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

#ifndef __LINUX_S2MPM07_REGULATOR_H
#define __LINUX_S2MPM07_REGULATOR_H

#include <linux/pmic/s2mpm07-register.h>

#define REGULATOR_DEV_NAME		"s2mpm07-regulator"

/* BUCKs 1R */
#define S2MPM07_BUCK_MIN1	300000
#define S2MPM07_BUCK_STEP1	6250
/* BUCKs SR1R */
#define S2MPM07_BUCK_MIN2	300000
#define S2MPM07_BUCK_STEP2	6250
/* (LV) NLDOs -> L1R, L2R, L3R, L4R, L5R, L7R, L8R, L9R, L12R, L13R, L14R, L15R */
#define S2MPM07_LDO_MIN1	300000
#define S2MPM07_LDO_STEP1	6250
/* (IV) PLDOs -> L6R, L10R, L16R, L17R */
#define S2MPM07_LDO_MIN2	1500000
#define S2MPM07_LDO_STEP2	12500
/* (MV) PLDOs L11R, L18R, L19R, L20R, L21R, L22R, L23R */
#define S2MPM07_LDO_MIN3	1800000
#define S2MPM07_LDO_STEP3	25000

/* soft start time */
#define S2MPM07_ENABLE_TIME_LDO		128
#define S2MPM07_ENABLE_TIME_BUCK	130
#define S2MPM07_ENABLE_TIME_BUCK_SR	130

#define _BUCK(macro)		S2MPM07_BUCK##macro
#define _LDO(macro)		S2MPM07_LDO##macro
#define _BB(macro)		S2MPM07_BB##macro
#define _REG(ctrl)		S2MPM07_PM1##ctrl
#define _TIME(macro)		S2MPM07_ENABLE_TIME##macro
#define _LDO_MIN(group)		S2MPM07_LDO_MIN##group
#define _LDO_STEP(group)	S2MPM07_LDO_STEP##group
#define _LDO_MASK(num)		S2MPM07_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPM07_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPM07_BUCK_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPM07_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPM07_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPM07_ENABLE_MASK,			\
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
	.enable_mask	= S2MPM07_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
/* S2MPM07 regulator ids */
enum s2mpm07_regulators_9945 {
	S2MPM07_BUCK1,
	S2MPM07_BUCK_SR1,
	//S2MPM07_LDO1,
	//S2MPM07_LDO2,
	//S2MPM07_LDO3,
	//S2MPM07_LDO4,
	//S2MPM07_LDO5,
	//S2MPM07_LDO6,
	//S2MPM07_LDO7,
	//S2MPM07_LDO8,
	//S2MPM07_LDO9,
	//S2MPM07_LDO10,
	//S2MPM07_LDO11,
	//S2MPM07_LDO12,
	//S2MPM07_LDO13,
	//S2MPM07_LDO14,
	//S2MPM07_LDO15,
	//S2MPM07_LDO16,
	//S2MPM07_LDO17,
	S2MPM07_LDO18,
	S2MPM07_LDO19,
	//S2MPM07_LDO20,
	//S2MPM07_LDO21,
	S2MPM07_REG_MAX,
};

static struct regulator_desc s2mpm07_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// BUCK 1R, SR1R
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1R_OUT1), _REG(_BUCK1R_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_BUCK_SR1R_OUT1), _REG(_BUCK_SR1R_CTRL), _TIME(_BUCK_SR)),
	// LDO 1R ~ 21R
	//LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1R_OUT), 1, _REG(_LDO1R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2R_OUT1), 1, _REG(_LDO2R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), 2, _REG(_LDO3R_OUT), 1, _REG(_LDO3R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4R_OUT), 1, _REG(_LDO4R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5R_OUT), 1, _REG(_LDO5R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6), 2, _REG(_LDO6R_CTRL), 2, _REG(_LDO6R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7), 1, _REG(_LDO7R_OUT), 1, _REG(_LDO7R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), 1, _REG(_LDO8R_OUT), 1, _REG(_LDO8R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO9", _LDO(9), 1, _REG(_LDO9R_OUT), 1, _REG(_LDO9R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO10", _LDO(10), 2, _REG(_LDO10R_CTRL), 2, _REG(_LDO10R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO11", _LDO(11), 3, _REG(_LDO11R_CTRL), 2, _REG(_LDO11R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO12", _LDO(12), 1, _REG(_LDO12R_OUT), 1, _REG(_LDO12R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO13", _LDO(13), 1, _REG(_LDO13R_OUT), 1, _REG(_LDO13R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO14", _LDO(14), 1, _REG(_LDO14R_OUT), 1, _REG(_LDO14R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO15", _LDO(15), 1, _REG(_LDO15R_OUT), 1, _REG(_LDO15R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO16", _LDO(16), 2, _REG(_LDO16R_CTRL), 2, _REG(_LDO16R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO17", _LDO(17), 2, _REG(_LDO17R_CTRL), 2, _REG(_LDO17R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO18", _LDO(18), 3, _REG(_LDO18R_CTRL), 2, _REG(_LDO18R_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO19", _LDO(19), 3, _REG(_LDO19R_CTRL), 2, _REG(_LDO19R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO20", _LDO(20), 3, _REG(_LDO20R_CTRL), 2, _REG(_LDO20R_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO21", _LDO(21), 3, _REG(_LDO21R_CTRL), 2, _REG(_LDO21R_CTRL), _TIME(_LDO)),
};

#endif /* CONFIG_SOC_S5E9945) */

#endif /* __LINUX_S2MPM07_REGULATOR_H */
