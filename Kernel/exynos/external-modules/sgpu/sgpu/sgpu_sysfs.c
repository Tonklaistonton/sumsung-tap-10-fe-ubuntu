#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/sysfs.h>

#include <trace/events/gpu_mem.h>

#ifdef CONFIG_DRM_SGPU_VENDOR_HOOKS
#include <trace/hooks/mm.h>
#endif

#include "amdgpu.h"
#include "amdgpu_gem.h"
#include "sgpu_swap.h"
#include "sgpu_sysfs.h"
#include "sgpu_pm_monitor.h"

#define PAGE_IN_KB (PAGE_SIZE / SZ_1K)

#if IS_ENABLED(CONFIG_DRM_SGPU_VENDOR_HOOKS)

static void sgpu_meminfo_gputotal_show(struct amdgpu_device *adev, struct seq_file *m)
{
	seq_printf(m, "GpuTotal:       %8ld kB\n",
		   (adev->num_kernel_pages + sgpu_get_nr_total_pages()) * PAGE_IN_KB);
}

#if IS_ENABLED(CONFIG_DRM_SGPU_GRAPHIC_MEMORY_RECLAIM)

static void sgpu_meminfo_gpuswap_show(struct seq_file *m)
{
	seq_printf(m, "GpuSwap:        %8ld kB\n", sgpu_get_nr_swap_pages() * PAGE_IN_KB);
}
#else
#define sgpu_meminfo_gpuswap_show(m) do { } while (0)
#endif

static void gpu_meminfo_show(void *data, struct seq_file *m)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	sgpu_meminfo_gputotal_show(adev, m);
	sgpu_meminfo_gpuswap_show(m);

	trace_gpu_mem_total(0, 0, (adev->num_kernel_pages + sgpu_get_nr_total_pages()) * PAGE_SIZE);
}

static void gpu_mem_show(void *data, unsigned int filter, nodemask_t *nodemask)
{
	size_t total_num_pages = 0;
	struct amdgpu_device *adev = data;
	struct drm_device *ddev = &adev->ddev;

	if (mutex_trylock(&ddev->filelist_mutex)) {
		struct drm_file *file;

		list_for_each_entry(file, &ddev->filelist, lhead) {
			size_t num_pages;
			struct amdgpu_fpriv *afpriv = file->driver_priv;

			if (!afpriv)
				continue;

			num_pages = afpriv->total_pages;
			if (!num_pages)
				continue;

			pr_info("pid: %6d %12zu kB\n", afpriv->tgid, num_pages * PAGE_IN_KB);
			total_num_pages += num_pages;
		}

		mutex_unlock(&ddev->filelist_mutex);
	} else {
		total_num_pages = sgpu_get_nr_total_pages();
		pr_info("pid:  user  %12zu kB\n", total_num_pages * PAGE_IN_KB);
	}

	pr_info("pid: kernel %12zu kB\n", adev->num_kernel_pages * PAGE_IN_KB);
	pr_info("total:   %15zu kB (gpuswap %zu kB)\n",
		(total_num_pages + adev->num_kernel_pages) * PAGE_IN_KB,
		sgpu_get_nr_swap_pages() * PAGE_IN_KB);
}

void sgpu_register_android_vh(struct amdgpu_device *adev)
{
	register_trace_android_vh_show_mem(gpu_mem_show, adev);
	register_trace_android_vh_meminfo_proc_show(gpu_meminfo_show, adev);
}

void sgpu_unregister_android_vh(struct amdgpu_device *adev)
{
	unregister_trace_android_vh_show_mem(gpu_mem_show, adev);
	unregister_trace_android_vh_meminfo_proc_show(gpu_meminfo_show, adev);
}
#else /* CONFIG_DRM_SGPU_VENDOR_HOOKS */
#define sgpu_register_android_vh(adev) do { } while (0)
#define sgpu_unregister_android_vh(adev) do { } while (0)
#endif /* CONFIG_DRM_SGPU_VENDOR_HOOKS */

static ssize_t mem_info_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	struct drm_device *ddev = adev_to_drm(adev);
	struct drm_file *file;
	ssize_t count = 0;
	size_t total_num_pages = 0;
	int r = 0;

	if (!ddev)
		return r;

	r = mutex_lock_interruptible(&ddev->filelist_mutex);
	if (r)
		return r;

	list_for_each_entry(file, &ddev->filelist, lhead) {
		size_t num_pages;
		struct amdgpu_fpriv *afpriv = file->driver_priv;

		if (!afpriv)
			continue;

		num_pages = afpriv->total_pages;
		if (!num_pages)
			continue;

		count += scnprintf(&buf[count], PAGE_SIZE - count, "pid: %6d %15zu\n",
				   afpriv->tgid, num_pages * PAGE_SIZE);
		total_num_pages += num_pages;
	}

	mutex_unlock(&ddev->filelist_mutex);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "pid: %6d %15zu\n",
			   0, adev->num_kernel_pages * PAGE_SIZE);
	total_num_pages += adev->num_kernel_pages;
	count += scnprintf(&buf[count], PAGE_SIZE - count, "total:      %15zu\n",
			   total_num_pages * PAGE_SIZE);
	return count;
}
static struct kobj_attribute attr_mem_info = __ATTR_RO_MODE(mem_info, 0440);

static ssize_t ctx_mem_info_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	struct drm_device *ddev = adev_to_drm(adev);
	struct drm_file *file;
	ssize_t count = 0;
	int r = 0;

	if (!ddev)
		return r;

	r = mutex_lock_interruptible(&ddev->filelist_mutex);
	if (r)
		return r;

	count += scnprintf(&buf[count], PAGE_SIZE - count, "PID    SIZE\n");

	list_for_each_entry(file, &ddev->filelist, lhead) {
		u64 total_ctx_mem;
		uint32_t id = 0;
		struct amdgpu_ctx *ctx;
		struct amdgpu_fpriv *afpriv = file->driver_priv;

		if (!afpriv)
			continue;

		mutex_lock(&afpriv->memory_lock);
		if (afpriv->total_pages == 0) {
			mutex_unlock(&afpriv->memory_lock);
			continue;
		}

		total_ctx_mem = 0;
		idr_for_each_entry(&afpriv->ctx_mgr.ctx_handles, ctx, id)
			total_ctx_mem += ctx->mem_size;

		if (total_ctx_mem > 0)
			count += scnprintf(&buf[count], PAGE_SIZE - count, "%6d %12llu\n",
					   afpriv->tgid, total_ctx_mem);

		mutex_unlock(&afpriv->memory_lock);
	}

	mutex_unlock(&ddev->filelist_mutex);

	return count;
}

static struct kobj_attribute attr_ctx_mem_info = __ATTR_RO_MODE(ctx_mem_info, 0440);

static ssize_t gpu_model_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct amdgpu_device *adev = kobj_to_adev(kobj);
	const char *product = "Unknown";

	if (AMDGPU_IS_MGFX0(adev->grbm_chip_rev))
		product = "920";
	else if (AMDGPU_IS_MGFX1(adev->grbm_chip_rev))
		product = "930";
	else if (AMDGPU_IS_MGFX2(adev->grbm_chip_rev))
		product = "940";
	else if (AMDGPU_IS_MGFX3(adev->grbm_chip_rev))
		product = "950";
	else if (AMDGPU_IS_MGFX1_MID(adev->grbm_chip_rev))
		product = "530";
	else if (AMDGPU_IS_MGFX2_MID(adev->grbm_chip_rev))
		product = "540";

	return scnprintf(buf, PAGE_SIZE, "Samsung Xclipse %s\n", product);
}
static struct kobj_attribute attr_gpu_model = __ATTR_RO_MODE(gpu_model, 0440);

#ifdef CONFIG_DRM_SGPU_DVFS
#include "exynos_gpu_interface.h"

static ssize_t gpu_mm_min_clock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", sgpu_get_gpu_mm_min_clock(kobj_to_adev(kobj)));
}

static ssize_t gpu_mm_min_clock_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long freq, delay;
	int ret;

	ret = sscanf(buf, "%lu", &freq);
	if (ret != 1)
		return -EINVAL;

	buf = strpbrk(buf, " ");
	if (buf == NULL)
		return -EINVAL;

	buf++;
	ret = sscanf(buf, "%lu", &delay);
	if (ret != 1)
		return -EINVAL;

	sgpu_set_gpu_mm_min_clock(kobj_to_adev(kobj), freq, delay);

	return count;
}
static struct kobj_attribute attr_gpu_mm_min_clock = __ATTR_RW_MODE(gpu_mm_min_clock, 0660);

static ssize_t gpu_disable_llc_way_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", sgpu_is_llc_way_disabled());
}

static ssize_t gpu_disable_llc_way_store(struct kobject *kobj, struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		return ret;

	sgpu_disable_llc_way(!!value);

	return count;
}
static struct kobj_attribute attr_gpu_disable_llc_way = __ATTR_RW_MODE(gpu_disable_llc_way, 0640);

static ssize_t gpu_cl_boost_disable_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", sgpu_get_cl_boost_disabled(kobj_to_adev(kobj)));
}

static ssize_t gpu_cl_boost_disable_store(struct kobject *kobj, struct kobj_attribute *attr,
					  const char *buf, size_t count)
{
	int ret, mode;

	ret = kstrtos32(buf, 0, &mode);
	if (ret != 0)
		return -EINVAL;
	if (mode != 0 && mode != 1)
		return -EINVAL;

	sgpu_set_cl_boost_disabled(kobj_to_adev(kobj), !!mode);

	return count;
}
static struct kobj_attribute attr_gpu_cl_boost_disable = __ATTR_RW_MODE(gpu_cl_boost_disable, 0640);

static ssize_t gpu_min_clock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", sgpu_get_gpu_min_clock());
}

static ssize_t gpu_min_clock_store(struct kobject *kobj, struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long freq;
	int ret = 0;

	ret = sscanf(buf, "%lu", &freq);
	if (ret != 1)
		return -EINVAL;

	sgpu_set_gpu_min_clock(kobj_to_adev(kobj), freq);

	return count;
}
static struct kobj_attribute attr_gpu_min_clock = __ATTR_RW_MODE(gpu_min_clock, 0640);

static ssize_t gpu_max_clock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", sgpu_get_gpu_max_clock());
}

static ssize_t gpu_max_clock_store(struct kobject *kobj, struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned long freq;
	int ret = 0;

	ret = sscanf(buf, "%lu", &freq);
	if (ret != 1)
		return -EINVAL;

	sgpu_set_gpu_max_clock(kobj_to_adev(kobj), freq);

	return count;
}
static struct kobj_attribute attr_gpu_max_clock = __ATTR_RW_MODE(gpu_max_clock, 0640);

static ssize_t gpu_siop_max_clock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", sgpu_get_gpu_siop_max_clock());
}

static ssize_t gpu_siop_max_clock_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int freq;
	int ret = 0;

	ret = kstrtou32(buf, 0, &freq);
	if (ret)
		return ret;

	sgpu_set_gpu_siop_max_clock(kobj_to_adev(kobj), freq);

	return count;
}
static struct kobj_attribute attr_gpu_siop_max_clock = __ATTR_RW_MODE(gpu_siop_max_clock, 0640);

static ssize_t gpu_busy_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{

	return scnprintf(buf, PAGE_SIZE, "%llu\n", sgpu_get_gpu_utilization(kobj_to_adev(kobj)));

}
static struct kobj_attribute attr_gpu_busy = __ATTR_RO_MODE(gpu_busy, 0440);

static ssize_t gpu_clock_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", sgpu_get_gpu_clock(kobj_to_adev(kobj)));
}
static struct kobj_attribute attr_gpu_clock = __ATTR_RO_MODE(gpu_clock, 0440);

static ssize_t gpu_freq_table_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	unsigned long *freq_table;
	ssize_t count = 0;
	int nent;

	nent = sgpu_get_gpu_freq_table(kobj_to_adev(kobj), &freq_table);
	if (nent < 0)
		return nent;

	while (nent-- > 0)
		count += scnprintf(&buf[count], PAGE_SIZE - count,
				   "%lu ", kobj_to_adev(kobj)->devfreq->profile->freq_table[nent]);

	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}
static struct kobj_attribute attr_gpu_freq_table = __ATTR_RO_MODE(gpu_freq_table, 0440);

static ssize_t gpu_governor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t count;

	count = sgpu_governor_current_info_show(kobj_to_adev(kobj)->devfreq, buf, PAGE_SIZE);
	if (count < 0) {
		DRM_WARN("Error reading current gpu governor info: %zd\n", count);
		return -EINVAL;
	}
	count += scnprintf(&buf[count], PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t gpu_governor_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	char str_governor[DEVFREQ_NAME_LEN + 1];
	int ret;

	ret = sscanf(buf, "%" __stringify(DEVFREQ_NAME_LEN) "s", str_governor);
	if (ret != 1)
		return -EINVAL;

	ret = sgpu_governor_change(kobj_to_adev(kobj)->devfreq, str_governor);
	if (ret)
		return ret;

	return count;
}
static struct kobj_attribute attr_gpu_governor = __ATTR_RW_MODE(gpu_governor, 0640);

static ssize_t gpu_available_governor_show(struct kobject *kobj, struct kobj_attribute *attr,
					   char *buf)
{
	return sgpu_governor_all_info_show(kobj_to_adev(kobj)->devfreq, buf);
}
static struct kobj_attribute attr_gpu_available_governor =
					__ATTR_RO_MODE(gpu_available_governor, 0440);

static ssize_t gpu_clock_stats_show(struct kobject *kobj, struct kobj_attribute *attr,
					   char *buf)
{
	return sgpu_devfreq_time_in_state_show(kobj_to_adev(kobj), buf);
}
static struct kobj_attribute attr_gpu_clock_stats = __ATTR_RO_MODE(gpu_clock_stats, 0440);

#endif /* CONFIG_DRM_SGPU_DVFS */

#if IS_ENABLED(CONFIG_GPU_THERMAL)
#include <linux/thermal.h>
#include <soc/samsung/tmu.h>

#define MCELSIUS	1000

static ssize_t gpu_tmu_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t count;
	int gpu_temp = 0, gpu_temp_int = 0, gpu_temp_point = 0, ret;
	static struct thermal_zone_device *tz;

	if(!tz)
		tz = thermal_zone_get_zone_by_name("G3D");

	ret = thermal_zone_get_temp(tz, &gpu_temp);
	if (ret) {
		DRM_WARN("Error reading temp of gpu thermal zone: %d\n", ret);
		return -EINVAL;
	}

#if !IS_ENABLED(CONFIG_EXYNOS_THERMAL_V2)
	gpu_temp *= MCELSIUS;
#endif

	gpu_temp_int = gpu_temp / 1000;
	gpu_temp_point = gpu_temp % 1000; /* % gpu_temp_int */
	count = scnprintf(buf, PAGE_SIZE, "%d.%-3d\n", gpu_temp_int, gpu_temp_point);

	return count;
}
static struct kobj_attribute attr_gpu_tmu = __ATTR_RO_MODE(gpu_tmu, 0440);
#endif

static struct attribute *sgpu_sysfs_entries[] = {
	&attr_gpu_model.attr,
	&attr_mem_info.attr,
	&attr_ctx_mem_info.attr,
#ifdef CONFIG_DRM_SGPU_GRAPHIC_MEMORY_RECLAIM
	&attr_gpu_swap_ctrl.attr,
#endif
#ifdef CONFIG_DRM_SGPU_DVFS
	&attr_gpu_disable_llc_way.attr,
	&attr_gpu_mm_min_clock.attr,
	&attr_gpu_cl_boost_disable.attr,
	&attr_gpu_min_clock.attr,
	&attr_gpu_max_clock.attr,
	&attr_gpu_siop_max_clock.attr,
	&attr_gpu_busy.attr,
	&attr_gpu_clock.attr,
	&attr_gpu_freq_table.attr,
	&attr_gpu_governor.attr,
	&attr_gpu_available_governor.attr,
	&attr_gpu_clock_stats.attr,
#endif
#if IS_ENABLED(CONFIG_GPU_THERMAL)
	&attr_gpu_tmu.attr,
#endif
	&attr_gpu_pm_monitor.attr,
#if IS_ENABLED(CONFIG_DRM_SGPU_GVF)
	&attr_sgpu_gvf_governor.attr,
	&attr_sgpu_gvf_governor_list.attr,
	&attr_sgpu_gvf_injector.attr,
	&attr_sgpu_gvf_injector_list.attr,
#endif
	&attr_gpu_ifpo_disable.attr,
	&attr_gpu_ifpo_state.attr,
	NULL,
};

static struct attribute_group sgpu_sysfs_attr_group = {
	.attrs = sgpu_sysfs_entries,
};

static void sgpu_sysfs_release(struct kobject *kobj)
{
}

static struct kobj_type sgpu_sysfs_ktype = {
	.release = sgpu_sysfs_release,
	.sysfs_ops = &kobj_sysfs_ops,
};

int sgpu_sysfs_init(struct amdgpu_device *adev)
{
	int ret;

	ret = kobject_init_and_add(&adev->sysfs_kobj, &sgpu_sysfs_ktype, kernel_kobj, "gpu");
	if(ret) {
		DRM_ERROR("failed to create sysfs kobj for SGPU");
		return ret;
	}

	ret = sysfs_create_group(&adev->sysfs_kobj, &sgpu_sysfs_attr_group);
	if (ret) {
		DRM_ERROR("failed to create sysfs files for SGPU");
		goto err;
	}

	ret = sgpu_sysfs_proc_init(&adev->sysfs_kobj);
	if (ret)
		goto err;

	sgpu_register_android_vh(adev);

	return 0;
err:
	kobject_put(&adev->sysfs_kobj);
	return ret;
}

void sgpu_sysfs_deinit(struct amdgpu_device *adev)
{
	sgpu_unregister_android_vh(adev);
	kobject_put(&adev->sysfs_kobj);
}
