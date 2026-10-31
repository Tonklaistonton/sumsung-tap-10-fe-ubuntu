/****************************************************************************
 *
 * Copyright (c) 2014 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/* Implements interface */

#include "platform_mif.h"

/* Interfaces it Uses */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/smc.h>
#include <soc/samsung/exynos-smc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <scsc/scsc_logring.h>
#include "mif_reg_S5E8845.h"
#include "platform_mif_module.h"
#include "miframman.h"
#include "platform_mif_s5e8845.h"

#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <soc/samsung/exynos/exynos-soc.h>
#else
#include <linux/soc/samsung/exynos-soc.h>
#endif
#endif

#ifdef CONFIG_SCSC_QOS
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/cal-if.h>
#endif

#if !defined(CONFIG_SOC_S5E8845)
#error Target processor CONFIG_SOC_S5E8845 not selected
#endif

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
/* Time to wait for CFG_REQ IRQ */
#define WLBT_BOOT_TIMEOUT (20 * HZ)

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/debug-snapshot.h>
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/debug-snapshot.h>
#else
#include <linux/debug-snapshot.h>
#endif
#endif

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
#include <linux/workqueue.h>
#endif

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/memlogger.h>
#else
#include <soc/samsung/memlogger.h>
#endif
#endif

#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
#include "pmu_cal.h"
#endif

#include <soc/samsung/exynos/exynos-hvc.h>

#ifdef CONFIG_WLBT_KUNIT
#include "./kunit/kunit_platform_mif_s5e8845.c"
#endif

static unsigned long sharedmem_base;
static size_t sharedmem_size;

static bool fw_compiled_in_kernel;
module_param(fw_compiled_in_kernel, bool, 0644);
MODULE_PARM_DESC(fw_compiled_in_kernel, "Use FW compiled in kernel");

#ifdef CONFIG_SCSC_CHV_SUPPORT
static bool chv_disable_irq;
module_param(chv_disable_irq, bool, 0644);
MODULE_PARM_DESC(chv_disable_irq, "Do not register for irq");
#endif

bool enable_hwbypass;
module_param(enable_hwbypass, bool, 0644);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables hwbypass");

static bool enable_platform_mif_arm_reset = true;
module_param(enable_platform_mif_arm_reset, bool, 0644);
MODULE_PARM_DESC(enable_platform_mif_arm_reset, "Enables WIFIBT ARM cores reset");

static bool disable_apm_setup = true;
module_param(disable_apm_setup, bool, 0644);
MODULE_PARM_DESC(disable_apm_setup, "Disable host APM setup");

static uint wlbt_status_timeout_value = 500;
module_param(wlbt_status_timeout_value, uint, 0644);
MODULE_PARM_DESC(wlbt_status_timeout_value, "wlbt_status_timeout(ms)");

static bool enable_scandump_wlbt_status_timeout;
module_param(enable_scandump_wlbt_status_timeout, bool, 0644);
MODULE_PARM_DESC(enable_scandump_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool force_to_wlbt_status_timeout;
module_param(force_to_wlbt_status_timeout, bool, 0644);
MODULE_PARM_DESC(force_to_wlbt_status_timeout, "wlbt_status_timeout(ms)");

static bool init_done;

#ifdef CONFIG_SCSC_QOS
/* 533Mhz / 1344Mhz / 2002Mhz for cpucl0 */
static uint qos_cpucl0_lv[] = {0, 0, 8, 15};
module_param_array(qos_cpucl0_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl0_lv, "S5E8845 DVFS Lv of CPUCL0 to apply Min/Med/Max PM QoS");

/* 533Mhz / 1444Mhz / 2400Mhz for cpucl1 */
static uint qos_cpucl1_lv[] = {0, 0, 9, 19};
module_param_array(qos_cpucl1_lv, uint, NULL, 0644);
MODULE_PARM_DESC(qos_cpucl1_lv, "S5E8845 DVFS Lv of CPUCL1 to apply Min/Med/Max PM QoS");
#endif /*CONFIG_SCSC_QOS*/

#include <linux/cpufreq.h>

static void power_supplies_on(struct platform_mif *platform);
inline void platform_int_debug(struct platform_mif *platform);

extern int mx140_log_dump(void);

#define platform_mif_from_mif_abs(MIF_ABS_PTR) container_of(MIF_ABS_PTR, struct platform_mif, interface)

inline void platform_mif_reg_write(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base + offset);
}

inline u32 platform_mif_reg_read(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base + offset);
}

inline void platform_mif_reg_write_wpan(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base_wpan + offset);
}

inline u32 platform_mif_reg_read_wpan(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base_wpan + offset);
}

inline void platform_mif_reg_write_pmu(struct platform_mif *platform, u16 offset, u32 value)
{
	writel(value, platform->base_pmu + offset);
}

inline u32 platform_mif_reg_read_pmu(struct platform_mif *platform, u16 offset)
{
	return readl(platform->base_pmu + offset);
}

#ifdef CONFIG_SCSC_QOS
static int platform_mif_set_affinity_cpu(struct scsc_mif_abs *interface, u8 cpu)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Change CPU affinity to %d\n", cpu);
	return irq_set_affinity_hint(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, cpumask_of(cpu));
}

int qos_get_param(char *buffer, const struct kernel_param *kp)
{
	/* To get cpu_policy of cl0 and cl1*/
	struct cpufreq_policy *cpucl0_policy = cpufreq_cpu_get(0);
	struct cpufreq_policy *cpucl1_policy = cpufreq_cpu_get(4);
	int count = 0;

	count += sprintf(buffer + count, "CPUCL0 QoS: %d, %d, %d\n",
			cpucl0_policy->freq_table[qos_cpucl0_lv[1]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[2]].frequency,
			cpucl0_policy->freq_table[qos_cpucl0_lv[3]].frequency);
	count += sprintf(buffer + count, "CPUCL1 QoS: %d, %d, %d\n",
			cpucl1_policy->freq_table[qos_cpucl1_lv[1]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[2]].frequency,
			cpucl1_policy->freq_table[qos_cpucl1_lv[3]].frequency);

	cpufreq_cpu_put(cpucl0_policy);
	cpufreq_cpu_put(cpucl1_policy);

	return count;
}

const struct kernel_param_ops domain_id_ops = {
	.set = NULL,
	.get = &qos_get_param,
};
module_param_cb(qos_info, &domain_id_ops, NULL, 0444);

static void platform_mif_verify_qos_table(struct platform_mif *platform)
{
	u32 index;
	u32 cl0_max_idx, cl1_max_idx;

	cl0_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl0_policy,
							platform->qos.cpucl0_policy->max);
	cl1_max_idx = cpufreq_frequency_table_get_index(platform->qos.cpucl1_policy,
							platform->qos.cpucl1_policy->max);

	for (index = SCSC_QOS_MIN; index <= SCSC_QOS_MAX; index++) {
		if (qos_cpucl0_lv[index] > cl0_max_idx)
			qos_cpucl0_lv[index] = cl0_max_idx;

		if (qos_cpucl1_lv[index] > cl1_max_idx)
			qos_cpucl1_lv[index] = cl1_max_idx;
	}
}

static int platform_mif_qos_init(struct platform_mif *platform)
{
	platform->qos.cpucl0_policy = cpufreq_cpu_get(0);
	platform->qos.cpucl1_policy = cpufreq_cpu_get(4);

	platform->qos_enabled = false;

	if ((!platform->qos.cpucl0_policy) || (!platform->qos.cpucl1_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS init error. CPU policy is not loaded\n");
		return -ENOENT;
	}

	/* verify pre-configured freq-levels of cpucl0/1 */
	platform_mif_verify_qos_table(platform);

	platform->qos_enabled = true;

	return 0;
}

static int platform_mif_pm_qos_add_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	qos_req->cpu_cluster0_policy = cpufreq_cpu_get(0);
	qos_req->cpu_cluster1_policy = cpufreq_cpu_get(4);

	if ((!qos_req->cpu_cluster0_policy) || (!qos_req->cpu_cluster1_policy)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"PM QoS add request error. CPU policy not loaded\n");
		return -ENOENT;
	}

	if (config == SCSC_QOS_DISABLED) {
		freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints,
						&qos_req->pm_qos_req_cl0, FREQ_QOS_MIN, 0);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints,
						&qos_req->pm_qos_req_cl1, FREQ_QOS_MIN, 0);
	} else {
		freq_qos_tracer_add_request(&qos_req->cpu_cluster0_policy->constraints,
										&qos_req->pm_qos_req_cl0, FREQ_QOS_MIN,
				platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency);
		freq_qos_tracer_add_request(&qos_req->cpu_cluster1_policy->constraints,
										&qos_req->pm_qos_req_cl1, FREQ_QOS_MIN,
				platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"PM QoS add request: %u. CL0 %u CL1 %u\n", config,
		platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency,
		platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);

	return 0;
}

static int platform_mif_pm_qos_update_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req, enum scsc_qos_config config)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	if (config == SCSC_QOS_DISABLED) {
		freq_qos_update_request(&qos_req->pm_qos_req_cl0, 0);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1, 0);
	} else {
		freq_qos_update_request(&qos_req->pm_qos_req_cl0,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency);
		freq_qos_update_request(&qos_req->pm_qos_req_cl1,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);
	}
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
			"PM QoS add request: %u. CL0 %u CL1 %u\n", config,
			platform->qos.cpucl0_policy->freq_table[qos_cpucl0_lv[config]].frequency,
			platform->qos.cpucl1_policy->freq_table[qos_cpucl1_lv[config]].frequency);

	return 0;
}

static int platform_mif_pm_qos_remove_request(struct scsc_mif_abs *interface, struct scsc_mifqos_request *qos_req)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (!platform)
		return -ENODEV;

	if (!platform->qos_enabled) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "PM QoS not configured\n");
		return -EOPNOTSUPP;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
						"PM QoS remove request\n");

	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl0);
	freq_qos_tracer_remove_request(&qos_req->pm_qos_req_cl1);

	return 0;
}
#endif /*CONFIG_SCSC_QOS*/

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
static void platform_recovery_disabled_reg(struct scsc_mif_abs *interface, bool (*handler)(void))
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif recovery %pS\n", handler);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = handler;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_recovery_disabled_unreg(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Unregistering mif recovery\n");
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->recovery_disabled = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}
#endif

static void platform_mif_irq_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL, "INT handler not registered\n");
}

static void platform_mif_irq_reset_request_default_handler(int irq, void *data)
{
	/* Avoid unused parameter error */
	(void)irq;
	(void)data;

	/* int handler not registered */
	SCSC_TAG_INFO_DEV(PLAT_MIF, NULL,
			  "INT reset_request handler not registered\n");
}

irqreturn_t platform_mif_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->wlan_handler);
	if (platform->wlan_handler != platform_mif_irq_default_handler) {
		platform->wlan_handler(irq, platform->irq_dev);
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "MIF Interrupt Handler not registered.\n");
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), (0xffff << 16));
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), (0xffff << 16));
	}

	return IRQ_HANDLED;
}

irqreturn_t platform_mif_isr_wpan(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "INT %pS\n", platform->wpan_handler);
	if (platform->wpan_handler != platform_mif_irq_default_handler) {
		platform->wpan_handler(irq, platform->irq_dev_wpan);
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "MIF Interrupt Handler not registered\n");
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), (0xffff << 16));
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), (0xffff << 16));
	}

	return IRQ_HANDLED;
}

irqreturn_t platform_wdog_isr(int irq, void *data)
{
	int ret = 0;
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INT received %d\n", irq);
	platform_int_debug(platform);

	if (platform->reset_request_handler != platform_mif_irq_reset_request_default_handler) {
		if (platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
			/* Spurious interrupt from the SOC during
			 *  CFG_REQ phase, just consume it
			 */
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "Spurious wdog irq during cfg_req phase\n");
			return IRQ_HANDLED;
		}
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		platform->reset_request_handler(irq, platform->irq_reset_request_dev);
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "WDOG Interrupt reset_request_handler not registered\n");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Disabling unhandled WDOG IRQ.\n");
		disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_inc(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt);
	}

	/* The wakeup source isn't cleared until WLBT is reset, so change the
	 *  interrupt type to suppress this
	 */
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	if (platform->recovery_disabled && platform->recovery_disabled()) {
#else
	if (mxman_recovery_disabled()) {
#endif
		ret = regmap_update_bits(platform->pmureg, WAKEUP_INT_TYPE,
					 RESETREQ_WLBT, 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Set RESETREQ_WLBT wakeup interrput type to EDGE.\n");
		if (ret < 0)
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
					 "Failed to Set WAKEUP_INT_IN[RESETREQ_WLBT]: %d\n", ret);
	}

	return IRQ_HANDLED;
}

irqreturn_t platform_mbox_pmu_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "INT received, boot_state = %u\n", platform->boot_state);
	if (platform->boot_state == WLBT_BOOT_CFG_DONE) {
		if (platform->pmu_handler != platform_mif_irq_default_handler)
			platform->pmu_handler(irq, platform->irq_dev_pmu);
		else
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					"MIF PMU Int Handler not registered\n");
	} else {
		/* platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ */
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Spurious mbox pmu irq during cfg_req phase\n");
	}
	return IRQ_HANDLED;
}

static void platform_wlbt_regdump(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	u32 val = 0;

	regmap_read(platform->pmureg, WLBT_STAT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STAT 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_DEBUG 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CONFIGURATION 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATUS 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_STATES 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_OPTION, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_OPTION 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CTRL_NS, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CTRL_NS 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_CTRL_S, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_CTRL_S 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_OUT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_OUT 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_IN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_IN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_IN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_IN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_EN, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_EN 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_TYPE, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_TYPE 0x%x\n", val);

	regmap_read(platform->pmureg, WLBT_INT_DIR, &val);
	SCSC_TAG_INFO(PLAT_MIF, "WLBT_INT_DIR 0x%x\n", val);

	regmap_read(platform->pmureg, SYSTEM_OUT, &val);
	SCSC_TAG_INFO(PLAT_MIF, "SYSTEM_OUT 0x%x\n", val);

	regmap_read(platform->pmureg, VGPIO_TX_MONITOR2, &val);
	SCSC_TAG_INFO(PLAT_MIF, "VGPIO_TX_MONITOR2 0x%x\n", val);
}

/*
 * Attached array contains the replacement PMU boot code which should
 * be programmed using the CBUS during the config phase.
 */
uint32_t ka_patch[] = {
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x60008000,
0x60000265,
0x60000295,
0x60000299,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x6000029d,
0x00000000,
0x00000000,
0x600002a1,
0x600002a5,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x8637bd05,
0x00000000,
0x00000001,
0x60000100,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x47804802,
0xf9ccf001,
0x4770e010,
0x60000275,
0x46012000,
0x46034602,
0x46054604,
0x46074606,
0x46814680,
0x46834682,
0x47704684,
0xe7fdbf30,
0x46c0e7fc,
0x46c0e7fa,
0x46c0e7f8,
0x46c0e7f6,
0x46c0e7f4,
0x4a0423be,
0x58d1005b,
0x18083101,
0x1a0958d1,
0x4770d4fc,
0x50000000,
0x28003801,
0x4770d1fc,
0x4c1ab570,
0xd1032801,
0x4d196823,
0xe0026866,
0x4d186863,
0x22006826,
0x605a601a,
0x60da609a,
0x4291611a,
0x751ad001,
0x2101e001,
0x619a7519,
0x2a006932,
0x2101d003,
0x430a699a,
0x78aa619a,
0xd0032a00,
0x699a2102,
0x619a430a,
0x699a2108,
0x619a430a,
0xd1062800,
0x34392201,
0x699a7022,
0x430a1849,
0x4a04619a,
0xbd7061da,
0x60000210,
0x6000022c,
0x60000218,
0xfafafafa,
0x210522b4,
0x4c36b570,
0x58a30092,
0x430b0005,
0x23b550a3,
0x58e0009b,
0xd1fc2803,
0x00220021,
0x32b431dc,
0x2d00680b,
0x4318d007,
0x21206008,
0x20026813,
0x6013430b,
0x4383e008,
0x43032001,
0x2120600b,
0x18006813,
0x6013430b,
0xff96f7ff,
0x21100022,
0x681332b4,
0x6013430b,
0x33c80023,
0xd0042d00,
0x3131681a,
0x430a31ff,
0x6819e002,
0x400a4a1c,
0x0022601a,
0x200021a0,
0x681332c8,
0x430b2501,
0x4b186013,
0x77982602,
0x33b40023,
0x3998681a,
0x601a430a,
0x3904681a,
0x601a430a,
0x432a681a,
0x681a601a,
0x601a4332,
0x009222b4,
0x360358a3,
0x50a343b3,
0x328c0022,
0x438b6813,
0x60130001,
0xff5ef7ff,
0x212023c0,
0x005b4a07,
0x221050d1,
0x00286d63,
0x65634393,
0x46c0bd70,
0x50000000,
0xfffffebf,
0x60000210,
0xe000e000,
0x2b026883,
0x390bd101,
0x2b04e002,
0x390cd103,
0x41484248,
0x2b08e02d,
0x390ed101,
0x2b10e7f8,
0x390fd101,
0x2b20e7f4,
0x390ad101,
0x2b40e01f,
0x3910d101,
0x2b80e01b,
0x3911d101,
0x2280e017,
0x42930052,
0x3912d101,
0x2280e011,
0x42930092,
0x3913d101,
0x2280e00b,
0x429300d2,
0x3914d101,
0x2280e005,
0x01122000,
0xd1024293,
0x4248391b,
0x47704148,
0x21012211,
0x675a4b04,
0x675a3206,
0x675a320e,
0x430a6bda,
0x477063da,
0x50000000,
0x2300b510,
0x49052240,
0xf0012004,
0x23c0fa11,
0x4a032110,
0x50d1005b,
0x46c0bd10,
0x600014c9,
0xe000e000,
0x2300b510,
0x49042240,
0xf0012006,
0x2240f9ff,
0x33fc4b02,
0xbd10605a,
0x600007f9,
0xe000e000,
0x2501b570,
0x6acb4933,
0x000c6b0a,
0x089b4053,
0x422b2201,
0x2304d1f7,
0xd01b2800,
0x421a6aca,
0x6acad156,
0x62ca431a,
0x421a6b22,
0x2023d0fc,
0xfeb6f7ff,
0x21020023,
0x6fda3308,
0x438a34fc,
0x6fda67da,
0x430a1849,
0x6ee267da,
0x40134b22,
0xe03d66e3,
0x42196ac9,
0x0023d03a,
0x6a1933fc,
0x62194311,
0x42116a59,
0x2304d0fc,
0x439a6ae2,
0x6b2262e2,
0xd1fc421a,
0x7a9b4b18,
0xd11d2b00,
0x21800023,
0x6a9a33fc,
0x430a0149,
0x629a21e0,
0x01c96eda,
0x2104430a,
0x340866da,
0x438a6fe2,
0x6fe267e2,
0x430a3902,
0x6a1a67e2,
0x438a3901,
0x6a1a621a,
0x438a3107,
0xe009621a,
0x22080023,
0x6a1933fc,
0x62194311,
0x42116a59,
0xe7d7d0fc,
0x46c0bd70,
0x50000000,
0xffff8fff,
0x60000210,
0x4c19b510,
0x2b007aa3,
0x7ae3d003,
0x2b002001,
0x2000d105,
0x42837fa3,
0x7fe0d001,
0x2301b2c0,
0xf7ff4018,
0x7aa3ff7d,
0xd0072b00,
0x2b007ae3,
0x2108d004,
0x69936822,
0xe003430b,
0x68222108,
0x438b6993,
0x7fa36193,
0xd0072b00,
0x2b007fe3,
0x2108d004,
0x69936862,
0xe003430b,
0x68622108,
0x438b6993,
0xbd106193,
0x60000210,
0x4e3db5f8,
0x33370033,
0xb2db781b,
0xd1722b01,
0x4d3a0032,
0x002c2080,
0x78113238,
0xb2c90440,
0x34944a37,
0xd1252901,
0x43196823,
0x68236021,
0x43034934,
0x002b6023,
0x68183398,
0x20e04001,
0x68196019,
0x43010380,
0x68216019,
0x400b4b2e,
0x010921c8,
0x0029430b,
0x319c6023,
0x4b2b6808,
0x20c84003,
0x430300c0,
0x002b600b,
0x33a82100,
0xe00b6019,
0x430b6821,
0x00036023,
0x430b6821,
0x60232180,
0x04896823,
0x6023430b,
0x68232180,
0x430b0309,
0x20300029,
0x31a46023,
0x2701680b,
0x600b4303,
0x38126823,
0x6022401a,
0xfdd2f7ff,
0x68232280,
0x431302d2,
0x200a6023,
0xfdcaf7ff,
0x3208002a,
0x002c6fd3,
0x67d343bb,
0x3488220c,
0x00386823,
0x3a044393,
0x60234313,
0xfdbaf7ff,
0x68232298,
0x43130052,
0x6c2b6023,
0x43bb3637,
0x2300642b,
0xbdf87033,
0x60000210,
0x50000000,
0xfffbffff,
0xfe3fffff,
0xfffe007f,
0xfc00003f,
0x4a0623a6,
0x58d1009b,
0xd1022801,
0x50d04308,
0x2004e002,
0x50d14301,
0x46c04770,
0x50000000,
0x2704b5f7,
0x698b9301,
0x000c0005,
0x423b0016,
0x2001d12d,
0xfeb6f7ff,
0x69a32208,
0x43130028,
0x69a361a3,
0x61a7431f,
0xffdaf7ff,
0x789b9b01,
0xd0142b00,
0x69b32201,
0x61b34013,
0x69b3d10f,
0x61b34313,
0x791b9b01,
0xd0082b00,
0x41684268,
0x320769b3,
0x61b34313,
0xf7ffb2c0,
0x69a3ffc1,
0xd404065b,
0x4a022101,
0x430b6bd3,
0xbdf763d3,
0x50000000,
0x4b0ab510,
0x07526d9a,
0x2140d50e,
0x32fc4a08,
0x6d9a6051,
0x430a393f,
0x2202659a,
0x42116d99,
0x2001d0fc,
0xfd92f7ff,
0x46c0bd10,
0x50000000,
0xe000e000,
0x49094b08,
0x4a097a98,
0xd1062800,
0x28007f98,
0x3339d005,
0x2b00781b,
0x2301d001,
0x2300e000,
0x4770548b,
0x60000210,
0x50000000,
0x0000029d,
0x2701b5f8,
0xfef8f7ff,
0x77af4d2c,
0xfebef7ff,
0xffdef7ff,
0x20044c2a,
0x00260022,
0x6813328c,
0x430336b4,
0x68336013,
0x603343bb,
0xfd1af7ff,
0x68332202,
0x43932008,
0xf7ff6033,
0x7f2bfd13,
0x19db21b0,
0x772bb2db,
0x33c80023,
0x438a681a,
0x7f2a601a,
0xd90342ba,
0x4a1a6819,
0x601a400a,
0x22040025,
0x682b35b4,
0x43932102,
0x682b602b,
0x43931892,
0x602b0022,
0x6813328c,
0x430b2620,
0x39012000,
0xf7ff6013,
0x2210fcf1,
0x20014b0e,
0x605e33fc,
0x34dc682b,
0x602b4393,
0xfcd6f7ff,
0x2001682b,
0x602b43b3,
0xf7ff2501,
0x6823fccf,
0x602343ab,
0xfdeaf7ff,
0xbdf80028,
0x60000210,
0x50000000,
0xfffffebf,
0xe000e000,
0x6e194b02,
0x42916e1a,
0x4770d0fc,
0x50000000,
0x4b38b5f7,
0x681f685d,
0x001c7d2a,
0xd1562a01,
0x2a03682a,
0x2a05d03e,
0x2a01d045,
0x7f9bd14c,
0xd0492b00,
0x6263686b,
0x42136a63,
0x23bed02c,
0x005b4e2d,
0x68ab58f1,
0x692362a3,
0xd0054213,
0x6aa36962,
0x1a5b1a52,
0xd91d429a,
0x21006e33,
0x4b269301,
0x681a6aa0,
0xf001685b,
0x0909f8a5,
0x6ae362e1,
0x429a9a01,
0x6eb3d20e,
0xd1072b00,
0x20236ae3,
0x23016673,
0xf7ff66b3,
0xe003fc79,
0xffb6f7ff,
0x66736ae3,
0x68ea2301,
0x4a154013,
0xe01177d3,
0x3a01686a,
0x414a4251,
0x77dab2d2,
0xfe06f7ff,
0x686ae008,
0x3a013339,
0x414a4251,
0x701ab2d2,
0xff1ef7ff,
0x602b2300,
0x692b752b,
0xd0052b00,
0x003a4b0a,
0x20000029,
0xfebef7ff,
0x49052010,
0x6d4b6d4a,
0x009b4382,
0x43134003,
0xbdf7654b,
0x60000210,
0x50000000,
0x60000200,
0x60000218,
0x4b042216,
0x43916ad9,
0x6b1962d9,
0xd1fc4211,
0x46c04770,
0x50000000,
0x4c1db510,
0x00232008,
0x681a33b0,
0x601a4302,
0x32b40022,
0x43016811,
0x68116011,
0x43013804,
0x681a6011,
0x20004302,
0xf7ff601a,
0x2200fd4b,
0x54e24b12,
0xffd8f7ff,
0x21800022,
0x68133294,
0x430b02c9,
0x4b0e6013,
0x001a2101,
0x70113237,
0x6e216e22,
0xd0fc428a,
0x009222b0,
0x2a0258a2,
0x6c22d102,
0xe0012010,
0x6c222008,
0x64224382,
0x64992201,
0xbd1064da,
0x50000000,
0x0000029d,
0x60000210,
0xb085b5f0,
0x4e42b672,
0x00347ab3,
0xd10d2b00,
0x2b007fb3,
0x0033d10a,
0x781a3337,
0xd00c2a01,
0x2b01781b,
0xf7ffd009,
0xe006ffa9,
0x33370023,
0x2b01781b,
0xf7ffd101,
0x3437fda9,
0x4c357823,
0x2b01b2db,
0x21a7d106,
0x00892209,
0x6c225462,
0x64234313,
0x22a7bf30,
0x0092230b,
0x54a321a8,
0x0089220f,
0x40135863,
0xd9fb2b02,
0x2b006cf3,
0x6e23d043,
0x42836e20,
0x23b0d0fc,
0x58e3009b,
0xd1022b02,
0x22106c23,
0x2208e001,
0x43136c23,
0x6423253d,
0x6cb36cb3,
0x23011ac0,
0x402b4345,
0x086b9301,
0x23c49302,
0x005b2500,
0x42ab58e3,
0x23b0d112,
0x58e3009b,
0xd1052b02,
0x33ff3383,
0x238058e5,
0xe004061b,
0x005b23c0,
0x3b6858e5,
0x42ab3bff,
0x426d41ad,
0x37fc0027,
0x213b6ffb,
0xf0009303,
0x9b03ff0d,
0x9b0118c0,
0x9b0218c0,
0x230018c0,
0x67fd1945,
0x221064f3,
0x43136ae3,
0x221062e3,
0x42136ae3,
0xb662d0fb,
0x46c0e77a,
0x60000210,
0x50000000,
0x33fc4b04,
0x43026a1a,
0x6a5a621a,
0x42904002,
0x4770d1fb,
0x50000000,
0x4c1fb510,
0x07db6b63,
0x2304d539,
0x00216ae2,
0x425a4013,
0x2307415a,
0x1a9b31f8,
0x431a680a,
0x0021600a,
0x680a31fc,
0x4293401a,
0x0021d1fb,
0x680a31f8,
0x600b4053,
0x6ae02304,
0x42584003,
0x20074143,
0xf7ff1ac0,
0x0023ffcf,
0x33fc2180,
0x00c96eda,
0x2101430a,
0x6f1866da,
0x42082201,
0x6ed8d1fb,
0x40014907,
0x6ba366d9,
0x63a34313,
0x42136b63,
0x2201d1fc,
0x43936ba3,
0xbd1063a3,
0x50000000,
0xfffffbff,
0x33fc4b04,
0x43826a1a,
0x6a5a621a,
0xd1fc4202,
0x46c04770,
0x50000000,
0x4d7cb5f7,
0x002b2080,
0x33fc2601,
0x01406a99,
0x62994301,
0x43026ada,
0x62da2001,
0xfaf8f7ff,
0x72a64c75,
0xf7ff77a6,
0xf7fffcc9,
0x2010fc91,
0xffdaf7ff,
0x328c002a,
0x95006813,
0x60134333,
0x6aea2302,
0x62ea431a,
0x6b129a00,
0x9201401a,
0xd1f92a02,
0x20232704,
0xfadaf7ff,
0x35b09d00,
0x43bb682b,
0xf7ff602b,
0x9a00fd95,
0x328c9e00,
0x36b46813,
0x6013433b,
0x682b2201,
0x43930038,
0x6833602b,
0x60334393,
0xfacef7ff,
0x9a01682b,
0x43932008,
0x6833602b,
0x60334393,
0xfac4f7ff,
0x7a2321b0,
0xb2db3301,
0x7f237223,
0xb2db3301,
0x9b007723,
0x681a33c8,
0x601a438a,
0x2a017f22,
0x6819d903,
0x400a4a4d,
0x2208601a,
0x27044b4c,
0x9c00681b,
0x4a4a4393,
0x601334b4,
0x68232208,
0x43bb9d00,
0x68236023,
0x43933590,
0x682b6023,
0x43933233,
0x43133a20,
0x4b41602b,
0x681b3a0b,
0x43939e00,
0x36c04a3e,
0x22016013,
0x00386833,
0x60334393,
0xfa88f7ff,
0x68332202,
0x43930038,
0xf7ff6033,
0x2105fa81,
0x9a006833,
0x603343bb,
0x681332c8,
0x438b0038,
0xf7ff6013,
0x2002fa75,
0xfa72f7ff,
0x682b2102,
0x431f9a00,
0x328c602f,
0x27206813,
0x3901430b,
0x60130008,
0xfa68f7ff,
0x20002101,
0xfa64f7ff,
0x682b2201,
0x43932002,
0xf7ff602b,
0x4b22fa59,
0x681b4a21,
0x43bb2002,
0x22086013,
0x43936833,
0x60339a00,
0x681132cc,
0x400b4b1c,
0xf7ff6013,
0x2208fa47,
0x20014b1a,
0x605a33fc,
0x9b00605f,
0x33fc4a18,
0x40116a99,
0x6ad96299,
0x62da400a,
0xfa2af7ff,
0x21012201,
0x4313682b,
0x602b9a00,
0x681332d8,
0x6013438b,
0x68232210,
0x60234393,
0x43bb6823,
0x9b006023,
0x681a33dc,
0x601a438a,
0xfb20f7ff,
0xfb30f7ff,
0xbdfe2001,
0x50000000,
0x60000210,
0xfffffebf,
0x500000b0,
0xfffffaaa,
0xe000e000,
0xffffefff,
0x4c58b5f7,
0x00232080,
0x33fc2501,
0x01406a99,
0x62994301,
0x43026ada,
0x62da2001,
0xf9f0f7ff,
0xfbc4f7ff,
0x729d4b50,
0xfb8af7ff,
0xf7ff2010,
0x0022fed3,
0x6813328c,
0x432b9400,
0x23026013,
0x431a6ae2,
0x9a0062e2,
0x401a6b12,
0x2a029201,
0x2704d1f9,
0xf7ff2023,
0x9c00f9d3,
0x682334b0,
0x602343bb,
0xfc8ef7ff,
0x68232201,
0x43930038,
0xf7ff6023,
0x6823f9d1,
0x20089a01,
0x60234393,
0xf9caf7ff,
0x9d004a38,
0x35907a13,
0xb2db3301,
0x22087213,
0x9e006823,
0x60234393,
0x3273682b,
0x3a204393,
0x602b4313,
0x3a4b6823,
0x36c04393,
0x68336023,
0x43933a0f,
0x60330038,
0xf9acf7ff,
0x9a016833,
0x43930038,
0xf7ff6033,
0x2105f9a5,
0x9a006833,
0x32c843bb,
0x68136033,
0x438b0038,
0xf7ff6013,
0x9801f999,
0xf996f7ff,
0x682b2101,
0x431f0008,
0xf7ff602f,
0x2208f993,
0x4b1b2741,
0x33fc9801,
0x682b605a,
0x602b43bb,
0xf984f7ff,
0x68232220,
0x43939801,
0x68336023,
0x43933a18,
0x60339a00,
0x681132cc,
0x400b4b11,
0xf7ff6013,
0x9b00f973,
0x33fc4a0f,
0x20016a99,
0x62994011,
0x400a6ad9,
0xf7ff62da,
0x2101f95b,
0x431f682b,
0x602f9b00,
0x681a33d8,
0x601a438a,
0xfa5ef7ff,
0xbdfe2001,
0x50000000,
0x60000210,
0xe000e000,
0xfffffaaa,
0xffffefff,
0x2601b5f0,
0x4c3b4d3a,
0xb0856e6b,
0x69239303,
0x40332023,
0x6a639302,
0x93014033,
0x66ab2300,
0xf932f7ff,
0x69236e2f,
0xd0122b00,
0x1afb69a3,
0x6923d403,
0x93024033,
0x6a63e00b,
0xd0082b00,
0x1afb6ae3,
0x69a3d405,
0x666b2023,
0xf7ff66ae,
0x4e28f91b,
0x2b006a73,
0x6af3d014,
0xd4041afb,
0x6a722301,
0x9201401a,
0x6933e00c,
0xd0092b00,
0x1aff69b3,
0x69b3d406,
0x666b2023,
0x66ab2301,
0xf902f7ff,
0x2b009b02,
0x9b01d020,
0xd0172b00,
0x61232300,
0x7aa36263,
0xd1052b00,
0x2b007fa3,
0xf7ffd102,
0xe018fde9,
0x2b007ab3,
0xf7ffd102,
0xe012feeb,
0x2b007fb3,
0xf7ffd10f,
0xe00cfbbf,
0x61239b01,
0x2b007aa3,
0xe7f0d107,
0x2b009b01,
0x9b02d003,
0x7fa36263,
0x6e2be7ed,
0x429a9a03,
0x2201d0fb,
0x43136deb,
0xb00565eb,
0x46c0bdf0,
0x50000000,
0x60000210,
0xb5704b10,
0x8405f3ef,
0x685a681a,
0x4014223f,
0x3c107a9a,
0x2a00001d,
0x0021d107,
0xf7ff480a,
0x2800f97d,
0xf7ffd001,
0x7fabfeb3,
0xd1072b00,
0x48060021,
0xf972f7ff,
0xd0012800,
0xfb82f7ff,
0x46c0bd70,
0x60000210,
0x60000218,
0x6000022c,
0xf7ffb510,
0xbd10ffd5,
0xf7ffb510,
0xbd10ffd1,
0xf7ffb510,
0xbd10ffcd,
0xf7ffb510,
0xbd10ffc9,
0xf7ffb510,
0xbd10ffc5,
0xf7ffb510,
0xbd10ffc1,
0xf7ffb510,
0xbd10ffbd,
0xf7ffb510,
0xbd10ffb9,
0xf7ffb510,
0xbd10ffb5,
0xf7ffb510,
0xbd10ffb1,
0xf7ffb510,
0xbd10ffad,
0x1e06b5f8,
0xd0054c5a,
0x23200021,
0x680a31c4,
0xd0fc421a,
0x230422b2,
0x00926ae1,
0xd0054219,
0x58a32111,
0x50a3430b,
0xe0032305,
0x58a12010,
0x50a14301,
0x008921b3,
0x401a5862,
0xd1fb429a,
0x6ae223b2,
0x0752009b,
0x2144d505,
0x430a58e2,
0x220a50e2,
0x2140e004,
0x430a58e2,
0x220850e2,
0x008921b3,
0x40135863,
0xd1fb429a,
0x27010025,
0x35d80022,
0x3290682b,
0x602b433b,
0x20106813,
0x601343bb,
0xfcccf7ff,
0x20800023,
0x6a9933fc,
0x43010140,
0x6ada6299,
0x00384302,
0xf7ff62da,
0x0023f823,
0x681a33cc,
0x37074932,
0x601a430a,
0x32d00022,
0x00206813,
0x6013430b,
0x32c00022,
0x21206813,
0x6013433b,
0x32b00022,
0x30ec6813,
0x6013430b,
0x682b0022,
0x32c83916,
0x2e00438b,
0x433bd008,
0x23e0602b,
0x600302db,
0x43196813,
0xe0046011,
0x6006602b,
0x438b6813,
0x00226013,
0x32c82105,
0x27046813,
0x6013430b,
0x26030023,
0x681a33c0,
0x433a2010,
0x681a601a,
0x43322508,
0x0023601a,
0x681a33b0,
0x601a4302,
0x432a681a,
0x6819601a,
0x60194339,
0x2155681a,
0x601a4332,
0x009222b2,
0x438b58a3,
0xf7ff50a3,
0x2100fcb9,
0x20014b09,
0xf7fe7299,
0x23c0ffd7,
0x005b4a07,
0x222050d5,
0x20016d63,
0x65634393,
0x46c0bdf8,
0x50000000,
0x00000555,
0x60000210,
0xe000e000,
0x4b59b5f7,
0x20004c59,
0xb29a58e3,
0x07db9201,
0xe097d400,
0x58e54b56,
0x280e1f68,
0xe090d900,
0xfb28f000,
0x8f15080f,
0x45412d37,
0x8f584b52,
0x00775e69,
0x4b4f2000,
0x42837a9b,
0xe081d000,
0x2000e013,
0x7f9b4b4b,
0xd17b4283,
0x4b49e014,
0x001a7a99,
0xd1052900,
0x2b007f9b,
0xf7ffd102,
0xe06ffc7b,
0x2b007a93,
0xf7ffd102,
0xe069fd7d,
0x20017f93,
0xd1652b00,
0xfa50f7ff,
0x2000e062,
0x7ab34e3c,
0xd05d4283,
0xfefaf7ff,
0x72f32301,
0x2000e058,
0x7fb34e37,
0xd0534283,
0xffb2f7fe,
0x77f32301,
0x2006e04e,
0xfbfaf7ff,
0x2200e02f,
0x731a4b30,
0x77da3301,
0x2001e029,
0x4b2d2200,
0x181b7318,
0xe03d77da,
0x4b2a2200,
0x731a2001,
0xe0033301,
0x4b272001,
0x181b7318,
0xe03177d8,
0x48242140,
0x69936802,
0x6193430b,
0x4b226801,
0x20016842,
0x2140e009,
0x6842481e,
0x430b6993,
0x68416193,
0x20006802,
0xf7ff4b1c,
0x2001f99b,
0x2040e018,
0x680b4917,
0x4302699a,
0x684a619a,
0x43036993,
0x680f6193,
0x4b13684e,
0x00390032,
0xf7ff383f,
0x4b11f987,
0x0031003a,
0xf7ff2000,
0x2000f981,
0x9a014b0e,
0x230850e2,
0xd1032800,
0x401d3b05,
0x432b3301,
0x50a34a05,
0x4b092280,
0x50e20252,
0x46c0bdf7,
0x00080028,
0x58000000,
0x00080100,
0x60000210,
0x6000022c,
0x60000218,
0x00080020,
0x00080008,
0x211023c0,
0x4a03b510,
0x2001005b,
0xf7ff50d1,
0xbd10fe73,
0xe000e000,
0x4a40b5f7,
0x68152700,
0x00146853,
0x7d2b9301,
0xd15f2b01,
0x4e3c682b,
0xd03d2b03,
0xd04b2b0c,
0xd1542b01,
0x42ba7a92,
0x2700d101,
0x686ae04f,
0x69226122,
0xd029421a,
0x68aa21be,
0x61620049,
0x58716a62,
0xd005421a,
0x69636aa2,
0x1a5b1a52,
0xd91b429a,
0x6e374b2d,
0x69602100,
0x685b681a,
0xfacef000,
0x61a10909,
0x429f69a3,
0x6eb3d20e,
0xd1072b00,
0x202369a3,
0x23016673,
0xf7fe66b3,
0xe003fea3,
0xf9e0f7ff,
0x667369a3,
0x68eb2701,
0x403b4a1c,
0xe01a72d3,
0x1e63686c,
0x414b4259,
0x72d3b2db,
0xf830f7ff,
0xd1be2c01,
0x6ab236fc,
0x40134b17,
0xe00a62b3,
0x68692080,
0x4b164a15,
0x50d10240,
0x58534915,
0x4303b29b,
0x23005053,
0x752b602b,
0x2b00692b,
0x4b11d005,
0x00299a01,
0xf7ff2001,
0x2020f8df,
0x6d4a4907,
0x43826d4b,
0x4003009b,
0x654b4313,
0xd0022f00,
0xf7ff381f,
0xbdf7fdef,
0x60000210,
0x50000000,
0x60000200,
0xffffefff,
0x58000000,
0x00080100,
0x00080008,
0x6000022c,
0xb672b5f7,
0x260122a7,
0x4b80210b,
0x00924d80,
0x54a9721e,
0x68da4c7f,
0x50e24b7f,
0x8f4ff3bf,
0x21004f7e,
0x487e003a,
0xfa84f000,
0x4a7e4b7d,
0x801a80df,
0x4f7e4a7d,
0x4a7e805a,
0x809a497e,
0x009b23c2,
0x23009300,
0x0018220e,
0xf97ef000,
0x220a2300,
0x00304979,
0xf0009700,
0x2398f977,
0x9300015b,
0x2300221a,
0x20024975,
0xf96ef000,
0x221a2300,
0x20034973,
0xf0009700,
0x0030f967,
0xf91ef000,
0x011b23d9,
0x4c6f50e6,
0x21002250,
0xf0000020,
0x4b6dfa4f,
0x4b6d6023,
0x23006063,
0x77237223,
0x72e619a3,
0x732677e6,
0x002377de,
0x701e3339,
0x009b23b0,
0x3b0258eb,
0x4153425a,
0xb2db0022,
0x70133238,
0x005b23c4,
0x781350ee,
0xd10842b3,
0x4b5f2208,
0x6c2b646b,
0x642b4393,
0x18926c2b,
0x2210e005,
0x43936c2b,
0x6c2b642b,
0x43133a08,
0x2380642b,
0x6423021b,
0x2080002b,
0x6a9933fc,
0x43010140,
0x6ada6299,
0x43024952,
0x200d62da,
0x22402300,
0xf8eaf000,
0x22402300,
0x2019494e,
0xf8e4f000,
0x22402300,
0x494c0018,
0xf8def000,
0x230127c0,
0x494a2240,
0xf0002003,
0x2308f8d7,
0x007f4e33,
0x224051f3,
0x49463b07,
0xf0002005,
0x2320f8cd,
0x51f32240,
0x23004943,
0xf0002001,
0x23a0f8c5,
0x009b2202,
0x494050f2,
0x323e2300,
0xf000200a,
0x2300f8bb,
0x493d2240,
0xf000200b,
0x2300f8b5,
0x493b2240,
0xf000200c,
0x2300f8af,
0x49392240,
0xf000200e,
0x2300f8a9,
0x49372240,
0xf000200f,
0x2300f8a3,
0x49352240,
0xf0002010,
0x2300f89d,
0x49332240,
0xf0002011,
0x2300f897,
0x49312240,
0xf0002012,
0x2300f891,
0x492f2240,
0xf0002013,
0x2300f88b,
0x492d2240,
0xf0002014,
0x2300f885,
0x492b2240,
0xf000201b,
0x2201f87f,
0x34376c2b,
0x642b4393,
0x42937823,
0xf7ffd001,
0xf7fff91d,
0x46c0f95d,
0x60000200,
0x50000000,
0xe000e000,
0x00000d08,
0x00000700,
0x60002100,
0x60000100,
0x00002000,
0x0000000f,
0x00001305,
0x00002100,
0x60000000,
0x60002000,
0x50000000,
0x58000000,
0x60000210,
0x60002640,
0x60002660,
0x03555555,
0x60001341,
0x60000bf9,
0x600004a9,
0x600014e1,
0x60000939,
0x60001021,
0x60001189,
0x60001191,
0x60001199,
0x600011a1,
0x600011a9,
0x600011b1,
0x600011b9,
0x60001171,
0x60001179,
0x60001181,
0x60001169,
0xb5100003,
0x4c053310,
0x5119009b,
0x18184b04,
0x00db2380,
0x700218c0,
0x46c0bd10,
0x60000100,
0xe000e000,
0x0004b513,
0xf0002004,
0x2c00f85d,
0x4b05d101,
0x4b05e000,
0x22079300,
0x49042300,
0xf0002004,
0xbd13f835,
0x00001308,
0x00001008,
0x60000100,
0x0004b5f8,
0x000e2000,
0x001d0017,
0xffe0f7ff,
0x4a100023,
0x009b3310,
0x2280509e,
0x00d24e0e,
0x189b1933,
0x701f2001,
0xffd2f7ff,
0x09630032,
0xd0072d00,
0x2001211f,
0x40884021,
0x31a00019,
0x51880089,
0x400c211f,
0x40a1391e,
0x009b3340,
0xbdf850d1,
0x60000100,
0xe000e000,
0x2410b510,
0x43084321,
0x49094c08,
0xf3bf5060,
0x20018f4f,
0x99024082,
0x0409021b,
0x430b4301,
0x4b04431a,
0xf3bf50e2,
0xbd108f6f,
0xe000e000,
0x00000d98,
0x00000d9c,
0x4a074b06,
0x50984907,
0x505a2200,
0x8f4ff3bf,
0x505a4905,
0x8f6ff3bf,
0x46c04770,
0xe000e000,
0x00000d94,
0x00000d98,
0x00000d9c,
0x4671b402,
0x00490849,
0x00495c09,
0xbc02448e,
0x46c04770,
0x08432200,
0xd374428b,
0x428b0903,
0x0a03d35f,
0xd344428b,
0x428b0b03,
0x0c03d328,
0xd30d428b,
0x020922ff,
0x0c03ba12,
0xd302428b,
0x02091212,
0x0b03d065,
0xd319428b,
0x0a09e000,
0x428b0bc3,
0x03cbd301,
0x41521ac0,
0x428b0b83,
0x038bd301,
0x41521ac0,
0x428b0b43,
0x034bd301,
0x41521ac0,
0x428b0b03,
0x030bd301,
0x41521ac0,
0x428b0ac3,
0x02cbd301,
0x41521ac0,
0x428b0a83,
0x028bd301,
0x41521ac0,
0x428b0a43,
0x024bd301,
0x41521ac0,
0x428b0a03,
0x020bd301,
0x41521ac0,
0x09c3d2cd,
0xd301428b,
0x1ac001cb,
0x09834152,
0xd301428b,
0x1ac0018b,
0x09434152,
0xd301428b,
0x1ac0014b,
0x09034152,
0xd301428b,
0x1ac0010b,
0x08c34152,
0xd301428b,
0x1ac000cb,
0x08834152,
0xd301428b,
0x1ac0008b,
0x08434152,
0xd301428b,
0x1ac0004b,
0x1a414152,
0x4601d200,
0x46104152,
0xe7ff4770,
0x2000b501,
0xf806f000,
0x46c0bd02,
0xd0f72900,
0x4770e776,
0x46c04770,
0x464fb5f0,
0xb4c04646,
0x0c360416,
0x00334699,
0x0c2c0405,
0x0c150c07,
0x437e4363,
0x4365436f,
0x19ad0c1c,
0x469c1964,
0xd90342a6,
0x025b2380,
0x44474698,
0x0c254663,
0x041d19ef,
0x434a464b,
0x0c2d4343,
0x19640424,
0x19c91899,
0xbc0c0020,
0x46994690,
0x46c0bdf0,
0x18820003,
0xd0024293,
0x33017019,
0x4770e7fa,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x01010000,
0x40020400,
0x00200400,
0x00002005,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
0x00000000,
};

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
static void wlbt_karam_dump(struct platform_mif *platform)
{
	unsigned int ka_addr = PMU_BOOT_RAM_START;
	unsigned int val;
	unsigned int ka_addr_end;

	if (platform->ka_patch_fw && !fw_compiled_in_kernel)
		ka_addr_end = ka_addr + platform->ka_patch_len;
	else
		ka_addr_end = ka_addr + sizeof(ka_patch);

	SCSC_TAG_INFO(PLAT_MIF, "Print KARAM area START:0x%p END:0x%p\n", ka_addr, ka_addr_end);

	regmap_write(platform->boot_cfg, PMU_BOOT, PMU_BOOT_AP_ACC);

	while (ka_addr < ka_addr_end) {
		regmap_read(platform->boot_cfg, ka_addr, &val);
		SCSC_TAG_INFO(PLAT_MIF, "0x%08x: 0x%08x\n", ka_addr, val);
		ka_addr += (unsigned int)sizeof(*ka_patch);
	}

	regmap_write(platform->boot_cfg, PMU_BOOT, PMU_BOOT_PMU_ACC);
}

static int wlbt_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct platform_mif *platform = container_of(nb, struct platform_mif, itmon_nb);
	int ret = NOTIFY_DONE;
	struct itmon_notifier *itmon_data = (struct itmon_notifier *)nb_data;

	if (!itmon_data) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "itmon_data is NULL");
		goto error_exit;
	}

	if (itmon_data->dest &&
		(!strncmp("WLBT", itmon_data->dest, sizeof("WLBT") - 1))) {
		platform_wlbt_regdump(&platform->interface);
		if ((itmon_data->target_addr >= PMU_BOOT_RAM_START)
			&& (itmon_data->target_addr <= PMU_BOOT_RAM_END))
			wlbt_karam_dump(platform);
		ret = ITMON_S2D_MASK;
	} else if (itmon_data->port &&
		(!strncmp("WLBT", itmon_data->port, sizeof("WLBT") - 1))) {
		platform_wlbt_regdump(&platform->interface);
		if (platform->mem_start)
			SCSC_TAG_INFO(PLAT_MIF, "Physical mem_start addr: 0x%lx\n", platform->mem_start);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
		if (platform->paddr)
			SCSC_TAG_INFO(PLAT_MIF, "Physical memlog_start addr: 0x%lx\n", platform->paddr);
#endif
		SCSC_TAG_INFO(PLAT_MIF, "ITMON Type: %d, Error code: %d\n", itmon_data->read, itmon_data->errcode);
		ret = ((!itmon_data->read) && (itmon_data->errcode == ITMON_ERRCODE_DECERR)) ? ITMON_SKIP_MASK : NOTIFY_OK;
	} else if (itmon_data->master &&
		(!strncmp("WLBT", itmon_data->master, sizeof("WLBT") - 1))) {
		platform_wlbt_regdump(&platform->interface);
		if (platform->mem_start)
			SCSC_TAG_INFO(PLAT_MIF, "Physical mem_start addr: 0x%lx\n", platform->mem_start);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
		if (platform->paddr)
			SCSC_TAG_INFO(PLAT_MIF, "Physical memlog_start addr: 0x%lx\n", platform->paddr);
#endif
		SCSC_TAG_INFO(PLAT_MIF, "ITMON Type: %d, Error code: %d\n", itmon_data->read, itmon_data->errcode);
		ret = ((!itmon_data->read) && (itmon_data->errcode == ITMON_ERRCODE_DECERR)) ? ITMON_SKIP_MASK : NOTIFY_OK;
	}

error_exit:
	return ret;
}
#endif



void platform_cfg_req_irq_clean_pending(struct platform_mif *platform)
{
	int irq;
	int ret;
	bool pending = 0;
	char *irqs_name = {"CFG_REQ"};

	irq = platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num;
	ret = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);

	if (!ret) {
		if (pending == 1) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "IRQCHIP_STATE %d(%s): pending %d",
					  irq, irqs_name, pending);
			pending = 0;
			ret = irq_set_irqchip_state(irq, IRQCHIP_STATE_PENDING, pending);
		}
	}
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static void platform_mif_set_memlog_baaw(struct platform_mif *platform, dma_addr_t paddr)
{
	unsigned int val = 0;

#define MEMLOG_CHECK(x) do { \
	int retval = (x); \
	if (retval < 0) {\
		pr_err("%s failed at L%d", __func__, __LINE__); \
		goto baaw1_error; \
	} \
} while (0)

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW_1 start\n");
#if IS_ENABLED(CONFIG_EXYNOS_EL2)
	MEMLOG_CHECK(exynos_hvc(HVC_FID_SET_BAAW_WINDOW,
				1,
				((u64)(WLBT_DBUS_BAAW_1_START >> 4) << 32) | (WLBT_DBUS_BAAW_1_END >> 4),
				(paddr >> 4),
				WLBT_BAAW_ACCESS_CTRL));

	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW1_D_WLBT_START, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_START: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW1_D_WLBT_END, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_END: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW1_D_WLBT_REMAP, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_REMAP: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW1_D_WLBT_INIT_DONE, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_ENABLE_DONE: 0x%x.\n", val);
#else
	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, BAAW1_D_WLBT_START, WLBT_DBUS_BAAW_1_START >> 4));
	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, BAAW1_D_WLBT_END, WLBT_DBUS_BAAW_1_END >> 4));
	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, BAAW1_D_WLBT_REMAP, paddr >> 4));
	MEMLOG_CHECK(regmap_write(platform->dbus_baaw, BAAW1_D_WLBT_INIT_DONE, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->dbus_baaw, BAAW1_D_WLBT_START, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_START: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW1_D_WLBT_END, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_END: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW1_D_WLBT_REMAP, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_REMAP: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW1_D_WLBT_INIT_DONE, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_1_ENABLE_DONE: 0x%x.\n", val);
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW_1 end\n");
baaw1_error:
	return;
}
#endif

/* WARNING! this function might be
 * running in IRQ context if CONFIG_SCSC_WLBT_CFG_REQ_WQ is not defined
 */
void platform_set_wlbt_regs(struct platform_mif *platform)
{
	u64 ret64 = 0;
	const u64 EXYNOS_WLBT = 0x1;
	/*s32 ret = 0;*/
	unsigned int ka_addr = PMU_BOOT_RAM_START;
	uint32_t *ka_patch_addr;
	uint32_t *ka_patch_addr_end;
	size_t ka_patch_len;
	unsigned int id;
	unsigned int val, val1, val2, val3, val4;
	int i, ret;
	unsigned long val_l;

	if (platform->ka_patch_fw && !fw_compiled_in_kernel) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "ka_patch present in FW image\n");
		ka_patch_addr = platform->ka_patch_fw;
		ka_patch_addr_end = ka_patch_addr + (platform->ka_patch_len / sizeof(uint32_t));
		ka_patch_len = platform->ka_patch_len;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "no ka_patch FW file. Use default(size:%d)\n",
				  ARRAY_SIZE(ka_patch));
		ka_patch_addr = &ka_patch[0];
		ka_patch_addr_end = ka_patch_addr + ARRAY_SIZE(ka_patch);
		ka_patch_len = ARRAY_SIZE(ka_patch);
	}

#define CHECK(x) do { \
	int retval = (x); \
	if (retval < 0) {\
		pr_err("%s failed at L%d", __func__, __LINE__); \
		goto cfg_error; \
	} \
} while (0)

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	/* Set TZPC to non-secure mode */
	ret64 = exynos_smc(SMC_CMD_CONN_IF, (EXYNOS_WLBT << 32) | EXYNOS_SET_CONN_TZPC, 0, 0);
	if (ret64)
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
		"Failed to set TZPC to non-secure mode: %llu\n", ret64);
	else
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"SMC_CMD_CONN_IF run successfully : %llu\n", ret64);

	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x14410204), 0xFFFFFFFF, 0);
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x14410214), 0xFFFFFFFF, 0);
// disabling S2MPU DBUS
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x14480054), 0x000000FF, 0);
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x14488054), 0x000000FF, 0);
	ret = exynos_smc_readsfr(0x14480058, &val_l);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Reading register 0x14480058, val = 0x%x\n", val_l);
	ret = exynos_smc_readsfr(0x14488058, &val_l);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Reading register 0x14488058, val = 0x%x\n", val_l);
// disabling S2MPU CBUS
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x14490054), 0x000000FF, 0);
	ret = exynos_smc(SMC_CMD_REG, SMC_REG_ID_SFR_W(0x14498054), 0x000000FF, 0);
	ret = exynos_smc_readsfr(0x14490058, &val_l);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Reading register 0x14490058, val = 0x%x\n", val_l);
	ret = exynos_smc_readsfr(0x14498058, &val_l);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Reading register 0x14498058, val = 0x%x\n", val_l);

	/* Remapping boot address for WLBT processor (0x9000_0000) */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_REMAP begin\n");
	CHECK(regmap_write(platform->wlbt_remap, WLAN_PROC_RMP_BOOT, (WLBT_DBUS_BAAW_0_START + platform->remap_addr_wlan) >> 12));
	regmap_read(platform->wlbt_remap, WLAN_PROC_RMP_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLAN_REMAP: 0x%x.\n", val);

	CHECK(regmap_write(platform->wlbt_remap, WPAN_PROC_RMP_BOOT, (WLBT_DBUS_BAAW_0_START + platform->remap_addr_wpan)  >> 12));
	regmap_read(platform->wlbt_remap, WPAN_PROC_RMP_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WPAN_REMAP: 0x%x.\n", val);

	/* CHIP_VERSION_ID - update with AP view of SOC revision */
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "CHIP_VERSION_ID begin\n");
	regmap_read(platform->wlbt_remap, CHIP_VERSION_ID_OFFSET, &id);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read CHIP_VERSION_ID 0x%x\n", id);
	if ((id & CHIP_VERSION_ID_IP_ID) >> CHIP_VERSION_ID_IP_ID_SHIFT == 0xA8)
		id = (id & ~CHIP_VERSION_ID_IP_ID) | (CHIP_IP_ID_8845 << CHIP_VERSION_ID_IP_ID_SHIFT);
	id &= ~(CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR);
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
	id |= ((exynos_soc_info.revision << CHIP_VERSION_ID_IP_MINOR_SHIFT) & (CHIP_VERSION_ID_IP_MAJOR | CHIP_VERSION_ID_IP_MINOR));
#endif
	CHECK(regmap_write(platform->wlbt_remap, CHIP_VERSION_ID_OFFSET, id));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated CHIP_VERSION_ID 0x%x\n", id);

	/* DBUS_BAAW regions */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW_0 begin\n");

	/* Shared DRAM mapping. The destination address is the location reserved
	 * by the kernel.
	 */
//#if IS_ENABLED(CONFIG_EXYNOS_EL2)
#if 0
	CHECK(exynos_hvc(HVC_FID_SET_BAAW_WINDOW,
			 0,
			 ((u64)(WLBT_DBUS_BAAW_0_START >> 4) << 32) | (WLBT_DBUS_BAAW_0_END >> 4),
			 (platform->mem_start >> 4),
			 WLBT_BAAW_ACCESS_CTRL));

	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_START, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "MOHIT : Updated WLBT_DBUS_BAAW_0_START: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_END, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_END: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_REMAP, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_REMAP: 0x%x.\n", val);
	val = exynos_hvc(HVC_FID_GET_BAAW_WINDOW, WLBT_PBUS_BAAW_DBUS_BASE + BAAW0_D_WLBT_INIT_DONE, 0, 0, 0);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);
#else
	CHECK(regmap_write(platform->dbus_baaw, BAAW0_D_WLBT_START, WLBT_DBUS_BAAW_0_START >> 4));
	CHECK(regmap_write(platform->dbus_baaw, BAAW0_D_WLBT_END, WLBT_DBUS_BAAW_0_END >> 4));
	CHECK(regmap_write(platform->dbus_baaw, BAAW0_D_WLBT_REMAP, platform->mem_start >> 4));
	CHECK(regmap_write(platform->dbus_baaw, BAAW0_D_WLBT_INIT_DONE, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->dbus_baaw, BAAW0_D_WLBT_START, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_START: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW0_D_WLBT_END, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_END: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW0_D_WLBT_REMAP, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_REMAP: 0x%x.\n", val);
	regmap_read(platform->dbus_baaw, BAAW0_D_WLBT_INIT_DONE, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated WLBT_DBUS_BAAW_0_ENABLE_DONE: 0x%x.\n", val);
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "DBUS_BAAW_0 end\n");

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	if (platform->paddr > 0)
		platform_mif_set_memlog_baaw(platform, platform->paddr);
#endif

	/* PBUS_BAAW regions */
	/* ref wlbt_if_S5E981.c, updated for MX450 memory map */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CBUS_BAAW begin\n");

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_0, WLBT_CBUS_BAAW_0_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_0, WLBT_CBUS_BAAW_0_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_0, WLBT_MAILBOX_AP_WLAN >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_0, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_0, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_0, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_0, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_0, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_0(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_1, WLBT_CBUS_BAAW_1_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_1, WLBT_CBUS_BAAW_1_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_1, WLBT_USI_CHUB2 >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_1, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_1, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_1, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_1, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_1, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_1(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_2, WLBT_CBUS_BAAW_2_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_2, WLBT_CBUS_BAAW_2_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_2, WLBT_SYSREG_COMBINE_CHUB2WLBT >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_2, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_2, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_2, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_2, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_2, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_2(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_3, WLBT_CBUS_BAAW_3_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_3, WLBT_CBUS_BAAW_3_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_3, WLBT_USI_CHUB0 >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_3, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_3, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_3, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_3, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_3, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_3(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_4, WLBT_CBUS_BAAW_4_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_4, WLBT_CBUS_BAAW_4_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_4, WLBT_GPIO_CHUB >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_4, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_4, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_4, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_4, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_4, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_4(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_5, WLBT_CBUS_BAAW_5_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_5, WLBT_CBUS_BAAW_5_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_5, WLBT_GPIO_CMGP >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_5, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_5, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_5, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_5, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_5, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_1(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_6, WLBT_CBUS_BAAW_6_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_6, WLBT_CBUS_BAAW_6_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_6, WLBT_SYSREG_CMGP2WLBT >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_6, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_6, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_6, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_6, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_6, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_6(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_7, WLBT_CBUS_BAAW_7_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_7, WLBT_CBUS_BAAW_7_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_7, WLBT_USI_CMGP0 >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_7, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_7, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_7, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_7, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_7, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_7(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_START_8, WLBT_CBUS_BAAW_8_START >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_END_8, WLBT_CBUS_BAAW_8_END >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_REMAP_8, WLBT_CHUB_SRAM >> 4));
	CHECK(regmap_write(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_8, WLBT_BAAW_ACCESS_CTRL));

	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_START_8, &val1);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_END_8, &val2);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_REMAP_8, &val3);
	regmap_read(platform->pbus_baaw, BAAW_C_WLBT_INIT_DONE_8, &val4);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated PBUS_BAAW_8(start, end, remap, enable):(0x%08x, 0x%08x, 0x%08x, 0x%08x)\n",
			  val1, val2, val3, val4);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "CBUS_BAAW end\n");

	/* PMU boot patch */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "AP accesses KARAM\n");
	CHECK(regmap_write(platform->boot_cfg, PMU_BOOT, PMU_BOOT_AP_ACC));
	regmap_read(platform->boot_cfg, PMU_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_SOURCE: 0x%x\n", val);
	i = 0;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch start\n");
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_addr : 0x%x\n", ka_patch_addr);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_addr_end: 0x%x\n", ka_patch_addr_end);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "ka_patch_len: 0x%x\n", ka_patch_len);

	while (ka_patch_addr < ka_patch_addr_end) {
		CHECK(regmap_write(platform->boot_cfg, ka_addr, *ka_patch_addr));
		ka_addr += (unsigned int)sizeof(unsigned int);
		ka_patch_addr++;
		i++;
	}
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "KA patch done\n");

	/* Notify PMU of configuration done */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT PMU accesses KARAM\n");
	CHECK(regmap_write(platform->boot_cfg, PMU_BOOT, PMU_BOOT_PMU_ACC));
	regmap_read(platform->boot_cfg, PMU_BOOT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Updated BOOT_SOURCE: 0x%x\n", val);

	/* WLBT FW could panic as soon as CFG_ACK is set, so change state.
	 * This allows early FW panic to be dumped.
	 */
	platform->boot_state = WLBT_BOOT_ACK_CFG_REQ;

	/* BOOT_CFG_ACK */
	CHECK(regmap_write(platform->boot_cfg, PMU_BOOT_ACK, PMU_BOOT_COMPLETE));
	regmap_read(platform->boot_cfg, PMU_BOOT_ACK, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Updated BOOT_CFG_ACK: 0x%x\n", val);

	/* Mark as CFQ_REQ handled, so boot may continue */
	platform->boot_state = WLBT_BOOT_CFG_DONE;
	goto done;

cfg_error:
	platform->boot_state = WLBT_BOOT_CFG_ERROR;
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			 "ERROR: WLBT Config failed. WLBT will not work\n");
done:
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
		"Complete CFG_ACK\n");
	complete(&platform->cfg_ack);
}

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
void platform_cfg_req_wq(struct work_struct *data)
{
	struct platform_mif *platform = container_of(data, struct platform_mif, cfgreq_wq);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "\n");
	platform_set_wlbt_regs(platform);
}
#endif

irqreturn_t platform_cfg_req_isr(int irq, void *data)
{
	struct platform_mif *platform = (struct platform_mif *)data;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "INT received, boot_state = %u\n", platform->boot_state);
	if (platform->boot_state == WLBT_BOOT_WAIT_CFG_REQ) {
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
		/* it is executed on process context. */
		queue_work(platform->cfgreq_workq, &platform->cfgreq_wq);
#else
		/* it is executed on interrupt context. */
		platform_set_wlbt_regs(platform);
#endif
	}
	return IRQ_HANDLED;
}

static void platform_mif_unregister_irq(struct platform_mif *platform)
{
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering IRQs\n");

#ifdef CONFIG_SCSC_QOS
	/* clear affinity mask */
	irq_set_affinity_hint(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, NULL);
#endif
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].irq_num, platform);
	/* Reset irq_disabled_cnt for WDOG IRQ since the IRQ itself is here
	 * unregistered and disabled
	 */
	atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	devm_free_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform);
}

static int platform_mif_register_irq(struct platform_mif *platform)
{
	int err;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering IRQs\n");

	/* Register MBOX irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering MBOX irq: %d flag 0x%x\n",
			  platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform->wlbt_irq[PLATFORM_MIF_MBOX].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num, platform_mif_isr,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX].flags, IRQ_NAME_MBOX, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register MBOX irq WPAN */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering MBOX WPAN irq: %d flag 0x%x\n",
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num, platform_mif_isr_wpan,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].flags, IRQ_NAME_MBOX_WPAN, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register WDOG irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering WDOG irq: %d flag 0x%x\n", platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_WDOG].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num, platform_wdog_isr,
			       platform->wlbt_irq[PLATFORM_MIF_WDOG].flags, IRQ_NAME_WDOG, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register WDOG handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Register MBOX irq PMU */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering MBOX PMU irq: %d flag 0x%x\n",
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].flags);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].irq_num, platform_mbox_pmu_isr,
			       platform->wlbt_irq[PLATFORM_MIF_MBOX_PMU].flags, IRQ_NAME_MBOX_PMU, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register MBOX PMU handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Mark as WLBT in reset before enabling IRQ to guard against
	 * spurious IRQ
	 */
	platform->boot_state = WLBT_BOOT_IN_RESET;
	smp_wmb(); /* commit before irq */

	/* Register WB2AP_CFG_REQ irq */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Registering CFG_REQ irq: %d flag 0x%x\n", platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num,
			  platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags);

	/* clean CFG_REQ PENDING interrupt. */
	platform_cfg_req_irq_clean_pending(platform);

	err = devm_request_irq(platform->dev, platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num, platform_cfg_req_isr,
			       platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].flags, DRV_NAME, platform);
	if (IS_ERR_VALUE((unsigned long)err)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to register CFG_REQ handler: %d. Aborting.\n", err);
		err = -ENODEV;
		return err;
	}

	/* Leave disabled until ready to handle */
	disable_irq_nosync(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	return 0;
}

static void platform_mif_destroy(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform_mif_unregister_irq(platform);
}

static char *platform_mif_get_uid(struct scsc_mif_abs *interface)
{
	/* Avoid unused parameter error */
	(void)interface;
	return "0";
}


static int platform_mif_start_wait_for_cfg_ack_completion(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* done as part of platform_mif_pmu_reset_release() init_done sequence*/
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "start wait for completion\n");
	/* At this point WLBT should assert the CFG_REQ IRQ, so wait for it */
	if (wait_for_completion_timeout(&platform->cfg_ack, WLBT_BOOT_TIMEOUT) == 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Timeout waiting for CFG_REQ IRQ\n");
		platform_wlbt_regdump(&platform->interface);
		return -ETIMEDOUT;
	}

	/* only continue if CFG_REQ IRQ configured WLBT/PMU correctly */
	if (platform->boot_state == WLBT_BOOT_CFG_ERROR) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "CFG_REQ failed to configure WLBT.\n");
		return -EIO;
	}

	return 0;
}

static int platform_mif_pmu_reset_release(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int		ret = 0;
	/* We're now ready for the IRQ */
	platform->boot_state = WLBT_BOOT_WAIT_CFG_REQ;
	smp_wmb(); /* commit before irq */

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "on boot_state = WLBT_BOOT_WAIT_CFG_REQ\n");
	if (!init_done) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "init\n");
		ret = pmu_cal_progress(platform, pmucal_wlbt.init, pmucal_wlbt.init_size);
		if (ret < 0)
			goto done;
		init_done = 1;
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "release\n");
		ret = pmu_cal_progress(platform, pmucal_wlbt.reset_release, pmucal_wlbt.reset_release_size);
		if (ret < 0)
			goto done;
	}

	/* Now handle the CFG_REQ IRQ */
	enable_irq(platform->wlbt_irq[PLATFORM_MIF_CFG_REQ].irq_num);

	/* Wait for CFG_ACK completion */
	ret = platform_mif_start_wait_for_cfg_ack_completion(interface);
done:
	return ret;
}

static int platform_mif_pmu_reset_assert(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
#ifdef CONFIG_WLBT_AUTOGEN_PMUCAL
	int r = 0;

	r = pmu_cal_progress(platform, pmucal_wlbt.reset_assert, pmucal_wlbt.reset_assert_size);

	if (r)
		platform_wlbt_regdump(&platform->interface);
	return r;
#else
#error "s5e8845 requires CONFIG_WLBT_AUTOGEN_PMUCAL falg enabled"
#endif
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int platform_mif_set_mem_region2
	(struct scsc_mif_abs *interface,
	void __iomem *_mem_region2,
	size_t _mem_size_region2)
{
	/* If memlogger API is enabled, mxlogger's mem should be set
	 * by another routine
	 */
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->mem_region2 = _mem_region2;
	platform->mem_size_region2 = _mem_size_region2;
	return 0;
}

static void platform_mif_set_memlog_paddr(struct scsc_mif_abs *interface, dma_addr_t paddr)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->paddr = paddr;
}

#endif

/* reset=0 - release from reset */
/* reset=1 - hold reset */
static int platform_mif_reset(struct scsc_mif_abs *interface, bool reset)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 ret = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (enable_platform_mif_arm_reset || !reset) {
		if (!reset) { /* Release from reset */
#if defined(CONFIG_ARCH_EXYNOS) || defined(CONFIG_ARCH_EXYNOS9)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				"SOC_VERSION: product_id 0x%x, rev 0x%x\n",
				exynos_soc_info.product_id, exynos_soc_info.revision);
#endif
			power_supplies_on(platform);
			ret = platform_mif_pmu_reset_release(interface);
		} else {
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
			cancel_work_sync(&platform->cfgreq_wq);
			flush_workqueue(platform->cfgreq_workq);
#endif
			/* Put back into reset */
			ret = platform_mif_pmu_reset_assert(interface);

			/* Free pmu array */
			if (platform->ka_patch_fw) {
				devm_kfree(platform->dev, platform->ka_patch_fw);
				platform->ka_patch_fw = NULL;
			}
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Not resetting ARM Cores - enable_platform_mif_arm_reset: %d\n",
				  enable_platform_mif_arm_reset);
	}
	return ret;
}

#define SCSC_STATIC_MIFRAM_PAGE_TABLE

static void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size)
{
	size_t      i;
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	struct page **pages = NULL;
#else
	static struct page *mif_map_pages[MIFRAMMAN_MAXMEM >> PAGE_SHIFT];
	struct page **pages = mif_map_pages;
#endif
	void        *vmem;

	size = PAGE_ALIGN(size);
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: kmalloc of %zd byte pages table failed\n", (size >> PAGE_SHIFT) * sizeof(*pages));
		return NULL;
	}
#else
	/* Reserve the table statically, but make sure .dts doesn't exceed it */

		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));

		/* Size passed in from .dts exceeds array */
		if (size > MIFRAMMAN_MAXMEM) {
			SCSC_TAG_ERR(PLAT_MIF,
				     "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
				     size, MIFRAMMAN_MAXMEM);
			return NULL;
		}
#endif

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	kfree(pages);
#endif
	if (!vmem)
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: vmap of %zd pages failed\n", (size >> PAGE_SHIFT));
	return (void __iomem *)vmem;
}

static void platform_mif_unmap_region(void *vmem)
{
	vunmap(vmem);
}

static void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u8                  i;

	if (allocated)
		*allocated = 0;

	platform->mem =
		platform_mif_map_region(platform->mem_start, platform->mem_size);

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Error remaping shared memory\n");
		return NULL;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Map: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);

	/* Initialise MIF registers with documented defaults */
	/* MBOXes */
	for (i = 0; i < NUM_MBOX_PLAT; i++) {
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
		platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
	}

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);

#ifdef CONFIG_SCSC_CHV_SUPPORT
	if (chv_disable_irq == true) {
		if (allocated)
			*allocated = platform->mem_size;
		return platform->mem;
	}
#endif
	/* register interrupts */
	if (platform_mif_register_irq(platform)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
		platform_mif_unmap_region(platform->mem);
		return NULL;
	}

	if (allocated)
		*allocated = platform->mem_size;
	/* Set the CR4 base address in Mailbox??*/
	return platform->mem;
}

/* HERE: Not sure why mem is passed in - its stored in platform -
 * as it should be
 */
static void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Avoid unused parameter error */
	(void)mem;

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);

#ifdef CONFIG_SCSC_CHV_SUPPORT
	/* Restore PIO changed by Maxwell subsystem */
	if (chv_disable_irq == false)
		/* Unregister IRQs */
		platform_mif_unregister_irq(platform);
#else
	platform_mif_unregister_irq(platform);
#endif
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
	platform_mif_unmap_region(platform->mem);
	platform->mem = NULL;
}

static u32 platform_mif_irq_bit_mask_status_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)) >> 16;
	else
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0)) >> 16;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
			   "Getting INTMR0 0x%x target %s\n",
			   val, (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	return val;
}

static u32 platform_mif_irq_get(struct scsc_mif_abs *interface, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;

	/* Function has to return the interrupts that are enabled *AND* not
	 *  masked
	 */
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;
	else
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Getting INTMSR0 0x%x target %s\n", val,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	return val;
}

static void platform_mif_irq_bit_set(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 reg;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}

	reg = INTGR1;
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));
	else
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(reg), (1 << bit_num));

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR1: bit %d on target %s\n", bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_irq_bit_clear(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	/* WRITE : 1 = Clears Interrupt */
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), ((1 << bit_num) << 16));
	else
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), ((1 << bit_num) << 16));

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTCR0: bit %d on target %s\n", bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_irq_bit_mask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 1 = Mask Interrupt */
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val | ((1 << bit_num) << 16));
	} else {
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 1 = Mask Interrupt */
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val | ((1 << bit_num) << 16));
	}
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTMR0: 0x%x bit %d on target %s\n", val | (1 << bit_num), bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_irq_bit_unmask(struct scsc_mif_abs *interface, int bit_num, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	if (bit_num >= 16) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect INT number: %d\n", bit_num);
		return;
	}
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	if (target == SCSC_MIF_ABS_TARGET_WLAN) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 0 = Unmask Interrupt */
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val & ~((1 << bit_num) << 16));
	} else {
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
		/* WRITE : 0 = Unmask Interrupt */
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val & ~((1 << bit_num) << 16));
	}
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "UNMASK Setting INTMR0: 0x%x bit %d on target %s\n", val & ~((1 << bit_num) << 16), bit_num,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
}

static void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting remapper address %u target %s\n", remap_addr,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform->remap_addr_wlan = remap_addr;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		platform->remap_addr_wpan = remap_addr;
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Incorrect target %d\n", target);
}

/* Return the contents of the mask register */
static u32 __platform_mif_irq_bit_mask_read(struct platform_mif *platform)
{
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read INTMR0: 0x%x\n", val);

	return val;
}

/* Return the contents of the mask register */
static u32 __platform_mif_irq_bit_mask_read_wpan(struct platform_mif *platform)
{
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(INTMR0));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read INTMR0: 0x%x\n", val);

	return val;
}

/* Write the mask register, destroying previous contents */
static void __platform_mif_irq_bit_mask_write(struct platform_mif *platform, u32 val)
{
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), val);
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write INTMR0: 0x%x\n", val);
}

static void __platform_mif_irq_bit_mask_write_wpan(struct platform_mif *platform, u32 val)
{
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), val);
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write INTMR0: 0x%x\n", val);
}

static void platform_mif_irq_reg_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler %pS in %p %p\n", handler, platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = handler;
	platform->irq_dev = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler %p\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_reg_handler_wpan(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif int handler for WPAN %pS in %p %p\n", handler, platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = handler;
	platform->irq_dev_wpan = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_unreg_handler_wpan(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif int handler for WPAN %p\n", interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev_wpan = NULL;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_reg_reset_request_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif reset_request int handler %pS in %p %p\n", handler, platform, interface);
	platform->reset_request_handler = handler;
	platform->irq_reset_request_dev = dev;
	if (atomic_read(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "Default WDOG handler disabled by spurios IRQ...re-enabling.\n");
		enable_irq(platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_num);
		atomic_set(&platform->wlbt_irq[PLATFORM_MIF_WDOG].irq_disabled_cnt, 0);
	}
}

static void platform_mif_irq_unreg_reset_request_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "UnRegistering mif reset_request int handler %p\n", interface);
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
}

static void platform_mif_suspend_reg_handler(struct scsc_mif_abs *interface,
		int (*suspend)(struct scsc_mif_abs *abs, void *data),
		void (*resume)(struct scsc_mif_abs *abs, void *data),
		void *data)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif suspend/resume handlers in %p %p\n", platform, interface);
	platform->suspend_handler = suspend;
	platform->resume_handler = resume;
	platform->suspendresume_data = data;
}

static void platform_mif_suspend_unreg_handler(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering mif suspend/resume handlers in %p %p\n", platform, interface);
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;
}

static u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 *addr;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "mbox index 0x%x target %s\n", mbox_index,
			  (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		addr = platform->base + MAILBOX_WLBT_REG(ISSR(mbox_index));
	else
		addr = platform->base_wpan + MAILBOX_WLBT_REG(ISSR(mbox_index));
	return addr;
}

static void platform_mif_irq_pmu_bit_mask(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(INTMR0));
	/* WRITE : 1 = Mask Interrupt */
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), val | (1 << 16));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTMR0: 0x%x bit 1 on target PMU\n", val | (1 << 16));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static void platform_mif_irq_pmu_bit_unmask(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 val;
	unsigned long       flags;

	spin_lock_irqsave(&platform->mif_spinlock, flags);
	val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(INTMR0));
	/* WRITE : 0 = Unmask Interrupt */
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), val & ~(1 << 16));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "UNMASK Setting INTMR0: 0x%x bit 1 on target PMU\n", val & ~(1 << 16));
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val;
	u32 irq_val;

	irq_val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;
	if (!irq_val) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Wrong PMU MAILBOX Interrupt!!\n");
		return 0;
	}

	val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(ISSR(0)));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read PMU MAILBOX: %u\n", val);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR0), (1 << 16));
	return val;
}

static int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(ISSR(0)), val);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write PMU MAILBOX: %u\n", val);

	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTGR1), 1);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR1: bit 1 on target PMU\n");
	return 0;
}

static int platform_load_pmu_fw(struct scsc_mif_abs *interface, u32 *ka_patch, size_t ka_patch_len)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (platform->ka_patch_fw) {
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
				   "ka_patch already allocated\n");
		devm_kfree(platform->dev, platform->ka_patch_fw);
	}

	platform->ka_patch_fw = devm_kzalloc(platform->dev, ka_patch_len, GFP_KERNEL);

	if (!platform->ka_patch_fw) {
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev, "Unable to allocate 0x%x\n", ka_patch_len);
		return -ENOMEM;
	}

	memcpy(platform->ka_patch_fw, ka_patch, ka_patch_len);
	platform->ka_patch_len = ka_patch_len;
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "load_pmu_fw sz %u done\n", ka_patch_len);
	return 0;
}

static void platform_mif_irq_reg_pmu_handler(struct scsc_mif_abs *interface, void (*handler)(int irq, void *data), void *dev)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering mif pmu int handler %pS in %p %p\n", handler, platform, interface);
	spin_lock_irqsave(&platform->mif_spinlock, flags);
	platform->pmu_handler = handler;
	platform->irq_dev_pmu = dev;
	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

static int platform_mif_wlbt_phandle_property_read_u32(struct scsc_mif_abs *interface,
				const char *phandle_name, const char *propname, u32 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct device_node *np;
	int err = 0;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get phandle: %s\n", phandle_name);
	np = of_parse_phandle(platform->dev->of_node, phandle_name, 0);
	if (np) {
		SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);
		if (of_property_read_u32(np, propname, out_value)) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
			err = -EINVAL;
		}
	} else {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "phandle doesn't exist\n");
		err = -EINVAL;
	}

	return err;
}

static int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem + platform->mem_size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Unable to get pointer reference\n");
		return -ENOMEM;
	}

	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)platform->mem);

	return 0;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static int platform_mif_get_mifram_ref_region2(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem_region2 + platform->mem_size_region2))
		return -ENOMEM;

	*ref = (scsc_mifram_ref)(
		(uintptr_t)ptr
		- (uintptr_t)platform->mem_region2
		+ (LOGGING_REF_OFFSET));
	return 0;
}
#endif

static void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size)
		return (void *)((uintptr_t)platform->mem + (uintptr_t)ref);
	else
		return NULL;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
static void *platform_mif_get_mifram_ptr_region2(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}
	ref -= LOGGING_REF_OFFSET;

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size_region2)
		return (void *)((uintptr_t)platform->mem_region2 + (uintptr_t)ref);
	else
		return NULL;
}
#endif

static void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_start) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	return (void *)((uintptr_t)platform->mem_start + (uintptr_t)ref);
}

static uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return vmalloc_to_pfn(platform->mem);
}

static struct device *platform_mif_get_mif_device(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	return platform->dev;
}

static bool platform_mif_wlbt_property_read_bool(struct scsc_mif_abs *interface, const char *propname)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	return of_property_read_bool(platform->np, propname);
}

static int platform_mif_wlbt_property_read_u8(struct scsc_mif_abs *interface,
					      const char *propname, u8 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	if (of_property_read_u8_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_u16(struct scsc_mif_abs *interface,
					       const char *propname, u16 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname, size);

	if (of_property_read_u16_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_u32(struct scsc_mif_abs *interface,
					       const char *propname, u32 *out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	if (of_property_read_u32_array(platform->np, propname, out_value, size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist\n");
		return -EINVAL;
	}
	return 0;
}

static int platform_mif_wlbt_property_read_string(struct scsc_mif_abs *interface,
						  const char *propname, char **out_value, size_t size)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int ret;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Get property: %s\n", propname);

	ret = of_property_read_string_array(platform->np, propname, (const char **)out_value, size);
	if (ret <= 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Property doesn't exist (ret=%d)\n", ret);
		return ret;
	}
	return ret;
}

static void platform_mif_irq_clear(void)
{
	/* Implement if required */
}

static int platform_mif_read_register(struct scsc_mif_abs *interface, u64 id, u32 *val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	if (id == SCSC_REG_READ_WLBT_STAT) {
		regmap_read(platform->pmureg, WLBT_STAT, val);
		return 0;
	}

	return -EIO;
}

static void platform_mif_dump_register(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long       flags;
	unsigned int val;
	int i;

	spin_lock_irqsave(&platform->mif_spinlock, flags);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR0)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR0 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR0)));

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTGR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTGR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTCR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTCR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTSR1)));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "INTMSR1 0x%08x\n", platform_mif_reg_read(platform, MAILBOX_WLBT_REG(INTMSR1)));

	for (i = 0; i < NUM_MBOX_PLAT; i++) {
		val = platform_mif_reg_read(platform, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLAN MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
		val = platform_mif_reg_read_wpan(platform, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WPAN MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
		val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(ISSR(i)));
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "PMU MBOX[%d]: ISSR(%d) val = 0x%x\n", i, i, val);
	}
	regmap_read(platform->pmureg, WLBT_CONFIGURATION, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_CONFIGURATION 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_STAT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_STAT 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_OUT, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_OUT 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_IN, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_IN 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_DEBUG, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_DEBUG 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_STATUS, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_STATUS 0x%08x\n", val);
	regmap_read(platform->pmureg, WLBT_STATES, &val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "WLBT_STATES 0x%08x\n", val);

	spin_unlock_irqrestore(&platform->mif_spinlock, flags);
}

inline void platform_int_debug(struct platform_mif *platform)
{
	int i;
	int irq;
	int ret;
	bool pending, active, masked;
	int irqs[] = {PLATFORM_MIF_MBOX, PLATFORM_MIF_WDOG};
	char *irqs_name[] = {"MBOX", "WDOG"};

	for (i = 0; i < (sizeof(irqs) / sizeof(int)); i++) {
		irq = platform->wlbt_irq[irqs[i]].irq_num;

		ret  = irq_get_irqchip_state(irq, IRQCHIP_STATE_PENDING, &pending);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_ACTIVE,  &active);
		ret |= irq_get_irqchip_state(irq, IRQCHIP_STATE_MASKED,  &masked);
		if (!ret)
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "IRQCHIP_STATE %d(%s): pending %d, active %d, masked %d\n",
					  irq, irqs_name[i], pending, active, masked);
	}
	platform_mif_dump_register(&platform->interface);
}

static void platform_mif_cleanup(struct scsc_mif_abs *interface)
{
}

static void platform_mif_restart(struct scsc_mif_abs *interface)
{
}

#ifdef CONFIG_OF_RESERVED_MEM
static int __init platform_mif_wifibt_if_reserved_mem_setup(struct reserved_mem *remem)
{
	SCSC_TAG_DEBUG(PLAT_MIF,
		       "memory reserved: mem_base=%#lx, mem_size=%zd\n",
		       (unsigned long)remem->base, (size_t)remem->size);

	sharedmem_base = remem->base;
	sharedmem_size = remem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(wifibt_if, "exynos,wifibt_if", platform_mif_wifibt_if_reserved_mem_setup);
#endif

struct scsc_mif_abs *platform_mif_create(struct platform_device *pdev)
{
	struct scsc_mif_abs *platform_if;
	struct platform_mif *platform =
		(struct platform_mif *)devm_kzalloc(&pdev->dev, sizeof(struct platform_mif), GFP_KERNEL);
	int err = 0;
	u8 i = 0;
	struct resource *reg_res;

	if (!platform)
		return NULL;

	SCSC_TAG_INFO_DEV(PLAT_MIF, &pdev->dev,
			  "Creating MIF platform device\n");

	platform_if = &platform->interface;

	/* initialise interface structure */
	platform_if->destroy = platform_mif_destroy;
	platform_if->get_uid = platform_mif_get_uid;
	platform_if->reset = platform_mif_reset;
	platform_if->wlbt_regdump = platform_wlbt_regdump;
	platform_if->map = platform_mif_map;
	platform_if->unmap = platform_mif_unmap;
	platform_if->irq_bit_set = platform_mif_irq_bit_set;
	platform_if->irq_get = platform_mif_irq_get;
	platform_if->irq_bit_mask_status_get = platform_mif_irq_bit_mask_status_get;
	platform_if->irq_bit_clear = platform_mif_irq_bit_clear;
	platform_if->irq_bit_mask = platform_mif_irq_bit_mask;
	platform_if->irq_bit_unmask = platform_mif_irq_bit_unmask;
	platform_if->remap_set = platform_mif_remap_set;
	platform_if->irq_reg_handler = platform_mif_irq_reg_handler;
	platform_if->irq_unreg_handler = platform_mif_irq_unreg_handler;
	platform_if->irq_reg_handler_wpan = platform_mif_irq_reg_handler_wpan;
	platform_if->irq_unreg_handler_wpan = platform_mif_irq_unreg_handler_wpan;
	platform_if->irq_reg_reset_request_handler = platform_mif_irq_reg_reset_request_handler;
	platform_if->irq_unreg_reset_request_handler = platform_mif_irq_unreg_reset_request_handler;
	platform_if->suspend_reg_handler = platform_mif_suspend_reg_handler;
	platform_if->suspend_unreg_handler = platform_mif_suspend_unreg_handler;
	platform_if->get_mbox_ptr = platform_mif_get_mbox_ptr;
	platform_if->get_mifram_ptr = platform_mif_get_mifram_ptr;
	platform_if->get_mifram_ref = platform_mif_get_mifram_ref;
	platform_if->get_mifram_pfn = platform_mif_get_mif_pfn;
	platform_if->get_mifram_phy_ptr = platform_mif_get_mifram_phy_ptr;
	platform_if->get_mif_device = platform_mif_get_mif_device;
	platform_if->irq_clear = platform_mif_irq_clear;
	platform_if->mif_dump_registers = platform_mif_dump_register;
	platform_if->mif_read_register = platform_mif_read_register;
	platform_if->mif_cleanup = platform_mif_cleanup;
	platform_if->mif_restart = platform_mif_restart;
#ifdef CONFIG_SCSC_QOS
	platform_if->mif_pm_qos_add_request = platform_mif_pm_qos_add_request;
	platform_if->mif_pm_qos_update_request = platform_mif_pm_qos_update_request;
	platform_if->mif_pm_qos_remove_request = platform_mif_pm_qos_remove_request;
	platform_if->mif_set_affinity_cpu = platform_mif_set_affinity_cpu;
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	platform_if->get_mifram_ptr_region2 = platform_mif_get_mifram_ptr_region2;
	platform_if->get_mifram_ref_region2 = platform_mif_get_mifram_ref_region2;
	platform_if->set_mem_region2 = platform_mif_set_mem_region2;
	platform_if->set_memlog_paddr = platform_mif_set_memlog_paddr;
	platform->paddr = 0;
#endif
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	platform_if->recovery_disabled_reg = platform_recovery_disabled_reg;
	platform_if->recovery_disabled_unreg = platform_recovery_disabled_unreg;
#endif
	platform_if->irq_pmu_bit_mask = platform_mif_irq_pmu_bit_mask;
	platform_if->irq_pmu_bit_unmask = platform_mif_irq_pmu_bit_unmask;
	platform_if->get_mbox_pmu = platform_mif_get_mbox_pmu;
	platform_if->set_mbox_pmu = platform_mif_set_mbox_pmu;
	platform_if->load_pmu_fw = platform_load_pmu_fw;
	platform_if->irq_reg_pmu_handler = platform_mif_irq_reg_pmu_handler;
	platform_if->wlbt_phandle_property_read_u32 = platform_mif_wlbt_phandle_property_read_u32;
	platform->pmu_handler = platform_mif_irq_default_handler;
	platform->irq_dev_pmu = NULL;
	/* Reset ka_patch pointer & size */
	platform->ka_patch_fw = NULL;
	platform->ka_patch_len = 0;
	/* Update state */
	platform->pdev = pdev;
	platform->dev = &pdev->dev;

	platform->wlan_handler = platform_mif_irq_default_handler;
	platform->wpan_handler = platform_mif_irq_default_handler;
	platform->irq_dev = NULL;
	platform->reset_request_handler = platform_mif_irq_reset_request_default_handler;
	platform->irq_reset_request_dev = NULL;
	platform->suspend_handler = NULL;
	platform->resume_handler = NULL;
	platform->suspendresume_data = NULL;

	platform->np = pdev->dev.of_node;
	platform_if->wlbt_property_read_bool = platform_mif_wlbt_property_read_bool;
	platform_if->wlbt_property_read_u8 = platform_mif_wlbt_property_read_u8;
	platform_if->wlbt_property_read_u16 = platform_mif_wlbt_property_read_u16;
	platform_if->wlbt_property_read_u32 = platform_mif_wlbt_property_read_u32;
	platform_if->wlbt_property_read_string = platform_mif_wlbt_property_read_string;
#ifdef CONFIG_OF_RESERVED_MEM
	if (!sharedmem_base) {
		struct device_node *np;

		np = of_parse_phandle(platform->dev->of_node, "memory-region", 0);
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "module build register sharedmem np %x\n", np);
		if (np && of_reserved_mem_lookup(np)) {
			platform->mem_start = of_reserved_mem_lookup(np)->base;
			platform->mem_size = of_reserved_mem_lookup(np)->size;
		}
	} else {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "built-in register sharedmem\n");
		platform->mem_start = sharedmem_base;
		platform->mem_size = sharedmem_size;
	}
#else
	/* If CONFIG_OF_RESERVED_MEM is not defined, sharedmem values should be
	 * parsed from the scsc_wifibt binding
	 */
	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-base", (u32 *) &sharedmem_base)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_start = sharedmem_base;

	if (of_property_read_u32(pdev->dev.of_node, "sharedmem-size", (u32 *) &sharedmem_size)) {
		err = -EINVAL;
		goto error_exit;
	}
	platform->mem_size = sharedmem_size;
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "platform->mem_start 0x%x platform->mem_size 0x%x\n",
			  (u32)platform->mem_start, (u32)platform->mem_size);
	if (platform->mem_start == 0)
		SCSC_TAG_WARNING_DEV(PLAT_MIF, platform->dev,
				     "platform->mem_start is 0");

	if (platform->mem_size == 0) {
		/* We return return if mem_size is 0 as it does not make any
		 * sense. This may be an indication of an incorrect platform
		 * device binding.
		 */
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "platform->mem_size is 0");
		err = -EINVAL;
		goto error_exit;
	}

	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, " 1 resource name is %s\n", reg_res->name);
	platform->base = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WLAN\n");
		err = PTR_ERR(platform->base);
		goto error_exit;
	}

	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "2 resource name is %s\n", reg_res->name);
	platform->base_wpan = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base_wpan)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_WPAN\n");
		err = PTR_ERR(platform->base_wpan);
		goto error_exit;
	}

	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "3 resource name is %s\n", reg_res->name);
	platform->base_pmu = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(platform->base_pmu)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Error getting mem resource for MAILBOX_PMU\n");
		err = PTR_ERR(platform->base_pmu);
		goto error_exit;
	}

#if 1
	for (i = 0; i < 6; i++) {

		int irq_num;
		int             irqtag = 0;
		struct resource irq_res;
		irq_num = of_irq_to_resource(platform->dev->of_node, i, &irq_res);
		if (irq_num <= 0) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "invalid irq_num %d for index %d\n", irq_num, i);
			continue;
		}
		if (!strcmp(irq_res.name, "MBOX_WLAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WLAN irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_MBOX;
		} else if (!strcmp(irq_res.name, "MBOX_WPAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WPAN irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_MBOX_WPAN;
		} else if (!strcmp(irq_res.name, "ALIVE")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "ALIVE irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_ALIVE;
		} else if (!strcmp(irq_res.name, "WDOG")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "WDOG irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_WDOG;
		} else if (!strcmp(irq_res.name, "CFG_REQ")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "CFG_REQ irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_CFG_REQ;
		} else if (!strcmp(irq_res.name, "MBOX_PMU")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_PMU irq %d flag 0x%x\n",
					  (u32)irq_res.start, (u32)irq_res.flags);
			irqtag = PLATFORM_MIF_MBOX_PMU;
		} else {
			SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev,
					 "Invalid irq res name: %s\n",
				irq_res.name);
		}
		platform->wlbt_irq[irqtag].irq_num = irq_res.start;
		platform->wlbt_irq[irqtag].flags = (irq_res.flags & IRQF_TRIGGER_MASK);
		atomic_set(&platform->wlbt_irq[irqtag].irq_disabled_cnt, 0);
/*
		irq_num = platform_get_irq_byname(pdev, "ALIVE");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq number for ALIVE is %d\n", irq_num);
		irq_num = platform_get_irq_byname(pdev, "MBOX_WLAN");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq number for MBOX_WLAN is %d\n", irq_num);
		irq_num = platform_get_irq_byname(pdev, "MBOX_PMU");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq number for MBOX_PMU is %d\n", irq_num);
		irq_num = platform_get_irq_byname(pdev, "MBOX_WPAN");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq number for MBOX_WPAN is %d\n", irq_num);
		irq_num = platform_get_irq_byname(pdev, "CFG_REQ");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq number for CFG_REQ is %d\n", irq_num);
		irq_num = platform_get_irq_byname(pdev, "WDOG");
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq number for WDOG is %d\n", irq_num);*/
	//	irq_num = platform_get_irq(pdev, i);
/*		if (irq_num < 0) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "No IRQ resource at index %d\n", i);
			WARN(1, "IRQ resource are not found\n");
			err = -ENOENT;
			goto error_exit;
		}
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "irq number is %d\n", irq_num);
		switch (irq_num) {
		case ALIVE:
			irqtag = PLATFORM_MIF_ALIVE;
			break;
		case MBOX_WLAN:
			irqtag = PLATFORM_MIF_MBOX;
			break;
		case MBOX_PMU:
			irqtag = PLATFORM_MIF_MBOX_PMU;
			break;
		case MBOX_WPAN:
			irqtag = PLATFORM_MIF_MBOX_WPAN;
			break;
		case CFG_REQ:
			irqtag = PLATFORM_MIF_CFG_REQ;
			break;
		case WDOG:
			irqtag = PLATFORM_MIF_WDOG;
			break;
		default:
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "unknown IRQ %d\n", irq_num);
			break;
		}
		if (irq_num > 0)
			atomic_set(&platform->wlbt_irq[irqtag].irq_disabled_cnt, 0);*/
	}
#else

	/* Get the 6 IRQ resources */
	for (i = 0; i < 6; i++) {
//		struct resource *irq_res = NULL;
//		int             irqtag;
		int irq_num;
/*
		irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!irq_res) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				"No IRQ resource at index %d\n", i);
			err = -ENOENT;
			goto error_exit;
		}*/
		irq_num = platform_get_irq(pdev, i);
		if (irq_num < 0) {
			SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "No IRQ resource at index %d\n", i);
			WARN(1, "IRQ resource are not found\n");
			err = -ENOENT;
			goto error_exit;
		}
/*
		if (!strcmp(irq_res->name, "MBOX_WLAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WLAN irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX;
		} else if (!strcmp(irq_res->name, "MBOX_WPAN")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_WPAN irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX_WPAN;
		} else if (!strcmp(irq_res->name, "ALIVE")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "ALIVE irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_ALIVE;
		} else if (!strcmp(irq_res->name, "WDOG")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "WDOG irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_WDOG;
		} else if (!strcmp(irq_res->name, "CFG_REQ")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "CFG_REQ irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_CFG_REQ;
		} else if (!strcmp(irq_res->name, "MBOX_PMU")) {
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
					  "MBOX_PMU irq %d flag 0x%x\n",
					  (u32)irq_res->start, (u32)irq_res->flags);
			irqtag = PLATFORM_MIF_MBOX_PMU;
		} else {
			SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev,
					 "Invalid irq res name: %s\n",
				irq_res->name);
			err = -EINVAL;
			goto error_exit;
		}
		platform->wlbt_irq[irqtag].irq_num = irq_res->start;
		platform->wlbt_irq[irqtag].flags = (irq_res->flags & IRQF_TRIGGER_MASK);
		atomic_set(&platform->wlbt_irq[irqtag].irq_disabled_cnt, 0);*/
	}
#endif

	/* PMU reg map - syscon */
	platform->pmureg = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,syscon-phandle");
	if (IS_ERR(platform->pmureg)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"syscon regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->pmureg));
		err = -EINVAL;
		goto error_exit;
	}

	/* Completion event and state used to indicate CFG_REQ IRQ occurred */
	init_completion(&platform->cfg_ack);
	platform->boot_state = WLBT_BOOT_IN_RESET;

	/* DBUS_BAAW */
	platform->dbus_baaw = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,dbus_baaw-syscon-phandle");
	if (IS_ERR(platform->dbus_baaw)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"dbus_baaw regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->dbus_baaw));
		err = -EINVAL;
		goto error_exit;
	}

	/* PBUS_BAAW */
	platform->pbus_baaw = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,pbus_baaw-syscon-phandle");
	if (IS_ERR(platform->pbus_baaw)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"pbus_baaw regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->pbus_baaw));
		err = -EINVAL;
		goto error_exit;
	}
	/* WLBT_REMAP */
	platform->wlbt_remap = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,wlbt_remap-syscon-phandle");
	if (IS_ERR(platform->wlbt_remap)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"wlbt_remap regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->wlbt_remap));
		err = -EINVAL;
		goto error_exit;
	}

	/* BOOT_CFG */
	platform->boot_cfg = syscon_regmap_lookup_by_phandle(platform->dev->of_node,
							   "samsung,boot_cfg-syscon-phandle");
	if (IS_ERR(platform->boot_cfg)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"boot_cfg regmap lookup failed. Aborting. %ld\n",
				PTR_ERR(platform->boot_cfg));
		err = -EINVAL;
		goto error_exit;
	}

#ifdef CONFIG_SCSC_QOS
	platform_mif_qos_init(platform);
#endif
	/* Initialize spinlock */
	spin_lock_init(&platform->mif_spinlock);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
	platform->itmon_nb.notifier_call = wlbt_itmon_notifier;
	itmon_notifier_chain_register(&platform->itmon_nb);
#endif

#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	platform->cfgreq_workq =
			 create_singlethread_workqueue("wlbt_cfg_reg_work");
	if (!platform->cfgreq_workq) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
				 "Error creating CFG_REQ singlethread_workqueue\n");
		err = -ENOMEM;
		goto error_exit;
	}

	INIT_WORK(&platform->cfgreq_wq, platform_cfg_req_wq);
#endif

	return platform_if;

error_exit:
	devm_kfree(&pdev->dev, platform);
	return NULL;
}

void platform_mif_destroy_platform(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
#ifdef CONFIG_SCSC_WLBT_CFG_REQ_WQ
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	destroy_workqueue(platform->cfgreq_workq);
#endif
}

struct platform_device *platform_mif_get_platform_dev(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	BUG_ON(!interface || !platform);

	return platform->pdev;
}

/* Preserve MIF registers during suspend.
 * If all users of the MIF (AP, mx140, CP, etc) release it, the registers
 * will lose their values. Save the useful subset here.
 *
 * Assumption: the AP will not change the register values between the suspend
 * and resume handlers being called!
 */
static void platform_mif_reg_save(struct platform_mif *platform)
{
	platform->mif_preserve.irq_bit_mask = __platform_mif_irq_bit_mask_read(platform);
	platform->mif_preserve.irq_bit_mask_wpan = __platform_mif_irq_bit_mask_read_wpan(platform);
}

/* Restore MIF registers that may have been lost during suspend */
static void platform_mif_reg_restore(struct platform_mif *platform)
{
	__platform_mif_irq_bit_mask_write(platform, platform->mif_preserve.irq_bit_mask);
	__platform_mif_irq_bit_mask_write_wpan(platform, platform->mif_preserve.irq_bit_mask_wpan);
}

int platform_mif_suspend(struct scsc_mif_abs *interface)
{
	int r = 0;
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	enable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num);
	enable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num);


	if (platform->suspend_handler)
		r = platform->suspend_handler(interface, platform->suspendresume_data);

	/* Save the MIF registers.
	 * This must be done last as the suspend_handler may use the MIF
	 */
	platform_mif_reg_save(platform);

	return r;
}

void platform_mif_resume(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	s32 ret;

	disable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX].irq_num);
	disable_irq_wake(platform->wlbt_irq[PLATFORM_MIF_MBOX_WPAN].irq_num);

	/* Restore the MIF registers.
	 * This must be done first as the resume_handler may use the MIF.
	 */
	platform_mif_reg_restore(platform);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
			  "Clear WLBT_ACTIVE_CLR flag\n");
	/* Clear WLBT_ACTIVE_CLR flag in WLBT_CTRL_NS */
	ret = regmap_update_bits(platform->pmureg, WLBT_CTRL_NS, WLBT_ACTIVE_CLR, WLBT_ACTIVE_CLR);
	if (ret < 0) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Failed to Set WLBT_CTRL_NS[WLBT_ACTIVE_CLR]: %d\n", ret);
	}

	if (platform->resume_handler)
		platform->resume_handler(interface, platform->suspendresume_data);
}

static void power_supplies_on(struct platform_mif *platform)
{
	/* The APM IPC in FW will be used instead */
	if (disable_apm_setup) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev,
				  "WLBT LDOs firmware controlled\n");
		return;
	}
}
