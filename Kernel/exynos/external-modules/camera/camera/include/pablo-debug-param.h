/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_DEBUG_PARAM_H
#define PABLO_DEBUG_PARAM_H

static inline int param_debug_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t0 : turn off debug\n"
				"\t1 : turn on debug\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_clock_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t0 : turn off debug\n"
				"\t1 : dump clock\n"
				"\t2 : print clock log\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_stream_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t0 : turn off debug\n"
				"\t1 : print log\n"
				"\t    - device/subdev-shot/done\n"
				"\t    - is chain interface\n"
				"\t    - ISP, MCSC subdev crop\n"
				"\t    - sensor moduel search fail\n"
				"\t    - device/subdev-shot/done\n"
				"\t2 : print hardware-shot/done log\n"
				"\t3 : print log\n"
				"\t    - video device Q/DQ\n"
				"\t    - ISP subdev format\n"
				"\t    - subdev stripe\n"
				"\t4 : print log\n"
				"\t    - is chain/sensor device buffer queue/finish\n"
				"\t    - subdev TAG log\n"
				"\t5 : print sensor TAG (hardIRQ/softIRQ) log\n"
				"\t6 : control repeat_clahe enable\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_hw_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t  0 : turn off debug\n"
				"\t>=1 : turn on is-hw-control, is-interface-ddk debug log\n"
				"\t>=2 : turn on is-interface-wrap, hardware debug log\n"
				"\t>=3 : turn on hw log level3, hardware/api debug log\n"
				"\t>=4 : turn on hw log level4, hardware/api debug log\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_irq_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t  0 : turn off debug\n"
				"\t>=1 : turn on CSIS frame start/line/end,\n"
				"\t              DMA start/end interrupt debug log\n"
				"\t>=2 : turn on CSIS link err debug log\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_csi_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t  0 : turn off debug\n"
				"\t>=1 : dump CSI, PHY, DMA sfr\n"
				"\t>=3 : configure phy from fw\n"
				"\t>=5 : enable CSIS line interrupt\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_time_queue_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set time queue average count\n"
				"\t0 : no effect, the value should be greater than 1.\n"
				"\tN : queue average count\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_time_shot_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set time shot average count\n"
				"\t0 : no effect, the value should be greater than 1.\n"
				"\tN : shot average count\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_dvfs_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t  0 : turn off debug\n"
				"\t>=1 : turn on dvfs control debug log\n"
				"\t>=2 : turn on dvfs control, mipi, secure etc. log\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_mem_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t  0 : turn off debug\n"
				"\t>=1 : turn on memory debug log\n"
				"\t>=2 : turn on memory sync debug log\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_lvn_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t  0 : turn off debug\n"
				"\t>=1 : turn on lvn debug log\n"
				"\t>=2 : turn on lvn sub buffer debug log\n"
				"\t>=3 : turn on lvn buf prepare/finish debug log\n"
				"\t>=4 : turn on lvn format, size, length debug log\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_llc_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set LLC size\n"
				"\t0 : no effect, the value should be greater than 1.\n"
				"\tN : LLC size\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_hw_dump_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t0 : turn off debug\n"
				"\t1 : dump hw CR or dbg state\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_iq_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set hw ip id for IQ debugging\n"
				"\t0 : no effect, the value should be greater than 1.\n"
				"\tN : hw ip id\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_phy_tune_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set phy tune mode\n"
				"\t0 : set PABLO_PHY_TUNE_DISABLE\n"
				"\t1 : set PABLO_PHY_TUNE_CPHY\n"
				"\t2 : set PABLO_PHY_TUNE_DPHY\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_crc_seed_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set crc seed for debugging\n"
				"\t0 : no effect, the value should be greater than 1.\n"
				"\tN : crc seed\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

static inline int param_debug_sensor_usage(char *buffer, const size_t buf_size)
{
	const char *usage_msg = "[value] number, set debug param value\n"
				"\t  0 : turn off debug\n"
				"\t>=1 : turn on sensor interface & CIS debug lv1 log\n"
				"\t>=2 : turn on sensor interface & CIS debug lv2 log\n"
				"\t>=3 : turn on actuator debug log\n"
				"\t>=4 : turn on flash debug log\n"
				"\t>=5 : turn on preprocessor debug log\n"
				"\t>=6 : turn on aperture debug log\n"
				"\t>=7 : turn on OIS debug log\n";

	return scnprintf(buffer, buf_size, usage_msg);
}

#endif /* PABLO_DEBUG_PARAM_H */
