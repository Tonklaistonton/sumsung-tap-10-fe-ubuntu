// SPDX-License-Identifier: GPL-2.0+
#include <linux/bits.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <asm/sysreg.h>

#include <soc/samsung/exynos-cpupm.h>

#ifndef EXYNOS_MPAM_H
#define EXYNOS_MPAM_H

#define MPAM_PARTID_DEFAULT 0
#define NUM_MPAM_ENTRIES 9
#define NUM_OF_MSC 3

static const char *mpam_entry_names[] = {
	"ROOT",		/* DEFAULT_PARTID */
	"background",
	"camera-daemon",
	"system-background",
	"foreground",
	"restricted",
	"top-app",
	"dexopt",
	"audio-app"
};
struct msc_domain;
struct msc_domain {
	struct platform_device *pdev;
	spinlock_t	lock;
	bool		enabled;
	int		id;
	u32		base_addr;
	u32		size;
	void __iomem	*base;
	u32		partid_count;
	struct kobject	*ko_root;
	void		(*restore_attr)(struct msc_domain *msc);
};

static const char *msc_domain_name[] = {
	"msc_dsu",
	"msc_llc"
};

#define for_each_msc(msc, id)	\
	for (id = -1; (msc) = next_msc_domain(&id), (msc) != NULL;)

#define FIELD_SET(reg, field, val) (reg = (reg & ~field) | FIELD_PREP(field, val))

extern void mpam_write_partid(unsigned int partid);
extern unsigned int mpam_get_partid_count(struct msc_domain *msc);
extern int mpam_late_init_notifier_register(struct notifier_block *nb);
extern int llc_mpam_alloc(unsigned int index, int size, int partid, int pmon_gr, int ns, int on);
extern int exynos_mpam_register_domain(struct msc_domain *msc_dom);
extern struct kobj_type *get_mpam_kobj_ktype(void);
#endif
