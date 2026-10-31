#include <sgpu-profiler.h>
#include <sgpu-profiler-governor.h>

static struct exynos_profiler_gpudev_fn *profiler_gpudev_fn;
static struct sgpu_profiler_governor_data window;

static uint64_t conv_utilization(struct devfreq *df, uint64_t utilization, uint64_t current_freq) {
	unsigned long cur_freq = current_freq;
	unsigned long max_freq = df->profile->freq_table[0];
	uint64_t weight_util = 0;
	uint64_t norm_util = 0;
    uint64_t conv_util = 0;
	uint32_t cur_idx = 0;
    uint64_t table_col = WINDOW_MAX_SIZE - 1;
	uint32_t i;

	norm_util = ((utilization * cur_freq) << NORMALIZE_SHIFT) / max_freq;

	cur_idx = window.cur_idx;
	window.cur_idx = (cur_idx + 1) % WINDOW_MAX_SIZE;
	window.item[cur_idx] = norm_util;

    for(i = cur_idx + 1; i <= cur_idx + WINDOW_MAX_SIZE; i++) {
        weight_util += window.item[i % WINDOW_MAX_SIZE] *
                weight_table[table_col--];
    }
    weight_util /= weight_table[WINDOW_MAX_SIZE];

	conv_util = weight_util * max_freq / cur_freq;

	return conv_util;
}

int sgpu_profiler_governor_get_target(struct devfreq *df, uint32_t *level) {
	unsigned long cur_freq = df->profile->freq_table[*level];
	unsigned long target_freq;
    uint64_t gpu_util, conv_util, norm_util;

	if (profiler_gpudev_fn->calc_utilization) {
        gpu_util = profiler_gpudev_fn->calc_utilization(df);
    } else {
        gpu_util = 60;
    }
	conv_util = conv_utilization(df, gpu_util, cur_freq);
	norm_util = ((conv_util) * cur_freq / 100 ) >> NORMALIZE_SHIFT;

	target_freq = profiler_gpudev_fn->pb_get_gpu_target_freq(cur_freq, gpu_util, norm_util);

	if (target_freq > cur_freq) {
		while (df->profile->freq_table[*level] < target_freq && *level > 0)
			(*level)--;
	} else {
		while (df->profile->freq_table[*level] > target_freq &&
		      *level < df->profile->max_state - 1)
			(*level)++;
		if (df->profile->freq_table[*level] < target_freq)
			(*level)--;
	}

	profiler_gpudev_fn->pb_set_cur_freq(PROFILER_GPU, df->profile->freq_table[*level]);
	return 0;
}
EXPORT_SYMBOL(sgpu_profiler_governor_get_target);

int sgpu_profiler_governor_clear(struct devfreq *df, uint32_t level) {
	return 0;
}
EXPORT_SYMBOL(sgpu_profiler_governor_clear);

char *sgpu_profiler_governor_get_name(void) {
    return profiler_governor_name;
}
EXPORT_SYMBOL(sgpu_profiler_governor_get_name);

void sgpu_profiler_governor_init(struct exynos_profiler_gpudev_fn *fn) {
    profiler_gpudev_fn = fn;
}
EXPORT_SYMBOL(sgpu_profiler_governor_init);
MODULE_LICENSE("GPL");