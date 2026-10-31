/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/pm_opp.h>
#include "../../../../../common/drivers/devfreq/governor.h"

#include <soc/samsung/exynos-devfreq.h>
#include <linux/devfreq.h>

struct devfreq_nop_gov_data {
	int dummy;
};

static int devfreq_governor_nop_func(struct devfreq *df,
					unsigned long *freq)
{
	struct exynos_devfreq_data *data = df->data;
	unsigned long min_freq, max_freq;
	int ret = 0;

	if (data->devfreq_disabled)
		return -EAGAIN;

	/* Get min/max policy */
	if (data->suspend_req) {
		min_freq = data->suspend_req;
		max_freq = data->suspend_req;
	} else {
		min_freq = exynos_pm_qos_request(data->pm_qos_class);
		max_freq = data->max_freq;
		if (data->pm_qos_class_max)
			max_freq = exynos_pm_qos_request(data->pm_qos_class_max);
	}

	ret = exynos_devfreq_get_recommended_freq(data->dev, &min_freq, 0);
	if (ret) {
		dev_err(data->dev, "%s: failed to get mapped freq\n", __func__);
		goto out;
	}

	ret = exynos_devfreq_get_recommended_freq(data->dev, &max_freq,
			DEVFREQ_FLAG_LEAST_UPPER_BOUND);
	if (ret) {
		dev_err(data->dev, "%s: failed to get mapped freq\n", __func__);
		goto out;
	}

	/* Update min/max policy to DM */
	policy_update_call_to_DM(data->dm_type, min_freq, max_freq);

	/* Get final_freq */
	*freq = min(min_freq, max_freq);

out:
	return ret;
}

static int devfreq_governor_nop_start(struct devfreq *df)
{
	struct exynos_devfreq_data *data;
	struct devfreq_nop_gov_data *gov_data;

	if (!df)
		return -EINVAL;
	if (!df->data)
		return -EINVAL;

	data = df->data;

	gov_data = kzalloc(sizeof(struct devfreq_nop_gov_data), GFP_KERNEL);
	if (!gov_data)
		return -ENOMEM;

	df->governor_data = gov_data;
	return 0;
}

static int devfreq_governor_nop_stop(struct devfreq *df)
{
	struct devfreq_nop_gov_data *gov_data = df->governor_data;

	kfree(gov_data);
	return 0;
}

static int devfreq_governor_nop_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	int ret;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = devfreq_governor_nop_start(devfreq);
		if (ret)
			return ret;
		break;
	case DEVFREQ_GOV_STOP:
		ret = devfreq_governor_nop_stop(devfreq);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}

static struct devfreq_governor devfreq_governor_nop = {
	.name = "nop",
	.get_target_freq = devfreq_governor_nop_func,
	.event_handler = devfreq_governor_nop_handler,
};

int exynos_devfreq_governor_nop_init(void)
{
	return devfreq_add_governor(&devfreq_governor_nop);
}

MODULE_LICENSE("GPL");
