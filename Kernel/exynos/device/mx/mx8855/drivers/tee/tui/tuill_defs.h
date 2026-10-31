/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * TUI LL device driver definitions header
 * This copy is for tuihwll driver
 *
 * There are two other copies of this file in
 * /TuiService/tui_service_jni/jni/tuill_defs.h and
 * /tzsl/src/tuill/common/tuill_defs.h
 */

#pragma once

#ifndef __USED_BY_TZSL__
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

//typedef void		*void_ptr;
//typedef void_ptr	addr_t;
//#define containerof(ptr, type, member)
//	((type *)((addr_t)(ptr) - offsetof(type, member)))

#define USEC_IN_MSEC  1000
#define MSEC_IN_SEC   1000
#define NSEC_IN_MSEC  1000000

#define OS_SWD_SOCKET_NAME  "tuill_swd_server"
#define OS_IWD_SOCKET_NAME  "tuill_iwd_server"
#define TOUCH_DRIVER_NAME   "tuill_touchdrv"
#define DISPLAY_DRIVER_NAME "tuill_dispdrv"
#define OS_DRIVER_NAME      "tuill_osdrv"

#define TUILLD_OSDRV_TA_NAME    "/dev/"OS_DRIVER_NAME
#define TUILLD_DISPDRV_TA_NAME  "/dev/"DISPLAY_DRIVER_NAME
#define TUILLD_TOUCHDRV_TA_NAME "/dev/"TOUCH_DRIVER_NAME

#define TUILL_SERVER_PREFIX   "/service/"
#define TUILL_SERVER_TEMPLATE TUILL_SERVER_PREFIX "%s"

#define TUILL_API_VERSION  1

#define MAX_DISPLAY 1

//--- Internal TUI LL spec types definition ---------------------------------
//these three constants weren't defined in TUI LL spec
//and shouldn't be visible for user, but we need them
#define TEE_PERIPHERAL_DISPLAY 0xABABABAB
#define TEE_PERIPHERAL_SERVICE 0xABABABAC
#define TEE_PERIPHERAL_TUIHW   0xABABABAD

#define TUILL_INVALID_HANDLE_VALUE (-1)
#define TUILL_OPENED_HANDLE_VALUE    1

#define STUI_DISPLAY_INFO_SIZE 10

typedef uint32_t tuill_handle_t;

struct __TEE_EventQueueHandle {
	tuill_handle_t hndl;
};

struct __TEE_EventSourceHandle {
	tuill_handle_t hndl;
};

struct __TEE_PeripheralHandle {
	tuill_handle_t hndl;
};

//--- Configuration enums definition ----------------------------------------
enum TUILL_IO_SOCKET {
	TUILLDRV_SWD_SOCK,
	TUILLDRV_IWD_SOCK,
	TUILLDRV_SOCK_MAX
};

enum tuill_swd_components {
	//enum order must be the same as in tuill_drivers
	TUILL_TOUCH,
	TUILL_DISPLAY,
	TUILL_SWD_MAX,
};

enum tuill_iwd_components {
	TUILL_TUIHW,
	TUILL_SERVICE,
	TUILL_IWD_MAX,
};

enum tuill_drivers {
	//used as TEE_PeripheralId
	TUILL_OS_DRV,
	//enum order must be the same as in tuill_swd_components
	TUILL_TOUCH_DRV,
	TUILL_DISPLAY_DRV,
	TUILL_DRV_MAX,
};

struct tuilldrv_touch_data {
	uint32_t display;
	uint32_t action;
	uint32_t finger;
	uint32_t pressure;
	uint32_t x;
	uint32_t y;
} __attribute__((packed, aligned(4)));

struct tuilldrv_tee_data {
	uint32_t event;
} __attribute__((packed, aligned(4)));

struct tuilldrv_send_event_data {
	uint32_t periph_id;
	uint32_t event_type;
	uint32_t ret_code;
	union {
	struct tuilldrv_touch_data touch;
	struct tuilldrv_tee_data   tee;
	} u;
} __attribute__((packed, aligned(4)));

//--- Socket commands and structures definition -----------------------------
//must match enum TUIInternalCommand in TUICmdWrapper.java
enum tuill_internal_commands {
	TUILL_ICMD_PING,
	//peripheral API
	TUILL_ICMD_GET_PERIPHERAL_LIST,
	TUILL_ICMD_CLOSE_PERIPHERAL, //single and multiple
	TUILL_ICMD_OPEN_PERIPHERAL,  //single and multiple
	//event API
	TUILL_ICMD_ADD_SOURCES,
	TUILL_ICMD_CANCEL_SOURCES,
	TUILL_ICMD_CLOSE_CLIENT_QUEUE,
	TUILL_ICMD_DROP_SOURCES,
	TUILL_ICMD_LIST_SOURCES,
	TUILL_ICMD_OPEN_CLIENT_QUEUE,
	TUILL_ICMD_WAIT_EVENT,
	//tui API
	TUILL_ICMD_GET_DISPLAY_INFO,
	TUILL_ICMD_TUI_INIT_SESSION_LOW,
	TUILL_ICMD_TUI_CLOSE_SESSION,
	TUILL_ICMD_BLIT_DISPLAY_SURFACE,
	//internal API
	TUILL_ICMD_SET_DRV_STATE,
	TUILL_ICMD_TOUCH_EVENT,
	TUILL_ICMD_OPEN_DRIVER,
	TUILL_ICMD_CLOSE_DRIVER,
	TUILL_ICMD_REBOOT_PHONE,
	TUILL_ICMD_CANCEL_TUI,
	TUILL_ICMD_DRIVER_CLOSED,
	TUILL_ICMD_TIMER_CREATE,

#ifdef BUILD_TYPE_debug
	//os_drv debug commands
	TUILL_ICMD_SEND_EVENT,
	TUILL_ICMD_REBOOT,
	TUILL_ICMD_SEND_ERRDATA,
	TUILL_ICMD_TEST_DRV_EVENT,
#endif //BUILD_TYPE_debug

	TUILL_ICMD_MAX,
};

#define RESPONSE_FLAG     0x80000000
#define INJECT_ERR_FLAG   0x40000000 //for debugging, injects error code
#define MAKE_TIMEOUT_FLAG 0x20000000 //for debugging, makes timeout error

struct FB_Data {
	//we need to return this data in case if we opened display
	uint32_t width;
	uint32_t height;
	uint64_t fb_physical;
	uint64_t fb_virtual;
	uint64_t fb_size;
	uint64_t wb_physical;
	uint64_t wb_virtual;
	uint64_t wb_size;
	uint64_t disp_physical;
	uint64_t disp_size;
	uint32_t touch_type;
	uint64_t lcd_info[STUI_DISPLAY_INFO_SIZE];
	uint32_t disp_if;
} __attribute__((packed, aligned(4)));

struct OpenPeripheral_cmd {
	uint32_t num;
	uint32_t peripheral_id[TUILL_DRV_MAX];
	uint32_t flags;
	struct FB_Data FB;
} __attribute__((packed, aligned(4)));

struct OpenPeripheral_rsp {
	struct FB_Data FB;
} __attribute__((packed, aligned(4)));

struct ClosePeripheral_cmd {
	uint32_t num;
	uint32_t peripheral_id[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4)));

#define TEE_MAX_EVENT_PAYLOAD_SIZE 32
#define TEE_MAX_EVENT_NUMBER       4

struct __TEE_Event_V1 {
	uint32_t       eventType;
	uint64_t       timestamp;
	tuill_handle_t event_handle;
	uint8_t        payload[TEE_MAX_EVENT_PAYLOAD_SIZE];
};

struct __TEE_Event {
	uint32_t version;
	uint32_t peripheral_id;
	union {
		struct __TEE_Event_V1 v1;
	} u;
};

//event commands start
struct AddSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4)));

struct CancelSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4)));

struct CloseQueue_cmd {
	tuill_handle_t queue_handle;
} __attribute__((packed, aligned(4)));

struct DropSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4)));

struct ListSources_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_sources;
} __attribute__((packed, aligned(4)));

struct ListSources_rsp {
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4)));

struct OpenQueue_cmd {
	uint32_t       timeout_ms;
	uint32_t       num_sources;
	tuill_handle_t sources[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4)));

struct OpenQueue_rsp {
	tuill_handle_t queue_handle;
} __attribute__((packed, aligned(4)));

struct WaitEvent_cmd {
	tuill_handle_t queue_handle;
	uint32_t       num_events;
	uint32_t       timeout_ms;
} __attribute__((packed, aligned(4)));

struct WaitEvent_rsp {
	uint32_t num_events;
	uint32_t dropped;
		struct __TEE_Event events[TEE_MAX_EVENT_NUMBER];
} __attribute__((packed, aligned(4)));

struct TouchEvent_cmd {
	uint32_t display;
	uint32_t action;
	uint32_t finger;
	uint32_t pressure;
	uint32_t x;
	uint32_t y;
} __attribute__((packed, aligned(4)));

struct OsEvent_cmd {
	uint32_t periph_id;
	uint32_t event_type;
	uint32_t event;
} __attribute__((packed, aligned(4)));

//event commands end

struct SetDrvInfo_cmd {
	uint32_t drv_tui_mode;
	uint32_t index; //from tuill_iwd_components
} __attribute__((packed, aligned(4)));

struct GetDisplayInfo_cmd {
	uint32_t version;
	uint32_t displayNumber;
} __attribute__((packed, aligned(4)));

struct GetDisplayInfo_rsp {
	uint32_t physical_width;
	uint32_t physical_height;
	uint32_t pixel_width;
	uint32_t pixel_height;
	uint32_t bit_depth;
	uint32_t flags;
	uint32_t num_periph;
	uint32_t associatedPeripherals[TUILL_DRV_MAX];
	uint64_t lcd_info[STUI_DISPLAY_INFO_SIZE];
	uint32_t disp_if;
} __attribute__((packed, aligned(4)));

struct peripheral_info {
	uint32_t type;
	uint32_t id;
} __attribute__((packed, aligned(4)));

struct GetPeripheralList_rsp {
	uint32_t num;
	struct peripheral_info list[TUILL_DRV_MAX];
} __attribute__((packed, aligned(4)));

struct InitTUISession_cmd {
	uint32_t flags;
	uint32_t timeout_ms;
	uint32_t peripheral_id[TUILL_DRV_MAX];
	uint32_t num;
} __attribute__((packed, aligned(4)));

struct CancelTUI_cmd {
	uint32_t event;
} __attribute__((packed, aligned(4)));

struct tuill_internal_command {
	uint32_t cmd;
	uint32_t ret_code;
	uint32_t version;
	uint32_t task_state; //duplicates field in long_cmd_t
	uint32_t task_id;    //duplicates field in long_cmd_t
	union {
		//event commands start
		struct AddSources_cmd    AddSrcs_cmd;
		struct CancelSources_cmd CancelSrcs_cmd;
		struct CloseQueue_cmd    CloseQue_cmd;
		struct DropSources_cmd   DropSrcs_cmd;
		struct ListSources_cmd   ListSrcs_cmd;
		struct ListSources_rsp   ListSrcs_rsp;
		struct OpenQueue_cmd     OpenQue_cmd;
		struct OpenQueue_rsp     OpenQue_rsp;
		struct WaitEvent_cmd     WaitEvnt_cmd;
		struct WaitEvent_rsp     WaitEvnt_rsp;
		struct TouchEvent_cmd    TouchEvnt_cmd;
		struct OsEvent_cmd       OsEvnt_cmd;
		//event commands end
		//peripheral commands start
		struct GetPeripheralList_rsp GetPeripheralLst_rsp;
		struct OpenPeripheral_cmd    OpenPeriph_cmd;
		struct OpenPeripheral_rsp    OpenPeriph_rsp;
		struct ClosePeripheral_cmd   ClosePeriph_cmd;
		//peripheral commands end
		//tui commands start
		struct GetDisplayInfo_cmd    GetDisplayInf_cmd;
		struct GetDisplayInfo_rsp    GetDisplayInf_rsp;
		struct InitTUISession_cmd    InitTUISess_cmd;
		//tui commands end
		//other commands start
		struct SetDrvInfo_cmd        SetDrvInf_cmd;
		struct OpenPeripheral_cmd    OpenDrvrs_cmd;
		struct OpenPeripheral_rsp    OpenDrvrs_rsp;
		struct ClosePeripheral_cmd   CloseDrvrs_cmd;
		struct CancelTUI_cmd         CnclTUI_cmd;
		//other commands end
	};
} __attribute__((packed, aligned(4)));

//--- Engine structures definition ------------------------------------------

struct tuill_buffer {
	int32_t data_len;
	char    data[sizeof(struct tuill_internal_command)];
} __attribute__((packed, aligned(4)));

typedef int32_t (*receive_t)(int32_t fd, struct tuill_buffer *data);
typedef void (*process_t)(int32_t fd);

struct tuill_callback {
	receive_t process_input;
	process_t process_hangup;
	process_t process_handhake;
};

enum TUILL_CLIENT_TYPE {
	TUILLDRV_CLIENT_SWD_DRV,
	TUILLDRV_CLIENT_SWD_CLIENT,
	TUILLDRV_CLIENT_IWD_CLIENT,
};

//typedef struct tuill_socket_params {
//	int32_t sfd; //socket fd
//	int32_t efd; //epoll fd
//} tuill_socket_params_t;

//common drv information
struct tuill_drv_ctx {
	uint32_t index;          //in drv_arr
	uint32_t peripheral_type;//GP enum values
	uint32_t peripheral_id;  //internal enum values
	uint32_t tuill_state;    //TEE_PERIPHERAL_STATE_FLAGS
};

//----- server side contexts for connected entities -------
struct tuill_drv_entity_ctx {
	struct tuill_drv_ctx drv_ctx;//shows that driver is connected to system
};

//used to indicate opened state for drivers
struct tuill_opened {
	bool  tui; //opened by TEE_TUI_InitSessionLow
	bool  interrupted; //message about this driver interrupt was received
	tuill_handle_t periph_hndl; //indicates session is opened
	uint32_t peripheral_type;
	uint32_t peripheral_id;
};

enum task_type {
	TASK_NONE,
	TASK_OPEN,
	TASK_CLOSE,
	TASK_DISPLAYINFO,
	TASK_CLOSE_NWD
};

enum task_cmd_state {
	TASK_CMD_VOID,
	TASK_CMD_NEEDED,
	TASK_CMD_SENT,
	TASK_CMD_DONE,
	TASK_CMD_ERROR,
	TASK_CMD_UNDO_SENT,
	TASK_CMD_UNDONE,
};

enum task_state {
	TASK_STATE_VOID,
	TASK_STATE_DOING,
	TASK_STATE_UNDOING,
};



struct tuill_iwd_ctx {
	uint32_t index; //from tuill_iwd_components
	uint32_t tui_mode;
};
//---------------------------------------------------------
