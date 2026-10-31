#include <kunit/test.h>
#include "../mxman.h"
#include "../mxlogger.h"

extern int (*fp_mxlogger_force_to_host_set_param_cb)(const char *val, const struct kernel_param *kp);
extern int (*fp_mxlogger_force_to_host_get_param_cb)(char *buffer, const struct kernel_param *kp);
extern void (*fp_mxlogger_message_handler)(const void *message, void *data);
extern void (*fp_mxlogger_message_handler_wpan)(const void *message, void *data);
extern void (*fp_mxlogger_wait_for_msg_reinit_completion)(struct mxlogger_channel *chan);
extern bool (*fp_mxlogger_wait_for_msg_reply)(struct mxlogger_channel *chan);
extern int (*fp_mxlogger_collect_init)(struct scsc_log_collector_client *collect_client);
extern int (*fp_mxlogger_collect_init_wpan)(struct scsc_log_collector_client *collect_client);
extern int (*fp_mxlogger_collect)(struct scsc_log_collector_client *collect_client, size_t size);
extern int (*fp_mxlogger_collect_wpan)(struct scsc_log_collector_client *collect_client, size_t size);
extern int (*fp_mxlogger_collect_end)(struct scsc_log_collector_client *collect_client);
extern int (*fp_mxlogger_collect_end_wpan)(struct scsc_log_collector_client *collect_client);
extern void (*fp_mxlogger_enable_channel)(struct mxlogger *mxlogger, bool enable, u8 channel);
extern int (*fp_mxlogger_buffer_to_index)(enum scsc_log_chunk_type fw_buffer);

struct mxlogger *k_mxlogger;
struct mxlogger_channel *k_mxlogger_channel;
struct log_msg_packet *k_log_msg_packet;
struct scsc_mx *mx;
struct scsc_log_collector_client collect_client;

static void mxlogger_testall(struct kunit *test)
{
	int i;

	set_test_in_mxlogger(test);

	k_mxlogger = test_alloc_mxlogger(test);

	k_mxlogger_channel = test_alloc_mxlogger_channel(test);
	k_mxlogger_channel->mxlogger = k_mxlogger;
	init_completion(&k_mxlogger_channel->rings_serialized_ops);

	k_log_msg_packet = test_alloc_log_msg_packet(test);
	k_log_msg_packet->msg = MM_MXLOGGER_INITIALIZED_EVT;
	fp_mxlogger_message_handler(k_log_msg_packet, k_mxlogger_channel);
	k_log_msg_packet->msg = MM_MXLOGGER_STARTED_EVT;
	fp_mxlogger_message_handler(k_log_msg_packet, k_mxlogger_channel);
	k_log_msg_packet->msg = MM_MXLOGGER_STOPPED_EVT;
	fp_mxlogger_message_handler(k_log_msg_packet, k_mxlogger_channel);
	//k_log_msg_packet->msg = MM_MXLOGGER_COLLECTION_FW_REQ_EVT;
	//k_log_msg_packet->arg = 0x00;
	//fp_mxlogger_message_handler(k_log_msg_packet, k_mxlogger_channel);
	k_log_msg_packet->msg = -1;
	fp_mxlogger_message_handler(k_log_msg_packet, k_mxlogger_channel);

	k_log_msg_packet->msg = -1;
	fp_mxlogger_message_handler_wpan(k_log_msg_packet, k_mxlogger_channel);

	fp_mxlogger_wait_for_msg_reinit_completion(k_mxlogger_channel);
	fp_mxlogger_wait_for_msg_reply(k_mxlogger_channel);

	fp_mxlogger_enable_channel(k_mxlogger, 0, 0);

	mxlogger_print_mapping(k_mxlogger->chan[MXLOGGER_CHANNEL_WLAN].cfg);

	mx = test_alloc_scscmx(test, get_mif());

	mxlogger_init(mx, k_mxlogger, MX_DRAM_SIZE_SECTION_LOG);

	char val[2] = "Y";

	fp_mxlogger_force_to_host_set_param_cb(val, NULL);
	fp_mxlogger_force_to_host_get_param_cb(val, NULL);

	mxlogger_unregister_global_observer("FAKE_OBSERVER");

	collect_client.prv = k_mxlogger;
	fp_mxlogger_collect_init(&collect_client);
	fp_mxlogger_collect_init_wpan(&collect_client);

	for (i = SCSC_LOG_CHUNK_SYNC; i <= SCSC_LOG_CHUNK_INVALID; i++) {
		collect_client.type = i;
		fp_mxlogger_collect(&collect_client, 0x4);
		fp_mxlogger_collect_wpan(&collect_client, 0x4);
	}
	fp_mxlogger_collect_end(&collect_client);
	fp_mxlogger_collect_end_wpan(&collect_client);

	mxlogger_init_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_init_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mxlogger_start_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_start_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mxlogger_generate_sync_record(k_mxlogger, MXLOGGER_SYN_LOGCOLLECTION);

	mxlogger_init_transport_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_init_transport_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mxlogger_get_channel_ref(k_mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_get_channel_ref(k_mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mxlogger_get_channel_len(k_mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_get_channel_len(k_mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mxlogger_stop_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_stop_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mxlogger_deinit_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WLAN);
	mxlogger_deinit_channel(k_mxlogger, SCSC_MIF_ABS_TARGET_WPAN);

	mxlogger_deinit(k_mxlogger->mx, k_mxlogger);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void mxlogger_mxlogger_enable_channel(struct kunit *test)
{
	struct mxlogger mxlogger;

	mutex_init(&mxlogger.lock);
	mxlogger.configured = true;

	mxlogger.chan[0].configured = true;
	mxlogger.chan[0].enabled = true;

	fp_mxlogger_enable_channel(&mxlogger, true, MXLOGGER_CHANNEL_WLAN);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void mxlogger_mxlogger_generate_sync_record(struct kunit *test)
{
	int MAX_RECORD = 10;
	int sync_buffer_index = 2;
	int sync_buffer_index_wpan = 4;
	struct mxlogger mxlogger;
	struct mxlogger_sync_record sync_records_in_buffer[MAX_RECORD];

	mutex_init(&mxlogger.lock);
	mxlogger.configured = true;

	mxlogger.chan[0].mem_sync_buf = &sync_records_in_buffer[0];
	mxlogger.chan[0].sync_buffer_index = sync_buffer_index;
	mxlogger.chan[0].enabled = true;
	mxlogger.chan[0].target = SCSC_MIF_ABS_TARGET_WLAN;

	mxlogger.chan[1].mem_sync_buf = &sync_records_in_buffer[0];
	mxlogger.chan[1].sync_buffer_index = sync_buffer_index_wpan;
	mxlogger.chan[1].enabled = true;
	mxlogger.chan[1].target = SCSC_MIF_ABS_TARGET_WPAN;

	mxlogger_generate_sync_record(&mxlogger, MXLOGGER_SYN_LOGCOLLECTION);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void mxlogger_mxlogger_register_global_observer_class(struct kunit *test)
{
	int res;

	res = mxlogger_register_global_observer_class("FW_LOG", 0x01);

	res = mxlogger_unregister_global_observer_class("FW_LOG", 0x01);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void mxlogger_mxlogger_buffer_to_index(struct kunit *test)
{
	fp_mxlogger_buffer_to_index(SCSC_LOG_CHUNK_SYNC);
	fp_mxlogger_buffer_to_index(SCSC_LOG_CHUNK_IMP);
	fp_mxlogger_buffer_to_index(SCSC_LOG_CHUNK_IMPD12);
	fp_mxlogger_buffer_to_index(SCSC_LOG_CHUNK_LINK);
	fp_mxlogger_buffer_to_index(SCSC_LOG_RESERVED_COMMON);
	fp_mxlogger_buffer_to_index(SCSC_LOG_RESERVED_BT);
	fp_mxlogger_buffer_to_index(SCSC_LOG_RESERVED_WLAN);
	fp_mxlogger_buffer_to_index(SCSC_LOG_RESERVED_RADIO);
	fp_mxlogger_buffer_to_index(SCSC_LOG_CHUNK_MXL);
	fp_mxlogger_buffer_to_index(SCSC_LOG_CHUNK_UDI);
	fp_mxlogger_buffer_to_index(SCSC_LOG_CHUNK_INVALID);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void mxlogger_mxlogger_get_fw_buf_size(struct kunit *test)
{
	size_t res;
	struct mxlogger mxlogger;
	struct mxlogger_config_area	cfg;

	mutex_init(&mxlogger.lock);
	res = mxlogger_get_fw_buf_size(&mxlogger, SCSC_LOG_CHUNK_SYNC, SCSC_MIF_ABS_TARGET_WLAN);

	mxlogger.initialized = true;
	mxlogger.chan[MXLOGGER_CHANNEL_WLAN].cfg = &cfg;
	res = mxlogger_get_fw_buf_size(&mxlogger, SCSC_LOG_CHUNK_SYNC, SCSC_MIF_ABS_TARGET_WLAN);
	res = mxlogger_get_fw_buf_size(&mxlogger, SCSC_LOG_CHUNK_INVALID, SCSC_MIF_ABS_TARGET_WLAN);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static void mxlogger_mxlogger_dump_fw_buf(struct kunit *test)
{
	size_t res;
	struct scsc_mx *mx;
	struct mxlogger mxlogger;
	struct mxlogger_config_area	cfg;

	int size = 10;
	int chunk_sync_size_in_fw = 100;

	mx = test_alloc_scscmx(test, get_mif());
	mxlogger.mx = mx;

	mutex_init(&mxlogger.lock);
	u32 *buf = kunit_kzalloc(test, size * sizeof(u32), GFP_KERNEL);

	res = mxlogger_dump_fw_buf(&mxlogger, SCSC_LOG_CHUNK_SYNC, buf, size, SCSC_MIF_ABS_TARGET_WLAN);

	mxlogger.initialized = true;
	mxlogger.chan[MXLOGGER_CHANNEL_WLAN].cfg = &cfg;
	cfg.bfds[SCSC_LOG_CHUNK_SYNC].location = -1;
	cfg.bfds[SCSC_LOG_CHUNK_SYNC].size = chunk_sync_size_in_fw;
	set_visible_sharedmem(test, true, chunk_sync_size_in_fw);
	res = mxlogger_dump_fw_buf(&mxlogger, SCSC_LOG_CHUNK_SYNC, buf, size, SCSC_MIF_ABS_TARGET_WLAN);

	KUNIT_EXPECT_STREQ(test, "OK", "OK");
}

static int test_init(struct kunit *test)
{
	return 0;
}

static void test_exit(struct kunit *test)
{
}

static struct kunit_case test_cases[] = {
	KUNIT_CASE(mxlogger_testall),
	KUNIT_CASE(mxlogger_mxlogger_enable_channel),
	KUNIT_CASE(mxlogger_mxlogger_generate_sync_record),
	KUNIT_CASE(mxlogger_mxlogger_register_global_observer_class),
	KUNIT_CASE(mxlogger_mxlogger_buffer_to_index),
	KUNIT_CASE(mxlogger_mxlogger_get_fw_buf_size),
	KUNIT_CASE(mxlogger_mxlogger_dump_fw_buf),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mxlogger_split",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("youknow.choi@samsung.com");


