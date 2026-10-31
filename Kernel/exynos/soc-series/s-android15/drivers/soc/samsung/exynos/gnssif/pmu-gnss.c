// SPDX-License-Identifier: GPL-2.0
#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/cal-if.h>
#if IS_ENABLED(CONFIG_EXYNOS_PMU_IF)
#include <soc/samsung/exynos-pmu-if.h>
#else
#include <soc/samsung/exynos-pmu.h>
#endif

#include "pmu-gnss.h"
#include "gnss_prj.h"

/* Connectivity sub system */
#define EXYNOS_GNSS		(0)
/* Target to set */
#define EXYNOS_SET_CONN_TZPC	(0)

#define gnss_pmu_read	exynos_pmu_read
#define gnss_pmu_write	exynos_pmu_write
#define gnss_pmu_update exynos_pmu_update

int gnss_baaw_write(struct gnss_remap *window, void __iomem *remap_reg)
{
	u32 gnss_start = 0;
	u32 gnss_end = 0;
	u32 ap_start = 0;
	u32 authority = 0;
	void __iomem *window_reg = NULL;

	if (window == NULL)
		return -EIO;

	gif_info("BAAW configuration at offset: 0x%X\n", window->offset);

	window_reg = remap_reg + window->offset;

	__raw_writel(window->gnss_start, window_reg + 0x0);
	gnss_start = __raw_readl(window_reg + 0x0);
	if (gnss_start != window->gnss_start) {
		gif_err("gnss_start:0x%08X => Read to verify:0x%08X\n",
			window->gnss_start, gnss_start);
		goto fail;
	}
	__raw_writel(window->gnss_end, window_reg + 0x4);
	gnss_end = __raw_readl(window_reg + 0x4);
	if (gnss_end != window->gnss_end) {
		gif_err("gnss_end:0x%08X => Read to verify:0x%08X\n",
			window->gnss_end, gnss_end);
		goto fail;
	}
	__raw_writel(window->ap_start, window_reg + 0x8);
	ap_start = __raw_readl(window_reg + 0x8);
	if (ap_start != window->ap_start) {
		gif_err("ap_start:0x%08X => Read to verify:0x%08X\n",
			window->ap_start, ap_start);
		goto fail;
	}
	__raw_writel(window->authority, window_reg + 0xC);
	authority = __raw_readl(window_reg + 0xC);
	if (authority != window->authority) {
		gif_err("authority:0x%08X => Read to verify:0x%08X\n",
			window->authority, authority);
		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

int gnss_barac_write(struct gnss_remap *window, void __iomem *remap_reg)
{
	u32 gnss_start = 0;
	u32 gnss_end = 0;
	u32 ap_start = 0;
	u32 ctrl = 0;
	void __iomem *window_reg = NULL;

	if (window == NULL)
		return -EIO;

	gif_info("BARAC window %u configuration offset\n", window->offset);

	window_reg = remap_reg + 0x1800 + (0x80 * window->offset);

	__raw_writel(window->gnss_start, window_reg + 0x8);
	gnss_start = __raw_readl(window_reg + 0x8);
	if (gnss_start != window->gnss_start) {
		gif_err("gnss_start should be: 0x%08X but currently: 0x%08X\n",
			window->gnss_start, gnss_start);
		goto fail;
	}
	__raw_writel(window->gnss_end, window_reg + 0xC);
	gnss_end = __raw_readl(window_reg + 0xC);
	if (gnss_end != window->gnss_end) {
		gif_err("gnss_end should be: 0x%08X but currently: 0x%08X\n",
			window->gnss_end, gnss_end);
		goto fail;
	}
	__raw_writel(window->ap_start, window_reg + 0x10);
	ap_start = __raw_readl(window_reg + 0x10);
	if (ap_start != window->ap_start) {
		gif_err("ap_start should be :0x%08X but currently: 0x%08X\n",
			window->ap_start, ap_start);
		goto fail;
	}
	switch(window->authority & 0xF) {
	case ACCESS_ENABLE_AW | ACCESS_ENABLE_AR:
		ctrl = 0x101;
		break;
	case ACCESS_ENABLE_AW:
		ctrl = 0x100;
		break;
	case ACCESS_ENABLE_AR:
		ctrl = 0x1;
		break;
	default:
		gif_err("authority is invalid:%d\n", window->authority);
		goto fail;
	}
	__raw_writel(ctrl, window_reg + 0x4);
	if (ctrl != __raw_readl(window_reg + 0x4)) {
		gif_err("read_enable should be: 0x%08X but currently: 0x%08X\n",
			ctrl,
			__raw_readl(window_reg + GNSS_BARAC_WINDOW_CTRL));
		goto fail;
	}

	return 0;

fail:
	return -EINVAL;
}

static int gnss_pmu_clear_interrupt(enum gnss_int_clear gnss_int)
{
	switch (gnss_int) {
	case GNSS_INT_WAKEUP_CLEAR:
		break;
	case GNSS_INT_ACTIVE_CLEAR:
		cal_gnss_active_clear();
		break;
	case GNSS_INT_WDT_RESET_CLEAR:
		break;
	default:
		gif_err("Unexpected interrupt value!\n");
		return -EIO;
	}

	return 0;
}

static void gnss_get_apreg(struct gnss_ctl *gc, struct gnss_apreg *apreg)
{
	struct gnss_pdata *pdata = gc->pdata;

	gnss_pmu_read(pdata->pmu_gnss_ctrl_ns, &apreg->CTRL_NS);
	gnss_pmu_read(pdata->pmu_gnss_ctrl_s, &apreg->CTRL_S);
	gnss_pmu_read(pdata->pmu_gnss_stat, &apreg->STAT);
	gnss_pmu_read(pdata->pmu_gnss_debug, &apreg->DEBUG);

	gif_info("APREG CTRL_NS 0x%08X CTRL_S 0x%08X STAT 0x%08X DEBUG 0x%08X\n",
			apreg->CTRL_NS, apreg->CTRL_S, apreg->STAT, apreg->DEBUG);
}

static bool check_apm_int_pending(void)
{
	return true;
}

static int gnss_pmu_release_reset(void)
{
	int ret = 0;

	if (check_apm_int_pending())
		cal_gnss_reset_release();
	else
		ret = -1;

	return ret;
}

static int gnss_pmu_hold_reset(void)
{
	int ret = 0;

	if (check_apm_int_pending()) {
		cal_gnss_reset_assert();
		msleep(50);
	} else
		ret = 1;

	return ret;
}

static int gnss_request_tzpc(void)
{
	int ret;

	ret = (int)exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_GNSS << 31) |
			EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret)
		gif_err("ERR: fail to TZPC setting - %d\n", ret);

	return ret;
}

static int gnss_request_gnss2ap_baaw(struct gnss_ctl *gc)
{
	struct gnss_pdata *pdata = gc->pdata;
	int ret = 0;
	int i = 0;

	gif_info("Config GNSS2AP BAAW\n");

	gif_info("DRAM Configuration\n");

	for (i = 0; i < pdata->num_wnd; i++) {
		ret = gnss_baaw_write(&pdata->wnd_set[i], pdata->remap_reg);
		if (ret)
			goto end;
	}

end:
	return ret;
}

static int gnss_request_gnss2ap_barac(struct gnss_ctl *gc)
{
	struct gnss_pdata *pdata = gc->pdata;
	u32 block_unmap_region = 0;
	int ret = 0;
	int i = 0;

	gif_info("Initialize GNSS2AP BARAC setting\n");

	__raw_writel(0x1, pdata->remap_reg + 0x1000);
	block_unmap_region = __raw_readl(pdata->remap_reg + 0x1000);
	if (block_unmap_region != 0x1) {
		gif_err("block_unmap_region should be: 0x%08X but currently: 0x%08X\n",
			1, block_unmap_region);
		ret = -EINVAL;
		goto end;
	}

	/* TODO: error handling setup */

	gif_info("Initialize each of GNSS2AP BARAC window\n");

	for (i = 0; i < pdata->num_wnd; i++) {
		ret = gnss_barac_write(&pdata->wnd_set[i], pdata->remap_reg);
		if (ret)
			goto end;
	}

end:
	return ret;
}

static int gnss_pmu_power_on(enum gnss_mode mode)
{
	int ret = 0;

	gif_info("mode[%d]\n", mode);

	if (mode == GNSS_POWER_ON) {
		if (cal_gnss_status() > 0) {
			if (check_apm_int_pending()) {
				gif_info("GNSS is already Power on, try reset\n");
				cal_gnss_reset_assert();
			} else
				ret = -1;
		} else {
			gif_info("GNSS Power On\n");
			cal_gnss_init();
		}
	} else {
		if (check_apm_int_pending())
			cal_gnss_reset_release();
		else
			ret = -1;
	}

	return ret;

}

static int gnss_pmu_init_conf(struct gnss_ctl *gc)
{
	return 0;
}

static struct gnssctl_pmu_ops pmu_ops = {
	.init_conf = gnss_pmu_init_conf,
	.hold_reset = gnss_pmu_hold_reset,
	.release_reset = gnss_pmu_release_reset,
	.power_on = gnss_pmu_power_on,
	.clear_int = gnss_pmu_clear_interrupt,
	.req_security = gnss_request_tzpc,
	.get_apreg = gnss_get_apreg,
};

void gnss_get_pmu_ops(struct gnss_ctl *gc)
{
	struct gnss_pdata *pdata = gc->pdata;

	gc->pmu_ops = &pmu_ops;

	switch (pdata->remap_hw) {
	case REMAP_HW_BAAW:
		gif_info("Use BAAW\n");
		gc->pmu_ops->req_address_remap = gnss_request_gnss2ap_baaw;
		break;
	case REMAP_HW_BARAC:
		gif_info("Use BARAC\n");
		gc->pmu_ops->req_address_remap = gnss_request_gnss2ap_barac;
		break;
	default:
		gif_err("Remap HW is not defined:%d\n", pdata->remap_hw);
		break;
	}
}
