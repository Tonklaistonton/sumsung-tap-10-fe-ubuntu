// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *               http://www.samsung.com
 */

#include <linux/pm_runtime.h>

#include "soc15_common.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "vangogh_lite_reg.h"
#include "sgpu_debug.h"
#include "sgpu_sysfs.h"

#include <soc/samsung/exynos/debug-snapshot.h>

#define RLC_SAFE_MODE_ENTRY_DONE    0x2
#define RLC_SAFE_MODE_EXIT_DONE     0x0

#define RLC_SAFE_MODE_POLLING_TIME_US	(1000)

#define IFPO_COUNT_KERNEL		0x00000001
#define IFPO_COUNT_KERNEL_MASK		0x000000FF
#define IFPO_COUNT_KERNEL_SHIFT		0
#define IFPO_COUNT_DEBUGFS		0x00004000
#define IFPO_COUNT_DEBUGFS_MASK		0x00004000
#define IFPO_COUNT_DEBUGFS_SHIFT	14
#define IFPO_COUNT_SYSFS		0x00008000
#define IFPO_COUNT_SYSFS_MASK		0x00008000
#define IFPO_COUNT_SYSFS_SHIFT		15
#define IFPO_COUNT_USER			0x00010000
#define IFPO_COUNT_USER_MASK		0x7FFF0000
#define IFPO_COUNT_USER_SHIFT		16

uint32_t sgpu_ifpo_get_hw_lock_value(struct amdgpu_device *adev)
{
	return ((readl_relaxed(adev->pm.pmu_mmio + BG3D_PWRCTL_STATUS_OFFSET) &
			BG3D_PWRCTL_STATUS_LOCK_VALUE_MASK) >>
			BG3D_PWRCTL_STATUS_LOCK_VALUE__SHIFT);
}

static void sgpu_ifpo_wait_rlc_safemode(struct amdgpu_device *adev, uint32_t val)
{
	int timeout_us = RLC_SAFE_MODE_POLLING_TIME_US;
	uint32_t rdata = 0;

	/* wait for RLC_SAFE_MODE */
	while (timeout_us-- > 0) {
		rdata = RREG32_SOC15(GC, 0, mmRLC_SAFE_MODE);
		if (rdata == val)
			return;
		udelay(1);
	}

	DRM_ERROR("HW defect detected: timeout, mmRLC_SAFE_MODE(0x%x/0x%x)", val, rdata);
	sgpu_debug_snapshot_expire_watchdog();
}

static void sgpu_ifpo_wait_force_idle(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	unsigned long r;

	r = wait_for_completion_interruptible_timeout(
					&ifpo->force_idle,
					msecs_to_jiffies(1000));
	if (r == 0) {
		dev_err(adev->dev, "Wait over 1sec for IFPO\n");
		sgpu_debug_snapshot_expire_watchdog();
	}
}

/*
 * IFPO half automation APIs
 * GPU can go to IFPO power off state if HW counter gets 0. The driver should
 * specify periods which GPU must be powered on, by using lock/unlock function.
 */

static void sgpu_ifpo_half_enable(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;
	uint32_t data = 0;

	/* Enable CP interrupts */
	data = RREG32_SOC15(GC, 0, mmCP_INT_CNTL);
	data = REG_SET_FIELD(data, CP_INT_CNTL, CMP_BUSY_INT_ENABLE, 1);
	data = REG_SET_FIELD(data, CP_INT_CNTL, CNTX_BUSY_INT_ENABLE, 1);
	data = REG_SET_FIELD(data, CP_INT_CNTL, CNTX_EMPTY_INT_ENABLE, 1);
	data = REG_SET_FIELD(data, CP_INT_CNTL, GFX_IDLE_INT_ENABLE, 1);
	WREG32_SOC15(GC, 0, mmCP_INT_CNTL, data);

	/* Enabling Save/Restore FSM */
	data = REG_SET_FIELD(0, RLC_SRM_CNTL, SRM_ENABLE, 1);
	data = REG_SET_FIELD(data, RLC_SRM_CNTL, AUTO_INCR_ADDR, 1);
	WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, data);

	/* RLC Safe mode entry */
	data = REG_SET_FIELD(0, RLC_SAFE_MODE, CMD, 1);
	data = REG_SET_FIELD(data, RLC_SAFE_MODE, MESSAGE, 1);
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, data);

	/* check RLC Safe mode entry */
	sgpu_ifpo_wait_rlc_safemode(adev, RLC_SAFE_MODE_ENTRY_DONE);

	/* Initialize PM/IFPO registers */
	WREG32_SOC15(GC, 0, mmRLC_GPM_TIMER_INT_3, 0x186A);	// GFXOFF timer – 250us
	data = REG_SET_FIELD(0, RLC_PG_CNTL, GFX_POWER_GATING_ENABLE, 1);
	WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, data);

	/* Default IFPO enable setting */
	writel(0x89, addr + BG3D_PWRCTL_CTL2_OFFSET);	// COLDVSGFXOFF|FAXI_MODE|PMU_INTF_EN

	/* RLC Safe mode exit */
	data = REG_SET_FIELD(0, RLC_SAFE_MODE, CMD, 1);
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, data);

	/* check RLC Safe mode exit */
	sgpu_ifpo_wait_rlc_safemode(adev, RLC_SAFE_MODE_EXIT_DONE);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO half init");
}

static void sgpu_ifpo_half_reset(struct amdgpu_device *adev)
{
	writel_relaxed(0x12, adev->pm.pmu_mmio + BG3D_PWRCTL_CTL2_OFFSET);
}

static inline bool __sgpu_ifpo_half_lock(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;
	uint32_t gpu_status = 0, data = 0;
	bool gpu_wakeup = false;

	gpu_status = readl_relaxed(addr + BG3D_PWRCTL_LOCK_OFFSET);

	/* Check if GPU was already powered off or middle of powering down */
	if ((gpu_status & BG3D_PWRCTL_LOCK_GPU_READY_MASK) == 0) {
		do {
			/* Dummy write to wake up GPU and disable power gating */
			WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, 0x0);
			/* Increase lock counter */
			gpu_status = readl(addr + BG3D_PWRCTL_LOCK_OFFSET);
		} while ((gpu_status & BG3D_PWRCTL_LOCK_GPU_READY_MASK) == 0);

		/* Enable Power Gating */
		data = REG_SET_FIELD(0, RLC_PG_CNTL, GFX_POWER_GATING_ENABLE, 1);
		WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, data);

		gpu_wakeup = true;

		vangogh_lite_set_didt_edc(adev, 0);
	}

	trace_sgpu_ifpo_power_on(gpu_wakeup);

	return gpu_wakeup;
}

static void sgpu_ifpo_half_lock(struct amdgpu_device *adev, bool nowait)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	int count;
	bool gpu_wakeup;

	if (addr == NULL)
		return;

	if (!nowait)
		sgpu_ifpo_wait_force_idle(adev);

	gpu_wakeup = __sgpu_ifpo_half_lock(adev);
	count = atomic_add_return(IFPO_COUNT_KERNEL, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
			"IFPO LOCK : hw(%#x) sw(%#x) wakeup(%u)",
			sgpu_ifpo_get_hw_lock_value(adev), count, gpu_wakeup);
}

static inline void __sgpu_ifpo_half_unlock(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;
	uint32_t hw_counter;

	/* Decrease lock counter */
	writel_relaxed(0x0, addr + BG3D_PWRCTL_LOCK_OFFSET);

	hw_counter = sgpu_ifpo_get_hw_lock_value(adev);
	trace_sgpu_ifpo_power_off(hw_counter);
}

static void sgpu_ifpo_half_unlock(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	int count;

	if (addr == NULL)
		return;

	__sgpu_ifpo_half_unlock(adev);
	count = atomic_sub_return(IFPO_COUNT_KERNEL, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO UNLOCK : hw(%#x) sw(%#x)",
			sgpu_ifpo_get_hw_lock_value(adev), count);
}

static void sgpu_ifpo_half_suspend(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	int count = atomic_add_return(IFPO_COUNT_KERNEL, &ifpo->count);

	/* GPU should be IFPO power on state before entering suspend */
	__sgpu_ifpo_half_lock(adev);
	/* reset ColdGfxOff = 2'b00 before entering suspend */
	sgpu_ifpo_half_reset(adev);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO SUSPEND : hw(%#x) sw(%#x)",
			sgpu_ifpo_get_hw_lock_value(adev), count);
}

static void sgpu_ifpo_half_resume(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	int ifpo_count = atomic_sub_return(IFPO_COUNT_KERNEL, &ifpo->count);
	int disable_count = 0;

	/*
	 * hw counter could be reset if GPU has been entered sleep mode,
	 * it's needed to clear both counters
	 */
	writel_relaxed(~0, addr + BG3D_PWRCTL_LOCK_OFFSET);

	disable_count += (ifpo_count & IFPO_COUNT_USER_MASK) >> IFPO_COUNT_USER_SHIFT;
	disable_count += (ifpo_count & IFPO_COUNT_DEBUGFS_MASK) >> IFPO_COUNT_DEBUGFS_SHIFT;
	disable_count += (ifpo_count & IFPO_COUNT_SYSFS_MASK) >> IFPO_COUNT_SYSFS_SHIFT;

	while (disable_count--)
		__sgpu_ifpo_half_lock(adev);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO RESUME : hw(%#x) sw(%#x)",
			sgpu_ifpo_get_hw_lock_value(adev), ifpo_count);
}

static void sgpu_ifpo_half_user_disable(struct amdgpu_device *adev, int count, bool disable)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	int ifpo_count, pm_rc;

	switch (count) {
	case IFPO_COUNT_USER:
	case IFPO_COUNT_DEBUGFS:
	case IFPO_COUNT_SYSFS:
		break;
	default:
		WARN(1, "Invalid request (%#x)\n", count);
		break;
	}

	/*
	 * IFPO count should be changed while it's not in force_idle period.
	 * Asynchronous setting of user request could lead mismatch of
	 * IFPO counter between SW and HW if GVF scenario is running.
	 */
	sgpu_ifpo_wait_force_idle(adev);

	ifpo_count = disable ? atomic_add_return(count, &ifpo->count) :
				atomic_sub_return(count, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO USER DISABLE : count(%#x)", ifpo_count);

	pm_rc = pm_runtime_get_if_active(adev_to_drm(adev)->dev, true);

	if (!pm_rc)
		return;

	if (disable)
		/* Increase lock counter to disable IFPO */
		__sgpu_ifpo_half_lock(adev);
	else
		/* Decrease lock counter to enable IFPO */
		__sgpu_ifpo_half_unlock(adev);

	if (pm_rc == 1)
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
}

static void sgpu_ifpo_half_force_enter(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	u32 ih_wptr;

	reinit_completion(&ifpo->force_idle);

	if (atomic_read(&ifpo->count) > IFPO_COUNT_KERNEL_MASK)
		return;

	/* Make sure that no pending IRQ exist */
	do {
		ih_wptr = amdgpu_ih_get_wptr(adev, &adev->irq.ih);
	} while (adev->irq.ih.last_rptr != ih_wptr);

	/* Clear HW counter to enter GFXOFF state */
	writel_relaxed(~0, addr + BG3D_PWRCTL_LOCK_OFFSET);

	trace_sgpu_ifpo_power_off(0);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
			"IFPO FORCE ENTER : hw(%#x) sw(%#x)",
			sgpu_ifpo_get_hw_lock_value(adev),
			atomic_read(&ifpo->count));
}

static void sgpu_ifpo_half_force_exit(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	int count, ifpo_count = atomic_read(&ifpo->count);

	if (ifpo_count > IFPO_COUNT_KERNEL_MASK)
		goto exit;

	count = ifpo_count & IFPO_COUNT_KERNEL_MASK;
	if (count > 0) {
		__sgpu_ifpo_half_lock(adev);

		/* Restore IFPO HW counter */
		while (count--)
			readl_relaxed(addr + BG3D_PWRCTL_LOCK_OFFSET);

		__sgpu_ifpo_half_unlock(adev);
	}

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
			"IFPO FORCE EXIT : hw(%#x) sw(%#x)",
			sgpu_ifpo_get_hw_lock_value(adev),
			atomic_read(&ifpo->count));
exit:
	complete_all(&ifpo->force_idle);
}

static struct sgpu_ifpo_func sgpu_ifpo_half_func = {
	.enable = sgpu_ifpo_half_enable,
	.suspend = sgpu_ifpo_half_suspend,
	.resume = sgpu_ifpo_half_resume,
	.lock = sgpu_ifpo_half_lock,
	.unlock = sgpu_ifpo_half_unlock,
	.reset = sgpu_ifpo_half_reset,
	.user_disable = sgpu_ifpo_half_user_disable,
	.force_enter = sgpu_ifpo_half_force_enter,
	.force_exit = sgpu_ifpo_half_force_exit,
};


/*
 * IFPO abort APIs
 * The driver can request IFPO power on even if IFPO power down sequence is
 * running. PMU abort function is required to support this strategy.
 */

static void sgpu_ifpo_abort_enable(struct amdgpu_device *adev)
{
	void *addr = adev->pm.pmu_mmio;
	uint32_t data = 0;

	// Enable CP interrupt configuration
	data = RREG32_SOC15(GC, 0, mmCP_INT_CNTL);
	data = REG_SET_FIELD(data, CP_INT_CNTL, CMP_BUSY_INT_ENABLE, 1);
	data = REG_SET_FIELD(data, CP_INT_CNTL, CNTX_BUSY_INT_ENABLE, 1);
	data = REG_SET_FIELD(data, CP_INT_CNTL, CNTX_EMPTY_INT_ENABLE, 1);
	data = REG_SET_FIELD(data, CP_INT_CNTL, GFX_IDLE_INT_ENABLE, 1);
	WREG32_SOC15(GC, 0, mmCP_INT_CNTL, data);

	// RLC Safe mode entry
	data = REG_SET_FIELD(0, RLC_SAFE_MODE, CMD, 1);
	data = REG_SET_FIELD(data, RLC_SAFE_MODE, MESSAGE, 1);
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, data);

	/* check RLC Safe mode entry */
	sgpu_ifpo_wait_rlc_safemode(adev, RLC_SAFE_MODE_ENTRY_DONE);

	// Initialize PM/IFPO registers
	WREG32_SOC15(GC, 0, mmRLC_GPM_TIMER_INT_3, 0x800);
	data = REG_SET_FIELD(0, RLC_PG_CNTL, GFX_POWER_GATING_ENABLE, 1);
	WREG32_SOC15(GC, 0, mmRLC_PG_CNTL, data);
	writel(0x8a, addr + BG3D_PWRCTL_CTL2_OFFSET);		// COLDVSGFXOFF|FAXI_MODE|GPUPWRREQ

	data = REG_SET_FIELD(0, RLC_SAFE_MODE, CMD, 1);
	WREG32_SOC15(GC, 0, mmRLC_SAFE_MODE, data);

	/* check RLC Safe mode exit */
	sgpu_ifpo_wait_rlc_safemode(adev, RLC_SAFE_MODE_EXIT_DONE);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO abort init");
}

static void sgpu_ifpo_abort_reset(struct amdgpu_device *adev)
{
	writel(0x12, adev->pm.pmu_mmio + BG3D_PWRCTL_CTL2_OFFSET);
}

static void __sgpu_ifpo_abort_power_on(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	void *addr = adev->pm.pmu_mmio;
	uint32_t gpu_status, data, timeout;

	if (ifpo->state)
		return;

	/* BG3D_DVFS_CTL 0x058 enable */
	writel(0x1, adev->pm.pmu_mmio + 0x58);

	gpu_status = readl(addr + BG3D_PWRCTL_LOCK_OFFSET);

	if (gpu_status & BG3D_PWRCTL_LOCK_GPU_READY_MASK) {
		/* Disable IFPO */
		writel(0x8a, addr + BG3D_PWRCTL_CTL2_OFFSET);
		/* Decrement lock counter */
		writel(0x0, addr + BG3D_PWRCTL_LOCK_OFFSET);

		trace_sgpu_ifpo_power_on(0);
	} else {
		do {
			gpu_status = readl(addr + BG3D_PWRCTL_STATUS_OFFSET);
		} while ((gpu_status & (BG3D_PWRCTL_STATUS_GPUPWRREQ_VALUE_MASK |
					BG3D_PWRCTL_STATUS_GPUPWRACK_MASK)) != 0);

		/* Disable IFPO, and keep GPUPWRREQ=0 */
		writel(0x88, addr + BG3D_PWRCTL_CTL2_OFFSET);

		/* Set GPUPWRREQ=1 */
		writel(0x8a, addr + BG3D_PWRCTL_CTL2_OFFSET);

		dbg_snapshot_pmu(ifpo->cal_id, __func__, DSS_FLAG_IN);

		/* PMUCAL_WAIT has 200ms for timeout */
		timeout = 200000;
		do {
			udelay(1);
			--timeout;
			gpu_status = readl(addr + BG3D_PWRCTL_STATUS_OFFSET);
		} while ((gpu_status & BG3D_PWRCTL_STATUS_GPUPWRACK_MASK) == 0
				&& timeout > 0);

		if (!timeout) {
			DRM_ERROR("GPUPWRACK timeout! BG3D_PWRCTL_STATUS = %#010x", gpu_status);
			sgpu_debug_snapshot_expire_watchdog();
		}

		dbg_snapshot_pmu(ifpo->cal_id, __func__, DSS_FLAG_OUT);

		trace_sgpu_ifpo_power_on(1);

		/* No need to decrement lock counter since IFPO is disabled
		 * before power-on, hence BG3D_PWRCTL_STATUS was polled.
		 * 2ms timeout is added to prevent infinite loop.
		 */
		timeout = 2000;
		do {
			udelay(1);
			--timeout;
			gpu_status = readl(addr + BG3D_PWRCTL_STATUS_OFFSET);
		} while ((gpu_status & BG3D_PWRCTL_STATUS_GPU_READY_MASK) == 0
				&& timeout > 0);

		if (!timeout) {
			DRM_ERROR("GPU_READY timeout! BG3D_PWRCTL_STATUS = %#010x", gpu_status);
			sgpu_debug_snapshot_expire_watchdog();
		}

		vangogh_lite_set_didt_edc(adev, 0);
		#if defined(CONFIG_GPU_VERSION_M2_MID)
		WREG32_SOC15(GC, 0, mmGL1C_CTRL, 0x008100F0);
		WREG32_SOC15(GC, 0, mmDB_EXCEPTION_CONTROL, 0x80880000);
		#endif
	}

	data = REG_SET_FIELD(0, RLC_SRM_CNTL, AUTO_INCR_ADDR, 1);
	WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, data);

	ifpo->state = true;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO POWER ON");
}

static void sgpu_ifpo_abort_lock(struct amdgpu_device *adev, bool nowait)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	unsigned long flags;
	int count;

	if (!adev->probe_done || adev->in_runpm)
		return;

	if (!nowait)
		sgpu_ifpo_wait_force_idle(adev);

	spin_lock_irqsave(&ifpo->lock, flags);

	__sgpu_ifpo_abort_power_on(adev);
	count = atomic_add_return(IFPO_COUNT_KERNEL, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO POWER ON : count(%#x)", count);

	spin_unlock_irqrestore(&ifpo->lock, flags);
}

static void __sgpu_ifpo_abort_power_off(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	uint32_t data;

	if (!ifpo->state)
		return;

	/* BG3D_DVFS_CTL 0x058 disable */
	writel(0x0, adev->pm.pmu_mmio + 0x58);

	data = REG_SET_FIELD(0, RLC_SRM_CNTL, SRM_ENABLE, 1);
	data = REG_SET_FIELD(data, RLC_SRM_CNTL, AUTO_INCR_ADDR, 1);
	WREG32_SOC15(GC, 0, mmRLC_SRM_CNTL, data);

	dbg_snapshot_pmu(ifpo->cal_id, __func__, DSS_FLAG_IN);

	/* Directly power off instead of using PMUCAL */
	writel(0x89, adev->pm.pmu_mmio + BG3D_PWRCTL_CTL2_OFFSET);

	dbg_snapshot_pmu(ifpo->cal_id, __func__, DSS_FLAG_OUT);

	ifpo->state = false;

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO POWER OFF");

	trace_sgpu_ifpo_power_off(0);
}

static void sgpu_ifpo_abort_unlock(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	unsigned long flags;
	int count;

	if (!adev->probe_done || adev->in_runpm)
		return;

	spin_lock_irqsave(&ifpo->lock, flags);

	count = atomic_sub_return(IFPO_COUNT_KERNEL, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO COUNT DEC : count(%#x)", count);

	if (atomic_read(&ifpo->count) > 0)
		goto out;

	if ((atomic_read(&adev->pc_count) != 0) ||
	    (atomic_read(&adev->sqtt_count) != 0)) {
		SGPU_LOG(adev, DMSG_INFO, DMSG_POWER,
				"skip : pc_count %d sqtt_count %d",
				atomic_read(&adev->pc_count),
				atomic_read(&adev->sqtt_count));
		goto out;
	}

	__sgpu_ifpo_abort_power_off(adev);
out:
	spin_unlock_irqrestore(&ifpo->lock, flags);
}

static void sgpu_ifpo_abort_suspend(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	unsigned long flags;
	int count;

	if (!adev->probe_done)
		return;

	spin_lock_irqsave(&ifpo->lock, flags);

	count = atomic_add_return(IFPO_COUNT_KERNEL, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO SUSPEND : count(%#x)", count);

	/* GPU should be in IFPO power on state before entering suspend */
	__sgpu_ifpo_abort_power_on(adev);

	/* reset ColdGfxOff = 2'b00 before entering suspend */
	sgpu_ifpo_abort_reset(adev);

	spin_unlock_irqrestore(&ifpo->lock, flags);
}

static void sgpu_ifpo_abort_resume(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	int count = atomic_sub_return(IFPO_COUNT_KERNEL, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO RESUME : count(%#x)", count);
}

static void sgpu_ifpo_abort_user_disable(struct amdgpu_device *adev, int count, bool disable)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	int ifpo_count;

	switch (count) {
	case IFPO_COUNT_USER:
	case IFPO_COUNT_DEBUGFS:
	case IFPO_COUNT_SYSFS:
		break;
	default:
		WARN(1, "Invalid request (%#x)\n", count);
		break;
	}

	/*
	 * IFPO count should be changed while it's not in force_idle period.
	 * Asynchronous setting of user request could break the pair of
	 * force_enter and force_exit operation if GVF scenario is running.
	 */
	sgpu_ifpo_wait_force_idle(adev);

	ifpo_count = disable ? atomic_add_return(count, &ifpo->count) :
				atomic_sub_return(count, &ifpo->count);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO USER DISABLE : count(%#x)", ifpo_count);
}

static void sgpu_ifpo_abort_force_enter(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	unsigned long flags;
	u32 ih_wptr;

	reinit_completion(&ifpo->force_idle);

	if (atomic_read(&ifpo->count) > IFPO_COUNT_KERNEL_MASK)
		return;

	/* Make sure that no pending IRQ exist */
	do {
		ih_wptr = amdgpu_ih_get_wptr(adev, &adev->irq.ih);
	} while (adev->irq.ih.last_rptr != ih_wptr);

	spin_lock_irqsave(&ifpo->lock, flags);

	__sgpu_ifpo_abort_power_off(adev);

	spin_unlock_irqrestore(&ifpo->lock, flags);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO FORCE ENTER : count(%#x)",
			atomic_read(&ifpo->count));
}

static void sgpu_ifpo_abort_force_exit(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	unsigned long flags;

	if (atomic_read(&ifpo->count) > IFPO_COUNT_KERNEL_MASK)
		goto exit;

	spin_lock_irqsave(&ifpo->lock, flags);

	/* Don't need power on if counter is already 0 */
	if (atomic_read(&ifpo->count) > 0)
		__sgpu_ifpo_abort_power_on(adev);

	spin_unlock_irqrestore(&ifpo->lock, flags);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "IFPO FORCE EXIT : count(%#x)",
			atomic_read(&ifpo->count));
exit:
	complete_all(&ifpo->force_idle);
}

static struct sgpu_ifpo_func sgpu_ifpo_abort_func = {
	.enable = sgpu_ifpo_abort_enable,
	.suspend = sgpu_ifpo_abort_suspend,
	.resume = sgpu_ifpo_abort_resume,
	.lock = sgpu_ifpo_abort_lock,
	.unlock = sgpu_ifpo_abort_unlock,
	.reset = sgpu_ifpo_abort_reset,
	.user_disable = sgpu_ifpo_abort_user_disable,
	.force_enter = sgpu_ifpo_abort_force_enter,
	.force_exit = sgpu_ifpo_abort_force_exit,
};

/* IFPO module initialization */

void sgpu_ifpo_init(struct amdgpu_device *adev)
{
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	int ret = 0;

	ret |= of_property_read_u32(adev->pldev->dev.of_node,
					"ifpo_type", &ifpo->type);
	ret |= of_property_read_u32(adev->pldev->dev.of_node,
					"ifpo_cal_id", &ifpo->cal_id);

	if (ret) {
		DRM_ERROR("cannot find ifpo_type or ifpo_cal_id field in dt");
		ifpo->type = IFPO_DISABLED;
		ifpo->cal_id = 0;
	}

	switch (ifpo->type) {
	case IFPO_DISABLED:
		ifpo->func = NULL;
		break;

	case IFPO_ABORT:
		ifpo->func = &sgpu_ifpo_abort_func;
		break;

	case IFPO_HALF_AUTO:
		ifpo->func = &sgpu_ifpo_half_func;
		break;

	default:
		DRM_ERROR("invalid ifpo_type(%u)", ifpo->type);
		ifpo->func = NULL;
		ifpo->type = IFPO_DISABLED;
		ifpo->cal_id = 0;
		break;
	}

	DRM_INFO("%s : IFPO type(%u) cal_id(%#010x)", __func__,
			ifpo->type, ifpo->cal_id);

	spin_lock_init(&ifpo->lock);
	atomic_set(&ifpo->count, 0);

	init_completion(&ifpo->force_idle);
	complete_all(&ifpo->force_idle);

	ifpo->state = true;
}

void sgpu_ifpo_enable(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->enable)
		adev->ifpo.func->enable(adev);
}

void sgpu_ifpo_suspend(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->suspend)
		adev->ifpo.func->suspend(adev);
}

void sgpu_ifpo_resume(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->resume)
		adev->ifpo.func->resume(adev);
}

void sgpu_ifpo_lock(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->lock)
		adev->ifpo.func->lock(adev, false);
}

void sgpu_ifpo_lock_nowait(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->lock)
		adev->ifpo.func->lock(adev, true);
}

void sgpu_ifpo_unlock(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->unlock)
		adev->ifpo.func->unlock(adev);
}

void sgpu_ifpo_reset(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->reset)
		adev->ifpo.func->reset(adev);
}

void sgpu_ifpo_user_disable(struct amdgpu_device *adev, bool disable)
{
	if (adev->ifpo.func && adev->ifpo.func->user_disable)
		adev->ifpo.func->user_disable(adev, IFPO_COUNT_USER, disable);
}

void sgpu_ifpo_force_enter(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->force_enter)
		adev->ifpo.func->force_enter(adev);
}

void sgpu_ifpo_force_exit(struct amdgpu_device *adev)
{
	if (adev->ifpo.func && adev->ifpo.func->force_exit)
		adev->ifpo.func->force_exit(adev);
}

#if defined(CONFIG_DEBUG_FS)
static int sgpu_ifpo_debugfs_disable_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	*val = !!(atomic_read(&adev->ifpo.count) & IFPO_COUNT_DEBUGFS_MASK);

	return 0;
}

static int sgpu_ifpo_debugfs_disable_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	bool is_disabled = !!(atomic_read(&ifpo->count) & IFPO_COUNT_DEBUGFS_MASK);

	if (val > 1)
		return -EINVAL;

	if (!ifpo->func || !ifpo->func->user_disable)
		return -EINVAL;

	if (is_disabled != val)
		ifpo->func->user_disable(adev, IFPO_COUNT_DEBUGFS, val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sgpu_debugfs_ifpo_disable_fops,
			sgpu_ifpo_debugfs_disable_get,
			sgpu_ifpo_debugfs_disable_set, "%llu\n");

int sgpu_ifpo_debugfs_init(struct drm_minor *minor)
{
	struct drm_device *drm = minor->dev;
	struct amdgpu_device *adev = drm_to_adev(drm);
	struct dentry *ent, *root = minor->debugfs_root;

	ent = debugfs_create_file("ifpo_disable", 0644, root, adev,
				  &sgpu_debugfs_ifpo_disable_fops);
	if (!ent) {
		DRM_ERROR("unable to create ifpo_disable debugfs file\n");
		return -EIO;
	}
	adev->ifpo.debugfs_disable = ent;

	return 0;
}
#else /* CONFIG_DEBUG_FS */
int sgpu_ifpo_debugfs_init(struct drm_minor *minor)
{
	return 0;
}
#endif  /* CONFIG_DEBUG_FS */

static ssize_t sgpu_ifpo_disable_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	bool is_disabled = !!(atomic_read(&ifpo->count) & IFPO_COUNT_SYSFS_MASK);
	u32 val;
	int ret;

	if (!ifpo->func || !ifpo->func->user_disable)
		return -EINVAL;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	if (val > 1)
		return -EINVAL;

	if (is_disabled != val)
		ifpo->func->user_disable(adev, IFPO_COUNT_SYSFS, val);

	return count;
}

static ssize_t sgpu_ifpo_disable_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	struct sgpu_ifpo *ifpo = &adev->ifpo;
	ssize_t count = 0;

	count += scnprintf(&buf[count], PAGE_SIZE - count, "%u\n",
				!!(atomic_read(&ifpo->count) &
					IFPO_COUNT_SYSFS_MASK));

	return count;
}
struct kobj_attribute attr_gpu_ifpo_disable = __ATTR_RW_MODE(sgpu_ifpo_disable, 0640);

static ssize_t sgpu_ifpo_state_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	int ifpo_count = atomic_read(&adev->ifpo.count);
	ssize_t count = 0;

	if (adev->ifpo.type == IFPO_DISABLED) {
		count += scnprintf(&buf[count], PAGE_SIZE - count,
					"IFPO is not supported\n");
		return count;
	}

	if (!(ifpo_count & ~IFPO_COUNT_KERNEL_MASK)) {
		count += scnprintf(&buf[count], PAGE_SIZE - count,
					"IFPO is enabled\n");
		return count;
	}

	count += scnprintf(&buf[count], PAGE_SIZE - count,
				"IFPO is disabled by\n");

	count += scnprintf(&buf[count], PAGE_SIZE - count, "user ioctl   : %u\n",
				(ifpo_count & IFPO_COUNT_USER_MASK)
					>> IFPO_COUNT_USER_SHIFT);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "sysfs node   : %u\n",
				(ifpo_count & IFPO_COUNT_SYSFS_MASK)
					>> IFPO_COUNT_SYSFS_SHIFT);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "debugfs node : %u\n",
				(ifpo_count & IFPO_COUNT_DEBUGFS_MASK)
					>> IFPO_COUNT_DEBUGFS_SHIFT);

	return count;
}
struct kobj_attribute attr_gpu_ifpo_state = __ATTR_RO_MODE(sgpu_ifpo_state, 0440);
