/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_LLS_H__
#define __KUNIT_MOCK_LLS_H__

#include "../lls.h"

#define slsi_lls_start_stats(args...)		kunit_mock_slsi_lls_start_stats(args)
#define slsi_lls_fill_stats(args...)		kunit_mock_slsi_lls_fill_stats(args)
#define slsi_lls_stop_stats(args...)		kunit_mock_slsi_lls_stop_stats(args)


static void kunit_mock_slsi_lls_start_stats(struct slsi_dev *sdev, u32 mpdu_size_threshold, u32 aggr_stat_gathering)
{
}

static int kunit_mock_slsi_lls_fill_stats(struct slsi_dev *sdev, u8 **src_buf, bool is_mlo)
{
	return 0;
}

static void kunit_mock_slsi_lls_stop_stats(struct slsi_dev *sdev, u32 stats_clear_req_mask)
{
}

#endif
