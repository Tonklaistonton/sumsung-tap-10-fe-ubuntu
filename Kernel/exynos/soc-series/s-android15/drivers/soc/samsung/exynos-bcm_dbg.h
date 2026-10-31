#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/suspend.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/of_reserved_mem.h>
#include <linux/panic_notifier.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <linux/sched/clock.h>

#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#elif IS_ENABLED(CONFIG_EXYNOS_ESCAV1)
#include <soc/samsung/acpm_ipc_ctrl.h>
#elif defined(CONFIG_EXYNOS_ADV_TRACER) || defined(CONFIG_EXYNOS_ADV_TRACER_MODULE)
#include <soc/samsung/exynos/exynos-adv-tracer-ipc.h>
#endif

#include <soc/samsung/exynos-bcm_dbg.h>
#include <soc/samsung/exynos-bcm_dbg-dt.h>
#include <soc/samsung/exynos-bcm_dbg-dump.h>
#if defined(CONFIG_EXYNOS_PD) || defined(CONFIG_EXYNOS_PD_MODULE)
#include <soc/samsung/exynos-pd.h>
#endif
#include <soc/samsung/cal-if.h>
#if defined(CONFIG_EXYNOS_ITMON) || defined(CONFIG_EXYNOS_ITMON_MODULE)
#include <soc/samsung/exynos/exynos-itmon.h>
#endif
#if defined(CONFIG_CPU_IDLE)
#include <soc/samsung/exynos-cpupm.h>
#endif
