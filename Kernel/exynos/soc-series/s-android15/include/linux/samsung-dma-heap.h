/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung dma heap header
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd.
 */

#ifndef _LINUX_SAMSUNG_DMA_HEAP_H_
#define _LINUX_SAMSUNG_DMA_HEAP_H_

void secure_heap_prefetch_enabled(const char *name);
void secure_heap_prefetch_disabled(const char *name);

#endif /* _LINUX_SAMSUNG_DMA_HEAP_H_ */
