// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include "scaler-format.h"
#include "scaler-regs.h"

static const struct sc_fmt sc_formats[] = {
	{
		.name		= "RGB565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.cfg_val	= SCALER_CFG_FMT_RGB565,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "RGB1555",
		.pixelformat	= V4L2_PIX_FMT_RGB555X,
		.cfg_val	= SCALER_CFG_FMT_ARGB1555,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "ARGB4444",
		.pixelformat	= V4L2_PIX_FMT_RGB444,
		.cfg_val	= SCALER_CFG_FMT_ARGB4444,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {	/* swaps of ARGB32 in bytes in half word, half words in word */
		.name		= "RGBA8888",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.cfg_val	= SCALER_CFG_FMT_RGBA8888 |
					SCALER_CFG_BYTE_HWORD_SWAP,
		.bitperpixel	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "BGRA8888",
		.pixelformat	= V4L2_PIX_FMT_BGR32,
		.cfg_val	= SCALER_CFG_FMT_ARGB8888,
		.bitperpixel	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "ARGB2101010",
		.pixelformat	= V4L2_PIX_FMT_ARGB2101010,
		.cfg_val	= SCALER_CFG_FMT_ARGB2101010,
		.bitperpixel	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "ABGR2101010",
		.pixelformat	= V4L2_PIX_FMT_ABGR2101010,
		.cfg_val	= SCALER_CFG_FMT_ABGR2101010,
		.bitperpixel	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "RGBA1010102",
		.pixelformat	= V4L2_PIX_FMT_RGBA1010102,
		.cfg_val	= SCALER_CFG_FMT_RGBA1010102,
		.bitperpixel	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "BGRA1010102",
		.pixelformat	= V4L2_PIX_FMT_BGRA1010102,
		.cfg_val	= SCALER_CFG_FMT_BGRA1010102,
		.bitperpixel	= { 32 },
		.num_planes	= 1,
		.num_comp	= 1,
		.is_rgb		= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P,
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.cfg_val	= SCALER_CFG_FMT_YCRCB420_2P,
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.cfg_val	= SCALER_CFG_FMT_YCRCB420_2P,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,	/* I420 */
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_3P,
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YVU 4:2:0 contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420,	/* YV12 */
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_3P,
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_3P,
		.bitperpixel	= { 8, 2, 2 },
		.num_planes	= 3,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_3P,
		.bitperpixel	= { 8, 2, 2 },
		.num_planes	= 3,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.cfg_val	= SCALER_CFG_FMT_YUYV,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.cfg_val	= SCALER_CFG_FMT_UYVY,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, YCrYCb",
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.cfg_val	= SCALER_CFG_FMT_YVYU,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.cfg_val	= SCALER_CFG_FMT_YCBCR422_2P,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.cfg_val	= SCALER_CFG_FMT_YCRCB422_2P,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.cfg_val	= SCALER_CFG_FMT_YCBCR422_3P,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 3,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12N,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P,
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous Y/CbCr 10-bit",
		.pixelformat	= V4L2_PIX_FMT_NV12N_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P |
					SCALER_CFG_10BIT_S10,
		.bitperpixel	= { 15 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 3-planar Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420N,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_3P,
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 3,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr 8+2 bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_S10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P |
					SCALER_CFG_10BIT_S10,
		.bitperpixel	= { 10, 5 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr 10-bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P |
					SCALER_CFG_BYTE_SWAP |
					SCALER_CFG_10BIT_P010,
		.bitperpixel	= { 16, 8 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr 10-bit",
		.pixelformat	= V4L2_PIX_FMT_NV12_P010,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P |
					SCALER_CFG_BYTE_SWAP |
					SCALER_CFG_10BIT_P010,
		.bitperpixel	= { 24 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CbCr 10-bit",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.cfg_val	= SCALER_CFG_FMT_YCBCR422_2P |
					SCALER_CFG_BYTE_SWAP |
					SCALER_CFG_10BIT_P010,
		.bitperpixel	= { 16, 16 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous, Y/CbCr 10-bit",
		.pixelformat	= V4L2_PIX_FMT_NV16_P210,
		.cfg_val	= SCALER_CFG_FMT_YCBCR422_2P |
					SCALER_CFG_BYTE_SWAP |
					SCALER_CFG_10BIT_P010,
		.bitperpixel	= { 32 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CbCr 8+2 bit",
		.pixelformat	= V4L2_PIX_FMT_NV16M_S10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR422_2P |
					SCALER_CFG_10BIT_S10,
		.bitperpixel	= { 10, 10 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CrCb 8+2 bit",
		.pixelformat	= V4L2_PIX_FMT_NV61M_S10B,
		.cfg_val	= SCALER_CFG_FMT_YCRCB422_2P |
					SCALER_CFG_10BIT_S10,
		.bitperpixel	= { 10, 10 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC 8 bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWC_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P |
					SCALER_CFG_SBWC_FORMAT,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC 10 bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWC_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC,
		.bitperpixel	= { 10, 5 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CrCb SBWC 8 bit",
		.pixelformat	= V4L2_PIX_FMT_NV21M_SBWC_8B,
		.cfg_val	= SCALER_CFG_FMT_YCRCB420_2P |
					SCALER_CFG_SBWC_FORMAT,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CrCb SBWC 10 bit",
		.pixelformat	= V4L2_PIX_FMT_NV21M_SBWC_10B,
		.cfg_val	= SCALER_CFG_FMT_YCRCB420_2P	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC,
		.bitperpixel	= { 10, 5 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr SBWC 8 bit",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWC_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P |
					SCALER_CFG_SBWC_FORMAT,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr SBWC 10 bit",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWC_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC,
		.bitperpixel	= { 10, 5 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC lossy 8 bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT,
		.bitperpixel	= { 6, 3 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC lossy 10 bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "Y mono",
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.cfg_val	= SCALER_CFG_FMT_Y_MONO,
		.bitperpixel	= { 8 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "Y mono 10 bit",
		.pixelformat	= V4L2_PIX_FMT_Y10,
		.cfg_val	= SCALER_CFG_FMT_Y_MONO |
				  SCALER_CFG_10BIT_P010,
		.bitperpixel	= { 16 },
		.num_planes	= 1,
		.num_comp	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.7 lossy 8 bit 32 align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_32_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_SBWC_BYTE_ALIGN(32),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.7 lossy 8 bit 64 align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_SBWC_BYTE_ALIGN(64),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr SBWC v2.7 lossy 8 bit 32 align",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWCL_32_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_SBWC_BYTE_ALIGN(32),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.7 lossy 8 bit 64 align",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWCL_64_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_SBWC_BYTE_ALIGN(64),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.7 lossy 10 bit 32 align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_32_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC	|
					SCALER_CFG_SBWC_BYTE_ALIGN(32),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.7 lossy 10 bit 64 align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC	|
					SCALER_CFG_SBWC_BYTE_ALIGN(64),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr SBWC v2.7 lossy 10 bit 32 align",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWCL_32_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC	|
					SCALER_CFG_SBWC_BYTE_ALIGN(32),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.7 lossy 10 bit 64 align",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWCL_64_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC	|
					SCALER_CFG_SBWC_BYTE_ALIGN(64),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr with SBWC layout",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWC_DECOMP,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P,
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr 10-bit with SBWC layout",
		.pixelformat	= V4L2_PIX_FMT_P010N_SBWC_DECOMP,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P |
					SCALER_CFG_BYTE_SWAP |
					SCALER_CFG_10BIT_P010,
		.bitperpixel	= { 24 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr aligned by 256",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWC_256_8B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_SBWC_BYTE_ALIGN(128),
		.bitperpixel	= { 12 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous, Y/CbCr 10-bit aligned by 256",
		.pixelformat	= V4L2_PIX_FMT_NV12N_SBWC_256_10B,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC	|
					SCALER_CFG_SBWC_BYTE_ALIGN(128),
		.bitperpixel	= { 24 },
		.num_planes	= 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.8 lossy FR 8 bit 64 align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_8B_FR,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_SBWC_LOSSY_FR_MODE |
					SCALER_CFG_SBWC_BYTE_ALIGN(64),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr SBWC v2.8 lossy FR 10 bit 64 align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_10B_FR,
		.cfg_val	= SCALER_CFG_FMT_YCBCR420_2P	|
					SCALER_CFG_SBWC_LOSSY	|
					SCALER_CFG_SBWC_FORMAT	|
					SCALER_CFG_10BIT_SBWC	|
					SCALER_CFG_SBWC_LOSSY_FR_MODE |
					SCALER_CFG_SBWC_BYTE_ALIGN(64),
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	},
};

const struct sc_fmt *sc_find_format(unsigned int pixfmt) {
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(sc_formats); ++i) {
		if (sc_formats[i].pixelformat == pixfmt) {
			return &sc_formats[i];
		}
	}

	return NULL;
}

const struct sc_fmt *sc_get_default_format(void) {
	return &sc_formats[0];
}

void sc_get_bytesperline(u32 pixelformat, u32 spansize, u32 *bytesperline) {
	switch (pixelformat) {
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_RGB444:
		*bytesperline = spansize * 2;
		break;
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_ARGB2101010:
	case V4L2_PIX_FMT_ABGR2101010:
	case V4L2_PIX_FMT_RGBA1010102:
	case V4L2_PIX_FMT_BGRA1010102:
		*bytesperline = spansize * 4;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_NV12N:
	case V4L2_PIX_FMT_NV12N_10B:
	case V4L2_PIX_FMT_YUV420N:
	case V4L2_PIX_FMT_NV12M_S10B:
		*bytesperline = spansize;
		break;
	case V4L2_PIX_FMT_NV12M_P010:
	case V4L2_PIX_FMT_NV12_P010:
	case V4L2_PIX_FMT_NV16M_P210:
	case V4L2_PIX_FMT_NV16_P210:
		*bytesperline = spansize * 2;
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
	case V4L2_PIX_FMT_NV61M_S10B:
	case V4L2_PIX_FMT_NV12M_SBWC_8B:
	case V4L2_PIX_FMT_NV12M_SBWC_10B:
	case V4L2_PIX_FMT_NV21M_SBWC_8B:
	case V4L2_PIX_FMT_NV21M_SBWC_10B:
	case V4L2_PIX_FMT_NV12N_SBWC_8B:
	case V4L2_PIX_FMT_NV12N_SBWC_10B:
	case V4L2_PIX_FMT_NV12M_SBWCL_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_10B:
	case V4L2_PIX_FMT_GREY:
		*bytesperline = spansize;
		break;
	case V4L2_PIX_FMT_Y10:
		*bytesperline = spansize * 2;
		break;
	case V4L2_PIX_FMT_NV12M_SBWCL_32_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_8B:
	case V4L2_PIX_FMT_NV12N_SBWCL_32_8B:
	case V4L2_PIX_FMT_NV12N_SBWCL_64_8B:
	case V4L2_PIX_FMT_NV12M_SBWCL_32_10B:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_10B:
	case V4L2_PIX_FMT_NV12N_SBWCL_32_10B:
	case V4L2_PIX_FMT_NV12N_SBWCL_64_10B:
	case V4L2_PIX_FMT_NV12N_SBWC_DECOMP:
		*bytesperline = spansize;
		break;
	case V4L2_PIX_FMT_P010N_SBWC_DECOMP:
		*bytesperline = spansize * 2;
		break;
	case V4L2_PIX_FMT_NV12N_SBWC_256_8B:
	case V4L2_PIX_FMT_NV12N_SBWC_256_10B:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_8B_FR:
	case V4L2_PIX_FMT_NV12M_SBWCL_64_10B_FR:
	default:
		*bytesperline = spansize;
		break;
	}
}
