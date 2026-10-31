/*
 * ncp.h
 *
 *  Created on: 2017. 7. 18.
 *      Author: kilyeon.im
 */

#ifndef NCP_H_
#define NCP_H_

/* please define these types if you don't have
 * typedef signed char s8;
 * typedef signed short s16;
 * typedef signed int s32;
 * typedef unsigned char u8;
 * typedef unsigned short u16;
 * typedef unsigned int u32;
 * typedef unsigned long ulong;
 * typedef unsigned long long u64;
 */

#define NCP_VERSION         28
#define NCP_MAGIC1          0x0C0FFEE0
#define NCP_MAGIC2          0xC0DEC0DE
#define NCP_MODEL_NAME_LEN  64
#define NCP_KERNEL_NAME_LEN 64
#define NCP_ADJACENT_LEN    6
#define NCP_CMDQ_CNT        2

/**
 * @brief an element of address vector table
 */
struct address_vector {
    /**
     * @brief index in address vector table
     * @details producer : compiler
     * @n consumer : driver, firmware
     * @n description : this index is required for partial update of address vector(0 ~ n)
     */
    u32 index;
    /**
     * @brief master address (KITT side)
     * @details producer : compiler, driver
     * @n consumer : driver, firmware
     * @n description : device virtual address or offset.
     * this address can point feature map, weight, golden etc.
     * if offset is provided by compiler in case of weight, feature map and golden,
     * driver should replace this offset with a device virtual address.
     * an offset value means offset from start of ncp body.
     */
    u32 m_addr;
    /**
     * @brief slave address (TURING side)
     * @details producer : driver
     * @n consumer : firmware
     * @n description :
     */
    u32 s_addr;
    /**
     * @brief size in byte
     * @details producer : compiler
     * @n consumer : driver
     * @n description : io feature map and intermediate feature map should have not zero size
     */
    u32 size;
};

enum ncp_memory_type {
    MEMORY_TYPE_IN_FMAP,    /* input feature map */
    MEMORY_TYPE_OT_FMAP,    /* output feature map */
    MEMORY_TYPE_IM_FMAP,    /* local intermediate feature map */
    MEMORY_TYPE_OT_PIX0,
    MEMORY_TYPE_OT_PIX1,
    MEMORY_TYPE_OT_PIX2,
    MEMORY_TYPE_OT_PIX3,
    MEMORY_TYPE_CUCODE,
    MEMORY_TYPE_WEIGHT,
    MEMORY_TYPE_CDATA,      /* const data for npu*/
    MEMORY_TYPE_RODATA,     /* read-only data for all*/
    MEMORY_TYPE_BYPASS,
    MEMORY_TYPE_GOLDEN,
    MEMORY_TYPE_GLOBAL_IM,  /* global intermediate feature map */
    MEMORY_TYPE_MAX
};

enum ncp_memory_pixel_format {
    MEMORY_PIXEL_FORMAT_INVALID = 0x0,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M = 0x105,  /* HAL_PIXEL_FORMAT_YCbCr_420_SP */
     /* 10-bit format (2 fd, 10bit, 2x byte) custom formats */
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M = 0x127,
     /* SBWC format */
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC = 0x130,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC  = 0x131,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC = 0x132,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC  = 0x133,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_SBWC = 0x134,
    /* SBWC Lossy formats */
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L50 = 0x140,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L75 = 0x141,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L50  = 0x150,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L75  = 0x151,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L40 = 0x160,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L60 = 0x161,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L80 = 0x162,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L40  = 0x170,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L60  = 0x171,
    MEMORY_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L80  = 0x172
};

/**
 * @brief an element of memory vector table
 */
struct memory_vector {
    /**
     * @brief memory type
     * @details producer : compiler
     * @n consumer : all
     * @n description : feature map, weight, weight mask, lut, ncp body. bias is included to ncp body.
     */
    u32 type;
    /**
     * @brief pixsel format
     * @details producer : compiler
     * @n consumer : driver
     * @n description : required pixel format (FOUR CC)
     */
    u32 pixel_format;
    /**
     * @brief pixel width
     * @details producer : compiler
     * @n consumer : all
     * @n description :
     */
    u32 width;
    /**
     * @brief pixel height
     * @details producer : compiler
     * @n consumer : all
     * @n description :
     */
    u32 height;
    /**
     * @brief channel count
     * @details producer : compiler
     * @n consumer : all
     * @n description :
     */
    u32 channels;
    /**
     * @brief width stride
     * @details producer : compiler
     * @n consumer : all
     * @n description : this stride value can be input or output stride
     */
    u32 wstride;
    /**
     * @brief channel stride
     * @details producer : compiler
     * @n consumer : all
     * @n description : this stride value can be input or output stride
     */
    u32 cstride;
    /**
     * @brief an index of io map table
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : an index pointing the place(abi) to be updated
     */
    u32 iomap_index;
    /**
     * @brief buffer id
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : a unique id to identify each buffers in a NNC
     */
    u32 buffer_id;
    /**
     * @brief an adjacent matrix for relationship between memories and executables
     */
    struct {
        /**
         * @brief executable ID
         * @details producer : compiler
         * @n consumer : firmware
         * @n description : an executable ID adjacent to the memory
         * 0 value is an empty slot
         */
        u32 executable_id;
        /**
         * @brief device ID
         * @details producer : driver
         * @n consumer : firmware
         * @n description : a device ID adjacent to the memory
         * 0 value is an empty slot
         */
        u32 device_id;
    } adjacent_matrix[NCP_ADJACENT_LEN];
    /**
     * @brief address vector index
     * @details producer : framework
     * @n consumer : firmware
     * @n description : index of array of struct address_vector
     */
    u32 address_vector_index;
    /**
     * @brief reserved
     * @details producer : TBD
     * @n consumer : TBD
     * @n description : TBD
     */
    u32 reserved[4];
};

/**
 * @brief an element of power eatimation vector table
 */
struct pwr_est_vector {
    /**
     * @brief power estimation vector index
     * @details producer : compiler
     * @n consumer : driver
     * @n description : index of a power estimation vector array
     */
    u32 index;
    /**
     * @brief layer id
     * @details producer : compiler
     * @n consumer : compiler
     * @n description : an internal layer id, which is generated in compiler for tracking
     */
    s32 layer_id;
    /**
     * @brief computational quantity
     * @details producer : compiler
     * @n consumer : driver
     * @n description : the amount of required mac by kilo op unit
     */
    u32 computational_quantity;
    /**
     * @brief io transfer size
     * @details producer : compiler
     * @n consumer : driver
     * @n description : io transfer size by kilo byte unit
     */
    u32 io_transfer_size;
};

/**
 * @brief an element of interruption vector table
 */
struct interruption_vector {
    /**
     * @brief index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : 0 <= index <= n
     */
    u32 index;
    /**
     * @brief the offset of cmdq isa
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : isa address =  body + isa offset
     */
    u32 isa_offset;
    /**
     * @brief the size of cmdq isa
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : the leftover cmdq isa size to be processed
     */
    u32 isa_size;
    /**
     * @brief the offset of weight
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : weight address =  body + weight offset
     */
    u32 weight_offset;
    /**
     * @brief the size of weight
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : the leftover const size to be processed
     */
    u32 weight_size;
    /**
     * @brief the offset of const data
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : const address =  body + const offset
     */
    u32 const_offset;
    /**
     * @brief the size of const data size
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : the leftover const size to be processed
     */
    u32 const_size;
};

enum group_status {
    GROUP_STATUS_STANDBY, /* not started yet */
    GROUP_STATUS_START, /* right before start */
    GROUP_STATUS_STOP, /* stopped on work */
    GROUP_STATUE_DONE, /* process done */
    GROUP_STATUS_END
};

enum group_flags {
    GROUP_FLAG_SKIP, /* this group should be skipped to process */
    GROUP_FLAG_NOTIFY, /* notify flag look forward to notifying a completion of group process by status update */
    GROUP_FLAG_COMPARE, /* this flag look forward to comparing between output and golden */
    GROUP_FLAG_LAST, /* last flag means last group in chunk it's contained */
    GROUP_FLAG_WFS, /* group have to wait for standby status(WFS) */
    GROUP_FLAG_END
};

/**
 * @brief an element of group vector table
 */
struct group_vector {
    /**
     * @brief group index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this index should be monotonously increased by one
     * range : 0 <= index < ncp_header->group_vector_cnt
     */
    u32 index;
    /**
     * @brief the status of group
     * @details producer : firmware
     * @n consumer : driver
     * @n description : this shows progress of group
     */
    u32 status;
    /**
     * @brief flags
     * @details producer : all
     * @n consumer : firmware
     * @n description : bit flag can be orred, each bit corresponds to each flag.
     */
    u32 flags;
    /**
     * @brief start sync id
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this field will be effective on multiple groups for interop
     */
    u32 interop_str_sync_id;
    /**
     * @brief end sync id
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this field will be effective on multiple groups for interop
     */
    u32 interop_end_sync_id;
    /**
     * @brief the offset of intrinsic
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : instrinsic address =  body offset + intrinsic offset
     */
    u32 intrinsic_offset;
    /**
     * @brief the size of intrinsic set
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : in byte
     */
    u32 intrinsic_size;
    /**
     * @brief the offset of ISA
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : isa address =  body offset + isa offset
     */
    u32 isa_offset[NCP_CMDQ_CNT];
    /**
     * @brief the size of command set
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : in byte
     */
    u32 isa_size[NCP_CMDQ_CNT];
    /**
     * @brief reserved for flow control
     * @details producer : TBD
     * @n consumer : TBD
     * @n description : TBD
     */
    u32 reserved[4];
};

/**
 * @brief an element of thread vector table
 */
struct thread_vector {
    /**
     * @brief vector index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this index should be monotonously increased by one
     * range : 0 <= index < ncp_header->thread_vector_cnt
     */
    u32 index;
    /**
     * @brief workload
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this value is a propotion(0 ~ 100) that a thread occupies on the total workload
     */
    u32 workload;
    /**
     * @brief group start index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this points the start index of group vector
     * range : 0 <= index < ncp_header-group_vector_cnt
     */
    u32 group_str_idx;
    /**
     * @brief group end index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this points the end index of group vector
     * range : 0 <= index < ncp_header->group_vector_cnt
     */
    u32 group_end_idx;
    /**
     * @brief interruption start index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : 0 <= index < ncp_header->interruption_vector_cnt
     */
    u32 interruption_str_idx;
    /**
     * @brief rq interruption end index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : 0 <= index < ncp_header->interruption_vector_cnt
     */
    u32 interruption_end_idx;
};

struct llc_vector {
    /**
     * @brief cacheable buffer size
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : cacheable buffer size(C) in MB
     */
    u32 cacheable_buffer_size;
    /**
     * @brief remaining dram io
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : dram io with C cacheable buffer(D)
     */
    u32 remaining_dram_io;
    /**
     * @brief remaining dram io ratio
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : % = (D * 100) / total dram io
     */
    u32 remaining_dram_io_ratio;
    /**
     * @brief io ratio per port
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : each port 8 bits is a ratio value by in PERCENTAGE
     * bit[31:24] : bit[23:16] : bit[15:8] : bit[7:0] = PORT3 : PORT2 : PORT1 : PORT0
     */
    u32 per_port_io_ratio;
};

enum ncp_kernel_type {
    KERNEL_ARM,
    KERNEL_DSP,
    KERNEL_TYPE_MAX,
};

struct descriptor_vector {
    /**
     * @brief kernel type
     * @details producer : compiler
     * @n consumer : all
     * @n description : identification value for distinguishing the descriptor that fits the target
     */
    u32 type;
    /**
     * @brief the offset of kernel descriptor
     * @details producer : compiler
     * @n consumer : all
     * @n description : descriptor address =  body + offset
     */
    u32 offset;
    /**
     * @brief size in byte
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : size of kernel descriptor
     */
    u32 size;
    /**
     * @brief kernel start index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this points the start index of kernel vector
     * range : 0 <= index < ncp_header-kernel_vector_cnt
     */
    u32 kenrel_str_idx;
    /**
     * @brief kernel end index
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : this points the end index of kernel vector
     * range : 0 <= index < ncp_header-kernel_vector_cnt
     */
    u32 kenrel_end_idx;
};

struct kernel_vector {
    /**
     * @brief the offset of embedded elf
     * @details producer : compiler
     * @n consumer : all
     * @n description : kernel(elf) address = body + offset
     */
    u32 offset;
    /**
     * @brief size in byte
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : size of elf binary
     */
    u32 size;
    /**
     * @brief kernel name
     * @details producer : compiler
     * @n consumer : all
     * @n description : kernel name in a string
     */
    s8 kernel_string[NCP_KERNEL_NAME_LEN];
};

enum ncp_vector_type {
    NCP_VECTOR_TYPE_ADDRESS = 0,
    NCP_VECTOR_TYPE_MEMORY,
    NCP_VECTOR_TYPE_GROUP,
    NCP_VECTOR_TYPE_THREAD,
    NCP_VECTOR_TYPE_INTERRUPTION,
    NCP_VECTOR_TYPE_DESCRIPTOR,
    NCP_VECTOR_TYPE_KERNEL,
    NCP_VECTOR_TYPE_PWREST,
    NCP_VECTOR_TYPE_LLC,
    NCP_VECTOR_TYPE_SPARE1,
    NCP_VECTOR_TYPE_SPARE2,
    NCP_VECTOR_TYPE_SPARE3,
    NCP_VECTOR_TYPE_SPARE4,
    NCP_VECTOR_TYPE_SPARE5,
    NCP_VECTOR_TYPE_MAX
};

enum ncp_operation_group {
    INT4    = 0x01,
    INT8    = 0x02,
    INT16   = 0x04,
    INT32   = 0x08,
    FP8EXP4 = 0x10,
    FP8EXP5 = 0x20,
    FP16    = 0x40,
    FP32    = 0x80,
};

/**
 * @brief ncp header to describe image structure
 */
struct ncp_header {
    /**
     * @brief magic number in the top of NCP
     * @details producer : compiler
     * @n consumer : all
     * @n description : for detecting memory invasion
     */
    u32 magic_number1;
    /**
     * @brief ncp header version
     * @details producer : compiler
     * @n consumer : all
     * @n description : a deicmal version number
     */
    u32 hdr_version;
    /**
     * @brief ncp header size
     * @details producer : compiler
     * @n consumer : all
     * @n description : a total ncp header size in byte includes
     * sizeof(ncp_header) + a total size of vector A + B + ...
     * NCP header and every vector should be concatenated without padding
     */
    u32 hdr_size;
    /**
     * @brief intrinsic version
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : a specific intrinsic version is required for the NCP
     */
    u32 intrinsic_version;
    /**
     * @brief application binary interface version
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : a specific abi version is required for the NCP
     */
    u32 abi_version;
    /**
     * @brief model name
     * @details producer : compiler
     * @n consumer : all
     * @n description : model name in a string for debugging
     */
    s8 model_name[NCP_MODEL_NAME_LEN];
    /**
     * @brief nnc id
     * @details producer : framework + driver
     * @n consumer : firmware
     * @n description : nnc_id is
     * [31:16] unique ID
     * [15:00] Subalgorithm ID
     */
    u32 nnc_id;
    /**
     * @brief executable id
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : executable_id is
     * [31:24] Model ID
     * [23:08] Submodel ID
     * [07:00] Executable ID
     */
    u32 executable_id;
    /**
     * @brief device class id
     * @details producer : framework + driver + compiler
     * @n consumer : firmware
     * @n description : device_id is
     * [19:16] device class ID (NP/GPU/HunterAE)
     * [15:12] AI cluster ID
     * [11:08] AI core ID
     * [07:04] Engine class ID (NPU/DSP/CA53)
     * [03:00] Engine ID
     */
    u32 device_id;
    /**
     * @brief priority
     * @details producer : framework + compiler
     * @n consumer : firmware
     * @n description : a priority value is in range of 0 ~ 255
     * [0x80:0xFF] High priority
     * [0x7F:0x40] Mid priority
     * [0x3F:0x00] Low priority
     */
    u32 priority;
    /**
     * @brief deadline
     * @details producer : framework + compiler
     * @n consumer : driver, firmware
     * @n description : deadline in micro second
     */
    u32 deadline;
    /**
     * @brief flags
     * @details producer : none
     * @n consumer : all
     * @n description : reserved for the future
     */
    u32 flags;
    /**
     * @brief workload
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : computational complexity of operation in cycle
     */
    u32 workload;
    /**
     * @brief computational workload
     * @details producer : compiler
     * @n consumer : driver and firmware
     * @n description : total computational workload in kilo op
     */
    u32 computational_workload;
    /**
     * @brief io workload
     * @details producer : compiler
     * @n consumer : driver and firmware
     * @n description : total sdma transfer size in kilo byte
     */
    u32 io_workload;
    /**
     * @brief operation group;
     * @details producer : compiler
     * @n consumer : driver and firmware
     * @n description : type group of featuremaps used by the model.
     */
    u32 operation_group;
    /**
     * @brief ncp body offset
     * @details producer : compiler
     * @n consumer : all
     * @n description : body offset in byte from header(zero base)
     */
    u32 body_offset;
    /**
     * @brief ncp body size
     * @details producer : compiler
     * @n consumer : all
     * @n description : ncp body size in byte
     */
    u32 body_size;
    /**
     * @brief reserved field
     * @details producer : compiler
     * @n consumer : all
     * @n description : backward compatibility for the future
     */
    u32 max_frequency;
    /**
     * @brief reserved field
     * @details producer : compiler
     * @n consumer : all
     * @n description : backward compatibility for the future
     */
    u32 gradual;
    /**
     * @brief reserved field
     * @details producer : compiler
     * @n consumer : all
     * @n description : backward compatibility for the future
     */
    u32 early_response;
    /**
     * @brief max frequency
     * @details producer : compiler
     * @n consumer : all
     * @n description : flag whether to operate with maximum frequency or not
     */
    u32 reserved[60];
    /**
     * @brief vector list to describe vector location info
     */
    struct {
        /**
         * @brief vector version
         * @details producer : compiler
         * @n consumer : driver, firmware
         * @n description : incremental version number
         * NCP header version should increase everytime vector versions increase
         */
        u32 version;
        /**
         * @brief vector offset
         * @details producer : compiler
         * @n consumer : all
         * @n description : vector offset in byte from header(zero base)
         */
        u32 offset;
        /**
         * @brief the number of vectors
         * @details producer : compiler
         * @n consumer : all
         * @n description : n
         */
        u32 cnt;
    } vector_list[NCP_VECTOR_TYPE_MAX];
    /**
     * @brief magic number
     * @details producer : compiler
     * @n consumer : firmware
     * @n description : required for header version matching
     */
    u32 magic_number2;
};

#define NCP_SET_(field, ptr, value) ((struct ncp_header *)ptr)->field = value
#define NCP_GET_(field, ptr) ((struct ncp_header *)ptr)->field
#define NCP_GET_OFFSET_(field) &((struct ncp_header *)0)->field

#define NCP_SET_VECTOR_OFFSET(type, ptr, value) ((struct ncp_header *)ptr)->vector_list[type].offset = value
#define NCP_SET_VECTOR_COUNT(type, ptr, value) ((struct ncp_header *)ptr)->vector_list[type].cnt = value
#define NCP_GET_VECTOR_OFFSET(type, ptr) ((struct ncp_header *)ptr)->vector_list[type].offset
#define NCP_GET_VECTOR_COUNT(type, ptr) ((struct ncp_header *)ptr)->vector_list[type].cnt


#define NCP_SET_ISA_OFFSET(core_id, ptr, value) ((struct group_vector *)ptr)->isa_offset[core_id] = value
#define NCP_SET_ISA_SIZE(core_id, ptr, value) ((struct group_vector *)ptr)->isa_size[core_id] = value
#define NCP_GET_ISA_OFFSET(core_id, ptr) ((struct group_vector *)ptr)->isa_offset[core_id]
#define NCP_GET_ISA_SIZE(core_id, ptr) ((struct group_vector *)ptr)->isa_size[core_id]

#define NCP_GET_FENCE_FLAG(ptr) (0x2)
#endif /* NCP_H_ */
