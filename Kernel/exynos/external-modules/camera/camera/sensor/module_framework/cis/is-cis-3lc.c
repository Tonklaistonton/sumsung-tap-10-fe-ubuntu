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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <videodev2_exynos_camera.h>
#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-cis-3lc.h"
#include "is-cis-3lc-setA-19p2.h"
#include "is-helper-ixc.h"

#define SENSOR_NAME "S5K3LC"

int sensor_3lc_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;

	cis->cis_data->sens_config_index_pre = SENSOR_3LC_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

void sensor_3lc_cis_set_mode_group(u32 mode)
{
	sensor_3lc_mode_groups[SENSOR_3LC_MODE_NORMAL] = mode;
	sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4] = MODE_GROUP_NONE;
	sensor_3lc_mode_groups[SENSOR_3LC_MODE_DSG] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_3LC_MODE_4000x3000_30FPS:
		sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4] =
			SENSOR_3LC_MODE_4000X3000_10FPS_LN4;
		break;
	case SENSOR_3LC_MODE_4000x2256_30FPS:
		sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4] =
			SENSOR_3LC_MODE_4000X2256_10FPS_LN4;
		break;
	case SENSOR_3LC_MODE_4000x3000_30FPS_R12:
		sensor_3lc_mode_groups[SENSOR_3LC_MODE_DSG] =
			SENSOR_3LC_MODE_4000x3000_30FPS_R12;
		break;
	case SENSOR_3LC_MODE_4000x2256_30FPS_R12:
		sensor_3lc_mode_groups[SENSOR_3LC_MODE_DSG] =
			SENSOR_3LC_MODE_4000x2256_30FPS_R12;
		break;
	}

	info("[%s] normal(%d) LN4(%d) DSG(%d)\n", __func__,
		sensor_3lc_mode_groups[SENSOR_3LC_MODE_NORMAL],
		sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4],
		sensor_3lc_mode_groups[SENSOR_3LC_MODE_DSG]);
}

static const struct is_cis_log log_3lc[] = {
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x0340, 0, "FLL"},
	{I2C_READ, 16, 0x0202, 0, "CIT"},
	{I2C_READ, 16, 0x0B30, 0, "FCM"},
	{I2C_WRITE, 16, 0xFCFC, 0x2000, "0x2000 page"},
	{I2C_READ, 16, 0x8CE0, 0, "model_id"},
	{I2C_READ, 16, 0x8CE2, 0, "rev_number"},
	{I2C_READ, 8, 0x8CE5, 0, "frame_count"},	
	{I2C_READ, 16, 0x8CE6, 0, "0x0100"},
	{I2C_READ, 16, 0x8CEA, 0, "FLL"},
	{I2C_READ, 16, 0x8CE8, 0, "CIT"},
	{I2C_READ, 16, 0x8CEC, 0, "FCM"},
	{I2C_READ, 16, 0x5B80 , 0, "0x5B80"},
	{I2C_READ, 16, 0x5B82 , 0, "0x5B82"},
	{I2C_READ, 16, 0x5B84 , 0, "0x5B84"},
	{I2C_READ, 16, 0x5B86 , 0, "0x5B86"},
	{I2C_READ, 16, 0x5B88 , 0, "0x5B88"},
	{I2C_READ, 16, 0x5B8A , 0, "0x5B8A"},
	{I2C_READ, 16, 0x5B8C , 0, "0x5B8C"},
	{I2C_READ, 16, 0x5B8E , 0, "0x5B8E"},	
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
};

int sensor_3lc_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_3lc, ARRAY_SIZE(log_3lc), (char *)__func__);

	return ret;
}

int sensor_3lc_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lc_private_data *priv = (struct sensor_3lc_private_data *)cis->sensor_info->priv;

	info("[%s] start\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	info("[%s] done\n", __func__);

	ret = sensor_cis_write_registers_locked(subdev, priv->prepare_fcm); //prepare FCM settings
	if (ret < 0)
		err("prepare_fcm fail!!");

	info("[%s] prepare_fcm done\n", __func__);

	return ret;
}

int sensor_3lc_cis_check_lownoise(cis_shared_data *cis_data, u32 *next_mode)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if (sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4] == MODE_GROUP_NONE) {
		ret = -1;
		goto EXIT;
	}

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN4:
		temp_mode = sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4];
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

int sensor_3lc_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;

	if (sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_3lc_mode_groups[SENSOR_3LC_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_3lc_mode_groups[SENSOR_3LC_MODE_DSG] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT;
		sensor_cis_get_mode_info(subdev, sensor_3lc_mode_groups[SENSOR_3LC_MODE_DSG],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;

	return ret;
}

int sensor_3lc_cis_update_seamless_mode(struct v4l2_subdev *subdev)
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

	next_mode = sensor_3lc_mode_groups[SENSOR_3LC_MODE_NORMAL];
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

	sensor_3lc_cis_check_lownoise(cis->cis_data, &next_mode);

	if ((mode == next_mode) || (next_mode == MODE_GROUP_NONE))
		return ret;

	next_mode_info = cis->sensor_info->mode_infos[next_mode];
	if (next_mode_info->setfile_fcm.size == 0) {
		err("check the fcm setting for mode(%d)", next_mode);
		return ret;
	}

	dbg_sensor(1, "[%s][%d] mode(%d) changing to next_mode(%d)",
		__func__, cis->cis_data->sen_vsync_count, mode, next_mode);
	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret |= sensor_cis_write_registers(subdev, next_mode_info->setfile_fcm);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

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
	return ret;
}

int sensor_3lc_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lc_private_data *priv =
		(struct sensor_3lc_private_data *)cis->sensor_info->priv;

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	mode_info = cis->sensor_info->mode_infos[mode];

	if (mode_info->setfile_fcm.size != 0) {
		info("[%s] valid fcm mode, write sram for mode:(%d)\n", __func__, mode);
		ret = sensor_cis_write_registers_locked(subdev, priv->load_sram[mode]);
		ret |= sensor_cis_write_registers_locked(subdev, mode_info->setfile);
		ret |= sensor_cis_write_registers_locked(subdev, mode_info->setfile_fcm);
	} else
		ret |= sensor_cis_write_registers_locked(subdev, mode_info->setfile);

	if (ret < 0)
		err("mode(%d) setting fail!!", mode);

	sensor_3lc_cis_set_mode_group(mode);
	sensor_3lc_cis_get_seamless_mode_info(subdev);

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

	info("[%s] mode changed(%d)\n", __func__, mode);

p_err:
	return ret;
}

int sensor_3lc_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	/* update mipi rate */
	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->write8(cis->client, 0x0105, 0x01);
	/* Sensor stream on */
	ret |= cis->ixc_ops->write16(cis->client, 0x0100, 0x0103);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis_data->stream_on = true;
	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3lc_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 cur_frame_count = 0;
	u8 hidden_cur_frame_count = 0;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
	cis->ixc_ops->read8(cis->client, 0x8CE5, &hidden_cur_frame_count);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);

	/* Sensor stream off */
	ret = cis->ixc_ops->write8(cis->client, 0x0100, 0x00);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = false;

	info("%s done, frame_count(%d:%d) \n", __func__, cur_frame_count, hidden_cur_frame_count);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3lc_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write16(cis->client, 0x0600, 0x0000);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write16(cis->client, 0x0600, 0x0001);
			cis->ixc_ops->write16(cis->client, 0x0602, 0x0000);
			cis->ixc_ops->write16(cis->client, 0x0604, 0x0000);
			cis->ixc_ops->write16(cis->client, 0x0606, 0x0000);
			cis->ixc_ops->write16(cis->client, 0x0608, 0x0000);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);

			cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		}
	}

	return ret;
}

int sensor_3lc_cis_init_state(struct v4l2_subdev *subdev, int mode)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->stream_on = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	return 0;
}

int sensor_3lc_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0, i2c_fail_max_cnt = 5;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
	ret = cis->ixc_ops->read8(cis->client, 0x8CE5, &sensor_fcount);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x8CE5, sensor_fcount, ret);

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x40000005 or 0x20008CE5)
	 * stream on (0x00 ~ 0xFE), stream off (0xFF)
	 */
	while (sensor_fcount != 0xFF) {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read8(cis->client, 0x8CE5, &sensor_fcount);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(%x), val(%x), try(%d), ret = %d\n",
				0x8CE5, sensor_fcount, i2c_fail_cnt, ret);

			if (i2c_fail_cnt >= i2c_fail_max_cnt) {
				err("[MOD:D:%d] %s, i2c fail, i2c_fail_cnt(%d) >= i2c_fail_max_cnt(%d), sensor_fcount(%d)",
						cis->id, __func__, i2c_fail_cnt, i2c_fail_max_cnt, sensor_fcount);
				ret = -EINVAL;
				goto p_err;
			}
		}
#if defined(USE_RECOVER_I2C_TRANS)
		if (i2c_fail_cnt >= USE_RECOVER_I2C_TRANS) {
			sensor_cis_recover_i2c_fail(subdev);
			err("[mod:d:%d] %s, i2c transfer, forcely power down/up",
				cis->id, __func__);
			ret = -EINVAL;
			goto p_err;
		}
#endif
		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME + 10);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}

p_err:
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	return ret;
}

static struct is_cis_ops cis_ops_3lc = {
	.cis_init = sensor_3lc_cis_init,
	.cis_init_state = sensor_cis_init_state,
	.cis_log_status = sensor_3lc_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_3lc_cis_set_global_setting,
	.cis_mode_change = sensor_3lc_cis_mode_change,
	.cis_stream_on = sensor_3lc_cis_stream_on,
	.cis_stream_off = sensor_3lc_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_3lc_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_cis_calc_again_code,
	.cis_calc_again_permile = sensor_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_set_test_pattern = sensor_3lc_cis_set_test_pattern,
	.cis_update_seamless_mode = sensor_3lc_cis_update_seamless_mode,
	.cis_update_seamless_history = sensor_cis_update_seamless_history,
	.cis_seamless_ctl_before_stream = sensor_cis_seamless_ctl_before_stream,
	.cis_wait_seamless_update_delay = sensor_cis_wait_seamless_update_delay,
};

DEFINE_I2C_DRIVER_PROBE(cis_3lc_probe_i2c)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	probe_info("%s: sensor_cis_probe started\n", __func__);
	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_3lc;

	/* below values depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GB_RG;
	cis->reg_addr = &sensor_3lc_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;
	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
			cis->sensor_info = &sensor_3lc_info_A;
		}
	}

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);

	return ret;
}

static const struct of_device_id sensor_is_cis_3lc_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-3lc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_is_cis_3lc_match);

static const struct i2c_device_id sensor_cis_3lc_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_3lc_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_is_cis_3lc_match,
	},
	.probe	= cis_3lc_probe_i2c,
	.id_table = sensor_cis_3lc_idt
};

#ifdef MODULE
module_driver(sensor_cis_3lc_driver, i2c_add_driver,
	i2c_del_driver)
#else
static int __init sensor_cis_3lc_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_3lc_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_3lc_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_3lc_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
