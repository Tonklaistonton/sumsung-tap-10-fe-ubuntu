/* SPDX-License-Identifier: GPL-2.0
 *
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
#ifndef __CLO_H__
#define __CLO_H__

#include <linux/printk.h>

#define clo_err(fmt, ...) pr_err("CLOE[%d] %s : "fmt"\n", raw_smp_processor_id(), __func__, ##__VA_ARGS__)
#define clo_info(fmt, ...) pr_info("CLOI[%d] %s : "fmt"\n", raw_smp_processor_id(), __func__, ##__VA_ARGS__)
#define clo_dbg(fmt, ...) pr_debug("CLOD[%d] %s : "fmt"\n", raw_smp_processor_id(), __func__, ##__VA_ARGS__)

extern long clo_gro_flush_time;
void clo_hook_skb(struct sk_buff *skb);
int clo_is_activated(void);

#endif // __CLO_H__
