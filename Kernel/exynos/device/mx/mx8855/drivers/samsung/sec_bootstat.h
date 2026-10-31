/* sec_bootstat.h
 *
 * Copyright (C) 2014 Samsung Electronics
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/exynos/debug-snapshot.h>
typedef struct dss_pmsg_notifier_data pmsg_notifier_data;
#elif IS_ENABLED(CONFIG_SEC_DEBUG)
#include <linux/sec_debug.h>
typedef struct sec_debug_pmsg_notifier_data pmsg_notifier_data;
#endif

static inline void pmsg_chain_register(struct notifier_block *nb) {
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	dbg_snapshot_pmsg_chain_register(nb);
#elif IS_ENABLED(CONFIG_SEC_DEBUG)
	sec_debug_pmsg_chain_register(nb);
#else
	pr_err("pmsg notifier chain was not registered");
#endif
}
