/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/delay.h>

#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-regs.h"
#include "npu-afm.h"
#include "npu-afm-debug.h"
#include "npu-hw-device.h"
#include "npu-scheduler.h"

#if IS_ENABLED(CONFIG_SOC_S5E9955)
static char *npu_afm_ip_name[] = {
	"DSP",
	"DNC",
	"NPU",
};

static const u32 NPU_AFM_FREQ_LEVEL[][3] = {
		/* DSP DNC	NPU */
/* SOD */       {1200000, 934000, 1200000},
/* OD */        {1200000, 800000, 1152000},
/* NM_SHI */    {1066000, 800000, 1104000},
/* NM_HI */     {1066000, 800000, 1066000},
/* NM_MID */    {934000, 800000, 934000},
/* NM_LO */     {800000, 664000, 800000},
};

static const char NPU_AFM_MODULE_ENABLE[][15] = {
	"afmgnpu0en",
	"afmgnpu1en",
	"afmsnpu0en",
	"afmsnpu1en"
};

static const char NPU_AFM_MODULE_DISABLE[][15] = {
	"afmgnpu0dis",
	"afmgnpu1dis",
	"afmsnpu0dis",
	"afmsnpu1dis"
};

#else // IS_ENABLED(CONFIG_SOC_S5E8855)
static char *npu_afm_ip_name[] = {
	"NPU",
};

static const u32 NPU_AFM_FREQ_LEVEL[][1] = {
	/* NPU */
	{1200000},
	{1066000},
	{666000},
	{333000},
};

static const char NPU_AFM_MODULE_ENABLE[][15] = {
       "afmgnpu0en"
};

static const char NPU_AFM_MODULE_DISABLE[][15] = {
       "afmgnpu0dis"
};

#endif

static inline u32 __get_freq_of_ip(u32 level, const char *ip) {
	for (int i = 0; i < ARRAY_SIZE(npu_afm_ip_name); i++)
		if (!strcmp(ip, npu_afm_ip_name[i]))
			return NPU_AFM_FREQ_LEVEL[level][i];

	npu_err("ip %s is not found in npu_afm_ip_name\n", ip);
	return NPU_AFM_FREQ_LEVEL[level][0];
}

static struct npu_afm_tdc tdc_state[ARRAY_SIZE(NPU_AFM_FREQ_LEVEL)];

static void __npu_afm_work(const char *ip, int freq);

#if IS_ENABLED(CONFIG_NPU_ONE_BUCK)
extern int sub_pmic_afm_update_reg(uint32_t sub_no, uint8_t reg, uint8_t val, uint8_t mask);
extern int sub_pmic_afm_read_reg(uint32_t sub_no, uint8_t reg, uint8_t *val);

static int npu_afm_update_reg(u8 reg, u8 val, u8 mask) {
	return sub_pmic_afm_update_reg(S2SE911_4_REG, reg, val, mask);
}

static int npu_afm_read_reg(u8 reg, u8 *val) {
	return sub_pmic_afm_read_reg(S2SE911_4_REG, reg, val);
	return 0;
}
#endif

static void npu_afm_control_global(struct npu_system *system, int enable)
{
	if (enable) {
		for (int i = 0; i < ARRAY_SIZE(NPU_AFM_MODULE_ENABLE); i++)
			npu_cmd_map(system, NPU_AFM_MODULE_ENABLE[i]);
	} else {
		for (int i = 0; i < ARRAY_SIZE(NPU_AFM_MODULE_DISABLE); i++)
			npu_cmd_map(system, NPU_AFM_MODULE_DISABLE[i]);
	}
}

static u32 npu_afm_check_gnpu0_itr(struct npu_system *system)
{
	return npu_cmd_map(system, "chkgnpu0itr");
}

static void npu_afm_clear_gnpu0_interrupt(struct npu_system *system)
{
	npu_cmd_map(system, "clrgnpu0itr");
}

static void npu_afm_onoff_gnpu0_interrupt(struct npu_system *system, int enable)
{
	if (enable)
		npu_cmd_map(system, "engnpu0itr");
	else
		npu_cmd_map(system, "disgnpu0itr");
}

static void npu_afm_clear_gnpu0_tdc(struct npu_system *system)
{
	npu_cmd_map(system, "clrgnpu0tdc");
}

static void npu_afm_check_gnpu0_tdc(struct npu_system *system)
{
	u32 level = 0;
	struct npu_afm *npu_afm = system->afm;

	if (npu_afm->afm_mode == NPU_AFM_MODE_TDC) {
		level = npu_afm->afm_max_lock_level[GNPU0];
		if (level == (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1))
			goto done;

		tdc_state[level].gnpu0_cnt += (u32)npu_cmd_map(system, "chkgnpu0tdc");
	}
done:
	return;
}

static irqreturn_t npu_afm_isr0(int irq, void *data)
{
	struct npu_system *system = (struct npu_system *)data;
	struct npu_afm *afm = system->afm;
	if (atomic_read(&(afm->ocp_warn_status[GNPU0])) == NPU_AFM_OCP_WARN)
		goto done;

	atomic_set(&(afm->ocp_warn_status[GNPU0]), NPU_AFM_OCP_WARN);
	npu_afm_onoff_gnpu0_interrupt(system, NPU_AFM_DISABLE);
	npu_afm_clear_gnpu0_interrupt(system);
	npu_afm_check_gnpu0_tdc(system);
	npu_afm_clear_gnpu0_tdc(system);

	queue_delayed_work(afm->afm_gnpu0_wq,
			&afm->afm_gnpu0_work,
			msecs_to_jiffies(0));
done:
	return IRQ_HANDLED;
}

static irqreturn_t (*afm_isr_list[])(int, void *) = {
	npu_afm_isr0,
};

static u32 npu_afm_get_next_level_normal(struct npu_afm *afm,
		int next)
{
	u32 level = 0;

	level = afm->afm_max_lock_level[GNPU0];
	if (next == NPU_AFM_DEC_FREQ) {
		if (level < (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1))
			level += 1;
	} else {/* next == NPU_AFM_INC_FREQ */
		if (level)
			level -= 1;
	}
	afm->afm_max_lock_level[GNPU0] = level;

	npu_info("next freq is %s (level %u)\n", (next == NPU_AFM_DEC_FREQ) ? "DECREASE" : "INCREASE", level);
	return level;
}

static u32 npu_afm_get_next_level_tdc(struct npu_afm *afm,
		int next)
{
	u32 level = 0;

	level = afm->afm_max_lock_level[GNPU0];
	if (next == NPU_AFM_DEC_FREQ) {
		while (level < (ARRAY_SIZE(NPU_AFM_FREQ_LEVEL) - 1)) {
			level += 1;
			if (tdc_state[level].gnpu0_cnt < afm->npu_afm_tdc_threshold)
				break;
		}
	} else {/* next == NPU_AFM_INC_FREQ */
		while (level > 1) { // level 0 is need?
			level -= 1;
			if (tdc_state[level].gnpu0_cnt < afm->npu_afm_tdc_threshold)
				break;
		}
	}
	afm->afm_max_lock_level[GNPU0] = level;

	npu_info("next freq is %s (level %u)(tdc count %u)\n", (next == NPU_AFM_DEC_FREQ) ? "DECREASE" : "INCREASE",
			level, tdc_state[level].gnpu0_cnt);

	return level;
}

static u32 npu_afm_get_next_level(struct npu_afm *afm,
		int next)
{
	u32 level = 0;

	if (afm->afm_mode == NPU_AFM_MODE_NORMAL) {
		level = npu_afm_get_next_level_normal(afm, next);
	} else { // afm->afm_mode == NPU_AFM_MODE_TDC
		level = npu_afm_get_next_level_tdc(afm, next);
	}

	return level;
}

static void __npu_afm_work(const char *ip, int freq)
{
#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp(ip, d->name)) {
			npu_dvfs_set_freq(d, &d->qos_req_max_afm, freq);
		}
	}
	mutex_unlock(&info->exec_lock);
#endif
	return;
}

static void npu_afm_restore_gnpu0_work(struct work_struct *work)
{
	u32 next_level = 0;
	u32 next_freq = 0;
	struct npu_afm *afm = container_of(to_delayed_work(work),
					struct npu_afm, afm_restore_gnpu0_work);
	struct npu_system *system = afm->system;

	if (atomic_read(&(afm->afm_state)) == NPU_AFM_STATE_CLOSE) {
		npu_info("Now afm is closed. So terminate work(%s).\n", __func__);
		return;
	}

	if (atomic_read(&(afm->restore_status[GNPU0])) == NPU_AFM_OCP_WARN)
		goto done;

	atomic_set(&(afm->restore_status[GNPU0]), NPU_AFM_OCP_WARN);

	if (npu_afm_check_gnpu0_itr(system)) {
		goto done;
	}

	next_level = npu_afm_get_next_level(afm, NPU_AFM_INC_FREQ);

	for (int i = ARRAY_SIZE(npu_afm_ip_name) - 1; i >= 0; i--) {
		next_freq = __get_freq_of_ip(next_level, npu_afm_ip_name[i]);
		npu_info("%s next : %u, max : %u\n",
			npu_afm_ip_name[i], next_freq, get_pm_qos_max(npu_afm_ip_name[i]));
		__npu_afm_work(npu_afm_ip_name[i], next_freq);
	}

	npu_info("Trying restore freq.\n");
	queue_delayed_work(afm->afm_restore_gnpu0_wq,
			&afm->afm_restore_gnpu0_work,
			msecs_to_jiffies(afm->npu_afm_restore_msec));

	atomic_set(&(afm->restore_status[GNPU0]), NPU_AFM_OCP_NONE);
done:
	return;
}

static void npu_afm_gnpu0_work(struct work_struct *work)
{
	u32 next_level = 0;
	u32 next_freq = 0;
	struct npu_afm *afm = container_of(to_delayed_work(work),
					struct npu_afm, afm_gnpu0_work);
	struct npu_system *system = afm->system;

	if (atomic_read(&(afm->afm_state)) == NPU_AFM_STATE_CLOSE) {
		npu_info("Now afm is closed. So terminate work(%s).\n", __func__);
		return;
	}

	npu_info("npu_afm_gnpu0_work start\n");

	next_level = npu_afm_get_next_level(afm, NPU_AFM_DEC_FREQ);

	for (int i = 0; i < ARRAY_SIZE(npu_afm_ip_name); i++) {
		next_freq = __get_freq_of_ip(next_level, npu_afm_ip_name[i]);
		npu_info("%s next : %u, max : %u\n",
			npu_afm_ip_name[i], next_freq, get_pm_qos_max(npu_afm_ip_name[i]));
		__npu_afm_work(npu_afm_ip_name[i], next_freq);
	}


	queue_delayed_work(afm->afm_restore_gnpu0_wq,
			&afm->afm_restore_gnpu0_work,
			msecs_to_jiffies(1000));

	npu_afm_onoff_gnpu0_interrupt(system, NPU_AFM_ENABLE);

	atomic_set(&(afm->ocp_warn_status[GNPU0]), NPU_AFM_OCP_NONE);

	npu_info("npu_afm_gnpu0_work end\n");
}

static void npu_afm_open_setting_sfr(struct npu_system *system, bool power_on) {
	if (power_on) {
		npu_afm_control_global(system, NPU_AFM_ENABLE);
		npu_afm_onoff_gnpu0_interrupt(system, NPU_AFM_ENABLE);
	}
}

static void npu_afm_close_setting_sfr(struct npu_system *system, bool power_on) {
	npu_afm_check_gnpu0_tdc(system);
	npu_afm_clear_gnpu0_tdc(system);

	if (power_on) {
		npu_afm_control_global(system, NPU_AFM_DISABLE);
		npu_afm_onoff_gnpu0_interrupt(system, NPU_AFM_DISABLE);
		npu_afm_clear_gnpu0_interrupt(system);
		for (int i = ARRAY_SIZE(npu_afm_ip_name) - 1; i >= 0; i--)
			__npu_afm_work(npu_afm_ip_name[i], __get_freq_of_ip(0, npu_afm_ip_name[i]));
	}
}

void npu_afm_open(struct npu_system *system, int hid)
{
	struct npu_afm *afm = system->afm;
	#if IS_ENABLED(CONFIG_NPU_ONE_BUCK)
	int ret = 0;
	int buck = 0;
	u8 enable_value = 0;

	ret = npu_afm_read_reg(
				afm->afm_buck_threshold_enable_offset[buck],
				&enable_value);
	if (ret) {
		npu_err("sub pmic read failed : %d\n", ret);
		return;
	}
	#endif

	npu_afm_control_global(system, NPU_AFM_ENABLE);
	npu_afm_onoff_gnpu0_interrupt(system, NPU_AFM_ENABLE);

	#if IS_ENABLED(CONFIG_NPU_ONE_BUCK)
	if (afm->afm_local_onoff) {
		npu_afm_update_reg(afm->afm_buck_threshold_enable_offset[buck],
				S2MPS_AFM_WARN_EN,
				S2MPS_AFM_WARN_EN);

		npu_afm_update_reg(afm->afm_buck_threshold_level_offset[buck],
				afm->afm_buck_threshold_level_value[buck],
				S2MPS_AFM_WARN_LVL_MASK);

		npu_afm_read_reg(
				afm->afm_buck_threshold_enable_offset[buck],
				&enable_value);

		npu_info("Enable SUB PMIC AFM enable offset: 0x%x, enable value: 0x%x, level offset: 0x%x, level value: 0x%x\n",
				afm->afm_buck_threshold_enable_offset[buck],
				enable_value,
				afm->afm_buck_threshold_level_offset[buck],
				afm->afm_buck_threshold_level_value[buck]);
	}
	#endif

	/* for S5E9955, all gnpu0/1 and snpu0/1 are on the one same buck. */
	for (int i = 0; i < BUCK_CNT; i++) {
		afm->afm_max_lock_level[i] = 0;
		atomic_set(&(afm->ocp_warn_status[i]), NPU_AFM_OCP_NONE);
		atomic_set(&(afm->restore_status[i]), NPU_AFM_OCP_NONE);
	}

	for (int i = 0; i < ARRAY_SIZE(NPU_AFM_FREQ_LEVEL); i++) {
		tdc_state[i].gnpu0_cnt = 0;
	}

	if (afm->npu_afm_irp_debug)
		npu_afm_debug_set_irp(system);
	if (afm->npu_afm_tdt_debug)
		npu_afm_debug_set_tdt(system);

	atomic_set(&(afm->afm_state), NPU_AFM_STATE_OPEN);

	npu_info("open success, hid : %d\n", hid);
}

void npu_afm_close(struct npu_system *system, int hid)
{
	struct npu_afm *afm = system->afm;
	#if IS_ENABLED(CONFIG_NPU_ONE_BUCK)
	int buck = 0;
	u8 enable_value;
	#endif

	npu_afm_control_global(system, NPU_AFM_DISABLE);

	#if IS_ENABLED(CONFIG_NPU_ONE_BUCK)
	if (afm->afm_local_onoff) {
		npu_afm_update_reg(afm->afm_buck_threshold_enable_offset[buck],
				(~S2MPS_AFM_WARN_EN & S2MPS_AFM_WARN_EN),
				S2MPS_AFM_WARN_EN);

		npu_afm_read_reg(
				afm->afm_buck_threshold_enable_offset[buck],
				&enable_value);

		npu_info("Disable SUB PMIC AFM enable offset: 0x%x, enable value: 0x%x\n",
				afm->afm_buck_threshold_enable_offset[buck],
				enable_value);
	}
	#endif

	cancel_delayed_work_sync(&afm->afm_gnpu0_work);
	cancel_delayed_work_sync(&afm->afm_restore_gnpu0_work);

	for (int i = ARRAY_SIZE(npu_afm_ip_name) - 1; i >= 0; i--)
		__npu_afm_work(npu_afm_ip_name[i], __get_freq_of_ip(0, npu_afm_ip_name[i]));

	atomic_set(&(afm->afm_state), NPU_AFM_STATE_CLOSE);

	npu_info("close success, hid : %d\n", hid);
}

static void npu_afm_dt_parsing(struct npu_system *system) {
	int ret = 0;
	char name[NPU_AFM_STR_LEN];

	struct device *dev = &system->pdev->dev;
	struct npu_afm *afm = system->afm;

	for (int i = 0; i < BUCK_CNT; i++) {
		name[0] = '\0';
		strncpy(name, "samsung,npuafm-threshold", NPU_AFM_STR_LEN);
		strncat(name, "-enable-offset", NPU_AFM_STR_LEN);

		ret = of_property_read_u32(dev->of_node, name, &(afm->afm_buck_threshold_enable_offset[i]));
		if (ret)
			afm->afm_buck_threshold_enable_offset[i] = false;

		name[0] = '\0';
		strncpy(name, "samsung,npuafm-threshold", NPU_AFM_STR_LEN);
		strncat(name, "-level-offset", NPU_AFM_STR_LEN);

		ret = of_property_read_u32(dev->of_node, name, &(afm->afm_buck_threshold_level_offset[i]));
		if (ret)
			afm->afm_buck_threshold_level_offset[i] = false;

		name[0] = '\0';
		strncpy(name, "samsung,npuafm-threshold", NPU_AFM_STR_LEN);
		strncat(name, "-level-value", NPU_AFM_STR_LEN);

		ret = of_property_read_u32(dev->of_node, name, &(afm->afm_buck_threshold_level_value[i]));
		if (ret)
			afm->afm_buck_threshold_level_value[i] = S2MPS_AFM_WARN_DEFAULT_LVL;

		probe_info("buck[%d] (enable offset: 0x%x, level offset: 0x%x, level value: 0x%x)\n",
				i,
				afm->afm_buck_threshold_enable_offset[i],
				afm->afm_buck_threshold_level_offset[i],
				afm->afm_buck_threshold_level_value[i]);
	}
}

static const struct npu_afm_ops n_npu_afm_ops = {
	.afm_open_setting_sfr = npu_afm_open_setting_sfr,
	.afm_close_setting_sfr = npu_afm_close_setting_sfr,
};

int npu_afm_probe(struct npu_device *device)
{
	int i, ret = 0, afm_irq_idx = 0;
	const char *buf;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;
	struct cpumask cpu_mask;
	struct npu_afm *afm;

	afm = devm_kzalloc(dev, sizeof(struct npu_afm), GFP_KERNEL);
	if (!afm) {
		ret = -ENOMEM;
		goto err_probe_irq;
	}

	system->afm = afm;
	afm->system = system;
	afm->afm_ops = &n_npu_afm_ops;

	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++, afm_irq_idx++) {
		ret = devm_request_irq(dev, system->irq[i], afm_isr_list[afm_irq_idx],
					IRQF_TRIGGER_HIGH, "exynos-npu", system);
		if (ret)
			probe_err("fail(%d) in devm_request_irq(%d)\n", ret, i);
	}

	ret = of_property_read_string(dev->of_node, "samsung,npuinter-isr-cpu-affinity", &buf);
	if (ret) {
		probe_info("AFM set the CPU affinity of ISR to 5\n");
		cpumask_set_cpu(5, &cpu_mask);
	}	else {
		probe_info("AFM set the CPU affinity of ISR to %s\n", buf);
		cpulist_parse(buf, &cpu_mask);
	}

	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++) {
		ret = irq_set_affinity_hint(system->irq[i], &cpu_mask);
		if (ret) {
			probe_err("fail(%d) in irq_set_affinity_hint(%d)\n", ret, i);
			goto err_probe_irq;
		}
	}

	INIT_DELAYED_WORK(&afm->afm_gnpu0_work, npu_afm_gnpu0_work);
	INIT_DELAYED_WORK(&afm->afm_restore_gnpu0_work, npu_afm_restore_gnpu0_work);

	afm->afm_gnpu0_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!afm->afm_gnpu0_wq) {
		probe_err("fail to create workqueue -> afm->afm_gnpu0_wq\n");
		ret = -EFAULT;
		goto err_probe;
	}

	afm->afm_restore_gnpu0_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!afm->afm_restore_gnpu0_wq) {
		probe_err("fail to create workqueue -> afm->afm_restore_gnpu0_wq\n");
		ret = -EFAULT;
		goto err_probe;
	}

#if IS_ENABLED(CONFIG_DEBUG_FS)
	ret = npu_afm_register_debug_sysfs(system);
	if (ret)
		probe_err("fail(%d) in npu_afm_register_debug_sysfs()\n", ret);
#endif

	npu_afm_dt_parsing(system);
	afm->afm_mode = NPU_AFM_MODE_NORMAL;
	afm->afm_local_onoff = false;
	afm->npu_afm_irp_debug = 0;
	afm->npu_afm_tdc_threshold = NPU_AFM_TDC_THRESHOLD;
	afm->npu_afm_restore_msec = NPU_AFM_RESTORE_MSEC;
	afm->npu_afm_tdt_debug = 0;
	afm->npu_afm_tdt = NPU_AFM_TDT;

	probe_info("NPU AFM probe success\n");
	return ret;
err_probe:
	if (afm)
		devm_kfree(dev, afm);
err_probe_irq:
	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}

	probe_err("NPU AFM probe failed(%d)\n", ret);
	return ret;
}

int npu_afm_release(struct npu_device *device)
{
	int i, ret = 0;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;
	struct npu_afm *afm = system->afm;

	for (i = AFM_IRQ_INDEX; i < (AFM_IRQ_INDEX + NPU_AFM_IRQ_CNT); i++) {
		irq_set_affinity_hint(system->irq[i], NULL);
		devm_free_irq(dev, system->irq[i], NULL);
	}

	if (afm)
		devm_kfree(dev, afm);

	system->afm = NULL;

	probe_info("NPU AFM release success\n");
	return ret;
}
