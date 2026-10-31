/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/printk.h>
#include <linux/cpuidle.h>
#include <soc/samsung/bts.h>
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#include <soc/samsung/exynos-devfreq.h>
#endif

#include "include/npu-config.h"
#include "npu-dtm.h"
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-util-regs.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"
#if IS_ENABLED(CONFIG_NPU_USE_ESCA_DTM)
#include "../../soc/samsung/exynos/esca/esca_ipc.h"

static struct esca_ipc_channel npu_esca_ipc_channel;
#endif

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
bool npu_dtm_get_flag(void)
{
	struct npu_scheduler_info *info;

	info = npu_scheduler_get_info();
	return (info->curr_thermal >= info->dtm_nm_lut[0]) ? TRUE : FALSE;
}

void npu_dtm_set(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;
	int npu_thermal = 0;
	s64 pid_val;
	s64 thermal_err;
	int idx_prev;
	int idx_curr;
	s64 err_sum = 0;
	int *th_err;
	int period = 1;
	int i;
	int qt_freq = 0;

	if (list_empty(&info->ip_list)) {
		npu_err("[PID]no device for scheduler\n");
		return;
	}
	th_err = &info->th_err_db[0];

	period = info->pid_period;
	thermal_zone_get_temp(info->npu_tzd, &npu_thermal);
	info->curr_thermal = npu_thermal;

	//NORMAL MODE Loop
	if ((info->mode == NPU_PERF_MODE_NORMAL ||
			info->mode == NPU_PERF_MODE_NPU_BOOST_ONEXE) &&
			info->pid_en == 0) {

		if (npu_thermal >= info->dtm_nm_lut[0]) {
			qt_freq = npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, info->dtm_nm_lut[1], DIT_CORE), DIT_CORE);
			npu_dbg("NORMAL mode DTM set freq : %d\n", qt_freq);
		} else {
			qt_freq = npu_scheduler_get_clk(info, 0, DIT_CORE);
		}
		if (info->dtm_prev_freq != qt_freq) {
			info->dtm_prev_freq = qt_freq;
			mutex_lock(&info->exec_lock);
			list_for_each_entry(d, &info->ip_list, ip_list) {
				if (!strcmp("NPU", d->name) || !strcmp("DSP", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max, qt_freq);
				}
				if (!strcmp("DNC", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max,
						npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, qt_freq, DIT_CORE), DIT_DNC));
				}
			}
			mutex_unlock(&info->exec_lock);
		}
#ifdef CONFIG_NPU_USE_PI_DTM_DEBUG
		info->debug_log[info->debug_cnt][0] = info->idx_cnt;
		info->debug_log[info->debug_cnt][1] = npu_thermal/1000;
		info->debug_log[info->debug_cnt][2] = info->dtm_prev_freq/1000;
		if (info->debug_cnt < PID_DEBUG_CNT - 1)
			info->debug_cnt += 1;
#endif
	}
	//PID MODE Loop
	else if (info->idx_cnt % period == 0) {
		if (info->pid_target_thermal == NPU_SCH_DEFAULT_VALUE)
			return;

		idx_curr = info->idx_cnt / period;
		idx_prev = (idx_curr - 1) % PID_I_BUF_SIZE;
		idx_curr = (idx_curr) % PID_I_BUF_SIZE;
		thermal_err = (int)info->pid_target_thermal - npu_thermal;

		if (thermal_err < 0)
			thermal_err = (thermal_err * info->pid_inv_gain) / 100;

		th_err[idx_curr] = thermal_err;

		for (i = 0 ; i < PID_I_BUF_SIZE ; i++)
			err_sum += th_err[i];

		pid_val = (s64)(info->pid_p_gain * thermal_err) + (s64)(info->pid_i_gain * err_sum);

		info->dtm_curr_freq += pid_val / 100;	//for int calculation

		if (info->dtm_curr_freq > PID_MAX_FREQ_MARGIN + info->pid_max_clk)
			info->dtm_curr_freq = PID_MAX_FREQ_MARGIN + info->pid_max_clk;

		qt_freq = npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, info->dtm_curr_freq, DIT_CORE), DIT_CORE);

		if (info->dtm_prev_freq != qt_freq) {
			info->dtm_prev_freq = qt_freq;
			mutex_lock(&info->exec_lock);
			list_for_each_entry(d, &info->ip_list, ip_list) {
				if (!strcmp("NPU", d->name) || !strcmp("DSP", d->name)) {
					//npu_dvfs_set_freq(d, &d->qos_req_min, qt_freq);
					npu_dvfs_set_freq(d, &d->qos_req_max, qt_freq);
				}
				if (!strcmp("DNC", d->name)) {
					npu_dvfs_set_freq(d, &d->qos_req_max,
						npu_scheduler_get_clk(info, npu_scheduler_get_lut_idx(info, qt_freq, DIT_CORE), DIT_DNC));
				}
			}
			mutex_unlock(&info->exec_lock);
			npu_dbg("BOOST mode DTM set freq : %d->%d\n", info->dtm_prev_freq, qt_freq);
		}

#ifdef CONFIG_NPU_USE_PI_DTM_DEBUG
		info->debug_log[info->debug_cnt][0] = info->idx_cnt;
		info->debug_log[info->debug_cnt][1] = npu_thermal/1000;
		info->debug_log[info->debug_cnt][2] = info->dtm_curr_freq/1000;
		if (info->debug_cnt < PID_DEBUG_CNT - 1)
			info->debug_cnt += 1;
#endif

	}
	info->idx_cnt = info->idx_cnt + 1;
}
#endif


#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
static unsigned int emode_start;
static unsigned int emode_release;
static bool emode_on = false;
atomic_t npu_dtm_session_cnt;

void npu_dtm_trigger_emode(struct npu_scheduler_info *info)
{
	int npu_thermal = 0;
	u32 session_cnt = 0;
	struct dhcp_table *dhcp_table = info->device->system.dhcp_table;

	thermal_zone_get_temp(info->npu_tzd, &npu_thermal);

	if (npu_thermal >= emode_start) {
		if (!emode_on) {
			dhcp_table->NPU_THERMAL = 0xCAFEC0DE;
			emode_on = true;
			session_cnt = (u32)(atomic_read(&npu_dtm_session_cnt));
			/* 1 model opens 2 sessions, a NPU/DSP session and the preset session.
			* Add 0.6 sec per model (msec) ; add 0.3 sec per session
			* info->device->system.fr_timeout = MIN(10000 + (session_cnt * 300), 18000);
			*/
			info->device->system.fr_timeout = 60000;
			npu_info("Trigger emode themal(%d) session_cnt(%u) fr_timeout(%u)\n", npu_thermal, session_cnt, info->device->system.fr_timeout);
		}
	} else if (npu_thermal < emode_release) {
		if (emode_on) {
			dhcp_table->NPU_THERMAL = 0x0;
			emode_on = false;
			info->device->system.fr_timeout = 10000;
			npu_info("Release emode themal(%d) fr_timeout(%u)\n", npu_thermal, info->device->system.fr_timeout);
		}
	}
}

static ssize_t emode_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "start : %u, release : %u\n", emode_start, emode_release);

	return ret;
}

static ssize_t emode_value_store(
	struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	int ret = 0;
	unsigned int x = 0;
	unsigned int y = 0;


	if (sscanf(buf, "%u %u", &x, &y) > 30000) {
		if (x > 110000 || y > 110000) {
			npu_err("Invalid emode start & release : start(%u), release(%u), please input 30000 ~ 110000\n", x, y);
			ret = -EINVAL;
			goto err_exit;
		}
	}

	emode_start = x;
	emode_release = y;

	ret = len;
err_exit:
	return ret;
}

static DEVICE_ATTR_RW(emode_value);

int npu_dtm_emode_probe(struct npu_device *device)
{
	int ret = 0;

	emode_start = EMODE_START;
	emode_release = EMODE_RELEASE;
	emode_on = false;
	atomic_set(&npu_dtm_session_cnt, 0);

	ret = sysfs_create_file(&device->dev->kobj, &dev_attr_emode_value.attr);
	if (ret) {
		probe_err("sysfs_create_file error : ret = %d\n", ret);
		goto err_exit;
	}

err_exit:
	return ret;

}
#endif

#if IS_ENABLED(CONFIG_NPU_USE_ESCA_DTM)
int npu_dtm_ipc_communicate(enum npu_esca_event data) {
	struct ipc_config ipc_config;
	union npu_esca_mbox_align __mbox_align__;
	if (!npu_esca_ipc_channel.ipc_ch) {
		npu_err("Sending IPC data before ESCA channel is opened\n");
		return -ENODATA;
	}

	/* Send data to ESCA */
	__mbox_align__.mbox.request = data;
	ipc_config.cmd = &__mbox_align__.mbox.request;
	ipc_config.response = true;
	ipc_config.indirection = false;

	npu_info("request npu_esca_mbox(%u) to ipc ch(0x%x)\n",
			data, npu_esca_ipc_channel.ipc_ch);
	esca_ipc_send_data(npu_esca_ipc_channel.ipc_ch, &ipc_config);

	/* Receive data from ESCA */
	npu_info("receive npu_esca_mbox(%u) from ipc ch(0x%x)\n",
			__mbox_align__.mbox.receive, npu_esca_ipc_channel.ipc_ch);

	return 0;
}

int npu_dtm_open(struct npu_device *device) {
	int ret;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;

	/* Step1. Request ESCA channel */
	ret = esca_ipc_request_channel(dev->of_node, NULL,
			&npu_esca_ipc_channel.ipc_ch, &npu_esca_ipc_channel.size);
	if (ret) {
		npu_err("fail(%d) in esca_ipc_request_channel(0x%x)\n",
			ret, npu_esca_ipc_channel.ipc_ch);
		goto err_probe_ipc;
	}

	npu_info("npu_esca_ipc_channel.ipc_ch = 0x%x, npu_esca_ipc_channel.size = %u\n",
			npu_esca_ipc_channel.ipc_ch, npu_esca_ipc_channel.size);

	/* Step2. Send and receive data to/from ESCA */
	ret = npu_dtm_ipc_communicate(NPU_ESCA_START);
	npu_info("esca_nputherm timer set %s\n",
			ret == 0 ? "success" : "fail");

	npu_info("NPU DTM open success\n");
	return ret;

err_probe_ipc:
	npu_err("NPU DTM open failed(%d)\n", ret);
	return ret;
}

int npu_dtm_close(struct npu_device *device) {
	int ret = 0;
	struct npu_system *system = &device->system;
	struct device *dev = &system->pdev->dev;

	ret = npu_dtm_ipc_communicate(NPU_ESCA_END);
	npu_info("esca_nputherm timer delete %s\n",
			ret == 0 ? "success" : "fail");

	ret = esca_ipc_release_channel(dev->of_node, npu_esca_ipc_channel.ipc_ch);
	if (ret)
		npu_err("fail(%d) in esca_ipc_release_channel(0x%x)\n",
			ret, npu_esca_ipc_channel.ipc_ch);

	npu_info("NPU DTM close success\n");
	return ret;
}
#endif
