/*
 * s2mpm07-irq.c - Interrupt controller support for S2MPM07
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
#include <linux/pmic/s2mpm07-mfd.h>
#include <linux/input.h>

static const uint8_t s2mpm07_mask_reg[] = {
	[S2MPM07_PMIC_INT1] = S2MPM07_PM1_INT1M,
	[S2MPM07_PMIC_INT2] = S2MPM07_PM1_INT2M,
	[S2MPM07_PMIC_INT3] = S2MPM07_PM1_INT3M,
};

static int s2mpm07_get_base_addr(struct s2mpm07_dev *s2mpm07,
			enum s2mpm07_irq_source src)
{
	switch (src) {
	case S2MPM07_PMIC_INT1 ... S2MPM07_PMIC_INT3:
		return s2mpm07->pm1;
	default:
		return -EINVAL;
	}
}

struct s2mpm07_irq_data {
	int mask;
	enum s2mpm07_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mpm07_irq_data s2mpm07_irqs[] = {
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_PWRONR_INT1,		S2MPM07_PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_PWRONF_INT1,		S2MPM07_PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_120C_INT1,			S2MPM07_PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_140C_INT1,			S2MPM07_PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_TSD_INT1,			S2MPM07_PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_WTSR_INT1,			S2MPM07_PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_WRSTB_INT1,		S2MPM07_PMIC_INT1, 1 << 6),

	DECLARE_IRQ(S2MPM07_PMIC_IRQ_OCP_B1R_INT2,		S2MPM07_PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_OCP_SR1R_INT2,		S2MPM07_PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_OI_B1R_INT2,		S2MPM07_PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_OI_SR1R_INT2,		S2MPM07_PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_OVP_INT2,			S2MPM07_PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_BUCK_AUTO_EXIT_INT2,	S2MPM07_PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPM07_PMIC_IRQ_OI_LDO20R_INT3,		S2MPM07_PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_OI_LDO21R_INT3,		S2MPM07_PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_TX_TRAN_FAIL_INT3,		S2MPM07_PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_CHECKSUM_INT3,		S2MPM07_PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_PARITY_ERR0_INT3,		S2MPM07_PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_PARITY_ERR1_INT3,		S2MPM07_PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_PARITY_ERR2_INT3,		S2MPM07_PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPM07_PMIC_IRQ_PARITY_ERR3_INT3,		S2MPM07_PMIC_INT3, 1 << 7),
};

static void s2mpm07_irq_lock(struct irq_data *data)
{
	struct s2mpm07_dev *s2mpm07 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpm07->irq_lock);
}

static void s2mpm07_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpm07_dev *s2mpm07 = irq_get_chip_data(data->irq);
	uint32_t i = 0;

	for (i = 0; i < S2MPM07_IRQ_GROUP_NR; i++) {
		uint8_t mask_reg = s2mpm07_mask_reg[i];
		int base_addr = s2mpm07_get_base_addr(s2mpm07, i);

		if (mask_reg == S2MPM07_REG_INVALID || base_addr == -EINVAL)
			continue;
		s2mpm07->irq_masks_cache[i] = s2mpm07->irq_masks_cur[i];

		s2p_write_reg(s2mpm07->sdev, base_addr,
				s2mpm07_mask_reg[i], s2mpm07->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpm07->irq_lock);
}

static const inline struct s2mpm07_irq_data *
irq_to_s2mpm07_irq(struct s2mpm07_dev *s2mpm07, int irq)
{
	return &s2mpm07_irqs[irq - s2mpm07->sdev->irq_base];
}

static void s2mpm07_irq_mask(struct irq_data *data)
{
	struct s2mpm07_dev *s2mpm07 = irq_get_chip_data(data->irq);
	const struct s2mpm07_irq_data *irq_data =
	    irq_to_s2mpm07_irq(s2mpm07, data->irq);

	if (irq_data->group >= S2MPM07_IRQ_GROUP_NR)
		return;

	s2mpm07->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpm07_irq_unmask(struct irq_data *data)
{
	struct s2mpm07_dev *s2mpm07 = irq_get_chip_data(data->irq);
	const struct s2mpm07_irq_data *irq_data =
	    irq_to_s2mpm07_irq(s2mpm07, data->irq);

	if (irq_data->group >= S2MPM07_IRQ_GROUP_NR)
		return;

	s2mpm07->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpm07_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpm07_irq_lock,
	.irq_bus_sync_unlock	= s2mpm07_irq_sync_unlock,
	.irq_mask		= s2mpm07_irq_mask,
	.irq_unmask		= s2mpm07_irq_unmask,
};

static void s2mpm07_report_irq(struct s2mpm07_dev *s2mpm07)
{
	uint32_t i = 0;

	/* Apply masking */
	for (i = 0; i < S2MPM07_IRQ_GROUP_NR; i++)
		s2mpm07->irq_reg[i] &= ~s2mpm07->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPM07_IRQ_NR; i++)
		if (s2mpm07->irq_reg[s2mpm07_irqs[i].group] & s2mpm07_irqs[i].mask)
			handle_nested_irq(s2mpm07->sdev->irq_base + i);
}

static int s2mpm07_irq_handler(struct device *dev, uint8_t *vgi_src)
{
	struct s2mpm07_dev *s2mpm07 = dev_get_drvdata(dev);
	uint8_t *irq_reg = NULL;
	int ret = 0;

	if (!dev || !vgi_src || !s2mpm07)
		return -ENODEV;

	if (!(vgi_src[2] & S2MPM07_VGI2_IRQ_RF))
		return 0;

	irq_reg = s2mpm07->irq_reg;

	ret = s2p_bulk_read(s2mpm07->sdev, s2mpm07->pm1, S2MPM07_PM1_INT1,
			S2MPM07_IRQ_GROUP_NR, &irq_reg[S2MPM07_PMIC_INT1]);
	if (ret) {
		dev_err(dev, "%s: Failed to read pmic interrupt: %d\n", __func__, ret);
		return ret;
	}

	dev_info(dev, "[RF_PMIC] %s: %#hhx %#hhx %#hhx\n", __func__,
			irq_reg[0], irq_reg[1], irq_reg[2]);

	s2mpm07_report_irq(s2mpm07);

	return 0;
}

static void s2mpm07_mask_interrupt(struct s2mpm07_dev *s2mpm07)
{
	uint32_t i = 0;

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPM07_IRQ_GROUP_NR; i++) {
		int base_addr = 0;

		s2mpm07->irq_masks_cur[i] = 0xff;
		s2mpm07->irq_masks_cache[i] = 0xff;

		base_addr = s2mpm07_get_base_addr(s2mpm07, i);
		if (base_addr == -EINVAL)
			continue;

		if (s2mpm07_mask_reg[i] == S2MPM07_REG_INVALID)
			continue;

		s2p_write_reg(s2mpm07->sdev, base_addr, s2mpm07_mask_reg[i], 0xff);
	}
}

static void s2mpm07_register_genirq(struct s2mpm07_dev *s2mpm07, const int irq_base)
{
	uint32_t i = 0;

	for (i = 0; i < S2MPM07_IRQ_NR; i++) {
		uint32_t cur_irq = i + irq_base;

		irq_set_chip_data(cur_irq, s2mpm07);
		irq_set_chip_and_handler(cur_irq, &s2mpm07_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, true);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}
}

int s2mpm07_irq_init(struct s2mpm07_dev *s2mpm07)
{
	struct s2p_dev *sdev = s2mpm07->sdev;
	const int irq_base = sdev->irq_base;

	if (!irq_base) {
		dev_err(s2mpm07->dev, "[RF_PMIC] %s: No interrupt base specified\n", __func__);
		return 0;
	}

	mutex_init(&s2mpm07->irq_lock);

	s2mpm07_mask_interrupt(s2mpm07);
	s2mpm07_register_genirq(s2mpm07, irq_base);

	s2p_register_irq_handler(s2mpm07->dev, s2mpm07_irq_handler);

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpm07_irq_init);
