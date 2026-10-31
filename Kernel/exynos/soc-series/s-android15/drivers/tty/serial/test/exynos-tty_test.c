// SPDX-License-Identifier: GPL-2.0-only
/**
 * Samsung Exynos TTY Serial Test Driver for KUNIT
 *
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#include <kunit/test.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include "../exynos_tty.h"
#include <kunit/visibility.h>

int exynos_serial_has_interrupt_mask(struct uart_port *port);

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

/* Exynos UART Port Data  */
static struct exynos_uart_port ourport;

static int exynos_tty_test_init(struct kunit *test)
{
	/* Init Exynos Port  Data */
	memset(&ourport, 0, sizeof(ourport));
	ourport.info= kmalloc(sizeof(struct exynos_uart_info),GFP_KERNEL);
	ourport.info->type = PORT_S3C6400;

	return 0;
}

static void exynos_tty_sample_test(struct kunit *test)
{
	int result = 0;

	result = exynos_serial_has_interrupt_mask(&(ourport.port));
	KUNIT_EXPECT_EQ(test, 1 , result);
}

static struct kunit_case exynos_tty_test_cases[] = {
	KUNIT_CASE(exynos_tty_sample_test),
	{},
};

static struct kunit_suite exynos_tty_test_suite = {
	.name = "uart_exynos",
	.init = exynos_tty_test_init,
	.test_cases = exynos_tty_test_cases,
};

kunit_test_suites(&exynos_tty_test_suite);

MODULE_LICENSE("GPL v2");
