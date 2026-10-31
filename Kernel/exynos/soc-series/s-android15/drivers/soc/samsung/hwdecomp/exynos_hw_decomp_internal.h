// SPDX-License-Identifier: GPL-2.0
/*
 * Exynos - HW Decompression engine Driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 *
 */
#ifndef _EXYNOS_HW_DECOMP_INTERNAL_H
#define _EXYNOS_HW_DECOMP_INTERNAL_H

#include <linux/percpu.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

struct exynos_hw_decomp_desc {
	struct kobject kobj;

	unsigned long __percpu *bounce_buffer;

	__iomem unsigned char *regs;

	struct notifier_block cpupm_nb;

	raw_spinlock_t lock;

	atomic64_t nr_success;
	atomic64_t nr_timeout;
	atomic64_t nr_failure;
	atomic64_t nr_unready;
	atomic64_t nr_busy;

	struct device *dev;

	/* the caller of the register function */
	unsigned long owner;
};

#endif
