// SPDX-License-Identifier: GPL-2.0-only
/*
 * IO Performance mode with UFS
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Authors:
 *	Kiwoong <kwmad.kim@samsung.com>
 */
#include <linux/of.h>
#include <linux/ems.h>
#include "ufs-exynos-perf.h"
#include "ufs-cal-if.h"
#include "ufs-exynos.h"
#if IS_ENABLED(CONFIG_EXYNOS_CPUPM)
#include <soc/samsung/exynos-cpupm.h>
#endif
#include <soc/samsung/bts.h>

#include <trace/events/ufs_exynos_perf.h>


/* control knob */
static int __ctrl_dvfs(struct ufs_perf *perf, ctrl_op op)
{
	struct ufs_perf_v1 *perf_v1 = &perf->stat_v1;
	int __maybe_unused i;
	struct cpumask __maybe_unused mask;
	int boost;

	trace_ufs_perf_lock("dvfs", op);
	if (op != CTRL_OP_UP && op != CTRL_OP_DOWN)
		return -1;

	boost = op == CTRL_OP_UP ? 1 : 0;

	emstune_set_sched_io_boost(boost);
	cpulist_parse(perf_v1->ecs_range[boost], &mask);
	ecs_request("ufs_perf", &mask, ECS_MIN);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	if (op == CTRL_OP_UP) {
		if (perf->val_pm_qos_int)
			exynos_pm_qos_update_request(&perf->pm_qos_int,
					perf->val_pm_qos_int);
		if (perf->val_pm_qos_mif)
			exynos_pm_qos_update_request(&perf->pm_qos_mif,
					perf->val_pm_qos_mif);
		if (perf_v1->bts_scen_idx != 0)
			bts_add_scenario(perf_v1->bts_scen_idx);
	} else {
		if (perf->val_pm_qos_int)
			exynos_pm_qos_update_request(&perf->pm_qos_int, 0);
		if (perf->val_pm_qos_mif)
			exynos_pm_qos_update_request(&perf->pm_qos_mif, 0);
		if (perf_v1->bts_scen_idx != 0)
			bts_del_scenario(perf_v1->bts_scen_idx);
	}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_CPUFREQ) || IS_ENABLED(CONFIG_FREQ_QOS_TRACER)
	for (i = 0; i < perf->num_clusters; i++) {
		struct cpufreq_policy *policy;

		policy = cpufreq_cpu_get(perf->clusters[i]);
		if (!policy)
			continue;

		if (!perf->pm_qos_cluster)
			return 0;

		if (!freq_qos_request_active(&perf->pm_qos_cluster[policy->cpu])) {
			pr_info("%s: unknown object[%d]\n", __func__, policy->cpu);
			continue;
		}

		if (op == CTRL_OP_UP)
			freq_qos_update_request(&perf->pm_qos_cluster[policy->cpu],
					perf->cluster_qos_value[i]);
		else
			freq_qos_update_request(&perf->pm_qos_cluster[policy->cpu], 0);

	}
#endif
	return 0;
}

/* policy */
static policy_res __policy_heavy(struct ufs_perf *perf, u32 i_policy)
{
	int index;
	ctrl_op op;
	static ctrl_op op_backup;
	unsigned long flags;
	struct ufs_perf_v1 *perf_v1 = &perf->stat_v1;
	policy_res res = R_OK;
	int mapped = 0;
	ufs_freq_sts state_seq = perf_v1->stats[CHUNK_SEQ].freq_state;
	ufs_freq_sts state_ran = perf_v1->stats[CHUNK_RAN].freq_state;

	/* for up, ORed. for down, ANDed */
	if (state_seq == FREQ_REACH || state_ran == FREQ_REACH) {
		op = CTRL_OP_UP;
		if (state_seq == FREQ_REACH)
			perf_v1->stats[CHUNK_SEQ].freq_state = FREQ_DWELL;
		if (state_ran == FREQ_REACH)
			perf_v1->stats[CHUNK_RAN].freq_state = FREQ_DWELL;
	} else if ((state_seq == FREQ_DROP &&
		    (state_ran == FREQ_DROP || state_ran == FREQ_RARE)) ||
		    (state_ran == FREQ_DROP &&
		    (state_seq == FREQ_DROP || state_seq == FREQ_RARE))) {
		op = CTRL_OP_DOWN;
		if (state_seq == FREQ_DROP)
			perf_v1->stats[CHUNK_SEQ].freq_state = FREQ_RARE;
		if (state_ran == FREQ_DROP)
			perf_v1->stats[CHUNK_RAN].freq_state = FREQ_RARE;
	} else
		op = CTRL_OP_NONE;

	/* put request */
	spin_lock_irqsave(&perf->lock_handle, flags);
	for (index = 0; index < __CTRL_REQ_MAX; index++) {
		if (!(BIT(index) & perf_v1->req_bits[i_policy]) ||
		    op == CTRL_OP_NONE || perf->ctrl_handle[index] == op)
			continue;
		if (op_backup == op)
			continue;

		perf->ctrl_handle[index] = op;
		op_backup = op;
		mapped++;
		//trace_ufs_perf_lock("ctrl_1", op);
	}
	spin_unlock_irqrestore(&perf->lock_handle, flags);
	if (op != CTRL_OP_NONE && mapped) {
		trace_ufs_perf_lock("ctrl", op);
		res = R_CTRL;
	}

	return res;
}

static policy_res (*__policy_set[__POLICY_MAX])(struct ufs_perf *perf,
		u32 i_policy) = {
	__policy_heavy,
};

static inline void __adjust_time(ufs_perf_stat_type *stat, s64 time)
{
	u32 th_count = stat->freq_state == FREQ_RARE ?
		stat->th_reach_count : stat->th_drop_count;

	if (stat->s_count > th_count)
		stat->s_time_start = time - stat->s_time_prev +
			stat->s_time_start;
	stat->s_time_prev = time;
}

#define CALC_DENSITY(c, d)	((c) * 100000 / (d))
#define GET_TIME_IN_US(ns)	((ns) / 1000)
static void __update_v1_queued(struct ufs_perf_v1 *perf_v1, u32 size)
{
	//__chuck_type chunk = size >= SZ_256K ? CHUNK_SEQ : CHUNK_RAN;
	__chuck_type chunk = size >= SZ_512K ? CHUNK_SEQ : CHUNK_RAN;
	int ret = 0;

	ufs_perf_stat_type *stat = &perf_v1->stats[chunk];
	s64 time = cpu_clock(raw_smp_processor_id());
	u64 interval;
	u64 th_interval;
	u64 duration;
	u32 density = 0;
	u32 count;

	/*
	 * There are only one case that we need to consider
	 *
	 * First, reset is intervened right after request done.
	 * In this situation, stat update will start again a little bit less
	 * than usual and this is not a big deal.
	 * For scenarios to require high throughput, a sort of tuning is
	 * required to prevent from frequent resets.
	 */
	del_timer(&perf_v1->reset_timer);
	count = ++stat->s_count;
	interval = GET_TIME_IN_US(time - stat->s_time_prev);
	duration = GET_TIME_IN_US(time - stat->s_time_start);
	stat->s_time_prev = time;
	switch (stat->freq_state) {
	case FREQ_RARE:
		ret = 1;
		th_interval = stat->th_reach_interval_in_us;
		if (interval > th_interval) {
			stat->s_time_start = time;
			stat->s_count = 0;
			ret = 2;
			break;
		}
		if (count < stat->th_reach_count) {
			ret = 3;
			break;
		}

		density = CALC_DENSITY(count, duration);
		if (density >= stat->th_reach_density) {
			stat->freq_state = FREQ_REACH;
			ret = 5;
		}
		break;
	case FREQ_DWELL:
		ret = 11;
		th_interval = stat->th_drop_interval_in_us;
		if (interval > th_interval) {
			stat->freq_state = FREQ_DROP;
			stat->s_count = 0;
			stat->s_time_start = time;
			ret = 12;
			break;
		}
		if (count < stat->th_drop_count) {
			ret = 13;
			break;
		}

		density = CALC_DENSITY(count, duration);
		if (density < stat->th_drop_density) {
			stat->freq_state = FREQ_DROP;
			stat->s_count = 0;
			stat->s_time_start = time;
			ret = 14;
		}
		break;
	case FREQ_DROP:
		ret = 21;
		th_interval = stat->th_drop_interval_in_us;
		if (interval > th_interval) {
			stat->s_count = 0;
			stat->s_time_start = time;
			ret = 22;
		}
		if (count < stat->th_drop_count) {
			ret = 23;
			break;
		}

		density = CALC_DENSITY(count, duration);
		if (density >= stat->th_drop_density) {
			stat->freq_state = FREQ_DWELL;
			ret = 24;
		}
		break;
	default:
		ret = 99;
		break;
	}

	trace_ufs_perf_update_v1("queued",
				 chunk,
				 count,
				 density,
				 stat->freq_state,
				 stat->s_time_start,
				 ret);
	perf_v1->chunk_prev = chunk;
	if (count == 1)
		stat->s_time_start = time;
}

static void __update_v1_reset(struct ufs_perf_v1 *perf_v1)
{
	//__update_v1_queued(stat, 0);

	mod_timer(&perf_v1->reset_timer,
		  jiffies + msecs_to_jiffies(perf_v1->th_reset_in_ms));
}

static policy_res __do_policy(struct ufs_perf *perf)
{
	struct ufs_perf_v1 *perf_v1 = &perf->stat_v1;
	policy_res res = R_OK;
	policy_res res_t;
	int index;

	for (index = 0; index < __POLICY_MAX; index++) {
		if (!(BIT(index) & perf_v1->policy_bits))
			continue;
		res_t = __policy_set[index](perf, index);
		if (res_t == R_CTRL)
			res = res_t;
	}

	return res;
}

static policy_res __update_v1(struct ufs_perf *perf, u32 qd, ufs_perf_op op,
		ufs_perf_entry entry)
{
	struct ufs_perf_v1 *perf_v1 = &perf->stat_v1;
	policy_res res = R_OK;
	unsigned long flags;

	/* sync case, freeze state for count */
	if (op == UFS_PERF_OP_S)
		return res;

	spin_lock_irqsave(&perf_v1->lock, flags);
	switch (entry) {
	case UFS_PERF_ENTRY_QUEUED:
		__update_v1_queued(perf_v1, qd);
		break;
	case UFS_PERF_ENTRY_RESET:
		__update_v1_reset(perf_v1);
		break;
	}

	/* do policy */
	res = __do_policy(perf);
	spin_unlock_irqrestore(&perf_v1->lock, flags);

	return res;
}

static void __reset_timer(struct timer_list *t)
{
	struct ufs_perf_v1 *perf_v1 = from_timer(perf_v1, t, reset_timer);
	struct ufs_perf *perf = container_of(perf_v1, struct ufs_perf, stat_v1);
	s64 time = cpu_clock(raw_smp_processor_id());
	unsigned long flags;
	int index;

	spin_lock_irqsave(&perf_v1->lock, flags);

	for (index = 0; index < CHUNK_NUM; index++) {
		perf_v1->stats[index].s_time_start = time;
		perf_v1->stats[index].s_time_prev = time;
		perf_v1->stats[index].freq_state = FREQ_DROP;
		perf_v1->stats[index].s_count = 0;
	}

	/* do policy */
	__do_policy(perf);
	spin_unlock_irqrestore(&perf_v1->lock, flags);

	/* wake-up handler */
	ufs_perf_wakeup(perf);

	for (index = 0; index < CHUNK_NUM; index++)
		trace_ufs_perf_update_v1("reset", index,
					perf_v1->stats[index].s_count,
					0,
					perf_v1->stats[index].freq_state,
					perf_v1->stats[index].s_time_start,
					0);
}

/* sysfs*/
struct __sysfs_attr {
	struct attribute attr;
	ssize_t (*show)(struct ufs_perf_v1 *perf_v1, char *buf);
	int (*store)(struct ufs_perf_v1 *perf_v1, u32 value);
};

#define __SYSFS_NODE(_name, _format)					\
static ssize_t __sysfs_##_name##_show(struct ufs_perf_v1 *perf_v1, char *buf)				\
{										\
	return snprintf(buf, PAGE_SIZE, _format,				\
			perf_v1->th_##_name);				\
};										\
static int __sysfs_##_name##_store(struct ufs_perf_v1 *perf_v1, u32 value)					\
{										\
	perf_v1->th_##_name = value;					\
										\
	return 0;								\
};										\
static struct __sysfs_attr __sysfs_node_##_name = {				\
	.attr = { .name = #_name, .mode = 0666 },				\
	.show = __sysfs_##_name##_show,						\
	.store = __sysfs_##_name##_store,					\
}

#define __SYSFS_NODE_SEQ(_name, _name_a, _format)					\
static ssize_t __sysfs_##_name##_show(struct ufs_perf_v1 *perf_v1, char *buf)				\
{										\
	return snprintf(buf, PAGE_SIZE, _format,				\
			perf_v1->stats[CHUNK_SEQ].th_##_name_a);				\
};										\
static int __sysfs_##_name##_store(struct ufs_perf_v1 *perf_v1, u32 value)					\
{										\
	perf_v1->stats[CHUNK_SEQ].th_##_name_a = value;					\
										\
	return 0;								\
};										\
static struct __sysfs_attr __sysfs_node_##_name = {				\
	.attr = { .name = #_name, .mode = 0666 },				\
	.show = __sysfs_##_name##_show,						\
	.store = __sysfs_##_name##_store,					\
}

#define __SYSFS_NODE_RAN(_name, _name_a, _format)					\
static ssize_t __sysfs_##_name##_show(struct ufs_perf_v1 *perf_v1, char *buf)				\
{										\
	return snprintf(buf, PAGE_SIZE, _format,				\
			perf_v1->stats[CHUNK_RAN].th_##_name_a);				\
};										\
static int __sysfs_##_name##_store(struct ufs_perf_v1 *perf_v1, u32 value)					\
{										\
	perf_v1->stats[CHUNK_RAN].th_##_name_a = value;					\
										\
	return 0;								\
};										\
static struct __sysfs_attr __sysfs_node_##_name = {				\
	.attr = { .name = #_name, .mode = 0666 },				\
	.show = __sysfs_##_name##_show,						\
	.store = __sysfs_##_name##_store,					\
}

__SYSFS_NODE(reset_in_ms, "%u\n");
__SYSFS_NODE(mixed_interval_in_us, "%u\n");

__SYSFS_NODE_SEQ(seq_reach_count, reach_count, "%u\n");
__SYSFS_NODE_SEQ(seq_reach_interval_in_us, reach_interval_in_us, "%lld\n");
__SYSFS_NODE_SEQ(seq_reach_density, reach_density, "%u\n");
__SYSFS_NODE_SEQ(seq_drop_count, drop_count, "%u\n");
__SYSFS_NODE_SEQ(seq_drop_interval_in_us, drop_interval_in_us, "%llu\n");
__SYSFS_NODE_SEQ(seq_drop_density, drop_density, "%u\n");

__SYSFS_NODE_RAN(ran_reach_count, reach_count, "%u\n");
__SYSFS_NODE_RAN(ran_reach_interval_in_us, reach_interval_in_us, "%lld\n");
__SYSFS_NODE_RAN(ran_reach_density, reach_density, "%u\n");
__SYSFS_NODE_RAN(ran_drop_count, drop_count, "%u\n");
__SYSFS_NODE_RAN(ran_drop_interval_in_us, drop_interval_in_us, "%llu\n");
__SYSFS_NODE_RAN(ran_drop_density, drop_density, "%u\n");

const static struct attribute *__sysfs_attrs[] = {
	&__sysfs_node_reset_in_ms.attr,
	&__sysfs_node_mixed_interval_in_us.attr,
	&__sysfs_node_seq_reach_count.attr,
	&__sysfs_node_seq_reach_interval_in_us.attr,
	&__sysfs_node_seq_reach_density.attr,
	&__sysfs_node_seq_drop_count.attr,
	&__sysfs_node_seq_drop_interval_in_us.attr,
	&__sysfs_node_seq_drop_density.attr,
	&__sysfs_node_ran_reach_count.attr,
	&__sysfs_node_ran_reach_interval_in_us.attr,
	&__sysfs_node_ran_reach_density.attr,
	&__sysfs_node_ran_drop_count.attr,
	&__sysfs_node_ran_drop_interval_in_us.attr,
	&__sysfs_node_ran_drop_density.attr,
	NULL,
};

static ssize_t __sysfs_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct ufs_perf_v1 *perf_v1 = container_of(kobj,
			struct ufs_perf_v1, sysfs_kobj);
	struct __sysfs_attr *param = container_of(attr,
			struct __sysfs_attr, attr);

	return param->show(perf_v1, buf);
}

static ssize_t __sysfs_store(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buf, size_t length)
{
	struct ufs_perf_v1 *perf_v1 = container_of(kobj,
			struct ufs_perf_v1, sysfs_kobj);
	struct __sysfs_attr *param = container_of(attr,
			struct __sysfs_attr, attr);
	u32 val;
	int ret = 0;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;

	ret = param->store(perf_v1, val);
	return (ret == 0) ? length : (ssize_t)ret;
}

static const struct sysfs_ops __sysfs_ops = {
	.show	= __sysfs_show,
	.store	= __sysfs_store,
};

static struct kobj_type __sysfs_ktype = {
	.sysfs_ops	= &__sysfs_ops,
	.release	= NULL,
};

static int __sysfs_init(struct ufs_perf_v1 *perf_v1)
{
	int error;

	/* create a path of /sys/kernel/ufs_perf_x */
	kobject_init(&perf_v1->sysfs_kobj, &__sysfs_ktype);
	error = kobject_add(&perf_v1->sysfs_kobj, kernel_kobj, "ufs_perf_v1_%c",
			(char)('0'));
	if (error) {
		pr_err("Fail to register sysfs directory: %d\n", error);
		goto fail_kobj;
	}

	/* create attributes */
	error = sysfs_create_files(&perf_v1->sysfs_kobj, __sysfs_attrs);
	if (error) {
		pr_err("Fail to create sysfs files: %d\n", error);
		goto fail_kobj;
	}

	return 0;

fail_kobj:
	kobject_put(&perf_v1->sysfs_kobj);
	return error;
}

static inline void __sysfs_exit(struct ufs_perf_v1 *perf_v1)
{
	kobject_put(&perf_v1->sysfs_kobj);
}

int ufs_perf_init_v1(struct ufs_perf *perf)
{
	struct ufs_perf_v1 *perf_v1 = &perf->stat_v1;
	int index;
	int res;

	/* register callbacks */
	perf->update[__UPDATE_V1] = __update_v1;
	perf->ctrl[__CTRL_REQ_DVFS] = __ctrl_dvfs;

	/* global stats */
	perf_v1->th_reset_in_ms = 150;
	perf_v1->th_mixed_interval_in_us = 30000;

	/* enable bits */
	perf_v1->policy_bits = POLICY_HEAVY;	//
	for (index = 0; index < __POLICY_MAX; index++) {
		if (!(BIT(index) & perf_v1->policy_bits))
			continue;
		perf_v1->req_bits[index] = CTRL_REQ_DVFS;	//
	}

	/* sysfs */
	res = __sysfs_init(perf_v1);

	/* sync */
	spin_lock_init(&perf_v1->lock);

	/* reset timer */
	timer_setup(&perf_v1->reset_timer, __reset_timer, 0);

	/* stats */
	for (index = 0; index < CHUNK_NUM; index++) {
		perf_v1->stats[index].s_time_start = cpu_clock(raw_smp_processor_id());
		perf_v1->stats[index].s_time_prev = perf_v1->stats[index].s_time_start;
		perf_v1->stats[index].freq_state = FREQ_RARE;
		perf_v1->stats[index].s_count = 0;
	}
	perf_v1->stats[CHUNK_SEQ].th_reach_count = 20;
	perf_v1->stats[CHUNK_SEQ].th_reach_interval_in_us = 10000;
	/* N * 10000 / us */
	perf_v1->stats[CHUNK_SEQ].th_reach_density = 500;
	perf_v1->stats[CHUNK_SEQ].th_drop_count = 5;
	perf_v1->stats[CHUNK_SEQ].th_drop_interval_in_us = 120000;
	/* N * 10000 / us */
	perf_v1->stats[CHUNK_SEQ].th_drop_density = 300;

	perf_v1->stats[CHUNK_RAN].th_reach_count = 50;
	perf_v1->stats[CHUNK_RAN].th_reach_interval_in_us = 5000;
	/* N * 10000 / us */
	perf_v1->stats[CHUNK_RAN].th_reach_density = 10000;
	perf_v1->stats[CHUNK_RAN].th_drop_count = 5;
	perf_v1->stats[CHUNK_RAN].th_drop_interval_in_us = 5000;
	/* N * 10000 / us */
	perf_v1->stats[CHUNK_RAN].th_drop_density = 5000;

#if defined(CONFIG_SOC_S5E8855)
	perf_v1->stats[CHUNK_SEQ].th_reach_count = 20;
	perf_v1->stats[CHUNK_SEQ].th_reach_density = 200;
	perf_v1->stats[CHUNK_SEQ].th_drop_count = 20;
	perf_v1->stats[CHUNK_SEQ].th_drop_density = 100;

	perf_v1->stats[CHUNK_RAN].th_drop_count = 50;
	perf_v1->stats[CHUNK_RAN].th_drop_interval_in_us = 10000;
#endif

	/* related to outside */
	ecs_request_register("ufs_perf", NULL, ECS_MIN);
	sprintf(perf_v1->ecs_range[0], "");
	sprintf(perf_v1->ecs_range[1], "0-%u", nr_cpu_ids - 1);

	return res;
}

void ufs_perf_exit_v1(struct ufs_perf *perf)
{
	struct ufs_perf_v1 *perf_v1 = &perf->stat_v1;

	/* sysfs */
	__sysfs_exit(perf_v1);

	ecs_request_unregister("ufs_perf", ECS_MIN);
}

MODULE_DESCRIPTION("Exynos UFS performance booster");
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
