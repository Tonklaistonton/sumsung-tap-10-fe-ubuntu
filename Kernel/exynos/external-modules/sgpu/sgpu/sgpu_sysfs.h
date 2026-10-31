#ifndef __SGPU_SYSFS_H__
#define __SGPU_SYSFS_H__

struct amdgpu_device;

#if IS_ENABLED(CONFIG_DRM_SGPU_EXYNOS)

#define kobj_to_adev(kobj) container_of(kobj, struct amdgpu_device, sysfs_kobj)

int sgpu_sysfs_init(struct amdgpu_device *adev);
void sgpu_sysfs_deinit(struct amdgpu_device *adev);
#else
static inline int sgpu_sysfs_init(struct amdgpu_device *adev)
{
	return 0;
}
#define sgpu_sysfs_deinit(adev) do { } while (0)
#endif
#endif /* __SGPU_SYSFS_H__ */
