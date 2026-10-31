/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/version.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/pm_opp.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "include/npu-preset.h"
#include "npu-device.h"
#include "include/npu-memory.h"
#include "npu-system.h"
#include "npu-dvfs.h"
#include "npu-qos.h"
#include "npu-dtm.h"
#include "npu-state.h"
#include "dsp-dhcp.h"

static struct npu_qos_setting *qos_setting;
static LIST_HEAD(qos_list);

static struct npu_qos_freq_lock qos_lock;

static s32 __update_freq_from_showcase(__u32 nCategory);
static ssize_t npu_show_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static int npu_qos_sysfs_create(struct npu_system *system);

static void __npu_pm_qos_add_notifier(int exynos_pm_qos_class,
					struct notifier_block *notifier)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	exynos_pm_qos_add_notifier(exynos_pm_qos_class, notifier);
#endif
}

int __req_param_qos(int uid, __u32 nCategory, struct exynos_pm_qos_request *req, s32 new_value)
{
	int ret = 0;
	s32 cur_value, rec_value;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;


	//Check that same uid, and category whether already registered.
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if ((qr->sessionUID == uid) && (qr->eCategory == nCategory)) {
			switch (nCategory) {
			case NPU_S_PARAM_QOS_MO_SCEN_ID_PRESET:
				cur_value = qr->req_mo_scen;
				npu_dbg("[U%u]Change Req MO scen. category : %u, from mo scen : %d to %d\n",
						uid, nCategory, cur_value, new_value);
				list_del(pos);

				qr->sessionUID = uid;
				qr->req_mo_scen = new_value;
				qr->eCategory = nCategory;
				list_add_tail(&qr->list, &qos_list);
				bts_del_scenario(cur_value);
				bts_add_scenario(qr->req_mo_scen);
				return ret;
			default:
				cur_value = qr->req_freq;
				npu_dbg("[U%u]Change Req Freq. category : %u, from freq : %d to %d\n",
						uid, nCategory, cur_value, new_value);
				list_del(pos);
				qr->sessionUID = uid;
				qr->req_freq = new_value;
				qr->eCategory = nCategory;
				list_add_tail(&qr->list, &qos_list);

				rec_value = __update_freq_from_showcase(nCategory);

				if (new_value > rec_value) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
					exynos_pm_qos_update_request(req, new_value);
#endif
					npu_dbg("[U%u]Changed Freq. category : %u, from freq : %d to %d\n",
							uid, nCategory, cur_value, new_value);
				} else {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
					exynos_pm_qos_update_request(req, rec_value);
#endif
					npu_dbg("[U%u]Recovered Freq. category : %u, from freq : %d to %d\n",
							uid, nCategory, cur_value, rec_value);
				}
				return ret;
			}
		}
	}

	//No Same uid, and category. Add new item
	qr = kmalloc(sizeof(struct npu_session_qos_req), GFP_KERNEL);
	if (!qr)
		return -ENOMEM;

	switch (nCategory) {
	case NPU_S_PARAM_QOS_MO_SCEN_ID_PRESET:
		qr->sessionUID = uid;
		qr->req_mo_scen = new_value;
		qr->eCategory = nCategory;
		list_add_tail(&qr->list, &qos_list);
		bts_add_scenario(qr->req_mo_scen);
		return ret;
	default:
		qr->sessionUID = uid;
		qr->req_freq = new_value;
		qr->eCategory = nCategory;
		list_add_tail(&qr->list, &qos_list);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
		//If new_value is lager than current value, update the freq
		cur_value = (s32)exynos_pm_qos_read_req_value(req->exynos_pm_qos_class, req);
		npu_dbg("[U%u]New Freq. category : %u freq : %u\n",
				qr->sessionUID, qr->eCategory, qr->req_freq);
		if (cur_value != new_value) {
			npu_dbg("[U%u]Update Freq. category : %u freq : %u\n",
					qr->sessionUID, qr->eCategory, qr->req_freq);
			exynos_pm_qos_update_request(req, new_value);
		}
#endif
		return ret;
	}
}

static int npu_qos_freq_qos_add_request(void)
{
	if (!qos_setting)
		return -EINVAL;

#if IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	return 0;
#endif

	if (!qos_setting->cl0_policy) {
		qos_setting->cl0_policy = cpufreq_cpu_get(0);
		if (qos_setting->cl0_policy) {
			freq_qos_tracer_add_request(&qos_setting->cl0_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl0, FREQ_QOS_MIN, 0);
			freq_qos_tracer_add_request(&qos_setting->cl0_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl0_max, FREQ_QOS_MAX,
					PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE);
		}
	}

	if (!qos_setting->cl1_policy) {
#if IS_ENABLED(CONFIG_SOC_S5E8855)
		qos_setting->cl1_policy = cpufreq_cpu_get(4);
#else
		qos_setting->cl1_policy = cpufreq_cpu_get(2);
#endif
		if (qos_setting->cl1_policy) {
			freq_qos_tracer_add_request(&qos_setting->cl1_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl1, FREQ_QOS_MIN, 0);
			freq_qos_tracer_add_request(&qos_setting->cl1_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl1_max, FREQ_QOS_MAX,
					PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
		}
	}
	if (!qos_setting->cl2_policy) {
		qos_setting->cl2_policy = cpufreq_cpu_get(7);
		if (qos_setting->cl2_policy) {
			freq_qos_tracer_add_request(&qos_setting->cl2_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl2, FREQ_QOS_MIN, 0);
			freq_qos_tracer_add_request(&qos_setting->cl2_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl2_max, FREQ_QOS_MAX,
					PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
		}
	}
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	if (!qos_setting->cl3_policy) {
		qos_setting->cl3_policy = cpufreq_cpu_get(9);
		if (qos_setting->cl3_policy) {
			freq_qos_tracer_add_request(&qos_setting->cl3_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl3, FREQ_QOS_MIN, 0);
			freq_qos_tracer_add_request(&qos_setting->cl3_policy->constraints,
					&qos_setting->npu_qos_req_cpu_cl3_max, FREQ_QOS_MAX,
					INT_MAX);
		}
	}
#endif
	return 0;
}

int __req_param_qos_cpu(int uid, __u32 nCategory, struct freq_qos_request *req, s32 new_value)
{
	int ret = 0;
	s32 cur_value, rec_value;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	npu_qos_freq_qos_add_request();

	//Check that same uid, and category whether already registered.
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if ((qr->sessionUID == uid) && (qr->eCategory == nCategory)) {
			cur_value = qr->req_freq;
			npu_dbg("[U%u]Change Req Freq. category : %u, from freq : %d to %d\n",
					uid, nCategory, cur_value, new_value);
			list_del(pos);
			qr->sessionUID = uid;
			qr->req_freq = new_value;
			qr->eCategory = nCategory;
			list_add_tail(&qr->list, &qos_list);
			rec_value = __update_freq_from_showcase(nCategory);

			if (new_value > rec_value) {
				freq_qos_update_request(req, new_value);
				npu_dbg("[U%u]Changed Freq. category : %u, from freq : %d to %d\n",
						uid, nCategory, cur_value, new_value);
			} else {
				freq_qos_update_request(req, rec_value);
				npu_dbg("[U%u]Recovered Freq. category : %u, from freq : %d to %d\n",
						uid, nCategory, cur_value, rec_value);
			}
			return ret;
		}
	}
	//No Same uid, and category. Add new item
	qr = kmalloc(sizeof(struct npu_session_qos_req), GFP_KERNEL);
	if (!qr)
		return -ENOMEM;

	qr->sessionUID = uid;
	qr->req_freq = new_value;
	qr->eCategory = nCategory;
	list_add_tail(&qr->list, &qos_list);

	npu_dbg("[U%u]Update Freq. category : %u freq : %u\n",
			qr->sessionUID, qr->eCategory, qr->req_freq);
	freq_qos_update_request(req, new_value);
	return ret;
}

static int npu_qos_max_notifier(struct notifier_block *nb,
		unsigned long freq, void *nb_data)
{
	/* skip max_notifier in boost mode */
	if (is_kpi_mode_enabled(true)) {
		npu_dbg("skip max_notifier for boost\n");
		return NOTIFY_DONE;
	}

	if (qos_setting->skip_max_noti) {
		npu_dbg("skip max_notifier for s_param max\n");
		qos_setting->skip_max_noti = false;
		return NOTIFY_DONE;
	}

	npu_dbg("NPU maxlock at %ld\n", freq);
	return NOTIFY_DONE;
}

int npu_qos_probe(struct npu_system *system)
{
	qos_setting = &(system->qos_setting);

	if (qos_setting->info == NULL) {
		probe_err("info is NULL!\n");
		return 0;
	}

	mutex_init(&qos_setting->npu_qos_lock);


	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_npu,
			PM_QOS_NPU_THROUGHPUT, 0);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_dsp,
			PM_QOS_DSP_THROUGHPUT, 0);
#endif
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_dnc,
			PM_QOS_DNC_THROUGHPUT, 0);
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_mif,
			PM_QOS_BUS_THROUGHPUT, 0);
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_int,
			PM_QOS_DEVICE_THROUGHPUT, 0);
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_gpu,
			PM_QOS_GPU_THROUGHPUT_MIN, 0);

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	dsufreq_qos_add_request("sysfs", &qos_setting->dsu_min_req,
		DEV_PM_QOS_MIN_FREQUENCY, 0);
	dsufreq_qos_add_request("sysfs", &qos_setting->dsu_max_req,
		DEV_PM_QOS_MAX_FREQUENCY, NPU_QOS_DSU_MAX_FREQ);
#endif

	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_npu_max,
			PM_QOS_NPU_THROUGHPUT_MAX,
			PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_dsp_max,
			PM_QOS_DSP_THROUGHPUT_MAX,
			PM_QOS_DSP_THROUGHPUT_MAX_DEFAULT_VALUE);
#endif
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_dnc_max,
			PM_QOS_DNC_THROUGHPUT_MAX,
			PM_QOS_DNC_THROUGHPUT_MAX_DEFAULT_VALUE);
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_mif_max,
			PM_QOS_BUS_THROUGHPUT_MAX,
			PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_int_max,
			PM_QOS_DEVICE_THROUGHPUT_MAX,
			PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE);
	npu_dvfs_pm_qos_add_request(&qos_setting->npu_qos_req_gpu_max,
			PM_QOS_GPU_THROUGHPUT_MAX,
			PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE);

	npu_qos_freq_qos_add_request();

	qos_setting->req_npu_freq = 0;
#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	qos_setting->req_dsp_freq = 0;
#endif
	qos_setting->req_dnc_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_gpu_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	qos_setting->req_cl3_freq = 0;
#endif
	qos_setting->req_mo_scen = 0;
	qos_setting->req_cpu_aff = 0;


	qos_lock.npu_freq_maxlock = PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE;
	qos_lock.dnc_freq_maxlock = PM_QOS_DNC_THROUGHPUT_MAX_DEFAULT_VALUE;
	qos_setting->npu_qos_max_nb.notifier_call = npu_qos_max_notifier;
	qos_setting->skip_max_noti = false;

	__npu_pm_qos_add_notifier(PM_QOS_NPU_THROUGHPUT_MAX, &qos_setting->npu_qos_max_nb);

	if (npu_qos_sysfs_create(system)) {
		probe_info("npu_qos_sysfs create failed\n");
		return -1;
	}

	if (npu_state_sysfs_create(system)) {
		probe_info("npu_state_sysfs create failed\n");
		return -1;
	}

	return 0;
}

int npu_qos_release(struct npu_system *system)
{
	return 0;
}

int npu_qos_open(struct npu_system *system)
{

	mutex_lock(&qos_setting->npu_qos_lock);

	qos_setting->req_npu_freq = 0;
	qos_setting->req_dnc_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_gpu_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	qos_setting->req_cl3_freq = 0;
#endif
	qos_setting->skip_max_noti = false;

	mutex_unlock(&qos_setting->npu_qos_lock);


	return 0;
}

int npu_qos_close(struct npu_system *system)
{
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	mutex_lock(&qos_setting->npu_qos_lock);

	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		switch (qr->eCategory) {
		case NPU_S_PARAM_QOS_MO_SCEN_ID_PRESET:
			bts_del_scenario(qr->req_mo_scen);
			break;
		default:
			break;
		}

		list_del(pos);
		if (qr)
			kfree(qr);
	}
	list_del_init(&qos_list);


	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_npu, 0);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_dsp, 0);
#endif
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_dnc, 0);
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_mif, 0);
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_int, 0);
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_gpu, 0);

	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_npu_max,
			PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_dsp_max,
			PM_QOS_DSP_THROUGHPUT_MAX_DEFAULT_VALUE);
#endif
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_dnc_max,
			PM_QOS_DNC_THROUGHPUT_MAX_DEFAULT_VALUE);
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_mif_max,
			PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE);
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_int_max,
			PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE);
	npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_gpu_max,
			PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE);

	npu_qos_freq_qos_add_request();

	if (!IS_ERR_OR_NULL(qos_setting->npu_qos_req_cpu_cl0.qos)) {
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl0, 0);
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl0_max,
			PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE);
	}
	if (!IS_ERR_OR_NULL(qos_setting->npu_qos_req_cpu_cl1.qos)) {
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl1, 0);
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl1_max,
			PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE);
	}

	if (!IS_ERR_OR_NULL(qos_setting->npu_qos_req_cpu_cl2.qos)) {
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl2, 0);
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl2_max,
			PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE);
	}
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	if (!IS_ERR_OR_NULL(qos_setting->npu_qos_req_cpu_cl3.qos)) {
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl3, 0);
		freq_qos_update_request(&qos_setting->npu_qos_req_cpu_cl3_max,
			INT_MAX);
	}
#endif

	qos_setting->req_npu_freq = 0;
	qos_setting->req_dnc_freq = 0;
	qos_setting->req_int_freq = 0;
	qos_setting->req_gpu_freq = 0;
	qos_setting->req_mif_freq = 0;
	qos_setting->req_cl0_freq = 0;
	qos_setting->req_cl1_freq = 0;
	qos_setting->req_cl2_freq = 0;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	qos_setting->req_cl3_freq = 0;
#endif

	qos_lock.npu_freq_maxlock = PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE;
	qos_setting->skip_max_noti = false;

	mutex_unlock(&qos_setting->npu_qos_lock);

	return 0;
}

static s32 __update_freq_from_showcase(__u32 nCategory)
{
	s32 nValue = 0;
	struct list_head *pos, *q;
	struct npu_session_qos_req *qr;

	qr = NULL;
	list_for_each_safe(pos, q, &qos_list) {
		qr = list_entry(pos, struct npu_session_qos_req, list);
		if (qr->eCategory == nCategory) {
			nValue = qr->req_freq > nValue ? qr->req_freq : nValue;
		}
	}

	return nValue;
}

static bool npu_qos_preset_is_valid_value(int value)
{
	if (value == NPU_QOS_DEFAULT_VALUE)
		return true;

	if (value >= 0)
		return true;

	return false;
}

static s32 npu_qos_preset_get_req_value(int value)
{
	if (value == NPU_QOS_DEFAULT_VALUE)
		return 0;
	else
		return value;
}

npu_s_param_ret npu_qos_param_handler(struct npu_session *sess, struct vs4l_param *param)
{
	struct npu_vertex_ctx vctx = sess->vctx;
	struct npu_device *npu_device = container_of(vctx.vertex, struct npu_device, vertex);
	struct npu_system *npu_system = &npu_device->system;
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	struct dhcp_table *dhcp_table = npu_system->dhcp_table;
#endif
	int ret, i;
	unsigned long tmp[] = {0};

#if IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	return 0;
#endif
	if (!qos_setting)
		return S_PARAM_NOMB;

	mutex_lock(&qos_setting->npu_qos_lock);

	switch (param->target) {
	case NPU_S_PARAM_QOS_NPU:
		qos_setting->req_npu_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu,
				qos_setting->req_npu_freq);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
		dhcp_table->PRESET_NPU_FREQ = qos_setting->req_npu_freq;
#endif
		break;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	case NPU_S_PARAM_QOS_DSP:
		qos_setting->req_dsp_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_dsp,
				qos_setting->req_dsp_freq);
		break;
#endif

	case NPU_S_PARAM_QOS_DNC:
		qos_setting->req_dnc_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_dnc,
					qos_setting->req_dnc_freq);
		break;

	case NPU_S_PARAM_QOS_MIF:
		qos_setting->req_mif_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_mif,
				qos_setting->req_mif_freq);
		break;

	case NPU_S_PARAM_QOS_INT:
		qos_setting->req_int_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_int,
				qos_setting->req_int_freq);
		break;

	case NPU_S_PARAM_QOS_NPU_MAX:
		qos_setting->skip_max_noti = true;
		qos_setting->req_npu_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu_max,
				qos_setting->req_npu_freq);
		break;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	case NPU_S_PARAM_QOS_DSP_MAX:
		qos_setting->req_dsp_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_DSP_THROUGHPUT_MAX_DEFAULT_VALUE;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_dsp_max,
				qos_setting->req_dsp_freq);
		break;
#endif

	case NPU_S_PARAM_QOS_DNC_MAX:
		qos_setting->req_dnc_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_DNC_THROUGHPUT_MAX_DEFAULT_VALUE;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_dnc_max,
					qos_setting->req_dnc_freq);
		break;

	case NPU_S_PARAM_QOS_MIF_MAX:
		qos_setting->req_mif_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_mif_max,
				qos_setting->req_mif_freq);
		break;

	case NPU_S_PARAM_QOS_INT_MAX:
		qos_setting->req_int_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_int_max,
				qos_setting->req_int_freq);
		break;

	case NPU_S_PARAM_QOS_CL0:
		qos_setting->req_cl0_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0,
				qos_setting->req_cl0_freq);
		break;

	case NPU_S_PARAM_QOS_CL1:
		qos_setting->req_cl1_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1,
				qos_setting->req_cl1_freq);
		break;

	case NPU_S_PARAM_QOS_CL2:
		qos_setting->req_cl2_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2,
				qos_setting->req_cl2_freq);
		break;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	case NPU_S_PARAM_QOS_CL3:
		qos_setting->req_cl3_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl3,
				qos_setting->req_cl3_freq);
		break;
#endif

	case NPU_S_PARAM_QOS_CL0_MAX:
		qos_setting->req_cl0_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0_max,
				qos_setting->req_cl0_freq);
		break;

	case NPU_S_PARAM_QOS_CL1_MAX:
		qos_setting->req_cl1_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1_max,
				qos_setting->req_cl1_freq);
		break;

	case NPU_S_PARAM_QOS_CL2_MAX:
		qos_setting->req_cl2_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2_max,
				qos_setting->req_cl2_freq);
		break;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	case NPU_S_PARAM_QOS_CL3_MAX:
		qos_setting->req_cl3_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : INT_MAX;
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl3_max,
				qos_setting->req_cl3_freq);
		break;
#endif

	case NPU_S_PARAM_QOS_NPU_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_npu_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu,
				qos_setting->req_npu_freq);
#if IS_ENABLED(CONFIG_SOC_S5E9955)
		dhcp_table->PRESET_NPU_FREQ = qos_setting->req_npu_freq;
#endif
		break;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	case NPU_S_PARAM_QOS_DSP_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_dsp_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_dsp,
				qos_setting->req_dsp_freq);
		break;
#endif

	case NPU_S_PARAM_QOS_MIF_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_mif_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_mif,
				qos_setting->req_mif_freq);
		break;

	case NPU_S_PARAM_QOS_INT_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_int_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target,
				&qos_setting->npu_qos_req_int,
				qos_setting->req_int_freq);
		break;

	case NPU_S_PARAM_QOS_CL0_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_cl0_freq = npu_qos_preset_get_req_value(param->offset);
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl0,
				qos_setting->req_cl0_freq);
		break;

	case NPU_S_PARAM_QOS_CL1_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_cl1_freq = npu_qos_preset_get_req_value(param->offset);
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl1,
				qos_setting->req_cl1_freq);
		break;

	case NPU_S_PARAM_QOS_CL2_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_cl2_freq = npu_qos_preset_get_req_value(param->offset);
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl2,
				qos_setting->req_cl2_freq);
		break;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	case NPU_S_PARAM_QOS_CL3_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_cl3_freq = npu_qos_preset_get_req_value(param->offset);
		__req_param_qos_cpu(sess->uid, param->target, &qos_setting->npu_qos_req_cpu_cl3,
				qos_setting->req_cl3_freq);
		break;
#endif

#if IS_ENABLED(CONFIG_SOC_S5E9955)
	case NPU_S_PARAM_QOS_DSU_MIN_PRESET:
		qos_setting->dsu_req_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ? NPU_QOS_DSU_MAX_FREQ : 0;
		dev_pm_qos_update_request(&qos_setting->dsu_min_req, qos_setting->dsu_req_freq);
		break;
	case NPU_S_PARAM_QOS_DSU_MAX_PRESET:
		qos_setting->dsu_req_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ? NPU_QOS_DSU_MAX_FREQ : 0;
		dev_pm_qos_update_request(&qos_setting->dsu_max_req, qos_setting->dsu_req_freq);
		break;

#endif

	case NPU_S_PARAM_QOS_MO_SCEN_ID_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_mo_scen = npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target, NULL, qos_setting->req_mo_scen);
		break;

	case NPU_S_PARAM_QOS_CPU_AFF_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_cpu_aff = param->offset;
		/* To be implemented */
		break;

#if IS_ENABLED(CONFIG_NPU_SET_DNC_FREQ)
	case NPU_S_PARAM_QOS_DNC_MIN_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->req_dnc_freq =
			npu_qos_preset_get_req_value(param->offset);
		__req_param_qos(sess->uid, param->target,
				&qos_setting->npu_qos_req_dnc,
				qos_setting->req_dnc_freq);
		break;
#endif

	case NPU_S_PARAM_QOS_GPU_MIN:
		qos_setting->req_gpu_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : 0;
		__req_param_qos(sess->uid, param->target,
				&qos_setting->npu_qos_req_gpu,
				qos_setting->req_gpu_freq);
		break;

	case NPU_S_PARAM_QOS_GPU_MAX:
		qos_setting->req_gpu_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE;
		__req_param_qos(sess->uid, param->target,
				&qos_setting->npu_qos_req_gpu_max,
				qos_setting->req_gpu_freq);
		break;

	case NPU_S_PARAM_QOS_NPU_MAX_PRESET:
		if (!npu_qos_preset_is_valid_value(param->offset))
			break;

		qos_setting->skip_max_noti = true;
		qos_setting->req_npu_freq = (param->offset != NPU_QOS_DEFAULT_VALUE) ?
			param->offset : PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE;
		__req_param_qos(sess->uid, param->target, &qos_setting->npu_qos_req_npu_max,
				qos_setting->req_npu_freq);
		break;

	case NPU_S_PARAM_CPU_AFF:
		tmp[0] = param->offset;
		if (tmp[0] == NPU_QOS_DEFAULT_VALUE)
			tmp[0] = npu_system->default_affinity;
		for (i = 0; i < NPU_SYSTEM_IRQ_MAX; i++) {
			ret = irq_set_affinity_hint(npu_system->irq[i], to_cpumask(tmp));
			if (ret) {
				npu_warn("fail(%d) in irq_set_affinity_hint(%d)\n", ret, i);
				goto err_set_irq;
			}
		}

		ret = set_cpu_affinity(tmp);
		if (unlikely(ret)) {
			npu_warn("fail(%d) in set_cpu_affinity(%u)\n", ret, param->offset);
			goto err_set_task;
		}
		break;

	default:
		mutex_unlock(&qos_setting->npu_qos_lock);
		return S_PARAM_NOMB;
	}

	mutex_unlock(&qos_setting->npu_qos_lock);
	return S_PARAM_HANDLED;
err_set_irq:
	for (i = 0; i < NPU_SYSTEM_IRQ_MAX; i++)
		irq_set_affinity_hint(npu_system->irq[i], NULL);
err_set_task:
	mutex_unlock(&qos_setting->npu_qos_lock);
	return ret;
}

static struct device_attribute npu_qos_sysfs_attr[] = {
	__ATTR(npu_freq_maxlock, 0664,
		npu_show_attrs_qos_sysfs,
		npu_store_attrs_qos_sysfs),
	__ATTR(dnc_freq_maxlock, 0664,
		npu_show_attrs_qos_sysfs,
		npu_store_attrs_qos_sysfs),
};

static struct attribute *npu_qos_sysfs_entries[] = {
	&npu_qos_sysfs_attr[0].attr,
	&npu_qos_sysfs_attr[1].attr,
	NULL,
};

static struct attribute_group npu_qos_attr_group = {
	.name = "qos_freq",
	.attrs = npu_qos_sysfs_entries,
};
enum {
	NPU_QOS_NPU_FREQ_MAXLOCK = 0,
	NPU_QOS_DNC_FREQ_MAXLOCK,
	NPU_QOS_MIF_FREQ_ATTR_NUM,
};

static ssize_t npu_show_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i = 0;
	const ptrdiff_t offset = attr - npu_qos_sysfs_attr;

	switch (offset) {
	case NPU_QOS_NPU_FREQ_MAXLOCK:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%u\n", qos_lock.npu_freq_maxlock);
		break;

	case NPU_QOS_DNC_FREQ_MAXLOCK:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%u\n", qos_lock.dnc_freq_maxlock);
		break;

	default:
		break;
	}

	return i;
}

static ssize_t npu_store_attrs_qos_sysfs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, value = 0;
	const ptrdiff_t offset = attr - npu_qos_sysfs_attr;

	ret = sscanf(buf, "%d", &value);
	if (ret > 0) {
		switch (offset) {
		case NPU_QOS_NPU_FREQ_MAXLOCK:
			qos_lock.npu_freq_maxlock = (u32)value;
			npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_npu_max, value);
			ret = count;
			break;

		case NPU_QOS_DNC_FREQ_MAXLOCK:
			qos_lock.dnc_freq_maxlock = (u32)value;
			npu_dvfs_pm_qos_update_request(&qos_setting->npu_qos_req_dnc_max, value);
			ret = count;
			break;

		default:
			break;
		}
	}

	return ret;
}

static int npu_qos_sysfs_create(struct npu_system *system)
{
	int ret = 0;
	struct npu_device *device;

	device = container_of(system, struct npu_device, system);

	probe_info("npu qos-sysfs create\n");
	probe_info("creating sysfs group %s\n", npu_qos_attr_group.name);

	ret = sysfs_create_group(&device->dev->kobj, &npu_qos_attr_group);
	if (ret) {
		probe_err("failed(%d) to create sysfs for %s\n",
						ret, npu_qos_attr_group.name);
	}

	return ret;
}
