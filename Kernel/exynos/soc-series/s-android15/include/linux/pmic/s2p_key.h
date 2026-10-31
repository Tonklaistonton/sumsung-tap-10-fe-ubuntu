#ifndef _S2P_KEYS_H
#define _S2P_KEYS_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>

struct s2p_keys_button {
	/* Configuration parameters */
	unsigned int code;      /* input event code (KEY_*, SW_*) */
	const char *desc;
	unsigned int type;      /* input event type (EV_KEY, EV_SW, EV_ABS) */
	bool wakeup;
};

struct s2p_keys_platform_data {
	struct s2p_keys_button *buttons;
	int nbuttons;
	const char *name;               /* input device name */
};

struct s2p_button_data {
	struct s2p_keys_button *button;
	struct input_dev *input;
	struct work_struct work;
	struct delayed_work key_work;
	struct workqueue_struct *irq_wqueue;
	bool key_pressed;
	bool suspended;
	int irq_key_p;
	int irq_key_r;
};

struct s2p_keys_drvdata {
	struct device *dev;
	const struct s2p_keys_platform_data *pdata;
	struct input_dev *input;
	struct s2p_button_data *button_data;
};

extern void s2p_keys_power_report_event(struct s2p_button_data *bdata);
extern int s2p_keys_open(struct input_dev *input);
extern void s2p_keys_close(struct input_dev *input);
extern void s2p_keys_press_irq_handler(struct s2p_keys_drvdata *ddata, const int irq);
extern void s2p_keys_release_irq_handler(struct s2p_keys_drvdata *ddata, const int irq);
extern int of_s2p_keys_parse_dt(struct s2p_keys_platform_data *pdata, struct device_node *key_np);
extern struct s2p_keys_drvdata *s2p_keys_set_drvdata(struct platform_device *pdev,
		struct s2p_keys_platform_data *pdata, struct input_dev *input);
extern void s2p_keys_remove(struct s2p_keys_drvdata *ddata, struct input_dev *input);
extern void s2p_keys_sel_suspend(struct s2p_keys_drvdata *ddata, bool sel);
#endif
