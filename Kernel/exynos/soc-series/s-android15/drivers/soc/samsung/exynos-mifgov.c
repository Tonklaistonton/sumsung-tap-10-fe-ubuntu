/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/syscore_ops.h>
#include <linux/panic_notifier.h>
#include <soc/samsung/exynos-mifgov.h>

static struct mifgov_data *mifgov_data;

static int __mifgov_ipc_send_data(struct mifgov_data *data,
				union mifgov_ipc_message *message)
{
	struct ipc_config config;
	int ret = 0;

	config.cmd = message->data;
	config.response = true;
	config.indirection = false;

	ret = esca_ipc_send_data(data->ipc_ch_num, &config);
	if (ret) {
		pr_err("%s: Failed to send IPC(%u) data\n",
			__func__, data->ipc_ch_num);
	}

	MIFGOV_DBG_LOG(mifgov_data->dev, "return IPC: %u, %u, %u, %u, %u, %u\n",
			message->req.msg,
			message->req.domain,
			message->req.fw_use,
			message->req.resp_rsvd0,
			message->req.resp_rsvd1,
			message->req.resp_rsvd2);

	return ret;
}

static int mifgov_ipc_send_data(struct mifgov_data *data,
				union mifgov_ipc_message *message)
{
	return __mifgov_ipc_send_data(data, message);
}

static void exynos_mifgov_send_bw_params(struct mifgov_data *data)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.bw_params.msg = MIFGOV_BW_PARAMS;
	message.bw_params.get = IPC_SET;
	message.bw_params.num_channel = data->bw_params.num_channel;
	message.bw_params.bus_width =
		data->bw_params.bus_width << 16 | data->bw_params.mif_bus_width << 0;
	message.bw_params.util =
		data->bw_params.int_util << 16 | data->bw_params.mif_util << 0;

	mifgov_ipc_send_data(data, &message);
}

static void exynos_mifgov_get_bw_params(struct mifgov_data *data,
					struct mifgov_bw_params *params)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.gov_params.msg = MIFGOV_BW_PARAMS;
	message.gov_params.get = IPC_GET;

	mifgov_ipc_send_data(data, &message);

	params->num_channel = message.bw_params.num_channel;
	params->mif_bus_width = message.bw_params.bus_width & 0xFFFF;
	params->bus_width = message.bw_params.bus_width >> 16 & 0xFFFF;
	params->mif_util = message.bw_params.util & 0xFFFF;
	params->int_util = message.bw_params.util >> 16 & 0xFFFF;
}

static void exynos_mifgov_get_gov_params(struct mifgov_data *data,
					struct mifgov_gov_params *params)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.gov_params.msg = MIFGOV_GOV_PARAMS;
	message.gov_params.get = IPC_GET;

	mifgov_ipc_send_data(data, &message);

	params->hold_time = message.gov_params.hold_time;
	params->bratio = message.gov_params.bratio;
	params->period = message.gov_params.period;
}

static void exynos_mifgov_send_gov_params(struct mifgov_data *data)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.gov_params.msg = MIFGOV_GOV_PARAMS;
	message.gov_params.get = IPC_SET;
	message.gov_params.hold_time = data->gov_params.hold_time;
	message.gov_params.bratio = data->gov_params.bratio;
	message.gov_params.period = data->gov_params.period;

	mifgov_ipc_send_data(data, &message);
}

static void exynos_mifgov_set_run(struct mifgov_data *data, u32 run)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.run.msg = MIFGOV_RUN;
	message.run.get = IPC_SET;
	message.run.run = run;

	mifgov_ipc_send_data(data, &message);
}

static void exynos_mifgov_get_run(struct mifgov_data *data, u32 *run)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.run.msg = MIFGOV_RUN;
	message.run.get = IPC_GET;

	mifgov_ipc_send_data(data, &message);

	*run = message.run.run;
}

static void exynos_mifgov_send_ipc_dbg_log(struct mifgov_data *data)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.dbg_log.msg = MIFGOV_DBGLOG_EN;
	message.dbg_log.get = IPC_SET;
	message.dbg_log.ipc_dbg_log_en = data->ipc_dbg_log_en;

	mifgov_ipc_send_data(data, &message);
}

static void exynos_mifgov_get_ipc_dbg_log(struct mifgov_data *data)
{
	union mifgov_ipc_message message;

	memset(&message, 0, sizeof(union mifgov_ipc_message));
	message.dbg_log.msg = MIFGOV_DBGLOG_EN;
	message.dbg_log.get = IPC_GET;

	mifgov_ipc_send_data(data, &message);

	data->ipc_dbg_log_en = message.dbg_log.ipc_dbg_log_en;
}

void exynos_mifgov_run(u32 run, const char *name)
{
	int i;

	spin_lock(&mifgov_data->lock);
	if (mifgov_data->run) {
		for (i = 0; i < NUM_USER; i++) {
			if (!mifgov_data->user_info[i].listed) {
				mifgov_data->user_info[i].listed = 1;
				strcpy(mifgov_data->user_info[i].name, name);
				break;
			} else {
				if (!strcmp(name, mifgov_data->user_info[i].name))
					break;
			}
		}

		if (i == NUM_USER)
			goto skip;

		if (!run) {
			mifgov_data->user_info[i].use_cnt++;
			mifgov_data->user_cnt++;
		} else {
			if (mifgov_data->user_info[i].use_cnt)
				mifgov_data->user_info[i].use_cnt--;
			if (mifgov_data->user_cnt)
				mifgov_data->user_cnt--;

			if (mifgov_data->user_cnt)
				goto skip;
		}
		exynos_mifgov_set_run(mifgov_data, run);
	} else {
		pr_info("%s: MIFGOV is not enabled\n", __func__);
	}
skip:
	spin_unlock(&mifgov_data->lock);
	if (strcmp(name, SKIP_LOG_IP))
		pr_info("%s: %s Called: %s\n", __func__, name, run ? "ON" : "OFF");
}
EXPORT_SYMBOL(exynos_mifgov_run);

static ssize_t gov_params_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct mifgov_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	struct mifgov_gov_params gov_params;

	exynos_mifgov_get_gov_params(data, &gov_params);

	count += snprintf(buf, PAGE_SIZE, "HOLD_TIME: %d, BRATIO: %d, PERIOD: %d\n",
			gov_params.hold_time,
			gov_params.bratio,
			gov_params.period);

	return count;
}

static ssize_t bw_params_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct mifgov_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	struct mifgov_bw_params bw_params;

	exynos_mifgov_get_bw_params(data, &bw_params);

	count += snprintf
		(buf, PAGE_SIZE, "NUM_CHANNEL: %d, MIF_BUS_WIDTH: %d,\
		 BUS_WIDTH: %d, MIF_UTIL: %d, INT_UTIL: %d\n",
			bw_params.num_channel,
			bw_params.mif_bus_width, bw_params.bus_width,
			bw_params.mif_util, bw_params.int_util);

	return count;
}

static ssize_t run_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct mifgov_data *data = platform_get_drvdata(pdev);
	unsigned int run;
	int ret;

	ret = sscanf(buf, "%u", &run);
	if (ret != 1) {
		pr_err("%s: usage: echo [EN] > run\n", __func__);
		return -EINVAL;
	}

	spin_lock(&mifgov_data->lock);
	data->run = run;
	exynos_mifgov_set_run(mifgov_data, run);
	spin_unlock(&mifgov_data->lock);

	return count;
}

static ssize_t run_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct mifgov_data *data = platform_get_drvdata(pdev);
	ssize_t count = 0;
	u32 run = 0;

	exynos_mifgov_get_run(data, &run);

	count += snprintf(buf, PAGE_SIZE, "RUN: %d\n", run);

	return count;
}

static ssize_t hold_time_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct mifgov_data *data = platform_get_drvdata(pdev);
	unsigned int hold_time;
	int ret;

	ret = sscanf(buf, "%u", &hold_time);
	if (ret != 1) {
		pr_err("%s: usage: echo [hold_time(msec)] > hold_time\n", __func__);
		return -EINVAL;
	}

	data->gov_params.hold_time = hold_time;
	exynos_mifgov_send_gov_params(mifgov_data);

	return count;
}

static ssize_t bratio_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct mifgov_data *data = platform_get_drvdata(pdev);
	unsigned int bratio;
	int ret;

	ret = sscanf(buf, "%u", &bratio);
	if (ret != 1) {
		pr_err("%s: usage: echo [ratio] > bratio\n", __func__);
		return -EINVAL;
	}

	data->gov_params.bratio = bratio;
	exynos_mifgov_send_gov_params(mifgov_data);

	return count;
}

static ssize_t period_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
					struct platform_device, dev);
	struct mifgov_data *data = platform_get_drvdata(pdev);
	unsigned int period;
	int ret;

	ret = sscanf(buf, "%u", &period);
	if (ret != 1) {
		pr_err("%s: usage: echo [ratio] > period\n", __func__);
		return -EINVAL;
	}

	data->gov_params.period = period;
	exynos_mifgov_send_gov_params(mifgov_data);

	return count;
}

static ssize_t dbg_log_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "dbg_log: %s\n",
			mifgov_dbg_log ? "enabled" : "disabled");

	return count;
}

static ssize_t dbg_log_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int dbg_log;
	int ret;

	ret = sscanf(buf, "%u", &dbg_log);
	if (ret != 1) {
		pr_err("%s: usage: echo [enable] > dbg_log\n", __func__);
		return -EINVAL;
	}

	mifgov_dbg_log = dbg_log;

	return count;
}

static ssize_t ipc_dbg_log_en_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int dbg_log;
	int ret;

	ret = sscanf(buf, "%u", &dbg_log);
	if (ret != 1) {
		pr_err("%s: usage: echo [enable] > ipc_dbg_log_en\n", __func__);
		return -EINVAL;
	}

	mifgov_data->ipc_dbg_log_en = dbg_log;
	exynos_mifgov_send_ipc_dbg_log(mifgov_data);

	return count;
}

static ssize_t ipc_dbg_log_en_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t count = 0;

	exynos_mifgov_get_ipc_dbg_log(mifgov_data);
	count += snprintf(buf + count, PAGE_SIZE, "ipc_dbg_log: %s\n",
			mifgov_data->ipc_dbg_log_en ? "enabled" : "disabled");

	return count;
}

static DEVICE_ATTR_RO(gov_params);
static DEVICE_ATTR_RO(bw_params);
static DEVICE_ATTR_RW(run);
static DEVICE_ATTR_WO(hold_time);
static DEVICE_ATTR_WO(bratio);
static DEVICE_ATTR_WO(period);
static DEVICE_ATTR_RW(dbg_log);
static DEVICE_ATTR_RW(ipc_dbg_log_en);

static struct attribute *exynos_mifgov_sysfs_entries[] = {
	&dev_attr_gov_params.attr,
	&dev_attr_bw_params.attr,
	&dev_attr_run.attr,
	&dev_attr_hold_time.attr,
	&dev_attr_bratio.attr,
	&dev_attr_period.attr,
	&dev_attr_dbg_log.attr,
	&dev_attr_ipc_dbg_log_en.attr,
	NULL,
};

static struct attribute_group exynos_mifgov_attr_group = {
	.name	= "mifgov_attr",
	.attrs = exynos_mifgov_sysfs_entries,
};

static int exynos_mifgov_sysfs_init(void)
{
	int ret = 0;

	ret = sysfs_create_group(&mifgov_data->dev->kobj, &exynos_mifgov_attr_group);
	if (ret)
		return -ENOMEM;

	return 0;
}

static void mifgov_ipc_handler(unsigned int *cmd, unsigned int size)
{
	u32 req_freq = cmd[0];
	u32 next_bw = cmd[1];

	pr_info("%s: req_freq: %uKhz, next_bw: %uMB/sec\n", __func__, req_freq, next_bw);
}

static int mifgov_panic_handler(struct notifier_block *nb, unsigned long l, void *buf)
{
	spin_lock(&mifgov_data->lock);
	if (mifgov_data->run)
		exynos_mifgov_set_run(mifgov_data, 0);
	spin_unlock(&mifgov_data->lock);

	return NOTIFY_DONE;
}

static int mifgov_suspend(struct device *dev)
{
	spin_lock(&mifgov_data->lock);
	if (mifgov_data->run)
		exynos_mifgov_set_run(mifgov_data, 0);
	spin_unlock(&mifgov_data->lock);

	return 0;
}

static int mifgov_resume(struct device *dev)
{
	spin_lock(&mifgov_data->lock);
	if (mifgov_data->run)
		exynos_mifgov_set_run(mifgov_data, mifgov_data->run);
	spin_unlock(&mifgov_data->lock);

	return 0;
}

static struct dev_pm_ops mifgov_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(mifgov_suspend, mifgov_resume)
};

static struct notifier_block nb_mifgov_panic = {
	.notifier_call = mifgov_panic_handler,
	.priority = INT_MAX,
};

static int mifgov_probe(struct platform_device *pdev)
{
	const char *tmp;
	int ret = 0;
	struct device_node *child_np = NULL;

	mifgov_data = devm_kzalloc(&pdev->dev, sizeof(struct mifgov_data), GFP_KERNEL);
	if (mifgov_data == NULL)
		return -ENOMEM;

	mifgov_data->dev = &pdev->dev;

	if (strcmp(pdev->name, "exynos-mifgov") == 0) {
		spin_lock_init(&mifgov_data->lock);
		if (of_have_populated_dt()) {
			if(of_property_read_u32_array(mifgov_data->dev->of_node, "bw_params",
						(u32 *)&mifgov_data->bw_params,
						sizeof(struct mifgov_bw_params)/sizeof(int))) {
				pr_err("%s: Unable to parse bandwidth params\n", __func__);
				ret = -EINVAL;
				return ret;
			}

			if(of_property_read_u32_array(mifgov_data->dev->of_node, "gov_params",
						(u32 *)&mifgov_data->gov_params,
						sizeof(struct mifgov_gov_params)/sizeof(int))) {
				pr_err("%s: Unable to parse governor params\n", __func__);
				ret = -EINVAL;
				return ret;
			}
			tmp = of_get_property(mifgov_data->dev->of_node, "use-mifgov", NULL);
			if (tmp && !strcmp(tmp, "enabled"))
				mifgov_data->run = true;
			else
				mifgov_data->run = false;


		} else {
			dev_err(mifgov_data->dev, "failed to get mifgov data from device tree\n");
		}
	}

	atomic_notifier_chain_register(&panic_notifier_list, &nb_mifgov_panic);
	platform_set_drvdata(pdev, mifgov_data);

	ret = exynos_mifgov_sysfs_init();

	child_np = of_get_child_by_name(mifgov_data->dev->of_node, "mifgov_noti_ipc");
	if (child_np) {
		ret = esca_ipc_request_channel(child_np, (ipc_callback)mifgov_ipc_handler,
				&mifgov_data->ipc_noti_ch_num, &mifgov_data->ipc_noti_ch_size);
		pr_info("%s: mifgov_noti_ipc_ch:%u, size: %u\n", __func__, mifgov_data->ipc_noti_ch_num,
				mifgov_data->ipc_noti_ch_size);
	}

	ret = esca_ipc_request_channel(mifgov_data->dev->of_node, NULL,
				&mifgov_data->ipc_ch_num, &mifgov_data->ipc_ch_size);

	pr_info("%s: esca request channel, mifgov ipc_ch: %u, size: %u\n",
				__func__, mifgov_data->ipc_ch_num, mifgov_data->ipc_ch_size);
	if (ret) {
		pr_info("%s: esca request channel is failed, ipc_ch: %u, size: %u\n",
				__func__, mifgov_data->ipc_ch_num, mifgov_data->ipc_ch_size);
		return ret;
	}

	exynos_mifgov_send_bw_params(mifgov_data);
	exynos_mifgov_send_gov_params(mifgov_data);
	if (mifgov_data->run)
		exynos_mifgov_set_run(mifgov_data, mifgov_data->run);

	pr_info("%s successfully done.\n", __func__);
	return ret;
}

static int mifgov_remove(struct platform_device *pdev)
{
	devm_kfree(&pdev->dev, mifgov_data);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

/* Device tree compatible information */
static const struct of_device_id exynos_mifgov_match[] = {
	{
		.compatible = "samsung,exynos-mifgov",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mifgov_match);

static struct platform_driver mifgov_pdrv = {
	.probe			= mifgov_probe,
	.remove			= mifgov_remove,
	.driver			= {
		.name		= "exynos-mifgov",
		.pm = &mifgov_pm_ops,
		.of_match_table	= exynos_mifgov_match,
	},
};

static int exynos_mifgov_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mifgov_pdrv);
	if (ret) {
		pr_err("Platform driver registration failed (err=%d)\n", ret);
		return ret;
	}

	pr_info("%s successfully done.\n", __func__);

	return 0;
}
arch_initcall(exynos_mifgov_init);

MODULE_SOFTDEP("pre: exynos_devfreq");
MODULE_LICENSE("GPL");
