#include <kunit/test.h>

#define kfifo_from_user(args...)			(0)
#define mxmgmt_transport_send(args...)			(0)
#define kfifo_to_user(args...)			(0)

static void channel_message_handler(const void *message, void *data);
void kunit_channel_message_handler(const void *message, void *data)
{
	channel_message_handler(message, data);
}

static void channel_message_handler_wpan(const void *message, void *data);
void kunit_channel_message_handler_wpan(const void *message, void *data)
{
	channel_message_handler_wpan(message, data);
}

static int cmd_dev_open(struct inode *inode, struct file *file);
int (*fp_cmd_dev_open)(struct inode *inode, struct file *file) = &cmd_dev_open;

static int cmd_dev_wpan_open(struct inode *inode, struct file *file);
int (*fp_cmd_dev_wpan_open)(struct inode *inode, struct file *file) = &cmd_dev_wpan_open;


static unsigned int cmd_dev_poll(struct file *filp, poll_table *wait);
unsigned int (*fp_cmd_dev_poll)(struct file *filp, poll_table *wait) = &cmd_dev_poll;

static unsigned int cmd_dev_wpan_poll(struct file *filp, poll_table *wait);
unsigned int (*fp_cmd_dev_wpan_poll)(struct file *filp, poll_table *wait) = &cmd_dev_wpan_poll;

static int cmd_dev_release(struct inode *inode, struct file *file);
int (*fp_cmd_dev_release)(struct inode *inode, struct file *file) = &cmd_dev_release;

static int cmd_dev_wpan_release(struct inode *inode, struct file *file);
int (*fp_cmd_dev_wpan_release)(struct inode *inode, struct file *file) = &cmd_dev_wpan_release;

static ssize_t cmd_dev_write(struct file *filp, const char *ubuf, size_t len, loff_t *offset);
ssize_t (*fp_cmd_dev_write)(struct file *filp, const char *ubuf, size_t len, loff_t *offset) = &cmd_dev_write;

static ssize_t cmd_dev_wpan_write(struct file *filp, const char *ubuf, size_t len, loff_t *offset);
ssize_t (*fp_cmd_dev_wpan_write)(struct file *filp, const char *ubuf, size_t len, loff_t *offset) = &cmd_dev_wpan_write;

static ssize_t cmd_dev_read(struct file *filp, char *ubuf, size_t len, loff_t *offset);
ssize_t (*fp_cmd_dev_read)(struct file *filp, char *ubuf, size_t len, loff_t *offset) = &cmd_dev_read;

static ssize_t cmd_dev_wpan_read(struct file *filp, char *ubuf, size_t len, loff_t *offset);
ssize_t (*fp_cmd_dev_wpan_read)(struct file *filp,  char *ubuf, size_t len, loff_t *offset) = &cmd_dev_wpan_read;
