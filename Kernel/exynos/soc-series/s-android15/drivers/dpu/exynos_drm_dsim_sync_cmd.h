/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for Samsung MIPI DSI Master Sync Command.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_DSI_SYNC_CMD_H__
#define __EXYNOS_DRM_DSI_SYNC_CMD_H__

#include "../panel/panel-samsung-drv.h"

#define MAX_SYNC_CMDSET_NUM 32
#define MAX_SYNC_CMDSET_PAYLOAD 1024

struct dsim_device;
struct dsim_sync_cmd {
	u32 id;
	struct dsim_device *dsim;

	bool enabled;
	bool pkt_go;
	struct mutex lock;
	struct completion ph_wr_comp;
	struct completion rd_comp;
	struct timer_list cmd_timer;
	struct work_struct cmd_work;

	int s_cmd_sram;
};

enum {
	MIPI_DCS_STILLEN		= 0x1B,
	MIPI_DCS_STILLOFF		= 0x1C,
	MIPI_DCS_STILLENFLY		= 0x1D,
};

static inline bool IS_DCS_STILL_CMD(struct mipi_dsi_packet *packet)
{
	return (!mipi_dsi_packet_format_is_long(packet->header[0] & 0x3f) &&
		(packet->header[1] == MIPI_DCS_STILLEN ||
		packet->header[1] == MIPI_DCS_STILLOFF ||
		packet->header[1] == MIPI_DCS_STILLENFLY));
}

#if IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)
struct dsim_sync_cmd *exynos_sync_cmd_register(struct dsim_device *dsim);
inline void dsim_enable_sync_command(struct dsim_device *dsim);
inline void dsim_disable_sync_command(struct dsim_device *dsim);
void dsim_sync_cmd_irq_handler(struct dsim_device *dsim, unsigned int int_src);
int dsim_sync_wait_fifo_empty(struct dsim_device *dsim, u32 frames);
int dsim_host_sync_transfer(struct mipi_dsi_host *host,
			    const struct mipi_dsi_msg *msg);
ssize_t dsim_dcs_sync_write_buffer(struct mipi_dsi_device *dsi,
				  const void *data, size_t len);

static inline int exynos_dcs_sync_write(struct exynos_panel *ctx, const void *data,
		size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	return dsim_dcs_sync_write_buffer(dsi, data, len);
}
#else
static inline struct dsim_sync_cmd *exynos_sync_cmd_register(struct dsim_device *dsim) { return NULL; }
static inline void dsim_enable_sync_command(struct dsim_device *dsim) {}
static inline void dsim_disable_sync_command(struct dsim_device *dsim) {}
static inline void dsim_sync_cmd_irq_handler(struct dsim_device *dsim, unsigned int int_src) {}
static inline int dsim_sync_wait_fifo_empty(struct dsim_device *dsim, u32 frames) { return -ENODEV; }
static inline int dsim_host_sync_transfer(struct mipi_dsi_host *host,
			    const struct mipi_dsi_msg *msg) { return 0; }
static inline ssize_t dsim_dcs_sync_write_buffer(struct mipi_dsi_device *dsi,
				  const void *data, size_t len) { return 0; }
static inline int exynos_dcs_sync_write(struct exynos_panel *ctx, const void *data,
		size_t len) { return -EPERM; }
#endif

#endif /* __EXYNOS_DRM_DSI_SYNC_CMD_H__ */
