// SPDX-License-Identifier: GPL-2.0

#ifndef _EXYNOS_HW_DECOMP_TEST_H
#define _EXYNOS_HW_DECOMP_TEST_H

extern struct exynos_hw_decomp_desc *decomp_desc;
extern int exynos_hw_decomp_probe(struct platform_device *pdev);
extern void __iomem *exynos_hw_decomp_remap(struct device_node *node, int index);
extern void exynos_hw_decomp_register_cpupm_callback(struct exynos_hw_decomp_desc *desc);
extern int exynos_hw_decomp_cpupm_notifier(struct notifier_block *self,
					unsigned long action, void *v);
extern int exynos_hw_decomp_suspend(struct device *dev);
extern int exynos_hw_decomp_resume(struct device *dev);

#endif
