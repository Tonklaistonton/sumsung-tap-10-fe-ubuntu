#ifdef CONFIG_GPU_VERSION_M0
#include "gc_10_4_0_sh_mask_m0.h"
#elif defined(CONFIG_GPU_VERSION_M1)
#include "gc_10_4_0_sh_mask_m1.h"
#elif defined(CONFIG_GPU_VERSION_M2)
#include "gc_10_4_0_sh_mask_m2.h"
#elif defined(CONFIG_GPU_VERSION_M2_MID)
#include "gc_10_4_0_sh_mask_m2.h"
#elif defined(CONFIG_GPU_VERSION_M3)
#include "gc_10_4_0_sh_mask_m3.h"
#else
#error "Please specify the GPU version in defconfig"
#endif

#ifdef CONFIG_DRM_SGPU_UNKNOWN_REGISTERS_ENABLE
// addressBlock: gc_sqdec
//SH_MEM_CONFIG
#define SH_MEM_CONFIG__DEFAULT_MTYPE__SHIFT                                                                   0x4
#define SH_MEM_CONFIG__RETRY_MODE__SHIFT                                                                      0xc

// addressBlock: gc_cppdec
//CP_MEM_SLP_CNTL
#define CP_MEM_SLP_CNTL__CP_MEM_LS_EN__SHIFT                                                                  0x0
#define CP_MEM_SLP_CNTL__CP_MEM_DS_EN__SHIFT                                                                  0x1
#define CP_MEM_SLP_CNTL__RESERVED__SHIFT                                                                      0x2
#define CP_MEM_SLP_CNTL__CP_LS_DS_BUSY_OVERRIDE__SHIFT                                                        0x7
#define CP_MEM_SLP_CNTL__CP_MEM_LS_ON_DELAY__SHIFT                                                            0x8
#define CP_MEM_SLP_CNTL__CP_MEM_LS_OFF_DELAY__SHIFT                                                           0x10
#define CP_MEM_SLP_CNTL__RESERVED1__SHIFT                                                                     0x18
#define CP_MEM_SLP_CNTL__CP_MEM_LS_EN_MASK                                                                    0x00000001L
#define CP_MEM_SLP_CNTL__CP_MEM_DS_EN_MASK                                                                    0x00000002L
#define CP_MEM_SLP_CNTL__RESERVED_MASK                                                                        0x0000007CL
#define CP_MEM_SLP_CNTL__CP_LS_DS_BUSY_OVERRIDE_MASK                                                          0x00000080L
#define CP_MEM_SLP_CNTL__CP_MEM_LS_ON_DELAY_MASK                                                              0x0000FF00L
#define CP_MEM_SLP_CNTL__CP_MEM_LS_OFF_DELAY_MASK                                                             0x00FF0000L
#define CP_MEM_SLP_CNTL__RESERVED1_MASK                                                                       0xFF000000L
//CP_RB1_BASE_HI
#define CP_RB1_BASE_HI__RB_BASE_HI__SHIFT                                                                     0x0
#define CP_RB1_BASE_HI__RB_BASE_HI_MASK                                                                       0x000000FFL
//CP_RB_WPTR_POLL_CNTL
#define CP_RB_WPTR_POLL_CNTL__POLL_FREQUENCY__SHIFT                                                           0x0
#define CP_RB_WPTR_POLL_CNTL__POLL_FREQUENCY_MASK                                                             0x0000FFFFL
#endif /* CONFIG_DRM_SGPU_UNKNOWN_REGISTERS_ENABLE */
