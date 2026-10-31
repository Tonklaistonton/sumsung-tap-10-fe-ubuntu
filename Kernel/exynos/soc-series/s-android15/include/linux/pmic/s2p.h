/*
 * s2p.h - Driver for the s2p
 *
 *  Copyright (C) 2023 Samsung Electrnoics
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

#ifndef __S2P_MFD_H__
#define __S2P_MFD_H__
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#define S2P_PMIC_NUM		(30)

struct s2p_irq_handler_ops {
	struct device *dev;

	int (*handler)(struct device *dev, uint8_t *vgi_src);
};

struct s2p_irq_dev {
	struct device *dev;

	/* IRQ */
	bool wakeup;
	int irq;

	uint8_t *vgi_src;
	uint8_t vgi_cnt;

	/* VGPIO_RX_MONITOR */
	void __iomem *mem_base;
	void __iomem *mem_base2;

	/* VGPIO_SYSREG */
	void __iomem *sysreg_vgpio2ap;
	void __iomem *sysreg_vgpio2pmu;

	int spmi_master_pmic;
	int vgpio_monitor;
	int vgpio_monitor2;
	int intcomb_vgpio2ap;
	int intcomb_vgpio2pmu;
	int intc0_ipend;
};

struct s2p_platform_data {
	struct device_node *bus_node;

	bool wakeup;
	uint8_t sid;
	int irq_base;
};

struct s2p_pmic_rev {
	uint8_t pmic_rev;
	uint8_t pmic_sw_rev;
	uint8_t pmic_hw_rev;
	uint8_t platform_rev;
};

struct s2p_reg_data {
	uint8_t base;
	uint8_t reg;
	uint8_t val;
};

enum s2p_irq_type {
	S2P_OCP,
	S2P_OI,
	S2P_TEMP,
	S2P_OVP,
};

struct s2p_ocp_oi_data {
	const char *name;
	int cnt;
	int irq_num;
	enum s2p_irq_type irq_type;
};

#define S2P_NAME_MAX	50
struct s2p_irq_info {
	char name[S2P_NAME_MAX];
	uint8_t count;
	uint32_t *irq_id;
	uint32_t *irq_cnt;
	char (*irq_name)[S2P_NAME_MAX];
	char (*irq_noti_name)[S2P_NAME_MAX];
	enum s2p_irq_type *irq_type;
	struct mutex lock;

	struct s2p_ocp_oi_data ocp_oi_data;
	struct delayed_work notifier_work;
	struct workqueue_struct *notifier_wqueue;
};

struct s2p_pmic_irq_list {
	uint8_t irq_type_cnt;
	struct s2p_irq_info *irqs;
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	char sysfs_name[S2P_NAME_MAX];
	struct device *irq_sysfs_dev;
#endif
};

struct s2p_dev {
	struct device *dev;
	struct s2p_platform_data *pdata;
	struct s2p_pmic_rev *rev_id;
	struct mutex bus_lock;
	struct device_node *bus_node;

	uint32_t irq_base;
	uint32_t irq_base_count;
	uint8_t sid;

	struct s2p_pmic_irq_list *irq_list;

	bool wakeup;
	int type;
	int device_type;

	int (*exynos_read_reg)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, uint8_t *dest);
	int (*exynos_bulk_read)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, int count, uint8_t *buf);
	int (*exynos_write_reg)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, uint8_t value);
	int (*exynos_bulk_write)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, int count, uint8_t *buf);
	int (*exynos_update_reg)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, uint8_t value, uint8_t mask);
};

extern int s2p_read_reg(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, uint8_t *dest);
extern int s2p_bulk_read(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf);
extern int s2p_write_reg(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, uint8_t value);
extern int s2p_bulk_write(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf);
extern int s2p_update_reg(struct s2p_dev *sdev, uint16_t base_addr, uint8_t reg, uint8_t val, uint8_t mask);

extern int s2p_init(struct device *dev, struct s2p_dev *sdev);
extern void s2p_remove(struct s2p_dev *sdev);

extern int s2p_set_registers(struct s2p_dev *sdev, const struct s2p_reg_data *reg_info,
		unsigned int reg_count);
extern int s2p_allocate_pmic_irq(struct device *dev,
		struct s2p_irq_info *pm_irq_info, const uint32_t type_size);
extern int s2p_create_irq_notifier_wq(struct device *dev, struct s2p_irq_info *pm_irq_info,
		const uint32_t type_size, const char *mfd_dev_name, const uint32_t dev_id);
extern irqreturn_t s2p_handle_regulator_irq(int irq, void *data);

extern void s2p_destroy_irq_list_mutex(struct s2p_pmic_irq_list *irq_list);

extern int s2p_register_irq_handler(struct device *dev, void *handler);
extern int s2p_create_irq_sysfs(struct s2p_dev *sdev, const char *mfd_dev_name, const uint32_t dev_id);
extern void s2p_destroy_workqueue(struct s2p_irq_info *pm_irq_info, const uint32_t type_size);
extern void s2p_remove_irq_sysfs_entries(struct device *sysfs_dev);
extern int s2p_register_ocp_oi_notifier(struct notifier_block *nb);
#endif /* __S2P_MFD_H__ */
