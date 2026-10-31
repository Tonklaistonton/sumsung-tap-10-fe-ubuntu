/******************************************************************************
 *                                                                            *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth Driver Unittest for KUNIT                                        *
 *                                                                            *
 ******************************************************************************/
#include <kunit/test.h>

/******************************************************************************
 * Test target code
 ******************************************************************************/
#include "../scsc_avdtp_detect.c"

static void scsc_avdtp_detect_rxtx_test(struct kunit *test)
{
	static struct scsc_bt_avdtp_detect_hci_connection *avdtp_hci;
	char testdata[AVDTP_DETECT_MIN_DATA_LENGTH_CON_RSP] = { 0x00, };

	// prepare
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);

	kunit_info(test, "single cid\n");
	testdata[2] = L2CAP_SIGNALING_CID & 0xff;

	kunit_info(test, "  connect req\n");
	testdata[4] = L2CAP_CODE_CONNECT_REQ;
	testdata[8] = L2CAP_AVDTP_PSM & 0xff;	// Dest CID
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);

	kunit_info(test, "  connect res\n");
	testdata[4] = L2CAP_CODE_CONNECT_RSP;
	scsc_avdtp_detect_rxtx(0, testdata, AVDTP_DETECT_MIN_DATA_LENGTH, true);
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);
	testdata[10] = 1; // Source CID
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);
	testdata[12] = HCI_L2CAP_CON_RSP_RESULT_REFUSED;
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);

	kunit_info(test, "  disconnect req\n");
	testdata[4] = L2CAP_CODE_DISCONNECT_REQ;
	testdata[8] = 1;
	testdata[10] = 2;
	kunit_info(test, "    crossing_signal == true\n");
	kunit_info(test, "      signal\n");
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->crossing_signal_conns = true;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	avdtp_hci->ongoing.incoming_signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->crossing_signal_conns = true;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	avdtp_hci->ongoing.incoming_signal.state = BT_AVDTP_STATE_IDLE_SIGNALING;
	avdtp_hci->ongoing.outgoing_signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);

	kunit_info(test, "      incoming_signal\n");
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->crossing_signal_conns = true;
	avdtp_hci->ongoing.incoming_signal.dst_cid = testdata[8];
	avdtp_hci->ongoing.incoming_signal.src_cid = testdata[10];
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);

	kunit_info(test, "      outgoing_signal\n");
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->crossing_signal_conns = true;
	avdtp_hci->ongoing.outgoing_signal.dst_cid = testdata[8];
	avdtp_hci->ongoing.outgoing_signal.src_cid = testdata[10];
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);

	kunit_info(test, "    crossing_signal == false\n");
	kunit_info(test, "      signal\n");
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->crossing_signal_conns = false;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_IDLE_SIGNALING;

	kunit_info(test, "      stream\n");
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->crossing_signal_conns = false;
	avdtp_hci->stream.dst_cid = testdata[8];
	avdtp_hci->stream.src_cid = testdata[10];
	avdtp_hci->stream.state = BT_AVDTP_STATE_COMPLETE_STREAMING;
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);

	kunit_info(test, "  short length\n");
	scsc_avdtp_detect_rxtx(0, testdata, AVDTP_DETECT_MIN_DATA_LENGTH-1, true);

	kunit_info(test, "not single cid\n");
	testdata[2] = 2;
	testdata[3] = 0;
	kunit_info(test, "  message type rsp accept\n");
	testdata[4] = AVDTP_MESSAGE_TYPE_RSP_ACCEPT;  // AVDTP_MESSAGE_TYPE_OFFSET
	testdata[8] = 2;
	testdata[10] = 2;
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];

	testdata[5] = AVDTP_SIGNAL_ID_START;  //AVDTP_SIGNAL_ID_OFFSET
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	testdata[5] = AVDTP_SIGNAL_ID_OPEN;  //AVDTP_SIGNAL_ID_OFFSET
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), false);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	testdata[5] = AVDTP_SIGNAL_ID_ABORT;  //AVDTP_SIGNAL_ID_OFFSET
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), false);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	testdata[5] = AVDTP_SIGNAL_ID_DISCOVER;  //AVDTP_SIGNAL_ID_OFFSET
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), false);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	testdata[5] = AVDTP_SIGNAL_ID_SET_CONF;  //AVDTP_SIGNAL_ID_OFFSET
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), false);

	kunit_info(test, "  message type rsp accept\n");
	testdata[4] = AVDTP_MESSAGE_TYPE_CMD;  // AVDTP_MESSAGE_TYPE_OFFSET
	testdata[5] = AVDTP_SIGNAL_ID_SET_CONF;  //AVDTP_SIGNAL_ID_OFFSET
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), false);
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	testdata[5] = AVDTP_SIGNAL_ID_ABORT;  //AVDTP_SIGNAL_ID_OFFSET
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);

	kunit_info(test, "  message type rsp reset\n");
	testdata[4] = AVDTP_MESSAGE_TYPE_RSP_REJECT;  // AVDTP_MESSAGE_TYPE_OFFSET
	testdata[5] = AVDTP_SIGNAL_ID_SET_CONF;  //AVDTP_SIGNAL_ID_OFFSET
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	avdtp_hci->tsep_detect.remote_snk_seid_candidate = true;
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), true);
	avdtp_hci = scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	avdtp_hci->signal.state = BT_AVDTP_STATE_COMPLETE_SIGNALING;
	avdtp_hci->signal.dst_cid = testdata[8];
	avdtp_hci->signal.src_cid = testdata[10];
	avdtp_hci->tsep_detect.remote_snk_seid_candidate = true;
	scsc_avdtp_detect_rxtx(0, testdata, sizeof(testdata), false);

	KUNIT_SUCCEED(test);
	//KUNIT_FAIL(test, "check\n");
}

static void scsc_avdtp_detect_reset_connection_handle_test(struct kunit *test)
{
	scsc_avdtp_detect_find_or_create_hci_connection(0, true);
	scsc_avdtp_detect_find_or_create_hci_connection(1, true);
	scsc_avdtp_detect_reset_connection_handle(0);
	KUNIT_SUCCEED(test);
}

static void scsc_avdtp_detect_connection_handle_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

static void scsc_avdtp_detect_test_init(struct kunit *test)
{
	test_common_scsc_bt_module_init_probe(test);
	test_common_scsc_bt_module_normal_open(test);
}

static void scsc_avdtp_detect_test_exit(struct kunit *test)
{
	test_common_scsc_bt_module_remove_exit(test);
	test_common_scsc_bt_module_normal_close(test);
}

static struct kunit_case scsc_avdtp_detect_test_cases[] = {
	KUNIT_CASE(scsc_avdtp_detect_rxtx_test),
	KUNIT_CASE(scsc_avdtp_detect_reset_connection_handle_test),
	KUNIT_CASE(scsc_avdtp_detect_connection_handle_test),
	{}
};

static struct kunit_suite scsc_avdtp_detect_test_suite = {
	.name = "scsc_avdtp_detect_unittest",
	.test_cases = scsc_avdtp_detect_test_cases,
	.init = scsc_avdtp_detect_test_init,
	.exit = scsc_avdtp_detect_test_exit,
};

kunit_test_suite(scsc_avdtp_detect_test_suite);
