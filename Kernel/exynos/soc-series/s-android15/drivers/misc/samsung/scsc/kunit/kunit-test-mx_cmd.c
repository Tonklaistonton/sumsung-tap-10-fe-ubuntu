#include <kunit/test.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <scsc/scsc_mx.h>
#include "../mxman.h"
#include "common.h"

extern ssize_t (*fp_cmd_dev_write)(struct file *filp, char *ubuf, size_t len, loff_t *offset);
extern ssize_t (*fp_cmd_dev_wpan_write)(struct file *filp, char *ubuf, size_t len, loff_t *offset);
extern int (*fp_cmd_dev_open)(struct inode *inode, struct file *file);
extern int (*fp_cmd_dev_wpan_open)(struct inode *inode, struct file *file);
extern unsigned int (*fp_cmd_dev_poll)(struct file *filp, poll_table *wait);
extern unsigned int (*fp_cmd_dev_wpan_poll)(struct file *filp, poll_table *wait);
extern int (*fp_cmd_dev_release)(struct inode *inode, struct file *file);
extern int (*fp_cmd_dev_wpan_release)(struct inode *inode, struct file *file);
extern ssize_t (*fp_cmd_dev_read)(struct file *filp, char *ubuf, size_t len, loff_t *offset);
extern ssize_t (*fp_cmd_dev_wpan_read)(struct file *filp, char *ubuf, size_t len, loff_t *offset);


struct cmd_msg_packet {
	char msg[127];
};

static void test_all(struct kunit *test)
{
	struct file fp;
	size_t len;
	loff_t *offset;
	poll_table *wait;

	struct mxman *mxman;
	struct scsc_mx *scscmx;

	fp_cmd_dev_open(NULL, NULL);
	scscmx = test_alloc_scscmx(test, get_mif());
	mxman = scsc_mx_get_mxman(scscmx);
	cmd_module_probe(NULL, scscmx, NULL);

	mxman->mxman_state = MXMAN_STATE_STOPPED;
	fp_cmd_dev_open(NULL, NULL);
	fp_cmd_dev_wpan_open(NULL, NULL);

	mxman->mxman_state = MXMAN_STATE_STARTED_WLAN;
	fp_cmd_dev_open(NULL, NULL);

	mxman->mxman_state = MXMAN_STATE_STARTED_WPAN;
	fp_cmd_dev_wpan_open(NULL, NULL);

	struct cmd_msg_packet msg;
	strcpy(msg.msg, "ABCDE");

	kunit_channel_message_handler(&msg, NULL);
	kunit_channel_message_handler_wpan(&msg, NULL);

	len = 3;
	offset = 0;

	fp_cmd_dev_write(&fp, "ABC", len, offset);
	fp_cmd_dev_read(&fp, "ABD", len, offset);
	fp_cmd_dev_poll(&fp, wait);
	fp_cmd_dev_release(NULL, NULL);

	fp_cmd_dev_wpan_read(&fp, "ABD", len, offset);
	fp_cmd_dev_wpan_write(&fp, "ABC", len, offset);
	fp_cmd_dev_wpan_poll(&fp, wait);
	fp_cmd_dev_wpan_release(NULL, NULL);

	cmd_module_remove(NULL, scscmx, NULL);
	scsc_mx_cmd_driver_destroy();
	scsc_mx_cmd_driver_destroy_wpan();

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
	KUNIT_CASE(test_all),
	{}
};

static struct kunit_suite test_suite[] = {
	{
		.name = "test_mx_cmd",
		.test_cases = test_cases,
		.init = test_init,
		.exit = test_exit,
	}
};

kunit_test_suites(test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yongjin.lim@samsung.com>");

