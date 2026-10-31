// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/io.h>
#include <linux/slab.h>

#include "npu-hw-device.h"
#include "npu-log.h"

#include "dsp-util.h"

#ifndef CONFIG_NPU_KUNIT_TEST
void dsp_util_bitmap_dump(struct dsp_util_bitmap *map)
{
	npu_notice("bitmap[%s] dump : size(%u)/used_size(%u)/base_bit(%u)\n",
			map->name, map->bitmap_size, map->used_size, map->base_bit);
	print_hex_dump(KERN_NOTICE, "[Exynos][DSP][NOTICE]: bitmap raw: ",
			DUMP_PREFIX_NONE, 32, 8, map->bitmap,
			BITS_TO_LONGS(map->bitmap_size) * sizeof(long), false);
}
#endif

int dsp_util_bitmap_set_region(struct dsp_util_bitmap *map, unsigned int size)
{
	int ret;
	unsigned long start, end, check;
	bool turn = false;

	if (!size) {
		ret = -EINVAL;
		npu_err("Invalid bitmap size[%s](%u)\n", map->name, size);
		goto p_err;
	}

	if (size > map->bitmap_size - map->used_size) {
		ret = -ENOMEM;
		npu_err("Not enough bitmap[%s](%u)\n", map->name, size);
		goto p_err;
	}

	start = map->base_bit;
again:
	start = find_next_zero_bit(map->bitmap, map->bitmap_size, start);

	end = start + size - 1;
	if (end >= map->bitmap_size) {
		if (turn) {
			ret = -ENOMEM;
			npu_err("Not enough contiguous bitmap[%s](%u)\n",
					map->name, size);
			goto p_err;
		} else {
			turn = true;
			start = 0;
			goto again;
		}
	}

	check = find_next_bit(map->bitmap, end, start);
	if (check < end) {
		start = check + 1;
		goto again;
	}

	bitmap_set(map->bitmap, start, size);
	map->base_bit = end + 1;
	map->used_size += size;

	return start;
p_err:
	dsp_util_bitmap_dump(map);
	return ret;
}

void dsp_util_bitmap_clear_region(struct dsp_util_bitmap *map,
		unsigned int start, unsigned int size)
{
	if ((map->bitmap_size < start + size - 1) ||
			size > map->used_size) {
		npu_warn("Invalid clear parameter[%s](%u/%u)\n",
				map->name, start, size);
		dsp_util_bitmap_dump(map);
		return;
	}

	map->used_size -= size;
	bitmap_clear(map->bitmap, start, size);
}

int dsp_util_bitmap_init(struct dsp_util_bitmap *map, const char *name,
		unsigned int size)
{
	int ret;

	if (!size) {
		ret = -EINVAL;
		dsp_err("bitmap size can not be zero\n");
		goto p_err;
	}

	map->bitmap = kzalloc(BITS_TO_LONGS(size) * sizeof(long), GFP_KERNEL);
	if (!map->bitmap) {
		ret = -ENOMEM;
		npu_err("Failed to init bitmap(%u/%lu)\n",
				size, BITS_TO_LONGS(size) * sizeof(long));
		goto p_err;
	}

	snprintf(map->name, DSP_BITMAP_NAME_LEN, "%s", name);
	map->bitmap_size = size;
	map->used_size = 0;
	map->base_bit = 0;

	return 0;
p_err:
	return ret;
}

void dsp_util_bitmap_deinit(struct dsp_util_bitmap *map)
{
	kfree(map->bitmap);
}
