/*
 * s2mps28-regulator.h - PMIC regulator for the S2MPS28
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

#ifndef __LINUX_S2MPS28_REGULATOR_H
#define __LINUX_S2MPS28_REGULATOR_H

#include <linux/pmic/s2mps28-register.h>

#define REGULATOR_DEV_NAME		"s2mps28-regulator"

/* BUCKs 1S ~ 5S  */
#define S2MPS28_BUCK_MIN1		300000
#define S2MPS28_BUCK_STEP1		6250
/* BUCK SR1S */
#define S2MPS28_BUCK_MIN2		300000
#define S2MPS28_BUCK_STEP2		6250
/* (LV) LDOs 1S ~ 5S */
#define S2MPS28_LDO_MIN1		300000
#define S2MPS28_LDO_STEP1		6250

/* Set LDO/BUCK soft time */
#define S2MPS28_ENABLE_TIME_LDO		120
#define S2MPS28_ENABLE_TIME_BUCK	130
#define S2MPS28_ENABLE_TIME_BUCK_SR	130
#define S2MPS28_ENABLE_TIME_BB		160

#define _BUCK(macro)		S2MPS28_BUCK##macro
#define _LDO(macro)		S2MPS28_LDO##macro
#define _BB(macro)              S2MPS28_BB##macro
#define _REG(ctrl)		S2MPS28_PM1##ctrl
#define _TIME(macro)		S2MPS28_ENABLE_TIME##macro
#define _LDO_MIN(group)		S2MPS28_LDO_MIN##group
#define _LDO_STEP(group)	S2MPS28_LDO_STEP##group
#define _LDO_MASK(num)		S2MPS28_LDO_VSEL_MASK##num
#define _BUCK_MIN(group)	S2MPS28_BUCK_MIN##group
#define _BUCK_STEP(group)	S2MPS28_BUCK_STEP##group
#define _BB_MIN(group)		S2MPS28_BB_MIN##group
#define _BB_STEP(group)		S2MPS28_BB_STEP##group

#define BUCK_DESC(_name, _id, g, v, e, t)	{		\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BUCK_MIN(g),				\
	.uV_step	= _BUCK_STEP(g),			\
	.n_voltages	= S2MPS28_BUCK_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS28_BUCK_VSEL_MASK,		\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS28_ENABLE_MASK,			\
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
	.enable_mask	= S2MPS28_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#define BB_DESC(_name, _id, g, v, e, t)	{			\
	.name		= _name,				\
	.id		= _id,					\
	.type		= REGULATOR_VOLTAGE,			\
	.owner		= THIS_MODULE,				\
	.min_uV		= _BB_MIN(g),				\
	.uV_step	= _BB_STEP(g),				\
	.n_voltages	= S2MPS28_BB_N_VOLTAGES,		\
	.vsel_reg	= v,					\
	.vsel_mask	= S2MPS28_BB_VSEL_MASK,			\
	.enable_reg	= e,					\
	.enable_mask	= S2MPS28_ENABLE_MASK,			\
	.enable_time	= t,					\
}

#if IS_ENABLED(CONFIG_SOC_S5E9945)
enum s2mps28_regulators_9945 {
	S2MPS28_BUCK1,
	S2MPS28_BUCK2,
	S2MPS28_BUCK3,
	S2MPS28_BUCK4,
	S2MPS28_BUCK5,
	S2MPS28_BUCK_SR1,
	S2MPS28_LDO1,
	S2MPS28_LDO2,
	S2MPS28_LDO3,
	S2MPS28_LDO4,
	S2MPS28_LDO5,
	S2MPS28_REG_MAX,
};

static struct regulator_desc s2mps28_1_regulators[] = {
	/* SUB1 BUCK 1S ~ 5S */
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1_OUT2), _REG(_BUCK1_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2_OUT1), _REG(_BUCK2_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3_OUT1), _REG(_BUCK3_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4_OUT1), _REG(_BUCK4_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), 1, _REG(_BUCK5_OUT1), _REG(_BUCK5_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_BUCK_SR1_OUT1), _REG(_BUCK_SR1_CTRL),
			_TIME(_BUCK_SR)),
	/* SUB1 LDO 1 ~ 5 */
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_OUT2), 1, _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_OUT1), 1, _REG(_LDO2_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_OUT1), 1, _REG(_LDO3_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4_OUT1), 1, _REG(_LDO4_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5_OUT1), 1, _REG(_LDO5_CTRL), _TIME(_LDO)),
};

static struct regulator_desc s2mps28_2_regulators[] = {
	/* SUB2 BUCK 1S ~ 5S */
	//BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1_OUT1), _REG(_BUCK1_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2_OUT1), _REG(_BUCK2_CTRL), _TIME(_BUCK)),
	//BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3_OUT1), _REG(_BUCK3_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4_OUT1), _REG(_BUCK4_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), 1, _REG(_BUCK5_OUT1), _REG(_BUCK5_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_BUCK_SR1_OUT1), _REG(_BUCK_SR1_CTRL),
			_TIME(_BUCK_SR)),
	/* SUB2 LDO 1 ~ 5 */
	//LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_OUT1), 1, _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_OUT1), 1, _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_OUT1), 1, _REG(_LDO3_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4_OUT1), 1, _REG(_LDO4_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5_OUT1), 1, _REG(_LDO5_CTRL), _TIME(_LDO)),
};

static struct regulator_desc s2mps28_3_regulators[] = {
	/* SUB3 BUCK 1S ~ 5S */
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1_OUT1), _REG(_BUCK1_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2_OUT1), _REG(_BUCK2_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3_OUT1), _REG(_BUCK3_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4_OUT1), _REG(_BUCK4_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), 1, _REG(_BUCK5_OUT1), _REG(_BUCK5_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_BUCK_SR1_OUT1), _REG(_BUCK_SR1_CTRL),
			_TIME(_BUCK_SR)),
	/* SUB3 LDO 1 ~ 5 */
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_OUT1), 1, _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_OUT1), 1, _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_OUT1), 1, _REG(_LDO3_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4_OUT1), 1, _REG(_LDO4_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5_OUT1), 1, _REG(_LDO5_CTRL), _TIME(_LDO)),
};

static struct regulator_desc s2mps28_4_regulators[] = {
	/* SUB4 BUCK 1S ~ 5S */
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1_OUT2), _REG(_BUCK1_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2_OUT2), _REG(_BUCK2_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3_OUT1), _REG(_BUCK3_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4_OUT1), _REG(_BUCK4_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), 1, _REG(_BUCK5_OUT1), _REG(_BUCK5_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_BUCK_SR1_OUT2), _REG(_BUCK_SR1_CTRL),
			_TIME(_BUCK_SR)),
	/* SUB4 LDO 1 ~ 5 */
	LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_OUT1), 1, _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_OUT1), 1, _REG(_LDO2_CTRL), _TIME(_LDO)),
	//LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_OUT1), 1, _REG(_LDO3_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4_OUT1), 1, _REG(_LDO4_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5_OUT1), 1, _REG(_LDO5_CTRL), _TIME(_LDO)),
};

static struct regulator_desc s2mps28_5_regulators[] = {
	/* SUB5 BUCK 1S ~ 5S */
	BUCK_DESC("BUCK1", _BUCK(1), 1, _REG(_BUCK1_OUT2), _REG(_BUCK1_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK2", _BUCK(2), 1, _REG(_BUCK2_OUT2), _REG(_BUCK2_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK3", _BUCK(3), 1, _REG(_BUCK3_OUT2), _REG(_BUCK3_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK4", _BUCK(4), 1, _REG(_BUCK4_OUT2), _REG(_BUCK4_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK5", _BUCK(5), 1, _REG(_BUCK5_OUT2), _REG(_BUCK5_CTRL), _TIME(_BUCK)),
	BUCK_DESC("BUCK_SR1", _BUCK(_SR1), 2, _REG(_BUCK_SR1_OUT1), _REG(_BUCK_SR1_CTRL),
			_TIME(_BUCK_SR)),
	/* SUB5 LDO 1 ~ 5 */
	//LDO_DESC("LDO1", _LDO(1), 1, _REG(_LDO1_OUT1), 1, _REG(_LDO1_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO2", _LDO(2), 1, _REG(_LDO2_OUT1), 1, _REG(_LDO2_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO3", _LDO(3), 1, _REG(_LDO3_OUT1), 1, _REG(_LDO3_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO4", _LDO(4), 1, _REG(_LDO4_OUT1), 1, _REG(_LDO4_CTRL), _TIME(_LDO)),
	LDO_DESC("LDO5", _LDO(5), 1, _REG(_LDO5_OUT1), 1, _REG(_LDO5_CTRL), _TIME(_LDO)),
};
#endif /* CONFIG_SOC_S5E9945 */

#endif /* __LINUX_S2MPS28_REGULATOR_H */

