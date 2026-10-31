// SPDX-License-Identifier: GPL-2.0+
//
// Exynos ARMv8 specific support for Samsung pinctrl/gpiolib driver
// with eint support.
//
// Copyright (c) 2012 Samsung Electronics Co., Ltd.
//		http://www.samsung.com
// Copyright (c) 2012 Linaro Ltd
//		http://www.linaro.org
// Copyright (c) 2017 Krzysztof Kozlowski <krzk@kernel.org>
//
// This file contains the Samsung Exynos specific information required by the
// the Samsung pinctrl/gpiolib driver. It also includes the implementation of
// external gpio and wakeup interrupt support.

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/soc/samsung/exynos-regs-pmu.h>

#include "pinctrl-samsung.h"
#include "pinctrl-exynos.h"

/*
 * Bank type for non-alive type. Bit fields:
 * CON: 4, DAT: 1, PUD: 4, DRV: 4, CONPDN: 2, PUDPDN: 4
 */
static const struct samsung_pin_bank_type exynos850_bank_type_off  = {
	.fld_width = { 4, 1, 4, 4, 2, 4, },
	.reg_offset = { 0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, },
};

/*
 * Bank type for alive type. Bit fields:
 * CON: 4, DAT: 1, PUD: 4, DRV: 4
 */
static const struct samsung_pin_bank_type exynos850_bank_type_alive = {
	.fld_width = { 4, 1, 4, 4, },
	.reg_offset = { 0x00, 0x04, 0x08, 0x0c, },
};

/*
 * pinctrl define for S5E9945
 */
/* pin banks of s5e9945 pin-controller (GPIO_ALIVE=0x12850000) */
static struct samsung_pin_bank_data s5e9945_pin_alive[] = {
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x0,   "gpa0", 0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x20,  "gpa1", 0x04, 0x08),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x40,  "gpa2", 0x08, 0x10),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x60,  "gpa3", 0x0c, 0x18),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x80,  "gpa4", 0x10, 0x20),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 2, 0xa0,  "gpq1"),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 1, 0xc0,  "gpq2"),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 2, 0xe0,  "gpq3"),
};

/* pin banks of s5e9945 pin-controller (GPIO_CMGP=0x14030000) */
static struct samsung_pin_bank_data s5e9945_pin_cmgp[] = {
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x0,   "gpm0",  0x00, 0x00, 0x288, 0),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x20,  "gpm1",  0x04, 0x04, 0x288, 2),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x40,  "gpm2",  0x08, 0x08, 0x288, 4),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x60,  "gpm3",  0x0c, 0x0c, 0x288, 6),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x80,  "gpm4",  0x10, 0x10, 0x288, 8),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0xa0,  "gpm5",  0x14, 0x14, 0x288, 10),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0xc0,  "gpm6",  0x18, 0x18, 0x288, 12),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0xe0,  "gpm7",  0x1c, 0x1c, 0x288, 14),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x100, "gpm8",  0x20, 0x20, 0x288, 16),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x120, "gpm9",  0x24, 0x24, 0x288, 18),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x140, "gpm10", 0x28, 0x28, 0x288, 20),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x160, "gpm11", 0x2c, 0x2c, 0x288, 22),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x180, "gpm12", 0x30, 0x30, 0x288, 24),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x1a0, "gpm13", 0x34, 0x34, 0x288, 26),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1c0, "gpm15", 0x38, 0x38, 0x288, 29),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1e0, "gpm16", 0x3c, 0x3c, 0x288, 30),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x200, "gpm17", 0x40, 0x40, 0x288, 31),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x220, "gpm18", 0x44, 0x44, 0x2b8, 0),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 4, 0x240, "gpq0"),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x260, "gpm14", 0x48, 0x48, 0x288, 28),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x280, "gpm20", 0x4c, 0x4c, 0x2b8, 2),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x2a0, "gpm21", 0x50, 0x50, 0x2b8, 3),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x2c0, "gpm22", 0x54, 0x54, 0x2b8, 4),
};

/* pin banks of s5e9945 pin-controller (GPIO_HSI1=0x18030000) */
static struct samsung_pin_bank_data s5e9945_pin_hsi1[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x0, "gpf0", 0x00, 0x00),
};

/* pin banks of s5e9945 pin-controller (GPIO_UFS=0x17040000) */
static struct samsung_pin_bank_data s5e9945_pin_ufs[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 7, 0x0, "gpf1", 0x00, 0x00),
};

/* pin banks of s5e9945 pin-controller (GPIO_HSI1UFS=0x17060000) */
static struct samsung_pin_bank_data s5e9945_pin_hsi1ufs[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x0, "gpf2", 0x00, 0x00),
};

/* pin banks of s5e9945 pin-controller (GPIO_PERIC0=0x10830000) */
static struct samsung_pin_bank_data s5e9945_pin_peric0[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0,   "gpp4",  0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x20,  "gpc0",  0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x40,  "gpc1",  0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 3, 0x60,  "gpg0",  0x0c, 0x0c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 7, 0x80, "gpg1",  0x10, 0x10),
};

/* pin banks of s5e9945 pin-controller (GPIO_PERIC1=0x11030000) */
static struct samsung_pin_bank_data s5e9945_pin_peric1[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0,  "gpp10",  0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x20,  "gpc2",  0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x40,  "gpc4",  0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x60,  "gpc5",  0x0c, 0x0c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x80,  "gpg2",  0x10, 0x10),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xa0,  "gpp7",  0x14, 0x14),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xc0, "gpp8",  0x18, 0x18),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xe0, "gpp9",  0x1c, 0x1c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x100, "gpc10", 0x20, 0x20),
};

/* pin banks of s5e9945 pin-controller (GPIO_PERIC2=0x11830000) */
static struct samsung_pin_bank_data s5e9945_pin_peric2[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0,   "gpb0",  0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x20,   "gpb1",  0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x40,   "gpb2",  0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x60,   "gpb3",  0x0c, 0x0c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x80,   "gpp0",  0x10, 0x10),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xa0,  "gpp1",  0x14, 0x14),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xc0,  "gpp2",  0x18, 0x18),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xe0,  "gpp3",  0x1c, 0x1c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x100,  "gpp5",  0x20, 0x20),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x120,  "gpp6",  0x24, 0x24),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x140,  "gpp11", 0x28, 0x28),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x160,  "gpc3",  0x2c, 0x2c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x180, "gpc6",  0x30, 0x30),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x1a0, "gpc7",  0x34, 0x34),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x1c0, "gpc8",  0x38, 0x38),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x1e0, "gpc9",  0x3c, 0x3c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 1, 0x200, "gpg3",  0x40, 0x40),
};

/* pin banks of s5e9945 pin-controller (GPIO_VTS=0x13020000) */
static struct samsung_pin_bank_data s5e9945_pin_vts[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 7, 0x0, "gpv0", 0x00, 0x00),
};

/* pin banks of s5e9945 pin-controller (GPIO_CHUBVTS=0x13EA0000) */
static struct samsung_pin_bank_data s5e9945_pin_chubvts[] = {
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x0,  "gph0", 0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x20, "gph1", 0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x40, "gph2", 0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x60, "gph3", 0x0c, 0x0c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x80, "gph6", 0x10, 0x10),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 7, 0xa0, "gph7", 0x14, 0x14),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0xc0, "gpb5", 0x18, 0x1c),
};

static struct samsung_pin_ctrl s5e9945_pin_ctrl[] = {
	{
		/* pin-controller instance 0 ALIVE data */
		.pin_banks	= s5e9945_pin_alive,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_alive),
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 1 CMGP data */
		.pin_banks	= s5e9945_pin_cmgp,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_cmgp),
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 2 HSI1 data */
		.pin_banks	= s5e9945_pin_hsi1,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_hsi1),
	}, {
		/* pin-controller instance 3 UFS data */
		.pin_banks	= s5e9945_pin_ufs,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_ufs),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 4 HSI1UFS data */
		.pin_banks	= s5e9945_pin_hsi1ufs,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_hsi1ufs),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 5 PERIC0 data */
		.pin_banks	= s5e9945_pin_peric0,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_peric0),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 6 PERIC1 data */
		.pin_banks	= s5e9945_pin_peric1,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_peric1),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 7 PERIC2 data */
		.pin_banks	= s5e9945_pin_peric2,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_peric2),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* For this block, pinctrl will not care for interrupt and S2R */
		/* pin-controller instance 8 VTS data */
		.pin_banks	= s5e9945_pin_vts,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_vts),
	}, {
		/* pin-controller instance 9 CHUBVTS data */
		.pin_banks	= s5e9945_pin_chubvts,
		.nr_banks	= ARRAY_SIZE(s5e9945_pin_chubvts),
	},
};

const struct samsung_pinctrl_of_match_data s5e9945_of_data __initconst = {
	.ctrl		= s5e9945_pin_ctrl,
	.num_ctrl	= ARRAY_SIZE(s5e9945_pin_ctrl),
};

/*
 * pinctrl define for S5E9955
 */
/* pin banks of s5e9955 pin-controller (GPIO_ALIVE=0x13850000) */
static struct samsung_pin_bank_data s5e9955_pin_alive[] = {
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x0,   "gpa0", 0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x20,  "gpa1", 0x04, 0x08),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x40,  "gpa2", 0x08, 0x10),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x60,  "gpa3", 0x0c, 0x18),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x80,  "gpa4", 0x10, 0x20),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 2, 0xa0,  "gpq1"),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 1, 0xc0,  "gpq2"),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 2, 0xe0,  "gpq3"),
};

/* pin banks of s5e9955 pin-controller (GPIO_CMGP=0x15030000) */
static struct samsung_pin_bank_data s5e9955_pin_cmgp[] = {
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x0,   "gpm0",  0x00, 0x00, 0x288, 0),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x20,  "gpm1",  0x04, 0x04, 0x288, 1),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x40,  "gpm2",  0x08, 0x08, 0x288, 2),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x60,  "gpm3",  0x0c, 0x0c, 0x288, 3),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x80,  "gpm4",  0x10, 0x10, 0x288, 4),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xa0,  "gpm5",  0x14, 0x14, 0x288, 5),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xc0,  "gpm6",  0x18, 0x18, 0x288, 6),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xe0,  "gpm7",  0x1c, 0x1c, 0x288, 7),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x100, "gpm8",  0x20, 0x20, 0x288, 8),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x120, "gpm9",  0x24, 0x24, 0x288, 9),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x140, "gpm10", 0x28, 0x28, 0x288, 10),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x160, "gpm11", 0x2c, 0x2c, 0x288, 11),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x180, "gpm12", 0x30, 0x30, 0x288, 12),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1a0, "gpm13", 0x34, 0x34, 0x288, 13),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1c0, "gpm14", 0x38, 0x38, 0x288, 14),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1e0, "gpm15", 0x3c, 0x3c, 0x288, 15),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x200, "gpm16", 0x40, 0x40, 0x288, 16),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x220, "gpm17", 0x44, 0x44, 0x288, 17),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x240, "gpm18", 0x48, 0x48, 0x288, 18),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x260, "gpm19", 0x4c, 0x4c, 0x288, 19),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x280, "gpm20", 0x50, 0x50, 0x288, 20),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x2a0, "gpm21", 0x54, 0x54, 0x288, 21),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x2c0, "gpm22", 0x58, 0x58, 0x288, 22),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x2e0, "gpm23", 0x5c, 0x5c, 0x288, 23),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x300, "gpm24", 0x60, 0x60, 0x288, 24),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x320, "gpm25", 0x64, 0x64, 0x288, 25),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x340, "gpm26", 0x68, 0x68, 0x288, 26),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x360, "gpm27", 0x6c, 0x6c, 0x288, 27),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x380, "gpm28", 0x70, 0x70, 0x288, 28),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x3a0, "gpm29", 0x74, 0x74, 0x288, 29),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x3c0, "gpm30", 0x78, 0x78, 0x288, 30),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x3e0, "gpm31", 0x7c, 0x7c, 0x288, 31),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x400, "gpm32", 0x80, 0x80, 0x2b8, 0),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x420, "gpm33", 0x84, 0x84, 0x2b8, 1),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x440, "gpm34", 0x88, 0x88, 0x2b8, 2),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x460, "gpm35", 0x8c, 0x8c, 0x2b8, 3),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x480, "gpm36", 0x90, 0x90, 0x2b8, 4),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 4, 0x4a0, "gpq0"),
};

/* pin banks of s5e9955 pin-controller (GPIO_HSI1=0x19050000) */
static struct samsung_pin_bank_data s5e9955_pin_hsi1[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x0, "gpf0", 0x00, 0x00),
};

/* pin banks of s5e9955 pin-controller (GPIO_UFS=0x18040000) */
static struct samsung_pin_bank_data s5e9955_pin_ufs[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 7, 0x0, "gpf1", 0x00, 0x00),
};

/* pin banks of s5e9955 pin-controller (GPIO_HSI1UFS=0x18060000) */
static struct samsung_pin_bank_data s5e9955_pin_hsi1ufs[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x0, "gpf2", 0x00, 0x00),
};

/* pin banks of s5e9955 pin-controller (GPIO_PERIC0=0x10830000) */
static struct samsung_pin_bank_data s5e9955_pin_peric0[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0,   "gpp4",  0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x20,  "gpc0",  0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x40,  "gpc1",  0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 3, 0x60,  "gpg3",  0x0c, 0x0c),
};

/* pin banks of s5e9955 pin-controller (GPIO_PERIC1=0x11030000) */
static struct samsung_pin_bank_data s5e9955_pin_peric1[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0,  "gpp7",  0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x20,  "gpp8",  0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x40,  "gpp9",  0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x60,  "gpp10",  0x0c, 0x0c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x80,  "gpc2",  0x10, 0x10),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0xa0, "gpc4",  0x14, 0x14),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0xc0, "gpc5",  0x18, 0x18),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0xe0, "gpc10",  0x1c, 0x1c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x100, "gpg2", 0x20, 0x20),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x120, "gpb0", 0x24, 0x24),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x140, "gpb1", 0x28, 0x28),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x160, "gpb2", 0x2c, 0x2c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 1, 0x180, "gpb3", 0x30, 0x30),
};

/* pin banks of s5e9955 pin-controller (GPIO_PERIC2=0x11830000) */
static struct samsung_pin_bank_data s5e9955_pin_peric2[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0,   "gpp0",  0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x20,   "gpp1",  0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x40,   "gpp2",  0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x60,   "gpp3",  0x0c, 0x0c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x80,   "gpp5",  0x10, 0x10),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xa0,  "gpp6",  0x14, 0x14),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xc0,  "gpp11",  0x18, 0x18),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0xe0,  "gpc3",  0x1c, 0x1c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x100,  "gpc6",  0x20, 0x20),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x120,  "gpc7",  0x24, 0x24),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x140,  "gpc8", 0x28, 0x28),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x160,  "gpc9",  0x2c, 0x2c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 1, 0x180,  "gpg0",  0x30, 0x30),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 7, 0x1a0,  "gpg1",  0x34, 0x34),
};

/* pin banks of s5e9955 pin-controller (GPIO_VTS=0x14020000) */
static struct samsung_pin_bank_data s5e9955_pin_vts[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 7, 0x0, "gpv0", 0x00, 0x00),
};

/* pin banks of s5e9955 pin-controller (GPIO_CHUBVTS=0x14EA0000) */
static struct samsung_pin_bank_data s5e9955_pin_chubvts[] = {
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x0,  "gph0", 0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x20, "gph1", 0x04, 0x04),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x40, "gph2", 0x08, 0x08),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x60, "gph3", 0x0c, 0x0c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x80, "gph4", 0x10, 0x10),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 7, 0xa0, "gph5", 0x14, 0x14),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0xc0, "gph6", 0x18, 0x1c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0xe0, "gph7", 0x18, 0x1c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 2, 0x100, "gph8", 0x18, 0x1c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 4, 0x120, "gpb4", 0x18, 0x1c),
};

/* pin banks of s5e9955 pin-controller (GPIO_DSE=0x10890000) */
static struct samsung_pin_bank_data s5e9955_pin_dse[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0, "gpd0", 0x00, 0x00),
};

static struct samsung_pin_ctrl s5e9955_pin_ctrl[] = {
	{
		/* pin-controller instance 0 ALIVE data */
		.pin_banks	= s5e9955_pin_alive,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_alive),
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 1 CMGP data */
		.pin_banks	= s5e9955_pin_cmgp,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_cmgp),
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 2 HSI1 data */
		.pin_banks	= s5e9955_pin_hsi1,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_hsi1),
	}, {
		/* pin-controller instance 3 UFS data */
		.pin_banks	= s5e9955_pin_ufs,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_ufs),
#if !defined(CONFIG_EXYNOS_EMULATOR)
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
#endif
	}, {
		/* pin-controller instance 4 HSI1UFS data */
		.pin_banks	= s5e9955_pin_hsi1ufs,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_hsi1ufs),
#if !defined(CONFIG_EXYNOS_EMULATOR)
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
#endif
	}, {
		/* pin-controller instance 5 PERIC0 data */
		.pin_banks	= s5e9955_pin_peric0,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_peric0),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 6 PERIC1 data */
		.pin_banks	= s5e9955_pin_peric1,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_peric1),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* pin-controller instance 7 PERIC2 data */
		.pin_banks	= s5e9955_pin_peric2,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_peric2),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
		/* For this block, pinctrl will not care for interrupt and S2R */
		/* pin-controller instance 8 VTS data */
		.pin_banks	= s5e9955_pin_vts,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_vts),
	}, {
		/* pin-controller instance 9 CHUBVTS data */
		.pin_banks	= s5e9955_pin_chubvts,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_chubvts),
	}, {
		/* pin-controller instance 10 DSE data */
		.pin_banks	= s5e9955_pin_dse,
		.nr_banks	= ARRAY_SIZE(s5e9955_pin_dse),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	},
};

const struct samsung_pinctrl_of_match_data s5e9955_of_data __initconst = {
	.ctrl		= s5e9955_pin_ctrl,
	.num_ctrl	= ARRAY_SIZE(s5e9955_pin_ctrl),
};

/*
 * pinctrl define for s5e8855
 */
/* pin banks of s5e8855 pin-controller (GPIO_ALIVE=0x11850000) */
static struct samsung_pin_bank_data s5e8855_pin_alive[] = {
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 8, 0x00, "gpa0", 0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 5, 0x20, "gpa1", 0x04, 0x08),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 3, 0x40, "gpq0"),
	EXYNOS9_PIN_BANK_EINTN(exynos850_bank_type_alive, 2, 0x60, "gpq1"),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x80, "gpc0", 0x08, 0x10),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xa0, "gpc1", 0x0c, 0x14),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xc0, "gpc2", 0x10, 0x18),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xe0, "gpc3", 0x14, 0x1c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x100, "gpc4", 0x18, 0x20),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x120, "gpc5", 0x1c, 0x24),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x140, "gpc6", 0x20, 0x28),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x160, "gpc7", 0x24, 0x2c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x180, "gpc8", 0x28, 0x30),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1a0, "gpc9", 0x2c, 0x34),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1c0, "gpc10", 0x30, 0x38),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1e0, "gpc11", 0x34, 0x3c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x200, "gpc12", 0x38, 0x40),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x220, "gpc13", 0x3c, 0x44),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x240, "gpc14", 0x40, 0x48),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x260, "gpj0", 0x44, 0x4c),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x280, "gpj1", 0x48, 0x50),
	EXYNOS9_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x2a0, "gpj2", 0x4c, 0x54),
};

/* pin banks of s5e8855 pin-controller (GPIO_CMGP=0x12030000) */
static struct samsung_pin_bank_data s5e8855_pin_cmgp[] = {
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x00,  "gpm0",  0x00, 0x00, 0x288, 0),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x20,  "gpm1",  0x04, 0x04, 0x288, 1),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x40,  "gpm2",  0x08, 0x08, 0x288, 2),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x60,  "gpm3",  0x0c, 0x0c, 0x288, 3),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x80,  "gpm4",  0x10, 0x10, 0x288, 4),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xa0,  "gpm5",  0x14, 0x14, 0x288, 5),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xc0,  "gpm6",  0x18, 0x18, 0x288, 6),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0xe0,  "gpm7",  0x1c, 0x1c, 0x288, 7),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x100, "gpm8",  0x20, 0x20, 0x288, 8),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x120, "gpm9",  0x24, 0x24, 0x288, 9),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x140, "gpm10", 0x28, 0x28, 0x288, 10),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x160, "gpm11", 0x2c, 0x2c, 0x288, 11),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x180, "gpm12", 0x30, 0x30, 0x288, 12),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1a0, "gpm13", 0x34, 0x34, 0x288, 13),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1c0, "gpm14", 0x38, 0x38, 0x288, 14),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x1e0, "gpm15", 0x3c, 0x3c, 0x288, 15),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x200, "gpm16", 0x40, 0x40, 0x288, 16),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x220, "gpm17", 0x44, 0x44, 0x288, 17),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x240, "gpm18", 0x48, 0x48, 0x288, 18),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x260, "gpm19", 0x4c, 0x4c, 0x288, 19),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x280, "gpm20", 0x50, 0x50, 0x288, 20),
	EXYNOS_CMGP_PIN_BANK_EINTW(exynos850_bank_type_alive, 1, 0x2a0, "gpm21", 0x54, 0x54, 0x288, 21),
};

/* pin banks of s5e8855 pin-controller (GPIO_HSIUFS=0x17040000) */
static struct samsung_pin_bank_data s5e8855_pin_hsiufs[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x0, "gpf3", 0x00, 0x00),
};

/* pin banks of s5e8855 pin-controller (GPIO_PERIC=0x15440000) */
static struct samsung_pin_bank_data s5e8855_pin_peric[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 8, 0x0,   "gpp0", 0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 8, 0x20,  "gpp1", 0x04, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 6, 0x40,  "gpp2", 0x08, 0x10),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x60,  "gpg0", 0x0c, 0x18),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 3, 0x80,  "gpg1", 0x10, 0x1c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 6, 0xa0,  "gpb0", 0x14, 0x20),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0xc0,  "gpb1", 0x18, 0x28),
};

/* pin banks of s5e8855 pin-controller (GPIO_PERIMMC=0x154f0000) */
static struct samsung_pin_bank_data s5e8855_pin_pericmmc[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 7, 0x0, "gpf2", 0x00, 0x00),
};

/* pin banks of s5e8855 pin-controller (GPIO_USI=0x15030000) */
static struct samsung_pin_bank_data s5e8855_pin_usi[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 8, 0x0, "gpp3", 0x00, 0x00),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x20, "gpp4", 0x04, 0x08),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 2, 0x40, "gpg2", 0x08, 0x0c),
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 1, 0x60, "gpg3", 0x0c, 0x10),
};

/* pin banks of s5e8855 pin-controller (GPIO_VTS=0x11780000) */
static struct samsung_pin_bank_data s5e8855_pin_vts[] = {
	EXYNOS9_PIN_BANK_EINTG(exynos850_bank_type_off, 4, 0x0, "gpv0", 0x00, 0x00),
};

static struct samsung_pin_ctrl s5e8855_pin_ctrl[] = {
	{
	/* pin-controller instance 0 ALIVE data */
		.pin_banks	= s5e8855_pin_alive,
		.nr_banks	= ARRAY_SIZE(s5e8855_pin_alive),
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
	/* pin-controller instance 1 CMGP data */
		.pin_banks      = s5e8855_pin_cmgp,
		.nr_banks       = ARRAY_SIZE(s5e8855_pin_cmgp),
		.eint_gpio_init = exynos_eint_gpio_init,
		.eint_wkup_init = exynos_eint_wkup_init,
		.suspend        = exynos_pinctrl_suspend,
		.resume         = exynos_pinctrl_resume,
        }, {
	/* pin-controller instance 2 HSIUFS data */
		.pin_banks	= s5e8855_pin_hsiufs,
		.nr_banks	= ARRAY_SIZE(s5e8855_pin_hsiufs),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
	/* pin-controller instance 3 PERIC data */
		.pin_banks	= s5e8855_pin_peric,
		.nr_banks	= ARRAY_SIZE(s5e8855_pin_peric),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
	/* pin-controller instance 4 PERIC MMC data */
		.pin_banks	= s5e8855_pin_pericmmc,
		.nr_banks	= ARRAY_SIZE(s5e8855_pin_pericmmc),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
	/* pin-controller instance 5 USI data */
		.pin_banks	= s5e8855_pin_usi,
		.nr_banks	= ARRAY_SIZE(s5e8855_pin_usi),
		.eint_gpio_init = exynos_eint_gpio_init,
		.suspend	= exynos_pinctrl_suspend,
		.resume		= exynos_pinctrl_resume,
	}, {
	/* pin-controller instance 6 VTS data */
		.pin_banks	= s5e8855_pin_vts,
		.nr_banks	= ARRAY_SIZE(s5e8855_pin_vts),
	},
};

const struct samsung_pinctrl_of_match_data s5e8855_of_data __initconst = {
	.ctrl		= s5e8855_pin_ctrl,
	.num_ctrl	= ARRAY_SIZE(s5e8855_pin_ctrl),
};

MODULE_LICENSE("GPL");
