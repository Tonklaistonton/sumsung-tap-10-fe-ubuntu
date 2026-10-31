// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/ip6_checksum.h>
#include <net/tcp.h>

#define CLO_PORT_APPLIED 1
#define CLO_PORT_DEFAULT 0
#define MAX_PORT_NUM (1<<16)
struct clo_port_filter {
	unsigned int size;
	uint8_t ports[MAX_PORT_NUM];
	struct rcu_head rcu;
};

spinlock_t lock_clo_port_filter;
struct clo_port_filter *clo_filter;

int set_clo_port(const char *buf, const struct kernel_param *kp)
{
	int len;
	int ret;
	unsigned int port;
	char *tmp, *copy, *token;
	const char *delim = ",";

	struct clo_port_filter *new_filter;
	struct clo_port_filter *old_filter;

	if (!buf)
		return 0;

	len = strlen(buf);
	tmp = copy = kstrdup(buf, GFP_KERNEL);
	if (!tmp)
		goto end;

	new_filter = kzalloc(sizeof(struct clo_port_filter), GFP_KERNEL);

	while ((token = strsep(&tmp, delim)) != NULL) {
		ret = kstrtouint(token, 10, &port);
		if (ret || port >= MAX_PORT_NUM)
			break;

		new_filter->ports[port] = CLO_PORT_APPLIED;
		new_filter->size++;
	}

	spin_lock(&lock_clo_port_filter);
	old_filter = rcu_dereference_protected(clo_filter,
					lockdep_is_held(&lock_clo_port_filter));
	rcu_assign_pointer(clo_filter, new_filter);
	spin_unlock(&lock_clo_port_filter);

	if (old_filter) {
		synchronize_rcu();
		kfree(old_filter);
	}

	kfree(copy);
end:
	return len;
}

int get_clo_port(char *buf, const struct kernel_param *kp)
{
	int len = 0;
	unsigned int port = 0;
	struct clo_port_filter *filter;

	rcu_read_lock();
	filter = rcu_dereference(clo_filter);

	if (filter) {
		for (port = 0; port < MAX_PORT_NUM; port++) {
			if (filter->ports[port])
				len += scnprintf(buf + len, PAGE_SIZE, "%u,", port);
		}
	}

	rcu_read_unlock();

	len += scnprintf(buf + len, PAGE_SIZE, "\n");
	return len;
}

const struct kernel_param_ops clo_port_ops = {
		.set = &set_clo_port,
		.get = &get_clo_port,
};

module_param_cb(clo_port_map,
	&clo_port_ops,
	NULL,
	0640);

#define CLO_PSH_OPTION_NONE 0x0
#define CLO_PSH_OPTION_COUNT 0x1
#define CLO_PSH_OPTION_RESET 0x2
#define CLO_PSH_OPTION_PORT 0x3

unsigned int clo_pkt_count;
module_param(clo_pkt_count, uint, 0640);
unsigned int clo_psh_pkt_count;
module_param(clo_psh_pkt_count, uint, 0640);
unsigned int clo_psh_option __read_mostly = CLO_PSH_OPTION_COUNT;
module_param(clo_psh_option, uint, 0640);
unsigned int clo_gro_enable __read_mostly;
module_param(clo_gro_enable, uint, 0640);

long clo_gro_flush_time = 10000;
EXPORT_SYMBOL_GPL(clo_gro_flush_time);
module_param(clo_gro_flush_time, long, 0644);


static inline void clo_tcp_replace_psh(struct sk_buff *skb, struct tcphdr *th)
{
	if (!th->psh)
		return;

	inet_proto_csum_replace4(&th->check, skb, htons(0x0008), htons(0x0000), 0);
	th->psh = 0;
}

static void clo_update_psh(struct sk_buff *skb)
{
	struct iphdr *hdr = NULL;
	struct ipv6hdr *hdr6 = NULL;
	struct tcphdr *th = NULL;
	uint16_t dst_port = 0;
	__be32 flags = 0;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		hdr = ip_hdr(skb);
		if (hdr->protocol == IPPROTO_TCP) {
			th = (struct tcphdr *)(skb_header_pointer(skb, sizeof(struct iphdr), sizeof(struct tcphdr), th));
			if (th)
				dst_port = ntohs(th->dest);
		}
		break;
	case htons(ETH_P_IPV6):
		hdr6 = ipv6_hdr(skb);
		if (hdr6->nexthdr == IPPROTO_TCP) {
			th = (struct tcphdr *)(skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(struct tcphdr), th));
			if (th)
				dst_port = ntohs(th->dest);
		}
		break;
	}

	if (!th)
		return;

	flags = tcp_flag_word(th);
	if (!(flags & TCP_FLAG_PSH))
		return;

	if (clo_psh_option == CLO_PSH_OPTION_COUNT)
		clo_psh_pkt_count++;
	else if (clo_psh_option == CLO_PSH_OPTION_RESET)
		clo_tcp_replace_psh(skb, th);
	else if (clo_psh_option == CLO_PSH_OPTION_PORT) {
		struct clo_port_filter *filter;

		rcu_read_lock();
		filter = rcu_dereference(clo_filter);

		if (!filter) {
			rcu_read_unlock();
			return;
		}

		if (filter->ports[dst_port] == CLO_PORT_APPLIED)
			clo_tcp_replace_psh(skb, th);

		rcu_read_unlock();
	}
}

static inline void clo_update_stats(struct sk_buff *skb)
{
	clo_pkt_count += ((unsigned int) (skb_shinfo(skb)->nr_frags) + 1);
}

void clo_hook_skb(struct sk_buff *skb)
{
	if (!clo_psh_option)
		return;

	clo_update_stats(skb);

	clo_update_psh(skb);
}
EXPORT_SYMBOL(clo_hook_skb);

int clo_is_activated(void)
{
	return clo_gro_enable;
}
EXPORT_SYMBOL(clo_is_activated);

static void clo_port_filter_init(void)
{
	struct clo_port_filter *obj = kzalloc(sizeof(struct clo_port_filter), GFP_ATOMIC);

	if (!obj)
		return;

	init_rcu_head(&obj->rcu);
	lock_clo_port_filter = __SPIN_LOCK_UNLOCKED(lock_clo_port_filter);

	spin_lock(&lock_clo_port_filter);
	rcu_assign_pointer(clo_filter, obj);
	spin_unlock(&lock_clo_port_filter);
}

static int clo_init(void)
{
	clo_port_filter_init();
	return 0;
}

static void clo_exit(void)
{
}

module_init(clo_init);
module_exit(clo_exit);

MODULE_AUTHOR("yw8738.kim@samsung.com");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Cross Layer Optimizer For Mobile Network");
MODULE_LICENSE("GPL v2");
