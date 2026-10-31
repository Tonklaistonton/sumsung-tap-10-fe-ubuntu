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
 * Fake tricks
 ******************************************************************************/
#include "fake.h"
#include <linux/firmware.h>
#include <scsc/scsc_mifram.h>

#ifndef UNUSED
#define UNUSED(x)       ((void)(x))
#endif

struct scsc_mx;
struct scsc_service;

struct fake_buffer_pool scsc_mx_fake_buffer_pool;
GLOBAL_FAKE(int, scsc_mx_service_mifram_alloc, struct scsc_service *service, size_t nbytes, scsc_mifram_ref *ref, u32 align)
{
	UNUSED(service);
	UNUSED(nbytes);
	UNUSED(ref);
	UNUSED(align);
	*ref = (uintptr_t)fake_buffer_alloc(&scsc_mx_fake_buffer_pool, nbytes);
	return *ref ? scsc_mx_service_mifram_alloc_ret : -ENOMEM;
}

GLOBAL_FAKE_VOID(scsc_mx_service_mifram_free, struct scsc_service *service, scsc_mifram_ref ref)
{
	UNUSED(service);
	fake_buffer_free((void *)(uintptr_t)ref);
}

static unsigned char fake_fw_data[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0 };
static struct firmware fake_fw = {
	.size = sizeof(fake_fw_data),
	.data = fake_fw_data,
	.priv = NULL,
};
LOCAL_FAKE(int, mx140_file_request_conf, struct scsc_mx *mx, const struct firmware **conf, const char *config_path, const char *filename)
{
	*conf = &fake_fw;
	return mx140_file_request_conf_ret;
}

LOCAL_FAKE_RET(int, scsc_mx_service_start, struct scsc_service *service, scsc_mifram_ref ref)

int scsc_mx_service_close_fail_count = 0;
LOCAL_FAKE(int, scsc_mx_service_close, struct scsc_service *service)
{
	UNUSED(service);
	if (scsc_mx_service_close_fail_count > 0) {
		scsc_mx_service_close_fail_count--;
		return -EBUSY;
	}
	return scsc_mx_service_close_ret;
}

LOCAL_FAKE_RET(bool, mxman_recovery_disabled, void)

#include <scsc/api/bt_audio.h>
LOCAL_FAKE_RET(struct scsc_bt_audio_abox *, scsc_mx_service_get_bt_audio_abox, struct scsc_service *service)

static int scsc_mx_service_mif_addr_to_ptr_error_after_call_times = 0;
GLOBAL_FAKE(void *, scsc_mx_service_mif_addr_to_ptr, struct scsc_service *service, scsc_mifram_ref ref)
{
	UNUSED(service);
	pr_info("called");
	if (scsc_mx_service_mif_addr_to_ptr_error_after_call_times > 0) {
		pr_info("it will return NULL after %d", scsc_mx_service_mif_addr_to_ptr_error_after_call_times);
		if (--scsc_mx_service_mif_addr_to_ptr_error_after_call_times == 0)
			return scsc_mx_service_mif_addr_to_ptr_ret;
	}
	return (void *)(uintptr_t)ref;
}

LOCAL_FAKE(int, scsc_mx_service_mif_ptr_to_addr, struct scsc_service *service, void *mem_ptr, scsc_mifram_ref *ref)
{
	UNUSED(service);
	*ref = (uintptr_t)mem_ptr;
	return scsc_mx_service_mif_ptr_to_addr_ret;
}

LOCAL_FAKE_RET(int, scsc_mx_service_stop, struct scsc_service *service)

static unsigned int fake_mx;
static int fake_addr;	/* Use only where data is not used */

/******************************************************************************
 * Test target code
 ******************************************************************************/
#include "../scsc_bt_module.c"

/******************************************************************************
 * Common test code
 ******************************************************************************/
static char buffer_pool[120000];
extern struct scsc_service *scsc_mx_service_open_boot_data_ret;
extern int scsc_mx_service_close_fail_count;
void test_common_scsc_bt_module_normal_open(struct kunit *test)
{
	kunit_info(test, "scsc_bt_module_open() - service start\n");

	/* Prepare */
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, sizeof(buffer_pool));
	mx140_file_request_conf_ret = 0;
	scsc_mx_service_open_boot_data_ret = (struct scsc_service *)&fake_addr;

	KUNIT_EXPECT_EQ(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
}

void test_common_scsc_bt_module_normal_close(struct kunit *test)
{
	/* wake_lock_active() aka scsc_wake_lock_active() makes Segfault
	 * error. Here is trick code. */
	struct wakeup_source ws;

	ws.active = 0;
	if (bt_svc->write_wake_lock.ws == NULL)
		bt_svc->write_wake_lock.ws = &ws;
	KUNIT_ASSERT_EQ(test, 0, scsc_bt_shm_fops.release(NULL, NULL));
	if (bt_svc->write_wake_lock.ws == &ws)
		bt_svc->write_wake_lock.ws = NULL;
}

static bool module_init_state = true;
int test_common_scsc_bt_module_init_probe(struct kunit *test)
{
	if (!module_init_state)
		scsc_bt_module_init();
	module_init_state = true;

	bt_driver.probe(&bt_driver, (struct scsc_mx *)&fake_mx, 0);
	return 0;
}

void test_common_scsc_bt_module_remove_exit(struct kunit *test)
{
	bt_driver.remove(&bt_driver, (struct scsc_mx *)&fake_mx, 0);
	if (module_init_state) {
		scsc_bt_module_exit();
	}
	module_init_state = false;
}

/******************************************************************************
 * Test cases
 ******************************************************************************/
static void scsc_bt_module_init_exit_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

static void scsc_bt_module_probe_remove_negavie_test(struct kunit *test)
{
	/* remove. It called prive() in init() */
	bt_driver.remove(&bt_driver, (struct scsc_mx *)&fake_mx, 0);

	kunit_info(test, "probe with recovery reason\n");
	bt_driver.probe(&bt_driver, (struct scsc_mx *)&fake_mx, SCSC_MODULE_CLIENT_REASON_RECOVERY_WLAN);
	bt_driver.probe(&bt_driver, (struct scsc_mx *)&fake_mx, SCSC_MODULE_CLIENT_REASON_RECOVERY);
	bt_svc->recovery_level = 1;
	bt_driver.probe(&bt_driver, (struct scsc_mx *)&fake_mx, SCSC_MODULE_CLIENT_REASON_RECOVERY);
	bt_svc->recovery_level = 0;

	kunit_info(test, "remove with recovery reason\n");
	bt_driver.probe(&bt_driver, (struct scsc_mx *)&fake_mx, 0);
	bt_driver.remove(&bt_driver, (struct scsc_mx *)&fake_mx, SCSC_MODULE_CLIENT_REASON_RECOVERY_WLAN);
	bt_driver.remove(&bt_driver, (struct scsc_mx *)&fake_mx, SCSC_MODULE_CLIENT_REASON_RECOVERY);
	bt_svc->recovery_level = 1;
	bt_driver.remove(&bt_driver, (struct scsc_mx *)&fake_mx, SCSC_MODULE_CLIENT_REASON_RECOVERY);
	//bt_svc->service_started = true;
	//bt_driver.remove(&bt_driver, (struct scsc_mx *)&fake_mx, SCSC_MODULE_CLIENT_REASON_RECOVERY);
	KUNIT_SUCCEED(test);
}

static void scsc_bt_shm_open_release_positive_test(struct kunit *test)
{
	struct wakeup_source ws;
	ws.active = 0;

	kunit_info(test, "open test\n");
	/* Prepare */
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, sizeof(buffer_pool));
	mx140_file_request_conf_ret = 0;
	bluetooth_address = 0xabcdef0102l;
	scsc_mx_service_open_boot_data_ret = (struct scsc_service *)&fake_addr;

	/* Do test */
	KUNIT_EXPECT_EQ(test, 0, scsc_bt_shm_fops.open(NULL, NULL));

	kunit_info(test, "release test\n");
	/* wake_lock_active() aka scsc_wake_lock_active() makes Segfault
	 * error. Here is trick code. */
	if (bt_svc->write_wake_lock.ws == NULL)
		bt_svc->write_wake_lock.ws = &ws;

	/* Do test */
#if 1
	/* Do not retry for test speed */
	scsc_mx_service_close_fail_count = 1;
#else
	/* It takes a long time */
	scsc_mx_service_close_fail_count = SLSI_BT_SERVICE_CLOSE_RETRY;
#endif
	KUNIT_ASSERT_EQ(test, 0, scsc_bt_shm_fops.release(NULL, NULL));

	if (bt_svc->write_wake_lock.ws == &ws)
		bt_svc->write_wake_lock.ws = NULL;
	KUNIT_SUCCEED(test);
}

static int fake_audio_for_build;
static int fake_dev_iommu_map(struct device *d, phys_addr_t a, size_t s)
{
	fake_audio_for_build++;
	return 0;
}
static void fake_dev_iommu_unmap(struct device *d, size_t s)
{
	fake_audio_for_build++;
}

static struct device fake_audio_dev;
static struct scsc_bt_audio_abox fake_audio_abox;
static void scsc_bt_shm_open_release_with_audio_dev_test(struct kunit *test)
{
	scsc_mx_service_get_bt_audio_abox_ret = &fake_audio_abox;

	kunit_info(test, "open/release test with audio - positive\n");
	scsc_bt_audio_register(&fake_audio_dev, fake_dev_iommu_map, fake_dev_iommu_unmap);
	scsc_bt_shm_open_release_positive_test(test);
	scsc_bt_audio_unregister(&fake_audio_dev);

	kunit_info(test, "open/release test with audio - negative\n");
	scsc_bt_audio_register(&fake_audio_dev, NULL, NULL);
	scsc_bt_shm_open_release_positive_test(test);
	scsc_bt_audio_unregister(&fake_audio_dev);
}

static void scsc_bt_shm_open_negative(struct kunit *test)
{
	struct wakeup_source ws;

	ws.active = 0;
	scsc_mx_service_get_bt_audio_abox_ret = &fake_audio_abox;

	kunit_info(test, "open negative test\n");

	/* Prepare */
	mx140_file_request_conf_ret = 0;
	scsc_mx_service_open_boot_data_ret = (struct scsc_service *)&fake_addr;
	if (bt_svc->write_wake_lock.ws == NULL)
		bt_svc->write_wake_lock.ws = &ws;

	/* Do test */
	kunit_info(test, "scsc_mx_service_start() fail test\n");
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, sizeof(buffer_pool));
	scsc_mx_service_start_ret = -EFAULT;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
	scsc_mx_service_start_ret = 0;

	kunit_info(test, "scsc_bt_shm_init() fail test\n");
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, sizeof(buffer_pool));
	scsc_mx_service_mif_addr_to_ptr_error_after_call_times = 2;
	scsc_mx_service_mif_addr_to_ptr_ret = NULL;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));

	kunit_info(test, "scsc_mx_service_start() for audio fail\n");
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, sizeof(buffer_pool));
	scsc_bt_audio_register(&fake_audio_dev, fake_dev_iommu_map, fake_dev_iommu_unmap);
	scsc_mx_service_mif_ptr_to_addr_ret = -EFAULT;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
	scsc_mx_service_mif_ptr_to_addr_ret = 0;
	scsc_bt_audio_unregister(&fake_audio_dev);

	kunit_info(test, "scsc_mx_service_mif_addr_to_ptr() bhcd_start fail\n");
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, sizeof(buffer_pool));
	scsc_mx_service_mif_addr_to_ptr_error_after_call_times = 1;
	scsc_mx_service_mif_addr_to_ptr_ret = NULL;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));

	kunit_info(test, "scsc_mx_service_mifram_alloc() for bsmhcp fail test\n");
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, 1000);
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));

	kunit_info(test, "scsc_mx_service_mifram_alloc() for start data fail test\n");
	fake_buffer_init(&scsc_mx_fake_buffer_pool, buffer_pool, 10);
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));

	kunit_info(test, "scsc_mx_service_open_boot_data() fail test\n");
	scsc_mx_service_open_boot_data_ret = NULL;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
	scsc_mx_service_open_boot_data_ret = (struct scsc_service *)&fake_addr;

	kunit_info(test, "busy test\n");
	atomic_inc(&bt_svc->service_users);
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
	atomic_inc(&bt_svc->service_users);

	kunit_info(test, "maxwell_creo is not set test\n");
	common_service.maxwell_core = NULL;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
	atomic_inc(&bt_svc->service_users);

	kunit_info(test, "recovery_in_progress() is set test\n");
	bt_svc->recovery_level = 1;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
	bt_svc->recovery_level = 0;

	kunit_info(test, "disable_service is set\n");
	disable_service = 1;
	KUNIT_EXPECT_NE(test, 0, scsc_bt_shm_fops.open(NULL, NULL));
	disable_service = 0;

	if (bt_svc->write_wake_lock.ws == &ws)
		bt_svc->write_wake_lock.ws = NULL;
	KUNIT_SUCCEED(test);
}

static void scsc_bt_shm_release_recovery(struct kunit *test)
{
	struct wakeup_source ws;
	ws.active = 0;

	kunit_info(test, "release test with recovery\n");
	test_common_scsc_bt_module_normal_open(test);

	bt_svc->recovery_level = MX_SYSERR_LEVEL_7;

	recovery_timeout = 10;
	test_common_scsc_bt_module_normal_close(test);

	KUNIT_SUCCEED(test);
	KUNIT_SUCCEED(test);
}

static void scsc_bt_mx_client_interface_test(struct kunit *test)
{
	int i;
	struct mx_syserr_decode err;

	kunit_info(test, "failure functions\n");
	test_helper_wait_init(test);

	err.level = 1;
	KUNIT_EXPECT_EQ(test, err.level, mx_bt_client.failure_notification(NULL, &err));

	bt_svc->recovery_level = 0;
	atomic_set(&bt_svc->error_count, 0);
	KUNIT_EXPECT_FALSE(test, mx_bt_client.stop_on_failure_v2(NULL, &err));
	KUNIT_EXPECT_EQ(test, bt_svc->recovery_level, err.level);
	KUNIT_EXPECT_EQ(test, 1, atomic_read(&bt_svc->error_count));

	test_helper_wait(&bt_svc->read_wait);
	mx_bt_client.failure_reset_v2(NULL, 0, 0);

	bt_svc->last_suspend_interrupt_count = 0;
	bt_svc->interrupt_count = 1;
	KUNIT_EXPECT_EQ(test, 0, mx_bt_client.resume(NULL));
	KUNIT_EXPECT_EQ(test, 0, mx_bt_client.suspend(NULL));
	KUNIT_EXPECT_EQ(test, bt_svc->last_suspend_interrupt_count, bt_svc->interrupt_count);

	/* ETC: For Audio reference */
	bt_audio.abox_virtual = kunit_kmalloc(test, sizeof(struct scsc_bt_audio_abox), GFP_KERNEL);
	bt_svc->abox_ref = (uintptr_t)bt_audio.abox_virtual;
	err.level = 2;
	KUNIT_EXPECT_FALSE(test, mx_bt_client.stop_on_failure_v2(NULL, &err));
	KUNIT_EXPECT_EQ(test, bt_svc->recovery_level, err.level);
	KUNIT_EXPECT_EQ(test, 2, atomic_read(&bt_svc->error_count));
	for (i = 0; i < SCSC_BT_AUDIO_ABOX_DATA_SIZE; i++) {
		KUNIT_EXPECT_TRUE(test, bt_audio.abox_virtual->abox_to_bt_streaming_if_data[i] == 0);
		KUNIT_EXPECT_TRUE(test, bt_audio.abox_virtual->bt_to_abox_streaming_if_data[i] == 0);
	}
	kunit_kfree(test, bt_audio.abox_virtual);
	bt_svc->abox_ref = 0;
	bt_audio.abox_virtual = NULL;

	test_helper_wait_deinit();
	KUNIT_SUCCEED(test);
}

static void scsc_bt_scsc_default_ioctl_test(struct kunit *test)
{
	scsc_bt_shm_fops.unlocked_ioctl(NULL, TCGETS, 0);
	KUNIT_EXPECT_EQ(test, 0l, scsc_default_ioctl(NULL, TCGETS, 0));
	KUNIT_SUCCEED(test);
}

static void scsc_bt_shm_irq_handler_test(struct kunit *test)
{
	test_helper_wait_init(test);
	test_helper_wait(&bt_svc->info_wait);
	scsc_bt_shm_irq_handler(0, NULL);
	test_helper_wait_deinit();
	KUNIT_SUCCEED(test);
}

static void slsi_sm_bt_service_cleanup_stop_service_negative_test(struct kunit *test)
{
	scsc_mx_service_stop_ret = -EPERM;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_bt_service_cleanup_stop_service());
	scsc_mx_service_stop_ret = -EILSEQ;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_bt_service_cleanup_stop_service());
	/* Others */
	scsc_mx_service_stop_ret = -EINVAL;
	KUNIT_EXPECT_EQ(test, scsc_mx_service_stop_ret,
			      slsi_sm_bt_service_cleanup_stop_service());
	scsc_mx_service_stop_ret = 0;
	KUNIT_SUCCEED(test);
}

static void scsc_bt_proc_test(struct kunit *test)
{
	struct file *file = kunit_kmalloc(test, sizeof(struct file), GFP_KERNEL);
	struct seq_file *s;
#define PROC_TEST_BUF_SIZE      (1024*2)
	char *buf = kunit_kmalloc(test, PROC_TEST_BUF_SIZE, GFP_USER);
	struct scsc_bt_avdtp_detect_hci_connection fake_connection;

	kunit_info(test, "Proc open\n");
	KUNIT_EXPECT_EQ(test, 0, scsc_bt_procfs_fops.proc_open(NULL, file));

	// fake seq_file
	s = file->private_data;
	s->count = 0;
	s->size = PROC_TEST_BUF_SIZE;
	s->buf = buf;

	// show without open
	slsi_bt_service_proc_show(s, NULL);

	// show with open
	test_common_scsc_bt_module_normal_open(test);

	fake_connection.next = NULL;
	bt_svc->avdtp_detect.connections = &fake_connection;

	slsi_bt_service_proc_show(s, NULL);
	bt_svc->avdtp_detect.connections = NULL;

	// with FW_INFORMATION
	bt_svc->bsmhcp_protocol->header.firmware_features |= BSMHCP_FEATURE_FW_INFORMATION;
	bt_svc->bsmhcp_protocol->information.user_defined_count = BSMHCP_FW_INFO_USER_DEFINED_COUNT + 1;
	slsi_bt_service_proc_show(s, NULL);

	test_common_scsc_bt_module_normal_close(test);

	kunit_kfree(test, buf);
	kunit_kfree(test, file);

	KUNIT_SUCCEED(test);
}

#define MXLOG_FILTER_TEST_BUF_SIZE      (1024*2)
static void scsc_bt_mxlog_filter_ops(struct kunit *test)
{
	char *buf = kunit_kmalloc(test, MXLOG_FILTER_TEST_BUF_SIZE, GFP_USER);
	char *testdata[2] = {
		"mxlog_filter=0x00000000\n",
		"mxlog_filter=0x12345678\n" };

	test_common_scsc_bt_module_normal_open(test);

	KUNIT_EXPECT_EQ(test, (int)strlen(testdata[0]), scsc_mxlog_filter_ops.get(buf, NULL));
	KUNIT_EXPECT_STREQ(test, testdata[0], buf);

	KUNIT_EXPECT_EQ(test, 0, scsc_mxlog_filter_ops.set("0x12345678\n", NULL));
	KUNIT_EXPECT_EQ(test, (int)strlen(testdata[1]), scsc_mxlog_filter_ops.get(buf, NULL));
	KUNIT_EXPECT_STREQ(test, testdata[1], buf);

	test_common_scsc_bt_module_normal_close(test);

	kunit_kfree(test, buf);

	KUNIT_SUCCEED(test);
}

#define BTLOG_ENABLE_TEST_BUF_SIZE      (1024*2)
static void scsc_bt_btlog_enables_ops(struct kunit *test)
{
	char *buf = kunit_kmalloc(test, BTLOG_ENABLE_TEST_BUF_SIZE, GFP_USER);
	char *testdata[2] = {
		"btlog_enables = 0x00000000000000000000000012345678\n",
		"btlog_enables = 0x12345678abcdef12345678abcdef1234\n" };

	KUNIT_EXPECT_EQ(test, (int)strlen(testdata[0]), scsc_btlog_enables_ops.get(buf, NULL));
	KUNIT_EXPECT_STREQ(test, testdata[0], buf);

	KUNIT_EXPECT_EQ(test, 0, scsc_btlog_enables_ops.set("0x12345678abcdef12345678abcdef1234\n", NULL));
	KUNIT_EXPECT_EQ(test, (int)strlen(testdata[1]), scsc_btlog_enables_ops.get(buf, NULL));
	KUNIT_EXPECT_STREQ(test, testdata[1], buf);

	KUNIT_EXPECT_EQ(test, 0, scsc_btlog_enables_ops.set("+0x12345678\n", NULL));
	KUNIT_EXPECT_EQ(test, (int)strlen(testdata[0]), scsc_btlog_enables_ops.get(buf, NULL));
	KUNIT_EXPECT_STREQ(test, testdata[0], buf);

	KUNIT_EXPECT_NE(test, 0, scsc_btlog_enables_ops.set(NULL, NULL));
	KUNIT_EXPECT_NE(test, 0, scsc_btlog_enables_ops.set("0\n", NULL));
	KUNIT_EXPECT_NE(test, 0, scsc_btlog_enables_ops.set("0x12345678abcdef12345678abcdef12345678\n", NULL));

	kunit_kfree(test, buf);

	KUNIT_SUCCEED(test);
}

static void scsc_bt_force_crash(struct kunit *test)
{
	test_common_scsc_bt_module_normal_open(test);

	KUNIT_EXPECT_EQ(test, 0, scsc_force_crash_ops.set("0xDEADDEAD", NULL));

	test_common_scsc_bt_module_normal_close(test);

	KUNIT_SUCCEED(test);
}

static void scsc_bt_get_audio_info(struct kunit *test)
{
	scsc_bt_audio_register(&fake_audio_dev, fake_dev_iommu_map, fake_dev_iommu_unmap);
	test_common_scsc_bt_module_normal_open(test);

	KUNIT_EXPECT_PTR_EQ(test,
		(void *)bt_audio.abox_physical->bt_to_abox_streaming_if_data,
		(void *)scsc_bt_audio_get_paddr_buf(true));
	KUNIT_EXPECT_PTR_EQ(test,
		(void *)bt_audio.abox_physical->abox_to_bt_streaming_if_data,
		(void *)scsc_bt_audio_get_paddr_buf(false));

	KUNIT_EXPECT_EQ(test,
		bt_audio.abox_virtual->streaming_if_0_sample_rate,
		scsc_bt_audio_get_rate(0));
	KUNIT_EXPECT_EQ(test,
		bt_audio.abox_virtual->streaming_if_1_sample_rate,
		scsc_bt_audio_get_rate(1));

	test_common_scsc_bt_module_normal_close(test);

	KUNIT_SUCCEED(test);
}

static void scsc_bt_trigger_recovery_test(struct kunit *test)
{
	kunit_info(test, "has user\n");
	test_common_scsc_bt_module_normal_open(test);
	KUNIT_EXPECT_EQ(test, 0, scsc_bt_kic_ops.trigger_recovery(NULL, slsi_kic_test_recovery_type_service_stop_panic));
	KUNIT_EXPECT_EQ(test, 0, scsc_bt_kic_ops.trigger_recovery(NULL, slsi_kic_test_recovery_type_service_start_panic));
	KUNIT_EXPECT_EQ(test, 0, scsc_bt_kic_ops.trigger_recovery(NULL, slsi_kic_test_recovery_type_subsystem_panic));
	test_common_scsc_bt_module_normal_close(test);

	KUNIT_EXPECT_EQ(test, 0, scsc_bt_kic_ops.trigger_recovery(NULL, slsi_kic_test_recovery_type_service_stop_panic));
	KUNIT_EXPECT_EQ(test, 0, scsc_bt_kic_ops.trigger_recovery(NULL, slsi_kic_test_recovery_type_service_start_panic));
	KUNIT_EXPECT_NE(test, 0, scsc_bt_kic_ops.trigger_recovery(NULL, slsi_kic_test_recovery_type_subsystem_panic));
	KUNIT_SUCCEED(test);
}

static void scsc_bt_moduel_etc_for_coverage(struct kunit *test)
{
	kunit_info(test, "mxman_recovery_disable() == false test\n");
	mxman_recovery_disabled_ret = true;
	test_common_scsc_bt_module_normal_open(test);
	mxman_recovery_disabled_ret = false;

	kunit_info(test, "slsi_bt_audio_probe error case\n");
	audio_device = NULL;
	slsi_bt_audio_probe();

	kunit_info(test, "slsi_bt_audio_remove normal case\n");
	audio_device = &fake_audio_dev;
	bt_audio.abox_physical = (struct scsc_bt_audio_abox *)&fake_audio_dev;
	bt_audio.dev_iommu_unmap = fake_dev_iommu_unmap;
	slsi_bt_audio_remove();

	audio_device = NULL;

	kunit_info(test, "firmware load err\n");
	mx140_file_request_conf_ret = -EFAULT;
	kunit_info(test, "firmware size=0 case\n");
	mx140_file_request_conf_ret = 0;
	fake_fw.size = 0;
	load_config();
	fake_fw.size = sizeof(fake_fw_data);
	test_common_scsc_bt_module_normal_close(test);

	KUNIT_SUCCEED(test);
}

static void scsc_bt_bt_fw_log_test(struct kunit *test)
{
	bt_fw_log(NULL, 0, 0, NULL);
	KUNIT_SUCCEED(test);
}

static void scsc_bt_fw_log_circ_buf_size_ops(struct kunit *test)
{
	char *buf = kunit_kmalloc(test, 64, GFP_USER);

	fw_log_circ_buf_size_ops.set("123", NULL);
	fw_log_circ_buf_size_ops.get(buf, NULL);

	kunit_kfree(test, buf);
	KUNIT_SUCCEED(test);
}

static void scsc_bt_fw_log_circ_buf_threshold_ops(struct kunit *test)
{
	char *buf = kunit_kmalloc(test, 64, GFP_USER);

	fw_log_circ_buf_threshold_ops.set("123", NULL);
	fw_log_circ_buf_threshold_ops.get(buf, NULL);

	kunit_kfree(test, buf);
	KUNIT_SUCCEED(test);
}

static struct kunit_case scsc_bt_test_cases[] = {
	KUNIT_CASE(scsc_bt_module_init_exit_test),
	KUNIT_CASE(scsc_bt_module_probe_remove_negavie_test),
	KUNIT_CASE(scsc_bt_shm_open_release_positive_test),
	KUNIT_CASE(scsc_bt_shm_open_release_with_audio_dev_test),
	KUNIT_CASE(scsc_bt_shm_open_negative),
	KUNIT_CASE(scsc_bt_shm_release_recovery),
	KUNIT_CASE(scsc_bt_mx_client_interface_test),
	KUNIT_CASE(scsc_bt_scsc_default_ioctl_test),
	KUNIT_CASE(scsc_bt_shm_irq_handler_test),
	KUNIT_CASE(slsi_sm_bt_service_cleanup_stop_service_negative_test),
	KUNIT_CASE(scsc_bt_proc_test),
	KUNIT_CASE(scsc_bt_mxlog_filter_ops),
	KUNIT_CASE(scsc_bt_btlog_enables_ops),
	KUNIT_CASE(scsc_bt_force_crash),
	KUNIT_CASE(scsc_bt_get_audio_info),
	KUNIT_CASE(scsc_bt_trigger_recovery_test),

	KUNIT_CASE(scsc_bt_bt_fw_log_test),
	KUNIT_CASE(scsc_bt_fw_log_circ_buf_size_ops),
	KUNIT_CASE(scsc_bt_fw_log_circ_buf_threshold_ops),

	KUNIT_CASE(scsc_bt_moduel_etc_for_coverage),
	{}
};

static struct kunit_suite scsc_bt_module_test_suite = {
	.name = "scsc_bt_module_unittest",
	.test_cases = scsc_bt_test_cases,
	.init = test_common_scsc_bt_module_init_probe,
	.exit = test_common_scsc_bt_module_remove_exit,
};

kunit_test_suite(scsc_bt_module_test_suite);
