/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "hts_devfs.h"
#include "hts_vh.h"
#include "hts_var.h"
#include "hts_logic.h"
#include "hts_backup.h"
#include "hts_command.h"
#include "hts_common.h"

#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/skbuff.h>

#define MASK_BUFFER_SIZE		(2)
#define MASK_GROUP_INDEX		(0)
#define MASK_KNOB_INDEX			(1)

#define PREDEFINED_BUFFER_SIZE		(4)
#define PREDEFINED_MODE_INDEX		(0)
#define PREDEFINED_MASK_INDEX		(1)
#define PREDEFINED_KNOB_INDEX		(2)
#define PREDEFINED_VALUE_INDEX		(3)

static int hts_devfs_predefined_set(struct hts_drvdata *drvdata, unsigned long arg)
{
	struct platform_device *pdev = drvdata->pdev;
	struct hts_devfs *devfs = &drvdata->devfs;
	struct hts_percpu *percpu = &drvdata->percpu;
	long predefined_buffer[PREDEFINED_BUFFER_SIZE];
	unsigned long *dataPtr = (unsigned long *)arg;

	if (copy_from_user(predefined_buffer, dataPtr, sizeof(predefined_buffer))) {
		dev_err(&pdev->dev, "Couldn't copy data from user for predefined value");
		return -EINVAL;
	}

	if (predefined_buffer[PREDEFINED_MODE_INDEX] < 0 ||
		MODE_COUNT <= predefined_buffer[PREDEFINED_MODE_INDEX])
		return -EINVAL;

	if (predefined_buffer[PREDEFINED_MASK_INDEX] < 0 ||
		devfs->mask_count <= predefined_buffer[PREDEFINED_MASK_INDEX])
		return -EINVAL;

	if (predefined_buffer[PREDEFINED_KNOB_INDEX] < 0 ||
		KNOB_COUNT <= predefined_buffer[PREDEFINED_KNOB_INDEX])
		return -EINVAL;

	percpu->predefined
		[predefined_buffer[PREDEFINED_MODE_INDEX]]
		[predefined_buffer[PREDEFINED_MASK_INDEX]]
		[predefined_buffer[PREDEFINED_KNOB_INDEX]] = predefined_buffer[PREDEFINED_VALUE_INDEX];

	return 0;
}

static void hts_devfs_predefined_clear(struct hts_percpu *percpu)
{
	memset(&percpu->predefined, 0, sizeof(percpu->predefined));
}

static void hts_devfs_percpu_clear(struct hts_percpu *percpu)
{
	memset(&percpu->backup, 0, sizeof(percpu->backup));
	memset(&percpu->predefined, 0, sizeof(percpu->predefined));
	memset(&percpu->transition_count, 0, sizeof(percpu->transition_count));
	memset(&percpu->written_count, 0, sizeof(percpu->written_count));
}

static inline void hts_devfs_data_initialize(struct hts_drvdata *drvdata)
{
	struct hts_devfs *devfs = &drvdata->devfs;

	devfs->enabled = 0;
	devfs->eval_tick_ms = DEFAULT_EVAL_WORKQUEUE_TICK_MS;
	devfs->wakeup_waiter = 0;

	cpumask_clear(&devfs->log_mask);
	cpumask_clear(&devfs->ref_mask);
	cpumask_clear(&devfs->def_mask);

	devfs->core_active_thre = 0;
	devfs->total_active_thre = 0;

	atomic_set(&devfs->predefined_mode, -1);

	memset(&devfs->available_cpu, 0, sizeof(devfs->available_cpu));
	memset(&devfs->mask_cpu, 0, sizeof(devfs->mask_cpu));
	devfs->mask_count = 0;

	memset(&devfs->target_cgroup, 0, sizeof(devfs->target_cgroup));
	devfs->cgroup_count = 0;

	hts_devfs_percpu_clear(&drvdata->percpu);

	hts_var_clear_mask(devfs);
	hts_var_clear_cgroup(devfs);
}

static inline void hts_devfs_set_log_mask(struct hts_devfs *devfs, unsigned long mask)
{
	int bit;

	if (devfs == NULL)
		return;

	cpumask_clear(&devfs->log_mask);
	for_each_set_bit(bit, &mask, CONFIG_VENDOR_NR_CPUS)
		cpumask_set_cpu(bit, &devfs->log_mask);
}

static inline void hts_devfs_set_ref_mask(struct hts_devfs *devfs, unsigned long mask)
{
	int bit;

	if (devfs == NULL)
		return;

	cpumask_clear(&devfs->ref_mask);
	for_each_set_bit(bit, &mask, CONFIG_VENDOR_NR_CPUS)
		cpumask_set_cpu(bit, &devfs->ref_mask);
}

static inline void hts_devfs_set_def_mask(struct hts_devfs *devfs, unsigned long mask)
{
	int bit;

	if (devfs == NULL)
		return;

	cpumask_clear(&devfs->def_mask);
	for_each_set_bit(bit, &mask, CONFIG_VENDOR_NR_CPUS)
		cpumask_set_cpu(bit, &devfs->def_mask);
}

static inline int hts_devfs_enable_mask(struct hts_devfs *devfs, unsigned long arg)
{
	int buf[MASK_BUFFER_SIZE];

        if (copy_from_user(buf, (void *)arg, sizeof(int) * MASK_BUFFER_SIZE))
                return -EINVAL;

	hts_var_enable_mask(devfs, buf[MASK_GROUP_INDEX], buf[MASK_KNOB_INDEX]);

	return 0;
}

static inline void hts_devfs_set_core_active_thre(struct hts_devfs *devfs, unsigned long threshold)
{
	if (devfs == NULL)
		return;

	devfs->core_active_thre = threshold;
}

static inline void hts_devfs_set_total_active_thre(struct hts_devfs *devfs, unsigned long threshold)
{
	if (devfs == NULL)
		return;

	devfs->total_active_thre = threshold;
}

static inline void hts_devfs_signal_workqueue(struct hts_devfs *devfs)
{
	if (!devfs->enabled)
		return;

	queue_delayed_work(devfs->wq, &devfs->work, msecs_to_jiffies(devfs->eval_tick_ms));
}

static inline void hts_devfs_modify_eval_tick(struct hts_devfs *devfs, unsigned long ms)
{
	devfs->eval_tick_ms = ms;
}

static inline void hts_devfs_enable_eval_tick(struct hts_drvdata *drvdata)
{
	drvdata->devfs.enabled = 1;
	drvdata->etc.enabled_count++;

	hts_devfs_signal_workqueue(&drvdata->devfs);
}

static inline void hts_devfs_disable_eval_tick(struct hts_devfs *devfs)
{
	devfs->enabled = 0;

	hts_pmu_unregister_event_all();
}

static inline void hts_devfs_fetch_cpu_util(struct hts_drvdata *drvdata)
{
	int cpu;
	struct hts_percpu *percpu = &drvdata->percpu;

	for_each_online_cpu(cpu) {
		kcpustat_cpu_fetch(&percpu->core_util[cpu], cpu);
	}
}

static inline int hts_devfs_start_tick(struct hts_drvdata *drvdata)
{
	int ret;

	if (drvdata->devfs.enabled)
		return 0;

	hts_devfs_fetch_cpu_util(drvdata);

	ret = hts_backup_default_value(drvdata);
	if (ret)
		return ret;

	ret = hts_pmu_register_event_all();
	if (ret)
		return ret;

	ret = hts_vh_register_tick(drvdata);
	if (ret)
		return ret;

	hts_devfs_enable_eval_tick(drvdata);

	return 0;
}

static inline int hts_devfs_stop_tick(struct hts_drvdata *drvdata)
{
	int ret;

	if (!drvdata->devfs.enabled)
		return 0;

	ret = hts_vh_unregister_tick(drvdata);
	if (ret)
		return ret;

	hts_devfs_disable_eval_tick(&drvdata->devfs);
	hts_backup_reset_value(drvdata);

	hts_devfs_data_initialize(drvdata);

	return 0;
}

static void hts_devfs_update_tick_wrapper(void *data)
{
	struct hts_drvdata *drvdata = (struct hts_drvdata *)data;

	hts_logic_read_task_data(drvdata, current);
}

static inline void hts_devfs_update_tick(struct hts_drvdata *drvdata)
{
	int cpu, ret;
	struct platform_device *pdev = drvdata->pdev;

	for_each_online_cpu(cpu) {
		ret = smp_call_function_single(cpu, hts_devfs_update_tick_wrapper, drvdata, 1);
		if (ret)
			dev_err(&pdev->dev, "IPI update tick couldn't be running CPU%d - %d", cpu, ret);
	}
}

static inline int hts_devfs_add_cgroup(struct hts_drvdata *drvdata, char *cgroup_name)
{
	char buf[CGROUP_BUFFER_LENGTH];

	if (copy_from_user(buf, cgroup_name, CGROUP_BUFFER_LENGTH))
		return -EINVAL;

	buf[CGROUP_BUFFER_LENGTH - 1] = '\0';
	hts_var_add_cgroup(drvdata, buf);

	return 0;
}

static int hts_devfs_configure_event(struct hts_drvdata *drvdata, unsigned long arg)
{
	struct platform_device *pdev = drvdata->pdev;
	int cpu, dataCount, signature, eventData[MAXIMUM_EVENT_COUNT + 1];
	int *dataPtr = (int *)arg;

	if (copy_from_user(&signature, dataPtr, sizeof(int)) ||
		copy_from_user(&cpu, dataPtr + 1, sizeof(int)) ||
		copy_from_user(&dataCount, dataPtr + 2, sizeof(int))) {
		dev_err(&pdev->dev, "Couldn't copy data from user for configuring event");
		return -EINVAL;
	}

	if (signature != SIGNATURE_ARGUMENT_START) {
		dev_err(&pdev->dev, "Configuring event starting data is invalid!");
		return -EINVAL;
	}

	if (dataCount < 0)
		return -EINVAL;

	if (dataCount > MAXIMUM_EVENT_COUNT)
		dataCount = MAXIMUM_EVENT_COUNT;

	if (copy_from_user(eventData, dataPtr + 3, sizeof(int) * (dataCount + 1)))
		return -EINVAL;

	if (eventData[dataCount] != SIGNATURE_ARGUMENT_END) {
		dev_err(&pdev->dev, "Configuring event ending data is invalid!");
		return -EINVAL;
	}

	if (cpu < 0 ||
		CONFIG_VENDOR_NR_CPUS <= cpu) {
		dev_err(&pdev->dev, "Core index is invalid CPU%d", cpu);
		return -EINVAL;
	}

	if (hts_pmu_configure_events(cpu, eventData, dataCount)) {
		dev_err(&pdev->dev, "Couldn't set event");
		return -EINVAL;
	}

	return 0;
}

static inline int hts_devfs_calculate_activity(struct hts_devfs *devfs)
{
	int cpu, cpu_count = 0, ret = 0;
	struct kernel_cpustat *prev_core_util, current_core_util;
	struct hts_drvdata *drvdata = container_of(devfs,
					struct hts_drvdata, devfs);
	struct hts_percpu *percpu = &drvdata->percpu;
	unsigned long active_time, idle_time, core_active_ratio, total_active_ratio = 0;

	if (devfs->core_active_thre == 0 &&
		devfs->total_active_thre == 0)
		return 1;

	for_each_online_cpu(cpu) {
		prev_core_util = &percpu->core_util[cpu];
		kcpustat_cpu_fetch(&current_core_util, cpu);

		active_time = (current_core_util.cpustat[CPUTIME_USER] - prev_core_util->cpustat[CPUTIME_USER]) +
			(current_core_util.cpustat[CPUTIME_NICE] - prev_core_util->cpustat[CPUTIME_NICE]) +
			(current_core_util.cpustat[CPUTIME_SYSTEM] - prev_core_util->cpustat[CPUTIME_SYSTEM]) +
			(current_core_util.cpustat[CPUTIME_SOFTIRQ] - prev_core_util->cpustat[CPUTIME_SOFTIRQ]) +
			(current_core_util.cpustat[CPUTIME_IRQ] - prev_core_util->cpustat[CPUTIME_IRQ]);

		idle_time = (current_core_util.cpustat[CPUTIME_IDLE] - prev_core_util->cpustat[CPUTIME_IDLE]) +
			(current_core_util.cpustat[CPUTIME_IOWAIT] - prev_core_util->cpustat[CPUTIME_IOWAIT]);

		core_active_ratio = active_time * 100 / (active_time + idle_time);
		if (core_active_ratio >= devfs->core_active_thre) {
			ret = 1;
			break;
		}

		total_active_ratio += core_active_ratio;
		cpu_count++;

		prev_core_util->cpustat[CPUTIME_USER]		= current_core_util.cpustat[CPUTIME_USER];
		prev_core_util->cpustat[CPUTIME_NICE]		= current_core_util.cpustat[CPUTIME_NICE];
		prev_core_util->cpustat[CPUTIME_SYSTEM]		= current_core_util.cpustat[CPUTIME_SYSTEM];
		prev_core_util->cpustat[CPUTIME_SOFTIRQ]	= current_core_util.cpustat[CPUTIME_SOFTIRQ];
		prev_core_util->cpustat[CPUTIME_IRQ]		= current_core_util.cpustat[CPUTIME_IRQ];
		prev_core_util->cpustat[CPUTIME_IDLE]		= current_core_util.cpustat[CPUTIME_IDLE];
		prev_core_util->cpustat[CPUTIME_IOWAIT]		= current_core_util.cpustat[CPUTIME_IOWAIT];
	}

	if (cpu_count != 0 &&
		(total_active_ratio / cpu_count >= devfs->total_active_thre))
		ret = 1;

	return ret;
}

static void hts_devfs_workqueue_monitor(struct work_struct *work)
{
	struct hts_devfs *devfs = container_of(work,
					struct hts_devfs, work.work);

	if (devfs->ems_mode &&
		hts_devfs_calculate_activity(devfs)) {
		devfs->wakeup_waiter = 1;
		wake_up_interruptible(&devfs->wait_queue);
	}

	if (devfs->enabled)
		hts_devfs_signal_workqueue(devfs);
}

static int hts_devfs_queue_initialize(struct hts_devfs *devfs)
{
	if (devfs == NULL) {
		pr_err("HTS : Couldn't get devfs for queue");
		return -EINVAL;
	}

	devfs->wq = create_freezable_workqueue("hts_wq");
	if (devfs->wq == NULL) {
		pr_err("HTS : Couldn't create workqueue");
		return -ENOMEM;
	}

	INIT_DEFERRABLE_WORK(&devfs->work, hts_devfs_workqueue_monitor);
	devfs->eval_tick_ms = DEFAULT_EVAL_WORKQUEUE_TICK_MS;

	devfs->wakeup_waiter = 0;
	init_waitqueue_head(&devfs->wait_queue);

	return 0;
}

static int hts_fops_open(struct inode *inode, struct file *filp)
{
	struct hts_fops_private_data *data = kzalloc(sizeof(struct hts_fops_private_data), GFP_KERNEL);

	if (data == NULL)
		return -ENOMEM;

	filp->private_data = data;

	return 0;
}

static ssize_t hts_fops_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct hts_fops_private_data *data = filp->private_data;
	struct hts_pmu_handle *pmu_handle = NULL;
	int core;

	if (buf == NULL ||
		data == NULL ||
		count < sizeof(int))
		return -EINVAL;

	if (copy_from_user(&core, buf, sizeof(int)))
		return -EFAULT;

	if (core < 0 ||
		core >= CONFIG_VENDOR_NR_CPUS)
		return -EINVAL;

	pmu_handle = hts_pmu_get_handle(core);
	if (pmu_handle == NULL)
		return -EINVAL;

	data->pmu_handle = pmu_handle;

	return sizeof(int);
}

static ssize_t hts_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static __poll_t hts_fops_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct hts_devfs *devfs = container_of(filp->f_op, struct hts_devfs, fops);
	__poll_t mask = 0;

	poll_wait(filp, &devfs->wait_queue, wait);

	if (devfs->wakeup_waiter) {
		mask = EPOLLIN | EPOLLRDNORM;
		devfs->wakeup_waiter = 0;
	}

	return mask;
}

static int hts_mmap_pmu(struct vm_area_struct *vma,
			struct hts_pmu_handle *pmu_handle,
			struct hts_mmap *mmap_buffer,
			unsigned long request_size)
{
	int ret = 0;
	struct page *buffer_page = __dev_alloc_pages(GFP_KERNEL | __GFP_ZERO,
					BUFFER_PAGE_EXP);

	if (buffer_page == NULL) {
		ret = -ENOMEM;
		goto err_alloc_page;
	}

	if (hts_atomic_cmpxhg(&mmap_buffer->ref_count)) {
		ret = -EINVAL;
		goto err_already_mapped;
	}
	
	if (remap_pfn_range(vma,
			vma->vm_start,
			page_to_pfn(buffer_page),
			request_size,
			vma->vm_page_prot)) {
		ret = -EAGAIN;
		goto err_mmap;
	}

	hts_atomic_set(&pmu_handle->preemption);

	mmap_buffer->buffer_page = buffer_page;
	mmap_buffer->buffer_event = page_to_virt(buffer_page);
	mmap_buffer->buffer_size = BUFFER_PAGE_SIZE;

	hts_atomic_clear(&pmu_handle->preemption);

	return 0;

err_mmap:
	mmap_buffer->buffer_event = NULL;
	mmap_buffer->buffer_page = NULL;
	mmap_buffer->buffer_size = 0;

	hts_atomic_clear(&mmap_buffer->ref_count);
err_already_mapped:
	__free_pages(buffer_page, BUFFER_PAGE_EXP);
err_alloc_page:

	return ret;
}

static int hts_mmap_freq(struct vm_area_struct *vma,
			struct hts_notifier *notifier,
			struct hts_mmap *mmap_buffer,
			unsigned long request_size)
{
	int ret = 0;
	unsigned long flags;
	struct page *buffer_page = NULL;

#if !defined(CONFIG_HTS_EXT)
	return -EINVAL;
#endif

	buffer_page = __dev_alloc_pages(GFP_KERNEL | __GFP_ZERO,
					BUFFER_PAGE_EXP);

	if (buffer_page == NULL) {
		ret = -ENOMEM;
		goto err_alloc_page;
	}

	if (hts_atomic_cmpxhg(&mmap_buffer->ref_count)) {
		ret = -EINVAL;
		goto err_already_mapped;
	}

	if (remap_pfn_range(vma,
			vma->vm_start,
			page_to_pfn(buffer_page),
			request_size,
			vma->vm_page_prot)) {
		ret = -EAGAIN;
		goto err_mmap;
	}

	spin_lock_irqsave(&notifier->lock, flags);

	mmap_buffer->buffer_page = buffer_page;
	mmap_buffer->buffer_event = page_to_virt(buffer_page);
	mmap_buffer->buffer_size = BUFFER_PAGE_SIZE;

	spin_unlock_irqrestore(&notifier->lock, flags);

	return 0;

err_mmap:
	mmap_buffer->buffer_event = NULL;
	mmap_buffer->buffer_page = NULL;
	mmap_buffer->buffer_size = 0;

	hts_atomic_clear(&mmap_buffer->ref_count);
err_already_mapped:
	__free_pages(buffer_page, BUFFER_PAGE_EXP);
err_alloc_page:

	return ret;
}

static int hts_fops_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	struct hts_drvdata *drvdata = container_of(filp->f_op, struct hts_drvdata, devfs.fops);
	struct hts_fops_private_data *data = filp->private_data;
	unsigned long request_size = vma->vm_end - vma->vm_start;
	struct hts_pmu_handle *pmu_handle;
	struct hts_mmap *mmap_buffer = NULL;

	if (data == NULL ||
		request_size != BUFFER_PAGE_SIZE)
		return -EINVAL;

	if (data->pmu_handle == NULL) {
		mmap_buffer = &drvdata->notifier.mmap;
		ret = hts_mmap_freq(vma, &drvdata->notifier, mmap_buffer, request_size);
	} else {
		pmu_handle = data->pmu_handle;
		mmap_buffer = &pmu_handle->mmap;

		ret = hts_mmap_pmu(vma, pmu_handle, mmap_buffer, request_size);
	}

	return ret;
}

static void hts_release_pmu(struct hts_pmu_handle *pmu_handle,
				struct hts_mmap *mmap_buffer)
{
	struct page *buffer_page = mmap_buffer->buffer_page;

	if (atomic_dec_return(&mmap_buffer->ref_count) > 0)
		return;

	hts_atomic_set(&pmu_handle->preemption);

	mmap_buffer->buffer_event = NULL;
	mmap_buffer->buffer_page = NULL;
	mmap_buffer->buffer_size = 0;

	hts_atomic_clear(&pmu_handle->preemption);

	if (buffer_page)
		__free_pages(buffer_page, BUFFER_PAGE_EXP);
}

static void hts_release_freq(struct hts_mmap *mmap_buffer,
				struct hts_notifier *notifier)
{
	struct page *buffer_page = mmap_buffer->buffer_page;
	unsigned long flags;

	if (atomic_dec_return(&mmap_buffer->ref_count) > 0)
		return;

	spin_lock_irqsave(&notifier->lock, flags);

	mmap_buffer->buffer_event = NULL;
	mmap_buffer->buffer_page = NULL;
	mmap_buffer->buffer_size = 0;

	spin_unlock_irqrestore(&notifier->lock, flags);

	if (buffer_page)
		__free_pages(buffer_page, BUFFER_PAGE_EXP);
}

static int hts_fops_release(struct inode *node, struct file *filp)
{
	struct hts_drvdata *drvdata = container_of(filp->f_op, struct hts_drvdata, devfs.fops);
	struct hts_fops_private_data *data = filp->private_data;
	struct hts_pmu_handle *pmu_handle;
	struct hts_mmap *pmu_mmap;

	if (data == NULL)
		return -EINVAL;

	if (data->pmu_handle == NULL) {
		pmu_mmap = &drvdata->notifier.mmap;
		hts_release_freq(pmu_mmap, &drvdata->notifier);
	} else {
		pmu_handle = data->pmu_handle;
		pmu_mmap = &pmu_handle->mmap;

		hts_release_pmu(pmu_handle, pmu_mmap);
	}

	kfree(data);
	filp->private_data = NULL;

	return 0;
}

static long hts_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct hts_drvdata *drvdata = container_of(filp->f_op, struct hts_drvdata, devfs.fops);
	struct hts_devfs *devfs = &drvdata->devfs;

	mutex_lock(&devfs->ioctl_lock);

	switch (cmd) {
		case IOCTL_MODIFY_TICK:
			hts_devfs_modify_eval_tick(devfs, arg);
			break;
		case IOCTL_START_TICK:
			hts_devfs_start_tick(drvdata);
			break;
		case IOCTL_STOP_TICK:
			hts_devfs_stop_tick(drvdata);
			break;
		case IOCTL_UPDATE_TICK:
			hts_devfs_update_tick(drvdata);
			break;
		case IOCTL_CGROUP_ADD:
			ret = hts_devfs_add_cgroup(drvdata, (char *)arg);
			break;
		case IOCTL_CGROUP_CLEAR:
			hts_var_clear_cgroup(devfs);
			break;
		case IOCTL_CONFIGURE_EVENT:
			ret = hts_devfs_configure_event(drvdata, arg);
			break;
		case IOCTL_CLEAR_EVENT:
			hts_pmu_reset_cpu(arg);
			break;
		case IOCTL_SET_LOG_MASK:
			hts_devfs_set_log_mask(devfs, arg);
			break;
		case IOCTL_SET_REF_MASK:
			hts_devfs_set_ref_mask(devfs, arg);
			break;
		case IOCTL_SET_DEF_MASK:
			hts_devfs_set_def_mask(devfs, arg);
			break;
		case IOCTL_MASK_ADD:
			hts_var_add_mask(devfs, arg);
			break;
		case IOCTL_MASK_CLEAR:
			hts_var_clear_mask(devfs);
			break;
		case IOCTL_MASK_ENABLE:
			ret = hts_devfs_enable_mask(devfs, arg);
			break;
		case IOCTL_PREDEFINED_SET:
			ret = hts_devfs_predefined_set(drvdata, arg);
			break;
		case IOCTL_PREDEFINED_CLEAR:
			hts_devfs_predefined_clear(&drvdata->percpu);
			break;
		case IOCTL_SET_CORE_THRE:
			hts_devfs_set_core_active_thre(devfs, arg);
			break;
		case IOCTL_SET_TOTAL_THRE:
			hts_devfs_set_total_active_thre(devfs, arg);
			break;
		default:
			break;
	}

	mutex_unlock(&devfs->ioctl_lock);

	return ret;
}

static int hts_devfs_fops_initialize(struct hts_devfs *devfs)
{
	int ret = 0;

	if (devfs == NULL) {
		pr_err("HTS : Couldn't get devfs for fops");
		return -EINVAL;
	}

	devfs->miscdev.minor		= MISC_DYNAMIC_MINOR;
	devfs->miscdev.name		= "hts";
	devfs->miscdev.fops		= &devfs->fops;

	devfs->fops.owner		= THIS_MODULE;
	devfs->fops.llseek		= noop_llseek;
	devfs->fops.open		= hts_fops_open;
	devfs->fops.write		= hts_fops_write;
	devfs->fops.read		= hts_fops_read;
	devfs->fops.poll		= hts_fops_poll;
	devfs->fops.mmap		= hts_fops_mmap;
	devfs->fops.release		= hts_fops_release;
	devfs->fops.unlocked_ioctl	= hts_fops_ioctl;
	devfs->fops.compat_ioctl	= hts_fops_ioctl;

	ret = misc_register(&devfs->miscdev);
	if (ret) {
		pr_err("HTS : Couldn't register misc dev");
		return ret;
	}

	return ret;
}

static void hts_devfs_fops_uninitialize(struct hts_devfs *devfs)
{
	misc_deregister(&devfs->miscdev);
}

void hts_devfs_control_tick(struct hts_drvdata *drvdata, int enable)
{
	if (enable)
		hts_devfs_start_tick(drvdata);
	else
		hts_devfs_stop_tick(drvdata);
}

int hts_devfs_initialize(struct hts_drvdata *drvdata)
{
	int ret = 0;
	struct hts_devfs *devfs = &drvdata->devfs;

	mutex_init(&devfs->ioctl_lock);

	hts_devfs_data_initialize(drvdata);

	ret = hts_devfs_fops_initialize(devfs);
	if (ret)
		goto err_fops_init;

	ret = hts_devfs_queue_initialize(devfs);
	if (ret)
		goto err_queue_init;

	return 0;

err_queue_init:
	hts_devfs_fops_uninitialize(devfs);
err_fops_init:

	return ret;
}
