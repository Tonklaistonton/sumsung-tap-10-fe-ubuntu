/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_common.h"

#define FLAG_UPDATING                   (0x80000000)
#define OFFSET_HEAD                     (0)
#define OFFSET_TAIL                     (1)

#if IS_ENABLED(CONFIG_SCHED_EMS)
#include "../../../../kernel/sched/ems/ems.h"
#define HTS_TASK_CONFIG(task)		(task_avd(task)->hts_config)
#else
#define HTS_TASK_CONFIG(task)		(task->android_vendor_data1[32])
#endif

struct hts_config *hts_task_get_config(struct task_struct *task)
{
	return (struct hts_config *)HTS_TASK_CONFIG(task);
}

struct hts_config *hts_task_get_or_alloc_config(struct task_struct *task)
{
	struct hts_config *config = (struct hts_config *)HTS_TASK_CONFIG(task);

	if (config == NULL) {
		config = kzalloc(sizeof(struct hts_config), GFP_KERNEL);
		HTS_TASK_CONFIG(task) = (u64)config;
	}

	return config;
}

void hts_task_free_config(struct task_struct *task)
{
	struct hts_config *config = (struct hts_config *)HTS_TASK_CONFIG(task);

	if (config == NULL)
		return;

	kfree(config);
}

# define plist_check_head(h)    do { } while (0)
void hts_plist_add(struct plist_node *node, struct plist_head *head)
{
	struct plist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	plist_check_head(head);
	WARN_ON(!plist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (plist_head_empty(head))
		goto ins_node;

	first = iter = plist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct plist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);

	plist_check_head(head);
}

void hts_plist_del(struct plist_node *node, struct plist_head *head)
{
	plist_check_head(head);

	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct plist_node *next;

			next = list_entry(node->node_list.next,
					struct plist_node, node_list);

			/* add the next plist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);

	plist_check_head(head);
}

void hts_write_to_buffer(unsigned long *buffer_event,
		int buffer_size,
		u64 *data,
		int data_count)
{
	unsigned int *idx_ptr = (unsigned int *)buffer_event;
	int head_idx, tail_idx, new_head_idx, new_tail_idx, left_size, chunk_left_size;

	if (buffer_event == NULL)
		return;

	head_idx = idx_ptr[OFFSET_HEAD];
	tail_idx = idx_ptr[OFFSET_TAIL];

	new_head_idx = (head_idx + data_count) % buffer_size;
	idx_ptr[OFFSET_HEAD] = FLAG_UPDATING | new_head_idx;

	left_size = (buffer_size + tail_idx - head_idx) % buffer_size;

	if (left_size &&
		left_size <= data_count) {
		new_tail_idx = (tail_idx + data_count) % buffer_size;
		idx_ptr[OFFSET_TAIL] = FLAG_UPDATING | new_tail_idx;
	}

	chunk_left_size = buffer_size - head_idx;
	if (chunk_left_size < data_count) {
		memcpy(&buffer_event[head_idx + BUFFER_OFFSET_SIZE], data, BUFFER_UNIT_SIZE * chunk_left_size);
		memcpy(&buffer_event[BUFFER_OFFSET_SIZE], data + chunk_left_size, BUFFER_UNIT_SIZE * (data_count - chunk_left_size));
	} else {
		memcpy(&buffer_event[head_idx + BUFFER_OFFSET_SIZE], data, BUFFER_UNIT_SIZE * data_count);
	}

	if (left_size &&
		left_size <= data_count)
		idx_ptr[OFFSET_TAIL] = new_tail_idx;

	idx_ptr[OFFSET_HEAD] = new_head_idx;
}
