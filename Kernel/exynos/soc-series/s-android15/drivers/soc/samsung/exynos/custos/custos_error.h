/*
 * psp_error.h - Samsung Psp Mailbox error return
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CUSTOS_ERROR_H__
#define __CUSTOS_ERROR_H__

/*****************************************************************************/
/* Define error return							     */
/*****************************************************************************/
/* Common */
#define RV_SUCCESS					0x0000
#define RV_PASS						0x1234
#define RV_FAIL						0x7432

/* Error */
#define RV_ERR_CUSTOS_WAIT_DATA_MSG_NO_PEER		0xD000

#endif /* __CUSTOS_ERROR_H__ */
