/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2024, Samsung Electronics.
 *
 */

#ifndef __SHM_IPC_PRIV_H__
#define __SHM_IPC_PRIV_H__

#include <dt-bindings/soc/samsung/exynos-cpif.h>

/* Reserved memory */
#define MAX_CP_RMEM	10

struct cp_reserved_mem {
	char *name;
	u32 index;
	unsigned long p_base;
	u32 size;
};

/* Shared memory */
struct cp_shared_mem {
	char *name;
	u32 index;
	u32 rmem;
	unsigned long p_base;
	u32 size;
	u32 option;
	bool cached;
	void __iomem *v_base;
};

/* Memory map on CP binary */
#define MAX_MAP_ON_CP		10

#if IS_ENABLED(CONFIG_CP_MEM_MAP_V1)
#define MAP_ON_CP_OFFSET	0xD0
#define MAP_ON_CP_OFFSET_ENC	0x4D0
#else
#define MAP_ON_CP_OFFSET	0xA0
#define MAP_ON_CP_OFFSET_ENC	0x4A0
#endif

struct cp_map_info {
	u32 name;
	u32 offset;
	u32 size;
#if IS_ENABLED(CONFIG_CP_MEM_MAP_V1)
	u32 option;
#endif
} __packed;

struct cp_mem_map {
	u32 version;
#if !IS_ENABLED(CONFIG_CP_MEM_MAP_V1)
	u32 secure_size;
	u32 ns_size;
#endif
	u32 map_count;
	struct cp_map_info map_info[MAX_MAP_ON_CP];
	u32 mem_guard;
} __packed;

#if IS_ENABLED(CONFIG_CP_MEM_MAP_V1)
#define CP_MEM_MAP_VER	(0x4d454d31)	/* MEM1 */
#else
#define CP_MEM_MAP_VER	(0x4d454d00)	/* MEM0 */
#endif

#define CP_MEM_MAP_CP0	(0x00435030)	/* CP0 */
#define CP_MEM_MAP_VSS	(0x00565353)	/* VSS */
#define CP_MEM_MAP_ABX	(0x00414258) /* ABX */
#define CP_MEM_MAP_IPC	(0x00495043) /* IPC */
#define CP_MEM_MAP_PKP	(0x00504b50) /* PKP */
#define CP_MEM_MAP_CP1	(0x00435031) /* CP1 */
#define CP_MEM_MAP_DDM	(0x0044444d) /* DDM */
#define CP_MEM_MAP_STS	(0x00535453) /* STS */
#define CP_MEM_MAP_LOG	(0x004c4f47) /* LOG */
#define CP_MEM_MAP_L2B	(0x004c3242) /* L2B */

struct cp_toc {
	char name[12];
	u32 img_offset;
	u32 mem_offset;
	u32 size;
	u32 crc;
	u32 reserved;
} __packed;

#endif /* __SHM_IPC_PRIV_H__ */
