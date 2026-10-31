// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * Revision: 108741
 * Date: 2022-11-21 10:31:27 +0800
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include "nt36xxx.h"

#if NVT_TOUCH_ESD_PROTECT
//#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

struct nvt_ts_data *ts;

#if IS_ENABLED(CONFIG_MTK_SPI)
const struct mt_chip_conf spi_ctrdata = {
	.setuptime = 25,
	.holdtime = 25,
	.high_time = 5,	/* 10MHz (SPI_SPEED=100M / (high_time+low_time(10ns)))*/
	.low_time = 5,
	.cs_idletime = 2,
	.ulthgh_thrsh = 0,
	.cpol = 0,
	.cpha = 0,
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.tx_endian = 0,
	.rx_endian = 0,
	.com_mod = DMA_TRANSFER,
	.pause = 0,
	.finish_intr = 1,
	.deassert = 0,
	.ulthigh = 0,
	.tckdly = 0,
};
#endif

#if IS_ENABLED(CONFIG_SPI_MT65XX)
#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
const struct mtk_chip_config spi_ctrdata = {
	.sample_sel = 0,

	.cs_setuptime = 25,
	.cs_holdtime = 0,
	.cs_idletime = 0,
	.tick_delay = 0,
};
#else
const struct mtk_chip_config spi_ctrdata = {
	.rx_mlsb = 1,
	.tx_mlsb = 1,
	.sample_sel = 0,

	.cs_setuptime = 25,
	.cs_holdtime = 0,
	.cs_idletime = 0,
	.deassert_mode = false,
	.tick_delay = 0,
};
#endif
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
static irqreturn_t nvt_ts_work_func(int irq, void *data);
irqreturn_t secure_filter_interrupt(struct nvt_ts_data *ts)
{
	mutex_lock(&ts->secure_lock);
	if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		} else {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
					__func__, (int)atomic_read(&ts->plat_data->secure_pending_irqs));
		}

		mutex_unlock(&ts->secure_lock);
		return IRQ_HANDLED;
	}

	mutex_unlock(&ts->secure_lock);
	return IRQ_NONE;
}

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */
ssize_t secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->plat_data->secure_enabled));
}

ssize_t secure_touch_enable_store(struct device *dev,
		struct device_attribute *addr, const char *buf, size_t count)
{
	int ret;
	unsigned long data;

	if (count > 2) {
		input_err(true, &ts->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	ret = kstrtoul(buf, 10, &data);
	if (ret != 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
				__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (sec_check_secure_trusted_mode_status(ts->plat_data))
			return -EBUSY;

		sec_delay(200);

		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Release All Finger */
		sec_input_release_all_finger(&ts->client->dev);

		if (pm_runtime_get_sync(ts->client->controller->dev.parent) < 0) {
			enable_irq(ts->client->irq);
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

		reinit_completion(&ts->plat_data->secure_powerdown);
		reinit_completion(&ts->plat_data->secure_interrupt);

		atomic_set(&ts->plat_data->secure_enabled, 1);
		atomic_set(&ts->plat_data->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);

	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_delay(200);

		pm_runtime_put_sync(ts->client->controller->dev.parent);
		atomic_set(&ts->plat_data->secure_enabled, 0);

		sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		sec_delay(10);

		nvt_ts_work_func(ts->client->irq, ts);
		complete(&ts->plat_data->secure_interrupt);
		complete_all(&ts->plat_data->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%ld\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

ssize_t secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val = 0;

	mutex_lock(&ts->secure_lock);
	if (atomic_read(&ts->plat_data->secure_enabled) == SECURE_TOUCH_DISABLE) {
		mutex_unlock(&ts->secure_lock);
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, -1, 0) == -1) {
		mutex_unlock(&ts->secure_lock);
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->plat_data->secure_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->plat_data->secure_pending_irqs));
	}

	mutex_unlock(&ts->secure_lock);
	complete(&ts->plat_data->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

int secure_touch_init(struct nvt_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->plat_data->secure_interrupt);
	init_completion(&ts->plat_data->secure_powerdown);

	return 0;
}

void secure_touch_stop(struct nvt_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->plat_data->secure_enabled)) {
		atomic_set(&ts->plat_data->secure_pending_irqs, -1);

		sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");

		if (stop)
			wait_for_completion_interruptible(&ts->plat_data->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}

static DEVICE_ATTR_RW(secure_touch_enable);
static DEVICE_ATTR_RO(secure_touch);
static DEVICE_ATTR_RO(secure_ownership);
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

/*
 * Description:
 *	Novatek touchscreen irq enable/disable function.
 *
 * return:
 *	n.a.
 */
void nvt_irq_enable(bool enable)
{
	struct irq_desc *desc = irq_to_desc(ts->client->irq);

	mutex_lock(&ts->irq_lock);
	if (enable) {
		while (desc->depth > 0)
			enable_irq(ts->client->irq);
	} else {
		disable_irq(ts->client->irq);
	}
	mutex_unlock(&ts->irq_lock);

	input_info(true, &ts->client->dev, "%s: enable=%d, depth=%d\n", __func__, enable, desc->depth);
}

void nvt_irq_disable_nosync(void)
{
	struct irq_desc *desc = irq_to_desc(ts->client->irq);

	mutex_lock(&ts->irq_lock);
	disable_irq_nosync(ts->client->irq);
	mutex_unlock(&ts->irq_lock);

	input_info(true, &ts->client->dev, "%s: depth=%d\n", __func__, desc->depth);
}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

static int nvt_stui_tsp_enter(void)
{
	int ret = 0;

	input_info(true, &ts->client->dev, ">> %s\n", __func__);

	nvt_irq_enable(false);

	ret = stui_spi_lock(ts->client->master);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
		nvt_irq_enable(true);
		return -1;
	}

	return 0;
}

static int nvt_stui_tsp_exit(void)
{
	int ret = 0;

	input_info(true, &ts->client->dev, ">> %s\n", __func__);

	ret = stui_spi_unlock(ts->client->master);
	if (ret < 0)
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);

	nvt_irq_enable(true);

	return ret;
}

static int nvt_stui_tsp_type(void)
{
	input_info(true, &ts->client->dev, ">> %s\n", __func__);

	return STUI_TSP_TYPE_NOVATEK;
}
#endif

/*
 * Description:
 *	Novatek touchscreen spi read/write core function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 */
static inline int32_t spi_read_write(struct spi_device *client, uint8_t *buf, size_t len, NVT_SPI_RW rw)
{
	struct spi_message m;
	struct spi_transfer t = {
		.len    = len,
	};
	int ret;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif
	if (sec_check_secure_trusted_mode_status(ts->plat_data))
		return -EBUSY;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	memset(ts->xbuf, 0, len + DUMMY_BYTES);
	memcpy(ts->xbuf, buf, len);

	switch (rw) {
	case NVTREAD:
		t.tx_buf = ts->xbuf;
		t.rx_buf = ts->rbuf;
		t.len = (len + DUMMY_BYTES);
		break;
	case NVTWRITE:
		t.tx_buf = ts->xbuf;
		break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 0);

	ret = spi_sync(client, &m);

	if (ts->plat_data->gpio_spi_cs > 0)
		gpio_direction_output(ts->plat_data->gpio_spi_cs, 1);

	if (rw == NVTREAD && ts->debug_flag & SEC_TS_DEBUG_PRINT_READ_CMD) {
		int buff_len = (len * 2 + DUMMY_BYTES) * 4;
		char *buff;
		char tbuff[10];
		int ii;

		buff = vmalloc(buff_len);
		if (!buff)
			return ret;

		snprintf(buff, len, "W: ");
		for (ii = 0; ii < len; ii++) {
			memset(tbuff, 0x00, sizeof(tbuff));
			snprintf(tbuff, sizeof(tbuff), "%02X ", ts->xbuf[ii]);
			strlcat(buff, tbuff, buff_len);
		}

		memset(tbuff, 0x00, 10);
		snprintf(tbuff, 10, "R: ");
		strlcat(buff, tbuff, buff_len);

		for (ii = 0; ii < len + DUMMY_BYTES; ii++) {
			memset(tbuff, 0x00, sizeof(tbuff));
			snprintf(tbuff, sizeof(tbuff), "%02X ", ts->rbuf[ii]);
			strlcat(buff, tbuff, buff_len);
		}

		input_err(true, &ts->client->dev, "%s: %s\n", __func__, buff);
		vfree(buff);
	} else if (rw == NVTWRITE && ts->debug_flag & SEC_TS_DEBUG_PRINT_WRITE_CMD) {
		int buff_len = len * 4;
		char *buff;
		char tbuff[10];
		int ii;

		buff = vmalloc(buff_len);
		if (!buff)
			return ret;

		snprintf(buff, len, "W: ");
		for (ii = 0; ii < len; ii++) {
			memset(tbuff, 0x00, sizeof(tbuff));
			snprintf(tbuff, sizeof(tbuff), "%02X ", ts->rbuf[ii]);
			strlcat(buff, tbuff, buff_len);
		}

		input_err(true, &ts->client->dev, "%s: %s\n", __func__, buff);
		vfree(buff);
	}

	return ret;
}

/*
 * Description:
 *	Novatek touchscreen spi read function.
 *
 * return:
 *	Executive outcomes. 2---succeed. -5---I/O error
 */
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return -EIO;
	}

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < NVT_SPI_RETRY_COUNT) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0)
			break;
		retries++;

		if (atomic_read(&ts->plat_data->shutdown_called)) {
			input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}

		if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
			input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}

		if (!nvt_ts_lcd_power_check()) {
			input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}
	}

	if (unlikely(retries == NVT_SPI_RETRY_COUNT)) {
		input_err(true, &client->dev, "read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (ts->rbuf+2), (len-1));
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*
 * Description:
 *	Novatek touchscreen spi write function.
 *
 * return:
 *	Executive outcomes. 1---succeed. -5---I/O error
 */
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode())
		return -EBUSY;
#endif

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return -EIO;
	}

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < NVT_SPI_RETRY_COUNT) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)
			break;
		retries++;

		if (atomic_read(&ts->plat_data->shutdown_called)) {
			input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}

		if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
			input_err(true, &client->dev, "%s: POWER_STATUS : OFF!\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}

		if (!nvt_ts_lcd_power_check()) {
			input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
			mutex_unlock(&ts->xbuf_lock);
			return -EIO;
		}
	}

	if (unlikely(retries == NVT_SPI_RETRY_COUNT)) {
		input_err(true, &client->dev, "write error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*
 * Description:
 *	Novatek touchscreen set index/page/addr address.
 *
 * return:
 *	Executive outcomes. 0---succeed. -5---access fail.
 */
int32_t nvt_set_page(uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return CTP_SPI_WRITE(ts->client, buf, 3);
}

/*
 * Description:
 *	Novatek touchscreen write data to specify address.
 *
 * return:
 *	Executive outcomes. 0---succeed. -5---access fail.
 */
int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = {0};

	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret) {
		input_err(true, &ts->client->dev, "set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = data;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret) {
		input_err(true, &ts->client->dev, "write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

/*
 * Description:
 *	Novatek touchscreen read value to specific register.
 *
 * return:
 *	Executive outcomes. 0---succeed. -5---access fail.
 */
int32_t nvt_read_reg(nvt_ts_reg_t reg, uint8_t *val)
{
	int32_t ret = 0;
	uint32_t addr = 0;
	uint8_t mask = 0;
	uint8_t shift = 0;
	uint8_t buf[8] = {0};
	uint8_t temp = 0;

	addr = reg.addr;
	mask = reg.mask;
	/* get shift */
	temp = reg.mask;
	shift = 0;
	while (1) {
		if ((temp >> shift) & 0x01)
			break;
		if (shift == 8) {
			input_err(true, &ts->client->dev, "mask all bits zero!\n");
			ret = -1;
			break;
		}
		shift++;
	}
	/* read the byte of the register is in */
	nvt_set_page(addr);
	buf[0] = addr & 0xFF;
	buf[1] = 0x00;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "CTP_SPI_READ failed!(%d)\n", ret);
		goto nvt_read_register_exit;
	}
	/* get register's value in its field of the byte */
	*val = (buf[1] & mask) >> shift;

nvt_read_register_exit:
	return ret;
}

int nvt_ts_cascade_ics_read_NT3652x(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len)
{
	u8 buf[10];
	int i;
	int retry = 20, ret;

	//---set xdata index to SPI DMA Registers---
	ret = nvt_set_page(ts->mmap->SPI_DMA_LENGTH);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA reg)\n", __func__);
		return ret;
	}

	//---set length---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LENGTH & 0xFF);
	buf[1] = (u8)((len - 1) & 0xFF);
	buf[2] = (u8)(((len - 1) >> 8) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set length\n", __func__);
		return ret;
	}

	//---set remote (ICS) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_REM_ADDR & 0xFF);
	buf[1] = (u8)(address & 0xFF);
	buf[2] = (u8)((address >> 8) & 0xFF);
	buf[3] = (u8)((address >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set remote addr\n", __func__);
		return ret;
	}

	//---set local (ICM) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LOC_ADDR & 0xFF);
	buf[1] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	buf[2] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 8) & 0xFF);
	buf[3] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set local addr\n", __func__);
		return ret;
	}

	//---trigger SPI_TX_READ_TRIGGER---
	buf[0] = (u8)(ts->mmap->SPI_TX_READ_TRIGGER & 0xFF);
	buf[1] = 0x01;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to trigger SPI_TX_READ_TRIGGER\n", __func__);
		return ret;
	}

	//---wait SPI DMA done---
	for (i = 0; i < retry; i++) {
		buf[0] = (u8)(ts->mmap->SPI_DMA_TX_INFO & 0xFF);
		buf[1] = 0xFF;
		ret = CTP_SPI_READ(ts->client, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to read SPI DMA done\n", __func__);
			return ret;
		}

		if (buf[1] == 0x00)
			break;

		msleep(20);
	}
	if (i == retry) {
		input_err(true, &ts->client->dev, "%s: write over retry limit\n", __func__);
		return -EIO;
	}

	//---read data from local (ICM) memory---
	ret = nvt_set_page(ts->mmap->LOC_RW_BUF_ADDR);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set page(ICM)\n", __func__);
		return ret;
	}

	data[0] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	ret = CTP_SPI_READ(ts->client, data, len);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read data\n", __func__);
		return ret;
	}

	return 0;
}

int nvt_ts_cascade_ics_read_NT3653x(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len)
{
	u8 buf[10];
	int i;
	int retry = 20, ret;

	//---set xdata index to SPI DMA Registers---
	ret = nvt_set_page(ts->mmap->SPI_DMA_LENGTH);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA reg)\n", __func__);
		return ret;
	}

	//---set length---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LENGTH & 0xFF);
	buf[1] = (u8)((len - 1) & 0xFF);
	buf[2] = (u8)(((len - 1) >> 8) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set length\n", __func__);
		return ret;
	}

	//---set xdata index to SPI DMA Registers---
	ret = nvt_set_page(ts->mmap->SPI_DMA_REM_ADDR);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA target reg)\n", __func__);
		return ret;
	}

	//---set remote (ICS) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_REM_ADDR & 0xFF);
	buf[1] = (u8)(address & 0xFF);
	buf[2] = (u8)((address >> 8) & 0xFF);
	buf[3] = (u8)((address >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set remote addr\n", __func__);
		return ret;
	}

	//---set local (ICM) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LOC_ADDR & 0xFF);
	buf[1] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	buf[2] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 8) & 0xFF);
	buf[3] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set local addr\n", __func__);
		return ret;
	}

	//---set xdata index to spi dma cmd address---
	nvt_set_page(ts->mmap->SPI_TX_CMD);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA cmd reg)\n", __func__);
		return ret;
	}

	//---Trigger SPI_TX_CMD[7:0] = 0xA6 (0xA6:R2X, 0xA3:R1X)---
	buf[0] = (u8)(ts->mmap->SPI_TX_CMD & 0xFF);
	buf[1] = 0xA6;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to trigger SPI_TX_CMD\n", __func__);
		return ret;
	}

	//---wait SPI DMA done---
	for (i = 0; i < retry; i++) {
		buf[0] = (u8)(ts->mmap->SPI_TX_CMD & 0xFF);
		buf[1] = 0xFF;
		ret = CTP_SPI_READ(ts->client, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to read SPI DMA done\n", __func__);
			return ret;
		}

		if (buf[1] == 0x00)
			break;

		msleep(20);
	}
	if (i == retry) {
		input_err(true, &ts->client->dev, "%s: read over retry limit\n", __func__);
		return -EIO;
	}

	//---read data from local (ICM) memory---
	ret = nvt_set_page(ts->mmap->LOC_RW_BUF_ADDR);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set page(ICM)\n", __func__);
		return ret;
	}

	data[0] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	ret = CTP_SPI_READ(ts->client, data, len);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read data\n", __func__);
		return ret;
	}

	return 0;
}

int nvt_ts_cascade_ics_read(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len)
{
	int ret;

	if (ts->mmap->SPI_TX_READ_TRIGGER)
		ret = nvt_ts_cascade_ics_read_NT3652x(ts, address, data, len);
	else
		ret = nvt_ts_cascade_ics_read_NT3653x(ts, address, data, len);

	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to read slave IC\n", __func__);
		return ret;
	}

	return 0;
}

int nvt_ts_cascade_ics_write_NT3652x(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len)
{
	u8 buf[10];
	int i;
	int retry = 20, ret;

	//---set xdata index to local (ICM) temp memory---
	ret = nvt_set_page(ts->mmap->LOC_RW_BUF_ADDR);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set xdata index(ICM)\n", __func__);
		return ret;
	}

	//---write data to local (ICM) memory---
	data[0] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, data, len);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write data\n", __func__);
		return ret;
	}

	//---set xdata index to SPI DMA Registers---
	ret = nvt_set_page(ts->mmap->SPI_DMA_LENGTH);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA reg)\n", __func__);
		return ret;
	}

	//---set length---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LENGTH & 0xFF);
	buf[1] = (u8)((len - 1) & 0xFF);
	buf[2] = (u8)(((len - 1) >> 8) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set length\n", __func__);
		return ret;
	}

	//---set remote (ICS) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_REM_ADDR & 0xFF);
	buf[1] = (u8)(address & 0xFF);
	buf[2] = (u8)((address >> 8) & 0xFF);
	buf[3] = (u8)((address >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set remote addr\n", __func__);
		return ret;
	}

	//---set local (ICM) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LOC_ADDR & 0xFF);
	buf[1] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	buf[2] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 8) & 0xFF);
	buf[3] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set local addr\n", __func__);
		return ret;
	}

	//---trigger SPI_TX_WRITE_TRIGGER---
	buf[0] = (u8)(ts->mmap->SPI_TX_WRITE_TRIGGER & 0xFF);
	buf[1] = 0x01;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to trigger SPI_TX_WRITE_TRIGGER\n", __func__);
		return ret;
	}

	//---wait SPI DMA done---
	for (i = 0; i < retry; i++) {
		buf[0] = (u8)(ts->mmap->SPI_DMA_TX_INFO & 0xFF);
		buf[1] = 0xFF;
		ret = CTP_SPI_READ(ts->client, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to read SPI DMA done\n", __func__);
			return ret;
		}

		if (buf[1] == 0x00)
			break;

		msleep(20);
	}
	if (i == retry) {
		input_err(true, &ts->client->dev, "%s: write over retry limit\n", __func__);
		return -EIO;
	}

	return 0;
}

int nvt_ts_cascade_ics_write_NT3653x(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len)
{
	u8 buf[10];
	int i;
	int retry = 20, ret;

	//---set xdata index to local (ICM) temp memory---
	ret = nvt_set_page(ts->mmap->LOC_RW_BUF_ADDR);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set xdata index(ICM)\n", __func__);
		return ret;
	}

	//---write data to local (ICM) memory---
	data[0] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, data, len);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write data\n", __func__);
		return ret;
	}

	//---set xdata index to SPI DMA Registers---
	ret = nvt_set_page(ts->mmap->SPI_DMA_LENGTH);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA len reg)\n", __func__);
		return ret;
	}

	//---set length---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LENGTH & 0xFF);
	buf[1] = (u8)((len - 1) & 0xFF);
	buf[2] = (u8)(((len - 1) >> 8) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set length\n", __func__);
		return ret;
	}

	//---set xdata index to SPI DMA Registers---
	ret = nvt_set_page(ts->mmap->SPI_DMA_REM_ADDR);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA target reg)\n", __func__);
		return ret;
	}

	//---set remote (ICS) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_REM_ADDR & 0xFF);
	buf[1] = (u8)(address & 0xFF);
	buf[2] = (u8)((address >> 8) & 0xFF);
	buf[3] = (u8)((address >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set remote addr\n", __func__);
		return ret;
	}

	//---set local (ICM) address---
	buf[0] = (u8)(ts->mmap->SPI_DMA_LOC_ADDR & 0xFF);
	buf[1] = (u8)(ts->mmap->LOC_RW_BUF_ADDR & 0xFF);
	buf[2] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 8) & 0xFF);
	buf[3] = (u8)((ts->mmap->LOC_RW_BUF_ADDR >> 16) & 0xFF);
	ret = CTP_SPI_WRITE(ts->client, buf, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to set local addr\n", __func__);
		return ret;
	}

	//---set xdata index to spi dma cmd address---
	nvt_set_page(ts->mmap->SPI_TX_CMD);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to set xdata index(SPI DMA cmd reg)\n", __func__);
		return ret;
	}

	//---Trigger SPI_TX_CMD[7:0] = 0x56 (0x56:W2X, 0x53:W1X)---
	buf[0] = (u8)(ts->mmap->SPI_TX_CMD & 0xFF);
	buf[1] = 0x56;
	ret = CTP_SPI_WRITE(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to trigger SPI_TX_CMD\n", __func__);
		return ret;
	}

	//---wait SPI DMA done---
	for (i = 0; i < retry; i++) {
		buf[0] = (u8)(ts->mmap->SPI_TX_CMD & 0xFF);
		buf[1] = 0xFF;
		ret = CTP_SPI_READ(ts->client, buf, 2);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: failed to read SPI DMA done\n", __func__);
			return ret;
		}

		if (buf[1] == 0x00)
			break;

		msleep(20);
	}
	if (i == retry) {
		input_err(true, &ts->client->dev, "%s: write over retry limit\n", __func__);
		return -EIO;
	}

	return 0;
}

int nvt_ts_cascade_ics_write(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len)
{
	int ret;

	if (ts->mmap->SPI_TX_WRITE_TRIGGER)
		ret = nvt_ts_cascade_ics_write_NT3652x(ts, address, data, len);
	else
		ret = nvt_ts_cascade_ics_write_NT3653x(ts, address, data, len);

	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to write slave IC\n", __func__);
		return ret;
	}

	return 0;
}

/*
 * Description:
 *Novatek touchscreen clear status & enable fw crc function.
 *
 * return:
 *	N/A.
 */
void nvt_fw_crc_enable(void)
{
	uint8_t buf[8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	//---clear fw reset status---
	buf[0] = EVENT_MAP_RESET_COMPLETE & (0x7F);
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = 0x00;
	buf[6] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 7);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---enable fw crc---
	buf[0] = EVENT_MAP_HOST_CMD & (0x7F);
	buf[1] = 0xAE;	//enable fw crc command
	buf[2] = 0x00;
	CTP_SPI_WRITE(ts->client, buf, 3);
}

/*
 * Description:
 *	Novatek touchscreen set boot ready function.
 *
 * return:
 *	N/A.
 */
void nvt_boot_ready(void)
{
	//---write BOOT_RDY status cmds---
	nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 1);

	sec_delay(5);

	if (ts->hw_crc == HWCRC_NOSUPPORT) {
		//---write BOOT_RDY status cmds---
		nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 0);

		//---write POR_CD cmds---
		nvt_write_addr(ts->mmap->POR_CD_ADDR, 0xA0);
	}
}

/*
 * Description:
 *	Novatek touchscreen eng reset cmd
 *	function.
 *
 * return:
 *	n.a.
 */
void nvt_eng_reset(void)
{
	//---eng reset cmds to ENG_RST_ADDR---
	nvt_write_addr(ENG_RST_ADDR, 0x5A);

	sec_delay(1);
}

/*
 * Description:
 *	Novatek touchscreen reset MCU
 *	function.
 *
 * return:
 *	n.a.
 */
void nvt_sw_reset(void)
{
	//---software reset cmds to SWRST_SIF_ADDR---
	nvt_write_addr(ts->swrst_sif_addr, 0x55);

	msleep(20);
}

/*
 * Description:
 *	Novatek touchscreen reset MCU then into idle mode
 *	function.
 *
 * return:
 *	n.a.
 */
void nvt_sw_reset_idle(void)
{
	//---MCU idle cmds to SWRST_SIF_ADDR---
	nvt_write_addr(ts->swrst_sif_addr, 0xAA);

	msleep(20);
}

/*
 * Description:
 *	Novatek touchscreen reset MCU (boot) function.
 *
 * return:
 *	n.a.
 */
void nvt_bootloader_reset(void)
{
	//---reset cmds to SWRST_SIF_ADDR---
	nvt_write_addr(ts->swrst_sif_addr, 0x69);

	sec_delay(5);

	if (ts->spi_rd_fast_addr) {
		/* disable SPI_RD_FAST */
		nvt_write_addr(ts->spi_rd_fast_addr, 0x00);
	}
}

/*
 * Description:
 *	Novatek touchscreen clear FW status function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---fail.
 */
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_SPI_READ(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		sec_delay(10);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*
 * Description:
 *	Novatek touchscreen check FW status function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---failed.
 */
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		sec_delay(10);
	}

	if (i >= retry) {
		input_err(true, &ts->client->dev, "failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*
 * Description:
 *	Novatek touchscreen check FW reset state function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -1---failed.
 */
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t retry_max = (check_reset_state == RESET_STATE_INIT) ? 10 : 50;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_SPI_READ(ts->client, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if (unlikely(retry > retry_max)) {
			input_err(true, &ts->client->dev, "error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		sec_delay(10);
	}

	if (!ret)
		input_info(true, &ts->client->dev, "%s : retry=%d, buf[1] = %x\n", __func__, retry, buf[1]);

	return ret;
}

/*
 * Description:
 *	Novatek touchscreen get novatek project id information
 *	function.
 *
 * return:
 *	Executive outcomes. 0---success. -1---fail.
 */
int32_t nvt_read_pid(void)
{
	uint8_t buf[4] = {0};
	int32_t ret = 0;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	CTP_SPI_READ(ts->client, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];
	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER_PROJECT_ID] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev, "PID=%04X\n", ts->nvt_pid);

	return ret;
}

/*
 * Description:
 *	Novatek touchscreen get firmware related information
 *	function.
 *
 * return:
 *	Executive outcomes. 0---success. -1---fail.
 */
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_SPI_READ(ts->client, buf, 17);
	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		input_err(true, &ts->client->dev, "FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		if (retry_count < 3) {
			retry_count++;
			input_err(true, &ts->client->dev, "retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			ts->plat_data->x_node_num = 18;
			ts->plat_data->y_node_num = 32;
			ts->plat_data->max_x = TOUCH_DEFAULT_MAX_WIDTH;
			ts->plat_data->max_y = TOUCH_DEFAULT_MAX_HEIGHT;

			input_err(true, &ts->client->dev, "Set default x_num=%d, y_num=%d, "
					"abs_x_max=%d, abs_y_max=%d!\n",
					ts->plat_data->x_node_num, ts->plat_data->y_node_num,
					ts->plat_data->max_x, ts->plat_data->max_y);
			ret = -1;
			goto out;
		}
	}
	ts->plat_data->x_node_num = buf[3];
	ts->plat_data->y_node_num = buf[4];
	ts->plat_data->max_x = (uint16_t)((buf[5] << 8) | buf[6]);
	ts->plat_data->max_y = (uint16_t)((buf[7] << 8) | buf[8]);

	input_info(true, &ts->client->dev, "%s : fw_ver=0x%02X, x_num=%d, y_num=%d, "
			"abs_x_max=%d, abs_y_max=%d, fw_type=0x%02X\n",
			__func__, buf[1], ts->plat_data->x_node_num, ts->plat_data->y_node_num,
			ts->plat_data->max_x, ts->plat_data->max_y, buf[14]);

	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_IC_VER] = buf[15];

	//---Get Novatek PID---
	nvt_read_pid();

	//---get panel id---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PANEL);
	buf[0] = EVENT_MAP_PANEL;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "nvt_ts_i2c_read error(%d)\n", ret);
		return ret;
	}
	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_MODULE_VER] = buf[1];

	//---get firmware version---
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);
	buf[0] = EVENT_MAP_FWINFO;
	ret = CTP_SPI_READ(ts->client, buf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "nvt_ts_i2c_read error(%d)\n", ret);
		return ret;
	}
	ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER] = buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	input_info(true, &ts->client->dev, "%s : fw_ver_ic = NO%02X%02X%02X%02X\n",
			__func__, ts->plat_data->img_version_of_ic[SEC_INPUT_FW_IC_VER],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER_PROJECT_ID],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_MODULE_VER],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER]);

out:
	return ret;
}

/*
 * Create Device Node (Proc Entry)
 */
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTSPI"

/*
 * Description:
 *	Novatek touchscreen /proc/NVTSPI read function.
 *
 * return:
 *	Executive outcomes. 2---succeed. -5,-14---failed.
 */
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_TRANSFER_LEN + 3) || (count < 3)) {
		input_err(true, &ts->client->dev, "invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = kzalloc((count), GFP_KERNEL);
	if (str == NULL) {
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = kzalloc((count), GFP_KERNEL | GFP_DMA);
	if (buf == NULL) {
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		input_err(true, &ts->client->dev, "copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&ts->nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVTWRITE) {	//SPI write
		while (retries < 20) {
			ret = CTP_SPI_WRITE(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			input_err(true, &ts->client->dev, "error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			input_err(true, &ts->client->dev, "error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVTREAD) {	//SPI read
		while (retries < 20) {
			ret = CTP_SPI_READ(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			input_err(true, &ts->client->dev, "error, retries=%d, ret=%d\n", retries, ret);
			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			input_err(true, &ts->client->dev, "error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		input_err(true, &ts->client->dev, "Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
	kfree(buf);
kzalloc_failed:
	return ret;
}

/*
 * Description:
 *	Novatek touchscreen /proc/NVTSPI open function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -12---failed.
 */
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kzalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL)
		return -ENOMEM;

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*
 * Description:
 *	Novatek touchscreen /proc/NVTSPI close function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 */
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

#if (KERNEL_VERSION(5, 6, 0) <= LINUX_VERSION_CODE)
static const struct proc_ops nvt_flash_fops = {
	.proc_open = nvt_flash_open,
	.proc_release = nvt_flash_close,
	.proc_read = nvt_flash_read,
};
#else
const struct file_operations nvt_flash_fops = {
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};
#endif

/*
 * Description:
 *	Novatek touchscreen /proc/NVTSPI initial function.
 *
 * return:
 *	Executive outcomes. 0---succeed. -12---failed.
 */
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL, &nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		input_err(true, &ts->client->dev, "%s: Failed!\n", __func__);
		return -ENOMEM;
	}

	input_info(true, &ts->client->dev, "%s: Succeeded!\n", __func__);

	input_info(true, &ts->client->dev, "%s: ============================================================\n", __func__);
	input_info(true, &ts->client->dev, "%s: Create /proc/%s\n", __func__, DEVICE_NAME);
	input_info(true, &ts->client->dev, "%s: ============================================================\n", __func__);

	return 0;
}

/*
 * Description:
 *	Novatek touchscreen /proc/NVTSPI deinitial function.
 *
 * return:
 *	n.a.
 */
static void nvt_flash_proc_deinit(void)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		input_info(true, &ts->client->dev, "%s: Removed /proc/%s\n", __func__, DEVICE_NAME);
	}
}
#endif

#if WAKEUP_GESTURE

/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE        0x01

/*
 * Description:
 *	Novatek touchscreen wake up gesture key report function.
 *
 * return:
 *	n.a.
 */
void nvt_ts_wakeup_gesture_report(uint8_t *data)
{
	uint8_t func_id = data[3];

//	input_info(true, &ts->client->dev, "gesture_id = %d\n", func_id);

	switch (func_id) {
	case GESTURE_DOUBLE_CLICK:
		input_report_key(ts->input_dev, KEY_WAKEUP, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, KEY_WAKEUP, 0);
		input_sync(ts->input_dev);
		input_info(true, &ts->client->dev, "Gesture : Double tap to wakeup\n");
		break;
	case GESTURE_SLIDE_UP:
		sec_cmd_send_gesture_uevent(&ts->sec, SPONGE_EVENT_TYPE_SPAY, 0, 0);
		break;
	default:
		input_err(true, &ts->client->dev, "invalid gesture event (%02X %02X %02X %02X %02X %02X)\n",
			data[0], data[1], data[2],
			data[3], data[4], data[5]);
		break;
	}
}
#endif

/*
 * Description:
 *	Novatek touchscreen parse device tree function.
 *
 * return:
 *	n.a.
 */
static int32_t nvt_parse_dt(struct nvt_ts_data *ts, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int32_t ret = 0;
	int count;

	input_info(true, dev, "%s: start!\n", __func__);

	if (!np)
		return -ENODEV;

	/* lcd reset */
	if (of_property_read_string(np, "novatek,name_lcd_rst", &ts->name_lcd_rst)) {
		input_err(true, dev, "%s: Failed to get name_lcd_rst property\n", __func__);
		ts->name_lcd_rst = NULL;
	}

	/* lcd regulator */
	count = of_property_count_strings(np, "novatek,regulator_name");
	if (count < 0) {
		ts->regulator_count = 0;
		input_err(true, dev, "%s: Failed to get regulator_name property\n", __func__);
	} else {
		ts->regulator_count = count;
		if (ts->regulator_count > NVT_TS_REGULATOR_MAX) {
			input_err(true, dev, "%s: regulator_count %d is over then regulator array size(%d)\n",
					__func__, ts->regulator_count, NVT_TS_REGULATOR_MAX);
			ts->regulator_count = NVT_TS_REGULATOR_MAX;
		}
		input_info(true, dev, "%s: try to get %d regulator\n", __func__, ts->regulator_count);
		ret = of_property_read_string_array(np, "novatek,regulator_name",
				ts->regulator_name, ts->regulator_count);
		if (ret < 0) {
			input_err(true, dev, "%s: Failed to get regulator_name property\n", __func__);
			return ret;
		}
		input_info(true, dev, "%s: success to get regulator, ret=%d\n", __func__, ret);
	}

#if NVT_TOUCH_SUPPORT_HW_RST
	ts->reset_gpio = of_get_named_gpio(np, "novatek,reset-gpio", 0);
	input_info(true, dev, "%s: novatek,reset-gpio=%d\n", __func__, ts->reset_gpio);

	/* request RST-pin (Output/High) */
	if (gpio_is_valid(ts->reset_gpio)) {
		ret = devm_gpio_request_one(dev, ts->reset_gpio, GPIOF_OUT_INIT_LOW, "NVT-tp-rst");
		if (ret) {
			input_err(true, &ts->client->dev, "Failed to request NVT-tp-rst GPIO\n");
			return ret;
		}
	}
#endif

	ret = of_property_read_u32(np, "novatek,spi-rd-fast-addr", &ts->spi_rd_fast_addr);
	if (ret) {
		input_info(true, dev, "%s: not support novatek,spi-rd-fast-addr\n", __func__);
		ts->spi_rd_fast_addr = 0;
	} else {
		input_info(true, dev, "%s: SPI_RD_FAST_ADDR=0x%06X\n", __func__, ts->spi_rd_fast_addr);
	}

	/* active support glove mode / other support high sensitivity mode */
	ts->enable_glove_mode = of_property_read_bool(np, "novatek,enable_glove_mode");
	input_info(true, dev, "%s: support %s\n",
				__func__, ts->enable_glove_mode ? "glove mode" : "high sensitivity mode");

	input_info(true, dev, "%s: end!\n", __func__);
	return 0;
}

static int nvt_regulator_init(struct nvt_ts_data *ts)
{
	int i;

	if (ts->name_lcd_rst) {
		ts->regulator_lcd_rst = devm_regulator_get(&ts->client->dev, ts->name_lcd_rst);
		if (IS_ERR(ts->regulator_lcd_rst)) {
			input_err(true, &ts->client->dev,
					"%s: Failed to get regulator_lcd_rst regulator.\n", __func__);
			return -ENODEV;
		}
		input_info(true, &ts->client->dev, "%s: init %s regulator\n", __func__, ts->name_lcd_rst);
	}

	for (i = 0; i < ts->regulator_count; i++) {
		ts->regulator[i] = devm_regulator_get(&ts->client->dev, ts->regulator_name[i]);
		if (IS_ERR(ts->regulator[i])) {
			input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
					__func__, ts->regulator_name[i]);
			return -ENODEV;
		}
		input_info(true, &ts->client->dev, "%s: init %s regulator\n", __func__, ts->regulator_name[i]);
	}

	return 0;
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i = 1 ; i < 7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	ts->irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	ts->esd_retry = enable ? 0 : ts->esd_retry;
	/* enable/disable esd check flag */
	ts->esd_check = enable;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - ts->irq_timer);
	uint8_t wbuf[4] = {0};

	//pr_info("esd_check = %d (retry %d)\n", esd_check, esd_retry);	//DEBUG

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && ts->esd_check) {
		mutex_lock(&ts->lock);
		input_err(true, &ts->client->dev, "%s: do ESD recovery, timer = %d, retry = %d\n",
					__func__, timer, ts->esd_retry);
		/* do esd recovery, reload fw */
		nvt_update_firmware(ts->plat_data->firmware_name);

		if ((sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE)) &&
			(ts->plat_data->lowpower_mode || ts->lcdoff_test)) {
			if (nvt_check_fw_reset_state(RESET_STATE_INIT))
				input_err(true, &ts->client->dev, "%s: Check FW init state failed after ESD recovery\n",
						__func__);
			else {
				//---write command to enter "wakeup gesture mode"---
				wbuf[0] = EVENT_MAP_HOST_CMD;
				wbuf[1] = LPWG_ENTER;
				wbuf[2] = 0x80;
				CTP_SPI_WRITE(ts->client, wbuf, 3);

				input_info(true, &ts->client->dev, "%s: re-enter lp mode, 0x%02X\n",
						__func__, ts->plat_data->lowpower_mode);
			}
		}

		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			input_err(true, &ts->client->dev, "%s: Check FW reK state failed after ESD recovery\n",
					__func__);
		else
			nvt_ts_mode_restore(ts);
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		ts->irq_timer = jiffies;
		/* update esd_retry counter */
		ts->esd_retry++;
	}

	queue_delayed_work(ts->nvt_esd_check_wq, &ts->nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_WDT_RECOVERY
static uint8_t recovery_cnt;
static uint8_t nvt_wdt_fw_recovery(uint8_t *point_data)
{
	uint32_t recovery_cnt_max = 10;
	uint8_t recovery_enable = false;
	uint8_t i = 0;

	recovery_cnt++;

	/* check pattern */
	for (i = 1 ; i < 7 ; i++) {
		if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
			recovery_cnt = 0;
			break;
		}
	}

	if (recovery_cnt > recovery_cnt_max) {
		recovery_enable = true;
		recovery_cnt = 0;
	}

	return recovery_enable;
}

void nvt_read_fw_history(uint32_t fw_history_addr)
{
	uint8_t i = 0;
	uint8_t buf[65] = { 0, };
	char str[128];

	if (fw_history_addr == 0)
		return;

	nvt_set_page(fw_history_addr);

	buf[0] = (uint8_t) (fw_history_addr & 0x7F);
	CTP_SPI_READ(ts->client, buf, 64+1);	//read 64bytes history

	//print all data
	input_info(true, &ts->client->dev, "fw history 0x%X:\n", fw_history_addr);
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str),
				"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
				buf[1+i*16], buf[2+i*16], buf[3+i*16], buf[4+i*16],
				buf[5+i*16], buf[6+i*16], buf[7+i*16], buf[8+i*16],
				buf[9+i*16], buf[10+i*16], buf[11+i*16], buf[12+i*16],
				buf[13+i*16], buf[14+i*16], buf[15+i*16], buf[16+i*16]);
		input_info(true, &ts->client->dev, "%s\n", str);
	}

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}

void nvt_read_fw_history_all(void)
{

	/* ICM History */
	nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0);
	nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1);

	/* ICS History */
	if (ts->is_cascade) {
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0_ICS);
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1_ICS);
	}
}
void nvt_clear_aci_error_flag(void)
{
	if (ts->mmap->ACI_ERR_CLR_ADDR == 0)
		return;

	nvt_write_addr(ts->mmap->ACI_ERR_CLR_ADDR, 0xA5);

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
}
#endif	/* #if NVT_TOUCH_WDT_RECOVERY */

static void nvt_print_info_work(struct work_struct *work)
{
	sec_input_print_info(&ts->client->dev, NULL);

	schedule_delayed_work(&ts->work_print_info, msecs_to_jiffies(TOUCH_PRINT_INFO_DWORK_TIME));
}

static void nvt_read_info_work(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	input_err(true, &ts->client->dev, "%s: factory bin : skip factory_cmd_result_all call\n", __func__);
#endif

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	read_tsp_info_onboot(&ts->sec);
#endif

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	schedule_work(&ts->work_print_info.work);
}

#if POINT_DATA_CHECKSUM
static int32_t nvt_ts_point_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++)
		checksum += buf[i + 1];

	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		input_err(true, &ts->client->dev, "%s : checksum not match.(point_data[%d]=0x%02X, checksum=0x%02X)\n",
				__func__, length, buf[length], checksum);

		for (i = 0; i < 10; i++) {
			input_dbg(true, &ts->client->dev, "%02X %02X %02X %02X %02X %02X\n",
				buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
		}

		input_dbg(true, &ts->client->dev, "%02X %02X %02X %02X %02X\n", buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}

#if SEC_LPWG_DUMP
// Due to more checksum length than point report, duplicate another checksum function
static int32_t nvt_ts_lpwg_dump_checksum(uint8_t *buf, uint32_t length)
{
	uint8_t checksum = 0;
	uint32_t i = 0;

	// Generate checksum
	for (i = 0; i < length - 1; i++)
		checksum += buf[i + 1];

	checksum = (~checksum + 1);

	// Compare ckecksum and dump fail data
	if (checksum != buf[length]) {
		input_err(true, &ts->client->dev, "%s : checksum not match.(lpwg_dump[%d]=0x%02X, checksum=0x%02X)\n",
					__func__, length, buf[length], checksum);

		for (i = 0; i < 10; i++) {
			input_dbg(true, &ts->client->dev, "%s : %02X %02X %02X %02X %02X %02X\n",
					__func__, buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
		}

		input_dbg(true, &ts->client->dev, "%s : %02X %02X %02X %02X %02X\n",
					__func__, buf[61], buf[62], buf[63], buf[64], buf[65]);

		return -1;
	}

	return 0;
}
#endif
#endif /* POINT_DATA_CHECKSUM */

#if SEC_FW_STATUS
#define FW_STATUS_OFFSET	(1 + 0x3C)	// 1 is for read dummy byte, total 2 bytes length for fw status

int nvt_ts_check_all_value(uint8_t *point_data)
{
	int tmp_val = 0xFF;
	int ret = 0, i;

	for (i = 1 ; i < POINT_DATA_CHECKSUM_LEN ; i++)
		tmp_val &= point_data[i];

	if (tmp_val == 0xff) {
		input_info(true, &ts->client->dev, "%s: all data is 0xff\n", __func__);
		ret = -1;
	}
	return ret;
}

void nvt_ts_ic_status(uint8_t *point_data, bool force_print)
{
	u16 fw_status = 0;
	u16 fw_status_changed = 0;
	char print_buff[800] = { 0 };
	char tmp_buff[100] = { 0 };

	if (force_print) {
		// print current status
		fw_status = ts->fw_status_record;
		fw_status_changed = 0xFFFF;
	} else {
		if (point_data == NULL) {
			input_err(true, &ts->client->dev, "%s: point_data is null\n", __func__);
			return;
		}

		// check abnormal event
		if (nvt_ts_check_all_value(point_data))
			return;

		// print change status
		fw_status = (point_data[FW_STATUS_OFFSET + 1] << 8) | point_data[FW_STATUS_OFFSET];
		fw_status_changed = fw_status ^ ts->fw_status_record;
		ts->fw_status_record = fw_status;
	}

	if (fw_status_changed & FW_STATUS_WATER_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Water flag %s)\n", __func__, (fw_status & FW_STATUS_WATER_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Water flag %s, ", (fw_status & FW_STATUS_WATER_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
		ts->plat_data->wet_mode = !!(fw_status & FW_STATUS_WATER_FLAG);
	}

	if (fw_status_changed & FW_STATUS_PALM_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Palm flag %s)\n", __func__, (fw_status & FW_STATUS_PALM_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Palm flag %s, ", (fw_status & FW_STATUS_PALM_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if ((fw_status_changed & FW_STATUS_HOPPING_FLAG) || (fw_status_changed & FW_STATUS_DIRTY_FLAG)) {
		u8 is_hopping = !!(fw_status & FW_STATUS_HOPPING_FLAG);
		u8 is_dirty = !!(fw_status & FW_STATUS_DIRTY_FLAG);

		ts->plat_data->noise_mode = is_hopping << 1 | is_dirty;
		atomic_set(&ts->plat_data->touch_noise_status, ts->plat_data->noise_mode);
		snprintf(tmp_buff, 100, "Noise level %d (hopping:%d, dirty:%d), ",
				ts->plat_data->noise_mode, is_hopping, is_dirty);
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
		sec_cmd_send_status_uevent(&ts->sec, STATUS_TYPE_NOISE, ts->plat_data->noise_mode);
		if (ts->plat_data->noise_mode)
			ts->plat_data->hw_param.noise_count++;
	}

	if (fw_status_changed & FW_STATUS_BENDING_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Bending flag %s)\n", __func__, (fw_status & FW_STATUS_BENDING_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Bending flag %s, ", (fw_status & FW_STATUS_BENDING_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

/*
	if (fw_status_changed & FW_STATUS_GLOVE_FLAG) {
//			input_info(true, &ts->client->dev, "%s: FW status (Glove flag %s)\n", __func__, (fw_status & FW_STATUS_GLOVE_FLAG) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "Glove flag %s, ", (fw_status & FW_STATUS_GLOVE_FLAG) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}
*/

	if (fw_status_changed & FW_STATUS_GND_UNSTABLE) {
//			input_info(true, &ts->client->dev, "%s: FW status (GND unstable %s)\n", __func__, (fw_status & FW_STATUS_GND_UNSTABLE) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "GND unstable %s, ", (fw_status & FW_STATUS_GND_UNSTABLE) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}

	if (fw_status_changed & FW_STATUS_TA_PIN) {
//			input_info(true, &ts->client->dev, "%s: FW status (TA pin %s)\n", __func__, (fw_status & FW_STATUS_TA_PIN) ? "ON" : "OFF");
		snprintf(tmp_buff, 100, "TA pin %s, ", (fw_status & FW_STATUS_TA_PIN) ? "ON" : "OFF");
		strlcat(print_buff, tmp_buff, sizeof(print_buff));
		memset(tmp_buff, 0x00, 100);
	}
	if (print_buff[0])
		input_info(true, &ts->client->dev, "%s: %s FW status %s\n",
					__func__, force_print ? "Current" : "Change", print_buff);

	if (force_print)
		return;

	switch ((fw_status & FW_STATUS_REK_STATUS) >> 8) {
	case 0:
		// Do nothing...
		break;
	case 1:
		input_err(true, &ts->client->dev, "%s: FW status (1D reK)\n", __func__);
		break;
	case 2:
		input_err(true, &ts->client->dev, "%s: FW status (2D RC reK)\n", __func__);
		break;
	case 3:
		input_err(true, &ts->client->dev, "%s: FW status (2D raw check reK)\n", __func__);
		break;
	default:
		input_err(true, &ts->client->dev, "%s: FW status (invalid reK status : 0x%04X)\n",
					__func__, fw_status);
	}
}
#endif

#define POINT_DATA_LEN 108
#define FINGER_MOVING		0x00
#define GLOVE_TOUCH		0x03
#define PALM_TOUCH		0x05

#if SEC_LPWG_DUMP
#define LPWG_DUMP_LOG_LEN		505	//2B Slot ID + 2B History Size + 500B History + 1B Checksum
#define LPWG_DUMP_EVENT_LEN		5
#define LPWG_DUMP_EVENT_MAX_NUM	100
#define LPWG_DUMP_EVENT_MSG_LEN	20

void nvt_ts_lpwg_dump_buf_init(void)
{
	ts->lpwg_dump_buf = devm_kzalloc(&ts->client->dev, LPWG_DUMP_TOTAL_SIZE, GFP_KERNEL);
	if (ts->lpwg_dump_buf == NULL)
		return;

	ts->lpwg_dump_buf_idx = 0;
	input_info(true, &ts->client->dev, "%s : done\n", __func__);
}

int nvt_ts_lpwg_dump_buf_write(u8 *buf)
{
	int i = 0;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return -1;
	}
//	input_info(true, &ts->client->dev, "%s : idx(%d) data (0x%X,0x%X,0x%X,0x%X,0x%X)\n",
//			__func__, ts->lpwg_dump_buf_idx, buf[0], buf[1], buf[2], buf[3], buf[4]);

	for (i = 0 ; i < LPWG_DUMP_PACKET_SIZE ; i++)
		ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx++] = buf[i];

	if (ts->lpwg_dump_buf_idx >= LPWG_DUMP_TOTAL_SIZE) {
		input_info(true, &ts->client->dev, "%s : write end of data buf(%d)!\n",
					__func__, ts->lpwg_dump_buf_idx);
		ts->lpwg_dump_buf_idx = 0;
	}
	return 0;
}

int nvt_ts_lpwg_dump_buf_read(u8 *buf)
{

	u8 read_buf[30] = { 0 };
	int read_packet_cnt;
	int start_idx;
	int i;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return 0;
	}

	if (ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 1] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 2] == 0) {
		start_idx = 0;
		read_packet_cnt = ts->lpwg_dump_buf_idx / LPWG_DUMP_PACKET_SIZE;
	} else {
		start_idx = ts->lpwg_dump_buf_idx;
		read_packet_cnt = LPWG_DUMP_TOTAL_SIZE / LPWG_DUMP_PACKET_SIZE;
	}

	input_info(true, &ts->client->dev, "%s : lpwg_dump_buf_idx(%d), start_idx (%d), read_packet_cnt(%d)\n",
				__func__, ts->lpwg_dump_buf_idx, start_idx, read_packet_cnt);

	for (i = 0 ; i < read_packet_cnt ; i++) {
		memset(read_buf, 0x00, 30);
		snprintf(read_buf, 30, "%03d : %02X%02X%02X%02X%02X\n",
					i, ts->lpwg_dump_buf[start_idx + 0], ts->lpwg_dump_buf[start_idx + 1],
					ts->lpwg_dump_buf[start_idx + 2], ts->lpwg_dump_buf[start_idx + 3],
					ts->lpwg_dump_buf[start_idx + 4]);

//		input_info(true, &ts->client->dev, "%s : %s\n", __func__, read_buf);
		strlcat(buf, read_buf, PAGE_SIZE);

		if (start_idx + LPWG_DUMP_PACKET_SIZE >= LPWG_DUMP_TOTAL_SIZE)
			start_idx = 0;
		else
			start_idx += 5;
	}

	return 0;
}

/*
 * Description:
 *	Novatek lpwg log dump function.
 *
 * return:
 *	n.a.
 */
void nvt_ts_lpwg_dump(void)
{
#if 0
	struct nvt_ts_lpwg_coordinate_event *p_lpwg_coordinate_event;
	struct nvt_ts_lpwg_gesture_event *p_lpwg_gesture_event;
	struct nvt_ts_lpwg_vendor_event *p_lpwg_vendor_event;
	char buff[LPWG_DUMP_EVENT_MSG_LEN] = { 0 };
#endif
	uint8_t log_dump[LPWG_DUMP_LOG_LEN + DUMMY_BYTES] = {0};
	u16 next_slot = 0, history_size = 0, event_cnt = 0, i = 0;
	int32_t ret = -1;

	if (ts->mmap->LPWG_DUMP_ADDR == 0) {
		input_err(true, &ts->client->dev, "%s: Invalid LPWG dump address.(%d)\n", __func__, ts->mmap->LPWG_DUMP_ADDR);
		return;
	}

	mutex_lock(&ts->lock);

	//---set xdata index to LPWG_DUMP_ADDR---
	nvt_set_page(ts->mmap->LPWG_DUMP_ADDR);

	//---read data from index---
	log_dump[0] = ts->mmap->LPWG_DUMP_ADDR & (0x7F);
	ret = CTP_SPI_READ(ts->client, log_dump, LPWG_DUMP_LOG_LEN + DUMMY_BYTES);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s:  CTP_SPI_READ failed.(%d)\n", __func__, ret);
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
	ret = nvt_ts_lpwg_dump_checksum(log_dump, LPWG_DUMP_LOG_LEN);
	if (ret)
		goto XFER_ERROR;

#endif /* POINT_DATA_CHECKSUM */

	// Deal with 4 header bytes, skip 1 dummy byte
	next_slot = log_dump[1] + (log_dump[2] << 8);
	history_size = log_dump[3] + (log_dump[4] << 8);
	event_cnt = history_size / LPWG_DUMP_EVENT_LEN;

	if (event_cnt < LPWG_DUMP_EVENT_MAX_NUM)
		i = 0;
	else
		i = next_slot;

	input_info(true, &ts->client->dev, "%s: event_cnt(%d) start i(%d)\n", __func__, event_cnt, i);

	for (; event_cnt > 0; i++, event_cnt--) {

		i %= LPWG_DUMP_EVENT_MAX_NUM;	// in case overrun, FIFO (Round Robin scheme)

		nvt_ts_lpwg_dump_buf_write(&log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)]);

#if 0
		//extra 1 event len for 1 dummy byte and 4 header bytes
		p_lpwg_coordinate_event = (struct nvt_ts_lpwg_coordinate_event *)&log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)];
//		input_info(true, &ts->client->dev, "%s: Event slot(%d) Event type(%d)\n",
//				__func__, i, p_lpwg_coordinate_event->event_type);

		if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_COOR) {
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev, "%s: slot(%2d) [%s] TID(%2d) : X(%4d) Y(%4d) Frame count(%4d)\n",
				__func__, i, p_lpwg_coordinate_event->touch_status ? "R" : "P", p_lpwg_coordinate_event->tid,
				((p_lpwg_coordinate_event->x_11_4 << 4) + p_lpwg_coordinate_event->x_3_0),
				((p_lpwg_coordinate_event->y_11_4 << 4) + p_lpwg_coordinate_event->y_3_0),
				((p_lpwg_coordinate_event->frame_count_9_8 << 8) + p_lpwg_coordinate_event->frame_count_7_0));
#else
			input_info(true, &ts->client->dev, "%s: slot(%2d) [%s] TID(%2d) Frame count(%4d)\n",
				__func__, i, p_lpwg_coordinate_event->touch_status ? "R" : "P", p_lpwg_coordinate_event->tid,
				((p_lpwg_coordinate_event->frame_count_9_8 << 8) + p_lpwg_coordinate_event->frame_count_7_0));
#endif
		} else if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_GESTURE) {
			p_lpwg_gesture_event = (struct nvt_ts_lpwg_gesture_event *)p_lpwg_coordinate_event;

			input_info(true, &ts->client->dev, "%s: slot(%2d) Gesture ID(%11s)\n",
				__func__, i, p_lpwg_gesture_event->gesture_id ? "Swipe up" : "Double tap");
		} else if (p_lpwg_coordinate_event->event_type == LPWG_EVENT_VENDOR) {
			p_lpwg_vendor_event = (struct nvt_ts_lpwg_vendor_event *)p_lpwg_coordinate_event;

			switch (p_lpwg_vendor_event->ng_code) {
			case 4:
				snprintf(buff, sizeof(buff), "%s", "Timing Err");
				break;
			case 5:
				snprintf(buff, sizeof(buff), "%s", "Distance Err");
				break;
			case 6:
				snprintf(buff, sizeof(buff), "%s", "Long Touch Err");
				break;
			case 7:
				snprintf(buff, sizeof(buff), "%s", "Multi-Finger");
				break;
			default:
				snprintf(buff, sizeof(buff), "%s", "Unknown Err Code");
			}
			input_info(true, &ts->client->dev, "%s: slot(%2d) Event type(Vendor) Info(%2d) NgType(%20s)\n",
					__func__, i, p_lpwg_vendor_event->info_type, buff);
		} else {	// invalid event
			input_info(true, &ts->client->dev, "%s: Event slot(%2d) Event type(Unknown)\n"
				"	invalid event!!! (%02X, %02X, %02X, %02X, %02X)\n",
				__func__, i, log_dump[LPWG_DUMP_EVENT_LEN * (i + 1)],
				log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 1], log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 2],
				log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 3], log_dump[LPWG_DUMP_EVENT_LEN * (i + 1) + 4]);
		}
#endif

	}

XFER_ERROR:
	//---set xdata index to EVENT_BUF_ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	mutex_unlock(&ts->lock);
}
#endif

static void nvt_ts_coord_parsing(struct nvt_ts_data *ts, struct nvt_ts_event_coord *p_event_coord, uint8_t *point_data, u8 id, u16 i)
{
	ts->plat_data->coord[id].id = id;
	ts->plat_data->coord[id].ttype = p_event_coord->status;
	ts->plat_data->coord[id].x = (u16)(p_event_coord->x_11_4 << 4) + (u16)(p_event_coord->x_3_0);
	ts->plat_data->coord[id].y = (u16)(p_event_coord->y_11_4 << 4) + (u16)(p_event_coord->y_3_0);
	ts->plat_data->coord[id].major = p_event_coord->w_major ? p_event_coord->w_major : 1;
	ts->plat_data->coord[id].minor = point_data[i + 99] ? point_data[i + 99] : 1;
	ts->plat_data->coord[id].z = p_event_coord->pressure_7_0;
#if SEC_FW_STATUS
	ts->plat_data->coord[id].noise_status = ts->plat_data->noise_mode;
#endif
	if (!ts->plat_data->coord[id].palm && (ts->plat_data->coord[id].ttype == PALM_TOUCH))
		ts->plat_data->coord[id].palm_count++;

	ts->plat_data->coord[id].palm = (ts->plat_data->coord[id].ttype == PALM_TOUCH) ? 1 : 0;
	if (ts->plat_data->coord[id].palm)
		ts->plat_data->palm_flag |= (1 << id);
	else
		ts->plat_data->palm_flag &= ~(1 << id);

	if (ts->plat_data->prev_coord[id].action == SEC_TS_COORDINATE_ACTION_PRESS ||
			ts->plat_data->prev_coord[id].action == SEC_TS_COORDINATE_ACTION_MOVE)
		ts->plat_data->coord[id].action = SEC_TS_COORDINATE_ACTION_MOVE;
	else
		ts->plat_data->coord[id].action = SEC_TS_COORDINATE_ACTION_PRESS;
}

void nvt_ts_touch_report(struct nvt_ts_data *ts, uint8_t *point_data)
{
	struct nvt_ts_event_coord *p_event_coord;
	u8 id = 0, status = 0, press_id[SEC_TS_SUPPORT_TOUCH_COUNT] = { 0 };
	u16 i = 0;

	for (i = 0; i < SEC_TS_SUPPORT_TOUCH_COUNT; i++) {
		p_event_coord = (struct nvt_ts_event_coord *)&point_data[1 + 6 * i];
		id = p_event_coord->id;
		if (!id || (id > SEC_TS_SUPPORT_TOUCH_COUNT))
			continue;

		id = id - 1;
		status = p_event_coord->status;

		if ((status == FINGER_MOVING) || (status == GLOVE_TOUCH) || (status == PALM_TOUCH)) {
#if NVT_TOUCH_ESD_PROTECT
			/* update interrupt timer */
			ts->irq_timer = jiffies;
#endif /* #if NVT_TOUCH_ESD_PROTECT */
			ts->plat_data->prev_coord[id] = ts->plat_data->coord[id];
			nvt_ts_coord_parsing(ts, p_event_coord, point_data, id, i);
			sec_input_coord_event_fill_slot(&ts->client->dev, id);
			press_id[id] = true;
		}
	}
	sec_input_coord_event_sync_slot(&ts->client->dev);

	for (i = 0; i < SEC_TS_SUPPORT_TOUCH_COUNT; i++) {
		if (!press_id[i] && (ts->plat_data->coord[i].action == SEC_TS_COORDINATE_ACTION_PRESS ||
				ts->plat_data->coord[i].action == SEC_TS_COORDINATE_ACTION_MOVE)) {
			ts->plat_data->prev_coord[i] = ts->plat_data->coord[i];
			ts->plat_data->coord[i].action = SEC_TS_COORDINATE_ACTION_RELEASE;
			ts->plat_data->coord[i].ttype = FINGER_MOVING; /* normal type */
			ts->plat_data->coord[i].palm = 0;
			sec_input_coord_event_fill_slot(&ts->client->dev, i);
		}
	}
	sec_input_coord_event_sync_slot(&ts->client->dev);
}

/*
 * Description:
 *	Novatek touchscreen work function.
 *
 * return:
 *	N.A
 */
static irqreturn_t nvt_ts_work_func(int irq, void *data)
{
	struct nvt_ts_data *ts = (struct nvt_ts_data *)data;
	uint8_t point_data[POINT_DATA_LEN + 1 + DUMMY_BYTES] = {0};
	uint8_t wbuf[4] = {0};
	int ret = -1;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->plat_data->secure_interrupt,
				msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
				"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif

	ret = sec_input_handler_start(&ts->client->dev);
	if (ret < 0)
		return IRQ_HANDLED;

	mutex_lock(&ts->lock);
	ret = CTP_SPI_READ(ts->client, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s:  CTP_SPI_READ failed.(%d)\n", __func__, ret);
		goto XFER_ERROR;
	}

#if NVT_TOUCH_WDT_RECOVERY
	/* ESD protect by WDT */
	if (nvt_wdt_fw_recovery(point_data)) {
		__pm_wakeup_event(ts->plat_data->sec_ws, 1000);
		input_err(true, &ts->client->dev, "Recover for fw reset, %02X\n", point_data[1]);

		if (point_data[1] == 0xFE) {
			nvt_sw_reset_idle();
			nvt_clear_aci_error_flag();
		}

		nvt_read_fw_history_all();

		nvt_update_firmware(ts->plat_data->firmware_name);

		if ((sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE)) &&
			(ts->plat_data->lowpower_mode || ts->lcdoff_test)) {
			if (nvt_check_fw_reset_state(RESET_STATE_INIT))
				input_err(true, &ts->client->dev, "%s: Check FW init state failed after FW reset recovery\n",
						__func__);
			else {
				//---write command to enter "wakeup gesture mode"---
				wbuf[0] = EVENT_MAP_HOST_CMD;
				wbuf[1] = LPWG_ENTER;
				wbuf[2] = 0x80;
				CTP_SPI_WRITE(ts->client, wbuf, 3);

				input_info(true, &ts->client->dev, "%s: re-enter lp mode, 0x%02X\n",
						__func__, ts->plat_data->lowpower_mode);
			}
		}

		if (nvt_check_fw_reset_state(RESET_STATE_REK))
			input_err(true, &ts->client->dev, "%s: Check FW reK state failed after FW reset recovery\n",
					__func__);
		else
			nvt_ts_mode_restore(ts);
		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_WDT_RECOVERY */

	/* ESD protect by FW handshake */
	if (nvt_fw_recovery(point_data)) {
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(true);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

#if POINT_DATA_CHECKSUM
	if (POINT_DATA_LEN >= POINT_DATA_CHECKSUM_LEN) {
		ret = nvt_ts_point_data_checksum(point_data, POINT_DATA_CHECKSUM_LEN);
		if (ret)
			goto XFER_ERROR;
	}
#endif /* POINT_DATA_CHECKSUM */

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE)) {
		if (point_data[2] == FUNCPAGE_GESTURE)
			nvt_ts_wakeup_gesture_report(point_data);
		else
			input_err(true, &ts->client->dev, "invalid lp event (%02X %02X %02X %02X %02X %02X)\n",
				point_data[0], point_data[1], point_data[2],
				point_data[3], point_data[4], point_data[5]);
		goto XFER_ERROR;
	}

#if SEC_FW_STATUS
	nvt_ts_ic_status(point_data, false);
#endif

	nvt_ts_touch_report(ts, point_data);

XFER_ERROR:
	mutex_unlock(&ts->lock);

	return IRQ_HANDLED;
}


/*
 * Description:
 *	Novatek touchscreen check chip version trim function.
 *
 * return:
 *	Executive outcomes. 0---NVT IC. -1---not NVT IC.
 */
static int32_t nvt_ts_check_chip_ver_trim(struct nvt_ts_hw_reg_addr_info hw_regs)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;
	uint8_t enb_casc = 0;

	/* hw reg mapping */
	ts->chip_ver_trim_addr = hw_regs.chip_ver_trim_addr;
	ts->swrst_sif_addr = hw_regs.swrst_sif_addr;
	ts->crc_err_flag_addr = hw_regs.crc_err_flag_addr;

	input_info(true, &ts->client->dev, "%s: check chip ver trim with chip_ver_trim_addr=0x%06x, "
			"swrst_sif_addr=0x%06x, crc_err_flag_addr=0x%06x\n",
			__func__, ts->chip_ver_trim_addr, ts->swrst_sif_addr, ts->crc_err_flag_addr);

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_bootloader_reset();

		nvt_set_page(ts->chip_ver_trim_addr);

		buf[0] = ts->chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_SPI_WRITE(ts->client, buf, 7);

		buf[0] = ts->chip_ver_trim_addr & 0x7F;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		CTP_SPI_READ(ts->client, buf, 7);
		input_info(true, &ts->client->dev, "buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX)
				found_nvt_chip = 1;

			if (found_nvt_chip) {
				input_info(true, &ts->client->dev, "This is NVT touch IC\n");
				if (trim_id_table[list].mmap->ENB_CASC_REG.addr) {
					/* check single or cascade */
					nvt_read_reg(trim_id_table[list].mmap->ENB_CASC_REG, &enb_casc);
					if (enb_casc & 0x01) {
						input_info(true, &ts->client->dev, "Single Chip\n");
						ts->mmap = trim_id_table[list].mmap;
						ts->is_cascade = false;
					} else {
						input_info(true, &ts->client->dev, "Cascade Chip\n");
						ts->mmap = trim_id_table[list].mmap_casc;
						ts->is_cascade = true;
					}
				} else {
					/* for chip that do not have ENB_CASC */
					ts->mmap = trim_id_table[list].mmap;
				}
				/* hw info */
				ts->hw_crc = trim_id_table[list].hwinfo->hw_crc;
				ts->auto_copy = trim_id_table[list].hwinfo->auto_copy;
				ts->bld_multi_header = trim_id_table[list].hwinfo->bld_multi_header;

				/* hw reg re-mapping */
				ts->chip_ver_trim_addr = trim_id_table[list].hwinfo->hw_regs->chip_ver_trim_addr;
				ts->swrst_sif_addr = trim_id_table[list].hwinfo->hw_regs->swrst_sif_addr;
				ts->crc_err_flag_addr = trim_id_table[list].hwinfo->hw_regs->crc_err_flag_addr;

				input_info(true, &ts->client->dev, "set reg chip_ver_trim_addr=0x%06x, "
						"swrst_sif_addr=0x%06x, crc_err_flag_addr=0x%06x\n",
						ts->chip_ver_trim_addr, ts->swrst_sif_addr, ts->crc_err_flag_addr);

				ret = 0;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(20);
	}

out:
	return ret;
}

/*
 * Description:
 *	Novatek touchscreen check chip version trim loop
 *	function. Check chip version trim via hw regs table.
 *
 * return:
 *	Executive outcomes. 0---NVT IC. -1---not NVT IC.
 */
static int32_t nvt_ts_check_chip_ver_trim_loop(void)
{
	uint8_t i = 0;
	int32_t ret = 0;

	struct nvt_ts_hw_reg_addr_info hw_regs_table[] = {
		hw_reg_addr_info,
		hw_reg_addr_info_old_w_isp,
		hw_reg_addr_info_legacy_w_isp
	};

	for (i = 0; i < (sizeof(hw_regs_table) / sizeof(struct nvt_ts_hw_reg_addr_info)); i++) {
		//---check chip version trim---
		ret = nvt_ts_check_chip_ver_trim(hw_regs_table[i]);
		if (!ret)
			break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
static int nvt_notifier_call(struct notifier_block *n, unsigned long event, void *data)
{
	struct panel_notifier_event_data *evtdata = data;

	if (event == PANEL_EVENT_ESD_STATE_CHANGED) {
		mutex_lock(&ts->lock);
		input_info(true, &ts->client->dev, "%s: PANEL_EVENT_ESD_STATE_CHANGED(#8), state(%d)\n", __func__, evtdata->state);
		ts->lcd_esd_recovery = 1;
		mutex_unlock(&ts->lock);

	} else if (event == PANEL_EVENT_PANEL_STATE_CHANGED) {
		input_dbg(false, &ts->client->dev, "%s: PANEL_EVENT_PANEL_STATE_CHANGED(#3), state = %d\n", __func__, evtdata->state);

		if (evtdata->state == PANEL_EVENT_PANEL_STATE_ON && ts->lcd_esd_recovery == 1) {
			input_info(true, &ts->client->dev, "%s: LCD ESD -> LCD ON , run esd recovery\n", __func__);
#if NVT_TOUCH_SUPPORT_HW_RST
			gpio_set_value(ts->reset_gpio, 1);
#endif
			mutex_lock(&ts->lock);
			nvt_update_firmware(ts->plat_data->firmware_name);
			nvt_check_fw_reset_state(RESET_STATE_REK);

			nvt_ts_mode_restore(ts);
			ts->lcd_esd_recovery = 0;
			mutex_unlock(&ts->lock);
		}

	} else if (event == PANEL_EVENT_UB_CON_STATE_CHANGED) {
		input_dbg(false, &ts->client->dev, "%s: PANEL_EVENT_UB_CON_STATE_CHANGED(#4), state = %d\n", __func__, evtdata->state);

		if (evtdata->state == PANEL_EVENT_UB_CON_STATE_DISCONNECTED) {
			input_info(true, &ts->client->dev, "%s: PANEL_EVENT_UB_CON_STATE_DISCONNECTED : disable irq & pin control\n", __func__);
			nvt_irq_enable(false);
			sec_input_pinctrl_configure(&ts->client->dev, false);
		}

	} else if (event != 0)
		input_dbg(false, &ts->client->dev, "%s: event = %ld, state = %d\n", __func__, event, evtdata->state);

	return 0;
}

#endif

int nvt_ts_set_charger_mode(struct device *dev, bool on)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
	u8 mode_cmd = 0;
	int ret;

	if (on)
		ts->plat_data->touch_functions |= CHARGER_MASK;
	else
		ts->plat_data->touch_functions &= ~CHARGER_MASK;

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: tsp ic is off\n", __func__);
		return -EIO;
	}

	if (!nvt_ts_lcd_power_check()) {
		input_err(true, &ts->client->dev, "%s: lcd is off\n", __func__);
		return -EIO;
	}

	if (mutex_lock_interruptible(&ts->lock)) {
		input_err(true, &ts->client->dev, "%s: another task is running\n",
				__func__);
		return -EBUSY;
	}

	if (ts->plat_data->touch_functions & CHARGER_MASK)
		mode_cmd = CHARGER_PLUG_AC;
	else
		mode_cmd = CHARGER_PLUG_OFF;

	ret = nvt_ts_mode_switch(ts, mode_cmd, false);
	if (ret < 0)
		input_err(true, &ts->client->dev, "failed to switch %s mode\n",
					(mode_cmd == CHARGER_PLUG_AC) ? "CHARGER_PLUG_AC" : "CHARGER_PLUG_OFF");
	else
		input_info(true, &ts->client->dev, "%s : %s done\n",
				__func__, ts->plat_data->touch_functions & CHARGER_MASK ? "attach" : "detach");

	mutex_unlock(&ts->lock);

	return ret;
}

static int nvt_ts_fw_update_on_probe(void)
{
	int32_t ret = 0;

	mutex_lock(&ts->lock);
	ts->fw_index = NVT_TSP_FW_IDX_BIN;
	ret = nvt_update_firmware(ts->plat_data->firmware_name);
	mutex_unlock(&ts->lock);
	if (ret) {
		input_err(true, &ts->client->dev, "%s : tsp fw update failed!\n", __func__);
		return ret;
	}

	/* Parsing criteria from dts */
	if (of_property_read_bool(ts->client->dev.of_node, "novatek,mp-support-dt")) {
		u8 mpcriteria[32] = { 0 };
		int pid;
		int return_val;

		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

		//---read project id---
		mpcriteria[0] = EVENT_MAP_PROJECTID;
		CTP_SPI_READ(ts->client, mpcriteria, 3);

		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

		pid = (mpcriteria[2] << 8) + mpcriteria[1];

		/*
		 * Parsing Criteria by Novatek PID
		 * The string rule is "novatek-mp-criteria-<nvt_pid>"
		 * nvt_pid is 2 bytes (show hex).
		 *
		 * Ex. nvt_pid = 500A
		 *	  mpcriteria = "novatek-mp-criteria-500A"
		 */
		snprintf(mpcriteria, sizeof(mpcriteria), "novatek-mp-criteria-%04X", pid);

		return_val = nvt_sec_mp_parse_dt(ts, mpcriteria);
		if (return_val)
			input_err(true, &ts->client->dev, "%s: failed to parse mp device tree\n", __func__);
	}
	return ret;
}

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static void nvt_notify_work(struct work_struct *work)
{
	int ret;

	mutex_lock(&ts->notify_work_lock);

	switch (ts->notify_type) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		ret = nvt_ts_set_spen_mode(ts, SPEN_MODE_ENABLE);
		input_info(true, &ts->client->dev, "%s: pen hover in detect, ret: %d\n", __func__, ret);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		ret = nvt_ts_set_spen_mode(ts, SPEN_MODE_DISABLE);
		input_info(true, &ts->client->dev, "%s: pen hover out detect, ret: %d\n", __func__, ret);
		break;
	default:
		input_info(true, &ts->client->dev, "%s: other notify is coming: %d\n", __func__, ts->notify_type);
		break;
	}

	mutex_unlock(&ts->notify_work_lock);
}

static int nvt_ts_input_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct nvt_ts_data *ts = container_of(n, struct nvt_ts_data, nvt_input_nb);

	ts->notify_type = data;
	cancel_delayed_work(&ts->notify_work);
	schedule_work(&ts->notify_work.work);

	return 0;
}
#endif

/*
 * Description:
 *	Novatek touchscreen driver probe function.
 *
 * return:
 *	Executive outcomes. 0---succeed. negative---failed
 */
static int32_t nvt_ts_probe(struct spi_device *client)
{
	int32_t ret = 0;
	struct sec_ts_plat_data *plat_data;
#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	static int deferred_flag;

	if (!deferred_flag) {
		deferred_flag = 1;
		input_info(true, &client->dev, "deferred_flag boot %s\n", __func__);
		return -EPROBE_DEFER;
	}
#endif

	input_info(true, &client->dev, "%s : start\n", __func__);

	ts = devm_kzalloc(&client->dev, sizeof(struct nvt_ts_data), GFP_KERNEL);
	if (ts == NULL)
		return -ENOMEM;

	ts->xbuf = devm_kzalloc(&client->dev, (NVT_TRANSFER_LEN + 1 + DUMMY_BYTES), GFP_KERNEL);
	if (ts->xbuf == NULL)
		return -ENOMEM;

	ts->rbuf = devm_kzalloc(&client->dev, NVT_READ_LEN, GFP_KERNEL);
	if (ts->rbuf == NULL)
		return -ENOMEM;

	if (client->dev.of_node) {
		plat_data = devm_kzalloc(&client->dev, sizeof(struct sec_ts_plat_data), GFP_KERNEL);
		if (!plat_data)
			return -ENOMEM;

		client->dev.platform_data = plat_data;

		ret = sec_input_parse_dt(&client->dev);
		if (ret) {
			input_err(true, &client->dev, "%s: Failed to parse dt\n", __func__);
			goto err_parse_dt_failed;
		}

		ret = nvt_parse_dt(ts, &client->dev);
		if (ret < 0) {
			input_err(true, &client->dev, "%s: Failed to parse dt(%d)\n", __func__, ret);
			goto err_parse_dt_failed;
		}
	} else {
		goto err_parse_dt_failed;
	}

	ts->client = client;
	ts->plat_data = plat_data;
	spi_set_drvdata(client, ts);

	ts->plat_data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(ts->plat_data->pinctrl))
		input_info(true, &ts->client->dev, "%s: could not get pinctrl\n", __func__);

	sec_input_pinctrl_configure(&client->dev, true);

	ret = nvt_regulator_init(ts);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: Failed to init regulator(%d)\n", __func__, ret);
		goto err_regulator_init;
	}

	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);

	//---prepare for spi parameter---
	if (ts->client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		input_err(true, &client->dev, "Full duplex not supported by master\n");
		ret = -EIO;
		goto err_ckeck_full_duplex;
	}

	ts->client->bits_per_word = 8;
	ts->client->mode = SPI_MODE_0;

	ret = spi_setup(ts->client);
	if (ret < 0) {
		input_err(true, &client->dev, "Failed to perform SPI setup\n");
		goto err_spi_setup;
	}

#if IS_ENABLED(CONFIG_MTK_SPI)
	/* old usage of MTK spi API */
	memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mt_chip_conf));
	ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

#if IS_ENABLED(CONFIG_SPI_MT65XX)
	/* new usage of MTK spi API */
	memcpy(&ts->spi_ctrl, &spi_ctrdata, sizeof(struct mtk_chip_config));
	ts->client->controller_data = (void *)&ts->spi_ctrl;
#endif

	input_info(true, &client->dev, "mode=%d, max_speed_hz=%d\n", ts->client->mode, ts->client->max_speed_hz);

	mutex_init(&ts->lock);
	mutex_init(&ts->irq_lock);
	mutex_init(&ts->xbuf_lock);
	mutex_init(&ts->plat_data->enable_mutex);
	mutex_init(&ts->notify_work_lock);
	snprintf(ts->plat_data->ic_vendor_name, sizeof(ts->plat_data->ic_vendor_name), "NO");

	if (ts->plat_data->support_vbus_notifier)
		ts->plat_data->set_charger_mode = nvt_ts_set_charger_mode;

	//---eng reset before TP_RESX high
	nvt_eng_reset();

#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	// need 10ms delay after POR(power on reset)
	msleep(20);

	//---check chip version trim---
	ret = nvt_ts_check_chip_ver_trim_loop();
	if (ret) {
		input_err(true, &client->dev, "chip is not identified\n");
		ret = -EINVAL;
		goto err_chipvertrim_failed;
	}

	ret = sec_input_device_register(&client->dev, ts);
	if (ret) {
		input_err(true, &client->dev, "failed to register input device, %d\n", ret);
		goto err_register_input_device;
	}

	ts->input_dev = ts->plat_data->input_dev;

	init_completion(&ts->plat_data->resume_done);
	INIT_DELAYED_WORK(&ts->work_print_info, nvt_print_info_work);
	INIT_DELAYED_WORK(&ts->work_read_info, nvt_read_info_work);
	INIT_DELAYED_WORK(&ts->notify_work, nvt_notify_work);
	ts->plat_data->sec_ws = wakeup_source_register(NULL, "TSP");

	ret = nvt_ts_sec_fn_init(ts);
	if (ret) {
		input_err(true, &client->dev, "failed to init for factory function\n");
		goto err_init_sec_fn;
	}

	//---set int-pin & request irq---
	client->irq = gpio_to_irq(plat_data->irq_gpio);
	if (client->irq) {
		plat_data->irq = client->irq;
		ret = devm_request_threaded_irq(&client->dev, client->irq, NULL, nvt_ts_work_func,
						IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT, NVT_SPI_NAME, ts);
		if (ret != 0) {
			input_err(true, &client->dev, "request irq failed. ret=%d\n", ret);
			goto err_int_request_failed;
		} else {
			nvt_irq_enable(false);
			input_err(true, &client->dev, "request irq %d succeed\n", client->irq);
		}
	}

	ret = nvt_ts_fw_update_on_probe();
	if (ret) {
		input_err(true, &client->dev, "nvt_ts_fw_update_on_probe failed. ret(%d)\n", ret);
		goto err_fw_update_failed;
	}

	input_info(true, &client->dev, "NVT_TOUCH_ESD_PROTECT is %d\n", NVT_TOUCH_ESD_PROTECT);
#if NVT_TOUCH_ESD_PROTECT
	INIT_DELAYED_WORK(&ts->nvt_esd_check_work, nvt_esd_check_func);
	ts->nvt_esd_check_wq = alloc_workqueue("nvt_esd_check_wq", WQ_MEM_RECLAIM, 1);
	if (!ts->nvt_esd_check_wq) {
		input_err(true, &client->dev, "nvt_esd_check_wq create workqueue failed\n");
		ret = -ENOMEM;
		goto err_create_nvt_esd_check_wq_failed;
	}
	queue_delayed_work(ts->nvt_esd_check_wq, &ts->nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	schedule_delayed_work(&ts->work_read_info, msecs_to_jiffies(100));

	//---set device node---
#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		input_err(true, &client->dev, "nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif

#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		input_err(true, &client->dev, "nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

	complete_all(&ts->plat_data->resume_done);

#if SEC_LPWG_DUMP
	nvt_ts_lpwg_dump_buf_init();
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	ts->lcd_nb.priority = 1;
	ts->lcd_nb.notifier_call = nvt_notifier_call;
	panel_notifier_register(&ts->lcd_nb);
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_tsp_init(nvt_stui_tsp_enter, nvt_stui_tsp_exit, nvt_stui_tsp_type);
	input_info(true, &client->dev, "secure touch support\n");
#endif

	ts->plat_data->dev = &ts->client->dev;
	ts->plat_data->bus_master = &ts->client->controller->dev;
	ts->plat_data->irq = client->irq;

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	mutex_init(&ts->secure_lock);
	if (sysfs_create_group(&ts->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);

	sec_secure_touch_register(ts,  &ts->client->dev, 1, &ts->input_dev->dev.kobj);
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	ret = sec_trusted_touch_init(&ts->client->dev);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to init trusted touch\n", __func__);
	}
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->nvt_input_nb, nvt_ts_input_notify_call, 1);
#endif
	sec_input_register_vbus_notifier(&client->dev);

	nvt_ts_mode_read(ts);

	nvt_irq_enable(true);
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	atomic_set(&ts->plat_data->enabled, 1);
#endif
	input_info(true, &client->dev, "%s : end\n", __func__);

	return 0;

#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
#if NVT_TOUCH_ESD_PROTECT
	if (ts->nvt_esd_check_wq) {
		cancel_delayed_work_sync(&ts->nvt_esd_check_work);
		destroy_workqueue(ts->nvt_esd_check_wq);
		ts->nvt_esd_check_wq = NULL;
	}
err_create_nvt_esd_check_wq_failed:
#endif
err_fw_update_failed:
err_int_request_failed:
	nvt_ts_sec_fn_remove(ts);
err_init_sec_fn:
	wakeup_source_unregister(ts->plat_data->sec_ws);
err_register_input_device:
err_chipvertrim_failed:
	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);
err_spi_setup:
err_ckeck_full_duplex:
err_regulator_init:
	spi_set_drvdata(client, NULL);
err_parse_dt_failed:
	input_err(true, &client->dev, "%s : end - fail unload driver\n", __func__);

	return ret;
}

/*
 * Description:
 *	Novatek touchscreen driver release function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 */
static int32_t nvt_ts_dev_remove(struct spi_device *client)
{
	input_info(true, &client->dev, "%s : Removing driver...\n", __func__);

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	panel_notifier_unregister(&ts->lcd_nb);
#endif
	sec_input_unregister_vbus_notifier(&client->dev);

	cancel_delayed_work_sync(&ts->work_print_info);

	nvt_ts_sec_fn_remove(ts);

#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

#if NVT_TOUCH_ESD_PROTECT
	if (ts->nvt_esd_check_wq) {
		cancel_delayed_work_sync(&ts->nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(ts->nvt_esd_check_wq);
		ts->nvt_esd_check_wq = NULL;
	}
#endif

	cancel_delayed_work_sync(&ts->work_read_info);

	nvt_irq_enable(false);
	sec_input_pinctrl_configure(&ts->client->dev, false);

	wakeup_source_unregister(ts->plat_data->sec_ws);
	mutex_destroy(&ts->xbuf_lock);
	mutex_destroy(&ts->lock);

	spi_set_drvdata(client, NULL);

	return 0;
}

#if (KERNEL_VERSION(5, 18, 0) <= LINUX_VERSION_CODE)
static void nvt_ts_remove(struct spi_device *client)
{
	nvt_ts_dev_remove(client);
}
#else
static int nvt_ts_remove(struct spi_device *client)
{
	nvt_ts_dev_remove(client);
	return 0;
}
#endif

static void nvt_ts_shutdown(struct spi_device *client)
{
	input_info(true, &client->dev, "%s : Shutdown driver...\n", __func__);

	if (ts == NULL) {
		input_err(true, &client->dev, "%s : tsp data null\n", __func__);
		return;
	}

	atomic_set(&ts->plat_data->shutdown_called, 1);

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	panel_notifier_unregister(&ts->lcd_nb);
#endif
	sec_input_unregister_vbus_notifier(&client->dev);

	cancel_delayed_work_sync(&ts->work_print_info);

#if NVT_TOUCH_ESD_PROTECT
	if (ts->nvt_esd_check_wq) {
		cancel_delayed_work_sync(&ts->nvt_esd_check_work);
		nvt_esd_check_enable(false);
		destroy_workqueue(ts->nvt_esd_check_wq);
		ts->nvt_esd_check_wq = NULL;
	}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	cancel_delayed_work_sync(&ts->work_read_info);

	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_OFF);

	nvt_irq_enable(false);
	sec_input_pinctrl_configure(&ts->client->dev, false);

	nvt_ts_sec_fn_remove(ts);
	wakeup_source_unregister(ts->plat_data->sec_ws);

#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
#endif

}


int nvt_ts_lcd_reset_ctrl(bool on)
{
	int retval;
	static bool enabled;

	if (enabled == on)
		return 0;

	if (ts->name_lcd_rst == NULL) {
		input_err(true, &ts->client->dev, "%s: name_lcd_rst is null\n", __func__);
		return 0;
	}

	if (on) {
		retval = regulator_enable(ts->regulator_lcd_rst);
		if (retval) {
			input_err(true, &ts->client->dev, "%s: Failed to enable regulator_lcd_rst: %d\n", __func__, retval);
			return retval;
		}

	} else {
		regulator_disable(ts->regulator_lcd_rst);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s %d done\n", __func__, on);

	return 0;
}

int nvt_ts_lcd_power_ctrl(bool on)
{
	int retval, i;
	static bool enabled;

	if (enabled == on)
		return 0;

	if (on) {
		for (i = 0; i < ts->regulator_count; i++) {
			retval = regulator_enable(ts->regulator[i]);
			if (retval) {
				input_err(true, &ts->client->dev, "%s: Failed to enable %s: %d\n",
						__func__, ts->regulator_name[i], retval);
				return retval;
			}
		}
	} else {
		for (i = ts->regulator_count - 1; i >= 0; i--)
			regulator_disable(ts->regulator[i]);
	}

	enabled = on;

	input_info(true, &ts->client->dev, "%s %d done\n", __func__, on);

	return 0;
}

bool nvt_ts_lcd_power_check(void)
{
	int enabled_count = 0, i;

	for (i = 0; i < ts->regulator_count; i++)
		enabled_count += regulator_is_enabled(ts->regulator[i]);

	if (ts->regulator_count > 0 && enabled_count == 0) {
		input_info(true, &ts->client->dev, "%s : regulator_count (%d), enabled_count zero(lcd off)\n",
					__func__, ts->regulator_count);
		return false;
	} else {
		return true;
	}
}

static void nvt_ts_set_lp_mode(struct nvt_ts_data *ts)
{
	uint8_t buf[4] = {0};

	nvt_ts_lcd_power_ctrl(true);
	nvt_ts_lcd_reset_ctrl(true);

	nvt_irq_enable(false);

	mutex_lock(&ts->lock);
	/* LPWG enter */
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = LPWG_ENTER;
	CTP_SPI_WRITE(ts->client, buf, 2);
	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_LPM);
	mutex_unlock(&ts->lock);

	nvt_irq_enable(true);
	enable_irq_wake(ts->client->irq);

	input_info(true, &ts->client->dev, "%s: called(%d)\n", __func__, ts->plat_data->lowpower_mode);
}

static void nvt_ts_set_icoff_mode(struct nvt_ts_data *ts)
{
	nvt_irq_enable(false);

	mutex_lock(&ts->lock);
	sec_input_pinctrl_configure(&ts->client->dev, false);
	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_OFF);
	mutex_unlock(&ts->lock);

	input_info(true, &ts->client->dev, "%s: power off %d\n", __func__, ts->plat_data->lowpower_mode);
}

void nvt_ts_set_sleep_mode(struct nvt_ts_data *ts)
{
	uint8_t buf[2];

	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = DEEP_SLEEP_ENTER;
	CTP_SPI_WRITE(ts->client, buf, 2);
	input_info(true, &ts->client->dev, "%s: deep sleep mode\n", __func__);
}

/*
 * Description:
 *	Novatek touchscreen driver suspend function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 */
int32_t nvt_ts_suspend(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
#if SEC_LPWG_DUMP
	u8 lpwg_dump[5] = {0x3, 0x0, 0x0, 0x0, 0x0};
#endif

	cancel_delayed_work_sync(&ts->work_read_info);

#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	atomic_set(&ts->plat_data->enabled, 0);
#endif

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	if (!sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWERON)) {
		input_info(true, &ts->client->dev, "%s: Touch is already suspend(%d)\n",
				__func__, atomic_read(&ts->plat_data->power_state));
		return 0;
	}

#if NVT_TOUCH_ESD_PROTECT
	input_info(true, &ts->client->dev, "cancel delayed work sync\n");
	cancel_delayed_work_sync(&ts->nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	input_info(true, &ts->client->dev, "%s : lp:%x, test:%d\n",
				__func__, ts->plat_data->lowpower_mode,
				ts->lcdoff_test);

	if (!sec_input_need_ic_off(ts->plat_data) || ts->lcdoff_test) {
		nvt_ts_set_lp_mode(ts);
#if SEC_LPWG_DUMP
		nvt_ts_lpwg_dump_buf_write(lpwg_dump);
#endif
	} else {
		nvt_ts_set_icoff_mode(ts);
		input_info(true, &ts->client->dev, "%s: lp:0x%02X\n",
				__func__, ts->plat_data->lowpower_mode);
	}

	sec_input_release_all_finger(&ts->client->dev);

	msleep(50);

	cancel_delayed_work(&ts->work_print_info);
	sec_input_print_info(&ts->client->dev, NULL);

	input_info(true, &ts->client->dev, "%s : end\n", __func__);

	return 0;
}

void nvt_ts_early_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, &ts->client->dev, "%s : start(%d)\n",
			__func__, atomic_read(&ts->plat_data->power_state));

	cancel_delayed_work_sync(&ts->work_read_info);

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return;
	}

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE)) {
		disable_irq_wake(ts->client->irq);

#if SEC_LPWG_DUMP
		input_info(true, &ts->client->dev, "%s : read lpgw logs start\n", __func__);
		nvt_ts_lpwg_dump();
		input_info(true, &ts->client->dev, "%s : read lpgw logs end\n", __func__);
#endif

		nvt_irq_enable(false);

		mutex_lock(&ts->lock);
		nvt_ts_lcd_reset_ctrl(false);
		mutex_unlock(&ts->lock);
	}
}

/*
 * Description:
 *	Novatek touchscreen driver resume function.
 *
 * return:
 *	Executive outcomes. 0---succeed.
 */
int32_t nvt_ts_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);
#if SEC_LPWG_DUMP
	u8 lpwg_dump[5] = {0x7, 0x0, 0x0, 0x0, 0x0};
#endif

	ts->lcd_esd_recovery = 0;
	cancel_delayed_work_sync(&ts->work_read_info);

	if (atomic_read(&ts->plat_data->shutdown_called)) {
		input_err(true, &ts->client->dev, "%s shutdown was called\n", __func__);
		return 0;
	}

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWERON)) {
		input_info(true, &ts->client->dev, "%s: Touch is already resume\n", __func__);
		return 0;
	}

	mutex_lock(&ts->lock);

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_LPMODE)) {
		nvt_ts_lcd_power_ctrl(false);
#if SEC_LPWG_DUMP
		nvt_ts_lpwg_dump_buf_write(lpwg_dump);
#endif
	} else {
		sec_input_pinctrl_configure(&ts->client->dev, true);
	}

	atomic_set(&ts->plat_data->power_state, SEC_INPUT_STATE_POWER_ON);

	atomic_set(&ts->plat_data->touch_noise_status, 0);
	ts->plat_data->noise_mode = 0;
	ts->plat_data->wet_mode = 0;

	// please make sure display reset(RESX) sequence and mipi dsi cmds sent before this
#if NVT_TOUCH_SUPPORT_HW_RST
	gpio_set_value(ts->reset_gpio, 1);
#endif

	if (nvt_update_firmware(ts->plat_data->firmware_name))
		input_err(true, &ts->client->dev, "download firmware failed, ignore check fw state\n");
	else
		nvt_check_fw_reset_state(RESET_STATE_REK);

	nvt_ts_mode_restore(ts);

#if NVT_TOUCH_ESD_PROTECT
	nvt_esd_check_enable(false);
	queue_delayed_work(ts->nvt_esd_check_wq, &ts->nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
#endif /* #if NVT_TOUCH_ESD_PROTECT */
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
	atomic_set(&ts->plat_data->enabled, 1);
#endif
	mutex_unlock(&ts->lock);

#if SEC_FW_STATUS
	nvt_ts_ic_status(NULL, true);
#endif

	nvt_irq_enable(true);

	cancel_delayed_work(&ts->work_print_info);
	ts->plat_data->print_info_cnt_open = 0;
	ts->plat_data->print_info_cnt_release = 0;
	schedule_work(&ts->work_print_info.work);

	input_info(true, &ts->client->dev, "%s end\n", __func__);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int nvt_pm_suspend(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	reinit_completion(&ts->plat_data->resume_done);

	return 0;
}

static int nvt_pm_resume(struct device *dev)
{
	struct nvt_ts_data *ts = dev_get_drvdata(dev);

	complete_all(&ts->plat_data->resume_done);

	return 0;
}
#endif

static const struct spi_device_id nvt_ts_id[] = {
	{ NVT_SPI_NAME, 0 },
	{ }
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id nvt_match_table[] = {
	{ .compatible = "nvt_ts_spi",},
	{ },
};
#endif
#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops nvt_dev_pm_ops = {
	.suspend = nvt_pm_suspend,
	.resume = nvt_pm_resume,
};
#endif

static struct spi_driver nvt_spi_driver = {
	.probe		= nvt_ts_probe,
	.remove		= nvt_ts_remove,
	.shutdown	= nvt_ts_shutdown,
	.id_table	= nvt_ts_id,
	.driver = {
		.name	= NVT_SPI_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = nvt_match_table,
#endif
#if IS_ENABLED(CONFIG_PM)
		.pm = &nvt_dev_pm_ops,
#endif
	},
};

/*
 * Description:
 *	Driver Install function.
 *
 * return:
 *	Executive Outcomes. 0---succeed. not 0---failed.
 */
static int32_t __init nvt_driver_init(void)
{
	int32_t ret = 0;

	pr_info("[sec_input] %s : start\n", __func__);

	//---add spi driver---
	ret = spi_register_driver(&nvt_spi_driver);
	if (ret) {
		pr_err("[sec_input] failed to add spi driver");
		goto err_driver;
	}

	pr_info("[sec_input] %s : finished\n", __func__);

err_driver:
	return ret;
}

/*
 * Description:
 *	Driver uninstall function.
 *
 * return:
 *	n.a.
 */
static void __exit nvt_driver_exit(void)
{
	spi_unregister_driver(&nvt_spi_driver);
}

module_init(nvt_driver_init);
module_exit(nvt_driver_exit);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
