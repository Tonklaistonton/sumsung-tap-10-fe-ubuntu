/*
 * main.c - Primary Security Processor driver for the Exynos
 *
 * Copyright (C) 2019 Samsung Electronics
 * Keunyoung Park <keun0.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/smc.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-pd.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/panic_notifier.h>
#include <linux/delay.h>

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#include <soc/samsung/exynos/memlogger.h>
#endif
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/exynos/debug-snapshot.h>
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
#include <soc/samsung/exynos/exynos-itmon.h>
#endif

#include <soc/samsung/exynos/psp/psp_mailbox_common.h>
#include <soc/samsung/exynos/psp/psp_mailbox_ree.h>
#include <soc/samsung/exynos/psp/psp_mailbox_sfr.h>

#define PSP_RET_OK		0
#define PSP_RET_FAIL		-1
#define PSP_RET_BUSY		0x20010	/* defined at LDFW */
#define PSP_MAX_USER_CNT	100
#define PSP_MAX_USER_PATH_LEN	128
#define PSP_K250_STABLE_TIME_MS    20

/* smc to call ldfw functions */
#define SMC_CMD_PSP		(0xC2001040)
#define PSP_CMD_SELF_TEST	(0x10)

/* ioctl command for TAC */
#define PSP_CMD_SYNC_MAGIC		'c'
#define PSP_CMD_SYNC_INIT		_IO(PSP_CMD_SYNC_MAGIC, 1)
#define PSP_CMD_SYNC_EXIT		_IOWR(PSP_CMD_SYNC_MAGIC, 2, uint64_t)
#define PSP_CMD_SYNC_TEST		_IOWR(PSP_CMD_SYNC_MAGIC, 3, uint64_t)
#define PSP_CMD_SYNC_DEBUG		_IOWR(PSP_CMD_SYNC_MAGIC, 4, uint64_t)

void __iomem *pmu_va_base;
extern void __iomem *mb_va_base;

spinlock_t psp_lock;
struct mutex psp_sync_lock;
extern struct exynos_chipid_info exynos_soc_info;
atomic_t psp_sleep;

EXPORT_SYMBOL(psp_sleep);

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
struct memlogs {
	struct memlog *mlog;
	struct memlog_obj *mlog_log;
	struct memlog_obj *mlog_log_file;
};
#endif

struct psp_device {
	struct device *dev;
	struct miscdevice misc_device;
	struct regulator *secure_nvm_ldo;
	unsigned long snvm_init_time;
	/* dbg */
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	struct memlogs mlog;
#endif
	struct timer_list dbg_timer;
	struct work_struct dbg_work;
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	struct notifier_block panic_nb;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
	struct notifier_block itmon_nb;
#endif
};

struct psp_user {
	char path[PSP_MAX_USER_PATH_LEN];
	unsigned long init_count;
	unsigned long init_time;
	unsigned long exit_count;
	unsigned long exit_time;
	unsigned long init_fail_count;
	unsigned long init_fail_time;
	unsigned long exit_fail_count;
	unsigned long exit_fail_time;
	struct psp_user *next;
};

struct psp_user *head = NULL;
unsigned int mb_irq;
struct psp_device *pspdev_bak = NULL;

static int exynos_psp_get_path(struct device *dev, struct task_struct *task, char *task_path)
{
	int ret = PSP_RET_OK;
	char *buf;
	char *path;
	struct mm_struct *mm;
	struct file *exe_file = NULL;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		psp_err(dev, "%s: fail to __get_free_page.\n", __func__);
		return -ENOMEM;
	}

	mm = get_task_mm(task);
	if (mm)
		exe_file = mm->exe_file;

	if (!exe_file) {
		ret = -ENOENT;
		psp_info(dev, "%s: check to get_task_exe_file.\n", __func__);
		goto end;
	}

	path = d_path(&exe_file->f_path, buf, PAGE_SIZE);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		psp_err(dev, "%s: fail to d_path. ret = 0x%x\n", __func__, ret);
		goto end;
	}

	memset(task_path, 0, PSP_MAX_USER_PATH_LEN);
	strncpy(task_path, path, PSP_MAX_USER_PATH_LEN - 1);
end:
	free_page((unsigned long)buf);

	return ret;
}

static int exynos_cm_smc(uint64_t *arg0, uint64_t *arg1,
			 uint64_t *arg2, uint64_t *arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(*arg0, *arg1, *arg2, *arg3, 0, 0, 0, 0, &res);

	*arg0 = res.a0;
	*arg1 = res.a1;
	*arg2 = res.a2;
	*arg3 = res.a3;

	return *arg0;
}

static int exynos_psp_self_test(struct device *dev, uint64_t test_mode)
{
	int ret = PSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	psp_info(dev, "call psp function: %d\n", (int)test_mode);

	reg0 = SMC_CMD_PSP;
	reg1 = PSP_CMD_SELF_TEST;
	reg2 = test_mode;
	reg3 = 0;

	spin_lock_irqsave(&psp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&psp_lock, flag);

	psp_info(dev, "return from ldfw: 0x%x\n", ret);

	return ret;
}

void psp_sync_mutex(uint32_t lock)
{
	if (lock == ON)
		mutex_lock(&psp_sync_lock);
	else if (lock == OFF)
		mutex_unlock(&psp_sync_lock);
}

static uint32_t psp_cmd_sync(struct psp_device *pspdev, uint64_t cmd, void __user *arg)
{
	uint32_t ret = PSP_RET_OK;
	uint64_t test_mode;

	psp_sync_mutex(ON);

	switch (cmd) {
	case PSP_CMD_SYNC_TEST:
		ret = get_user(test_mode, (uint64_t __user *)arg);
		if (unlikely(ret)) {
			psp_err(pspdev->dev, "%s: fail to get_user. ret = 0x%x\n", __func__, ret);
			break;
		}
		ret = exynos_psp_self_test(pspdev->dev, test_mode);
		break;
	default:
		psp_err(pspdev->dev, "%s: invalid ioctl cmd: 0x%llx\n", __func__, cmd);
		ret = -EPERM;
		break;
	}

	psp_sync_mutex(OFF);

	return ret;
}

static long psp_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
{
	int ret = PSP_RET_OK;
	char path[PSP_MAX_USER_PATH_LEN];

	struct psp_device *pspdev = filp->private_data;
	void __user *arg = (void __user *)__arg;

	ret = exynos_psp_get_path(pspdev->dev, current, path);
	if (ret) {
		psp_err(pspdev->dev, "%s: fail to get user path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	psp_info(pspdev->dev, "requested by %s\n", path);

	return psp_cmd_sync(pspdev, cmd, arg);
}

static int psp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct psp_device *pspdev = container_of(misc,
			struct psp_device, misc_device);

	file->private_data = pspdev;

	psp_info(pspdev->dev, "driver open is done\n");

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define pr_memlog(pspdev, fmt, ...)                             \
do {                                                            \
	if (pspdev->mlog.mlog_log)                    \
		memlog_write_printf(pspdev->mlog.mlog_log,               \
	                        MEMLOG_LEVEL_EMERG,             \
	                        fmt, ##__VA_ARGS__);            \
	else                                                    \
		pr_err(fmt, ##__VA_ARGS__);                     \
} while (0)
#else
#define pr_memlog psp_info
#endif

void exynos_psp_faulthandler(struct work_struct *work)
{
	psp_info(0, "%s called\n", __func__);

	mailbox_fault_and_callback();
	msleep(1000);

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	dump_stack();
	if (dbg_snapshot_expire_watchdog())
		pr_err("WDT reset fails\n");
#endif
}

static irqreturn_t exynos_psp_mb_irq_handler(int irq, void *data)
{
	uint32_t ret;
	unsigned int status = read_sfr(mb_va_base + PSP_MB_INTSR0_OFFSET);
	struct psp_device *pspdev = data;

#if defined(CONFIG_CUSTOS_INTERRUPT)
	if (status & PSP_MB_INTGR0_ON) {
		ret = mailbox_receive_and_callback();
		if (ret) {
			psp_err(0, "%s [ret: 0x%X]\n", __func__, ret);
		}
		write_sfr(mb_va_base + PSP_MB_INTCR0_OFFSET, PSP_MB_INTGR0_ON);
	}
#endif

	if (status & PSP_MB_INTGR0_LOG_ON) {
		ret = mailbox_receive_flush_log();
		if (ret)
			psp_err(0, "%s [ret: 0x%X]\n", __func__, ret);
		write_sfr(mb_va_base + PSP_MB_INTCR0_OFFSET, PSP_MB_INTGR0_LOG_ON);
	}

	if (status & PSP_MB_INTGR0_DBG_ON) {
		schedule_work(&pspdev->dbg_work);
		write_sfr(mb_va_base + PSP_MB_INTCR0_OFFSET, PSP_MB_INTGR0_DBG_ON);
	}

	return IRQ_HANDLED;
}

static void psp_dbg_timer_callback(struct timer_list *timer)
{
	psp_info(0, "%s called\n", __func__);
}

static int exynos_psp_memlog_init(struct psp_device *pspdev)
{
#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
	int ret;

	ret = memlog_register("psp", pspdev->dev, &pspdev->mlog.mlog);
	if (ret) {
		psp_err(pspdev->dev, "%s: psp memlog registration fail ret:%d\n", __func__, ret);
		return ret;
	}

	pspdev->mlog.mlog_log_file =
			memlog_alloc_file(pspdev->mlog.mlog, "log-fil", SZ_4K, SZ_16K, 200, 4);
	dev_info(pspdev->dev, "psp memlog log file alloc: %s\n",  pspdev->mlog.mlog_log_file ? "pass" : "fail");

	if (pspdev->mlog.mlog_log_file) {
		memlog_obj_set_sysfs_mode(pspdev->mlog.mlog_log_file, true);
		/* error handling */
		pspdev->mlog.mlog_log =
		    memlog_alloc_printf(pspdev->mlog.mlog, SZ_4K, pspdev->mlog.mlog_log_file, "log-mem", 0);
		dev_info(pspdev->dev, "psp memlog log print alloc: %s\n",  pspdev->mlog.mlog_log ? "pass" : "fail");
	}
	dev_info(pspdev->dev, "psp memlog log print %s\n", pspdev->mlog.mlog_log ? "pass" : "fail");

	return ret;
#else
	return 0;
#endif
}

static const struct file_operations psp_fops = {
	.owner		= THIS_MODULE,
	.open		= psp_open,
	.unlocked_ioctl	= psp_ioctl,
	.compat_ioctl	= psp_ioctl,
};

static void exynos_psp_notify_dump(void)
{
	int cnt = 0;
	pr_info("%s: notify it to custos", __func__);
	write_sfr(mb_va_base + PSP_MB_INTGR1_OFFSET, IWC_DEBUG);

	do {
		if (read_sfr(mb_va_base + PSP_MB_INTSR1_OFFSET) == 0) {
			break;
		}
		udelay(100);
	} while (cnt++ < 1000);
	udelay(100);
	pr_info("%s: notify done (cnt: %d: sr:%x)", __func__, cnt, read_sfr(mb_va_base + PSP_MB_INTSR1_OFFSET));
}

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
static int exynos_psp_panic_handler(struct notifier_block *nb,
				unsigned long action, void *data)
{
	pr_info("%s: notify it to custos", __func__);
	exynos_psp_notify_dump();
	return NOTIFY_OK;
}
#endif

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
static int exynos_psp_itmon_handler(struct notifier_block *nb,
				unsigned long action, void *data)
{
	pr_info("%s: notify it to custos", __func__);
	exynos_psp_notify_dump();
	return NOTIFY_OK;
}
#endif

static int exynos_psp_probe(struct platform_device *pdev)
{
	int ret = PSP_RET_OK;
	struct psp_device *pspdev = NULL;
	struct irq_data *stmb_irqd = NULL;
	irq_hw_number_t hwirq = 0;

	pspdev = kzalloc(sizeof(struct psp_device), GFP_KERNEL);
	if (!pspdev) {
		psp_err(&pdev->dev, "%s: fail to kzalloc.\n", __func__);
		return -ENOMEM;
	}
	pspdev_bak = pspdev;

	platform_set_drvdata(pdev, pspdev);
	pspdev->dev = &pdev->dev;

	spin_lock_init(&psp_lock);
	mutex_init(&psp_sync_lock);
	atomic_set(&psp_sleep, 0);

	/* set misc driver */
	memset((void *)&pspdev->misc_device, 0, sizeof(struct miscdevice));
	pspdev->misc_device.minor = MISC_DYNAMIC_MINOR;
	pspdev->misc_device.name = "psp";
	pspdev->misc_device.fops = &psp_fops;
	ret = misc_register(&pspdev->misc_device);
	if (ret) {
		psp_err(pspdev->dev, "%s: fail to misc_register. ret = %d\n", __func__, ret);
		ret = -ENOMEM;
		goto err;
	}

	ret = device_init_wakeup(pspdev->dev, true);
	if (ret) {
		psp_err(pspdev->dev, "%s: fail to init wakeup. ret = %d\n", __func__, ret);
		goto err;
	}

	mb_irq = irq_of_parse_and_map(pspdev->dev->of_node, 0);
	if (!mb_irq) {
		psp_err(pspdev->dev, "Fail to get irq from dt. mb_irq = 0x%X\n", mb_irq);
		return -EINVAL;
	}

	stmb_irqd = irq_get_irq_data(mb_irq);
	if (!stmb_irqd) {
		psp_err(pspdev->dev, "Fail to get irq_data\n");
		return -EINVAL;
	}

	hwirq = irqd_to_hwirq(stmb_irqd);

	ret = devm_request_irq(pspdev->dev, mb_irq,
			exynos_psp_mb_irq_handler,
			IRQ_TYPE_LEVEL_HIGH, pdev->name, pspdev);

	ret = enable_irq_wake(mb_irq);
	if (ret) {
		psp_err(pspdev->dev, "Fail to enable irq wake. ret = 0x%X\n", ret);
		return -EINVAL;
	}

	ret = exynos_psp_mailbox_map_sfr();
	if (ret) {
		dev_err(pspdev->dev, "Fail to ioremap. ret = 0x%X\n", ret);
		return -EINVAL;
	}

	/* debug features */
	timer_setup(&pspdev->dbg_timer, psp_dbg_timer_callback, 0);
	ret = exynos_psp_memlog_init(pspdev);
	if (ret) {
		dev_err(pspdev->dev, "Fail to init memlog. ret = 0x%X\n", ret);
		return -EINVAL;
	}

	INIT_WORK(&pspdev->dbg_work, exynos_psp_faulthandler);

#if IS_ENABLED(CONFIG_EXYNOS_ITMON) || IS_ENABLED(CONFIG_EXYNOS_ITMON_V2)
	/* add itmon notifier */
	pspdev->itmon_nb.notifier_call = exynos_psp_itmon_handler;
	itmon_notifier_chain_register(&pspdev->itmon_nb);
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	/* register panic handler */
	pspdev->panic_nb.notifier_call = exynos_psp_panic_handler;
	atomic_notifier_chain_register(&panic_notifier_list, &pspdev->panic_nb);
#endif

	return PSP_RET_OK;
err:
	kfree(pspdev);

	return ret;
}

static int exynos_psp_remove(struct platform_device *pdev)
{
	struct psp_device *pspdev = platform_get_drvdata(pdev);

	misc_deregister(&pspdev->misc_device);
	kfree(pspdev);

	return 0;
}

static void exynos_psp_shutdown(struct platform_device *pdev)
{
	exynos_psp_notify_dump();
}

static int exynos_psp_suspend(struct device *dev)
{
	int ret = PSP_RET_OK;

	//TODO enable
	//atomic_set(&psp_sleep, 1);
	//write_sfr(mb_va_base + PSP_MB_INTGR1_OFFSET, IWC_SUSPEND);

	psp_info(dev, "%s done", __func__);

	return ret;
}

static int exynos_psp_resume(struct device *dev)
{
	int ret = PSP_RET_OK;

	//TODO enable
	//atomic_set(&psp_sleep, 0);
	//write_sfr(mb_va_base + PSP_MB_INTGR1_OFFSET, IWC_RESUME);

	psp_info(dev, "%s done", __func__);

	return ret;
}

static struct dev_pm_ops exynos_psp_pm_ops = {
	.suspend	= exynos_psp_suspend,
	.resume		= exynos_psp_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_psp_match[] = {
	{
		.compatible = "samsung,exynos-psp",
	},
	{},
};
#endif

static struct platform_driver exynos_psp_driver = {
	.probe		= exynos_psp_probe,
	.remove		= exynos_psp_remove,
	.shutdown	= exynos_psp_shutdown,
	.driver		= {
		.name	= "psp",
		.owner	= THIS_MODULE,
		.pm     = &exynos_psp_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = exynos_psp_match,
#endif
	},
};

static int __init exynos_psp_init(void)
{
	int ret = PSP_RET_OK;

	ret = platform_driver_register(&exynos_psp_driver);
	if (ret) {
		pr_err("[EXYNOS][PSP] %s: fail to register driver. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	pr_info("[EXYNOS][PSP] driver is registered, (c) 2024 Samsung Electronics\n");

	return 0;
}

static void __exit exynos_psp_exit(void)
{
	platform_driver_unregister(&exynos_psp_driver);
}

module_init(exynos_psp_init);
module_exit(exynos_psp_exit);

MODULE_DESCRIPTION("EXYNOS Primary Security Processor driver");
MODULE_AUTHOR("Keunyoung Park <keun0.park@samsung.com>");
MODULE_LICENSE("GPL");
