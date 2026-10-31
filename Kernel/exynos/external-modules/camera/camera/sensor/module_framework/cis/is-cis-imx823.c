/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/syscalls.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-cis-imx823.h"
#include "is-cis-imx823-setA-19p2.h"

#include "is-helper-ixc.h"
#include "interface/is-interface-library.h"

#define SENSOR_NAME "IMX823"

u32 sensor_imx823_cis_calc_again_code(u32 permille)
{
	return (16384*10 - (16384000*10 / permille))/10;
}

u32 sensor_imx823_cis_calc_again_permile(u32 code)
{
	return 16384000 / (16384 - code);
}

int sensor_imx823_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->cis_data->sens_config_index_pre = SENSOR_IMX823_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

void sensor_imx823_cis_set_mode_group(u32 mode)
{
	sensor_imx823_mode_groups[SENSOR_IMX823_MODE_NORMAL] = mode;
	sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_IMX823_MODE_4000x3000_30FPS:
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4] =
			SENSOR_IMX823_MODE_4000X3000_10FPS_LN4;
		break;
	case SENSOR_IMX823_MODE_4000x2256_30FPS:
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4] =
			SENSOR_IMX823_MODE_4000X2256_10FPS_LN4;
		break;
	case SENSOR_IMX823_MODE_3184x2388_30FPS:
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4] =
			SENSOR_IMX823_MODE_3184X2388_10FPS_LN4;
		break;
	case SENSOR_IMX823_MODE_3184x1792_30FPS:
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4] =
			SENSOR_IMX823_MODE_3184X1792_10FPS_LN4;
		break;
	case SENSOR_IMX823_MODE_4000x3000_30FPS_R12:
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_DSG] =
			SENSOR_IMX823_MODE_4000x3000_30FPS_R12;
		break;
	case SENSOR_IMX823_MODE_4000x2256_30FPS_R12:
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_DSG] =
			SENSOR_IMX823_MODE_4000x2256_30FPS_R12;
		break;
	}

	info("[%s] normal(%d) LN4(%d)\n", __func__,
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_NORMAL],
		sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4]);
}

static const struct is_cis_log log_imx823[] = {
	{I2C_READ, 16, 0x0016, 0, "model_id"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
	{I2C_READ, 8, 0x0830, 0, "0x0830"},
	{I2C_READ, 8, 0x0832, 0, "0x0832"},
	{I2C_READ, 8, 0x0808, 0, "0x0808"},
	{I2C_READ, 16, 0x080A, 0, "0x080A"},
	{I2C_READ, 16, 0x080C, 0, "0x080C"},
	{I2C_READ, 16, 0x080E, 0, "0x080E"},
	{I2C_READ, 16, 0x0810, 0, "0x0810"},
	{I2C_READ, 16, 0x0812, 0, "0x0812"},
	{I2C_READ, 16, 0x0814, 0, "0x0814"},
	{I2C_READ, 16, 0x0816, 0, "0x0816"},
	{I2C_READ, 16, 0x0818, 0, "0x0818"},
	{I2C_READ, 16, 0x0824, 0, "0x0824"},
	{I2C_READ, 16, 0x0826, 0, "0x0826"},
};

int sensor_imx823_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_imx823, ARRAY_SIZE(log_imx823), (char *)__func__);

	return ret;
}

int sensor_imx823_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_imx823_private_data *priv = NULL;

	priv = (struct sensor_imx823_private_data *)cis->sensor_info->priv;

	info("[%s] start\n", __func__);
	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0) {
		err("global setting fail!!");
		goto p_err;
	}

	info("[%s] done\n", __func__);

p_err:
	return ret;
}

int sensor_imx823_cis_check_lownoise(cis_shared_data *cis_data, u32 *next_mode)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if (sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN4:
		temp_mode = sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4];
		break;
	case IS_CIS_LN2:
	case IS_CIS_LNOFF:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE)
		ret = -1;

	if (ret == 0)
		*next_mode = temp_mode;
EXIT:
	return ret;
}

int sensor_imx823_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;

	if (sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_imx823_mode_groups[SENSOR_IMX823_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_imx823_mode_groups[SENSOR_IMX823_MODE_DSG] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT;
		sensor_cis_get_mode_info(subdev, sensor_imx823_mode_groups[SENSOR_IMX823_MODE_DSG],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}
	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_imx823_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	unsigned int next_mode = 0;
	cis_shared_data *cis_data;
	const struct sensor_cis_mode_info *next_mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;
	mode = cis_data->sens_config_index_cur;
	next_mode = mode;

	next_mode = sensor_imx823_mode_groups[SENSOR_IMX823_MODE_NORMAL];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		return -EINVAL;
	}

	if (cis->cis_data->stream_on == false
		&& cis->cis_data->seamless_ctl_before_stream.mode == 0) {
		info("[%s] skip update seamless mode in stream off\n", __func__);
		return 0;
	}

	cis->cis_data->seamless_ctl_before_stream.mode = 0;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	sensor_imx823_cis_check_lownoise(cis->cis_data, &next_mode);

	if ((mode == next_mode) || (next_mode == MODE_GROUP_NONE))
		goto p_i2c_unlock;

	next_mode_info = cis->sensor_info->mode_infos[next_mode];
	if (next_mode_info->setfile_fcm.size == 0) {
		err("check the fcm setting for mode(%d)", next_mode);
		goto p_i2c_unlock;
	}

	dbg_sensor(1, "[%s][%d] mode(%d) changing to next_mode(%d)",
		__func__, cis->cis_data->sen_vsync_count, mode, next_mode);
	ret |= sensor_cis_write_registers(subdev, next_mode_info->setfile_fcm);

	cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis->cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	info("[%s][%d] pre(%d)->cur(%d), 12bit[%d] LN[%d] AEB[%d] ZOOM[%d], time %lldus\n",
		__func__, cis->cis_data->sen_vsync_count,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_hdr_mode,
		cis_data->cur_remosaic_zoom_ratio,
		PABLO_KTIME_US_DELTA_NOW(st));

	cis->cis_data->seamless_mode_changed = true;

p_i2c_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_imx823_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	const struct sensor_cis_mode_info *mode_info;

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	info("[%s] sensor mode(%d)\n", __func__, mode);

	mode_info = cis->sensor_info->mode_infos[mode];

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_imx823_set_registers fail!!");
		goto p_err;
	}

	/* Disable Embedded Data */
	ret = cis->ixc_ops->write8(cis->client, 0x3970, 0x00);
	if (ret < 0)
		err("disable emb data fail");

	sensor_imx823_cis_set_mode_group(mode);
	sensor_imx823_cis_get_seamless_mode_info(subdev);

	cis->cis_data->sens_config_index_pre = mode;
	cis->cis_data->remosaic_mode = mode_info->remosaic_mode;
	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->pre_12bit_mode = mode_info->state_12bit;
	cis->cis_data->pre_remosaic_zoom_ratio = 0;

	if (cis->cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_LN2)
		cis->cis_data->cur_lownoise_mode = IS_CIS_LN2;
	else if (cis->cis_data->seamless_ctl_before_stream.mode & SENSOR_MODE_LN4)
		cis->cis_data->cur_lownoise_mode = IS_CIS_LN4;
	else
		cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

p_err:
	info("[%s] X\n", __func__);
	return ret;
}

int sensor_imx823_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	cis->ixc_ops->write8(cis->client, 0x0830, 0x00); /* disable the periodic skew */

	/* Sensor stream on */
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	cis->cis_data->stream_on = true;

	info("%s\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx823_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 cur_frame_count = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	info("%s: frame_count(0x%x)\n", __func__, cur_frame_count);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_imx823_cis_init,
	.cis_init_state = sensor_cis_init_state,
	.cis_log_status = sensor_imx823_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_imx823_cis_set_global_setting,
	.cis_mode_change = sensor_imx823_cis_mode_change,
	.cis_stream_on = sensor_imx823_cis_stream_on,
	.cis_stream_off = sensor_imx823_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_set_long_term_exposure = sensor_cis_long_term_exposure,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_imx823_cis_calc_again_code,
	.cis_calc_again_permile = sensor_imx823_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_update_seamless_mode = sensor_imx823_cis_update_seamless_mode,
	.cis_update_seamless_history = sensor_cis_update_seamless_history,
	.cis_seamless_ctl_before_stream = sensor_cis_seamless_ctl_before_stream,
	.cis_wait_seamless_update_delay = sensor_cis_wait_seamless_update_delay,
};

DEFINE_I2C_DRIVER_PROBE(cis_imx823_probe_i2c)
{
	int ret;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_BG_GR;
	cis->reg_addr = &sensor_imx823_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("%s setfile_A\n", __func__);
	else
		err("%s setfile index out of bound, take default (setfile_A)", __func__);

	cis->sensor_info = &sensor_imx823_info_A;
	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);
	return ret;
}

PKV_DEFINE_I2C_DRIVER_REMOVE(cis_imx823_remove_i2c)
{
	PKV_DEFINE_I2C_DRIVER_REMOVE_RETURN;
}

static const struct of_device_id sensor_cis_imx823_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx823",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx823_match);

static const struct i2c_device_id sensor_cis_imx823_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx823_driver = {
	.probe	= cis_imx823_probe_i2c,
	.remove	= cis_imx823_remove_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx823_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx823_idt
};

#ifdef MODULE
module_driver(sensor_cis_imx823_driver, i2c_add_driver,
	i2c_del_driver);
#else
static int __init sensor_cis_imx823_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx823_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx823_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx823_init);
#endif
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
