/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include "sec_key_notifier.h"

/* Input sequence 9530 */
#define CRASH_COUNT_FIRST 9
#define CRASH_COUNT_SECOND 5
#define CRASH_COUNT_THIRD 3

struct crash_key {
	unsigned int keycode;
	unsigned int count;
};

const struct crash_key key_combination[] = {
	{SEC_KN_PRIMARY, CRASH_COUNT_FIRST},
	{SEC_KN_SECONDARY, CRASH_COUNT_SECOND},
	{SEC_KN_PRIMARY, CRASH_COUNT_THIRD},
};

#if IS_ENABLED(CONFIG_SEC_KN_WATCH)
#define pressing_hold_key true
#else
static bool pressing_hold_key;
#endif

static DEFINE_SPINLOCK(sec_crash_key_user_lock);

static ktime_t limit_time;
static struct hrtimer crash_key_user_timer;

static size_t curr_count;
static size_t curr_step;

static inline void _reset_crash_key(void)
{
	pr_info("reset\n");
	curr_step = 0;
	curr_count = 0;
}

static inline void reset_crash_key(void)
{
	spin_lock(&sec_crash_key_user_lock);
	_reset_crash_key();
	spin_unlock(&sec_crash_key_user_lock);
}

static void increase_count(unsigned int keycode)
{
	spin_lock(&sec_crash_key_user_lock);
	if (keycode != key_combination[curr_step].keycode) {
		pr_info("keycode doesn't match current step");
		hrtimer_try_to_cancel(&crash_key_user_timer);
		_reset_crash_key();
		if (keycode != SEC_KN_PRIMARY)
			goto out;
	}

	if (!curr_count && !curr_step) {
		hrtimer_start(&crash_key_user_timer, limit_time, HRTIMER_MODE_REL);
		pr_info("hrtimer_start\n");
	}

	if (++curr_count == key_combination[curr_step].count) {
		if (++curr_step == 3)
			panic("User Crash Key");
		curr_count = 0;
	}
out:
	spin_unlock(&sec_crash_key_user_lock);
}

static enum hrtimer_restart crash_key_user_callback(struct hrtimer *hrtimer)
{
	pr_err("time expired\n");
	reset_crash_key();
	return HRTIMER_NORESTART;
}

static int check_crash_keys_in_user(struct notifier_block *nb,
				unsigned long type, void *data)
{
	struct sec_key_notifier_param *param = data;

	switch (param->keycode) {
#if !IS_ENABLED(CONFIG_SEC_KN_WATCH)
	case SEC_KN_HOLD:
		pressing_hold_key = param->down;
		if (!param->down)
			reset_crash_key();
		return NOTIFY_DONE;
#endif
	case SEC_KN_PRIMARY:
	case SEC_KN_SECONDARY:
		if (!param->down || !pressing_hold_key)
			break;
		increase_count(param->keycode);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block seccmn_user_crash_key_notifier = {
	.notifier_call = check_crash_keys_in_user
};

static int __init sec_crash_key_user_init(void)
{
	spin_lock_init(&sec_crash_key_user_lock);

	hrtimer_init(&crash_key_user_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	crash_key_user_timer.function = crash_key_user_callback;
	limit_time = ktime_set(5, 0); /* 5 seconds */

	return sec_kn_register_notifier(&seccmn_user_crash_key_notifier);
}

static void sec_crash_key_user_exit(void)
{
	sec_kn_unregister_notifier(&seccmn_user_crash_key_notifier);
}

module_init(sec_crash_key_user_init);
module_exit(sec_crash_key_user_exit);

MODULE_DESCRIPTION("Samsung Crash-key-user driver");
MODULE_LICENSE("GPL v2");
