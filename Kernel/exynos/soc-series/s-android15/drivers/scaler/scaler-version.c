// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include "scaler-version.h"
#include "scaler-regs.h"

/* must specify in revers order of SCALER_VERSION(xyz) */
static const u32 sc_version_table[][2] = {
        { 0x10000101, SCALER_VERSION(10, 0, 1) }, /* SC_POLY */
	{ 0x10000000, SCALER_VERSION(10, 0, 0) }, /* SC_POLY */
	{ 0x09000101, SCALER_VERSION(9, 0, 1) }, /* SC_POLY */
	{ 0x09000000, SCALER_VERSION(9, 0, 0) }, /* SC_POLY */
	{ 0x08000000, SCALER_VERSION(8, 0, 0) }, /* SC_POLY */
        { 0x07000103, SCALER_VERSION(7, 0, 1) }, /* SC_POLY */
	{ 0x07000101, SCALER_VERSION(7, 0, 1) }, /* SC_POLY */
	{ 0x07000100, SCALER_VERSION(7, 0, 1) }, /* SC_POLY */
	{ 0x07000000, SCALER_VERSION(7, 0, 1) }, /* SC_POLY */
	{ 0x06000100, SCALER_VERSION(6, 0, 1) }, /* SC_POLY */
	{ 0x06000000, SCALER_VERSION(5, 2, 0) }, /* SC_POLY */
	{ 0x05000100, SCALER_VERSION(5, 2, 0) }, /* SC_POLY */
	{ 0x05000000, SCALER_VERSION(5, 2, 0) }, /* SC_POLY */
	{ 0x04000001, SCALER_VERSION(5, 1, 0) }, /* SC_POLY */
	{ 0x04000000, SCALER_VERSION(5, 1, 0) }, /* SC_POLY */
	{ 0x02000100, SCALER_VERSION(5, 0, 1) }, /* SC_POLY */
	{ 0x02000000, SCALER_VERSION(5, 0, 0) },
	{ 0x80060007, SCALER_VERSION(4, 2, 0) }, /* SC_BI */
	{ 0x01200100, SCALER_VERSION(4, 2, 0) }, /* SC_BI */
	{ 0x0100000f, SCALER_VERSION(4, 0, 1) }, /* SC_POLY */
	{ 0xA0000013, SCALER_VERSION(4, 0, 1) },
	{ 0xA0000012, SCALER_VERSION(4, 0, 1) },
	{ 0x80050007, SCALER_VERSION(4, 0, 0) }, /* SC_POLY */
	{ 0xA000000B, SCALER_VERSION(3, 0, 2) },
	{ 0xA000000A, SCALER_VERSION(3, 0, 2) },
	{ 0x8000006D, SCALER_VERSION(3, 0, 1) },
	{ 0x80000068, SCALER_VERSION(3, 0, 0) },
	{ 0x8004000C, SCALER_VERSION(2, 2, 0) },
	{ 0x80000008, SCALER_VERSION(2, 1, 1) },
	{ 0x80000048, SCALER_VERSION(2, 1, 0) },
	{ 0x80010000, SCALER_VERSION(2, 0, 1) },
	{ 0x80000047, SCALER_VERSION(2, 0, 0) },
};

static const struct sc_variant sc_variants[] = {
	{
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 16384,
			.max_h		= 16384,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 16384,
			.max_h		= 16384,
		},
		.version		= SCALER_VERSION(7, 0, 1),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_up_swmax		= SCALE_RATIO_CONST(1, 64),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v6,
		.ratio_20bit		= 1,
		.initphase		= 1,
		.pixfmt_10bit		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 16384,
			.max_h		= 16384,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 16384,
			.max_h		= 16384,
		},
		.version		= SCALER_VERSION(6, 0, 1),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_up_swmax		= SCALE_RATIO_CONST(1, 64),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v6,
		.ratio_20bit		= 1,
		.initphase		= 1,
		.pixfmt_10bit		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(5, 2, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_up_swmax		= SCALE_RATIO_CONST(1, 64),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v5,
		.ratio_20bit		= 1,
		.initphase		= 1,
		.pixfmt_10bit		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(5, 1, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_up_swmax		= SCALE_RATIO_CONST(1, 64),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v4,
		.blending		= 0,
		.prescale		= 0,
		.ratio_20bit		= 1,
		.initphase		= 1,
		.pixfmt_10bit		= 1,
		.minsize_srcplane	= 4096 + 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(5, 0, 1),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_up_swmax		= SCALE_RATIO_CONST(1, 64),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v4,
		.blending		= 0,
		.prescale		= 0,
		.ratio_20bit		= 1,
		.initphase		= 1,
		.pixfmt_10bit		= 1,
		.extra_buf		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(5, 0, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_up_swmax		= SCALE_RATIO_CONST(1, 64),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v4,
		.blending		= 0,
		.prescale		= 0,
		.ratio_20bit		= 1,
		.initphase		= 1,
		.pixfmt_10bit		= 1,
		.extra_buf		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(4, 2, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v3,
		.blending		= 1,
		.prescale		= 0,
		.ratio_20bit		= 1,
		.initphase		= 1,
		.is_bilinear		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(4, 0, 1),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_up_swmax		= SCALE_RATIO_CONST(1, 16),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v4,
		.blending		= 0,
		.prescale		= 0,
		.ratio_20bit		= 1,
		.initphase		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(4, 0, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v3,
		.blending		= 0,
		.prescale		= 0,
		.ratio_20bit		= 0,
		.initphase		= 0,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(3, 0, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(16, 1),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL_v3,
		.blending		= 0,
		.prescale		= 1,
		.ratio_20bit		= 1,
		.initphase		= 1,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(2, 2, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL,
		.blending		= 1,
		.prescale		= 0,
		.ratio_20bit		= 0,
		.initphase		= 0,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.version		= SCALER_VERSION(2, 0, 1),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL,
		.blending		= 0,
		.prescale		= 0,
		.ratio_20bit		= 0,
		.initphase		= 0,
	}, {
		.limit_input = {
			.min_w		= 16,
			.min_h		= 16,
			.max_w		= 8192,
			.max_h		= 8192,
		},
		.limit_output = {
			.min_w		= 4,
			.min_h		= 4,
			.max_w		= 4096,
			.max_h		= 4096,
		},
		.version		= SCALER_VERSION(2, 0, 0),
		.sc_up_max		= SCALE_RATIO_CONST(1, 8),
		.sc_down_min		= SCALE_RATIO_CONST(4, 1),
		.sc_down_swmin		= SCALE_RATIO_CONST(16, 1),
		.int_en_mask		= SCALER_INT_EN_ALL,
		.blending		= 0,
		.prescale		= 0,
		.ratio_20bit		= 0,
		.initphase		= 0,
	},
};

u32 sc_get_version(u32 hw_verion) {
	size_t ivar;
	u32 sc_version;

	/* selects the lowest version number if no version is matched */
	for (ivar = 0; ivar < ARRAY_SIZE(sc_version_table); ivar++) {
		sc_version = sc_version_table[ivar][1];
		if (hw_verion == sc_version_table[ivar][0])
			break;
	}
	return sc_version;
}

const struct sc_variant *sc_get_variant(u32 sc_version) {
	size_t ivar;
	const struct sc_variant *sc_var = NULL;

	for (ivar = 0; ivar < ARRAY_SIZE(sc_variants); ivar++) {
		if (sc_version >= sc_variants[ivar].version) {
			sc_var = &sc_variants[ivar];
			break;
		}
	}
	return sc_var;
}
