/*
 *  s2p_adc.c - Support for ADC in SAMSUNG PMIC
 *
 *  n channels, n-bit ADC
 *
 *  Copyright (c) 2023 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/pmic/s2p_adc.h>

int s2p_read_raw(struct s2p_adc *info, struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	unsigned long channel = chan->address;
	int ret = 0;

	/* critical section start */
	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	if (channel >= info->max_channel || !info->data->read_data)
		return -EINVAL;

	mutex_lock(&info->lock);

	if (info->data->read_data(info, channel))
		pr_err("%s: read_data fail\n", __func__);
	else {
		*val = info->value;
		*val2 = 0;
		ret = IIO_VAL_INT;
	}

	mutex_unlock(&info->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_read_raw);

int s2p_adc_remove_devices(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);
	int ret = 0;

	platform_device_unregister(pdev);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_adc_remove_devices);

void s2p_adc_remove(struct iio_dev *indio_dev)
{
	device_for_each_child(&indio_dev->dev, NULL, s2p_adc_remove_devices);
}
EXPORT_SYMBOL_GPL(s2p_adc_remove);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("GP-ADC driver for common");
MODULE_LICENSE("GPL v2");

