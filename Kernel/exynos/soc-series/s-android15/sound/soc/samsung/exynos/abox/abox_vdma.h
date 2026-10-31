/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ALSA SoC - Samsung Abox Virtual DMA driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_VDMA_H
#define __SND_SOC_ABOX_VDMA_H

#include <sound/pcm.h>

#include "abox_compress.h"
#define NAME_LENGTH SZ_32
#define VDMA_COUNT_MAX 40

struct abox_vdma_rtd {
	struct snd_dma_buffer buffer;
	struct snd_pcm_hardware hardware;
	struct snd_pcm_substream *substream;
	struct abox_ion_buf *ion_buf;
	struct snd_hwdep *hwdep;
	unsigned long iova;
	size_t pointer;
	bool iommu_mapped;
	bool dma_alloc;
};

struct abox_vdma_info {
	struct device *dev;
	int id;
	int stream;
	bool aaudio;
	char name[NAME_LENGTH];
	struct abox_vdma_rtd rtd[SNDRV_PCM_STREAM_LAST + 1];
	struct abox_compr_data compr_data;
};

/**
 * Register abox virtual component
 * @param[in]	dev		pointer to abox device
 * @param[in]	id		unique buffer id
 * @param[in]	name		name of the component
 * @param[in]	aaudio		whether the pcm supports aaudio
 * @param[in]	playback	playback capability
 * @param[in]	capture		capture capability
 * @return	device containing asoc component
 */
extern struct device *abox_vdma_register_component(struct device *dev,
		int id, const char *name, bool aaudio,
		struct snd_pcm_hardware *playback,
		struct snd_pcm_hardware *capture);

/**
 * Initialize abox vdma
 * @param[in]	dev_abox	pointer to abox device
 */
extern void abox_vdma_init(struct device *dev_abox);

extern struct abox_vdma_info *abox_vdma_get_info(int id);

#endif /* __SND_SOC_ABOX_VDMA_H */
