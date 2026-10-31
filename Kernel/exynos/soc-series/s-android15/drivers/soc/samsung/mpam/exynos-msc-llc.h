#include "exynos-mpam.h"
#include "exynos-mpam-internal.h"

struct llc_request_kobj {
	struct msc_llc_domain *msc_llc;
	struct kobject kobj;
	unsigned int prio;
	unsigned int alloc_size;
	unsigned int partid;
	unsigned int enabled;
	bool init;
};

struct msc_llc_domain {
	struct msc_domain msc;
	u32 cpbm_nbits;
	u32 llc_reqeust_count;

	struct kobject ko_root;
	struct llc_request_kobj *ko_llc_request;
	unsigned int llc_request_count;
};

extern int llc_mpam_alloc(unsigned int index, int size, int partid, int pmon_gr, int ns, int on);
