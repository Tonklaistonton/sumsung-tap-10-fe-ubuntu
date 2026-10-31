//SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos5 SoC series EEPROM driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-sensor-eeprom-hm6.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

#define SENSOR_EEPROM_NAME "HM6"
static bool check_eeprom_data_read;

int is_eeprom_hm6_check_all_crc(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_eeprom *eeprom = NULL;

	FIMC_BUG(!subdev);

	eeprom = (struct is_eeprom *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!eeprom);


	return ret;
}

int is_eeprom_hm6_get_cal_data(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_eeprom *eeprom;
	struct i2c_client *client;
	char   header_ver[IS_HEADER_VER_SIZE + 1];
	u32 read_addr = 0x0;
	int cal_size = 0;

	FIMC_BUG(!subdev);

	eeprom = (struct is_eeprom *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!eeprom);

	client = eeprom->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}
	if (!check_eeprom_data_read) {
		cal_size = eeprom->total_size;
		ret = eeprom->ixc_ops->read8_size(client, &header_ver[0],
							EEPROM_HEADER_VERSION_START, IS_HEADER_VER_SIZE);
		if (ret < 0) {
			err("failed to is_i2c_read (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
		info("EEPROM header version = %s(%x%x%x%x)\n", header_ver,
			header_ver[0], header_ver[1], header_ver[2], header_ver[3]);

		ret = eeprom->ixc_ops->read8_size(client, &eeprom->data[0], read_addr, cal_size);
		if (ret < 0) {
			err("failed to is_i2c_read (%d)\n", ret);
			ret = -EINVAL;
			goto exit;
		}
		check_eeprom_data_read = true;
	}
exit:
	info("%s Done\n", __func__);

	return ret;
}

int is_eeprom_hm6_get_cal_buf(struct v4l2_subdev *subdev, char **buf)
{
	struct is_eeprom *eeprom;

	FIMC_BUG(!subdev);
	eeprom = (struct is_eeprom *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!eeprom);

	*buf = eeprom->data;
	if (*buf == NULL) {
		err("cal buf rom data is null.");
		return -EINVAL;
	}

	return 0;
}

static struct is_eeprom_ops sensor_eeprom_ops = {
	.eeprom_read = is_eeprom_hm6_get_cal_data,
	.eeprom_check_all_crc = is_eeprom_hm6_check_all_crc,
	// .eeprom_get_sensor_id = is_eeprom_get_sensor_id,
	.eeprom_get_cal_buf = is_eeprom_hm6_get_cal_buf,
};

DEFINE_I2C_DRIVER_PROBE(sensor_eeprom_hm6_probe_i2c)
{
	int ret = 0;
	int i;
	struct is_core *core;
	struct v4l2_subdev *subdev_eeprom = NULL;
	struct is_eeprom *eeprom = NULL;
	struct is_device_sensor *device;
	struct device *dev;
	struct device_node *dnode;
	u32 sensor_id = 0;
	u32 rom_size = 0;

	FIMC_BUG(!client);

	core = pablo_get_core_async();
	if (!core) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &client->dev;
	dnode = dev->of_node;
	ret = of_property_read_u32(dnode, "rom_size", &rom_size);
	if (ret) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}
	ret = of_property_read_u32(dnode, "id", &sensor_id);
	if (ret) {
		probe_info("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[sensor_id];
	if (!device) {
		err("sensor device is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	eeprom = devm_kzalloc(dev, sizeof(struct is_eeprom), GFP_KERNEL);
	if (!eeprom) {
		err("eeprom is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	eeprom->data = devm_kzalloc(dev, IS_MAX_CAL_SIZE, GFP_KERNEL);
	if (!eeprom->data) {
		err("data is NULL");
		ret = -ENOMEM;
		goto p_err_eeprom_data;
	}

	subdev_eeprom = devm_kzalloc(dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_eeprom) {
		err("subdev_eeprom is NULL");
		ret = -ENOMEM;
		goto p_err_eeprom_subdev;
	}

	eeprom->id = EEPROM_NAME_HM6;
	eeprom->subdev = subdev_eeprom;
	eeprom->device = sensor_id;
	eeprom->client = client;
	eeprom->ixc_lock = NULL;
	eeprom->total_size = rom_size;
	eeprom->eeprom_ops = &sensor_eeprom_ops;
	eeprom->ixc_ops = pablo_get_i2c();

	device->subdev_eeprom = subdev_eeprom;
	device->eeprom = eeprom;
	check_eeprom_data_read = false;

	for (i = 0; i < CAMERA_CRC_INDEX_MAX; i++)
		device->cal_status[i] = CRC_NO_ERROR;

	v4l2_set_subdevdata(subdev_eeprom, eeprom);
	v4l2_set_subdev_hostdata(subdev_eeprom, device);

	snprintf(subdev_eeprom->name, V4L2_SUBDEV_NAME_SIZE, "eeprom-subdev.%d", eeprom->id);

	probe_info("%s done\n", __func__);

	return ret;

p_err_eeprom_subdev:
	devm_kfree(dev, eeprom->data);

p_err_eeprom_data:
	devm_kfree(dev, eeprom);

p_err:
	return ret;
}

static const struct of_device_id sensor_eeprom_hm6_match[] = {
	{
		.compatible = "samsung,exynos-is-sensor-eeprom-hm6",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_eeprom_hm6_match);

static const struct i2c_device_id sensor_eeprom_hm6_idt[] = {
	{ SENSOR_EEPROM_NAME, 0 },
	{},
};

static struct i2c_driver sensor_eeprom_hm6_driver = {
	.probe  = sensor_eeprom_hm6_probe_i2c,
	.driver = {
		.name	= SENSOR_EEPROM_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_eeprom_hm6_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_eeprom_hm6_idt
};

static int __init sensor_eeprom_hm6_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_eeprom_hm6_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_eeprom_hm6_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_eeprom_hm6_init);

MODULE_LICENSE("GPL");
