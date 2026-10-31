subdir-ccflags-y			:= -I$(srctree)/$(src)

obj-$(CONFIG_DPU_EXYNOS_KUNIT_TEST)                     += dpu_exynos_test.o
dpu_exynos_test-$(CONFIG_DPU_EXYNOS_KUNIT_TEST)         += exynos_drm_fb_test.o      \
                                                           exynos_drm_partial_test.o \
                                                           dpu_kunit_helper.o

dpu_exynos_test-$(CONFIG_DPU_DSIM_BIST_KUNIT_TEST)      += dpu_kunit_dsim_bist.o
dpu_exynos_test-$(CONFIG_DPU_DECON_COLORMAP_KUNIT_TEST) += dpu_kunit_decon_colormap.o
dpu_exynos_test-$(CONFIG_DPU_DMA_KUNIT_TEST)            += dpu_kunit_dma_buf.o dpu_kunit_helper.o
