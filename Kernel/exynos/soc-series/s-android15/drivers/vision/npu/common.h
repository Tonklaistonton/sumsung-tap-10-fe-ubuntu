/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _COMMOM_H_
#define _COMMOM_H_

enum numCtrl_e {
	ECTRL_LOW = 1,
#if (CONFIG_NPU_MAILBOX_VERSION >= 9)
	ECTRL_MEDIUM,
#endif
	ECTRL_HIGH,
	ECTRL_ACK,
#if (CONFIG_NPU_MAILBOX_VERSION >= 9)
	ECTRL_NACK,
#endif
	ECTRL_REPORT,
};


#endif /* _COMMOM_H_ */
