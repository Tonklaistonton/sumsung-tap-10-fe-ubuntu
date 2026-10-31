// SPDX-License-Identifier: GPL-2.0-only
/*
 * @file sgpu_gvf_governor.c
 * @copyright 2024 Samsung Electronics
 */

#include <linux/ktime.h>
#include <linux/pm_runtime.h>

#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "sgpu_governor.h"
#include "exynos_gpu_interface.h"
#include "sgpu_gvf_governor.h"
#include "sgpu_sysfs.h"

#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"
#include "gc/gc_10_4_0_default.h"
#include "soc15_common.h"

#define SGPU_GVF_NAME_LEN	20

/* Governors */
static struct sgpu_gvf_gov_param_ratio {
	uint32_t	target_ratio;
	uint64_t	max_rest_time;
	uint64_t	min_rest_time;
} gvf_param_ratio;

static void sgpu_gvf_governor_ratio_init(struct amdgpu_device *adev, uint32_t level)
{
	struct sgpu_gvf *gvf = &adev->gvf;
	uint32_t min_rest_ratio, max_rest_ratio;

	gvf->sampling_time_ms = sgpu_gvf_get_param(level, SGPU_GVF_PARAM_SAMPLING_TIME_MS);
	min_rest_ratio = sgpu_gvf_get_param(level, SGPU_GVF_PARAM_MIN_REST_RATIO);
	max_rest_ratio = sgpu_gvf_get_param(level, SGPU_GVF_PARAM_MAX_REST_RATIO);

	gvf_param_ratio.target_ratio = sgpu_gvf_get_param(level, SGPU_GVF_PARAM_TARGET_RATIO);
	gvf_param_ratio.max_rest_time = (uint64_t)(gvf->sampling_time_ms *
					NSEC_PER_MSEC * max_rest_ratio / 100);
	gvf_param_ratio.min_rest_time = (uint64_t)(gvf->sampling_time_ms *
					NSEC_PER_MSEC * min_rest_ratio / 100);

	dev_info(adev->dev, "%s: sampling_time_ms(%u) target_ratio(%u) "
				"max_rest_ratio(%u) min_rest_ratio(%u)",
				gvf->governor->name, gvf->sampling_time_ms,
				gvf_param_ratio.target_ratio,
				max_rest_ratio, min_rest_ratio);
}

static uint64_t sgpu_gvf_governor_ratio_calc_idle(struct amdgpu_device *adev)
{
	struct sgpu_gvf *gvf = &adev->gvf;
	uint64_t cur_idle_time_ns = sgpu_pm_monitor_get_idle_time(adev);
	uint64_t cur_time_ns = ktime_get_ns();
	uint64_t total_idle_time_ns = cur_idle_time_ns - gvf->base_idle_time_ns;
	uint64_t total_time_ns = cur_time_ns - gvf->base_time_ns;
	uint64_t cur_idle_ratio, injection_time_ns, calc_time_ns;

	cur_idle_ratio = (total_idle_time_ns * 100) / total_time_ns;

	/* No need to inject idle */
	if (gvf_param_ratio.target_ratio <= cur_idle_ratio)
		return 0;

	calc_time_ns = ((gvf_param_ratio.target_ratio * total_time_ns) -
				(total_idle_time_ns * 100)) /
				(100 - gvf_param_ratio.target_ratio);

	/* limit for max idle time */
	injection_time_ns = min(calc_time_ns, gvf_param_ratio.max_rest_time);
	/* limit for min idle time */
	injection_time_ns = max(injection_time_ns, gvf_param_ratio.min_rest_time);

	trace_sgpu_gvf_governor_ratio_calc_idle(gvf_param_ratio.target_ratio,
				cur_idle_ratio, calc_time_ns, injection_time_ns,
				total_idle_time_ns, total_time_ns);

	return injection_time_ns;
}

static struct sgpu_gvf_governor sgpu_gvf_governor_info[SGPU_GVF_GOVERNOR_MAX] = {
	[SGPU_GVF_GOVERNOR_RATIO] = {
		.name = "RATIO",
		.init = sgpu_gvf_governor_ratio_init,
		.calc_idle = sgpu_gvf_governor_ratio_calc_idle,
	},
};

int sgpu_gvf_governor_set(struct amdgpu_device *adev, uint32_t id)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	BUG_ON(id >= SGPU_GVF_GOVERNOR_MAX);

	mutex_lock(&gvf->lock);

	gvf->governor = &sgpu_gvf_governor_info[id];

	if (gvf->enable && gvf->governor->init)
		gvf->governor->init(adev, gvf->level);

	mutex_unlock(&gvf->lock);

	dev_info(adev->dev, "%s: %s", __func__, gvf->governor->name);

	return 0;
}

static ssize_t sgpu_gvf_governor_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	char str_governor[SGPU_GVF_NAME_LEN + 1];
	uint32_t id = SGPU_GVF_GOVERNOR_MAX;
	int ret, i;

	ret = sscanf(buf, "%" __stringify(SGPU_GVF_NAME_LEN) "s", str_governor);
	if (ret != 1)
		return ret;

	for (i = 0; i < SGPU_GVF_GOVERNOR_MAX; ++i) {
		if (!strncmp(sgpu_gvf_governor_info[i].name, str_governor,
				SGPU_GVF_NAME_LEN)) {
			id = i;
			break;
		}
	}

	if (id == SGPU_GVF_GOVERNOR_MAX) {
		dev_err(adev->dev, "Invalid governor : %s", str_governor);
		return -EINVAL;
	}

	ret = sgpu_gvf_governor_set(adev, id);
	if (ret)
		return ret;

	return count;
}

static ssize_t sgpu_gvf_governor_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	struct sgpu_gvf *gvf = &adev->gvf;
	ssize_t count = 0;

	count += scnprintf(&buf[count], PAGE_SIZE - count,
				"%s\n", gvf->governor->name);

	return count;

}
struct kobj_attribute attr_sgpu_gvf_governor = __ATTR_RW_MODE(sgpu_gvf_governor, 0640);

static ssize_t sgpu_gvf_governor_list_show(struct kobject *kobj,
						struct kobj_attribute *attr,
						char *buf)
{
	ssize_t count = 0;
	int i;

	for (i = 0; i < SGPU_GVF_GOVERNOR_MAX; ++i)
		count += scnprintf(&buf[count], PAGE_SIZE - count,
			"%s ", sgpu_gvf_governor_info[i].name);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}
struct kobj_attribute attr_sgpu_gvf_governor_list = __ATTR_RO_MODE(sgpu_gvf_governor_list, 0440);


/* Idle Injectors */
static void sgpu_gvf_injector_sched_control_enter_idle(struct amdgpu_device *adev)
{
	int i, r;

	for (i = 0; i < adev->num_rings; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;

		drm_sched_stop(&ring->sched, NULL);

		r = amdgpu_fence_wait_empty(ring);
		if (r)
			amdgpu_fence_driver_force_completion(ring);
	}

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "Stop drm schedulers");

	sgpu_ifpo_force_enter(adev);
}

static void sgpu_gvf_injector_sched_control_exit_idle(struct amdgpu_device *adev)
{
	int i;

	sgpu_ifpo_force_exit(adev);

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->sched.thread)
			continue;

		drm_sched_start(&ring->sched, true);
	}

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "Start drm schedulers");
}

static struct sgpu_gvf_inj_param_hwq {
	unsigned long gfx_timeout[AMDGPU_MAX_GFX_RINGS];
	unsigned long expire_jiffies;
	bool skip_unmap;
} gvf_param_hwq;

static void sgpu_gvf_cp_gfx_halt(struct amdgpu_device *adev)
{
	u32 tmp = RREG32_SOC15(GC, 0, mmCP_ME_CNTL);

	tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, ME_HALT, 1);
	tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, PFP_HALT, 1);
	WREG32_SOC15(GC, 0, mmCP_ME_CNTL, tmp);
}

static void sgpu_gvf_cp_gfx_unhalt(struct amdgpu_device *adev)
{
	u32 tmp = RREG32_SOC15(GC, 0, mmCP_ME_CNTL);

	tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, ME_HALT, 0);
	tmp = REG_SET_FIELD(tmp, CP_ME_CNTL, PFP_HALT, 0);
	WREG32_SOC15(GC, 0, mmCP_ME_CNTL, tmp);
}

#define SGPU_GVF_SCHED_MAX_TIMEOUT	(LONG_MAX / 2)

static unsigned long sgpu_gvf_suspend_sched_timeout(struct drm_gpu_scheduler *sched)
{
	unsigned long sched_timeout, remain, now;

	spin_lock(&sched->job_list_lock);

	now = jiffies;
	sched_timeout = sched->work_tdr.timer.expires;

	if (mod_delayed_work(sched->timeout_wq, &sched->work_tdr,
			SGPU_GVF_SCHED_MAX_TIMEOUT)
			&& time_after(sched_timeout, now))
		remain = sched_timeout - now;
	else
		remain = sched->timeout;

	spin_unlock(&sched->job_list_lock);

	return remain;
}

static void sgpu_gvf_resume_sched_timeout(struct drm_gpu_scheduler *sched,
						unsigned long remaining)
{
	spin_lock(&sched->job_list_lock);

	if (list_empty(&sched->pending_list))
		cancel_delayed_work(&sched->work_tdr);
	else
		mod_delayed_work(sched->timeout_wq, &sched->work_tdr, remaining);

	spin_unlock(&sched->job_list_lock);
}

static void sgpu_gvf_injector_hwq_control_enter_idle(struct amdgpu_device *adev)
{
	int i;

	if (!atomic_read(&adev->ifpo.count)) {
		SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "Skip unmap HW queues");
		gvf_param_hwq.skip_unmap = true;
		goto skip_unmap;
	}

	gvf_param_hwq.skip_unmap = false;
	gvf_param_hwq.expire_jiffies = jiffies + SGPU_GVF_SCHED_MAX_TIMEOUT;

	sgpu_ifpo_lock(adev);

	trace_sgpu_gvf_injector_hwq_control_dequeue(1);
	for (i = 0; i < adev->gfx.num_gfx_rings; ++i) {
		struct amdgpu_ring *ring = &adev->gfx.gfx_ring[i];

		if (!ring || !ring->sched.thread)
			continue;

		gvf_param_hwq.gfx_timeout[i] = sgpu_gvf_suspend_sched_timeout(&ring->sched);

		adev->gfx.me.unmap_queue(ring);
	}
	trace_sgpu_gvf_injector_hwq_control_dequeue(0);

	sgpu_ifpo_unlock(adev);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "Unmap HW queues");

skip_unmap:
	sgpu_ifpo_force_enter(adev);
}

static void sgpu_gvf_injector_hwq_control_exit_idle(struct amdgpu_device *adev)
{
	int i;

	if (gvf_param_hwq.skip_unmap) {
		SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "Skip map HW queues");
		goto skip_map;
	}

	sgpu_ifpo_lock_nowait(adev);

	sgpu_gvf_cp_gfx_halt(adev);

	trace_sgpu_gvf_injector_hwq_control_enqueue(1);
	for (i = 0; i < adev->gfx.num_gfx_rings; ++i) {
		struct amdgpu_ring *ring = &adev->gfx.gfx_ring[i];

		if (!ring || !ring->sched.thread)
			continue;

		adev->gfx.me.map_queue(ring);

		/* job timeout work has been refreshed during idle period */
		if (ring->sched.work_tdr.timer.expires < gvf_param_hwq.expire_jiffies)
			continue;

		sgpu_gvf_resume_sched_timeout(&ring->sched, gvf_param_hwq.gfx_timeout[i]);
	}
	trace_sgpu_gvf_injector_hwq_control_enqueue(0);

	sgpu_gvf_cp_gfx_unhalt(adev);

	for (i = 0; i < adev->gfx.num_gfx_rings; ++i) {
		struct amdgpu_ring *ring = &adev->gfx.gfx_ring[i];

		/* Update wptr if pending jobs exist */
		if (atomic_read(&ring->fence_drv.last_seq) != ring->fence_drv.sync_seq) {
			uint64_t wptr = ring->wptr & ~ring->funcs->align_mask;

			if (ring->use_doorbell) {
				atomic64_set((atomic64_t *)&adev->wb.wb[ring->wptr_offs], wptr);
				WDOORBELL64(ring->doorbell_index, wptr);
			} else {
				WREG32_SOC15(GC, 0, mmCP_RB0_WPTR, lower_32_bits(wptr));
				WREG32_SOC15(GC, 0, mmCP_RB0_WPTR_HI, upper_32_bits(wptr));
			}

			SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "%s wptr:%llx", ring->name, wptr);
		}
	}

	sgpu_ifpo_unlock(adev);

	SGPU_LOG(adev, DMSG_INFO, DMSG_POWER, "Map HW queues");

skip_map:
	sgpu_ifpo_force_exit(adev);
}

static struct sgpu_gvf_injector sgpu_gvf_injector_info[SGPU_GVF_INJECTOR_MAX] = {
	[SGPU_GVF_INJECTOR_SCHED_CONTROL] = {
		.name = "SCHED_CONTROL",
		.enter_idle = sgpu_gvf_injector_sched_control_enter_idle,
		.exit_idle = sgpu_gvf_injector_sched_control_exit_idle,
	},
	[SGPU_GVF_INJECTOR_HW_QUEUE_CONTROL] = {
		.name = "HW_QUEUE_CONTROL",
		.enter_idle = sgpu_gvf_injector_hwq_control_enter_idle,
		.exit_idle = sgpu_gvf_injector_hwq_control_exit_idle,
	},
};

int sgpu_gvf_injector_set(struct amdgpu_device *adev, uint32_t id)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	BUG_ON(id >= SGPU_GVF_INJECTOR_MAX);

	if (gvf->enable) {
		dev_err(adev->dev, "Cannot change injector while GVF is enabled");
		return -EINVAL;
	}

	gvf->injector = &sgpu_gvf_injector_info[id];

	dev_info(adev->dev, "%s: %s", __func__, gvf->injector->name);

	return 0;
}

static ssize_t sgpu_gvf_injector_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	char str_injector[SGPU_GVF_NAME_LEN + 1];
	uint32_t id = SGPU_GVF_INJECTOR_MAX;
	int ret, i;

	ret = sscanf(buf, "%" __stringify(SGPU_GVF_NAME_LEN) "s", str_injector);
	if (ret != 1)
		return ret;

	for (i = 0; i < SGPU_GVF_INJECTOR_MAX; ++i) {
		if (!strncmp(sgpu_gvf_injector_info[i].name, str_injector,
				SGPU_GVF_NAME_LEN)) {
			id = i;
			break;
		}
	}

	if (id == SGPU_GVF_INJECTOR_MAX) {
		dev_err(adev->dev, "Invalid injector : %s", str_injector);
		return -EINVAL;
	}

	ret = sgpu_gvf_injector_set(adev, id);
	if (ret)
		return ret;

	return count;
}

static ssize_t sgpu_gvf_injector_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	struct sgpu_gvf *gvf = &adev->gvf;
	ssize_t count = 0;

	count += scnprintf(&buf[count], PAGE_SIZE - count,
				"%s\n", gvf->injector->name);

	return count;

}
struct kobj_attribute attr_sgpu_gvf_injector = __ATTR_RW_MODE(sgpu_gvf_injector, 0640);

static ssize_t sgpu_gvf_injector_list_show(struct kobject *kobj,
						struct kobj_attribute *attr,
						char *buf)
{
	ssize_t count = 0;
	int i;

	for (i = 0; i < SGPU_GVF_INJECTOR_MAX; ++i)
		count += scnprintf(&buf[count], PAGE_SIZE - count,
			"%s ", sgpu_gvf_injector_info[i].name);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}
struct kobj_attribute attr_sgpu_gvf_injector_list = __ATTR_RO_MODE(sgpu_gvf_injector_list, 0440);
