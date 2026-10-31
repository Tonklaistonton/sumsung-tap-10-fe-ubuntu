#ifndef __UNPU_H__
#define __UNPU_H__

#include <linux/debugfs.h>
#include "include/npu-binary.h"

#define UNPU_CLIENT_INTERFACE_MAX_REGS (40)
#define MICRO_MBOX_SIZE              (8)
#define UNPU_MAX_IRQ 1

enum unpu_state {
	UNPU_STATE_INIT = 0x0,
	UNPU_STATE_READY = 0xA,
	UNPU_STATE_RUN = 0xB,
	UNPU_STATE_DONE = 0xC,
	UNPU_STATE_DEAD = 0xDEAD
};

enum unpu_request {
	UNPU_REQ_INIT = 0x0,
	UNPU_REQ_LOAD = 0x1,
	UNPU_REQ_PROCESS = 0x2,
	UNPU_REQ_DEINIT = 0x3,
	/* to clear flag_npu_done */
	UNPU_REQ_CLEAR_DONE = 0x4,
	UNPU_REQ_TEST = 0x5
};

enum unpu_response {
	UNPU_RES_INIT = 0x10,
	UNPU_RES_LOAD = 0x20,
	UNPU_RES_PROCESS = 0x30,
	UNPU_RES_DEINIT = 0x40,
	UNPU_RES_TEST = 0x50,
	UNPU_RES_ERR = 0xBAD
};

struct unpu_mbox_interface {
	volatile u32 intr;
	volatile uint32_t verstat;
	volatile uint32_t npu_signature;
	volatile uint32_t client_interface_ptr;
	volatile uint32_t client_signature;
};

struct unpu_client_interface {
	volatile uint32_t npu_state;
	volatile uint32_t flag_npu_run;
	volatile uint32_t flag_npu_done;
	volatile uint32_t client_request;
	volatile uint32_t npu_response;
	volatile uint32_t data[UNPU_CLIENT_INTERFACE_MAX_REGS];
	volatile uint32_t magic;
};

enum unpu_iomem_names {
	UNPU_CTRL,
	YAMIN_CTRL,
	UNPU_SRAM,
	NPUMEM_SRAM,
	NPUMEM_CTRL_S,
	UNPU_BAAW,
	UNPU_MAX_IOMEM,
};

struct unpu_iomem_data {
	void __iomem *vaddr;
	phys_addr_t paddr;
	resource_size_t	size;
};

struct unpu_rmem_data {
	const char	*name;
	phys_addr_t paddr;
	void __iomem *vaddr;
	struct reserved_mem *rmem;
	resource_size_t	size;
};

struct unpu_debug {
	struct dentry *dfile_root;
	unsigned long  state;
};

struct unpu_device
{
	u32 pmu_offset;
	int irq_num;
	int irq[UNPU_MAX_IRQ];
	struct device *dev;
	struct platform_device *pdev;
	struct unpu_iomem_data iomem_data[UNPU_MAX_IOMEM];
	struct unpu_rmem_data rmem_data;
	struct unpu_debug debug;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	struct imgloader_desc imgloader;
#endif
#if IS_ENABLED(CONFIG_UNPU_POWER_NOTIFIER)
	struct notifier_block noti_data_for_unpu;
#endif
	wait_queue_head_t done_wq;
};

int __init unpu_register(void);
int unpu_ncp_test(void);
int unpu_nw_request_test(u32 param);
#endif
