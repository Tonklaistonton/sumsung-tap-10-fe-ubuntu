/*
 * Copyright 2015, 2019-2021 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 */
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_sws.h"
#include "sgpu_bpmd.h"
#include "sgpu_debug.h"
#include "sgpu_swap.h"
#include "soc15_common.h"
#include "gc/gc_10_4_0_offset.h"
#include "gc/gc_10_4_0_sh_mask.h"

#ifdef CONFIG_DRM_SGPU_EXYNOS
#include "exynos_gpu_interface.h"
#endif /* CONFIG_DRM_SGPU_EXYNOS */

#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GPU)
#include <soc/samsung/profiler/exynos-profiler-extif.h>
#endif

static enum drm_gpu_sched_stat amdgpu_job_timedout(struct drm_sched_job *s_job)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(s_job->sched);
	struct drm_gpu_scheduler *sched = s_job->sched;
	struct amdgpu_job *job = to_amdgpu_job(s_job);
	struct amdgpu_task_info ti;
	struct amdgpu_device *adev = ring->adev;
	int r = 0;
	uint32_t value = 0, data = 0;
	char comm[TASK_COMM_LEN];

	if(!mutex_trylock(&adev->recovery_mutex)){
		DRM_ERROR("%s: job_id %lld: Giving another "
			  "chance to the second timeout. skip recovery\n",
			  ring->name, s_job->id);

		spin_lock(&sched->job_list_lock);
		list_add(&s_job->list, &sched->pending_list);
		spin_unlock(&sched->job_list_lock);

		return DRM_GPU_SCHED_STAT_NOMINAL;
	}

	amdgpu_vm_get_task_info(ring->adev, job->pasid, &ti);
	DRM_ERROR("ring %s timeout, signaled seq=%u, emitted seq=%u\n",
		  job->base.sched->name,
		  atomic_read(&ring->fence_drv.last_seq),
		  ring->fence_drv.sync_seq);

	DRM_ERROR("Process information: process %s pid %d thread %s pid %d submit by %s(%d)\n",
		  ti.process_name, ti.tgid, ti.task_name, ti.pid,
		  get_task_comm(comm, job->submit_task), job->submit_task->pid);

	SGPU_LOG(adev, DMSG_INFO, DMSG_ETC,
			"Process information: process %s pid %d thread %s pid %d\n",
			ti.process_name, ti.tgid, ti.task_name, ti.pid);

	if (adev->runpm) {
		r = pm_runtime_get_sync(adev->ddev.dev);
		if (r < 0)
			goto pm_put;
	}

	/* sgpu_gvf thread gets unparked in amdgpu_device_resume() */
	sgpu_gvf_suspend(adev);

	sgpu_ifpo_lock(adev);

	if (ring->funcs->check_ring_done && s_job->s_fence->parent) {
		struct dma_fence *fence = s_job->s_fence->parent;

		job = to_amdgpu_job(s_job);
		DRM_INFO("%s: vmid %u job_id %lld num_ibs %d " \
			 "FENCE drm %lld/%lld/%lld sgpu %lld/%lld\n",
			 ring->name, job->vmid, s_job->id,
			 job->num_ibs,
			 s_job->s_fence->scheduled.context,
			 s_job->s_fence->finished.context,
			 s_job->s_fence->finished.seqno,
			 fence->context, fence->seqno);

		SGPU_LOG(adev, DMSG_INFO, DMSG_ETC,
				"%s: job_id %lld num_ibs %d FENCE drm %lld/%lld/%lld sgpu %lld/%lld\n",
				ring->name, s_job->id,
				job->num_ibs,
				s_job->s_fence->scheduled.context,
				s_job->s_fence->finished.context,
				s_job->s_fence->finished.seqno,
				fence->context, fence->seqno);

		ring->funcs->check_ring_done(ring);
	}

	if (AMDGPU_IS_MGFX2_EVT1(adev->grbm_chip_rev)) {
		/* Logs to check if job timeout occurred by back-pressure issue */
		uint32_t idx;

		DRM_INFO("GPU REG DUMP : ");
		DRM_INFO("GL2C_CTRL3 %#10x", RREG32_SOC15(GC, 0, mmGL2C_CTRL3));
		DRM_INFO("GL2C_CTRL4 %#10x", RREG32_SOC15(GC, 0, mmGL2C_CTRL4));

		value = REG_SET_FIELD(0, GRBM_GFX_INDEX, SA_BROADCAST_WRITES, 1);
		value = REG_SET_FIELD(value, GRBM_GFX_INDEX, INSTANCE_BROADCAST_WRITES, 1);
		value = REG_SET_FIELD(value, GRBM_GFX_INDEX, SE_BROADCAST_WRITES, 1);
		/* GL2ACEM_STUS per GRBM_GFX_INDEX */
		for (idx = 0; idx < 4; ++idx) {
			data = REG_SET_FIELD(value, GRBM_GFX_INDEX, INSTANCE_INDEX, idx);
			WREG32_SOC15(GC, 0, mmGRBM_GFX_INDEX, data);
			DRM_INFO("GL2ACEM_STUS[%u] %#10x", idx,
				RREG32_SOC15(GC, 0, mmGL2ACEM_STUS));
		}
		WREG32_SOC15(GC, 0, mmGRBM_GFX_INDEX, value);

		DRM_INFO("GRBM_STATUS %#10x", RREG32_SOC15(GC, 0, mmGRBM_STATUS));
		DRM_INFO("GRBM_STATUS2 %#10x", RREG32_SOC15(GC, 0, mmGRBM_STATUS2));
		DRM_INFO("GRBM_STATUS3 %#10x", RREG32_SOC15(GC, 0, mmGRBM_STATUS3));
	}

	sgpu_bpmd_dump(ring->adev);

	if (IS_ENABLED(CONFIG_DRM_AMDGPU_DUMP) && adev->asic_funcs->get_asic_status)
		adev->asic_funcs->get_asic_status(adev, NULL, 0);

	if (amdgpu_fault_detect) {
		if (test_bit(FAULT_DETECT_RUNNING,
				&adev->fault_detect_flags)) {
			set_bit(FAULT_DETECT_JOB_TIMEOUT,
					&adev->fault_detect_flags);
			set_bit(FAULT_DETECT_WAKEUP,
					&adev->fault_detect_flags);
			wake_up(&adev->fault_detect_wake_up);
		}
	}

	if (sgpu_jobtimeout_to_panic_check(adev, ring->funcs->type)) {
		list_add(&s_job->list, &s_job->sched->pending_list);
		sgpu_debug_snapshot_expire_watchdog();
	}

	memset(&ti, 0, sizeof(struct amdgpu_task_info));
	adev->job_hang = true;
	amdgpu_device_gpu_recover(ring->adev, job);
	adev->job_hang = false;

	sgpu_ifpo_unlock(adev);

	sgpu_gvf_resume(adev);

	if (adev->runpm) {
		pm_runtime_mark_last_busy(adev->ddev.dev);
		pm_runtime_put_autosuspend(adev->ddev.dev);
	}
	mutex_unlock(&adev->recovery_mutex);
	return DRM_GPU_SCHED_STAT_NOMINAL;
pm_put:
	if (adev->runpm)
		pm_runtime_put_autosuspend(adev->ddev.dev);
	mutex_unlock(&adev->recovery_mutex);
	return DRM_GPU_SCHED_STAT_ENODEV;
}

int amdgpu_job_alloc(struct amdgpu_device *adev, unsigned num_ibs,
		     struct amdgpu_job **job, struct amdgpu_vm *vm)
{
	size_t size = sizeof(struct amdgpu_job);

	if (num_ibs == 0)
		return -EINVAL;

	size += sizeof(struct amdgpu_ib) * num_ibs;

	*job = kzalloc(size, GFP_KERNEL);
	if (!*job)
		return -ENOMEM;

	/*
	 * Initialize the scheduler to at least some ring so that we always
	 * have a pointer to adev.
	 */
	(*job)->base.sched = &adev->rings[0]->sched;
	(*job)->vm = vm;
	(*job)->ibs = (void *)&(*job)[1];

	amdgpu_sync_create(&(*job)->sync);
	amdgpu_sync_create(&(*job)->sched_sync);
	(*job)->vram_lost_counter = atomic_read(&adev->vram_lost_counter);
	(*job)->vm_pd_addr = AMDGPU_BO_INVALID_OFFSET;

	(*job)->end_of_frame = false;

	return 0;
}

int amdgpu_job_alloc_with_ib(struct amdgpu_device *adev, unsigned size,
		enum amdgpu_ib_pool_type pool_type,
		struct amdgpu_job **job)
{
	int r;

	r = amdgpu_job_alloc(adev, 1, job, NULL);
	if (r)
		return r;

	(*job)->num_ibs = 1;
	r = amdgpu_ib_get(adev, NULL, size, pool_type, &(*job)->ibs[0]);
	if (r)
		kfree(*job);

	return r;
}

static void amdgpu_job_wa_pc_rings(struct amdgpu_ctx *ctx,
				   struct amdgpu_ib *ib)
{
	if (ib->flags & AMDGPU_IB_FLAG_PERF_COUNTER) {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->pc_gfx_rings |= (1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->pc_compute_rings |= (1 << ib->ring);
	} else {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->pc_gfx_rings &= ~(1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->pc_compute_rings &= ~(1 << ib->ring);
	}
}

static void amdgpu_job_wa_sqtt_rings(struct amdgpu_ctx *ctx,
				     struct amdgpu_ib *ib)
{
	if (ib->flags & AMDGPU_IB_FLAG_SQ_THREAD_TRACE) {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->sqtt_gfx_rings |= (1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->sqtt_compute_rings |= (1 << ib->ring);
	} else {
		if (ib->ip_type == AMDGPU_HW_IP_GFX)
			ctx->sqtt_gfx_rings &= ~(1 << ib->ring);
		else if (ib->ip_type == AMDGPU_HW_IP_COMPUTE)
			ctx->sqtt_compute_rings &= ~(1 << ib->ring);
	}
}

static void amdgpu_job_track_pc_sqtt(struct amdgpu_device *adev,
				     struct amdgpu_job *job)
{
	struct amdgpu_ctx *ctx = job->ctx;
	struct amdgpu_ib *ib;
	bool old_rings, new_rings;
	int i;

	job->pc_wa_enable = job->pc_wa_disable = false;
	job->sqtt_wa_enable = job->sqtt_wa_disable = false;
	for (i = 0; i < job->num_ibs; i++) {
		ib = &job->ibs[i];

		/* Are there any rings that have pc active */
		old_rings = (ctx->pc_gfx_rings || ctx->pc_compute_rings);
		amdgpu_job_wa_pc_rings(ctx, ib);
		new_rings = (ctx->pc_gfx_rings || ctx->pc_compute_rings);
		/* If old and new is not equal, it means there is a change
		 * in Perfcount active/inactive. */
		if (old_rings != new_rings) {
			/* If new_rings is true, enable workaround for this job.
			 * If new_rings is false, disable workaround after this job.*/
			if (new_rings) {
				job->pc_wa_enable = true;
				job->pc_wa_disable = false;
			} else
				job->pc_wa_disable = true;
		}

		old_rings = (ctx->sqtt_gfx_rings || ctx->sqtt_compute_rings);
		amdgpu_job_wa_sqtt_rings(ctx, ib);
		new_rings = (ctx->sqtt_gfx_rings || ctx->sqtt_compute_rings);
		if (old_rings != new_rings) {
			if (new_rings) {
				job->sqtt_wa_enable = true;
				job->sqtt_wa_disable = false;
			} else
				job->sqtt_wa_disable = true;
		}
	}
}

static void amdgpu_job_pc_workaround_enable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	/* if adev->pc_count is 0, workaround is disabled.
	 * Enable the workaround. */
	if (atomic_read(&adev->pc_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_PERFCOUNTER, 1);
	atomic_inc(&adev->pc_count);
}

static void amdgpu_job_pc_workaround_disable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (atomic_read(&adev->pc_count) == 0) {
		DRM_ERROR("Tracking Perfcounter active/inactive out of bound\n");
		return;
	}

	atomic_dec(&adev->pc_count);
	/* if adev->pc_count become 0, workaround is enabled.
	 * Disable the workaround. */
	if (atomic_read(&adev->pc_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_PERFCOUNTER, 0);
}

static void amdgpu_job_sqtt_workaround_enable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	/* if adev->sqtt_count is 0, workaround is disabled.
	 * Enable the workaround. */
	if (atomic_read(&adev->sqtt_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_SQ_THREAD_TRACE, 1);
	atomic_inc(&adev->sqtt_count);
}

static void amdgpu_job_sqtt_workaround_disable(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (atomic_read(&adev->sqtt_count) == 0) {
		DRM_ERROR("Tracking SQTT active/inactive out of bound\n");
		return;
	}

	atomic_dec(&adev->sqtt_count);
	/* if adev->sqtt_count become 0, workaround is enabled.
	 * Disable the workaround. */
	if (atomic_read(&adev->sqtt_count) == 0)
		amdgpu_gfx_sw_workaround(adev, WA_CG_SQ_THREAD_TRACE, 0);
}

void amdgpu_job_set_resources(struct amdgpu_job *job, struct amdgpu_bo *gds,
			      struct amdgpu_bo *gws, struct amdgpu_bo *oa)
{
	if (gds) {
		job->gds_base = amdgpu_bo_gpu_offset(gds) >> PAGE_SHIFT;
		job->gds_size = amdgpu_bo_size(gds) >> PAGE_SHIFT;
	}
	if (gws) {
		job->gws_base = amdgpu_bo_gpu_offset(gws) >> PAGE_SHIFT;
		job->gws_size = amdgpu_bo_size(gws) >> PAGE_SHIFT;
	}
	if (oa) {
		job->oa_base = amdgpu_bo_gpu_offset(oa) >> PAGE_SHIFT;
		job->oa_size = amdgpu_bo_size(oa) >> PAGE_SHIFT;
	}
}

void amdgpu_job_free_resources(struct amdgpu_job *job)
{
	struct dma_fence *f;
	unsigned i;

	/* use sched fence if available */
	f = job->base.s_fence ? &job->base.s_fence->finished : job->fence;

	for (i = 0; i < job->num_ibs; ++i)
		amdgpu_ib_free(&job->ibs[i], f);
}

#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GPU)
static void amdgpu_job_user_time(struct drm_sched_job *s_job)
{
	struct amdgpu_job *job = to_amdgpu_job(s_job);
	struct amdgpu_ring *ring = to_amdgpu_ring(s_job->sched);

	if (ring->funcs->type != AMDGPU_RING_TYPE_GFX)
		return;

	sgpu_profiler_update_interframe_hw(s_job->s_fence->scheduled.timestamp,
					   s_job->s_fence->finished.timestamp,
					   job->end_of_frame);
}
#else
#define amdgpu_job_user_time(s_job)	do { } while (0)
#endif /* CONFIG_EXYNOS_PROFILER_GPU */

static void amdgpu_job_free_cb(struct drm_sched_job *s_job)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(s_job->sched);
	struct amdgpu_device *adev = ring->adev;
	struct amdgpu_job *job = to_amdgpu_job(s_job);
	bool fault_detect_notify = false;

	/* Call sgpu_swap_unlock() only for standalone jobs or gang submit leaders
	 * and skip the gang submit members. For standalone jobs job->gang_submit
	 * is NULL, for gang jobs amdgpu_job_set_gang_leader() sets it to leader's
	 * job->base.s_fence->scheduled. */
	if (!job->gang_submit || job->gang_submit == &job->base.s_fence->scheduled)
		sgpu_swap_unlock(container_of(job->vm, struct amdgpu_fpriv, vm));

	if (sgpu_hang_detect) {
		if (ring->funcs->type == AMDGPU_RING_TYPE_GFX &&
			(atomic_dec_return(&adev->hang_gfx_job_cnt) == 0)) {
				clear_bit(HANG_DETECT_RUNNING,
					&adev->hang_detect_flags);
		}
	}

	if (amdgpu_fault_detect) {
		if (ring->funcs->type == AMDGPU_RING_TYPE_GFX &&
				(atomic_dec_return(&adev->gfx_job_cnt) == 0)) {
			clear_bit(FAULT_DETECT_GFX_ACTIVE,
					&adev->fault_detect_flags);
			fault_detect_notify = true;
		} else if (ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE &&
				(atomic_dec_return(&adev->compute_job_cnt)
						== 0)) {
			clear_bit(FAULT_DETECT_COMPUTE_ACTIVE,
					&adev->fault_detect_flags);
			fault_detect_notify = true;
		}

		/* If both GFX and Compute are idle inform to fault detect */
		if (fault_detect_notify
			&& (!test_bit(FAULT_DETECT_GFX_ACTIVE,
				&adev->fault_detect_flags) &&
					!test_bit(FAULT_DETECT_COMPUTE_ACTIVE,
						&adev->fault_detect_flags))) {

			set_bit(FAULT_DETECT_WAKEUP,
					&adev->fault_detect_flags);
			wake_up(&adev->fault_detect_wake_up);
		}
	}

	amdgpu_job_user_time(s_job);

	/* TODO Should this before the previous the conditional block */
	if (ring->sws_ctx.ctx && ring->sws_ctx.ctx->secure_mode && amdgpu_tmz)
		amdgpu_sws_put_tmz_queue(ring->sws_ctx.ctx,
					 &job->base.s_fence->finished);

	SGPU_LOG(adev, DMSG_INFO, DMSG_ETC,
		 "ring=%s, job-id=%d: vmid=%u, pasid=%u, drm %llu/%llu/%llu",
		 ring->name, job->base.id, job->vmid, job->pasid,
		 job->base.s_fence->scheduled.context,
		 job->base.s_fence->finished.context,
		 job->base.s_fence->finished.seqno);

	put_task_struct(job->submit_task);
	drm_sched_job_cleanup(s_job);

	dma_fence_put(job->fence);    // TODO delete
	amdgpu_sync_free(&job->sync);
	amdgpu_sync_free(&job->sched_sync);

	/* Is workaround not needed after this job */
	mutex_lock(&adev->pc_sqtt_mutex);
	if (job->pc_wa_disable)
		amdgpu_job_pc_workaround_disable(ring);
	if (job->sqtt_wa_disable)
		amdgpu_job_sqtt_workaround_disable(ring);
	mutex_unlock(&adev->pc_sqtt_mutex);

	kfree(job);
}

void amdgpu_job_set_gang_leader(struct amdgpu_job *job,
				struct amdgpu_job *leader)
{
	struct dma_fence *fence = &leader->base.s_fence->scheduled;

	WARN_ON(job->gang_submit);

	/*
	 * Don't add a reference when we are the gang leader to avoid circle
	 * dependency.
	 */
	if (job != leader)
		dma_fence_get(fence);
	job->gang_submit = fence;
}

void amdgpu_job_free(struct amdgpu_job *job)
{
	amdgpu_job_free_resources(job);

	dma_fence_put(job->fence);
	amdgpu_sync_free(&job->sync);
	amdgpu_sync_free(&job->sched_sync);
	if (job->gang_submit != &job->base.s_fence->scheduled)
		dma_fence_put(job->gang_submit);

	kfree(job);
}

int amdgpu_job_submit(struct amdgpu_job *job, struct drm_sched_entity *entity,
		      void *owner, struct dma_fence **f)
{
	int r;

	if (!f)
		return -EINVAL;

	r = drm_sched_job_init(&job->base, entity, owner);
	if (r)
		return r;

	drm_sched_job_arm(&job->base);

	*f = dma_fence_get(&job->base.s_fence->finished);
	amdgpu_job_free_resources(job);
	drm_sched_entity_push_job(&job->base);

	return 0;
}

int amdgpu_job_submit_direct(struct amdgpu_job *job, struct amdgpu_ring *ring,
			     struct dma_fence **fence)
{
	int r;

	job->base.sched = &ring->sched;
	r = amdgpu_ib_schedule(ring, job->num_ibs, job->ibs, NULL, fence);
	job->fence = dma_fence_get(*fence);
	if (r)
		return r;

	/* update ring fence seq by SW */
	if (job->ifh_mode &&
	    (ring->funcs->type == AMDGPU_RING_TYPE_GFX ||
	     ring->funcs->type == AMDGPU_RING_TYPE_SDMA ||
	     ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE))
		amdgpu_fence_driver_force_completion(ring);

	amdgpu_job_free(job);
	return 0;
}

static struct dma_fence *amdgpu_job_prepare_job(struct drm_sched_job *sched_job,
					       struct drm_sched_entity *s_entity)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(s_entity->rq->sched);
	struct amdgpu_job *job = to_amdgpu_job(sched_job);
	struct amdgpu_vm *vm = job->vm;
	struct dma_fence *fence;
	int r;

	fence = amdgpu_sync_get_fence(&job->sync);

	while (fence == NULL && vm && !job->vmid) {
		r = amdgpu_vmid_grab(vm, ring, &job->sync,
				     &job->base.s_fence->finished,
				     job);
		if (r)
			DRM_ERROR("Error getting VM ID (%d)\n", r);

		fence = amdgpu_sync_get_fence(&job->sync);
	}

	/* get fence on tmz queue after vmid is ready */
	if (!fence && vm && job->vmid && job->ctx->secure_mode && amdgpu_tmz) {
		r = amdgpu_sws_get_tmz_queue(ring,
					     &job->base.s_fence->finished,
					     job);

		if (r)
			DRM_ERROR("Error getting tmz queue (%d)\n", r);

		fence = amdgpu_sync_get_fence(&job->sync);
	}

	if (!fence && job->gang_submit)
		fence = amdgpu_device_switch_gang(ring->adev, job->gang_submit);

	return fence;
}

static struct dma_fence *amdgpu_job_run(struct drm_sched_job *sched_job)
{
	struct amdgpu_ring *ring = to_amdgpu_ring(sched_job->sched);
	struct amdgpu_device *adev = ring->adev;
	struct dma_fence *fence = NULL, *finished;
	struct amdgpu_job *job;
	int r = 0;

	if (adev->runpm) {
		r = pm_runtime_get_sync(adev->ddev.dev);
		if (r < 0)
			goto pm_put;
		r = 0;
	}

	sgpu_ifpo_lock(adev);

	job = to_amdgpu_job(sched_job);
	finished = &job->base.s_fence->finished;

	WARN_ON(amdgpu_sync_peek_fence(&job->sync, NULL));

	trace_amdgpu_sched_run_job(job);

	/* Is workaround needed for this job */
	if (!amdgpu_in_reset(adev)) {
		mutex_lock(&adev->pc_sqtt_mutex);
		amdgpu_job_track_pc_sqtt(adev, job);
		if (job->pc_wa_enable)
			amdgpu_job_pc_workaround_enable(ring);
		if (job->sqtt_wa_enable)
			amdgpu_job_sqtt_workaround_enable(ring);
		mutex_unlock(&adev->pc_sqtt_mutex);
	}

	/* Skip job if VRAM is lost and never resubmit gangs */
	if (job->vram_lost_counter != atomic_read(&adev->vram_lost_counter))
	    /* TODO: || (job->job_run_counter && job->gang_submit) */
		dma_fence_set_error(finished, -ECANCELED);

	if (finished->error < 0) {
		DRM_INFO("Skip scheduling IBs!\n");
	} else if (job->vm->process_flags == PF_EXITING) {
		DRM_INFO("Skip scheduling IBs PF_EXITING!\n");
		dma_fence_set_error(finished, -ENOEXEC);
	} else {
		r = amdgpu_ib_schedule(ring, job->num_ibs, job->ibs, job,
				       &fence);
		if (r) {
			DRM_ERROR("Error scheduling IBs (%d)\n", r);
		} else {
			if (sgpu_hang_detect) {
				atomic_inc(&adev->hang_gfx_job_cnt);
				if (!test_bit(HANG_DETECT_RUNNING, &adev->hang_detect_flags))
					wake_up_interruptible(&adev->hang_detect_wake_up);
			}

			if (amdgpu_fault_detect) {
				if (ring->funcs->type ==
						AMDGPU_RING_TYPE_GFX) {

					atomic_inc(&adev->gfx_job_cnt);

					set_bit(
					FAULT_DETECT_GFX_ACTIVE,
					&adev->fault_detect_flags);

					if (!test_bit(FAULT_DETECT_RUNNING,
						&adev->fault_detect_flags)) {

						set_bit(
						FAULT_DETECT_WAKEUP,
						&adev->fault_detect_flags
						);

						wake_up(
						&adev->fault_detect_wake_up);
					}
				} else if (ring->funcs->type
						== AMDGPU_RING_TYPE_COMPUTE) {

					atomic_inc(&adev->compute_job_cnt);

					set_bit(
					FAULT_DETECT_COMPUTE_ACTIVE,
					&adev->fault_detect_flags);

					if (!test_bit(FAULT_DETECT_RUNNING,
						&adev->fault_detect_flags)) {

						set_bit(
						FAULT_DETECT_WAKEUP,
						&adev->fault_detect_flags);

						wake_up(
						&adev->fault_detect_wake_up);
					}
				}
			}
		}
	}
	/* if gpu reset, hw fence will be replaced here */
	dma_fence_put(job->fence);
	job->fence = dma_fence_get(fence);

	amdgpu_job_free_resources(job);

	/* update ring fence seq by SW */
	if (job->ifh_mode &&
	    (ring->funcs->type == AMDGPU_RING_TYPE_GFX ||
	     ring->funcs->type == AMDGPU_RING_TYPE_SDMA ||
	     ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE))
		amdgpu_fence_driver_force_completion(ring);

	fence = r ? ERR_PTR(r) : fence;

	sgpu_ifpo_unlock(adev);

pm_put:
	if (adev->runpm)
		pm_runtime_put_autosuspend(adev->ddev.dev);

	return fence;
}

#define to_drm_sched_job(sched_job)		\
		container_of((sched_job), struct drm_sched_job, queue_node)

void amdgpu_job_stop_all_jobs_on_sched(struct drm_gpu_scheduler *sched)
{
	struct drm_sched_job *s_job;
	struct drm_sched_entity *s_entity = NULL;
	int i;

	/* Signal all jobs not yet scheduled */
	for (i = DRM_SCHED_PRIORITY_COUNT - 1; i >= DRM_SCHED_PRIORITY_MIN; i--) {
		struct drm_sched_rq *rq = &sched->sched_rq[i];

		if (!rq)
			continue;

		spin_lock(&rq->lock);
		list_for_each_entry(s_entity, &rq->entities, list) {
			while ((s_job = to_drm_sched_job(spsc_queue_pop(&s_entity->job_queue)))) {
				struct drm_sched_fence *s_fence = s_job->s_fence;

				dma_fence_signal(&s_fence->scheduled);
				dma_fence_set_error(&s_fence->finished, -EHWPOISON);
				dma_fence_signal(&s_fence->finished);
			}
		}
		spin_unlock(&rq->lock);
	}

	/* Signal all jobs already scheduled to HW */
	list_for_each_entry(s_job, &sched->pending_list, list) {
		struct drm_sched_fence *s_fence = s_job->s_fence;

		dma_fence_set_error(&s_fence->finished, -EHWPOISON);
		dma_fence_signal(&s_fence->finished);
	}
}

const struct drm_sched_backend_ops amdgpu_sched_ops = {
	.prepare_job = amdgpu_job_prepare_job,
	.run_job = amdgpu_job_run,
	.timedout_job = amdgpu_job_timedout,
	.free_job = amdgpu_job_free_cb
};
