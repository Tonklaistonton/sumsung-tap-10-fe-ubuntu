// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 */

#include <linux/pmic/s2p_pinctrl.h>
#include <linux/pmic/s2mpa05-mfd.h>

enum s2mpa05_gpio_io_param {
	S2MPA05_GPIO_MODE_DIGITAL_INPUT = (0 << S2MPA05_GPIO_OEN_SHIFT),
	S2MPA05_GPIO_MODE_DIGITAL_OUTPUT = (1 << S2MPA05_GPIO_OEN_SHIFT),
};

enum s2mpa05_gpio_pull_param {
	S2MPA05_GPIO_PULL_DISABLE = (0 << S2MPA05_GPIO_PULL_SHIFT),
	S2MPA05_GPIO_PULL_DOWN = (1 << S2MPA05_GPIO_PULL_SHIFT),
	S2MPA05_GPIO_PULL_UP = (2 << S2MPA05_GPIO_PULL_SHIFT),
};

static const unsigned int s2mpa05_gpio_pins[] = {0, 1};

#define S2MPA05_GROUP_SIZE	1

static const struct pingroup s2mpa05_pin_groups[] = {
	PINCTRL_PINGROUP("EXTRA_PMIC_GPIO", s2mpa05_gpio_pins, ARRAY_SIZE(s2mpa05_gpio_pins)),
};

static const char *const s2mpa05_gpio_groups[] = {
	"gpio_e0", "gpio_e1",
};

static struct pinctrl_pin_desc s2mpa05_pin_desc[] = {
	PINCTRL_PIN(0, "gpio_e0"),
	PINCTRL_PIN(1, "gpio_e1"),
};

static const char *const s2mpa05_strengths[] = {
	"1mA", "2mA", "3mA", "4mA", "5mA", "6mA", "not-use", "not-use"
};

static uint32_t s2mpa05_get_gpio_output(const uint32_t pin, const int val)
{
	return (val >> (pin % S2MPA05_GPIO_RANGE)) & 0x1;
}

static uint8_t s2mpa05_get_set_reg(const uint32_t pin)
{
	return S2MPA05_GPIO_GPIO0_SET + pin;
}

static uint8_t s2mpa05_get_status_reg(const uint32_t pin)
{
	return S2MPA05_GPIO_GPIO_STATUS1;
}

static int s2mpa05_set_gpio_conf_output(const struct s2p_gpio_state *state, uint32_t arg,
		 			 uint32_t pin, int val_set)
{
	arg = (arg << S2MPA05_GPIO_OUT_SHIFT) & S2MPA05_GPIO_OUT_MASK;
	val_set = (val_set & ~S2MPA05_GPIO_OUT_MASK) | arg;

	return val_set;
}

static int s2mpa05_gpio_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(s2p_gpio_functions);
}

static const char *s2mpa05_gpio_get_function_name(struct pinctrl_dev *pctldev,
					       uint32_t function)
{
	return s2p_gpio_functions[function];
}

static int s2mpa05_gpio_get_function_groups(struct pinctrl_dev *pctldev,
					 uint32_t selector,
					 const char *const **groups,
					 uint32_t *const num_qgroups)
{
	const struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_get_function_groups(s2mpa05_state, groups, num_qgroups);

	return ret;
}

static int s2mpa05_gpio_set_mux(struct pinctrl_dev *pctldev, uint32_t func_selector,
				uint32_t group_selector)
{
	const struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_set_mux(s2mpa05_state, pctldev, func_selector, group_selector);

	return ret;
}

static const struct pinmux_ops s2mpa05_gpio_pinmux_ops = {
	.get_functions_count	= s2mpa05_gpio_get_functions_count,
	.get_function_name	= s2mpa05_gpio_get_function_name,
	.get_function_groups	= s2mpa05_gpio_get_function_groups,
	.set_mux		= s2mpa05_gpio_set_mux,
};


static int s2mpa05_gpio_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);

	return s2mpa05_state->ngroups;
}

static const char *s2mpa05_gpio_get_group_name(struct pinctrl_dev *pctldev, uint32_t selector)
{
	struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);

	return s2mpa05_state->groups[selector].name;
}

static int s2mpa05_gpio_get_group_pins(struct pinctrl_dev *pctldev, uint32_t selector,
				    const uint32_t **pins, uint32_t *num_pins)
{
	struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	int ret = 0;

	ret = s2p_gpio_get_group_pins(s2mpa05_state, selector, pins, num_pins);

	return ret;
}

static void s2mpa05_gpio_pin_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpa05_pad = pctldev->desc->pins[pin].drv_data;

	s2p_gpio_pin_dbg_show(s2mpa05_state, s2mpa05_pad, s, pin);
}

static const struct pinctrl_ops s2mpa05_gpio_pinctrl_ops = {
	.get_groups_count	= s2mpa05_gpio_get_groups_count,
	.get_group_name		= s2mpa05_gpio_get_group_name,
	.get_group_pins		= s2mpa05_gpio_get_group_pins,
	.dt_node_to_map		= pinconf_generic_dt_node_to_map_pin,
	.dt_free_map		= pinctrl_utils_free_map,
	.pin_dbg_show		= s2mpa05_gpio_pin_dbg_show,
};

static int s2mpa05_gpio_pin_config_get(struct pinctrl_dev *pctldev,
					uint32_t pin, unsigned long *config)
{
	struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpa05_pad = pctldev->desc->pins[pin].drv_data;
	int ret = 0;

	ret = s2p_gpio_pin_config_get(s2mpa05_state, s2mpa05_pad, pin, config);

	return ret;
}

static int s2mpa05_gpio_pin_config_set(struct pinctrl_dev *pctldev, uint32_t pin,
					unsigned long *configs, uint32_t num_configs)
{
	struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpa05_pad = pctldev->desc->pins[pin].drv_data;
	int ret = 0;

	ret = s2p_gpio_pin_config_set(s2mpa05_state, s2mpa05_pad, pin, configs, num_configs);

	return ret;
}

static int s2mpa05_gpio_pin_config_group_get(struct pinctrl_dev *pctldev,
					uint32_t selector, unsigned long *config)
{
	const struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = s2mpa05_state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2mpa05_gpio_pin_config_get(pctldev, i, config);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int s2mpa05_gpio_pin_config_group_set(struct pinctrl_dev *pctldev, uint32_t selector,
					unsigned long *configs, uint32_t num_configs)
{
	const struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	uint32_t npins = 0;
	uint32_t i = 0;
	int ret = 0;

	npins = s2mpa05_state->groups[selector].npins;

	for (i = 0; i < npins; i++) {
		ret = s2mpa05_gpio_pin_config_set(pctldev, i, configs, num_configs);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void s2mpa05_gpio_pin_config_dbg_show(struct pinctrl_dev *pctldev,
					struct seq_file *s, uint32_t pin)
{
	struct s2p_gpio_state *s2mpa05_state = pinctrl_dev_get_drvdata(pctldev);
	struct s2p_gpio_pad *s2mpa05_pad = pctldev->desc->pins[pin].drv_data;

	s2p_gpio_pin_dbg_show(s2mpa05_state, s2mpa05_pad, s, pin);
}

static const struct pinconf_ops s2mpa05_gpio_pinconf_ops = {
	.is_generic			= false,
	.pin_config_get			= s2mpa05_gpio_pin_config_get,
	.pin_config_set			= s2mpa05_gpio_pin_config_set,
	.pin_config_group_get		= s2mpa05_gpio_pin_config_group_get,
	.pin_config_group_set		= s2mpa05_gpio_pin_config_group_set,
	.pin_config_dbg_show		= s2mpa05_gpio_pin_config_dbg_show,
};

static int s2mpa05_gpio_direction_input(struct gpio_chip *chip, uint32_t pin)
{
	const struct s2p_gpio_state *s2mpa05_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_direction_input(s2mpa05_state, pin);

	return ret;
}

static int s2mpa05_gpio_direction_output(struct gpio_chip *chip, uint32_t pin, int val)
{
	const struct s2p_gpio_state *s2mpa05_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_direction_output(s2mpa05_state, pin, val);

	return ret;
}

static int s2mpa05_gpio_get(struct gpio_chip *chip, uint32_t pin)
{
	struct s2p_gpio_state *s2mpa05_state = gpiochip_get_data(chip);
	struct s2p_gpio_pad *s2mpa05_pad = s2mpa05_state->ctrl->desc->pins[pin].drv_data;

	if (s2p_get_gpio_info(s2mpa05_state, pin) < 0)
		return -EINVAL;

	dev_info(s2mpa05_state->dev, "[EXT_PMIC] %s: pin%d: DAT(%s)\n",
				__func__, pin, s2mpa05_pad->output ? "high" : "low");

	return s2mpa05_pad->output;
}

static void s2mpa05_gpio_set(struct gpio_chip *chip, uint32_t pin, int value)
{
	struct s2p_gpio_state *s2mpa05_state = gpiochip_get_data(chip);
	unsigned long config;

	dev_info(s2mpa05_state->dev, "[EXT_PMIC] %s: pin%d: Set DAT(%s_%#x)\n",
				__func__, pin, value ? "high" : "low", value);

	config = pinconf_to_config_packed(S2P_GPIO_CONF_OUTPUT, value);
	s2mpa05_gpio_pin_config_set(s2mpa05_state->ctrl, pin, &config, 1);
}

static int s2mpa05_gpio_of_xlate(struct gpio_chip *chip,
			      const struct of_phandle_args *gpio_desc,
			      u32 *flags)
{
	struct s2p_gpio_state *s2mpa05_state = gpiochip_get_data(chip);
	int ret = 0;

	ret = s2p_gpio_of_xlate(s2mpa05_state, chip, gpio_desc, flags);

	return ret;
}

static void s2mpa05_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	const struct s2p_gpio_state *s2mpa05_state = gpiochip_get_data(chip);
	uint32_t i;

	for (i = 0; i < chip->ngpio; i++) {
		s2mpa05_gpio_pin_dbg_show(s2mpa05_state->ctrl, s, i);
		seq_puts(s, "\n");
	}
}

static int s2mpa05_gpio_set_config(struct gpio_chip *chip, uint32_t pin, unsigned long config)
{
	const struct s2p_gpio_state *s2mpa05_state = gpiochip_get_data(chip);

	dev_err(s2mpa05_state->dev, "[EXT_PMIC] %s: pin%d: config(%#lx)\n", __func__, pin, config);

	return s2mpa05_gpio_pin_config_set(s2mpa05_state->ctrl, pin, &config, 1);
}

static const struct gpio_chip s2mpa05_gpio_chip = {
	.direction_input	= s2mpa05_gpio_direction_input,
	.direction_output	= s2mpa05_gpio_direction_output,
	.get			= s2mpa05_gpio_get,
	.set			= s2mpa05_gpio_set,
	.request		= gpiochip_generic_request,
	.free			= gpiochip_generic_free,
	.of_xlate		= s2mpa05_gpio_of_xlate,
	.dbg_show		= s2mpa05_gpio_dbg_show,
	.set_config		= s2mpa05_gpio_set_config,
};

static struct pinctrl_desc s2mpa05_pinctrl_desc = {
	.pctlops		= &s2mpa05_gpio_pinctrl_ops,
	.pmxops			= &s2mpa05_gpio_pinmux_ops,
	.confops		= &s2mpa05_gpio_pinconf_ops,
	.owner			= THIS_MODULE,
	.pins			= s2mpa05_pin_desc,
	.npins			= ARRAY_SIZE(s2mpa05_pin_desc),
	.custom_params		= s2p_gpio_bindings,
	.num_custom_params	= ARRAY_SIZE(s2p_gpio_bindings),
#ifdef CONFIG_DEBUG_FS
	.custom_conf_items	= s2p_conf_items,
#endif
};

static int s2mpa05_gpio_parse_dt(const struct s2mpa05_dev *iodev, struct s2p_gpio_state *state)
{
	struct device_node *s2mpa05_mfd_np = NULL, *s2mpa05_gpio_np = NULL;
	uint32_t val;
	int ret;

	if (!iodev->dev->of_node) {
		pr_err("%s: error\n", __func__);
		return -ENODEV;
	}

	s2mpa05_mfd_np = iodev->dev->of_node;

	s2mpa05_gpio_np = of_find_node_by_name(s2mpa05_mfd_np, "s2mpa05-gpio");
	if (!s2mpa05_gpio_np) {
		pr_err("%s: could not find current_node\n", __func__);
		return -ENODEV;
	}
	state->dev->of_node = s2mpa05_gpio_np;

	ret = of_property_read_u32(s2mpa05_gpio_np, "samsung,npins", &val);
	if (ret)
		state->npins = ARRAY_SIZE(s2mpa05_pin_desc);
	else
		state->npins = val;

	return 0;
}

static int s2mpa05_set_gpio_pad(const struct s2p_gpio_state *state)
{
	struct s2p_gpio_pad *s2mpa05_pad = NULL, *s2mpa05_pads = NULL;
	int i = 0, ret = 0;

	s2mpa05_pads = devm_kcalloc(state->dev, state->npins, sizeof(*s2mpa05_pads), GFP_KERNEL);
	if (!s2mpa05_pads)
		return -ENOMEM;

	for (i = 0; i < state->npins; i++) {
		s2mpa05_pad = &s2mpa05_pads[i];
		s2mpa05_pin_desc[i].drv_data = s2mpa05_pad;

		ret = s2p_gpio_populate(state, s2mpa05_pad, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static void s2mpa05_set_gpio_chip(struct s2p_gpio_state *s2mpa05_state)
{
	s2mpa05_state->chip = s2mpa05_gpio_chip;
	s2mpa05_state->chip.parent = s2mpa05_state->dev;
	s2mpa05_state->chip.base = -1;
	s2mpa05_state->chip.ngpio = s2mpa05_state->npins;
	s2mpa05_state->chip.label = dev_name(s2mpa05_state->dev);
	s2mpa05_state->chip.of_gpio_n_cells = 2;
	s2mpa05_state->chip.can_sleep = false;
	//state->chip.of_node = state->dev->of_node;	//TODO: chage main line
}

static void s2mpa05_set_gpio_state(struct s2p_gpio_state *state)
{
	state->groups = s2mpa05_pin_groups;
	state->ngroups = S2MPA05_GROUP_SIZE;
	state->gpio_groups = s2mpa05_gpio_groups;
	state->gpio_groups_size = ARRAY_SIZE(s2mpa05_gpio_groups);
	state->pin_desc = s2mpa05_pin_desc;
	state->strengths = s2mpa05_strengths;

	state->bit->gpio_oen_shift = S2MPA05_GPIO_OEN_SHIFT;
	state->bit->gpio_oen_mask = S2MPA05_GPIO_OEN_MASK;
	state->bit->gpio_out_shift = S2MPA05_GPIO_OUT_SHIFT;
	state->bit->gpio_out_mask = S2MPA05_GPIO_OUT_MASK;
	state->bit->gpio_pull_shift = S2MPA05_GPIO_PULL_SHIFT;
	state->bit->gpio_pull_mask = S2MPA05_GPIO_PULL_MASK;
	state->bit->gpio_drv_str_shift = S2MPA05_GPIO_DRV_STR_SHIFT;
	state->bit->gpio_drv_str_mask = S2MPA05_GPIO_DRV_STR_MASK;
	state->bit->gpio_mode_digit_input = S2MPA05_GPIO_MODE_DIGITAL_INPUT;
	state->bit->gpio_mode_digit_output = S2MPA05_GPIO_MODE_DIGITAL_OUTPUT;
	state->bit->gpio_pull_disable = S2MPA05_GPIO_PULL_DISABLE;
	state->bit->gpio_pull_down = S2MPA05_GPIO_PULL_DOWN;
	state->bit->gpio_pull_up = S2MPA05_GPIO_PULL_UP;

	state->get_gpio_output_func = s2mpa05_get_gpio_output;
	state->get_set_reg_func = s2mpa05_get_set_reg;
	state->get_status_reg_func = s2mpa05_get_status_reg;
	state->set_gpio_conf_output_func = s2mpa05_set_gpio_conf_output;
}

static int s2mpa05_gpio_probe(struct platform_device *pdev)
{
	const struct s2mpa05_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct s2p_gpio_state *s2mpa05_state = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "[EXT_PMIC] %s: start\n", __func__);

	s2mpa05_state = devm_kzalloc(&pdev->dev, sizeof(*s2mpa05_state), GFP_KERNEL);
	if (!s2mpa05_state)
		return -ENOMEM;

	s2mpa05_state->bit = devm_kzalloc(&pdev->dev, sizeof(*s2mpa05_state->bit), GFP_KERNEL);
	if (!s2mpa05_state->bit)
		return -ENOMEM;

	platform_set_drvdata(pdev, s2mpa05_state);
	s2mpa05_state->dev = &pdev->dev;
	s2mpa05_state->sdev = iodev->sdev;
	s2mpa05_state->gpio_addr = iodev->gpio;
	s2mpa05_pinctrl_desc.name = dev_name(&pdev->dev);

	s2mpa05_set_gpio_state(s2mpa05_state);

	ret = s2mpa05_gpio_parse_dt(iodev, s2mpa05_state);
	if (ret < 0)
		return ret;

	ret = s2mpa05_set_gpio_pad(s2mpa05_state);
	if (ret < 0)
		return ret;

	s2mpa05_set_gpio_chip(s2mpa05_state);

	s2mpa05_state->ctrl = devm_pinctrl_register(s2mpa05_state->dev, &s2mpa05_pinctrl_desc, s2mpa05_state);
	if (IS_ERR(s2mpa05_state->ctrl))
		return PTR_ERR(s2mpa05_state->ctrl);

	ret = devm_gpiochip_add_data(s2mpa05_state->dev, &s2mpa05_state->chip, s2mpa05_state);
	if (ret) {
		dev_err(s2mpa05_state->dev, "can't add gpio chip\n");
		return ret;
	}

	/*
	 * For DeviceTree-supported systems, the gpio core checks the
	 * pinctrl's device node for the "gpio-ranges" property.
	 * If it is present, it takes care of adding the pin ranges
	 * for the driver. In this case the driver can skip ahead.
	 *
	 * In order to remain compatible with older, existing DeviceTree
	 * files which don't set the "gpio-ranges" property or systems that
	 * utilize ACPI the driver has to call gpiochip_add_pin_range().
	 */
	if (!of_property_read_bool(s2mpa05_state->dev->of_node, "gpio-ranges")) {
		ret = gpiochip_add_pin_range(&s2mpa05_state->chip, dev_name(s2mpa05_state->dev), 0, 0,
					     s2mpa05_state->npins);
		if (ret) {
			dev_err(&pdev->dev, "failed to add pin range\n");
			return ret;
		}
	}

	dev_info(&pdev->dev, "[EXT_PMIC] %s: end\n", __func__);

	return 0;
}

static const struct of_device_id s2mpa05_gpio_of_match[] = {
	{ .compatible = "s2mpa05-gpio" },
	{ },
};

MODULE_DEVICE_TABLE(of, s2mpa05_gpio_of_match);

static struct platform_driver s2mpa05_gpio_driver = {
	.driver = {
		   .name = "s2mpa05-gpio",
		   .of_match_table = s2mpa05_gpio_of_match,
	},
	.probe	= s2mpa05_gpio_probe,
};

module_platform_driver(s2mpa05_gpio_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Samsung SPMI PMIC GPIO pin control driver");
MODULE_ALIAS("platform:samsung-spmi-gpio");
MODULE_LICENSE("GPL v2");
