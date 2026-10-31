/*
 * s2p_rtc.h
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; ehither version 2 of the License, or (at your option)
 *  any later version.
 *
 */
#ifndef __LINUX_S2P_RTC_H
#define __LINUX_S2P_RTC_H

#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/pmic/s2p.h>

struct s2p_rtc_pdata {
	bool wtsr_en;
	bool smpl_en;
	int wtsr_timer_val;
	int smpl_timer_val;
	bool check_jigon_en;

	int smpl_warn;
	bool smpl_warn_en;
	uint32_t smpl_warn_vth;
	uint32_t smpl_warn_hys;

	struct rtc_time *init_time;
};

struct s2p_rtc_reg {
	uint16_t pm1_status1;
	uint16_t pm1_status2;

	uint16_t ctrl;
	uint16_t wtsr_smpl;
	uint16_t update;
	uint16_t time_sec;
	uint16_t alarm_sec[2];
	uint16_t secure[4];
};

struct s2p_rtc_mask {
	uint8_t alarm0_en;

	uint8_t bcd_en;
	uint8_t model_24;
	uint8_t rudr;
	uint8_t wudr;
	uint8_t audr;
	uint8_t freeze;
	uint8_t hour_pm;
	uint8_t alarm_enable;
	uint8_t wtsr_en;
	uint8_t smpl_en;
	uint8_t wtsrt;
	uint8_t smplt;
	uint8_t jigonb;
};

struct s2p_rtc_shift {
	uint8_t wtsr_en;
	uint8_t smpl_en;
	uint8_t wtsrt;
	uint8_t smplt;
};

struct s2p_rtc_info {
	struct device		*dev;
	struct s2p_dev		*sdev;
	struct rtc_device	*rdev;
	struct s2p_rtc_pdata	*pdata;
	struct mutex		lock;
	struct delayed_work	irq_work;
	struct wakeup_source	*rtc_ws;

	uint16_t rtc_addr;
	uint16_t pm1_addr;
	struct s2p_rtc_reg	*reg;
	struct s2p_rtc_mask	*mask;
	struct s2p_rtc_shift	*shift;

	uint8_t update_reg_val;
	bool use_irq;
	bool alarm_enabled;
	int smpl_warn;

	int alarm0_irq;
	int smpl_warn_irq;
	bool smpl_warn_flag;
	uint32_t smpl_warn_cnt;
	bool wtsr_en;
	bool smpl_en;

	int alarm0_irq_enum;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	char sysfs_name[S2P_NAME_MAX];
	struct device *rtc_sysfs_dev;
#endif
};

enum S2P_RTC_OP {
	S2P_RTC_READ,
	S2P_RTC_WRITE_TIME,
	S2P_RTC_WRITE_ALARM,
};

enum {
	RTC_SEC = 0,
	RTC_MIN,
	RTC_HOUR,
	RTC_WEEKDAY,
	RTC_DATE,
	RTC_MONTH,
	RTC_YEAR,
	NR_RTC_CNT_REGS,
};

enum S2P_SMPL_WARN {
	S2P_SMPL_WARN_ACTIVE,
	S2P_SMPL_WARN_DEACTIVE,
};

/* RTC SECURE */
#define RTC_BOOTING	(3)
#define RTC_SUSPEND	(4)
#define RTC_RESUME	(5)
#define RTC_SHUTDOWN	(6)
#define RTC_POWEROFF	(7)

#define S2P_RTC_BIT(mask, shift, val)	(((val) << shift) & mask)

extern void s2p_rtc_wakeup_event(struct s2p_rtc_info *info, unsigned int msec);

extern int s2p_secure_debug_set_config(uint8_t addr, uint8_t config);
extern int s2p_secure_debug_read(uint8_t addr, uint8_t bit);
extern void s2p_secure_debug_write(uint8_t addr, uint8_t bit, uint8_t val);
extern void s2p_secure_debug_clear(void);

extern void s2p_data_to_tm(uint8_t *data, struct rtc_time *tm);
extern int s2p_tm_to_data(struct rtc_time *tm, uint8_t *data);
extern int s2p_rtc_update(struct s2p_rtc_info *info, enum S2P_RTC_OP op);
extern int s2p_rtc_read_time(struct s2p_rtc_info *info, struct rtc_time *tm);
extern int s2p_rtc_set_time(struct s2p_rtc_info *info, struct rtc_time *tm);
extern int s2p_rtc_read_alarm(struct s2p_rtc_info *info, struct rtc_wkalrm *alrm);
extern int s2p_rtc_set_alarm(struct s2p_rtc_info *info, struct rtc_wkalrm *alrm);
extern int s2p_rtc_alarm_irq_enable(struct s2p_rtc_info *info, unsigned int enabled);

extern void s2p_rtc_set_wtsr(struct s2p_rtc_info *info, bool wtsr_en);
extern void s2p_rtc_set_smpl(struct s2p_rtc_info *info, bool smpl_en);
extern void s2p_rtc_set_wtsr_timer(struct s2p_rtc_info *info);
extern void s2p_rtc_set_smpl_timer(struct s2p_rtc_info *info);
extern int s2p_rtc_set_interrupt(struct s2p_rtc_info *info, int irq_base);
extern int s2p_rtc_init_pdata(struct s2p_rtc_info *info, struct device *dev);
extern int s2p_rtc_init_reg(struct s2p_rtc_info *info);
#if IS_ENABLED(CONFIG_RTC_AUTO_PWRON)
extern int pon_alarm_get_lpcharge(void);
extern void pon_alarm_parse_data(char *alarm_data, struct rtc_wkalrm *alm);
#endif

extern int s2p_rtc_set_config(struct s2p_rtc_info *info, uint8_t config);
extern int s2p_smpl_warn_register_notifier(struct notifier_block *nb);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
extern int s2p_create_rtc_sysfs(struct s2p_rtc_info *info, const char *mfd_dev_name, const uint32_t dev_id);
extern void s2p_remove_rtc_sysfs_entries(struct device *rtc_sysfs);
#endif /* CONFIG_DRV_SAMSUNG_PMIC */

#endif /* __LINUX_S2P_RTC_H */
