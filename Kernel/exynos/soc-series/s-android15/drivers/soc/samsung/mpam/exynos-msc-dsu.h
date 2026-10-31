#include "exynos-mpam.h"
#include "exynos-mpam-internal.h"

struct msc_part_kobj {
	struct msc_dsu_domain *msc_dsu;
	unsigned int partid;
	struct kobject kobj;
	unsigned long *cpbm;
};

struct msc_dsu_domain {
	struct msc_domain msc;
	u32 restore_partid_count;
	int has_ris;
	union {
		struct {
			bool has_cpor;
			bool has_ccap;
		};
		u8 part_feats;
	};
	u32 cpbm_nbits;
	u32 cmax_nbits;
	u32 cmax_shift;

	struct kobject		ko_root;
	struct kobject		ko_part_dir;
	struct kobject		ko_entry_dir;
	struct msc_part_kobj	*ko_parts;
};
