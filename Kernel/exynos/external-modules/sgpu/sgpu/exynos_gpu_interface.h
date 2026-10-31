#include <linux/devfreq.h>
#include <linux/kernel.h>

#ifdef CONFIG_DRM_SGPU_EXYNOS

#include "soc/samsung/cal-if.h"
#include "soc/samsung/fvmap.h"

#include "amdgpu.h"
#include "sgpu_governor.h"

#define TABLE_MAX                      (200)
#define SYSBUSY_FREQ_THRESHOLD         (500000)
#define SYSBUSY_UTIL_THRESHOLD         (70)

int exynos_dvfs_preset(struct devfreq *df);
int exynos_dvfs_postclear(struct devfreq *df);

void gpu_dvfs_init_utilization_notifier_list(void);
int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb);
int gpu_dvfs_unregister_utilization_notifier(struct notifier_block *nb);
void gpu_dvfs_notify_utilization(void);

int gpu_dvfs_init_table(struct freq_volt *tb, unsigned long *freq_table, int states, int level);
int gpu_dvfs_get_step(void);
int *gpu_dvfs_get_freq_table(void);

int gpu_dvfs_get_clock(int level);
int gpu_dvfs_get_voltage(int clock);

int gpu_dvfs_get_max_freq(void);
inline int gpu_dvfs_get_max_locked_freq(void);
int gpu_dvfs_set_max_freq(unsigned long freq);

int gpu_dvfs_get_min_freq(void);
inline int gpu_dvfs_get_min_locked_freq(void);
int gpu_dvfs_set_min_freq(unsigned long freq);

int gpu_dvfs_get_cur_clock(void);

unsigned long gpu_dvfs_get_maxlock_freq(void);
int gpu_dvfs_set_maxlock_freq(unsigned long freq);

unsigned long gpu_dvfs_get_minlock_freq(void);
int gpu_dvfs_set_minlock_freq(unsigned long freq);

ktime_t gpu_dvfs_update_time_in_state(unsigned long freq);
ktime_t *gpu_dvfs_get_time_in_state(void);
ktime_t gpu_dvfs_get_tis_last_update(void);

ktime_t gpu_dvfs_get_gpu_queue_last_updated(void);

unsigned int gpu_dvfs_get_polling_interval(void);
int gpu_dvfs_set_polling_interval(unsigned int value);

char *gpu_dvfs_get_governor(void);
int gpu_dvfs_set_governor(char* str_governor);
void gpu_dvfs_set_autosuspend_delay(int delay);

int gpu_dvfs_set_disable_llc_way(int val);

int gpu_dvfs_get_utilization(void);

int gpu_tmu_notifier(int frequency);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
unsigned long gpu_afm_decrease_maxlock(unsigned int down_step);
unsigned long gpu_afm_release_maxlock(void);
void gpu_gvf_set_freq_lock(uint32_t freq);
void gpu_gvf_release_freq_lock(void);
#else
static inline void gpu_afm_decrease_maxlock(unsigned int down_step) { }
static inline void gpu_afm_release_maxlock(void) { }
#define gpu_gvf_set_freq_lock(freq)	do { } while (0)
#define gpu_gvf_release_freq_lock(void)	do { } while (0)
#endif /* CONFIG_EXYNOS_PM_QOS */

void sgpu_disable_llc_way(bool disable);
bool sgpu_is_llc_way_disabled(void);

void sgpu_set_gpu_mm_min_clock(struct amdgpu_device *adev, unsigned long freq, unsigned long delay);
static inline unsigned long sgpu_get_gpu_mm_min_clock(struct amdgpu_device *adev)
{
	return sgpu_governor_get_mm_min_clock(adev->devfreq->governor_data);
}

void sgpu_set_gpu_min_clock(struct amdgpu_device *adev, unsigned long freq);
unsigned long sgpu_get_gpu_min_clock(void);

void sgpu_set_gpu_max_clock(struct amdgpu_device *adev, unsigned long freq);
unsigned long sgpu_get_gpu_max_clock(void);

void sgpu_set_gpu_siop_max_clock(struct amdgpu_device *adev, unsigned long freq);
unsigned long sgpu_get_gpu_siop_max_clock(void);

static inline uint64_t sgpu_get_gpu_utilization(struct amdgpu_device *adev)
{
	struct devfreq *df = adev->devfreq;

	return (uint64_t)(df->last_status.busy_time * 100 / df->last_status.total_time);
}

static inline unsigned long sgpu_get_gpu_clock(struct amdgpu_device *adev)
{
	struct devfreq *df = adev->devfreq;
	unsigned long freq;

	if (df->profile->get_cur_freq && !df->profile->get_cur_freq(df->dev.parent, &freq))
		return freq;
	return df->previous_freq;
}

static inline int sgpu_get_gpu_freq_table(struct amdgpu_device *adev, unsigned long *freq_table[])
{
	struct devfreq *df = adev->devfreq;

	if (!df || !df->profile || !df->profile->freq_table)
		return -EINVAL;

	*freq_table = df->profile->freq_table;
	return df->profile->max_state;
}

unsigned long long gpu_dvfs_calc_utilization(struct devfreq *df);

int exynos_interface_init(struct devfreq *df);
int exynos_interface_deinit(struct devfreq *df);
#endif
