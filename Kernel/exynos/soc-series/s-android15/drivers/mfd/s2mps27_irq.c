/*
 * s2mps27-irq.c - Interrupt controller support for S2MPS27
 *
 * Copyright (C) 2023 Samsung Electronics Co.Ltd
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
#include <linux/pmic/s2mps27-mfd.h>
#include <linux/input.h>

static const uint8_t s2mps27_mask_reg[] = {
	[S2MPS27_PMIC_INT1] = S2MPS27_PM1_INT1M,
	[S2MPS27_PMIC_INT2] = S2MPS27_PM1_INT2M,
	[S2MPS27_PMIC_INT3] = S2MPS27_PM1_INT3M,
	[S2MPS27_PMIC_INT4] = S2MPS27_PM1_INT4M,
	[S2MPS27_PMIC_INT5] = S2MPS27_PM1_INT5M,
	[S2MPS27_ADC_INTP] = S2MPS27_ADC_ADC_INTM,
};

static int s2mps27_get_base_addr(struct s2mps27_dev *s2mps27,
			enum s2mps27_irq_source src)
{
	switch (src) {
	case S2MPS27_PMIC_INT1 ... S2MPS27_PMIC_INT5:
		return s2mps27->pm1;
	case S2MPS27_ADC_INTP:
		return s2mps27->adc;
	default:
		return -EINVAL;
	}
}

struct s2mps27_irq_data {
	int mask;
	enum s2mps27_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mps27_irq_data s2mps27_irqs[] = {
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PWRONR_INT1,		S2MPS27_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PWRONP_INT1,		S2MPS27_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_JIGONBF_INT1,		S2MPS27_PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_JIGONBR_INT1,		S2MPS27_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_ACOKBF_INT1,		S2MPS27_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_ACOKBR_INT1,		S2MPS27_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PWRON1S_INT1,		S2MPS27_PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_MRB_INT1,			S2MPS27_PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTC60S_INT2,		S2MPS27_PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTCA1_INT2,		S2MPS27_PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTCA0_INT2,		S2MPS27_PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_SMPL_INT2,			S2MPS27_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_RTC1S_INT2,		S2MPS27_PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_WTSR_INT2,			S2MPS27_PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_SPMI_LDO_OK_FAIL_INT2,	S2MPS27_PMIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_WRSTB_INT2,		S2MPS27_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_INT120C_INT3,		S2MPS27_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_INT140C_INT3,		S2MPS27_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_TSD_INT3,			S2MPS27_PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OVP_INT3,			S2MPS27_PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_TX_TRAN_FAIL_INT3,		S2MPS27_PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OTP_CSUM_ERR_INT3,		S2MPS27_PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_VOLDNR_INT3,		S2MPS27_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_VOLDNP_INT3,		S2MPS27_PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OCP_SR1_INT4,		S2MPS27_PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OCP_BB1_INT4,		S2MPS27_PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_SR1_INT4,		S2MPS27_PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_BB1_INT4,		S2MPS27_PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR0_INT4,		S2MPS27_PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR1_INT4,		S2MPS27_PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR2_INT4,		S2MPS27_PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_PARITY_ERR3_INT4,		S2MPS27_PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L1_INT5,		S2MPS27_PMIC_INT5, 1 << 0),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L7_INT5,		S2MPS27_PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L8_INT5,		S2MPS27_PMIC_INT5, 1 << 2),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L9_INT5,		S2MPS27_PMIC_INT5, 1 << 3),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L10_INT5,		S2MPS27_PMIC_INT5, 1 << 4),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_OI_L11_INT5,		S2MPS27_PMIC_INT5, 1 << 5),
	DECLARE_IRQ(S2MPS27_PMIC_IRQ_WDT_INT5,			S2MPS27_PMIC_INT5, 1 << 6),
};

enum s2mps27_irq_type {
	S2MPS27_OCP,
	S2MPS27_OI,
	S2MPS27_TEMP,
	S2MPS27_IRQ_TYPE_CNT,
};

#define DECLARE_OI(_base, _reg, _val) {.base = (_base), .reg = (_reg), .val = (_val) }
static const struct s2p_reg_data s2mps27_oi_ocp[] = {
	/* BUCK_SR1, BUCK_BOOST OCL,OI function enable, OI power down,OVP disable */
	/* OI detection time window : 300us, OI comp. output count : 50 times */
	DECLARE_OI(S2MPS27_PM1_ADDR, S2MPS27_PM1_BUCK_SR1_OCP, 0xCC),
	DECLARE_OI(S2MPS27_PM1_ADDR, S2MPS27_PM1_BB_OCP, 0xCC),
};

static void s2mps27_irq_lock(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mps27->irq_lock);
}

static void s2mps27_irq_sync_unlock(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);
	uint32_t i = 0;

	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++) {
		uint8_t mask_reg = s2mps27_mask_reg[i];
		int base_addr = s2mps27_get_base_addr(s2mps27, i);

		if (mask_reg == S2MPS27_REG_INVALID || base_addr == -EINVAL)
			continue;
		s2mps27->irq_masks_cache[i] = s2mps27->irq_masks_cur[i];

		s2p_write_reg(s2mps27->sdev, base_addr,
				s2mps27_mask_reg[i], s2mps27->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mps27->irq_lock);
}

static const inline struct s2mps27_irq_data *
irq_to_s2mps27_irq(struct s2mps27_dev *s2mps27, int irq)
{
	return &s2mps27_irqs[irq - s2mps27->sdev->irq_base];
}

static void s2mps27_irq_mask(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);
	const struct s2mps27_irq_data *irq_data =
	    irq_to_s2mps27_irq(s2mps27, data->irq);

	if (irq_data->group >= S2MPS27_IRQ_GROUP_NR)
		return;

	s2mps27->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mps27_irq_unmask(struct irq_data *data)
{
	struct s2mps27_dev *s2mps27 = irq_get_chip_data(data->irq);
	const struct s2mps27_irq_data *irq_data =
	    irq_to_s2mps27_irq(s2mps27, data->irq);

	if (irq_data->group >= S2MPS27_IRQ_GROUP_NR)
		return;

	s2mps27->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mps27_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mps27_irq_lock,
	.irq_bus_sync_unlock	= s2mps27_irq_sync_unlock,
	.irq_mask		= s2mps27_irq_mask,
	.irq_unmask		= s2mps27_irq_unmask,
};

static void s2mps27_report_irq(struct s2mps27_dev *s2mps27)
{
	uint32_t i = 0;

	/* Apply masking */
	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++)
		s2mps27->irq_reg[i] &= ~s2mps27->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPS27_IRQ_NR; i++)
		if (s2mps27->irq_reg[s2mps27_irqs[i].group] & s2mps27_irqs[i].mask)
			handle_nested_irq(s2mps27->sdev->irq_base + i);
}

static void s2mps27_clear_irq_regs(uint8_t *irq_reg)
{
	uint32_t i = 0;

	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++)
		irq_reg[i] &= 0x00;
}

static bool s2mps27_is_duplicate_key(const uint8_t *irq_reg, const uint8_t reg_int, const uint8_t mask_status)
{
	if ((irq_reg[reg_int] & mask_status) == mask_status)
		return true;

	return false;
}

static int s2mps27_get_pmic_key(const uint8_t *irq_reg)
{
	if ((irq_reg[S2MPS27_PMIC_INT3] & 0xC0) == 0xC0)
		return KEY_VOLUMEDOWN;
	else
		return KEY_POWER;
}

static int s2mps27_key_detection(struct s2mps27_dev *s2mps27)
{
	int ret = 0, key = 0;
	uint8_t val, reg_int, reg_status, mask_status, flag_status, key_release, key_press;
	uint8_t *irq_reg = s2mps27->irq_reg;

	key = s2mps27_get_pmic_key(irq_reg);

	switch (key) {
	case KEY_POWER:
		reg_int = S2MPS27_PMIC_INT1;
		reg_status = S2MPS27_PM1_STATUS1;
		mask_status = 0x03;
		flag_status = S2MPS27_STATUS1_PWRON;
		key_release = S2MPS27_PWRKEY_RELEASE;
		key_press = S2MPS27_PWRKEY_PRESS;
		break;
	case KEY_VOLUMEDOWN:
		reg_int = S2MPS27_PMIC_INT3;
		reg_status = S2MPS27_PM1_STATUS1;
		mask_status = 0xC0;
		flag_status = S2MPS27_STATUS1_MR1B;
		key_release = S2MPS27_VOLDN_RELEASE;
		key_press = S2MPS27_VOLDN_PRESS;
		break;
	default:
		return 0;
	}

	/* Determine release/press edge for PWR_ON key */
	if (s2mps27_is_duplicate_key(irq_reg, reg_int, mask_status)) {
		ret = s2p_read_reg(s2mps27->sdev, s2mps27->pm1, reg_status, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			goto power_key_err;
		}
		irq_reg[reg_int] &= (~mask_status);

		if (val & flag_status) {
			irq_reg[reg_int] = key_release;

			s2mps27_report_irq(s2mps27);
			s2mps27_clear_irq_regs(irq_reg);

			irq_reg[reg_int] = key_press;
		} else {
			irq_reg[reg_int] = key_press;

			s2mps27_report_irq(s2mps27);
			s2mps27_clear_irq_regs(irq_reg);

			irq_reg[reg_int] = key_release;
		}
	}
	return 0;

power_key_err:
	return -EINVAL;
}

static int s2mps27_irq_handler(struct device *dev, uint8_t *vgi_src)
{
	struct s2mps27_dev *s2mps27 = dev_get_drvdata(dev);
	uint8_t *irq_reg = NULL;
	int ret = 0;

	if (!dev || !vgi_src || !s2mps27)
		return -ENODEV;

	if (!(vgi_src[4] & S2MPS27_VGI4_IRQ_M) && !(vgi_src[1] & S2MPS27_VGI1_IRQ_S1)
		&& !(vgi_src[1] & S2MPS27_VGI1_IRQ_S2) && !(vgi_src[2] & S2MPS27_VGI2_IRQ_S3)
	       	&& !(vgi_src[4] & S2MPS27_VGI4_IRQ_S4) && !(vgi_src[5] & S2MPS27_VGI5_IRQ_S5)
		&& !(vgi_src[1] & S2MPS27_VGI1_IRQ_EXTRA) && !(vgi_src[2] & S2MPS27_VGI2_IRQ_RF)) {
		dev_info(dev, "[MAIN_PMIC] %s: r0(%#hhx) r1(%#hhx) r2(%#hhx) r3(%#hhx) r4(%#hhx), r5(%#hhx)\n",
			__func__, vgi_src[0], vgi_src[1], vgi_src[2], vgi_src[3], vgi_src[4], vgi_src[5]);
		return -EINVAL;
	}

	if (!(vgi_src[4] & S2MPS27_VGI4_IRQ_M))
		return 0;

	irq_reg = s2mps27->irq_reg;

	ret = s2p_bulk_read(s2mps27->sdev, s2mps27->pm1, S2MPS27_PM1_INT1,
			S2MPS27_IRQ_GROUP_NR, &irq_reg[S2MPS27_PMIC_INT1]);
	if (ret) {
		dev_err(dev, "%s: Failed to read pmic interrupt: %d\n", __func__, ret);
		return ret;
	}

	dev_info(dev, "[MAIN_PMIC] %s: %#hhx %#hhx %#hhx %#hhx %#hhx\n", __func__,
			irq_reg[0], irq_reg[1], irq_reg[2], irq_reg[3], irq_reg[4]);

	ret = s2mps27_key_detection(s2mps27);
	if (ret < 0) {
		dev_err(dev, "[MAIN_PMIC] %s: KEY detection error\n", __func__);
		return ret;
	}

	s2mps27_report_irq(s2mps27);

	return 0;
}

static void s2mps27_mask_interrupt(struct s2mps27_dev *s2mps27)
{
	uint32_t i = 0;

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPS27_IRQ_GROUP_NR; i++) {
		int base_addr = 0;

		s2mps27->irq_masks_cur[i] = 0xff;
		s2mps27->irq_masks_cache[i] = 0xff;

		base_addr = s2mps27_get_base_addr(s2mps27, i);
		if (base_addr == -EINVAL)
			continue;

		if (s2mps27_mask_reg[i] == S2MPS27_REG_INVALID)
			continue;

		s2p_write_reg(s2mps27->sdev, base_addr, s2mps27_mask_reg[i], 0xff);
	}
}

static void s2mps27_register_genirq(struct s2mps27_dev *s2mps27, const int irq_base)
{
	uint32_t i = 0;

	for (i = 0; i < S2MPS27_IRQ_NR; i++) {
		uint32_t cur_irq = i + irq_base;

		irq_set_chip_data(cur_irq, s2mps27);
		irq_set_chip_and_handler(cur_irq, &s2mps27_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, true);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}
}

static int s2mps27_set_regulator_interrupt(struct s2mps27_dev *s2mps27, const uint32_t irq_base)
{
	struct s2p_dev *sdev = s2mps27->sdev;
	struct device *dev = sdev->dev;
	struct s2p_irq_info *pmic_irq = NULL;
	struct s2p_irq_info *pm_irq_info = NULL;
	const char int_type_name[S2MPS27_IRQ_TYPE_CNT][10] = { "OCP", "OI", "TEMP" };
	const char int_type_count[S2MPS27_IRQ_TYPE_CNT] = { S2MPS27_BUCK_MAX, S2MPS27_BUCK_MAX, S2MPS27_TEMP_MAX };
	const char temp_int_type[S2MPS27_TEMP_MAX][10] = { "120C", "140C" };
	struct s2p_pmic_irq_list irq_list;
	uint32_t i = 0;
	int ret = 0;

	if (!irq_base) {
		dev_err(dev, "%s: Failed to get irq base: %u\n", __func__, irq_base);
		return -ENODEV;
	}

	dev_info(dev, "[MAIN PMIC] %s: start, irq_base: %u\n", __func__, irq_base);

	pm_irq_info = devm_kzalloc(dev, S2MPS27_IRQ_TYPE_CNT * sizeof(struct s2p_irq_info), GFP_KERNEL);
	if (!pm_irq_info)
		return -ENOMEM;

	for (i = 0; i < S2MPS27_IRQ_TYPE_CNT; i++) {
		strcpy(pm_irq_info[i].name, int_type_name[i]);
		pm_irq_info[i].count = int_type_count[i];
	}

	ret = s2p_allocate_pmic_irq(dev, pm_irq_info, S2MPS27_IRQ_TYPE_CNT);
	if (ret < 0)
		return ret;

	ret = s2p_create_irq_notifier_wq(dev, pm_irq_info, S2MPS27_IRQ_TYPE_CNT, MFD_DEV_NAME, 0);
	if (ret < 0)
		return ret;

	irq_list.irq_type_cnt = S2MPS27_IRQ_TYPE_CNT;
	irq_list.irqs = pm_irq_info;
	sdev->irq_list = &irq_list;

	/* BUCK_SR1, BB1 OCP ineterrupt */
	pmic_irq = &pm_irq_info[S2MPS27_OCP];
	for (i = 0; i < pmic_irq->count; i++) {
		pmic_irq->irq_id[i] = irq_base + S2MPS27_PMIC_IRQ_OCP_SR1_INT4 + i;
		pmic_irq->irq_type[i] = S2P_OCP;

		/* Dynamic allocation for device name */
		if (i < BB1M_IDX) {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BUCK_SR%dM_%s_IRQ%d@%s", i + 1, pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BUCK_SR%dM_%s_IRQ", i + 1, pmic_irq->name);
		}
		else {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BB%dM_%s_IRQ%d@%s", i + 1 - BB1M_IDX, pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BB%dM_%s_IRQ", i + 1 - BB1M_IDX, pmic_irq->name);
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

	/* BUCK_SR1, BB1 OI interrupt */
	pmic_irq = &pm_irq_info[S2MPS27_OI];
	for (i = 0; i < pmic_irq->count; i++) {
		pmic_irq->irq_id[i] = irq_base + S2MPS27_PMIC_IRQ_OI_SR1_INT4 + i;
		pmic_irq->irq_type[i] = S2P_OI;

		/* Dynamic allocation for device name */
		if (i < BB1M_IDX) {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BUCK_SR%dM_%s_IRQ%d@%s", i + 1, pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BUCK_SR%dM_%s_IRQ", i + 1, pmic_irq->name);
		}
		else {
			snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
				"BB%dM_%s_IRQ%d@%s", i + 1 - BB1M_IDX, pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
			snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
				"BB%dM_%s_IRQ", i + 1 - BB1M_IDX, pmic_irq->name);
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
	pmic_irq = &pm_irq_info[S2MPS27_TEMP];
	for (i = 0; i < pmic_irq->count; i++) {
		pmic_irq->irq_id[i] = irq_base + S2MPS27_PMIC_IRQ_INT120C_INT3 + i;
		pmic_irq->irq_type[i] = S2P_TEMP;

		/* Dynamic allocation for device name */
		snprintf(pmic_irq->irq_name[i], sizeof(pmic_irq->irq_name[i]) - 1,
			"%sM_%s_IRQ%d@%s", temp_int_type[i], pmic_irq->name,
				pmic_irq->irq_id[i], dev_name(dev));
		snprintf(pmic_irq->irq_noti_name[i], sizeof(pmic_irq->irq_noti_name[i]) - 1,
			"%sM_%s_IRQ", temp_int_type[i], pmic_irq->name);

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

static int s2mps27_set_buck_oi_ocp(struct s2mps27_dev *s2mps27)
{
	int ret = 0;
	struct s2p_dev *sdev = s2mps27->sdev;

	ret = s2p_set_registers(sdev, s2mps27_oi_ocp, ARRAY_SIZE(s2mps27_oi_ocp));
	if (ret < 0) {
		dev_err(sdev->dev, "%s: fail(%d)\n", __func__, ret);
		return ret;
	}

	return ret;
}

int s2mps27_irq_init(struct s2mps27_dev *s2mps27)
{
	struct s2p_dev *sdev = s2mps27->sdev;
	const int irq_base = sdev->irq_base;
	int ret = 0;

	if (!irq_base) {
		dev_err(s2mps27->dev, "[MAIN_PMIC] %s: No interrupt base specified\n", __func__);
		return 0;
	}

	mutex_init(&s2mps27->irq_lock);

	s2mps27_mask_interrupt(s2mps27);
	s2mps27_register_genirq(s2mps27, irq_base);

	s2p_register_irq_handler(s2mps27->dev, s2mps27_irq_handler);

	ret = s2mps27_set_regulator_interrupt(s2mps27, irq_base);
	if (ret < 0) {
		dev_err(sdev->dev, "%s: set regulator interrupt fail\n", __func__);
		return ret;
	}

	ret = s2mps27_set_buck_oi_ocp(s2mps27);
	if (ret < 0) {
		dev_err(sdev->dev, "%s: s2mps27_buck_oi_ocp fail\n", __func__);
		goto err_irq_info;
	}
	return 0;

err_irq_info:
	s2p_destroy_irq_list_mutex(sdev->irq_list);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mps27_irq_init);
