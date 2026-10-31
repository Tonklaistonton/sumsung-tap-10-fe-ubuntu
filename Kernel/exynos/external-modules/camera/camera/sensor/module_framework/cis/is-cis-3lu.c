/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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
#include "is-cis-3lu.h"
#include "is-cis-3lu-setA-19p2.h"
#include "is-helper-ixc.h"

#define SENSOR_NAME "S5K3LU"
#define REG_W_RETRY_CNT 3

int sensor_3lu_cis_set_global_setting(struct v4l2_subdev *subdev);
int sensor_3lu_cis_wait_streamoff(struct v4l2_subdev *subdev);

void sensor_3lu_cis_set_mode_group(u32 mode)
{
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_DEFAULT] = mode;
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] = MODE_GROUP_NONE;
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4] = MODE_GROUP_NONE;
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_AEB] = MODE_GROUP_NONE;

	switch (mode) {
	case SENSOR_3LU_4000X3000_30FPS_DSG:
	case SENSOR_3LU_4000X3000_30FPS_ADC:
	case SENSOR_3LU_4000X3000_30FPS_12BIT_LN2:
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG] = SENSOR_3LU_4000X3000_30FPS_DSG;
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] = SENSOR_3LU_4000X3000_30FPS_12BIT_LN2;
		break;
	case SENSOR_3LU_4000X2252_30FPS_DSG:
	case SENSOR_3LU_4000X2252_60FPS_ADC:
	case SENSOR_3LU_4000X2252_30FPS_12BIT_LN2:
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG] = SENSOR_3LU_4000X2252_30FPS_DSG;
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] = SENSOR_3LU_4000X2252_30FPS_12BIT_LN2;
		break;
	case SENSOR_3LU_4000X2252_60FPS_10BIT:
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_AEB] = SENSOR_3LU_4000X2252_60FPS_AEB;
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] = SENSOR_3LU_4000X2252_30FPS_10BIT_LN2;
		break;
	case SENSOR_3LU_4000X3000_60FPS_10BIT:
	case SENSOR_3LU_4000X3000_60FPS_AEB:
	case SENSOR_3LU_4000X3000_15FPS_10BIT_LN4:
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_AEB] = SENSOR_3LU_4000X3000_60FPS_AEB;
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4] = SENSOR_3LU_4000X3000_15FPS_10BIT_LN4;
		break;
	}

	info("[%s] default(%d) aeb(%d) idcg(%d) ln2(%d) ln4(%d)\n", __func__,
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_DEFAULT],
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_AEB],
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG],
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2],
		sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4]);
}

#ifdef USE_CAMERA_EMBEDDED_HEADER
#define SENSOR_3LU_VALID_TAG 0x5A
#define SENSOR_3LU_FRAME_ID_PAGE 0
#define SENSOR_3LU_FRAME_ID_OFFSET 26

static u32 frame_id_idx = SENSOR_3LU_FRAME_ID_PAGE + SENSOR_3LU_FRAME_ID_OFFSET;

static int sensor_3lu_cis_get_frame_id(struct v4l2_subdev *subdev, u8 *embedded_buf, u16 *frame_id)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (embedded_buf[frame_id_idx-1] == SENSOR_3LU_VALID_TAG) {
		*frame_id = embedded_buf[frame_id_idx];
		dbg_sensor(1, "frame_id(0x%x)", *frame_id);
	} else {
		err("invalid valid tag(0x%x)", embedded_buf[frame_id_idx-1]);
		*frame_id = 1;
	}

	cis->cis_data->sen_frame_id = *frame_id;

	return ret;
}
#endif

int sensor_3lu_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	cis_setting_info setinfo;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lu_private_runtime *priv_runtime = (struct sensor_3lu_private_runtime *)cis->priv_runtime;

	setinfo.param = NULL;
	setinfo.return_value = 0;

#if !defined(CONFIG_CAMERA_VENDOR_MCD)
	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = sensor_cis_check_rev(cis);
	if (ret < 0) {
		warn("sensor_3lu_check_rev is fail when cis init");
		return -EINVAL;
	}
#endif

	priv_runtime->seamless_delay_flag = false;

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->cis_data->remosaic_mode = false;
	cis->cis_data->pre_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->cur_12bit_mode = SENSOR_12BIT_STATE_OFF;
	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	sensor_3lu_mode_groups[SENSOR_3LU_MODE_DEFAULT] = MODE_GROUP_NONE;
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG] = MODE_GROUP_NONE;
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] = MODE_GROUP_NONE;
	sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4] = MODE_GROUP_NONE;

	cis->cis_data->sens_config_index_pre = SENSOR_3LU_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

static const struct is_cis_log log_3lu[] = {
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	/* 4000 page */
	{I2C_WRITE, 16, 0x6000, 0x0005, "page unlock"},
	{I2C_WRITE, 16, 0xFCFC, 0x4000, "0x4000 page"},
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 16, 0x0002, 0, "revision_number"},
	{I2C_READ, 16, 0x0100, 0, "0x0100"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x0702, 0, "0x0702"},
	{I2C_READ, 16, 0x0704, 0, "0x0704"},
	{I2C_READ, 16, 0x0340, 0, "0x0340"},
	{I2C_READ, 16, 0x0342, 0, "0x0342"},
	{I2C_READ, 16, 0x0344, 0, "0x0344"},
	{I2C_READ, 16, 0x0346, 0, "0x0346"},
	{I2C_READ, 16, 0x0348, 0, "0x0348"},
	{I2C_READ, 16, 0x034A, 0, "0x034A"},
	{I2C_READ, 16, 0x034C, 0, "0x034C"},
	{I2C_READ, 16, 0x034E, 0, "0x034E"},
	{I2C_READ, 16, 0x0350, 0, "0x0350"},
	{I2C_READ, 16, 0x0352, 0, "0x0352"},
	{I2C_READ, 16, 0x0B30, 0, "0x0B30"},
	{I2C_WRITE, 16, 0x6000, 0x0085, "page lock"},
};

int sensor_3lu_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_3lu, ARRAY_SIZE(log_3lu), (char *)__func__);

	return ret;
}

int sensor_3lu_cis_set_global_setting_internal(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u32 retry = REG_W_RETRY_CNT;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lu_private_data *priv = (struct sensor_3lu_private_data *)cis->sensor_info->priv;
	struct is_device_sensor *device;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (device->ex_scenario == IS_SCENARIO_SECURE && cis->cis_data->cis_rev == LATEST_3LU_REV) {
		info("[%s] sensor_3lu FD mode setting\n", __func__);
		ret = sensor_cis_write_registers_locked(subdev, priv->secure);
	} else {
		info("[%d][%s] init setting enter\n", cis->id, __func__);
		ret = sensor_cis_write_registers_locked(subdev, priv->init);
		if (ret < 0) {
			err("[%d]sensor_3lu_set_registers fail!!", cis->id);
			return ret;
		}
		info("[%d][%s] init setting done\n", cis->id, __func__);

		if (cis->cis_data->cis_rev != LATEST_3LU_REV) {
			info("[%d][%s] Tnp setting enter\n", cis->id, __func__);
			ret = sensor_cis_write_registers_locked(subdev, priv->tnp);
			if (ret < 0) {
				err("[%d]sensor_3lu_set_registers fail!!", cis->id);
				return ret;
			}
			info("[%d][%s] Tnp setting done\n", cis->id, __func__);
		}

		while (--retry) {
			info("[%d][%s] global setting enter\n", cis->id, __func__);
			ret = sensor_cis_write_registers_locked(subdev, priv->global);
			if (!ret) {
				info("[%d][%s] global setting done\n", cis->id, __func__);
				break;
			}
			if (!retry) {
				err("[%d]sensor_3lu_set_registers fail!!", cis->id);
				return ret;
			}
			udelay(10000);
			info("[%d][%s] global setting retry(%u)\n", cis->id, __func__, retry);
		}
	}

	if (!device->pdata->scramble) { /* default enable */
		ret |= sensor_cis_write_registers_locked(subdev, priv->disable_scramble);
		if (ret < 0) {
			err("sensor_3lu_set_registers fail!!");
			return ret;
		}
		info("[%s] disable_scramble\n", __func__);
	}

	return ret;
}

int sensor_3lu_cis_check_aeb(struct v4l2_subdev *subdev, u32 *next_mode)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (cis->cis_data->stream_on == false
		|| is_sensor_g_ex_mode(device) != EX_AEB
		|| sensor_3lu_mode_groups[SENSOR_3LU_MODE_AEB] == MODE_GROUP_NONE) {
		if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
			info("[%s] current AEB status is enabled. need AEB disable\n", __func__);
			cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
			/* AEB disable */
			cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
			cis->ixc_ops->write8(cis->client, 0x0E00, 0x00);
			info("[%s] disable AEB in not support mode\n", __func__);
		}
		return -1; //continue;
	}

	switch (cis->cis_data->cur_hdr_mode) {
	case SENSOR_HDR_MODE_2AEB_2VC:
		temp_mode = sensor_3lu_mode_groups[SENSOR_3LU_MODE_AEB];
		break;
	case SENSOR_HDR_MODE_SINGLE:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE)
		ret = -1;

	if (ret == 0)
		*next_mode = temp_mode;

	return ret;
}

int sensor_3lu_cis_check_12bit(cis_shared_data *cis_data, u32 *next_mode)
{
	int ret = 0;

	if (sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG] == MODE_GROUP_NONE)
		return -1;

	switch (cis_data->cur_12bit_mode) {
	case SENSOR_12BIT_STATE_REAL_12BIT:
		*next_mode = sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG];
		break;
	case SENSOR_12BIT_STATE_PSEUDO_12BIT:
	default:
		ret = -1;
		break;
	}

	return ret;
}

int sensor_3lu_cis_check_lownoise(cis_shared_data *cis_data, u32 *next_mode)
{
	int ret = 0;
	u32 temp_mode = MODE_GROUP_NONE;

	if ((sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] == MODE_GROUP_NONE
		&& sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4] == MODE_GROUP_NONE))
		return -1;

	switch (cis_data->cur_lownoise_mode) {
	case IS_CIS_LN2:
		temp_mode = sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2];
		break;
	case IS_CIS_LN4:
		temp_mode = sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4];
		break;
	case IS_CIS_LNOFF:
	default:
		break;
	}

	if (temp_mode == MODE_GROUP_NONE) {
		ret = -1;
	}

	if (ret == 0)
		*next_mode = temp_mode;

	return ret;
}

int sensor_3lu_cis_update_seamless_mode(struct v4l2_subdev *subdev)
{
	int ret = 0;
	unsigned int mode = 0;
	u16 cur_fast_change_idx = FCI_NONE_3LU;
	unsigned int next_mode = 0;
	const struct sensor_cis_mode_info *next_mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lu_private_runtime *priv_runtime = (struct sensor_3lu_private_runtime *)cis->priv_runtime;
	struct is_device_sensor *device;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	mode = cis->cis_data->sens_config_index_cur;

	if (priv_runtime->seamless_delay_count > 0)
		priv_runtime->seamless_delay_count--;

	if (priv_runtime->seamless_delay_flag == true) {
		info("[%s] processing seamless delay for stream off\n", __func__);
		return ret;
	}

	if (device->ex_scenario == IS_SCENARIO_SECURE) {
		dbg_sensor(1, "[%s] skip mode change IS_SCENARIO_SECURE)\n", __func__);
		return ret;
	}

	next_mode = sensor_3lu_mode_groups[SENSOR_3LU_MODE_DEFAULT];
	if (next_mode == MODE_GROUP_NONE) {
		err("mode group is none");
		return -1;
	}

	if (cis->cis_data->cur_pattern_mode != SENSOR_TEST_PATTERN_MODE_OFF) {
		dbg_sensor(1, "[%s] cur_pattern_mode (%d)", __func__, cis->cis_data->cur_pattern_mode);
		return ret;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);

	sensor_3lu_cis_check_aeb(subdev, &next_mode);

	if (cis->cis_data->cur_hdr_mode != SENSOR_HDR_MODE_2AEB_2VC) {
		sensor_3lu_cis_check_lownoise(cis->cis_data, &next_mode);
		sensor_3lu_cis_check_12bit(cis->cis_data, &next_mode);
	}

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->read16(cis->client, 0x0B30, &cur_fast_change_idx);

	next_mode_info = cis->sensor_info->mode_infos[next_mode];

	if ((mode == next_mode && cur_fast_change_idx == next_mode_info->setfile_fcm_index)
		|| next_mode == MODE_GROUP_NONE)
		goto EXIT;

	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	ret |= cis->ixc_ops->write16(cis->client, 0x0B30, next_mode_info->setfile_fcm_index);
	// X frames delay // return to previous index after Y frame (disable : 0)
	ret |= cis->ixc_ops->write16(cis->client, 0x0B32, 0x0100);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x2000);
	ret |= cis->ixc_ops->write16(cis->client, 0x0FF4, 0x0000);
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);
	if (ret < 0)
		err("sensor_3lu_set_registers fail!!");

	cis->cis_data->sens_config_index_pre = cis->cis_data->sens_config_index_cur;
	cis->cis_data->sens_config_index_cur = next_mode;

	CALL_CISOPS(cis, cis_data_calculation, subdev, next_mode);

	if (cis->cis_data->cur_lownoise_mode > 0
		|| cis->cis_data->cur_12bit_mode == SENSOR_12BIT_STATE_REAL_12BIT
		|| cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC)
		priv_runtime->seamless_delay_count = 3;

	info("[%s] pre(%d)->cur(%d), 12bit[%d] LN[%d] AEB[%d] => fast_change_idx[0x%x]\n",
		__func__,
		cis->cis_data->sens_config_index_pre, cis->cis_data->sens_config_index_cur,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_hdr_mode,
		next_mode_info->setfile_fcm_index);

	cis->cis_data->pre_12bit_mode = cis->cis_data->cur_12bit_mode;
	cis->cis_data->pre_lownoise_mode = cis->cis_data->cur_lownoise_mode;
	cis->cis_data->pre_hdr_mode = cis->cis_data->cur_hdr_mode;

EXIT:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_3lu_cis_get_seamless_mode_info(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = cis->cis_data;
	int ret = 0, cnt = 0;

	if (sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN2;
		sensor_cis_get_mode_info(subdev, sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_LN4;
		sensor_cis_get_mode_info(subdev, sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	if (sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG] != MODE_GROUP_NONE) {
		cis_data->seamless_mode_info[cnt].mode = SENSOR_MODE_REAL_12BIT;
		sensor_cis_get_mode_info(subdev, sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG],
			&cis_data->seamless_mode_info[cnt]);
		cnt++;
	}

	cis_data->seamless_mode_cnt = cnt;
	return ret;
}

int sensor_3lu_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	u32 ex_mode;
	struct is_device_sensor *device;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lu_private_data *priv = (struct sensor_3lu_private_data *)cis->sensor_info->priv;
	struct sensor_3lu_private_runtime *priv_runtime = (struct sensor_3lu_private_runtime *)cis->priv_runtime;
	u32 retry = REG_W_RETRY_CNT;

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		return -EINVAL;
	}

	priv_runtime->seamless_delay_count = 0;
	priv_runtime->seamless_delay_flag = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	if (device->ex_scenario == IS_SCENARIO_SECURE && cis->cis_data->cis_rev == LATEST_3LU_REV) {
		info("[%s] skip mode change IS_SCENARIO_SECURE)\n", __func__);
		return ret;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ex_mode = is_sensor_g_ex_mode(device);
	sensor_3lu_cis_set_mode_group(mode);

	info("[%s] sensor mode(%d) ex_mode(%d)\n", __func__, mode, ex_mode);

	if (sensor_3lu_mode_groups[SENSOR_3LU_MODE_IDCG] == MODE_GROUP_NONE
		&& sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN2] == MODE_GROUP_NONE
		&& sensor_3lu_mode_groups[SENSOR_3LU_MODE_LN4] == MODE_GROUP_NONE
		&& sensor_3lu_mode_groups[SENSOR_3LU_MODE_AEB] == MODE_GROUP_NONE) {
		info("[%d][%s] FCM disable \n", cis->id, __func__);
		//Fast Change disable
		ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		ret |= cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
		ret |= cis->ixc_ops->write16(cis->client, 0x0B30, 0x01FF);
	} else {
		while (--retry) {
			info("[%d][%s] FCM LUT setting enter\n", cis->id, __func__);
			ret = sensor_cis_write_registers(subdev, priv->fcm_lut);
			if (!ret) {
				info("[%d][%s] FCM LUT setting done\n", cis->id, __func__);
				break;
			}

			if (!retry) {
				err("[%d]FCM LUT write fail!!", cis->id);
				goto EXIT;
			}

			info("[%d][%s] FCM LUT setting retry(%u)\n", cis->id, __func__, retry);
		}
	}

	mode_info = cis->sensor_info->mode_infos[mode];

	retry = REG_W_RETRY_CNT;
	while (--retry) {
		ret = sensor_cis_write_registers(subdev, mode_info->setfile);
		if (!ret)
			break;

		if (!retry) {
			err("sensor_3lu_set_registers fail!!");
			goto EXIT;
		}

		info("sensor_3lu_set_registers retry(%u)", retry);
	}

	cis->cis_data->sens_config_index_pre = mode;

	cis->cis_data->pre_12bit_mode = mode_info->state_12bit;
	cis->cis_data->cur_12bit_mode = mode_info->state_12bit;

	cis->cis_data->pre_lownoise_mode = IS_CIS_LNOFF;
	cis->cis_data->cur_lownoise_mode = IS_CIS_LNOFF;

	cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
	/* AEB disable */
	ret |= cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	ret |= cis->ixc_ops->write8(cis->client, 0x0E00, 0x00);

	info("[%s] mode[%d] 12bit[%d] LN[%d] AEB[%d]\n",
		__func__, mode,
		cis->cis_data->cur_12bit_mode,
		cis->cis_data->cur_lownoise_mode,
		cis->cis_data->cur_hdr_mode);

EXIT:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	sensor_3lu_cis_update_seamless_mode(subdev);
	sensor_3lu_cis_get_seamless_mode_info(subdev);

	/* sensor_3lu_cis_log_status(subdev); */
	info("[%s] X\n", __func__);

	return ret;
}

int sensor_3lu_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;

	info("[%s] global setting start\n", __func__);
	/* setfile global setting is at camera entrance */
	ret = sensor_3lu_cis_set_global_setting_internal(subdev);
	if (ret < 0) {
		err("sensor_3lu_set_registers fail!!");
		return ret;
	}
	info("[%s] global setting done\n", __func__);

	return ret;
}

int sensor_3lu_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u16 fast_change_idx = 0x01FF;
	struct is_device_sensor *device;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);

	cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	/* update mipi rate */
	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);

	/* EMB Header off */
	ret = cis->ixc_ops->write8(cis->client, 0x0118, 0x00);
	if (ret < 0)
		err("[%d]EMB header off fail", cis->id);

	info("%s - fast_change_idx(0x%x)\n", __func__, fast_change_idx);

	/* Sensor stream on */
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3lu_cis_wait_seamless_update_delay(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lu_private_runtime *priv_runtime = (struct sensor_3lu_private_runtime *)cis->priv_runtime;

	/* LN Off -> LN2 -> N+2 frame -> Stream Off */
	/* AEB Off -> AEB ON -> N+2 frame -> Stream Off */
	if (priv_runtime->seamless_delay_count > 0) {
		priv_runtime->seamless_delay_flag = true;
		info("[%s] seamless_delay_count : %d , cis_data->cur_frame_us_time(%d)\n",
			__func__, priv_runtime->seamless_delay_count, cis->cis_data->cur_frame_us_time);

		if (cis->cis_data->cur_frame_us_time > 300000 && cis->cis_data->cur_frame_us_time < 2000000)
			// for Hyperlapse night mode
			msleep(cis->cis_data->cur_frame_us_time * priv_runtime->seamless_delay_count / 1000);
		else
			msleep(100 * priv_runtime->seamless_delay_count);
	}

	return 0;
}

static void sensor_3lu_cis_wait_stream_off_condition(struct v4l2_subdev *subdev)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	int retry = 250;
	u8 cur_frame_count = 0;

	if (cis->long_term_mode.sen_strm_off_on_enable) {
		dbg_sensor(1, "[MOD:D:%d] %s, skip for long term exposure\n", cis->id, __func__);
		return;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	while (--retry && (cur_frame_count == 0x1)) {
		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME + 10);

		IXC_MUTEX_LOCK(cis->ixc_lock);
		cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), retry(%d)\n",
				cis->id, __func__, cur_frame_count, retry);
	}

	if (cur_frame_count == 0x1)
		err("sensor_fcount is always 0x1\n");
}

int sensor_3lu_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 cur_frame_count = 0;
	u16 fast_change_idx = 0x01FF;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3lu_private_runtime *priv_runtime = (struct sensor_3lu_private_runtime *)cis->priv_runtime;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	sensor_3lu_cis_wait_stream_off_condition(subdev);

	IXC_MUTEX_LOCK(cis->ixc_lock);
	if (cis->cis_data->cur_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC
		&& cis->cis_data->pre_hdr_mode == SENSOR_HDR_MODE_2AEB_2VC) {
		info("[%s] current AEB status is enabled. need AEB disable\n", __func__);
		cis->cis_data->pre_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		cis->cis_data->cur_hdr_mode = SENSOR_HDR_MODE_SINGLE;
		/* AEB disable */
		cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
		cis->ixc_ops->write8(cis->client, 0x0E00, 0x00);
		info("[%s] disable AEB\n", __func__);
	}

	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	cis->ixc_ops->write16(cis->client, 0xFCFC, 0x4000);
	cis->ixc_ops->write16(cis->client, 0x6000, 0x0005);
	cis->ixc_ops->read16(cis->client, 0x0B30, &fast_change_idx);
	info("[%s] frame_count(0x%x), fast_change_idx(0x%x)\n", __func__, cur_frame_count, fast_change_idx);

	cis->ixc_ops->write16(cis->client, 0x6000, 0x0085);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = false;
	priv_runtime->seamless_delay_flag = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static struct is_cis_ops cis_ops_3lu = {
	.cis_init = sensor_3lu_cis_init,
	.cis_log_status = sensor_3lu_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_3lu_cis_set_global_setting,
	.cis_mode_change = sensor_3lu_cis_mode_change,
	.cis_stream_on = sensor_3lu_cis_stream_on,
	.cis_stream_off = sensor_3lu_cis_stream_off,
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
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
#ifdef USE_CAMERA_EMBEDDED_HEADER
	.cis_get_frame_id = sensor_3lu_cis_get_frame_id,
#endif
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
//	.cis_recover_stream_on = sensor_cis_recover_stream_on,
//	.cis_recover_stream_off = sensor_cis_recover_stream_off,
	.cis_set_test_pattern = sensor_cis_set_test_pattern,
	.cis_update_seamless_mode = sensor_3lu_cis_update_seamless_mode,
	.cis_wait_seamless_update_delay = sensor_3lu_cis_wait_seamless_update_delay,
	.cis_set_flip_mode = sensor_cis_set_flip_mode,
};

DEFINE_I2C_DRIVER_PROBE(cis_3lu_probe_i2c)
{
	int ret;
	u32 mclk_freq_khz;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;
	struct device *dev;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("[%s] sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	dev = &client->dev;
	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_3lu;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->use_seamless_mode = true;
	cis->reg_addr = &sensor_3lu_reg_addr;
	cis->priv_runtime =
		devm_kzalloc(dev, sizeof(struct sensor_3lu_private_runtime), GFP_KERNEL);
	if (!cis->priv_runtime) {
		pablo_free(cis->cis_data);
		pablo_free(cis->subdev);
		probe_err("cis->priv_runtime is NULL");
		return -ENOMEM;
	}

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	mclk_freq_khz = sensor_peri->module->pdata->mclk_freq_khz;

	if (mclk_freq_khz == 19200) {
		if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
			probe_info("%s setfile_A mclk: 19.2MHz\n", __func__);
		else
			err("setfile index out of bound, take default (setfile_A mclk: 19.2MHz)");

		cis->sensor_info = &sensor_3lu_info_A_19p2;
	}

	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);

	return ret;
}

PKV_DEFINE_I2C_DRIVER_REMOVE(cis_3lu_remove_i2c)
{
	PKV_DEFINE_I2C_DRIVER_REMOVE_RETURN;
}

static const struct of_device_id sensor_cis_3lu_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-3lu",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_3lu_match);

static const struct i2c_device_id sensor_cis_3lu_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_3lu_driver = {
	.probe	= cis_3lu_probe_i2c,
	.remove	= cis_3lu_remove_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_3lu_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_3lu_idt
};

#ifdef MODULE
module_driver(sensor_cis_3lu_driver, i2c_add_driver,
	i2c_del_driver)
#else
static int __init sensor_cis_3lu_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_3lu_driver);
	if (ret)
		err("failed to add %s driver: %d",
			sensor_cis_3lu_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_3lu_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
