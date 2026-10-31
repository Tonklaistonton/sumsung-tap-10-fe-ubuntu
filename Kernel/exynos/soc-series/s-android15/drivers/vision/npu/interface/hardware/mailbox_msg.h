/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef NPU_MAILBOX_MSG_H_
#define NPU_MAILBOX_MSG_H_

#if (CONFIG_NPU_COMMAND_VERSION == 12)
#include "mailbox_msg_v12.h"
#elif (CONFIG_NPU_COMMAND_VERSION == 13)
#include "mailbox_msg_v13.h"
#else
#error Unsupported Command version
#endif

#endif	/* NPU_MAILBOX_MSG_H_ */
