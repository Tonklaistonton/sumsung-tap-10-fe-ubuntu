#ifndef _GOVERNOR_LEALT_H
#define _GOVERNOR_LEALT_H

#include <linux/kernel.h>
#include <linux/devfreq.h>

enum common_ev_idx {
	INST_IDX,
	CYC_IDX,
	MISS_IDX,
	NUM_COMMON_EVS
};

struct core_dev_map {
	unsigned int core_mhz;
	unsigned int targetload;
};

enum lealt_llc_control {
	LEALT_LLC_CONTROL_OFF = 0,
	LEALT_LLC_CONTROL_NONE,
	LEALT_LLC_CONTROL_ON,
};

struct lealt_sample {
	unsigned int seq_no;
	s64 duration; /* us */

	int lat_cpu;
	unsigned long lat_freq;
	unsigned long lat_minlock;
	unsigned long lat_maxlock;
	unsigned long targetload;

	unsigned long mif_avg_freq;
	unsigned long mif_active_load;
	unsigned long mif_active_freq;
	unsigned long bw_maxlock;

	enum lealt_llc_control next_llc;
	unsigned long next_freq;
};

int lealt_mon_get_stability(int domain_id);
int lealt_mon_update_counts_all(void);
int lealt_mon_get_metrics(struct lealt_sample *sample);
#endif /* _GOVERNOR_LEALT_H */
