/*
 * rtc-s2se910.c
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pmic/s2p_rtc.h>
#include <linux/pmic/s2se910-mfd.h>
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
#include <linux/reboot.h>
#include <linux/fs.h>
#include <soc/samsung/exynos-pmu-if.h>

#define POWER_SYSIP_INFORM3       0x080C // TODO: check it out
static unsigned int is_charging_mode;
static int boot_alarm_irq;
static bool boot_alarm_enabled;
static struct delayed_work restart_work;
static struct workqueue_struct *restart_wqueue;
#endif /* CONFIG_RTC_BOOT_ALARM */

static int s2se910_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return s2p_rtc_read_time(info, tm);
}

static int s2se910_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return s2p_rtc_set_time(info, tm);
}

static int s2se910_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return s2p_rtc_read_alarm(info, alrm);
}

static int s2se910_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return s2p_rtc_set_alarm(info, alrm);
}

static int s2se910_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return s2p_rtc_alarm_irq_enable(info, enabled);
}

static const struct rtc_class_ops s2se910_rtc_ops = {
	.read_time = s2se910_rtc_read_time,
	.set_time = s2se910_rtc_set_time,
	.read_alarm = s2se910_rtc_read_alarm,
	.set_alarm = s2se910_rtc_set_alarm,
	.alarm_irq_enable = s2se910_rtc_alarm_irq_enable,
};

#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
static int s2se910_rtc_read_boot_time(const struct s2p_rtc_info *info, struct rtc_time *tm,
					uint8_t reg, uint8_t *data, const char *func_name)
{
	int ret = 0;

	ret = s2p_bulk_read(info->sdev, info->rtc_addr, reg, NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	s2p_data_to_tm(data, tm);

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(%d)\n", func_name,
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_wday);

	return ret;
}

static int s2se910_rtc_stop_boot_alarm0(struct s2p_rtc_info *info)
{
	uint8_t data[NR_RTC_CNT_REGS];
	int ret = 0, i = 0;
	struct rtc_time tm;

	ret = s2se910_rtc_read_boot_time(info, &tm, S2SE910_RTC_RTC_A0SEC, data, __func__);
	if (ret < 0)
		return ret;

	for (i = 0; i < NR_RTC_CNT_REGS; i++)
		data[i] &= ~S2SE910_RTC_ALARM_ENABLE_MASK;

	ret = s2p_bulk_write(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_A0SEC,
			NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_ALARM);

	return ret;
}

static int s2se910_rtc_stop_boot_alarm(struct s2p_rtc_info *info)
{
	uint8_t data[NR_RTC_CNT_REGS];
	int ret = 0, i = 0;
	struct rtc_time tm;

	ret = s2se910_rtc_read_boot_time(info, &tm, S2SE910_RTC_RTC_A1SEC, data, __func__);
	if (ret < 0)
		return ret;

	for (i = 0; i < NR_RTC_CNT_REGS; i++)
		data[i] &= ~S2SE910_RTC_ALARM_ENABLE_MASK;

	ret = s2p_bulk_write(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_A1SEC,
			NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_ALARM);

	return ret;
}

static int s2se910_rtc_start_boot_alarm(struct s2p_rtc_info *info)
{
	int ret = 0;
	uint8_t data[NR_RTC_CNT_REGS];
	struct rtc_time tm;

	ret = s2se910_rtc_read_boot_time(info, &tm, S2SE910_RTC_RTC_A1SEC, data, __func__);
	if (ret < 0)
		return ret;

	data[RTC_SEC] |= S2SE910_RTC_ALARM_ENABLE_MASK;
	data[RTC_MIN] |= S2SE910_RTC_ALARM_ENABLE_MASK;
	data[RTC_HOUR] |= S2SE910_RTC_ALARM_ENABLE_MASK;
	data[RTC_WEEKDAY] &= 0x00;

	if (data[RTC_DATE] & 0x1F)
		data[RTC_DATE] |= S2SE910_RTC_ALARM_ENABLE_MASK;
	if (data[RTC_MONTH] & 0x0F)
		data[RTC_MONTH] |= S2SE910_RTC_ALARM_ENABLE_MASK;
	if (data[RTC_YEAR] & 0x7F)
		data[RTC_YEAR] |= S2SE910_RTC_ALARM_ENABLE_MASK;

	ret = s2p_bulk_write(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_A1SEC,
			NR_RTC_CNT_REGS, data);
	if (ret < 0)
		return ret;

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_ALARM);

	return ret;
}

static int s2se910_rtc_read_boot_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);
	uint8_t data[NR_RTC_CNT_REGS];
	uint8_t val = 0;
	int ret = 0;

	mutex_lock(&info->lock);

	ret = s2p_rtc_update(info, S2P_RTC_READ);
	if (ret < 0)
		goto out;

	ret = s2se910_rtc_read_boot_time(info, &alrm->time, S2SE910_RTC_RTC_A1SEC, data, __func__);
	if (ret < 0)
		goto out;

	alrm->enabled = boot_alarm_enabled;

	ret = s2p_read_reg(info->sdev, info->rtc_addr, S2SE910_RTC_STATUS2,
			&val);
	if (ret < 0) {
		dev_err(info->dev, "%s: %d fail to read STATUS2 reg(%d)\n",
			__func__, __LINE__, ret);
		goto out;
	}

	if (val & S2SE910_RTCA1E)
		alrm->pending = 1;
	else
		alrm->pending = 0;

out:
	mutex_unlock(&info->lock);

	return ret;
}

static int _s2se910_rtc_set_boot_alarm(struct s2p_rtc_info *info,
				struct rtc_wkalrm *alrm)
{
	uint8_t data[NR_RTC_CNT_REGS];
	int ret = 0;

	mutex_lock(&info->lock);

	ret = s2p_tm_to_data(&alrm->time, data);
	if (ret < 0)
		goto err;

	dev_info(info->dev, "%s: %d-%02d-%02d %02d:%02d:%02d(0x%02x)%s, [%s]\n",
			__func__, data[RTC_YEAR] + 2000, data[RTC_MONTH],
			data[RTC_DATE], data[RTC_HOUR] & 0x1F, data[RTC_MIN],
			data[RTC_SEC], data[RTC_WEEKDAY],
			data[RTC_HOUR] & S2SE910_RTC_HOUR_AMPM_MASK ? "PM" : "AM",
			alrm->enabled ? "enabled" : "disabled");

	ret = s2se910_rtc_stop_boot_alarm(info);
	if (ret < 0) {
		dev_err(info->dev, "%s: stop_boot_alarm failed\n", __func__);
		goto err;
	}

	ret = s2p_read_reg(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_UPDATE,
			&info->update_reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: read reg fail(%d)\n", __func__, ret);
		goto err;
	}

	if (alrm->enabled)
		info->update_reg_val |= S2SE910_RTC_WAKE_MASK;
	else
		info->update_reg_val &= ~S2SE910_RTC_WAKE_MASK;

	ret = s2p_write_reg(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_UPDATE,
			info->update_reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: write reg fail(%d)\n", __func__, ret);
		goto err;
	}
	else
		usleep_range(1000, 1005);

	ret = s2p_bulk_write(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_A1SEC,
			NR_RTC_CNT_REGS, data);
	if (ret < 0) {
		dev_err(info->dev, "%s: bulk write reg fail(%d)\n", __func__,
			ret);
		goto err;
	}

	ret = s2p_rtc_update(info, S2P_RTC_WRITE_ALARM);
	if (ret < 0) {
		dev_err(info->dev, "%s: rtc_update fail(%d)\n", __func__, ret);
		goto err;
	}

	if (alrm->enabled)
		ret = s2se910_rtc_start_boot_alarm(info);

err:
	mutex_unlock(&info->lock);

	return ret;
}

static int _s2se910_rtc_disable_boot_alarm(struct s2p_rtc_info *info, struct rtc_wkalrm *alrm)
{
	int ret = 0;

	mutex_lock(&info->lock);

	ret = s2se910_rtc_stop_boot_alarm(info);
	if (ret < 0) {
		dev_err(info->dev, "%s: stop_boot_alarm failed\n", __func__);
		goto err;
	}

	ret = s2p_read_reg(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_UPDATE,
			&info->update_reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: read reg fail(%d)\n", __func__, ret);
		goto err;
	}

	info->update_reg_val &= ~S2SE910_RTC_WAKE_MASK;

	ret = s2p_write_reg(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_UPDATE,
			info->update_reg_val);
	if (ret < 0)
		dev_err(info->dev, "%s: write reg fail(%d)\n", __func__, ret);

err:
	mutex_unlock(&info->lock);

	return ret;
}

static int s2se910_rtc_set_boot_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);
	int ret;

	boot_alarm_enabled = alrm->enabled;
	/*if get disabled bootalarm, directly stop, do not set time*/
	if (!alrm->enabled)
		ret = _s2se910_rtc_disable_boot_alarm(info, alrm);
	else
		ret = _s2se910_rtc_set_boot_alarm(info, alrm);

	return ret;
}

static void s2se910_rtc_restart_work_func(struct work_struct *work)
{
	kernel_restart(NULL);
}

static int s2se910_rtc_set_restart_wqueue(void)
{
	restart_wqueue = create_singlethread_workqueue("rtc-restart-wqueue");
	if (!restart_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&restart_work, s2se910_rtc_restart_work_func);

	return 0;
}

static irqreturn_t s2se910_rtc_boot_alarm_irq(int irq, void *data)
{
	struct s2p_rtc_info *info = data;

	dev_info(info->dev, "[PMIC] %s: irq(%d)\n", __func__, irq);

	rtc_update_irq(info->rdev, 1, RTC_IRQF | RTC_AF);
	__pm_wakeup_event(info->rtc_ws, 500);

	dev_info(info->dev, "[PMIC] %s: is_charging_mode(%d)\n", __func__, is_charging_mode);
	if (boot_alarm_enabled) {
		if (is_charging_mode)
			queue_delayed_work(restart_wqueue, &restart_work, 1);
		else {
			boot_alarm_enabled = 0;
			info->update_reg_val &= ~S2SE910_RTC_WAKE_MASK;
			s2p_write_reg(info->sdev, info->rtc_addr,
				S2SE910_RTC_RTC_UPDATE, info->update_reg_val);
		}
	}

	return IRQ_HANDLED;
}

static ssize_t bootalarm_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct rtc_wkalrm alrm;
	struct rtc_device *rtc = to_rtc_device(dev);

	/* Don't show disabled alarms.  For uniformity, RTC alarms are
	 * conceptually one-shot, even though some common RTCs (on PCs)
	 * don't actually work that way.
	 *
	 * NOTE: RTC implementations where the alarm doesn't match an
	 * exact YYYY-MM-DD HH:MM[:SS] date *must* disable their RTC
	 * alarms after they trigger, to ensure one-shot semantics.
	 */
	ret = mutex_lock_interruptible(&rtc->ops_lock);
	if (ret)
		return ret;

	ret = s2se910_rtc_read_boot_alarm(rtc->dev.parent, &alrm);
	mutex_unlock(&rtc->ops_lock);
	if (ret == 0) {
		ret = sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d(%d) [%s, %s]\n",
			alrm.time.tm_year + 1900, alrm.time.tm_mon + 1,
			alrm.time.tm_mday, alrm.time.tm_hour, alrm.time.tm_min,
			alrm.time.tm_sec, alrm.time.tm_wday,
			((alrm.enabled == 1) ? "Enable" : "Disable"),
			((alrm.pending == 1) ? "Pending" : "Not Pending"));
	}

	return ret;
}

static ssize_t bootalarm_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	ssize_t retval = 0;
	unsigned long now = 0, alrm_long = 0;
	struct rtc_wkalrm alarm;
	struct rtc_device *rtc = to_rtc_device(dev);
	char *buf_ptr = NULL;
	int adjust = 0;
	int ret = 0;

	/* Only request alarms to be triggered in the future.
	 * by write another time, e.g. 0 meaning Jan 1 1970 UTC.
	 */
	retval = rtc_read_time(rtc, &alarm.time);
	if (retval < 0)
		return retval;
	now = rtc_tm_to_time64(&alarm.time);

	buf_ptr = (char *)buf;
	if (*buf_ptr == '+') {
		buf_ptr++;
		adjust = 1;
	}

	ret = kstrtoul(buf_ptr, 0, &alrm_long);
	if (ret) {
		pr_info("%s: kstrtoul error\n", __func__);
		return -1;
	}
	if (alrm_long == 0)
		alarm.enabled = 0;
	else
		alarm.enabled = 1;
	if (adjust)
		alrm_long += now;

	rtc_time64_to_tm(alrm_long, &alarm.time);

	retval = mutex_lock_interruptible(&rtc->ops_lock);
	if (retval)
		return retval;

	retval = s2se910_rtc_set_boot_alarm(rtc->dev.parent, &alarm);
	mutex_unlock(&rtc->ops_lock);

	return (retval < 0) ? retval : n;
}

static DEVICE_ATTR_RW(bootalarm);
static struct attribute *s2se910_rtc_attrs[] = {
	&dev_attr_bootalarm.attr,
	NULL
};

static const struct attribute_group s2se910_rtc_sysfs_files = {
	.attrs	= s2se910_rtc_attrs,
};

static int s2se910_rtc_init_boot_alarm_reg(struct s2p_rtc_info *info)
{
	uint8_t data[NR_RTC_CNT_REGS];
	uint8_t update_val = 0;
	struct rtc_time alrm, now;
	unsigned long alrm_int = 0 , now_int = 0;
	int ret = 0;

	ret = s2se910_rtc_read_boot_time(info, &alrm, S2SE910_RTC_RTC_A1SEC, data, __func__);
	if (ret < 0)
		return ret;

	ret = s2p_read_reg(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_UPDATE,
			&update_val);
	if (ret < 0) {
		dev_err(info->dev, "%s: read update reg fail(%d)\n",
			__func__, ret);
		return ret;
	}

	boot_alarm_enabled = (update_val & S2SE910_RTC_WAKE_MASK) ? 1 : 0;
	if (boot_alarm_enabled) {
		s2se910_rtc_read_time(info->dev, &now);
		now_int = rtc_tm_to_time64(&now);
		alrm_int = rtc_tm_to_time64(&alrm);
		if (now_int >= alrm_int) {
			boot_alarm_enabled = 0;
			update_val &= ~S2SE910_RTC_WAKE_MASK;
		}
	}

	info->update_reg_val = update_val;

	ret = s2p_write_reg(info->sdev, info->rtc_addr, S2SE910_RTC_RTC_UPDATE,
			info->update_reg_val);
	if (ret < 0)
		dev_err(info->dev, "%s: fail to write update reg(%d)\n",
			__func__, ret);

	return ret;
}

static int s2se910_rtc_init_boot_alarm(struct s2p_rtc_info *info)
{
	int ret = 0;

	boot_alarm_irq = info->sdev->irq_base + S2SE910_PMIC_IRQ_RTCA1_PM_INT2;

	ret = exynos_pmu_read(POWER_SYSIP_INFORM3, &is_charging_mode);
	if (ret < 0) {
		dev_err(info->dev, "Failed to get charge mode: %d\n", ret);
		return ret;
	}

	ret = s2se910_rtc_set_restart_wqueue();
	if (ret < 0) {
		dev_err(info->dev, "Failed to set workqueue: %d\n", ret);
		return ret;
	}

	/* Set a VGPIO for ALARM1 IRQ */
	ret = devm_request_threaded_irq(info->dev, boot_alarm_irq, NULL,
					s2se910_rtc_boot_alarm_irq, 0,
					"rtc-alarm1", info);
	if (ret < 0) {
		dev_err(info->dev, "Failed to request alarm1 IRQ: %d: %d\n",
			boot_alarm_irq, ret);
		return ret;
	}

	ret = rtc_add_group(info->rdev, &s2se910_rtc_sysfs_files);
	if (ret < 0)
		dev_err(info->dev, "Failed to create sysfs: %d\n", ret);

	return ret;
}
#endif /* CONFIG_RTC_BOOT_ALARM */

static int s2se910_rtc_set_register_info(struct s2p_rtc_info *info)
{
	struct s2p_rtc_reg *reg;
	struct s2p_rtc_mask *mask;
	struct s2p_rtc_shift *shift;

	reg = devm_kzalloc(info->dev, sizeof(struct s2p_rtc_reg), GFP_KERNEL);
	if (!reg)
		return -ENOMEM;

	mask = devm_kzalloc(info->dev, sizeof(struct s2p_rtc_mask), GFP_KERNEL);
	if (!mask)
		return -ENOMEM;

	shift = devm_kzalloc(info->dev, sizeof(struct s2p_rtc_shift), GFP_KERNEL);
	if (!shift)
		return -ENOMEM;

	/* Set register and mask value in RTC sheet */
	reg->pm1_status1 = S2SE910_RTC_STATUS1;
	reg->pm1_status2 = S2SE910_RTC_STATUS2;

	mask->jigonb = BIT(1);
	mask->alarm0_en = S2SE910_RTCA0E;

	/* Set register and mask/shift value in RTC sheet */
	reg->ctrl = S2SE910_RTC_RTC_CTRL;
	reg->wtsr_smpl = S2SE910_RTC_RTC_WTSR_SMPL;
	reg->update = S2SE910_RTC_RTC_UPDATE;
	reg->time_sec = S2SE910_RTC_RTC_SEC;
	reg->alarm_sec[0] = S2SE910_RTC_RTC_A0SEC;
	reg->alarm_sec[1] = S2SE910_RTC_RTC_A1SEC;
	reg->secure[0] = S2SE910_RTC_RTC_SECURE1;
	reg->secure[1] = S2SE910_RTC_RTC_SECURE2;
	reg->secure[2] = S2SE910_RTC_RTC_SECURE3;
	reg->secure[3] = S2SE910_RTC_RTC_SECURE4;

	mask->bcd_en = S2SE910_RTC_BCD_EN_MASK;
	mask->model_24 = S2SE910_RTC_MODEL24_MASK;
	mask->rudr = S2SE910_RTC_RUDR_MASK;
	mask->wudr = S2SE910_RTC_WUDR_MASK;
	mask->audr = S2SE910_RTC_AUDR_MASK;
	mask->freeze = S2SE910_RTC_FREEZE_MASK;
	mask->hour_pm = S2SE910_RTC_HOUR_AMPM_MASK;
	mask->alarm_enable = S2SE910_RTC_ALARM_ENABLE_MASK;
	mask->wtsr_en = S2SE910_RTC_WTSR_EN_MASK;
	mask->smpl_en = S2SE910_RTC_SMPL_EN_MASK;
	mask->wtsrt = S2SE910_RTC_WTSRT_MASK;
	mask->smplt = S2SE910_RTC_SMPLT_MASK;

	shift->wtsr_en = S2SE910_RTC_WTSR_EN_SHIFT;
	shift->smpl_en = S2SE910_RTC_SMPL_EN_SHIFT;
	shift->wtsrt = S2SE910_RTC_WTSRT_SHIFT;
	shift->smplt = S2SE910_RTC_SMPLT_SHIFT;

	info->reg = reg;
	info->mask = mask;
	info->shift = shift;

	return 0;
}

static int s2se910_rtc_probe(struct platform_device *pdev)
{
	struct s2se910_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2p_rtc_info *info = NULL;
	int ret = 0;

	pr_info("[MAIN_PMIC] %s: start\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct s2p_rtc_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &pdev->dev;
	info->sdev = iodev->sdev;
	info->rtc_addr = iodev->rtc;
	info->pm1_addr = iodev->rtc;
	info->alarm0_irq_enum = S2SE910_PMIC_IRQ_RTCA0_PM_INT2;

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);

	ret = s2se910_rtc_set_register_info(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "s2se910_rtc_set_register_info fail(%d)\n",
			ret);
		goto err_rtc_init_reg;
	}

	ret = s2p_rtc_init_pdata(info, iodev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "s2p_rtc_init_pdata fail(%d)\n", ret);
		goto err_rtc_init_reg;
	}

	ret = s2p_rtc_init_reg(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "s2p_rtc_init_reg fail(%d)\n", ret);
		goto err_rtc_init_reg;
	}
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	ret = s2se910_rtc_init_boot_alarm_reg(info);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"s2se910_rtc_init_boot_alarm_reg fail(%d)\n", ret);
		goto err_rtc_init_reg;
	}
#endif
	info->wtsr_en = info->pdata->wtsr_en;
	s2p_rtc_set_wtsr_timer(info);
	s2p_rtc_set_wtsr(info, info->wtsr_en);

	info->smpl_en = info->pdata->smpl_en;
	s2p_rtc_set_smpl_timer(info);
	s2p_rtc_set_smpl(info, info->smpl_en);

	ret = device_init_wakeup(info->dev, true);
	if (ret < 0) {
		dev_err(&pdev->dev, "device_init_wakeup fail(%d)\n", ret);
		goto err_device_init_wakeup;
	}
	info->rtc_ws = wakeup_source_register(info->dev, "rtc-s2p");

	info->rdev = devm_rtc_device_register(&pdev->dev, "s2se910-rtc",
					&s2se910_rtc_ops, THIS_MODULE);
	if (IS_ERR(info->rdev)) {
		dev_err(&pdev->dev, "devm_rtc_device_register fail(%d)\n", ret);
		ret = PTR_ERR(info->rdev);
		goto err_rtc_init;
	}

	ret = s2p_rtc_set_interrupt(info, info->sdev->irq_base);
	if (ret < 0) {
		dev_err(&pdev->dev, "s2p_rtc_set_interrupt fail(%d)\n", ret);
		goto err_rtc_init;
	}

#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	ret = s2se910_rtc_init_boot_alarm(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "s2se910_rtc_init_boot_alarm fail(%d)\n",
			ret);
		goto err_rtc_init;
	}
#endif

	/* Set secure debug */
	ret = s2p_secure_debug_set_config(S2SE910_RTC_RTC_SECURE1, RTC_BOOTING);
	if (ret < 0) {
		dev_err(&pdev->dev, "s2p_secure_debug_set_config fail(%d)\n",
			ret);
		goto err_rtc_init;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2p_create_rtc_sysfs(info, MFD_DEV_NAME, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: fail to create rtc sysfs(%d)\n", __func__, ret);
		goto err_rtc_init;
	}
#endif

	/* Add Asynchronous suspend and resume of devices WA in solomon */
	device_enable_async_suspend(info->dev);

	pr_info("[MAIN_PMIC] %s: end\n", __func__);

	return 0;

err_rtc_init:
	wakeup_source_unregister(info->rtc_ws);
err_device_init_wakeup:
	device_init_wakeup(&pdev->dev, false);
err_rtc_init_reg:
	mutex_destroy(&info->lock);

	return ret;
}

static int s2se910_rtc_remove(struct platform_device *pdev)
{
	struct s2p_rtc_info *info = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	s2p_remove_irq_sysfs_entries(info->rtc_sysfs_dev);
#endif

	if (!info->alarm_enabled)
		enable_irq(info->alarm0_irq);
	if (info->dev->power.wakeup)
		device_init_wakeup(&pdev->dev, false);
	mutex_destroy(&info->lock);
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	cancel_delayed_work_sync(&restart_work);
	destroy_workqueue(restart_wqueue);
#endif
	wakeup_source_unregister(info->rtc_ws);

	return 0;
}

static void s2se910_rtc_shutdown(struct platform_device *pdev)
{
	struct s2p_rtc_info *info = platform_get_drvdata(pdev);

	if (info->wtsr_en)
		s2p_rtc_set_wtsr(info, false);
	if (info->smpl_en)
		s2p_rtc_set_smpl(info, false);
#if IS_ENABLED(CONFIG_RTC_BOOT_ALARM)
	s2se910_rtc_stop_boot_alarm0(info);
#endif

	/* Set secure debug */
	if (s2p_secure_debug_set_config(S2SE910_RTC_RTC_SECURE1, RTC_SHUTDOWN) < 0)
		pr_err("%s: s2p_secure_debug_set_config failed\n", __func__);
}

static const struct platform_device_id s2se910_rtc_id[] = {
	{ "s2se910-rtc", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, s2se910_rtc_id);

static int s2se910_rtc_suspend(struct device *dev)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return s2p_rtc_set_config(info, RTC_SUSPEND);
}

static int s2se910_rtc_resume(struct device *dev)
{
	struct s2p_rtc_info *info = dev_get_drvdata(dev);

	return s2p_rtc_set_config(info, RTC_RESUME);
}

static SIMPLE_DEV_PM_OPS(s2se910_rtc_pm, s2se910_rtc_suspend, s2se910_rtc_resume);

static struct platform_driver s2se910_rtc_driver = {
	.driver		= {
		.name	= "s2se910-rtc",
		.owner	= THIS_MODULE,
		.pm	= &s2se910_rtc_pm,
	},
	.probe		= s2se910_rtc_probe,
	.remove		= s2se910_rtc_remove,
	.shutdown	= s2se910_rtc_shutdown,
	.id_table	= s2se910_rtc_id,
};

module_platform_driver(s2se910_rtc_driver);

/* Module information */
MODULE_DESCRIPTION("Samsung RTC driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
