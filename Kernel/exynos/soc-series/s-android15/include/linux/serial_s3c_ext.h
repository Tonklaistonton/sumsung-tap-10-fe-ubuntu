// SPDX-License-Identifier: GPL-2.0
#ifndef __EXYNOS_SERIAL_S3C_EXT_H
#define __EXYNOS_SERIAL_S3C_EXT_H

/* External Driver Header for Samsung SoC onboard UARTs */

typedef void (*s3c_wake_peer_t)(struct uart_port *port);
extern s3c_wake_peer_t s3c2410_serial_wake_peer[CONFIG_SERIAL_SAMSUNG_UARTS];

#endif /* __EXYNOS_SERIAL_S3C_EXT_H */
