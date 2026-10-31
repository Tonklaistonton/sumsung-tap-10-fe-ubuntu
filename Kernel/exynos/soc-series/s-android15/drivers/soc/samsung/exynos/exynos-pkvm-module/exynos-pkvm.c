/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS pKVM module loader
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/arm-smccc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/smp.h>

#include <asm/kvm_asm.h>
#include <asm/kvm_pkvm_module.h>

#include <soc/samsung/exynos/exynos-hvc.h>
#include <soc/samsung/exynos/exynos-pkvm-module.h>


int __kvm_nvhe_exynos_pkvm_module_init(const struct pkvm_module_ops *ops);
void __kvm_nvhe_exynos_pkvm_hvc_handler(struct user_pt_regs *host_ctxt);

extern unsigned long long __kvm_nvhe_pkvm_mod_base;
extern unsigned long long __kvm_nvhe_pkvm_mod_size;
extern unsigned long long __kvm_nvhe_pkvm_mod_kimg_offset;
extern pkvm_mod_vm_request_t __kvm_nvhe_pkvm_mod_vm_request;

static unsigned long exynos_pkvm_mod_fid;
static bool exynos_pkvm_mod_enabled;


unsigned long exynos_hvc(unsigned long hvc_fid,
			 unsigned long arg1,
			 unsigned long arg2,
			 unsigned long arg3,
			 unsigned long arg4)
{
	struct arm_smccc_res res;

	if (exynos_pkvm_mod_enabled)
		arm_smccc_hvc(KVM_HOST_SMCCC_ID(exynos_pkvm_mod_fid),
				hvc_fid, arg1, arg2, arg3, arg4, 0, 0, &res);
	else
		arm_smccc_hvc(hvc_fid, arg1, arg2, arg3, arg4, 0, 0, 0, &res);

	return res.a0;
};
EXPORT_SYMBOL(exynos_hvc);

static void __exynos_pkvm_update_fg_write_trap(void *hfgwtr_mask)
{
	struct sysreg_mask *mask = (struct sysreg_mask *)hfgwtr_mask;
	unsigned long ret;

	ret = exynos_hvc(HVC_FID_PKVM_UPDATE_FG_WRITE_TRAP,
			 mask->set_mask, mask->clear_mask,
			 0, 0);
	if (ret)
		pr_err("%s: Fine-Grained write trap update failure (%#lx)\n",
				__func__, ret);
}

int exynos_pkvm_update_fg_write_trap(unsigned long sysreg_bit_pos,
				     bool trap)
{
	struct sysreg_mask mask;

	if (sysreg_bit_pos >= 64) {
		pr_err("%s: Invalid sysreg_bit_pos (%ld)\n",
				__func__, sysreg_bit_pos);
		return -EINVAL;
	}

	if (trap) {
		mask.set_mask = BIT(sysreg_bit_pos);
		mask.clear_mask = 0;
	} else {
		mask.set_mask = 0;
		mask.clear_mask = BIT(sysreg_bit_pos);
	}

	on_each_cpu(__exynos_pkvm_update_fg_write_trap, (void *)&mask, 1);

	return 0;

}
EXPORT_SYMBOL(exynos_pkvm_update_fg_write_trap);

static void exynos_pkvm_update_hcr_el2(void *discard)
{
	unsigned long ret;

	ret = exynos_hvc(HVC_FID_PKVM_UPDATE_HCR, 0, 0, 0, 0);
	if (ret)
		pr_err("%s: HCR_EL2 update failure (%#lx)\n",
				__func__, ret);
}

static void exynos_pkvm_set_cpu_context(void *discard)
{
	unsigned long ret;

	ret = exynos_hvc(HVC_FID_PKVM_SET_CPU_CONTEXT, 0, 0, 0, 0);
	if (ret)
		pr_err("%s: CPU context update failure (%#lx)\n",
				__func__, ret);
}

pkvm_mod_vm_request_t exynos_pkvm_module_get_s2mpu_ptr(void)
{
	static bool is_set = false;

	if (is_set)
		return 0;

	is_set = true;

	return __kvm_nvhe_pkvm_mod_vm_request;
}
EXPORT_SYMBOL(exynos_pkvm_module_get_s2mpu_ptr);

static int exynos_pkvm_module_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *mem_np;
	struct resource res;
	unsigned long token;
	unsigned long long symbol;
	int ret;

	mem_np = of_parse_phandle(np, "memory_region", 0);
	if (mem_np == NULL) {
		pr_err("%s: Fail to find harx_binary node\n", __func__);
		return -ENODEV;
	}

	ret = of_address_to_resource(mem_np, 0, &res);
	if (ret) {
		pr_err("%s: Fail to get binary base and size\n", __func__);;
		return ret;
	}

	__kvm_nvhe_pkvm_mod_base = res.start;
	__kvm_nvhe_pkvm_mod_size = res.end - res.start;

	/* Calculate kimg_offset with any symbol */
	symbol = (unsigned long long)exynos_hvc;
	__kvm_nvhe_pkvm_mod_kimg_offset = __phys_to_kimg(symbol) - symbol - kaslr_offset();

	ret = pkvm_load_el2_module(__kvm_nvhe_exynos_pkvm_module_init, &token);
	if (ret && ret != -EOPNOTSUPP) {
		pr_err("%s: Exynos pKVM module init fail ret[%d]\n",
				__func__, ret);
		return ret;
	}

	if (ret == -EOPNOTSUPP) {
		pr_info("%s: pKVM module disabled, pKVM not enabled\n",
				__func__);
		return 0;
	}

	exynos_pkvm_mod_enabled = true;

	ret = pkvm_register_el2_mod_call(&__kvm_nvhe_exynos_pkvm_hvc_handler, token);
	if (ret < 0) {
		pr_err("%s: Module HVC handler registration fail ret[%d]\n",
				__func__, ret);
		return ret;
	} else {
		exynos_pkvm_mod_fid = (unsigned long)ret;
	}

	/* Update HCR_EL2 bits which plug-in set at bootloader stage */
	on_each_cpu(exynos_pkvm_update_hcr_el2, NULL, 1);

	on_each_cpu(exynos_pkvm_set_cpu_context, NULL, 1);


	pr_notice("%s: Exynos pKVM module init done\n", __func__);

	return 0;
}

static const struct of_device_id exynos_pkvm_module_of_match[] = {
	{ .compatible = "samsung,exynos-pkvm-module" },
	{},
};

static struct platform_driver exynos_pkvm_module_driver = {
        .probe  = exynos_pkvm_module_probe,
        .driver = {
                .name = "exynos-pkvm-module",
                .owner = THIS_MODULE,
		.of_match_table = exynos_pkvm_module_of_match,
        },
};

module_platform_driver(exynos_pkvm_module_driver);

MODULE_DESCRIPTION("Exynos pKVM module driver");
MODULE_AUTHOR("<kn_hong.choi@samsung.com@samsung.com>");
MODULE_LICENSE("GPL");
