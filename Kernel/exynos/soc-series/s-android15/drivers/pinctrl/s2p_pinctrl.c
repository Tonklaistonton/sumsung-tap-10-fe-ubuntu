// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 */
#include <linux/pmic/s2p_pinctrl.h>

int s2p_gpio_read(const struct s2p_gpio_state *state, const uint8_t addr)
{
	uint8_t val = 0;
	int ret = 0;

	ret = s2p_read_reg(state->sdev, state->gpio_addr, addr, &val);
	if (ret)
		dev_err(state->dev, "%s: read %#02x%02hhx: %#02hhx failed\n",
				__func__, state->gpio_addr, addr, val);
	else
		ret = val;

	return ret;
}

int s2p_gpio_write(const struct s2p_gpio_state *state, const uint8_t addr, const uint8_t val)
{
	return s2p_write_reg(state->sdev, state->gpio_addr, addr, val);
}

static bool s2p_get_gpio_mode(const struct s2p_gpio_state *state, const int val)
{
	return ((val & state->bit->gpio_oen_mask) >> state->bit->gpio_oen_shift) ? true : false;
}

static uint32_t s2p_get_gpio_pull(const struct s2p_gpio_state *state, const int val)
{
	return ((val & state->bit->gpio_pull_mask) >> state->bit->gpio_pull_shift) % ARRAY_SIZE(s2p_biases);
}

static uint32_t s2p_get_gpio_strength(const struct s2p_gpio_state *state, const int val)
{
	uint8_t array_size = state->bit->gpio_drv_str_mask + 1;

	return ((val & state->bit->gpio_drv_str_mask) >> state->bit->gpio_drv_str_shift) % array_size;
}

static uint32_t s2p_get_gpio_output(const struct s2p_gpio_state *state,
				const uint32_t pin, const int val_status)
{
	return state->get_gpio_output_func(pin, val_status);
}

static uint8_t s2p_get_set_reg(const struct s2p_gpio_state *state, const uint32_t pin)
{
	return state->get_set_reg_func(pin);
}

static uint8_t s2p_get_status_reg(const struct s2p_gpio_state *state, const uint32_t pin)
{
	return state->get_status_reg_func(pin);
}

static int s2p_set_gpio_conf_output(const struct s2p_gpio_state *state, uint32_t arg,
				uint32_t pin, int val_set)
{
	return state->set_gpio_conf_output_func(state, arg, pin, val_set);
}

static int s2p_get_gpio_state(const struct s2p_gpio_state *state, struct s2p_gpio_pad *pad,
				const uint32_t pin, const uint8_t reg_set, const uint8_t reg_status,
				int *val_set, int *val_status)
{
	*val_set = s2p_gpio_read(state, reg_set);
	if (*val_set < 0)
		return -EINVAL;

	*val_status = s2p_gpio_read(state, reg_status);
	if (*val_status < 0)
		return -EINVAL;

	pad->output_enabled = s2p_get_gpio_mode(state, *val_set);
	pad->output = s2p_get_gpio_output(state, pin, *val_status);
	pad->pull = s2p_get_gpio_pull(state, *val_set);
	pad->strength = s2p_get_gpio_strength(state, *val_set);

	return 0;
}

int s2p_get_gpio_info(const struct s2p_gpio_state *state, const uint32_t pin)
{
	struct s2p_gpio_pad *pad = state->pin_desc[pin].drv_data;
	const uint8_t reg_set = s2p_get_set_reg(state, pin);
	const uint8_t reg_status = s2p_get_status_reg(state, pin);
	int val_set = 0, val_status = 0, ret = 0;

	ret = s2p_get_gpio_state(state, pad, pin, reg_set, reg_status, &val_set, &val_status);
	if (ret < 0) {
		dev_info(state->dev, "%s: s2p_get_gpio_state() failed", __func__);
		return -EINVAL;
	}

	dev_info(state->dev, "[PMIC] %s: pin%d: (%#02hhx:%#02hhx), (%#02hhx:%#02hhx)\n",
				__func__, pin, reg_status, val_status, reg_set, val_set);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_get_gpio_info);

int s2p_gpio_get_function_groups(const struct s2p_gpio_state *state, const char *const **groups,
				uint32_t *const num_qgroups)
{
	int ret = 0;

	*groups = state->gpio_groups;
	*num_qgroups = state->gpio_groups_size;

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_gpio_get_function_groups);

int s2p_gpio_set_mux(const struct s2p_gpio_state *state, struct pinctrl_dev *pctldev,
			uint32_t func_selector, uint32_t group_selector)
{
	int ret = 0;
	size_t npins = 0;
	uint32_t i = 0;
	struct s2p_gpio_pad *pad = NULL;

	npins = state->groups[group_selector].npins;

	for (i = 0; i < npins; i++) {
		pad = pctldev->desc->pins[i].drv_data;
		pad->function = func_selector;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_gpio_set_mux);

int s2p_gpio_get_group_pins(struct s2p_gpio_state *state, uint32_t selector,
			     const uint32_t **pins, uint32_t *num_pins)
{
	if (selector >= state->ngroups)
		return -EINVAL;

	*pins = state->groups[selector].pins;
	*num_pins = state->groups[selector].npins;

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_gpio_get_group_pins);

void s2p_gpio_pin_dbg_show(struct s2p_gpio_state *state, struct s2p_gpio_pad *pad,
			    struct seq_file *s, uint32_t pin)
{
	const char *const *s2p_strengths = state->strengths;

	if (s2p_get_gpio_info(state, pin) < 0) {
		seq_printf(s, "s2p_get_gpio_info fail(%d)", __LINE__);
		return;
	}

	seq_printf(s, "%s(%#x) MODE(%s) DRV_STR(%s_%#x) DAT(%s_%#x)",
			s2p_biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			s2p_strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);
}
EXPORT_SYMBOL_GPL(s2p_gpio_pin_dbg_show);

int s2p_gpio_pin_config_get(struct s2p_gpio_state *state, struct s2p_gpio_pad *pad,
			uint32_t pin, unsigned long *config)
{
	const uint32_t param = pinconf_to_config_param(*config);
	uint32_t arg = 0;

	if (s2p_get_gpio_info(state, pin) < 0)
		return -EINVAL;

	switch (param) {
	case S2P_GPIO_CONF_DISABLE:
	case S2P_GPIO_CONF_PULL_DOWN:
	case S2P_GPIO_CONF_PULL_UP:
		arg = pad->pull;
		break;
	case S2P_GPIO_CONF_INPUT_ENABLE:
	case S2P_GPIO_CONF_OUTPUT_ENABLE:
		arg = pad->output_enabled;
		break;
	case S2P_GPIO_CONF_OUTPUT:
		arg = pad->output;
		break;
	case S2P_GPIO_CONF_DRIVE_STRENGTH:
		arg = pad->strength;
		break;
	default:
		return -EINVAL;
	}

	*config = pinconf_to_config_packed(param, arg);

	dev_info(state->dev, "[PMIC] %s: pin%d: param(%#x), arg(%#x)\n", __func__, pin, param, arg);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_gpio_pin_config_get);

int s2p_gpio_pin_config_set(const struct s2p_gpio_state *state, struct s2p_gpio_pad *pad,
			uint32_t pin, unsigned long *configs, uint32_t num_configs)
{
	const uint8_t reg_set = s2p_get_set_reg(state, pin);
	const char *const *s2p_strengths = state->strengths;
	uint32_t param = 0, arg = 0, i = 0;
	int ret = 0, val_set = 0, cnt = 0;
	char buf[1024] = {0, };

	val_set = s2p_gpio_read(state, reg_set);
	if (val_set < 0)
		return -EINVAL;

	for (i = 0; i < num_configs; i++) {
		param = pinconf_to_config_param(configs[i]);
		arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
		case S2P_GPIO_CONF_DISABLE:
			val_set = (val_set & ~state->bit->gpio_pull_mask) | state->bit->gpio_pull_disable;
			break;
		case S2P_GPIO_CONF_PULL_DOWN:
			val_set = (val_set & ~state->bit->gpio_pull_mask) | state->bit->gpio_pull_down;
			break;
		case S2P_GPIO_CONF_PULL_UP:
			val_set = (val_set & ~state->bit->gpio_pull_mask) | state->bit->gpio_pull_up;
			break;
		case S2P_GPIO_CONF_INPUT_ENABLE:
			val_set = (val_set & ~state->bit->gpio_oen_mask) | state->bit->gpio_mode_digit_input;
			break;
		case S2P_GPIO_CONF_OUTPUT_ENABLE:
			val_set = (val_set & ~state->bit->gpio_oen_mask) | state->bit->gpio_mode_digit_output;
			break;
		case S2P_GPIO_CONF_OUTPUT:
			val_set = s2p_set_gpio_conf_output(state, arg, pin, val_set);
			break;
		case S2P_GPIO_CONF_DRIVE_STRENGTH:
			arg = (arg << state->bit->gpio_drv_str_shift) & state->bit->gpio_drv_str_mask;
			val_set = (val_set & ~state->bit->gpio_drv_str_mask) | arg;
			break;
		default:
			return -ENOTSUPP;
		}
	}

	ret = s2p_gpio_write(state, reg_set, val_set);
	if (ret < 0) {
		dev_err(state->dev, "[PMIC] %s: s2p_gpio_write fail\n", __func__);
		return ret;
	}

	if (s2p_get_gpio_info(state, pin) < 0)
		return -EINVAL;

	cnt += snprintf(buf + cnt, sizeof(buf) - 1,
			"reg(%#02x%02hhx:%#02hhx), %s(%#x), MODE(%s), DRV_STR(%s_%#x), DAT(%s_%#x)",
			state->gpio_addr, reg_set, val_set,
			s2p_biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			s2p_strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);

	dev_info(state->dev, "[PMIC] %s: pin%d: %s\n", __func__, pin, buf);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_gpio_pin_config_set);

int s2p_gpio_direction_input(const struct s2p_gpio_state *state, uint32_t pin)
{
	int ret = 0;
	unsigned long config = 0;

	config = pinconf_to_config_packed(S2P_GPIO_CONF_INPUT_ENABLE, 1);
	ret = state->ctrl->desc->confops->pin_config_set(state->ctrl, pin, &config, 1);

	dev_info(state->dev, "[PMIC] %s: pin%d: ret(%#x)\n", __func__, pin, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_gpio_direction_input);

int s2p_gpio_direction_output(const struct s2p_gpio_state *state, uint32_t pin, int val)
{
	unsigned long configs[] = {S2P_GPIO_CONF_OUTPUT_ENABLE, S2P_GPIO_CONF_OUTPUT};
	uint32_t i = 0;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(configs); i++)
		configs[i] = pinconf_to_config_packed(configs[i], val);
	ret = state->ctrl->desc->confops->pin_config_set(state->ctrl, pin, configs, ARRAY_SIZE(configs));

	dev_info(state->dev, "[PMIC] %s: pin%d: val(%#x), ret(%#x)\n", __func__, pin, val, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_gpio_direction_output);

int s2p_gpio_pin_xlate(struct s2p_gpio_state *state, const uint32_t gpio)
{
	/* Translate a DT GPIO specifier into a PMIC GPIO number */
	if (gpio < state->npins)
		return gpio;
	else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(s2p_gpio_pin_xlate);

int s2p_gpio_of_xlate(struct s2p_gpio_state * state, struct gpio_chip *chip,
		       const struct of_phandle_args *gpio_desc, u32 *flags)
{
	int ret = 0;

	if (chip->of_gpio_n_cells < 2)
		return -EINVAL;

	if (flags)
		*flags = gpio_desc->args[1];

	ret = s2p_gpio_pin_xlate(state, gpio_desc->args[0]);
	if (ret < 0) {
		dev_err(state->dev, "%s: Invalid GPIO pin(%d)\n", __func__, gpio_desc->args[0]);
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(s2p_gpio_of_xlate);

int s2p_gpio_populate(const struct s2p_gpio_state *state,
		       struct s2p_gpio_pad *pad, const uint32_t pin)
{
	const uint8_t reg_set = s2p_get_set_reg(state, pin);
	const uint8_t reg_status = s2p_get_status_reg(state, pin);
	const char *const *s2p_strengths = state->strengths;
	int val_set = 0, val_status = 0, ret = 0;

	ret = s2p_get_gpio_state(state, pad, pin, reg_set, reg_status, &val_set, &val_status);
	if (ret < 0) {
		dev_info(state->dev, "%s: s2p_get_gpio_state() failed", __func__);
		return -EINVAL;
	}

	dev_info(state->dev, "[PMIC] %s: pin%d: %s(%#x), MODE(%s), "
			"DRV_STR(%s_%#x), DAT(%s_%#x)\n", __func__, pin,
			s2p_biases[pad->pull], pad->pull,
			pad->output_enabled ? "output" : "input",
			s2p_strengths[pad->strength], pad->strength,
			pad->output ? "high" : "low", pad->output);

	return 0;
}
EXPORT_SYMBOL_GPL(s2p_gpio_populate);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung SPMI PMIC GPIO pin control common driver");
MODULE_ALIAS("platform:samsung-spmi-gpio-common");
MODULE_LICENSE("GPL v2");
