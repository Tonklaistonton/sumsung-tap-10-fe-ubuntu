/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *            http://www.samsung.com
 */

#ifndef _SGPU_IFPO_H_
#define _SGPU_IFPO_H_

#include <linux/completion.h>

struct amdgpu_device;

enum sgpu_ifpo_type {
	IFPO_DISABLED	= 0,
	IFPO_ABORT	= 1,
	IFPO_HALF_AUTO	= 2,

	IFPO_TYPE_LAST
};

struct sgpu_ifpo_func {
	void (*enable)(struct amdgpu_device *adev);
	void (*suspend)(struct amdgpu_device *adev);
	void (*resume)(struct amdgpu_device *adev);
	void (*lock)(struct amdgpu_device *adev, bool nowait);
	void (*unlock)(struct amdgpu_device *adev);
	void (*reset)(struct amdgpu_device *adev);
	/* IFPO disable request from users */
	void (*user_disable)(struct amdgpu_device *adev, int count, bool disable);
	/* Make GPU enter IFPO forcebly */
	void (*force_enter)(struct amdgpu_device *adev);
	/* Release force-enter state */
	void (*force_exit)(struct amdgpu_device *adev);
};

struct sgpu_ifpo {
	enum sgpu_ifpo_type	type;
	uint32_t		cal_id;

	atomic_t		count;
	bool			state;
	spinlock_t		lock;

	struct completion	force_idle;

#ifdef CONFIG_DEBUG_FS
	struct dentry		*debugfs_disable;
#endif

	struct sgpu_ifpo_func	*func;
};

#ifdef CONFIG_DRM_SGPU_EXYNOS

void sgpu_ifpo_init(struct amdgpu_device *adev);
uint32_t sgpu_ifpo_get_hw_lock_value(struct amdgpu_device *adev);

void sgpu_ifpo_enable(struct amdgpu_device *adev);
void sgpu_ifpo_suspend(struct amdgpu_device *adev);
void sgpu_ifpo_resume(struct amdgpu_device *adev);
void sgpu_ifpo_lock(struct amdgpu_device *adev);
void sgpu_ifpo_lock_nowait(struct amdgpu_device *adev);
void sgpu_ifpo_unlock(struct amdgpu_device *adev);
void sgpu_ifpo_reset(struct amdgpu_device *adev);
void sgpu_ifpo_user_disable(struct amdgpu_device *adev, bool disable);
void sgpu_ifpo_force_enter(struct amdgpu_device *adev);
void sgpu_ifpo_force_exit(struct amdgpu_device *adev);

int sgpu_ifpo_debugfs_init(struct drm_minor *minor);

extern struct kobj_attribute attr_gpu_ifpo_disable;
extern struct kobj_attribute attr_gpu_ifpo_state;

#else

#define sgpu_ifpo_init(adev)				do { } while (0)
#define sgpu_ifpo_get_hw_lock_value(adev)		(0)

#define sgpu_ifpo_enable(adev)				do { } while (0)
#define sgpu_ifpo_suspend(adev)				do { } while (0)
#define sgpu_ifpo_resume(adev)				do { } while (0)
#define sgpu_ifpo_lock(adev)				do { } while (0)
#define sgpu_ifpo_lock_nowait(adev)			do { } while (0)
#define sgpu_ifpo_unlock(adev)				do { } while (0)
#define sgpu_ifpo_reset(adev)				do { } while (0)
#define sgpu_ifpo_user_disable(adev, disable)		do { } while (0)
#define sgpu_ifpo_force_enter(adev)			do { } while (0)
#define sgpu_ifpo_force_exit(adev)			do { } while (0)

#define sgpu_ifpo_debugfs_init(minor)			(0)

#endif /* CONFIG_DRM_SGPU_EXYNOS */

#endif /* _SGPU_IFPO_H_ */
