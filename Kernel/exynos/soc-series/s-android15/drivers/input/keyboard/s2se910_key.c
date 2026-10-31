/*
 * Driver for Power key/Vol Down key on s2se910 IC by PWRON,Vol-down rising, falling interrupts.
 *
 * drivers/input/keyboard/s2se910_key.c
 * S2SE910 Keyboard Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pmic/s2se910-mfd.h>
#include <linux/pmic/s2p_key.h>

static void s2se910_keys_work_func(struct work_struct *work)
{
	struct s2p_button_data *bdata = container_of(work,
						      struct s2p_button_data,
						      key_work.work);

	s2p_keys_power_report_event(bdata);
}

static irqreturn_t s2se910_keys_press_irq_handler(int irq, void *dev_id)
{
	struct s2p_keys_drvdata *ddata = dev_id;

	s2p_keys_press_irq_handler(ddata, irq);

	return IRQ_HANDLED;
}

static irqreturn_t s2se910_keys_release_irq_handler(int irq, void *dev_id)
{
	struct s2p_keys_drvdata *ddata = dev_id;

	s2p_keys_release_irq_handler(ddata, irq);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_OF)
static struct s2p_keys_platform_data *
of_s2se910_keys_parse_dt(struct s2se910_dev *iodev)
{
	#define S2SE910_SUPPORT_KEY_NUM	(2)	// power, vol-down key
	struct device *dev = iodev->dev;
	struct device_node *mfd_np = NULL, *key_np = NULL;
	struct s2p_keys_platform_data *pdata = NULL;
	int error = 0, nbuttons = 0;
	size_t size;

	if (!dev->of_node) {
		dev_err(dev, "%s: Failed to get of_node\n", __func__);
		error = -ENODEV;
		goto err_out;
	}

	mfd_np = dev->of_node;

	key_np = of_find_node_by_name(mfd_np, "s2se910-keys");
	if (!key_np) {
		pr_err("%s: could not find current_node\n", __func__);
		error = -ENOENT;
		goto err_out;
	}

	nbuttons = of_get_child_count(key_np);
	if (nbuttons != S2SE910_SUPPORT_KEY_NUM) {
		dev_warn(dev, "%s: s2se910-keys support only two button(%d)\n",
			__func__, nbuttons);
		error = -ENODEV;
		goto err_out;
	}

	pdata = devm_kzalloc(dev, sizeof(struct s2p_keys_platform_data), GFP_KERNEL);
	if (!pdata) {
		error = -ENOMEM;
		goto err_out;
	}

	size = nbuttons * sizeof(struct s2p_keys_button);
	pdata->buttons = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!pdata->buttons) {
		error = -ENOMEM;
		goto err_out;
	}

	pdata->nbuttons = nbuttons;

	error = of_s2p_keys_parse_dt(pdata, key_np);
	if (error < 0)
		goto err_out;

	return pdata;
err_out:
	return ERR_PTR(error);
}
#else
static inline struct s2p_keys_platform_data *
of_s2se910_keys_parse_dt(struct s2se910_dev *iodev)
{
	return ERR_PTR(-ENODEV);
}
#endif

static void s2se910_keys_set_input_device(struct platform_device *pdev,
		struct s2p_keys_platform_data *pdata, struct input_dev *input)
{
	input->name		= pdata->name ? : pdev->name;
	input->phys 		= "s2se910-keys/input0";
	input->dev.parent	= &pdev->dev;
	input->open		= s2p_keys_open;
	input->close		= s2p_keys_close;

	input->id.bustype	= BUS_I2C;
	input->id.vendor	= 0x0001;
	input->id.product	= 0x0001;
	input->id.version	= 0x0100;
}

static int s2se910_keys_set_interrupt(struct s2p_keys_drvdata *ddata, int irq_base)
{
	#define POWER_KEY_IDX		(0)
	#define VOLDN_KEY_IDX		(1)
	struct device *dev = ddata->dev;
	int ret = 0;
	uint32_t i = 0, idx = 0;
	static char irq_name[4][32] = {0, };
	char name[4][32] = {"PWRONR_IRQ", "PWRONP_IRQ", "VOLDNR_IRQ", "VOLDNP_IRQ"};

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct s2p_button_data *bdata = &ddata->button_data[i];

		idx = i * 2;
		if (i == POWER_KEY_IDX) {
			bdata->irq_key_r = irq_base + S2SE910_PMIC_IRQ_PWRONR_PM_INT1;
			bdata->irq_key_p = irq_base + S2SE910_PMIC_IRQ_PWRONP_PM_INT1;
		} else if (i == VOLDN_KEY_IDX) {
			bdata->irq_key_r = irq_base + S2SE910_PMIC_IRQ_VOLDNR_PM_INT3;
			bdata->irq_key_p = irq_base + S2SE910_PMIC_IRQ_VOLDNP_PM_INT3;
		} else {
			dev_info(dev, "%s: Failed to get Key index: %d\n", __func__, i);
			goto err;
		}

		snprintf(irq_name[idx], sizeof(irq_name[idx]) - 1, "%s@%s",
			 name[idx], dev_name(dev));
		snprintf(irq_name[idx + 1], sizeof(irq_name[idx + 1]) - 1, "%s@%s",
			 name[idx + 1], dev_name(dev));

		ret = devm_request_threaded_irq(dev, bdata->irq_key_r, NULL,
						s2se910_keys_release_irq_handler, 0,
						irq_name[idx], ddata);
		if (ret < 0) {
			dev_err(dev, "%s: fail to request %s: %d: %d\n",
				__func__, name[idx], bdata->irq_key_r, ret);
			goto err;
		}

		ret = devm_request_threaded_irq(dev, bdata->irq_key_p, NULL,
						s2se910_keys_press_irq_handler, 0,
						irq_name[idx + 1], ddata);
		if (ret < 0) {
			dev_err(dev, "%s: fail to request %s: %d: %d\n",
				__func__, name[idx + 1], bdata->irq_key_p, ret);
			goto err;
		}
	}

	return 0;
err:
	return -1;
}

static int s2se910_keys_set_buttondata(struct s2p_keys_drvdata *ddata,
				     struct input_dev *input, bool *wakeup)
{
	uint32_t cnt = 0;

	for (cnt = 0; cnt < ddata->pdata->nbuttons; cnt++) {
		struct s2p_keys_button *button = &ddata->pdata->buttons[cnt];
		struct s2p_button_data *bdata = &ddata->button_data[cnt];
		char device_name[32] = {0, };

		bdata->input = input;
		bdata->button = button;

		if (button->wakeup)
			*wakeup = true;

		/* Dynamic allocation for workqueue name */
		snprintf(device_name, sizeof(device_name) - 1,
			"s2se910-keys-wq%d@%s", cnt, dev_name(ddata->dev));

		bdata->irq_wqueue = create_singlethread_workqueue(device_name);
		if (!bdata->irq_wqueue) {
			pr_err("%s: fail to create workqueue\n", __func__);
			return -ENOMEM;
		}
		INIT_DELAYED_WORK(&bdata->key_work, s2se910_keys_work_func);

		input_set_capability(input, button->type ?: EV_KEY, button->code);
	}

	return cnt;
}

static int s2se910_keys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s2se910_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2p_keys_platform_data *pdata = NULL;
	struct s2p_keys_drvdata *ddata = NULL;
	struct input_dev *input = NULL;
	int ret = 0, count = 0;
	bool wakeup = false;

	pr_info("%s: start\n", __func__);

	pdata = of_s2se910_keys_parse_dt(iodev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "failed to allocate device\n");
		return -ENOMEM;
	}

	s2se910_keys_set_input_device(pdev, pdata, input);

	ddata = s2p_keys_set_drvdata(pdev, pdata, input);
	if (IS_ERR(ddata)) {
		pr_err("%s: power_keys_set_drvdata fail\n", __func__);
		return PTR_ERR(ddata);
	}

	ret = s2se910_keys_set_buttondata(ddata, input, &wakeup);
	if (ret < 0) {
		pr_err("%s: s2se910_keys_set_buttondata fail\n", __func__);
		return ret;
	} else
		count = ret;

	ret = device_init_wakeup(dev, wakeup);
	if (ret < 0) {
		pr_err("%s: device_init_wakeup fail(%d)\n", __func__, ret);
		goto fail;
	}

	ret = s2se910_keys_set_interrupt(ddata, iodev->sdev->irq_base);
	if (ret < 0) {
		pr_err("%s: s2se910_keys_set_interrupt fail\n", __func__);
		goto fail;
	}

	ret = input_register_device(input);
	if (ret) {
		dev_err(dev, "%s: unable to register input device, error: %d\n",
			__func__, ret);
		goto fail;
	}

	pr_info("%s: end\n", __func__);

	return 0;

fail:
	while (--count >= 0) {
		struct s2p_button_data *bdata = &ddata->button_data[count];

		if (bdata->irq_wqueue)
			destroy_workqueue(bdata->irq_wqueue);

		cancel_delayed_work_sync(&bdata->key_work);
	}

	return ret;
}

static int s2se910_keys_remove(struct platform_device *pdev)
{
	struct s2p_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;

	device_init_wakeup(&pdev->dev, false);

	s2p_keys_remove(ddata, input);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int s2se910_keys_suspend(struct device *dev)
{
	struct s2p_keys_drvdata *ddata = dev_get_drvdata(dev);

	s2p_keys_sel_suspend(ddata, true);

	return 0;
}

static int s2se910_keys_resume(struct device *dev)
{
	struct s2p_keys_drvdata *ddata = dev_get_drvdata(dev);

	s2p_keys_sel_suspend(ddata, false);

	return 0;
}
#endif

static struct of_device_id s2se910_keys_of_match[] = {
	{ .compatible = "s2se910-power-keys", },
	{ },
};
MODULE_DEVICE_TABLE(of, s2se910_keys_of_match);

static SIMPLE_DEV_PM_OPS(s2se910_keys_pm_ops, s2se910_keys_suspend, s2se910_keys_resume);

static struct platform_driver s2se910_keys_device_driver = {
	.probe		= s2se910_keys_probe,
	.remove		= s2se910_keys_remove,
	.driver		= {
		.name	= "s2se910-power-keys",
		.owner	= THIS_MODULE,
		.pm	= &s2se910_keys_pm_ops,
		.of_match_table = of_match_ptr(s2se910_keys_of_match),
	}
};

module_platform_driver(s2se910_keys_device_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Keyboard driver for s2se910");
MODULE_ALIAS("platform:s2se910 Power/Vol-down key");
