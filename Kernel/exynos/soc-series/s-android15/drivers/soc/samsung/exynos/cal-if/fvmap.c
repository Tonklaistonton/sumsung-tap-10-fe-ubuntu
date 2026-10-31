#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <soc/samsung/cal-if.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <soc/samsung/fvmap.h>

#include "cmucal.h"
#include "vclk.h"
#include "ra.h"

#define FVMAP_SIZE		(SZ_8K)
#define STEP_UV			(6250)

void __iomem *fvmap_base;
void __iomem *sram_fvmap_base;

#define MARGIN_NR (10)
#define MARGIN_NR_ARGS (4)
#define MARGIN_GRANULARITY (6250)

static int margin_args[MARGIN_NR][MARGIN_NR_ARGS];
module_param_array_named(margin_0, margin_args[0], int, NULL, 0);
module_param_array_named(margin_1, margin_args[1], int, NULL, 0);
module_param_array_named(margin_2, margin_args[2], int, NULL, 0);
module_param_array_named(margin_3, margin_args[3], int, NULL, 0);
module_param_array_named(margin_4, margin_args[4], int, NULL, 0);
module_param_array_named(margin_5, margin_args[5], int, NULL, 0);
module_param_array_named(margin_6, margin_args[6], int, NULL, 0);
module_param_array_named(margin_7, margin_args[7], int, NULL, 0);
module_param_array_named(margin_8, margin_args[8], int, NULL, 0);
module_param_array_named(margin_9, margin_args[9], int, NULL, 0);

int fvmap_set_raw_voltage_table(unsigned int id, int uV)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header fv_table;
	int num_of_lv;
	int idx, i;

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table.table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		fv_table.table[i].volt += uV / STEP_UV;

	return 0;
}

static ssize_t show_margin(struct kobject *kobj,
			   struct kobj_attribute *attr, char *buf)
{
	int size, i;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE,
			"%11s | %10s | %10s | %10s | %10s\n",
			"Name", "Index", "Lower", "Upper", "Margin");

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);
	for (i = 0; i < size; i++) {
		struct vclk *vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		u32 vclk_id;

		if (vclk == NULL)
			continue;

		vclk_id = vclk->id & MASK_OF_ID;

		len += snprintf(buf + len, PAGE_SIZE,
			"%11s | %10d | %10d | %10d | %10d\n",
			vclk->name, vclk_id, vclk->margin_lower,
			vclk->margin_upper, vclk->margin_val);
	}

	return len;
}

static int fvmap_set_margin(int id, int lower, int upper, int margin)
{
	int size, i, ret;

	if (margin >= MARGIN_GRANULARITY || margin <= -MARGIN_GRANULARITY) {
		margin /= MARGIN_GRANULARITY;
		margin *= MARGIN_GRANULARITY;
	}

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);
	for (i = 0; i < size; i++) {
		struct vclk *vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		u32 vclk_id;

		if (vclk == NULL)
			continue;

		vclk_id = vclk->id & MASK_OF_ID;

		if (id != -1 && id != vclk_id)
			continue;

		/* Set margin */
		ret = cal_dfs_set_volt_margin(vclk_id, lower, upper, margin);
		if (ret) {
			pr_err("%s: failed to set margin (domain: %s lower: %d, upper: %d, margin: %d)\n",
				__func__, vclk->name, lower, upper, margin);
			continue;
		}

		pr_auto(ASL5, "%s: domain: %s lower: %d, upper: %d, margin: %d\n",
			__func__, vclk->name, lower, upper, margin);

		vclk->margin_lower = lower;
		vclk->margin_upper = upper;
		vclk->margin_val = margin;
	}

	return 0;
}

static ssize_t store_margin(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char temp_buf[64];
	int id, lower, upper, margin, ret;

	strncpy(temp_buf, buf, sizeof(temp_buf));

	ret = sscanf(buf, "%d %d %d %d", &id, &lower, &upper, &margin);
	if (ret != 4)
		return -EINVAL;

	fvmap_set_margin(id, lower, upper, margin);

	return count;
}

static struct kobj_attribute kobj_attr_margin =
		__ATTR(margin, 0600, show_margin, store_margin);

static struct attribute *margin_attrs[] = {
	&kobj_attr_margin.attr,
	NULL,
};

static const struct attribute_group margin_group = {
	.attrs = margin_attrs,
};

unsigned int dvfs_calibrate_voltage(unsigned int rate_target, unsigned int rate_up,
		unsigned int rate_down, unsigned int volt_up, unsigned int volt_down)
{
	unsigned int volt_diff_step;
	unsigned int rate_diff;
	unsigned int rate_per_step;
	unsigned int ret;

	if (rate_up < 0x100)
		return volt_down * STEP_UV;

	if (rate_down == 0)
		return volt_up * STEP_UV;

	if ((rate_up == rate_down) || (volt_up == volt_down))
		return volt_up * STEP_UV;

	volt_diff_step = (volt_up - volt_down);
	rate_diff = rate_up - rate_down;
	rate_per_step = rate_diff / volt_diff_step;
	ret = (unsigned int)((volt_up - ((rate_up - rate_target) / rate_per_step)) + 0) * STEP_UV;

	return ret;
}

int fvmap_get_freq_volt_table(unsigned int id, void *freq_volt_table, unsigned int table_size)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header fv_table;
	int idx, i, j;
	int num_of_lv;
	unsigned int target, rate_up, rate_down, volt_up, volt_down;
	struct freq_volt *table = (struct freq_volt *)freq_volt_table;

	if (!IS_ACPM_VCLK(id))
		return -EINVAL;

	if (!table)
		return -ENOMEM;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table.table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < table_size; i++) {
		for (j = 1; j < num_of_lv; j++) {
			rate_up = fv_table.table[j - 1].rate;
			rate_down = fv_table.table[j].rate;
			if ((table[i].rate <= rate_up) && (table[i].rate >= rate_down)) {
				volt_up = fv_table.table[j - 1].volt;
				volt_down = fv_table.table[j].volt;
				target = table[i].rate;
				table[i].volt = dvfs_calibrate_voltage(target,
						rate_up, rate_down, volt_up, volt_down);
				break;
			}
		}

		if (table[i].volt == 0) {
			if (table[i].rate > fv_table.table[0].rate)
				table[i].volt = fv_table.table[0].volt * STEP_UV;
			else if (table[i].rate < fv_table.table[num_of_lv - 1].rate)
				table[i].volt = fv_table.table[num_of_lv - 1].volt * STEP_UV;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fvmap_get_freq_volt_table);

int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header fv_table;
	int idx, i;
	int num_of_lv;

	if (!IS_ACPM_VCLK(id))
		return 0;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table.table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table.table[i].volt * STEP_UV;

	return num_of_lv;

}
EXPORT_SYMBOL_GPL(fvmap_get_voltage_table);

int fvmap_get_raw_voltage_table(unsigned int id)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header fv_table;
	int idx, i;
	int num_of_lv;
	unsigned int table[20];

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table.table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table.table[i].volt * STEP_UV;

	for (i = 0; i < num_of_lv; i++)
		printk("dvfs id : %d  %d Khz : %d uv\n", ACPM_VCLK_TYPE | id, fv_table.table[i].rate, table[i]);

	return 0;
}

static int fvmap_apply_booting_margin(void)
{
	int i;
	for (i = 0; i < MARGIN_NR; i++) {
		int *arg = &margin_args[i][0];

		if (!arg[0] && !arg[1] && !arg[2] && !arg[3])
			continue;

		fvmap_set_margin(arg[0], arg[1], arg[2], arg[3]);
	}

	return 0;
}

static void fvmap_copy_from_sram(void __iomem *map_base, void __iomem *sram_base)
{
	struct fvmap_header *fvmap_header, *header;
	struct rate_volt_header old, new;
	struct clocks clks;
	struct pll_header *plls;
	struct vclk *vclk;
	unsigned int member_addr;
	unsigned int blk_idx;
	int size;
	int i, j;

	fvmap_header = map_base;
	header = vmalloc(FVMAP_SIZE);
	memcpy(header, sram_base, FVMAP_SIZE);

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	for (i = 0; i < size; i++) {
		/* load fvmap info */
		fvmap_header[i].domain_id = header[i].domain_id;
		fvmap_header[i].num_of_lv = header[i].num_of_lv;
		fvmap_header[i].num_of_members = header[i].num_of_members;
		fvmap_header[i].num_of_pll = header[i].num_of_pll;
		fvmap_header[i].num_of_mux = header[i].num_of_mux;
		fvmap_header[i].num_of_div = header[i].num_of_div;
		fvmap_header[i].o_famrate = header[i].o_famrate;
		fvmap_header[i].init_lv = header[i].init_lv;
		fvmap_header[i].num_of_child = header[i].num_of_child;
		fvmap_header[i].parent_id = header[i].parent_id;
		fvmap_header[i].parent_offset = header[i].parent_offset;
		fvmap_header[i].block_addr[0] = header[i].block_addr[0];
		fvmap_header[i].block_addr[1] = header[i].block_addr[1];
		fvmap_header[i].block_addr[2] = header[i].block_addr[2];
		fvmap_header[i].o_members = header[i].o_members;
		fvmap_header[i].o_ratevolt = header[i].o_ratevolt;
		fvmap_header[i].o_tables = header[i].o_tables;

		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;
#ifdef CONFIG_EXYNOS_DEBUG_INFO
		pr_info("domain_id : %s - id : %x\n",
			vclk->name, fvmap_header[i].domain_id);
		pr_info("  num_of_lv      : %d\n", fvmap_header[i].num_of_lv);
		pr_info("  num_of_members : %d\n", fvmap_header[i].num_of_members);
#endif
		old.table = sram_base + fvmap_header[i].o_ratevolt;
		new.table = map_base + fvmap_header[i].o_ratevolt;

		for (j = 0; j < fvmap_header[i].num_of_members; j++) {
			clks.addr = sram_base + fvmap_header[i].o_members;

			if (j < fvmap_header[i].num_of_pll) {
				plls = sram_base + clks.addr[j];
				member_addr = plls->addr - 0x90000000;
			} else {

				member_addr = (clks.addr[j] & ~0x3) & 0xffff;
				blk_idx = clks.addr[j] & 0x3;

				member_addr |= ((fvmap_header[i].block_addr[blk_idx]) << 16) - 0x90000000;
			}

			vclk->list[j] = cmucal_get_id_by_addr(member_addr);
#ifdef CONFIG_EXYNOS_DEBUG_INFO
			if (vclk->list[j] == INVALID_CLK_ID)
				pr_info("  Invalid addr :0x%x\n", member_addr);
			else
				pr_info("  DVFS CMU addr:0x%x\n", member_addr);
#endif
		}
#ifdef CONFIG_EXYNOS_DEBUG_INFO
		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			new.table[j].rate = old.table[j].rate;
			new.table[j].volt = old.table[j].volt;
			pr_info("  lv : [%7d], volt = %d uV \n",
				new.table[j].rate, new.table[j].volt * STEP_UV);
		}
#endif
	}

	vfree((void *)header);
}

int fvmap_init(void __iomem *sram_base)
{
	void __iomem *map_base;
	struct kobject *kobj;

	map_base = kzalloc(FVMAP_SIZE, GFP_KERNEL);

	fvmap_base = map_base;
	sram_fvmap_base = sram_base;
	pr_info("%s:fvmap initialize %llx\n", __func__, (u64)sram_base);
	fvmap_copy_from_sram(map_base, sram_base);
	fvmap_apply_booting_margin();

	/* margin for each doamin at runtime */
	kobj = kobject_create_and_add("margin", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create margin kboject\n");

	if (sysfs_create_group(kobj, &margin_group))
		pr_err("Fail to create margin group\n");

	kobj = kobject_create_and_add("asv-g", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create asv-g kboject\n");

	if (sysfs_create_group(kobj, &asv_g_spec_grp))
		pr_err("Fail to create asv_g_spec group\n");

	return 0;
}
EXPORT_SYMBOL_GPL(fvmap_init);

MODULE_LICENSE("GPL");
