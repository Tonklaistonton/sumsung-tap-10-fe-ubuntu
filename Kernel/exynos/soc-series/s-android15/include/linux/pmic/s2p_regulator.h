/*
 * s2p_regulator.h - Driver for the Samsung SoC PMIC
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

#ifndef __LINUX_S2P_REGULATOR_H
#define __LINUX_S2P_REGULATOR_H

#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/pmic/pmic_class.h>

/**
 * s2p_regulator_data - each regulator data
 * @id: regulator id
 * @midx: matching regulator index of device tree and deivce driver
 * @initdata: regulator init data (contraints, supplies, ...)
 * @reg_node: dt node of regulator data
 */
struct s2p_regulator_data {
	int id;
	int midx;
	struct regulator_init_data *initdata;
	struct device_node *reg_node;
};

/*
 * s2p_pmic_data - pmic data for DT
 */
struct s2p_pmic_data {
	int num_rdata;
	struct s2p_regulator_data *rdata;
	bool wtsr_en;
	bool jig_reboot_en;
	bool inst_acok_en;
	int smpl_warn_vth;
	uint8_t sid;
	uint32_t pmic_src_no;
	uint16_t on_src_reg;
	uint32_t on_src_cnt;
	uint16_t off_src_reg;
	uint32_t off_src_cnt;
};

struct s2p_reg_update {
	char *name;
	uint8_t reg;
	uint8_t mask;
	uint8_t shift;
	bool en;
};

struct s2p_pmic_regulators_desc {
	uint8_t count;
	struct regulator_desc *desc;
};

struct s2p_regulator_bus {
	uint8_t sid;
	struct mutex bus_lock;
	struct device_node *bus_node;

	int (*read_reg)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, uint8_t *dest);
	int (*bulk_read)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, int count, uint8_t *buf);
	int (*write_reg)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, uint8_t value);
	int (*bulk_write)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, int count, uint8_t *buf);
	int (*update_reg)(struct device_node *np, uint8_t sid,
				uint16_t type, uint8_t reg, uint8_t value, uint8_t mask);
};

struct s2p_src_info {
	struct s2p_regulator_bus *bus_info;
	uint32_t pmic_src_no;
	uint16_t on_src_reg;
	uint32_t on_src_cnt;
	uint16_t off_src_reg;
	uint32_t off_src_cnt;
};

struct s2p_regulator_info {
	int device_type;
	int num_regulators;
	struct device *dev;
	struct regulator_dev **rdev;
	unsigned int *enmode;
	struct s2p_regulator_bus *bus_info;
	struct s2p_pmic_regulators_desc *rdesc;
	uint8_t pm_addr;
	struct mutex pm_lock;
	uint8_t enable_shift_bit;
	uint32_t regulator_num;
	struct s2p_src_info *src_info;

	/* selectable usage according to pmic */
	int wtsr_en;
	bool inst_acok_en;
	bool jig_reboot_en;

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	char sysfs_name[32];
	struct pmic_sysfs_dev *pmic_sysfs;
	int (*check_base_address)(uint8_t base_addr);
#endif
};

/* kernel ops api function */
extern int s2p_regulator_read_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, uint8_t *dest);
extern int s2p_regulator_bulk_read_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf);
extern int s2p_regulator_write_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, uint8_t val);
extern int s2p_regulator_bulk_write_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, int count, uint8_t *buf);
extern int s2p_regulator_update_reg(struct s2p_regulator_bus *bus_info, uint16_t base_addr, uint8_t reg, uint8_t val, uint8_t mask);

extern unsigned int s2p_of_map_mode(unsigned int f_mode);
extern int s2p_set_mode(struct regulator_dev *rdev, struct s2p_regulator_info *pmic_info,
			unsigned int f_mode);
extern unsigned int s2p_get_mode(struct regulator_dev *rdev, struct s2p_regulator_info *pmic_info);
extern int s2p_enable(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev);
extern int s2p_disable_regmap(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev);
extern int s2p_is_enabled_regmap(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev);
extern int s2p_get_voltage_sel_regmap(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev);
extern int s2p_set_voltage_sel_regmap(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev, unsigned sel);
extern int s2p_set_voltage_time_sel(struct s2p_regulator_info *pmic_info,
		struct regulator_dev *rdev,
		unsigned int old_selector,
		unsigned int new_selector);
extern int s2p_register_regulators(struct s2p_regulator_info *pmic_info,
		struct s2p_pmic_data *pdata);
extern struct s2p_regulator_bus* s2p_init_bus(struct device *dev);

#if IS_ENABLED(CONFIG_OF)
extern int of_s2p_regulator_parse_dt(struct device *dev,
		struct s2p_pmic_data *pdata,
		struct s2p_pmic_regulators_desc *regulators,
		struct s2p_regulator_info *pmic_info);
#endif
extern int s2p_init_src_info(struct s2p_regulator_info *pmic_info, struct s2p_pmic_data *pdata);
extern int s2p_get_pwronsrc(uint32_t pmic_no, uint8_t *arr, size_t arr_size);
extern int s2p_get_pwroffsrc(uint32_t pmic_no, uint8_t *arr, size_t arr_size);
extern int s2p_clear_pwroffsrc(uint32_t pmic_no);
extern int s2p_pmic_remove(struct s2p_regulator_info *pmic_info);

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
extern int s2p_create_sysfs(struct s2p_regulator_info *pmic_info);
extern void s2p_remove_sysfs_entries(struct device *sysfs_dev);
#endif /* CONFIG_DRV_SAMSUNG_PMIC */

#endif /*__LINUX_S2P_REGULATOR_H */
