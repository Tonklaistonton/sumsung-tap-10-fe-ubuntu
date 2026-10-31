/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-device-sensor-peri.h"
#include "is-vendor-test-sensor.h"
#include "is-vendor-mipi.h"
#include "is-vendor-private.h"

#ifdef USE_SENSOR_DEBUG
static int backup_i2c_addr_list[SENSOR_POSITION_MAX][DEVICE_TYPE_MAX];
static bool i2c_block_current_state[SENSOR_POSITION_MAX][DEVICE_TYPE_MAX];

static const struct cam_cp_cell_info test_cp_cell_infos[] = {
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01, 300, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02, 900, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03, 1900, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04, 2175, 1, 20000, 25),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05, 2525, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07, 3100, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08, 3625, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12, 5095, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_103_LTE_LB13, 5230, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_107_LTE_LB17, 5790, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18, 5925, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_109_LTE_LB19, 6075, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20, 6300, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25, 8365, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26, 8865, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28, 9435, 1, 10000, 25),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32, 10140, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38, 38000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39, 38450, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40, 39400, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41, 40620, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66, 66886, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_256_NR5G_N001, 428000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_257_NR5G_N002, 392000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_258_NR5G_N003, 368500, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005, 176300, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_262_NR5G_N007, 531000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008, 188500, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_267_NR5G_N012, 147500, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_268_NR5G_N013, 150200, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_273_NR5G_N018, 173500, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_275_NR5G_N020, 161200, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_280_NR5G_N025, 392500, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_281_NR5G_N026, 175300, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028, 156100, 1, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_293_NR5G_N038, 519000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_294_NR5G_N039, 380000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_295_NR5G_N040, 470000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_296_NR5G_N041, 518601, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_321_NR5G_N066, 431000, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_330_NR5G_N075, 294900, 1, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_332_NR5G_N077, 650000, 1, 100000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_333_NR5G_N078, 636667, 1, 100000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_091_LTE_LB01, 300, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_092_LTE_LB02, 900, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_093_LTE_LB03, 1900, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_094_LTE_LB04, 2175, 2, 20000, 25),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_095_LTE_LB05, 2525, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_097_LTE_LB07, 3100, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_098_LTE_LB08, 3625, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_102_LTE_LB12, 5095, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_103_LTE_LB13, 5230, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_107_LTE_LB17, 5790, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_108_LTE_LB18, 5925, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_109_LTE_LB19, 6075, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_110_LTE_LB20, 6300, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_115_LTE_LB25, 8365, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_116_LTE_LB26, 8865, 2, 10000, 25),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_118_LTE_LB28, 9435, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_122_LTE_LB32, 10140, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_128_LTE_LB38, 38000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_129_LTE_LB39, 38450, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_130_LTE_LB40, 39400, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_131_LTE_LB41, 40620, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_3_LTE, CAM_BAND_156_LTE_LB66, 66886, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_256_NR5G_N001, 428000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_257_NR5G_N002, 392000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_258_NR5G_N003, 368500, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_260_NR5G_N005, 176300, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_262_NR5G_N007, 531000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_263_NR5G_N008, 188500, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_267_NR5G_N012, 147500, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_268_NR5G_N013, 150200, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_273_NR5G_N018, 173500, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_275_NR5G_N020, 161200, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_280_NR5G_N025, 392500, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_281_NR5G_N026, 175300, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_283_NR5G_N028, 156100, 2, 10000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_293_NR5G_N038, 519000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_294_NR5G_N039, 380000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_295_NR5G_N040, 470000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_296_NR5G_N041, 518601, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_321_NR5G_N066, 431000, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_330_NR5G_N075, 294900, 2, 20000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_332_NR5G_N077, 650000, 2, 100000, -10),
	DEFINE_TEST_BAND_INFO(CAM_RAT_7_NR5G, CAM_BAND_333_NR5G_N078, 636667, 2, 100000, -10),
};

/* Sensor Test with ADB Commend */
static int test_set_adaptive_mipi_mode(const char *val, const struct kernel_param *kp)
{
	int ret, position, argc, adaptive_mipi_mode;
	char **argv;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &position);
	ret = kstrtouint(argv[1], 0, &adaptive_mipi_mode);

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	cis = &sensor_peri->cis;
	WARN_ON(!cis);

	if (cis->mipi_sensor_mode_size == 0) {
		info("[%s][%d] adaptive mipi is not supported", __func__, position);
	} else {
		info("[%s][%d] adaptive_mipi is %s", __func__,
			position, (adaptive_mipi_mode == 1) ? "disabled" : "enabled");
		if (adaptive_mipi_mode == 1)
			cis->vendor_use_adaptive_mipi = false;
		else
			cis->vendor_use_adaptive_mipi = true;
	}

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_mipi_mode = {
	.set = test_set_adaptive_mipi_mode,
	.get = NULL,
};

/**
 * Command : adb shell "echo 0 1 > /sys/module/fimc_is/parameters/test_mipi_mode"
 * @param 0 Select sensor position
 * @param 1 Select adaptive mipi mode : Disable[1]
 */
module_param_cb(test_mipi_mode, &param_ops_test_mipi_mode, NULL, 0644);

static struct cam_cp_noti_cell_infos g_test_cell_infos;

u32 cell_info_offsets[] = {
	offsetof(struct cam_cp_cell_info, rat),
	offsetof(struct cam_cp_cell_info, band),
	offsetof(struct cam_cp_cell_info, channel),
	offsetof(struct cam_cp_cell_info, connection_status),
	offsetof(struct cam_cp_cell_info, bandwidth),
	offsetof(struct cam_cp_cell_info, sinr),
	offsetof(struct cam_cp_cell_info, rsrp),
	offsetof(struct cam_cp_cell_info, rsrq),
	offsetof(struct cam_cp_cell_info, cqi),
	offsetof(struct cam_cp_cell_info, dl_mcs),
	offsetof(struct cam_cp_cell_info, pusch_power),
};

u32 cell_info_sizes[] = {
	sizeof(((struct cam_cp_cell_info *)0)->rat),
	sizeof(((struct cam_cp_cell_info *)0)->band),
	sizeof(((struct cam_cp_cell_info *)0)->channel),
	sizeof(((struct cam_cp_cell_info *)0)->connection_status),
	sizeof(((struct cam_cp_cell_info *)0)->bandwidth),
	sizeof(((struct cam_cp_cell_info *)0)->sinr),
	sizeof(((struct cam_cp_cell_info *)0)->rsrp),
	sizeof(((struct cam_cp_cell_info *)0)->rsrq),
	sizeof(((struct cam_cp_cell_info *)0)->cqi),
	sizeof(((struct cam_cp_cell_info *)0)->dl_mcs),
	sizeof(((struct cam_cp_cell_info *)0)->pusch_power),
};

/* CP Test with ADB Commend */
static int test_set_manual_mipi_cell_infos(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	int argc;
	char **argv;
	char *command;
	int i;
	int pos;
	int32_t i32tmp;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	command = argv[0];

	info("[%s] command : %s ", __func__, command);

	if (!strcmp(command, "set")) {
		pos = g_test_cell_infos.num_cell;
		for (i = 1; i < argc; i++) {
			ret |= kstrtoint(argv[i], 0, &i32tmp);
			memcpy((char *)&(g_test_cell_infos.cell_list[pos]) + cell_info_offsets[i-1],
					&i32tmp,
					cell_info_sizes[i - 1]);
		}

		g_test_cell_infos.num_cell++;
	} else if (!strcmp(command, "clear")) {
		memset(&g_test_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
		is_vendor_set_cp_test_cell(false);
	} else if (!strcmp(command, "execute")) {
		is_vendor_set_cp_test_cell_infos(&g_test_cell_infos);
		is_vendor_set_cp_test_cell(true);
	} else {
		err("[%s] command error %s", __func__, command);
		ret = -EINVAL;
	}

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_manual_mipi_cell_infos = {
	.set = test_set_manual_mipi_cell_infos,
	.get = NULL,
};

/**
 * Command : adb shell "echo set 1 1 0 1 10000 > /sys/module/fimc_is/parameters/test_manual_mipi_cell_infos"
 * Command : adb shell "echo clear > /sys/module/fimc_is/parameters/test_manual_mipi_cell_infos"
 * Command : adb shell "echo execute > /sys/module/fimc_is/parameters/test_manual_mipi_cell_infos"
 * @param 0 Command to execute (set, clear, execute)
 * @param 1 rat
 * @param 2 band
 * @param 3 channel
 * @param 4 connection_status
 * @param 5 bandwidth
 * @param 6 sinr
 * @param 7 rsrp
 * @param 8 rsrq
 * @param 9 cqi
 * @param 10 dl_mcs
 * @param 11 pusch_power
 */
module_param_cb(test_manual_mipi_cell_infos, &param_ops_test_manual_mipi_cell_infos, NULL, 0644);

/* CP Test with ADB Commend */
static int test_set_auto_mipi_cell_infos(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	int argc;
	char **argv;
	int i;
	int pos;
	int test_idx;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	memset(&g_test_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));

	for (i = 0; i < argc; i++) {
		ret |= kstrtoint(argv[i], 0, &test_idx);

		info("[%s] apply test cell info index : %d", __func__, test_idx);
		test_idx--; //start with 0
		pos = g_test_cell_infos.num_cell;
		memcpy(&g_test_cell_infos.cell_list[pos],
				&test_cp_cell_infos[test_idx],
				sizeof(struct cam_cp_cell_info));
		g_test_cell_infos.num_cell++;
	}

	is_vendor_set_cp_test_cell_infos(&g_test_cell_infos);
	is_vendor_set_cp_test_cell(true);

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_auto_mipi_cell_infos = {
	.set = test_set_auto_mipi_cell_infos,
	.get = NULL,
};

/**
 * Command : adb shell "echo 1 3 6 > /sys/module/fimc_is/parameters/test_auto_mipi_cell_infos"
 * @param 0 test cp cell info index starting with 1
 */
module_param_cb(test_auto_mipi_cell_infos, &param_ops_test_auto_mipi_cell_infos, NULL, 0644);

static int test_set_seamless_mode(const char *val, const struct kernel_param *kp)
{
	int ret, position, argc, hdr, ln, sensor_12bit_state, zoom_ratio;
	char **argv;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &position);
	info("[%s] sensor position %d", __func__, position);

	ret = kstrtouint(argv[1], 0, &hdr);
	info("[%s] hdr mode mode %d", __func__, hdr);

	ret = kstrtouint(argv[2], 0, &ln);
	info("[%s] LN mode %d", __func__, ln);

	ret = kstrtouint(argv[3], 0, &sensor_12bit_state);
	info("[%s] 12bit state %d", __func__, sensor_12bit_state);

	ret = kstrtouint(argv[4], 0, &zoom_ratio);
	info("[%s] zoom_ratio %d", __func__, zoom_ratio);

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	cis = &sensor_peri->cis;
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis->cis_data->cur_hdr_mode = hdr;
	cis->cis_data->cur_lownoise_mode = ln;
	cis->cis_data->cur_12bit_mode = sensor_12bit_state;
	cis->cis_data->cur_remosaic_zoom_ratio = zoom_ratio;

	ret = CALL_CISOPS(cis, cis_update_seamless_mode, cis->subdev);

	argv_free(argv);
	return ret;
}

static int test_get_seamless_mode(char *buffer, const struct kernel_param *kp)
{
	int position, hdr, ln, sensor_12bit_state, zoom_ratio, pos;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;
	int len = 0;

	for (pos = 0; pos < SENSOR_POSITION_MAX; pos++) {
		is_vendor_get_module_from_position(pos, &module);
		if(!module) continue;

		sensor_peri = (struct is_device_sensor_peri *)module->private_data;
		WARN_ON(!sensor_peri);

		cis = &sensor_peri->cis;
		WARN_ON(!cis);
		WARN_ON(!cis->cis_data);

		if (cis->cis_data->stream_on) {
			position = pos;
			hdr = cis->cis_data->cur_hdr_mode;
			ln = cis->cis_data->cur_lownoise_mode;
			sensor_12bit_state = cis->cis_data->cur_12bit_mode;
			zoom_ratio = cis->cis_data->cur_remosaic_zoom_ratio;

			len += sprintf(buffer + len, "pos(%d) aeb(%d) ln(%d) dt(%d) zr(%d)\n",
				position, hdr, ln, sensor_12bit_state, zoom_ratio);
		}
	}

	return len;
}

static const struct kernel_param_ops param_ops_test_seamless_mode = {
	.set = test_set_seamless_mode,
	.get = test_get_seamless_mode,
};

/**
 * Command : adb shell "echo 0 1 2 3 4 > /sys/module/fimc_is/parameters/test_seamless_mode"
 * @param 0 Select sensor position
 * @param 1 Select hdr mode for AEB
 * @param 2 Select ln mode
 * @param 3 Select sensor 12bit state
 * @param 4 Select cropped remosaic mode(zoom ratio)
 */
module_param_cb(test_seamless_mode, &param_ops_test_seamless_mode, NULL, 0644);

static int test_set_i2c_block(const char *val, const struct kernel_param *kp)
{
	bool is_all_eeprom_i2c_enable = true;
	bool is_device_i2c_block[DEVICE_TYPE_MAX] = {false, };
	int ret = 0, position, argc, i = 0;
	char **argv;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module = NULL;
	struct is_cis *cis = NULL;
	struct is_actuator *actuator = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct is_device_sensor *device = NULL;
	struct i2c_client *device_client[DEVICE_TYPE_MAX] = {NULL, };
	struct is_core *core = is_get_is_core();
	struct is_vendor_private *vendor_priv = core->vendor.private_data;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &position);
	info("[%s] sensor position(%d)\n", __func__, position);

	for (i = SENSOR_TYPE; i < DEVICE_TYPE_MAX; i++) {
		ret = kstrtobool(argv[i + 1], &is_device_i2c_block[i]);
		info("[%s] is_device_i2c_block[%d](%d)\n", __func__, i, is_device_i2c_block[i]);
	}

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	subdev = module->subdev;
	WARN_ON(!subdev);

	device = v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	info("[%s] sensor_name(%s) position(%d)\n", __func__, module->sensor_name, position);

	if (position >= SENSOR_POSITION_MAX || position < SENSOR_POSITION_REAR) {
		err("not available position");
		argv_free(argv);
		return -EINVAL;
	}

	cis = &sensor_peri->cis;
	if (cis) {
		device_client[SENSOR_TYPE] = cis->client;
		info("[%s] cis_addr(0x%x)\n", __func__, device_client[SENSOR_TYPE]->addr);
	} else
		err("not available cis");

	actuator = device->actuator[device->pdev->id];
	if (actuator) {
		device_client[ACTUATOR_TYPE] = actuator->client;
		info("[%s] actuator_addr(0x%x)\n", __func__, device_client[ACTUATOR_TYPE]->addr);
	} else
		err("not available actuator");

	device_client[EEPROM_TYPE] = vendor_priv->rom[module->pdata->rom_id].client[0];
	if (device_client[EEPROM_TYPE])
		info("[%s] eeprom_addr(0x%x)\n", __func__, device_client[EEPROM_TYPE]->addr);
	else
		err("not available eeprom_client");

	for (i = SENSOR_TYPE; i < DEVICE_TYPE_MAX; i++) {
		if (device_client[i]) {
			if (device_client[i]->addr)
				backup_i2c_addr_list[position][i] = device_client[i]->addr;

			device_client[i]->addr
				= is_device_i2c_block[i] ? 0 : backup_i2c_addr_list[position][i];
			i2c_block_current_state[position][i] = is_device_i2c_block[i];
		}
	}

	for (i = SENSOR_POSITION_REAR; i < SENSOR_POSITION_MAX; i++) {
		if (i2c_block_current_state[i][EEPROM_TYPE] == true) {
			is_all_eeprom_i2c_enable = false;
			break;
		}
	}

	vendor_priv->force_cal_reload = is_all_eeprom_i2c_enable ? false : true;
	info("[%s] force_cal_reload %s\n", __func__,
		is_all_eeprom_i2c_enable ? "disabled" : "enabled");

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_i2c_block = {
	.set = test_set_i2c_block,
	.get = NULL,
};

/**
 * Command : adb shell "echo 0 1 2 3 > /sys/module/fimc_is/parameters/test_i2c_block"
 * @param 0 Select sensor position
 * @param 1 Select sensor i2c block enable
 * @param 2 Select actuator i2c block enable
 * @param 3 Select eeprom i2c block enable
 */
module_param_cb(test_i2c_block, &param_ops_test_i2c_block, NULL, 0644);
#endif
