/* linux/drivers/devfreq/exynos-devfreq.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS SoC series devfreq common driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/reboot.h>
#include <linux/suspend.h>
#include <linux/io.h>
#include <linux/sched/clock.h>
#include <linux/clk.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/bts.h>
#include <linux/of_platform.h>
#include "../soc/samsung/exynos/cal-if/acpm_dvfs.h"
#include <soc/samsung/exynos-pd.h>
#include <linux/cpumask.h>
#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/exynos/debug-snapshot.h>

#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-dm.h>
#if IS_ENABLED(CONFIG_EXYNOS_ACPM)
#include "../soc/samsung/exynos/acpm/acpm.h"
#include "../soc/samsung/exynos/acpm/acpm_ipc.h"
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif

#include "../../../../../common/drivers/devfreq/governor.h"

#if IS_ENABLED (CONFIG_EXYNOS_THERMAL_V2) && IS_ENABLED(CONFIG_DEV_THERMAL)
#include <soc/samsung/dev_cooling.h>
#endif

#include <sound/samsung/abox.h>

#include <kunit/visibility.h>

#include <trace/events/power.h>
#define CREATE_TRACE_POINTS
#include <trace/events/exynos_devfreq.h>
static struct exynos_devfreq_data **devfreq_data;
static u32 freq_array[6];
static u32 boot_array[2];

/**
 * exynos_devfreq_get_freq_level() - Lookup freq_table for the frequency
 * @devfreq:	the devfreq instance
 * @freq:	the target frequency
 */
static inline int exynos_devfreq_get_freq_level(struct devfreq *devfreq, unsigned long freq)
{
	int lev;

	if (!devfreq->profile->freq_table)
		return -ENODEV;

	for (lev = 0; lev < devfreq->profile->max_state; lev++)
		if (freq == devfreq->profile->freq_table[lev])
			return lev;

	return -EINVAL;
}

static void exynos_devfreq_call_trans_chain(struct exynos_devfreq_data *data,
					    unsigned long target_freq)
{
	struct exynos_devfreq_freqs freqs;

	freqs.new_freq = target_freq;
	freqs.old_freq = data->old_freq;
	freqs.new_lv = exynos_devfreq_get_freq_level(data->devfreq, target_freq);
	freqs.old_lv = exynos_devfreq_get_freq_level(data->devfreq,
								data->old_freq);

	srcu_notifier_call_chain(&data->trans_nh, data->devfreq_type, &freqs);
}

int exynos_devfreq_register_trans_notifier(struct device *dev,
					   struct notifier_block *nb)
{
	struct platform_device *pdev
		= container_of(dev, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	if (!data)
		return -ENODEV;

	ret = srcu_notifier_chain_register(&data->trans_nh, nb);
	if (ret)
		return ret;

	exynos_devfreq_call_trans_chain(data, data->old_freq);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_devfreq_register_trans_notifier);

int exynos_devfreq_unregister_trans_notifier(struct device *dev,
					     struct notifier_block *nb)
{
	struct platform_device *pdev
		= container_of(dev, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);

	if (!data)
		return -ENODEV;

	return srcu_notifier_chain_unregister(&data->trans_nh, nb);
}
EXPORT_SYMBOL_GPL(exynos_devfreq_unregister_trans_notifier);

/**
 * exynos_devfreq_update_status() - Update statistics of devfreq behavior
 * @devfreq:	the devfreq instance
 * @freq:	the update target frequency
 */
static int exynos_devfreq_update_status(struct exynos_devfreq_data *devdata)
{
	int prev_lev, ret = 0;
	unsigned long cur_time;
	struct devfreq *devfreq;
	unsigned long flags;

	spin_lock_irqsave(&devdata->update_status_lock, flags);

	devfreq = devdata->devfreq;

	cur_time = jiffies;

	/* Immediately exit if old_freq is not initialized yet. */
	if (!devdata->old_freq)
		goto out;

	prev_lev = exynos_devfreq_get_freq_level(devfreq, devdata->old_freq);
	if (prev_lev < 0) {
		ret = prev_lev;
		goto out;
	}

	devdata->time_in_state[prev_lev] +=
			 cur_time - devdata->last_stat_updated;

out:
	devdata->last_stat_updated = cur_time;
	spin_unlock_irqrestore(&devdata->update_status_lock, flags);

	return ret;
}

/* Functions for DVFS Manager (include ESCA) */
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
static inline unsigned int ect_find_constraint_freq(struct ect_minlock_domain *ect_domain,
					unsigned int freq)
{
	unsigned int i;

	for (i = 0; i < ect_domain->num_of_level; i++)
		if (ect_domain->level[i].main_frequencies == freq)
			break;

	return ect_domain->level[i].sub_frequencies;
}

int exynos_devfreq_get_recommended_freq(struct device *dev,
					unsigned long *target_freq, u32 flags)
{
	struct dev_pm_opp *target_opp;
	int ret = 0;

	target_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR(target_opp)) {
		dev_err(dev, "not found valid OPP table\n");
		ret = PTR_ERR(target_opp);
		goto out;
	}

	*target_freq = dev_pm_opp_get_freq(target_opp);
	dev_pm_opp_put(target_opp);

out:
	return ret;
}

static inline int exynos_devfreq_set_freq_pre(struct exynos_devfreq_data *data,
		unsigned long *target_freq, int flag)
{
	int ret = 0;

	ret = exynos_devfreq_get_recommended_freq(data->dev, target_freq, 0);
	if (ret)
		goto out;

	data->new_freq = (u32)(*target_freq);

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	if (data->old_freq == data->new_freq) {
		ret = 1;
		goto out;
	}
#endif

	/* Trace before freq scaling */
	trace_exynos_devfreq(data->devfreq_type, data->old_freq, data->new_freq, flag);
#if !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	dbg_snapshot_freq(data->ess_flag, data->old_freq, data->new_freq, flag);
#endif

	if (data->pm_domain) {
		if (!exynos_pd_status(data->pm_domain)) {
			dev_err(data->dev, "power domain %s is offed\n", data->pm_domain->name);
			return -EINVAL;
		}
	}

out:
	return ret;

}
static inline int exynos_devfreq_set_freq(struct exynos_devfreq_data *data,
		unsigned long *target_freq)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER) && IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	u32 old_freq = data->old_freq;
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	ret = DM_CALL(data->dm_type, target_freq);

	data->old_freq = *target_freq;

#if !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	dbg_snapshot_freq(data->ess_flag, old_freq, data->old_freq, DSS_FLAG_OUT);
#endif
#elif IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	ret = cal_dfs_set_rate(data->dfs_id, (unsigned long)*target_freq);
#endif

	if (ret) {
		dev_err(data->dev, "failed set frequency to CAL (%uKhz)\n",
				data->new_freq);
		ret = -EINVAL;
	}

	return ret;
}

static inline void exynos_devfreq_set_freq_post(struct exynos_devfreq_data *data,
		unsigned long *target_freq, int flag, unsigned int relation)
{
	int ret;

	/* Trace after freq scaling */
#if !IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
	dbg_snapshot_freq(data->ess_flag, data->old_freq, *target_freq, flag);
#endif
	trace_exynos_devfreq(data->devfreq_type, data->old_freq, *target_freq, flag);
	trace_clock_set_rate(dev_name(data->dev), *target_freq, raw_smp_processor_id());

	ret = exynos_devfreq_update_status(data);
	if (ret)
		dev_err(data->dev,"failed to update_status (%d), %u KHz\n",
			ret, data->old_freq);

	exynos_devfreq_call_trans_chain(data, *target_freq);

	WRITE_ONCE(data->old_freq, *target_freq);
}

static int exynos_devfreq_target(struct exynos_devfreq_data *data,
		unsigned long *target_freq)
{
	int ret = -EAGAIN;

	if (data->devfreq_disabled)
		goto out;

	/* Pre-process for scaling frequency */
	ret = exynos_devfreq_set_freq_pre(data, target_freq, DSS_FLAG_IN);
	if (ret)
		goto out;

	/* Scaling frequency */
	ret = exynos_devfreq_set_freq(data, target_freq);
	if (ret)
		goto out;

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	/* Post-process for scaling freqeucny */
	exynos_devfreq_set_freq_post(data, target_freq, DSS_FLAG_OUT, 0);
#endif

out:
	if (ret < 0)
		dev_err(data->dev, "failed set frequency (%uKhz --> %uKhz)\n",
			data->old_freq, data->new_freq);
	else
		ret = 0;

	return ret;
}

static int devfreq_frequency_scaler(int dm_type, void *devdata,
				u32 target_freq, unsigned int relation)
{
	struct exynos_devfreq_data *data = devdata;
	struct devfreq *devfreq = data->devfreq;
	int err = 0;
	unsigned long freq = target_freq;

	if (IS_ERR_OR_NULL(devfreq)) {
		pr_err("%s: No such devfreq for dm_type(%d)\n", __func__, dm_type);
		err = -ENODEV;
		goto err_out;
	}

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	mutex_lock(&data->lock);

	err = exynos_devfreq_target(data, &freq);

	mutex_unlock(&data->lock);
#elif IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	exynos_devfreq_set_freq_post(data, &freq, DSS_FLAG_ON, relation);
#endif

err_out:

	return err;
}
#endif
/* End of Functions for DVFS Manager (include ESCA) */

/* Callback functions linked to Kernel DEVFREQ Framework */
static int exynos_devfreq_dm_call(struct device *parent, unsigned long *target_freq, u32 flags)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);

	/* Check condition */
	if (data->devfreq_disabled) {
		ret = -EAGAIN;
		goto out;
	}

	data->new_freq = *target_freq;

#endif
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	exynos_devfreq_target(data, target_freq);

#elif IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	DM_CALL(data->dm_type, target_freq);
#endif

	*target_freq = data->old_freq;
out:
	return ret;
}

static int exynos_devfreq_get_cur_freq(struct device *dev,
					    unsigned long *freq)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	*freq = data->old_freq;

	return ret;
}
/* End of callback functions linked to Kernel DEVFREQ Framework */

int exynos_devfreq_get_freq_infos(struct device *dev,
				   struct exynos_devfreq_freq_infos *infos)
{
	struct platform_device *pdev =
		container_of(dev, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int i;

	if (!data)
		return -ENODEV;

	mutex_lock(&data->lock);
	infos->pm_qos_class = data->pm_qos_class;
	infos->pm_qos_class_max = data->pm_qos_class_max;
	infos->max_state = data->max_state;

	infos->freq_table = kzalloc(sizeof(*(infos->freq_table))
				    * data->max_state, GFP_KERNEL);
	if (!infos->freq_table || !data->opp_list) {
		mutex_unlock(&data->lock);
		return -ENOMEM;
	}

	for (i = 0; i < data->max_state; i++)
		infos->freq_table[i] = data->opp_list[i].rate;

	infos->cur_freq = data->old_freq;
	infos->max_freq = infos->freq_table[0];
	infos->min_freq = infos->freq_table[data->max_state - 1];
	mutex_unlock(&data->lock);

	return 0;
}
EXPORT_SYMBOL(exynos_devfreq_get_freq_infos);

static int exynos_constraint_parse(struct exynos_devfreq_data *data,
		unsigned int min_freq, unsigned int max_freq)
{
	struct device_node *np, *child;
	u32 num_child, constraint_dm_type, constraint_type;
	const char *devfreq_domain_name;
	int i = 0, j, const_flag = 1;
	void *min_block, *dvfs_block;
	struct ect_dvfs_domain *dvfs_domain;
	struct ect_minlock_domain *ect_domain;
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	struct exynos_dm_freq *const_table;
#endif
	const char *master_dm_name;
	np = of_get_child_by_name(data->dev->of_node, "skew");
	if (!np)
		return 0;
	num_child = of_get_child_count(np);
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	data->nr_constraint = num_child;
	data->constraint = kzalloc(sizeof(struct exynos_dm_constraint *) * num_child, GFP_KERNEL);
#endif
	if (of_property_read_string(data->dev->of_node, "devfreq_domain_name", &devfreq_domain_name))
		return -ENODEV;

	dvfs_block = ect_get_block(BLOCK_DVFS);
	if (dvfs_block == NULL)
		return -ENODEV;

	dvfs_domain = ect_dvfs_get_domain(dvfs_block, (char *)devfreq_domain_name);
	if (dvfs_domain == NULL)
		return -ENODEV;

	/* Although there is not any constraint, MIF table should be sent to FVP */
	min_block = ect_get_block(BLOCK_MINLOCK);
	if (min_block == NULL) {
		dev_info(data->dev, "There is not a min block in ECT\n");
		const_flag = 0;
	}

	for_each_available_child_of_node(np, child) {
		int use_level = 0;
		ect_domain = NULL;

		if (!of_property_read_string(child, "master_dm_name", &master_dm_name)) {
			dev_info(data->dev, "master_dm_name: %s\n", master_dm_name);
			ect_domain = ect_minlock_get_domain(min_block, (char *)master_dm_name);
		} else {
			dev_info(data->dev, "master_dm_name: %s\n", devfreq_domain_name);
			ect_domain = ect_minlock_get_domain(min_block, (char *)devfreq_domain_name);
		}

		if (ect_domain == NULL) {
			dev_info(data->dev, "There is not a domain in min block\n");
			const_flag = 0;
		}

		if (of_property_read_u32(child, "constraint_dm_type", &constraint_dm_type))
			return -ENODEV;
		if (of_property_read_u32(child, "constraint_type", &constraint_type))
			return -ENODEV;
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
		if (const_flag) {
			data->constraint[i] =
				kzalloc(sizeof(struct exynos_dm_constraint), GFP_KERNEL);
			if (data->constraint[i] == NULL) {
				dev_err(data->dev, "failed to allocate constraint\n");
				return -ENOMEM;
			}

			const_table = kzalloc(sizeof(struct exynos_dm_freq) * ect_domain->num_of_level, GFP_KERNEL);
			if (const_table == NULL) {
				dev_err(data->dev, "failed to allocate constraint\n");
				kfree(data->constraint[i]);
				return -ENOMEM;
			}

			data->constraint[i]->guidance = true;
			data->constraint[i]->constraint_type = constraint_type;
			data->constraint[i]->dm_slave = constraint_dm_type;
			data->constraint[i]->table_length = ect_domain->num_of_level;
			data->constraint[i]->freq_table = const_table;
		}
#endif
		for (j = 0; j < dvfs_domain->num_of_level; j++) {
			if (data->opp_list[j].rate > max_freq ||
					data->opp_list[j].rate < min_freq)
				continue;

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
			if (const_flag) {
				const_table[use_level].master_freq = data->opp_list[j].rate;
				const_table[use_level].slave_freq
					= ect_find_constraint_freq(ect_domain, data->opp_list[j].rate);
			}
#endif
			use_level++;
		}
		i++;
	}
	return 0;
}

VISIBLE_IF_KUNIT int exynos_devfreq_reboot(struct exynos_devfreq_data *data)
{
	if (exynos_pm_qos_request_active(&data->default_pm_qos_min))
		exynos_pm_qos_update_request(&data->default_pm_qos_min,
				data->reboot_freq);
	return 0;

}
EXPORT_SYMBOL_IF_KUNIT(exynos_devfreq_reboot);

static unsigned long exynos_devfreq_freq_mapping(struct exynos_devfreq_data *data,
					  unsigned long freq)
{
	int i, abs_tmp, min_idx = 0, min_val = INT_MAX;

	for (i = 0; i < data->max_state; i++) {
		abs_tmp = abs((int)data->opp_list[i].rate - (int)freq);
		if (abs_tmp <= min_val) {
			min_val = abs_tmp;
			min_idx = i;
		} else {
			break;
		}
	}
	return data->opp_list[min_idx].rate;
}

static unsigned long _exynos_devfreq_get_freq(unsigned int devfreq_type)
{
	struct exynos_devfreq_data *data = NULL;
	unsigned long freq;

	if (devfreq_data)
		data = devfreq_data[devfreq_type];

	if (!data) {
		printk("Fail to get exynos_devfreq_data\n");
		return 0;
	}

	if (data->clk) {
		if (preemptible() && !in_interrupt()) {
			if (devfreq_type == DEVFREQ_MIF)
				freq = (clk_get_rate(data->clk) / 1000) / 2;
			else
				freq = (clk_get_rate(data->clk) / 1000);
			freq = exynos_devfreq_freq_mapping(data, freq);
		} else {
			freq = data->old_freq;
		}
	} else {
		freq = cal_dfs_get_rate(data->dfs_id);
	}

	if ((u32)freq == 0) {
		if (data->clk)
			dev_err(data->dev, "failed get frequency from clock framework\n");
		else
			dev_err(data->dev, "failed get frequency from CAL\n");

		freq = data->old_freq;
	}

	return freq;
}

unsigned long exynos_devfreq_get_domain_freq(unsigned int devfreq_type)
{
	struct exynos_devfreq_data *data = devfreq_data[devfreq_type];

	if (!data)
		return 0;

	return data->old_freq;
}
EXPORT_SYMBOL(exynos_devfreq_get_domain_freq);

static int exynos_devfreq_get_freq(struct device *dev, u32 *cur_freq,
		struct clk *clk, struct exynos_devfreq_data *data)
{
	if (data->pm_domain) {
		if (!exynos_pd_status(data->pm_domain)) {
			dev_err(dev, "power domain %s is offed\n", data->pm_domain->name);
			*cur_freq = 0;
			return -EINVAL;
		}
	}

	*cur_freq = (u32)_exynos_devfreq_get_freq(data->devfreq_type);

	if (*cur_freq == 0) {
		dev_err(dev, "failed get frequency\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos_devfreq_init_freq_table(struct exynos_devfreq_data *data)
{
	u32 max_freq, min_freq;
	unsigned long tmp_max, tmp_min;
	struct dev_pm_opp *target_opp;
	u32 flags = 0;
	int i, ret;

	max_freq = (u32)cal_dfs_get_max_freq(data->dfs_id);
	if (!max_freq) {
		dev_err(data->dev, "failed get max frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "max_freq: %uKhz, get_max_freq: %uKhz\n",
			data->max_freq, max_freq);

	if (max_freq < data->max_freq) {
		flags |= DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_max = (unsigned long)max_freq;
		target_opp = devfreq_recommended_opp(data->dev, &tmp_max, flags);
		if (IS_ERR(target_opp)) {
			dev_err(data->dev, "not found valid OPP for max_freq\n");
			return PTR_ERR(target_opp);
		}

		data->max_freq = (u32)dev_pm_opp_get_freq(target_opp);
		dev_pm_opp_put(target_opp);
	}

	/* min ferquency must be equal or under max frequency */
	if (data->min_freq > data->max_freq)
		data->min_freq = data->max_freq;

	min_freq = (u32)cal_dfs_get_min_freq(data->dfs_id);
	if (!min_freq) {
		dev_err(data->dev, "failed get min frequency\n");
		return -EINVAL;
	}

	dev_info(data->dev, "min_freq: %uKhz, get_min_freq: %uKhz\n",
			data->min_freq, min_freq);

	if (min_freq > data->min_freq) {
		flags &= ~DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_min = (unsigned long)min_freq;
		target_opp = devfreq_recommended_opp(data->dev, &tmp_min, flags);
		if (IS_ERR(target_opp)) {
			dev_err(data->dev, "not found valid OPP for min_freq\n");
			return PTR_ERR(target_opp);
		}

		data->min_freq = (u32)dev_pm_opp_get_freq(target_opp);
		dev_pm_opp_put(target_opp);
	}

	dev_info(data->dev, "min_freq: %uKhz, max_freq: %uKhz\n",
			data->min_freq, data->max_freq);

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].rate > data->max_freq ||
			data->opp_list[i].rate < data->min_freq)
			dev_pm_opp_disable(data->dev, (unsigned long)data->opp_list[i].rate);
	}

	data->devfreq_profile.initial_freq = cal_dfs_get_boot_freq(data->dfs_id);
	data->suspend_freq = cal_dfs_get_resume_freq(data->dfs_id);

	ret = exynos_constraint_parse(data, min_freq, max_freq);
	if (ret) {
		dev_err(data->dev, "failed to parse constraint table\n");
		return -EINVAL;
	}

	if (data->use_acpm) {
		ret = exynos_acpm_set_init_freq(data->dfs_id, data->devfreq_profile.initial_freq);
		if (ret) {
			dev_err(data->dev, "failed to set init freq\n");
			return -EINVAL;
		}
	}

	return 0;
}

#if defined(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG) || defined(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG_MODULE)
static ssize_t show_exynos_devfreq_info(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int i;

	count = snprintf(buf, PAGE_SIZE, "[Exynos DEVFREQ Data]\n"
			 "devfreq dev name : %20s\n"
			 "devfreq type     : %20d\n"
			 "Exynos SS flag   : %20u\n",
			 dev_name(data->dev), data->devfreq_type, data->ess_flag);

	count += snprintf(buf + count, PAGE_SIZE, "\n<Frequency data>\n"
			  "OPP list length  : %20u\n", data->max_state);
	count += snprintf(buf + count, PAGE_SIZE, "freq opp table\n");
	count += snprintf(buf + count, PAGE_SIZE, "\t  idx      freq       volt\n");

	for (i = 0; i < data->max_state; i++)
		count += snprintf(buf + count, PAGE_SIZE, "\t%5u %10lu %10u\n",
				  i, data->opp_list[i].rate,
				  data->opp_list[i].volt);

	count += snprintf(buf + count, PAGE_SIZE,
			  "default_qos     : %20u\n" "initial_freq    : %20lu\n"
			  "min_freq        : %20u\n" "max_freq        : %20u\n"
			  "boot_timeout(s) : %20u\n" "max_state       : %20u\n",
			  data->default_qos, data->devfreq_profile.initial_freq,
			  data->min_freq, data->max_freq, data->boot_qos_timeout, data->max_state);

	count += snprintf(buf + count, PAGE_SIZE, "\n<Governor data>\n");
	count += snprintf(buf + count, PAGE_SIZE,
			  "governor_name   : %20s\n",
			  data->governor_name);
	return count;
}

static ssize_t show_exynos_devfreq_get_freq(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 get_freq = 0;

	if (exynos_devfreq_get_freq(data->dev, &get_freq, data->clk, data))
		dev_err(data->dev, "failed get freq\n");

	count = snprintf(buf, PAGE_SIZE, "%10u Khz\n", get_freq);

	return count;
}

static ssize_t show_debug_scaling_devfreq_max(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int val;

	if (data->pm_qos_class_max) {
		val = exynos_pm_qos_read_req_value(data->pm_qos_class_max, &data->debug_pm_qos_max);
		if (val < 0) {
			dev_err(dev, "failed to read requested value\n");
			return count;
		}
		count += snprintf(buf, PAGE_SIZE, "%d\n", val);
	}

	return count;
}

static ssize_t store_debug_scaling_devfreq_max(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 qos_value;

	ret = sscanf(buf, "%u", &qos_value);
	if (ret != 1)
		return -EINVAL;

	if (data->pm_qos_class_max) {
		if (exynos_pm_qos_request_active(&data->debug_pm_qos_max))
			exynos_pm_qos_update_request(&data->debug_pm_qos_max, qos_value);
	}

	return count;
}

static ssize_t show_debug_scaling_devfreq_min(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int val;

	val = exynos_pm_qos_read_req_value(data->pm_qos_class, &data->debug_pm_qos_min);
	if (val < 0) {
		dev_err(dev, "failed to read requested value\n");
		return count;
	}

	count += snprintf(buf, PAGE_SIZE, "%d\n", val);

	return count;
}

static ssize_t store_debug_scaling_devfreq_min(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 qos_value;

	ret = sscanf(buf, "%u", &qos_value);
	if (ret != 1)
		return -EINVAL;

	if (exynos_pm_qos_request_active(&data->debug_pm_qos_min))
		exynos_pm_qos_update_request(&data->debug_pm_qos_min, qos_value);

	return count;
}

static DEVICE_ATTR(exynos_devfreq_info, 0640, show_exynos_devfreq_info, NULL);
static DEVICE_ATTR(exynos_devfreq_get_freq, 0640, show_exynos_devfreq_get_freq, NULL);
static DEVICE_ATTR(debug_scaling_devfreq_min, 0640, show_debug_scaling_devfreq_min, store_debug_scaling_devfreq_min);
static DEVICE_ATTR(debug_scaling_devfreq_max, 0640, show_debug_scaling_devfreq_max,
						store_debug_scaling_devfreq_max);

static struct attribute *exynos_devfreq_sysfs_entries[] = {
	&dev_attr_exynos_devfreq_info.attr,
	&dev_attr_exynos_devfreq_get_freq.attr,
	&dev_attr_debug_scaling_devfreq_min.attr,
	&dev_attr_debug_scaling_devfreq_max.attr,
	NULL,
};

static struct attribute_group exynos_devfreq_attr_group = {
	.name = "exynos_data",
	.attrs = exynos_devfreq_sysfs_entries,
};
#endif

static ssize_t show_scaling_devfreq_min(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int val;

	val = exynos_pm_qos_read_req_value(data->pm_qos_class, &data->sys_pm_qos_min);
	if (val < 0) {
		dev_err(dev, "failed to read requested value\n");
		return count;
	}

	count += snprintf(buf, PAGE_SIZE, "%d\n", val);

	return count;
}

static ssize_t store_scaling_devfreq_min(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 qos_value;

	ret = sscanf(buf, "%u", &qos_value);
	if (ret != 1)
		return -EINVAL;

	if (exynos_pm_qos_request_active(&data->sys_pm_qos_min))
		exynos_pm_qos_update_request(&data->sys_pm_qos_min, qos_value);

	return count;
}

static DEVICE_ATTR(scaling_devfreq_min, 0640, show_scaling_devfreq_min, store_scaling_devfreq_min);

static ssize_t show_scaling_devfreq_max(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int val;

	val = exynos_pm_qos_read_req_value(data->pm_qos_class_max, &data->sys_pm_qos_max);
	if (val < 0) {
		dev_err(dev, "failed to read requested value\n");
		return count;
	}

	count += snprintf(buf, PAGE_SIZE, "%d\n", val);

	return count;
}

static ssize_t store_scaling_devfreq_max(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct device *parent = dev->parent;
	struct platform_device *pdev = container_of(parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret;
	u32 qos_value;

	ret = sscanf(buf, "%u", &qos_value);
	if (ret != 1)
		return -EINVAL;

	if (exynos_pm_qos_request_active(&data->sys_pm_qos_max))
		exynos_pm_qos_update_request(&data->sys_pm_qos_max, qos_value);

	return count;
}

static DEVICE_ATTR(scaling_devfreq_max, 0640, show_scaling_devfreq_max, store_scaling_devfreq_max);

static ssize_t time_in_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev->parent, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	struct devfreq *devfreq = to_devfreq(dev);
	ssize_t len = 0;
	int i, err;
	unsigned int max_state = devfreq->profile->max_state;

	mutex_lock(&data->lock);
	err = exynos_devfreq_update_status(data);
	if (err) {
		mutex_unlock(&data->lock);
		return 0;
	}

	for (i = 0; i < max_state; i++) {
		len += sprintf(buf + len, "%8lu",
				devfreq->profile->freq_table[i]);
		len += sprintf(buf + len, "%10u\n",
			jiffies_to_msecs(data->time_in_state[i]));
	}
	mutex_unlock(&data->lock);
	return len;
}
static DEVICE_ATTR_RO(time_in_state);


#ifdef CONFIG_OF
#if IS_ENABLED(CONFIG_ECT)
static int exynos_devfreq_parse_ect(struct exynos_devfreq_data *data, const char *dvfs_domain_name)
{
	int cal_id = data->dfs_id, i;

	data->max_state = cal_dfs_get_lv_num(cal_id);
	data->opp_list = kzalloc(sizeof(struct dvfs_rate_volt) * data->max_state, GFP_KERNEL);
	if (!data->opp_list) {
		pr_err("%s: failed to allocate opp_list\n", __func__);
		return -ENOMEM;
	}

	if (!cal_dfs_get_rate_asv_table(cal_id, data->opp_list)) {
		pr_err("%s: failed to get fv table from CAL\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < data->max_state; i++) {
		pr_info("%s: freq %lu volt %u\n", __func__, data->opp_list[i].rate, data->opp_list[i].volt);
	}

	return 0;
}
#endif

static int exynos_devfreq_parse_dt(struct device_node *np, struct exynos_devfreq_data *data)
{
	const char *use_acpm;
	const char *devfreq_domain_name;
	const char *buf;
	const char *pd_name;
	const char *use_dtm;
#if IS_ENABLED(CONFIG_ECT)
	int not_using_ect = true;
#endif
	int ess_flag;
	if (!np)
		return -ENODEV;

	if (of_property_read_u32(np, "devfreq_type", &data->devfreq_type))
		return -ENODEV;
	if (of_property_read_u32(np, "pm_qos_class", &data->pm_qos_class))
		return -ENODEV;
	if (of_property_read_u32(np, "pm_qos_class_max", &data->pm_qos_class_max))
		return -ENODEV;

	if (of_property_read_u32(np, "dfs_id", &data->dfs_id) &&
			of_property_match_string(np, "clock-names", buf))
		return -ENODEV;

	if (of_property_read_string(np, "devfreq_domain_name", &devfreq_domain_name))
		return -ENODEV;
	ess_flag = dbg_snapshot_get_freq_idx(devfreq_domain_name);
	if (ess_flag != -EFAULT)
		data->ess_flag = ess_flag;
#if IS_ENABLED(CONFIG_ECT)
	not_using_ect = exynos_devfreq_parse_ect(data, devfreq_domain_name);
	if (not_using_ect) {
		dev_err(data->dev, "cannot parse the DVFS info in ECT");
		return -ENODEV;
	}
#endif
	if (of_property_read_string(np, "pd_name", &pd_name)) {
		dev_info(data->dev, "no power domain\n");
		data->pm_domain = NULL;
	} else {
		dev_info(data->dev, "power domain: %s\n", pd_name);
		data->pm_domain = exynos_pd_lookup_name(pd_name);
	}

	data->clk = devm_clk_get(data->dev, "DEVFREQ");
	if (data->clk && !IS_ERR(data->clk))
		dev_info(data->dev, "%s clock info exist\n", devfreq_domain_name);
	else
		data->clk = NULL;

	if (of_property_read_u32_array(np, "freq_info", (u32 *)&freq_array,
				       (size_t)(ARRAY_SIZE(freq_array))))
		return -ENODEV;

	data->devfreq_profile.initial_freq = freq_array[0];
	data->default_qos = freq_array[1];
	data->suspend_freq = freq_array[2];
	data->min_freq = freq_array[3];
	data->max_freq = freq_array[4];
	data->reboot_freq = freq_array[5];

	if (of_property_read_u32_array(np, "boot_info", (u32 *)&boot_array,
				       (size_t)(ARRAY_SIZE(boot_array)))) {
		data->boot_qos_timeout = 0;
		data->boot_freq = 0;
		dev_info(data->dev, "This doesn't use boot value\n");
	} else {
		data->boot_qos_timeout = boot_array[0];
		data->boot_freq = boot_array[1];
	}

	if(of_property_read_string(np, "governor", &data->governor_name)) {
		return -EINVAL;
	}

	if (!of_property_read_string(np, "use_acpm", &use_acpm)) {
		if (!strcmp(use_acpm, "true")) {
			data->use_acpm = true;
		} else {
			data->use_acpm = false;
			dev_info(data->dev, "This does not use acpm\n");
		}
	} else {
		dev_info(data->dev, "This does not use acpm\n");
		data->use_acpm = false;
	}

	if (!of_property_read_string(np, "use_dtm", &use_dtm)) {
		if (!strcmp(use_dtm, "true")) {
			data->use_dtm = true;
			dev_info(data->dev, "This domain controlled by DTM\n");
		} else {
			data->use_dtm = false;
		}
	} else {
		data->use_dtm = false;
	}

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	if (of_property_read_u32(np, "dm-index", &data->dm_type)) {
		dev_err(data->dev, "not support dvfs manager\n");
		return -ENODEV;
	}
#endif
	return 0;
}

#else
static int exynos_devfreq_parse_dt(struct device_node *np, struct exynos_devfrq_data *data)
{
	return -EINVAL;
}
#endif

static int exynos_init_freq_table(struct exynos_devfreq_data *data)
{
	int i, ret;
	u32 freq, volt;

	for (i = 0; i < data->max_state; i++) {
		freq = data->opp_list[i].rate;
		volt = data->opp_list[i].volt;

		data->devfreq_profile.freq_table[i] = freq;

		ret = dev_pm_opp_add(data->dev, freq, volt);
		if (ret) {
			dev_err(data->dev, "failed to add opp entries %uKhz\n", freq);
			return ret;
		} else {
			dev_info(data->dev, "DEVFREQ : %8uKhz, %8uuV\n", freq, volt);
		}
	}

	ret = exynos_devfreq_init_freq_table(data);
	if (ret) {
		dev_err(data->dev, "failed init frequency table\n");
		return ret;
	}

	return 0;
}

static int exynos_devfreq_reboot_notifier(struct notifier_block *nb, unsigned long val, void *v)
{
	struct exynos_devfreq_data *data = container_of(nb, struct exynos_devfreq_data,
							reboot_notifier);

	if (exynos_pm_qos_request_active(&data->default_pm_qos_min))
		exynos_pm_qos_update_request(&data->default_pm_qos_min, data->reboot_freq);

	if (exynos_devfreq_reboot(data)) {
		dev_err(data->dev, "failed reboot\n");
		return NOTIFY_BAD;
	}

	return NOTIFY_OK;
}

static int exynos_devfreq_pm_qos_notifier(struct notifier_block *nb, unsigned long val, void *v)
{
	struct devfreq_notifier_block *nb_container = container_of(nb, struct devfreq_notifier_block, nb);
	struct devfreq *devfreq = nb_container->df;
	int ret;

	mutex_lock(&devfreq->lock);
	ret = update_devfreq(devfreq);
	if (ret)
		dev_err(&devfreq->dev,
		   "%s: failed to update devfreq (ret: %d)\n", __func__, ret);
	mutex_unlock(&devfreq->lock);

	return NOTIFY_OK;
}

#if IS_ENABLED(CONFIG_EXYNOS_ACPM) || IS_ENABLED(CONFIG_EXYNOS_ESCA)
/*
 * If you change the release to false,
 * In FVP, the process of checking policy_max is skipped.
 * Initial value of release flag is true.
 * "true" means state of AP is running
 * "false means state of AP is sleep.
 */
static int exynos_devfreq_fvp_policy_release(struct exynos_devfreq_data *data, bool release)
{
	int size, ch_num;
	unsigned int cmd[4];
	struct ipc_config config;
	int ret = 0;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	ret = esca_ipc_request_channel(data->dev->of_node, NULL, &ch_num, &size);
#else
	ret = acpm_ipc_request_channel(data->dev->of_node, NULL, &ch_num, &size);
#endif
	if (ret) {
		dev_err(data->dev, "acpm request channel is failed, id:%u, size:%u\n", ch_num, size);
		return -EINVAL;
	}

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = data->devfreq_type;
	config.cmd[1] = release;
	config.cmd[2] = DATA_INIT;
	config.cmd[3] = RELEASE;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	ret = esca_ipc_send_data(ch_num, &config);
#else
	ret = acpm_ipc_send_data(ch_num, &config);
#endif
	if (ret) {
		dev_err(data->dev, "failed to send release infomation to FVP");
		return -EINVAL;
	}
	return 0;
}
#endif

static int exynos_devfreq_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret = 0;
	u32 get_freq = 0;

	if (data->use_acpm) {
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
		unsigned long req_freq = 0;

		mutex_lock(&data->devfreq->lock);

#if IS_ENABLED(CONFIG_EXYNOS_ACPM) || IS_ENABLED(CONFIG_EXYNOS_ESCA)
		ret = exynos_devfreq_fvp_policy_release(data, false);
		if (ret) {
			mutex_unlock(&data->devfreq->lock);
			return ret;
		}
#endif
		if (abox_is_on()) {
			if (data->devfreq_type == DEVFREQ_MIF)
				data->suspend_req = (unsigned long)abox_get_requiring_mif_freq_in_khz();
			else if (data->devfreq_type == DEVFREQ_INT)
				data->suspend_req = (unsigned long)abox_get_requiring_int_freq_in_khz();
		}

		if (data->suspend_req > data->suspend_freq)
			dev_info(dev, "devfreq_type:%d, changed str_freq by abox:%lu\n",
					data->devfreq_type, req_freq);
		else
			data->suspend_req = data->suspend_freq;

		ret = update_devfreq(data->devfreq);
		if (ret && ret != -EAGAIN) {
			dev_err(&data->devfreq->dev, "devfreq failed with (%d) error\n", ret);
			mutex_unlock(&data->devfreq->lock);
			return NOTIFY_BAD;
		}
		mutex_unlock(&data->devfreq->lock);
#endif
	} else { /* use_acpm is false */
		if (exynos_pm_qos_request_active(&data->default_pm_qos_min))
			exynos_pm_qos_update_request(&data->default_pm_qos_min,
					data->suspend_freq);
	}

	if (exynos_devfreq_get_freq(data->dev, &get_freq, data->clk, data))
		dev_err(data->dev, "failed get freq\n");

	dev->power.must_resume = true;

	dev_info(data->dev, "Suspend_frequency is %u\n", get_freq);

	return ret;
}

static int exynos_devfreq_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_devfreq_data *data = platform_get_drvdata(pdev);
	int ret = 0;
	u32 cur_freq;

	if (!exynos_devfreq_get_freq(data->dev, &cur_freq, data->clk, data))
		dev_info(data->dev, "Resume frequency is %u\n", cur_freq);

	if (data->use_acpm) {
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
		mutex_lock(&data->devfreq->lock);
#if IS_ENABLED(CONFIG_EXYNOS_ACPM) || IS_ENABLED(CONFIG_EXYNOS_ESCA)
		ret = exynos_devfreq_fvp_policy_release(data, true);
		if (ret) {
			mutex_unlock(&data->devfreq->lock);
			return ret;
		}
#endif
		data->suspend_req = 0;
		ret = update_devfreq(data->devfreq);
		if (ret && ret != -EAGAIN) {
			dev_err(&data->devfreq->dev, "devfreq failed with (%d) error\n", ret);
			mutex_unlock(&data->devfreq->lock);
			return NOTIFY_BAD;
		}
		mutex_unlock(&data->devfreq->lock);
#endif
	} else { /* use_acpm is false */
		if (exynos_pm_qos_request_active(&data->default_pm_qos_min))
			exynos_pm_qos_update_request(&data->default_pm_qos_min, data->default_qos);
	}
	return ret;
}

static int exynos_devfreq_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_devfreq_data *data;
	struct dev_pm_opp *init_opp;
	unsigned long init_freq = 0;
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	int nr_constraint;
	int err;
#endif

	data = kzalloc(sizeof(struct exynos_devfreq_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&pdev->dev, "failed to allocate devfreq data\n");
		ret = -ENOMEM;
		goto err_data;
	}

	data->dev = &pdev->dev;

	mutex_init(&data->lock);
	spin_lock_init(&data->update_status_lock);

	/* parsing devfreq dts data for exynos */
	ret = exynos_devfreq_parse_dt(data->dev->of_node, data);
	if (ret) {
		dev_err(data->dev, "failed to parse private data\n");
		goto err_parse_dt;
	}

	data->devfreq_profile.max_state = data->max_state;
	data->devfreq_profile.target = exynos_devfreq_dm_call;
	data->devfreq_profile.get_cur_freq = exynos_devfreq_get_cur_freq;

	data->devfreq_profile.freq_table = kzalloc(sizeof(*(data->devfreq_profile.freq_table)) * data->max_state, GFP_KERNEL);
	if (data->devfreq_profile.freq_table == NULL) {
		dev_err(data->dev, "failed to allocate for freq_table\n");
		ret = -ENOMEM;
		goto err_freqtable;
	}

	ret = exynos_init_freq_table(data);
	if (ret) {
		dev_err(data->dev, "failed initailize freq_table\n");
		goto err_init_table;
	}

	srcu_init_notifier_head(&data->trans_nh);

	devfreq_data[data->devfreq_type] = data;
	platform_set_drvdata(pdev, data);

	data->old_freq = (u32)data->devfreq_profile.initial_freq;
	data->last_stat_updated = jiffies;

	init_freq = (unsigned long)data->old_freq;
	init_opp = devfreq_recommended_opp(data->dev, &init_freq, 0);
	if (IS_ERR(init_opp)) {
		dev_err(data->dev, "not found valid OPP table for sync\n");
		ret = PTR_ERR(init_opp);
		goto err_get_opp;
	}
	dev_pm_opp_put(init_opp);

#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	ret = exynos_dm_data_init(data->dm_type, data, data->min_freq, data->max_freq, data->old_freq);
	if (ret) {
		dev_err(data->dev, "failed DVFS Manager data init\n");
		goto err_dm_data_init;
	}

	for (nr_constraint = 0; nr_constraint < data->nr_constraint; nr_constraint++) {
		if(data->constraint[nr_constraint]) {
			ret = register_exynos_dm_constraint_table(data->dm_type,
				data->constraint[nr_constraint]);
			if (ret) {
				dev_err(data->dev,"failed registration constraint table(%d)\n",
					nr_constraint);
				goto err_dm_table;
			}
		}
	}
#endif
	/* This flag guarantees initial frequency during boot time */
	data->devfreq_disabled = true;

	data->devfreq = devfreq_add_device(data->dev, &data->devfreq_profile,
					   data->governor_name, data);

	if (IS_ERR(data->devfreq)) {
		dev_err(data->dev, "failed devfreq device added\n");
		ret = -EINVAL;
		goto err_devfreq;
	}

	data->time_in_state = devm_kcalloc(data->dev,
			data->devfreq->profile->max_state,
			sizeof(unsigned long),
			GFP_KERNEL);
	if (!data->time_in_state) {
		err = -ENOMEM;
		goto err_devfreq;
	}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER)
	err = register_exynos_dm_freq_scaler(data->dm_type, devfreq_frequency_scaler);
	if (err)
		goto err_dm_scaler;
#endif

	/* ADD pm_qos notifier */
	data->nb.df = data->devfreq;
	data->nb.nb.notifier_call = exynos_devfreq_pm_qos_notifier;
	data->nb.nb.priority = INT_MAX - 1;

	ret = exynos_pm_qos_add_notifier(data->pm_qos_class, &data->nb.nb);
	if (ret < 0)
		goto err_add_pm_qos_min;

	if (data->pm_qos_class_max) {
		data->nb_max.df = data->devfreq;
		data->nb_max.nb.notifier_call = exynos_devfreq_pm_qos_notifier;

		ret = exynos_pm_qos_add_notifier(data->pm_qos_class_max, &data->nb_max.nb);
		if (ret < 0) {
			exynos_pm_qos_remove_notifier(data->pm_qos_class, &data->nb.nb);
			goto err_add_pm_qos_max;
		}
	}

//	dev_pm_qos_update_request(&data->devfreq->user_min_freq_req, data->min_freq);
//	dev_pm_qos_update_request(&data->devfreq->user_max_freq_req, data->max_freq);

	exynos_pm_qos_add_request(&data->sys_pm_qos_min, (int)data->pm_qos_class, init_freq);
	exynos_pm_qos_add_request(&data->sys_pm_qos_max, (int)data->pm_qos_class_max, INT_MAX);
#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG)
	exynos_pm_qos_add_request(&data->debug_pm_qos_min, (int)data->pm_qos_class, init_freq);
	exynos_pm_qos_add_request(&data->debug_pm_qos_max, (int)data->pm_qos_class_max, INT_MAX);
#endif
	if (data->pm_qos_class_max)
		exynos_pm_qos_add_request(&data->default_pm_qos_max, (int)data->pm_qos_class_max,
				   INT_MAX);
	exynos_pm_qos_add_request(&data->default_pm_qos_min, (int)data->pm_qos_class, init_freq);
	exynos_pm_qos_add_request(&data->boot_pm_qos, (int)data->pm_qos_class,
			   0);

	ret = devfreq_register_opp_notifier(data->dev, data->devfreq);
	if (ret) {
		dev_err(data->dev, "failed register opp notifier\n");
		goto err_opp_noti;
	}

	data->reboot_notifier.notifier_call = exynos_devfreq_reboot_notifier;
	ret = register_reboot_notifier(&data->reboot_notifier);
	if (ret) {
		dev_err(data->dev, "failed register reboot notifier\n");
		goto err_reboot_noti;
	}

	ret = sysfs_create_file(&data->devfreq->dev.kobj, &dev_attr_scaling_devfreq_min.attr);
	if (ret)
		dev_warn(data->dev, "failed create sysfs for devfreq exynos_pm_qos_min\n");

	ret = sysfs_create_file(&data->devfreq->dev.kobj, &dev_attr_scaling_devfreq_max.attr);
	if (ret)
		dev_warn(data->dev, "failed create sysfs for devfreq exynos_pm_qos_max\n");

	ret = sysfs_create_file(&data->devfreq->dev.kobj, &dev_attr_time_in_state.attr);
	if (ret)
		dev_warn(data->dev, "failed create sysfs for devfreq time_in_state\n");

#if IS_ENABLED(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG)
	ret = sysfs_create_group(&data->devfreq->dev.kobj, &exynos_devfreq_attr_group);
	if (ret)
		dev_warn(data->dev, "failed create sysfs for devfreq data\n");
#endif

	data->devfreq_disabled = false;

	if (!data->pm_domain) {
		if (data->devfreq_type != 0 || (num_online_cpus() > (num_possible_cpus() / 2)))
		/* set booting frequency during booting time */
			exynos_pm_qos_update_request_timeout(&data->boot_pm_qos, data->boot_freq,
						data->boot_qos_timeout * USEC_PER_SEC);
		else
			dev_info(data->dev, "skip boot freq setup\n");
	} else {
		pm_runtime_enable(&pdev->dev);
		pm_runtime_get_sync(&pdev->dev);
		exynos_pm_qos_update_request(&data->boot_pm_qos, data->default_qos);
		pm_runtime_put_sync(&pdev->dev);
	}

	// Update all pm_qos handles
	exynos_pm_qos_update_request(&data->sys_pm_qos_min, data->min_freq);
	exynos_pm_qos_update_request(&data->debug_pm_qos_min, data->min_freq);
	exynos_pm_qos_update_request(&data->default_pm_qos_min, data->default_qos);
	if (data->pm_qos_class_max) {
		exynos_pm_qos_update_request(&data->default_pm_qos_max, data->max_freq);
		exynos_pm_qos_update_request(&data->debug_pm_qos_max, data->max_freq);
	}

#if IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2) && IS_ENABLED(CONFIG_DEV_THERMAL)
	// Init dev_cooling_device
	if (data->use_dtm) {
		exynos_dev_cooling_register(data->dev->of_node, data);
		dev_info(data->dev, "devfreq cooling device registered");
	}
#endif

	device_enable_async_suspend(data->dev);

	dev_info(data->dev, "devfreq is initialized!!\n");

	return 0;

err_reboot_noti:
	devfreq_unregister_opp_notifier(data->dev, data->devfreq);
err_opp_noti:
	exynos_pm_qos_remove_request(&data->boot_pm_qos);
	exynos_pm_qos_remove_request(&data->default_pm_qos_min);
	if (data->pm_qos_class_max)
		exynos_pm_qos_remove_request(&data->default_pm_qos_min);
#if defined(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG) || defined(CONFIG_ARM_EXYNOS_DEVFREQ_DEBUG_MODULE)
	exynos_pm_qos_remove_request(&data->debug_pm_qos_min);
	exynos_pm_qos_remove_request(&data->debug_pm_qos_max);
#endif
	exynos_pm_qos_remove_request(&data->sys_pm_qos_min);
	exynos_pm_qos_remove_request(&data->sys_pm_qos_max);
	devfreq_remove_device(data->devfreq);
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	unregister_exynos_dm_freq_scaler(data->dm_type);
err_add_pm_qos_max:
	kfree((void *)&data->nb_max.nb);
err_add_pm_qos_min:
	kfree((void *)&data->nb.nb);
err_dm_scaler:
#endif
err_devfreq:
#if IS_ENABLED(CONFIG_EXYNOS_DVFS_MANAGER) || IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
err_dm_table:
err_dm_data_init:
#endif
err_get_opp:
//err_old_idx:
	platform_set_drvdata(pdev, NULL);
err_init_table:
	kfree(data->devfreq_profile.freq_table);
err_freqtable:
err_parse_dt:
	mutex_destroy(&data->lock);
	kfree(data);
err_data:

	return ret;
}

static struct platform_device_id exynos_devfreq_driver_ids[] = {
	{
	 .name = EXYNOS_DEVFREQ_MODULE_NAME,
	 },
	{},
};

MODULE_DEVICE_TABLE(platform, exynos_devfreq_driver_ids);

static const struct of_device_id exynos_devfreq_match[] = {
	{
	 .compatible = "samsung,exynos-devfreq",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, exynos_devfreq_match);

static const struct dev_pm_ops exynos_devfreq_pm_ops = {
	.suspend_late = exynos_devfreq_suspend,
	.resume_early = exynos_devfreq_resume,
};

static struct platform_driver exynos_devfreq_driver = {
	.probe = exynos_devfreq_probe,
	.id_table = exynos_devfreq_driver_ids,
	.driver = {
		.name = EXYNOS_DEVFREQ_MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &exynos_devfreq_pm_ops,
		.of_match_table = exynos_devfreq_match,
	},
};

static int exynos_devfreq_root_probe(struct platform_device *pdev)
{
	struct device_node *np;
	int num_domains;

	exynos_devfreq_governor_nop_init();

	np = pdev->dev.of_node;
	platform_driver_register(&exynos_devfreq_driver);

	/* alloc memory for devfreq data structure */
	num_domains = of_get_child_count(np);
	devfreq_data = (struct exynos_devfreq_data **)kzalloc(sizeof(struct exynos_devfreq_data *)
			* num_domains + 2, GFP_KERNEL);

	/* probe each devfreq node */
	of_platform_populate(np, NULL, NULL, NULL);

	return 0;
}

static const struct of_device_id exynos_devfreq_root_match[] = {
	{
		.compatible = "samsung,exynos-devfreq-root",
	},
	{},
	};

static struct platform_driver exynos_devfreq_root_driver = {
	.probe = exynos_devfreq_root_probe,
	.driver = {
		.name = "exynos-devfreq-root",
		.owner = THIS_MODULE,
		.of_match_table = exynos_devfreq_root_match,
	},
};

static int exynos_devfreq_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&exynos_devfreq_root_driver);
	if (ret) {
		pr_err("Platform driver registration failed (err=%d)\n", ret);
		return ret;
	}

	pr_info("%s successfully done.\n", __func__);

	return 0;
}
arch_initcall(exynos_devfreq_init);

MODULE_AUTHOR("Taekki Kim <taekki.kim@samsung.com>");
MODULE_AUTHOR("Hanjun Shin <hanjun.shin@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS Soc series devfreq common driver");
MODULE_SOFTDEP("pre: exynos_dm exynos_thermal  post: exynos-cpufreq mali_kbase");
MODULE_LICENSE("GPL");
