/*
 * s2mpu16-irq.c - Interrupt controller support for S2MPU16
 *
 * Copyright (C) 2024 Samsung Electronics Co.Ltd
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

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/pmic/s2mpu16-mfd.h>
#include <linux/input.h>

static const uint8_t s2mpu16_mask_reg[] = {
	[S2MPU16_PMIC_INT1] = S2MPU16_PM1_INT1M,
	[S2MPU16_PMIC_INT2] = S2MPU16_PM1_INT2M,
	[S2MPU16_PMIC_INT3] = S2MPU16_PM1_INT3M,
	[S2MPU16_PMIC_INT4] = S2MPU16_PM1_INT4M,
	[S2MPU16_PMIC_INT5] = S2MPU16_PM1_INT5M,
	[S2MPU16_PMIC_INT6] = S2MPU16_PM1_INT6M,
	[S2MPU16_PMIC_INT7] = S2MPU16_PM1_INT7M,
};

static int s2mpu16_get_base_addr(struct s2mpu16_dev *s2mpu16,
			enum s2mpu16_irq_source src)
{
	switch (src) {
	case S2MPU16_PMIC_INT1 ... S2MPU16_PMIC_INT7:
		return s2mpu16->pmic1;
	default:
		return -EINVAL;
	}
}

struct s2mpu16_irq_data {
	int mask;
	enum s2mpu16_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mpu16_irq_data s2mpu16_irqs[] = {
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_PWRONR_INT1,		S2MPU16_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_PWRONF_INT1,		S2MPU16_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_INT120C_INT1,		S2MPU16_PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_INT140C_INT1,		S2MPU16_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_TSD_INT1,			S2MPU16_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_WTSR_INT1,			S2MPU16_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_WRSTB_INT1,		S2MPU16_PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_TX_TRAN_FAIL_INT1,		S2MPU16_PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B1S_INT2,		S2MPU16_PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B2S_INT2,		S2MPU16_PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B3S_INT2,		S2MPU16_PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B4S_INT2,		S2MPU16_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B5S_INT2,		S2MPU16_PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B6S_INT2,		S2MPU16_PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B7S_INT2,		S2MPU16_PMIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B8S_INT2,		S2MPU16_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B9S_INT3,		S2MPU16_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_B10S_INT3,		S2MPU16_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OCP_SR1S_INT3,		S2MPU16_PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_UV_BB_INT3,		S2MPU16_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_BB_NTR_DET_INT3,		S2MPU16_PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B1S_INT4,		S2MPU16_PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B2S_INT4,		S2MPU16_PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B3S_INT4,		S2MPU16_PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B4S_INT4,		S2MPU16_PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B5S_INT4,		S2MPU16_PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B6S_INT4,		S2MPU16_PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B7S_INT4,		S2MPU16_PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B8S_INT4,		S2MPU16_PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B9S_INT5,		S2MPU16_PMIC_INT5, 1 << 0),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_B10S_INT5,		S2MPU16_PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OI_SR1S_INT5,		S2MPU16_PMIC_INT5, 1 << 4),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_OVP_INT5,			S2MPU16_PMIC_INT5, 1 << 6),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_AUTO_EXIT_INT5,		S2MPU16_PMIC_INT5, 1 << 7),

	DECLARE_IRQ(S2MPU16_PMIC_IRQ_LDO13_SCP_INT6,		S2MPU16_PMIC_INT6, 1 << 0),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_CHECKSUM_INT6,		S2MPU16_PMIC_INT6, 1 << 3),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_PARITY_ERR_DATA_INT6,	S2MPU16_PMIC_INT6, 1 << 4),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_PARITY_ERR_ADDR_L_INT6,	S2MPU16_PMIC_INT6, 1 << 5),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_PARITY_ERR_ADDR_H_INT6,	S2MPU16_PMIC_INT6, 1 << 6),
	DECLARE_IRQ(S2MPU16_PMIC_IRQ_PARITY_ERR_CMD_INT6,	S2MPU16_PMIC_INT6, 1 << 7),

	DECLARE_IRQ(S2MPU16_PMIC_IRQ_SPMI_LDO_OK_F_INT7,	S2MPU16_PMIC_INT7, 1 << 0),

};

enum s2mpu16_irq_type {
	S2MPU16_OCP,
	S2MPU16_OI,
	S2MPU16_TEMP,
	S2MPU16_IRQ_TYPE_CNT,
};

#define DECLARE_OI(_base, _reg, _val) {.base = (_base), .reg = (_reg), .val = (_val) }
static const struct s2p_reg_data s2mpu16_oi_ocp[] = {
	/* BUCK 1S to 10S, SR1(BB), OI,OCP function enable, OI power down,OVP disable */
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK1S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK2S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK3S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK4S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK5S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK6S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK7S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK8S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK9S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_BUCK10S_OCP, 0xCC),
	DECLARE_OI(S2MPU16_PMIC1_ADDR, S2MPU16_PM1_SR1S_OCP, 0xCC),
};

static void s2mpu16_irq_lock(struct irq_data *data)
{
	struct s2mpu16_dev *s2mpu16 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpu16->irq_lock);
}

static void s2mpu16_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpu16_dev *s2mpu16 = irq_get_chip_data(data->irq);
	uint32_t i = 0;

	for (i = 0; i < S2MPU16_IRQ_GROUP_NR; i++) {
		uint8_t mask_reg = s2mpu16_mask_reg[i];
		int base_addr = s2mpu16_get_base_addr(s2mpu16, i);

		if (mask_reg == 0xff || base_addr == -EINVAL)
			continue;
		s2mpu16->irq_masks_cache[i] = s2mpu16->irq_masks_cur[i];

		s2p_write_reg(s2mpu16->sdev, base_addr,
				s2mpu16_mask_reg[i], s2mpu16->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpu16->irq_lock);
}

static const inline struct s2mpu16_irq_data *
irq_to_s2mpu16_irq(struct s2mpu16_dev *s2mpu16, int irq)
{
	return &s2mpu16_irqs[irq - s2mpu16->sdev->irq_base];
}

static inline void s2mpu16_set_irq_mask(struct irq_data *data, bool mask,
						const char *func_name)
{
	struct s2mpu16_dev *s2mpu16 = irq_get_chip_data(data->irq);
	const struct s2mpu16_irq_data *irq_data = irq_to_s2mpu16_irq(s2mpu16, data->irq);

	if (irq_data->group >= S2MPU16_IRQ_GROUP_NR) {
		dev_err(s2mpu16->dev, "%s: irq_data groups exceeded (group num: %d)\n",
				func_name, irq_data->group);
		return;
	}

	if (mask)
		s2mpu16->irq_masks_cur[irq_data->group] |= irq_data->mask;
	else
		s2mpu16->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static void s2mpu16_irq_mask(struct irq_data *data)
{
	s2mpu16_set_irq_mask(data, true, __func__);
}

static void s2mpu16_irq_unmask(struct irq_data *data)
{
	s2mpu16_set_irq_mask(data, false, __func__);
}

static struct irq_chip s2mpu16_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpu16_irq_lock,
	.irq_bus_sync_unlock	= s2mpu16_irq_sync_unlock,
	.irq_mask		= s2mpu16_irq_mask,
	.irq_unmask		= s2mpu16_irq_unmask,
};

static void s2mpu16_report_irq(struct s2mpu16_dev *s2mpu16)
{
	uint32_t i = 0;

	/* Apply masking */
	for (i = 0; i < S2MPU16_IRQ_GROUP_NR; i++)
		s2mpu16->irq_reg[i] &= ~s2mpu16->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPU16_IRQ_NR; i++)
		if (s2mpu16->irq_reg[s2mpu16_irqs[i].group] & s2mpu16_irqs[i].mask)
			handle_nested_irq(s2mpu16->sdev->irq_base + i);
}

static int s2mpu16_enable_irq(struct s2mpu16_dev *s2mpu16, const bool en)
{
	return s2p_update_reg(s2mpu16->sdev, S2MPU16_COMMON_ADDR, S2MPU16_COMMON_TX_MASK,
			(en) ? 0 : 0x1, 0x1);
}

static int s2mpu16_irq_handler(struct device *dev, uint8_t *vgi_src)
{
	struct s2mpu16_dev *s2mpu16 = dev_get_drvdata(dev);
	uint8_t *irq_reg = NULL;
	int ret = 0;

	if (!vgi_src || !s2mpu16)
		return -ENODEV;

	if (!(vgi_src[1] & S2MPU16_VGI0_IRQ_S1))
		return 0;

	irq_reg = s2mpu16->irq_reg;

	ret = s2mpu16_enable_irq(s2mpu16, false);
	if (ret)
		return ret;

	ret = s2p_bulk_read(s2mpu16->sdev, s2mpu16->pmic1, S2MPU16_PM1_INT1,
			S2MPU16_IRQ_GROUP_NR, &irq_reg[S2MPU16_PMIC_INT1]);
	if (ret) {
		dev_err(dev, "[SUB_PMIC] %s: Failed to read pmic interrupt: %d\n", __func__, ret);
		return ret;
	}

	ret = s2mpu16_enable_irq(s2mpu16, true);
	if (ret)
		return ret;

	dev_info(dev, "[SUB_PMIC] %s: %#hhx %#hhx %#hhx %#hhx %#hhx\n", __func__,
			irq_reg[0], irq_reg[1], irq_reg[2], irq_reg[3], irq_reg[4]);

	s2mpu16_report_irq(s2mpu16);

	return 0;
}

static void s2mpu16_mask_interrupt(struct s2mpu16_dev *s2mpu16)
{
	uint32_t i = 0;

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPU16_IRQ_GROUP_NR; i++) {
		int base_addr = 0;

		s2mpu16->irq_masks_cur[i] = 0xff;
		s2mpu16->irq_masks_cache[i] = 0xff;

		base_addr = s2mpu16_get_base_addr(s2mpu16, i);
		if (base_addr == -EINVAL)
			continue;

		if (s2mpu16_mask_reg[i] == 0xff)
			continue;

		s2p_write_reg(s2mpu16->sdev, base_addr, s2mpu16_mask_reg[i], 0xff);
	}
}

static void s2mpu16_register_genirq(struct s2mpu16_dev *s2mpu16, const int irq_base)
{
	uint32_t i = 0;

	for (i = 0; i < S2MPU16_IRQ_NR; i++) {
		uint32_t cur_irq = i + irq_base;

		irq_set_chip_data(cur_irq, s2mpu16);
		irq_set_chip_and_handler(cur_irq, &s2mpu16_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, true);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}
}

static int s2mpu16_set_regulator_interrupt(struct s2mpu16_dev *s2mpu16, const uint32_t irq_base)
{
	struct s2p_dev *sdev = s2mpu16->sdev;
	struct device *dev = sdev->dev;
	struct s2p_irq_info *pmic_irq = NULL;
	struct s2p_irq_info *pm_irq_info = NULL;
	const char int_type_name[S2MPU16_IRQ_TYPE_CNT][10] = { "OCP", "OI", "TEMP" };
	const int int_type_count[S2MPU16_IRQ_TYPE_CNT] = { S2MPU16_BUCK_MAX, S2MPU16_BUCK_MAX, S2MPU16_TEMP_MAX };
	const char temp_int_type[S2MPU16_TEMP_MAX][10] = { "120C", "140C" };
	struct s2p_pmic_irq_list *irq_list = NULL;
	uint32_t i = 0;
	int ret = 0;

	if (!irq_base) {
		dev_err(dev, "%s: Failed to get irq base: %u\n", __func__, irq_base);
		return -ENODEV;
	}

	dev_info(dev, "[SUB PMIC] %s: start, irq_base: %u\n", __func__, irq_base);

	pm_irq_info = devm_kzalloc(dev, S2MPU16_IRQ_TYPE_CNT * sizeof(struct s2p_irq_info), GFP_KERNEL);
	if (!pm_irq_info)
		return -ENOMEM;

	for (i = 0; i < S2MPU16_IRQ_TYPE_CNT; i++) {
		strcpy(pm_irq_info[i].name, int_type_name[i]);
		pm_irq_info[i].count = int_type_count[i];
	}

	ret = s2p_allocate_pmic_irq(dev, pm_irq_info, S2MPU16_IRQ_TYPE_CNT);
	if (ret < 0)
		return ret;

	ret = s2p_create_irq_notifier_wq(dev, pm_irq_info, S2MPU16_IRQ_TYPE_CNT, MFD_DEV_NAME, 0);
	if (ret < 0)
		return ret;

	irq_list = devm_kzalloc(dev, sizeof(struct s2p_pmic_irq_list), GFP_KERNEL);
	if (!irq_list)
		return -ENOMEM;

	irq_list->irq_type_cnt = S2MPU16_IRQ_TYPE_CNT;
	irq_list->irqs = pm_irq_info;
	sdev->irq_list = irq_list;

	/* BUCK 1M to 8M, SR1M to SR4M OCP interrupt */
	pmic_irq = &pm_irq_info[S2MPU16_OCP];
	for (i = 0; i < pmic_irq->count; i++) {
		pmic_irq->irq_id[i] = irq_base + S2MPU16_PMIC_IRQ_OCP_B1S_INT2 + i;
		pmic_irq->irq_type[i] = S2P_OCP;

		/* Dynamic allocation for device name */
		if (i < BUCK_SR1S_IDX) {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BUCK%dS_%s_IRQ%d@%s", i + 1, pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BUCK%dS_%s_IRQ", i + 1, pmic_irq->name);
		}
		else {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BUCK_SR%dS_%s_IRQ%d@%s", i + 1 - BUCK_SR1S_IDX,
				pmic_irq->name, pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BUCK_SR%dS_%s_IRQ", i + 1 - BUCK_SR1S_IDX, pmic_irq->name);
		}
		ret = devm_request_threaded_irq(dev, pmic_irq->irq_id[i], NULL,
						s2p_handle_regulator_irq, 0,
						pmic_irq->irq_name[i], pmic_irq);
		if (ret < 0) {
			dev_err(dev, "Failed to request %s: %d, errono: %d\n",
				pmic_irq->irq_name[i], pmic_irq->irq_id[i], ret);
			goto err;
		}
	}

	/* BUCK 1M to 8M, SR1M to SR4M OI interrupt */
	pmic_irq = &pm_irq_info[S2MPU16_OI];
	for (i = 0; i < pmic_irq->count; i++) {
		pmic_irq->irq_id[i] = irq_base + S2MPU16_PMIC_IRQ_OI_B1S_INT4 + i;
		pmic_irq->irq_type[i] = S2P_OI;

		/* Dynamic allocation for device name */
		if (i < BUCK_SR1S_IDX) {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BUCK%dS_%s_IRQ%d@%s", i + 1, pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BUCK%dS_%s_IRQ", i + 1, pmic_irq->name);
		}
		else {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BUCK_SR%dS_%s_IRQ%d@%s", i + 1 - BUCK_SR1S_IDX,
				pmic_irq->name, pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BUCK_SR%dS_%s_IRQ", i + 1 - BUCK_SR1S_IDX, pmic_irq->name);
		}
		ret = devm_request_threaded_irq(dev, pmic_irq->irq_id[i], NULL,
						s2p_handle_regulator_irq, 0,
						pmic_irq->irq_name[i], pmic_irq);
		if (ret < 0) {
			dev_err(dev, "Failed to request %s: %d, errono: %d\n",
				pmic_irq->irq_name[i], pmic_irq->irq_id[i], ret);
			goto err;
		}
	}

	/* Thermal interrupt */
	pmic_irq = &pm_irq_info[S2MPU16_TEMP];
	for (i = 0; i < pmic_irq->count; i++) {
		pmic_irq->irq_id[i] = irq_base + S2MPU16_PMIC_IRQ_INT120C_INT1 + i;
		pmic_irq->irq_type[i] = S2P_TEMP;

		/* Dynamic allocation for device name */
		snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
			"%s_S_%s_IRQ%d@%s", temp_int_type[i], pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
		snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
			"%s_S_%s_IRQ", temp_int_type[i], pmic_irq->name);

		ret = devm_request_threaded_irq(dev, pmic_irq->irq_id[i], NULL,
						s2p_handle_regulator_irq, 0,
						pmic_irq->irq_name[i], pmic_irq);
		if (ret < 0) {
			dev_err(dev, "Failed to request %s: %d, errono: %d\n",
				pmic_irq->irq_name[i], pmic_irq->irq_id[i], ret);
			goto err;
		}
	}

	return 0;

err:
	s2p_destroy_irq_list_mutex(sdev->irq_list);
	s2p_destroy_workqueue(sdev->irq_list->irqs, sdev->irq_list->irq_type_cnt);

	return ret;
}

static int s2mpu16_set_buck_oi_ocp(struct s2mpu16_dev *s2mpu16)
{
	int ret = 0;
	struct s2p_dev *sdev = s2mpu16->sdev;

	ret = s2p_set_registers(sdev, s2mpu16_oi_ocp, ARRAY_SIZE(s2mpu16_oi_ocp));
	if (ret < 0) {
		dev_err(sdev->dev, "%s: fail(%d)\n", __func__, ret);
		return ret;
	}

	return ret;
}

int s2mpu16_irq_init(struct s2mpu16_dev *s2mpu16)
{
	struct s2p_dev *sdev = s2mpu16->sdev;
	const int irq_base = sdev->irq_base;
	int ret = 0;

	if (!irq_base) {
		dev_err(s2mpu16->dev, "[SUB_PMIC] %s: No interrupt base specified\n", __func__);
		return 0;
	}

	mutex_init(&s2mpu16->irq_lock);

	s2mpu16_mask_interrupt(s2mpu16);
	s2mpu16_register_genirq(s2mpu16, irq_base);

	s2p_register_irq_handler(s2mpu16->dev, s2mpu16_irq_handler);

	ret = s2mpu16_set_regulator_interrupt(s2mpu16, irq_base);
	if (ret < 0) {
		dev_err(sdev->dev, "%s: set regulator interrupt fail\n", __func__);
		return ret;
	}

	ret = s2mpu16_set_buck_oi_ocp(s2mpu16);
	if (ret < 0) {
		dev_err(sdev->dev, "%s: s2mpu16_set_buck_oi_ocp fail\n", __func__);
		goto err_irq_info;
	}
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	ret = s2p_create_irq_sysfs(sdev, MFD_DEV_NAME, 0);
	if (ret < 0) {
		dev_err(sdev->dev, "%s: fail to create sysfs\n", __func__);
		goto err_irq_info;
	}
#endif
	return 0;

err_irq_info:
	s2p_destroy_irq_list_mutex(sdev->irq_list);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mpu16_irq_init);
