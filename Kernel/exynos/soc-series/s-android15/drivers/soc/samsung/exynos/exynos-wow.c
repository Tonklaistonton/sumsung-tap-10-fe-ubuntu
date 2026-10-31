/*
 * exynos-wow.c - Exynos Workload Watcher Driver
 *
 *  Copyright (C) 2021 Samsung Electronics
 *  Hanjun Shin <hanjun.shin@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <soc/samsung/exynos-wow.h>
#include "exynos-wow.h"
#include <linux/of_address.h>
#include "esca/esca_ipc.h"
#define CREATE_TRACE_POINTS
#include <trace/events/exynos_wow.h>

static struct exynos_wow_data *exynos_wow;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
static void __iomem *exynos_wow_remap(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int num_pages = ((addr + size) >> PAGE_SHIFT)
					- (addr >> PAGE_SHIFT)+ 1;
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages = NULL;
	void __iomem *v_addr = NULL;
	unsigned long offset;

	if (!addr)
		return 0;

	offset = addr & (PAGE_SIZE - 1);
	addr = (addr >> PAGE_SHIFT) << PAGE_SHIFT;

	pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return 0;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);

	return v_addr + offset;
}

static void exynos_wow_send_message(struct exynos_wow_data *data,
				    enum exynos_wow_ipc_msg msg,
				    struct exynos_wow_ipc_response *response)
{
	struct ipc_config config;
	union exynos_wow_ipc_message message = {0, };
	int ret;

	dev_dbg(data->dev, "IPC request msg: %d\n", msg);
	message.req.msg = msg;

	config.cmd = message.data;
	config.response = true;
	config.indirection = false;

	ret = esca_ipc_send_data(data->ipc_ch, &config);

	dev_dbg(data->dev, "IPC response: %u, %u, %u, %u, %u, %u\n",
						message.resp.msg,
						message.resp.fw_use,
						message.resp.resp_0,
						message.resp.resp_1,
						message.resp.resp_2,
							message.resp.resp_3);
	if (response) {
		*response = message.resp;
	}
}

static int exynos_esca_wow_parse_dt(struct platform_device *pdev)
{
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	struct device_node *np;
	int size, i, cnt, ret = 0;
	void __iomem *base;
	struct exynos_wow_ipc_response response;

	np = pdev->dev.of_node;
	data->polling_delay = WOW_POLLING_MS_MAX;

	ret = esca_ipc_request_channel(pdev->dev.of_node, NULL,
				       &data->ipc_ch, &size);
	if (ret) {
		dev_err(&pdev->dev, "cannot request esca ipc channel %d\n", ret);
		goto out;
	}
	dev_info(&pdev->dev, "esca ipc channel 0x%x\n", data->ipc_ch);

	exynos_wow_send_message(exynos_wow, WOW_GET_ADDR_META, &response);

	base = exynos_wow_remap(response.resp_2, response.resp_1);
	if (!base) {
		dev_err(&pdev->dev, "fail to phys_to_virt()\n");
		goto out;
	}

	for (i = 0, cnt = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		u32 name, dummy;
		name = __raw_readl(base + sizeof(u32) * cnt++);
		strncpy(ip_data->ppc_name, (char *)&name, 4);

		ip_data->enable = __raw_readl(base + sizeof(u32) * cnt++);
		ip_data->bus_width = __raw_readl(base + sizeof(u32) * cnt++);
		ip_data->nr_info = __raw_readl(base + sizeof(u32) * cnt++);
		dummy = __raw_readl(base + sizeof(u32) * cnt++);
		dev_info(&pdev->dev,
			"master: %s, enable: %u, bus_width: %u, nr_info: %u\n",
		       ip_data->ppc_name,
		       ip_data->enable,
		       ip_data->bus_width,
		       ip_data->nr_info);
	}

	if (response.resp_1 != sizeof(u32) * cnt) {
		dev_err(&pdev->dev, "total: %u, cnt: %d\n",
			 response.resp_1, cnt);
	}

	exynos_wow_send_message(exynos_wow, WOW_GET_DATA, &response);
	data->dump_base = exynos_wow_remap(response.resp_2, response.resp_1);

	exynos_wow_send_message(exynos_wow, WOW_GET_ADDR_TS, &response);
	data->ts_base = exynos_wow_remap(response.resp_1,
				response.resp_2 * response.resp_3);
	data->ts_offset = response.resp_2;
	data->ts_nr_levels = response.resp_3;
	dev_info(&pdev->dev, "ts_offset: %u, ts_nr_levels: %u\n",
		       data->ts_offset, data->ts_nr_levels);
out:
	return ret;
}
#else
static int exynos_wow_set_counter(struct exynos_wow_data *data, int mode)
{
	int i, ret = 0;
	unsigned int value, offset;

	switch (mode) {
		case WOW_PPC_INIT:
			value = 1 << WOW_PPC_CNTENS_CCNT_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT0_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT1_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT2_OFFSET |
				1 << WOW_PPC_CNTENS_PMCNT3_OFFSET;
			offset = WOW_PPC_CNTENS;
			break;
		case WOW_PPC_START:
			value = WOW_PPC_PMNC_GLB_CNT_EN | WOW_PPC_PMNC_Q_CH_MODE;
			offset = WOW_PPC_PMNC;
			break;
		case WOW_PPC_STOP:
			value = 0;
			offset = WOW_PPC_PMNC;
			break;
		case WOW_PPC_RESET:
			value = WOW_PPC_PMNC_RESET_CCNT_PMCNT;
			offset = WOW_PPC_PMNC;
			break;
		default:
			return -EINVAL;
	}

	for (i = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		int j;

		for (j = 0; j < ip_data->nr_base; j++) {
			void __iomem *base = ip_data->wow_base[j];

			// start counter
			writel(value, base + offset);
		}
	}

	return ret;
}

static int exynos_wow_accumulator(struct exynos_wow_data *data)
{
	int i, j;

	exynos_wow_set_counter(data, WOW_PPC_STOP);

	for (i = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		int ip_type = ip_data->ip_type;
		struct exynos_wow_profile_data *pd = &data->profile.data[ip_type];

		for (j = 0; j < ip_data->nr_base; j++) {
			void __iomem *base = ip_data->wow_base[j];
			u32 ccnt, pmcnt0, pmcnt1, pmcnt2, pmcnt3;

			// Get counter value
			ccnt = __raw_readl(base + WOW_PPC_CCNT);
			pmcnt0 = __raw_readl(base + WOW_PPC_PMCNT0);
			pmcnt1 = __raw_readl(base + WOW_PPC_PMCNT1);
			pmcnt2 = __raw_readl(base + WOW_PPC_PMCNT2);
			pmcnt3 = __raw_readl(base + WOW_PPC_PMCNT3_HIGH) & 0xff;
			pmcnt3 += __raw_readl(base + WOW_PPC_PMCNT3);

			pd->ccnt_osc += ccnt * ip_data->nr_ppc;
			pd->ccnt += ccnt * ip_data->nr_ppc;
			pd->active += pmcnt0 * ip_data->nr_ppc;
			pd->transfer_data += pmcnt1 * ip_data->nr_ppc;
			pd->nr_requests += pmcnt2 * ip_data->nr_ppc;
			pd->mo_count += pmcnt3 * ip_data->nr_ppc;
		}
	}

	exynos_wow_set_counter(data, WOW_PPC_RESET);
	exynos_wow_set_counter(data, WOW_PPC_INIT);
	exynos_wow_set_counter(data, WOW_PPC_START);

	return 0;
}

static int exynos_wow_parse_dt(struct platform_device *pdev)
{
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	struct device_node *np;
	int ret = 0, i = 0;
	struct device_node *wow_ip, *child;
	struct resource res;

	np = pdev->dev.of_node;
	if (of_property_read_u32(np, "polling_delay", &data->polling_delay))
		data->polling_delay = 60000;
	dev_info(&pdev->dev, "polling_delay (%u)\n", data->polling_delay);

	wow_ip = of_find_node_by_name(np, "wow-ip");

	for_each_available_child_of_node(wow_ip, child) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i++];
		int index = 0, reg_index[10];

		if (of_property_read_u32(child, "bus_width",
					 &ip_data->bus_width))
			ip_data->bus_width = 32;
		dev_info(&pdev->dev, "bus_width (%u)\n", ip_data->bus_width);

		if (of_property_read_u32(child, "nr_base", &ip_data->nr_base))
			ip_data->nr_base = 1;
		dev_info(&pdev->dev, "nr_base (%u)\n", ip_data->nr_base);

		if (of_property_read_u32(child, "nr_ppc", &ip_data->nr_ppc))
			ip_data->nr_ppc = 1;
		dev_info(&pdev->dev, "nr_ppc (%u)\n", ip_data->nr_ppc);

		ip_data->nr_info = ip_data->nr_base * ip_data->nr_ppc;
		ip_data->enable = true;

		if (of_property_read_u32(child, "ip_type", &ip_data->ip_type))
			ip_data->ip_type = 1;
		dev_info(&pdev->dev, "ip_type (%u)\n", ip_data->ip_type);

		strncpy(ip_data->ppc_name, child->name, PPC_NAME_LENGTH);

		of_property_read_u32_array(child, "reg_index",
					   reg_index, ip_data->nr_base);

		ip_data->wow_base = kzalloc(sizeof(void __iomem *)
					    * ip_data->nr_base,
					    GFP_KERNEL);

		for (index = 0; index < ip_data->nr_base; index++) {
			of_address_to_resource(np, reg_index[index], &res);

			ip_data->wow_base[index] = devm_ioremap(&pdev->dev,
						res.start, resource_size(&res));
			dev_info(&pdev->dev, "paddr (0x%llx) vaddr (0x%llx)\n",
				 (u64)res.start,
				 (u64)ip_data->wow_base[index]);
		}
	}

	return ret;
}
#endif

static int exynos_wow_set_polling(struct exynos_wow_data *data)
{
	schedule_delayed_work_on(0, &data->dwork, msecs_to_jiffies(data->polling_delay));

	return 0;
}

static int exynos_wow_start_polling(struct exynos_wow_data *data)
{
	schedule_delayed_work_on(0, &data->dwork, 0);

	return 0;
}

static int exynos_wow_stop_polling(struct exynos_wow_data *data)
{
	cancel_delayed_work(&data->dwork);

	return 0;
}

static int exynos_wow_mode_control(struct exynos_wow_data *data, enum exynos_wow_mode mode)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
	mutex_lock(&data->lock);
#endif
	if(data->mode == mode)
		return ret;

	data->mode = mode;

	switch (mode) {
	case WOW_DISABLED:
		exynos_wow_stop_polling(data);
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
		exynos_wow_send_message(exynos_wow, WOW_SET_STOP, NULL);
#else
		exynos_wow_set_counter(data, WOW_PPC_STOP);
#endif
		break;
	case WOW_ENABLED:
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
		exynos_wow_send_message(exynos_wow, WOW_SET_START, NULL);
#else
		exynos_wow_set_counter(data, WOW_PPC_STOP);
		exynos_wow_set_counter(data, WOW_PPC_RESET);
		exynos_wow_set_counter(data, WOW_PPC_INIT);
		exynos_wow_set_counter(data, WOW_PPC_START);
#endif
		exynos_wow_start_polling(data);
		break;
	default:
		break;
	}

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
	mutex_unlock(&data->lock);
#endif

	return ret;
}

int exynos_wow_get_meta(struct exynos_wow_meta *result)
{
	struct exynos_wow_data *data = exynos_wow;
	int i;

	if (!data)
		return -ENODEV;

	for (i = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		struct exynos_wow_meta_data *md = &result->data[i];

		md->enable = ip_data->enable;
		md->bus_width = ip_data->bus_width;
		md->nr_info = ip_data->nr_info;
	}

	result->ts_nr_levels = data->ts_nr_levels;

	return 0;
}
EXPORT_SYMBOL(exynos_wow_get_meta);

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
int exynos_wow_get_data(struct exynos_wow_profile *result)
{
	struct exynos_wow_data *data = exynos_wow;
	struct exynos_wow_ipc_response response;
	void __iomem *base;
	int i, cnt;
	u32 start_time;
	ktime_t latency;

	if (!data)
		return -ENODEV;

	if (!data->mode)
		return -ENODEV;

	if (!data->dump_base)
		return -ENODEV;

	base = data->dump_base;

	mutex_lock(&data->lock);
	WRITE_ONCE(result->ktime, ktime_get());
	WRITE_ONCE(start_time ,esca_get_gptimer());
	exynos_wow_send_message(data, WOW_GET_DATA, &response);

	latency = esca_time_calc(start_time, response.resp_3);
	result->ktime += latency;
	dev_dbg(data->dev, "IPC delay: %llu\n", latency);

	for (i = 0, cnt = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_profile_data *pd =
				&result->data[i];
		pd->ccnt_osc = __raw_readq(base + sizeof(u64) * cnt++);
		pd->ccnt = __raw_readq(base + sizeof(u64) * cnt++);
		pd->active = __raw_readq(base + sizeof(u64) * cnt++);
		pd->transfer_data =
			__raw_readq(base + sizeof(u64) * cnt++);
		pd->nr_requests =
			__raw_readq(base + sizeof(u64) * cnt++);
		pd->mo_count = __raw_readq(base + sizeof(u64) * cnt++);
	}

	if (response.resp_1 != sizeof(u64) * cnt) {
		dev_err(data->dev, "total: %u, cnt: %d\n",
			 response.resp_1, cnt);
	}

	for (i = 0; i < data->ts_nr_levels; i++) {
		struct exynos_wow_ts_data *ts = &result->ts[i];

		ts->time = __raw_readq(data->ts_base + data->ts_offset * i
					+ sizeof(u64) * 0);
		ts->active_time = __raw_readq(data->ts_base + data->ts_offset * i
					+ sizeof(u64) * 1);
		ts->tdata = __raw_readq(data->ts_base + data->ts_offset * i
					+ sizeof(u64) * 2);
	}

	mutex_unlock(&data->lock);

	return 0;
}
#else
int exynos_wow_get_data(struct exynos_wow_profile *result)
{
	struct exynos_wow_data *data = exynos_wow;
	unsigned long flags;

	if (!data)
		return -ENODEV;

	if (!data->mode)
		return -ENODEV;

	spin_lock_irqsave(&data->lock, flags);

	exynos_wow_accumulator(data);
	data->profile.ktime = ktime_get();

	*result = data->profile;

	spin_unlock_irqrestore(&data->lock, flags);

	return 0;
}
#endif
EXPORT_SYMBOL(exynos_wow_get_data);

static void exynos_wow_work(struct work_struct *work)
{
	struct exynos_wow_data *data =
		container_of(work, struct exynos_wow_data, dwork.work);
	struct exynos_wow_profile profile;
	static struct exynos_wow_profile last_profile;
	int i;
	u64 duration;

	exynos_wow_get_data(&profile);
	duration = profile.ktime - last_profile.ktime;

	for (i = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_profile_data *old_pd, *new_pd;
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];
		u64 tdata_sum, bw;

		if (!ip_data->enable || !ip_data->trace_on)
			continue;

		old_pd = &last_profile.data[i];
		new_pd = &profile.data[i];

		tdata_sum = (new_pd->transfer_data - old_pd->transfer_data)
				* ip_data->bus_width;
		bw = tdata_sum / (duration / NSEC_PER_USEC);

		trace_exynos_wow(ip_data->ppc_name, (u32)bw);
	}

	last_profile = profile;

	if (data->mode == WOW_ENABLED)
		exynos_wow_set_polling(data);
}

/* sysfs nodes for amb control */
#define sysfs_printf(...) count += snprintf(buf + count, PAGE_SIZE, __VA_ARGS__)

static ssize_t
exynos_wow_mode_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0;

	sysfs_printf("%s\n", data->mode ? "enabled" : "disabled");

	return count;
}

static ssize_t
exynos_wow_mode_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int input_mode, ret;

	ret = sscanf(buf, "%d", &input_mode);

	switch(input_mode) {
		case WOW_DISABLED:
		case WOW_ENABLED:
			exynos_wow_mode_control(data, input_mode);
			break;
		default:
			break;
	}

	return count;
}

static ssize_t
exynos_wow_trace_ipmask_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0;
	int i;

	for (i = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];

		sysfs_printf("%s: %d\n", ip_data->ppc_name, ip_data->trace_on);
	}

	return count;
}

static ssize_t
exynos_wow_trace_ipmask_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	u32 ip_mask;
	int ret, i;

	ret = sscanf(buf, "%x", &ip_mask);
	if (!ret)
		goto out;

	for (i = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_ip_data *ip_data = &data->ip_data[i];

		ip_data->trace_on = !!(ip_mask & (1 << i));
	}

out:
	return count;
}

static ssize_t
exynos_wow_polling_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	unsigned int count = 0;

	sysfs_printf("polling_delay: %u\n", data->polling_delay);

	return count;
}

static ssize_t
exynos_wow_polling_store(struct device *dev, struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	u32 polling_ms;
	int ret;

	ret = sscanf(buf, "%u", &polling_ms);

	if (polling_ms && polling_ms > WOW_POLLING_MS_MAX)
		polling_ms = WOW_POLLING_MS_MAX;

	if (polling_ms && polling_ms < WOW_POLLING_MS_MIN)
		polling_ms = WOW_POLLING_MS_MIN;

	data->polling_delay = polling_ms;

	return count;
}

static ssize_t
exynos_wow_profile_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	unsigned int i, count = 0;
	static struct exynos_wow_profile last_profile;
	struct exynos_wow_profile profile;
	struct exynos_wow_meta meta;
	u64 duration;

	exynos_wow_get_data(&profile);
	exynos_wow_get_meta(&meta);
	sysfs_printf("Start time: %llu, End time: %llu\n",
		     last_profile.ktime, profile.ktime);
	duration = profile.ktime - last_profile.ktime;
	sysfs_printf("Elapsed time: %llu (ms)\n\n", duration / NSEC_PER_MSEC);

	sysfs_printf("%8s | %16s | %16s | %16s \n",
		"level", "time_in_state", "active_ts", "tdata_in_state");
	for (i = 0; i < meta.ts_nr_levels; i++) {
		struct exynos_wow_ts_data *old = &last_profile.ts[i];
		struct exynos_wow_ts_data *new = &profile.ts[i];

		sysfs_printf("%8u | %16llu | %16llu | %16llu \n",
			i,
			new->time - old->time,
			new->active_time - old->active_time,
			new->tdata - old->tdata);
	}
	sysfs_printf("\n");

	sysfs_printf("%11s | %11s | %11s | %11s | %11s | %11s\n",
		"IP_TYPE", "MHz", "CLK_ON (%)", "ACTIVE (%)", "MBytes/s", "ns");
	for (i = 0; i < NR_MASTERS; i++) {
		struct exynos_wow_profile_data *old_pd, *new_pd;
		struct exynos_wow_meta_data *md = &meta.data[i];
		u64 ccnt_osc_avg, ccnt_avg, active_avg, tdata_sum, nr_req_sum,
		    mo_cnt_sum;

		if (!md->enable)
			continue;

		old_pd = &last_profile.data[i];
		new_pd = &profile.data[i];

		ccnt_osc_avg = (new_pd->ccnt_osc - old_pd->ccnt_osc)
				/ md->nr_info;
		ccnt_avg = (new_pd->ccnt - old_pd->ccnt) / md->nr_info;
		active_avg = (new_pd->active - old_pd->active) / md->nr_info;
		tdata_sum = (new_pd->transfer_data - old_pd->transfer_data)
				* md->bus_width;
		nr_req_sum = new_pd->nr_requests - old_pd->nr_requests;
		mo_cnt_sum = new_pd->mo_count - old_pd->mo_count;

		sysfs_printf(
			"%11s | %11llu | %11llu | %11llu | %11llu | %11llu\n",
			data->ip_data[i].ppc_name,
			ccnt_osc_avg / (duration / NSEC_PER_USEC),
			ccnt_avg * 100UL / ccnt_osc_avg,
			active_avg * 100UL / ccnt_osc_avg,
			tdata_sum / (duration / NSEC_PER_USEC),
			mo_cnt_sum / nr_req_sum * (duration / ccnt_osc_avg));
	}


	last_profile = profile;
	return count;
}

static DEVICE_ATTR(wow_mode, S_IWUSR | S_IRUGO,
		exynos_wow_mode_show, exynos_wow_mode_store);

static DEVICE_ATTR(trace_ipmask, S_IWUSR | S_IRUGO,
		exynos_wow_trace_ipmask_show, exynos_wow_trace_ipmask_store);

static DEVICE_ATTR(polling_delay, S_IWUSR | S_IRUGO,
		exynos_wow_polling_show, exynos_wow_polling_store);

static DEVICE_ATTR(profile, S_IRUGO,
		exynos_wow_profile_show, NULL);

static struct attribute *exynos_wow_attrs[] = {
	&dev_attr_wow_mode.attr,
	&dev_attr_profile.attr,
	&dev_attr_trace_ipmask.attr,
	&dev_attr_polling_delay.attr,
	NULL,
};

static const struct attribute_group exynos_wow_attr_group = {
	.attrs = exynos_wow_attrs,
};

static int exynos_wow_work_init(struct platform_device *pdev)
{
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	INIT_DELAYED_WORK(&data->dwork, exynos_wow_work);

	return ret;
}

static int exynos_wow_probe(struct platform_device *pdev)
{
	int ret = 0;

	exynos_wow = kzalloc(sizeof(struct exynos_wow_data), GFP_KERNEL);
	platform_set_drvdata(pdev, exynos_wow);

	exynos_wow->dev = &pdev->dev;

# if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
	mutex_init(&exynos_wow->lock);
	ret = exynos_esca_wow_parse_dt(pdev);
#else
	spin_lock_init(&exynos_wow->lock);
	ret = exynos_wow_parse_dt(pdev);
#endif
	if (ret) {
		dev_err(&pdev->dev, "failed to parse dt (%d)\n", ret);
		kfree(exynos_wow);
		exynos_wow = NULL;
		return ret;
	}

	exynos_wow_work_init(pdev);

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_wow_attr_group);
	if (ret)
		dev_err(&pdev->dev, "cannot create exynos wow attr group\n");

	exynos_wow_mode_control(exynos_wow, WOW_ENABLED);

	dev_info(&pdev->dev, "Probe exynos wow successfully\n");

	return 0;
}

static int exynos_wow_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	ret = exynos_wow_mode_control(data, WOW_DISABLED);

	return ret;
}

static int exynos_wow_resume(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct exynos_wow_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	ret = exynos_wow_mode_control(data, WOW_ENABLED);

	return ret;
}

static const struct of_device_id exynos_wow_match[] = {
#if IS_ENABLED(CONFIG_EXYNOS_ESCA_WOW)
	{ .compatible = "samsung,exynos-esca-wow", },
#else
	{ .compatible = "samsung,exynos-wow", },
#endif
	{ /* sentinel */ },
};

static const struct dev_pm_ops exynos_wow_pm_ops = {
	.suspend_late = exynos_wow_suspend,
	.resume_early = exynos_wow_resume,
};

static struct platform_driver exynos_wow_driver = {
	.driver = {
		.name   = "exynos-wow",
		.of_match_table = exynos_wow_match,
		.suppress_bind_attrs = true,
		.pm = &exynos_wow_pm_ops,
	},
	.probe = exynos_wow_probe,
};
static int exynos_wow_init(void)
{
	return platform_driver_register(&exynos_wow_driver);
}
late_initcall(exynos_wow_init);

MODULE_DEVICE_TABLE(of, exynos_wow_match);

MODULE_AUTHOR("Hanjun Shin <hanjun.shin@samsung.com>");
MODULE_DESCRIPTION("EXYNOS Workload Watcher Driver");
MODULE_LICENSE("GPL");
