#include <linux/errno.h>
#include <kunit/test.h>
#include <exynos_drm_fb.h>
#include <exynos_drm_modifier.h>

#include "exynos_drm_fb_test.h"

MODULE_IMPORT_NS(EXPORTED_FOR_KUNIT_TESTING);

static void test_exynos_drm_fb_is_colormap_true(struct kunit *test)
{
	struct drm_framebuffer *fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	struct exynos_drm_gem *exynos_gem = kunit_kzalloc(test, sizeof(*exynos_gem), GFP_KERNEL);

	exynos_gem->flags = EXYNOS_DRM_GEM_FLAG_COLORMAP;
	fb->obj[0] = &exynos_gem->base;
	KUNIT_EXPECT_TRUE(test, exynos_drm_fb_is_colormap(fb));
}

static void test_exynos_drm_fb_is_colormap_false(struct kunit *test)
{
	struct drm_framebuffer *fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	struct exynos_drm_gem *exynos_gem = kunit_kzalloc(test, sizeof(*exynos_gem), GFP_KERNEL);

	fb->obj[0] = &exynos_gem->base;
	KUNIT_EXPECT_FALSE(test, exynos_drm_fb_is_colormap(fb));
}

static void test_exynos_drm_fb_set_offsets_invalid_format(struct kunit *test)
{
	struct drm_gem_object gem0;
	struct drm_gem_object gem1;
	struct drm_format_info rgb_format = {
		.is_yuv = false,
	};
	struct drm_format_info yuv_format = {
		.is_yuv = true,
	};
	struct drm_framebuffer invalid_fbs[] = {
		{
			.offsets[0] = 0x0,
			.offsets[1] = 0x0,
			.obj[0] = &gem0,
			.obj[1] = &gem0,
			.format = &rgb_format,
		},
		{
			.offsets[0] = 0x0,
			.offsets[1] = 0x0,
			.obj[0] = &gem0,
			.obj[1] = &gem1,
			.format = &yuv_format,
		},
		{
			.offsets[0] = 0x0,
			.offsets[1] = 0x100,
			.obj[0] = &gem0,
			.obj[1] = &gem0,
			.format = &yuv_format,
		},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(invalid_fbs); ++i)
		KUNIT_EXPECT_NE(test, 0, exynos_drm_fb_set_offsets(&invalid_fbs[i]));
}

struct exynos_fb_offset_test {
	char *name;
	struct drm_framebuffer fb;
	unsigned int expected_offset;
};

struct drm_format_info nv12_format = {
	.format = DRM_FORMAT_NV12,
	.is_yuv = true,
};
struct drm_format_info p010_format = {
	.format = DRM_FORMAT_P010,
	.is_yuv = true,
};

struct drm_gem_object dummy_gem;
struct exynos_fb_offset_test test_cases[] = {
	{
		.name = "nv12 sbwc lossless 32B align",
		.fb = {
			.format = &nv12_format,
			.modifier = DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0,
					SBWC_ALIGN_32),
			.offsets[0] = 0x0,
			.offsets[1] = 0x0,
			.obj[0] = &dummy_gem,
			.obj[1] = &dummy_gem,
			.width = 600,
			.height = 600,
		},
		.expected_offset = 372416,
	},
	{
		.name = "nv12 sbwc lossless 256B align",
		.fb = {
			.format = &nv12_format,
			.modifier = DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0,
					SBWC_ALIGN_256),
			.offsets[0] = 0x0,
			.offsets[1] = 0x0,
			.obj[0] = &dummy_gem,
			.obj[1] = &dummy_gem,
			.width = 600,
			.height = 600,
		},
		.expected_offset = 391936,
	},
	{
		.name = "p010 sbwc lossless 32B align",
		.fb = {
			.format = &p010_format,
			.modifier = DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0,
					SBWC_ALIGN_32),
			.offsets[0] = 0x0,
			.offsets[1] = 0x0,
			.obj[0] = &dummy_gem,
			.obj[1] = &dummy_gem,
			.width = 600,
			.height = 600,
		},
		.expected_offset = 739584,
	},
	{
		.name = "p010 sbwc lossless 256B align",
		.fb = {
			.format = &p010_format,
			.modifier = DRM_FORMAT_MOD_SAMSUNG_SBWC(SBWC_MOD_LOSSLESS, 0,
					SBWC_ALIGN_256),
			.offsets[0] = 0x0,
			.offsets[1] = 0x0,
			.obj[0] = &dummy_gem,
			.obj[1] = &dummy_gem,
			.width = 600,
			.height = 600,
		},
		.expected_offset = 742144,
	},
};

static void test_exynos_drm_fb_set_offsets_valid_format(struct kunit *test)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(test_cases); ++i) {
		exynos_drm_fb_set_offsets(&test_cases[i].fb);
		KUNIT_EXPECT_EQ_MSG(test, test_cases[i].expected_offset,
				test_cases[i].fb.offsets[1], "%s", test_cases[i].name);
	}
}

static void test_exynos_get_format_info_colormap(struct kunit *test)
{
	const struct drm_mode_fb_cmd2 colormap_fb_cmd2 = {
		.pixel_format = DRM_FORMAT_BGRA8888,
		.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_COLORMAP,
	};

	KUNIT_EXPECT_PTR_EQ(test, drm_format_info(colormap_fb_cmd2.pixel_format),
			exynos_get_format_info(&colormap_fb_cmd2));
}

static void test_exynos_get_format_info_sajc(struct kunit *test)
{
	const struct drm_mode_fb_cmd2 sajc_fb_cmd2[] = {
		{
			.pixel_format = DRM_FORMAT_ARGB8888,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
		{
			.pixel_format = DRM_FORMAT_ABGR8888,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
		{
			.pixel_format = DRM_FORMAT_XRGB8888,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
		{
			.pixel_format = DRM_FORMAT_XBGR8888,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
		{
			.pixel_format = DRM_FORMAT_RGB565,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
		{
			.pixel_format = DRM_FORMAT_ARGB2101010,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
		{
			.pixel_format = DRM_FORMAT_ABGR2101010,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
		{
			.pixel_format = DRM_FORMAT_ABGR16161616F,
			.modifier[0] = DRM_FORMAT_MOD_SAMSUNG_SAJC(0, 0, 0),
		},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(sajc_fb_cmd2); ++i) {
		const struct drm_format_info *info = exynos_get_format_info(&sajc_fb_cmd2[i]);
		KUNIT_EXPECT_EQ(test, 2, info->num_planes);
	}
}

static void test_exynos_drm_fb_dma_addr_no_gem(struct kunit *test)
{
	struct drm_framebuffer *fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, exynos_drm_fb_dma_addr(fb, 0));
}

static void test_exynos_drm_fb_dma_addr_single_fd(struct kunit *test)
{
	struct drm_framebuffer *fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	struct exynos_drm_gem *exynos_gem = kunit_kzalloc(test, sizeof(*exynos_gem), GFP_KERNEL);

	exynos_gem->dma_addr = 0x10000;
	fb->obj[0] = &exynos_gem->base;
	fb->obj[1] = &exynos_gem->base;
	fb->offsets[0] = 0x0;
	fb->offsets[1] = 0x100;

	KUNIT_EXPECT_EQ(test, 0x10000, exynos_drm_fb_dma_addr(fb, 0));
	KUNIT_EXPECT_EQ(test, 0x10100, exynos_drm_fb_dma_addr(fb, 1));
}

static void test_exynos_drm_fb_dma_addr_dual_fd(struct kunit *test)
{
	struct drm_framebuffer *fb = kunit_kzalloc(test, sizeof(*fb), GFP_KERNEL);
	struct exynos_drm_gem *exynos_gem1 = kunit_kzalloc(test, sizeof(*exynos_gem1), GFP_KERNEL);
	struct exynos_drm_gem *exynos_gem2 = kunit_kzalloc(test, sizeof(*exynos_gem2), GFP_KERNEL);

	exynos_gem1->dma_addr = 0x10000;
	exynos_gem2->dma_addr = 0x20000;
	fb->obj[0] = &exynos_gem1->base;
	fb->obj[1] = &exynos_gem2->base;
	fb->offsets[0] = 0x0;
	fb->offsets[1] = 0x0;

	KUNIT_EXPECT_EQ(test, 0x10000, exynos_drm_fb_dma_addr(fb, 0));
	KUNIT_EXPECT_EQ(test, 0x20000, exynos_drm_fb_dma_addr(fb, 1));
}

static struct kunit_case exynos_drm_fb_test_cases[] = {
	KUNIT_CASE(test_exynos_drm_fb_is_colormap_true),
	KUNIT_CASE(test_exynos_drm_fb_is_colormap_false),
	KUNIT_CASE(test_exynos_drm_fb_set_offsets_invalid_format),
	KUNIT_CASE(test_exynos_drm_fb_set_offsets_valid_format),
	KUNIT_CASE(test_exynos_get_format_info_colormap),
	KUNIT_CASE(test_exynos_get_format_info_sajc),
	KUNIT_CASE(test_exynos_drm_fb_dma_addr_no_gem),
	KUNIT_CASE(test_exynos_drm_fb_dma_addr_single_fd),
	KUNIT_CASE(test_exynos_drm_fb_dma_addr_dual_fd),
	{}
};

static struct kunit_suite exynos_drm_fb_test_suite = {
	.name = "disp_exynos_fb",
	.test_cases = exynos_drm_fb_test_cases,
};
kunit_test_suite(exynos_drm_fb_test_suite);

MODULE_LICENSE("GPL");
