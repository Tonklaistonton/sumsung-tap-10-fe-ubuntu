// SPDX-License-Identifier: GPL-2.0
/*
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/usb/of.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include <linux/dmapool.h>
#include <linux/dma-mapping.h>

#include <linux/phy/phy.h>
#include <../drivers/usb/host/xhci.h>
#include <../drivers/usb/host/xhci-plat.h>
#include "xhci-exynos-audio.h"

#define DEQ_ADDR_BOTTOM_CLEAR 0xFFFFFFFFFFFFF000 // 64bit

struct xhci_exynos_audio *g_xhci_exynos_audio;
EXPORT_SYMBOL_GPL(g_xhci_exynos_audio);
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
extern int xhci_check_trb_in_td_math(struct xhci_hcd *xhci);
extern int xhci_address_device(struct usb_hcd *hcd, struct usb_device *udev);

void xhci_exynos_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring);

struct xhci_ring *xhci_ring_alloc_uram(struct xhci_hcd *xhci,
				       unsigned int num_segs,
				       unsigned int cycle_state,
				       enum xhci_ring_type type,
				       unsigned int max_packet, gfp_t flags, u32 endpoint_type);
u32 ext_ep_type = 0;

void xhci_exynos_enable_event_ring(struct xhci_hcd *xhci)
{
	u32 temp;
	u64 temp_64;

	temp_64 = xhci_read_64(xhci, &g_xhci_exynos_audio->ir->ir_set->erst_dequeue);
	temp_64 &= ~ERST_EXYNOS_PTR_MASK;
	xhci_info(xhci, "ERST2 deq = 64'h%0lx", (unsigned long)temp_64);

	xhci_info(xhci, "// [USB Audio] Set the interrupt modulation register");
	temp = readl(&g_xhci_exynos_audio->ir->ir_set->irq_control);
	temp &= ~ER_IRQ_INTERVAL_MASK;
	/*
	 * the increment interval is 8 times as much as that defined
	 * in xHCI spec on MTK's controller
	 */
	temp |= (u32) ((xhci->quirks & XHCI_MTK_HOST) ? 20 : 160);
	writel(temp, &g_xhci_exynos_audio->ir->ir_set->irq_control);

	temp = readl(&g_xhci_exynos_audio->ir->ir_set->irq_pending);
	xhci_info(xhci,
		  "// [USB Audio] Enabling event ring interrupter %p by writing 0x%x to irq_pending",
		  g_xhci_exynos_audio->ir->ir_set, (unsigned int)ER_IRQ_ENABLE(temp));
	writel(ER_IRQ_ENABLE(temp), &g_xhci_exynos_audio->ir->ir_set->irq_pending);
}
EXPORT_SYMBOL_GPL(xhci_exynos_enable_event_ring);

static void xhci_exynos_set_hc_event_deq_audio(struct xhci_hcd *xhci)
{
	u64 temp;
	dma_addr_t deq;

	deq =
	    xhci_trb_virt_to_dma(g_xhci_exynos_audio->ir->event_ring->deq_seg,
				 g_xhci_exynos_audio->ir->event_ring->dequeue);
	if (deq == 0 && !in_interrupt())
		xhci_warn(xhci, "WARN something wrong with SW event ring " "dequeue ptr.\n");
	/* Update HC event ring dequeue pointer */
	temp = xhci_read_64(xhci, &g_xhci_exynos_audio->ir->ir_set->erst_dequeue);
	temp &= ERST_EXYNOS_PTR_MASK;
	/* Don't clear the EHB bit (which is RW1C) because
	 * there might be more events to service.
	 */
	temp &= ~ERST_EHB;
	xhci_info(xhci, "//[%s] Write event ring dequeue pointer = 0x%llx, "
		  "preserving EHB bit", __func__, ((u64) deq & (u64) ~ ERST_EXYNOS_PTR_MASK) | temp);
	xhci_write_64(xhci, ((u64) deq & (u64) ~ ERST_EXYNOS_PTR_MASK) | temp,
		      &g_xhci_exynos_audio->ir->ir_set->erst_dequeue);
}

int xhci_exynos_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		struct usb_host_endpoint *ep)
{
	int ret;
	struct xhci_hcd *xhci;
	struct xhci_virt_device *virt_dev;
	struct usb_endpoint_descriptor *d = &ep->desc;

	/* Check Feedback Endpoint */
	if ((d->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
				USB_ENDPOINT_XFER_ISOC) {
		if ((d->bmAttributes & USB_ENDPOINT_USAGE_MASK) ==
					USB_ENDPOINT_USAGE_FEEDBACK) {
			g_xhci_exynos_audio->feedback = 1;
		} else
			g_xhci_exynos_audio->feedback = 0;
	}

	pr_debug("%s +++", __func__);
	ret = xhci_add_endpoint(hcd, udev, ep);
	g_xhci_exynos_audio->feedback = 0;

	if (!ret && udev->slot_id) {
		xhci = hcd_to_xhci(hcd);
		virt_dev = xhci->devs[udev->slot_id];
	}
	pr_debug("%s ---", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(xhci_exynos_add_endpoint);
int xhci_exynos_alloc_event_ring(struct xhci_hcd *xhci, gfp_t flags)
{
	dma_addr_t dma;
	unsigned int val;
	u64 val_64;
	struct xhci_segment *seg;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	g_xhci_exynos_audio->ir = &(g_xhci_exynos_audio->ir_data);

	if(g_xhci_exynos_audio->ir->ir_set) {
		pr_info("%s: ir_set! skip alloc again!\n", __func__);
		goto alloc_skip;
	}

	g_xhci_exynos_audio->ir->ir_set = &xhci->run_regs->ir_set[1];
#if !IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	g_xhci_exynos_audio->save_addr = dma_alloc_coherent(dev, PAGE_SIZE, &dma, flags);
	g_xhci_exynos_audio->save_dma = dma;
	xhci_info(xhci, "// Save address = 0x%llx (DMA), %p (virt)",
		  (unsigned long long)g_xhci_exynos_audio->save_dma, g_xhci_exynos_audio->save_addr);
#endif
	if ((xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)) {
		/* for AUDIO erst */
		g_xhci_exynos_audio->ir->event_ring =
		    xhci_ring_alloc_uram(xhci, ERST_NUM_SEGS, 1, TYPE_EVENT, 0, flags, 0);
		if (!g_xhci_exynos_audio->ir->event_ring)
			goto fail;

		g_xhci_exynos_audio->ir->erst.entries =
		    ioremap(EXYNOS_URAM_ABOX_ERST_SEG_ADDR, sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS);
		if (!g_xhci_exynos_audio->ir->erst.entries)
			goto fail;

		dma = EXYNOS_URAM_ABOX_ERST_SEG_ADDR;
		xhci_info(xhci, "ABOX audio ERST allocated at 0x%x", EXYNOS_URAM_ABOX_ERST_SEG_ADDR);
	} else {
		/* for AUDIO erst */
		g_xhci_exynos_audio->ir->event_ring = xhci_ring_alloc(xhci, ERST_NUM_SEGS, 1, TYPE_EVENT, 0, flags);
		if (!g_xhci_exynos_audio->ir->event_ring)
			goto fail;
		g_xhci_exynos_audio->ir->erst.entries =
		    //dma_pre_alloc_coherent(xhci, sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS, &dma, flags);
		    dma_alloc_coherent(dev, sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS, &dma, flags);
		if (!g_xhci_exynos_audio->ir->erst.entries)
			goto fail;
	}
	xhci_info(xhci, "// Allocated event ring segment table at 0x%llx", (unsigned long long)dma);

	memset(g_xhci_exynos_audio->ir->erst.entries, 0, sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS);
	g_xhci_exynos_audio->ir->erst.num_entries = ERST_NUM_SEGS;
	g_xhci_exynos_audio->ir->erst.erst_dma_addr = dma;
	xhci_info(xhci,
		  "// Set ERST to 0; private num segs = %i, virt addr = %p, dma addr = 0x%llx",
		  g_xhci_exynos_audio->ir->erst.num_entries, g_xhci_exynos_audio->ir->erst.entries, (unsigned long long)g_xhci_exynos_audio->ir->erst.erst_dma_addr);

	/* set ring base address and size for each segment table entry */
	for (val = 0, seg = g_xhci_exynos_audio->ir->event_ring->first_seg; val < ERST_NUM_SEGS; val++) {
		struct xhci_erst_entry *entry = &g_xhci_exynos_audio->ir->erst.entries[val];

		entry->seg_addr = cpu_to_le64(seg->dma);
		entry->seg_size = cpu_to_le32(TRBS_PER_SEGMENT);
		entry->rsvd = 0;
		seg = seg->next;
	}

alloc_skip:
	/* set ERST count with the number of entries in the segment table */
	val = readl(&g_xhci_exynos_audio->ir->ir_set->erst_size);
	val &= ERST_SIZE_MASK;
	val |= ERST_NUM_SEGS;
	xhci_info(xhci, "// Write ERST size = %i to ir_set 0 (some bits preserved)", val);
	writel(val, &g_xhci_exynos_audio->ir->ir_set->erst_size);

	xhci_info(xhci, "// Set ERST entries to point to event ring.");
	/* set the segment table base address */
	xhci_info(xhci, "// Set ERST base address for ir_set 0 = 0x%llx",
		  (unsigned long long)g_xhci_exynos_audio->ir->erst.erst_dma_addr);
	val_64 = xhci_read_64(xhci, &g_xhci_exynos_audio->ir->ir_set->erst_base);
	val_64 &= ERST_EXYNOS_PTR_MASK;
	val_64 |= (g_xhci_exynos_audio->ir->erst.erst_dma_addr & (u64) ~ ERST_EXYNOS_PTR_MASK);
	xhci_write_64(xhci, val_64, &g_xhci_exynos_audio->ir->ir_set->erst_base);

	/* Set the event ring dequeue address */
	xhci_exynos_set_hc_event_deq_audio(xhci);
	xhci_info(xhci, "// Wrote ERST address to ir_set 1.");

	return 0;
 fail:
	return -1;
}
EXPORT_SYMBOL_GPL(xhci_exynos_alloc_event_ring);

void xhci_exynos_free_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx)
{
	/* Ignore dma_pool_free if it is allocated from URAM */
	if (ctx->dma != EXYNOS_URAM_DEVICE_CTX_ADDR)
		dma_pool_free(xhci->device_pool, ctx->bytes, ctx->dma);
}

void xhci_exynos_alloc_container_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, int type, gfp_t flags)
{
	if (type != XHCI_CTX_TYPE_INPUT
	    && g_xhci_exynos_audio->exynos_uram_ctx_alloc == 0 && xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		/* Only first Device Context uses URAM */
		int i;
		ctx->bytes = ioremap(EXYNOS_URAM_DEVICE_CTX_ADDR, 2112);
		if (!ctx->bytes)
			return;

		for (i = 0; i < 2112; i++)
			ctx->bytes[i] = 0;

		ctx->dma = EXYNOS_URAM_DEVICE_CTX_ADDR;
		g_xhci_exynos_audio->usb_audio_ctx_addr = ctx->bytes;
		g_xhci_exynos_audio->exynos_uram_ctx_alloc = 1;
		xhci_info(xhci, "First device context allocated at URAM(%x)", EXYNOS_URAM_DEVICE_CTX_ADDR);
	} else {
		ctx->bytes = dma_pool_zalloc(xhci->device_pool, flags, &ctx->dma);
	}
}

struct xhci_ring *xhci_exynos_alloc_transfer_ring(struct xhci_hcd *xhci,
						  u32 endpoint_type,
						  enum xhci_ring_type ring_type,
						  unsigned int max_packet, gfp_t mem_flags)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		/* If URAM is not allocated, it try to allocate from URAM */
		if (g_xhci_exynos_audio->exynos_uram_isoc_out_alloc == 0 && endpoint_type == ISOC_OUT_EP) {
			xhci_info(xhci, "First ISOC OUT ring is allocated from URAM.\n");
			return xhci_ring_alloc_uram(xhci, 1, 1, ring_type, max_packet, mem_flags, endpoint_type);

			g_xhci_exynos_audio->exynos_uram_isoc_out_alloc = 1;
		} else if (g_xhci_exynos_audio->exynos_uram_isoc_in_alloc == 0
			   && endpoint_type == ISOC_IN_EP && EXYNOS_URAM_ISOC_IN_RING_ADDR != 0x0) {
			xhci_info(xhci, "First ISOC IN ring is allocated from URAM.\n");
			return xhci_ring_alloc_uram(xhci, 1, 1, ring_type, max_packet, mem_flags, endpoint_type);

			g_xhci_exynos_audio->exynos_uram_isoc_in_alloc = 1;
		} else {
			return xhci_ring_alloc(xhci, 2, 1, ring_type, max_packet, mem_flags);
		}

	} else {
		return xhci_ring_alloc(xhci, 2, 1, ring_type, max_packet, mem_flags);
	}
}

void xhci_exynos_segment_free_skip(struct xhci_hcd *xhci, struct xhci_segment *seg)
{
	if (seg->trbs) {
		/* Check URAM address for memory free */
		if (seg->dma == EXYNOS_URAM_ABOX_EVT_RING_ADDR) {
			iounmap(seg->trbs);
		} else if (seg->dma == EXYNOS_URAM_ISOC_OUT_RING_ADDR) {
			g_xhci_exynos_audio->exynos_uram_isoc_out_alloc = 0;
			if (in_interrupt())
				g_xhci_exynos_audio->usb_audio_isoc_out_addr = (u8 *) seg->trbs;
			else
				iounmap(seg->trbs);
		} else if (seg->dma == EXYNOS_URAM_ISOC_IN_RING_ADDR) {
			g_xhci_exynos_audio->exynos_uram_isoc_in_alloc = 0;
			if (in_interrupt())
				g_xhci_exynos_audio->usb_audio_isoc_in_addr = (u8 *) seg->trbs;
			else
				iounmap(seg->trbs);
		} else
			dma_pool_free(xhci->segment_pool, seg->trbs, seg->dma);

		seg->trbs = NULL;
	}
	kfree(seg->bounce_buf);
	kfree(seg);
}

void xhci_exynos_free_segments_for_ring(struct xhci_hcd *xhci, struct xhci_segment *first)
{
	struct xhci_segment *seg;

	seg = first->next;

	if (!seg)
		xhci_err(xhci, "segment is null unexpectedly\n");

	while (seg != first) {
		struct xhci_segment *next = seg->next;
		xhci_exynos_segment_free_skip(xhci, seg);
		seg = next;
	}
	xhci_exynos_segment_free_skip(xhci, first);
}

void xhci_exynos_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring)
{
	if (!ring)
		return;

	//trace_xhci_ring_free(ring);

	if (ring->first_seg) {
		if (ring->type == TYPE_STREAM)
			xhci_remove_stream_mapping(ring);

		xhci_exynos_free_segments_for_ring(xhci, ring->first_seg);
	}

	kfree(ring);
}

int endpoint_ring_table[31];

void xhci_exynos_free_transfer_ring(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev, unsigned int ep_index)
{
	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		if (endpoint_ring_table[ep_index]) {
			pr_info("%s: endpoint %d has been already free-ing\n", __func__, ep_index);
		}
		if (xhci->quirks & BIT_ULL(60)) {
			pr_info("%s: endpoint %d free is requested from reset_bandwidth\n", __func__, ep_index);
			xhci_exynos_ring_free(xhci, virt_dev->eps[ep_index].new_ring);
			xhci->quirks &= ~(BIT_ULL(60));
			return;
		}
		endpoint_ring_table[ep_index] = 1;
		xhci_exynos_ring_free(xhci, virt_dev->eps[ep_index].ring);
		endpoint_ring_table[ep_index] = 0;
	} else {
		xhci_ring_free(xhci, virt_dev->eps[ep_index].ring);
	}

	return;
}

bool xhci_exynos_is_usb_offload_enabled(struct xhci_hcd * xhci,
					struct xhci_virt_device * virt_dev, unsigned int ep_index)
{
	g_xhci_exynos_audio->xhci = xhci;
	return true;
}

struct xhci_device_context_array *xhci_exynos_alloc_dcbaa(struct xhci_hcd *xhci, gfp_t flags)
{
	struct device	*dev = xhci_to_hcd(xhci)->self.sysdev;
	dma_addr_t	dma = 0;

	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		int i;

		xhci_info(xhci, "DCBAA is allocated at 0x%x(URAM)", EXYNOS_URAM_DCBAA_ADDR);
		/* URAM allocation for DCBAA */
		xhci->dcbaa = ioremap(EXYNOS_URAM_DCBAA_ADDR, sizeof(*xhci->dcbaa));
		if (!xhci->dcbaa)
			return NULL;
		/* Clear DCBAA */
		for (i = 0; i < MAX_HC_SLOTS; i++)
			xhci->dcbaa->dev_context_ptrs[i] = 0x0;

		xhci->dcbaa->dma = EXYNOS_URAM_DCBAA_ADDR;

	} else {
		xhci_info(xhci, "URAM quirk is not set! Use DRAM\n");
		xhci->dcbaa = dma_alloc_coherent(dev, sizeof(*xhci->dcbaa), &dma,
				flags);
		if (!xhci->dcbaa)
			return NULL;
		xhci->dcbaa->dma = dma;
	}

	return xhci->dcbaa;
}

void xhci_exynos_free_dcbaa(struct xhci_hcd *xhci)
{
	struct device	*dev = xhci_to_hcd(xhci)->self.sysdev;

	if (xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO) {
		iounmap(xhci->dcbaa);
		if (g_xhci_exynos_audio->usb_audio_ctx_addr != NULL) {
			iounmap(g_xhci_exynos_audio->usb_audio_ctx_addr);
			g_xhci_exynos_audio->usb_audio_ctx_addr = NULL;
		}
		if (g_xhci_exynos_audio->usb_audio_isoc_out_addr != NULL) {
			iounmap(g_xhci_exynos_audio->usb_audio_isoc_out_addr);
			g_xhci_exynos_audio->usb_audio_isoc_out_addr = NULL;
		}
		if (g_xhci_exynos_audio->usb_audio_isoc_in_addr != NULL) {
			iounmap(g_xhci_exynos_audio->usb_audio_isoc_in_addr);
			g_xhci_exynos_audio->usb_audio_isoc_in_addr = NULL;
		}
	} else {
		dma_free_coherent(dev, sizeof(*xhci->dcbaa),
				xhci->dcbaa, xhci->dcbaa->dma);
		xhci->dcbaa = NULL;
	}
}

/* URAM Allocation Functions */
extern void xhci_segment_free(struct xhci_hcd *xhci, struct xhci_segment *seg);
extern void xhci_link_segments(struct xhci_segment *prev,
			       struct xhci_segment *next, enum xhci_ring_type type, bool chain_links);
extern void xhci_initialize_ring_info(struct xhci_ring *ring, unsigned int cycle_state);

static struct xhci_segment *xhci_segment_alloc_uram(struct xhci_hcd *xhci,
						    unsigned int cycle_state, unsigned int max_packet, gfp_t flags)
{
	struct xhci_segment *seg;
	dma_addr_t dma;
	int i;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	seg = kzalloc_node(sizeof(*seg), flags, dev_to_node(dev));
	if (!seg)
		return NULL;

	seg->trbs = ioremap(EXYNOS_URAM_ABOX_EVT_RING_ADDR, TRB_SEGMENT_SIZE);
	if (!seg->trbs)
		return NULL;

	dma = EXYNOS_URAM_ABOX_EVT_RING_ADDR;

	if (max_packet) {
		seg->bounce_buf = kzalloc_node(max_packet, flags, dev_to_node(dev));
		if (!seg->bounce_buf) {
			dma_pool_free(xhci->segment_pool, seg->trbs, dma);
			kfree(seg);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	xhci_info(xhci, "ABOX Event Ring is allocated at 0x%x", EXYNOS_URAM_ABOX_EVT_RING_ADDR);
	seg->next = NULL;

	return seg;
}

static struct xhci_segment *xhci_segment_alloc_uram_ep(struct xhci_hcd *xhci,
						       unsigned int cycle_state,
						       unsigned int max_packet,
						       gfp_t flags, int seg_num, u32 endpoint_type)
{
	struct xhci_segment *seg;
	dma_addr_t dma;
	int i;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	seg = kzalloc_node(sizeof(*seg), flags, dev_to_node(dev));
	if (!seg)
		return NULL;

	if (seg_num != 0) {
		/* Support just one segment */
		xhci_err(xhci, "%s : Unexpected SEG NUMBER!\n", __func__);
		return NULL;
	}

	if (endpoint_type == ISOC_OUT_EP) {
		if (!g_xhci_exynos_audio->feedback)
			seg->trbs = ioremap(EXYNOS_URAM_ISOC_OUT_RING_ADDR, TRB_SEGMENT_SIZE);
		else
			seg->trbs = dma_pool_zalloc(xhci->segment_pool, flags, &dma);
		if (!seg->trbs)
			return NULL;

		if (!g_xhci_exynos_audio->feedback)
			dma = EXYNOS_URAM_ISOC_OUT_RING_ADDR;
		xhci_info(xhci, "First ISOC-OUT Ring is allocated at 0x%llx", dma);
	} else if (endpoint_type == ISOC_IN_EP) {
		if (!g_xhci_exynos_audio->feedback)
			seg->trbs = ioremap(EXYNOS_URAM_ISOC_IN_RING_ADDR, TRB_SEGMENT_SIZE);
		else
			seg->trbs = dma_pool_zalloc(xhci->segment_pool, flags, &dma);
		if (!seg->trbs)
			return NULL;

		if (!g_xhci_exynos_audio->feedback)
			dma = EXYNOS_URAM_ISOC_IN_RING_ADDR;
		xhci_info(xhci, "First ISOC-IN Ring is allocated at 0x%llx", dma);
	} else {
		xhci_err(xhci, "%s : Unexpected EP Type!\n", __func__);
		return NULL;
	}

	for (i = 0; i < 256; i++) {
		seg->trbs[i].link.segment_ptr = 0;
		seg->trbs[i].link.intr_target = 0;
		seg->trbs[i].link.control = 0;
	}

	if (max_packet) {
		seg->bounce_buf = kzalloc_node(max_packet, flags, dev_to_node(dev));
		if (!seg->bounce_buf) {
			dma_pool_free(xhci->segment_pool, seg->trbs, dma);
			kfree(seg);
			return NULL;
		}
	}
	/* If the cycle state is 0, set the cycle bit to 1 for all the TRBs */
	if (cycle_state == 0) {
		for (i = 0; i < TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	seg->next = NULL;

	return seg;
}

static int xhci_alloc_segments_for_ring_uram(struct xhci_hcd *xhci,
					     struct xhci_segment **first,
					     struct xhci_segment **last,
					     unsigned int num_segs,
					     unsigned int cycle_state,
					     enum xhci_ring_type type,
					     unsigned int max_packet, gfp_t flags, u32 endpoint_type)
{
	struct xhci_segment *prev;
	bool chain_links;

	/* Set chain bit for 0.95 hosts, and for isoc rings on AMD 0.96 host */
	chain_links = ! !(xhci_link_trb_quirk(xhci) || (type == TYPE_ISOC && (xhci->quirks & XHCI_AMD_0x96_HOST)));

	if (type == TYPE_ISOC) {
		prev = xhci_segment_alloc_uram_ep(xhci, cycle_state, max_packet, flags, 0, endpoint_type);
	} else if (type == TYPE_EVENT) {
		prev = xhci_segment_alloc_uram(xhci, cycle_state, max_packet, flags);
	} else {
		xhci_err(xhci, "Unexpected TYPE for URAM allocation!\n");
		return -ENOMEM;
	}

	if (!prev)
		return -ENOMEM;
	num_segs--;

	*first = prev;
	while (num_segs > 0) {
		struct xhci_segment *next = NULL;

		if (type == TYPE_ISOC) {
			prev = xhci_segment_alloc_uram_ep(xhci, cycle_state, max_packet, flags, 1, endpoint_type);
		} else if (type == TYPE_EVENT) {
			next = xhci_segment_alloc_uram(xhci, cycle_state, max_packet, flags);
		} else {
			xhci_err(xhci, "Unexpected TYPE for URAM alloc(multi)!\n");
			return -ENOMEM;
		}

		if (!next) {
			prev = *first;
			while (prev) {
				next = prev->next;
				xhci_segment_free(xhci, prev);
				prev = next;
			}
			return -ENOMEM;
		}
		xhci_link_segments(prev, next, type, chain_links);

		prev = next;
		num_segs--;
	}
	xhci_link_segments(prev, *first, type, chain_links);
	*last = prev;

	return 0;
}

struct xhci_ring *xhci_ring_alloc_uram(struct xhci_hcd *xhci,
				       unsigned int num_segs,
				       unsigned int cycle_state,
				       enum xhci_ring_type type,
				       unsigned int max_packet, gfp_t flags, u32 endpoint_type)
{
	struct xhci_ring *ring;
	int ret;
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	ring = kzalloc_node(sizeof(*ring), flags, dev_to_node(dev));
	if (!ring)
		return NULL;

	ring->num_segs = num_segs;
	ring->bounce_buf_len = max_packet;
	INIT_LIST_HEAD(&ring->td_list);
	ring->type = type;
	if (num_segs == 0)
		return ring;

	ret = xhci_alloc_segments_for_ring_uram(xhci, &ring->first_seg,
						&ring->last_seg, num_segs,
						cycle_state, type, max_packet, flags, endpoint_type);
	if (ret)
		goto fail;

	/* Only event ring does not use link TRB */
	if (type != TYPE_EVENT) {
		/* See section 4.9.2.1 and 6.4.4.1 */
		ring->last_seg->trbs[TRBS_PER_SEGMENT - 1].link.control |= cpu_to_le32(LINK_TOGGLE);
	}
	xhci_initialize_ring_info(ring, cycle_state);
	//trace_xhci_ring_alloc(ring);
	return ring;

 fail:
	kfree(ring);
	return NULL;
}

unsigned int xhci_exynos_get_endpoint_address(unsigned int ep_index)
{
	unsigned int number = DIV_ROUND_UP(ep_index, 2);
	unsigned int direction = ep_index % 2 ? USB_DIR_OUT : USB_DIR_IN;
	return direction | number;
}
EXPORT_SYMBOL_GPL(xhci_exynos_get_endpoint_address);

int xhci_exynos_vendor_init(struct xhci_hcd *xhci)
{
	return 0;
}

void xhci_exynos_vendor_cleanup(struct xhci_hcd *xhci)
{
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;

	dma_free_coherent(dev, sizeof(PAGE_SIZE),
				g_xhci_exynos_audio->save_addr,
				g_xhci_exynos_audio->save_dma);

	if (!(xhci->quirks & XHCI_USE_URAM_FOR_EXYNOS_AUDIO)) {
		xhci_ring_free(xhci, g_xhci_exynos_audio->ir->event_ring);
		g_xhci_exynos_audio->ir->event_ring = NULL;

		dma_free_coherent(dev,
				sizeof(struct xhci_erst_entry) * ERST_NUM_SEGS,
				g_xhci_exynos_audio->ir->erst.entries,
				g_xhci_exynos_audio->ir->erst.erst_dma_addr);
	}
}
EXPORT_SYMBOL_GPL(xhci_exynos_vendor_cleanup);

static struct xhci_vendor_ops ops = {
	.vendor_init = xhci_exynos_vendor_init,
	.vendor_cleanup = xhci_exynos_vendor_cleanup,
	.is_usb_offload_enabled = xhci_exynos_is_usb_offload_enabled,
	.alloc_dcbaa = xhci_exynos_alloc_dcbaa,
	.free_dcbaa = xhci_exynos_free_dcbaa,
	.alloc_transfer_ring = xhci_exynos_alloc_transfer_ring,
	.free_transfer_ring = xhci_exynos_free_transfer_ring,
	.alloc_container_ctx = xhci_exynos_alloc_container_ctx,
	.free_container_ctx = xhci_exynos_free_container_ctx,
};

int xhci_exynos_audio_get_phy(struct device *parent, struct platform_device *pdev)
{
	g_xhci_exynos_audio->phy = devm_phy_get(parent, "usb2-phy");
	if (IS_ERR_OR_NULL(g_xhci_exynos_audio->phy)) {
		g_xhci_exynos_audio->phy = NULL;
		dev_err(&pdev->dev, "%s: failed to get phy\n", __func__);
		return -1;
	}

	return 0;
}

static struct xhci_plat_priv_overwrite xhci_plat_vendor_overwrite;

int xhci_exynos_register_vendor_ops(struct xhci_vendor_ops *vendor_ops)
{
	if (vendor_ops == NULL)
		return -EINVAL;

	xhci_plat_vendor_overwrite.vendor_ops = vendor_ops;

	return 0;
}

int xhci_vendor_init(struct xhci_hcd *xhci)
{
	struct xhci_vendor_ops *ops = NULL;

	if (xhci_plat_vendor_overwrite.vendor_ops)
		ops = xhci->vendor_ops = xhci_plat_vendor_overwrite.vendor_ops;

	if (ops && ops->vendor_init)
		return ops->vendor_init(xhci);
	return 0;
}
EXPORT_SYMBOL_GPL(xhci_vendor_init);

int xhci_exynos_audio_init(struct device *parent, struct platform_device *pdev)
{
	int ret;
	int value;
	int use_uram = 0;

	ret = xhci_exynos_register_vendor_ops(&ops);
	if (!ret) {
		pr_info("%s register ops done!\n", __func__);
	} else {
		dev_err(&pdev->dev, "register vendor ops failed\n");
		return -1;
	}

	ret = of_property_read_u32(parent->of_node,
				"xhci_use_uram_for_audio", &value);

	if (ret == 0 && value == 1) {
		/*
		 * Check URAM address. At least the following address should
		 * be defined.(Otherwise, URAM feature will be disabled.)
		 */
		if (EXYNOS_URAM_DCBAA_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_ERST_SEG_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_EVT_RING_ADDR == 0x0 ||
				EXYNOS_URAM_DEVICE_CTX_ADDR == 0x0 ||
				EXYNOS_URAM_ISOC_OUT_RING_ADDR == 0x0) {
			dev_info(&pdev->dev,
				"Some URAM addresses are not defiend!\n");
			return -1;
		}
		dev_info(&pdev->dev, "Support URAM for USB audio.\n");
		/* Initialization Default Value */
		dev_info(&pdev->dev, "Init g_xhci_exynos_audio.\n");
		g_xhci_exynos_audio->exynos_uram_ctx_alloc = false;
		g_xhci_exynos_audio->exynos_uram_isoc_out_alloc = false;
		g_xhci_exynos_audio->exynos_uram_isoc_in_alloc = false;
		g_xhci_exynos_audio->usb_audio_ctx_addr = NULL;
		g_xhci_exynos_audio->usb_audio_isoc_out_addr = NULL;
		g_xhci_exynos_audio->usb_audio_isoc_in_addr = NULL;

		use_uram = 1;
	} else {
		dev_info(&pdev->dev, "URAM is not used.\n");
	}

	ret = of_property_read_u32(parent->of_node,
			"usb_audio_offloading", &value);
	if (ret == 0 && value == 1) {
		dev_info(&pdev->dev, "USB Audio offloading is supported\n");

		ret = xhci_exynos_audio_get_phy(parent, pdev);
		if (ret) {
			dev_err(&pdev->dev, "USB Audio PHY get fail\n");
			return -1;
		}
	} else
		dev_err(&pdev->dev, "No usb offloading, err = %d\n", ret);

	return use_uram ? XHCI_USE_URAM : XHCI_USE_DRAM;
}
EXPORT_SYMBOL_GPL(xhci_exynos_audio_init);

int xhci_exynos_audio_alloc(struct device *parent)
{
	dma_addr_t	dma;

	dev_info(parent, "%s\n", __func__);

	g_xhci_exynos_audio = devm_kzalloc(parent, sizeof(struct xhci_exynos_audio), GFP_KERNEL);
	memset(g_xhci_exynos_audio, 0, sizeof(struct xhci_exynos_audio));

#if IS_ENABLED(CONFIG_USB_XHCI_SIDEBAND)
	g_xhci_exynos_audio->save_addr = dma_alloc_coherent(parent, PAGE_SIZE, &dma, GFP_KERNEL);
	g_xhci_exynos_audio->save_dma = dma;
	dev_info(parent, "// Save address = 0x%llx (DMA), %p (virt)",
		  (unsigned long long)g_xhci_exynos_audio->save_dma, g_xhci_exynos_audio->save_addr);
#endif

	/* In data buf alloc */
	g_xhci_exynos_audio->in_addr = dma_alloc_coherent(parent,
			(PAGE_SIZE * 256), &dma, GFP_KERNEL);
	g_xhci_exynos_audio->in_dma = dma;
	dev_info(parent, "// IN Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)g_xhci_exynos_audio->in_dma, g_xhci_exynos_audio->in_addr);

	/* Out data buf alloc */
	g_xhci_exynos_audio->out_addr = dma_alloc_coherent(parent,
			(PAGE_SIZE * 256), &dma, GFP_KERNEL);
	g_xhci_exynos_audio->out_dma = dma;
	dev_info(parent, "// OUT Data address = 0x%llx (DMA), %p (virt)",
		(unsigned long long)g_xhci_exynos_audio->out_dma, g_xhci_exynos_audio->out_addr);

	return 0;
}
EXPORT_SYMBOL_GPL(xhci_exynos_audio_alloc);
#endif

MODULE_LICENSE("GPL v2");
