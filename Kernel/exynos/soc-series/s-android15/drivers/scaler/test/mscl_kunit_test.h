/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Kunit of Exynos Scaler driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _MSCL_KUNIT_TEST_H_
#define _MSCL_KUNIT_TEST_H_

inline unsigned int sc_ext_buf_size(int width);

void get_blend_value(unsigned int *cfg, u32 val, bool pre_multi);

u32 sc_coef_adjust(u32 val);

void sc_hwset_blend(struct sc_dev *sc, enum sc_blend_op bl_op, bool pre_multi, unsigned char g_alpha);

bool sc_hwset_clear_votf_clock_en(struct sc_tws *tws);

void sc_hwset_color_fill(struct sc_dev *sc, unsigned int val);

void sc_hwset_init_votf(struct sc_dev *sc);

void sc_hwset_tws_flush(struct sc_tws *tws);

void sc_hwset_votf(struct sc_dev *sc, struct sc_tws *tws);

void sc_hwset_votf_en(struct sc_dev *sc, bool enable);

void sc_hwset_votf_ring_clk_en(struct sc_dev *sc);

void sc_votf_read_reg_and_print(struct sc_dev *sc, u32 offset);

struct sc_ctx *sc_select_ctx(struct sc_dev *sc);

#endif /* _MSCL_KUNIT_TEST_H_ */
