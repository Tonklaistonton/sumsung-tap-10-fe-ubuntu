/*
 * s2mpa05-regulator.h - PMIC regulator for the S2MPA05
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

#ifndef __LINUX_S2MPA05_REGULATOR_H
#define __LINUX_S2MPA05_REGULATOR_H

#include <linux/pmic/s2mpa05-register.h>

#define REGULATOR_DEV_NAME		"s2mpa05-regulator"

/* BUCKs 1S to 4S */
#define S2MPA05_BUCK_MIN1		300000
#define S2MPA05_BUCK_STEP1		6250
/* LDOs 1S to 3S */
#define S2MPA05_LDO_MIN1		1800000
#define S2MPA05_LDO_STEP1		25000
/* LDO 4S */
#define S2MPA05_LDO_MIN2		300000
#define S2MPA05_LDO_STEP2		6250

/* Set LDO/BUCK soft time */
#define S2MPA05_ENABLE_TIME_LDO		128
#define S2MPA05_ENABLE_TIME_BUCK	130

/* LDO/BUCK output voltage control */
#define S2MPA05_LDO_VSEL_MASK1		0xFF	/* LDO_OUT */
#define S2MPA05_LDO_VSEL_MASK2		0x3F	/* LDO_CTRL */
#define S2MPA05_BUCK_VSEL_MASK		0xFF
#define S2MPA05_BUCK_N_VOLTAGES 	(S2MPA05_BUCK_VSEL_MASK + 1)

/* BUCK/LDO Enable control[7:6] */
#define S2MPA05_ENABLE_SHIFT		6
#define S2MPA05_ENABLE_MASK		(0x03 << S2MPA05_ENABLE_SHIFT)
#define S2MPA05_SEL_VGPIO_ON		(0x01 << S2MPA05_ENABLE_SHIFT)

#define _BUCK(macro)		S2MPA05_BUCK##macro
#define _LDO(macro)		S2MPA05_LDO##macro
#define _REG(ctrl)		S2MPA05_PM1##ctrl
#define _TIME(macro)		S2MPA05_ENABLE_TIME##macro
#define _LDO_MIN(group)		S2MPA05_LDO_MIN##group
#define _LDO_STEP(group)	S2MPA05_LDO_STEP##group
#define _LDO_MASK(num)		S2MPA05_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPA05_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPA05_BUCK_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPA05_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPA05_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPA05_ENABLE_MASK,			\
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
	.enable_mask	= S2MPA05_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
/* s2mpa05 Regulator ids */
enum s2mpa05_regulators_9945 {
	S2MPA05_BUCK1,
	S2MPA05_BUCK2,
	S2MPA05_BUCK3,
	S2MPA05_BUCK4,
	S2MPA05_LDO1,
	S2MPA05_LDO2,
	S2MPA05_LDO3,
	//S2MPA05_LDO4,
	S2MPA05_REG_MAX,
};

static struct regulator_desc s2mpa05_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	/* BUCK 1S to 4S */
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1_OUT2), _REG(_BUCK1_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2_OUT1), _REG(_BUCK2_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3_OUT1), _REG(_BUCK3_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4_OUT1), _REG(_BUCK4_CTRL), _TIME(_BUCK)),
	/* LDO 1S to 4S */
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_CTRL), 2, _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_CTRL), 2, _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_CTRL), 2, _REG(_LDO3_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), 2, _REG(_LDO4_OUT1), 1, _REG(_LDO4_CTRL), _TIME(_LDO)),
};
#elif IS_ENABLED(CONFIG_SOC_S5E9955)
/* s2mpa05 Regulator ids */
enum s2mpa05_regulators_9955 {
	S2MPA05_BUCK1,
	S2MPA05_BUCK2,
	S2MPA05_BUCK3,
	S2MPA05_BUCK4,
	S2MPA05_LDO1,
	S2MPA05_LDO2,
	S2MPA05_LDO3,
	S2MPA05_LDO4,
	S2MPA05_REG_MAX,
};

static struct regulator_desc s2mpa05_regulators[] = {
	/* name, id, group, vsel_reg, vsel_mask, enable_reg, ramp_delay */
	/* BUCK 1S to 4S */
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1_OUT1), _REG(_BUCK1_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2_OUT1), _REG(_BUCK2_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3_OUT1), _REG(_BUCK3_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4_OUT2), _REG(_BUCK4_CTRL), _TIME(_BUCK)),
	/* LDO 1S to 4S */
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_CTRL), 2, _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_CTRL), 2, _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_CTRL), 2, _REG(_LDO3_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), 2, _REG(_LDO4_OUT1), 1, _REG(_LDO4_CTRL), _TIME(_LDO)),
};
#endif /* CONFIG_SOC_S5E9945,9955 */
#endif /* __LINUX_S2MPA05_REGULATOR_H */
