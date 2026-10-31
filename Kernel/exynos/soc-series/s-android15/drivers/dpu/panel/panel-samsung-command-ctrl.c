// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung Panel Command Control Driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_platform.h>
#include <linux/module.h>

#include "panel-samsung-drv.h"
#include "../exynos_drm_dsim_sync_cmd.h"

static bool force_burst_cmd;
module_param(force_burst_cmd, bool, 0600);
MODULE_PARM_DESC(force_burst_cmd, "converting MIPI cmd to burst command");

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#define MIN_SIZE_TO_DSIMFC 5
static int min_size_to_dsimfc = MIN_SIZE_TO_DSIMFC;
module_param(min_size_to_dsimfc, int, 0600);

static int force_dsimfc;
module_param(force_dsimfc, int, 0600);
MODULE_PARM_DESC(force_dsimfc, "sending MIPI cmd through DSIMFC DMA");
#endif

bool force_sync_cmd;
module_param(force_sync_cmd, bool, 0600);
MODULE_PARM_DESC(force_sync_cmd, "converting MIPI WRDISBV(51h) cmd to sync cmd");

static bool force_burst_sync_cmd;
module_param(force_burst_sync_cmd, bool, 0600);
MODULE_PARM_DESC(force_burst_sync_cmd, "converting MIPI sync cmd to burst sync command");

static void exynos_panel_send_pps_command(struct exynos_panel *ctx,
				const struct exynos_panel_mode *pmode,
				enum exynos_panel_cmd type)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	const struct exynos_display_cmd_set *send_cmds;
	struct drm_dsc_picture_parameter_set *payload;
	struct exynos_mipi_dsi_msg *emsg;
	struct mipi_dsi_msg *msg;

	send_cmds = &pmode->cmd_list[type];
	emsg = send_cmds->emsg;
	msg = &emsg->msg;
	payload = (struct drm_dsc_picture_parameter_set *)msg->tx_buf;
	mipi_dsi_picture_parameter_set(dsi, payload);
}

static int exynos_panel_send_generic_command(struct exynos_panel *ctx,
					const struct exynos_panel_mode *pmode,
					enum exynos_panel_cmd type)
{
	const struct exynos_display_cmd_set *send_cmds;
	struct exynos_mipi_dsi_msg *emsg;
	struct mipi_dsi_msg *msg;
	int i, len, delay, ret = 0;
	u8 *payload;

	if (!pmode)
		return -EINVAL;

	send_cmds = &pmode->cmd_list[type];
	if (send_cmds->cmd_cnt == 0)
		return 0;

	if (force_burst_cmd)
		goto burst_tx;

non_burst_tx:
	emsg = send_cmds->emsg;
	for (i = 0; i < send_cmds->cmd_cnt; ++i) {
		msg = &emsg[i].msg;
		payload = (u8 *)msg->tx_buf;
		len = msg->tx_len;
		delay = emsg[i].wait_ms;
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
		if (force_dsimfc & (1 << ctx->id) && len >= min_size_to_dsimfc)
			ret = drm_mipi_fcmd_write(ctx, payload, len, 1);
		else
			ret = exynos_dcs_write(ctx, payload, len);
#else
		ret = exynos_dcs_write(ctx, payload, len);
#endif
		if (ret < 0)
			pr_err("failed to transfer ret(%d)\n", ret);

		if (delay)
			msleep(delay);
	}
	return ret;

burst_tx:
	emsg = send_cmds->emsg;
	for (i = 0; i < send_cmds->cmd_cnt; ++ i){
		msg = &emsg[i].msg;
		payload = (u8 *)msg->tx_buf;
		len = msg->tx_len;
		if (emsg[i].wait_ms != 0) {
			panel_warn(ctx, "could not convert to burst command due to cmd delay\n");
			exynos_drm_cmdset_cleanup(ctx);
			goto non_burst_tx;
		}
		exynos_drm_cmdset_add(ctx, MIPI_DSI_DCS_LONG_WRITE, len, payload);
	}

	ret = exynos_drm_cmdset_flush(ctx, false, false);
	if (ret < 0) {
		panel_warn(ctx, "failed to send burst cmd. fallback to non-burst mode\n");
		goto non_burst_tx;
	}

	return ret;
}

int exynos_panel_send_sync_command(struct exynos_panel *ctx,
				const struct exynos_display_cmd_set *send_cmds)
{
	struct exynos_mipi_dsi_msg *emsg;
	struct mipi_dsi_msg *msg;
	int i, len, delay, ret = 0;
	u8 *payload;

	if (force_sync_cmd)
		panel_info(ctx, "+\n");
	else
		panel_debug(ctx, "+\n");

	if (!send_cmds)
		return -EINVAL;

	if (send_cmds->cmd_cnt == 0)
		return ret;

	if (force_burst_sync_cmd)
		goto burst_sync_tx;

non_burst_sync_tx:
	emsg = send_cmds->emsg;
	for (i = 0; i < send_cmds->cmd_cnt; ++i) {
		msg = &emsg[i].msg;
		payload = (u8 *)msg->tx_buf;
		len = msg->tx_len;
		delay = emsg[i].wait_ms;
		ret = exynos_dcs_sync_write(ctx, payload, len);
		if (ret < 0)
			pr_err("failed to sync transfer ret(%d)\n", ret);

		if (delay)
			msleep(delay);
	}

	if (force_sync_cmd)
		panel_info(ctx, "transfer result: %s\n",
			(ret < 0) ? (ret == -EAGAIN ? "try again" : "fail") : "pass");
	else
		panel_debug(ctx, "transfer result: %s\n",
			(ret < 0) ? (ret == -EAGAIN ? "try again" : "fail") : "pass");

	return ret;

burst_sync_tx:
	/* TODO: Implement later */
	pr_info("burst sync tx will be supported later\n");
	goto non_burst_sync_tx;
}

static int panel_command_ctrl_disable(struct drm_panel *panel)
{
	struct exynos_panel *ctx = to_exynos_panel(panel);
	const struct exynos_panel_mode *pmode = ctx->current_mode;

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));
	panel_info(ctx, "+\n");
	exynos_panel_send_generic_command(ctx, pmode, PANEL_OFF_CMDS);
	panel_info(ctx, "-\n");

	return 0;
}

static int panel_command_ctrl_unprepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx = to_exynos_panel(panel);

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	panel_info(ctx, "+\n");
	if (is_panel_active(ctx) || is_panel_idle(ctx))
		exynos_panel_set_power(ctx, false);
	ctx->state = EXYNOS_PANEL_STATE_OFF;
	panel_info(ctx, "-\n");
	return 0;
}

static int panel_command_ctrl_prepare(struct drm_panel *panel)
{
	struct exynos_panel *ctx = to_exynos_panel(panel);

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	panel_info(ctx, "+\n");
	exynos_panel_set_power(ctx, true);
	panel_info(ctx, "-\n");

	return 0;
}

static int panel_command_ctrl_set_brightness(struct exynos_panel *ctx, u16 br)
{
	struct exynos_panel_desc *pdesc = ctx->desc;
	u16 brightness;

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	panel_debug(ctx, "+\n");

	brightness = pdesc->brightness.endian_swap ? (br & 0xff) << 8 | br >> 8 : br;

	if (force_sync_cmd) {
		struct exynos_display_cmd_set send_cmds = {0, };
		struct exynos_mipi_dsi_msg emsg = {0, };
		struct mipi_dsi_msg *br_msg;
		u8 cmd[3] = {MIPI_DCS_SET_DISPLAY_BRIGHTNESS, brightness & 0xff, brightness >> 8};
		int ret = 0;

		send_cmds.cmd_cnt = 1;
		send_cmds.emsg = &emsg;
		br_msg = &send_cmds.emsg->msg;
		br_msg->tx_buf = &cmd;
		br_msg->tx_len = sizeof(cmd);

		if (mutex_is_locked(&ctx->mode_lock))
			mutex_unlock(&ctx->mode_lock);

		ret = exynos_panel_send_sync_command(ctx, &send_cmds);

		if (!mutex_is_locked(&ctx->mode_lock))
			mutex_lock(&ctx->mode_lock);

		if (ret >= 0)
			return 0;
	}

	return exynos_dcs_set_brightness(ctx, brightness);
}

static void panel_command_ctrl_set_resolution(struct exynos_panel *ctx,
				const struct exynos_panel_mode *pmode)
{
	const struct exynos_display_mode *exynos_mode = &pmode->exynos_mode;

	exynos_panel_send_generic_command(ctx, pmode, PANEL_PRE_PPS_CMDS);
	if (exynos_mode->dsc.enabled) {
		exynos_dcs_compression_mode(ctx, true);
		exynos_panel_send_pps_command(ctx, pmode, PANEL_PPS_CMDS);
	} else {
		exynos_dcs_compression_mode(ctx, false);
	}
	exynos_panel_send_generic_command(ctx, pmode, PANEL_POST_PPS_CMDS);

	exynos_panel_send_generic_command(ctx, pmode, PANEL_MRES_CMDS);
}

static void panel_command_ctrl_set_vrefresh(struct exynos_panel *ctx,
				const struct exynos_panel_mode *pmode)
{
#if IS_ENABLED(CONFIG_EXYNOS_CMD_SVSYNC) || IS_ENABLED(CONFIG_EXYNOS_CMD_SVSYNC_ONOFF)
	exynos_panel_send_generic_command(ctx, pmode, PANEL_TECTRL_CMDS);
#endif
	exynos_panel_send_generic_command(ctx, pmode, PANEL_VREF_CMDS);
}

static void panel_command_ctrl_set_operation_switch(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode)
{
	const struct exynos_display_cmd_set *send_cmds;

	if (!pmode)
		return;

	send_cmds = &pmode->cmd_list[PANEL_MODE_SWITCH_CMDS];
	exynos_panel_send_sync_command(ctx, send_cmds);
}

static void panel_command_ctrl_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode, unsigned int flags)
{
	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	if (!is_panel_active(ctx) && ctx->state != EXYNOS_PANEL_STATE_IDLE)
		return;

	if (SEAMLESS_MODESET_MRES & flags)
		panel_command_ctrl_set_resolution(ctx, pmode);
	if (SEAMLESS_MODESET_VREF & flags)
		panel_command_ctrl_set_vrefresh(ctx, pmode);
	if (SEAMLESS_MODESET_OPMS & flags)
		panel_command_ctrl_set_operation_switch(ctx, pmode);
}

static void panel_command_ctrl_lp_mode_set(struct exynos_panel *ctx,
		const struct exynos_panel_mode *pmode)
{
	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	if (!is_panel_active(ctx))
		return;

	panel_info(ctx, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));
}

#define MAX_STEP_PATTERN	(5)
static void panel_command_ctrl_freq_step_change(struct exynos_panel *ctx,
						const struct exynos_freq_step *freq_step)
{
	u8 passwd_unlock[] =	{ 0xF0, 0x5A, 0x5A };
	u8 passwd_lock[] =	{ 0xF0, 0xA5, 0xA5 };
	u8 first_dur_cmd0[] =	{ 0xB0, 0x00, 0x92, 0xBD };
	u8 first_dur_cmd1[] =	{ 0xBD, 0x00, 0x00};
	u8 first_cnt_cmd0[] =	{ 0xB0, 0x00, 0x83, 0xBD };
	u8 first_cnt_cmd1[] =	{ 0xBD, 0x00 };

	u8 other_dur_cmd0[] =	{ 0xB0, 0x00, 0x9E, 0xBD };
	u8 other_dur_cmd1[] =	{ 0xBD, 0x00, 0x00, 0x00, 0x00, 0x01, 0xDC, 0x01, 0xDC, 0x01,
				0xDC, 0x01, 0xDC, 0x01, 0xDC, 0x01, 0xDC };
	u8 other_cnt_cmd0[] =	{ 0xB0, 0x00, 0xAE, 0xBD };
	u8 other_cnt_cmd1[] =	{ 0xBD, 0x00, 0x00, 0x00, 0x00 };
	int i;

	panel_debug(ctx, "+\n");

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	if (!is_panel_active(ctx) && ctx->state != EXYNOS_PANEL_STATE_IDLE)
		return;

	first_dur_cmd1[2] = freq_step->dur[0] - 2;
	first_cnt_cmd1[1] = freq_step->num_repeat[0];
	for (i = 1; i < freq_step->cnt; ++i) {
		other_dur_cmd1[i * 2] = (freq_step->dur[i] - 2);
		other_cnt_cmd1[i] = freq_step->num_repeat[i] - 1;
	}

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));
	exynos_dcs_write(ctx, passwd_unlock, sizeof(passwd_unlock));
	exynos_dcs_write(ctx, passwd_lock, sizeof(passwd_lock));

	exynos_dcs_write(ctx, first_dur_cmd0, sizeof(first_dur_cmd0));
	exynos_dcs_write(ctx, first_dur_cmd1, sizeof(first_dur_cmd1));
	exynos_dcs_write(ctx, first_cnt_cmd0, sizeof(first_cnt_cmd0));
	exynos_dcs_write(ctx, first_cnt_cmd1, sizeof(first_cnt_cmd1));

	exynos_dcs_write(ctx, other_dur_cmd0, sizeof(other_dur_cmd0));
	exynos_dcs_write(ctx, other_dur_cmd1, sizeof(other_dur_cmd1));
	exynos_dcs_write(ctx, other_cnt_cmd0, sizeof(other_cnt_cmd0));
	exynos_dcs_write(ctx, other_cnt_cmd1, sizeof(other_cnt_cmd1));
	panel_debug(ctx, "-\n");
}

static void panel_command_ctrl_panel_update(struct exynos_panel *ctx)
{
	const struct exynos_panel_mode *pmode = ctx->current_mode;

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	if (!is_panel_active(ctx) && ctx->state != EXYNOS_PANEL_STATE_IDLE)
		return;

	panel_debug(ctx, "+\n");
	exynos_panel_send_generic_command(ctx, pmode, PANEL_UPDATE_CMDS);
	panel_debug(ctx, "-\n");
}

static int panel_command_ctrl_enable(struct drm_panel *panel)
{
	struct exynos_panel *ctx = to_exynos_panel(panel);
	const struct exynos_panel_mode *pmode = ctx->current_mode;
	const struct exynos_display_mode *exynos_mode = &pmode->exynos_mode;
	const struct drm_display_mode *mode;

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	if (!pmode) {
		panel_err(ctx, "no current mode set\n");
		return -EINVAL;
	}
	mode = &pmode->mode;

	panel_info(ctx, "+\n");

	if (exynos_mode->dsc.enabled) {
		exynos_dcs_compression_mode(ctx, true);
		exynos_panel_send_pps_command(ctx, pmode, PANEL_PPS_CMDS);
	}

	exynos_panel_send_generic_command(ctx, pmode, PANEL_ON_CMDS);
	panel_command_ctrl_set_vrefresh(ctx, pmode);
	exynos_panel_send_generic_command(ctx, pmode, PANEL_MRES_CMDS);
	exynos_panel_send_generic_command(ctx, pmode, PANEL_MODE_SWITCH_CMDS);
	exynos_panel_send_generic_command(ctx, pmode, PANEL_DISP_ON_CMDS);

	ctx->state = EXYNOS_PANEL_STATE_ON;

	panel_info(ctx, "-\n");

	return 0;
}

static bool panel_command_ctrl_lastclose(struct exynos_panel *ctx, bool close)
{
	const struct exynos_panel_mode *pmode = ctx->current_mode;
	bool updated = false;
	int ret = 0;

	WARN_ON(!mutex_is_locked(&ctx->mode_lock));

	if (!is_panel_active(ctx) && !is_panel_idle(ctx))
		goto exit;

	if (!pmode) {
		panel_err(ctx, "no current mode set\n");
		goto exit;
	}

	if (close)
		ret = exynos_panel_send_generic_command(ctx, pmode, PANEL_CLOSE_CMDS);
	else
		ret = exynos_panel_send_generic_command(ctx, pmode, PANEL_OPEN_CMDS);

	if (!ret) {
		panel_info(ctx, "- : (%s) -> (%s)\n", ctx->lastclosed ? "close" : "open",
				close ? "close" : "open");;
		ctx->lastclosed = close;
		updated = true;
	}

exit:
	return updated;
}

static const struct drm_panel_funcs panel_command_ctrl_drm_funcs = {
	.disable = panel_command_ctrl_disable,
	.unprepare = panel_command_ctrl_unprepare,
	.prepare = panel_command_ctrl_prepare,
	.enable = panel_command_ctrl_enable,
	.get_modes = exynos_panel_get_modes,
};

static const struct exynos_panel_funcs panel_command_ctrl_exynos_funcs = {
	.set_brightness = panel_command_ctrl_set_brightness,
	.set_lp_mode = panel_command_ctrl_lp_mode_set,
	.mode_set = panel_command_ctrl_mode_set,
	.lastclose = panel_command_ctrl_lastclose,
	.change_freq_step = panel_command_ctrl_freq_step_change,
	.panel_update = panel_command_ctrl_panel_update,
};

const struct exynos_panel_func_set samsung_panel_command_ctrl = {
	.panel_func = &panel_command_ctrl_drm_funcs,
	.exynos_panel_func = &panel_command_ctrl_exynos_funcs,
};

static const struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,command-ctrl", .data = &samsung_panel_command_ctrl },
	{ }
};
MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static struct mipi_dsi_driver exynos_panel_driver = {
	.probe = exynos_panel_probe,
	.remove = exynos_panel_remove,
	.shutdown = exynos_panel_shutdown,
	.driver = {
		.name = "panel-samsung-command-ctrl",
		.of_match_table = exynos_panel_of_match,
	},
};
module_mipi_dsi_driver(exynos_panel_driver);

MODULE_AUTHOR("Beobki Jung <beobki.jung@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung panel command control driver");
MODULE_LICENSE("GPL");
