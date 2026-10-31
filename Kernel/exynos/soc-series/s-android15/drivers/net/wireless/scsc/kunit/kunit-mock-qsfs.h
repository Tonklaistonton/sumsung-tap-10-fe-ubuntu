/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_QSFS_H__
#define __KUNIT_MOCK_QSFS_H__

#include "../qsfs.h"

#define slsi_get_qsfs_feature_set(args...)		kunit_mock_slsi_get_qsfs_feature_set(args)
#define slsi_qsf_init(arg)				kunit_mock_slsi_qsf_init(arg)

static void kunit_mock_slsi_get_qsfs_feature_set(struct slsi_dev *sdev)
{
}

static void slsi_qsf_init(struct slsi_dev *sdev)
{
}
#endif
