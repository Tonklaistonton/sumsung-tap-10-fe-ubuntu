#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/of_device.h>
#include <linux/serdev.h>
#include <linux/mutex.h>

#include <linux/mod_devicetable.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>
#include <net/bluetooth/hci_core.h>

#include <../drivers/bluetooth/hci_uart.h>

#include "hci_pkt.h"
#include "slsi_bt_io.h"
#include "slsi_bt_controller.h"

static DEFINE_MUTEX(rx_lock);

struct slsi_data {
	struct hci_uart     *hu;

	struct sk_buff_head txq;

	struct hci_trans    *top_htr;
	struct hci_trans    *bottom_htr;

	unsigned long       flags;
};

/* SLSI_DATA flag bits */
#define SLSI_HUP_OPENED                0
#define SLSI_HUP_READY                 1    /* SLSI_HUP is setup */
#define SLSI_HUP_STOP_RECV_FRAME       2
#define SLSI_HUP_STOP_SEND_FRAME       3
#define SLSI_HUP_FILE_IO_CHANNEL       4    /* SLSI_HUP is opened by io */

struct slsi_serdev {
	struct hci_uart     serdev_hu;
} *slsidev;

/*
 * htr interface
 *
 * hci_uart_send_skb sends tx data to the controller by serial interface. The
 * packet is passed from upper layer of htr. The upper layer may be H5.
 *
 * hci_uart_recv_skb receives rx data from the controller by serial interface.
 * The packet will be passed to upper layer of htr. The upper layer may be H5.
 */
static int hci_uart_send_skb(struct hci_trans *htr, struct sk_buff *skb)
{
	if (htr && skb) {
		struct slsi_data *slsi = htr->tdata;
		int len = skb->len;

		TR_DBG("len: %d\n", len);
		skb_queue_tail(&slsi->txq, skb);
		hci_uart_tx_wakeup(slsi->hu);
		return len;
	}
	return 0;
}

int hci_uart_recv_skb(struct hci_trans *htr, struct sk_buff *slsi_skb)
{
	struct slsi_data *slsi = htr->tdata;
	struct hci_uart *hu = slsi->hu;
	struct hci_dev *hdev = hu->hdev;
	struct sk_buff *skb;

	TR_DBG("\n");
	if (slsi_skb && test_bit(SLSI_HUP_READY, &slsi->flags)) {
		if (test_bit(SLSI_HUP_STOP_RECV_FRAME, &slsi->flags)) {
			kfree_skb(slsi_skb);
			return 0;
		}

		skb = bt_skb_alloc(slsi_skb->len, GFP_ATOMIC);
		if (!skb) {
			kfree_skb(slsi_skb);
			return -ENOMEM;
		}

		TR_DBG("packet type: %u\n", GET_HCI_PKT_TYPE(slsi_skb));
		hci_skb_pkt_type(skb) = GET_HCI_PKT_TYPE(slsi_skb);
		hci_skb_expect(skb) = slsi_skb->len;

		skb_put_data(skb, slsi_skb->data, slsi_skb->len);
		kfree_skb(slsi_skb);

		return hci_recv_frame(hdev, skb);
	}
	return -EINVAL;
}

/*
 * slsi_hup functions are HCI UART Protocol APIs for slsi.
 */
static int slsi_hup_open(struct hci_uart *hu)
{
	struct slsi_data *slsi;

	BT_INFO("\n");
	/*
	if (!hci_uart_has_flow_control(hu))
		return -EOPNOTSUPP;
	*/

	slsi = kzalloc(sizeof(struct slsi_data), GFP_KERNEL);
	if (!slsi)
		return -ENOMEM;

	slsi->hu = hu;
	skb_queue_head_init(&slsi->txq);

	hu->priv = slsi;

	set_bit(SLSI_HUP_OPENED, &slsi->flags);
	return 0;
}

static int slsi_bt_setup(struct slsi_data *slsi, unsigned int enable_trs)
{
	int ret = 0;

	if (slsi == NULL)
		return -EINVAL;

	ret = slsi_bt_open(enable_trs, &slsi->top_htr);
	if (ret < 0) {
		BT_ERR("slsi_bt_open failed.\n");
		return ret;
	}

	/* Set htr to the bottom of htrs */
	if (slsi->top_htr) {
		slsi->bottom_htr = slsi->top_htr;
		while (hci_trans_get_next(slsi->bottom_htr))
			slsi->bottom_htr = hci_trans_get_next(slsi->bottom_htr);

		slsi->bottom_htr->tdata = slsi;
		slsi->bottom_htr->send_skb = hci_uart_send_skb;
	}

	set_bit(SLSI_HUP_READY, &slsi->flags);
	clear_bit(SLSI_HUP_STOP_RECV_FRAME, &slsi->flags);
	clear_bit(SLSI_HUP_STOP_SEND_FRAME, &slsi->flags);
	return ret;
}

static int slsi_bt_clear(struct slsi_data *slsi)
{
	if (slsi) {
		slsi->top_htr = NULL;
		slsi->bottom_htr = NULL;

		skb_queue_purge(&slsi->txq);

		clear_bit(SLSI_HUP_READY, &slsi->flags);
	}
	return slsi_bt_release();
}

static int slsi_hup_shutdown(struct hci_dev *hdev)
{
	struct hci_uart *hu = hci_get_drvdata(hdev);
	struct slsi_data *slsi = hu->priv;
	int ret = 0;

	BT_INFO("\n");
	if (!test_bit(SLSI_HUP_READY, &slsi->flags))
		return 0;

	mutex_lock(&rx_lock);
	set_bit(SLSI_HUP_STOP_RECV_FRAME, &slsi->flags);
	hdev->hw_error = NULL;
	hdev->cmd_timeout = NULL;
	ret = slsi_bt_clear(slsi);
	mutex_unlock(&rx_lock);

	return ret;
}

static int slsi_hup_close(struct hci_uart *hu)
{
	struct slsi_data *slsi = hu->priv;

	BT_INFO("\n");
	if (!test_bit(SLSI_HUP_OPENED, &slsi->flags))
		return 0;

	hu->priv = NULL;
	clear_bit(SLSI_HUP_OPENED, &slsi->flags);
	kfree(slsi);

	return 0;
}

static int slsi_hup_setup(struct hci_uart *hu)
{
	struct hci_dev *hdev = hu->hdev;
	struct slsi_data *slsi = hu->priv;
	unsigned int enable_trs;
	int ret;

	BT_INFO("\n");
	if (!test_bit(SLSI_HUP_OPENED, &slsi->flags))
		return -ENXIO;

	if (test_bit(SLSI_HUP_FILE_IO_CHANNEL, &slsi->flags)) {
		BT_ERR("slsi_bt is busy\n");
		return -EBUSY;
	}

	if (test_bit(SLSI_HUP_READY, &slsi->flags))
		return 0;

	set_bit(HCI_QUIRK_NON_PERSISTENT_SETUP, &hdev->quirks);

	enable_trs = SLSI_BT_TR_EN_PROP |
		     SLSI_BT_TR_EN_BCSP |
		     SLSI_BT_TR_EN_HCI_UART;
	ret = slsi_bt_setup(slsi, enable_trs);
	if (ret)
		return ret;

	/* Additional setup */
	slsi->top_htr->tdata = slsi;
	slsi->top_htr->recv_skb = hci_uart_recv_skb;

	hdev->shutdown = slsi_hup_shutdown;
	return 0;
}

static int slsi_hup_recv(struct hci_uart *hu, const void *data, int count)
{
	struct slsi_data *slsi = hu->priv;
	struct hci_trans *upper = NULL;
	int offset = 0, ret;

	if (!test_bit(SLSI_HUP_READY, &slsi->flags))
		return -ENXIO;

	mutex_lock(&rx_lock);
	if (test_bit(SLSI_HUP_STOP_RECV_FRAME, &slsi->flags))
		goto out;

	TR_DBG("count: %d bytes\n", count);
	upper = hci_trans_get_prev(slsi->bottom_htr);
	if (upper != NULL && upper->recv != NULL) {
		while (count > 0) {
			ret = upper->recv(upper, data + offset, count, 0);

			/* In the case of 1byte (c0), ret can be 0. In this
			 * case, it has already been copied */
			if (ret <= 0)
				break;

			offset += ret;
			count -= ret;
		}
	} else
		TR_WARNING("It does not have valid upper layer\n");

out:
	mutex_unlock(&rx_lock);
	return count;
}

static int slsi_hup_enqueue(struct hci_uart *hu, struct sk_buff *skb)
{
	struct slsi_data *slsi = hu->priv;
	struct hci_trans *htr = hci_trans_get_next(slsi->top_htr);
	struct sk_buff *slsi_skb;

	TR_DBG("pkt_type: %u, len: %zu\n", hci_skb_pkt_type(skb), skb->len);

	if (!test_bit(SLSI_HUP_READY, &slsi->flags))
		return -ENXIO;

	if (test_bit(SLSI_HUP_STOP_SEND_FRAME, &slsi->flags)) {
		kfree_skb(skb);
		return 0;
	}

	slsi_skb = __alloc_hci_pkt_skb(skb->len, 0);
	if (slsi_skb) {
		unsigned char type = hci_skb_pkt_type(skb);
		SET_HCI_PKT_TYPE(slsi_skb, type);
		SET_HCI_PKT_TR_TYPE(slsi_skb, HCI_TRANS_HCI);

		skb_put_data(slsi_skb, skb->data, skb->len);
	}
	kfree_skb(skb);

	if (!htr) {
		TR_WARNING("htr is null\n");
		kfree_skb(skb);
		return -EIO;
	}
	clear_bit(HCI_UART_TX_WAKEUP, &hu->tx_state);
	htr->send_skb(htr, slsi_skb);

	return 0;
}

static struct sk_buff *slsi_hup_dequeue(struct hci_uart *hu)
{
	struct slsi_data *slsi = hu->priv;
	struct sk_buff *skb;

	TR_DBG("\n");
	if (!slsi || !test_bit(SLSI_HUP_READY, &slsi->flags))
		return NULL;

	skb = skb_dequeue(&slsi->txq);
	if (!skb_queue_empty(&slsi->txq))
		set_bit(HCI_UART_TX_WAKEUP, &hu->tx_state);
	return skb;
}

static int slsi_hup_flush(struct hci_uart *hu)
{
	struct slsi_data *slsi = hu->priv;

	TR_DBG("\n");
	skb_queue_purge(&slsi->txq);
	return 0;
}

static const struct hci_uart_proto slsi_proto = {
	.id		= HCI_UART_H4,
	.name		= "SLSI_HCI_UART",
	.init_speed     = 4800000,
	.oper_speed     = 4800000,
	.open		= slsi_hup_open,
	.close		= slsi_hup_close,
	.setup          = slsi_hup_setup,
	.recv		= slsi_hup_recv,
	.enqueue	= slsi_hup_enqueue,
	.dequeue	= slsi_hup_dequeue,
	.flush		= slsi_hup_flush,
};

void slsi_hci_uart_resume(struct hci_trans *htr)
{
	struct slsi_data *slsi = htr->tdata;

	if (slsi) {
		clear_bit(SLSI_HUP_STOP_RECV_FRAME, &slsi->flags);
		clear_bit(SLSI_HUP_STOP_SEND_FRAME, &slsi->flags);
	}
}

void slsi_hci_uart_block(struct hci_trans *htr)
{
	struct slsi_data *slsi = htr->tdata;

	if (slsi) {
		set_bit(SLSI_HUP_STOP_RECV_FRAME, &slsi->flags);
		set_bit(SLSI_HUP_STOP_SEND_FRAME, &slsi->flags);
	}
}

int slsi_hci_uart_open_io(void)
{
	struct hci_uart *hu = &slsidev->serdev_hu;
	struct hci_dev *hdev = hu->hdev;
	struct slsi_data *slsi;
	unsigned int enable_trs;
	int ret = 0;

	BT_INFO("\n");
	if (hdev == NULL)
		return -EIO;

	ret = hdev->open(hdev);
	if (ret) {
		BT_ERR("hdev open failed: %d\n", ret);
		return -EIO;
	}

	/* hdev->open sets slsi_data to the hu->priv */
	slsi = hu->priv;
	if (test_bit(SLSI_HUP_READY, &slsi->flags)) {
		BT_ERR("slsi_bt is busy\n");
		return -EBUSY;
	}

	enable_trs = SLSI_BT_TR_EN_H4   | SLSI_BT_TR_EN_PROP |
		     SLSI_BT_TR_EN_BCSP | SLSI_BT_TR_EN_HCI_UART;

	set_bit(SLSI_HUP_FILE_IO_CHANNEL, &slsi->flags);
	return slsi_bt_setup(slsi, enable_trs);
}

int slsi_hci_uart_close_io(void)
{
	struct hci_uart *hu = &slsidev->serdev_hu;
	struct hci_dev *hdev = hu->hdev;
	struct slsi_data *slsi = hu->priv;

	BT_INFO("\n");

	slsi_hup_flush(hu);
	hdev->close(hdev);
	clear_bit(SLSI_HUP_FILE_IO_CHANNEL, &slsi->flags);
	return slsi_bt_clear(slsi);
}

static int slsi_serdev_probe(struct serdev_device *serdev)
{
	BT_INFO("\n");

	slsidev = devm_kzalloc(&serdev->dev, sizeof(*slsidev), GFP_KERNEL);
	if (!slsidev)
		return -ENOMEM;

	slsidev->serdev_hu.serdev = serdev;
	serdev_device_set_drvdata(serdev, slsidev);

	return hci_uart_register_device(&slsidev->serdev_hu, &slsi_proto);
}

static void slsi_serdev_remove(struct serdev_device *serdev)
{
	struct slsi_serdev *slsidev = serdev_device_get_drvdata(serdev);

	hci_uart_unregister_device(&slsidev->serdev_hu);
	slsidev = NULL;
}


static const struct of_device_id slsi_bluetooth_of_match[] = {
	{ .compatible = "samsung,s6375-bt",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, slsi_bluetooth_of_match);

static struct serdev_device_driver slsi_serdev_driver = {
	.probe = slsi_serdev_probe,
	.remove = slsi_serdev_remove,
	.driver = {
		.name = "hci_uart_slsi_bt",
		.of_match_table = slsi_bluetooth_of_match,
	},
};

int hci_slsi_init(void)
{
	BT_INFO("\n");
	return serdev_device_driver_register(&slsi_serdev_driver);
}

int hci_slsi_deinit(void)
{
	BT_INFO("deinit\n");
	serdev_device_driver_unregister(&slsi_serdev_driver);
	return 0;
}

MODULE_LICENSE("GPL");
