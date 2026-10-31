// SPDX-License-Identifier: GPL-2.0-only
/*
 * @file sgpu_gvf_governor.h
 * @copyright 2024 Samsung Electronics
 */

#ifndef __SGPU_GVF_GOVERNOR_H__
#define __SGPU_GVF_GOVERNOR_H__

#define SGPU_GVF_DEFAULT_GOVERNOR	SGPU_GVF_GOVERNOR_RATIO

enum sgpu_gvf_governor_type {
	SGPU_GVF_GOVERNOR_RATIO,
	SGPU_GVF_GOVERNOR_MAX,
};

struct sgpu_gvf_governor {
	char *name;

	void (*init)(struct amdgpu_device *adev, uint32_t level);
	uint64_t (*calc_idle)(struct amdgpu_device *adev);
};


#define SGPU_GVF_DEFAULT_INJECTOR	SGPU_GVF_INJECTOR_HW_QUEUE_CONTROL

enum sgpu_gvf_idle_injector_type {
	SGPU_GVF_INJECTOR_SCHED_CONTROL,
	SGPU_GVF_INJECTOR_HW_QUEUE_CONTROL,
	SGPU_GVF_INJECTOR_MAX,
};

struct sgpu_gvf_injector {
	char *name;

	void (*enter_idle)(struct amdgpu_device *adev);
	void (*exit_idle)(struct amdgpu_device *adev);
};

#if IS_ENABLED(CONFIG_DRM_SGPU_GVF)
int sgpu_gvf_governor_set(struct amdgpu_device *adev, uint32_t id);
int sgpu_gvf_injector_set(struct amdgpu_device *adev, uint32_t id);
extern struct kobj_attribute attr_sgpu_gvf_governor;
extern struct kobj_attribute attr_sgpu_gvf_governor_list;
extern struct kobj_attribute attr_sgpu_gvf_injector;
extern struct kobj_attribute attr_sgpu_gvf_injector_list;
#else
#define sgpu_gvf_governor_set(adev, id)		do { } while (0)
#define sgpu_gvf_injector_set(adev, id)		do { } while (0)
#endif /* CONFIG_DRM_SGPU_GVF */

#endif /* __SGPU_GVF_GOVERNOR_H__ */
