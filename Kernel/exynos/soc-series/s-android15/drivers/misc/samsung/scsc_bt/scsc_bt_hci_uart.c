#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/of_device.h>
#include <linux/serdev.h>

#include <linux/mod_devicetable.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>
#include <net/bluetooth/hci_core.h>

#include <../drivers/bluetooth/hci_uart.h>
#include <scsc/scsc_logring.h>
#include "scsc_bt_priv.h"

struct scsc_data {
	struct hci_uart     *hu;
	unsigned long       flags;

	struct sk_buff_head txq;
	struct sk_buff      *rx_skb;
};

struct scsc_serdev {
	struct hci_uart     serdev_hu;
} *scscdev;

/* SLSI_DATA flag bits */
#define SLSI_HUP_OPENED                0
#define SLSI_HUP_READY                 1
#define SLSI_HUP_STOP_RECV_FRAME       2
#define SLSI_HUP_STOP_SEND_FRAME       3

#ifdef CONFIG_SCSC_BT_KUNIT_TEST
extern int mock_hci_uart_register_device(struct hci_uart *hu, const struct hci_uart_proto *p);
#define hci_uart_register_device mock_hci_uart_register_device

#undef serdev_device_driver_register
#define serdev_device_driver_register(x) 0
#endif

/*
 * scsc_hup functions are HCI UART Protocol APIs for scsc.
 */
static int scsc_hup_open(struct hci_uart *hu)
{
	struct scsc_data *scsc;

	SCSC_TAG_INFO(BT_COMMON, "\n");
	scsc = kzalloc(sizeof(struct scsc_data), GFP_KERNEL);
	if (!scsc)
		return -ENOMEM;

	scsc->hu = hu;
	skb_queue_head_init(&scsc->txq);

	hu->priv = scsc;

	set_bit(SLSI_HUP_OPENED, &scsc->flags);
	return 0;
}

static int scsc_setup_diag(struct hci_dev *hdev, bool enable)
{
	SCSC_TAG_INFO(BT_COMMON, "handle first open\n");

	set_bit(HCI_QUIRK_NON_PERSISTENT_SETUP, &hdev->quirks);
	return 0;
}

static int scsc_hup_shutdown(struct hci_dev *hdev)
{
	struct hci_uart *hu = hci_get_drvdata(hdev);
	struct scsc_data *scsc = hu->priv;

	SCSC_TAG_INFO(BT_COMMON, "\n");
	if (!test_bit(SLSI_HUP_READY, &scsc->flags))
		return 0;

	hdev->hw_error = NULL;
	hdev->cmd_timeout = NULL;

	skb_queue_purge(&scsc->txq);

	clear_bit(SLSI_HUP_READY, &scsc->flags);
	return scsc_bt_h4_release();
}

static int scsc_hup_close(struct hci_uart *hu)
{
	struct scsc_data *scsc = hu->priv;

	SCSC_TAG_INFO(BT_COMMON, "\n");
	if (!test_bit(SLSI_HUP_OPENED, &scsc->flags))
		return 0;

	hu->priv = NULL;
	clear_bit(SLSI_HUP_OPENED, &scsc->flags);
	kfree(scsc);

	return 0;
}

static int scsc_hup_setup(struct hci_uart *hu)
{
	struct hci_dev *hdev = hu->hdev;
	struct scsc_data *scsc = hu->priv;
	int ret = 0;

	SCSC_TAG_INFO(BT_COMMON, "\n");
	if (!test_bit(SLSI_HUP_OPENED, &scsc->flags))
		return -ENXIO;

	if (test_bit(SLSI_HUP_READY, &scsc->flags))
		return 0;

	/*
	 * This file is for using the Linux Bluetooth subsystem for AIDL. The
	 * net/bluetooth performs some operations after registering a hci
	 * device.  However, we don't want these operations that are out of our
	 * control and the first driver open after booting to be performed at
	 * this stage because it cannot to download converted HCF file at the
	 * openning time. Therefore, we add this flag to ignore the first open
	 * (during Bluetooth device registeration) after device botting.
	 */
	if (!test_bit(HCI_QUIRK_NON_PERSISTENT_DIAG, &hdev->quirks)) {
		SCSC_TAG_INFO(BT_COMMON, "set the diagnostic function.\n");

		set_bit(HCI_QUIRK_NON_PERSISTENT_DIAG, &hdev->quirks);
		hci_dev_set_flag(hdev, HCI_VENDOR_DIAG);
		hdev->set_diag = scsc_setup_diag;

		SCSC_TAG_INFO(BT_COMMON, "skip this setup.\n");
		return -EAGAIN;
	}

	ret = scsc_bt_h4_open(SCSC_USER_TYPE_HCIUART);
	if (ret < 0) {
		SCSC_TAG_ERR(BT_COMMON, "slsi_bt_open failed.\n");
		goto out;
	}

	hdev->shutdown = scsc_hup_shutdown;
	set_bit(SLSI_HUP_READY, &scsc->flags);
out:
	return ret;
}

static const struct h4_recv_pkt scsc_bt_recv_pkts[] = {
	{ H4_RECV_ACL,    .recv = hci_recv_frame   },
	{ H4_RECV_SCO,    .recv = hci_recv_frame   },
	{ H4_RECV_EVENT,  .recv = hci_recv_frame   },
	{ H4_RECV_ISO,    .recv = hci_recv_frame   },
};

/* TODO: h4_recv_buf needs to be add at abi_gki_aarch64_exynos */
#ifndef USE_H4_RECV_PKT_API
static struct sk_buff *_h4_recv_buf(struct hci_dev *hdev, struct sk_buff *skb,
				    const unsigned char *buffer, int count)
{
	const struct h4_recv_pkt *pkts = scsc_bt_recv_pkts;
	int pkts_count = ARRAY_SIZE(scsc_bt_recv_pkts);

	/* Check for error from previous call */
	if (IS_ERR(skb))
		skb = NULL;

	while (count) {
		int i, len;

		if (!count)
			break;

		if (!skb) {
			for (i = 0; i < pkts_count; i++) {
				if (buffer[0] != (&pkts[i])->type)
					continue;

				skb = bt_skb_alloc((&pkts[i])->maxlen,
						   GFP_ATOMIC);
				if (!skb)
					return ERR_PTR(-ENOMEM);

				hci_skb_pkt_type(skb) = (&pkts[i])->type;
				hci_skb_expect(skb) = (&pkts[i])->hlen;
				break;
			}

			/* Check for invalid packet type */
			if (!skb)
				return ERR_PTR(-EILSEQ);

			count -= 1;
			buffer += 1;
		}

		len = min_t(uint, hci_skb_expect(skb) - skb->len, count);
		skb_put_data(skb, buffer, len);

		count -= len;
		buffer += len;

		/* Check for partial packet */
		if (skb->len < hci_skb_expect(skb))
			continue;

		for (i = 0; i < pkts_count; i++) {
			if (hci_skb_pkt_type(skb) == (&pkts[i])->type)
				break;
		}

		if (i >= pkts_count) {
			kfree_skb(skb);
			return ERR_PTR(-EILSEQ);
		}

		if (skb->len == (&pkts[i])->hlen) {
			u16 dlen;

			switch ((&pkts[i])->lsize) {
			case 0:
				/* No variable data length */
				dlen = 0;
				break;
			case 1:
				/* Single octet variable length */
				dlen = skb->data[(&pkts[i])->loff];
				hci_skb_expect(skb) += dlen;

				if (skb_tailroom(skb) < dlen) {
					kfree_skb(skb);
					return ERR_PTR(-EMSGSIZE);
				}
				break;
			case 2:
				/* Double octet variable length */
				dlen = skb->data[(&pkts[i])->loff] |
				       skb->data[(&pkts[i])->loff+1] << 8;
				hci_skb_expect(skb) += dlen;

				if (skb_tailroom(skb) < dlen) {
					kfree_skb(skb);
					return ERR_PTR(-EMSGSIZE);
				}
				break;
			default:
				/* Unsupported variable length */
				kfree_skb(skb);
				return ERR_PTR(-EILSEQ);
			}

			if (!dlen) {
				/* No more data, complete frame */
				(&pkts[i])->recv(hdev, skb);
				skb = NULL;
			}
		} else {
			/* Complete frame */
			(&pkts[i])->recv(hdev, skb);
			skb = NULL;
		}
	}

	return skb;
}
#endif

static int scsc_hup_recv(struct hci_uart *hu, const void *data, int count)
{
	struct hci_dev *hdev = hu->hdev;
	struct scsc_data *scsc = hu->priv;

	SCSC_TAG_DEBUG(BT_RX, "\n");
	if (!test_bit(SLSI_HUP_READY, &scsc->flags))
		return -ENXIO;

	if (test_bit(SLSI_HUP_STOP_RECV_FRAME, &scsc->flags))
		return 0;

	SCSC_TAG_DEBUG(BT_RX, "count=%d\n", count);
	if (count <= 0 || !data)
		return -EINVAL;

#ifdef USE_H4_RECV_PKT_API
	scsc->rx_skb = h4_recv_buf(hdev, scsc->rx_skb, data, count,
			  scsc_bt_recv_pkts, ARRAY_SIZE(scsc_bt_recv_pkts));
#else
	scsc->rx_skb = _h4_recv_buf(hdev, scsc->rx_skb, data, count);
#endif
	if (IS_ERR(scsc->rx_skb)) {
		int err = PTR_ERR(scsc->rx_skb);
		SCSC_TAG_ERR(BT_RX, "Frame reassembly failed (%d)", err);
		scsc->rx_skb = NULL;
		return err;
	}
	return count;
}

static int scsc_hup_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct scsc_data *scsc = hu->priv;

	if (!test_bit(SLSI_HUP_READY, &scsc->flags))
		return -ENXIO;

	if (test_bit(SLSI_HUP_STOP_SEND_FRAME, &scsc->flags)) {
		kfree_skb(skb);
		return 0;
	}

	memcpy(skb_push(skb, 1), &hci_skb_pkt_type(skb), 1);
	skb_queue_tail(&scsc->txq, skb);

	return 0;
}

static struct sk_buff *scsc_hup_dequeue(struct hci_uart *hu)
{
	struct scsc_data *scsc = hu->priv;
	struct sk_buff *skb;

	if (!scsc || !test_bit(SLSI_HUP_READY, &scsc->flags))
		return NULL;

	skb = skb_dequeue(&scsc->txq);
	if (!skb_queue_empty(&scsc->txq))
		set_bit(HCI_UART_TX_WAKEUP, &hu->tx_state);

	return skb;
}

static int scsc_hup_flush(struct hci_uart *hu)
{
	struct scsc_data *scsc = hu->priv;

	skb_queue_purge(&scsc->txq);
	return 0;
}

static const struct hci_uart_proto scsc_proto = {
	.id		= HCI_UART_H4,
	.name		= "SCSC_HCI_UART",
	.open		= scsc_hup_open,
	.close		= scsc_hup_close,
	.setup  	= scsc_hup_setup,
	.recv		= scsc_hup_recv,
	.enqueue	= scsc_hup_enqueue,
	.dequeue	= scsc_hup_dequeue,
	.flush		= scsc_hup_flush,
};

void scsc_hci_uart_resume(void)
{
	if (scscdev && scscdev->serdev_hu.priv) {
		struct scsc_data *scsc = NULL;

		scsc = (struct scsc_data *)scscdev->serdev_hu.priv;

		clear_bit(SLSI_HUP_STOP_RECV_FRAME, &scsc->flags);
		clear_bit(SLSI_HUP_STOP_SEND_FRAME, &scsc->flags);
	}
}

void scsc_hci_uart_block(void)
{
	struct scsc_data *scsc = NULL;

	if (scscdev && scscdev->serdev_hu.priv) {
		scsc = (struct scsc_data *)scscdev->serdev_hu.priv;

		set_bit(SLSI_HUP_STOP_RECV_FRAME, &scsc->flags);
		set_bit(SLSI_HUP_STOP_SEND_FRAME, &scsc->flags);
	}
}

static int scsc_serdev_probe(struct serdev_device *serdev)
{
	SCSC_TAG_INFO(BT_COMMON, "\n");

	scscdev = devm_kzalloc(&serdev->dev, sizeof(*scscdev), GFP_KERNEL);
	if (!scscdev)
		return -ENOMEM;

	scscdev->serdev_hu.serdev = serdev;
	serdev_device_set_drvdata(serdev, scscdev);

	return hci_uart_register_device(&scscdev->serdev_hu, &scsc_proto);
}

static void scsc_serdev_remove(struct serdev_device *serdev)
{
	struct scsc_serdev *scscdev = serdev_device_get_drvdata(serdev);

	hci_uart_unregister_device(&scscdev->serdev_hu);

	serdev_device_set_drvdata(serdev, NULL);
	scscdev = NULL;
}

static const struct of_device_id scsc_bluetooth_of_match[] = {
	{ .compatible = "samsung,scsc_bt", },
	{ },
};
MODULE_DEVICE_TABLE(of, scsc_bluetooth_of_match);

static struct serdev_device_driver scsc_serdev_driver = {
	.probe = scsc_serdev_probe,
	.remove = scsc_serdev_remove,
	.driver = {
		.name = "hci_uart_scsc_bt",
		.of_match_table	= of_match_ptr(scsc_bluetooth_of_match),
	},
};

int hci_scsc_init(void)
{
	int ret;
	SCSC_TAG_INFO(BT_COMMON, "\n");
	ret = serdev_device_driver_register(&scsc_serdev_driver);
	SCSC_TAG_INFO(BT_COMMON, "serdev_device_driver_register: %d\n", ret);

	return ret;
}

int hci_scsc_deinit(void)
{
	SCSC_TAG_INFO(BT_COMMON, "deinit\n");
	serdev_device_driver_unregister(&scsc_serdev_driver);
	return 0;
}

MODULE_LICENSE("GPL");
