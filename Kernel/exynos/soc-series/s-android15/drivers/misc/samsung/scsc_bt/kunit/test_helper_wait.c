/******************************************************************************
 *                                                                            *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Test helper for bluetooth driver unit test                                 *
 *                                                                            *
 ******************************************************************************/
/******************************************************************************
 * Helper for sync wait_queue
 ******************************************************************************/
#include "test_helper_wait.h"

struct test_helper_wait {
	struct kunit *test;

	struct workqueue_struct *wq;
	struct work_struct wait_work;
	struct work_struct wake_up_after_ms_work;

	wait_queue_head_t *target_wait;

	bool wait_ready;
	unsigned int wakeup_ms;

	void (*fn)(void);
} wait_helper;

static void test_helper_wait_work(struct work_struct *work)
{
	struct test_helper_wait *hlp = &wait_helper;

	kunit_info(hlp->test, "Wait for event");
	wait_event_interruptible(*hlp->target_wait, hlp->wait_ready);
	kunit_info(hlp->test, "woken");
}

static void test_helper_wake_up_work(struct work_struct *work)
{
	struct test_helper_wait *hlp = &wait_helper;

	if (hlp->fn != NULL)
		hlp->fn();

	kunit_info(hlp->test, "wake up after %d ms", hlp->wakeup_ms);
	msleep(hlp->wakeup_ms);
	wake_up(hlp->target_wait);
	hlp->fn = NULL;
}

void test_helper_wait_init(struct kunit *test)
{
	struct test_helper_wait *hlp = &wait_helper;

	hlp->test = test;
	hlp->wq = create_singlethread_workqueue("scsc_bt_unittest_helper");
	if (hlp->wq == NULL) {
		kunit_err(hlp->test, "Fail to create workqueue\n");
		return;
	}
	INIT_WORK(&hlp->wait_work, test_helper_wait_work);
	INIT_WORK(&hlp->wake_up_after_ms_work, test_helper_wake_up_work);
	hlp->fn = NULL;
}

void test_helper_wait_deinit(void)
{
	struct test_helper_wait *hlp = &wait_helper;

	flush_workqueue(hlp->wq);
	//destroy_workqueue(wq);
	cancel_work_sync(&hlp->wait_work);
	cancel_work_sync(&hlp->wake_up_after_ms_work);
	hlp->test = NULL;
	hlp->fn = NULL;
}

void test_helper_wait(wait_queue_head_t *w)
{
	struct test_helper_wait *hlp = &wait_helper;

	hlp->target_wait = w;
	hlp->wait_ready = false;
	queue_work(hlp->wq, &hlp->wait_work);
	msleep(10);
	hlp->wait_ready = true;
}

void test_helper_wake_up_after_ms(wait_queue_head_t *w, unsigned int ms, void (*fn)(void))
{
	struct test_helper_wait *hlp = &wait_helper;

	hlp->target_wait = w;
	hlp->wakeup_ms = ms;
	hlp->fn = fn;
	queue_work(hlp->wq, &hlp->wake_up_after_ms_work);
}

static struct wakeup_source fake_ws;
void test_helper_write_wake_lock_dummy(struct wakeup_source **ws, bool enable)
{
	if (enable) {
		fake_ws.active = 0;
		if (*ws == NULL)
			*ws = &fake_ws;
	} else {
		if (*ws == &fake_ws)
			*ws = NULL;
	}
}
