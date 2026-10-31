// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-debug.h"
#include "pablo-json.h"
#include "is-core.h"
#include "pablo-kernel-variant.h"

#define SUPPORT_PAGE_NUM_MAX 3
#define STREAM_UNIT_BUF_SIZE (PAGE_SIZE >> 1)
#define WRITABLE_PAGE_SIZE (PAGE_SIZE - 1)

enum pablo_stream_info_mode {
	PABLO_STREAM_INFO_MODE_DEFAULT,
	PABLO_STREAM_INFO_MODE_STREAM = PABLO_STREAM_INFO_MODE_DEFAULT,
	PABLO_STREAM_INFO_MODE_IP,
	PABLO_STREAM_INFO_MODE_GET_REMAINED,
	PABLO_STREAM_INFO_MODE_MAX,
};

enum pablo_stream_edge_io {
	PABLO_STREAM_EDGE_IO_OTF,
	PABLO_STREAM_EDGE_IO_M2M,
};

struct pablo_stream_edge {
	bool has;
	enum pablo_stream_edge_io io;
	u32 connected_node;
	u32 width;
	u32 height;
};

struct pablo_stream_node {
	u32 id;
	const char *name;
	struct pablo_stream_edge in;
	struct pablo_stream_edge out;
};

struct pablo_stream {
	u32 id;
	const char *name;
	const char *sensor;
	u32 node_count;
	struct pablo_stream_node node[];
};

struct pablo_stream_sysfs_param {
	enum pablo_stream_info_mode mode;
	u32 stream_id;
	u32 group_id;
	int total_size;
	int remained_size;
};

static const char *pablo_stream_name_repro = "REPROCESSING";
static const char *pablo_stream_name_multich = "MULTI CHANNEL";
static const char *pablo_stream_name_preview = "PREVIEW";

static const char *const pablo_stream_edge_io_names[] = {
	"OTF",
	"M2M",
};

static struct pablo_stream_sysfs_param pablo_stream_info_param;
static char *internal_buf;

static int pablo_stream_info_set(const char *val, const struct kernel_param *kp);
static int pablo_stream_info_get(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_stream_info_param_ops = {
	.set = pablo_stream_info_set,
	.get = pablo_stream_info_get,
};
module_param_cb(debug_stream_info, &pablo_stream_info_param_ops, &pablo_stream_info_param, 0644);

static char *make_node_info_json(struct pablo_stream_edge *inout, char *buffer, size_t *rem)
{
	char *p;

	p = pablo_json_object_open(buffer, NULL, rem);
	p = pablo_json_nstr(p, "io", pablo_stream_edge_io_names[inout->io],
		strlen(pablo_stream_edge_io_names[inout->io]), rem);
	p = pablo_json_uint(p, "width", inout->width, rem);
	p = pablo_json_uint(p, "height", inout->height, rem);
	p = pablo_json_uint(p, "connected_node", inout->connected_node, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}

static char *make_stream_info_json(struct pablo_stream *stream, char *buffer, size_t *rem)
{
	int i;
	char *p;

	p = pablo_json_object_open(buffer, NULL, rem);
	p = pablo_json_uint(p, "id", stream->id, rem);
	p = pablo_json_nstr(p, "name", stream->name, strlen(stream->name), rem);
	p = pablo_json_nstr(p, "sensor", stream->sensor, strlen(stream->sensor), rem);
	p = pablo_json_array_open(p, "nodes", rem);

	for (i = 0; i < stream->node_count; i++) {
		p = pablo_json_object_open(p, NULL, rem);
		p = pablo_json_nstr(
			p, "name", stream->node[i].name, strlen(stream->node[i].name), rem);
		p = pablo_json_uint(p, "id", stream->node[i].id, rem);

		if (stream->node[i].in.has) {
			p = pablo_json_array_open(p, "input", rem);
			p = make_node_info_json(&stream->node[i].in, p, rem);
			p = pablo_json_array_close(p, rem);
		}

		if (stream->node[i].out.has) {
			p = pablo_json_array_open(p, "output", rem);
			p = make_node_info_json(&stream->node[i].out, p, rem);
			p = pablo_json_array_close(p, rem);
		}
		p = pablo_json_object_close(p, rem);
	}

	p = pablo_json_array_close(p, rem);
	p = pablo_json_object_close(p, rem);

	return p;
}

static int get_stream_info(char *buffer, size_t buf_size)
{
	int i, idx, need_page_num;
	u32 group_count = 0;
	u32 stream_count = 0;
	char *p = buffer;
	size_t rem = buf_size, available_size = buf_size;
	struct is_core *core;
	struct is_groupmgr *grpmgr;
	struct is_group *group;
	struct pablo_stream *stream;

	core = is_get_is_core();
	grpmgr = is_get_groupmgr();

	/* get the number of opened stream to predict buf size */
	for (i = 0; i < IS_STREAM_COUNT; i++) {
		if (test_bit(IS_ISCHAIN_OPEN, &core->ischain[i].state))
			++stream_count;
	}

	/* measure buf size and if buf_size is big than PAGE_SIZE, use internal buf */
	need_page_num = stream_count * STREAM_UNIT_BUF_SIZE / PAGE_SIZE;
	if (need_page_num > 1) {
		internal_buf = pablo_zalloc(PAGE_SIZE * need_page_num, GFP_KERNEL);
		if (!internal_buf)
			return -ENOMEM;

		p = internal_buf;
		rem = available_size = PAGE_SIZE * need_page_num;
	}

	/* make stream info json */
	p = pablo_json_object_open(p, NULL, &rem);
	p = pablo_json_array_open(p, "stream", &rem);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		if (test_bit(IS_ISCHAIN_OPEN, &core->ischain[i].state)) {
			/* get the number of valid group */
			for (group = grpmgr->leader[i], group_count = 0; group; group = group->next)
				++group_count;

			/* fill stream info */
			stream = pablo_zalloc(struct_size(stream, node, group_count), GFP_KERNEL);
			if (!stream)
				return -ENOMEM;

			stream->id = i;
			stream->sensor = pablo_lib_get_stream_prefix(i);
			stream->node_count = group_count;

			group = grpmgr->leader[i];
			if (test_bit(IS_GROUP_REPROCESSING, &group->state))
				stream->name = pablo_stream_name_repro;
			else if (test_bit(IS_GROUP_USE_MULTI_CH, &group->state))
				stream->name = pablo_stream_name_multich;
			else
				stream->name = pablo_stream_name_preview;

			for (group = grpmgr->leader[i], idx = 0; group;
				group = group->next, idx++) {
				stream->node[idx].id = group->id;
				stream->node[idx].name = group_id_name[group->id];

				if (group->prev) {
					stream->node[idx].in.io =
						test_bit(IS_GROUP_OTF_INPUT, &group->state) ?
							PABLO_STREAM_EDGE_IO_OTF :
							PABLO_STREAM_EDGE_IO_M2M;
					stream->node[idx].in.connected_node = group->prev->id;
					stream->node[idx].in.width = group->leader.input.width;
					stream->node[idx].in.height = group->leader.input.height;
					stream->node[idx].in.has = true;
				}

				if (group->next) {
					stream->node[idx].out.io =
						test_bit(IS_GROUP_OTF_OUTPUT, &group->state) ?
							PABLO_STREAM_EDGE_IO_OTF :
							PABLO_STREAM_EDGE_IO_M2M;
					stream->node[idx].out.connected_node = group->next->id;
					stream->node[idx].out.width = group->leader.output.width;
					stream->node[idx].out.height = group->leader.output.height;
					stream->node[idx].out.has = true;
				}
			}
			p = make_stream_info_json(stream, p, &rem);
			pablo_free(stream);
		}
	}

	p = pablo_json_array_close(p, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);
	/* indicate EOD with new line */
	*p++ = '\n';
	rem--;

	return (available_size - rem);
}

static int make_ip_info_json(
	char *buffer, size_t buf_size, struct is_param_region *param, u32 group_id)
{
	size_t wr_size, new_len;
	size_t rem = buf_size, available_size = buf_size;
	char *p = buffer, *new_buf;
	u32 try_page_num = 1;
	u32 closing_size = 3;

	p = buffer;
	rem = buf_size;
	while (try_page_num < SUPPORT_PAGE_NUM_MAX) {
		p = pablo_json_object_open(p, NULL, &rem);
		p = pablo_json_object_open(p, group_id_name[group_id], &rem);

		wr_size = is_hw_get_param_dump(p, rem, param, group_id);
		rem = (rem > wr_size) ? rem - wr_size : 0;
		p += wr_size;
		if (rem > closing_size)
			break;

		/* need to use internal memory */
		new_len = available_size + PAGE_SIZE;
		new_buf = internal_buf ?
				  pkv_kvrealloc(internal_buf, available_size, new_len, GFP_KERNEL) :
				  pablo_zalloc(new_len, GFP_KERNEL);

		if (!new_buf)
			return -ENOMEM;

		p = internal_buf = new_buf;
		rem = available_size = new_len;
		try_page_num++;
	}

	if (try_page_num == SUPPORT_PAGE_NUM_MAX) {
		pr_err("Exceed the max number of supported kernel pages\n");
		return -ENOMEM;
	}

	p = pablo_json_object_close(p, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);
	/* indicate EOD with new line */
	*p++ = '\n';
	rem--;

	return (available_size - rem);
}

static int get_ip_info(char *buffer, size_t size, u32 stream_id, u32 group_id)
{
	struct is_device_ischain *ischain;
	struct is_group *group;
	struct is_groupmgr *grpmgr;
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	struct is_core *core;

	core = is_get_is_core();
	grpmgr = is_get_groupmgr();
	ischain = &core->ischain[stream_id];

	if (!test_bit(IS_ISCHAIN_OPEN, &ischain->state)) {
		pr_err("Not opened stream %d\n", stream_id);
		return -EINVAL;
	}

	group = grpmgr->leader[stream_id];
	while (group) {
		if (group_id == group->id)
			break;
		group = group->next;
	}

	if (!group) {
		pr_err("Can't find group id(%d)\n", group_id);
		return -EINVAL;
	}

	framemgr = GET_HEAD_GROUP_FRAMEMGR(group);
	if (framemgr) {
		frame = peek_frame_tail(framemgr, FS_FREE);
		if (frame && frame->fcount > 0)
			return make_ip_info_json(
				buffer, size, &ischain->is_region->parameter, group_id);
	}

	pr_warn("Can't find available group info(%d)\n", group_id);

	return 0;
}

static void clear_preset(struct pablo_stream_sysfs_param *param)
{
	if (internal_buf) {
		pablo_free(internal_buf);
		internal_buf = NULL;
	}
	param->mode = PABLO_STREAM_INFO_MODE_STREAM;
	param->remained_size = param->total_size = 0;
}

static int pablo_stream_info_set(const char *val, const struct kernel_param *kp)
{
	struct pablo_stream_sysfs_param *param = (struct pablo_stream_sysfs_param *)kp->arg;
	int ret = -EINVAL;
	int argc;
	char **argv;
	u32 stream_id, group_id;

	clear_preset(param);

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("No argument!\n");
		return ret;
	}

	if (argc < 1) {
		pr_err("Not enough parameters. %d < 1\n", argc);
		goto func_exit;
	}

	if (!strncmp(argv[0], "ip", 2)) {
		if (argc < 3) {
			pr_err("Not enough parameters. %d < 3\n", argc);
			goto func_exit;
		}

		ret = kstrtouint(argv[1], 0, &stream_id);
		if (ret || stream_id >= IS_STREAM_COUNT) {
			pr_err("Invalid stream %d ret %d\n", stream_id, ret);
			goto func_exit;
		}

		ret = kstrtouint(argv[2], 0, &group_id);
		if (ret || group_id >= GROUP_ID_MAX) {
			pr_err("Invalid group id %d ret %d\n", group_id, ret);
			goto func_exit;
		}

		param->stream_id = stream_id;
		param->group_id = group_id;
		param->mode = PABLO_STREAM_INFO_MODE_IP;
	} else {
		pr_err("Invalid argument\n");
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_stream_info_get(char *buffer, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	size_t offset = 0, wr_size;
	size_t buf_size = WRITABLE_PAGE_SIZE;
	struct pablo_stream_sysfs_param *param = (struct pablo_stream_sysfs_param *)kp->arg;

	if (param->mode >= PABLO_STREAM_INFO_MODE_MAX)
		goto err_exit;

	if (param->mode == PABLO_STREAM_INFO_MODE_GET_REMAINED) {
		if (param->total_size >= param->remained_size) {
			wr_size = (param->remained_size >= WRITABLE_PAGE_SIZE) ?
					  WRITABLE_PAGE_SIZE :
					  param->remained_size;
			offset = param->total_size - param->remained_size;
			param->remained_size = param->remained_size - wr_size;
		} else {
			goto err_exit;
		}
	} else {
		if (param->mode == PABLO_STREAM_INFO_MODE_STREAM)
			ret = get_stream_info(buffer, buf_size);
		else if (param->mode == PABLO_STREAM_INFO_MODE_IP)
			ret = get_ip_info(buffer, buf_size, param->stream_id, param->group_id);

		if (ret < 0)
			goto err_exit;

		param->total_size = ret;
		wr_size = (param->total_size >= WRITABLE_PAGE_SIZE) ? WRITABLE_PAGE_SIZE :
								      param->total_size;
		param->remained_size = param->total_size - wr_size;
	}

	if (internal_buf)
		memcpy(buffer, internal_buf + offset, wr_size);

	if (param->remained_size <= 0)
		clear_preset(param);
	else
		param->mode = PABLO_STREAM_INFO_MODE_GET_REMAINED;

	return wr_size;

err_exit:
	clear_preset(param);
	return ret;
}
