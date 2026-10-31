/******************************************************************************
 *                                                                            *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth Driver Unittest for KUNIT                                        *
 *                                                                            *
 ******************************************************************************/
#include <kunit/test.h>
#include "test_helper_wait.h"

/******************************************************************************
 * Test target code
 ******************************************************************************/
#include "../scsc_shm.c"

/******************************************************************************
 * Test common
 ******************************************************************************/
extern int test_common_scsc_bt_module_init_probe(struct kunit *test);
extern void test_common_scsc_bt_module_remove_exit(struct kunit *test);
extern void test_common_scsc_bt_module_normal_open(struct kunit *test);
extern void test_common_scsc_bt_module_normal_close(struct kunit *test);

static enum {
	SCSC_BT_DEINIT,
	SCSC_BT_CLOSED,
	SCSC_BT_OPENED,
	SCSC_BT_OPEN_REQUEST,
} scsc_bt_open_state;

int scsc_shm_test_init(struct kunit *test)
{
	test_common_scsc_bt_module_init_probe(test);
	if (scsc_bt_open_state == SCSC_BT_OPEN_REQUEST ||
	    scsc_bt_open_state == SCSC_BT_CLOSED) {
		test_common_scsc_bt_module_normal_open(test);
		scsc_bt_open_state = SCSC_BT_OPENED;
	}
	return 0;
}

void scsc_shm_test_exit(struct kunit *test)
{
	test_common_scsc_bt_module_remove_exit(test);
	if (scsc_bt_open_state == SCSC_BT_OPENED) {
		test_common_scsc_bt_module_normal_close(test);
		scsc_bt_open_state = SCSC_BT_CLOSED;
	}
}

static void scsc_shm_test_prepare(struct kunit *test)
{
	test_common_scsc_bt_module_init_probe(test);
	scsc_bt_open_state = SCSC_BT_OPEN_REQUEST;
	test_common_scsc_bt_module_normal_open(test);
	scsc_bt_open_state = SCSC_BT_OPENED;
	bt_svc = get_bt_service();
	KUNIT_SUCCEED(test);
}

static void scsc_shm_test_end(struct kunit *test)
{
	test_common_scsc_bt_module_normal_close(test);
	scsc_bt_open_state = SCSC_BT_CLOSED;
	test_common_scsc_bt_module_remove_exit(test);
	scsc_bt_open_state = SCSC_BT_DEINIT;
	KUNIT_SUCCEED(test);
}

/******************************************************************************
 * Test cases
 ******************************************************************************/
void reset_mailbox(void)
{
	bt_svc->bsmhcp_protocol->header.mailbox_hci_evt_write = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_hci_evt_read = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_acl_rx_write = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_acl_rx_read = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_acl_free_write = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_acl_free_read = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_iq_report_write = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_iq_report_read = 0;
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_rx_write = 0;
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_rx_read = 0;
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_free_write = 0;
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_free_read = 0;

	bt_svc->bsmhcp_protocol->header.mailbox_hci_cmd_write = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_hci_cmd_read = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_acl_tx_write = 0;
	bt_svc->bsmhcp_protocol->header.mailbox_acl_tx_read = 0;
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_tx_write = 0;
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_tx_read = 0;

	bt_svc->mailbox_hci_evt_read = 0;
	bt_svc->mailbox_hci_evt_write = 0;
	bt_svc->mailbox_acl_rx_read = 0;
	bt_svc->mailbox_acl_rx_write = 0;
	bt_svc->mailbox_acl_free_read = 0;
	bt_svc->mailbox_acl_free_write = 0;
	bt_svc->mailbox_iq_report_read = 0;
	bt_svc->mailbox_iq_report_write = 0;
	bt_svc->mailbox_iso_rx_read = 0;
	bt_svc->mailbox_iso_rx_write = 0;
	bt_svc->mailbox_iso_free_read = 0;
	bt_svc->mailbox_iso_free_write = 0;
}

static void scsc_shm_read_data_available_test(struct kunit *test)
{
	reset_mailbox();
	bt_svc->bsmhcp_protocol->header.mailbox_hci_evt_write = 1;
	KUNIT_EXPECT_TRUE(test, scsc_read_data_available());

	reset_mailbox();
	bt_svc->bsmhcp_protocol->header.mailbox_acl_rx_write = 1;
	KUNIT_EXPECT_TRUE(test, scsc_read_data_available());

	reset_mailbox();
	bt_svc->bsmhcp_protocol->header.mailbox_acl_free_write = 1;
	KUNIT_EXPECT_TRUE(test, scsc_read_data_available());

	reset_mailbox();
	bt_svc->bsmhcp_protocol->header.mailbox_iq_report_write = 1;
	KUNIT_EXPECT_TRUE(test, scsc_read_data_available());

	reset_mailbox();
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_rx_write = 1;
	KUNIT_EXPECT_TRUE(test, scsc_read_data_available());

	reset_mailbox();
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_free_write = 1;
	KUNIT_EXPECT_TRUE(test, scsc_read_data_available());

	reset_mailbox();
	KUNIT_EXPECT_FALSE(test, scsc_read_data_available());

	KUNIT_SUCCEED(test);
}

static void scsc_bt_shm_irq_handler_test(struct kunit *test)
{

	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, true);
	test_helper_wait_init(test);

	reset_mailbox();
	bt_svc->bsmhcp_protocol->header.panic_deathbed_confession = 1;
	test_helper_wait(&bt_svc->read_wait);

	scsc_bt_shm_irq_handler(0, NULL);
	test_helper_wait_deinit();

	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, false);

	KUNIT_SUCCEED(test);
}

static void scsc_shm_h4_poll_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	bool backup_service_started = bt_svc->service_started;

	test_helper_wait_init(test);

	test_helper_wake_up_after_ms(&bt_svc->read_wait, 100, NULL);
	KUNIT_EXPECT_EQ(test, POLLOUT, (int)scsc_bt_shm_h4_poll(file, NULL));

	bt_svc->service_started = false;
	test_helper_wake_up_after_ms(&bt_svc->read_wait, 100, NULL);
	KUNIT_EXPECT_EQ(test, POLLOUT, (int)scsc_bt_shm_h4_poll(file, NULL));
	bt_svc->service_started = backup_service_started;

	reset_mailbox();
	bt_svc->bsmhcp_protocol->header.mailbox_hci_evt_write = 1;
	test_helper_wake_up_after_ms(&bt_svc->read_wait, 100, NULL);
	KUNIT_EXPECT_EQ(test, POLLIN | POLLRDNORM, (int)scsc_bt_shm_h4_poll(file, NULL));


	test_helper_wait_deinit();
	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void reset_read_information(void)
{
	bt_svc->read_operation = BT_READ_OP_NONE;
	bt_svc->read_offset = 0;
	bt_svc->hci_event_paused = false;
	bt_svc->data_paused = false;
	bt_svc->processed[0] = false;
	bt_svc->connection_handle_list[1].state = CONNECTION_NONE;
}

static void reset_connection_information(void)
{
	int i;

	for (i = 0; i < SCSC_BT_CONNECTION_INFO_MAX; i++)
		bt_svc->connection_handle_list[i].state = CONNECTION_NONE;
}

static void write_hci_evt(struct BSMHCP_TD_HCI_EVT *td)
{
	int w = bt_svc->bsmhcp_protocol->header.mailbox_hci_evt_write;

	bt_svc->bsmhcp_protocol->hci_evt_transfer_ring[w] = *td;
	BSMHCP_INCREASE_INDEX(w, BSMHCP_TRANSFER_RING_EVT_SIZE);
	bt_svc->bsmhcp_protocol->header.mailbox_hci_evt_write = w;
}

static void set_error(void)
{
	atomic_inc(&bt_svc->error_count);
}

static void scsc_shm_h4_read_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct BSMHCP_TD_HCI_EVT testtd;
	char buf[1024], *p;
	ssize_t expect_len;

	// Prepare
	reset_mailbox();
	reset_read_information();
	reset_connection_information();
	memset(&testtd, 0, sizeof(struct BSMHCP_TD_HCI_EVT));
	testtd.hci_connection_handle = 1;
	testtd.length = HCI_EVENT_HEADER_LENGTH + testtd.data[1];
	expect_len = 0;

#if 1
	kunit_info(test, "fg int\n");
	reset_mailbox();
	reset_read_information();
	testtd.event_type = BSMHCP_EVENT_TYPE_NONE;
	testtd.length = HCI_EVENT_HEADER_LENGTH + testtd.data[1];
	write_hci_evt(&testtd);
	bt_svc->bsmhcp_protocol->header.mailbox_acl_rx_read = 1;
	bt_svc->bsmhcp_protocol->header.mailbox_iq_report_read = 1;
	expect_len = H4_HEADER_SIZE + testtd.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
	reset_mailbox();
	reset_read_information();
#endif
#if 1
	kunit_info(test, "connection max, handle error during read\n");
	testtd.event_type = BSMHCP_EVENT_TYPE_CONNECTED;
	testtd.hci_connection_handle = SCSC_BT_CONNECTION_INFO_MAX;
	write_hci_evt(&testtd);
	expect_len = H4_HEADER_SIZE;
	p = buf;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
	p += expect_len;
	expect_len = sizeof(h4_hci_event_hardware_error) - H4_HEADER_SIZE;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
	KUNIT_EXPECT_TRUE(test, !memcmp(h4_hci_event_hardware_error, buf, expect_len));
	atomic_set(&bt_svc->error_count, 0);
	bt_svc->connection_handle_list[testtd.hci_connection_handle].state = CONNECTION_NONE;
	testtd.hci_connection_handle = 1;
	reset_read_information();
#endif
#if 1
	kunit_info(test, "NONBLOCK requested\n");
	reset_mailbox();
	reset_read_information();
	// Nothing updated
	file->f_flags |= O_NONBLOCK;
	KUNIT_EXPECT_EQ(test, -EAGAIN, (int)scsc_bt_shm_h4_read(file, buf, 1, 0));
	file->f_flags &= ~O_NONBLOCK;
#endif
#if 1
	kunit_info(test, "wait for interrupt in read() + error during waiting\n");
	test_helper_wait_init(test);
	test_helper_wake_up_after_ms(&bt_svc->read_wait, 100, set_error);
	expect_len = sizeof(h4_hci_event_hardware_error);
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
	KUNIT_EXPECT_TRUE(test, !memcmp(h4_hci_event_hardware_error, buf, expect_len));
	atomic_set(&bt_svc->error_count, 0);
	test_helper_wait_deinit();
#endif

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_h4_read_hci_evt_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct BSMHCP_TD_HCI_EVT testtd;
	char buf[10];
	ssize_t expect_len;

	// Prepare
	reset_mailbox();
	reset_read_information();
	reset_connection_information();
	memset(&testtd, 0, sizeof(struct BSMHCP_TD_HCI_EVT));
	testtd.hci_connection_handle = 1;
	testtd.length = HCI_EVENT_HEADER_LENGTH + testtd.data[1];
	expect_len = 0;

#if 1
	kunit_info(test, "read(1) for H4 header, read(%d) for packet\n", HCI_EVENT_HEADER_LENGTH);
	testtd.event_type = BSMHCP_EVENT_TYPE_NONE;
	write_hci_evt(&testtd);
	KUNIT_EXPECT_EQ(test, (ssize_t)H4_HEADER_SIZE, scsc_bt_shm_h4_read(file, buf, H4_HEADER_SIZE, 0));
	expect_len = HCI_EVENT_HEADER_LENGTH;
	KUNIT_EXPECT_EQ(test, expect_len,
			scsc_bt_shm_h4_read(file, buf+1, expect_len, 0));
	KUNIT_EXPECT_EQ(test, (char)HCI_EVENT_PKT, buf[0]);
#endif
#if 1
	kunit_info(test, "read(%d) for H4 header + packet\n", H4_HEADER_SIZE + HCI_EVENT_HEADER_LENGTH);
	testtd.event_type = BSMHCP_EVENT_TYPE_NONE;
	write_hci_evt(&testtd);
	KUNIT_EXPECT_EQ(test, (char)HCI_EVENT_PKT, buf[0]);
	expect_len = 1 + testtd.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
#endif
#if 1
	kunit_info(test, "pre processed\n");
	bt_svc->processed[bt_svc->bsmhcp_protocol->header.mailbox_hci_evt_read] = true;
	testtd.event_type = BSMHCP_EVENT_TYPE_NONE;
	write_hci_evt(&testtd);
	write_hci_evt(&testtd);
	expect_len = 1 + testtd.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
#endif
#if 1
	kunit_info(test, "connected\n");
	testtd.event_type = BSMHCP_EVENT_TYPE_CONNECTED;
	write_hci_evt(&testtd);
	expect_len = 1 + testtd.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
	bt_svc->connection_handle_list[testtd.hci_connection_handle].state = CONNECTION_NONE;
#endif
#if 1
	kunit_info(test, "disconnected\n");
	bt_svc->connection_handle_list[testtd.hci_connection_handle].state = CONNECTION_DISCONNECTED;
	testtd.event_type = BSMHCP_EVENT_TYPE_DISCONNECTED;
	write_hci_evt(&testtd);
	expect_len = H4_HEADER_SIZE + testtd.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
#endif
#if 1
	kunit_info(test, "invalid hci event\n");
	testtd.event_type = BSMHCP_EVENT_TYPE_NONE;
	testtd.length = 1;
	write_hci_evt(&testtd);
	KUNIT_EXPECT_EQ(test, 1, (int)scsc_bt_shm_h4_read(file, buf, 1, 0));
	KUNIT_EXPECT_EQ(test, 1, atomic_read(&bt_svc->error_count));
	atomic_set(&bt_svc->error_count, 0);
	testtd.length = HCI_EVENT_HEADER_LENGTH + testtd.data[1];
#endif
	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void write_acl_rx_data(struct BSMHCP_TD_ACL_RX *td)
{
	int w = bt_svc->bsmhcp_protocol->header.mailbox_acl_rx_write;

	bt_svc->bsmhcp_protocol->acl_rx_transfer_ring[w] = *td;
	BSMHCP_INCREASE_INDEX(w, BSMHCP_TRANSFER_RING_ACL_SIZE);
	bt_svc->bsmhcp_protocol->header.mailbox_acl_rx_write = w;
}

static void scsc_shm_h4_read_acl_data_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct BSMHCP_TD_ACL_RX testtd;
	char buf[1024], *p;
	char testdata[] = { 0x01, 0x02, 0x03, 0x04 };
	ssize_t expect_len;
	int testconnection = 0;

	// Prepare
	reset_mailbox();
	reset_read_information();
	reset_connection_information();
	bt_svc->connection_handle_list[testconnection].state = CONNECTION_ACTIVE;
	memset(&testtd, 0, sizeof(struct BSMHCP_TD_ACL_RX));
	testtd.header.hci_connection_handle = testconnection;
	testtd.header.pb_flag = 0;;
	testtd.header.flag = 0;
	testtd.header.length = sizeof(testdata);
	memcpy(testtd.data, testdata, testtd.header.length);
	expect_len = 0;

#if 1
	kunit_info(test, "read three times\n");
	write_acl_rx_data(&testtd);
	p = buf;
	KUNIT_EXPECT_EQ(test, (ssize_t)H4_HEADER_SIZE, scsc_bt_shm_h4_read(file, p, H4_HEADER_SIZE, 0));
	p += H4_HEADER_SIZE;
	expect_len = ACLDATA_HEADER_SIZE;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
	p += expect_len;
	expect_len = testtd.header.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
#endif

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void write_iso_rx_data(struct BSMHCP_TD_ISO_RX *td)
{
	int w = bt_svc->bsmhcp_protocol->header_2.mailbox_iso_rx_write;

	bt_svc->bsmhcp_protocol->iso_rx_transfer_ring[w] = *td;
	BSMHCP_INCREASE_INDEX(w, BSMHCP_TRANSFER_RING_ISO_RX_SIZE);
	bt_svc->bsmhcp_protocol->header_2.mailbox_iso_rx_write = w;
}

static void scsc_shm_h4_read_iso_data_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct BSMHCP_TD_ISO_RX testtd;
	char buf[1024], *p;
	char testdata[] = { 0x01, 0x02, 0x03, 0x04 };
	ssize_t expect_len;
	int testconnection = 0;

	// Prepare
	reset_mailbox();
	reset_read_information();
	reset_connection_information();
	bt_svc->connection_handle_list[testconnection].state = CONNECTION_ACTIVE;
	memset(&testtd, 0, sizeof(struct BSMHCP_TD_ISO_RX));
	testtd.header.hci_connection_handle = testconnection;
	testtd.header.pb_flag = 0;;
	testtd.header.flag = 0;
	testtd.header.length = sizeof(testdata);
	memcpy(testtd.data, testdata, testtd.header.length);
	expect_len = 0;

#if 1
	kunit_info(test, "read three times\n");
	write_iso_rx_data(&testtd);
	p = buf;
	KUNIT_EXPECT_EQ(test, (ssize_t)H4_HEADER_SIZE, scsc_bt_shm_h4_read(file, p, H4_HEADER_SIZE, 0));
	p += H4_HEADER_SIZE;
	expect_len = ISODATA_HEADER_SIZE;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
	p += expect_len;
	expect_len = (buf[3] | (buf[4]<<8));
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
#endif

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void write_credit(struct BSMHCP_TD_ACL_TX_FREE *td_af, struct BSMHCP_TD_ISO_TX_FREE *td_if)
{
	int w;

	if (td_af) {
		w = bt_svc->bsmhcp_protocol->header.mailbox_acl_free_write;

		bt_svc->bsmhcp_protocol->acl_tx_free_transfer_ring[w] = *td_af;
		BSMHCP_INCREASE_INDEX(w, BSMHCP_TRANSFER_RING_ACL_SIZE);
		bt_svc->bsmhcp_protocol->header.mailbox_acl_free_write = w;
	}
	if (td_if) {
		w = bt_svc->bsmhcp_protocol->header_2.mailbox_iso_free_write;

		bt_svc->bsmhcp_protocol->iso_tx_free_transfer_ring[w] = *td_af;
		BSMHCP_INCREASE_INDEX(w, BSMHCP_TRANSFER_RING_ISO_TX_SIZE);
		bt_svc->bsmhcp_protocol->header_2.mailbox_iso_free_write = w;
	}
}

static void scsc_shm_h4_read_credit_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct BSMHCP_TD_ACL_TX_FREE testtd_af;
	struct BSMHCP_TD_ISO_TX_FREE testtd_if;
	char buf[10], *p;
	ssize_t expect_len;

	kunit_info(test, "credit for acl, iso read three times\n");
	bt_svc->connection_handle_list[0].state = CONNECTION_ACTIVE;
	testtd_af.hci_connection_handle = 0;
	bt_svc->connection_handle_list[1].state = CONNECTION_ACTIVE;
	testtd_if.hci_connection_handle = 1;
	write_credit(&testtd_af, &testtd_if);
	p = buf;
	KUNIT_EXPECT_EQ(test, (ssize_t)H4_HEADER_SIZE, scsc_bt_shm_h4_read(file, p, H4_HEADER_SIZE, 0));
	p += H4_HEADER_SIZE;
	expect_len = HCI_EVENT_HEADER_LENGTH;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
	p += expect_len;
	expect_len = buf[2];
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));

	kunit_info(test, "credit test 2\n");
	bt_svc->connection_handle_list[2].state = CONNECTION_ACTIVE;
	testtd_af.hci_connection_handle = 2;
	write_credit(&testtd_af, NULL);
	write_credit(&testtd_af, NULL);
	expect_len = H4_HEADER_SIZE + HCI_EVENT_HEADER_LENGTH;
	p = buf;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
	p += expect_len;
	expect_len = buf[2];
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void write_iq_report(struct BSMHCP_TD_IQ_REPORTING_EVT *td)
{
	int w = bt_svc->bsmhcp_protocol->header.mailbox_iq_report_write;

	bt_svc->bsmhcp_protocol->iq_reporting_transfer_ring[w] = *td;
	BSMHCP_INCREASE_INDEX(w, BSMHCP_TRANSFER_RING_IQ_REPORT_SIZE);
	bt_svc->bsmhcp_protocol->header.mailbox_iq_report_write = w;
}

static void scsc_shm_h4_read_iq_report_evt_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct BSMHCP_TD_HCI_EVT testhcitd;
	struct BSMHCP_TD_IQ_REPORTING_EVT testtd;
	char buf[1024], *p;
	ssize_t expect_len;

	// Prepare
	reset_mailbox();
	reset_read_information();
	reset_connection_information();
	memset(&testhcitd, 0, sizeof(struct BSMHCP_TD_HCI_EVT));
	testhcitd.event_type = BSMHCP_EVENT_TYPE_IQ_REPORT_ENABLED;
	testhcitd.data[1] = 0;
	testhcitd.length = HCI_EVENT_HEADER_LENGTH;
	write_hci_evt(&testhcitd);
	expect_len = H4_HEADER_SIZE + testhcitd.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));

	memset(&testtd, 0, sizeof(struct BSMHCP_TD_IQ_REPORTING_EVT));
	testtd.sync_handle = 1;
	testtd.sample_count = 2;

#if 1
	kunit_info(test, "read twice\n");
	testtd.subevent_code = HCI_LE_CONNECTIONLESS_IQ_REPORT_EVENT_SUB_CODE;
	write_iq_report(&testtd);
	p = buf;
	expect_len = H4_HEADER_SIZE + HCI_EVENT_HEADER_LENGTH;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
	p += expect_len;
	expect_len = buf[2];
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));

	kunit_info(test, "read onece\n");
	testtd.subevent_code = HCI_LE_CONNECTION_IQ_REPORT_EVENT_SUB_CODE;
	write_iq_report(&testtd);
	expect_len += H4_HEADER_SIZE + HCI_EVENT_HEADER_LENGTH;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, p, expect_len, 0));
#endif

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_h4_read_queue_sync_helper_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct BSMHCP_TD_HCI_EVT testtd;
	char buf[10];
	ssize_t expect_len;

	// Prepare
	reset_mailbox();
	reset_read_information();
	reset_connection_information();
	memset(&testtd, 0, sizeof(struct BSMHCP_TD_HCI_EVT));
	testtd.length = HCI_EVENT_HEADER_LENGTH + testtd.data[1];
	expect_len = 0;
#if 1
	kunit_info(test, "test queue helper\n");
	bt_svc->hci_event_paused = true;
	bt_svc->data_paused = true;
	testtd.event_type = BSMHCP_EVENT_TYPE_NONE;
	write_hci_evt(&testtd);
	testtd.event_type = BSMHCP_EVENT_TYPE_CONNECTED;
	bt_svc->bsmhcp_protocol->acl_rx_transfer_ring[0].header.hci_connection_handle = 1;
	testtd.hci_connection_handle = 1;
	write_hci_evt(&testtd);
	expect_len = 1;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
	expect_len = testtd.length;
	KUNIT_EXPECT_EQ(test, expect_len, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
#endif
#if 1
	kunit_info(test, "test queue helper - not found\n");
	reset_mailbox();
	bt_svc->hci_event_paused = true;
	bt_svc->data_paused = true;
	testtd.event_type = BSMHCP_EVENT_TYPE_NONE;
	testtd.hci_connection_handle = 0;
	write_hci_evt(&testtd);
	expect_len = 1 + testtd.length;
	test_helper_wait_init(test);
	test_helper_wake_up_after_ms(&bt_svc->read_wait, 10, set_error);
	KUNIT_EXPECT_EQ(test, (ssize_t)-EAGAIN, scsc_bt_shm_h4_read(file, buf, expect_len, 0));
	atomic_set(&bt_svc->error_count, 0);
	test_helper_wait_deinit();
#endif
	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_h4_write_hci_cmd_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	char testdata[] = { HCI_COMMAND_PKT, 0x00, 0x00, 0x01, 0xab };

	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, true);
	KUNIT_EXPECT_EQ(test, (ssize_t)sizeof(testdata), scsc_bt_shm_h4_write(file, testdata, sizeof(testdata), 0));
	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, false);

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_h4_write_acl_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	char testdata[] = { HCI_ACLDATA_PKT, 0x00, 0x00, 0x01, 0x00, 0xab };

	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, true);
	KUNIT_EXPECT_EQ(test, (ssize_t)sizeof(testdata), scsc_bt_shm_h4_write(file, testdata, sizeof(testdata), 0));
	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, false);

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_h4_write_iso_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	char testdata[] = { HCI_ISODATA_PKT,
			0x00, /* CONN HANDLE */
			0x00, /* CONN HANDLE, FLAGS */
			0x06, 0x00, /* Length */
			0x00, 0x00, /* Seq number */
			0x06, 0x00, /* SDU length */
			0xab, 0xcd, };
	char testdata2[] = { HCI_ISODATA_PKT,
			0x00, /* CONN HANDLE */
			BSMHCP_ISO_PB_FLAG_CONTINUE, /* CONN HANDLE, FLAGS */
			0x02, 0x00, /* Length */
			0xef, 0xab };
	char testdata3[] = { HCI_ISODATA_PKT,
			0x00, /* CONN HANDLE */
			BSMHCP_ISO_PB_FLAG_LAST, /* CONN HANDLE, FLAGS */
			0x02, 0x00, /* Length */
			0xcd, 0xef };
	char testdata4[] = { HCI_ISODATA_PKT,
			0x00, /* CONN HANDLE */
			BSMHCP_ISO_PB_FLAG_COMPLETE, /* CONN HANDLE, FLAGS */
			0x06, 0x00, /* Length */
			0x00, 0x00, /* Seq number */
			0x02, 0x00, /* SDU length */
			0xab, 0xcd, };

	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, true);
	kunit_info(test, "First packet");
	KUNIT_EXPECT_EQ(test, (ssize_t)sizeof(testdata), scsc_bt_shm_h4_write(file, testdata, sizeof(testdata), 0));

	kunit_info(test, "Continue packet");
	KUNIT_EXPECT_EQ(test, (ssize_t)sizeof(testdata2), scsc_bt_shm_h4_write(file, testdata2, sizeof(testdata2), 0));
	kunit_info(test, "Last packet");
	KUNIT_EXPECT_EQ(test, (ssize_t)sizeof(testdata3), scsc_bt_shm_h4_write(file, testdata3, sizeof(testdata3), 0));
	kunit_info(test, "Complate packet");
	KUNIT_EXPECT_EQ(test, (ssize_t)sizeof(testdata4), scsc_bt_shm_h4_write(file, testdata4, sizeof(testdata4), 0));
	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, false);

	kunit_kfree(test, file);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_h4_avdtp_detect_write_test(struct kunit *test)
{
	reset_mailbox();
	reset_read_information();
	reset_connection_information();
	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, true);

	scsc_bt_shm_h4_avdtp_detect_write(0, 0, 0);

	test_helper_write_wake_lock_dummy(&bt_svc->write_wake_lock.ws, false);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_wakelock_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

static void scsc_shm_fw_log_simple_call_test(struct kunit *test)
{
	const char *data = "1234567890";
	char *buf = kunit_kmalloc(test, 64, GFP_USER);

	kunit_info(test, "fw_log_circ_buf_push(0) / fw_log_circ_buf_pop();");
	KUNIT_EXPECT_TRUE(test, fw_log_circ_buf_push(0x00));
	KUNIT_EXPECT_EQ(test, 0x00, fw_log_circ_buf_pop());

	kunit_info(test, "fw_log_circ_buf_remove(0);");
	KUNIT_EXPECT_TRUE(test, fw_log_circ_buf_push(0x00));
	KUNIT_EXPECT_EQ(test, 1, fw_log_circ_buf_remove(1));

	/*--------------------------------------------------
	* // TODO: add a argument checking
	* kunit_info(test, "scsc_bt_fw_log_circ_buf_write(0, NULL, 0);");
	* scsc_bt_fw_log_circ_buf_write(0, NULL, 0);
	* kunit_info(test, "scsc_bt_fw_log_circ_buf_read(NULL, 0);");
	* scsc_bt_fw_log_circ_buf_read(NULL, 0);
	*--------------------------------------------------*/
	kunit_info(test, "scsc_bt_fw_log_circ_buf_write(0, data, FW_LOG_HEADER_SIZE+1);");
	KUNIT_EXPECT_EQ(test,
			FW_LOG_HEADER_SIZE+1,
			scsc_bt_fw_log_circ_buf_write(0, data, FW_LOG_HEADER_SIZE+1)
			);

	kunit_info(test, "negative scsc_bt_fw_log_circ_buf_write(0, data, bt_svc->fw_log.circ_buf_size+1);");
	KUNIT_EXPECT_EQ(test,
			0,
			scsc_bt_fw_log_circ_buf_write(0, data, bt_svc->fw_log.circ_buf_size+1)
			);

	kunit_info(test, "scsc_bt_fw_log_circ_buf_read(buf, FW_LOG_HEADER_SIZE+1);");
	KUNIT_EXPECT_EQ(test,
			1,
			scsc_bt_fw_log_circ_buf_read(buf, FW_LOG_HEADER_SIZE+1)
			);

#if IS_ENABLED(CONFIG_SCSC_MXLOGGER)
	kunit_info(test, "scsc_bt_fw_log_update_filter(?, 0, 0, 0, 0);");
	scsc_bt_fw_log_update_filter(1, 0, 0, 0, 0);
	KUNIT_EXPECT_TRUE(test, bt_svc->fw_log.observers_registered);
	scsc_bt_fw_log_update_filter(0, 0, 0, 0, 0);
	KUNIT_EXPECT_FALSE(test, bt_svc->fw_log.observers_registered);
#endif

	kunit_info(test, "csc_bt_fw_log_state_handler();");
	scsc_bt_fw_log_state_handler();

	kunit_info(test, "csc_bt_big_reset_connection_handles(0);");
	scsc_bt_big_reset_connection_handles(0);

	kunit_info(test, "csc_hci_fw_log_read(NULL, 0);");
	KUNIT_EXPECT_EQ(test, 0, scsc_hci_fw_log_read(NULL, 0));

	kunit_kfree(test, buf);
	KUNIT_SUCCEED(test);
}

static void scsc_shm_etc_call_test(struct kunit *test)
{
	scsc_bt_clear_paused_acl_rx(0);
	KUNIT_SUCCEED(test);
}

static struct kunit_case scsc_shm_test_cases[] = {
	KUNIT_CASE(scsc_shm_test_prepare),

	KUNIT_CASE(scsc_shm_read_data_available_test),
	KUNIT_CASE(scsc_bt_shm_irq_handler_test),
	KUNIT_CASE(scsc_shm_h4_poll_test),
	KUNIT_CASE( scsc_shm_h4_read_test),
	KUNIT_CASE(scsc_shm_h4_read_hci_evt_test),
	KUNIT_CASE(scsc_shm_h4_read_acl_data_test),
	KUNIT_CASE(scsc_shm_h4_read_iso_data_test),
	KUNIT_CASE(scsc_shm_h4_read_credit_test),
	KUNIT_CASE(scsc_shm_h4_read_iq_report_evt_test),
	KUNIT_CASE(scsc_shm_h4_read_queue_sync_helper_test),
	KUNIT_CASE(scsc_shm_h4_write_hci_cmd_test),
	KUNIT_CASE(scsc_shm_h4_write_acl_test),
	KUNIT_CASE(scsc_shm_h4_write_iso_test),
	KUNIT_CASE(scsc_shm_h4_avdtp_detect_write_test),

	KUNIT_CASE(scsc_shm_wakelock_test),
	KUNIT_CASE(scsc_shm_fw_log_simple_call_test),

	KUNIT_CASE(scsc_shm_etc_call_test),

	KUNIT_CASE(scsc_shm_test_end),
	{}
};

static struct kunit_suite scsc_shm_test_suite = {
	.name = "scsc_shm_unittest",
	.test_cases = scsc_shm_test_cases,
	.init = scsc_shm_test_init,
	.exit = scsc_shm_test_exit,
};

kunit_test_suite(scsc_shm_test_suite);
