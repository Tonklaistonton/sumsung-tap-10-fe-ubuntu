// SPDX-License-Identifier: GPL-2.0
/*
 * Exynos - HW Decompression engine Driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 *
 */

#ifndef _EXYNOS_HW_DECOMP_REGS_H
#define _EXYNOS_HW_DECOMP_REGS_H

struct exynos_DECOMP_SRC_CONFIG_field {
	u64 src_addr		:36;
	u64 page_size		:2;
	u64 rsvd		:10;
	u64 src_size		:16;
};

struct exynos_DECOMP_IF_CONFIG_field {
	u64 wu_policy		:2;
	u64 rsvd0		:2;
	u64 arcache		:4;
	u64 awcache		:4;
	u64 ardomain		:2;
	u64 awdomain		:2;
	u64 done_mask		:1;
	u64 page_perf_up	:1;
	u64 rsvd1		:46;
};

struct exynos_DECOMP_DST_CONFIG_field {
	u64 dst_addr		:36;
	u64 enable		:1;
	u64 rsvd0		:11;
	u64 debug_en		:1;
	u64 rsvd1		:15;
};

struct exynos_DECOMP_RESULT_field {
	u64 decomp_done		:1;
	u64 decomp_err		:1;
	u64 rsvd0		:6;
	u64 out_src		:24;
	u64 rsvd1		:32;
};

struct exynos_DECOMP_STATUS_field {
	u64 wr_ptr		:16;
	u64 rd_ptr		:16;
	u64 status		:4;
	u64 error_info		:4;
	u64 rsvd		:24;
};

struct exynos_DECOMP_DEBUG_field {
	u64 rsvd		:64;
};

struct exynos_DECOMP_TIMEOUT_field {
	u64 timeout_value	:32;
	u64 rsvd		:31;
	u64 timeout_enable	:1;
};

#define EXYNOS_DECOMP_REGISTER(name)				\
typedef struct exynos_DECOMP_##name {				\
	union {							\
		struct exynos_DECOMP_##name##_field field;	\
		u64 reg;					\
	};							\
} name##_t;

EXYNOS_DECOMP_REGISTER(SRC_CONFIG)
EXYNOS_DECOMP_REGISTER(IF_CONFIG)
EXYNOS_DECOMP_REGISTER(DST_CONFIG)
EXYNOS_DECOMP_REGISTER(RESULT)
EXYNOS_DECOMP_REGISTER(STATUS)
EXYNOS_DECOMP_REGISTER(DEBUG)
EXYNOS_DECOMP_REGISTER(TIMEOUT)

#define EXYNOS_REG_DECOMP_SRC_CONFIG	0x00
#define EXYNOS_REG_DECOMP_IF_CONFIG	0x08
#define EXYNOS_REG_DECOMP_DST_CONFIG	0x10
#define EXYNOS_REG_DECOMP_RESULT	0x18
#define EXYNOS_REG_DECOMP_STATUS	0x20
#define EXYNOS_REG_DECOMP_DEBUG		0x28
#define EXYNOS_REG_DECOMP_TIMEOUT	0x30

#define GEN_EXYNOS_DECOMP_RW_REGS(name)						\
static inline u64 read_DECOMP_##name (__iomem unsigned char *base)		\
{										\
	u64 reg;								\
										\
	reg = readq(base + EXYNOS_REG_DECOMP_##name);				\
	return reg;								\
}										\
static inline void write_DECOMP_##name (__iomem unsigned char *base, u64 val)	\
{										\
	writeq(val, base + EXYNOS_REG_DECOMP_##name);				\
}

#endif
