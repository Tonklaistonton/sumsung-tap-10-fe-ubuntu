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
#include "npu-device.h"
#include "npu-llc.h"
#include "npu-dvfs.h"
#include "npu-bts.h"
#include "npu-dtm.h"
#include "npu-util-regs.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"

static struct npu_scheduler_info *g_npu_scheduler_info;
static struct npu_scheduler_control *npu_sched_ctl_ref;
#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
const struct npu_scheduler_utilization_ops s_utilization_ops = {
	.dev = NULL,
	.utilization_ops = &n_utilization_ops,
};
#endif

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
static void npu_scheduler_work(struct work_struct *work);
#endif
static void npu_scheduler_boost_off_work(struct work_struct *work);
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
static void npu_scheduler_set_llc(struct npu_session *sess, u32 size);
#endif

struct npu_scheduler_info *npu_scheduler_get_info(void)
{
	return g_npu_scheduler_info;
}

static void npu_scheduler_hwacg_set(u32 hw, u32 hwacg)
{
	int ret = 0;
	struct npu_hw_device *hdev;

	npu_info("start HWACG setting[%u], value : %u\n", hw, hwacg);

	if (hw == HWACG_NPU) {
		hdev = npu_get_hdev("NPU");
		if (atomic_read(&hdev->boot_cnt.refcount)) {
			if (hwacg == 0x01) {
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgdisnpu");
			} else if (hwacg == 0x02) {
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "acgdisnpum");
				if (ret)
					goto done;
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgenlh");
			} else if (hwacg == NPU_SCH_DEFAULT_VALUE) {
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgennpu");
			}
		}
	} else if (hw == HWACG_DSP) {
		hdev = npu_get_hdev("DSP");
		if (atomic_read(&hdev->boot_cnt.refcount)) {
			if (hwacg == 0x01)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgdisdsp");
			else if (hwacg == 0x02)
				goto done;
			else if (hwacg == NPU_SCH_DEFAULT_VALUE)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgendsp");
		}
	} else if (hw == HWACG_DNC) {
		hdev = npu_get_hdev("DNC");
		if (atomic_read(&hdev->boot_cnt.refcount)) {
			if (hwacg == 0x01)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgdisdnc");
			else if (hwacg == 0x02)
				goto done;
			else if (hwacg == NPU_SCH_DEFAULT_VALUE)
				ret = npu_cmd_map(&g_npu_scheduler_info->device->system, "hwacgendnc");
		}
	}

done:
	if (ret)
		npu_err("fail(%d) in npu_cmd_map for hwacg\n", ret);
}

/* Call-back from Protodrv */
static int npu_scheduler_save_result(struct npu_session *dummy, struct nw_result result)
{
	if (unlikely(!npu_sched_ctl_ref)) {
		npu_err("Failed to get npu_scheduler_control\n");
		return -EINVAL;
	}

	npu_trace("scheduler request completed : result = %u\n", result.result_code);

	npu_sched_ctl_ref->result_code = result.result_code;
	atomic_set(&npu_sched_ctl_ref->result_available, 1);

	wake_up(&npu_sched_ctl_ref->wq);
	return 0;
}

static void npu_scheduler_set_policy(struct npu_scheduler_info *info,
							struct npu_nw *nw)
{
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
	nw->param0 = info->mode;
	nw->param1 = ((info->llc_mode >> 24) & 0xFF) * npu_get_configs(NPU_LLC_CHUNK_SIZE);
#else
	nw->param0 = info->mode;
#endif
	npu_info("llc_mode(%u)\n", nw->param0);
}


/* Send mode info to FW and check its response */
static int npu_scheduler_send_mode_to_hw(struct npu_session *session,
					struct npu_scheduler_info *info)
{
	int ret;
	struct npu_nw nw;
	int retry_cnt;
	struct npu_vertex_ctx *vctx = &session->vctx;

	if (!(vctx->state & BIT(NPU_VERTEX_POWER))) {
		info->wait_hw_boot_flag = 1;
		npu_info("HW power off state: %d %d\n", info->mode, info->llc_ways);
		return 0;
	}

	memset(&nw, 0, sizeof(nw));
	nw.cmd = NPU_NW_CMD_MODE;
	nw.uid = session->uid;
	nw.session = session;

	/* Set callback function on completion */
	nw.notify_func = npu_scheduler_save_result;

	/* Set LLC policy for FW */
	npu_scheduler_set_policy(info, &nw);

	if ((info->prev_mode == info->mode) && (info->llc_status)) {
		npu_dbg("same mode and llc\n");
		return 0;
	}

	retry_cnt = 0;
	atomic_set(&npu_sched_ctl_ref->result_available, 0);
	while ((ret = npu_ncp_mgmt_put(&nw)) <= 0) {
		npu_info("queue full when inserting scheduler control message. Retrying...");
		if (retry_cnt++ >= NPU_SCHEDULER_CMD_POST_RETRY_CNT) {
			npu_err("timeout exceeded.\n");
			ret = -EWOULDBLOCK;
			goto err_exit;
		}
		msleep(NPU_SCHEDULER_CMD_POST_RETRY_INTERVAL);
	}
	/* Success */
	npu_info("scheduler control message has posted\n");

	ret = wait_event_timeout(
		npu_sched_ctl_ref->wq,
		atomic_read(&npu_sched_ctl_ref->result_available),
		NPU_SCHEDULER_HW_RESP_TIMEOUT);
	if (ret < 0) {
		npu_err("wait_event_timeout error(%d)\n", ret);
		goto err_exit;
	}
	if (!atomic_read(&npu_sched_ctl_ref->result_available)) {
		npu_err("timeout waiting H/W response\n");
		ret = -ETIMEDOUT;
		goto err_exit;
	}
	if (npu_sched_ctl_ref->result_code != NPU_ERR_NO_ERROR) {
		npu_err("hardware reply with NDONE(%d)\n", npu_sched_ctl_ref->result_code);
		ret = -EFAULT;
		goto err_exit;
	}
	ret = 0;

err_exit:
	return ret;
}

static int npu_scheduler_init_dt(struct npu_scheduler_info *info)
{
	int i, count, ret = 0;
	unsigned long f = 0;
	struct dev_pm_opp *opp;
	char *tmp_name;
	struct npu_scheduler_dvfs_info *dinfo;
	struct of_phandle_args pa;
	struct device *dev = info->dev;


#if IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	return 0;
#endif

	probe_info("scheduler init by devicetree\n");
	if (unlikely(!dev)) {
		probe_err("Failed to get device\n");
		ret = -EINVAL;
		goto err_exit;
	}

	count = of_property_count_strings(info->dev->of_node, "samsung,npusched-names");
	if (IS_ERR_VALUE((unsigned long)count)) {
		probe_err("invalid npusched list in %s node\n", info->dev->of_node->name);
		ret = -EINVAL;
		goto err_exit;
	}

	for (i = 0; i < count; i += 2) {
		/* get dvfs info */
		dinfo = (struct npu_scheduler_dvfs_info *)devm_kzalloc(info->dev,
				sizeof(struct npu_scheduler_dvfs_info), GFP_KERNEL);
		if (!dinfo) {
			probe_err("failed to alloc dvfs info\n");
			ret = -ENOMEM;
			goto err_exit;
		}

		/* get dvfs name (same as IP name) */
		ret = of_property_read_string_index(info->dev->of_node,
				"samsung,npusched-names", i,
				(const char **)&dinfo->name);
		if (ret) {
			probe_err("failed to read dvfs name %d from %s node : %d\n",
					i, info->dev->of_node->name, ret);
			goto err_dinfo;
		}
		/* get governor name  */
		ret = of_property_read_string_index(info->dev->of_node,
				"samsung,npusched-names", i + 1,
				(const char **)&tmp_name);
		if (ret) {
			probe_err("failed to read dvfs name %d from %s node : %d\n",
					i + 1, info->dev->of_node->name, ret);
			goto err_dinfo;
		}

		probe_info("set up %s with %s governor\n", dinfo->name, tmp_name);

		/* get dvfs and pm-qos info */
		ret = of_parse_phandle_with_fixed_args(info->dev->of_node,
				"samsung,npusched-dvfs",
				NPU_SCHEDULER_DVFS_TOTAL_ARG_NUM, i / 2, &pa);
		if (ret) {
			probe_err("failed to read dvfs args %d from %s node : %d\n",
					i / 2, info->dev->of_node->name, ret);
			goto err_dinfo;
		}

		dinfo->dvfs_dev = of_find_device_by_node(pa.np);
		if (!dinfo->dvfs_dev) {
			probe_err("invalid dt node for %s devfreq device with %d args\n",
					dinfo->name, pa.args_count);
			ret = -EINVAL;
			goto err_dinfo;
		}
		f = ULONG_MAX;
		opp = dev_pm_opp_find_freq_floor(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid max freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->max_freq = f;
			dev_pm_opp_put(opp);
		}
		f = 0;
		opp = dev_pm_opp_find_freq_ceil(&dinfo->dvfs_dev->dev, &f);
		if (IS_ERR(opp)) {
			probe_err("invalid min freq for %s\n", dinfo->name);
			ret = -EINVAL;
			goto err_dinfo;
		} else {
			dinfo->min_freq = f;
			dev_pm_opp_put(opp);
		}
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_min,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_min),
				dinfo->min_freq);
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max),
				dinfo->max_freq);
#endif
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_min_dvfs_cmd,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_min_dvfs_cmd),
				dinfo->min_freq);
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_dvfs_cmd,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_dvfs_cmd),
				dinfo->max_freq);
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_min_nw_boost,
				get_pm_qos_min(dinfo->name),
				dinfo->min_freq);
		probe_info("add pm_qos min request for boosting %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_min_nw_boost),
				dinfo->min_freq);

#if IS_ENABLED(CONFIG_NPU_WITH_CAM_NOTIFICATION)
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_cam_noti,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request for camera %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_cam_noti),
				dinfo->max_freq);
#endif
#if IS_ENABLED(CONFIG_NPU_AFM)
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_afm,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request for afm %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_afm),
				dinfo->max_freq);
#endif
#if IS_ENABLED(CONFIG_NPU_CHECK_PRECISION)
		npu_dvfs_pm_qos_add_request(&dinfo->qos_req_max_precision,
				get_pm_qos_max(dinfo->name),
				dinfo->max_freq);
		probe_info("add pm_qos max request for precision %s %d as %d\n",
				dinfo->name,
				npu_dvfs_pm_qos_get_class(&dinfo->qos_req_max_precision),
				dinfo->max_freq);
#endif

		probe_info("%s %d %d %d %d %d %d\n", dinfo->name,
				pa.args[0], pa.args[1], pa.args[2],
				pa.args[3], pa.args[4], pa.args[5]);

		/* reset values */
		dinfo->cur_freq = dinfo->min_freq;
		/* add device in scheduler */
		list_add_tail(&dinfo->ip_list, &info->ip_list);

		probe_info("add %s in list\n", dinfo->name);
	}

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	ret = of_property_read_u32_array(info->dev->of_node, "samsung,npudvfs-table-num", info->dvfs_table_num, 2);
	if (ret) {
		probe_err("failed to get npudvfs-table-num (%d)\n", ret);
		ret = -EINVAL;
		goto err_dinfo;
	}
	probe_info("DVFS table num < %d %d >", info->dvfs_table_num[0], info->dvfs_table_num[1]);

	info->dvfs_table = (u32 *)devm_kzalloc(info->dev, sizeof(u32) * info->dvfs_table_num[0] * info->dvfs_table_num[1], GFP_KERNEL);
	if (!info->dvfs_table) {
		probe_err("failed to alloc info->dvfs_table)\n");
		ret = -ENOMEM;
		goto err_dinfo;
	}

	ret = of_property_read_u32_array(info->dev->of_node, "samsung,npudvfs-table", (u32 *)info->dvfs_table,
			info->dvfs_table_num[0] * info->dvfs_table_num[1]);
	if (ret) {
		probe_err("failed to get npudvfs-table (%d)\n", ret);
		ret = -EINVAL;
		goto err_dinfo;
	}

	for (i = 0; i < info->dvfs_table_num[0] * info->dvfs_table_num[1]; i++) {
			probe_info("DVFS table[%d][%d] < %d >", i / info->dvfs_table_num[1], i % info->dvfs_table_num[1], info->dvfs_table[i]);
	}
#endif
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	info->pid_max_clk = (info->pid_max_clk < info->dvfs_table[0]) ? info->dvfs_table[0] : info->pid_max_clk;
	{
		u32 t_dtm_param[7];

		ret = of_property_read_u32_array(info->dev->of_node, "samsung,npudtm-param", t_dtm_param, 7);
		if (ret) {
			probe_err("failed to get npudtm-param (%d)\n", ret);
			ret = -EINVAL;
			goto err_dinfo;
		}
		info->pid_target_thermal = t_dtm_param[0];
		info->pid_p_gain = t_dtm_param[1];
		info->pid_i_gain = t_dtm_param[2];
		info->pid_inv_gain = t_dtm_param[3];
		info->pid_period = t_dtm_param[4];
		info->dtm_nm_lut[0] = t_dtm_param[5];
		info->dtm_nm_lut[1] = t_dtm_param[6];
	}
#endif
	return ret;
err_dinfo:
	devm_kfree(info->dev, dinfo);
err_exit:
	return ret;
}

u32 npu_get_perf_mode(void)
{
	struct npu_scheduler_info *info = npu_scheduler_get_info();
	return info->mode;
}

static int npu_scheduler_init_info(s64 now, struct npu_scheduler_info *info)
{
	int i, ret = 0;
	const char *mode_name;
	struct npu_qos_setting *qos_setting;

	probe_info("scheduler info init\n");
	qos_setting = &(info->device->system.qos_setting);

	info->enable = 1;	/* default enable */
	ret = of_property_read_string(info->dev->of_node,
			"samsung,npusched-mode", &mode_name);
	if (ret)
		info->mode = NPU_PERF_MODE_NORMAL;
	else {
		for (i = 0; i < ARRAY_SIZE(npu_perf_mode_name); i++) {
			if (!strcmp(npu_perf_mode_name[i], mode_name))
				break;
		}
		if (i == ARRAY_SIZE(npu_perf_mode_name)) {
			probe_err("Fail on %s, number out of bounds in array=[%lu]\n", __func__,
									ARRAY_SIZE(npu_perf_mode_name));
			return -1;
		}
		info->mode = i;
	}

	for (i = 0; i < NPU_PERF_MODE_NUM; i++)
		info->mode_ref_cnt[i] = 0;

	probe_info("NPU mode : %s\n", npu_perf_mode_name[info->mode]);
	info->bts_scenindex = -1;
	info->llc_status = 0;
	info->llc_ways = 0;
	info->hwacg_status = HWACG_STATUS_ENABLE;

	info->boost_count = 0;
	info->dd_direct_path = 0;
	info->wait_hw_boot_flag = 0;
	atomic_set(&info->cpuidle_cnt, 0);
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	info->pid_en = 0;
	info->debug_log_en = 0;
	info->curr_thermal = 0;
	info->dtm_prev_freq = -1;
#endif

	ret = of_property_read_u32(info->dev->of_node,
			"samsung,npusched-period", &info->period);
	if (ret)
		info->period = NPU_SCHEDULER_DEFAULT_PERIOD;

	probe_info("NPU period %d ms\n", info->period);

	/* initialize FPS information */
	mutex_init(&info->fps_lock);
	INIT_LIST_HEAD(&info->fps_load_list);

	INIT_LIST_HEAD(&info->ip_list);

	/* de-activated scheduler */
	info->activated = 0;
	mutex_init(&info->exec_lock);
	mutex_init(&info->param_handle_lock);
	info->is_dvfs_cmd = false;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_init(info->dev, &info->sws,
				NPU_WAKE_LOCK_SUSPEND, "npu-scheduler");
#endif

	memset((void *)&info->sched_ctl, 0, sizeof(struct npu_scheduler_control));
	npu_sched_ctl_ref = &info->sched_ctl;

	init_waitqueue_head(&info->sched_ctl.wq);

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	INIT_DELAYED_WORK(&info->sched_work, npu_scheduler_work);
#endif
	INIT_DELAYED_WORK(&info->boost_off_work, npu_scheduler_boost_off_work);

	probe_info("scheduler info init done\n");

	return 0;
}

int npu_scheduler_boost_on(struct npu_scheduler_info *info)
{
	struct npu_scheduler_dvfs_info *d;

	npu_info("boost on (count %d)\n", info->boost_count + 1);
	if (likely(info->boost_count == 0)) {
		if (unlikely(list_empty(&info->ip_list))) {
			npu_err("no device for scheduler\n");
			return -EPERM;
		}

		mutex_lock(&info->exec_lock);
		list_for_each_entry(d, &info->ip_list, ip_list) {
#if IS_ENABLED(CONFIG_SOC_S5E9955)
			if (!strcmp("DNC", d->name)) {
#else
			if (!strcmp("NPU", d->name)) {
#endif
				if (!test_bit(NPU_DEVICE_STATE_OPEN, &info->device->state))
					continue;

				npu_dvfs_set_freq_boost(d, &d->qos_req_min_nw_boost, d->max_freq);
				npu_info("boost on freq for %s : %d\n", d->name, d->max_freq);
			}
		}
		mutex_unlock(&info->exec_lock);
	}
	info->boost_count++;
	return 0;
}

static int __npu_scheduler_boost_off(struct npu_scheduler_info *info)
{
	int ret = 0;
	struct npu_scheduler_dvfs_info *d;

	if (list_empty(&info->ip_list)) {
		npu_err("no device for scheduler\n");
		ret = -EPERM;
		goto p_err;
	}

	mutex_lock(&info->exec_lock);
	list_for_each_entry(d, &info->ip_list, ip_list) {
#if IS_ENABLED(CONFIG_SOC_S5E9955)
		if (!strcmp("DNC", d->name)) {
#else
		if (!strcmp("NPU", d->name)) {
#endif
			if (!test_bit(NPU_DEVICE_STATE_OPEN, &info->device->state))
					continue;

				npu_dvfs_set_freq(d, &d->qos_req_min_nw_boost, d->min_freq);
				npu_info("boost off freq for %s : %d\n", d->name, d->min_freq);
			}
		}
	mutex_unlock(&info->exec_lock);
	return ret;
p_err:
	return ret;
}

int npu_scheduler_boost_off(struct npu_scheduler_info *info)
{
	int ret = 0;

	info->boost_count--;
	npu_info("boost off (count %d)\n", info->boost_count);

	if (info->boost_count <= 0) {
		ret = __npu_scheduler_boost_off(info);
		info->boost_count = 0;
	} else if (info->boost_count > 0)
		queue_delayed_work(info->sched_wq, &info->boost_off_work,
				msecs_to_jiffies(NPU_SCHEDULER_BOOST_TIMEOUT));

	return ret;
}

int npu_scheduler_boost_off_timeout(struct npu_scheduler_info *info, s64 timeout)
{
	int ret = 0;

	if (timeout == 0) {
		npu_scheduler_boost_off(info);
	} else if (timeout > 0) {
		queue_delayed_work(info->sched_wq, &info->boost_off_work,
				msecs_to_jiffies(timeout));
	} else {
		npu_err("timeout cannot be less than 0\n");
		ret = -EPERM;
		goto p_err;
	}
	return ret;
p_err:
	return ret;
}

static void npu_scheduler_boost_off_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, boost_off_work.work);
	npu_scheduler_boost_off(info);
}

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
int npu_scheduler_get_lut_idx(struct npu_scheduler_info *info, int clk, dvfs_ip_type ip_type)
{
	int lut_idx;

	if (ip_type >= info->dvfs_table_num[1])
		ip_type = DIT_CORE;

	for (lut_idx = 0; lut_idx < info->dvfs_table_num[0]; lut_idx++) {
		if (clk >= info->dvfs_table[lut_idx * info->dvfs_table_num[1] + ip_type])
			return lut_idx;
	}
	return lut_idx;
}
int npu_scheduler_get_clk(struct npu_scheduler_info *info, int lut_idx, dvfs_ip_type ip_type)
{
	if (lut_idx < 0)
		lut_idx = 0;
	else if (lut_idx > (info->dvfs_table_num[0] - 1))
		lut_idx = info->dvfs_table_num[0] -1;

	if (ip_type >= info->dvfs_table_num[1])
		ip_type = DIT_CORE;

	return info->dvfs_table[lut_idx * info->dvfs_table_num[1] + ip_type];
}
#endif

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
static void npu_scheduler_work(struct work_struct *work)
{
	struct npu_scheduler_info *info;

	/* get basic information */
	info = container_of(work, struct npu_scheduler_info, sched_work.work);
	set_cpus_allowed_ptr(current, cpumask_of(0));

#if 0 //IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	npu_dtm_set(info);
#endif
	npu_dtm_trigger_emode(info);

	queue_delayed_work_on(0, info->sched_wq, &info->sched_work,
			msecs_to_jiffies(info->period));
}
#endif

static ssize_t npu_scheduler_debugfs_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	int i, ret = 0;
	char buf[30];
	ssize_t size;
	int x;

	for (i = 0; i < ARRAY_SIZE(npu_scheduler_debugfs_name); i++) {
		if (!strcmp(npu_scheduler_debugfs_name[i], filp->f_path.dentry->d_iname))
			break;
	}
	if (i == ARRAY_SIZE(npu_scheduler_debugfs_name)) {
		probe_err("Fail on %s, number out of bounds in array=[%lu]\n", __func__,
								ARRAY_SIZE(npu_scheduler_debugfs_name));
		return -1;
	}

	size = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (size <= 0) {
		ret = -EINVAL;
		npu_err("Failed to get user parameter(%zd)\n", size);
		goto p_err;
	}
	buf[size - 1] = '\0';

	ret = sscanf(buf, "%d", &x);
	if (ret != 1) {
		npu_err("Failed to get period parameter(%d)\n", ret);
		ret = -EINVAL;
		goto p_err;
	}
	probe_info("input params is  %u\n", x);

	switch(i)
	{
	case NPU_SCHEDULER_PERIOD:
		g_npu_scheduler_info->period = (u32)x;
	break;

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	case NPU_SCHEDULER_PID_TARGET_THERMAL:
		g_npu_scheduler_info->pid_target_thermal = (u32)x;
		break;

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	case NPU_SCHEDULER_PID_P_GAIN:
		g_npu_scheduler_info->pid_p_gain = x;
		break;

	case NPU_SCHEDULER_PID_I_GAIN:
		g_npu_scheduler_info->pid_i_gain = x;
		break;

	case NPU_SCHEDULER_PID_D_GAIN:
		g_npu_scheduler_info->pid_inv_gain = x;
		break;

	case NPU_SCHEDULER_PID_PERIOD:
		g_npu_scheduler_info->pid_period = x;
		break;
#endif
#endif
	default:
		break;
	}

p_err:
	return ret;
}

static int npu_scheduler_debugfs_show(struct seq_file *file, void *unused)
{
	int i;
#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
	int j;
#endif

	for (i = 0; i < ARRAY_SIZE(npu_scheduler_debugfs_name); i++) {
		if (!strcmp(npu_scheduler_debugfs_name[i], file->file->f_path.dentry->d_iname))
			break;
	}
	if (i == ARRAY_SIZE(npu_perf_mode_name)) {
		probe_err("Fail on %s, number out of bounds in array=[%lu]\n", __func__,
								ARRAY_SIZE(npu_perf_mode_name));
		return -1;
	}

	switch(i)
	{
	case NPU_SCHEDULER_PERIOD:
		seq_printf(file, "%d\n", g_npu_scheduler_info->period);
	break;

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	case NPU_SCHEDULER_PID_TARGET_THERMAL:
		seq_printf(file, "%u\n", g_npu_scheduler_info->pid_target_thermal);
		break;

#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	case NPU_SCHEDULER_PID_P_GAIN:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_p_gain);
		break;

	case NPU_SCHEDULER_PID_I_GAIN:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_i_gain);
		break;

	case NPU_SCHEDULER_PID_D_GAIN:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_inv_gain);
		break;

	case NPU_SCHEDULER_PID_PERIOD:
		seq_printf(file, "%d\n", g_npu_scheduler_info->pid_period);
		break;

	case NPU_SCHEDULER_DEBUG_LOG:
	{
		int freq_avr = 0;
		int t;

		if (g_npu_scheduler_info->debug_dump_cnt == 0) {
			seq_printf(file, "[idx/total] 98C_cnt freq_avr [idx thermal T_freq freq_avr]\n");
			for (t = 0; t < g_npu_scheduler_info->debug_cnt; t++)
				freq_avr += npu_scheduler_get_clk(g_npu_scheduler_info,
							npu_scheduler_get_lut_idx(g_npu_scheduler_info,
								g_npu_scheduler_info->debug_log[t][2], 0), 0);

			freq_avr /= g_npu_scheduler_info->debug_cnt;
		}

		if (g_npu_scheduler_info->debug_dump_cnt + 20 < g_npu_scheduler_info->debug_cnt) {
			for (t = g_npu_scheduler_info->debug_dump_cnt;
					 t < g_npu_scheduler_info->debug_dump_cnt + 20; t++)
				seq_printf(file, "[%d/%d] %d [%d %d %d %d]\n",
						t,
						g_npu_scheduler_info->debug_cnt,
						freq_avr,
						(int)g_npu_scheduler_info->debug_log[t][0],
						(int)g_npu_scheduler_info->debug_log[t][1],
						(int)g_npu_scheduler_info->debug_log[t][2],
						(int)npu_scheduler_get_clk(g_npu_scheduler_info,
						npu_scheduler_get_lut_idx(g_npu_scheduler_info,
						g_npu_scheduler_info->debug_log[t][2], 0), 0));

			g_npu_scheduler_info->debug_dump_cnt += 20;
		}
	}
#else
	case NPU_SCHEDULER_DEBUG_LOG:
#endif
		g_npu_scheduler_info->debug_log_en = 1;
		break;

#endif

#if IS_ENABLED(CONFIG_NPU_USE_UTIL_STATS)
	case NPU_SCHEDULER_CPU_UTILIZATION:
		if (g_npu_scheduler_info->activated)
			seq_printf(file, "%u\n", s_utilization_ops.utilization_ops->get_s_cpu_utilization());
		break;

	case NPU_SCHEDULER_DSP_UTILIZATION:
		if (g_npu_scheduler_info->activated)
			seq_printf(file, "%u\n", s_utilization_ops.utilization_ops->get_s_dsp_utilization());
		break;

	case NPU_SCHEDULER_NPU_UTILIZATION:
		if (g_npu_scheduler_info->activated) {
			for (j = 0; j < CONFIG_NPU_NUM_CORES; j++)
				seq_printf(file, "%u ", s_utilization_ops.utilization_ops->get_s_npu_utilization(j));
			seq_printf(file, "\n");
		}
		break;
#endif

	default:
		break;
	}

	return 0;
}

static int npu_scheduler_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, npu_scheduler_debugfs_show, inode->i_private);
}

const struct file_operations npu_scheduler_debugfs_fops = {
	.open           = npu_scheduler_debugfs_open,
	.read           = seq_read,
	.write		= npu_scheduler_debugfs_write,
	.llseek         = seq_lseek,
	.release        = single_release
};

int npu_scheduler_probe(struct npu_device *device)
{
	int i, ret = 0;
	s64 now;
	struct npu_scheduler_info *info;

	info = kzalloc(sizeof(struct npu_scheduler_info), GFP_KERNEL);
	if (!info) {
		probe_err("failed to alloc info\n");
		ret = -ENOMEM;
		goto err_info;
	}
	memset(info, 0, sizeof(struct npu_scheduler_info));
	device->sched = info;
	device->system.qos_setting.info = info;
	info->device = device;
	info->dev = device->dev;

	now = npu_get_time_us();

	/* init scheduler data */
	ret = npu_scheduler_init_info(now, info);
	if (ret) {
		probe_err("fail(%d) init info\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	for(i=0; i< ARRAY_SIZE(npu_scheduler_debugfs_name); i++) {
		ret = npu_debug_register(npu_scheduler_debugfs_name[i], &npu_scheduler_debugfs_fops);
		if (ret) {
			probe_err("loading npu_debug : debugfs for dvfs_info can not be created(%d)\n", ret);
			goto err_info;
		}
	}

	/* init scheduler with dt */
	ret = npu_scheduler_init_dt(info);
	if (ret) {
		probe_err("fail(%d) initial setting with dt\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	/* init dvfs command list with dt */
	ret = npu_dvfs_init_cmd_list(&device->system, info);
	if (ret) {
		probe_err("fail(%d) initial dvfs command setting with dt\n", ret);
		ret = -EFAULT;
		goto err_info;
	}

	info->sched_wq = create_singlethread_workqueue(dev_name(device->dev));
	if (!info->sched_wq) {
		probe_err("fail to create workqueue\n");
		ret = -EFAULT;
		goto err_info;
	}
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	/* Get NPU Thermal zone */
	info->npu_tzd = thermal_zone_get_zone_by_name("NPU");
#endif
	g_npu_scheduler_info = info;

	return ret;
err_info:
	if (info)
		kfree(info);
	g_npu_scheduler_info = NULL;
	return ret;
}

int npu_scheduler_release(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	info = device->sched;

	g_npu_scheduler_info = NULL;

	return ret;
}

int npu_scheduler_register_session(const struct npu_session *session)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;

	info = g_npu_scheduler_info;

	mutex_lock(&info->fps_lock);
	/* create load data for session */
	l = kzalloc(sizeof(struct npu_scheduler_fps_load), GFP_KERNEL);
	if (!l) {
		npu_err("failed to alloc fps_load\n");
		ret = -ENOMEM;
		mutex_unlock(&info->fps_lock);
		return ret;
	}
	l->session = session;
	l->uid = session->uid;
	l->priority = session->sched_param.priority;
	l->mode = NPU_PERF_MODE_NORMAL;
	list_add(&l->list, &info->fps_load_list);

	npu_info("load for uid %d (p %d) added\n",
			l->uid, l->priority);
	mutex_unlock(&info->fps_lock);

	return ret;
}

void npu_scheduler_unregister_session(const struct npu_session *session)
{
	struct npu_scheduler_info *info;
	struct npu_scheduler_fps_load *l;

	info = g_npu_scheduler_info;

	mutex_lock(&info->fps_lock);
	/* delete load data for session */
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == session->uid) {
			list_del(&l->list);
			kfree(l);
			npu_info("load for uid %d deleted\n", session->uid);
			break;
		}
	}

	mutex_unlock(&info->fps_lock);
}

int npu_scheduler_open(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	int i;
#endif

	info = device->sched;

	if (info->llc_status) {
		info->mode = NPU_PERF_MODE_NORMAL;
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
		npu_set_llc(info);
#endif
	}

	/* activate scheduler */
	info->activated = 1;
	info->is_dvfs_cmd = false;
	info->llc_status = 0;
	info->llc_ways = 0;
	info->dd_direct_path = 0;
	info->prev_mode = -1;
	info->wait_hw_boot_flag = 0;
	info->enable = 1;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM_DEBUG)
	info->debug_cnt = 0;
	info->debug_dump_cnt = 0;
#endif
	info->dtm_curr_freq = info->pid_max_clk;
	for (i = 0; i < PID_I_BUF_SIZE; i++)
		info->th_err_db[i] = 0;
#endif

	/* set dvfs command list for default mode */
	npu_dvfs_cmd_map(info, "open");

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	queue_delayed_work_on(0, info->sched_wq, &info->sched_work,
			msecs_to_jiffies(100));
#endif
#endif

	return ret;
}

int npu_scheduler_close(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;
	struct npu_scheduler_dvfs_info *d;

	info = device->sched;

	/* de-activate scheduler */
	info->activated = 0;
	info->llc_ways = 0;
	info->dd_direct_path = 0;
	info->wait_hw_boot_flag = 0;
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	info->pid_en = 0;
#endif

#ifdef CONFIG_NPU_SCHEDULER_OPEN_CLOSE
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	cancel_delayed_work_sync(&info->sched_work);
#endif
#endif
	npu_info("boost off (count %d)\n", info->boost_count);
	__npu_scheduler_boost_off(info);
	info->boost_count = 0;
	info->is_dvfs_cmd = false;

	/* set dvfs command list for default mode */
	npu_dvfs_cmd_map(info, "close");

	list_for_each_entry(d, &info->ip_list, ip_list) {
		/* set frequency */
		npu_dvfs_set_freq(d, NULL, 0);
	}
	npu_scheduler_system_param_unset();

	return ret;
}

int npu_scheduler_resume(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	if (unlikely(!device)) {
		npu_err("Failed to get npu_device\n");
		return -EINVAL;
	}

	info = device->sched;

	/* re-schedule work */
	if (info->activated) {
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
		cancel_delayed_work_sync(&info->sched_work);
#endif
#if IS_ENABLED(CONFIG_PM_SLEEP)
		npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
		queue_delayed_work_on(0, info->sched_wq, &info->sched_work,
				msecs_to_jiffies(100));
#endif
	}

	npu_info("done\n");
	return ret;
}

int npu_scheduler_suspend(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	if (unlikely(!device)) {
		npu_err("Failed to get npu_device\n");
		return -EINVAL;
	}

	info = device->sched;

#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	if (info->activated)
		cancel_delayed_work_sync(&info->sched_work);
#endif

	npu_info("done\n");
	return ret;
}

int npu_scheduler_start(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	info = device->sched;

#ifdef CONFIG_NPU_SCHEDULER_START_STOP
#if IS_ENABLED(CONFIG_PM_SLEEP)
	npu_wake_lock_timeout(info->sws, msecs_to_jiffies(100));
#endif
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	queue_delayed_work_on(0, info->sched_wq, &info->sched_work,
				msecs_to_jiffies(100));
#endif
#endif
	return ret;
}

int npu_scheduler_stop(struct npu_device *device)
{
	int ret = 0;
	struct npu_scheduler_info *info;

	info = device->sched;

#ifdef CONFIG_NPU_SCHEDULER_START_STOP
#if IS_ENABLED(CONFIG_NPU_USE_DTM_EMODE)
	cancel_delayed_work_sync(&info->sched_work);
#endif
#endif
	npu_info("boost off (count %d)\n", info->boost_count);
	__npu_scheduler_boost_off(info);
	info->boost_count = 0;

	return ret;
}

static u32 calc_next_mode(struct npu_scheduler_info *info, u32 req_mode, u32 prev_mode, npu_uid_t uid)
{
	int i;
	u32 ret;
	u32 sess_prev_mode = 0;
	int found = 0;
	struct npu_scheduler_fps_load *l;

	ret = NPU_PERF_MODE_NORMAL;

	mutex_lock(&info->fps_lock);
	list_for_each_entry(l, &info->fps_load_list, list) {
		if (l->uid == uid) {
			found = 1;
			/* read previous mode of a session and update next mode */
			sess_prev_mode = l->mode;
			l->mode = req_mode;
			break;
		}
	}
	mutex_unlock(&info->fps_lock);

	if (unlikely(!found))
		return ret;

	/* update reference count for each mode */
	if (req_mode == NPU_PERF_MODE_NORMAL) {
		if (info->mode_ref_cnt[sess_prev_mode] > 0)
			info->mode_ref_cnt[sess_prev_mode]--;
	} else {
		info->mode_ref_cnt[req_mode]++;
	}

	/* calculate next mode */
	for (i = 0; i < NPU_PERF_MODE_NUM; i++)
		if (info->mode_ref_cnt[i] > 0)
			ret = i;

	return ret;
}

void npu_scheduler_system_param_unset(void)
{
	mutex_lock(&g_npu_scheduler_info->param_handle_lock);
	if (g_npu_scheduler_info->bts_scenindex > 0) {
		int ret = npu_bts_del_scenario(g_npu_scheduler_info->bts_scenindex);
		if (ret)
			npu_err("npu_bts_del_scenario failed : %d\n", ret);
	}

	mutex_unlock(&g_npu_scheduler_info->param_handle_lock);
}

void npu_scheduler_set_cpuidle(u32 val)
{
	npu_info("preset_cpuidle : %u - %d\n",
		val, atomic_read(&g_npu_scheduler_info->cpuidle_cnt));

	if (val == 0x01) {
		if (atomic_inc_return(&g_npu_scheduler_info->cpuidle_cnt) == 1) {
			cpuidle_pause_and_lock();
			npu_info("cpuidle_pause\n");
		}
	} else if (val == NPU_SCH_DEFAULT_VALUE) {
		if (atomic_dec_return(&g_npu_scheduler_info->cpuidle_cnt) == 0) {
			cpuidle_resume_and_unlock();
			npu_info("cpuidle_resume\n");
		}
	} else {
		npu_err("invalid scheduler mode setting, val: %u cpuidle_count: %d\n",
			val, atomic_read(&g_npu_scheduler_info->cpuidle_cnt));
	}
}


static void npu_scheduler_set_DD_log(u32 val)
{
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	if (g_npu_scheduler_info->debug_log_en != 0)
		return;
#endif
	npu_info("preset_DD_log turn on/off : %u\n", val);

#if !IS_ENABLED(CONFIG_NPU_BRINGUP_NOTDONE)
	if (val == 0x01)
		npu_log_set_loglevel(val, MEMLOG_LEVEL_ERR);
	else
		npu_log_set_loglevel(0, MEMLOG_LEVEL_CAUTION);
#endif

}

#if IS_ENABLED(CONFIG_NPU_USE_LLC)
static void npu_scheduler_set_llc(struct npu_session *sess, u32 size)
{
	int ways = 0;
	struct npu_vertex_ctx *vctx = &sess->vctx;
	u32 size_in_bytes = size * K_SIZE;

	if (size == NPU_SCH_DEFAULT_VALUE)
		size_in_bytes = 0;
	else if (size > npu_get_configs(NPU_LLC_CHUNK_SIZE)*LLC_MAX_WAYS)
		size_in_bytes = npu_get_configs(NPU_LLC_CHUNK_SIZE)*LLC_MAX_WAYS;

	ways = size_in_bytes / npu_get_configs(NPU_LLC_CHUNK_SIZE);
	sess->llc_ways = ways;

	npu_dbg("last ways[%u], new ways[%u] req_size[%u]\n",
			g_npu_scheduler_info->llc_ways, ways, size);

	g_npu_scheduler_info->llc_ways = ways;

	if (!(vctx->state & BIT(NPU_VERTEX_POWER))) {
		g_npu_scheduler_info->wait_hw_boot_flag = 1;
		npu_info("set_llc - HW power off state: %d %d\n",
			g_npu_scheduler_info->mode, g_npu_scheduler_info->llc_ways);
		return;
	}

	npu_set_llc(g_npu_scheduler_info);
	npu_scheduler_send_mode_to_hw(sess, g_npu_scheduler_info);
}
#endif

void npu_scheduler_send_wait_info_to_hw(struct npu_session *session,
					struct npu_scheduler_info *info)
{
	int ret;

	if (info->wait_hw_boot_flag) {
		info->wait_hw_boot_flag = 0;
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
		npu_set_llc(info);
#endif
		ret = npu_scheduler_send_mode_to_hw(session, info);
		if (ret < 0)
			npu_err("send_wait_info_to_hw error %d\n", ret);
	}
}

npu_s_param_ret npu_scheduler_param_handler(struct npu_session *sess, struct vs4l_param *param)
{
	int found = 0;
	struct npu_scheduler_fps_load *l;
	npu_s_param_ret ret = S_PARAM_HANDLED;
	u32 req_mode, prev_mode, next_mode;

	if (!g_npu_scheduler_info)
		return S_PARAM_NOMB;

	mutex_lock(&g_npu_scheduler_info->fps_lock);
	list_for_each_entry(l, &g_npu_scheduler_info->fps_load_list, list) {
		if (l->uid == sess->uid) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&g_npu_scheduler_info->fps_lock);
	if (!found) {
		npu_err("UID %d NOT found\n", sess->uid);
		ret = S_PARAM_NOMB;
		return ret;
	}

	mutex_lock(&g_npu_scheduler_info->param_handle_lock);
	switch (param->target) {
	case NPU_S_PARAM_LLC_SIZE:
	case NPU_S_PARAM_LLC_SIZE_PRESET:
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
		if (g_npu_scheduler_info->activated) {
			npu_dbg("S_PARAM_LLC - size : %dKB\n", param->offset);
			npu_scheduler_set_llc(sess, param->offset);
		}
#endif
		break;

	case NPU_S_PARAM_PERF_MODE:
		if (param->offset < NPU_PERF_MODE_NUM) {
			req_mode = param->offset;
			prev_mode = g_npu_scheduler_info->mode;
			g_npu_scheduler_info->prev_mode = prev_mode;
			next_mode = calc_next_mode(g_npu_scheduler_info, req_mode, prev_mode, sess->uid);
			g_npu_scheduler_info->mode = next_mode;

			npu_dbg("req_mode:%u, prev_mode:%u, next_mode:%u\n",
					req_mode, prev_mode, next_mode);

			if (g_npu_scheduler_info->activated && req_mode == next_mode) {
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
				npu_scheduler_set_llc(sess, npu_kpi_llc_size(g_npu_scheduler_info));
#else
				npu_scheduler_send_mode_to_hw(sess, g_npu_scheduler_info);
#endif

#if IS_ENABLED(CONFIG_NPU_USE_ESCA_DTM)
				if (is_kpi_mode_enabled(true))
					npu_dtm_ipc_communicate(NPU_ESCA_BOOST);
#endif

				npu_dbg("new NPU performance mode : %s\n",
						npu_perf_mode_name[g_npu_scheduler_info->mode]);
			}
		} else {
			npu_err("Invalid NPU performance mode : %d\n",
					param->offset);
			ret = S_PARAM_NOMB;
		}
		break;

	case NPU_S_PARAM_PRIORITY:
		if (param->offset > NPU_SCHEDULER_PRIORITY_MAX) {
			npu_err("Invalid priority : %d\n", param->offset);
			ret = S_PARAM_NOMB;
		} else {
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			l->priority = param->offset;

			/* TODO: hand over priority info to session */

			npu_dbg("set priority of uid %d as %d\n",
					l->uid, l->priority);
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
		}
		break;

	case NPU_S_PARAM_TPF:
		if (param->offset == 0) {
			npu_err("No TPF setting : %d\n", param->offset);
			ret = S_PARAM_NOMB;
		} else {
			mutex_lock(&g_npu_scheduler_info->fps_lock);
			sess->tpf = param->offset;
#if IS_ENABLED(CONFIG_NPU_GOVERNOR)
			sess->tpf_requested = true;
#endif
			npu_dbg("set tpf of uid %d as %u\n",
					l->uid, param->offset);
			mutex_unlock(&g_npu_scheduler_info->fps_lock);
		}
		break;

	case NPU_S_PARAM_MO_SCEN_PRESET:
		npu_dbg("MO_SCEN_PRESET : %u\n", (u32)param->offset);
		if (param->offset == MO_SCEN_NORMAL) {
			npu_dbg("MO_SCEN_PRESET : npu_normal\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("npu_normal");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == MO_SCEN_PERF) {
			npu_dbg("MO_SCEN_PRESET : npu_performace\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("npu_performance");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == MO_SCEN_G3D_HEAVY) {
			npu_dbg("MO_SCEN_PRESET : g3d_heavy\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("g3d_heavy");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == MO_SCEN_G3D_PERF) {
			npu_dbg("MO_SCEN_PRESET : g3d_performance\n");
			g_npu_scheduler_info->bts_scenindex = npu_bts_get_scenindex("g3d_performance");

			if (g_npu_scheduler_info->bts_scenindex < 0) {
				npu_err("npu_bts_get_scenindex failed : %d\n", g_npu_scheduler_info->bts_scenindex);
				ret = g_npu_scheduler_info->bts_scenindex;
				break;
			}

			ret = npu_bts_add_scenario(g_npu_scheduler_info->bts_scenindex);
			if (ret)
				npu_err("npu_bts_add_scenario failed : %d\n", ret);
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("MO_SCEN_PRESET : default\n");
			if (g_npu_scheduler_info->bts_scenindex > 0) {
				ret = npu_bts_del_scenario(g_npu_scheduler_info->bts_scenindex);
				if (ret)
					npu_err("npu_bts_del_scenario failed : %d\n", ret);
			}
		}
		break;

	case NPU_S_PARAM_CPU_DISABLE_IDLE_PRESET:
		npu_scheduler_set_cpuidle(param->offset);
		break;

	case NPU_S_PARAM_DD_LOG_OFF_PRESET:
		npu_scheduler_set_DD_log(param->offset);
		break;

	case NPU_S_PARAM_HWACG_NPU_DISABLE_PRESET:
		npu_scheduler_hwacg_set(HWACG_NPU, param->offset);
		break;

#if IS_ENABLED(CONFIG_DSP_USE_VS4L)
	case NPU_S_PARAM_HWACG_DSP_DISABLE_PRESET:
		npu_scheduler_hwacg_set(HWACG_DSP, param->offset);
		break;
#endif

	case NPU_S_PARAM_HWACG_DNC_DISABLE_PRESET:
		npu_scheduler_hwacg_set(HWACG_DNC, param->offset);
		break;

	case NPU_S_PARAM_DD_DIRECT_PATH_PRESET:
		npu_dbg("DD_DIRECT_PATH val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			npu_dbg("DD_DIRECT_PATH On\n");
			g_npu_scheduler_info->dd_direct_path = 0x01;
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("DD_DIRECT_PATH Off\n");
			g_npu_scheduler_info->dd_direct_path = 0x00;
		}

		break;
#if IS_ENABLED(CONFIG_NPU_USE_LLC)
	case NPU_S_PARAM_FW_KPI_MODE:
		break;
#endif
#if IS_ENABLED(CONFIG_NPU_USE_PI_DTM)
	case NPU_S_PARAM_DTM_PID_EN:
		npu_dbg("DTM_PID_EN val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			npu_dbg("DTM_PID On\n");
			g_npu_scheduler_info->pid_en = 0x01;
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			npu_dbg("DTM_PID Off\n");
			g_npu_scheduler_info->pid_en = 0x00;
		}
		break;

	case NPU_S_PARAM_DTM_TARGET_THERMAL:
		npu_dbg("DTM_TARGET_THERMAL : %u\n", (u32)param->offset);
		g_npu_scheduler_info->pid_target_thermal = param->offset;
		break;

	case NPU_S_PARAM_DTM_MAX_CLK:
		npu_dbg("DTM_MAX_CLK : %u\n", (u32)param->offset);
		g_npu_scheduler_info->pid_max_clk = param->offset;
		break;
#endif

	case NPU_S_PARAM_DVFS_DISABLE:
		g_npu_scheduler_info->enable = 0x00;
		break;

	case NPU_S_PARAM_BLOCKIO_UFS_MODE:
		/* UFS boost API is deprecated. */
		break;

	case NPU_S_PARAM_DMABUF_LAZY_UNMAPPING_DISABLE:
		npu_dbg("DMABUF_LAZY_UNMAPPING_DISABLE  val : %u\n", (u32)param->offset);
		if (param->offset == 0x01) {
			sess->lazy_unmap_disable = NPU_SESSION_LAZY_UNMAP_DISABLE;
		} else if (param->offset == NPU_SCH_DEFAULT_VALUE) {
			sess->lazy_unmap_disable = NPU_SESSION_LAZY_UNMAP_ENABLE;
		}
		break;


	default:
		ret = S_PARAM_NOMB;
		break;
	}
	mutex_unlock(&g_npu_scheduler_info->param_handle_lock);

	return ret;
}

npu_s_param_ret npu_preference_param_handler(struct npu_session *sess, struct vs4l_param *param)
{
	u32 preference;
	npu_s_param_ret ret = S_PARAM_HANDLED;

	preference = param->offset;
	if ((preference >= CMD_LOAD_FLAG_IMB_PREFERENCE1) && (preference <= CMD_LOAD_FLAG_IMB_PREFERENCE5))
		sess->preference = param->offset;
	else
		sess->preference = CMD_LOAD_FLAG_IMB_PREFERENCE2;
	npu_dbg("preference = %u\n",preference);
	return ret;
}
