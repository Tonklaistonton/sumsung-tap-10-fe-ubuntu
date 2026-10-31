#ifndef __S2P_ADC_H
#define __S2P_ADC_H

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/input.h>
#include <linux/iio/iio.h>

#include <linux/pmic/s2p.h>

#define ADC_DATA_MASK		(0xFFFF)
#define INVALID_ADC_DATA	(0xFFFF)
#define INVALID_SID_DATA	(0xFFFE)
#define INVALID_SEND_DATA	(0xFFFD)

#define ADC_CHANNEL(_index, _id) {			\
	.type = IIO_VOLTAGE,				\
	.indexed = 1,					\
	.channel = _index,				\
	.address = _index,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
	.datasheet_name = _id,				\
}

struct s2p_adc {
	struct s2p_adc_data	*data;
	struct device		*dev;
	struct s2p_dev *sdev;
	uint16_t 	adc_addr;
	int max_channel;
	struct device_node	*gpadc_node;
	uint32_t		value;
	struct mutex lock;
};

struct s2p_adc_data {
	int num_channels;

	int (*read_data)(struct s2p_adc *info, unsigned long chan);
};

extern int s2p_read_raw(struct s2p_adc *info, struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask);
extern int s2p_adc_remove_devices(struct device *dev, void *c);
extern void s2p_adc_remove(struct iio_dev *indio_dev);
#endif /* __S2P_ADC_H */
