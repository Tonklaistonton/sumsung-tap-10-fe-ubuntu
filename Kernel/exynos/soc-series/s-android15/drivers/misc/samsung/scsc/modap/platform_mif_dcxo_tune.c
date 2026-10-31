#include "platform_mif_dcxo_tune.h"

static u32 oldapm_intmr1_val;
static void __iomem *base_apm;

inline void platform_mif_reg_write_apm(u16 offset, u32 value)
{
	writel(value, base_apm + offset);
}

inline u32 platform_mif_reg_read_apm(u16 offset)
{
	return readl(base_apm + offset);
}

static void platform_mif_send_dcxo_cmd(struct scsc_mif_abs *interface, u8 opcode, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	static u8 seq = 0;

	platform_mif_reg_write_apm(MAILBOX_WLBT_REG(ISSR(0)), BUILD_ISSR0_VALUE(opcode, seq));
	platform_mif_reg_write_apm(MAILBOX_WLBT_REG(ISSR(1)), val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write APM MAILBOX: 0x%p\n", val);

	seq = (seq + 1) % APM_CMD_MAX_SEQ_NUM;

	platform_mif_reg_write_apm(MAILBOX_WLBT_REG(INTGR0), (1 << APM_IRQ_BIT_DCXO_SHIFT) << 16);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR0: bit 1 on target APM\n");
}

static int platform_mif_irq_register_mbox_apm(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	int	i;

	/* Initialise MIF registers with documented defaults */
	/* MBOXes */
	for (i = 0; i < 8; i++) {
		platform_mif_reg_write_apm(MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
	}

	// INTXR0 : AP/FW -> APM , INTXR1 : APM -> AP/FW
	/* MRs */ /*1's - set bit 1 as unmasked */
	oldapm_intmr1_val = platform_mif_reg_read_apm(MAILBOX_WLBT_REG(INTMR1));
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "APM MAILBOX INTMR1 %p\n", oldapm_intmr1_val);
	platform_mif_reg_write_apm(MAILBOX_WLBT_REG(INTMR1),
				   oldapm_intmr1_val & ~(1 << APM_IRQ_BIT_DCXO_SHIFT));
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write_apm(MAILBOX_WLBT_REG(INTCR1), (1 << APM_IRQ_BIT_DCXO_SHIFT));

	/* Register MBOX irq APM */
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Registering MBOX APM\n");

	return 0;
}

static void platform_mif_irq_unregister_mbox_apm(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unregistering MBOX APM irq\n");

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write_apm(MAILBOX_WLBT_REG(INTMR1), oldapm_intmr1_val);

	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write_apm(MAILBOX_WLBT_REG(INTCR1), (1 << APM_IRQ_BIT_DCXO_SHIFT));
}

static int platform_mif_check_dcxo_ack(struct scsc_mif_abs *interface, u8 opcode, u32* val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	unsigned long timeout;
	u32 irq_val;
	int ret;

	SCSC_TAG_INFO(PLAT_MIF, "wait for dcxo tune ack\n");

	timeout = jiffies + msecs_to_jiffies(500);
	do {
		irq_val = platform_mif_reg_read_apm(MAILBOX_WLBT_REG(INTMSR1));
		if (irq_val & (1 << APM_IRQ_BIT_DCXO_SHIFT)) {
			SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "APM MAILBOX INTMSR1 %p\n", irq_val);

			irq_val = platform_mif_reg_read_apm(MAILBOX_WLBT_REG(ISSR(2)));
			SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read Ack for setting DCXO tune: 0x%p\n", irq_val);

			ret = (irq_val & MASK_DONE) >> SHIFT_DONE ? 0 : 1;

			if (opcode == OP_GET_TUNE && val != NULL) {
				irq_val = platform_mif_reg_read_apm(MAILBOX_WLBT_REG(ISSR(3)));
				SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Read tune value for DCXO: 0x%p\n", irq_val);

				*val = irq_val;
			}

			platform_mif_reg_write_apm(MAILBOX_WLBT_REG(INTCR1), (1 << APM_IRQ_BIT_DCXO_SHIFT));
			goto done;
		}
	} while (time_before(jiffies, timeout));

	SCSC_TAG_INFO(PLAT_MIF, "timeout waiting for INTMSR1 bit 1 0x%08x\n", irq_val);
	return -ECOMM;
done:
	return ret;
}

static int platform_mif_dcxo_tune_resource_init(struct platform_device *pdev)
{
	struct resource *reg_res;
	int err;
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	base_apm = devm_ioremap_resource(&pdev->dev, reg_res);
	if (IS_ERR(base_apm)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, &pdev->dev,
			"Error getting mem resource for MAILBOX_APM\n");
		err = PTR_ERR(base_apm);
		return err;
	}
	return 0;
}

int platform_mif_dcxo_tune_init(struct platform_device *pdev, struct scsc_mif_abs *interface)
{
	interface->send_dcxo_cmd = platform_mif_send_dcxo_cmd;
	interface->check_dcxo_ack = platform_mif_check_dcxo_ack;
	interface->irq_register_mbox_apm = platform_mif_irq_register_mbox_apm;
	interface->irq_unregister_mbox_apm = platform_mif_irq_unregister_mbox_apm;

	return platform_mif_dcxo_tune_resource_init(pdev);
}
