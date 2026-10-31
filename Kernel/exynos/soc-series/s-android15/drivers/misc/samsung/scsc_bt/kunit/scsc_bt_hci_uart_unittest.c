/******************************************************************************
 *                                                                            *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth Driver Unittest for KUNIT                                        *
 *                                                                            *
 ******************************************************************************/
#include <kunit/test.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci.h>
#include <net/bluetooth/hci_core.h>
#include <linux/slab.h>
#include <linux/kallsyms.h>
#include <linux/serdev.h>
#include "fake.h"


/******************************************************************************
 * Mock
 ******************************************************************************/
// disable serial_deb_bus, so that the mock_serdev_device_driver_register is not needed.
#if IS_ENABLED(CONFIG_SERIAL_DEV_BUS)
#undef CONFIG_SERIAL_DEV_BUS
#endif

#ifndef UNUSED
#define UNUSED(x)		((void)(x))
#endif

struct hci_uart_proto;
struct hci_uart;

// function definition
#define hci_recv_frame mock_hci_recv_frame
#define hci_uart_unregister_device mock_hci_uart_unregister_device
#define devm_kzalloc mock_devm_kzalloc

// function mocked
static int mock_hci_recv_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
	UNUSED(hdev);
	UNUSED(skb);

	return 1;
}

int mock_hci_uart_register_device(struct hci_uart *hu, const struct hci_uart_proto *p)
{
	UNUSED(hu);
	UNUSED(p);

	return 0;
}

void mock_hci_uart_unregister_device(struct hci_uart *hu)
{
	UNUSED(hu);
}

static void *mock_devm_kzalloc(struct device *dev, size_t size, gfp_t gfp)
{
	return kzalloc(size, gfp);
}

#ifdef USE_H4_RECV_PKT_API
struct h4_recv_pkt;
#define h4_recv_buf mock_h4_recv_buf
static struct sk_buff *mock_h4_recv_buf(struct hci_dev *hdev,
					  struct sk_buff *skb,
					  const unsigned char *buffer,
					  int count,
					  const struct h4_recv_pkt *pkts,
					  int pkts_count)
{
	UNUSED(hdev);
	UNUSED(skb);
	UNUSED(buffer);
	UNUSED(count);
	UNUSED(pkts);
	UNUSED(pkts_count);

	return NULL;
}
#endif


/******************************************************************************
 * Include the code under test
 ******************************************************************************/
#include "../scsc_bt_hci_uart.c"


/******************************************************************************
 * extern
 ******************************************************************************/
extern int test_common_scsc_bt_module_init_probe(struct kunit *test);
extern void test_common_scsc_bt_module_remove_exit(struct kunit *test);
extern void test_common_scsc_bt_module_normal_open(struct kunit *test);
extern void test_common_scsc_bt_module_normal_close(struct kunit *test);


/******************************************************************************
 * global struct variable definition
 ******************************************************************************/
static struct scsc_serdev mokd_sserdev = {};
static struct hci_dev moke_hdev={};
static char buffer_pool[120000];
extern struct fake_buffer_pool scsc_mx_fake_buffer_pool;


/******************************************************************************
 * Test target code
 ******************************************************************************/
static void util_scsc_hup_open(struct kunit *test)
{
	int ret;
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc;

	kunit_info(test, "scsc_hup_open\n");
	ret = scsc_hup_open(hu);
	scsc = hu->priv;

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_NOT_NULL(test, scsc);
	KUNIT_EXPECT_TRUE(test, test_bit(SLSI_HUP_OPENED, &scsc->flags));
}

static void util_scsc_hup_close(struct kunit *test)
{
	int ret;
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc = hu->priv;

	KUNIT_EXPECT_TRUE(test, test_bit(SLSI_HUP_OPENED, &scsc->flags));

	kunit_info(test, "scsc_hup_close\n");
	ret = scsc_hup_close(hu);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, test_bit(SLSI_HUP_OPENED, &scsc->flags));
}

static void scsc_hup_open_close_test(struct kunit *test)
{
	util_scsc_hup_open(test);
	util_scsc_hup_close(test);
}

static void util_scsc_hup_setup(struct kunit *test)
{
	int ret;
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc;

	hu->hdev = &moke_hdev;
	dev_set_drvdata(&moke_hdev.dev, hu);

	test_common_scsc_bt_module_init_probe(test);

	util_scsc_hup_open(test);
	scsc = hu->priv;

	/* Testing for abnormal conditions: mifram alloc failed Start */
	ret = scsc_hup_setup(hu);

	KUNIT_EXPECT_LT(test, ret, 0);
	/* Testing for abnormal conditions: mifram alloc failed End */

	// Prepare
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, sizeof(buffer_pool));

	kunit_info(test, "scsc_hup_setup\n");
	ret = scsc_hup_setup(hu);

	KUNIT_EXPECT_EQ(test, 0, ret);

	/* Testing for abnormal conditions: SLSI_HUP_READY is set Start */
	ret = scsc_hup_setup(hu);

	KUNIT_EXPECT_EQ(test, 0, ret);
	/* Testing for abnormal conditions: SLSI_HUP_READY is set End */
}

static void util_scsc_hup_shutdown(struct kunit *test)
{
	int ret;
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc = hu->priv;

	if(!test_bit(SLSI_HUP_READY, &scsc->flags)) {
		kunit_info(test, "mock system before test");
		util_scsc_hup_setup(test);
	}

	kunit_info(test, "scsc_hup_shutdown\n");
	ret = scsc_hup_shutdown(&moke_hdev);

	KUNIT_EXPECT_EQ(test, 0, ret);

	util_scsc_hup_close(test);
	test_common_scsc_bt_module_remove_exit(test);

	KUNIT_SUCCEED(test);
}

static void scsc_hup_setup_shutdown_test(struct kunit *test)
{
	util_scsc_hup_setup(test);
	util_scsc_hup_shutdown(test);
}

static void scsc_hup_recv_test(struct kunit *test)
{
	// Event: HCI_RESET
	const unsigned char event_hci_reset[] = {
		0x04,		// HCI Event packet type
		0x0E,		// Event code (Command Complete)
		0x04,		// Parameter total length
		0x01,		// Number hci command packets
		0x03, 0x0C, // Command opcode (0x0C03)
		0x00		// Status (Success)
	};
	int ret;
	int count = sizeof(event_hci_reset);
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc;
	struct sk_buff *skb = alloc_skb(10, GFP_KERNEL);

	util_scsc_hup_open(test);
	scsc = hu->priv;

	// Setup mock structures
	memset(hu, 0, sizeof(&hu));
	memset(&moke_hdev, 0, sizeof(moke_hdev));
	memset(scsc, 0, sizeof(&scsc));

	hu->hdev = &moke_hdev;

	skb_queue_head_init(&scsc->txq);

	scsc->rx_skb = NULL;

	set_bit(SLSI_HUP_READY, &scsc->flags);

	kunit_info(test, "scsc_hup_recv\n");
	ret = scsc_hup_recv(hu, event_hci_reset, count);

	KUNIT_EXPECT_EQ(test, ret, count);

	util_scsc_hup_close(test);

	if (scsc->rx_skb)
		kfree_skb(scsc->rx_skb);
}

static void scsc_hup_enqueue_dequeue_test(struct kunit *test)
{
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc;
	struct sk_buff *skb;
	struct sk_buff *dequeued_skb;
	int ret;
	bool empty;

	util_scsc_hup_setup(test);
	scsc = hu->priv;

	/* Testing for abnormal conditions: SLSI_HUP_STOP_SEND_FRAME set Start */
	skb = alloc_skb(12, GFP_KERNEL);
	set_bit(SLSI_HUP_STOP_SEND_FRAME, &scsc->flags);
	ret = scsc_hup_enqueue(hu, skb);

	KUNIT_EXPECT_EQ(test, 0, ret);

	kfree_skb(skb);
	clear_bit(SLSI_HUP_STOP_SEND_FRAME, &scsc->flags);
	/* Testing for abnormal conditions: SLSI_HUP_STOP_SEND_FRAME set End */

	skb = alloc_skb(12, GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, skb);

	skb_reserve(skb, 10);
	*(unsigned char *)skb_push(skb, 1) = HCI_ACLDATA_PKT;
	skb_put(skb, 1);

	kunit_info(test, "scsc_hup_enqueue\n");
	ret = scsc_hup_enqueue(hu, skb);
	empty = skb_queue_empty(&scsc->txq);

	KUNIT_EXPECT_EQ(test, 0, ret);
	// empty is false
	KUNIT_EXPECT_FALSE(test, empty);

	kunit_info(test, "scsc_hup_dequeue\n");
	dequeued_skb = scsc_hup_dequeue(hu);
	empty = skb_queue_empty(&scsc->txq);

	KUNIT_ASSERT_NOT_NULL(test, dequeued_skb);
	KUNIT_EXPECT_EQ(test, skb, dequeued_skb);
	// empty is true
	KUNIT_EXPECT_TRUE(test, empty);

	util_scsc_hup_shutdown(test);
	kfree_skb(skb);
}

static void scsc_hup_flush_test(struct kunit *test)
{
	int ret;
	bool empty;
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc;

	util_scsc_hup_open(test);
	scsc = hu->priv;

	kunit_info(test, "scsc_hup_flush\n");
	ret = scsc_hup_flush(hu);
	empty = skb_queue_empty(&scsc->txq);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, empty);

	util_scsc_hup_close(test);
}

static void util_hci_scsc_init(struct kunit *test)
{
	int ret;

	kunit_info(test, "hci_scsc_init\n");
	ret = hci_scsc_init();

	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void util_hci_scsc_deinit(struct kunit *test)
{
	int ret;

	kunit_info(test, "hci_scsc_deinit\n");
	ret = hci_scsc_deinit();

	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void hci_scsc_init_deinit_test(struct kunit *test)
{
	util_hci_scsc_init(test);
	util_hci_scsc_deinit(test);
}

static void util_scsc_hci_uart_block(struct kunit *test)
{
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc;

	util_scsc_hup_open(test);
	scsc = hu->priv;

	scscdev = &mokd_sserdev;

	kunit_info(test, "scsc_hci_uart_block\n");
	scsc_hci_uart_block();

	KUNIT_EXPECT_TRUE(test, test_bit(SLSI_HUP_STOP_RECV_FRAME, &scsc->flags));
	KUNIT_EXPECT_TRUE(test, test_bit(SLSI_HUP_STOP_SEND_FRAME, &scsc->flags));
}

static void util_scsc_hci_uart_resume(struct kunit *test)
{
	struct hci_uart *hu = &mokd_sserdev.serdev_hu;
	struct scsc_data *scsc = hu->priv;

	kunit_info(test, "scsc_hci_uart_resume\n");
	scsc_hci_uart_resume();

	KUNIT_EXPECT_FALSE(test, test_bit(SLSI_HUP_STOP_RECV_FRAME, &scsc->flags));
	KUNIT_EXPECT_FALSE(test, test_bit(SLSI_HUP_STOP_SEND_FRAME, &scsc->flags));

	util_scsc_hup_close(test);
}

static void scsc_hci_uart_block_resume_test(struct kunit *test)
{
	util_scsc_hci_uart_block(test);
	util_scsc_hci_uart_resume(test);
}

static void scsc_serdev_probe_remote_test(struct kunit *test)
{
	int ret;
	struct serdev_device *mock_serdev_dev;
	struct scsc_serdev *scscdev;

	mock_serdev_dev = kzalloc(sizeof(struct serdev_device), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, mock_serdev_dev);

	kunit_info(test, "scsc_serdev_probe\n");
	ret = scsc_serdev_probe(mock_serdev_dev);

	KUNIT_EXPECT_EQ(test, 0, ret);

	scscdev = serdev_device_get_drvdata(mock_serdev_dev);

	KUNIT_ASSERT_NOT_NULL(test, scscdev);

	kunit_info(test, "scsc_serdev_remove\n");
	scsc_serdev_remove(mock_serdev_dev);
	scscdev = serdev_device_get_drvdata(mock_serdev_dev);

	KUNIT_EXPECT_NULL(test, scscdev);

	kfree(mock_serdev_dev);
}

static struct kunit_case scsc_bt_hci_uart_test_cases[] = {
	KUNIT_CASE(scsc_hup_open_close_test),
	KUNIT_CASE(scsc_hup_setup_shutdown_test),
	KUNIT_CASE(scsc_hup_recv_test),
	KUNIT_CASE(scsc_hup_enqueue_dequeue_test),
	KUNIT_CASE(scsc_hup_flush_test),
	KUNIT_CASE(hci_scsc_init_deinit_test),
	KUNIT_CASE(scsc_hci_uart_block_resume_test),
	KUNIT_CASE(scsc_serdev_probe_remote_test),
	{}
};

static struct kunit_suite scsc_bt_hci_uart_test_suite = {
	.name = "scsc_bt_hci_uart_unittest",
	.test_cases = scsc_bt_hci_uart_test_cases,
	.init = NULL,
	.exit = NULL,
};

kunit_test_suite(scsc_bt_hci_uart_test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SSCR");
MODULE_VERSION("-develop");
MODULE_DESCRIPTION("scsc_bt driver KUnit");
