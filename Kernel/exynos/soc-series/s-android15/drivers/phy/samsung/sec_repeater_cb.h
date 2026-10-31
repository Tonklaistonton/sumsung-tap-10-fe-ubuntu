#ifndef _SEC_REPEATER_CB_H
#define _SEC_REPEATER_CB_H

struct sec_repeater_cb_data {
	struct device *dev;
	int (*repeater_enable)(bool en);
};

#if IS_ENABLED(CONFIG_USB_REPEATER)
extern int cb_repeater_enable(bool en);

extern int register_repeater_enable(int (*repeater_enable) (bool en));
#else
static inline int cb_repeater_enable(bool en) {return 0; }

static inline int register_repeater_enable
		(int (*repeater_enable) (bool en)) {return 0; }
#endif
#endif /* _SEC_REPEATER_CB_H */
