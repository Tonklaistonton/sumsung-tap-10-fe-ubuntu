// SPDX-License-Identifier: GPL-2.0-only
/*
 * @file sgpu_gvf.c
 * @copyright 2024 Samsung Electronics
 */

#include <linux/ktime.h>
#include <linux/delay.h>

#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "sgpu_governor.h"
#include "exynos_gpu_interface.h"

static uint32_t *gvf_param_table;
static uint32_t gvf_param_max_level;

uint32_t sgpu_gvf_get_param(uint32_t level, uint32_t param)
{
	WARN_ON(level >= gvf_param_max_level || param >= SGPU_GVF_PARAM_NR);

	return gvf_param_table[(level) * SGPU_GVF_PARAM_NR + (param)];
}

void sgpu_gvf_set_param(uint32_t level, uint32_t param, uint32_t val)
{
	WARN_ON(level >= gvf_param_max_level || param >= SGPU_GVF_PARAM_NR);

	gvf_param_table[(level) * SGPU_GVF_PARAM_NR + (param)] = val;
}

uint32_t sgpu_gvf_get_max_level(struct amdgpu_device *adev)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	if (!gvf->initialized)
		return 0;

	return gvf->max_level - 1;
}

uint32_t sgpu_gvf_get_run_freq(struct amdgpu_device *adev)
{
	return adev->gvf.run_freq;
}

static void sgpu_gvf_refresh_window(struct amdgpu_device *adev)
{
	struct sgpu_gvf *gvf = &adev->gvf;
	uint64_t cur_idle_time_ns = sgpu_pm_monitor_get_idle_time(adev);
	uint64_t cur_time_ns = ktime_get_ns();
	uint64_t total_time_ns = cur_time_ns - gvf->base_time_ns;
	uint64_t total_idle_time_ns, idle_ratio;

	if (total_time_ns / NSEC_PER_MSEC < gvf->monitor_window_ms)
		return;

	total_idle_time_ns = cur_idle_time_ns - gvf->base_idle_time_ns;
	idle_ratio = (total_idle_time_ns * 100) / total_time_ns;

	trace_sgpu_gvf_refresh_window(idle_ratio, total_idle_time_ns, total_time_ns);

	dev_info(adev->dev, "Refresh GVF Window : idle %llu%% in %llums",
				idle_ratio, total_time_ns / NSEC_PER_MSEC);

	gvf->base_time_ns = cur_time_ns;
	gvf->base_idle_time_ns = cur_idle_time_ns;
}

static bool sgpu_gvf_paused(void)
{
	if (kthread_should_park()) {
		kthread_parkme();
		return true;
	}

	return false;
}

static bool sgpu_gvf_check_sampling_time(struct sgpu_gvf *gvf)
{
	if (time_after(jiffies, gvf->expire_jiffies))
		return true;

	mod_timer(&gvf->wakeup_timer, gvf->expire_jiffies);

	return false;
}

static int sgpu_gvf_main(void *param)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)param;
	struct sgpu_gvf *gvf = &adev->gvf;
	uint32_t idle_time_ms = 0;

	while (!kthread_should_stop()) {
		wait_event_interruptible(gvf->waitq,
					(!sgpu_gvf_paused() && gvf->enable &&
					sgpu_gvf_check_sampling_time(gvf)) ||
					kthread_should_stop());

		if (kthread_should_stop() || !gvf->enable)
			continue;

		gvf->expire_jiffies = jiffies + msecs_to_jiffies(gvf->sampling_time_ms);

		mutex_lock(&gvf->lock);

		if (gvf->governor->calc_idle)
			idle_time_ms = gvf->governor->calc_idle(adev) / NSEC_PER_MSEC;

		mutex_unlock(&gvf->lock);

		trace_sgpu_gvf_main("calc_idle", idle_time_ms);
		if (!idle_time_ms)
			goto skip_idle;

		trace_sgpu_gvf_main("enter_idle", idle_time_ms);
		if (gvf->injector->enter_idle)
			gvf->injector->enter_idle(adev);

		schedule_timeout_interruptible(msecs_to_jiffies(idle_time_ms));

		trace_sgpu_gvf_main("exit_idle", idle_time_ms);
		if (gvf->injector->exit_idle)
			gvf->injector->exit_idle(adev);
skip_idle:
		sgpu_gvf_refresh_window(adev);
	}

	return 0;
}

static void sgpu_gvf_set_enable(struct amdgpu_device *adev, bool enable)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	if (enable) {
		gvf->enable = true;

		gvf->base_time_ns = ktime_get_ns();
		gvf->base_idle_time_ns = sgpu_pm_monitor_get_idle_time(adev);
		gvf->expire_jiffies = jiffies + msecs_to_jiffies(gvf->sampling_time_ms);

		wake_up(&gvf->waitq);
	} else
		gvf->enable = false;

	trace_sgpu_gvf_set_enable(gvf->enable, gvf->monitor_window_ms,
					gvf->sampling_time_ms);

	dev_info(adev->dev, "%s enable(%u) monitor_window_ms(%u)",
			__func__, gvf->enable, gvf->monitor_window_ms);
}

void sgpu_gvf_set_level(struct amdgpu_device *adev, uint32_t level)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	if (!gvf->initialized || gvf->level == level)
		return;

	if (level >= gvf->max_level) {
		dev_info(adev->dev, "Invalid GVF level %d", level);
		return;
	}

	mutex_lock(&gvf->lock);

	if (level > 0) {
		if (gvf->governor->init)
			gvf->governor->init(adev, level);

		if (!gvf->level)
			sgpu_gvf_set_enable(adev, true);
		else
			dev_info(adev->dev, "GVF level change %u -> %u",
					gvf->level, level);
	} else
		sgpu_gvf_set_enable(adev, false);

	gvf->level = level;

	mutex_unlock(&gvf->lock);
}

void sgpu_gvf_resume(struct amdgpu_device *adev)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	if (gvf->initialized)
		kthread_unpark(gvf->thread);
}

void sgpu_gvf_suspend(struct amdgpu_device *adev)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	if (gvf->initialized)
		kthread_park(gvf->thread);
}

static void sgpu_gvf_wakeup(struct timer_list *t)
{
	struct sgpu_gvf *gvf = from_timer(gvf, t, wakeup_timer);

	wake_up(&gvf->waitq);
}

static void sgpu_gvf_param_init(struct amdgpu_device *adev)
{
	struct sgpu_gvf *gvf = &adev->gvf;
	uint32_t i;

	for (i = 1; i < gvf->max_level; i++) {
		sgpu_gvf_set_param(gvf->max_level - i,
					SGPU_GVF_PARAM_TARGET_RATIO,
					(gvf->table[i - 1] * 100 / gvf->run_freq));
		sgpu_gvf_set_param(gvf->max_level - i,
					SGPU_GVF_PARAM_SAMPLING_TIME_MS,
					SGPU_GVF_SAMPLING_TIME_MS);
		sgpu_gvf_set_param(gvf->max_level - i,
					SGPU_GVF_PARAM_MAX_REST_RATIO,
					SGPU_GVF_MAX_REST_RATIO);
		sgpu_gvf_set_param(gvf->max_level - i,
					SGPU_GVF_PARAM_MIN_REST_RATIO,
					SGPU_GVF_MIN_REST_RATIO);
	}
}

int sgpu_gvf_init(struct amdgpu_device *adev)
{
	struct device *dev = adev->dev;
	struct sgpu_gvf *gvf = &adev->gvf;
	int gvf_depth, ret = 0;
	uint32_t *gvf_table;

	gvf_depth = of_property_count_u32_elems(dev->of_node, "gvf_table");
	if (gvf_depth < 0) {
		dev_info(dev, "Disable GPU Virtual Frequency\n");
		return ret;
	}
	gvf->max_level = gvf_depth + 1;

	gvf_table = kcalloc(gvf_depth, sizeof(uint32_t), GFP_KERNEL);
	if (!gvf_table) {
		ret = -ENOMEM;
		goto error;
	}

	ret = of_property_read_u32_array(dev->of_node, "gvf_table",
					 gvf_table, gvf_depth);
	if (ret) {
		dev_err(dev, "Cannot read the gvf_table node in DT\n");
		goto error_kfree1;
	}
	gvf->table = gvf_table;

	ret = of_property_read_u32(dev->of_node, "gvf_run_freq", &gvf->run_freq);
	if (ret) {
		dev_err(dev, "Cannot read the gvf_run_freq node in DT\n");
		goto error_kfree1;
	}

	gvf_param_table = kcalloc(gvf->max_level * SGPU_GVF_PARAM_NR,
					sizeof(uint32_t), GFP_KERNEL);
	if (!gvf_param_table) {
		ret = -ENOMEM;
		goto error_kfree1;
	}
	gvf_param_max_level = gvf->max_level;

	sgpu_gvf_param_init(adev);

	mutex_init(&gvf->lock);

	gvf->enable = false;
	gvf->level = 0;

	gvf->monitor_window_ms = sgpu_gvf_window_ms;

	sgpu_gvf_governor_set(adev, SGPU_GVF_DEFAULT_GOVERNOR);
	sgpu_gvf_injector_set(adev, SGPU_GVF_DEFAULT_INJECTOR);

	init_waitqueue_head(&gvf->waitq);
	timer_setup(&gvf->wakeup_timer, sgpu_gvf_wakeup, 0);

	gvf->thread = kthread_run(sgpu_gvf_main, adev, "sgpu_gvf");
	if (IS_ERR(gvf->thread)) {
		ret = PTR_ERR(gvf->thread);
		gvf->thread = NULL;
		dev_err(adev->dev, "Failed to create thread for SGPU GVF\n");
		goto error_kfree2;
	}

	gvf->initialized = true;

	return 0;

error_kfree2:
	kfree(gvf_param_table);
error_kfree1:
	kfree(gvf_table);
error:
	return ret;
}

void sgpu_gvf_fini(struct amdgpu_device *adev)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	if (!gvf->initialized)
		return;

	if (gvf->thread)
		kthread_stop(gvf->thread);

	kfree(gvf->table);
	kfree(gvf_param_table);
}

/* Debugfs nodes */
#ifdef CONFIG_DEBUG_FS
static int sgpu_gvf_lock_freq_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	*val = adev->gvf.run_freq;

	return 0;
}

static int sgpu_gvf_lock_freq_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;
	struct devfreq_dev_profile *dp = adev->devfreq->profile;
	struct sgpu_gvf *gvf = &adev->gvf;
	uint32_t max_freq = dp->freq_table[0];
	uint32_t min_freq = dp->freq_table[dp->max_state - gvf->max_level];

	if (val > max_freq || val < min_freq)
		return -EINVAL;

	gvf->run_freq = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sgpu_debugfs_gvf_lock_freq_fops,
			sgpu_gvf_lock_freq_get,
			sgpu_gvf_lock_freq_set, "%llu\n");

static int sgpu_gvf_monitor_window_ms_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	*val = adev->gvf.monitor_window_ms;

	return 0;
}

static int sgpu_gvf_monitor_window_ms_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	adev->gvf.monitor_window_ms = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sgpu_debugfs_gvf_monitor_window_ms_fops,
			sgpu_gvf_monitor_window_ms_get,
			sgpu_gvf_monitor_window_ms_set, "%llu\n");

static void sgpu_gvf_set_enable_manual(struct amdgpu_device *adev, bool enable)
{
	struct sgpu_gvf *gvf = &adev->gvf;

	if (!gvf->initialized)
		return;

	if (adev->gvf.level > 0) {
		dev_err(adev->dev, "GVF is enabled by level %u", adev->gvf.level);
		return;
	}

	mutex_lock(&gvf->lock);

	if (enable) {
		/* Custom parameters are stored at level 0 */
		if (gvf->governor->init)
			gvf->governor->init(adev, 0);
		gpu_gvf_set_freq_lock(gvf->run_freq);
		sgpu_gvf_set_enable(adev, true);
	} else {
		sgpu_gvf_set_enable(adev, false);
		gpu_gvf_release_freq_lock();
	}

	mutex_unlock(&gvf->lock);
}

static int sgpu_gvf_start_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	*val = adev->gvf.enable;

	return 0;
}

static int sgpu_gvf_start_set(void *data, u64 val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;

	sgpu_gvf_set_enable_manual(adev, val ? true : false);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sgpu_debugfs_gvf_start_fops,
			sgpu_gvf_start_get,
			sgpu_gvf_start_set, "%llu\n");

static int sgpu_gvf_param_show(struct seq_file *m, void *p)
{
	seq_printf(m, "sampling_time(ms) = %u\n", sgpu_gvf_get_param(0,
			SGPU_GVF_PARAM_SAMPLING_TIME_MS));

	seq_printf(m, "target_idle_ratio(0-100%%) = %u\n", sgpu_gvf_get_param(0,
			SGPU_GVF_PARAM_TARGET_RATIO));

	seq_printf(m, "max_rest_ratio(0-100%%) = %u\n", sgpu_gvf_get_param(0,
			SGPU_GVF_PARAM_MAX_REST_RATIO));

	seq_printf(m, "min_rest_ratio(0-100%%) = %u\n", sgpu_gvf_get_param(0,
			SGPU_GVF_PARAM_MIN_REST_RATIO));

	return 0;
}

static ssize_t sgpu_gvf_param_write(struct file *f, const char __user *data,
		size_t len, loff_t *loff)
{
	char buf[20] = {0, };
	u32 param[SGPU_GVF_PARAM_NR];
	u32 param_min[] = {0, 0, 0, 0};
	u32 param_max[] = {UINT_MAX, 100, 100, 100};
	int r, i;

	if (len >= 20)
		return -EINVAL;

	if (copy_from_user(&buf, data, len))
		return -EFAULT;

	r = sscanf(buf, "%u %u %u %u", &param[SGPU_GVF_PARAM_SAMPLING_TIME_MS],
					&param[SGPU_GVF_PARAM_TARGET_RATIO],
					&param[SGPU_GVF_PARAM_MAX_REST_RATIO],
					&param[SGPU_GVF_PARAM_MIN_REST_RATIO]);
	if (r != SGPU_GVF_PARAM_NR)
		return -EINVAL;

	for (i = 0; i < SGPU_GVF_PARAM_NR; ++i)
		if (param[i] < param_min[i] || param[i] > param_max[i])
			return -EINVAL;

	sgpu_gvf_set_param(0, SGPU_GVF_PARAM_SAMPLING_TIME_MS,
			param[SGPU_GVF_PARAM_SAMPLING_TIME_MS]);

	sgpu_gvf_set_param(0, SGPU_GVF_PARAM_TARGET_RATIO,
			param[SGPU_GVF_PARAM_TARGET_RATIO]);

	sgpu_gvf_set_param(0, SGPU_GVF_PARAM_MAX_REST_RATIO,
			param[SGPU_GVF_PARAM_MAX_REST_RATIO]);

	sgpu_gvf_set_param(0, SGPU_GVF_PARAM_MIN_REST_RATIO,
			param[SGPU_GVF_PARAM_MIN_REST_RATIO]);

	return len;
}

static int sgpu_gvf_param_open(struct inode *inode, struct file *f)
{
	return single_open(f, sgpu_gvf_param_show, inode->i_private);
}

static const struct file_operations sgpu_debugfs_gvf_param_fops = {
	.owner	 = THIS_MODULE,
	.open	 = sgpu_gvf_param_open,
	.read	 = seq_read,
	.write   = sgpu_gvf_param_write,
	.llseek  = seq_lseek,
	.release = single_release
};

int sgpu_debugfs_gvf_init(struct amdgpu_device *adev)
{
	struct drm_minor *minor = adev_to_drm(adev)->render;
	struct dentry *root = minor->debugfs_root;
	struct sgpu_gvf *gvf = &adev->gvf;

	gvf->debugfs_params = debugfs_create_file("gvf_params",
					0644, root, adev,
					&sgpu_debugfs_gvf_param_fops);
	if (!gvf->debugfs_params) {
		dev_err(adev->dev, "unable to create gvf_params file\n");
		return -EIO;
	}

	gvf->debugfs_lock_freq = debugfs_create_file("gvf_lock_freq",
					0644, root, adev,
					&sgpu_debugfs_gvf_lock_freq_fops);
	if (!gvf->debugfs_lock_freq) {
		dev_err(adev->dev, "unable to create gvf_lock_freq file\n");
		goto error_lock_freq;
	}

	gvf->debugfs_monitor_window_ms = debugfs_create_file(
					"gvf_monitor_window_ms",
					0644, root, adev,
					&sgpu_debugfs_gvf_monitor_window_ms_fops);
	if (!gvf->debugfs_monitor_window_ms) {
		dev_err(adev->dev, "unable to create gvf_monitor_window_ms file\n");
		goto error_monitor_window_ms;
	}

	gvf->debugfs_start = debugfs_create_file("gvf_start",
					0644, root, adev,
					&sgpu_debugfs_gvf_start_fops);
	if (!gvf->debugfs_start) {
		dev_err(adev->dev, "unable to create gvf_start file\n");
		goto error_start;
	}

	return 0;

error_start:
	debugfs_remove(gvf->debugfs_monitor_window_ms);
error_monitor_window_ms:
	debugfs_remove(gvf->debugfs_lock_freq);
error_lock_freq:
	debugfs_remove(gvf->debugfs_params);

	return -EIO;
}
#else
int sgpu_debugfs_gvf_init(struct amdgpu_device *adev)
{
	return 0;
}
#endif
