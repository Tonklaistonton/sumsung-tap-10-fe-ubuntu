/*
***************************************************************************
*
*  Copyright (C) 2020 Advanced Micro Devices, Inc.  All rights reserved.
*
***************************************************************************/

#ifndef __AMDGPU_VANGOGH_REG_H__
#define __AMDGPU_VANGOGH_REG_H__

#define BG3D_PWRCTL_CTL1_OFFSET				0x0
#define BG3D_PWRCTL_CTL2_OFFSET				0x4
#define BG3D_PWRCTL_STATUS_OFFSET			0x8
#define BG3D_PWRCTL_LOCK_OFFSET				0xc
#define BG3D_PWRCTL_REFCNT				0x20
#define BG3D_PWRCTL_BUSYCNT				0x24
#define BG3D_PWRCTL_IFPOCNT				0x28

#define BG3D_PWRCTL_CTL2_GPUPWRREQ			0x12

#define BG3D_PWRCTL_STATUS_LOCK_VALUE__SHIFT		0x18
#define BG3D_PWRCTL_STATUS_GPU_READY_MASK		0x80000000L
#define BG3D_PWRCTL_STATUS_GPUPWRREQ_VALUE_MASK		0x40000000L
#define BG3D_PWRCTL_STATUS_LOCK_VALUE_MASK		0x3F000000L
#define BG3D_PWRCTL_STATUS_GPUPWRACK_MASK		0x00000002L

#define BG3D_PWRCTL_LOCK_GPU_READY_MASK			0x80000000L
#define BG3D_PWRCTL_LOCK_GPUPWRREQ_VALUE_MASK		0x40000000L

#define SMC_SAFE_WAIT_US				1000

#define GL2ACEM_RST_OFFSET				0x33A00
#define QUIESCE_WAIT_US					50
#define QUIESCE_TIMEOUT_US				10000

#define DIDT_IND_INDEX_MAX_VALUE			0x20

struct sgpu_didt_edc {
	bool		enable;

	bool		didt_enable;
	unsigned int	didt_threshold_freq;

	bool		edc_enable;
	unsigned int	edc_threshold_freq;

	uint32_t	values[DIDT_IND_INDEX_MAX_VALUE];

#ifdef CONFIG_DEBUG_FS
	struct dentry	*debugfs_didt_threshold_freq;
	struct dentry	*debugfs_edc_threshold_freq;
	struct dentry	*debugfs_didt_edc_values;
#endif
};

#endif
