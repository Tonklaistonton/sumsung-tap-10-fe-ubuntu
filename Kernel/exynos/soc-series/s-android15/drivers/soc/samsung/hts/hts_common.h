/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HTS_COMMON_H_
#define _HTS_COMMON_H_

#include "hts.h"

#include <linux/sched.h>
#include <linux/atomic/atomic-instrumented.h>

#define BUFFER_UNIT_SIZE		(sizeof(u64))
#define BUFFER_OFFSET_SIZE		(1)

static inline int hts_atomic_cmpxhg(atomic_t *val)
{
        if (atomic_cmpxchg(val, 0, 1) == 1)
                return -EAGAIN;

        return 0;
}

static inline void hts_atomic_set(atomic_t *val)
{
        while (hts_atomic_cmpxhg(val))
                cpu_relax();
}

static inline void hts_atomic_clear(atomic_t *val)
{
        atomic_set(val, 0);
}

struct hts_config *hts_task_get_config(struct task_struct *task);
struct hts_config *hts_task_get_or_alloc_config(struct task_struct *task);
void hts_task_free_config(struct task_struct *task);

void hts_plist_add(struct plist_node *node, struct plist_head *head);
void hts_plist_del(struct plist_node *node, struct plist_head *head);

void hts_write_to_buffer(unsigned long *buffer_event, int buffer_size, u64 *data, int data_count);

#endif /* _HTS_COMMON_H_ */
