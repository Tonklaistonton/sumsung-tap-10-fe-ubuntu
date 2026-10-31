/*
 * s2rp910-regulator.h - PMIC regulator for the S2RP910
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

#ifndef __LINUX_S2RP910_REGULATOR_H
#define __LINUX_S2RP910_REGULATOR_H

#include <linux/pmic/s2rp910-register.h>

#define REGULATOR_DEV_NAME		"s2rp910-regulator"

/* BUCK SR1R */
#define S2RP910_BUCK_MIN1	 300000
#define S2RP910_BUCK_STEP1	   6250

/* (LV) NLDOs -> L1R, L2R, L3R, L4R, L5R, L7R */
#define S2RP910_LDO_MIN1	 300000
#define s2RP910_LDO_STEP1	   6250
/* (IV) PLDOs -> L6R, L8R */
#define S2RP910_LDO_MIN2	1500000
#define S2RP910_LDO_STEP2	  12500

#define S2RP910_ENABLE_TIME_LDO		120
#define S2RP910_ENABLE_TIME_BUCK_SR	130

/* BUCK/LDO output voltage control */
#define S2RP910_LDO_VSEL_MASK1			0xFF /* LDO_OUT */
#define S2RP910_LDO_VSEL_MASK2			0x3F /* LDO_CTRL */
#define S2RP910_BUCK_VSEL_MASK			0xFF
#define S2RP910_BUCK_N_VOLTAGES			(S2RP910_BUCK_VSEL_MASK + 1)

/* BUCK/LDO Enable control [7:6] */
#define S2RP910_ENABLE_SHIFT			6
#define S2RP910_ENABLE_MASK			(0x03 << S2RP910_ENABLE_SHIFT)
#define S2RP910_SEL_VGPIO_ON			(0x01 << S2RP910_ENABLE_SHIFT)

#define _BUCK(macro)		S2RP910_BUCK##macro
#define _LDO(macro)		S2RP910_LDO##macro
#define _BUCK_REG(ctrl)		S2RP910_BUCK##ctrl
#define _LDO_REG(ctrl)		S2RP910_LDO##ctrl
#define _TIME(macro)		S2RP910_ENABLE_TIME##macro
#define _LDO_MIN(group)		S2RP910_LDO_MIN##group
#define _LDO_STEP(group)	S2RP910_LDO_STEP##group
#define _LDO_MASK(num)		S2RP910_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2RP910_BUCK_MIN##group
#define _BUCK_STEP(group)	S2RP910_BUCK_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2RP910_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2RP910_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2RP910_ENABLE_MASK,			\
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
	.enable_mask	= S2RP910_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#if IS_ENABLED(CONFIG_SOC_S5E9955)
/* S2RP910 regulator ids */
enum s2rp910_regulators {
	S2RP910_BUCK_SR1,
	S2RP910_LDO1,
	S2RP910_LDO2,
	S2RP910_LDO3,
	S2RP910_LDO4,
	S2RP910_LDO5,
	S2RP910_LDO6,
	S2RP910_LDO7,
	S2RP910_LDO8,
	S2RP910_REG_MAX,
};

static struct regulator_desc s2rp910_1_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// SR1R
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 1, _BUCK_REG(_SR1_OUT1), _BUCK_REG(_SR1_CTRL), _TIME(_BUCK_SR)),
	// LDO 1R ~ 8R
	//LDO_DESC("LDO1", _LDO(1), 1, _LDO_REG(_LDO1_OUT1), 1, _LDO_REG(_LDO1_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), 1, _LDO_REG(_LDO2_OUT1), 1, _LDO_REG(_LDO2_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), 2, _LDO_REG(_LDO3_OUT1), 1, _LDO_REG(_LDO3_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), 1, _LDO_REG(_LDO4_OUT1), 1, _LDO_REG(_LDO4_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), 1, _LDO_REG(_LDO5_OUT1), 1, _LDO_REG(_LDO5_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6), 2, _LDO_REG(_LDO6_CTRL), 2, _LDO_REG(_LDO6_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7), 1, _LDO_REG(_LDO7_OUT1), 1, _LDO_REG(_LDO7_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), 2, _LDO_REG(_LDO8_CTRL), 2, _LDO_REG(_LDO8_CTRL), _TIME(_LDO)),
};

static struct regulator_desc s2rp910_2_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	// SR1R
	//BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 1, _BUCK_REG(_SR1_OUT1), _BUCK_REG(_SR1_CTRL), _TIME(_BUCK_SR)),
	// LDO 1R ~ 8R
	//LDO_DESC("LDO1", _LDO(1), 1, _LDO_REG(_LDO1_OUT1), 1, _LDO_REG(_LDO1_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO2", _LDO(2), 1, _LDO_REG(_LDO2_OUT1), 1, _LDO_REG(_LDO2_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), 2, _LDO_REG(_LDO3_OUT1), 1, _LDO_REG(_LDO3_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), 1, _LDO_REG(_LDO4_OUT1), 1, _LDO_REG(_LDO4_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), 1, _LDO_REG(_LDO5_OUT1), 1, _LDO_REG(_LDO5_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO6", _LDO(6), 2, _LDO_REG(_LDO6_CTRL), 2, _LDO_REG(_LDO6_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO7", _LDO(7), 1, _LDO_REG(_LDO7_OUT1), 1, _LDO_REG(_LDO7_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO8", _LDO(8), 2, _LDO_REG(_LDO8_CTRL), 2, _LDO_REG(_LDO8_CTRL), _TIME(_LDO)),
};

#endif /* CONFIG_SOC_S5E9955) */
#endif /* __LINUX_S2RP910_REGULATOR_H */
