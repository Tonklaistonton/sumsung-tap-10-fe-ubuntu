/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "npu-device.h"
#include "npu-precision.h"
#include "npu-util-memdump.h"

struct npu_precision *npu_precision;

static int npu_precision_parsing_dt(struct npu_device *device) {
	int ret = 0;
	struct device *dev = device->dev;
	struct npu_freq *freq_levels;
	u32 *freq_num = &npu_precision->freq_num;
	u32 *freq_ctrl = npu_precision->freq_ctrl;

	ret = of_property_read_u32(dev->of_node, "samsung,npuprecision-npufreq-num",
		freq_num);
	if (ret) {
		probe_err("failed parsing precision dt(npuprecision-npufreq-num)\n");
		goto err;
	}

	npu_precision->freq_levels = devm_kzalloc(dev, (*freq_num) * sizeof(struct npu_freq), GFP_KERNEL);
	if (!npu_precision->freq_levels) {
		probe_err("failed allocating freq_levels\n");
		ret = -ENOMEM;
		goto err;
	}
	freq_levels = npu_precision->freq_levels;

	ret = of_property_read_u32_array(dev->of_node, "samsung,npuprecision-npufreq-levels",
			(u32 *)freq_levels, (*freq_num) * 2);
	if (ret) {
		probe_err("failed parsing precision dt(npuprecision-npufreq-levels)\n");
		goto err;
	}

	ret = of_property_read_u32_array(dev->of_node, "samsung,npuprecision-npufreq-ctrl",
			freq_ctrl, FREQ_CTRL_SIZE);
	if (ret) {
		probe_err("failed parsing precision dt(npuprecision-npufreq-ctrl)\n");
		goto err;
	}

	return ret;
err:
	if (npu_precision->freq_levels)
		devm_kfree(dev, npu_precision->freq_levels);
	return ret;
}

static void __npu_precision_work(int freq)
{
#if 0
	/*
		We have issue of voltage margin about NPU engine in ROOT project
		So, if ncp_type are unknown or 16bit, we set maxlock 1066MHz/935MHz(NPU/DNC).
		But, Now we do not have backdata of voltage margin about SOLOMON chipset.
		First of all, Skip this algorithm.
	*/
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_info *info;
	struct npu_freq *freq_levels = npu_precision->freq_levels;
	u32 *freq_ctrl = npu_precision->freq_ctrl;

	info = npu_scheduler_get_info();

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
		if (!strcmp("NPU", d->name)) {
			npu_dvfs_set_freq(d, &d->qos_req_max_precision, freq);
		} else if (!strcmp("DNC", d->name)) {
			if (freq == freq_levels[freq_ctrl[MID]].npu) {
				npu_dvfs_set_freq(d, &d->qos_req_max_precision,
					freq_levels[freq_ctrl[MID]].dnc);
			} else if (freq == freq_levels[freq_ctrl[HIGH]].npu) {
				npu_dvfs_set_freq(d, &d->qos_req_max_precision,
					freq_levels[freq_ctrl[HIGH]].dnc);
			}
		}
	}
	mutex_unlock(&info->exec_lock);
#endif
	return;
}

static int __npu_precision_active_check(void)
{
	int i, freq;
	struct npu_precision_model_info *h;
	struct hlist_node *tmp;
	struct npu_freq *freq_level = npu_precision->freq_levels;
	u32 *freq_ctrl = npu_precision->freq_ctrl;

	freq = freq_level[freq_ctrl[HIGH]].npu;

	mutex_lock(&npu_precision->model_lock);
	hash_for_each_safe(npu_precision->precision_active_info_hash, i, tmp, h, hlist) {
		if (h->active) {
			if (!h->type || (h->type == PRE_INT16) || (h->type == PRE_FP16)) {
				npu_info(" model : %s is %u type\n", h->model_name, h->type);
				freq = freq_level[freq_ctrl[MID]].npu;
				break;
			}
		}
	}
	mutex_unlock(&npu_precision->model_lock);

	npu_info("next maxlock is %d\n", freq);
	return freq;
}

static void npu_precision_work(struct work_struct *work)
{
	__npu_precision_work(__npu_precision_active_check());
}

static u32 npu_get_hash_name_key(const char *model_name,
		unsigned int computational_workload,
		unsigned int io_workload)
{
	u32 key = 0;
	char c = 0;
	int i = 0;

	key |= ((u32)computational_workload) << 16;
	key |= ((u32)io_workload) & 0x0000FFFF;



	while ((c = *model_name++)) {
		key |= ((u32)c) << ((8 * i++) & 31);
	}

	return key;
}

static inline bool is_matching_ncp_for_hash_with_session
	(struct npu_precision_model_info *info, struct npu_session *session)
{
	return (!strcmp(info->model_name, session->model_name)) &&
		(info->computational_workload == session->computational_workload) &&
		(info->io_workload == session->io_workload);
}

static int __copy_model_info(struct npu_precision_model_info *to_info,
		struct npu_session *from_session) {

	if (!to_info || !from_session) {
		npu_err("NULL pointer error. to_info = [%p], from_session = [%p]",
			to_info, from_session);
		return -ENOMEM;
	}

	to_info->active = true;
	to_info->type = from_session->featuremapdata_type;
	to_info->computational_workload = from_session->computational_workload;
	to_info->io_workload = from_session->io_workload;
	strncpy(to_info->model_name, from_session->model_name, NCP_MODEL_NAME_LEN);
	return 0;
}

static int __set_model_info(struct npu_precision_model_info *model_info) {
	if (!model_info)
		return -EINVAL;

	atomic_set(&model_info->ref, 0);
	model_info->active = true;
	return 0;
}

static struct npu_precision_model_info *__alloc_model_info(void) {
	struct npu_precision_model_info *model_info =
		kzalloc(sizeof(*model_info), GFP_KERNEL);

	if (!model_info)
		return NULL;

	return model_info;
}

static void __free_model_info(struct npu_precision_model_info *model_info) {
	if (model_info)
		kfree(model_info);
}

void npu_precision_active_info_hash_add(struct npu_session *session)
{
	u32 key;
	u32 find = 0;
	struct npu_precision_model_info *info;
	struct npu_precision_model_info *model_info;

	key = session->key;
	mutex_lock(&npu_precision->model_lock);
	hash_for_each_possible(npu_precision->precision_saved_info_hash, info, hlist, key) {
		if (is_matching_ncp_for_hash_with_session(info, session)) {
			find = PRECISION_REGISTERED;
			npu_dbg("ref : %d, find model : %s[type : %u]\n",
				atomic_read(&info->ref), info->model_name, info->type);
			if ((atomic_inc_return(&info->ref) == 0x1)) {
				hash_add(npu_precision->precision_active_info_hash,
						&info->hlist, key);
				break;
			}
		}
	}

	if (!find) {
		u32 ncp_type = 0;

#if IS_ENABLED(CONFIG_SOC_S5E9955)
		ncp_type = session->featuremapdata_type;
#endif
		npu_dbg("new model : %s [ncp_type : %u]\n", session->model_name, ncp_type);
		model_info = __alloc_model_info();
		if (!model_info) {
			npu_err("failed __alloc_model_info\n");
			goto err_exit;
		}

		__set_model_info(model_info);
		__copy_model_info(model_info, session);

		hash_add(npu_precision->precision_saved_info_hash,
			&model_info->hlist, key);

		atomic_inc(&(model_info->ref));
		hash_add(npu_precision->precision_active_info_hash,
			&model_info->hlist, key);
	}

	mutex_unlock(&npu_precision->model_lock);
	__npu_precision_work(__npu_precision_active_check());
	return;
err_exit:
	mutex_unlock(&npu_precision->model_lock);
}

void npu_precision_hash_update(struct npu_session *session, u32 type)
{
	u32 key, find = 0;
	struct npu_precision_model_info *info;
	struct npu_scheduler_info *sched;

	sched = npu_scheduler_get_info();

	if (sched &&
		(sched->mode == NPU_PERF_MODE_NPU_BOOST ||
		sched->mode == NPU_PERF_MODE_NPU_BOOST_PRUNE)) {
		npu_info("skip hash_update on BOOST or PDCL\n");
		return;
	}

	key = session->key;
	mutex_lock(&npu_precision->model_lock);
	hash_for_each_possible(npu_precision->precision_active_info_hash, info, hlist, key) {
		if (is_matching_ncp_for_hash_with_session(info, session)) {
			find = PRECISION_REGISTERED;
			info->type = type;
			break;
		}
	}

	if (!find) {
		npu_err("Active hash is corruptioned - model_info is not found\n");
	}

	mutex_unlock(&npu_precision->model_lock);

	queue_delayed_work(npu_precision->precision_wq,
			&npu_precision->precision_work,
			msecs_to_jiffies(0));
}

void npu_precision_active_info_hash_delete(struct npu_session *session)
{
	u32 key;
	struct npu_precision_model_info *info;

	key = npu_get_hash_name_key(session->model_name,
			session->computational_workload, session->io_workload);
	mutex_lock(&npu_precision->model_lock);
	hash_for_each_possible(npu_precision->precision_active_info_hash, info, hlist, key) {
		if (is_matching_ncp_for_hash_with_session(info, session)) {
			npu_dbg("ref : %d, drop model : %s[type : %u]\n",
				atomic_read(&info->ref), info->model_name, info->type);
			if ((atomic_dec_return(&info->ref) == 0x0)) {
				info->active = false;
				hash_del(&info->hlist);
				break;
			}
		}
	}
	mutex_unlock(&npu_precision->model_lock);

	queue_delayed_work(npu_precision->precision_wq,
			&npu_precision->precision_work,
			msecs_to_jiffies(0));
}

int npu_precision_open(struct npu_system *system)
{
	int ret = 0;
	return ret;
}

int npu_precision_close(struct npu_system *system)
{
	int ret = 0;

	cancel_delayed_work_sync(&npu_precision->precision_work);

	__npu_precision_work(PRECISION_RELEASE_FREQ);

	return ret;
}

int npu_precision_probe(struct npu_device *device)
{
	int ret = 0;
	npu_precision = kzalloc(sizeof(*npu_precision), GFP_KERNEL);
	if (!npu_precision) {
		ret = -ENOMEM;
		goto err_probe;
	}

	ret = npu_precision_parsing_dt(device);
	if (ret) {
		probe_err("npu_precision parsing dt failed\n");
		goto err_probe;
	}

	hash_init(npu_precision->precision_saved_info_hash);
	hash_init(npu_precision->precision_active_info_hash);

	mutex_init(&npu_precision->model_lock);

	INIT_DELAYED_WORK(&npu_precision->precision_work, npu_precision_work);

	npu_precision->precision_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!npu_precision->precision_wq) {
		probe_err("fail to create workqueue -> npu_precision->precision_wq\n");
		ret = -EFAULT;
		goto err_probe;
	}

	probe_info("NPU precision probe success\n");
	return ret;

err_probe:
	if (npu_precision)
		kfree(npu_precision);
	probe_err("NPU precision probe failed\n");
	return ret;
}

int npu_precision_release(struct npu_device *device)
{
	int ret = 0;
	int i;
	struct npu_precision_model_info *info;
	struct hlist_node *tmp;
	struct npu_freq *freq_levels;

	if (!npu_precision) {
		npu_err("npu precision is NULL\n");
		return -ENOMEM;
	}

	freq_levels = npu_precision->freq_levels;

	mutex_lock(&npu_precision->model_lock);
	hash_for_each_safe(npu_precision->precision_saved_info_hash, i, tmp, info, hlist) {
		info->active = false;
		info->type = 0;
		info->computational_workload = 0;
		info->io_workload = 0;
		strncpy(info->model_name, "already_unregistered", NCP_MODEL_NAME_LEN);
		hash_del(&info->hlist);
		__free_model_info(info);
	}
	mutex_unlock(&npu_precision->model_lock);

	if (freq_levels)
		kfree(freq_levels);

	if (npu_precision)
		kfree(npu_precision);
	probe_info("NPU precision probe success\n");
	return ret;
}
