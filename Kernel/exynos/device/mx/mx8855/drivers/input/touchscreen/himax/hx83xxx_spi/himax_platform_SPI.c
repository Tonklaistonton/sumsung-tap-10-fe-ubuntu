/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for QCT platform
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_common.h"
#include "himax_platform_SPI.h"
#include <linux/spi/spi.h>


int i2c_error_count;
struct spi_device *spi;

static uint8_t *gBuffer;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
irqreturn_t himax_ts_thread(int irq, void *ptr);
irqreturn_t himax_secure_filter_interrupt(struct himax_ts_data *ts)
{
	if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, ts->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->plat_data->secure_pending_irqs));
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}
/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
static ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->plat_data->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->spi->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->spi->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->spi->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_delay(200);

		/* synchronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->spi->irq);

		/* Release All Finger */
		sec_input_release_all_finger(&ts->spi->dev);

		if (pm_runtime_get_sync(ts->plat_data->bus_master->parent) < 0) {
			enable_irq(ts->spi->irq);
			input_err(true, &ts->spi->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->himax_input_nb, NOTIFIER_SECURE_TOUCH_ENABLE, NULL);
#endif
		reinit_completion(&ts->plat_data->secure_powerdown);
		reinit_completion(&ts->plat_data->secure_interrupt);

		atomic_set(&ts->plat_data->secure_enabled, 1);
		atomic_set(&ts->plat_data->secure_pending_irqs, 0);

		enable_irq(ts->spi->irq);

		input_info(true, &ts->spi->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->spi->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(ts->plat_data->bus_master->parent);
		atomic_set(&ts->plat_data->secure_enabled, 0);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		himax_ts_thread(ts->spi->irq, ts);
		complete(&ts->plat_data->secure_interrupt);
		complete(&ts->plat_data->secure_powerdown);

		input_info(true, &ts->spi->dev, "%s: secure touch disable\n", __func__);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
		sec_input_notify(&ts->himax_input_nb, NOTIFIER_SECURE_TOUCH_DISABLE, NULL);
#endif
	} else {
		input_err(true, &ts->spi->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->spi->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->spi->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->spi->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->plat_data->secure_pending_irqs));
	}

	complete(&ts->plat_data->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

static int secure_touch_init(struct himax_ts_data *ts)
{
	input_info(true, &ts->spi->dev, "%s\n", __func__);

	init_completion(&ts->plat_data->secure_interrupt);
	init_completion(&ts->plat_data->secure_powerdown);

	return 0;
}

void secure_touch_stop(struct himax_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->plat_data->secure_enabled)) {
		atomic_set(&ts->plat_data->secure_pending_irqs, -1);

		sysfs_notify(&ts->plat_data->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->plat_data->secure_powerdown);

		input_info(true, &ts->spi->dev, "%s: %d\n", __func__, stop);
	}
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
		secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);
static DEVICE_ATTR(secure_ownership, S_IRUGO, secure_ownership_show, NULL);
static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};
#endif

#if defined(HX_PLATFOME_DEFINE_KEY)
void himax_platform_key(void)
{
	I("Nothing to be done! Plz cancel it!\n");
}
#endif

void himax_vk_parser(struct device_node *dt,
						struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;

	node = of_parse_phandle(dt, "virtualkey", 0);

	if (node == NULL) {
		I(" DT-No vk info in DT\n");
		return;
	}
	while ((pp = of_get_next_child(node, pp)))
		cnt++;

	if (!cnt)
		return;

	vk = kcalloc(cnt, sizeof(struct himax_virtual_key), GFP_KERNEL);
	if (vk == NULL) {
		E("%s, vk init fail!\n", __func__);
		return;
	}
	pp = NULL;

	while ((pp = of_get_next_child(node, pp))) {
		if (of_property_read_u32(pp, "idx", &data) == 0)
			vk[i].index = data;

		if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
			vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
			vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
		} else {
			I(" range faile\n");
		}

		i++;
	}

	pdata->virtual_key = vk;

	for (i = 0; i < cnt; i++)
		I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i, pdata->virtual_key[i].index,
			 pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);

}

int himax_parse_dt(struct himax_ts_data *ts,
					struct himax_i2c_platform_data *pdata)
{
	struct device_node *dt = ts->dev->of_node;
	struct property *prop;
	u32 data = 0;
	int err, i, count;
	u32 coords[20];

	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_reset))
		I(" DT:gpio_rst value is not valid\n");

	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_3v3_en))
		I(" DT:gpio_3v3_en value is not valid\n");

	pdata->gpio_vendor_check = of_get_named_gpio(dt, "himax,vendor_check-gpio", 0);
	if (gpio_is_valid(pdata->gpio_vendor_check)) {
		err = of_property_read_u32(dt, "himax,vendor_check_enable_value", &data);
		if (err < 0) {
			E(" DT: failed to get vendor_check_enable_value, %d\n", err);
		} else {
			int vendor_check_value = gpio_get_value(pdata->gpio_vendor_check);

			if (vendor_check_value != data) {
				E(" DT: gpio_vendor_check is %d, himax ic is not connected\n",
					vendor_check_value);
				return -ENODEV;
			}
		}
	}

	I(" DT:gpio_rst=%d, gpio_3v3_en=%d, gpio_vendor_check=%d\n",
		pdata->gpio_reset, pdata->gpio_3v3_en, pdata->gpio_vendor_check);

	/* lcd regulator */
	if (of_property_read_string(dt, "himax,name_lcd_rst", &pdata->name_lcd_rst)) {
		input_err(true, ts->dev, "%s: Failed to get name_lcd_rst property\n", __func__);
		pdata->name_lcd_rst = NULL;
	}

	count = of_property_count_strings(dt, "himax,regulator_name");
	if (count < 0) {
		ts->regulator_count = 0;
		input_err(true, ts->dev, "%s: Failed to get regulator_name property\n", __func__);
	} else {
		ts->regulator_count = count;
		if (ts->regulator_count > HIMAX_TS_REGULATOR_MAX) {
			input_err(true, ts->dev, "%s: regulator_count %d is over then regulator array size(%d)\n",
					__func__, ts->regulator_count, HIMAX_TS_REGULATOR_MAX);
			ts->regulator_count = HIMAX_TS_REGULATOR_MAX;
		}
		input_info(true, ts->dev, "%s: try to get %d regulator\n", __func__, ts->regulator_count);
		err = of_property_read_string_array(dt, "himax,regulator_name",
				pdata->regulator_name, ts->regulator_count);
		if (err < 0) {
			input_err(true, ts->dev, "%s: Failed to get regulator_name property\n", __func__);
			return err;
		}
		input_info(true, ts->dev, "%s: success to get regulator, ret=%d\n", __func__, err);
	}

	if (of_property_read_u32_array(dt, "himax,display-coords", coords, 2)) {
		E(" %s:Fail to read display-coords\n", __func__);
		return -ENOENT;
	}
	pdata->screenWidth  = coords[0];
	pdata->screenHeight = coords[1];
	I(" DT-%s:display-coords = (%d, %d)\n", __func__, pdata->screenWidth, pdata->screenHeight);

#ifdef HX_FIX_TOUCH_INFO
	prop = of_find_property(dt, "himax,fix_touch_info", NULL);
	if (prop && prop->length) {
		if (of_property_read_u32_array(dt, "himax,fix_touch_info", coords, FIX_HX_MAX)) {
			E(" %s:Fail to read fix_touch_info\n", __func__);
			return -ENOENT;
		}

		ic_data->HX_RX_NUM = coords[FIX_HX_RX_NUM];
		ic_data->HX_TX_NUM = coords[FIX_HX_TX_NUM];
		ic_data->HX_BT_NUM = coords[FIX_HX_BT_NUM];
		ic_data->HX_X_RES = coords[FIX_HX_X_RES];
		ic_data->HX_Y_RES = coords[FIX_HX_Y_RES];
		ic_data->HX_MAX_PT = coords[FIX_HX_MAX_PT];
		ic_data->HX_XY_REVERSE = coords[FIX_HX_XY_REVERSE] ? true:false;
		ic_data->HX_INT_IS_EDGE = coords[FIX_HX_INT_IS_EDGE] ? true:false;

		I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d\n", __func__, ic_data->HX_RX_NUM, ic_data->HX_TX_NUM, ic_data->HX_MAX_PT);
		I("%s:HX_XY_REVERSE =%d,HX_Y_RES =%d,HX_X_RES=%d\n", __func__, ic_data->HX_XY_REVERSE, ic_data->HX_Y_RES, ic_data->HX_X_RES);
		I("%s:HX_INT_IS_EDGE =%d\n", __func__, ic_data->HX_INT_IS_EDGE);
	} else {
		I("%s DT: need to set fix_touch_info or not a zefoflash\n", __func__);
	}
#endif

	if (of_property_read_u32(dt, "himax,report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d\n", pdata->protocol_type);
	}

	prop = of_find_property(dt, "himax,one_frame_delay", NULL);
	if (prop && prop->length) {
		err = of_property_read_u32(dt, "himax,one_frame_delay", &data);
		if (err < 0) {
			E("%s:Unable to read himax,one_frame_delay property\n", __func__);
			pdata->one_frame_delay = 20;
		} else {
			pdata->one_frame_delay = data;
			I("%s DT:one_frame_delay=%d\n", __func__, pdata->one_frame_delay);
		}
	} else {
		pdata->one_frame_delay = 20;
		I("%s DT: set default one_frame_delay\n", __func__);
	}


	if (of_property_read_string(dt, "himax,project_name", &pdata->proj_name) < 0) {
		/* prevent from due to use strcmp with null pointer */
		pdata->proj_name = "HIMAX";
		D("parsing from dt FAIL!!!!!, use default project name = %s\n", pdata->proj_name);
	}

	prop = of_find_property(dt, "himax,notch-setting", NULL);
	if (prop) {
		ic_data->notch_sz = prop->length / sizeof(u32);
		if (ic_data->notch_sz > 0) {
			ic_data->notch_arr = kzalloc(sizeof(int) * ic_data->notch_sz, GFP_KERNEL);
			if (of_property_read_u32_array(dt, "himax,notch-setting", ic_data->notch_arr, ic_data->notch_sz) == 0) {
				for (i = 0; i < ic_data->notch_sz; i++)
					I(" DT-%s:himax,notch-setting val%d=%d\n", __func__, i, ic_data->notch_arr[i]);
			} else {
				E(" DT-%s:notch-setting size is wrong\n", __func__);
			}
		}
	}
	pdata->notify_tsp_esd = of_property_read_bool(dt, "himax,notify_tsp_esd");

	input_info(true, ts->dev, "%s : support: %s\n", __func__, pdata->notify_tsp_esd ? "ESD" : "");

	himax_vk_parser(dt, pdata);
	return 0;
}
EXPORT_SYMBOL(himax_parse_dt);

static ssize_t himax_spi_sync(struct himax_ts_data *ts, struct spi_message *message)
{
	int status;

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		E("%s: now IC status is plat_data->shutdown_called\n", __func__);
		return -EIO;
	}

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		E("%s: now IC status is OFF\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode()) {
		E("%s: now secure touch mode\n", __func__);
		return -EBUSY;
	}
#endif

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	status = spi_sync(ts->spi, message);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (status == 0) {
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static int himax_spi_read(uint8_t *command, uint8_t command_len, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	struct himax_ts_data *ts = private_ts;
	struct spi_message message;
	struct spi_transfer xfer[2];
	uint8_t *rbuff, *cbuff;
	int retry;
	int error;

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		E("%s: now IC status is plat_data->shutdown_called\n", __func__);
		return -EIO;
	}

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		E("%s: now IC status is OFF\n", __func__);
		return -EIO;
	}
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode()) {
		E("%s: now secure touch mode\n", __func__);
		return -EBUSY;
	}
#endif

	rbuff = kzalloc(sizeof(uint8_t) * length, GFP_KERNEL);
	if (!rbuff)
		return -ENOMEM;

	cbuff = kzalloc(sizeof(uint8_t) * command_len, GFP_KERNEL);
	if (!cbuff) {
		kfree(rbuff);
		return -ENOMEM;
	}

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	memcpy(cbuff, command, command_len);
	xfer[0].tx_buf = cbuff;
	xfer[0].len = command_len;
	xfer[0].cs_change = 0;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].rx_buf = rbuff;
	xfer[1].len = length;
	xfer[1].cs_change = 0;
	spi_message_add_tail(&xfer[1], &message);

	for (retry = 0; retry < toRetry; retry++) {
		if (ts->plat_data->gpio_spi_cs > 0)
			gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

		error = spi_sync(private_ts->spi, &message);

		if (ts->plat_data->gpio_spi_cs > 0)
			gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

		if (unlikely(error))
			E("SPI read error: %d\n", error);
		else
			break;
	}

	if (retry == toRetry) {
		E("%s: SPI read error retry over %d\n",
			__func__, toRetry);
		kfree(cbuff);
		kfree(rbuff);
		return -EIO;
	}
	memcpy(data, rbuff, length);

	kfree(cbuff);
	kfree(rbuff);

	return 0;
}

static int himax_spi_write(uint8_t *buf, uint32_t length)
{
	struct himax_ts_data *ts = private_ts;
	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= length,
			.cs_change	= 0,
	};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		E("%s: now IC status is plat_data->shutdown_called\n", __func__);
		return -EIO;
	}

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		E("%s: now IC status is OFF\n", __func__);
		return -EIO;
	}
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;
		
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode()) {
		E("%s: now secure touch mode\n", __func__);
		return -EBUSY;
	}
#endif

	return himax_spi_sync(private_ts, &m);

}

int himax_bus_read(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	int result = 0;
	uint8_t spi_format_buf[3];

	mutex_lock(&(private_ts->spi_lock));
	spi_format_buf[0] = 0xF3;
	spi_format_buf[1] = command;
	spi_format_buf[2] = 0x00;

	result = himax_spi_read(&spi_format_buf[0], 3, data, length, toRetry);
	mutex_unlock(&(private_ts->spi_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_read);

int himax_bus_write(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	uint8_t *spi_format_buf = gBuffer;
	int i = 0;
	int result = 0;

	mutex_lock(&(private_ts->spi_lock));
	spi_format_buf[0] = 0xF2;
	spi_format_buf[1] = command;

	for (i = 0; i < length; i++)
		spi_format_buf[i + 2] = data[i];

	result = himax_spi_write(spi_format_buf, length + 2);
	mutex_unlock(&(private_ts->spi_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_write);

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

int himax_bus_master_write(uint8_t *data, uint32_t length, uint8_t toRetry)
{
	uint8_t *buf;

	struct spi_transfer	t;
	struct spi_message	m;
	int result = 0;

	buf = kzalloc(length * sizeof(uint8_t), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&(private_ts->spi_lock));
	memcpy(buf, data, length);
	t.tx_buf = buf;
	t.len = length;
	t.cs_change = 0;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	result = himax_spi_sync(private_ts, &m);
	mutex_unlock(&(private_ts->spi_lock));

	kfree(buf);
	return result;
}
EXPORT_SYMBOL(himax_bus_master_write);

void himax_int_enable(int enable)
{
	if (enable == INT_ENABLE)
		sec_input_irq_enable(private_ts->plat_data);
	else if (enable == INT_DISABLE_NOSYNC)
		sec_input_irq_disable_nosync(private_ts->plat_data);
	else if (enable == INT_DISABLE_SYNC)
		sec_input_irq_disable(private_ts->plat_data);
	else
		I("%s: faulty enable value %d\n", __func__, enable);
}
EXPORT_SYMBOL(himax_int_enable);

#ifdef HX_RST_PIN_FUNC
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}
EXPORT_SYMBOL(himax_rst_gpio_set);
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int himax_regulator_configure(struct himax_i2c_platform_data *pdata)
{
	int retval;
	/* struct i2c_client *client = private_ts->client; */

	pdata->vcc_dig = regulator_get(private_ts->dev, "vdd");

	if (IS_ERR(pdata->vcc_dig)) {
		E("%s: Failed to get regulator vdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_dig);
		return retval;
	}

	pdata->vcc_ana = regulator_get(private_ts->dev, "avdd");

	if (IS_ERR(pdata->vcc_ana)) {
		E("%s: Failed to get regulator avdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_ana);
		regulator_put(pdata->vcc_dig);
		return retval;
	}

	return 0;
};

static void himax_regulator_deinit(struct himax_i2c_platform_data *pdata)
{
	I("%s: entered.\n", __func__);

	if (!IS_ERR(pdata->vcc_ana))
		regulator_put(pdata->vcc_ana);

	if (!IS_ERR(pdata->vcc_dig))
		regulator_put(pdata->vcc_dig);

	I("%s: regulator put, completed.\n", __func__);
};

static int himax_power_on(struct himax_i2c_platform_data *pdata, bool on)
{
	int retval;

	if (on) {
		retval = regulator_enable(pdata->vcc_dig);

		if (retval) {
			E("%s: Failed to enable regulator vdd\n",
			  __func__);
			return retval;
		}

		/*msleep(100);*/
		usleep_range(1000, 1001);
		retval = regulator_enable(pdata->vcc_ana);

		if (retval) {
			E("%s: Failed to enable regulator avdd\n",
			  __func__);
			regulator_disable(pdata->vcc_dig);
			return retval;
		}
	} else {
		regulator_disable(pdata->vcc_dig);
		regulator_disable(pdata->vcc_ana);
	}

	return 0;
}

int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error;
	/* struct i2c_client *client = private_ts->client; */

	error = himax_regulator_configure(pdata);

	if (error) {
		E("Failed to intialize hardware\n");
		goto err_regulator_not_on;
	}

#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_reset);
			goto err_regulator_on;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_req;
		}
	}

#endif
	error = himax_power_on(pdata, true);

	if (error) {
		E("Failed to power on hardware\n");
		goto err_power_on;
	}

	/*msleep(20);*/
	usleep_range(2000, 2001);
#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_set_gpio_reset;
		}
		usleep_range(5000, 5000);
	}

#endif
	return 0;
err_set_gpio_reset:
	himax_power_on(pdata, false);
err_power_on:
#ifdef HX_RST_PIN_FUNC
err_gpio_reset_req:
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

err_regulator_on:
#endif
	himax_regulator_deinit(pdata);
err_regulator_not_on:
	return error;
}

#else
int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error = 0;
	/* struct i2c_client *client = private_ts->client; */
#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");

		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			goto err_gpio_reset_req;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_dir;
		}
	}

#endif

	if (gpio_is_valid(private_ts->plat_data->gpio_spi_cs)) {
		error = gpio_direction_output(private_ts->plat_data->gpio_spi_cs, 1);
		if (error) {
			E("unable to set direction for gpio [%d]\n", private_ts->plat_data->gpio_spi_cs);
			goto err_gpio_cs_dir;
		}
		E("%s: gpio_spi_cs %d\n", __func__, private_ts->plat_data->gpio_spi_cs);
	}

#ifdef HX_PON_PIN_SUPPORT
	if (gpio_is_valid(pdata->gpio_pon)) {
		error = gpio_request(pdata->gpio_pon, "hmx_pon_gpio");

		if (error) {
			E("unable to request scl gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_req;
		}

		error = gpio_direction_output(pdata->gpio_pon, 0);

		I("gpio_pon LOW [%d]\n", pdata->gpio_pon);

		if (error) {
			E("unable to set direction for pon gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_dir;
		}
	}
#endif

	if (pdata->gpio_3v3_en >= 0) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");

		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			goto err_gpio_3v3_req;
		}

		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en set 1 get pin = %d\n", gpio_get_value(pdata->gpio_3v3_en));
	}

#ifdef HX_PON_PIN_SUPPORT
	msleep(20);
#else
	usleep_range(2000, 2001);
#endif

#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_set_high;
		}
		usleep_range(5000, 5000);
	}
#endif

#ifdef HX_PON_PIN_SUPPORT
	msleep(800);

	if (gpio_is_valid(pdata->gpio_pon)) {

		error = gpio_direction_output(pdata->gpio_pon, 1);

		I("gpio_pon HIGH [%d]\n", pdata->gpio_pon);

		if (error) {
			E("gpio_pon unable to set direction for gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_set_high;
		}
	}
#endif
	return error;

#ifdef HX_PON_PIN_SUPPORT
err_gpio_pon_set_high:
#endif
#ifdef HX_RST_PIN_FUNC
err_gpio_reset_set_high:
#endif
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);
err_gpio_3v3_req:
#ifdef HX_PON_PIN_SUPPORT
err_gpio_pon_dir:
	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
err_gpio_pon_req:
#endif
err_gpio_cs_dir:
#ifdef HX_RST_PIN_FUNC
err_gpio_reset_dir:
	if (pdata->gpio_reset >= 0)
		gpio_free(pdata->gpio_reset);
err_gpio_reset_req:
#endif
	return error;
}

#endif

void himax_gpio_power_deconfig(struct himax_i2c_platform_data *pdata)
{
#ifdef HX_RST_PIN_FUNC
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
#endif

#if defined(CONFIG_HMX_DB)
	himax_power_on(pdata, false);
	himax_regulator_deinit(pdata);
#else
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);

#ifdef HX_PON_PIN_SUPPORT
	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
#endif

#endif
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	struct himax_ts_data *ts = (struct himax_ts_data *)ptr;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (himax_secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->plat_data->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, ts->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif
	himax_ts_isr_func(ts);

	return IRQ_HANDLED;
}

int himax_int_register_trigger(void)
{
	int ret = 0;
	struct himax_ts_data *ts = private_ts;

	if (ic_data->HX_INT_IS_EDGE) {
		I("%s edge triiger falling\n", __func__);
		ret = devm_request_threaded_irq(ts->dev, ts->plat_data->irq, NULL, himax_ts_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	} else {
		I("%s level trigger low\n", __func__);
		ret = devm_request_threaded_irq(ts->dev, ts->plat_data->irq, NULL, himax_ts_thread,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	}

	return ret;
}

int himax_int_en_set(void)
{
	int ret = NO_ERR;

	ret = himax_int_register_trigger();
	return ret;
}

int himax_ts_register_interrupt(void)
{
	int ret = 0;

	ret = himax_int_register_trigger();
	if (ret == 0) {
		I("%s: irq enabled at gpio: %d\n", __func__, private_ts->plat_data->irq);
#ifdef HX_SMART_WAKEUP
		irq_set_irq_wake(private_ts->plat_data->irq, 1);
#endif
	}

	return ret;
}

int himax_ts_unregister_interrupt(void)
{
	I("%s: entered.\n", __func__);

	/* Work functon */
#ifdef HX_SMART_WAKEUP
	irq_set_irq_wake(private_ts->plat_data->irq, 0);
#endif
	I("%s: irq disabled at gpio: %d\n", __func__, private_ts->plat_data->irq);

	return 0;
}

static int himax_common_late_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	if (!ts->initialized)
		return -ECANCELED;
#endif
	himax_chip_common_late_suspend(ts);
	return 0;
}

static int himax_common_early_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	if (!ts->initialized)
		return -ECANCELED;
#endif
	himax_chip_common_early_suspend(ts);
	return 0;
}

#ifndef HX_CONTAINER_SPEED_UP
static int himax_common_late_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	/*
	 *	wait until device resume for TDDI
	 *	TDDI: Touch and display Driver IC
	 */
	if (!ts->initialized) {
		if (himax_chip_common_init())
			return -ECANCELED;
	}
#endif
	himax_chip_common_late_resume(ts);
	return 0;
}
static int himax_common_early_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	/*
	 *	wait until device resume for TDDI
	 *	TDDI: Touch and display Driver IC
	 */
	if (!ts->initialized) {
		if (himax_chip_common_init())
			return -ECANCELED;
	}
#endif
	himax_chip_common_early_resume(ts);
	return 0;
}
#endif

static int himax_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct himax_ts_data *ts = container_of(this, struct himax_ts_data, reboot_notifier);

	I("%s %s: enter\n", HIMAX_LOG_TAG, __func__);

	himax_chip_common_early_suspend(ts);

	I("%s %s: exit\n", HIMAX_LOG_TAG, __func__);

	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static void himax_input_notify_work(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, himax_input_notify_work.work);

	switch (ts->input_notify) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		himax_set_ap_change_mode(SPEN_MODE, 1);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		himax_set_ap_change_mode(SPEN_MODE, 0);
		break;
	default:
		break;
	}
}

static int himax_input_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct himax_ts_data *ts = container_of(n, struct himax_ts_data, himax_input_nb);

	if (!ts)
		return -ENODEV;

	switch (data) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		cancel_delayed_work(&ts->himax_input_notify_work);
		ts->input_notify = NOTIFIER_WACOM_PEN_HOVER_IN;
		schedule_work(&ts->himax_input_notify_work.work);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		cancel_delayed_work(&ts->himax_input_notify_work);
		ts->input_notify = NOTIFIER_WACOM_PEN_HOVER_OUT;
		schedule_work(&ts->himax_input_notify_work.work);
		break;
	default:
		break;
	}

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
struct himax_ts_data *stui_ts;
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

static int himax_stui_tsp_enter(void)
{
	int ret = 0;
	struct spi_device *spi = stui_ts->spi;

	input_info(true, &spi->dev, ">> %s\n", __func__);

	himax_int_enable(INT_DISABLE_NOSYNC);

	ret = stui_spi_lock(spi->master);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
		himax_int_enable(INT_ENABLE);
		return -1;
	}

	return 0;
}

static int himax_stui_tsp_exit(void)
{
	int ret = 0;
	struct spi_device *spi = stui_ts->spi;

	input_info(true, &spi->dev, ">> %s\n", __func__);

	ret = stui_spi_unlock(spi->master);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);
	}

	himax_int_enable(INT_ENABLE);

	return ret;
}

static int himax_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_HIMAX;
}
#endif

int himax_chip_common_probe(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	int ret = 0;

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	static int deferred_flag;

	if (!deferred_flag) {
		deferred_flag = 1;
		input_info(true, &spi->dev, "deferred_flag boot %s\n", __func__);
		return -EPROBE_DEFER;
	}
#endif

	input_info(true, &spi->dev, "%s:Enter\n", __func__);

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		dev_err(&spi->dev,
				"%s: Full duplex not supported by host\n", __func__);
		return -EIO;
	}

	gBuffer = devm_kzalloc(&spi->dev, sizeof(uint8_t) * (HX_MAX_WRITE_SZ + 6), GFP_KERNEL);
	if (gBuffer == NULL) {
		KE("%s: allocate gBuffer failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_gbuffer_failed;
	}

	ts = devm_kzalloc(&spi->dev, sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		KE("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	
	private_ts = ts;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	spi->chip_select = 0;

	ts->spi = spi;
	mutex_init(&(ts->spi_lock));
	ts->dev = &spi->dev;
	dev_set_drvdata(&spi->dev, ts);
	spi_set_drvdata(spi, ts);

	ts->plat_data = devm_kzalloc(&spi->dev, sizeof(struct sec_ts_plat_data), GFP_KERNEL);
	if (!ts->plat_data) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	ts->plat_data->dev = &ts->spi->dev;
	spi->dev.platform_data = ts->plat_data;
	ret = sec_input_parse_dt(ts->dev);
	if (ret < 0) {
		input_err(true, ts->dev, "%s: failed to parse dt\n", __func__);
		goto err_alloc_data_failed;
	}

	mutex_init(&ts->plat_data->enable_mutex);

#if defined(HX_USB_DETECT_GLOBAL)
	if (ts->plat_data->support_vbus_notifier)
		ts->plat_data->set_charger_mode = himax_cable_detect_func;
#endif

	ts->initialized = false;
	ret = himax_chip_common_init();
	if (ret < 0)
		goto err_chip_common_init;

	ts->pdata->early_suspend = himax_common_early_suspend;
	ts->pdata->late_suspend = himax_common_late_suspend;
	ts->pdata->early_resume = himax_common_early_resume;
	ts->pdata->late_resume = himax_common_late_resume;

	ts->reboot_notifier.notifier_call = himax_reboot_notifier;
	register_reboot_notifier(&ts->reboot_notifier);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->himax_input_nb, himax_input_notify_call, 1);
	INIT_DELAYED_WORK(&ts->himax_input_notify_work, himax_input_notify_work);
#endif

#if defined(HX_USB_DETECT_GLOBAL)
	sec_input_register_vbus_notifier(ts->dev);
#endif

#if SEC_LPWG_DUMP
	himax_lpwg_dump_buf_init();
#endif
	spi->irq = gpio_to_irq(ts->plat_data->irq_gpio);
	ts->plat_data->irq = spi->irq;
	ts->plat_data->bus_master = &ts->spi->controller->dev;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (sysfs_create_group(&ts->plat_data->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &spi->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts, ts->dev, 1, &ts->input_dev->dev.kobj);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	ret = sec_trusted_touch_init(&ts->spi->dev);
	if (ret < 0) 
		input_err(true, &ts->spi->dev, "%s: Failed to init trusted touch\n", __func__);
#endif
#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_ts = ts;
	stui_tsp_init(himax_stui_tsp_enter, himax_stui_tsp_exit, himax_stui_tsp_type);
	input_info(true, stui_ts->dev, "secure touch support\n");
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	atomic_set(&ts->plat_data->enabled, 1);
#endif

	return 0;

err_chip_common_init:
	mutex_destroy(&ts->plat_data->enable_mutex);
err_alloc_data_failed:
err_alloc_gbuffer_failed:
	return ret;
}

int himax_chip_common_dev_remove(struct spi_device *spi)
{
	struct himax_ts_data *ts = spi_get_drvdata(spi);

#if defined(HX_USB_DETECT_GLOBAL)
	sec_input_unregister_vbus_notifier(ts->dev);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_unregister_notify(&ts->himax_input_nb);
	cancel_delayed_work_sync(&ts->himax_input_notify_work);
#endif

	himax_pinctrl_configure(ts, false);
	msleep(ts->pdata->one_frame_delay);
#ifdef HX_RST_PIN_FUNC
		if (gpio_get_value(ts->pdata->gpio_reset)) {
			gpio_set_value(ts->pdata->gpio_reset, 0);
			I("%s make TP_RESET low\n", __func__);
		}
#endif
	if (g_hx_chip_inited)
		himax_chip_common_deinit();

	mutex_destroy(&ts->plat_data->enable_mutex);

	ts->spi = NULL;
	/* spin_unlock_irq(&ts->spi_lock); */
	spi_set_drvdata(spi, NULL);

	KI("%s: completed.\n", __func__);

	return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
void himax_chip_common_remove(struct spi_device *spi)
{
	himax_chip_common_dev_remove(spi);
}
#else
int himax_chip_common_remove(struct spi_device *spi)
{
	himax_chip_common_dev_remove(spi);
	return 0;
}
#endif
static int himax_pm_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

static int himax_pm_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	complete_all(&ts->plat_data->resume_done);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops himax_common_pm_ops = {
	.suspend = himax_pm_suspend,
	.resume  = himax_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id himax_match_table[] = {
	{.compatible = "himax,hxcommon" },
	{},
};
#else
#define himax_match_table NULL
#endif

static struct spi_driver himax_common_driver = {
	.driver = {
		.name =		HIMAX_common_NAME,
		.owner =	THIS_MODULE,
		.of_match_table = himax_match_table,
#if IS_ENABLED(CONFIG_PM)
		.pm = &himax_common_pm_ops,
#endif
	},
	.probe =	himax_chip_common_probe,
	.remove =	himax_chip_common_remove,
};

static int __init himax_common_init(void)
{
	KI("Himax common touch panel driver init\n");
	D("Himax check double loading\n");
	if (g_mmi_refcnt++ > 0) {
		KI("Himax driver has been loaded! ignoring....\n");
		return 0;
	}
	spi_register_driver(&himax_common_driver);
	input_log_fix();

	return 0;
}

static void __exit himax_common_exit(void)
{
#if IS_ENABLED(CONFIG_QGKI)
	if (spi) {
		spi_unregister_device(spi);
		spi = NULL;
	}
	spi_unregister_driver(&himax_common_driver);
#endif
}

module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");
