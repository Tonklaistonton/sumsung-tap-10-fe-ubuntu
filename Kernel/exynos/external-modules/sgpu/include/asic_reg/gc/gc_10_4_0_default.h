#ifdef CONFIG_GPU_VERSION_M0
#include "gc_10_4_0_default_m0.h"
#elif defined(CONFIG_GPU_VERSION_M1)
#include "gc_10_4_0_default_m1.h"
#elif defined(CONFIG_GPU_VERSION_M2)
#include "gc_10_4_0_default_m2.h"
#elif defined(CONFIG_GPU_VERSION_M2_MID)
#include "gc_10_4_0_default_m2.h"
#elif defined(CONFIG_GPU_VERSION_M3)
#include "gc_10_4_0_default_m3.h"
#else
#error "Please specify the GPU version in defconfig"
#endif
