/*
 *  sec_audio_exynos.c
 *
 *  Copyright (c) 2024 Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/of.h>
#include <linux/platform_device.h>

#include <sound/samsung/sec_audio_exynos.h>

static struct sec_audio_exynos_data *exynos_data;
static size_t rmem_size_min[TYPE_SIZE_MAX] = {0xab0cab0c, 0xab0cab0c};

/* sec_audio_debug callback */
int register_abox_log_extra_copy(void (*abox_log_extra_copy) (char *src_base,
		unsigned int index_reader, unsigned int index_writer,
		unsigned int src_buff_size))
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return -EFAULT;
	}

	if (exynos_data->abox_log_extra_copy) {
		dev_err(exynos_data->dev, "%s: Already registered\n", __func__);
		return -EEXIST;
	}

	exynos_data->abox_log_extra_copy = abox_log_extra_copy;

	return 0;
}
EXPORT_SYMBOL_GPL(register_abox_log_extra_copy);

int register_set_modem_event(void (*set_modem_event) (int event))
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return -EFAULT;
	}

	if (exynos_data->set_modem_event) {
		dev_err(exynos_data->dev, "%s: Already registered\n", __func__);
		return -EEXIST;
	}

	exynos_data->set_modem_event = set_modem_event;

	return 0;
}
EXPORT_SYMBOL_GPL(register_set_modem_event);

int register_abox_debug_string_update(void (*abox_debug_string_update)
		(enum abox_debug_err_type type, int p1, int p2, int p3, int audio_mode,
		unsigned long long audio_mode_time))
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return -EFAULT;
	}

	if (exynos_data->abox_debug_string_update) {
		dev_err(exynos_data->dev, "%s: Already registered\n", __func__);
		return -EEXIST;
	}

	exynos_data->abox_debug_string_update = abox_debug_string_update;

	return 0;
}
EXPORT_SYMBOL_GPL(register_abox_debug_string_update);

void abox_log_extra_copy(char *src_base, unsigned int index_reader,
		unsigned int index_writer, unsigned int src_buff_size)
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return;
	}

	if(exynos_data->abox_log_extra_copy)
		exynos_data->abox_log_extra_copy(src_base, index_reader,
				index_writer, src_buff_size);
}
EXPORT_SYMBOL_GPL(abox_log_extra_copy);

void set_modem_event(int event)
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return;
	}

	if(exynos_data->set_modem_event)
		exynos_data->set_modem_event(event);
}
EXPORT_SYMBOL_GPL(set_modem_event);

void abox_debug_string_update(enum abox_debug_err_type type, int p1, int p2, int p3, int audio_mode,
		unsigned long long audio_mode_time)
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return;
	}

	if(exynos_data->abox_debug_string_update)
		exynos_data->abox_debug_string_update(type, p1, p2, p3,
				audio_mode, audio_mode_time);
}
EXPORT_SYMBOL_GPL(abox_debug_string_update);

int force_upload;
int check_upload_mode_disabled(void)
{
	pr_info("%s: 0x%x\n", __func__, !force_upload);
	return !force_upload;
}
EXPORT_SYMBOL_GPL(check_upload_mode_disabled);

int debug_level;
int check_debug_level_low(void)
{
	int debug_level_low = 0;

	pr_info("%s: 0x%x\n", __func__, debug_level);
	if (debug_level == 0x4f4c) {
		debug_level_low = 1;
	} else {
		debug_level_low = 0;
	}

	return debug_level_low;
}
EXPORT_SYMBOL_GPL(check_debug_level_low);

size_t get_rmem_size_min(enum rmem_size_type id)
{
	return rmem_size_min[id];
}
EXPORT_SYMBOL_GPL(get_rmem_size_min);

/* sec_audio_sysfs callback */
int register_send_adsp_silent_reset_ev(void (*send_adsp_silent_reset_ev) (void))
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return -EFAULT;
	}

	if (exynos_data->send_adsp_silent_reset_ev) {
		dev_err(exynos_data->dev, "%s: Already registered\n", __func__);
		return -EEXIST;
	}

	exynos_data->send_adsp_silent_reset_ev = send_adsp_silent_reset_ev;

	return 0;
}
EXPORT_SYMBOL_GPL(register_send_adsp_silent_reset_ev);

void send_adsp_silent_reset_ev(void)
{
	if (exynos_data == NULL) {
		pr_err("%s: exynos_data is null\n", __func__);
		return;
	}

	if(exynos_data->send_adsp_silent_reset_ev)
		exynos_data->send_adsp_silent_reset_ev();
}
EXPORT_SYMBOL_GPL(send_adsp_silent_reset_ev);

static int sec_audio_exynos_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	unsigned int val = 0;
	const char *property_val;

	dev_info(dev, "%s probe\n", __func__);

	exynos_data = kzalloc(sizeof(*exynos_data), GFP_KERNEL);
	if (!exynos_data) {
		dev_info(dev, "%s memory allocation failed\n", __func__);
		return -ENOMEM;
	}

	ret = of_property_read_u32(np, "abox_dbg_size_min", &val);
	if (ret < 0)
		dev_err(dev, "%s: failed to get abox_dbg_size_min %d\n", __func__, ret);
	else
		rmem_size_min[TYPE_ABOX_DBG_SIZE] = (size_t) val;

	ret = of_property_read_u32(np, "abox_slog_size_min", &val);
	if (ret < 0)
		dev_err(dev, "%s: failed to get abox_slog_size_min %d\n", __func__, ret);
	else
		rmem_size_min[TYPE_ABOX_SLOG_SIZE] = (size_t) val;

	debug_level = 0x4f4c;
	np = of_find_node_by_name(NULL, "sec_debug_level");
	if (!np) {
		dev_err(dev, "%s: sec_debug_level doesn't exist\n", __func__);
	}
	else {
		ret = of_property_read_u32(np, "value", &val);
		if (ret < 0)
			dev_err(dev, "%s: failed to get sec_debug_level %d\n", __func__, ret);
		else
			debug_level = val;
	}

	force_upload = 0;
	np = of_find_node_by_name(NULL, "sec_force_upload");
	if (!np) {
		dev_err(dev, "%s: sec_force_upload doesn't exist\n", __func__);
	}
	else {
		ret = of_property_read_string(np, "status", &property_val);
		if (ret < 0)
			dev_err(dev, "%s: failed to get sec_force_upload %d\n", __func__, ret);
		else
			force_upload = strncmp("okay", property_val, 10) == 0 ? 1 : 0;
	}

	exynos_data->dev = dev;

	dev_info(dev, "%s probe done\n", __func__);
	return ret;
}

static int sec_audio_exynos_remove(struct platform_device *pdev)
{
	kfree(exynos_data);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sec_audio_exynos_of_match[] = {
	{ .compatible = "samsung,audio-exynos", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_audio_exynos_of_match);
#endif /* CONFIG_OF */

static struct platform_driver sec_audio_exynos_driver = {
	.driver		= {
		.name	= "sec-audio-exynos",
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(sec_audio_exynos_of_match),
#endif /* CONFIG_OF */
	},

	.probe = sec_audio_exynos_probe,
	.remove = sec_audio_exynos_remove,
};

module_platform_driver(sec_audio_exynos_driver);

MODULE_DESCRIPTION("Samsung Electronics Audio exynos driver");
MODULE_LICENSE("GPL");
