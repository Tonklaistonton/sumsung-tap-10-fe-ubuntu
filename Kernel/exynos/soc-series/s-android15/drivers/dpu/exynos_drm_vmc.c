// SPDX-License-Identifier: GPL-2.0-only
/* exynos_drm_vmc.c
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <drm/drm_atomic.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>

#include <drm/drm_vblank.h>

#include <exynos_display_common.h>
#include <exynos_drm_crtc.h>
#include <exynos_drm_connector.h>
#include <exynos_drm_bridge.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_dsim.h>
#include <exynos_drm_dpp.h>
#include <exynos_drm_dsim_sync_cmd.h>
#include <exynos_drm_profiler.h>
#include <exynos_drm_debug.h>
#include <exynos_drm_vmc.h>
#include <exynos_drm_hwapg.h>
#include <samsung_drm.h>

#include <exynos_panel.h>

#include <vmc_cal.h>
#include <regs-vmc.h>

#include <dpu_trace.h>

static int dpu_vmc_log_level = 6;
module_param(dpu_vmc_log_level, int, 0600);
MODULE_PARM_DESC(dpu_vmc_log_level, "log level for dpu vmc [default : 6]");

static bool dpu_vmc_sr_disable;
module_param(dpu_vmc_sr_disable, bool, 0600);
MODULE_PARM_DESC(dpu_vmc_sr_disable, "disable for self refresh [default: false]");

static int dpu_vmc_sr_cnt;
module_param(dpu_vmc_sr_cnt, int, 0600);
MODULE_PARM_DESC(dpu_vmc_sr_cnt, "entry count for self refresh [default: 0]");

static int dpu_vmc_sr_cmd = MIPI_DCS_STILLEN;
module_param(dpu_vmc_sr_cmd, int, 0600);
MODULE_PARM_DESC(dpu_vmc_sr_cmd, "entry command for self refresh [default: 0x1B]");

#define vmc_info(vmc, fmt, ...)		\
dpu_pr_info(drv_name((vmc)), 0, dpu_vmc_log_level, fmt, ##__VA_ARGS__)

#define vmc_warn(vmc, fmt, ...)		\
dpu_pr_warn(drv_name((vmc)), 0, dpu_vmc_log_level, fmt, ##__VA_ARGS__)

#define vmc_err(vmc, fmt, ...)		\
dpu_pr_err(drv_name((vmc)), 0, dpu_vmc_log_level, fmt, ##__VA_ARGS__)

#define vmc_debug(vmc, fmt, ...)	\
dpu_pr_debug(drv_name((vmc)), 0, dpu_vmc_log_level, fmt, ##__VA_ARGS__)

enum vmc_state {
	VMC_STATE_OFF = 0,
	VMC_STATE_INIT,
	VMC_STATE_ON,
	VMC_STATE_STILL,
	VMC_STATE_MAX,
};

struct vmc_resources {
	void __iomem *regs;
	void __iomem *pmu_regs;
	struct pinctrl *pinctrl;
	struct pinctrl_state *esync_on;
	struct pinctrl_state *esync_off;
	struct {
		bool en;
		u32 wakeup;
		u32 vsync_frame;
		u32 vsync_frame_mod;
	} irqs;
};

struct vmc_device;
struct exynos_vmc_ops {
	int (*check)(struct vmc_device *vmc ,struct drm_atomic_state *state);
	void (*set_config)(struct vmc_device *vmc, const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode);
	void (*still_off)(struct vmc_device *vmc, bool sync_cmd);
	void (*enable)(struct vmc_device *vmc);
	void (*disable)(struct vmc_device *vmc);
	void (*enable_irqs)(struct vmc_device *vmc);
	void (*disable_irqs)(struct vmc_device *vmc);
	void (*dump)(struct vmc_device *vmc);
	void (*switching)(struct vmc_device *vmc);
};

struct vmc_device {
	struct device *dev;
	struct decon_device *decon;
	spinlock_t slock;
	struct mutex lock;
	struct kthread_work still_work;
	struct completion rw_ready;
	enum vmc_state state;
	struct vmc_resources res;
	struct vmc_config config;
	const struct exynos_vmc_ops *ops;
	s64 reset_cnt;
	atomic64_t trig_cnt;
	atomic64_t block_cnt;
	u32 min_frame_interval_hz;
	u32 esync_hz;

	/* te_ignore
	 * lastclosed true -> ignore true
	 * lastclosed false -> ignore false
	 */
	bool te_ignore;
};

static inline bool IS_VMC_ON_STATE(struct vmc_device *vmc)
{
	return vmc->state == VMC_STATE_INIT||
		vmc->state == VMC_STATE_ON ||
		vmc->state == VMC_STATE_STILL;
}

static int __parse_dt(struct vmc_device *vmc, struct device_node *np)
{
	return 0;
}

void __iomem *__dpu_get_pmu_vmc_addr(void)
{
	void __iomem *regs;

	if (of_have_populated_dt()) {
		struct device_node *nd;
		nd = of_find_compatible_node(NULL, NULL,
				"samsung,exynos-pmu-vmc");
		if (!nd) {
			pr_err("failed find compatible node(pmu-vmc)");
			return NULL;
		}

		regs = of_iomap(nd, 0);
		if (!regs) {
			pr_err("Failed to get pmu-vmc address.");
			return NULL;
		}
	} else {
		pr_err("failed have populated device tree");
		return NULL;
	}

	return regs;
}

static int __remap_regs(struct vmc_device *vmc)
{
	struct device *dev = vmc->dev;
	struct device_node *np = dev->of_node;
	const char *reg_name = "vmc";
	struct resource res;
	int i, ret;

	i = of_property_match_string(np, "reg-names", reg_name);
	if (i < 0) {
		vmc_info(vmc, "failed to find %s SFR region\n", reg_name);
		return 0;
	}

	ret = of_address_to_resource(np, i, &res);
	if (ret)
		return ret;

	vmc->res.regs = devm_ioremap(dev, res.start, resource_size(&res));
	if (IS_ERR_OR_NULL(vmc->res.regs)) {
		vmc_err(vmc, "failed to find %s SFR region\n", reg_name);
		return -EINVAL;
	}

	vmc->res.pmu_regs = __dpu_get_pmu_vmc_addr();

	vmc_regs_desc_init(vmc->res.regs, reg_name);

	return 0;
}

static int __get_pinctrl(struct vmc_device *vmc)
{
	int ret = 0;

	vmc->res.pinctrl = devm_pinctrl_get(vmc->dev);
	if (IS_ERR(vmc->res.pinctrl)) {
		vmc_debug(vmc, "failed to get pinctrl\n");
		ret = PTR_ERR(vmc->res.pinctrl);
		vmc->res.pinctrl = NULL;
		goto err;
	}

	vmc->res.esync_on = pinctrl_lookup_state(vmc->res.pinctrl, "vmc_esync_on");
	if (IS_ERR(vmc->res.esync_on)) {
		vmc_err(vmc, "failed to get vmc_esync_on pin state\n");
		ret = PTR_ERR(vmc->res.esync_on);
		vmc->res.esync_on = NULL;
		goto err;
	}
	vmc->res.esync_off = pinctrl_lookup_state(vmc->res.pinctrl,
			"vmc_esync_off");
	if (IS_ERR(vmc->res.esync_off)) {
		vmc_err(vmc, "failed to get vmc_esync_off pin state\n");
		ret = PTR_ERR(vmc->res.esync_off);
		vmc->res.esync_off = NULL;
		goto err;
	}

err:
	return ret;
}

static void __set_esync_pinctrl(struct vmc_device *vmc, bool en)
{
	int ret;

	if (!vmc->res.pinctrl || !vmc->res.esync_on)
		return;

	ret = pinctrl_select_state(vmc->res.pinctrl,
			en ? vmc->res.esync_on : vmc->res.esync_off);
	if (ret)
		vmc_err(vmc, "failed to control vmc ESYNC signal(%d)\n", en);
	else
		vmc_debug(vmc, "succeeded to control vmc ESYNC signal(%d)\n", en);
}

__weak
u32 decon_reg_get_idle_status(u32 id)
{
	return 0;
}

static void
_vmc_set_runtime_pm(struct vmc_device *vmc, bool en)
{
	vmc_debug(vmc, "+(%s)\n", en ? "get_sync" : "put_sync");

	if (en)
		pm_runtime_get_sync(vmc->dev);
	else
		pm_runtime_put_sync(vmc->dev);

	vmc_debug(vmc, "-\n");
}

static void vmc_configure_still_dma(struct vmc_device *vmc)
{
	struct decon_device *decon = vmc->decon;
	u64 rsc_ch = decon_reg_get_rsc_ch(decon->id);
	int i;

	vmc_debug(vmc,
		 "reconfigure dma of last image for still, rsc:0x%016llx\n",
		 rsc_ch);

	for (i = 0; i < MAX_WIN_PER_DECON; ++i) {
		uint8_t occupancy = (rsc_ch >> (i * 4)) & 0xF;
		struct dpp_device *dpp;
		if ((occupancy & 0x7) == decon->id) {
			struct exynos_hwapg_context *hwapg_ctx;
			dpp = get_dpp_drvdata(i);
			hwapg_ctx = dpp->dpuf->hwapg_ctx;
			if (hwapg_ctx) {
				const struct exynos_hwapg_ops *ops =
					hwapg_ctx->ops;
				ops->block(hwapg_ctx,
					   vmc_to_hwapg_src(0));
			}
			dpp_restore(dpp);
		}
	}
}

static bool dpu_vmc_sr_cmd_gen_path;
module_param(dpu_vmc_sr_cmd_gen_path, bool, 0600);
static void __still_work(struct kthread_work *work)
{
	struct vmc_device *vmc =
		container_of(work, struct vmc_device, still_work);
	struct decon_device *decon = vmc->decon;
	struct exynos_drm_crtc *exynos_crtc = decon->crtc;;
	struct dsim_device *dsim = decon_get_dsim(decon);
	ssize_t err;
	struct mipi_dsi_device *dsi;
	u8 tx_data = (u8)dpu_vmc_sr_cmd;
	int i;

	vmc_debug(vmc, "+(state=%d)\n", vmc->state);

	if (!dsim) {
		vmc_err(vmc, "there is no dsi device to transfer command\n");
		return;
	}
	DPU_ATRACE_BEGIN(__func__);
	mutex_lock(&vmc->lock);
	dsi = dsim->dsi_device;

	if (decon->state == DECON_STATE_HIBERNATION ||
		decon->state == DECON_STATE_OFF ||
		decon->config.mode.op_mode == DECON_COMMAND_MODE)
		goto exit;

	if (vmc->state != VMC_STATE_ON)
		goto exit;

	vmc_debug(vmc, "self refresh trigger!!!(%lld)\n", atomic64_read(&vmc->trig_cnt));

	reinit_completion(&decon->framestart_done);
	DPU_ATRACE_BEGIN(__func__);
	/**
	 * still command is recommended being transmitted
	 *  through a dedicated sync command path due to critical timing.
	 */
	if (!dpu_vmc_sr_cmd_gen_path && dsim->sync_cmd) {
		struct mipi_dsi_msg msg = {
			.channel = dsi->channel,
			.type = MIPI_DSI_DCS_SHORT_WRITE,
			.tx_buf = &tx_data,
			.tx_len = sizeof(tx_data)
		};
		err = dsim_host_sync_transfer(dsi->host, &msg);
	} else
		err = mipi_dsi_dcs_write(dsi, tx_data, NULL, 0);

	for (i = 0; i < MAX_DPUF_CNT; i++) {
		if (decon->config.support_dpuf_hwapg[i]) {
			vmc_configure_still_dma(vmc);
			break;
		}
	}

	if (exynos_crtc->ops->update_request)
		exynos_crtc->ops->update_request(exynos_crtc);

	DPU_ATRACE_END(__func__);

	if (err < 0) {
		vmc_err(vmc, "failed to send still command\n");
		goto exit;
	} else {
		vmc_debug(vmc, "The last image for SR will be sent soon.\n");
		DPU_EVENT_LOG("STILL_ON", exynos_crtc, 0, NULL);
		vmc->state = VMC_STATE_STILL;
	}

	if (exynos_crtc->ops->wait_framestart)
		exynos_crtc->ops->wait_framestart(exynos_crtc);

	vmc_debug(vmc, "-(state=%d)\n", vmc->state);

exit:
	mutex_unlock(&vmc->lock);
	DPU_ATRACE_END(__func__);
}

__weak
u32 vmc_reg_get_vmc_en(void)
{
	return 0;
}

__weak
u32 vmc_reg_get_mask_on(void)
{
	return 1;
}

__weak
void vmc_reg_set_mask_on_force_up(u32 on)
{
	return;
}

static void __vmc_set_ignore_rw(struct vmc_device *vmc)
{
	struct decon_device *decon = vmc->decon;
	struct dsim_device *dsim = decon_get_dsim(decon);
	bool lastclosed = false;

	lastclosed = exynos_drm_bridge_read_lastclose(dsim->panel_bridge);

	mutex_lock(&vmc->lock);
	if (vmc->state == VMC_STATE_OFF || lastclosed == vmc->te_ignore)
		goto exit;

	if (lastclosed) {
		if (vmc_reg_get_mask_on()) {
			vmc_reg_set_mask_on_force_up(0);
			vmc_info(vmc, "RW signal will be ignored\n");
		}
	} else {
		if (!vmc_reg_get_mask_on()) {
			vmc_reg_set_mask_on_force_up(1);
			vmc_info(vmc, "RW signal will not be ignored\n");
		}
	}

	vmc_info(vmc, "ignore_rw updated! (%d->%d)\n", vmc->te_ignore, lastclosed);
	vmc->te_ignore = lastclosed;

exit:
	mutex_unlock(&vmc->lock);
}

void vmc_set_ignore_rw(struct drm_crtc *crtc)
{
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	struct vmc_device *vmc = exynos_crtc->vmc;
	struct decon_device *decon;

	if (!vmc)
		return;

	decon =	vmc->decon;
	if (!decon->config.mode.vmc_mode.en)
		return;

	__vmc_set_ignore_rw(vmc);
}

static irqreturn_t vmc_irq_handler(int irq, void *dev_data)
{
	struct vmc_device *vmc = dev_data;
	struct decon_device *decon = vmc->decon;
	struct exynos_drm_crtc *exynos_crtc = decon->crtc;
	u32 int_src;

	spin_lock(&vmc->slock);

	vmc_debug(vmc, "+(state=%d)\n", vmc->state);
	int_src = vmc_reg_get_interrupt_and_clear();

	if (int_src & VMC_IRQ_WU) {
		vmc_debug(vmc, "WU\n");
		DPU_EVENT_LOG("VMC_WU", exynos_crtc, 0, NULL);
	}

	if (int_src & VMC_IRQ_VSYNC_FRM) {
		vmc_debug(vmc, "VSYNC_FRM\n");
		atomic64_set(&vmc->trig_cnt, vmc->reset_cnt);
		DPU_EVENT_LOG("VMC_VSYNC_FRM", exynos_crtc, 0, NULL);
	}

	if (int_src & VMC_IRQ_VSYNC_FRM_MOD) {
		vmc_debug(vmc, "VSYNC_FRM_MOD\n");
		exynos_drm_crtc_handle_vblank(exynos_crtc);
	}

	if (IS_ERR_OR_NULL(exynos_crtc))
		goto end;

end:
	vmc_debug(vmc, "-(state=%d)\n", vmc->state);

	spin_unlock(&vmc->slock);

	return IRQ_HANDLED;
}

static int __register_irqs(struct vmc_device *vmc)
{
	struct device *dev = vmc->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	vmc->res.irqs.wakeup = of_irq_get_byname(np, "wakeup");
	ret = devm_request_irq(dev, vmc->res.irqs.wakeup, vmc_irq_handler,
			0, dev_name(dev), vmc);
	if (ret) {
		vmc_err(vmc, "failed to request irq\n");
		return ret;
	}
	disable_irq(vmc->res.irqs.wakeup);

	vmc->res.irqs.vsync_frame = of_irq_get_byname(np, "vsync_frm");
	ret = devm_request_irq(dev, vmc->res.irqs.vsync_frame, vmc_irq_handler,
			0, dev_name(dev), vmc);
	if (ret) {
		vmc_err(vmc, "failed to request irq\n");
		return ret;
	}
	disable_irq(vmc->res.irqs.vsync_frame);

	vmc->res.irqs.vsync_frame_mod = of_irq_get_byname(np, "vsync_frm_mod");
	ret = devm_request_irq(dev, vmc->res.irqs.vsync_frame_mod, vmc_irq_handler,
			0, dev_name(dev), vmc);
	if (ret) {
		vmc_err(vmc, "failed to request irq\n");
		return ret;
	}
	disable_irq(vmc->res.irqs.vsync_frame_mod);
	vmc->res.irqs.en = false;

	return ret;
}

static int __init_resources(struct vmc_device *vmc)
{
	int ret;

	ret = __remap_regs(vmc);
	if (ret)
		return ret;

	ret = __get_pinctrl(vmc);
	if (ret)
		return ret;

	ret = __register_irqs(vmc);
	if (ret)
		return ret;

	return 0;
}

static void __enable_irqs(struct vmc_device *vmc)
{
	if (vmc->res.irqs.en)
		return;

	//enable_irq(vmc->res.irqs.wakeup);
	enable_irq(vmc->res.irqs.vsync_frame);
	enable_irq(vmc->res.irqs.vsync_frame_mod);
	vmc->res.irqs.en = true;
}

static void __disable_irqs(struct vmc_device *vmc)
{
	if (!vmc->res.irqs.en)
		return;

	//disable_irq(vmc->res.irqs.wakeup);
	disable_irq(vmc->res.irqs.vsync_frame);
	disable_irq(vmc->res.irqs.vsync_frame_mod);
	vmc->res.irqs.en = false;
}

static int __vmc_check(struct vmc_device *vmc, struct drm_atomic_state *state)
{
	struct drm_crtc *crtc = &vmc->decon->crtc->base;
	struct drm_crtc_state *new_crtc_state =
				drm_atomic_get_new_crtc_state(state, crtc);
	struct exynos_drm_crtc_state *new_exynos_crtc_state =
				to_exynos_crtc_state(new_crtc_state);
	struct dpu_panel_timing p_timing;
	const struct exynos_display_mode *exynos_mode;
	u32 emission_num;

	exynos_mode = &new_exynos_crtc_state->exynos_mode;
	if (!exynos_mode || !exynos_mode->vhm)
		return 0;

	if (vmc->esync_hz % vmc->min_frame_interval_hz) {
		vmc_err(vmc, "esync_hz is not multiple of min_frame_interval_hz\n");
		return -EINVAL;
	}
	emission_num = vmc->esync_hz / vmc->min_frame_interval_hz;

	convert_drm_mode_to_timing(&p_timing, &new_crtc_state->adjusted_mode, exynos_mode);
	if ((p_timing.vsa + p_timing.vbp + p_timing.vactive + p_timing.vfp)
			% emission_num) {
		vmc_err(vmc, "hsync_num is not multiple of emission_num\n");
		return -EINVAL;
	}

	if (p_timing.hsa < 4) {
		vmc_err(vmc, "hsa(%u) < 4\n", p_timing.hsa);
		return -EINVAL;
	}

	return 0;
}

static void
__vmc_set_config(struct vmc_device *vmc, const struct drm_display_mode *mode,
		 const struct exynos_display_mode *exynos_mode)
{
	struct vmc_config *config = &vmc->config;

	if (!exynos_mode->vhm)
		return;

	config->clk_sel = VMC_CLK_CLK1;
	config->emission_num = vmc->esync_hz / vmc->min_frame_interval_hz;
	convert_drm_mode_to_timing(&config->p_timing, mode, exynos_mode);

	vmc_debug(vmc, "emission num(%u), esync(%d), min_frame_interval_hz(%u)",
		config->emission_num, drm_mode_vrefresh(mode), vmc->min_frame_interval_hz);
}

static void
__vmc_set_still_off(struct vmc_device *vmc, bool sync_cmd)
{
	struct decon_device *decon = vmc->decon;
	struct dsim_device *dsim = decon_get_dsim(decon);
	struct mipi_dsi_device *dsi = dsim->dsi_device;
	u8 tx_data = (u8)MIPI_DCS_STILLOFF;
	ssize_t err;

	vmc_debug(vmc, "self refresh off!!!(state=%d)\n", vmc->state);

	DPU_ATRACE_BEGIN(__func__);
	/**
	 * still command is recommended being transmitted
	 *  through a dedicated sync command path due to critical timing.
	 */
	if (!dpu_vmc_sr_cmd_gen_path && sync_cmd && dsim->sync_cmd) {
		struct mipi_dsi_msg msg = {
			.channel = dsi->channel,
			.type = MIPI_DSI_DCS_SHORT_WRITE,
			.tx_buf = &tx_data,
			.tx_len = sizeof(tx_data)
		};
		err = dsim_host_sync_transfer(dsi->host, &msg);
	} else
		err = mipi_dsi_dcs_write(dsi, tx_data, NULL, 0);

	if (err < 0)
		vmc_err(vmc, "failed to send still off command\n");
	DPU_ATRACE_END(__func__);
}

static void __vmc_enable(struct vmc_device *vmc)
{
	_vmc_set_runtime_pm(vmc, true);

	vmc_reg_init(&vmc->config);
	__enable_irqs(vmc);
	vmc_reg_start();

	__set_esync_pinctrl(vmc, true);

	if (vmc->decon->state == DECON_STATE_INIT)
		vmc->state = VMC_STATE_STILL;
	else
		vmc->state = VMC_STATE_INIT;
}

static void __vmc_disable(struct vmc_device *vmc)
{
	__set_esync_pinctrl(vmc, false);

	vmc_reg_stop();
	__disable_irqs(vmc);

	_vmc_set_runtime_pm(vmc, false);
	vmc->state = VMC_STATE_OFF;
}

static void __vmc_switching(struct vmc_device *vmc)
{
	_vmc_set_runtime_pm(vmc, true);

	vmc_reg_switching_init(&vmc->config);
	__enable_irqs(vmc);
	vmc_reg_start();

	__set_esync_pinctrl(vmc, true);
	vmc->state = VMC_STATE_INIT;
}

__weak
void __vmc_pmu_dump(void __iomem *regs)
{
	pr_info("%s function is not defined\n", __func__);
	return;
}

static void _vmc_dump(struct vmc_device *vmc)
{
	bool active;

	active = pm_runtime_active(vmc->dev);
	pr_info("VMC power %s state\n", active ? "on" : "off");
	__vmc_pmu_dump(vmc->res.pmu_regs);
	__vmc_dump(vmc->res.regs);
}

struct exynos_vmc_ops vmc_ops = {
	.check = __vmc_check,
	.set_config = __vmc_set_config,
	.still_off = __vmc_set_still_off,
	.enable = __vmc_enable,
	.disable = __vmc_disable,
	.enable_irqs = __enable_irqs,
	.disable_irqs = __disable_irqs,
	.dump = _vmc_dump,
	.switching = __vmc_switching,
};

static ssize_t vmc_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vmc_device *vmc = dev_get_drvdata(dev);
	bool en;

	if (kstrtobool(buf, &en)) {
		vmc_err(vmc, "invalid vmc enable value\n");
		return count;
	}

	if (en)
		vmc_atomic_enable(vmc);
	else
		vmc_atomic_disable(vmc);

	return count;
}

void vmc_dump(void *ctx);
static ssize_t vmc_dump_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct vmc_device *vmc = dev_get_drvdata(dev);
	enum vmc_state org_state;

	if (IS_ERR_OR_NULL(vmc))
		goto exit;

	org_state = vmc->state;

	vmc->state = VMC_STATE_ON;

	vmc_dump(vmc);

	vmc->state = org_state;

exit:
	return count;
}

static DEVICE_ATTR_WO(vmc_en);
static DEVICE_ATTR_WO(vmc_dump);

static const struct attribute *vmc_attrs[] = {
	&dev_attr_vmc_en.attr,
	&dev_attr_vmc_dump.attr,
	NULL
};

static int vmc_probe(struct platform_device *pdev)
{
	struct vmc_device *vmc;
	struct device *dev = &pdev->dev;
	int ret;

	vmc = devm_kzalloc(dev, sizeof(struct vmc_device), GFP_KERNEL);
	if (!vmc)
		return -ENOMEM;

	vmc->dev = dev;
	ret = __parse_dt(vmc, dev->of_node);
	if (ret)
		goto err;

	pm_runtime_enable(&pdev->dev);

	ret = __init_resources(vmc);
	if (ret)
		goto err;

	spin_lock_init(&vmc->slock);
	mutex_init(&vmc->lock);
	init_completion(&vmc->rw_ready);
	complete(&vmc->rw_ready);

	vmc->ops = &vmc_ops;

	vmc->state = VMC_STATE_OFF;

	platform_set_drvdata(pdev, vmc);

	kthread_init_work(&vmc->still_work, __still_work);

	ret = sysfs_create_files(&dev->kobj, vmc_attrs);
	if (ret)
		vmc_err(vmc, "failed to create sysfs\n");

	vmc_info(vmc, "successfully probed");

	return 0;

err:
	vmc_err(vmc, "probe failed");

	return ret;

}

static int vmc_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id vmc_of_match[] = {
	{ .compatible = "samsung,exynos-vmc" },
	{},
};

struct platform_driver vmc_driver = {
	.probe		= vmc_probe,
	.remove		= vmc_remove,
	.driver		= {
		.name	= "exynos-vmc",
		.owner = THIS_MODULE,
		.of_match_table = vmc_of_match,
	},
};

bool is_vmc_still_blocked(void *ctx)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return false;

	return (atomic64_read(&vmc->block_cnt) > 0);
}

static inline bool __need_still(struct vmc_device *vmc)
{
	return !dpu_vmc_sr_disable &&
               !is_vmc_still_blocked(vmc) &&
	       vmc->state == VMC_STATE_ON &&
               atomic64_dec_and_test(&vmc->trig_cnt);
}

#define OFFSET (1)
static inline void __update_still_reset_cnt(struct vmc_device *vmc,
                                            struct drm_crtc_state *new_crtc_state)
{
	struct exynos_drm_crtc_state *new_exynos_crtc_state;
	struct exynos_drm_connector *exynos_conn;
	const struct exynos_drm_connector_funcs *funcs;
	const struct exynos_freq_step *freq_step = NULL;

	exynos_conn = crtc_get_exynos_conn(new_crtc_state, DRM_MODE_CONNECTOR_DSI);
	if (exynos_conn) {
		new_exynos_crtc_state = to_exynos_crtc_state(new_crtc_state);
		funcs =	exynos_conn->funcs;
		if (funcs->get_freq_step)
			freq_step = funcs->get_freq_step(exynos_conn,
					new_exynos_crtc_state->frame_interval_ns);
	}

	spin_lock_irq(&vmc->slock);
	if (dpu_vmc_sr_cnt)
		vmc->reset_cnt = dpu_vmc_sr_cnt - OFFSET;
	else if (freq_step)
		vmc->reset_cnt = (s64)(freq_step->dur[0]) - OFFSET;

	spin_unlock_irq(&vmc->slock);
}

void vmc_still_block(void *ctx, bool cancel_work)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return;

	atomic64_inc(&vmc->block_cnt);
	if (cancel_work)
		kthread_cancel_work_sync(&vmc->still_work);
}

void vmc_still_unblock(void *ctx)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return;

	atomic64_add_unless(&vmc->block_cnt, -1, 0);
}

void vmc_still_on(void *ctx)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;
	struct exynos_drm_crtc *exynos_crtc;

	if (!vmc)
		return;

	if (!__need_still(vmc))
		return;

	decon = vmc->decon;
	exynos_crtc = decon->crtc;

	kthread_queue_work(&exynos_crtc->worker, &vmc->still_work);
	vmc_debug(vmc, "queue still_work\n");
}

void vmc_still_off(void *ctx, bool sync_cmd)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;

	if (!vmc)
		return;

	decon = vmc->decon;

	mutex_lock(&vmc->lock);
	if (vmc->state != VMC_STATE_STILL)
		goto exit;

	if (vmc->ops && vmc->ops->still_off)
		vmc->ops->still_off(vmc, sync_cmd);

	DPU_EVENT_LOG("STILL_OFF", decon->crtc, 0, NULL);

	vmc->state = VMC_STATE_INIT;
exit:
	mutex_unlock(&vmc->lock);
}

int vmc_atomic_check(void *ctx, struct drm_atomic_state *state)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return 0;

	if (vmc->ops && vmc->ops->check)
		return vmc->ops->check(vmc, state);

	return 0;
}

void vmc_dump(void *ctx)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return;

	if (!IS_VMC_ON_STATE(vmc)) {
		vmc_info(vmc, "vmc state is not on\n");
		return;
	}

	if (vmc->ops && vmc->ops->dump)
		vmc->ops->dump(vmc);
}

void vmc_atomic_enable(void *ctx)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;

	if (!vmc)
		return;

	decon = vmc->decon;
	if (!decon->config.mode.vmc_mode.en)
		return;

	vmc_info(vmc, "+++(state=%d)\n", vmc->state);
	mutex_lock(&vmc->lock);
	if (IS_VMC_ON_STATE(vmc)) {
		vmc_info(vmc, "already enabled\n");
		goto exit;
	}

	if (vmc->ops && vmc->ops->enable)
		vmc->ops->enable(vmc);

	DPU_EVENT_LOG("VMC_ENABLED", decon->crtc, 0, NULL);
	vmc_info(vmc, "---(state=%d)\n", vmc->state);
exit:
	mutex_unlock(&vmc->lock);
}

void vmc_atomic_disable(void *ctx)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;

	if (!vmc)
		return;

	decon = vmc->decon;

	if (!decon->config.mode.vmc_mode.en)
		return;

	vmc_info(vmc, "+++(state=%d)\n", vmc->state);
	mutex_lock(&vmc->lock);
	if (vmc->state == VMC_STATE_OFF) {
		vmc_info(vmc, "already disabled\n");
		goto exit;
	}

	if (vmc->ops && vmc->ops->disable)
		vmc->ops->disable(vmc);

	DPU_EVENT_LOG("VMC_DISABLED", decon->crtc, 0, NULL);
	vmc_info(vmc, "---(state=%d)\n", vmc->state);
exit:
	mutex_unlock(&vmc->lock);
}

void vmc_atomic_exit_hiber(void *ctx)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return;

	if (vmc->ops && vmc->ops->enable_irqs)
		vmc->ops->enable_irqs(vmc);
}

void vmc_atomic_enter_hiber(void *ctx)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return;

	if (vmc->ops && vmc->ops->disable_irqs)
		vmc->ops->disable_irqs(vmc);
}

void vmc_atomic_switching_prepare(void *ctx, struct drm_atomic_state *old_state)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *new_crtc_state;

	if (!vmc)
		return;

	decon = vmc->decon;
	exynos_crtc = vmc->decon->crtc;
	new_crtc_state = drm_atomic_get_new_crtc_state(old_state, &exynos_crtc->base);

	__update_still_reset_cnt(vmc, new_crtc_state);
	if (!new_crtc_state->plane_mask)
		return;

	kthread_cancel_work_sync(&vmc->still_work);

	vmc_debug(vmc, "+++(state=%d)\n", vmc->state);

	mutex_lock(&vmc->lock);
	if (vmc->ops && vmc->ops->still_off)
		vmc->ops->still_off(vmc, false);

	vmc->state = VMC_STATE_ON;
	vmc_debug(vmc, "---(state=%d)\n", vmc->state);
	mutex_unlock(&vmc->lock);

	return;
}

void vmc_atomic_switching(void *ctx, bool en)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;

	if (!vmc)
		return;

	decon = vmc->decon;

	vmc_info(vmc, "+++(state=%d)\n", vmc->state);
	mutex_lock(&vmc->lock);

	if (en) {
		if (IS_VMC_ON_STATE(vmc)) {
			vmc_info(vmc, "already enabled\n");
			goto exit;
		}

		if (vmc->ops && vmc->ops->enable)
			vmc->ops->switching(vmc);
	} else {
		if (vmc->state == VMC_STATE_OFF) {
			vmc_info(vmc, "already disabled\n");
			goto exit;
		}

		if (vmc->ops && vmc->ops->disable)
			vmc->ops->disable(vmc);
	}

	DPU_EVENT_LOG("VMC_SWITCHING", decon->crtc, 0, NULL);
	vmc_info(vmc, "---(state=%d)\n", vmc->state);
exit:
	mutex_unlock(&vmc->lock);
}

void vmc_atomic_set_config(void *ctx,
				const struct drm_display_mode *mode,
				const struct exynos_display_mode *exynos_mode)
{
	struct vmc_device *vmc = ctx;

	if (!vmc)
		return;

	vmc_info(vmc, "+++(state=%d)\n", vmc->state);

	if (vmc->ops && vmc->ops->set_config)
		vmc->ops->set_config(vmc, mode, exynos_mode);

	vmc_info(vmc, "---(state=%d)\n", vmc->state);
}

void vmc_atomic_lock(void *ctx, bool lock)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;

	if (!vmc)
		return;

	decon = vmc->decon;
	if (!decon->config.mode.vmc_mode.en)
		return;

	vmc_debug(vmc, "+++(state=%d)\n", vmc->state);
	if (lock) {
		mutex_lock(&vmc->lock);
	} else
		mutex_unlock(&vmc->lock);
}

void vmc_atomic_update(void *ctx, struct drm_atomic_state *old_state)
{
	struct vmc_device *vmc = ctx;
	struct decon_device *decon;
	struct exynos_drm_crtc *exynos_crtc;
	struct drm_crtc_state *new_crtc_state;

	if (!vmc)
		return;

	decon = vmc->decon;
	exynos_crtc = vmc->decon->crtc;
	if (!decon->config.mode.vmc_mode.en)
		return;

	new_crtc_state = drm_atomic_get_new_crtc_state(old_state, &exynos_crtc->base);
	__update_still_reset_cnt(vmc, new_crtc_state);
	if (!new_crtc_state->plane_mask)
		return;

	__vmc_set_ignore_rw(vmc);
	vmc_still_off(vmc, true);

	mutex_lock(&vmc->lock);
	vmc_debug(vmc, "+++(state=%d)\n", vmc->state);

	if (vmc->state == VMC_STATE_INIT)
		vmc->state = VMC_STATE_ON;
	mutex_unlock(&vmc->lock);

	vmc_debug(vmc, "---(state=%d)\n", vmc->state);
}

void *vmc_register(struct decon_device *decon)
{
	struct platform_device *vmc_pdev;
	struct device_node *vmc_np;
	struct vmc_device *vmc = NULL;

	vmc_np = of_parse_phandle(decon->dev->of_node, "vmc", 0);
	if (!vmc_np)
		return NULL;

	vmc_pdev = of_find_device_by_node(vmc_np);
	if (vmc_pdev)
		vmc = platform_get_drvdata(vmc_pdev);
	of_node_put(vmc_np);

	vmc->decon = decon;
	vmc->min_frame_interval_hz = decon->restriction.min_frame_interval_hz;
	vmc->esync_hz = decon->restriction.esyn_hz;
	vmc_info(vmc, "decon%d display path will operate through the control of VMC\n", decon->id);

	return vmc;
}
