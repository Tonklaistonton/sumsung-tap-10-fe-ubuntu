/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CUSTOS_DEV_H__
#define __CUSTOS_DEV_H__

/* IOCTL commands */
#define CUSTOS_IOC_MAGIC 'c'

#define CUSTOS_IOCTL_SET_TIMEOUT _IOW(CUSTOS_IOC_MAGIC, 1, uint64_t)

#define CUSTOS_IOCTL_GET_HANDLE \
	_IOWR(CUSTOS_IOC_MAGIC, 4, struct custos_memory_pair)

#define CUSTOS_IOCTL_GET_VERSION _IOR(CUSTOS_IOC_MAGIC, 10, struct custos_version)

#define CUSTOS_TURN_ON 1
#define CUSTOS_TURN_OFF 2

#define CUSTOS_IOCTL_TURN_CUSTOS _IOW(CUSTOS_IOC_MAGIC, 20, uint64_t)

struct custos_memory_pair {
	unsigned long address;
	unsigned long handle;
};

struct custos_version {
	uint32_t major;
	uint32_t minor;
};

#endif /* __CUSTOS_DEV_H__ */
