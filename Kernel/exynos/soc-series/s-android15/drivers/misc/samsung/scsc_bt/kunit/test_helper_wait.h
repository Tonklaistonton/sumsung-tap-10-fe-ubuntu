/******************************************************************************
 *                                                                            *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Test helper for bluetooth driver unit test                                 *
 *                                                                            *
 ******************************************************************************/
#ifndef __TEST_HELPER_WAIT_H__
#define __TEST_HELPER_WAIT_H__

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <scsc/scsc_wakelock.h>
#include <kunit/test.h>

/******************************************************************************
 * Helper for sync wait_queue
 ******************************************************************************/
void test_helper_wait_init(struct kunit *test);
void test_helper_wait_deinit(void);

void test_helper_wait(wait_queue_head_t *w);
void test_helper_wake_up_after_ms(wait_queue_head_t *w, unsigned int ms, void (*fn)(void));

void test_helper_write_wake_lock_dummy(struct wakeup_source **ws, bool enable);
#endif /* __TEST_HELPER_WAIT_H__ */
