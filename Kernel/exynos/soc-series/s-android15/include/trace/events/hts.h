#undef TRACE_SYSTEM
#define TRACE_SYSTEM hts

#if !defined(_TRACE_HTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HTS_H

#include <linux/tracepoint.h>

#define NAME_LEN	(16)

TRACE_EVENT(arch_control,

	TP_PROTO(unsigned int tid, char *name, unsigned long value),

	TP_ARGS(tid, name, value),

	TP_STRUCT__entry(
		__field(	unsigned int,		tid			)
		__array(		char,		name,	NAME_LEN	)
		__field(	unsigned long,		value			)
	),

	TP_fast_assign(
		__entry->tid			= tid;
		memcpy(__entry->name,	name, NAME_LEN);
		__entry->value			= value;
	),

	TP_printk("tid=%u name=%s value=%lx",
		__entry->tid, __entry->name, __entry->value)
);

#endif /* _TRACE_HTS_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/events

/* This part must be outside protection */
#include <trace/define_trace.h>
