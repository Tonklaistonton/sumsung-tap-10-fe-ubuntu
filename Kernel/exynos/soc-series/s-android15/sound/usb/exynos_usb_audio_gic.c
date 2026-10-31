// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *   USB Audio offloading Driver for Exynos
 *
 *   Copyright (c) 2017 by Kyounghye Yun <k-hye.yun@samsung.com>
 *
 */


#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/iommu.h>
#include <linux/phy/phy.h>

#include <soc/samsung/exynos-smc.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <trace/hooks/usb.h>
#include "exynos_usb_audio.h"
#include "usbaudio.h"
#include "helper.h"
#include "card.h"
#include "quirks.h"
#include "power.h"

#include "../../drivers/usb/host/xhci.h"

#define DEBUG 1
#define PARAM1 0x4
#define PARAM2 0x8
#define PARAM3 0xc
#define PARAM4 0x10
#define RESPONSE 0x20

#define USB_IPC_IRQ	4

#define SUBSTREAM_FLAG_DATA_EP_STARTED	0
#define SUBSTREAM_FLAG_SYNC_EP_STARTED	1

static const unsigned long USB_LOCK_ID_OUT = 0x4F425355; // ASCII Value : USBO
static const unsigned long USB_LOCK_ID_IN = 0x49425355; // ASCII Value : USBI

static struct exynos_usb_audio usb_audio;
static struct snd_usb_platform_ops platform_ops;
static int exynos_usb_audio_unmap_all(void);
static void exynos_usb_audio_offload_init(void);
#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
static void exynos_usb_audio_offload_store_sideband_info(struct usb_device *udev);
#else
static void exynos_usb_audio_offload_store_hw_info(void);
#endif
void exynos_usb_xhci_recover(void);

struct hcd_hw_info *g_hwinfo;
EXPORT_SYMBOL_GPL(g_hwinfo);
// int usb_audio_connection;
// EXPORT_SYMBOL_GPL(usb_audio_connection);
struct wakeup_source *main_hcd_wakelock; /* Wakelock for HS HCD */
EXPORT_SYMBOL_GPL(main_hcd_wakelock);
struct wakeup_source *shared_hcd_wakelock; /* Wakelock for SS HCD */
EXPORT_SYMBOL_GPL(shared_hcd_wakelock);
int xhci_exynos_pm_state; /* xhci_exynos suspend/resume state */
EXPORT_SYMBOL_GPL(xhci_exynos_pm_state);

extern u32 usb_user_scenario;

struct usb_hcd *exynos_main_hcd;
EXPORT_SYMBOL_GPL(exynos_main_hcd);
struct usb_hcd *exynos_shared_hcd;
EXPORT_SYMBOL_GPL(exynos_shared_hcd);

struct xhci_exynos_audio {
	struct phy *phy;
	struct usb_hcd *hcd;
	dma_addr_t save_dma;
	void *save_addr;
	dma_addr_t out_dma;
	void *out_addr;
	dma_addr_t in_dma;
	void *in_addr;
	struct xhci_hcd	*xhci;
	int feedback;
	struct xhci_interrupter ir_data;
	struct xhci_interrupter *ir;
};

struct xhci_exynos_audio *g_xhci_exynos_audio;

static void __iomem *base_addr;
static struct usb_audio_msg usb_msg;
struct iommu_domain *iommu_domain;
static struct usb_audio_gic_data *gic_data;

struct audio_work {
	struct usb_device	*work_udev;

	int is_conn;
	bool sysmmu_mapping_done;
};

static struct audio_work audio_work_data;

static const struct of_device_id exynos_usb_audio_match[] = {
	{
		.compatible = "exynos-usb-audio-offloading",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_usb_audio_match);

static inline u32 upper_32b(u64 dma) { return upper_32_bits(le64_to_cpu(dma)); }
static inline u32 lower_32b(u64 dma) { return lower_32_bits(le64_to_cpu(dma)); }

static int gicd_xwritel(struct usb_audio_gic_data *data, u32 value,
							unsigned int offset)
{
	unsigned long arg1;
	int ret = 0;

	offset += data->gicd_base_phys;
	arg1 = SMC_REG_ID_SFR_W(offset);
	ret = exynos_smc(SMC_CMD_REG, arg1, value, 0);
	if (ret < 0)
		dev_err(data->dev, "write fail %#x: %d\n", offset, ret);

	return ret;
}

int usb_audio_gic_generate_interrupt(unsigned int irq)
{

	pr_debug("%s(%d)\n", __func__, irq);

	return gicd_xwritel(gic_data, (0x1 << 16) | (irq & 0xf), 0xf00);
}

static int usb_msg_to_uram(struct device *dev)
{
	unsigned int resp = 0;
	int ret = 0, timeout = 1000;

	mutex_lock(&usb_audio.msg_lock);
	iowrite32(usb_msg.type, base_addr);
	iowrite32(usb_msg.param1, base_addr + PARAM1);
	iowrite32(usb_msg.param2, base_addr + PARAM2);
	iowrite32(usb_msg.param3, base_addr + PARAM3);
	iowrite32(usb_msg.param4, base_addr + PARAM4);

	iowrite32(0, base_addr + RESPONSE);
	resp = ioread32(base_addr + RESPONSE);
	pr_info("[%s] pre-resp: 0x%x\n", __func__, resp);

	phy_set_mode_ext(g_hwinfo->phy, PHY_MODE_ABOX_POWER, 1);

	ret = usb_audio_gic_generate_interrupt(USB_IPC_IRQ);

	if (ret < 0)
		pr_err("failed to generate interrupt\n");

	while (timeout) {
		resp = ioread32(base_addr + RESPONSE);
		if (resp != 0) {
			pr_info("[%s] resp: 0x%x\n", __func__, resp);
			break;
		}
		udelay(300);
		timeout--;
	}

	if (timeout == 0) {
		pr_err("No response to message. err = %d\n", resp);
		ret = -ETIMEDOUT;
	}

	phy_set_mode_ext(g_hwinfo->phy, PHY_MODE_ABOX_POWER, 0);

	iowrite32(0, base_addr + RESPONSE);

	mutex_unlock(&usb_audio.msg_lock);
	return ret;
}

static int usb_audio_iommu_map(unsigned long iova, phys_addr_t addr,
		size_t bytes)
{
	int ret = 0;

	ret = iommu_map(iommu_domain, iova, addr, bytes, 0, GFP_KERNEL);
	if (ret < 0)
		pr_info("iommu_map(%#lx) fail: %d\n", iova, ret);

	atomic_set(&usb_audio.unmap_all_done, 0);

	return ret;
}

static int usb_audio_iommu_unmap(unsigned long iova, size_t bytes)
{
	int ret = 0;

	ret = iommu_unmap(iommu_domain, iova, bytes);
	if (ret < 0)
		pr_info("iommu_unmap(%#lx) fail: %d\n", iova, ret);

	return ret;
}

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
static int xhci_exynos_enable_interrupt(struct xhci_sideband *sb, u32 imod_interval)
{
	if (!sb || !sb->ir)
		return -ENODEV;

	xhci_set_interrupter_moderation(sb->ir, imod_interval);
	sb->ir->skip_events = false;
	xhci_enable_interrupter(sb->ir);

	return 0;
}

static int xhci_exynos_disable_interrupter(struct xhci_sideband *sb)
{
	struct xhci_interrupter *ir = sb->ir;
	u32 iman;

	if (!ir || !ir->ir_set)
		return -EINVAL;

	mutex_lock(&sb->mutex);

	iman = readl(&ir->ir_set->irq_pending);
	writel(ER_IRQ_DISABLE(iman), &ir->ir_set->irq_pending);

	/* Set skip_events 1 to skip xhci_disable_interrupter()
	 * in xhci_sideband_remove_interrupter() */
	sb->ir->skip_events = 1;
	mutex_unlock(&sb->mutex);

	return 0;
}
#endif

static int exynos_usb_audio_set_device(struct usb_device *udev)
{
	int ret = 0;
	usb_audio.udev = udev;
	usb_audio.is_audio = 1;

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	usb_audio.sb = xhci_sideband_register(udev);
	if (!usb_audio.sb) {
		usb_audio.is_audio = 0;
		return -EBUSY;
	}

	xhci_sideband_create_interrupter(usb_audio.sb, 1, 1, 1);
	ret = xhci_exynos_enable_interrupt(usb_audio.sb, 40000);
#endif
	return ret;
}

static int exynos_usb_audio_map_buf(struct usb_device *udev)
{
	struct hcd_hw_info *hwinfo = g_hwinfo;
	int ret;

	if (DEBUG) {
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s\n", __func__);
		pr_info("pcm in data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(hwinfo->in_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->in_dma)));
		pr_info("pcm out data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(hwinfo->out_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->out_dma)));
	}

	/* iommu map for in data buffer */
	usb_audio.in_buf_addr = hwinfo->in_dma;

	ret = usb_audio_iommu_map(USB_AUDIO_PCM_INBUF, hwinfo->in_dma, USB_AUDIO_PAGE_SIZE * 256);
	if (ret) {
		pr_err("abox iommu mapping for pcm in buf is failed\n");
		return ret;
	}

	/* iommu map for out data buffer */
	usb_audio.out_buf_addr = hwinfo->out_dma;

	ret = usb_audio_iommu_map(USB_AUDIO_PCM_OUTBUF, hwinfo->out_dma, USB_AUDIO_PAGE_SIZE * 256);
	if (ret) {
		pr_err("abox iommu mapping for pcm out buf is failed\n");
		return ret;
	}

	return 0;
}

static int exynos_usb_audio_hcd(struct usb_device *udev)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	struct hcd_hw_info *hwinfo = g_hwinfo;
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	exynos_usb_audio_offload_store_sideband_info(udev);
#else
	exynos_usb_audio_offload_store_hw_info();
#endif

	if (DEBUG) {
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s udev->devnum %d\n", __func__, udev->devnum);
		dev_info(&udev->dev, "=======[Check HW INFO] ========\n");
		dev_info(&udev->dev, "slot_id : %d\n", hwinfo->slot_id);
		dev_info(&udev->dev, "dcbaa : %#08llx\n", hwinfo->dcbaa_dma);
		dev_info(&udev->dev, "save : %#08llx\n", hwinfo->save_dma);
		dev_info(&udev->dev, "in_ctx : %#08llx\n", hwinfo->in_ctx);
		dev_info(&udev->dev, "out_ctx : %#08llx\n", hwinfo->out_ctx);
		dev_info(&udev->dev, "erst : %#08x %08x\n",
					upper_32_bits(le64_to_cpu(hwinfo->erst_addr)),
					lower_32_bits(le64_to_cpu(hwinfo->erst_addr)));
		dev_info(&udev->dev, "old erst : %#08llx\n", usb_audio.erst_addr);
		dev_info(&udev->dev, "use_uram : %d\n", hwinfo->use_uram);
		dev_info(&udev->dev, "===============================\n");
	}

	usb_audio.is_first_probe = 1;
	pr_info("%s: send is_first_probe %d\n", __func__, usb_audio.is_first_probe);

	/* back up each address for unmap */
	usb_audio.dcbaa_dma = hwinfo->dcbaa_dma;
	usb_audio.save_dma = hwinfo->save_dma;
	usb_audio.in_ctx = hwinfo->in_ctx;
	usb_audio.out_ctx = hwinfo->out_ctx;
	usb_audio.erst_addr = hwinfo->erst_addr;
	usb_audio.speed = hwinfo->speed;
	usb_audio.use_uram = hwinfo->use_uram;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : SFR MAPPING!\n");

	mutex_lock(&usb_audio.lock);
	if (!atomic_read(&usb_audio.unmap_all_done)) {
		dev_err(dev, "iommu unmap was not done on previous disconnect. force to unmap again\n");
		exynos_usb_audio_unmap_all();
	}
	ret = usb_audio_iommu_map(USB_AUDIO_XHCI_BASE, USB_AUDIO_XHCI_BASE,
			USB_AUDIO_PAGE_SIZE * 16);
	/*
	 * Check whether usb buffer was unmapped already.
	 * If not, unmap all buffers and try map again.
	 */
	if (ret == -EADDRINUSE) {
		//cancel_work_sync(&usb_audio.usb_work);
		pr_err("iommu unmapping not done. unmap here ret: %d\n", ret);
		exynos_usb_audio_unmap_all();
		ret = usb_audio_iommu_map(USB_AUDIO_XHCI_BASE,
				USB_AUDIO_XHCI_BASE, USB_AUDIO_PAGE_SIZE * 16);
	}

	if (ret) {
		pr_err("iommu mapping for in buf failed %d\n", ret);
		goto err;
	}

	/*DCBAA mapping*/
	ret = usb_audio_iommu_map(USB_AUDIO_SAVE_RESTORE, hwinfo->save_dma, USB_AUDIO_PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for save_restore buffer is failed\n");
		goto err;
	}

	/*Device Context mapping*/
	ret = usb_audio_iommu_map(USB_AUDIO_DEV_CTX, hwinfo->out_ctx, USB_AUDIO_PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for device ctx is failed\n");
		goto err;
	}

	/*Input Context mapping*/
	ret = usb_audio_iommu_map(USB_AUDIO_INPUT_CTX, hwinfo->in_ctx, USB_AUDIO_PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for input ctx is failed\n");
		goto err;
	}

	/*URAM Mapping*/
	ret = usb_audio_iommu_map(USB_URAM_BASE, USB_URAM_BASE, USB_URAM_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for URAM buffer is failed\n");
		goto err;
	}

	/* Mapping both URAM and original ERST address */
	ret = usb_audio_iommu_map(USB_AUDIO_ERST, hwinfo->erst_addr,
							USB_AUDIO_PAGE_SIZE);
	if (ret) {
		pr_err(" abox iommu mapping for erst is failed\n");
		goto err;
	}

	usb_msg.type = GIC_USB_XHCI;
	usb_msg.param1 = usb_audio.is_first_probe;
	usb_msg.param2 = hwinfo->slot_id;
	usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->erst_addr));
	usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->erst_addr));

	ret = usb_msg_to_uram(dev);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
		goto err;
	}

	usb_audio.is_first_probe = 0;
	mutex_unlock(&usb_audio.lock);

	return 0;
err:
	dev_err(&udev->dev, "%s error = %d\n", __func__, ret);
	usb_audio.is_first_probe = 0;
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_usb_audio_desc(struct usb_device *udev)
{
	int configuration, cfgno, i;
	unsigned char *buffer;
	unsigned int len = g_hwinfo->rawdesc_length;
	u64 desc_addr;
	u64 offset;
	int ret = 0;

	if (DEBUG)
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s\n", __func__);

	configuration = usb_choose_configuration(udev);

	cfgno = -1;
	for (i = 0; i < udev->descriptor.bNumConfigurations; i++) {
		if (udev->config[i].desc.bConfigurationValue ==
				configuration) {
			cfgno = i;
			pr_info("%s - chosen = %d, c = %d\n", __func__,
				i, configuration);
			break;
		}
	}

	if (cfgno == -1) {
		pr_info("%s - config select error, i=%d, c=%d\n",
			__func__, i, configuration);
		cfgno = 0;
	}

	mutex_lock(&usb_audio.lock);
	/* need to memory mapping for usb descriptor */
	buffer = udev->rawdescriptors[cfgno];
	desc_addr = virt_to_phys(buffer);
	offset = desc_addr % USB_AUDIO_PAGE_SIZE;

	/* store address information */
	usb_audio.desc_addr = desc_addr;
	usb_audio.offset = offset;

	desc_addr -= offset;

	ret = usb_audio_iommu_map(USB_AUDIO_DESC, desc_addr, (USB_AUDIO_PAGE_SIZE * 2));
	if (ret) {
		dev_err(&udev->dev, "USB AUDIO: abox iommu mapping for usb descriptor is failed\n");
		goto err;
	}

	usb_msg.type = GIC_USB_DESC;
	usb_msg.param1 = 1;
	usb_msg.param2 = len;
	usb_msg.param3 = offset;
	usb_msg.param4 = usb_audio.speed;

	if (DEBUG)
		dev_info(&udev->dev, "paddr : %#08llx / offset : %#08llx / len : %d / speed : %d\n",
							desc_addr + offset, offset, len, usb_audio.speed);

	ret = usb_msg_to_uram(&udev->dev);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb desc control failed\n");
		goto err;
	}
	mutex_unlock(&usb_audio.lock);

	dev_info(&udev->dev, "USB AUDIO: Mapping descriptor for using on Abox USB F/W & Nofity mapping is done!");

	return 0;
err:
	dev_err(&udev->dev, "%s error = %d\n", __func__, ret);
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_usb_audio_conn(struct usb_device *udev)
{
	int ret = 0;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	pr_info("USB DEVICE IS CONNECTION\n");

	mutex_lock(&usb_audio.lock);
	usb_msg.type = GIC_USB_CONN;
	usb_msg.param1 = true;

	atomic_set(&usb_audio.indeq_map_done, 0);
	atomic_set(&usb_audio.outdeq_map_done, 0);
	atomic_set(&usb_audio.fb_indeq_map_done, 0);
	atomic_set(&usb_audio.fb_outdeq_map_done, 0);
	atomic_set(&usb_audio.pcm_open_done, 0);
	ret = usb_msg_to_uram(&udev->dev);
	if (ret) {
		pr_err("erap usb conn control failed\n");
		goto err;
	}
	usb_audio.usb_audio_state = USB_AUDIO_CONNECT;
	// usb_audio_connection = 1;

err:
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_usb_audio_disconn(void)
{
	int ret = 0;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	pr_info("USB DEVICE IS DISCONNECTION\n");

	mutex_lock(&usb_audio.lock);
	usb_msg.type = GIC_USB_CONN;
	usb_msg.param1 = false;

	if (usb_audio.is_audio == 0) {
		pr_err("Is not USB Audio device\n");
		ret = -ENODEV;
		goto err;
	} else {
		usb_audio.is_audio = 0;
		usb_audio.usb_audio_state = USB_AUDIO_REMOVING;
		ret = usb_msg_to_uram(&usb_audio.phy->dev);
		if (ret) {
			pr_err("erap usb dis_conn control failed\n");
			goto err;
		}
		exynos_usb_audio_unmap_all();
	}

err:
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_usb_audio_pcm(bool is_open, bool direction)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	int ret = 0;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	if (is_open && g_hwinfo->need_first_probe)
		exynos_usb_xhci_recover();

	mutex_lock(&usb_audio.lock);

	if (usb_audio.is_audio == 0 || audio_work_data.sysmmu_mapping_done == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio = %d, sysmmu mapping = %d. return!\n",
				usb_audio.is_audio, audio_work_data.sysmmu_mapping_done);
		mutex_unlock(&usb_audio.lock);
		return -ENODEV;
	}

	if (is_open)
		atomic_set(&usb_audio.pcm_open_done, 1);

	dev_info(dev, "PCM %s %s\n", direction? "IN" : "OUT", is_open ? "OPEN" : "CLOSE");

	usb_msg.type = GIC_USB_PCM_OPEN;
	usb_msg.param1 = is_open;
	usb_msg.param2 = direction;

	ret = usb_msg_to_uram(dev);
	mutex_unlock(&usb_audio.lock);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "ERAP USB PCM control failed\n");
		return ret;
	}

	return 0;
}

/*
 * return usb scenario info
 */
int exynos_usb_scenario_info(void)
{
	return usb_user_scenario;
}
EXPORT_SYMBOL_GPL(exynos_usb_scenario_info);

void exynos_usb_wakelock(bool lock, u32 usb_scenario)
{
	if (usb_audio.low_power_call) {
		pr_info("%s: lock = %d\n", __func__, lock);

		usb_user_scenario = usb_scenario;

		if (lock)
			__pm_stay_awake(main_hcd_wakelock);
		else
			__pm_relax(main_hcd_wakelock);
	}
}

static int exynos_usb_scenario_ctl_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = AUDIO_MODE_NORMAL;
	uinfo->value.integer.max = AUDIO_MODE_CALL_SCREEN;
	return 0;
}

static int exynos_usb_scenario_ctl_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = usb->user_scenario;
	return 0;
}

static int exynos_usb_scenario_ctl_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);
	int changed = 0;
	u32 new_mode;
	int err = 0;

	if (usb->user_scenario !=
	     ucontrol->value.integer.value[0]) {
		pr_info("%s, scenario = %d, set = %ld\n", __func__,
			usb->user_scenario, ucontrol->value.integer.value[0]);
		new_mode = ucontrol->value.integer.value[0];
		/* New audio mode is call */
		if (new_mode == AUDIO_MODE_IN_CALL &&
				usb->user_scenario != AUDIO_MODE_IN_CALL) {
			err = snd_usb_autoresume(usb_audio.chip);
			pr_info("resume usage = %x, active = %d, err = %d\n",
				atomic_read(&usb_audio.chip->intf[0]->dev.power.usage_count),
				atomic_read(&usb_audio.chip->active), err);
			exynos_usb_wakelock(0, new_mode);
		} else if (usb->user_scenario == AUDIO_MODE_IN_CALL && /* previous mode */
						 new_mode != AUDIO_MODE_IN_CALL) {
			snd_usb_autosuspend(usb_audio.chip);
			pr_info("suspend usage = %x, active = %d\n",
				atomic_read(&usb_audio.chip->intf[0]->dev.power.usage_count),
				atomic_read(&usb_audio.chip->active));

			exynos_usb_wakelock(1, new_mode);
		}
		usb->user_scenario = ucontrol->value.integer.value[0];
		changed = 1;
	}

	return changed;
}

/*
 * Set bypass = 1 will skip USB suspend.
 */
static int exynos_usb_check_pm_bypass(struct usb_device *udev, int suspend)
{
	int user_scenario;
	struct usb_device	*hdev;

	hdev = udev->bus->root_hub;

	if (!exynos_main_hcd) {
		pr_info("%s: hcd 0\n", __func__);
		return false;
	}

	/* check main hcd, return if hcd is shared */
	if (exynos_main_hcd->self.root_hub != hdev)
		return false;

	user_scenario = exynos_usb_scenario_info();
	pr_info("%s: scenario = %d, suspend = %d, pm = %d\n",
		__func__, user_scenario, suspend, xhci_exynos_pm_state);

	/*
	 * Bus resume should be done for usb to work
	 * even if it's already call mode.
	 */
	if (suspend == false && xhci_exynos_pm_state != BUS_RESUME)
		return false;

	if (user_scenario == AUDIO_MODE_IN_CALL) {
		pr_info("%s: bypass = 1\n", __func__);
		return true;
	}

	return false;
}

/*
 * Set bypass = 1 will skip USB suspend.
 */
static void exynos_usb_vendor_dev_suspend(void *unused, struct usb_device *udev,
				   pm_message_t msg, int *bypass)
{
	int ret;

	ret = exynos_usb_check_pm_bypass(udev, true);
	if (ret)
		*bypass = 1;

	return;
}

/*
 * Set bypass = 1 will skip USB resume.
 */
static void exynos_usb_vendor_dev_resume(void *unused, struct usb_device *udev,
				  pm_message_t msg, int *bypass)
{
	int ret;

	ret = exynos_usb_check_pm_bypass(udev, false);
	if (ret)
		*bypass = 1;

	return;
}

int exynos_usb_vendor_helper_init(void)
{
	int ret = 0;

	ret = register_trace_android_rvh_usb_dev_suspend(exynos_usb_vendor_dev_suspend, NULL);
	if (ret)
		pr_err("register_trace_android_rvh_usb_dev_suspend failed, ret:%d\n", ret);

	ret = register_trace_android_vh_usb_dev_resume(exynos_usb_vendor_dev_resume, NULL);
	if (ret)
		pr_err("register_trace_android_vh_usb_dev_resume failed, ret:%d\n", ret);

	return ret;
}

static int exynos_usb_audio_unmap_all(void)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	u64 addr;
	u64 offset;
	int ret = 0;
	int err = 0;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	/* unmapping in pcm buffer */
	addr = usb_audio.in_buf_addr;
	if (DEBUG)
		pr_info("PCM IN BUFFER FREE: paddr = %#08llx\n", addr);

	ret = usb_audio_iommu_unmap(USB_AUDIO_PCM_INBUF, USB_AUDIO_PAGE_SIZE * 256);
	if (ret < 0) {
		pr_err("iommu un-mapping for in buf failed %d\n", ret);
		return ret;
	}

	addr = usb_audio.out_buf_addr;
	if (DEBUG)
		pr_info("PCM OUT BUFFER FREE: paddr = %#08llx\n", addr);

	ret = usb_audio_iommu_unmap(USB_AUDIO_PCM_OUTBUF, USB_AUDIO_PAGE_SIZE * 256);
	if (ret < 0) {
		err = ret;
		pr_err("iommu unmapping for pcm out buf failed\n");
	}

	/* unmapping usb descriptor */
	addr = usb_audio.desc_addr;
	offset = usb_audio.offset;

	if (DEBUG)
		pr_info("DESC BUFFER: paddr : %#08llx / offset : %#08llx\n",
				addr, offset);

	ret = usb_audio_iommu_unmap(USB_AUDIO_DESC, USB_AUDIO_PAGE_SIZE * 2);
	if (ret < 0) {
		err = ret;
		pr_err("iommu unmapping for descriptor failed\n");
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_SAVE_RESTORE, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu unmapping for dcbaa failed\n");
	}

	/*Device Context unmapping*/
	ret = usb_audio_iommu_unmap(USB_AUDIO_DEV_CTX, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu unmapping for device ctx failed\n");
	}

	/*Input Context unmapping*/
	ret = usb_audio_iommu_unmap(USB_AUDIO_INPUT_CTX, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu unmapping for input ctx failed\n");
	}

	/*ERST unmapping*/
	ret = usb_audio_iommu_unmap(USB_AUDIO_ERST, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu Un-mapping for erst failed\n");
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_IN_DEQ, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("abox iommu un-mapping for in buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_FBOUT_DEQ, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("abox iommu un-mapping for fb_out buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_OUT_DEQ, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("iommu un-mapping for out buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_FBIN_DEQ, USB_AUDIO_PAGE_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err("iommu un-mapping for fb_in buf failed %d\n", ret);
	}

	ret = usb_audio_iommu_unmap(USB_AUDIO_XHCI_BASE, USB_AUDIO_PAGE_SIZE * 16);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu Un-mapping for in buf failed\n");
	}

	ret = usb_audio_iommu_unmap(USB_URAM_BASE, USB_URAM_SIZE);
	if (ret < 0) {
		err = ret;
		pr_err(" abox iommu Un-mapping for in buf failed - URAM\n");
	}

	atomic_set(&usb_audio.unmap_all_done, 1);

	return err;
}

int exynos_usb_audio_pcmbuf(struct usb_device *udev)
{

	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	struct hcd_hw_info *hwinfo = g_hwinfo;
	u64 out_dma = 0;
	int ret = 0;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	mutex_lock(&usb_audio.lock);

	if (usb_audio.is_audio == 0 || audio_work_data.sysmmu_mapping_done == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio = %d, sysmmu mapping = %d. return!\n",
				usb_audio.is_audio, audio_work_data.sysmmu_mapping_done);
		mutex_unlock(&usb_audio.lock);
		return -ENODEV;
	}

	out_dma = iommu_iova_to_phys(iommu_domain, USB_AUDIO_PCM_OUTBUF);
	usb_msg.type = GIC_USB_PCM_BUF;
	usb_msg.param1 = lower_32_bits(le64_to_cpu(out_dma));
	usb_msg.param2 = upper_32_bits(le64_to_cpu(out_dma));
	usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->in_dma));
	usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->in_dma));

	if (DEBUG) {
		pr_info("pcm out data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(out_dma)),
				lower_32_bits(le64_to_cpu(out_dma)));
		pr_info("pcm in data buffer pa addr : %#08x %08x\n",
				upper_32_bits(le64_to_cpu(hwinfo->in_dma)),
				lower_32_bits(le64_to_cpu(hwinfo->in_dma)));
		pr_info("erap param2 : %#08x param1 : %08x\n",
				usb_msg.param2, usb_msg.param1);
		pr_info("erap param4 : %#08x param3 : %08x\n",
				usb_msg.param4, usb_msg.param3);
	}

	ret = usb_msg_to_uram(dev);
	mutex_unlock(&usb_audio.lock);

	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb transfer pcm buffer is failed\n");
		return ret; /* need to fix err num */
	}

	return 0;
}

static unsigned int xhci_exynos_get_endpoint_address(unsigned int ep_index)
{
	unsigned int number = DIV_ROUND_UP(ep_index, 2);
	unsigned int direction = ep_index % 2 ? USB_DIR_OUT : USB_DIR_IN;
	return direction | number;
}

void exynos_usb_audio_get_deq(struct usb_device *udev, int interface, int alternate, int direction)
{
	struct usb_host_config *config;
	struct usb_interface *iface;
	struct usb_host_interface *alt;
	struct usb_host_endpoint *host_ep;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_ep_ctx *ep_ctx;
	struct usb_endpoint_descriptor *desc;
	unsigned int i, ep_index, ep_addr;
	int fb_out_change = 0;
	int fb_in_change = 0;

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	xhci = usb_audio.sb->xhci;
	virt_dev = usb_audio.sb->vdev;
#else
       xhci = g_xhci_exynos_audio->xhci;
       virt_dev = xhci->devs[usb_audio.udev->slot_id];
#endif

	config = udev->actconfig;
	iface = usb_ifnum_to_if(udev, interface);
	alt = usb_altnum_to_altsetting(iface, alternate);

	for (i = 0; i < alt->desc.bNumEndpoints; i++) {
		host_ep = &(alt->endpoint[i]);
		desc = &(host_ep->desc);
		ep_index = xhci_get_endpoint_index(desc);
		ep_addr = xhci_exynos_get_endpoint_address(ep_index);
		ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->out_ctx, ep_index);

		pr_debug("%s: bmAttributes = %x\n", __func__, desc->bmAttributes);
		pr_debug("%s: bLength = %x\n", __func__, desc->bLength);
		pr_debug("%s: bEndpointAddress = %x\n", __func__, desc->bEndpointAddress);
		pr_debug("%s: bSynchAddress = %x\n", __func__, desc->bSynchAddress);

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
		xhci_sideband_add_endpoint(usb_audio.sb, host_ep);
		pr_info("%s xhci_sideband_add_endpoint ep_index: %d\n", __func__, ep_index);
#endif

		/* Only Feedback endpoint(Not implict feedback data endpoint) */
		if ((desc->bmAttributes & USB_ENDPOINT_USAGE_MASK) == USB_ENDPOINT_USAGE_FEEDBACK) {
			pr_info("%s Only Feedback EP\n", __func__);
			if (usb_endpoint_out(ep_addr)) { // OUT
				pr_info("Feedback OUT ISO endpoint #0%x 0x%x\n",
					desc->bEndpointAddress, desc->bSynchAddress);
				if (fb_out_change == 0) {
					g_hwinfo->fb_old_out_deq = g_hwinfo->fb_out_deq;
					g_hwinfo->fb_out_deq = ep_ctx->deq;
					pr_info("[%s] ep%d set fb out deq : %#08llx\n", __func__, ep_index,
						g_hwinfo->fb_out_deq);
					fb_out_change = 1;
				} else {
					pr_info("fb_out_deq is already changed %#08llx\n", ep_ctx->deq);
				}
			} else if(!(usb_endpoint_out(ep_addr))){ //IN
				pr_info("Feedback IN ISO endpoint #0%x 0x%x\n",
					desc->bEndpointAddress, desc->bSynchAddress);
				if (fb_in_change == 0) {
					g_hwinfo->fb_old_in_deq = g_hwinfo->fb_in_deq;
					g_hwinfo->fb_in_deq = ep_ctx->deq;
					pr_info("[%s] ep%d set fb in deq : %#08llx\n", __func__, ep_index,
						g_hwinfo->fb_in_deq);
					fb_in_change = 1;
				} else {
					pr_info("fb_in_deq is already changed %#08llx\n", ep_ctx->deq);
				}
			} else
				pr_info("[%s] ep%d set feedback deq error!\n", __func__, ep_index);

		} else { /* Non-Feedback EP */
			pr_info("%s Non-Feedback EP\n", __func__);
			if ((direction == SNDRV_PCM_STREAM_PLAYBACK) && usb_endpoint_out(ep_addr)) { // OUT
				pr_info(" This is OUT ISO endpoint #0%x 0x%x\n",
					desc->bEndpointAddress, desc->bSynchAddress);
				g_hwinfo->old_out_deq = g_hwinfo->out_deq;
				g_hwinfo->out_deq = ep_ctx->deq;
				pr_info("[%s] ep%d set out deq : %#08llx\n", __func__, ep_index, g_hwinfo->out_deq);
				if ((desc->bLength > 7) && (desc->bSynchAddress != 0x0)) { //Feedback IN
					pr_info ("Feedback IN ISO endpoint #0%x 0x%x\n",
						desc->bEndpointAddress, desc->bSynchAddress);
					ep_index = (desc->bSynchAddress & USB_ENDPOINT_NUMBER_MASK) * 2;
					ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->out_ctx, ep_index);
					if (fb_in_change == 0) {
						g_hwinfo->fb_old_in_deq = g_hwinfo->fb_in_deq;
						g_hwinfo->fb_in_deq = ep_ctx->deq;
						pr_info("[%s] ep%d set fb in deq : %#08llx\n", __func__, ep_index,
							g_hwinfo->fb_in_deq);
						fb_in_change = 1;
					} else {
						pr_info("fb_in_deq is already changed %#08llx\n", ep_ctx->deq);
					}
				}
			} else if((direction == SNDRV_PCM_STREAM_CAPTURE) && !(usb_endpoint_out(ep_addr)) ){ // IN
				pr_info(" This is IN ISO endpoint #0%x 0x%x\n",
					desc->bEndpointAddress, desc->bSynchAddress);
				g_hwinfo->old_in_deq = g_hwinfo->in_deq;
				g_hwinfo->in_deq = ep_ctx->deq;
				pr_info("[%s] ep%d set in deq : %#08llx\n", __func__, ep_index, g_hwinfo->in_deq);
				if ((desc->bLength > 7) && (desc->bSynchAddress != 0x0)) { // Feedback OUT
					pr_info ("Feedback OUT ISO endpoint #0%x 0x%x\n",
						desc->bEndpointAddress, desc->bSynchAddress);
					ep_index = desc->bSynchAddress;
					ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->out_ctx, ep_index);
					if (fb_out_change == 0) {
						g_hwinfo->fb_old_out_deq = g_hwinfo->fb_out_deq;
						g_hwinfo->fb_out_deq = ep_ctx->deq;
						pr_info("[%s] ep%d set fb out deq : %#08llx\n", __func__, ep_index,
							g_hwinfo->fb_out_deq);
						fb_out_change = 1;
					} else {
						pr_info("fb_out_deq is already changed %#08llx\n", ep_ctx->deq);
					}
				}
			} else
				pr_info("[%s] ep%d set non-feedback deq error!\n", __func__, ep_index);
		}
	}

}
int exynos_usb_audio_setintf(struct usb_device *udev, int iface, int alt, int direction)
{
	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	struct hcd_hw_info *hwinfo = g_hwinfo;
	int ret;
	u64 in_offset, out_offset;

	if (DEBUG)
		dev_info(&udev->dev, "USB_AUDIO_IPC : %s, alt = %d\n",
				__func__, alt);

	if (g_hwinfo->need_first_probe)
		exynos_usb_xhci_recover();

	mutex_lock(&usb_audio.lock);

	if (atomic_read(&usb_audio.pcm_open_done) == 0) {
		dev_info(dev, "USB_AUDIO_IPC : pcm node was not opened!\n");
		ret = -EPERM;
		goto err;
	}

	if (usb_audio.is_audio == 0 || audio_work_data.sysmmu_mapping_done == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio = %d, sysmmu mapping = %d. return!\n",
				usb_audio.is_audio, audio_work_data.sysmmu_mapping_done);
		ret = -ENODEV;
		goto err;
	}

	exynos_usb_audio_get_deq(udev, iface, alt, direction);

	usb_msg.type = GIC_USB_SET_INTF;
	usb_msg.param1 = alt;
	usb_msg.param2 = iface;

	if (direction == SNDRV_PCM_STREAM_CAPTURE) {
		/* IN EP */
		dev_dbg(dev, "in deq : %#08llx, %#08llx, %#08llx, %#08llx\n",
				hwinfo->in_deq, hwinfo->old_in_deq,
				hwinfo->fb_out_deq, hwinfo->fb_old_out_deq);
		if (!atomic_read(&usb_audio.indeq_map_done) ||
			(hwinfo->in_deq != hwinfo->old_in_deq)) {
			dev_info(dev, "in_deq map required\n");
			if (atomic_read(&usb_audio.indeq_map_done)) {
				/* Blocking SYSMMU Fault by ABOX */
				usb_msg.param1 = 0;
				ret = usb_msg_to_uram(dev);
				if (ret) {
					dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
					goto err;
				}
				usb_msg.param1 = alt;
				ret = usb_audio_iommu_unmap(USB_AUDIO_IN_DEQ, USB_AUDIO_PAGE_SIZE);
				if (ret < 0) {
					pr_err("un-map of in buf failed %d\n", ret);
					goto err;
				}
			}
			atomic_set(&usb_audio.indeq_map_done, 1);

			in_offset = hwinfo->in_deq % USB_AUDIO_PAGE_SIZE;
			ret = usb_audio_iommu_map(USB_AUDIO_IN_DEQ,
					(hwinfo->in_deq - in_offset),
					USB_AUDIO_PAGE_SIZE);
			if (ret < 0) {
				pr_err("map for in buf failed %d\n", ret);
				goto err;
			}
		}

		if (hwinfo->fb_out_deq) {
			dev_dbg(dev, "fb_out deq : %#08llx\n", hwinfo->fb_out_deq);
			if (!atomic_read(&usb_audio.fb_outdeq_map_done) ||
					(hwinfo->fb_out_deq != hwinfo->fb_old_out_deq)) {
				if (atomic_read(&usb_audio.fb_outdeq_map_done)) {
					/* Blocking SYSMMU Fault by ABOX */
					usb_msg.param1 = 0;
					ret = usb_msg_to_uram(dev);
					if (ret) {
						dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
						goto err;
					}
					usb_msg.param1 = alt;
					ret = usb_audio_iommu_unmap(USB_AUDIO_FBOUT_DEQ,
							USB_AUDIO_PAGE_SIZE);
					if (ret < 0) {
						pr_err("un-map for fb_out buf failed %d\n", ret);
						goto err;
					}
				}
				atomic_set(&usb_audio.fb_outdeq_map_done, 1);

				out_offset = hwinfo->fb_out_deq % USB_AUDIO_PAGE_SIZE;
				ret = usb_audio_iommu_map(USB_AUDIO_FBOUT_DEQ,
						(hwinfo->fb_out_deq - out_offset),
						USB_AUDIO_PAGE_SIZE);
				if (ret < 0) {
					pr_err("map for fb_out buf failed %d\n", ret);
					goto err;
				}
			}
		}
		usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->in_deq));
		usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->in_deq));
	} else {
		/* OUT EP */
		dev_dbg(dev, "out deq : %#08llx, %#08llx, %#08llx, %#08llx\n",
				hwinfo->out_deq, hwinfo->old_out_deq,
				hwinfo->fb_in_deq, hwinfo->fb_old_in_deq);
		if (!atomic_read(&usb_audio.outdeq_map_done) ||
			(hwinfo->out_deq != hwinfo->old_out_deq)) {
			dev_info(dev, "out_deq map required\n");
			if (atomic_read(&usb_audio.outdeq_map_done)) {
				/* Blocking SYSMMU Fault by ABOX */
				usb_msg.param1 = 0;
				ret = usb_msg_to_uram(dev);
				if (ret) {
					dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
					goto err;
				}
				usb_msg.param1 = alt;
				dev_info(dev, "USB_AUDIO_OUT_DEQ unmap\n");
				ret = usb_audio_iommu_unmap(USB_AUDIO_OUT_DEQ, USB_AUDIO_PAGE_SIZE);
				if (ret < 0) {
					pr_err("un-map for out buf failed %d\n", ret);
					goto err;
				}
			}
			atomic_set(&usb_audio.outdeq_map_done, 1);

			out_offset = hwinfo->out_deq % USB_AUDIO_PAGE_SIZE;
			ret = usb_audio_iommu_map(USB_AUDIO_OUT_DEQ,
					(hwinfo->out_deq - out_offset),
					USB_AUDIO_PAGE_SIZE);
			if (ret < 0) {
				pr_err("map for out buf failed %d\n", ret);
				goto err;
			}
		}

		if (hwinfo->fb_in_deq) {
			dev_info(dev, "fb_in deq : %#08llx\n", hwinfo->fb_in_deq);
			if (!atomic_read(&usb_audio.fb_indeq_map_done) ||
					(hwinfo->fb_in_deq != hwinfo->fb_old_in_deq)) {
				if (atomic_read(&usb_audio.fb_indeq_map_done)) {
					/* Blocking SYSMMU Fault by ABOX */
					usb_msg.param1 = 0;
					ret = usb_msg_to_uram(dev);
					if (ret) {
						dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
						goto err;
					}
					usb_msg.param1 = alt;
					ret = usb_audio_iommu_unmap(USB_AUDIO_FBIN_DEQ,
							USB_AUDIO_PAGE_SIZE);
					if (ret < 0) {
						pr_err("un-map for fb_in buf failed %d\n", ret);
						goto err;
					}
				}
				atomic_set(&usb_audio.fb_indeq_map_done, 1);

				in_offset = hwinfo->fb_in_deq % USB_AUDIO_PAGE_SIZE;
				ret = usb_audio_iommu_map(USB_AUDIO_FBIN_DEQ,
						(hwinfo->fb_in_deq - in_offset),
						USB_AUDIO_PAGE_SIZE);
				if (ret < 0) {
					pr_err("map for fb_in buf failed %d\n", ret);
					goto err;
				}
			}
		}

		usb_msg.param3 = lower_32_bits(le64_to_cpu(hwinfo->out_deq));
		usb_msg.param4 = upper_32_bits(le64_to_cpu(hwinfo->out_deq));
	}

	/* one more check connection to prevent kernel panic */
	if (usb_audio.is_audio == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio is 0. return!\n");
		ret = -ENODEV;
		goto err;
	}

	ret = usb_msg_to_uram(dev);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb hcd control failed\n");
		goto err;
	}
	mutex_unlock(&usb_audio.lock);

	if (DEBUG) {
		dev_info(&udev->dev, "Alt#%d / Intf#%d / Direction %s / EP DEQ : %#08x %08x\n",
				usb_msg.param1, usb_msg.param2,
				direction ? "IN" : "OUT",
				usb_msg.param4, usb_msg.param3);
	}

	return 0;

err:
	dev_err(&udev->dev, "%s error = %d\n", __func__, ret);
	mutex_unlock(&usb_audio.lock);
	return ret;
}

static int exynos_pcm_open_ctl_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xFFFFFFFF;
	return 0;
}

static int exynos_pcm_open_ctl_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = usb->pcm_index;
	return 0;
}

#define SND_PCM_ONOFF_MASK 0xffff0000
#define SND_PCM_DEV_NUM_MASK 0xff00
#define SND_PCM_STREAM_DIRECTION 0xff

static int exynos_pcm_open_ctl_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs = NULL;
	int changed = 0;
	u32 value, pcm_idx, dir, onoff;

	value = ucontrol->value.integer.value[0];
	//dir = 0; /* SNDRV_PCM_STREAM_PLAYBACK */

	pr_info("%s, value = 0x%x\n", __func__, value);

	dir = value & SND_PCM_STREAM_DIRECTION;
	pcm_idx = (value & SND_PCM_DEV_NUM_MASK) >> 8;
	onoff = (value & SND_PCM_ONOFF_MASK) >> 16;

	if (dir >= ARRAY_SIZE(as->substream)) {
		pr_err("%s: invalid dir %u >= %ld\n", __func__, dir,
				ARRAY_SIZE(as->substream));
		goto open_failed;
	}

	if (pcm_idx >= usb->chip->pcm_devs) {
		pr_err("%s: invalid pcm dev number %u > %d\n", __func__,
			pcm_idx, usb->chip->pcm_devs);
		goto open_failed;
	}

	list_for_each_entry(as, &usb->chip->pcm_list, list) {
		if (as->pcm_index == pcm_idx) {
			pr_info("%s matched. idx = %d\n", __func__, as->pcm_index);
			subs = &as->substream[dir];
		}
	}

	if (subs == NULL) {
		pr_info("%s: subs is NULL\n", __func__);
		goto open_failed;
	}

	if (onoff == 1) {
		exynos_usb_audio_pcm(1, dir);
	} else if (onoff == 0) {
		if (atomic_read(&usb_audio.pcm_open_done) == 0) {
			pr_info("%s : pcm node was not opened!\n", __func__);
			return 0;
		}
		exynos_usb_audio_pcm(0, dir);
	}
	changed = 1;

	set_bit(SUBSTREAM_FLAG_DATA_EP_STARTED, &subs->flags);
	set_bit(SUBSTREAM_FLAG_SYNC_EP_STARTED, &subs->flags);

open_failed:
	return changed;
}

static int exynos_pcm_prepare_ctl_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xFFFFFFFF;
	return 0;
}

static int exynos_pcm_prepare_ctl_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = usb->pcm_index;
	return 0;
}

static int exynos_pcm_prepare_ctl_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct exynos_usb_audio *usb = snd_kcontrol_chip(kcontrol);
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs = NULL;
	//struct usb_interface *usb_iface;
	struct usb_device *udev;
	int changed = 0;
	u32 value, pcm_idx, dir, onoff;
	int ret = 0;

	value = ucontrol->value.integer.value[0];
	//dir = 0; /* SNDRV_PCM_STREAM_PLAYBACK */

	pr_info("%s, value = 0x%x\n", __func__, value);

	dir = value & SND_PCM_STREAM_DIRECTION;
	pcm_idx = (value & SND_PCM_DEV_NUM_MASK) >> 8;
	onoff = (value & SND_PCM_ONOFF_MASK) >> 16;

	if (dir >= ARRAY_SIZE(as->substream)) {
		pr_err("%s: invalid dir %u >= %ld\n", __func__, dir,
				ARRAY_SIZE(as->substream));
		goto prepare_failed;
	}

	if (pcm_idx >= usb->chip->pcm_devs) {
		pr_err("%s: invalid pcm dev number %u > %d\n", __func__,
			pcm_idx, usb->chip->pcm_devs);
		goto prepare_failed;
	}

	list_for_each_entry(as, &usb->chip->pcm_list, list) {
		if (as->pcm_index == pcm_idx) {
			pr_info("%s matched. idx = %d\n", __func__, as->pcm_index);
			subs = &as->substream[dir];
		}
	}
	//usb_iface = usb_ifnum_to_if(subs->stream->chip->dev,\
	//			    subs->data_endpoint->iface);
	//udev = interface_to_usbdev(usb_iface);

	if (subs == NULL) {
		pr_info("%s: subs is NULL\n", __func__);
		goto prepare_failed;
	}

	udev = subs->dev;

	exynos_usb_audio_pcmbuf(udev);
	exynos_usb_audio_setintf(udev, subs->data_endpoint->iface, \
			subs->data_endpoint->altsetting, subs->direction);
	ret = exynos_usb_audio_set_rate(subs->data_endpoint->cur_audiofmt->iface,\
			subs->data_endpoint->cur_rate, subs->data_endpoint->cur_audiofmt->altsetting);
	changed = 1;

prepare_failed:
	return changed;
}

static int exynos_usb_add_ctls(struct snd_usb_audio *chip,
				unsigned long private_value)
{
	struct snd_kcontrol_new knew = {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.name = "USB Audio Mode",
		.info = exynos_usb_scenario_ctl_info,
		.get = exynos_usb_scenario_ctl_get,
		.put = exynos_usb_scenario_ctl_put,
	};

	struct snd_kcontrol_new knew_open = {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.name = "USB Audio Open",
		.info = exynos_pcm_open_ctl_info,
		.get = exynos_pcm_open_ctl_get,
		.put = exynos_pcm_open_ctl_put,
	};

	struct snd_kcontrol_new knew_prepare = {
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.name = "USB Audio Prepare",
		.info = exynos_pcm_prepare_ctl_info,
		.get = exynos_pcm_prepare_ctl_get,
		.put = exynos_pcm_prepare_ctl_put,
	};

	int err;

	if (!chip)
		return -ENODEV;

	/* Call mode */
	knew.private_value = private_value;
	usb_audio.kctl = snd_ctl_new1(&knew, &usb_audio);
	if (!usb_audio.kctl) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl new error\n", __func__);
		return -ENOMEM;
	}
	err = snd_ctl_add(chip->card, usb_audio.kctl);
	if (err < 0) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl add error\n", __func__);
		return err;
	}

	/* PCM Open */
	knew_open.private_value = private_value;
	usb_audio.kctl1 = snd_ctl_new1(&knew_open, &usb_audio);
	if (!usb_audio.kctl1) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl1 new error\n", __func__);
		return -ENOMEM;
	}
	err = snd_ctl_add(chip->card, usb_audio.kctl1);
	if (err < 0) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl1 add error\n", __func__);
		return err;
	}

	/* PCM Prepare */
	knew_prepare.private_value = private_value;
	usb_audio.kctl2 = snd_ctl_new1(&knew_prepare, &usb_audio);
	if (!usb_audio.kctl2) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl2 new error\n", __func__);
		return -ENOMEM;
	}
	err = snd_ctl_add(chip->card, usb_audio.kctl2);
	if (err < 0) {
		dev_err(&usb_audio.udev->dev,
			"USB_AUDIO_IPC : %s-ctl2 add error\n", __func__);
		return err;
	}
	pr_info("%s, card_num = %d, pcm_devs = %d\n", __func__,
		chip->card->number, chip->pcm_devs);

	return 0;
}

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
void exynos_usb_audio_offload_store_sideband_info(struct usb_device *udev)
{
	struct xhci_sideband *sb = usb_audio.sb;
	struct xhci_hcd *xhci = sb->xhci;
	struct xhci_virt_device *virt_dev = sb->vdev;
	struct xhci_erst_entry *entry = xhci->interrupters[1]->erst.entries;

	//virt_dev = xhci->devs[usb_audio.udev->slot_id];

	g_hwinfo->dcbaa_dma = xhci->dcbaa->dma;
	g_hwinfo->save_dma = g_xhci_exynos_audio->save_dma;
	//g_hwinfo->cmd_ring = xhci->op_regs->cmd_ring;
	g_hwinfo->slot_id = usb_audio.udev->slot_id;
	g_hwinfo->in_dma = g_xhci_exynos_audio->in_dma;
	g_hwinfo->in_buf = g_xhci_exynos_audio->in_addr;
	g_hwinfo->out_dma = g_xhci_exynos_audio->out_dma;
	g_hwinfo->out_buf = g_xhci_exynos_audio->out_addr;
	g_hwinfo->in_ctx = virt_dev->in_ctx->dma;
	g_hwinfo->out_ctx = virt_dev->out_ctx->dma;
	g_hwinfo->erst_addr = entry->seg_addr;
	g_hwinfo->speed = usb_audio.udev->speed;
	g_hwinfo->phy = usb_audio.phy;

	pr_info("<<< %s\n", __func__);
}
#else
static void exynos_usb_audio_offload_store_hw_info(void)
{
	struct xhci_hcd *xhci = g_xhci_exynos_audio->xhci;
	struct xhci_virt_device *virt_dev;
	struct xhci_erst_entry *entry;

	g_xhci_exynos_audio->ir = &(g_xhci_exynos_audio->ir_data);
	entry = &g_xhci_exynos_audio->ir->erst.entries[0];

	virt_dev = xhci->devs[usb_audio.udev->slot_id];

	g_hwinfo->dcbaa_dma = xhci->dcbaa->dma;
	g_hwinfo->save_dma = g_xhci_exynos_audio->save_dma;
	//g_hwinfo->cmd_ring = xhci->op_regs->cmd_ring;
	g_hwinfo->slot_id = usb_audio.udev->slot_id;
	g_hwinfo->in_dma = g_xhci_exynos_audio->in_dma;
	g_hwinfo->in_buf = g_xhci_exynos_audio->in_addr;
	g_hwinfo->out_dma = g_xhci_exynos_audio->out_dma;
	g_hwinfo->out_buf = g_xhci_exynos_audio->out_addr;
	g_hwinfo->in_ctx = virt_dev->in_ctx->dma;
	g_hwinfo->out_ctx = virt_dev->out_ctx->dma;
	g_hwinfo->erst_addr = entry->seg_addr;
	g_hwinfo->speed = usb_audio.udev->speed;
	g_hwinfo->phy = g_xhci_exynos_audio->phy;

	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)
		g_hwinfo->use_uram = true;
	else
		g_hwinfo->use_uram = false;

	pr_info("<<< %s\n", __func__);
}
#endif

void usb_probe_work(void)
{
	if (!audio_work_data.is_conn) {
		pr_info("%s, already disconnected, return \n", __func__);
		return;
	}

	exynos_usb_audio_hcd(audio_work_data.work_udev);
	exynos_usb_audio_conn(audio_work_data.work_udev);
	exynos_usb_audio_desc(audio_work_data.work_udev);
	exynos_usb_audio_add_control(usb_audio.chip);
	audio_work_data.sysmmu_mapping_done = 1;
}

static int xhci_exynos_audio_alloc(struct device *parent)
{
	dma_addr_t	dma;

	dev_info(parent, "%s\n", __func__);

	g_xhci_exynos_audio = devm_kzalloc(parent, sizeof(struct xhci_exynos_audio), GFP_KERNEL);
	memset(g_xhci_exynos_audio, 0, sizeof(struct xhci_exynos_audio));

	g_xhci_exynos_audio->save_addr = dma_alloc_coherent(parent,
			USB_AUDIO_PAGE_SIZE, &dma, GFP_KERNEL);
	g_xhci_exynos_audio->save_dma = dma;
	dev_info(parent, "// Save address = 0x%llx (DMA), %p (virt)",
		  (unsigned long long)g_xhci_exynos_audio->save_dma, g_xhci_exynos_audio->save_addr);

	/* In data buf alloc */
	g_xhci_exynos_audio->in_addr = dma_alloc_coherent(parent,
			(USB_AUDIO_PAGE_SIZE * 256), &dma, GFP_KERNEL);
	g_xhci_exynos_audio->in_dma = dma;
	dev_info(parent, "// IN Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)g_xhci_exynos_audio->in_dma, g_xhci_exynos_audio->in_addr);

	/* Out data buf alloc */
	g_xhci_exynos_audio->out_addr = dma_alloc_coherent(parent,
			(USB_AUDIO_PAGE_SIZE * 256), &dma, GFP_KERNEL);
	g_xhci_exynos_audio->out_dma = dma;
	dev_info(parent, "// OUT Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)g_xhci_exynos_audio->out_dma, g_xhci_exynos_audio->out_addr);

	return 0;
}

int exynos_usb_audio_offload_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *np_abox;
	struct device_node *np_tmp;
	struct platform_device *pdev_abox;
	struct platform_device *pdev_tmp;
	struct device	*dev_gic;
	int	err = 0;
	int value;

	if (DEBUG)
		dev_info(dev, "USB_AUDIO_IPC : %s\n", __func__);

	usb_audio.dev = dev;

	np_abox = of_parse_phandle(np, "abox", 0);
	if (!np_abox) {
		dev_err(dev, "Failed to get abox device node\n");
		goto probe_failed;
	}

	pdev_abox = of_find_device_by_node(np_abox);
	if (!pdev_abox) {
		dev_err(dev, "Failed to get abox platform device\n");
		goto probe_failed;
	}

	iommu_domain = iommu_get_domain_for_dev(&pdev_abox->dev);
	if (!iommu_domain) {
		dev_err(dev, "Failed to get iommu domain\n");
		goto probe_failed;
	}

	np_tmp = of_parse_phandle(np, "samsung,abox-gic", 0);
	if (!np_tmp) {
		dev_err(dev, "Failed to get abox-gic device node\n");
		goto probe_failed;
	}

	pdev_tmp = of_find_device_by_node(np_tmp);
	if (!pdev_tmp) {
		dev_err(dev, "Failed to get abox-gic platform device\n");
		goto probe_failed;
	}

	if (!of_property_read_u32(dev->of_node, "low_power_call", &value)) {
		usb_audio.low_power_call = value ? true : false;
		dev_info(dev, "low_power_call = %d\n", usb_audio.low_power_call);
	} else {
		dev_err(dev, "can't get low_power_call\n");
		usb_audio.low_power_call = false;
	}

	base_addr = ioremap(EXYNOS_URAM_ABOX_EMPTY_ADDR, EMPTY_SIZE);
	if (!base_addr) {
		dev_err(dev, "URAM ioremap failed\n");
		goto probe_failed;
	}

	dev_gic = &pdev_tmp->dev;
	gic_data = dev_get_drvdata(dev_gic);

	/* Get USB2.0 PHY for main hcd */
	usb_audio.phy = devm_phy_get(dev, "usb2-phy");
	if (IS_ERR_OR_NULL(usb_audio.phy)) {
		usb_audio.phy = NULL;
		dev_err(dev, "%s: failed to get phy\n", __func__);
	}

	mutex_init(&usb_audio.lock);
	mutex_init(&usb_audio.msg_lock);

	usb_audio.hcd_pdev = pdev;
	usb_audio.abox = pdev_abox;

	//exynos_usb_audio_offload_init();
	usb_audio.usb_audio_state = USB_AUDIO_DISCONNECT;
	atomic_set(&usb_audio.unmap_all_done, 1);
#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	usb_audio.sb = NULL;
#endif
	exynos_usb_vendor_helper_init();

	g_hwinfo = devm_kzalloc(dev, sizeof(struct hcd_hw_info), GFP_KERNEL);
	memset(g_hwinfo, 0, sizeof(struct hcd_hw_info));

	snd_usb_register_platform_ops(&platform_ops);
	xhci_exynos_audio_alloc(usb_audio.dev);

	return 0;

probe_failed:
	err = -EPROBE_DEFER;

	return err;
}

static void exynos_usb_audio_offload_init(void)
{
	pr_info("%s\n", __func__);
	/* usb_audio initialization */

	if (g_hwinfo->need_first_probe) {
		usb_audio.is_first_probe = 1;
		g_hwinfo->need_first_probe = false;
	}

	usb_audio.udev = NULL;
	usb_audio.is_audio = 0;
	usb_audio.user_scenario = AUDIO_MODE_NORMAL;
	usb_audio.usb_audio_state = USB_AUDIO_DISCONNECT;
	// usb_audio_connection = 0;
	xhci_exynos_pm_state = BUS_RESUME;
	usb_user_scenario = 0;

	atomic_set(&usb_audio.indeq_map_done, 0);
	atomic_set(&usb_audio.outdeq_map_done, 0);
	atomic_set(&usb_audio.fb_indeq_map_done, 0);
	atomic_set(&usb_audio.fb_outdeq_map_done, 0);
	atomic_set(&usb_audio.pcm_open_done, 0);

	memset(g_hwinfo, 0, sizeof(struct hcd_hw_info));

	/* Get USB2.0 PHY for main hcd */
	//usb_audio.phy = g_xhci_exynos->phy_usb2;
	//pr_info("jdh: %s: set 2.0 phy!\n", __func__);
}

static struct usb_interface *exynos_find_audio_interface(struct snd_usb_audio *chip)
{
	struct usb_host_config *config = chip->dev->actconfig;
	struct usb_interface *intf;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	int i;

	if (!config) {
		pr_err("%s: config is NULL\n", __func__);
		return NULL;
	}

	for (i = 0; i < config->desc.bNumInterfaces; i++) {
		intf = config->interface[i];
		alts = &intf->altsetting[0];
		altsd = get_iface_desc(alts);
		pr_info("%s: intf[%d] Subclass 0x%02x\n", __func__, i,
						altsd->bInterfaceSubClass);
		if (altsd->bInterfaceClass == USB_CLASS_AUDIO &&
			altsd->bInterfaceSubClass == USB_SUBCLASS_AUDIOCONTROL) {
			return intf;
		}
	}

	pr_err("%s: Can't find USB_SUBCLASS_AUDIOCONTROL\n", __func__);
	return NULL;
}

/* card */
void exynos_usb_audio_connect(struct snd_usb_audio *chip)
{
	struct usb_interface *intf;
	struct usb_device *udev;
	int ret;

	intf = exynos_find_audio_interface(chip);
	if (intf == NULL) {
		pr_info("USB_AUDIO_IPC : %s - usb interface is NULL\n", __func__);
		return;
	}

	pr_info("USB_AUDIO_IPC : %s - USB Audio device detected!\n", __func__);
	udev = interface_to_usbdev(intf);

	if ((usb_audio.usb_audio_state == USB_AUDIO_DISCONNECT)
		|| (usb_audio.usb_audio_state == USB_AUDIO_REMOVING)) {
		pr_info("USB_AUDIO_IPC : %s - USB Audio set!\n", __func__);
		exynos_usb_audio_offload_init();

		ret = exynos_usb_audio_set_device(udev);
		if (ret < 0)
			return;

		audio_work_data.work_udev = udev;
		audio_work_data.is_conn = 1;
		usb_audio.chip = chip;
		usb_probe_work();
		if (!audio_work_data.is_conn) {
			pr_info("%s, already disconnected, return\n", __func__);
			return;
		}

		for (int timeout = 300;(audio_work_data.sysmmu_mapping_done != 1); timeout--) {
			mdelay(1);
			if (!timeout) {
				pr_info("%s - USB Audio is no set!\n", __func__);
				break;
			}
		}

		exynos_usb_audio_map_buf(udev);

		if (udev->do_remote_wakeup)
			usb_enable_autosuspend(udev);
	} else {
		pr_err("USB audio is can not support second device!!");
	}
}

void exynos_usb_audio_disconnect(struct snd_usb_audio *chip)
{
#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
#endif
	int ret;

	audio_work_data.is_conn = 0;
	audio_work_data.sysmmu_mapping_done = 0;
	usb_audio.user_scenario = AUDIO_MODE_NORMAL;
	phy_set_mode_ext(usb_audio.phy, PHY_MODE_CALL_EXIT, AUDIO_MODE_NORMAL);

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	if (usb_audio.sb) {
		ret = xhci_exynos_disable_interrupter(usb_audio.sb);
		if (ret < 0)
			pr_info("%s: return ERR %d\n", __func__, ret);
	}
#endif
	ret = exynos_usb_audio_disconn();
	if (ret < 0)
		pr_info("%s: return ERR %d\n", __func__, ret);

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	if (usb_audio.sb) {
		xhci = usb_audio.sb->xhci;
		virt_dev = usb_audio.sb->vdev;
		if (virt_dev == xhci->devs[usb_audio.udev->slot_id]) {
			xhci_sideband_unregister(usb_audio.sb);
			usb_audio.sb = NULL;
		} else {
			dev_err(&usb_audio.udev->dev, "vdev unmatch. skip unregister sideband\n");
		}
	} else {
		pr_err("usb_audio.sb is NULL\n");
	}
#endif
}

/* pcm */
int exynos_usb_audio_set_rate(int iface, int rate, int alt)
{

	struct platform_device *pdev = usb_audio.abox;
	struct device *dev = &pdev->dev;
	int ret;

	if (DEBUG)
		pr_info("USB_AUDIO_IPC : %s\n", __func__);

	mutex_lock(&usb_audio.lock);

	if (usb_audio.is_audio == 0 || audio_work_data.sysmmu_mapping_done == 0) {
		dev_info(dev, "USB_AUDIO_IPC : is_audio = %d, sysmmu mapping = %d. return!\n",
				usb_audio.is_audio, audio_work_data.sysmmu_mapping_done);
		mutex_unlock(&usb_audio.lock);
		return -ENODEV;
	}

	usb_msg.type = GIC_USB_SAMPLE_RATE;
	usb_msg.param1 = iface;
	usb_msg.param2 = rate;
	usb_msg.param3 = alt;

	ret = usb_msg_to_uram(dev);
	mutex_unlock(&usb_audio.lock);
	if (ret) {
		dev_err(&usb_audio.udev->dev, "erap usb transfer sample rate is failed\n");
		return ret;
	}

	return 0;
}

int exynos_usb_audio_add_control(struct snd_usb_audio *chip)
{
	int ret;

	if (chip != NULL) {
		exynos_usb_add_ctls(chip, 0);
		return 0;
	}

	ret = usb_audio.user_scenario;

	return ret;
}

void exynos_usb_xhci_recover(void)
{
	int ret;

	if (g_hwinfo->need_first_probe) {
		pr_info("%s, Register sideband  again\n", __func__);

		/* exynos_usb_audio_disconnect process */
		exynos_usb_audio_disconnect(usb_audio.chip);

		/* exynos_usb_audio_connect process */
		exynos_usb_audio_offload_init();
		ret = exynos_usb_audio_set_device(audio_work_data.work_udev);
		if (ret < 0)
			return;

		audio_work_data.is_conn = 1;
		/* usb_probe_work process */
		exynos_usb_audio_hcd(audio_work_data.work_udev);
		exynos_usb_audio_conn(audio_work_data.work_udev);
		exynos_usb_audio_desc(audio_work_data.work_udev);
		audio_work_data.sysmmu_mapping_done = 1;

		if (!audio_work_data.is_conn) {
			pr_info("%s, already disconnected, return\n", __func__);
			return;
		}

		for (int timeout = 300;(audio_work_data.sysmmu_mapping_done != 1); timeout--) {
			mdelay(1);
			if (!timeout) {
				pr_info("%s - USB Audio is no set!\n", __func__);
				break;
			}
		}

		exynos_usb_audio_map_buf(audio_work_data.work_udev);
	}
}

static struct snd_usb_platform_ops platform_ops = {
	.connect_cb = exynos_usb_audio_connect,
	.disconnect_cb = exynos_usb_audio_disconnect,
};

static int exynos_usb_audio_probe(struct platform_device *pdev)
{
	int ret;
	pr_info("%s\n", __func__);

	ret = exynos_usb_audio_offload_probe(pdev);
	return ret;
}

static int exynos_usb_audio_remove(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	return 0;
}

static struct platform_driver exynos_usb_audio_driver = {
	.probe		= exynos_usb_audio_probe,
	.remove		= exynos_usb_audio_remove,
	.driver		= {
		.name	= "exynos-usb-audio",
		.of_match_table = exynos_usb_audio_match,
	},
};

static int exynos_usb_audio_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&exynos_usb_audio_driver);
}

static void exynos_usb_audio_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&exynos_usb_audio_driver);
}

late_initcall(exynos_usb_audio_init);
module_exit(exynos_usb_audio_exit);
//module_platform_driver(exynos_usb_audio_driver);

MODULE_AUTHOR("Jaehun Jung <jh0801.jung@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos USB Audio offloading driver");

