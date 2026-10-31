/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/unaligned.h>

#include <dpu_trace.h>
#include <drm/drm_of.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_vblank.h>
#include <exynos_display_common.h>

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/gpio/consumer.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include <linux/dma-heap.h>
#include <linux/iosys-map.h>

#include <video/mipi_display.h>

#include <soc/samsung/exynos-cpupm.h>

#include <exynos_drm_crtc.h>
#include <exynos_drm_connector.h>
#include <exynos_drm_dsim.h>
#include <exynos_drm_dsim_sync_cmd.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_profiler.h>
#include <exynos_drm_bridge.h>
#include <regs-dsim.h>

static int dpu_sync_cmd_log_level = 6;
module_param(dpu_sync_cmd_log_level, int, 0600);
MODULE_PARM_DESC(dpu_sync_cmd_log_level, "log level for dsim sync command [default : 6]");

#define SYNC_CMD_NAME "exynos-sync-cmd"
#define dsim_sync_info(sync_cmd, fmt, ...)	\
	dpu_pr_info(SYNC_CMD_NAME, (sync_cmd)->id, dpu_sync_cmd_log_level, fmt, ##__VA_ARGS__)

#define dsim_sync_warn(sync_cmd, fmt, ...)	\
	dpu_pr_warn(SYNC_CMD_NAME, (sync_cmd)->id, dpu_sync_cmd_log_level, fmt, ##__VA_ARGS__)

#define dsim_sync_err(sync_cmd, fmt, ...)	\
	dpu_pr_err(SYNC_CMD_NAME, (sync_cmd)->id, dpu_sync_cmd_log_level, fmt, ##__VA_ARGS__)

#define dsim_sync_debug(sync_cmd, fmt, ...)	\
	dpu_pr_debug(SYNC_CMD_NAME, (sync_cmd)->id, dpu_sync_cmd_log_level, fmt, ##__VA_ARGS__)

inline void dsim_enable_sync_command(struct dsim_device *dsim)
{
	if (!dsim || !dsim->sync_cmd)
		return;

	dsim->sync_cmd->enabled = true;
}
inline void dsim_disable_sync_command(struct dsim_device *dsim)
{
	if (!dsim || !dsim->sync_cmd)
		return;

	dsim->sync_cmd->enabled = false;
}

static inline bool is_sync_cmd_enabled(const struct dsim_device *dsim)
{
	if (!dsim)
		return false;

	if (!dsim->sync_cmd || !dsim->sync_cmd->enabled)
		return false;

	return true;
}

#define DFT_FPS				(60)	/* default fps */
int dsim_sync_wait_fifo_empty(struct dsim_device *dsim, u32 frames)
{
	int cnt, fps;
	u64 time_us;
	struct dsim_sync_cmd *sync_cmd;

	if (!is_sync_cmd_enabled(dsim))
		return -ENODEV;

	fps = dsim->config.p_timing.vrefresh ? : DFT_FPS;
	time_us = frames * USEC_PER_SEC / fps;
	cnt = time_us / 10;

	sync_cmd = dsim->sync_cmd;

	do {
		if (dsim_reg_sync_header_fifo_is_empty(sync_cmd->id) &&
			dsim_reg_sync_payload_fifo_is_empty(sync_cmd->id))
			break;
		usleep_range(10, 11);
	} while (--cnt);

	if (!cnt)
		dsim_sync_err(sync_cmd, "failed usec(%lld)\n", time_us);

	return cnt;
}

static void dsim_sync_cmd_timeout_handler(struct timer_list *arg)
{
	struct dsim_sync_cmd *sync_cmd = from_timer(sync_cmd, arg, cmd_timer);
	queue_work(system_unbound_wq, &sync_cmd->cmd_work);
}

static void dsim_sync_cmd_fail_detector(struct work_struct *work)
{
	struct dsim_sync_cmd *sync_cmd = container_of(work, struct dsim_sync_cmd, cmd_work);
	struct dsim_device *dsim = sync_cmd->dsim;

	dsim_sync_debug(sync_cmd, "+\n");

	mutex_lock(&sync_cmd->lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_sync_err(dsim, "DSIM is not ready. state(%d)\n",
				dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&sync_cmd->cmd_timer)
			&& dsim_reg_sync_header_fifo_is_empty(sync_cmd->id)) {
		reinit_completion(&sync_cmd->ph_wr_comp);
		dsim_reg_clear_int(sync_cmd->id, DSIM_INTSRC_SYNC_CMD_PH_FIFO_EMPTY);
		sync_cmd->pkt_go = false;
		goto exit;
	}

exit:
	mutex_unlock(&sync_cmd->lock);

	dsim_sync_debug(sync_cmd, "-\n");
}

void dsim_sync_cmd_irq_handler(struct dsim_device *dsim, unsigned int int_src)
{
	struct dsim_sync_cmd *sync_cmd;

	if (!is_sync_cmd_enabled(dsim))
		return;

	sync_cmd = dsim->sync_cmd;
	if (int_src & DSIM_INTSRC_SYNC_CMD_PH_FIFO_EMPTY) {
		del_timer(&sync_cmd->cmd_timer);
		complete(&sync_cmd->ph_wr_comp);
		sync_cmd->pkt_go = false;
		dsim_sync_debug(sync_cmd, "SYNC_CMD_PH_FIFO_EMPTY irq occurs\n");
	}
	if (int_src & DSIM_INTSRC_SYNC_CMD_PL_FIFO_EMPTY)
		dsim_sync_debug(sync_cmd, "SYNC_CMD_PL_FIFO_EMPTY irq occurs\n");

	return;
}

static void
dsim_long_sync_data_wr(struct dsim_sync_cmd *sync_cmd, const u8 buf[], size_t len)
{
	const u8 *end = buf + len;
	u32 payload = 0;

	dsim_sync_debug(sync_cmd, "sync payload length(%zu)\n", len);

	while (buf < end) {
		size_t pkt_size = min_t(size_t, 4, end - buf);

		if (pkt_size == 4)
			payload = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
		else if (pkt_size == 3)
			payload = buf[0] | buf[1] << 8 | buf[2] << 16;
		else if (pkt_size == 2)
			payload = buf[0] | buf[1] << 8;
		else if (pkt_size == 1)
			payload = buf[0];

		dsim_reg_wr_tx_sync_payload(sync_cmd->id, payload);
		dsim_sync_debug(sync_cmd, "pkt_size %zu payload: %#x\n", pkt_size, payload);

		buf += pkt_size;
	}
}

#define PACKET_TYPE(p) ((p)->header[0] & 0x3f)
static bool dsim_sync_fifo_empty_needed(struct dsim_sync_cmd *sync_cmd,
				   struct mipi_dsi_packet *packet)
{
	u8 type = PACKET_TYPE(packet);

	if (type == MIPI_DSI_DCS_READ) {
		dsim_sync_debug(sync_cmd, "read cmd requested\n");
		return true;
	}

	if (mipi_dsi_packet_format_is_long(type) &&
			(packet->payload[0] == MIPI_DCS_SET_COLUMN_ADDRESS ||
			 packet->payload[0] == MIPI_DCS_SET_PAGE_ADDRESS)) {
		dsim_sync_debug(sync_cmd, "partial update cmd requested\n");
		return true;
	}

	/* Check a FIFO level whether writable or not */
	if (!dsim_reg_is_writable_sync_fifo_state(sync_cmd->id)) {
		dsim_sync_debug(sync_cmd, "FIFO exceeds threshold\n");
		return true;
	}

	return false;
}

static int
dsim_wait_for_sync_cmd_fifo_empty(struct dsim_sync_cmd *sync_cmd, bool must_wait)
{
	int ret = 0;

	dsim_sync_debug(sync_cmd, "+\n");

	if (!must_wait) {
		/* timer is running, but already command is transferred */
		if (dsim_reg_sync_header_fifo_is_empty(sync_cmd->id))
			del_timer(&sync_cmd->cmd_timer);

		dsim_sync_debug(sync_cmd, "Doesn't need to wait sync_fifo_completion\n");
		return ret;
	}

	del_timer(&sync_cmd->cmd_timer);
	dsim_sync_debug(sync_cmd, "Waiting for sync_fifo_completion...\n");

	if (!wait_for_completion_timeout(&sync_cmd->ph_wr_comp,
						MIPI_WR_TIMEOUT)) {
		if (dsim_reg_sync_header_fifo_is_empty(sync_cmd->id)) {
			reinit_completion(&sync_cmd->ph_wr_comp);
			dsim_reg_clear_int(sync_cmd->id,
					DSIM_INTSRC_SYNC_CMD_PH_FIFO_EMPTY);
			sync_cmd->pkt_go = false;
			goto exit;
		}
		ret = -ETIMEDOUT;
	}

	if ((is_dsim_enabled(sync_cmd->dsim)) && (ret == -ETIMEDOUT)) {
		dsim_sync_err(sync_cmd, "have timed out(sync command fifo)\n");
		dsim_dump(sync_cmd->dsim);
	}

exit:
	dsim_sync_debug(sync_cmd, "-\n");

	return ret;
}

static bool
dsim_is_writable_sync_pl_fifo_status(struct dsim_sync_cmd *sync_cmd, u32 word_cnt)
{
	if (dsim_reg_get_sync_pl_fifo_remain_bytes(sync_cmd->id) >= word_cnt)
		return true;
	else
		return false;
}

int dsim_check_sync_ph_threshold(struct dsim_sync_cmd *sync_cmd, u32 cmd_cnt)
{
	int cnt = 5000;
	u32 available = 0;

	available = dsim_reg_is_writable_sync_ph_fifo_state(sync_cmd->id, cmd_cnt);

	/* Wait FIFO empty status during 50ms */
	if (!available) {
		dsim_sync_debug(sync_cmd, "waiting for SYNC PH FIFO empty\n");
		do {
			if (dsim_reg_sync_header_fifo_is_empty(sync_cmd->id))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}
	return cnt;
}

int dsim_check_sync_pl_threshold(struct dsim_sync_cmd *sync_cmd, u32 d1)
{
	int cnt = 5000;

	if (!dsim_is_writable_sync_pl_fifo_status(sync_cmd, d1)) {
		do {
			if (dsim_reg_sync_payload_fifo_is_empty(sync_cmd->id))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}

	return cnt;
}

#define DSIM_SYNC_CMD_CHANGE_TO_LONG
static int
_dsim_write_sync_data(struct dsim_sync_cmd *sync_cmd, const struct mipi_dsi_packet *packet)
{
	struct dsim_device *dsim = sync_cmd->dsim;
	struct exynos_drm_crtc *exynos_crtc = dsim_get_exynos_crtc(dsim);
	u8 type = PACKET_TYPE(packet);

	DPU_EVENT_LOG("DSIM_SYNC_COMMAND", exynos_crtc, EVENT_FLAG_LONG,
			dpu_dsi_packet_to_string, packet);

	if (!dsim_check_sync_ph_threshold(sync_cmd, 1)) {
		dsim_sync_err(sync_cmd, "ID(%#x): sync ph(%#x) don't available\n",
				type, packet->header[1]);
		return -EINVAL;
	}

#if defined(DSIM_SYNC_CMD_CHANGE_TO_LONG)
	if (mipi_dsi_packet_format_is_long(type)) {
		if (!dsim_check_sync_pl_threshold(sync_cmd, ALIGN(packet->payload_length, 4))) {
			dsim_sync_err(sync_cmd, "ID(%#x): sync_pl(%#x) don't available\n",
					type, packet->header[1]);
			dsim_sync_err(sync_cmd, "sync_pl_threshold(%d) sync_pl_cnt(%d) wc(%zu)\n",
				sync_cmd->s_cmd_sram,
				(sync_cmd->s_cmd_sram - dsim_reg_get_sync_pl_fifo_remain_bytes(sync_cmd->id)),
				packet->payload_length);
			return -EINVAL;
		}
		dsim_long_sync_data_wr(sync_cmd, packet->payload, packet->payload_length);

		dsim_reg_wr_tx_sync_header(sync_cmd->id, type, packet->header[1], packet->header[2],
				exynos_mipi_dsi_packet_format_is_read(type));
	} else { // short
		u32 payload = 0;
		u8 payload_size = 2;
		if (!dsim_check_sync_pl_threshold(sync_cmd, ALIGN(payload_size, 4))) {
			dsim_sync_err(sync_cmd, "ID(%#x): sync_pl(%#x) don't available\n",
					type, packet->header[1]);
			dsim_sync_err(sync_cmd, "sync_pl_threshold(%d) sync_pl_cnt(%d) wc(%zu)\n",
				sync_cmd->s_cmd_sram,
				(sync_cmd->s_cmd_sram - dsim_reg_get_sync_pl_fifo_remain_bytes(sync_cmd->id)),
				packet->payload_length);
			return -EINVAL;
		}
		payload = packet->header[1] | (packet->header[2] << 8);
		dsim_reg_wr_tx_sync_payload(sync_cmd->id, payload);

		dsim_reg_wr_tx_sync_header(sync_cmd->id, MIPI_DSI_DCS_LONG_WRITE,
				payload_size, 0,
				exynos_mipi_dsi_packet_format_is_read(type));
	}
#else
	if (mipi_dsi_packet_format_is_long(type)) {
		if (!dsim_check_sync_pl_threshold(sync_cmd, ALIGN(packet->payload_length, 4))) {
			dsim_sync_err(sync_cmd, "ID(%#x): sync_pl(%#x) don't available\n",
					type, packet->header[1]);
			dsim_sync_err(sync_cmd, "sync_pl_threshold(%d) sync_pl_cnt(%d) wc(%zu)\n",
				sync_cmd->s_cmd_sram,
				(sync_cmd->s_cmd_sram - dsim_reg_get_sync_pl_fifo_remain_bytes(sync_cmd->id)),
				packet->payload_length);
			return -EINVAL;
		}
		dsim_long_sync_data_wr(sync_cmd, packet->payload, packet->payload_length);
	}

	dsim_reg_wr_tx_sync_header(sync_cmd->id, type, packet->header[1], packet->header[2],
			exynos_mipi_dsi_packet_format_is_read(type));

#endif
	return 0;
}

static void dsim_start_sync_cmd(struct dsim_device *dsim, bool trigger)
{
	const struct decon_device *decon = dsim_get_decon(dsim);

	decon_reg_set_shd_up_option(decon->id, trigger,
				(struct decon_mode *)&decon->config.mode);
}

__weak bool dsim_reg_check_pkt_go_rdy(u32 id, u32 timeout) { return true; }

static int
dsim_write_sync_data(struct dsim_sync_cmd *sync_cmd, const struct mipi_dsi_msg *msg)
{
	struct dsim_device *dsim;
	struct mipi_dsi_packet packet;
	int ret = 0;
	bool must_wait;
	u8 type;

	dsim_sync_debug(sync_cmd, "type[%#x], cmd[%#x], tx_len[%zu]\n",
			msg->type, *(u8 *)msg->tx_buf, msg->tx_len);

	ret = mipi_dsi_create_packet(&packet, msg);
	type = PACKET_TYPE(&packet);
	if (ret) {
		dsim_sync_info(sync_cmd, "ID(%#x): is not supported.\n", type);
		return ret;
	}

	dsim = sync_cmd->dsim;
	if (!sync_cmd->pkt_go && !dsim_reg_check_pkt_go_rdy(dsim->id, USEC_PER_SEC)) {
		dsim_dump(dsim);
		goto err_exit;
	}

	if (dsim_exit_pll_sleep(dsim))
		goto err_exit;

	if (!dsim_reg_is_writable_sync_fifo_state(sync_cmd->id)) {
		dsim_sync_debug(sync_cmd, "FIFO exceeds threshold\n");
		if (dsim_wait_for_sync_cmd_fifo_empty(sync_cmd, true))
			dsim_sync_err(sync_cmd, "previous sync cmd wr timeout\n");
	}

	if (dsim->config.burst_cmd_en && !dsim->burst_cmd.init_done) {
		dsim_reg_set_burst_cmd(dsim->id, dsim->config.mode);
		dsim->burst_cmd.init_done = 1;
	}

	reinit_completion(&sync_cmd->ph_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SYNC_CMD_PH_FIFO_EMPTY);

	/* Run write-fail dectector */
	mod_timer(&sync_cmd->cmd_timer, jiffies + MIPI_WR_TIMEOUT);

	ret = _dsim_write_sync_data(sync_cmd, &packet);
	if (ret)
		goto err_exit;

	must_wait = IS_DCS_STILL_CMD(&packet) ? false :
			dsim_sync_fifo_empty_needed(sync_cmd, &packet);

	dsim_start_sync_cmd(dsim, must_wait);
	dsim_sync_debug(sync_cmd, "ID(%#x): sync cmd transfer is triggered(%#x)\n",
			type, packet.header[1]);

	/* set packet go ready*/
	if (dsim->config.burst_cmd_en) {
		sync_cmd->pkt_go = true;
		dsim_reg_set_packetgo_ready(dsim->id);
	}

	ret = dsim_wait_for_sync_cmd_fifo_empty(sync_cmd, must_wait);
	if (ret < 0)
		dsim_sync_err(sync_cmd, "ID(%#x): sync cmd wr timeout %#x\n",
				type, packet.header[1]);

err_exit:
	dsim_allow_pll_sleep(dsim);
	return ret ? ret : msg->tx_len;
}

int dsim_host_sync_transfer(struct mipi_dsi_host *host,
			    const struct mipi_dsi_msg *msg)
{
	struct dsim_device *dsim = host_to_dsi(host);
	struct dsim_sync_cmd *sync_cmd;
	int ret;

	if (!dsim || !dsim->sync_cmd) {
		pr_warn("dsim sync command is not supported\n");
		return -EINVAL;
	}

	sync_cmd = dsim->sync_cmd;
	dsim_sync_debug(sync_cmd, "+\n");

	mutex_lock(&sync_cmd->lock);
	if (!is_dsim_enabled(dsim)) {
		dsim_sync_err(sync_cmd, "Not ready(%d)\n", dsim->state);
		ret = -EAGAIN;
		goto exit;
	}

	dsim_check_cmd_transfer_mode(dsim, msg);

	if (exynos_mipi_dsi_packet_format_is_read(msg->type)) {
		/* TODO: Implement later */
		dsim_sync_err(sync_cmd, "Not support synchronous read\n");
		ret = -EINVAL;
		goto exit;
	} else
		ret = dsim_write_sync_data(sync_cmd, msg);

exit:
	mutex_unlock(&sync_cmd->lock);

	dsim_sync_debug(sync_cmd, "-\n");

	return ret;
}

ssize_t dsim_dcs_sync_write_buffer(struct mipi_dsi_device *dsi,
				  const void *data, size_t len)
{
	struct mipi_dsi_msg msg = {
		.channel = dsi->channel,
		.tx_buf = data,
		.tx_len = len
	};

	switch (len) {
	case 0:
		return -EINVAL;

	case 1:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE;
		break;

	case 2:
		msg.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		break;

	default:
		msg.type = MIPI_DSI_DCS_LONG_WRITE;
		break;
	}

	if (dsi->mode_flags & MIPI_DSI_MODE_LPM)
		msg.flags |= MIPI_DSI_MSG_USE_LPM;

	return dsim_host_sync_transfer(dsi->host, &msg);
}
EXPORT_SYMBOL(dsim_dcs_sync_write_buffer);

struct dsim_sync_cmd *exynos_sync_cmd_register(struct dsim_device *dsim)
{
	struct dsim_sync_cmd *sync_cmd;

	if (!IS_ENABLED(CONFIG_EXYNOS_DSIM_SYNC_CMD)) {
		pr_info("sync-command feature is not supported\n");
		return NULL;
	}

	sync_cmd = devm_kzalloc(dsim->dev, sizeof(struct dsim_sync_cmd), GFP_KERNEL);
	if (!sync_cmd) {
		pr_err("failed to alloc sync_cmd");
		return NULL;
	}

	sync_cmd->dsim = dsim;
	sync_cmd->id = dsim->id;
	sync_cmd->s_cmd_sram = dsim->s_cmd_sram;

	mutex_init(&sync_cmd->lock);
	init_completion(&sync_cmd->ph_wr_comp);
	init_completion(&sync_cmd->rd_comp);

	timer_setup(&sync_cmd->cmd_timer, dsim_sync_cmd_timeout_handler, 0);
	INIT_WORK(&sync_cmd->cmd_work, dsim_sync_cmd_fail_detector);

	return sync_cmd;
}
