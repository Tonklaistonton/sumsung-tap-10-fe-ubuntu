#include "exynos-mpam.h"
#include "exynos-mpam-internal.h"

#define SMC_DEFAULT_PARTID_COUNT 32

#define NUM_OF_MAX_BLOCK 10

#define SPARE		(0x64)
#define PARTIDSEL_0	(0xF53C)
#define PARTIDSEL_1	(PARTIDSEL_0 + SPARE)

#define CTRL8_0 (0xF538)
#define CTRL8_1 (CTRL8_0 + SPARE)
#define CTRL9_0 (PARTIDSEL_0)
#define CTRL9_1 (CTRL9_0 + SPARE)

#define MPAMPRI GENMASK(2, 1)

struct smc_part_kobj {
	struct msc_smc_domain *msc_smc;
	unsigned int partid;
	struct kobject kobj;
	int request;
};

struct msc_smc_domain {
	struct msc_domain msc;
	struct kobject ko_root;
	struct kobject ko_part_dir;
	struct kobject ko_entry_dir;
	struct smc_part_kobj *ko_parts;

	void __iomem *base[NUM_OF_MAX_BLOCK];
	u32 base_addr[NUM_OF_MAX_BLOCK];
	u32 base_count;
};
