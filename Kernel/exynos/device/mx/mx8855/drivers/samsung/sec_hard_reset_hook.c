/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "sec_key_notifier.h"

#if IS_ENABLED(CONFIG_SEC_KN_WATCH) && IS_ENABLED(CONFIG_SEC_FACTORY)
#define HOLD_TIME 15
#else
#define HOLD_TIME 6
#endif

static bool pressing_hold_key;
static bool pressing_sub_key;

static ktime_t hold_time;
static struct hrtimer hard_reset_hook_timer;

static bool hard_reset_occurred;

/* Proc node to enable hard reset */
static bool hard_reset_hook_enable = 1;
module_param_named(hard_reset_hook_enable, hard_reset_hook_enable, bool, 0664);
MODULE_PARM_DESC(hard_reset_hook_enable, "1: Enabled, 0: Disabled");

static enum hrtimer_restart hard_reset_hook_callback(struct hrtimer *hrtimer)
{
	pr_err("Hard Reset\n");
	hard_reset_occurred = true;
	panic("Hard Reset Hook");
	return HRTIMER_RESTART;
}

static int hard_reset_hook(struct notifier_block *nb,
			   unsigned long type, void *data)
{
	struct sec_key_notifier_param *param = data;

	if (unlikely(!hard_reset_hook_enable))
		return NOTIFY_DONE;

	switch (param->keycode) {
	case SEC_KN_HOLD:
		pressing_hold_key = param->down;
		break;
#if IS_ENABLED(CONFIG_SEC_KN_WATCH)
	case SEC_KN_SECONDARY:
#else
	case SEC_KN_PRIMARY:
#endif
		pressing_sub_key = param->down;
		break;
	default:
		return NOTIFY_DONE;
	}

	if (pressing_hold_key && pressing_sub_key) {
		hrtimer_start(&hard_reset_hook_timer,
			hold_time, HRTIMER_MODE_REL);
		pr_info("%s : hrtimer_start\n", __func__);
	} else {
		hrtimer_try_to_cancel(&hard_reset_hook_timer);
	}

	return NOTIFY_OK;
}

static struct notifier_block seccmn_hard_reset_notifier = {
	.notifier_call = hard_reset_hook
};

void hard_reset_delay(void)
{
	/* HQE team request hard reset key should guarantee 7 seconds.
	 * To get proper stack, hard reset hook starts after 6 seconds.
	 * And it will reboot before 7 seconds.
	 * Add delay to keep the 7 seconds
	 */
	if (hard_reset_occurred) {
		pr_err("wait until warm or manual reset is triggered\n");
		mdelay(2000); // TODO: change to dev_mdelay
	}
}
EXPORT_SYMBOL(hard_reset_delay);

static int __init hard_reset_hook_init(void)
{
	hrtimer_init(&hard_reset_hook_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	hard_reset_hook_timer.function = hard_reset_hook_callback;
	hold_time = ktime_set(HOLD_TIME, 0);

	return sec_kn_register_notifier(&seccmn_hard_reset_notifier);
}

static void hard_reset_hook_exit(void)
{
	sec_kn_unregister_notifier(&seccmn_hard_reset_notifier);
}

module_init(hard_reset_hook_init);
module_exit(hard_reset_hook_exit);

MODULE_DESCRIPTION("Samsung Hard_reset_hook driver");
MODULE_LICENSE("GPL v2");
