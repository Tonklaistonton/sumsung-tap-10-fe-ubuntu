/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_IOCTL_H__
#define __KUNIT_MOCK_IOCTL_H__

#include "../ioctl.h"

#define slsi_ioctl(args...)			kunit_mock_slsi_ioctl(args)
#define slsi_get_private_command_args(args...)	kunit_mock_slsi_get_private_command_args(args)
#define slsi_get_sta_info(args...)		kunit_mock_slsi_get_sta_info(args)
#define slsi_verify_ioctl_args(args...)		kunit_mock_slsi_verify_ioctl_args(args)

static int kunit_mock_slsi_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	return 0;
}

static struct slsi_ioctl_args *kunit_mock_slsi_get_private_command_args(char *buffer, int buf_len, int max_arg_count)
{
	struct slsi_ioctl_args *ioctl_args = NULL;
	char *pos                          = buffer;

	ioctl_args = kmalloc(sizeof(*ioctl_args) + sizeof(u8 *) * max_arg_count, GFP_KERNEL);
	if (!ioctl_args)
		return NULL;
	memset(ioctl_args->args, '\0', sizeof(u8 *) * max_arg_count);

	ioctl_args->arg_count = 0;
	while (buf_len > 0 && ioctl_args->arg_count < max_arg_count) {
		pos = strchr(pos, ' ');
		if (!pos)
			break;
		buf_len = buf_len - (pos - buffer + 1);
		if (buf_len <= 0)
			break;
		*pos = '\0';
		pos++;
		while (*pos == ' ')
			pos++;
		buffer = pos;
		ioctl_args->args[ioctl_args->arg_count++] = pos;
	}
	return ioctl_args;
}

static int kunit_mock_slsi_get_sta_info(struct net_device *dev, char *command, int buf_len)
{
	struct netdev_vif   *ndev_vif = netdev_priv(dev);

	if (!ndev_vif->activated)
		return -EINVAL;

	return 0;
}

static int slsi_verify_ioctl_args(struct slsi_dev *sdev, struct slsi_ioctl_args *ioctl_args)
{
	if (!ioctl_args) {
		SLSI_ERR(sdev, "Malloc of ioctl_args failed.\n");
		return -ENOMEM;
	}

	if (!ioctl_args->arg_count) {
		kfree(ioctl_args);
		return -EINVAL;
	}

	return 0;
}
#endif
