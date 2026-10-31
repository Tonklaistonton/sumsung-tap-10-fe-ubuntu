// SPDX-License-Identifier: GPL-2.0
/*
 * Dma buf container support
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/miscdevice.h>
#include <linux/dma-buf-container.h>
#include <linux/module.h>

#include "dma-buf-container.h"
#include "heap_private.h"

#define MAX_BUFCON_BUFS 64
#define MAX_BUFCON_SRC_BUFS (MAX_BUFCON_BUFS - 1)

static int dmabuf_container_get_user_data(unsigned int cmd, void __user *arg,
					  int fds[])
{
	int __user *ufds;
	int count, ret;

#ifdef CONFIG_COMPAT
	if (cmd == DMABUF_CONTAINER_COMPAT_IOCTL_MERGE) {
		struct compat_dma_buf_merge __user *udata = arg;
		compat_uptr_t compat_ufds;

		ret = get_user(compat_ufds, &udata->dma_bufs);
		ret |= get_user(count, &udata->count);
		ufds = compat_ptr(compat_ufds);
	} else
#endif
	{
		struct dma_buf_merge __user *udata = arg;

		ret = get_user(ufds, &udata->dma_bufs);
		ret |= get_user(count, &udata->count);
	}

	if (ret) {
		perr("failed to read data from user");
		return -EFAULT;
	}

	if ((count < 1) || (count > MAX_BUFCON_SRC_BUFS)) {
		perr("invalid buffer count %u", count);
		return -EINVAL;
	}

	if (copy_from_user(fds, ufds, sizeof(fds[0]) * count)) {
		perr("failed to read %u dma_bufs from user", count);
		return -EFAULT;
	}

	return count;
}

static int dmabuf_container_put_user_data(unsigned int cmd, void __user *arg,
					  struct dma_buf *merged)
{
	int fd = get_unused_fd_flags(O_CLOEXEC);
	int ret;

	if (fd < 0) {
		perr("failed to get new fd");
		return fd;
	}

#ifdef CONFIG_COMPAT
	if (cmd == DMABUF_CONTAINER_COMPAT_IOCTL_MERGE) {
		struct compat_dma_buf_merge __user *udata = arg;

		ret = put_user(fd, &udata->dmabuf_container);
	} else
#endif
	{
		struct dma_buf_merge __user *udata = arg;

		ret = put_user(fd, &udata->dmabuf_container);
	}

	if (ret) {
		perr("failed to store dmabuf_container fd to user");

		put_unused_fd(fd);
		return ret;
	}

	fd_install(fd, merged->file);

	return 0;
}

/*
 * struct dma_buf_container - container description
 * @table:	dummy sg_table for container
 * @count:	the number of the buffers
 * @dmabuf_mask: bit-mask of dma-bufs in @dmabufs.
 *               @dmabuf_mask is 0(unmasked) on creation of a dma-buf container.
 * @dmabufs:	dmabuf array representing each buffers
 */
struct dma_buf_container {
	struct samsung_dma_buffer *buffer;
	int		count;
	u64		dmabuf_mask;
	struct dma_buf	*dmabufs[];
};

static void dmabuf_container_put_dmabuf(struct dma_buf_container *container)
{
	int i;

	for (i = 0; i < container->count; i++)
		dma_buf_put(container->dmabufs[i]);
}

static void dmabuf_container_dma_buf_release(struct dma_buf *dmabuf)
{
	struct samsung_dma_buffer *buffer = dmabuf->priv;
	struct dma_buf_container *container = buffer->priv;

	samsung_heap_dma_buf_release(dmabuf);

	dmabuf_container_put_dmabuf(container);
	kfree(container);
}

static struct dma_buf_ops dmabuf_container_dma_buf_ops = {
	.map_dma_buf = samsung_heap_map_dma_buf,
	.unmap_dma_buf = samsung_heap_unmap_dma_buf,
	.begin_cpu_access = samsung_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = samsung_heap_dma_buf_end_cpu_access,
	.vmap = samsung_heap_vmap,
	.vunmap = samsung_heap_vunmap,
	.mmap = samsung_heap_mmap,
	.get_flags = samsung_heap_dma_buf_get_flags,
	.release = dmabuf_container_dma_buf_release,
};

static bool is_dmabuf_container(struct dma_buf *dmabuf)
{
	return dmabuf->ops == &dmabuf_container_dma_buf_ops;
}

static struct dma_buf_container *get_container(struct dma_buf *dmabuf)
{
	struct samsung_dma_buffer *buffer = dmabuf->priv;

	return buffer->priv;
}

static int get_dma_buf_count(struct dma_buf *dmabuf)
{
	return is_dmabuf_container(dmabuf) ? get_container(dmabuf)->count : 1;
}

static struct dma_buf *__dmabuf_container_get_buffer(struct dma_buf *dmabuf,
						     int index)
{
	struct dma_buf *out = is_dmabuf_container(dmabuf)
			      ? get_container(dmabuf)->dmabufs[index] : dmabuf;

	get_dma_buf(out);

	return out;
}

static struct dma_buf *dmabuf_container_export(struct dma_buf_container *bufcon)
{
	DEFINE_SAMSUNG_DMA_BUF_EXPORT_INFO(exp_info, "dmabuf-container");
	struct dma_buf *dmabuf;

	exp_info.ops = &dmabuf_container_dma_buf_ops;
	exp_info.size = bufcon->buffer->len;
	exp_info.flags = O_RDWR;
	exp_info.priv = bufcon->buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		perrfn("failed to export buffer (ret: %ld)", PTR_ERR(dmabuf));
		return dmabuf;
	}

	bufcon->buffer->i_node = file_inode(dmabuf->file)->i_ino;
	atomic_long_add(dmabuf->size, &bufcon->buffer->heap->total_bytes);
	dmabuf_trace_alloc(dmabuf);

	return dmabuf;
}

void dup_container_sg_table(struct dma_buf_container *container)
{
	struct scatterlist *sg, *new_sg;
	int i, j;

	new_sg = container->buffer->sg_table.sgl;

	for (i = 0; i < container->count; i++) {
		struct samsung_dma_buffer *buffer = container->dmabufs[i]->priv;

		for_each_sgtable_sg(&buffer->sg_table, sg, j) {
			sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
			new_sg = sg_next(new_sg);
		}
	}
}

static struct samsung_dma_heap dmabuf_container_heap;

static struct dma_buf *create_dmabuf_container(struct dma_buf *bufs[],
					       int count)
{
	struct samsung_dma_buffer *buffer;
	struct dma_buf_container *container;
	struct dma_buf *merged;
	int ret, total = 0;
	int i, j, nents = 0, nelem = 0;
	unsigned long flags, container_flags;
	unsigned long size = 0;

	for (i = 0; i < count; i++)
		total += get_dma_buf_count(bufs[i]);

	if (total > MAX_BUFCON_BUFS) {
		perr("too many (%u) dmabuf merge request", total);
		return ERR_PTR(-EINVAL);
	}

	container = kzalloc(sizeof(*container) +
			    sizeof(container->dmabufs[0]) * total, GFP_KERNEL);
	if (!container)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++)
		for (j = 0; j < get_dma_buf_count(bufs[i]); j++)
			container->dmabufs[nelem++] =
				__dmabuf_container_get_buffer(bufs[i], j);

	container->count = nelem;

	dma_buf_get_flags(container->dmabufs[0], &container_flags);

	if (dma_heap_flags_protected(container_flags)) {
		ret = -EINVAL;

		perr("dmabuf container for protected buffer is not allowed");
		goto err_flags;
	}

	for (i = 0; i < container->count; i++) {
		struct dma_buf *dmabuf = container->dmabufs[i];
		struct samsung_dma_buffer *buf = dmabuf->priv;

		dma_buf_get_flags(dmabuf, &flags);

		if (container_flags != flags) {
			ret = -EINVAL;

			perr("flags in container are not the same %lu %lu",
			     container_flags, flags);
			goto err_flags;
		}

		nents += buf->sg_table.orig_nents;
		size += dmabuf->size;
	}

	buffer = samsung_dma_buffer_alloc(&dmabuf_container_heap, size);
	if (IS_ERR(buffer)) {
		ret = PTR_ERR(buffer);

		goto err_flags;
	}
	buffer->flags = container_flags;
	buffer->priv = container;

	container->buffer = buffer;

	if (sg_alloc_table(&buffer->sg_table, nents, GFP_KERNEL)) {
		ret = -ENOMEM;

		goto err_buffer_alloc;
	}
	dup_container_sg_table(container);

	merged = dmabuf_container_export(container);
	if (IS_ERR(merged)) {
		ret = PTR_ERR(merged);

		perr("failed to export dmabuf container");
		goto err_export;
	}

	return merged;

err_export:
	sg_free_table(&buffer->sg_table);
err_buffer_alloc:
	samsung_dma_buffer_free(buffer);
err_flags:
	dmabuf_container_put_dmabuf(container);
	kfree(container);

	return ERR_PTR(ret);
}

struct dma_buf *dmabuf_container_create(int fds[], int count)
{
	struct dma_buf *bufs[MAX_BUFCON_SRC_BUFS];
	struct dma_buf *merged;
	int i;

	for (i = 0; i < count; i++) {
		bufs[i] = dma_buf_get(fds[i]);
		if (IS_ERR(bufs[i])) {
			merged = bufs[i];
			perr("failed to get dmabuf of fd %d @ %u/%u", fds[i], i, count);

			goto err_get;
		}
	}

	merged = create_dmabuf_container(bufs, count);
	/*
	 * reference count of dma_bufs (file->f_count) in bufs[] are increased
	 * again in create_dmabuf_container(). So they should be decremented
	 * before return.
	 */
err_get:
	while (i-- > 0)
		dma_buf_put(bufs[i]);

	return merged;
}
EXPORT_SYMBOL_GPL(dmabuf_container_create);

long dma_buf_merge_ioctl(unsigned int cmd, unsigned long arg)
{
	int fds[MAX_BUFCON_SRC_BUFS];
	int count;
	struct dma_buf *merged;
	long ret;

	count = dmabuf_container_get_user_data(cmd, (void __user *)arg, fds);
	if (count < 0)
		return count;

	merged = dmabuf_container_create(fds, count);
	if (IS_ERR(merged))
		return PTR_ERR(merged);

	ret = dmabuf_container_put_user_data(cmd, (void __user *)arg, merged);
	if (ret)
		dma_buf_put(merged);

	return ret;
}

int dmabuf_container_set_mask_user(unsigned long arg)
{
	struct dma_buf_mask __user *udata = (void __user *)arg;
	struct dma_buf *dmabuf;
	int container, ret;
	u32 mask_array[2];
	u64 mask;

	ret = get_user(container, &udata->dmabuf_container);
	ret |= copy_from_user(mask_array, udata->mask, sizeof(mask_array));
	if (ret) {
		perr("failed to data from user");
		return -EFAULT;
	}

	dmabuf = dma_buf_get(container);
	if (IS_ERR(dmabuf)) {
		perr("failed to get dmabuf %d", container);
		return PTR_ERR(dmabuf);
	}

	mask = ((u64)mask_array[1] << 32) | mask_array[0];
	ret = dmabuf_container_set_mask(dmabuf, mask);
	dma_buf_put(dmabuf);

	return ret;
}

int dmabuf_container_get_mask_user(unsigned long arg)
{
	struct dma_buf_mask __user *udata = (void __user *)arg;
	struct dma_buf *dmabuf;
	int container, ret;
	u32 mask_array[2];
	u64 mask;

	ret = get_user(container, &udata->dmabuf_container);
	if (ret) {
		perr("failed to read data from user");
		return -EFAULT;
	}

	dmabuf = dma_buf_get(container);
	if (IS_ERR(dmabuf)) {
		perr("failed to get dmabuf %d", container);
		return PTR_ERR(dmabuf);
	}

	ret = dmabuf_container_get_mask(dmabuf, &mask);
	if (ret)
		goto err;

	mask_array[0] = (mask << 32) >> 32;
	mask_array[1] = mask >> 32;

	ret = copy_to_user(udata->mask, mask_array, sizeof(mask_array));
	if (ret)
		perr("failed to write mask to user");
err:
	dma_buf_put(dmabuf);
	return ret;
}

int dmabuf_container_set_mask(struct dma_buf *dmabuf, u64 mask)
{
	struct dma_buf_container *container;

	if (!is_dmabuf_container(dmabuf)) {
		perr("given dmabuf is not dma-buf container");
		return -EINVAL;
	}

	container = get_container(dmabuf);

	if (mask & ~((1 << container->count) - 1)) {
		perr("invalid mask %#llx for %u buffers", mask, container->count);
		return -EINVAL;
	}

	get_container(dmabuf)->dmabuf_mask = mask;

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_container_set_mask);

int dmabuf_container_get_mask(struct dma_buf *dmabuf, u64 *mask)
{
	if (!is_dmabuf_container(dmabuf)) {
		perr("given dmabuf is not dma-buf container");
		return -EINVAL;
	}

	*mask = get_container(dmabuf)->dmabuf_mask;

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_container_get_mask);

int dmabuf_container_get_count(struct dma_buf *dmabuf)
{
	if (!is_dmabuf_container(dmabuf))
		return -EINVAL;

	return get_container(dmabuf)->count;
}
EXPORT_SYMBOL_GPL(dmabuf_container_get_count);

struct dma_buf *dmabuf_container_get_buffer(struct dma_buf *dmabuf, int index)
{
	struct dma_buf_container *container = get_container(dmabuf);

	if (!is_dmabuf_container(dmabuf))
		return NULL;

	if (WARN_ON(index >= container->count))
		return NULL;

	get_dma_buf(container->dmabufs[index]);

	return container->dmabufs[index];
}
EXPORT_SYMBOL_GPL(dmabuf_container_get_buffer);

struct dma_buf *dma_buf_get_any(int fd)
{
	struct dma_buf *dmabuf = dma_buf_get(fd);
	struct dma_buf *anybuf = __dmabuf_container_get_buffer(dmabuf, 0);

	dma_buf_put(dmabuf);

	return anybuf;
}
EXPORT_SYMBOL_GPL(dma_buf_get_any);

static long dmabuf_container_ioctl(struct file *filp, unsigned int cmd,
				   unsigned long arg)
{
	switch (cmd) {
#ifdef CONFIG_COMPAT
	case DMABUF_CONTAINER_COMPAT_IOCTL_MERGE:
#endif
	case DMABUF_CONTAINER_IOCTL_MERGE:
		return dma_buf_merge_ioctl(cmd, arg);
	case DMABUF_CONTAINER_IOCTL_SET_MASK:
		return dmabuf_container_set_mask_user(arg);
	case DMABUF_CONTAINER_IOCTL_GET_MASK:
		return dmabuf_container_get_mask_user(arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations dmabuf_container_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = dmabuf_container_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= dmabuf_container_ioctl,
#endif
};

static struct miscdevice dmabuf_container_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dmabuf_container",
	.fops = &dmabuf_container_fops,
};

/*
 * samsung_dma_heap is dummy for maintain the code stability.
 * The dmabuf by exporting container is created only by /dev/dmabuf-container.
 * That should not be created by /dev/dma_heap/dmabuf-container, it is not permitted.
 */
int container_dma_heap_allocate(struct samsung_dma_buffer *buffer)
{
	return -EPERM;
}

void container_dma_heap_release(struct samsung_dma_buffer *buffer, enum df_reason reason)
{
}

static const struct samsung_heap_ops container_dma_heap_ops = {
	.allocate = container_dma_heap_allocate,
	.release = container_dma_heap_release,
};

static struct samsung_dma_heap dmabuf_container_heap = {
	.name = "dmabuf_container",
	.ops = &container_dma_heap_ops,
};

int __init dmabuf_container_init(void)
{
	int ret = misc_register(&dmabuf_container_dev);

	if (ret) {
		perr("failed to register %s", dmabuf_container_dev.name);
		return ret;
	}

	ret = samsung_heap_add(&dmabuf_container_heap);
	if (ret)
		return ret;

	return 0;
}
