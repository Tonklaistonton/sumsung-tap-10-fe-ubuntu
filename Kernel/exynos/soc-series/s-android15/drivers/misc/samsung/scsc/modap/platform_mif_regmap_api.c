/****************************************************************************
 *
 * Copyright (c) 2014 - 2024 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/version.h>

//#include <soc/samsung/exynos-smc.h>
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_HVC)
#include <soc/samsung/exynos/exynos-hvc.h>
#endif
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
#include <soc/samsung/exynos/exynos-s2mpu.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
#include "../modap/platform_mif_regmap_api.h"
#include "../mif_reg.h"
#include "../baaw.h"
#include "../regmap_register.h"
#else
#include "modap/platform_mif_regmap_api.h"
#include "mif_reg.h"
#include "baaw.h"
#include "regmap_register.h"
#endif

#ifdef CONFIG_WLBT_KUNIT
#include "../kunit/kunit_platform_mif_regmap_api.c"
#elif defined CONFIG_SCSC_WLAN_KUNIT_TEST
#include "../kunit/kunit_net_mock.h"
#endif
#define MBOX_COUNT 5

#define COMP_RET(x, y) do { \
	if (x != y) {\
		pr_err("%s failed at L%d", __FUNCTION__, __LINE__); \
		return -1; \
	} \
} while (0)

#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
#define WLBT_SUBSYS_NAME "WLBT"
#endif

const char regmap_lookup_table[REGMAP_LIST_SIZE][REGMAP_LIST_LENGTH] = {
	{"samsung,syscon-phandle"},
	{"samsung,dbus_baaw-syscon-phandle"},
	{"samsung,boot_cfg-syscon-phandle"},
	{"samsung,pbus_baaw-syscon-phandle"},
	{"samsung,wlbt_remap-syscon-phandle"},
#ifdef CONFIG_SCSC_I3C
	{"samsung,i3c_apm_pmic-syscon-phandle"},
#endif
};

const u32 mailbox_ap_to_wlbt_reg[MBOX_COUNT] = {
	MAILBOX_WLBT_REG(INTGR0),
	MAILBOX_WLBT_REG(INTCR0),
	MAILBOX_WLBT_REG(INTMR0),
	MAILBOX_WLBT_REG(INTSR0),
	MAILBOX_WLBT_REG(INTMSR0)
};

const u32 mailbox_wlbt_to_ap_reg[MBOX_COUNT] = {
	MAILBOX_WLBT_REG(INTGR1),
	MAILBOX_WLBT_REG(INTCR1),
	MAILBOX_WLBT_REG(INTMR1),
	MAILBOX_WLBT_REG(INTSR1),
	MAILBOX_WLBT_REG(INTMSR1)
};

const char* mailbox_reg_name[MBOX_COUNT] = {
	"INTGR [0, 1]",
	"INTCR [0, 1]",
	"INTMR [0, 1]",
	"INTSR [0, 1]",
	"INTMSR [0, 1]"
};

struct regmap *platform_mif_get_regmap(
	struct platform_mif *platform,
	enum regmap_name _regmap_name)
{
	return platform->regmap[_regmap_name];
}

int platform_mif_set_regmap(struct platform_mif *platform)
{
	int i;

	platform->regmap = (struct regmap **)devm_kzalloc(platform->dev, sizeof(struct regmap *) * REGMAP_LIST_SIZE, GFP_KERNEL);

	/* Completion event and state used to indicate CFG_REQ IRQ occurred */
	init_completion(&platform->cfg_ack);
	platform->boot_state = WLBT_BOOT_IN_RESET;

	for (i = 0; i < REGMAP_LIST_SIZE; i++) {
		//struct regmap *regmap = platform_mif_get_regmap(platform, i);
		const char *lookup_table = regmap_lookup_table[i];

		platform->regmap[i] = syscon_regmap_lookup_by_phandle(
			platform->dev->of_node,
			lookup_table);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "[%s] -- regmap[%d] : %p\n", __func__, i, platform->regmap[i]);

		if (IS_ERR(platform->regmap[i])) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"%s failed. Aborting. %ld\n",
				lookup_table,
				PTR_ERR(platform->regmap[i]));
			devm_kfree(platform->dev, platform->regmap);
			return -EINVAL;
		}
	}

	return 0;
}

static void platform_wlbt_print_mailbox_subsystem(enum scsc_mif_abs_target target)
{
	u32 ap_to_wlbt;
	u32 wlbt_to_ap;
	int i;

	for (i = 0; i < MBOX_COUNT; i++) {
		const char* reg_name = mailbox_reg_name[i];
		ap_to_wlbt = platform_mif_reg_read(target, mailbox_ap_to_wlbt_reg[i]);
		wlbt_to_ap = platform_mif_reg_read(target, mailbox_wlbt_to_ap_reg[i]);

		SCSC_TAG_INFO(PLAT_MIF, "%s : [0x%08x, 0x%08x]\n",
			reg_name, ap_to_wlbt, wlbt_to_ap);
	}
}

static void platform_wlbt_print_mailbox(struct platform_mif *platform)
{
	platform_wlbt_print_mailbox_subsystem(SCSC_MIF_ABS_TARGET_WLAN);
	platform_wlbt_print_mailbox_subsystem(SCSC_MIF_ABS_TARGET_WPAN);
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
	platform_wlbt_print_mailbox_subsystem(SCSC_MIF_ABS_TARGET_PMU);
#endif
}

static void platform_wlbt_print_pmureg_register(struct platform_mif *platform)
{
	int i;
	struct regmap *regmap = platform_mif_get_regmap(platform, PMUREG);

	for (i = 0; i < sizeof(pmureg_register_offset) / sizeof(const u32); i++) {
		u32 val;
		regmap_read(regmap, pmureg_register_offset[i], &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "%s -- 0x%x\n",
			pmureg_register_name[i], val);
	}
}

static void platform_wlbt_print_boot_cfg_register(struct platform_mif *platform)
{
	int i;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);
	for (i = 0; i < sizeof(boot_cfg_register_offset) / sizeof(const u32); i++) {
		u32 val;
		regmap_read(regmap, boot_cfg_register_offset[i], &val);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "%s -- 0x%x\n",
			boot_cfg_register_name[i], val);
	}
}

#ifdef CONFIG_SOC_S5E8825
#ifdef CONFIG_SOC_S5E8535
#ifdef CONFIG_SOC_S5E5515
static void platform_wlbt_print_karam(struct platform_mif *platform)
{
	u32 val;
	uint32_t ka_val = 0;
	unsigned int ka_addr = PMU_BOOT_RAM_START;
	unsigned int ka_offset = 0;
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

	SCSC_TAG_INFO(PLAT_MIF, "AP accesses KARAM\n");
	regmap_write(regmap, PMU_BOOT, PMU_BOOT_AP_ACC);
	regmap_read(regmap, PMU_BOOT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "Updated BOOT_SOURCE: 0x%x\n", val);
	SCSC_TAG_INFO(PLAT_MIF, "KARAM Info:\n");
	while (ka_addr <= PMU_BOOT_RAM_END) {
		regmap_read(regmap, ka_addr, &ka_val);
		SCSC_TAG_INFO(PLAT_MIF, "0x%08x(0x%x)", ka_val, ka_offset);
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_offset += (unsigned int)sizeof(unsigned int);
	}

	SCSC_TAG_INFO(PLAT_MIF, "WLBT PMU accesses KARAM\n");

	regmap_write(regmap, PMU_BOOT, PMU_BOOT_PMU_ACC);
	regmap_read(regmap, PMU_BOOT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "Updated BOOT_SOURCE: 0x%x\n", val);
	regmap_read(regmap, PMU_BOOT_ACK, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "BOOT_CFG_ACK 0x%x\n", val);
}
#endif
#endif
#endif

int platform_mif_set_dbus_baaw(struct platform_mif *platform)
{
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
	int ret;
#endif
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_HVC)
	unsigned int val;

	val = exynos_hvc(
		HVC_FID_SET_BAAW_WINDOW,
		0,
		((u64)(WLBT_DBUS_BAAW_0_START >> BAAW_BIT_SHIFT) << 32) | (WLBT_DBUS_BAAW_0_END >> BAAW_BIT_SHIFT),
		(platform->mem_start >> BAAW_BIT_SHIFT),
		WLBT_BAAW_ACCESS_CTRL);

	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_START, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_START: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_END, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_END: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_REMAP, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_REMAP: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_INIT_DONE, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);

#else
#if IS_ENABLED(CONFIG_WLBT_IS_BARAC_PRESENT)
	/* BARAC configurations */
	int i;
	unsigned int val1, val2, val3, val4, val5, val6;
	struct regmap *regmap = platform_mif_get_regmap(platform, DBUS_BAAW);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW begin\n");

	regmap_write(regmap, BARAC_D_WLBT_BARAC_BA, BARAC_BA_VAL);
	regmap_read(regmap, BARAC_D_WLBT_BARAC_BA, &val6);

	for (i = 0; i < DBUS_BARAC_W_COUNT; i++) {

		regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_CTRL0, BARAC_CTRL_VAL);
		regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_CTRL0, &val1);

		regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_BASE_ADDR0, WLBT_DBUS_BARAC_0_START >> BARAC_BIT_SHIFT);
		regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_BASE_ADDR0, &val2);

		regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_END_ADDR0, WLBT_DBUS_BARAC_0_END >> BARAC_BIT_SHIFT);
		regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_END_ADDR0, &val3);

		regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_REMAP_ADDR0, platform->mem_start >> BARAC_BIT_SHIFT);
		regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_REMAP_ADDR0, &val4);

		regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_STATUS0, &val5);

		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Updated DBUS_BAAW_%d(BA, ctrl, start, end, remap, status):(0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
				  i, val6, val1, val2, val3, val4, val5);

	}
#else
	/* BAAW configurations */
	int i;
	unsigned int val1, val2, val3, val4;
	struct regmap *regmap = platform_mif_get_regmap(platform, DBUS_BAAW);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW begin\n");
	for (i = 0; i < DBUS_BAAW_COUNT; i++) {
		regmap_write(regmap, dbus_baaw_offset[i][0], dbus_baaw_value[i][0] >> BAAW_BIT_SHIFT);
		regmap_read(regmap, dbus_baaw_offset[i][0], &val1);
		COMP_RET(dbus_baaw_value[i][0] >> BAAW_BIT_SHIFT, val1);

		regmap_write(regmap, dbus_baaw_offset[i][1], dbus_baaw_value[i][1] >> BAAW_BIT_SHIFT);
		regmap_read(regmap, dbus_baaw_offset[i][1], &val2);
		COMP_RET(dbus_baaw_value[i][1] >> BAAW_BIT_SHIFT, val2);

		regmap_write(regmap, dbus_baaw_offset[i][2], platform->mem_start >> BAAW_BIT_SHIFT);
		regmap_read(regmap, dbus_baaw_offset[i][2], &val3);

		regmap_write(regmap, dbus_baaw_offset[i][3], dbus_baaw_value[i][3]);
		regmap_read(regmap, dbus_baaw_offset[i][3], &val4);
		COMP_RET(dbus_baaw_value[i][3], val4);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Updated DBUS_BAAW_%d(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
				  i, val1, val2, val3, val4);

	}
#endif
#endif
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
        if (!platform->dbus_baaw0_allowed_list_set) {
#if IS_ENABLED(CONFIG_WLBT_IS_BARAC_PRESENT)
		ret = exynos_s2mpu_subsystem_set_allowlist(WLBT_SUBSYS_NAME, platform->mem_start,
							   dbus_baaw_value[0][2] - dbus_baaw_value[0][1] + 1);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "update harx BAAW0: 0x%lx, %ld %s(%d)\n",
				  platform->mem_start, dbus_baaw_value[0][2] - dbus_baaw_value[0][1] + 1,
				  ret == 0 ? "SUCCESS" : "FAIL", ret);
#else
		ret = exynos_s2mpu_subsystem_set_allowlist(WLBT_SUBSYS_NAME, platform->mem_start,
							   WLBT_DBUS_BAAW_0_END - WLBT_DBUS_BAAW_0_START + 1);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "update harx BAAW0: 0x%lx, %ld %s(%d)\n",
				  platform->mem_start, WLBT_DBUS_BAAW_0_END - WLBT_DBUS_BAAW_0_START + 1,
				  ret == 0 ? "SUCCESS" : "FAIL", ret);
#endif
		platform->dbus_baaw0_allowed_list_set = 1;
        }
#endif
	return 0;
}

int platform_mif_set_pbus_baaw(struct platform_mif *platform)
{
	struct regmap *regmap = platform_mif_get_regmap(platform, PBUS_BAAW);
#if IS_ENABLED(CONFIG_WLBT_IS_BARAC_PRESENT)
	/* BARAC configurations */
	unsigned int val1, val2, val3, val4, val5, val6;
	int i;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CBUS BARAC begin\n");

	regmap_write(regmap, BARAC_C_WLBT_BARAC_BA, BARAC_BA_VAL);
	regmap_read(regmap, BARAC_C_WLBT_BARAC_BA, &val6);

	for (i = 0; i < PBUS_BARAC_W_COUNT; i++) {
		regmap_write(regmap,
			pbus_baaw_offset[i][0],
			pbus_baaw_value[i][0]);
		regmap_read(regmap,
			pbus_baaw_offset[i][0],
			&val1);
		COMP_RET(pbus_baaw_value[i][0], val1);

		regmap_write(regmap,
			pbus_baaw_offset[i][1],
			pbus_baaw_value[i][1] >> BARAC_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][1],
			&val2);
		COMP_RET(pbus_baaw_value[i][1] >> BARAC_BIT_SHIFT, val2);

		regmap_write(regmap,
			pbus_baaw_offset[i][2],
			pbus_baaw_value[i][2] >> BARAC_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][2],
			&val3);
		COMP_RET(pbus_baaw_value[i][2] >> BARAC_BIT_SHIFT, val3);

		regmap_write(regmap,
			pbus_baaw_offset[i][3],
			pbus_baaw_value[i][3] >> BARAC_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][3],
			&val4);
		COMP_RET(pbus_baaw_value[i][3] >> BARAC_BIT_SHIFT, val4);

		regmap_read(regmap, pbus_baaw_offset[i][4], &val5);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_%d(BA, ctrl, start, end, remap, status ):(0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  i, val6, val1, val2, val3, val4, val5);

	}
#else
	/* BAAW configurations */
	unsigned int val1, val2, val3, val4;
	int i;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CBUS_BAAW begin\n");
	for (i = 0; i < PBUS_BAAW_COUNT; i++) {
		regmap_write(regmap,
			pbus_baaw_offset[i][0],
			pbus_baaw_value[i][0] >> BAAW_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][0],
			&val1);
		COMP_RET(pbus_baaw_value[i][0] >> BAAW_BIT_SHIFT, val1);

		regmap_write(regmap,
			pbus_baaw_offset[i][1],
			pbus_baaw_value[i][1] >> BAAW_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][1],
			&val2);
		COMP_RET(pbus_baaw_value[i][1] >> BAAW_BIT_SHIFT, val2);

		regmap_write(regmap,
			pbus_baaw_offset[i][2],
			pbus_baaw_value[i][2] >> BAAW_BIT_SHIFT);
		regmap_read(regmap,
			pbus_baaw_offset[i][2],
			&val3);
		COMP_RET(pbus_baaw_value[i][2] >> BAAW_BIT_SHIFT, val3);

		regmap_write(regmap,
			pbus_baaw_offset[i][3],
			pbus_baaw_value[i][3]);
		regmap_read(regmap,
			pbus_baaw_offset[i][3],
			&val4);
		COMP_RET(pbus_baaw_value[i][3], val4);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_%d(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  i, val1, val2, val3, val4);

	}
#endif
	return 0;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
void platform_mif_set_memlog_baaw(struct platform_mif *platform)
{
	unsigned int val1, val2, val3, val4, val5;
	dma_addr_t paddr = platform->paddr;
	((void )(val5));
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
	int ret;
#endif
	struct regmap *regmap = platform_mif_get_regmap(platform, DBUS_BAAW);

	((void )(val5));

#if IS_ENABLED(CONFIG_WLBT_EXYNOS_HVC)
#if IS_ENABLED(CONFIG_WLBT_IS_BARAC_PRESENT)
	/* TODO: Need to see how to call these calls for BARAC configuration. */
#else
	exynos_hvc(
		HVC_FID_SET_BAAW_WINDOW,
		1,
		((u64)(WLBT_DBUS_BAAW_1_START >> BAAW_BIT_SHIFT) << 32) | (WLBT_DBUS_BAAW_1_END >> BAAW_BIT_SHIFT),
		(paddr >> BAAW_BIT_SHIFT),
		WLBT_BAAW_ACCESS_CTRL);

	val1 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_START, 0, 0, 0);
	val2 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_END, 0, 0, 0);
	val3 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_REMAP, 0, 0, 0);
	val4 = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + MEMLOG_BAAW_WLBT_INIT_DONE, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated DBUS_BAAW_1(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);
#endif
#else
#if IS_ENABLED(CONFIG_WLBT_IS_BARAC_PRESENT)

	regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_CTRL1, BARAC_CTRL_VAL);
	regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_CTRL1, &val1);

	regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_BASE_ADDR1, WLBT_DBUS_BARAC_1_START >> BARAC_BIT_SHIFT);
	regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_BASE_ADDR1, &val2);

	regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_END_ADDR1, WLBT_DBUS_BARAC_1_END >> BARAC_BIT_SHIFT);
	regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_END_ADDR1, &val3);

	regmap_write(regmap, BARAC_D_WLBT_BA_WINDOW_REMAP_ADDR1, paddr >> BARAC_BIT_SHIFT);
	regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_REMAP_ADDR1, &val4);

	regmap_read(regmap, BARAC_D_WLBT_BA_WINDOW_STATUS1, &val5);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated DBUS_BARAC WINDOW_1(ctrl, start, end, remap, status):(0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4, val5);
#else
	regmap_write(regmap, MEMLOG_BAAW_WLBT_START, WLBT_DBUS_BAAW_1_START >> BAAW_BIT_SHIFT);
	regmap_write(regmap, MEMLOG_BAAW_WLBT_END, WLBT_DBUS_BAAW_1_END >> BAAW_BIT_SHIFT);
	regmap_write(regmap, MEMLOG_BAAW_WLBT_REMAP, paddr >> BAAW_BIT_SHIFT);
	regmap_write(regmap, MEMLOG_BAAW_WLBT_INIT_DONE, WLBT_BAAW_ACCESS_CTRL);

	regmap_read(regmap, BAAW1_D_WLBT_START, &val1);
	regmap_read(regmap, BAAW1_D_WLBT_END, &val2);
	regmap_read(regmap, BAAW1_D_WLBT_REMAP, &val3);
	regmap_read(regmap, BAAW1_D_WLBT_INIT_DONE, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated DBUS_BAAW_1(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);
#endif
#endif
#if IS_ENABLED(CONFIG_WLBT_EXYNOS_S2MPU)
	if (!platform->dbus_baaw1_allowed_list_set) {
#if IS_ENABLED(CONFIG_WLBT_IS_BARAC_PRESENT)
		ret = exynos_s2mpu_subsystem_set_allowlist(WLBT_SUBSYS_NAME, paddr,
							   WLBT_DBUS_BARAC_1_END - WLBT_DBUS_BARAC_1_START + 1);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "update harx BAAW1: 0x%lx, %ld %s(%d)\n", paddr,
				  WLBT_DBUS_BARAC_1_END - WLBT_DBUS_BARAC_1_START + 1,
				  ret == 0 ? "SUCCESS" : "FAIL", ret);
#else
		ret = exynos_s2mpu_subsystem_set_allowlist(WLBT_SUBSYS_NAME, paddr,
							   WLBT_DBUS_BAAW_1_END - WLBT_DBUS_BAAW_1_START + 1);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "update harx BAAW1: 0x%lx, %ld %s(%d)\n", paddr,
				  WLBT_DBUS_BAAW_1_END - WLBT_DBUS_BAAW_1_START + 1,
				  ret == 0 ? "SUCCESS" : "FAIL", ret);
#endif
		platform->dbus_baaw1_allowed_list_set = 1;
	}
#endif
}
#endif

void platform_wlbt_regdump(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long flags;
	int i;
	unsigned int val;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_wlbt_print_mailbox(platform);
	platform_wlbt_print_pmureg_register(platform);
	platform_wlbt_print_boot_cfg_register(platform);

	for (i = 0; i < NUM_MBOX_PLAT; i++) {
		val = platform_mif_reg_read(SCSC_MIF_ABS_TARGET_WLAN, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLAN MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
		val = platform_mif_reg_read(SCSC_MIF_ABS_TARGET_WPAN, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WPAN MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
#if defined(CONFIG_WLBT_PMU2AP_MBOX)
		val = platform_mif_reg_read(SCSC_MIF_ABS_TARGET_PMU, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PMU MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
#endif
	}
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

void platform_mif_init_regdump(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;
	interface->wlbt_regdump = platform_wlbt_regdump;
	interface->mif_dump_registers = platform_wlbt_regdump;
}


