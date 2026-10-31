/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM exynos_wow

#if !defined(_TRACE_EXYNOS_WOW_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EXYNOS_WOW_H

#include <linux/tracepoint.h>

TRACE_EVENT(exynos_wow,
	TP_PROTO(const char *name, unsigned int bw),

	TP_ARGS(name, bw),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, bw)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->bw = bw;
	),

	TP_printk("type=%s bw=%u",
		__get_str(name), __entry->bw)
);
#endif /* _TRACE_EXYNOS_WOW_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
