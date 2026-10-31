// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2010 - 2022 Novatek, Inc.
 *
 * Revision: 108741
 * Date: 2022-11-21 10:31:27 +0800 (¶g¤@, 21 ¤Q¤@¤ë 2022)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#ifndef 	_LINUX_NVT_TOUCH_H
#define		_LINUX_NVT_TOUCH_H

#include "../../../sec_input/sec_input.h"
#include "nt36xxx_mem_map.h"
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include "../../../sec_input/sec_secure_touch.h"
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE 0
#if IS_ENABLED(CONFIG_INPUT_SEC_TRUSTED_TOUCH)
#include "../../../sec_input/sec_trusted_touch.h"
#endif
#endif

#if IS_ENABLED(CONFIG_MTK_SPI)
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#endif

#if IS_ENABLED(CONFIG_SPI_MT65XX)
#include <linux/platform_data/spi-mt65xx.h>
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
#include <linux/sec_panel_notifier_v2.h>
#endif

extern void stui_tsp_init(int (*stui_tsp_enter)(void), int (*stui_tsp_exit)(void), int (*stui_tsp_type)(void));

#define NVT_DEBUG 1

//---SPI driver info.---
#define NVT_SPI_NAME "NVT-ts"

//---Input device info.---
#define NVT_TS_NAME "sec_touchscreen"

#define ENG_RST_ADDR 0x7FFF80

//---Touch info.---
#define TOUCH_DEFAULT_MAX_WIDTH 1800
#define TOUCH_DEFAULT_MAX_HEIGHT 2880

/* Enable only when module have tp reset pin and connected to host */
#define NVT_TOUCH_SUPPORT_HW_RST 0

//---Customerized func.---
#define NVT_TOUCH_PROC 1
#define NVT_TOUCH_EXT_PROC 1
#define WAKEUP_GESTURE 1
#define SEC_LPWG_DUMP 1
#define SEC_FW_STATUS 1

#if WAKEUP_GESTURE
#define GESTURE_DOUBLE_CLICK    15
#define GESTURE_SLIDE_UP        21
#endif

enum NVT_TSP_FW_INDEX {
	NVT_TSP_FW_IDX_BIN	= 0,
	NVT_TSP_FW_IDX_UMS	= 1,
	NVT_TSP_FW_IDX_MP	= 2,
};

#define POINT_DATA_CHECKSUM 1
#define POINT_DATA_CHECKSUM_LEN 65

#define NVT_SPI_RETRY_COUNT	3

//---ESD Protect.---
#define NVT_TOUCH_ESD_PROTECT 0
#define NVT_TOUCH_ESD_CHECK_PERIOD 1500	/* ms */
#define NVT_TOUCH_WDT_RECOVERY 1

#define TOUCH_PRINT_INFO_DWORK_TIME	30000	/* 30s */

struct nvt_ts_event_coord {
	u8 status:3;
	u8 id:5;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 w_major;
	u8 pressure_7_0;
} __packed;

struct nvt_firmware {
	u8 *data;
	size_t size;
};

#if SEC_LPWG_DUMP
struct nvt_ts_lpwg_coordinate_event {
	u8 event_type:2;
	u8 touch_status:2;
	u8 tid:2;
	u8 frame_count_9_8:2;
	u8 frame_count_7_0;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
} __packed;

struct nvt_ts_lpwg_gesture_event {
	u8 event_type:2;
	u8 gesture_id:6;
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
	u8 reserved_4;
} __packed;

struct nvt_ts_lpwg_vendor_event {
	u8 event_type:2;
	u8 info_type:6;
	u8 ng_code;
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
} __packed;

enum {
	LPWG_EVENT_COOR = 0x00,
	LPWG_EVENT_GESTURE = 0x01,
	LPWG_EVENT_VENDOR = 0x02,
	LPWG_EVENT_RESERVED_1 = 0x03
};

enum {
	LPWG_TOUCH_STATUS_PRESS = 0x00,
	LPWG_TOUCH_STATUS_RELEASE = 0x01,
	LPWG_TOUCH_STATUS_RESERVED_1 = 0x02,
	LPWG_TOUCH_STATUS_RESERVED_2 = 0x03
};

enum {
	LPWG_GESTURE_DT = 0x00,	// Double tap
	LPWG_GESTURE_SW = 0x01,	// Swipe up
	LPWG_GESTURE_RESERVED_1 = 0x02,
	LPWG_GESTURE_RESERVED_2 = 0x03
};

typedef enum {
	LPWG_DUMP_DISABLE = 0,
	LPWG_DUMP_ENABLE = 1,
} LPWG_DUMP;

#define LPWG_DUMP_PACKET_SIZE	5		/* 5 byte */
#define LPWG_DUMP_TOTAL_SIZE	500		/* 5 byte * 100 */
#endif

#define NVT_TS_REGULATOR_MAX	5

struct nvt_ts_data {
	struct spi_device *client;
	struct sec_ts_plat_data *plat_data;
	struct input_dev *input_dev;

	const char *firmware_name_mp;
	u32 open_test_spec[2];
	u32 short_test_spec[2];
	int diff_test_frame;
	u32 fdm_x_num;

	uint32_t eng_rst_addr;
	uint32_t spi_rd_fast_addr;	//read from dtsi

	int32_t reset_gpio;
	uint32_t reset_flags;
	struct mutex lock;
	struct mutex irq_lock;
	const struct nvt_ts_mem_map *mmap;
	uint8_t cascade_2nd_header_info;
	uint8_t hw_crc;
	uint8_t auto_copy;
	uint8_t bld_multi_header;
	uint16_t nvt_pid;
	uint8_t *rbuf;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t crc_err_flag_addr;
	bool is_cascade;
	bool enable_glove_mode;
	int debug_flag;
#if IS_ENABLED(CONFIG_MTK_SPI)
	struct mt_chip_conf spi_ctrl;
#endif
#if IS_ENABLED(CONFIG_SPI_MT65XX)
	struct mtk_chip_config spi_ctrl;
#endif
	struct sec_cmd_data sec;
	bool lcdoff_test;

	u16 landscape_deadzone[2];

	struct delayed_work work_read_info;
	struct delayed_work work_print_info;

	struct delayed_work notify_work;
	struct mutex notify_work_lock;
	int notify_type;

	const char *name_lcd_rst;
	struct regulator *regulator_lcd_rst;

	int regulator_count;
	const char *regulator_name[NVT_TS_REGULATOR_MAX];
	struct regulator *regulator[NVT_TS_REGULATOR_MAX];

#if SEC_FW_STATUS
	u16 fw_status_record;
#endif

	int fw_index;
	struct nvt_firmware	*cur_fw;
	struct nvt_firmware	*nvt_bin_fw;
	struct nvt_firmware	*nvt_mp_fw;
	struct nvt_firmware	*nvt_ums_fw;

#if SEC_LPWG_DUMP
	u8 *lpwg_dump_buf;
	u16 lpwg_dump_buf_idx;
	u16 lpwg_dump_buf_size;
#endif
	int lcd_esd_recovery;

	bool cmd_result_all_onboot;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
		struct mutex secure_lock; //TBD : move to sec_input.h
#endif
#if NVT_TOUCH_ESD_PROTECT
	struct delayed_work nvt_esd_check_work;
	struct workqueue_struct *nvt_esd_check_wq;
	unsigned long irq_timer;
	uint8_t esd_check;
	uint8_t esd_retry;
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	int grip_edgehandler_restore_data[SEC_CMD_PARAM_NUM];
	int setgrip_restore_data[SEC_CMD_PARAM_NUM];
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	struct notifier_block nvt_input_nb;
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER_V2)
	struct notifier_block lcd_nb;
#endif

};

#if NVT_TOUCH_PROC
struct nvt_flash_data{
	rwlock_t lock;
};
#endif

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN,	// normal run
	RESET_STATE_MAX  = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
	EVENT_MAP_HOST_CMD                     = 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE  = 0x51,
	EVENT_MAP_FUNCT_STATE                  = 0x5C,
	EVENT_MAP_RESET_COMPLETE               = 0x60,
	EVENT_MAP_FWINFO                       = 0x78,
	EVENT_MAP_PROXIMITY_SCAN               = 0x7F,
	EVENT_MAP_PANEL                        = 0x8F,
	EVENT_MAP_PROJECTID                    = 0x9A,
	EVENT_MAP_SENSITIVITY_DIFF             = 0x9D,	// 18 bytes
	EVENT_MAP_PROXIMITY_SUM                = 0xB0, // 2 bytes
	EVENT_MAP_PROXIMITY_THD                = 0xB2, // 2 bytes
} SPI_EVENT_MAP;

typedef enum {
	TEST_RESULT_PASS = 0x00,
	TEST_RESULT_FAIL = 0x01,
} TEST_RESULT;

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define NVT_TRANSFER_LEN	(63*1024)
#define NVT_READ_LEN		(2*1024)
#define BLOCK_64KB_NUM 4

#define DEEP_SLEEP_ENTER    0x11
#define LPWG_ENTER          0x13
#define PROX_SCAN_START     0x08
#define PROX_SCAN_STOP      0x09

#define NORMAL_MODE		0x00
#define TEST_MODE_1		0x21
#define TEST_MODE_2		0x22
#define MP_MODE_CC		0x41
#define FREQ_HOP_DISABLE	0x66
#define FREQ_HOP_ENABLE		0x65
#define HANDSHAKING_HOST_READY	0xBB

#define CHARGER_PLUG_OFF		0x51
#define CHARGER_PLUG_AC			0x53
#define GLOVE_ENTER				0xB1
#define GLOVE_LEAVE				0xB2
#define HOLSTER_ENTER			0xB5
#define HOLSTER_LEAVE			0xB6
#define EDGE_REJ_VERTICLE_MODE	0xBA
#define EDGE_REJ_LEFT_UP_MODE	0xBB
#define EDGE_REJ_RIGHT_UP_MODE	0xBC
#define HIGH_SENSITIVITY_ENTER	0xBD
#define HIGH_SENSITIVITY_LEAVE	0xBE
#define BLOCK_AREA_ENTER		0x71
#define BLOCK_AREA_LEAVE		0x72
#define EDGE_AREA_ENTER			0x73 //min:7
#define EDGE_AREA_LEAVE			0x74 //0~6
#define HOLE_AREA_ENTER			0x75 //0~120
#define HOLE_AREA_LEAVE			0x76 //no report
#define SPAY_SWIPE_ENTER		0x77
#define SPAY_SWIPE_LEAVE		0x78
#define DOUBLE_CLICK_ENTER		0x79
#define DOUBLE_CLICK_LEAVE		0x7A
#define SENSITIVITY_ENTER		0x7B
#define SENSITIVITY_LEAVE		0x7C
#define EXTENDED_CUSTOMIZED_CMD	0x7F

typedef enum {
	SET_GRIP_EXECPTION_ZONE = 1,
	SET_GRIP_PORTRAIT_MODE = 2,
	SET_GRIP_LANDSCAPE_MODE = 3,
	SET_PALM_MODE = 4,
	SET_TOUCH_DEBOUNCE = 5,	// for SIP
	SET_GAME_MODE = 6,
	SET_HIGH_SENSITIVITY_MODE = 7,
#if SEC_LPWG_DUMP
	SET_LPWG_DUMP = 0x0B,
#endif
	SET_SPEN_MODE = 0x0C,
} EXTENDED_CUSTOMIZED_CMD_TYPE;

typedef enum {
	PALM_DISABLE = 0,
	PALM_NORMAL = 1,
	PALM_SUPPORT_STYLUS = 2,	//to prevent stylus not working in NOTE APP
} PALM_MODE;

typedef enum {
	DEBOUNCE_NORMAL = 0,
	DEBOUNCE_LOWER = 1,		//to optimize tapping performance for SIP
} TOUCH_DEBOUNCE;

typedef enum {
	GAME_MODE_DISABLE = 0,
	GAME_MODE_ENABLE = 1,
} GAME_MODE;

typedef enum {
	HIGH_SENSITIVITY_DISABLE = 0,
	HIGH_SENSITIVITY_ENABLE = 1,
} HIGH_SENSITIVITY_MODE;

typedef enum {
	SPEN_MODE_DISABLE = 0,	// SPEN out range
	SPEN_MODE_ENABLE = 1,	// SPEN in range
} SPEN_MODE;

#define BUS_TRANSFER_LENGTH  256

#define XDATA_SECTOR_SIZE	256

#define CMD_RESULT_WORD_LEN	10

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))


/* function bit combination code */
#define EDGE_REJ_VERTICLE	1
#define EDGE_REJ_LEFT_UP	2
#define EDGE_REJ_RIGHT_UP	3

#define ORIENTATION_0		0
#define ORIENTATION_90		1
#define ORIENTATION_180		2
#define ORIENTATION_270		3

#define OPEN_SHORT_TEST		1
#define CHECK_ONLY_OPEN_TEST	1
#define CHECK_ONLY_SHORT_TEST	2

typedef enum {
	FUNCT_MIN		= 0,
	GLOVE			= 1,
	CHARGER			= 2,
	HOLSTER			= 4,
	EDGE_REJECT_L		= 5,
	EDGE_REJECT_H		= 6,
	EDGE_PIXEL		= 7,
	HOLE_PIXEL		= 8,
	SPAY_SWIPE		= 9,
	DOUBLE_CLICK		= 10,
	SENSITIVITY		= 11,
	BLOCK_AREA		= 12,
	HOPPING			= 13,
	HIGH_SENSITIVITY	= 14,
	FUNCT_MAX,
} FUNCT_BIT;

#define GLOVE_MASK		(1 << GLOVE)
#define CHARGER_MASK		(1 << CHARGER)
#define HOLSTER_MASK		(1 << HOLSTER)
#define EDGE_REJECT_MASK	((1 << EDGE_REJECT_L) | (1 << EDGE_REJECT_H))
#define EDGE_PIXEL_MASK		(1 << EDGE_PIXEL)
#define HOLE_PIXEL_MASK		(1 << HOLE_PIXEL)
#define SPAY_SWIPE_MASK		(1 << SPAY_SWIPE)
#define DOUBLE_CLICK_MASK	(1 << DOUBLE_CLICK)
#define SENSITIVITY_MASK	(1 << SENSITIVITY)
#define BLOCK_AREA_MASK		(1 << BLOCK_AREA)
#define HOPPING_MASK		(1 << HOPPING)
#define HIGH_SENSITIVITY_MASK	(1 << HIGH_SENSITIVITY)

#define FUNCT_ALL_MASK		(GLOVE_MASK | CHARGER_MASK | HOLSTER_MASK | EDGE_REJECT_MASK | EDGE_PIXEL_MASK |\
				HOLE_PIXEL_MASK | SPAY_SWIPE_MASK | DOUBLE_CLICK_MASK | SENSITIVITY_MASK | BLOCK_AREA_MASK |\
				HOPPING_MASK | HIGH_SENSITIVITY_MASK)

enum {
	BUILT_IN = 0,
	UMS,
	NONE,
	SPU,
};

#if SEC_FW_STATUS
enum {
	// 1st byte
	FW_STATUS_PRINT_OR_NOT = 0x01,
	FW_STATUS_WATER_FLAG = 0x02,
	FW_STATUS_PALM_FLAG = 0x04,
	FW_STATUS_HOPPING_FLAG = 0x08,
	FW_STATUS_BENDING_FLAG = 0x10,
	FW_STATUS_GLOVE_FLAG = 0x20,
	FW_STATUS_GND_UNSTABLE = 0x40,
	FW_STATUS_TA_PIN = 0x80,
	// 2nd byte
	FW_STATUS_REK_STATUS = (0x07 << 8),
	FW_STATUS_DIRTY_FLAG = (0x08 << 8),
};
#endif

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

//---extern structures---
extern struct nvt_ts_data *ts;

//---extern functions---
int32_t CTP_SPI_READ(struct spi_device *client, uint8_t *buf, uint16_t len);
int32_t CTP_SPI_WRITE(struct spi_device *client, uint8_t *buf, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_eng_reset(void);
void nvt_sw_reset(void);
void nvt_sw_reset_idle(void);
void nvt_boot_ready(void);
void nvt_fw_crc_enable(void);
int32_t nvt_update_firmware(const char *firmware_name);
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
int32_t nvt_get_fw_info(void);
int32_t nvt_clear_fw_status(void);
int32_t nvt_check_fw_status(void);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
int nvt_ts_mode_restore(struct nvt_ts_data *ts);
int nvt_ts_fw_update_from_bin(struct nvt_ts_data *ts);
int nvt_ts_fw_update_from_mp_bin(struct nvt_ts_data *ts, bool is_start);
int nvt_ts_fw_update_from_external(struct nvt_ts_data *ts);
//int nvt_ts_resume_pd(struct nvt_ts_data *ts);
int nvt_get_checksum(struct nvt_ts_data *ts, u8 *csum_result, u8 csum_size);
int32_t nvt_set_page(uint32_t addr);
int nvt_ts_cascade_ics_read(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len);
int nvt_ts_cascade_ics_write(struct nvt_ts_data *ts, u32 address, u8 *data, u16 len);
int nvt_ts_mode_switch_extended(struct nvt_ts_data *ts, u8 *cmd, u8 len, bool print_log);
int nvt_ts_mode_switch(struct nvt_ts_data *ts, u8 cmd, bool print_log);
void nvt_irq_enable(bool enable);
bool nvt_ts_lcd_power_check(void);
void nvt_ts_set_sleep_mode(struct nvt_ts_data *ts);
int nvt_ts_set_spen_mode(struct nvt_ts_data *ts, u8 mode);
void nvt_read_fw_history(uint32_t fw_history_addr);

int nvt_ts_sec_fn_init(struct nvt_ts_data *ts);
void nvt_ts_sec_fn_remove(struct nvt_ts_data *ts);

#if NVT_TOUCH_EXT_PROC
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
#endif

#if NVT_TOUCH_ESD_PROTECT
extern void nvt_esd_check_enable(uint8_t enable);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

u16 nvt_ts_mode_read(struct nvt_ts_data *ts);

void nvt_ts_early_resume(struct device *dev);
int32_t nvt_ts_resume(struct device *dev);
int32_t nvt_ts_suspend(struct device *dev);
void read_tsp_info_onboot(void *);
int nvt_sec_mp_parse_dt(struct nvt_ts_data *ts, const char *node_compatible);
#if SEC_LPWG_DUMP
int nvt_ts_lpwg_dump_buf_read(u8 *buf);
#endif
#endif /* _LINUX_NVT_TOUCH_H */
