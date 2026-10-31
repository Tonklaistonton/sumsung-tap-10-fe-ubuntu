#ifndef __CMUCAL_VCLK_H__
#define __CMUCAL_VCLK_H__

#include "../../cmucal.h"

enum vclk_id {

/* DVFS VCLK*/
    VCLK_VDD_ALIVE = DFS_VCLK_TYPE,
    VCLK_VDD_MM,
    VCLK_VDD_CAM,
    VCLK_VDD_CHUBVTS,
    VCLK_VDDI,
    VCLK_VDD_CPUCL0,
    VCLK_VDD_NPU,
    VCLK_VDD_G3D,
    VCLK_VDD_MIF,
	end_of_dfs_vclk,
	num_of_dfs_vclk = end_of_dfs_vclk - DFS_VCLK_TYPE,

/* SPECIAL VCLK*/
	end_of_common_vclk,
	num_of_common_vclk = end_of_common_vclk - ((MASK_OF_ID & end_of_dfs_vclk) | COMMON_VCLK_TYPE),

/* GATE VCLK*/
	end_of_gate_vclk,
	num_of_gate_vclk = end_of_gate_vclk - ((MASK_OF_ID & end_of_common_vclk) | GATE_VCLK_TYPE),
};
#endif
