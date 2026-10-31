#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
#include <soc/samsung/esca.h>
#else
#include <soc/samsung/acpm_ipc_ctrl.h>
#endif

#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/exynos_debug_freq.h>
#include <linux/module.h>

#include "acpm_dvfs.h"
#include "cmucal.h"

static struct acpm_dvfs acpm_dvfs;
static struct acpm_dvfs acpm_noti_mif;
static struct acpm_dvfs acpm_noti_dvfs;
static struct exynos_pm_qos_request mif_request_from_acpm;
static struct exynos_pm_qos_request int_request_from_acpm;

#if IS_ENABLED(CONFIG_PRECISE_DVFS_LOGGING)
struct dss_freq {
	u16 rsvd_0;
	u16 fw_use;
	u8 domain;
	u8 seq_no;
	u16 duration;
	u32 start_time;
	u16 old_freq;
	u16 new_freq;
};

struct dss_time_sync {
	u64 ktime;
	u32 rtc;
};

static void acpm_update_time_sync(struct dss_time_sync* ts)
{
	isb();
	ts->rtc = esca_get_gptimer();
	ts->ktime = local_clock();
	isb();
}

static ktime_t acpm_rtc_to_ktime(struct dss_time_sync *ts, u32 rtc)
{
	ktime_t offset = esca_time_calc_s32(ts->rtc, rtc);

	return ts->ktime + offset;
}

static void acpm_noti_dvfs_cb(unsigned int *cmd, unsigned int size)
{
	static struct dss_time_sync dss_time_sync;
	static u32 last_end_tick;
	static u8 last_seq_no;
	struct dss_freq *ipc = (struct dss_freq *)cmd;
	struct vclk *vclk;
	ktime_t start_time, end_time;

	if (last_end_tick && ++last_seq_no != ipc->seq_no)
		pr_err("%s: DVFS log has missing, expected: %u, cur: %u\n",
		       __func__, last_seq_no, ipc->seq_no);
	last_seq_no = ipc->seq_no;

	if (esca_time_calc(last_end_tick, ipc->start_time) > (25 * NSEC_PER_MSEC))
		acpm_update_time_sync(&dss_time_sync);
	last_end_tick = ipc->start_time + ipc->duration;

	start_time = acpm_rtc_to_ktime(&dss_time_sync, ipc->start_time);
	end_time = acpm_rtc_to_ktime(&dss_time_sync, last_end_tick);

	vclk = cmucal_get_node(ACPM_VCLK_TYPE | ipc->domain);
	if (!vclk) {
		pr_err("%s: Cannot find domain: %u\n", __func__, ipc->domain);
		return;
	}

	pr_debug("%s: seq: %d domain: %s(%d), old: %u, new: %u, end: %lld\n",
		__func__, ipc->seq_no, vclk->name, vclk->dss_idx, ipc->old_freq,
		ipc->new_freq, end_time);

	dbg_snapshot_freq(vclk->dss_idx, start_time, ipc->seq_no,
			  ipc->old_freq * 1000, ipc->new_freq * 1000, DSS_FLAG_IN);
	dbg_snapshot_freq(vclk->dss_idx, end_time, ipc->seq_no,
			  ipc->old_freq * 1000, ipc->new_freq * 1000, DSS_FLAG_OUT);
}
#else
static void acpm_noti_dvfs_cb(unsigned int *cmd, unsigned int size)
{
}
#endif

int exynos_acpm_set_rate(unsigned int id, unsigned long rate, bool fast_switch)
{
	struct ipc_config config;
	unsigned int cmd[4];
	unsigned long long before, after, latency;
	unsigned int ch;
	int ret;
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	u32 start_time = esca_get_gptimer();
#else
	u32 start_time = acpm_get_peri_timer();
#endif
	config.cmd = cmd;
	config.response = !fast_switch;
	config.indirection = false;
	config.cmd[0] = id;
	config.cmd[1] = (unsigned int)rate;
	config.cmd[2] = fast_switch ? FREQ_REQ_FAST : FREQ_REQ;
	config.cmd[3] = (start_time == 0xFFFF) ? 0 : start_time;

	ch = fast_switch ? acpm_dvfs.fast_ch_num : acpm_dvfs.ch_num;

#if IS_ENABLED(CONFIG_EXYNOS_DEBUG_FREQ)
	secdbg_freq_check(id, rate);
#endif
	before = sched_clock();
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	ret = esca_ipc_send_data(ch, &config);
#else
	ret = acpm_ipc_send_data(ch, &config);
#endif
	after = sched_clock();
	latency = after - before;
	if (ret)
		pr_err("%s:[%d] latency = %llu ret = %d",
			__func__, id, latency, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_acpm_set_rate);

int exynos_acpm_set_init_freq(unsigned int dfs_id, unsigned long freq)
{
	struct ipc_config config;
	unsigned int cmd[4];
	unsigned long long before, after, latency;
	int ret, id;

	id = GET_IDX(dfs_id);

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = id;
	config.cmd[1] = (unsigned int)freq;
	config.cmd[2] = DATA_INIT;
	config.cmd[3] = SET_INIT_FREQ;

	before = sched_clock();
#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	ret = esca_ipc_send_data(acpm_dvfs.ch_num, &config);
#else
	ret = acpm_ipc_send_data(acpm_dvfs.ch_num, &config);
#endif
	after = sched_clock();
	latency = after - before;
	if (ret)
		pr_err("%s:[%d] latency = %llu ret = %d",
			__func__, id, latency, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_acpm_set_init_freq);
#if 0
unsigned long exynos_acpm_get_rate(unsigned int id)
{
	struct ipc_config config;
	unsigned int cmd[4];
	unsigned long long before, after, latency;
	int ret;

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = id;
	config.cmd[1] = 0;
	config.cmd[2] = FREQ_GET;
	config.cmd[3] = 0;

	before = sched_clock();
	ret = acpm_ipc_send_data(acpm_dvfs.ch_num, &config);
	after = sched_clock();
	latency = after - before;
	if (ret)
		pr_err("%s:[%d] latency = %llu ret = %d", __func__,
			id, latency, ret);

	return config.cmd[1];
}
#endif
#define LOWER_UPPER_MASK ((1<<13) - 1)
int exynos_acpm_set_volt_margin(unsigned int id, int lower, int upper, int volt)
{
	struct ipc_config config;
	unsigned int cmd[4];
	unsigned long long before, after, latency;
	int ret;

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = id;
	config.cmd[1] = volt;
	config.cmd[2] = MARGIN_REQ;
	config.cmd[3] = (lower & LOWER_UPPER_MASK) << 6;
	config.cmd[3] |= (upper & LOWER_UPPER_MASK) << 19;

	before = sched_clock();

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	ret = esca_ipc_send_data(acpm_dvfs.ch_num, &config);
#else
	ret = acpm_ipc_send_data(acpm_dvfs.ch_num, &config);
#endif
	after = sched_clock();
	latency = after - before;
	if (ret)
		pr_err("%s: failed to set margin [%d] latency = %llu ret = %d",
			__func__, id, latency, ret);

	return ret;
}

static void acpm_noti_mif_callback(unsigned int *cmd, unsigned int size)
{
	unsigned int domain_id = cmd[2];
	unsigned int freq = cmd[1];

	pr_debug("%s : %s req %d KHz\n", __func__, domain_id ? "INT" : "MIF", freq);

	if (domain_id == 0)
		exynos_pm_qos_update_request_nosync(&mif_request_from_acpm, freq);
	else if (domain_id == 1)
		exynos_pm_qos_update_request_nosync(&int_request_from_acpm, freq);
}

static int acpm_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0, i;
	struct device_node *np;

	acpm_noti_mif.dev = dev->of_node;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	ret = esca_ipc_request_channel(acpm_noti_mif.dev,
				 acpm_noti_mif_callback,
				 &acpm_noti_mif.ch_num,
				 &acpm_noti_mif.size);
#else
	ret = acpm_ipc_request_channel(acpm_noti_mif.dev,
				 acpm_noti_mif_callback,
				 &acpm_noti_mif.ch_num,
				 &acpm_noti_mif.size);
#endif
	if (ret < 0)
		return ret;

	np = of_get_child_by_name(dev->of_node, "noti_dvfs");
	if (np) {
		acpm_noti_dvfs.dev = np;

		ret = esca_ipc_request_channel(acpm_noti_dvfs.dev,
					 acpm_noti_dvfs_cb,
					 &acpm_noti_dvfs.ch_num,
					 &acpm_noti_dvfs.size);
	}

	for (i = 0; i < cmucal_get_list_size(ACPM_VCLK_TYPE); i++) {
		struct vclk *vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);

		vclk->dss_idx = dbg_snapshot_get_freq_idx(vclk->name);
	}

	exynos_pm_qos_add_request(&mif_request_from_acpm, PM_QOS_BUS_THROUGHPUT, 0);
	exynos_pm_qos_add_request(&int_request_from_acpm, PM_QOS_DEVICE_THROUGHPUT, 0);

	return ret;
}

void exynos_acpm_set_device(void *dev)
{
	acpm_dvfs.dev = dev;
}
EXPORT_SYMBOL_GPL(exynos_acpm_set_device);

void exynos_acpm_set_fast_switch(unsigned int ch)
{
	acpm_dvfs.fast_ch_num = ch;
}
EXPORT_SYMBOL_GPL(exynos_acpm_set_fast_switch);

static int acpm_dvfs_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id acpm_dvfs_match[] = {
	{ .compatible = "samsung,exynos-acpm-dvfs" },
	{},
};
MODULE_DEVICE_TABLE(of, acpm_dvfs_match);

static struct platform_driver samsung_acpm_dvfs_driver = {
	.probe	= acpm_dvfs_probe,
	.remove	= acpm_dvfs_remove,
	.driver	= {
		.name = "exynos-acpm-dvfs",
		.owner	= THIS_MODULE,
		.of_match_table	= acpm_dvfs_match,
	},
};

int exynos_acpm_dvfs_init(void)
{
	int ret;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA)
	ret = esca_ipc_request_channel(acpm_dvfs.dev,
				 NULL,
				 &acpm_dvfs.ch_num,
				 &acpm_dvfs.size);
#else
	ret = acpm_ipc_request_channel(acpm_dvfs.dev,
				 NULL,
				 &acpm_dvfs.ch_num,
				 &acpm_dvfs.size);
#endif
	if (ret < 0)
		pr_err("acpm_dvfs_init fail ret = %d\n", ret);

	secdbg_freq_init();

	return platform_driver_register(&samsung_acpm_dvfs_driver);
}

MODULE_LICENSE("GPL");
