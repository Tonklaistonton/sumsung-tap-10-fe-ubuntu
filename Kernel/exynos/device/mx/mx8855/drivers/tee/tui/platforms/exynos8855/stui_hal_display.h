/*
 * Samsung TUI HW Handler driver. Display functions.
 *
 * Copyright (c) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STUI_HAL_DISPLAY_H__
#define __STUI_HAL_DISPLAY_H__

#include <linux/types.h>

struct stui_buf_info;

struct resolution_info {
	uint32_t xres;
	uint32_t yres;
	uint32_t mode;
};

extern int exynos_atomic_enter_tui(void);
extern int exynos_atomic_exit_tui(void);
extern void exynos_tui_set_stui_funcs(struct stui_buf_info *(*func1)(void), void (*func2)(void));
extern void exynos_tui_get_resolution(struct resolution_info *res_info);
extern int exynos_tui_get_panel_info(u64 *buf, int size);

extern struct stui_buf_info g_stui_buf_info;
extern struct device *dev_tui;

#endif //__STUI_HAL_DISPLAY_H__
