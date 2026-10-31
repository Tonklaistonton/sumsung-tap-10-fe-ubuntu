/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_LOCAL_PACKET_CAPTURE_H__
#define __KUNIT_MOCK_LOCAL_PACKET_CAPTURE_H__

#include "../local_packet_capture.h"

#define slsi_lpc_init()					0
#define slsi_lpc_deinit(arg)				0
#define slsi_lpc_is_lpc_enabled()			kunit_mock_slsi_lpc_is_enabled()
#define slsi_lpc_start(args...)				kunit_mock_slsi_lpc_start(args)
#define slsi_lpc_stop(args...)				kunit_mock_slsi_lpc_stop(args)
#define slsi_lpc_send_ampdu_rx_later(args...)		kunit_mock_slsi_lpc_send_ampdu_rx_later(args)
#define slsi_lpc_add_packet_data(args...)		kunit_mock_slsi_lpc_add_packet_data(args)
#define slsi_lpc_is_lpc_enabled_by_name(arg)		kunit_mock_slsi_lpc_is_lpc_enabled_by_name(arg)


static int kunit_mock_slsi_lpc_is_enabled(void)
{
	return 0;
}

static int kunit_mock_slsi_lpc_start(struct slsi_dev *sdev, int type, const char *lpc_name)
{
	return 0;
}

static int kunit_mock_slsi_lpc_stop(struct slsi_dev *sdev, const char *lpc_name, bool force)
{
	return 0;
}

static int kunit_mock_slsi_lpc_send_ampdu_rx_later(u32 lpc_tag, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_lpc_add_packet_data(u32 lpc_tag, struct sk_buff *skb, int data_type)
{
	return 0;
}

static int kunit_mock_slsi_lpc_is_lpc_enabled_by_name(const char *lpc_name)
{
	return slsi_lpc_is_lpc_enabled();
}
#endif
