/*
 *  s2rp910_adc.c - Support for ADC in s2rp910 PMIC
 *
 *  2 channels, 12-bit ADC
 *
 *  Copyright (c) 2024 Samsung Electronics Co., Ltd
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
#include <linux/pmic/s2rp910-mfd.h>

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#endif

/* GP-ADC setting */
#define ADC_MAX_CHANNELS	(2)

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
static int s2rp910_adc_read_data(struct s2p_adc *info, unsigned long chan)
{
	unsigned int channel_num = 0, size = 0, gpadc_data = 0;
	unsigned int command[4] = {0,};
	struct ipc_config config;
	struct device_node *esca_gpadc_node = info->gpadc_node;
	int ret = 0;

	if (!esca_ipc_request_channel(esca_gpadc_node, NULL, &channel_num, &size)) {
		config.cmd = command;
		config.cmd[0] = 0;
		config.cmd[1] = (((info->sdev->sid & 0xF) << 24) | chan);
		config.response = true;
		config.indirection = false;

		ret = esca_ipc_send_data(channel_num, &config);
		if (ret) {
			pr_err("%s: esca_ipc_send_data fail\n", __func__);
			goto err;
		}
		gpadc_data = (config.cmd[1] & ADC_DATA_MASK);
		if (gpadc_data == INVALID_ADC_DATA)
			pr_err("%s: Conversion timed out!\n", __func__);
		else if (gpadc_data == INVALID_SID_DATA)
			pr_err("%s: SID error!\n", __func__);

		info->value = gpadc_data;

		esca_ipc_release_channel(esca_gpadc_node, channel_num);
	} else {
		pr_err("%s: ipc request_channel fail, id:%u, size:%u\n",
			__func__, channel_num, size);
		ret = -EBUSY;
		goto err;
	}

	return ret;
err:
	info->value = INVALID_SEND_DATA;
	return ret;
}
#else
static int s2rp910_adc_read_data(struct s2p_adc *info, unsigned long chan)
{
	pr_info("%s: Need CONFIG_EXYNOS_ESCA configs\n", __func__);

	return -EINVAL;
}
#endif

static const struct s2p_adc_data s2rp910_adc_info = {
	.num_channels	= ADC_MAX_CHANNELS,
	.read_data	= s2rp910_adc_read_data,
};

static const struct of_device_id s2rp910_adc_match[] = {
	{
		.compatible = "s2rp910-gpadc",
		.data = &s2rp910_adc_info,
	},
	{},
};
MODULE_DEVICE_TABLE(of, s2rp910_adc_match);

static struct s2p_adc_data *s2rp910_adc_get_data(void)
{
	return (struct s2p_adc_data *)&s2rp910_adc_info;
}

static int s2rp910_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct s2p_adc *info = iio_priv(indio_dev);

	return s2p_read_raw(info, indio_dev, chan, val, val2, mask);
}

static const struct iio_info s2rp910_adc_iio_info = {
	.read_raw = &s2rp910_read_raw,
};

static const struct iio_chan_spec s2rp910_adc_iio_channels[] = {
	ADC_CHANNEL(0, "adc0"),
	ADC_CHANNEL(1, "adc1"),
};

static int s2rp910_adc_remove_devices(struct device *dev, void *c)
{
	int ret = 0;

	ret = s2p_adc_remove_devices(dev, c);

	return ret;
}

static int s2rp910_adc_parse_dt(struct s2rp910_dev *iodev, struct s2p_adc *info)
{
	struct device_node *mfd_np = NULL, *gpadc_np = NULL;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		goto err;
	}

	mfd_np = iodev->dev->of_node;

	gpadc_np = of_find_node_by_name(mfd_np, "s2rp910-gpadc");
	if (!gpadc_np) {
		pr_err("%s: could not find current_node\n", __func__);
		goto err;
	}

	info->gpadc_node = gpadc_np;

	return 0;
err:
	return -1;
}

static int s2rp910_adc_probe(struct platform_device *pdev)
{
	struct s2rp910_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2p_adc *info = NULL;
	struct iio_dev *indio_dev = NULL;
	int dev_type = iodev->sdev->device_type;
	int ret = -ENODEV;

	dev_info(&pdev->dev, "[RF%d_PMIC] %s: Start\n", dev_type + 1, __func__);

	indio_dev = devm_iio_device_alloc(&pdev->dev, (int)sizeof(struct s2p_adc));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);
	info->sdev = iodev->sdev;
	info->adc_addr = iodev->adc;
	info->dev = &pdev->dev;
	info->max_channel = ADC_MAX_CHANNELS;
	info->data = s2rp910_adc_get_data();

	ret = s2rp910_adc_parse_dt(iodev, info);
	if (ret < 0) {
		pr_err("%s: s2rp910_adc_parse_dt fail\n", __func__);
		return -ENODEV;
	}

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, indio_dev);

	indio_dev->name = dev_name(&pdev->dev);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = info->gpadc_node;
	indio_dev->info = &s2rp910_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = s2rp910_adc_iio_channels;
	indio_dev->num_channels = info->data->num_channels;

	ret = devm_iio_device_register(info->dev, indio_dev);
	if (ret)
		goto err_mutex;

	ret = of_platform_populate(info->gpadc_node, s2rp910_adc_match, NULL, &indio_dev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed adding child nodes\n", __func__);
		goto err_of_populate;
	}

	dev_info(&pdev->dev, "[RF%d_PMIC] %s: End\n", dev_type + 1, __func__);

	return 0;

err_of_populate:
	device_for_each_child(&indio_dev->dev, NULL, s2rp910_adc_remove_devices);
err_mutex:
	mutex_destroy(&info->lock);

	return ret;
}

static int s2rp910_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct s2p_adc *info = iio_priv(indio_dev);

	mutex_destroy(&info->lock);

	s2p_adc_remove(indio_dev);

	return 0;
}

static const struct platform_device_id s2rp910_pmic_id[] = {
	{ "s2rp910-1-gpadc", TYPE_S2RP910_1},
	{ "s2rp910-2-gpadc", TYPE_S2RP910_2},
	{ },
};
MODULE_DEVICE_TABLE(platform, s2rp910_pmic_id);

static struct platform_driver s2rp910_adc_driver = {
	.probe		= s2rp910_adc_probe,
	.remove		= s2rp910_adc_remove,
	.driver		= {
		.name	= "s2rp910-gpadc",
		.of_match_table = s2rp910_adc_match,
	},
	.id_table = s2rp910_pmic_id,
};

module_platform_driver(s2rp910_adc_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("GP-ADC driver for s2rp910");
MODULE_LICENSE("GPL v2");
