#ifndef _TSMUX_EXYNOS_TEST_H
#define _TSMUX_EXYNOS_TEST_H

bool is_otf_job_done(struct tsmux_context *ctx);

void tsmux_handle_otf_job_done(struct tsmux_device *tsmux_dev, int job_id);

bool tsmux_handle_otf_partial_done(struct tsmux_device *tsmux_dev, int job_id);

bool tsmux_set_global_tsmux_dev(struct tsmux_device *tsmux_dev);

int tsmux_packetize(struct packetizing_param *param);

#endif /* _TSMUX_EXYNOS_TEST_H */
