#ifndef __PLATFORM_MIF_DCXO_TUNE__
#define __PLATFORM_MIF_DCXO_TUNE__

#include "platform_mif_memory_api.h"
//#include "platform_mif_regmap_api.h"
#include "platform_mif_irq_api.h"
#include "../mif_reg.h"

#define OP_GET_TUNE (0x4)
#define OP_SET_TUNE (0x5)
#define SHIFT_OPCODE (12)
#define MASK_OPCODE (0xF)

#define SHIFT_SEQ (16)
#define MASK_SEQ (0x3F)

#define MASK_DONE (0x2000)
#define SHIFT_DONE (13)

#define APM_CMD_MAX_SEQ_NUM (64)

// for opcode OP_GET_TUNE or OP_SET_TUNE
#define BUILD_ISSR0_VALUE(opcode, seq) 	(((opcode & MASK_OPCODE) << SHIFT_OPCODE) | ((seq & MASK_SEQ) << SHIFT_SEQ))
#define APM_IRQ_BIT_DCXO_SHIFT (1)

int platform_mif_dcxo_tune_init(struct platform_device *pdev, struct scsc_mif_abs *interface);

#endif
