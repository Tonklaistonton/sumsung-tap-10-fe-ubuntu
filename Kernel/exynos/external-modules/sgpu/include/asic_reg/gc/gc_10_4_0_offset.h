#ifdef CONFIG_GPU_VERSION_M0
#include "gc_10_4_0_offset_m0.h"
#elif defined(CONFIG_GPU_VERSION_M1)
#include "gc_10_4_0_offset_m1.h"
#elif defined(CONFIG_GPU_VERSION_M2)
#include "gc_10_4_0_offset_m2.h"
#elif defined(CONFIG_GPU_VERSION_M2_MID)
#include "gc_10_4_0_offset_m2.h"
#elif defined(CONFIG_GPU_VERSION_M3)
#include "gc_10_4_0_offset_m3.h"
#else
#error "Please specify the GPU version in defconfig"
#endif

#ifdef CONFIG_DRM_SGPU_UNKNOWN_REGISTERS_ENABLE
// addressBlock: gc_sdma0_sdma0dec
// base address: 0x4980
#define mmSDMA0_STATUS_REG                                                                             0x0025
#define mmSDMA0_STATUS_REG_BASE_IDX                                                                    0

// addressBlock: gc_sdma1_sdma1dec
// base address: 0x6180
#define mmSDMA1_STATUS_REG                                                                             0x0625
#define mmSDMA1_STATUS_REG_BASE_IDX                                                                    0

// addressBlock: gc_grbmdec
// base address: 0x8000
#ifndef CONFIG_GPU_VERSION_M3
#define mmGRBM_STATUS_SE1                                                                              0x0da6
#define mmGRBM_STATUS_SE1_BASE_IDX                                                                     0
#endif
#define mmGRBM_STATUS_SE2                                                                              0x0dae
#define mmGRBM_STATUS_SE2_BASE_IDX                                                                     0
#define mmGRBM_STATUS_SE3                                                                              0x0daf
#define mmGRBM_STATUS_SE3_BASE_IDX                                                                     0

// addressBlock: gc_cppdec
// base address: 0xc080
#define mmCP_RB1_BASE                                                                                  0x1e00
#define mmCP_RB1_BASE_BASE_IDX                                                                         0
#define mmCP_MEM_SLP_CNTL                                                                              0x1e19
#define mmCP_MEM_SLP_CNTL_BASE_IDX                                                                     0
#define mmCP_RB1_BASE_HI                                                                               0x1e52
#define mmCP_RB1_BASE_HI_BASE_IDX                                                                      0

// addressBlock: gc_rlcdec
// base address: 0x3b000
#define mmRLC_SPARE_INT                                                                                0x4ccc
#define mmRLC_SPARE_INT_BASE_IDX                                                                       1

// addressBlock: sqind
// base address: 0x0
#define ixSQ_WAVE_INST_DW0                                                                             0x010a
#endif /* CONFIG_DRM_SGPU_UNKNOWN_REGISTERS_ENABLE */
