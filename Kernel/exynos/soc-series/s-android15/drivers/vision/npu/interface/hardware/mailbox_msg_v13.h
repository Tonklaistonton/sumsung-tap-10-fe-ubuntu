/*
 * mailbox_msg_v13.h
 *
 *  Created on: 2024. 04. 22.
 */

#ifndef MAILBOX_MSG_V13_H_
#define MAILBOX_MSG_V13_H_

/*
PURGE : Intend of this command is clean-up a session in device driver.
Firmware shoud immediatly clean-up all the outstanting requests(Both processing and
network management) for specified uid. Firmware should response with DONE
message after the clean-up is completed. If the clean-up was unsuccessful,
the firmware will resonse NDONE message and driver will forcefully restart
NPU hardware. Before the firmware response DONE message for PURGE request,
DONE/NDONE response for outstanding processing requsests are generated.
Driver should properly handle those replies.

POWER_DOWN : Intentd of this command is make the NPU ready for power down.
Driver send this command to notify the NPU is about to power down. It implies
PURGE request for all valid uid - Firmware should clean-up all the out-standing
requests at once. After the clean-up is completed, Firmware should make sure that
there is no outstanding bus request to outside of NPU. After the check is done,
Firmware should issue WFI call to allow the NPU go power down safely and reply
DONE message via mailbox.
*/

#define MESSAGE_MAGIC			0xC0DECAFE
#define MESSAGE_MARK			0xDEADC0DE

#define CMD_LOAD_FLAG_IMB_MASK		0x7
#define CMD_LOAD_FLAG_IMB_PREFERENCE1	0x1 /* statically for alls */
#define CMD_LOAD_FLAG_IMB_PREFERENCE2	0x2 /* statically only for one */
#define CMD_LOAD_FLAG_IMB_PREFERENCE3	0x3 /* dynamically for alls */
#define CMD_LOAD_FLAG_IMB_PREFERENCE4   0x4
#define CMD_LOAD_FLAG_IMB_PREFERENCE5   0x5
#define CMD_LOAD_FLAG_STATIC_PRIORITY	(1 << 3)

/* payload size of load includes only ncp header */
struct cmd_load {
	u32 oid;        /* object id */
	u32 flags;      /* feature bit fields */
	u32 priority;   /* priority */
	u32 bound_id;   /* core bound id */
	u32 deadline;   /* task deadline time */
	u32 dl_uid;     /* dynamic loader uid */
};

struct cmd_unload {
	u32 oid;
};

#define CMD_PROCESS_FLAG_PROFILE_CTRL   (1 << 0) /* profile the ncp execution */
#define CMD_PROCESS_FLAG_BATCH          (1 << 1) /* this is a batch request */
#define CMD_PROCESS_FLAG_INTEROP        (1 << 2) /* this is a interop request */

struct cmd_process {
	u32 oid;
	u32 flags;
	u32 priority;
	u32 fid;        /* frame id or batch id for extended process cmd*/
	u32 deadline;
	u32 timestamp;
};

struct cmd_purge {
	u32 oid;
	u32 flags;
};

/**
 * @brief device class id
 * @details producer : framework + driver + compiler
 * @n consumer : firmware
 * @n description : device_id is
 * [19:16] device class ID (NP/GPU/HunterAE)
 * [15:12] AI cluster ID
 * [11:08] AI core ID
 * [07:04] Engine class ID (NPU/DSP/CA53)
 * [03:00] Engine ID
 */
struct cmd_pwr_ctl {
	u32 device_id;
	u32 flags;
};

struct cmd_fw_test {
	u32 test_type;  /* type of test(unit-test or safety-test) */
	u32 test_id;    /* number of testcase to be executed */
};

struct cmd_done {
	u32 oid;
	u32 fid;
	u32 flags;
	u32 data;
};

struct cmd_ndone {
	u32 oid;
	u32 fid;
	u32 flags;
	u32 error;      /* error code */
};

struct cmd_policy {
	u32 policy;
};

struct cmd_mode {
	u32 flags;
	u32 mode;
	u32 llc_size; /* llc size in bytes */
};

struct cmd_imb_size {
#if IS_ENABLED(CONFIG_SOC_S5E9955)
	u32 page_idx;
#else
	u32 flags;
#endif
	u32 imb_size; /* IMB dva size in bytes */
	u32 addr1;
	u32 size1;
	u32 addr2;
	u32 size2;
};

enum cmd_imb_type {
	IMB_ALLOC_TYPE_LOW = 0,             /* Set Bit 0 of imb_type to allocate in Low IMB Region */
	IMB_ALLOC_TYPE_HIGH,                /* Set Bit 1 of imb_type to allocate in High IMB Region */
	IMB_DEALLOC_TYPE_LOW,               /* Set Bit 2 of imb_type to deallocate in Low IMB Region */
	IMB_DEALLOC_TYPE_HIGH,              /* Set Bit 3 of imb_type to deallocate in High IMB Region */
};

struct cmd_imb_req {
	u32 imb_type;
	u32 imb_lsize;
	u32 imb_hsize;
};

struct cmd_imb_rsp {
	u32 imb_type;
	u32 imb_lsize;
	u32 imb_hsize;
};

struct cmd_suspend {
	u32 flags;
};

struct cmd_votf {
	u32 oid;
	u32 prop;
};


struct cmd_time_sync {
	u32 unit;           /* for time sync synchornization between the Host and FW. */
};

enum message_cmd {
	COMMAND_LOAD,
	COMMAND_UNLOAD,
	COMMAND_PROCESS,
	COMMAND_PURGE,
	COMMAND_PWR_CTL,
	COMMAND_FW_TEST,
	COMMAND_POLICY,
	COMMAND_MODE,
	COMMAND_IMB_SIZE,
	COMMAND_IMB_RSP,
	COMMAND_CANCEL,
	COMMAND_VOTF,
	COMMAND_SUSPEND,
	COMMAND_WFI,
	COMMAND_H2F_MAX_ID,
	COMMAND_DONE = 100,
	COMMAND_NDONE,
	COMMAND_IMB_REQ,
	COMMAND_TIME_SYNC,
	COMMAND_F2H_MAX_ID
};

struct message {
	u32             magic;          /* magic number */
	u32             mid;            /* message id */
	u32             command;        /* command id */
	u32             length;         /* size in bytes */
	u32             self;           /* self pointer */
	u32             data;           /* the pointer of command */
};

enum cmd_load_id {
	COMMAND_LOAD_USER_NCP,
	COMMAND_LOAD_HDR_COPY,
};

struct cmd_load_payload {
	u32             addr;           /* dma start addr */
	u32             size;
	u32             id;             /* enum cmd_load_id */
};

struct cmd_wfi {
	u32 on;
	u32 page_idx;
};

struct command {
	union {
		struct cmd_load         load;
		struct cmd_unload       unload;
		struct cmd_process      process;
		struct cmd_fw_test      fw_test;
		struct cmd_purge        purge;
		struct cmd_pwr_ctl      pwr_ctl;
		struct cmd_policy       policy;
		struct cmd_mode         mode;
		struct cmd_done         done;
		struct cmd_ndone        ndone;
		struct cmd_imb_size     imb;
		struct cmd_imb_req      imb_req;
		struct cmd_imb_rsp      imb_rsp;
		struct cmd_suspend      suspend;
		struct cmd_votf         votf;
		struct cmd_time_sync    time_sync;
		struct cmd_wfi          wfi;
	} c; /* specific command properties */

	u32             length; /* the size of payload */
	u32             payload;
};

#endif
