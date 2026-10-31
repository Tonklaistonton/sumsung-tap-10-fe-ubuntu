#ifndef _SEC_USB_CB_H
#define _SEC_USB_CB_H

#include <linux/usb/composite.h>
#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
#include <linux/usb/f_ss_mon_gadget.h>
#endif

enum usblog_for_cb {
	SEC_CB_USBSTATE,
	SEC_CB_EVENT,
	SEC_CB_EXT_EVENT,
	SEC_CB_USBLOG_END,
};

enum nottify_extra_for_cb {
	SEC_CB_EXTRA_EVENT_ENABLE_USB_DATA,
	SEC_CB_EXTRA_EVENT_DISABLE_USB_DATA,
};

enum notify_events_for_cb {
	SEC_CB_EVENT_ALL_DISABLE,
	SEC_CB_EVENT_HOST_DISABLE,
	SEC_CB_EVENT_CLIENT_DISABLE,
	SEC_CB_NOTIFY_EVENT_END,
};

enum notify_event_status_for_cb {
	SEC_CB_EVENT_DISABLED,
	SEC_CB_EVENT_DISABLING,
	SEC_CB_EVENT_ENABLED,
	SEC_CB_EVENT_ENABLING,
	SEC_CB_EVENT_BLOCKED,
	SEC_CB_EVENT_BLOCKING,
	SEC_CB_EVENT_STATUS_END,
};
enum notify_block_type_for_cb {
	SEC_CB_BLOCK_TYPE_NONE = 0,
	SEC_CB_BLOCK_TYPE_HOST = (1 << 0),
	SEC_CB_BLOCK_TYPE_CLIENT = (1 << 1),
	SEC_CB_BLOCK_TYPE_ALL = (1 << 0 | 1 << 1),
};

struct sec_usb_functions {
	void (*usb_reset_notify)(struct usb_gadget *gadget);
	void (*vbus_session_notify)(struct usb_gadget *gadget, int is_active, int ret);
	void (*make_suspend_current_event)(void);
	int (*Is_Accessory_mode)(void);
	void (*store_usblog_notify)(int type, void *param1, void *param2);
	bool (*is_blocked)(int type);
};

#if	IS_ENABLED(CONFIG_SEC_USB_CB)
extern void store_usblog_notify_cb(int type, void *param1, void *param2);
extern void usb_reset_notify_cb(struct usb_gadget *gadget);
extern void vbus_session_notify_cb(struct usb_gadget *gadget, int is_active, int ret);
extern void make_suspend_current_event_cb(void);
extern void store_usblog_notify_cb(int type, void *param1, void *param2);
extern bool is_blocked_cb(int type);

extern int register_ss_mon_funcs(struct sec_usb_functions *funcs);
#else
static inline void store_usblog_notify_cb(int type, void *param1, void *param2)
{return 0; }
static inline void usb_reset_notify_cb(struct usb_gadget *gadget)
{return 0; }
static inline void vbus_session_notify_cb(struct usb_gadget *gadget, int is_active, int ret)
{return 0; }
static inline void make_suspend_current_event_cb(void)
{return 0; }
static inline void store_usblog_notify_cb(int type, void *param1, void *param2)
{return 0; }
static inline bool is_blocked_cb(int type)
{return 0; }

static inline int register_ss_mon_funcs(struct sec_usb_functions *funcs)
{return 0; }
#endif
#endif /* _SEC_USB_CB_H */
