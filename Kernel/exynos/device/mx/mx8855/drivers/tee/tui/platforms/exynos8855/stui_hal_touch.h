/*
 * Copyright (c) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STUI_HAL_TOUCH_H__
#define __STUI_HAL_TOUCH_H__

#if IS_ENABLED(CONFIG_INPUT_SEC_INPUT)
extern int stui_tsp_enter(void);
extern int stui_tsp_exit(void);
extern int stui_tsp_type(void);
#endif

#endif //__STUI_HAL_TOUCH_H__
