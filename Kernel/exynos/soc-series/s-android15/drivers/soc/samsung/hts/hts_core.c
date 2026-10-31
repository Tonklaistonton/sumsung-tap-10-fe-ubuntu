/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_core.h"
#include "hts_pmu.h"

#include <linux/cpuhotplug.h>

static int hts_core_online(unsigned int cpu)
{
	int ret;

	ret = hts_pmu_register_event(cpu);
	if (ret)
		pr_err("HTS : Couldn't reigster event successfully CPU%d - %d", cpu, ret);

	return 0;
}

static int hts_core_offline(unsigned int cpu)
{
	hts_pmu_release_event(cpu);

	return 0;
}

static int hts_core_register_hotplug_notifier(void)
{
	if (cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
				"hts_core",
				hts_core_online,
				hts_core_offline) < 0)
		return -EINVAL;

	return 0;
}

int hts_core_initialize(struct hts_drvdata *drvdata)
{
	return hts_core_register_hotplug_notifier();
}
