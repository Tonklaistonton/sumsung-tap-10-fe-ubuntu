/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/gfp.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>

#include <soc/samsung/exynos/debug-snapshot.h>

#define NUM_OF_MIF_CH        (4)
#define DEBUG_TIMING_SET     (0x2086490)

unsigned int lock_mon_ofs[] = {0x734,  0x738};
unsigned int lp_con_offset = 0x18;

typedef union {
        volatile unsigned int data;
        struct {
                unsigned int lock_value_max_err : (9 - 0 + 1);
                unsigned int lock_value_min_err : (19 - 10 + 1);
                unsigned int lock_value_diff : (29 - 20 + 1);
                unsigned int lock_value_mon_en : (30 - 30 + 1);
                unsigned int lock_value_mon_clear : (31 - 31 + 1);
        } bitfield;
} phy_lock_val_mon_t;

typedef union {
	volatile unsigned int data;
	struct {
		unsigned int ctrl_pulld_dqs : (1 - 0 + 1);
		unsigned int reserved3 : (2 - 2 + 1);
		unsigned int write_se_wck : (3 - 3 + 1);
		unsigned int reserved2 : (5 - 4 + 1);
		unsigned int ctrl_scheduler_en : (6 - 6 + 1);
		unsigned int ctrl_dq_rcv_on : (7 - 7 + 1);
		unsigned int ctrl_dqs_drv_off : (8 - 8 + 1);
		unsigned int wck_enable : (9 - 9 + 1);
		unsigned int dqs_enable : (10 - 10 + 1);
		unsigned int mdll_cg_en : (11 - 11 + 1);
		unsigned int pcl_pd : (12 - 12 + 1);
		unsigned int scheduler_hw_clock_gating_disable : (13 - 13 + 1);
		unsigned int ds_io_pd : (14 - 14 + 1);
		unsigned int cs_io_pd : (15 - 15 + 1);
		unsigned int ctrl_pulld_dq : (17 - 16 + 1);
		unsigned int ctrl_wck_phy_cg_en_ignore : (19 - 18 + 1);
		unsigned int reserved1 : (20 - 20 + 1);
		unsigned int gategen_phy_cg_en_ignore : (21 - 21 + 1);
		unsigned int wr_cg_off_apb : (22 - 22 + 1);
		unsigned int tphy_wrdata_apb : (23 - 23 + 1);
		unsigned int ignore_phy_cg_en : (24 - 24 + 1);
		unsigned int dll_upd_dynamic_cg_en : (25 - 25 + 1);
		unsigned int sync_cg_off : (26 - 26 + 1);
		unsigned int sync_cg_ctrl_en : (27 - 27 + 1);
		unsigned int gate_gen_dyn_cg_en : (28 - 28 + 1);
		unsigned int reserved0 : (31 - 29 + 1);
	} bitfield;
} phy_lp_con0_t;

void __iomem			*timing_set_addr;

struct lock_irq_desc {
	int irq;
	unsigned int ch;
};

struct exynos_phylock_desc {
	struct lock_irq_desc *ddrphylockdelta_irqs;
	raw_spinlock_t lock;
	int irq_num;
	void __iomem			*phy_base[NUM_OF_MIF_CH];
};

static struct exynos_phylock_desc phylock_desc;

static struct lock_irq_desc *get_lock_irq_desc(int irq)
{
	int i;
	struct lock_irq_desc *desc = NULL;

	for (i = 0; i < phylock_desc.irq_num; i++) {
		if (phylock_desc.ddrphylockdelta_irqs[i].irq == irq) {
			desc = &phylock_desc.ddrphylockdelta_irqs[i];
			break;
		}
	}
	return desc;
}

static irqreturn_t exynos_ddrphylockdelta_handler(int irq, void *dev_id)
{
	struct lock_irq_desc *lock_irq_desc;
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned int previous_val;
	void * lpcon_addr, *lockmon_addr;
	phy_lp_con0_t lp_con0;
	phy_lock_val_mon_t lock_val_mon;
	int timing_set = __raw_readl(timing_set_addr);

	lock_irq_desc = get_lock_irq_desc(irq);

	if (!lock_irq_desc) {
		printk("Unexpected DDRPHY_LOCK_VAL_MON ECC IRQ(%d)", irq);
		return IRQ_NONE;
	}

	lpcon_addr = \
		phylock_desc.phy_base[lock_irq_desc->ch] \
		+ lp_con_offset;
		
	lockmon_addr = \
		phylock_desc.phy_base[lock_irq_desc->ch] \
		+ lock_mon_ofs[timing_set];

	// clock gating off
	previous_val = __raw_readl(lpcon_addr);
	lp_con0.data = previous_val;
	lp_con0.bitfield.pcl_pd = 0;
	lp_con0.bitfield.sync_cg_off = 1;
	lp_con0.bitfield.ignore_phy_cg_en = 1;
	__raw_writel(lp_con0.data, lpcon_addr);

	if (desc && desc->action && desc->action->name)
		printk("[DDRPHY_LOCK_VAL_MON] %s\n", desc->action->name);
	else
		printk("[DDRPHY_LOCK_VAL_MON] irq=%d\n", irq);

	// get min/max
	lock_val_mon.data = __raw_readl(lockmon_addr);

	printk("[DDRPHY_LOCK_VAL_MON] [CH%d] diff:%d, min:%d, max:%d\n", \
		lock_irq_desc->ch, lock_val_mon.bitfield.lock_value_diff, \
		lock_val_mon.bitfield.lock_value_min_err, \
		lock_val_mon.bitfield.lock_value_max_err);

	// err clear
	lock_val_mon.bitfield.lock_value_mon_clear = 1;
	__raw_writel(lock_val_mon.data, lockmon_addr);

	lock_val_mon.bitfield.lock_value_mon_clear = 0;
	__raw_writel(lock_val_mon.data, lockmon_addr);

	// restore lp_con
	__raw_writel(previous_val, lpcon_addr);


	return IRQ_HANDLED;
}

static int exynos_ddrphylockdelta_handler_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct property *prop;
	const char *name;
	char *buf = (char *)__get_free_page(GFP_KERNEL);
	int irq_num;
	int err = 0, idx = 0;
	int ch, ch_num;
	unsigned int base = 0;

	if (!buf) {
		err = -ENOMEM;
		goto err_buf_alloc;
	}


	irq_num = of_property_count_strings(np, "interrupt-names");
	if (irq_num <= 0) {
		dev_err(&pdev->dev, \
			"Invalid Interrupt-names property count(%d)\n",\
			 irq_num);
		err = -EINVAL;
		goto err_irq_name_counts;
	}

	phylock_desc.ddrphylockdelta_irqs = \
		kzalloc(sizeof(struct lock_irq_desc) * irq_num, GFP_KERNEL);
	if (!phylock_desc.ddrphylockdelta_irqs) {
		err = -ENOMEM;
		goto err_desc_alloc;
	}

	//cpu_hotplug_disable();

	phylock_desc.irq_num = irq_num;
	timing_set_addr = ioremap(DEBUG_TIMING_SET, SZ_4K);

	ch_num = of_property_count_u32_elems(np, "base");
	for (ch = 0; ch < ch_num; ch++) {
		of_property_read_u32_index(np, "base", ch, &base);
		phylock_desc.phy_base[ch] = ioremap(base, SZ_4K);
	}

	of_property_for_each_string(np, "interrupt-names", prop, name) {
		unsigned int irq, val;

		if (!name) {
			dev_err(&pdev->dev, "no such name\n");
			err = -EINVAL;
			break;
		}

		if (!of_property_read_u32_index(np, "ch", idx, &val))
			phylock_desc.ddrphylockdelta_irqs[idx].ch = val;
		else
			phylock_desc.ddrphylockdelta_irqs[idx].ch = 0;


		irq = platform_get_irq(pdev, idx);

		phylock_desc.ddrphylockdelta_irqs[idx].irq = irq;

		err = request_irq(irq, exynos_ddrphylockdelta_handler, \
			IRQF_NOBALANCING, name, NULL);
		if (err) {
			dev_err(&pdev->dev, \
				"unable irq%u PHY_LOCK_MON handler[%s]\n", \
				irq, name);
			break;
		}
		dev_info(&pdev->dev, \
			"Success irq%u PHY_LOCK_MON handler[%s]\n", \
			irq, name);

		idx++;
	}

	if (irq_num != idx) {
		int i;

		dev_err(&pdev->dev, \
			"failed, irq_num not matched(%d/%d)\n", idx, irq_num);
		for (i = 0; i < idx; i++)
			free_irq(phylock_desc.ddrphylockdelta_irqs[i].irq, \
				 NULL);

		kfree(phylock_desc.ddrphylockdelta_irqs);
		phylock_desc.ddrphylockdelta_irqs = NULL;
		phylock_desc.irq_num = 0;
		goto err_register_irq;
	}

	raw_spin_lock_init(&phylock_desc.lock);
err_register_irq:
err_desc_alloc:
err_irq_name_counts:
	free_page((unsigned long)buf);
err_buf_alloc:
	return err;
}

static const struct of_device_id exynos_ddrphylockdelta_handler_matches[] = {
	{ .compatible = "samsung,exynos-ddrphylockdelta-handler", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ddrphylockdelta_handler_matches);

static struct platform_driver exynos_ddrphylockdelta_handler_driver = {
	.probe	= exynos_ddrphylockdelta_handler_probe,
	.driver	= {
		.name	= "exynos-ddrphylockdelta-handler",
		.of_match_table	= \
			of_match_ptr(exynos_ddrphylockdelta_handler_matches),
	},
};
module_platform_driver(exynos_ddrphylockdelta_handler_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EXYNOS DDRPHYLOCKDELTA HANDLER");

