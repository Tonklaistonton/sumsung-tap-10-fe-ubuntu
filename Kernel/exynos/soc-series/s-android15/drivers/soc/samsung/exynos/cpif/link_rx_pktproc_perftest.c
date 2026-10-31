// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018-2019, Samsung Electronics.
 *
 */

#include <net/ip.h>
#include <net/udp.h>
#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE_IOMMU)
#include "link_device_pcie_iommu.h"
#endif
#include "link_rx_pktproc.h"

static struct pktproc_perftest_data perftest_data[PERFTEST_MODE_MAX] = {
	{
		/* empty */
	},
	{
		/* PERFTEST_MODE_IPV4
		 * port: 5000 -> 5001
		 * payload: 1464 (0x5b8)
		 */
		.header = {
			0x45, 0x00, 0x05, 0xB8, 0x00, 0x00, 0x40, 0x00,
			0x80, 0x11, 0x71, 0xDF, 0xC0, 0xA8, 0x01, 0x03,
			0xC0, 0xA8, 0x01, 0x02, 0x13, 0x88, 0x13, 0x89,
			0x05, 0xA4, 0x00, 0x00
		},
		.header_len = 28,
		.dst_port_offset = 22,
		.packet_len = 1464
	},
	{
		/* PERFTEST_MODE_CLAT
		 * port: 5000 -> 5001
		 * payload: 1444 (0x5a4)
		 */
		.header = {
			0x60, 0x0a, 0xf8, 0x0c, 0x05, 0xa4, 0x11, 0x40,
			0x00, 0x64, 0xff, 0x9b, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x02,
			0x20, 0x01, 0x02, 0xd8, 0xe1, 0x43, 0x7b, 0xfb,
			0x1d, 0xda, 0x90, 0x9d, 0x8b, 0x8d, 0x05, 0xe7,
			0x13, 0x88, 0x13, 0x89, 0x05, 0xa4, 0x00, 0x00,
		},
		.header_len = 48,
		.dst_port_offset = 42,
		.packet_len = 1484
	},
	{
		/* PERFTEST_MODE_IPV6
		 * port: 5000 -> 5001
		 * payload: 1444 (0x5a4)
		 */
		.header = {
			0x60, 0x0a, 0xf8, 0x0c, 0x05, 0x90, 0x11, 0x40,
			0x20, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
			0x20, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
			0x7d, 0xae, 0x3d, 0x4e, 0xac, 0xf2, 0x8a, 0x2b,
			0x13, 0x88, 0x13, 0x89, 0x05, 0x90, 0x00, 0x00,
		},
		.header_len = 48,
		.dst_port_offset = 42,
		.packet_len = 1464
	},
	{
		/* PERFTEST_MODE_IPV4_LRO (csum 1234)
		 * port: 5000 -> 5001
		 * payload: 1464 (0x5b8)
		 */
		.header = {
			0x45, 0x00, 0x05, 0xB8, 0x00, 0x00, 0x40, 0x00,
			0x80, 0x11, 0x00, 0x00, 0xC0, 0xA8, 0x01, 0x03,
			0xC0, 0xA8, 0x01, 0x02, 0x13, 0x88, 0x13, 0x89,
			0x05, 0xA4, 0x12, 0x34
		},
		.header_len = 28,
		.dst_port_offset = 22,
		.packet_len = 1464
	},
};

static void pktproc_perftest_napi_schedule(void *arg)
{
	struct pktproc_queue *q = (struct pktproc_queue *)arg;

	if (!q) {
		mif_err_limited("q is null\n");
		return;
	}

	if (!pktproc_get_usage(q))
		return;

	napi_schedule(q->napi_ptr);
}

static unsigned int pktproc_perftest_gen_rx_packet(
		struct pktproc_queue *q, int packet_num, int session)
{
	struct pktproc_desc *desc = q->desc;
	struct pktproc_perftest *perf = &q->ppa->perftest;
	u32 header_len = perftest_data[perf->mode].header_len;
	u32 packet_len = perftest_data[perf->mode].packet_len;
	u32 rear_ptr;
	unsigned int space, loop_count;
	u8 *src = NULL;
	u32 *seq;
	u16 *dst_port;
	u16 *dst_addr;
	int i, j;
#if IS_ENABLED(CONFIG_CP_PKTPROC_LRO)
	int total_length = 0;
	bool lro_start = true; /* this var used for LRO only */
	struct iphdr *iph = NULL;
	struct udphdr *uh = NULL;
#endif
	rear_ptr = *q->rear_ptr;
	space = circ_get_space(q->num_desc, rear_ptr, *q->fore_ptr);
	loop_count = min_t(unsigned int, space, packet_num);

	for (i = 0 ; i < loop_count ; i++) {
		/* set desc */
		desc[rear_ptr].status =
			PKTPROC_STATUS_DONE | PKTPROC_STATUS_TCPC | PKTPROC_STATUS_IPCS;
		desc[rear_ptr].length = packet_len;
		desc[rear_ptr].filter_result = 0x9;
		desc[rear_ptr].channel_id = perf->ch;

		/* set data */
#if IS_ENABLED(CONFIG_LINK_DEVICE_PCIE_IOMMU)
		src = q->ioc.pf_buf[rear_ptr] + q->ppa->skb_padding_size;
#elif IS_ENABLED(CONFIG_EXYNOS_CPIF_IOMMU) || IS_ENABLED(CONFIG_EXYNOS_CPIF_IOMMU_V9)
		src = q->manager->apair_arr[rear_ptr].ap_addr + q->ppa->skb_padding_size;
#else
		src = desc[rear_ptr].cp_data_paddr -
				q->cp_buff_pbase + q->q_buff_vbase;
#endif
		if (!src) {
			mif_err_limited("src is null\n");
			return -EINVAL;
		}
		memset(src, 0x0, desc[rear_ptr].length);
		memcpy(src, perftest_data[perf->mode].header, header_len);
		seq = (u32 *)(src + header_len);
		*seq = htonl(perf->seq_counter[session]++);
		dst_port = (u16 *)(src + perftest_data[perf->mode].dst_port_offset);
		*dst_port = htons(5001 + session);
		if (perf->mode == PERFTEST_MODE_CLAT) {
			for (j = 0 ; j < 8 ; j++) {
				dst_addr = (u16 *)(src + 24 + (j * 2));
				*dst_addr = htons(perf->clat_ipv6[j]);
			}
		}

#if IS_ENABLED(CONFIG_CP_PKTPROC_LRO)
		desc[rear_ptr].lro = LRO_MODE_ON; /* LRO on and packet is LRO */
		if (perf->mode == PERFTEST_MODE_IPV4_LRO) {
			desc[rear_ptr].lro |= LRO_PACKET;
			desc[rear_ptr].lro_q_idx = 0;
			desc[rear_ptr].lro_head_offset = header_len;

			if (lro_start) {
				total_length += packet_len;
				iph = (struct iphdr *)src;
				uh = (struct udphdr *)(src + sizeof(struct iphdr));
			} else
				total_length += packet_len - header_len;

			if ((i == (loop_count - 1)) || (total_length + (packet_len - header_len) > SZ_64K)) {
				desc[rear_ptr].lro |= LRO_LAST_SEG;
				iph->tot_len = htons(total_length);
				uh->len = htons(total_length - sizeof(struct iphdr));
				lro_start = true; // start another lro
				total_length = 0;
			} else if (lro_start) {
				desc[rear_ptr].lro |= LRO_FIRST_SEG;
				lro_start = false;
			} else /* MID segs */
				desc[rear_ptr].lro |= LRO_MID_SEG;
		}
#endif
		rear_ptr = circ_new_ptr(q->num_desc, rear_ptr, 1);
	}
	*q->rear_ptr = rear_ptr;

	return loop_count;
}

static int pktproc_perftest_thread(void *arg)
{
	struct mem_link_device *mld = (struct mem_link_device *) arg;
	struct pktproc_adaptor *ppa = &mld->pktproc;
	struct pktproc_queue *q = ppa->q[0];
	struct pktproc_perftest *perf = &ppa->perftest;
	bool session_queue = false;
	int i, pkts;

	if (ppa->use_exclusive_irq && (perf->session > 1) && (perf->session <= ppa->num_queue))
		session_queue = true;

	/* max 1023 packets per 1ms for 12Gbps */
	pkts = (perf->session > 0 ? (1023 / perf->session) : 0);
	do {
		for (i = 0 ; i < perf->session ; i++) {
			int napi_cpu = perf->ipi_cpu[0];

			if (session_queue)
				q = ppa->q[i];

			if (!pktproc_perftest_gen_rx_packet(q, pkts, i))
				continue;

			if (session_queue)
				napi_cpu = perf->ipi_cpu[i];

			if (napi_cpu >= 0 && cpu_online(napi_cpu)) {
				smp_call_function_single(napi_cpu,
							 pktproc_perftest_napi_schedule,
							 (void *)q, 0);
			} else {
				pktproc_perftest_napi_schedule(q);
			}
		}

		udelay(perf->udelay);

		if (kthread_should_stop())
			break;
	} while (perf->test_run);

	return 0;
}

ssize_t perftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);
	struct link_device *ld = get_current_link(mc->iod);
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct pktproc_adaptor *ppa = &mld->pktproc;
	struct pktproc_perftest *perf = &ppa->perftest;
	unsigned int mode = 0, session, perf_cpu;

	static struct task_struct *worker_task;
	int ret;

	perf->ipi_cpu[0] = -1;
	if (ppa->use_exclusive_irq) {
		perf->ipi_cpu[0] = 4;
		perf->ipi_cpu[1] = 4;
		perf->ipi_cpu[2] = 4;
		perf->ipi_cpu[3] = 4;
	}

	switch (perf->mode) {
	case PERFTEST_MODE_CLAT:
		ret = sscanf(buf, "%d %d %hu %d %d %hx:%hx:%hx:%hx:%hx:%hx:%hx:%hx %d %d %d %d",
			     &mode, &session, &perf->ch, &perf_cpu, &perf->udelay,
			     &perf->clat_ipv6[0], &perf->clat_ipv6[1], &perf->clat_ipv6[2],
			     &perf->clat_ipv6[3], &perf->clat_ipv6[4], &perf->clat_ipv6[5],
			     &perf->clat_ipv6[6], &perf->clat_ipv6[7],
			     &perf->ipi_cpu[0], &perf->ipi_cpu[1], &perf->ipi_cpu[2],
			     &perf->ipi_cpu[3]);
		break;
	default:
		ret = sscanf(buf, "%d %d %hu %d %d %d %d %d %d",
			     &mode, &session, &perf->ch, &perf_cpu, &perf->udelay,
			     &perf->ipi_cpu[0], &perf->ipi_cpu[1], &perf->ipi_cpu[2],
			     &perf->ipi_cpu[3]);
		break;
	}

	if (ret < 1 || mode > PERFTEST_MODE_MAX)
		return -EINVAL;

	perf->mode = mode;
	perf->session = session > PKTPROC_MAX_QUEUE ? PKTPROC_MAX_QUEUE : session;
	perf->cpu = perf_cpu > num_possible_cpus() ? num_possible_cpus() - 1 : perf_cpu;

	switch (perf->mode) {
	case PERFTEST_MODE_STOP:
		if (perf->test_run)
			kthread_stop(worker_task);

		perf->seq_counter[0] = 0;
		perf->seq_counter[1] = 0;
		perf->seq_counter[2] = 0;
		perf->seq_counter[3] = 0;
		perf->test_run = false;
		break;
	case PERFTEST_MODE_IPV4:
#if IS_ENABLED(CONFIG_CP_PKTPROC_LRO)
	case PERFTEST_MODE_IPV4_LRO:
#endif
	case PERFTEST_MODE_CLAT:
	case PERFTEST_MODE_IPV6:
		if (perf->test_run)
			kthread_stop(worker_task);

		perf->test_run = true;
		worker_task = kthread_create_on_node(pktproc_perftest_thread,
			mld, cpu_to_node(perf->cpu), "perftest/%d", perf->cpu);
		kthread_bind(worker_task, perf->cpu);
		wake_up_process(worker_task);
		break;
	default:
		break;
	}

	return count;
}


ssize_t perftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct modem_ctl *mc = dev_get_drvdata(dev);
	struct link_device *ld = get_current_link(mc->iod);
	struct mem_link_device *mld = to_mem_link_device(ld);
	struct pktproc_adaptor *ppa = &mld->pktproc;
	struct pktproc_perftest *perf = &ppa->perftest;
	ssize_t count = 0;

	count += scnprintf(&buf[count], PAGE_SIZE - count, "test_run:%d\n", perf->test_run);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "mode:%d\n", perf->mode);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "session:%d\n", perf->session);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "ch:%d\n", perf->ch);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "udelay:%d\n", perf->udelay);
	count += scnprintf(&buf[count], PAGE_SIZE - count, "cpu:%d\n", perf->cpu);

	if (ppa->use_exclusive_irq)
		count += scnprintf(&buf[count], PAGE_SIZE - count, "ipi cpu:%d %d %d %d\n",
			perf->ipi_cpu[0], perf->ipi_cpu[1], perf->ipi_cpu[2], perf->ipi_cpu[3]);

	if (perf->mode == PERFTEST_MODE_CLAT)
		count += scnprintf(&buf[count], PAGE_SIZE - count, "clat %x:%x:%x:%x:%x:%x:%x:%x\n",
			perf->clat_ipv6[0], perf->clat_ipv6[1], perf->clat_ipv6[2],
			perf->clat_ipv6[3], perf->clat_ipv6[4], perf->clat_ipv6[5],
			perf->clat_ipv6[6], perf->clat_ipv6[7]);

	return count;
}

