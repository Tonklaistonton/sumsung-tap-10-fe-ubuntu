// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 */

#include <linux/pmic/s2p_key.h>

#define WAKELOCK_TIME		HZ/10

static int s2p_keys_wake_lock_timeout(struct device *dev, long timeout)
{
	struct wakeup_source *ws = NULL;

	if (!dev->power.wakeup) {
		dev_err(dev, "%s: Not register wakeup source\n", __func__);
		return -1;
	}

	ws = dev->power.wakeup;
	__pm_wakeup_event(ws, jiffies_to_msecs(timeout));

	return 0;
}

void s2p_keys_power_report_event(struct s2p_button_data *bdata)
{
	const struct s2p_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	struct s2p_keys_drvdata *ddata = input_get_drvdata(input);
	unsigned int type = button->type ?: EV_KEY;
	int state = bdata->key_pressed;

	if (s2p_keys_wake_lock_timeout(ddata->dev, WAKELOCK_TIME) < 0) {
		dev_err(ddata->dev, "%s: s2p_keys_wake_lock_timeout fail\n", __func__);
		return;
	}

	/* Report new key event */
	input_event(input, type, button->code, !!state);

	/* Sync new input event */
	input_sync(input);

	if (bdata->button->wakeup)
		pm_relax(bdata->input->dev.parent);
}
EXPORT_SYMBOL_GPL(s2p_keys_power_report_event);

static void s2p_keys_report_state(struct s2p_keys_drvdata *ddata)
{
	uint32_t i = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct s2p_button_data *bdata = &ddata->button_data[i];

		bdata->key_pressed = false;
		s2p_keys_power_report_event(bdata);
	}
}

int s2p_keys_open(struct input_dev *input)
{
	struct s2p_keys_drvdata *ddata = input_get_drvdata(input);

	dev_info(ddata->dev, "%s()\n", __func__);

	s2p_keys_report_state(ddata);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_keys_open);

void s2p_keys_close(struct input_dev *input)
{
	struct s2p_keys_drvdata *ddata = input_get_drvdata(input);

	dev_info(ddata->dev, "%s()\n", __func__);
}
EXPORT_SYMBOL_GPL(s2p_keys_close);

void s2p_keys_press_irq_handler(struct s2p_keys_drvdata *ddata, const int irq)
{
	uint32_t i = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct s2p_button_data *bdata = &ddata->button_data[i];

		if (bdata->irq_key_p == irq) {
			bdata->key_pressed = true;

			if (bdata->button->wakeup) {
				const struct s2p_keys_button *button = bdata->button;

				pm_stay_awake(bdata->input->dev.parent);
				if (bdata->suspended  &&
				    (button->type == 0 || button->type == EV_KEY)) {
					/*
					 * Simulate wakeup key press in case the key has
					 * already released by the time we got interrupt
					 * handler to run.
					 */
					input_report_key(bdata->input, button->code, 1);
				}
			}
			queue_delayed_work(bdata->irq_wqueue, &bdata->key_work, 0);
		}
	}
}
EXPORT_SYMBOL_GPL(s2p_keys_press_irq_handler);

void s2p_keys_release_irq_handler(struct s2p_keys_drvdata *ddata, const int irq)
{
	uint32_t i = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct s2p_button_data *bdata = &ddata->button_data[i];

		if (bdata->irq_key_r == irq) {
			bdata->key_pressed = false;
			queue_delayed_work(bdata->irq_wqueue, &bdata->key_work, 0);
		}
	}
}
EXPORT_SYMBOL_GPL(s2p_keys_release_irq_handler);

int of_s2p_keys_parse_dt(struct s2p_keys_platform_data *pdata, struct device_node *key_np)
{
	uint32_t i = 0;
	struct device_node *pp = NULL;
	struct s2p_keys_button *button = NULL;

	for_each_child_of_node(key_np, pp) {
		button = &pdata->buttons[i++];
		if (of_property_read_u32(pp, "linux,code", &button->code))
			return -EINVAL;

		button->desc = of_get_property(pp, "label", NULL);

		button->wakeup = of_property_read_bool(pp, "wakeup");

		if (of_property_read_u32(pp, "linux,input-type", &button->type))
			button->type = EV_KEY;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(of_s2p_keys_parse_dt);

struct s2p_keys_drvdata *s2p_keys_set_drvdata(struct platform_device *pdev,
			struct s2p_keys_platform_data *pdata, struct input_dev *input)
{
	struct s2p_keys_drvdata *ddata = NULL;
	struct device *dev = &pdev->dev;
	size_t size = 0;

	ddata = devm_kzalloc(dev, sizeof(struct s2p_keys_drvdata), GFP_KERNEL);
	if (!ddata) {
		dev_err(dev, "failed to allocate ddata.\n");
		return ERR_PTR(-ENOMEM);
	}

	size = pdata->nbuttons * sizeof(struct s2p_button_data);
	ddata->button_data = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!ddata->button_data) {
		dev_err(dev, "failed to allocate button_data.\n");
		return ERR_PTR(-ENOMEM);
	}

	ddata->dev	= dev;
	ddata->pdata	= pdata;
	ddata->input	= input;

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	return ddata;
}
EXPORT_SYMBOL_GPL(s2p_keys_set_drvdata);

void s2p_keys_remove(struct s2p_keys_drvdata *ddata, struct input_dev *input)
{
	uint32_t i = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct s2p_button_data *bdata = &ddata->button_data[i];

		if (bdata->irq_wqueue)
			destroy_workqueue(bdata->irq_wqueue);

		cancel_delayed_work_sync(&bdata->key_work);
	}

	input_unregister_device(input);
}
EXPORT_SYMBOL_GPL(s2p_keys_remove);

void s2p_keys_sel_suspend(struct s2p_keys_drvdata *ddata, bool sel)
{
	uint32_t i = 0;

	if (device_may_wakeup(ddata->dev)) {
		for (i = 0; i < ddata->pdata->nbuttons; i++) {
			struct s2p_button_data *bdata = &ddata->button_data[i];

			bdata->suspended = sel;
		}
	}
}
EXPORT_SYMBOL_GPL(s2p_keys_sel_suspend);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Keyboard driver for s2p");
MODULE_ALIAS("platform:s2p_keys");
