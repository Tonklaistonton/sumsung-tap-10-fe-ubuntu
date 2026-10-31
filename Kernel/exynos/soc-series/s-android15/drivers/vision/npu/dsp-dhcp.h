#ifndef __DSP_DHCP_H__
#define __DSP_DHCP_H__

#include <linux/device.h>
#include <linux/platform_device.h>

struct dhcp_prof_info {
	u32 PROFILE_MAX_ITER;
	u32 PROFILE_SIZE;
	u32 PROFILE_IOVA;
};

struct dhcp_ivp_info {
    u32 PM_ADDR;
    u32 PM_SIZE;
    u32 DM_ADDR;
    u32 DM_SIZE;
};

struct dhcp_table {
	u32 DRIVER_SYNC_VERSION;
	u32 FIRMWARE_SYNC_VERSION;

	u32 CHIP_REV;
	u32 DNC_PWR_CTRL;
	u32 SECURE_BOOT;
	u32 RECOVERY_DISABLE;

	struct dhcp_ivp_info IVP_INFO;

	u32 PDCL_MBOX_ADDR;
	u32 PDCL_MBOX_RESULT;

	u32 DL_TABLE_ADDR;

	u32 PROF_MODE;
	struct dhcp_prof_info PROF_MAIN[32];
	struct dhcp_prof_info PROF_INFO[32];

	u32 IMB_INIT_INFO[4];

	u32 DUMP_MODE;
	u32 DUMP_START_ADDR;
	u32 DUMP_END_ADDR;

	u32 DVFS_TABLE_ADDR;

	u32 NPU_THERMAL;

	u32 CIDLE;

	u32 STM_ENABLE;

	u32 DUMP_DEBUG_PC;

	u32 TIME_IN_STATE[16];
	u32 PRESET_NPU_FREQ;

	u32 EMODE_VALUE;

	u32 ENABLE_APG;

	volatile u32 APG_STATE;
	volatile u32 APG_COUNT;
	volatile u32 APG_TIME;

	u32 ENABLE_DUMP_APG;

	u32 ENTER_WFI;
	u32 FORCE_SUSPEND_READY;
	u32 FORCE_SUSPEND_REQUEST;
};

union dsp_dhcp_pwr_ctl {
	u32	value;
	struct {
		u16	dsp_pm;
		u16	npu_pm;
	};
};

union dsp_dhcp_core_ctl {
	u32	value;
	struct {
		/* number of supported npu cores */
		u8	npu_cores_nr;
		/* active npu core bitmap */
		u8	npu_active_bm;
		/* number  of supported dsp cores */
		u8	dsp_cores_nr;
		/* active dsp core bitmap */
		u8	dsp_active_bm;
	};
};

struct npu_device;
struct npu_system;
struct npu_session;
int dsp_dhcp_update_pwr_status(struct npu_device *rdev,
			u32 type, bool on);
int dsp_dhcp_update_imb(struct npu_system *system,
			dma_addr_t daddr, size_t size);

int	dsp_dhcp_init(struct npu_device *rdev);

int	dsp_dhcp_probe(struct npu_device *dev);
int dsp_dhcp_get_master(struct npu_session *session, u32 type);

#endif
