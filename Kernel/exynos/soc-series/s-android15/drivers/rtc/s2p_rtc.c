/*
 * s2p_rtc.c
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
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pmic/s2p_rtc.h>
#include <linux/pmic/pmic_class.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <linux/notifier.h>

#define S2P_SMPL_WARN_POLL_DELAY	(10)

static struct s2p_rtc_info *static_info;

static BLOCKING_NOTIFIER_HEAD(smpl_warn_notifier);
int s2p_smpl_warn_register_notifier(struct notifier_block *nb)
{
	int ret = 0;

	ret = blocking_notifier_chain_register(&smpl_warn_notifier, nb);
	if (ret < 0)
		pr_err("%s: fail to register smpl warn notifier\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_smpl_warn_register_notifier);

void s2p_smpl_warn_call_notifier(enum S2P_SMPL_WARN status)
{
	blocking_notifier_call_chain(&smpl_warn_notifier, status, 0);
}

void s2p_rtc_wakeup_event(struct s2p_rtc_info *info, unsigned int msec)
{
	__pm_wakeup_event(info->rtc_ws, msec);
}
EXPORT_SYMBOL_GPL(s2p_rtc_wakeup_event);

static bool s2p_is_wrong_secure_addr(const uint8_t addr)
{
	if (addr != static_info->reg->secure[0] &&
	    addr != static_info->reg->secure[3])
		return true;

	return false;
}

int s2p_secure_debug_set_config(uint8_t addr, uint8_t config)
{
	const char *config_bit[] = {"RTC_BOOTING", "RTC_SUSPEND", "RTC_RESUME",
				    "RTC_SHUTDOWN", "RTC_POWEROFF"};
	unsigned char val = 0;
	int ret = 0;

	if (s2p_is_wrong_secure_addr(addr)) {
		pr_err("%s: wrong addr. (%#2x)\n", __func__, addr);
		return -1;
	}

	if (config != RTC_BOOTING && config != RTC_SUSPEND &&
	    config != RTC_RESUME && config != RTC_SHUTDOWN &&
	    config != RTC_POWEROFF) {
		pr_err("%s: wrong config(%#2x)\n", __func__, config);
		return -1;
	}

	ret = s2p_update_reg(static_info->sdev, static_info->rtc_addr,
			     addr, config, 0xF);
	if (ret < 0) {
		pr_err("%s: s2p_update_reg failed\n", __func__);
		return -1;
	}

	ret = s2p_read_reg(static_info->sdev, static_info->rtc_addr,
			   addr, &val);
	if (ret < 0) {
		pr_err("%s: s2p_read_reg failed\n", __func__);
		return -1;
	}

	pr_info("[PMIC] %s: %s: %#2x(%#2x)\n", __func__,
		config_bit[config - RTC_BOOTING], addr, val);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_secure_debug_set_config);

int s2p_secure_debug_read(uint8_t addr, uint8_t bit)
{
	int ret = 0;
	unsigned char val;

	if (s2p_is_wrong_secure_addr(addr))
		return -1;

	if (bit > 7)
		return -1;

	ret = s2p_read_reg(static_info->sdev, static_info->rtc_addr, addr, &val);
	if (ret < 0)
		return -1;

	val = (val >> bit) & 0x1;

	return val;
}
EXPORT_SYMBOL_GPL(s2p_secure_debug_read);

void s2p_secure_debug_write(uint8_t addr, uint8_t bit, uint8_t val)
{
	int ret = 0;

	if (s2p_is_wrong_secure_addr(addr))
		return;

	if (bit > 7)
		return;

	if (val != 0 && val != 1)
		return;

	ret = s2p_update_reg(static_info->sdev, static_info->rtc_addr,
			     addr, val << bit, 1 << bit);
	if (ret < 0)
		return;
}
EXPORT_SYMBOL_GPL(s2p_secure_debug_write);

void s2p_secure_debug_clear(void)
{
	int ret = 0, i = 0;

	for (i = 0; i <= 3; i++) {
		if (i == 1 || i == 2)
			continue;
		ret = s2p_write_reg(static_info->sdev, static_info->rtc_addr,
				    static_info->reg->secure[i], 0x00);
		if (ret < 0)
			return;
	}
}
EXPORT_SYMBOL_GPL(s2p_secure_debug_clear);

int s2p_rtc_set_config(struct s2p_rtc_info *info, uint8_t config)
{
	struct rtc_time tm;
	int ret = 0;

	ret = s2p_rtc_read_time(info, &tm);
	if (ret < 0)
		pr_err("%s: fail to read rtc time\n", __func__);

	/* Set secure debug */
	if (s2p_secure_debug_set_config(info->reg->secure[0], config) < 0)
		pr_err("%s: s2p_secure_debug_set_config fail, config(%d)\n",
			__func__, config);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_config);

void s2p_data_to_tm(uint8_t *data, struct rtc_time *tm)
{
	tm->tm_sec = data[RTC_SEC] & 0x7f;
	tm->tm_min = data[RTC_MIN] & 0x7f;
	tm->tm_hour = data[RTC_HOUR] & 0x1f;
	tm->tm_wday = __fls(data[RTC_WEEKDAY] & 0x7f);
	tm->tm_mday = data[RTC_DATE] & 0x1f;
	tm->tm_mon = (data[RTC_MONTH] & 0x0f) - 1;
	tm->tm_year = (data[RTC_YEAR] & 0x7f) + 100;
	tm->tm_yday = 0;
	tm->tm_isdst = 0;
}
EXPORT_SYMBOL_GPL(s2p_data_to_tm);

int s2p_tm_to_data(struct rtc_time *tm, uint8_t *data)
{
	data[RTC_SEC] = tm->tm_sec;
	data[RTC_MIN] = tm->tm_min;

	if (tm->tm_hour >= 12)
		data[RTC_HOUR] = tm->tm_hour | static_info->mask->hour_pm;
	else
		data[RTC_HOUR] = tm->tm_hour;

	data[RTC_WEEKDAY] = BIT(tm->tm_wday);
	data[RTC_DATE] = tm->tm_mday;
	data[RTC_MONTH] = tm->tm_mon + 1;
	data[RTC_YEAR] = tm->tm_year > 100 ? (tm->tm_year - 100) : 0;

	if (tm->tm_year < 100) {
		pr_warn("%s: RTC cannot handle the year %d\n", __func__,
			1900 + tm->tm_year);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_tm_to_data);

int s2p_rtc_update(struct s2p_rtc_info *info, enum S2P_RTC_OP op)
{
	uint8_t data = 0;
	int ret = 0;

	if (!info || !info->sdev || !info->dev) {
		pr_err("%s: NULL pointer error\n", __func__);
		return -ENODEV;
	}

	switch (op) {
	case S2P_RTC_READ:
		data = info->mask->rudr;
		break;
	case S2P_RTC_WRITE_TIME:
		data = info->mask->wudr;
		break;
	case S2P_RTC_WRITE_ALARM:
		data = info->mask->audr;
		break;
	default:
		dev_err(info->dev, "%s: invalid op(%d)\n", __func__, op);
		return -EINVAL;
	}

	data |= info->update_reg_val;

	ret = s2p_write_reg(info->sdev, info->rtc_addr, info->reg->update, data);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to write update reg(%d, %u)\n",
			__func__, ret, data);
	else
		usleep_range(1000, 1005);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_rtc_update);

int s2p_rtc_read_time(struct s2p_rtc_info *info, struct rtc_time *tm)
{
	uint8_t data[NR_RTC_CNT_REGS];
	int ret = 0;

	mutex_lock(&info->lock);
	ret = s2p_rtc_update(info, S2P_RTC_READ);
	if (ret < 0)
		goto out;

	ret = s2p_bulk_read(info->sdev, info->rtc_addr, info->reg->time_sec,
			    NR_RTC_CNT_REGS, data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read time reg(%d)\n", __func__,
			ret);
		goto out;
	}

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02hhx)%s\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1f, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & info->mask->hour_pm ? "PM" : "AM");

	s2p_data_to_tm(data, tm);
	ret = rtc_valid_tm(tm);
out:
	mutex_unlock(&info->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_rtc_read_time);

int s2p_rtc_set_time(struct s2p_rtc_info *info, struct rtc_time *tm)
{
	uint8_t data[NR_RTC_CNT_REGS];
	int ret = 0;

	mutex_lock(&info->lock);
	ret = s2p_tm_to_data(tm, data);
	if (ret < 0)
		goto out;

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02hhx)%s\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1f, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & info->mask->hour_pm ? "PM" : "AM");

	ret = s2p_bulk_write(info->sdev, info->rtc_addr, info->reg->time_sec,
			     NR_RTC_CNT_REGS, data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write time reg(%d)\n", __func__,
			ret);
		goto out;
	}

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_TIME);
out:
	mutex_unlock(&info->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_time);

int s2p_rtc_read_alarm(struct s2p_rtc_info *info, struct rtc_wkalrm *alrm)
{
	uint8_t data[NR_RTC_CNT_REGS];
	uint8_t val = 0;
	int ret = 0;

	mutex_lock(&info->lock);
	ret = s2p_rtc_update(info, S2P_RTC_READ);
	if (ret < 0)
		goto out;

	ret = s2p_bulk_read(info->sdev, info->rtc_addr, info->reg->alarm_sec[0],
			NR_RTC_CNT_REGS, data);
	if (ret < 0) {
		dev_err(info->dev, "%s:%d fail to read alarm reg(%d)\n",
			__func__, __LINE__, ret);
		goto out;
	}

	s2p_data_to_tm(data, &alrm->time);

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(%d)\n", __func__,
			alrm->time.tm_year + 1900, alrm->time.tm_mon + 1,
			alrm->time.tm_mday, alrm->time.tm_hour,
			alrm->time.tm_min, alrm->time.tm_sec,
			alrm->time.tm_wday);

	alrm->enabled = info->alarm_enabled;

	ret = s2p_read_reg(info->sdev, info->pm1_addr, info->reg->pm1_status2,
			&val);
	if (ret < 0) {
		dev_err(info->dev, "%s:%d fail to read PM1 STATUS2 reg(%d)\n",
			__func__, __LINE__, ret);
		goto out;
	}

	if (val & info->mask->alarm0_en)
		alrm->pending = 1;
	else
		alrm->pending = 0;

out:
	mutex_unlock(&info->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_rtc_read_alarm);

static int s2p_rtc_set_alarm_enable(struct s2p_rtc_info *info, bool enabled)
{
	if (!info->use_irq)
		return -EPERM;

	if (enabled && !info->alarm_enabled) {
		info->alarm_enabled = true;
		enable_irq(info->alarm0_irq);
	} else if (!enabled && info->alarm_enabled) {
		info->alarm_enabled = false;
		disable_irq(info->alarm0_irq);
	}

	return 0;
}

int s2p_rtc_enable_alarm(struct s2p_rtc_info *info, uint8_t *data, int alrm_no)
{
	uint32_t i = 0;
	int ret = 0;

	if (alrm_no != 0 && alrm_no != 1)
		return -EINVAL;

	for (i = 0; i < NR_RTC_CNT_REGS; i++)
		data[i] |= info->mask->alarm_enable;

	ret = s2p_bulk_write(info->sdev, info->rtc_addr,
			info->reg->alarm_sec[alrm_no], NR_RTC_CNT_REGS, data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to disable alarm reg(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_ALARM);

	return ret;
}

int s2p_rtc_disable_alarm(struct s2p_rtc_info *info, uint8_t *data, int alrm_no)
{
	uint32_t i = 0;
	int ret = 0;

	if (alrm_no != 0 && alrm_no != 1)
		return -EINVAL;

	for (i = 0; i < NR_RTC_CNT_REGS; i++)
		data[i] &= ~(info->mask->alarm_enable);

	ret = s2p_bulk_write(info->sdev, info->rtc_addr,
			info->reg->alarm_sec[alrm_no], NR_RTC_CNT_REGS, data);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to disable alarm reg(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_ALARM);

	return ret;
}

/* To operate after an alarm, always set alarm initialization */
int s2p_rtc_set_alarm(struct s2p_rtc_info *info, struct rtc_wkalrm *alrm)
{
	uint8_t data[NR_RTC_CNT_REGS];
	int ret = 0;

	mutex_lock(&info->lock);
	ret = s2p_tm_to_data(&alrm->time, data);
	if (ret < 0)
		goto out;

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02hhx)%s\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1f, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & info->mask->hour_pm ? "PM" : "AM");

	ret = s2p_rtc_disable_alarm(info, data, 0);
	if (ret < 0)
		goto out;

	ret = s2p_rtc_set_alarm_enable(info, alrm->enabled);
	if (ret < 0)
		goto out;

	ret = s2p_rtc_enable_alarm(info, data, 0);
out:
	mutex_unlock(&info->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_alarm);

int s2p_rtc_alarm_irq_enable(struct s2p_rtc_info *info, unsigned int enabled)
{
	int ret = 0;

	mutex_lock(&info->lock);
	ret = s2p_rtc_set_alarm_enable(info, enabled);
	mutex_unlock(&info->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_rtc_alarm_irq_enable);

static bool s2p_is_jigonb_low(struct s2p_rtc_info *info)
{
	int ret = 0;
	uint8_t val = 0;

	ret = s2p_read_reg(info->sdev, info->pm1_addr, info->reg->pm1_status1,
			   &val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read PM1 STATUS1 reg(%d)\n",
			__func__, ret);
		return false;
	}

	return !(val & info->mask->jigonb);
}

void s2p_rtc_set_wtsr(struct s2p_rtc_info *info, bool wtsr_en)
{
	int ret = 0;

	if (!info)
		return;

	ret = s2p_update_reg(info->sdev, info->rtc_addr, info->reg->wtsr_smpl,
			wtsr_en << info->shift->wtsr_en, info->mask->wtsr_en);
	if (ret < 0) {
		dev_err(info->dev, "%s: update reg fail(%d)\n", __func__, ret);
		return;
	}

	pr_info("%s: WTSR: %s\n", __func__, wtsr_en ? "enable" : "disable");
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_wtsr);

void s2p_rtc_set_smpl(struct s2p_rtc_info *info, bool smpl_en)
{
	int ret = 0;

	if (!info)
		return;

	if (info->pdata->check_jigon_en && s2p_is_jigonb_low(info))
		smpl_en = false;

	ret = s2p_update_reg(info->sdev, info->rtc_addr, info->reg->wtsr_smpl,
			smpl_en << info->shift->smpl_en, info->mask->smpl_en);
	if (ret < 0) {
		dev_err(info->dev, "%s: update reg fail(%d)\n", __func__, ret);
		return;
	}

	pr_info("%s: SMPL: %s\n", __func__, smpl_en ? "enable" : "disable");

	info->smpl_en = info->pdata->smpl_en = smpl_en;
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_smpl);

void s2p_rtc_set_wtsr_timer(struct s2p_rtc_info *info)
{
	int ret = 0;

	if (!info)
		return;

	ret = s2p_update_reg(info->sdev, info->rtc_addr, info->reg->wtsr_smpl,
			info->pdata->wtsr_timer_val >> info->shift->wtsrt,
			info->mask->wtsrt);
	if (ret < 0)
		dev_err(info->dev, "%s: read reg fail(%d)\n", __func__, ret);
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_wtsr_timer);

void s2p_rtc_set_smpl_timer(struct s2p_rtc_info *info)
{
	int ret = 0;

	if (!info)
		return;

	ret = s2p_update_reg(info->sdev, info->rtc_addr, info->reg->wtsr_smpl,
			info->pdata->wtsr_timer_val >> info->shift->wtsrt,
			info->mask->wtsrt);
	if (ret < 0)
		dev_err(info->dev, "%s: read reg fail(%d)\n", __func__, ret);
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_smpl_timer);

static irqreturn_t s2p_smpl_warn_irq_handler(int irq, void *data)
{
	struct s2p_rtc_info *info = data;

	if (!info->rdev)
		return IRQ_HANDLED;

	if (gpio_get_value(info->smpl_warn) & 0x1)
		return IRQ_HANDLED;

	dev_info(info->dev, "[PMIC] %s: SMPL_WARN, cnt: %d\n", __func__, ++info->smpl_warn_cnt);
	dbg_snapshot_pmic(irq, "SMPL_WARN_S_IRQ", info->smpl_warn_cnt, 0);
	info->smpl_warn_flag = true;

	disable_irq_nosync(info->smpl_warn_irq);

	queue_delayed_work(system_freezable_wq, &info->irq_work, 0);

	return IRQ_HANDLED;
}

static void s2p_smpl_warn_work(struct work_struct *work)
{
	struct s2p_rtc_info *info = container_of(work,
			struct s2p_rtc_info, irq_work.work);
	int state = 0;

	if (info->smpl_warn_flag) {
		s2p_smpl_warn_call_notifier(S2P_SMPL_WARN_ACTIVE);
		dev_info(info->dev, "[PMIC] %s: SMPL_WARN_ACTIVE Notifier\n", __func__);
		info->smpl_warn_flag = false;
		queue_delayed_work(system_freezable_wq, &info->irq_work,
				msecs_to_jiffies(S2P_SMPL_WARN_POLL_DELAY));

		return;
	}

	state = (gpio_get_value(info->smpl_warn) & 0x1);

	if (!state) {
		queue_delayed_work(system_freezable_wq, &info->irq_work,
				msecs_to_jiffies(S2P_SMPL_WARN_POLL_DELAY));
	} else {
		dev_info(info->dev, "%s: SMPL_WARN polling End!\n", __func__);
		dbg_snapshot_pmic(0, "SMPL_WARN_E_IRQ", 0, 0);
		s2p_smpl_warn_call_notifier(S2P_SMPL_WARN_DEACTIVE);

		enable_irq(info->smpl_warn_irq);
	}
}

static irqreturn_t s2p_rtc_alarm_irq(int irq, void *data)
{
	struct s2p_rtc_info *info = data;

	if (!info->rdev)
		return IRQ_HANDLED;

	dev_info(info->dev, "[PMIC] %s: irq(%d)\n", __func__, irq);

	rtc_update_irq(info->rdev, 1, RTC_IRQF | RTC_AF);
	s2p_rtc_wakeup_event(info, 500);

	return IRQ_HANDLED;
}

int s2p_rtc_set_interrupt(struct s2p_rtc_info *info, int irq_base)
{
	struct s2p_rtc_pdata *pdata = info->pdata;
	int ret = 0;

	if (!irq_base) {
		dev_err(info->dev, "[PMIC] %s: Failed to get irq base %d\n",
			__func__, irq_base);
		return -ENODEV;
	}

	info->alarm0_irq = irq_base + info->alarm0_irq_enum;

	/* Set a VGPIO for ALARM0 IRQ */
	ret = devm_request_threaded_irq(info->dev, info->alarm0_irq, NULL,
					s2p_rtc_alarm_irq, 0, "rtc-alarm0", info);
	if (ret < 0) {
		dev_err(info->dev, "%s: Failed to request alarm IRQ: %d: %d\n",
			__func__, info->alarm0_irq, ret);
		return ret;
	}

	disable_irq(info->alarm0_irq);
	disable_irq(info->alarm0_irq);
	info->use_irq = true;

	/* Set a GPIO pin for SMPL IRQ */
	if (pdata->smpl_warn_en) {
		if (!gpio_is_valid(pdata->smpl_warn)) {
			dev_err(info->dev, "%s: smpl_warn GPIO NOT VALID\n",
				__func__);
			return -1;
		}

		INIT_DELAYED_WORK(&info->irq_work, s2p_smpl_warn_work);

		info->smpl_warn_irq = gpio_to_irq(pdata->smpl_warn);

		irq_set_status_flags(info->smpl_warn_irq, IRQ_DISABLE_UNLAZY);

		ret = devm_request_threaded_irq(info->dev, info->smpl_warn_irq,
			s2p_smpl_warn_irq_handler, NULL,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, "SMPL WARN", info);
		if (ret < 0) {
			dev_err(info->dev,
				"%s: Failed to request smpl warn IRQ: %d: %d\n",
				__func__, info->smpl_warn_irq, ret);
			return -1;
		}

		info->smpl_warn = pdata->smpl_warn;
		info->smpl_warn_flag = false;
	}

	enable_irq(info->alarm0_irq);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_rtc_set_interrupt);


#if IS_ENABLED(CONFIG_OF)
static int of_s2p_rtc_parse_dt(struct device *dev, struct s2p_rtc_pdata *pdata)
{
	struct device_node *np = dev->of_node;
	struct device_node *rtc_np = NULL;
	int ret = 0;
	uint32_t val = 0;

	if (!np)
		return -ENODEV;

	rtc_np = of_find_node_by_name(np, "s2p_rtc");
	if (!rtc_np) {
		dev_err(dev, "%s has not s2p_rtc node\n", __func__);
		return -ENODEV;
	}

	/* wtsr and smpl in dt */
	ret = of_property_read_u32(rtc_np, "wtsr_en", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_en = !!val;

	ret = of_property_read_u32(rtc_np, "smpl_en", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_en = !!val;

	ret = of_property_read_u32(rtc_np, "wtsr_timer_val", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_timer_val = val;

	ret = of_property_read_u32(rtc_np, "smpl_timer_val", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_timer_val = val;

	ret = of_property_read_u32(rtc_np, "check_jigon_en", &val);
	pdata->check_jigon_en = !!val;

	/* smpl warning in dt */
	pdata->smpl_warn = of_get_named_gpio(np, "gpios", 0);

	ret = of_property_read_u32(np, "smpl_warn_en", &val);
	if (ret)
		return -EINVAL;
	pdata->smpl_warn_en = !!val;

	/* initial time in dt */
	pdata->init_time = devm_kzalloc(dev, sizeof(struct rtc_time),
					GFP_KERNEL);
	if (!pdata->init_time)
		return -ENOMEM;

	ret = of_property_read_u32(rtc_np, "init_time,sec",
			&pdata->init_time->tm_sec);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(rtc_np, "init_time,min",
			&pdata->init_time->tm_min);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(rtc_np, "init_time,hour",
			&pdata->init_time->tm_hour);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(rtc_np, "init_time,mday",
			&pdata->init_time->tm_mday);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(rtc_np, "init_time,mon",
			&pdata->init_time->tm_mon);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(rtc_np, "init_time,year",
			&pdata->init_time->tm_year);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(rtc_np, "init_time,wday",
			&pdata->init_time->tm_wday);
	if (ret)
		return -EINVAL;

	return 0;
};
#else
static int of_s2p_rtc_parse_dt(struct device *dev, struct s2p_rtc_pdata *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

int s2p_rtc_init_pdata(struct s2p_rtc_info *info, struct device *dev)
{
	struct s2p_rtc_pdata *pdata = NULL;
	int ret = 0;

	pdata = devm_kzalloc(info->dev, sizeof(struct s2p_rtc_pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_s2p_rtc_parse_dt(dev, pdata);
	if (ret < 0) {
		dev_err(dev, "%s: Failed to get device of_node\n", __func__);
		return ret;
	}
	info->pdata = pdata;

	static_info = info;

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_rtc_init_pdata);

int s2p_rtc_init_reg(struct s2p_rtc_info *info)
{
	struct s2p_rtc_pdata *pdata = info->pdata;
	uint8_t ctrl_val = 0;
	int ret = 0;

	ret = s2p_read_reg(info->sdev, info->rtc_addr, info->reg->update,
			&info->update_reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read update reg(%d)\n",
			__func__, ret);
		return ret;
	}

	info->update_reg_val &= ~(info->mask->wudr | info->mask->rudr |
			info->mask->audr | info->mask->freeze);

	ret = s2p_write_reg(info->sdev, info->rtc_addr, info->reg->update,
			info->update_reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
			__func__, ret);
		return ret;
	}

	ret = s2p_rtc_update(info, S2P_RTC_READ);
	if (ret < 0)
		return ret;

	ret = s2p_read_reg(info->sdev, info->rtc_addr, info->reg->ctrl,
			&ctrl_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to read control reg(%d)\n",
			__func__, ret);
		return ret;
	}

	/* If the value of RTC_CTRL register is 0, RTC registers were reset */
	if (ctrl_val & info->mask->model_24)
		return 0;

	/* Set RTC control register : Binary mode, 24hour mode */
	ret = s2p_write_reg(info->sdev, info->rtc_addr, info->reg->ctrl,
			info->mask->model_24);
	if (ret < 0) {
		dev_err(info->dev, "%s: write reg fail(%d)\n", __func__, ret);
		return ret;
	}

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_ALARM);
	if (ret < 0)
		return ret;

	ret = s2p_rtc_set_time(info, pdata->init_time);
	dev_info(info->dev, "%s: initialize RTC time\n", __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_rtc_init_reg);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t s2p_rtc_read_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->smpl_warn_cnt);
}

static struct pmic_device_attribute s2p_rtc_attr[] = {
	PMIC_ATTR(smpl_warn_cnt, S_IRUGO, s2p_rtc_read_show, NULL),
};

int s2p_create_rtc_sysfs(struct s2p_rtc_info *info, const char *mfd_dev_name, const uint32_t dev_id)
{
	struct device *rtc_sysfs = NULL;
	char device_name[S2P_NAME_MAX] = {0, };
	int ret = 0, i = 0;
	uint32_t cnt = 0;

	/* Dynamic allocation for device name */
	cnt = snprintf(info->sysfs_name, S2P_NAME_MAX - 1, "%s", mfd_dev_name);
	if (dev_id)
		snprintf(info->sysfs_name + cnt, S2P_NAME_MAX - 1, "-%d", dev_id);

	snprintf(device_name, S2P_NAME_MAX - 1, "%s-rtc", info->sysfs_name);

	info->rtc_sysfs_dev = pmic_device_create(info, device_name);
	rtc_sysfs = info->rtc_sysfs_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(s2p_rtc_attr); i++) {
		ret = device_create_file(rtc_sysfs, &s2p_rtc_attr[i].dev_attr);
		if (ret)
			goto remove_rtc_device;
	}

	return 0;

remove_rtc_device:
	for (i--; i >= 0; i--)
		device_remove_file(rtc_sysfs, &s2p_rtc_attr[i].dev_attr);
	pmic_device_destroy(rtc_sysfs->devt);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_create_rtc_sysfs);

void s2p_remove_rtc_sysfs_entries(struct device *rtc_sysfs)
{
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(s2p_rtc_attr); i++)
		device_remove_file(rtc_sysfs, &s2p_rtc_attr[i].dev_attr);
	pmic_device_destroy(rtc_sysfs->devt);
}
EXPORT_SYMBOL_GPL(s2p_remove_rtc_sysfs_entries);
#endif /* CONFIG_DRV_SAMSUNG_PMIC */

MODULE_LICENSE("GPL");
