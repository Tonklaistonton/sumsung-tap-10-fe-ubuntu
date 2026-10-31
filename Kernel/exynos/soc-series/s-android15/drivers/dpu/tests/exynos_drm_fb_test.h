#ifndef __EXYNOS_DRM_FB_TEST_H
#define __EXYNOS_DRM_FB_TEST_H

int exynos_drm_fb_set_offsets(struct drm_framebuffer *fb);
const struct drm_format_info *exynos_get_format_info(const struct drm_mode_fb_cmd2 *cmd);
dma_addr_t exynos_drm_fb_dma_addr(const struct drm_framebuffer *fb, int index);

#endif /* __EXYNOS_DRM_FB_TEST_H */
