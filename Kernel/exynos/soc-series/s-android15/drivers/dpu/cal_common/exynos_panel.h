/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung EXYNOS Panel Information.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PANEL_H__
#define __EXYNOS_PANEL_H__

#include <linux/kernel.h>
#include <video/videomode.h>
#include <drm/drm_modes.h>
#include <exynos_drm_connector.h>
#include <exynos_drm_crtc.h>

struct dpu_panel_timing {
	unsigned int vactive;
	unsigned int vfp;
	unsigned int vsa;
	unsigned int vbp;

	unsigned int hactive;
	unsigned int hfp;
	unsigned int hsa;
	unsigned int hbp;

	unsigned int vrefresh;
};

struct exynos_dsc {
	bool enabled;
	u32 dsc_count;
	u32 slice_count;
	u32 slice_width;
	u32 slice_height;
};

/* return compressed DSC slice width(unit: pixel cnt) */
static inline u32 get_comp_dsc_width(const struct exynos_dsc *dsc, u32 bpc, u32 align)
{
	unsigned int slice_width_pixels =
				DIV_ROUND_UP(dsc->slice_width * bpc, 8);

	return ALIGN(DIV_ROUND_UP(slice_width_pixels, 3), align);
}

static inline void convert_drm_mode_to_timing(struct dpu_panel_timing *p_timing,
					      const struct drm_display_mode *mode,
                                              const struct exynos_display_mode *exynos_mode)
{
	struct videomode vm;

	drm_display_mode_to_videomode(mode, &vm);

	p_timing->vactive = vm.vactive;
	p_timing->vfp = vm.vfront_porch;
	p_timing->vbp = vm.vback_porch;
	p_timing->vsa = vm.vsync_len;

	p_timing->hactive = vm.hactive;
	p_timing->hfp = vm.hfront_porch;
	p_timing->hbp = vm.hback_porch;
	p_timing->hsa = vm.hsync_len;
	p_timing->vrefresh = drm_mode_vrefresh(mode);

	if (IS_PSCALER_ENABLED(exynos_mode->scaler_type)) {
		p_timing->vactive = exynos_mode->desired_vdisplay;
		p_timing->hactive = exynos_mode->desired_hdisplay;
	}
}
#endif /* __EXYNOS_PANEL_H__ */
