/****************************************************************************
 *
 * Copyright (c) 2024 - 2024 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/uaccess.h>

#include <scsc/scsc_mx.h>
#include <scsc/scsc_logring.h>
#include "mxmgmt_transport_format.h"
#include "mxmgmt_transport.h"
#include "mxman.h"

#ifdef CONFIG_WLBT_KUNIT
#include "./kunit/kunit_mx_cmd.c"
#endif

#define PACKET_SIZE MXMGR_MESSAGE_PAYLOAD_SIZE
#define NUM_PACKET 10
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct cmd_msg_packet {
	char msg[PACKET_SIZE];
};

struct mx_cmd {
	struct kfifo read_buffer;
	struct kfifo write_buffer;
};

struct buffer_ref {
	struct kfifo *read_buffer;
	struct kfifo *write_buffer;
};

static dev_t cmd_dev_t;
static struct class *cmd_class;
static struct cdev  *cmd_cdev;

struct mx_cmd cmd_wlan;
wait_queue_head_t read_wait;

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static dev_t cmd_dev_t_wpan;
static struct class *cmd_class_wpan;
static struct cdev  *cmd_cdev_wpan;

struct mx_cmd cmd_wpan;
wait_queue_head_t read_wait_wpan;
#endif

static struct scsc_mx *mx;

void cmd_module_probe(struct scsc_mx_module_client *module_client, struct scsc_mx *scsc_mx, enum scsc_module_client_reason reason)
{
	mx = scsc_mx;

	SCSC_TAG_INFO(MXMAN_TEST, "OK\n");
}

void cmd_module_remove(struct scsc_mx_module_client *module_client, struct scsc_mx *scsc_mx, enum scsc_module_client_reason reason)
{
	if (mx != scsc_mx) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx != mx\n");
		return;
	}

	SCSC_TAG_INFO(MXMAN_TEST, "OK\n");
}

static struct scsc_mx_module_client mx_cmd_driver = {
	.name = "MX cmd wlan driver",
	.probe = cmd_module_probe,
	.remove = cmd_module_remove,
};

static struct scsc_mx_module_client mx_cmd_driver_wpan = {
	.name = "MX cmd wpan driver",
	.probe = cmd_module_probe,
	.remove = cmd_module_remove,
};

void set_rwbuffer(struct buffer_ref *ref, enum scsc_subsystem sub)
{
	if (sub == SCSC_SUBSYSTEM_WLAN) {
		ref->write_buffer = &cmd_wlan.write_buffer;
		ref->read_buffer = &cmd_wlan.read_buffer;
	}
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	else if (sub == SCSC_SUBSYSTEM_WPAN) {
		ref->write_buffer = &cmd_wpan.write_buffer;
		ref->read_buffer = &cmd_wpan.read_buffer;
	}
#endif
}

static void channel_message_handler(const void *message, void *data)
{
	const struct cmd_msg_packet *msg = message;
	struct buffer_ref buffer = {NULL, NULL};
	
	buffer.write_buffer = &cmd_wlan.write_buffer;
	buffer.read_buffer = &cmd_wlan.read_buffer;

	SCSC_TAG_INFO(MXMAN_TEST, "Receive buffer available : %d\n", kfifo_avail(buffer.read_buffer));

	if (kfifo_avail(buffer.read_buffer) >= PACKET_SIZE) {
		kfifo_in(buffer.read_buffer, msg->msg, PACKET_SIZE);
		SCSC_TAG_INFO(MXMAN_TEST, "Received message moved to read buffer(%d)\n", kfifo_len(buffer.read_buffer));
		wake_up_interruptible(&read_wait);
	} else {
		SCSC_TAG_ERR(MXMAN_TEST, "Not enough read buffer to handle received message\n");
	}
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static void channel_message_handler_wpan(const void *message, void *data)
{
	const struct cmd_msg_packet *msg = message;
	struct buffer_ref buffer = {NULL, NULL};
	
	buffer.write_buffer = &cmd_wpan.write_buffer;
	buffer.read_buffer = &cmd_wpan.read_buffer;

	SCSC_TAG_INFO(MXMAN_TEST, "Receive buffer available : %d\n", kfifo_avail(buffer.read_buffer));

	if (kfifo_avail(buffer.read_buffer) >= PACKET_SIZE) {
		kfifo_in(buffer.read_buffer, msg->msg, PACKET_SIZE);
		SCSC_TAG_INFO(MXMAN_TEST, "Received message moved to read buffer(%d)\n", kfifo_len(buffer.read_buffer));
		wake_up_interruptible(&read_wait_wpan);
	} else {
		SCSC_TAG_ERR(MXMAN_TEST, "Not enough read buffer to handle received message\n");
	}
}
#endif

static int dev_open(enum scsc_subsystem sub)
{
#if defined(CONFIG_SCSC_PCIE_CHIP)
	int ret = 0;
#endif
	struct mxman *mxman;
	struct buffer_ref buffer = {NULL, NULL};

	SCSC_TAG_INFO(MXMAN_TEST, "open mx cmd(%d)\n", sub);

	if (mx == NULL) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx is NULL\n");
		return -EFAULT;
	}

	mxman = scsc_mx_get_mxman(mx);

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	if (!mxman_if_subsys_active(mxman, sub))
#else
	if (mxman->mxman_state != MXMAN_STATE_STARTED)
#endif
	{
		SCSC_TAG_ERR(MXMAN_TEST, "(%d) is not active\n", sub);
		return -EFAULT;
	}

#if defined(CONFIG_SCSC_PCIE_CHIP)
	ret = scsc_mx_service_claim(DEFAULT_CLAIM_TYPE);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error claiming link\n");
		return ret;
	}
#endif

	set_rwbuffer(&buffer, sub);

	if (sub == SCSC_SUBSYSTEM_WLAN) {
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mx),
			MMTRANS_CHAN_ID_MAXWELL_COMMAND, &channel_message_handler, NULL);
	}
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	else if (sub == SCSC_SUBSYSTEM_WPAN) {
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mx),
			MMTRANS_CHAN_ID_MAXWELL_COMMAND, &channel_message_handler_wpan, NULL);
	}
#endif
	SCSC_TAG_INFO(MXMAN_TEST, "handler registered\n");

	if (kfifo_alloc(buffer.write_buffer, PACKET_SIZE * NUM_PACKET, GFP_KERNEL)){
		SCSC_TAG_ERR(MXMAN_TEST, "error kfifo_alloc\n");
		return -EFAULT;
	}

	if (kfifo_alloc(buffer.read_buffer, PACKET_SIZE * NUM_PACKET, GFP_KERNEL)){
		SCSC_TAG_ERR(MXMAN_TEST, "error kfifo_alloc\n");
		return -EFAULT;
	}

	return 0;
}

static int cmd_dev_open(struct inode *inode, struct file *file)
{
	return dev_open(SCSC_SUBSYSTEM_WLAN);
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static int cmd_dev_wpan_open(struct inode *inode, struct file *file)
{
	return dev_open(SCSC_SUBSYSTEM_WPAN);
}
#endif

static int dev_release(enum scsc_subsystem sub)
{
#if defined(CONFIG_SCSC_PCIE_CHIP)
	int ret = 0;
#endif
	struct buffer_ref buffer = {NULL, NULL};

	SCSC_TAG_INFO(MXMAN_TEST, "close mx cmd(%d)\n", sub);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	ret = scsc_mx_service_release(DEFAULT_CLAIM_TYPE);
	if (ret) {
		SCSC_TAG_ERR(MXMAN, "Error releasing link\n");
		return ret;
	}
#endif

	set_rwbuffer(&buffer, sub);

	if (mx == NULL) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx NULL \n");
		return -EFAULT;
	}

	if (sub == SCSC_SUBSYSTEM_WLAN) {
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mx),
		MMTRANS_CHAN_ID_MAXWELL_COMMAND, NULL, NULL);
	}
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	else if (sub == SCSC_SUBSYSTEM_WPAN) {
		mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport_wpan(mx),
		MMTRANS_CHAN_ID_MAXWELL_COMMAND, NULL, NULL);
	}
#endif
	SCSC_TAG_INFO(MXMAN_TEST, "handler released\n");

	kfifo_free(buffer.write_buffer);
	kfifo_free(buffer.read_buffer);

	return 0;
}

static int cmd_dev_release(struct inode *inode, struct file *file)
{
	return dev_release(SCSC_SUBSYSTEM_WLAN);
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static int cmd_dev_wpan_release(struct inode *inode, struct file *file)
{
	return dev_release(SCSC_SUBSYSTEM_WPAN);
}
#endif

static ssize_t dev_write(const char *data, size_t len, enum scsc_subsystem sub)
{
	int ret, transfered = 0;
	char packet_to_fw[PACKET_SIZE];
	unsigned int copied = 0;
	struct buffer_ref buffer = {NULL, NULL};

	set_rwbuffer(&buffer, sub);

	if (buffer.write_buffer == NULL)
		return -EFAULT;

	if (kfifo_avail(buffer.write_buffer) >= len + PACKET_SIZE) {
		ret = kfifo_from_user(buffer.write_buffer, data, len, &copied);
		if (!ret) {
			SCSC_TAG_INFO(MX_MMAP, "Copied %d bytes from user.\n", copied);
			ret = copied;
		} else {
			SCSC_TAG_ERR(MX_MMAP, "returned : %d\n", ret);
			return ret;
		}
	}

	while (kfifo_len(buffer.write_buffer) >= PACKET_SIZE) {
		ret = kfifo_out(buffer.write_buffer, packet_to_fw, PACKET_SIZE);
		if (mx != NULL) {
			if (sub == SCSC_SUBSYSTEM_WLAN) {
				mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mx),
				MMTRANS_CHAN_ID_MAXWELL_COMMAND, packet_to_fw, PACKET_SIZE);
			}
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
			else if (sub == SCSC_SUBSYSTEM_WPAN) {
				mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport_wpan(mx),
				MMTRANS_CHAN_ID_MAXWELL_COMMAND, packet_to_fw, PACKET_SIZE);
			}
#endif
			SCSC_TAG_INFO(MX_MMAP, "1 packet %d bytes sent to fw\n", PACKET_SIZE);
			transfered += PACKET_SIZE;
		}
		SCSC_TAG_INFO(MX_MMAP, "%d bytes ramains after a packet sent to fw\n", kfifo_len(buffer.write_buffer));
	}

	SCSC_TAG_INFO(MX_MMAP, "Finally, %d transfered, %d bytes ramains after write operation\n", transfered, kfifo_len(buffer.write_buffer));

	return copied;
}

static ssize_t cmd_dev_write(struct file *file, const char *data, size_t len, loff_t *offset)
{
	return dev_write(data, len, SCSC_SUBSYSTEM_WLAN);
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static ssize_t cmd_dev_wpan_write(struct file *file, const char *data, size_t len, loff_t *offset)
{
	return dev_write(data, len, SCSC_SUBSYSTEM_WPAN);
}
#endif

static ssize_t dev_read(struct file *file, char *data, size_t len, enum scsc_subsystem sub)
{
	int ret = 0;
	unsigned int copied;
	struct buffer_ref buffer = {NULL, NULL};
	wait_queue_head_t *rwait = NULL;

	set_rwbuffer(&buffer, sub);

	if (buffer.read_buffer == NULL)
		return -EFAULT;

	SCSC_TAG_INFO(MX_MMAP, "%d requested, %d in read buffer\n", len, kfifo_len(buffer.read_buffer));

	while (len) {
		if (!kfifo_is_empty(buffer.read_buffer)) {
			ret = kfifo_to_user(buffer.read_buffer, data, MIN(len, kfifo_len(buffer.read_buffer)), &copied);
			if (!ret) {
				SCSC_TAG_INFO(MX_MMAP, "%d bytes copied to user, %d bytes left in read buffer\n", copied, kfifo_len(buffer.read_buffer));
				ret = copied;
			}
			break;
		}

		SCSC_TAG_INFO(MX_MMAP, "no more message available to send\n");

		if (file->f_flags & O_NONBLOCK) {
			SCSC_TAG_INFO(MX_MMAP, "O_NONBLOCK\n");
			ret = -EAGAIN;
			break;
		}
		
		if (sub == SCSC_SUBSYSTEM_WLAN) {
			rwait = &read_wait;
		}
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
		else if (sub == SCSC_SUBSYSTEM_WPAN) {
			rwait = &read_wait_wpan;
		}
#endif
		ret = wait_event_interruptible(*rwait,
				!kfifo_is_empty(buffer.read_buffer));

		if (ret < 0) {
			break;
		}
	}

	SCSC_TAG_INFO(MX_MMAP, "retuned : %d\n", ret);

	return ret;
}

static ssize_t cmd_dev_read(struct file *file, char *data, size_t len, loff_t *offset)
{
	return dev_read(file, data, len, SCSC_SUBSYSTEM_WLAN);
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static ssize_t cmd_dev_wpan_read(struct file *file, char *data, size_t len, loff_t *offset)
{
	return dev_read(file, data, len, SCSC_SUBSYSTEM_WPAN);
}
#endif

static unsigned int dev_poll(struct file *filp, poll_table *wait, enum scsc_subsystem sub)
{
	unsigned int mask = 0;
	struct buffer_ref buffer = {NULL, NULL};
	wait_queue_head_t *rwait = NULL;

	set_rwbuffer(&buffer, sub);

	if (sub == SCSC_SUBSYSTEM_WLAN) {
		rwait = &read_wait;
	}
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
	else if (sub == SCSC_SUBSYSTEM_WPAN) {
		rwait = &read_wait_wpan;
	}
#endif

	if (buffer.read_buffer == NULL || rwait == NULL)
		return -EFAULT;

	poll_wait(filp, rwait, wait);

	if (!kfifo_is_empty(buffer.read_buffer)) {
		SCSC_TAG_INFO(MX_MMAP, "readable & ");
		mask |= POLLIN | POLLRDNORM;  /* readeable */
	}
	SCSC_TAG_INFO(MX_MMAP, "writable\n");
	mask |= POLLOUT | POLLWRNORM;         /* writable */

	return mask;
}

static unsigned int cmd_dev_poll(struct file *filp, poll_table *wait)
{
	return dev_poll(filp, wait, SCSC_SUBSYSTEM_WLAN);
}

#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static unsigned int cmd_dev_wpan_poll(struct file *filp, poll_table *wait)
{
	return dev_poll(filp, wait, SCSC_SUBSYSTEM_WPAN);
}
#endif

static const struct file_operations mx_cmd_dev_fops = {
	.owner = THIS_MODULE,
	.open = cmd_dev_open,
	.read = cmd_dev_read,
	.write = cmd_dev_write,
	.poll = cmd_dev_poll,
	.release = cmd_dev_release,
};
 
#if defined(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM)
static const struct file_operations mx_cmd_dev_wpan_fops = {
	.owner = THIS_MODULE,
	.open = cmd_dev_wpan_open,
	.read = cmd_dev_wpan_read,
	.write = cmd_dev_wpan_write,
	.poll = cmd_dev_wpan_poll,
	.release = cmd_dev_wpan_release,
};
#endif

int scsc_mx_cmd_driver_create(void)
{
	int r;

	const char *chrdev_region = "wlbt-mx-cmd-wlan";
	const char *class_name = "mx_cmd_class_wlan";
	const char *device_name = "mx_cmd_wlan";

	SCSC_TAG_INFO(MXMAN_TEST, "\n");

	r = scsc_mx_module_register_client_module(&mx_cmd_driver);
	if (r) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_module_register_client_module failed: r=%d\n", r);
		return r;
	}

	init_waitqueue_head(&read_wait);

	r = alloc_chrdev_region(&cmd_dev_t, 0, 1, chrdev_region);

	if (r < 0) {
		SCSC_TAG_ERR(MXMAN_TEST, "failed to alloc chrdev region\n");
		goto fail_alloc_chrdev_region;
	}

	cmd_cdev = cdev_alloc();
	if (!cmd_cdev) {
		r = -ENOMEM;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}

	cdev_init(cmd_cdev, &mx_cmd_dev_fops);
	r = cdev_add(cmd_cdev, cmd_dev_t, 1);
	if (r < 0) {
		SCSC_TAG_ERR(MXMAN_TEST, "failed to add cdev\n");
		goto fail_add_cdev;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	cmd_class = class_create(class_name);
#else
	cmd_class = class_create(THIS_MODULE, class_name);
#endif
	if (!cmd_class) {
		r = -EEXIST;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(cmd_class, NULL, cmd_dev_t, NULL, "%s_%d", device_name, MINOR(cmd_dev_t))) {
		r = -EINVAL;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to create device\n");
		goto fail_create_device;
	}

	return 0;
fail_create_device:
	class_destroy(cmd_class);
fail_create_class:
	cdev_del(cmd_cdev);
fail_add_cdev:
fail_alloc_cdev:
	unregister_chrdev_region(cmd_dev_t, 1);
fail_alloc_chrdev_region:
	return r;
}

int scsc_mx_cmd_driver_create_wpan(void)
{
	int r;

	const char *chrdev_region = "wlbt-mx-cmd-wpan";
	const char *class_name = "mx_cmd_class_wpan";
	const char *device_name = "mx_cmd_wpan";

	SCSC_TAG_INFO(MXMAN_TEST, "\n");

	r = scsc_mx_module_register_client_module(&mx_cmd_driver_wpan);
	if (r) {
		SCSC_TAG_ERR(MXMAN_TEST, "scsc_mx_module_register_client_module failed: r=%d\n", r);
		return r;
	}

	init_waitqueue_head(&read_wait_wpan);

	r = alloc_chrdev_region(&cmd_dev_t_wpan, 0, 1, chrdev_region);

	if (r < 0) {
		SCSC_TAG_ERR(MXMAN_TEST, "failed to alloc chrdev region\n");
		goto fail_alloc_chrdev_region;
	}

	cmd_cdev_wpan= cdev_alloc();
	if (!cmd_cdev_wpan) {
		r = -ENOMEM;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}

	cdev_init(cmd_cdev_wpan, &mx_cmd_dev_wpan_fops);
	r = cdev_add(cmd_cdev_wpan, cmd_dev_t_wpan, 1);
	if (r < 0) {
		SCSC_TAG_ERR(MXMAN_TEST, "failed to add cdev\n");
		goto fail_add_cdev;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	cmd_class_wpan= class_create(class_name);
#else
	cmd_class_wpan= class_create(THIS_MODULE, class_name);
#endif
	if (!cmd_class_wpan) {
		r = -EEXIST;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(cmd_class_wpan, NULL, cmd_dev_t_wpan, NULL, "%s_%d", device_name, MINOR(cmd_dev_t_wpan))) {
		r = -EINVAL;
		SCSC_TAG_ERR(MXMAN_TEST, "failed to create device\n");
		goto fail_create_device;
	}

	return 0;
fail_create_device:
	class_destroy(cmd_class_wpan);
fail_create_class:
	cdev_del(cmd_cdev_wpan);
fail_add_cdev:
fail_alloc_cdev:
	unregister_chrdev_region(cmd_dev_t_wpan, 1);
fail_alloc_chrdev_region:
	return r;
}

void scsc_mx_cmd_driver_destroy(void)
{
	SCSC_TAG_INFO(MXMAN_TEST, "\n");

	scsc_mx_module_unregister_client_module(&mx_cmd_driver);

	device_destroy(cmd_class, cmd_dev_t);
	class_destroy(cmd_class);
	cdev_del(cmd_cdev);
	unregister_chrdev_region(cmd_dev_t, 1);
}

void scsc_mx_cmd_driver_destroy_wpan(void)
{
	SCSC_TAG_INFO(MXMAN_TEST, "\n");

	scsc_mx_module_unregister_client_module(&mx_cmd_driver_wpan);

	device_destroy(cmd_class_wpan, cmd_dev_t_wpan);
	class_destroy(cmd_class_wpan);
	cdev_del(cmd_cdev_wpan);
	unregister_chrdev_region(cmd_dev_t_wpan, 1);
}
