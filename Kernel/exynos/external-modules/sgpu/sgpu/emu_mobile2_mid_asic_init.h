/*
 * Copyright 2023 Samsung Electronics.
 */

#ifndef MOBILE2_MID_ASIC_INIT_H
#define MOBILE2_MID_ASIC_INIT_H

struct __reg_setting_m2_mid {

  unsigned int reg;
  unsigned int val;

};

/* From ./out/linux_3.10.0_64.VCS/mobile2/common/tmp/proj/verif_release_ro/
 * register_pkg/63.11/src/generators/gen_register_info/flows/golden_register_value.h */
struct __reg_setting_m2_mid emu_mobile2_mid_setting_cl135710[] = {
       /* 6.4.3*/
       { 0x00031104, 0x000C0100}, /* SPI_CONFIG_CNTL_1 */
       { 0x00030934, 0x00000001}, /* VGT_NUM_INSTANCES */
       { 0x00028350, 0x00000000}, /* PA_SC_RASTER_CONFIG */
       { 0x00037008, 0x00000000}, /* CB_PERFCOUNTER0_SELECT1 */

       /* 6.4.6*/
       { 0x0000317D, 0x00020000}, /* CP_DEBUG_2 */
};
#endif