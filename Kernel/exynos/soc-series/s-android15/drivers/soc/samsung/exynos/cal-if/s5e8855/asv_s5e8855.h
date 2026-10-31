#ifndef __ASV_EXYNOS8855_H__
#define __ASV_EXYNOS8855_H__

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kobject.h>

#define PR_INFO(fmt, ...) \
       printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)

#define ASV_TABLE_BASE	(0x10009000)
#define ID_TABLE_BASE	(0x10000000)

#define T4_ADDR		(0x1000F060)
#define T4_OFFSET	(24)
#define T4_MASK		(0xff)
#define T5_ADDR		(0x1000F064)
#define T5_OFFSET	(0)
#define T5_MASK		(0xff)
#define T6_ADDR		(0x1000F064)
#define T6_OFFSET	(8)
#define T6_MASK		(0xff)

static struct dentry *rootdir;

struct asv_tbl_info {
	unsigned bigcpu_asv_group:4;
	unsigned midcpu_asv_group:4;
	unsigned littlecpu_asv_group:4;
	unsigned g3d_asv_group:4;
	unsigned mif_asv_group:4;
	unsigned int_asv_group:4;
	unsigned cam_asv_group:4;
	unsigned npu_asv_group:4;

	unsigned cp_cpu_asv_group:4;
	unsigned cp_asv_group:4;
	unsigned dsu_asv_group:4;
	unsigned sci_asv_group:4;
	unsigned sci_modify_group:4;
	unsigned dsu_modify_group:4;
	unsigned cp_modify_group:4;
	unsigned cp_cpu_modify_group:4;

	unsigned bigcpu_modify_group:4;
	unsigned midcpu_modify_group:4;
	unsigned littlecpu_modify_group:4;
	unsigned g3d_modify_group:4;
	unsigned mif_modify_group:4;
	unsigned int_modify_group:4;
	unsigned cam_modify_group:4;
	unsigned npu_modify_group:4;

	unsigned asv_table_version:7;
	unsigned asv_method:1;
	unsigned asb_version:7;
	unsigned vtyp_modify_enable:1;
	unsigned asvg_version:4;
	unsigned product:2;
	unsigned asb_pgm_reserved:10;
};

struct id_tbl_info {
	unsigned padding;
	unsigned int chip_id_2;

	unsigned chip_id_1:13;
	unsigned reserved_0:3;
	unsigned reserved_1:8;
	unsigned reserved_2:8;

	unsigned asb_version_sub1:4;
	unsigned asb_version_sub2:4;
	unsigned asb_test_version:8;
	unsigned char reserved_4:8;
	unsigned reserved_5:8;

	unsigned char reserved_6:8;
	unsigned char reserved_7:8;
	unsigned short sub_rev:4;
	unsigned short main_rev:4;
	unsigned reserved_8:8;
	unsigned reserved_9:8;
	unsigned reserved_10:8;
	unsigned reserved_11:8;
	unsigned reserved_12:8;
};

#if IS_ENABLED(CONFIG_SHOW_ASV)
struct power_info {
	unsigned int t4;
	unsigned int t5;
	unsigned int t6;
} power_info;
#endif

#define ASV_INFO_ADDR_CNT	((int)sizeof(struct asv_tbl_info) / 4)
#define ID_INFO_ADDR_CNT	((int)sizeof(struct id_tbl_info) / 4)

static struct asv_tbl_info asv_tbl;
static struct id_tbl_info id_tbl;

int asv_get_grp(unsigned int id)
{
	int grp = -1;

	switch (id) {
	case CPUCL2:
		grp = asv_tbl.bigcpu_asv_group + asv_tbl.bigcpu_modify_group;
		break;
	case CPUCL1:
		grp = asv_tbl.midcpu_asv_group + asv_tbl.midcpu_modify_group;
		break;
	case CPUCL0:
		grp = asv_tbl.littlecpu_asv_group + asv_tbl.littlecpu_modify_group;
		break;
	case G3D:
		grp = asv_tbl.g3d_asv_group + asv_tbl.g3d_modify_group;
		break;
	case MIF:
		grp = asv_tbl.mif_asv_group + asv_tbl.mif_modify_group;
		break;
	case INT:
		grp = asv_tbl.int_asv_group + asv_tbl.int_modify_group;
		break;
	case CAM:
		grp = asv_tbl.cam_asv_group + asv_tbl.cam_modify_group;
		break;
	case NPU:
		grp = asv_tbl.npu_asv_group + asv_tbl.npu_modify_group;
		break;
	case CP_CPU:
		grp = asv_tbl.cp_cpu_asv_group + asv_tbl.cp_cpu_modify_group;
		break;
	case CP:
		grp = asv_tbl.cp_asv_group + asv_tbl.cp_modify_group;
		break;
	case DSU:
		grp = asv_tbl.dsu_asv_group + asv_tbl.dsu_modify_group;
		break;
	case INTSCI:
		grp = asv_tbl.sci_asv_group + asv_tbl.sci_modify_group;
		break;
	default:
		PR_INFO("Un-support asv grp %d\n", id);
	}
	return grp;
}

int asv_get_ids_info(unsigned int id)
{
	int ids = 0;

	switch (id) {
	default:
		PR_INFO("Un-support ids info %d\n", id);
	}

	return ids;
}

int asv_get_table_ver(void)
{
	return asv_tbl.asv_table_version;
}

void id_get_rev(unsigned int *main_rev, unsigned int *sub_rev)
{
#if IS_ENABLED(CONFIG_ACPM_DVFS)
	*main_rev = id_tbl.main_rev;
	*sub_rev =  id_tbl.sub_rev;
#endif
	*main_rev = 0;
	*sub_rev = 0;
}

int id_get_product_line(void)
{
	return id_tbl.chip_id_1 >> 10;
} EXPORT_SYMBOL(id_get_product_line);


int id_get_asb_ver(void)
{
	return id_tbl.asb_test_version + id_tbl.asb_version_sub1 + id_tbl.asb_version_sub2;
} EXPORT_SYMBOL(id_get_asb_ver);

#if IS_ENABLED(CONFIG_SHOW_ASV)
int print_asv_table(char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf + size, "chipid : 0x%04x%08x\n", id_tbl.chip_id_1, id_tbl.chip_id_2);
	size += sprintf(buf + size, "main revision : %d\n", id_tbl.main_rev);
	size += sprintf(buf + size, "sub revision : %d\n", id_tbl.sub_rev);
	size += sprintf(buf + size, "\n");
	size += sprintf(buf + size, "  asv_table_version : %d\n", asv_get_table_ver());
	size += sprintf(buf + size, "\n");
	size += sprintf(buf + size, "  big cpu grp : %d\n", asv_get_grp(CPUCL2));
	size += sprintf(buf + size, "  mid cpu grp : %d\n", asv_get_grp(CPUCL1));
	size += sprintf(buf + size, "  little cpu grp : %d\n", asv_get_grp(CPUCL0));
	size += sprintf(buf + size, "  g3d grp : %d\n", asv_get_grp(G3D));
	size += sprintf(buf + size, "  mif grp : %d\n", asv_get_grp(MIF));
	size += sprintf(buf + size, "  int grp : %d\n", asv_get_grp(INT));
	size += sprintf(buf + size, "  sci grp : %d\n", asv_get_grp(INTSCI));
	size += sprintf(buf + size, "  cam grp : %d\n", asv_get_grp(CAM));
	size += sprintf(buf + size, "  npu grp : %d\n", asv_get_grp(NPU));
	size += sprintf(buf + size, "  cp_cpu grp : %d\n", asv_get_grp(CP_CPU));
	size += sprintf(buf + size, "  cp grp : %d\n", asv_get_grp(CP));
	size += sprintf(buf + size, "  dsu grp : %d\n", asv_get_grp(DSU));
	size += sprintf(buf + size, "\n");
	size += sprintf(buf + size, "  asb_version : %d\n", id_get_asb_ver());
	size += sprintf(buf + size, "\n");

	return size;
}

static ssize_t
asv_info_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *buf;
	int r;

	buf = kzalloc(sizeof(char) * 2048, GFP_KERNEL);
	r = print_asv_table(buf);

	r = simple_read_from_buffer(ubuf, cnt, ppos, buf, r);

	kfree(buf);

	return r;
}

static const struct file_operations asv_info_fops = {
	.open		= simple_open,
	.read		= asv_info_read,
	.llseek		= seq_lseek,
};

static ssize_t show_t4(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_info.t4);
}
static struct kobj_attribute t4 =
__ATTR(t4, 0444, show_t4, NULL);

static ssize_t show_t5(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_info.t5);
}
static struct kobj_attribute t5 =
__ATTR(t5, 0444, show_t5, NULL);

static ssize_t show_t6(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", power_info.t6);
}
static struct kobj_attribute t6 =
__ATTR(t6, 0444, show_t6, NULL);

static ssize_t show_asb_ver(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", id_get_asb_ver());
}
static struct kobj_attribute asb_ver =
__ATTR(asb_ver, 0444, show_asb_ver, NULL);

static struct attribute *asv_info_attrs[] = {
	&t4.attr,
	&t5.attr,
	&t6.attr,
	&asb_ver.attr,
	NULL,
};

static const struct attribute_group asv_info_grp = {
	.attrs = asv_info_attrs,
};
#endif

#if IS_ENABLED(CONFIG_SHOW_ASV)
int asv_debug_init(void)
{
	struct dentry *d;
	struct kobject *kobj;

	rootdir = debugfs_create_dir("asv", NULL);
	if (!rootdir)
		return -ENOMEM;

	d = debugfs_create_file("asv_info", 0600, rootdir, NULL, &asv_info_fops);
	if (!d)
		return -ENOMEM;

	kobj = kobject_create_and_add("asv", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create asv kboject\n");

	if (sysfs_create_group(kobj, &asv_info_grp))
		pr_err("Fail to create asv_info group\n");

	return 0;
}
#endif

int asv_table_init(void)
{
#if IS_ENABLED(CONFIG_ACPM_DVFS)
	int i;
	unsigned int *p_table;
	void __iomem *regs;
	unsigned long tmp;

	p_table = (unsigned int *)&asv_tbl;

	for (i = 0; i < ASV_INFO_ADDR_CNT; i++) {
		exynos_smc_readsfr((unsigned long)(ASV_TABLE_BASE + 0x4 * i), &tmp);
		*(p_table + i) = (unsigned int)tmp;
	}

	p_table = (unsigned int *)&id_tbl;
	regs = ioremap(ID_TABLE_BASE, ID_INFO_ADDR_CNT * sizeof(int));
	for (i = 0; i < ID_INFO_ADDR_CNT; i++)
		*(p_table + i) = __raw_readl(regs + 0x4 * i);

#if IS_ENABLED(CONFIG_SHOW_ASV)

	regs = ioremap(T4_ADDR, sizeof(int));
	tmp = __raw_readl(regs);
	power_info.t4 = (tmp >> T4_OFFSET) & T4_MASK;
	regs = ioremap(T5_ADDR, sizeof(int));
	tmp = __raw_readl(regs);
	power_info.t5 = (tmp >> T5_OFFSET) & T5_MASK;
	regs = ioremap(T6_ADDR, sizeof(int));
	tmp = __raw_readl(regs);
	power_info.t6 = (tmp >> T6_OFFSET) & T6_MASK;

	asv_debug_init();
#endif

	return asv_tbl.asv_table_version;
#endif
	return 0;
}
#endif
