#ifndef __EXYNOS_PROFILER_IF_H__
#define __EXYNOS_PROFILER_IF_H__

/* Profilers */
#include <soc/samsung/profiler/exynos-profiler-fn.h>
#include <soc/samsung/profiler/exynos-profiler-extif.h>

/* Kernel */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/percpu.h>
#include <linux/cpufreq.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/cpuhotplug.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/thermal.h>

#include <linux/slab.h>
#include <linux/pm_opp.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/pm_opp.h>

#include <asm/perf_event.h>

#include <trace/hooks/cpuidle.h>
#include <trace/hooks/cpufreq.h>
/* SoC Specific */
#include <soc/samsung/exynos-dm.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/exynos-sci.h>
#include <soc/samsung/exynos-devfreq.h>
#if IS_ENABLED(CONFIG_EXYNOS_MIFGOV)
#include <soc/samsung/exynos-mifgov.h>
#endif
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/bts.h>

#include <soc/samsung/exynos-wow.h>
#include <linux/exynos-dsufreq.h>

#if IS_ENABLED(CONFIG_SCHED_EMS)
#include "../../../../../../kernel/sched/ems/ems.h"
#endif
#endif /* __EXYNOS_PROFILER_IF_H__ */
